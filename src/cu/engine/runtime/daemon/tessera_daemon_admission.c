// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_daemon_admission.c: frames, admission, the handler and the timer
#include "tessera_daemon_internal.h"

_Alignas(8) static const char s_daemon_precalc[] = "precalc saved\n";
_Alignas(8) static const char s_daemon_reason_grace[] = "reserved past its grace time over its signum's last peak";
_Alignas(8) static const char s_daemon_reason_ended[] = "its process ended";

void daemon_frame_start(TesseraFrame *frame, unsigned int type, unsigned long long identity)
{
    memset(frame, 0, sizeof(*frame));
    frame->magic = TESSERA_MAGIC;
    frame->version = TESSERA_VERSION;
    frame->type = type;
    memcpy(frame->device, g_daemon.device, TESSERA_DEVICE_BYTES);
    frame->identity = identity;
    frame->luid = g_daemon.luid;
}

static void daemon_peer_wake(TesseraPeer *peer)
{
#if defined(_WIN32)
    SetEvent(peer->wake);
#else
    const char byte = 1;
    const ssize_t woken = write(peer->wake[1], &byte, 1u);
    (void)woken;
#endif
}

static void daemon_post(TesseraPeer *peer, unsigned int type, unsigned long long identity, unsigned long long bytes,
                        unsigned long long measured)
{
    daemon_frame_start(&peer->decisive, type, identity);
    peer->decisive.signum = peer->signum;
    peer->decisive.bytes = bytes;
    peer->decisive.measured = measured;
    peer->has_decisive = 1;
    daemon_peer_wake(peer);
}

static TesseraPeer *daemon_peer_of(unsigned long long identity)
{
    for (TesseraPeer *peer = g_daemon.peers; peer != NULL; peer = peer->next)
    {
        if ((identity != 0ull) && (peer->identity == identity))
        {
            return peer;
        }
    }
    return NULL;
}

void daemon_device_read(void)
{
    unsigned long long capacity = 0ull;
    unsigned long long in_use = 0ull;
    if (!tessera_measure_device(g_daemon.measure, &capacity, &in_use))
    {
        return;
    }
    // the host's processors are in use by the jobs running on them, as each reports; the desktop's own work runs on
    // the cores reserved from the jobs and is not counted
    for (unsigned long long at = 0ull; tessera_measure_host(g_daemon.measure) && (at < g_daemon.ledger.job_count);
         at += 1ull)
    {
        const TesseraJob *const job = &g_daemon.ledger.jobs[at];
        in_use += (job->state == TESSERA_JOB_RUNNING) ? job->used : 0ull;
    }
    tessera_ledger_device(&g_daemon.ledger, capacity, in_use);
}

static void daemon_admit(unsigned long long now)
{
    daemon_device_read();
    const unsigned long long start_limit = g_daemon.ledger.job_count;
    if (start_limit == 0ull)
    {
        return;
    }
    TesseraEvent *const events = (TesseraEvent *)calloc((size_t)start_limit, sizeof(TesseraEvent));
    if (events == NULL)
    {
        return;
    }
    const unsigned long long made = tessera_ledger_admit(&g_daemon.ledger, now, events, start_limit);
    for (unsigned long long at = 0ull; at < made; at += 1ull)
    {
        TesseraPeer *const peer = daemon_peer_of(events[at].identity);
        if (peer != NULL)
        {
            daemon_post(peer, TESSERA_NOTICE_ADMITTED, events[at].identity, events[at].bytes, events[at].measured);
            // an admission informs the job its whole declaration as the daemon records it: the bytes its process
            // allocated as it requested and the bytes it declared on top
            peer->decisive.declared = peer->located + peer->declared;
        }
    }
    free(events);
    daemon_signal_changed();
}

void daemon_job_dropped(TesseraPeer *peer, unsigned long long now, const char *reason)
{
    const TesseraJob *const job = tessera_ledger_job(&g_daemon.ledger, peer->identity);
    if (job == NULL)
    {
        return;
    }
    daemon_ticket_write(peer->identity, peer, job->peak, job->used, reason);
    tessera_ledger_release(&g_daemon.ledger, peer->identity, now, 0);
    peer->identity = 0ull;
    daemon_admit(now);
}

void daemon_handle(TesseraPeer *peer, const TesseraFrame *frame)
{
    const unsigned long long now = daemon_now();
    if (frame->type == TESSERA_REQUEST_SUBMIT)
    {
        const int acceptable = (peer->identity == 0ull) && (peer->lost_identity == 0ull) &&
                               (memcmp(frame->device, g_daemon.device, TESSERA_DEVICE_BYTES) == 0) &&
                               (frame->luid == g_daemon.luid) && (frame->declared != 0ull) &&
                               (frame->sweep_microseconds != 0ull);
        TesseraJobRequest request;
        request.signum = frame->signum;
        request.declared = frame->declared;
        request.grace_microseconds = frame->grace_microseconds;
        request.sweep_microseconds = frame->sweep_microseconds;
        request.idle_microseconds = frame->idle_microseconds;
        request.override_budget = frame->override_budget;
        // the bytes the job's process already has allocated as it requests, its context and whatever it retained from
        // an earlier job: read by its pid, or taken from its own report where no pid is read from outside
        request.located = 0ull;
        if (tessera_measure_reported(g_daemon.measure))
        {
            request.located = frame->measured;
        }
        else if (!tessera_measure_process(g_daemon.measure, peer->pid, &request.located))
        {
            request.located = 0ull;
        }
        unsigned long long identity = 0ull;
        TesseraEvent event;
        if (!acceptable || !tessera_ledger_submit(&g_daemon.ledger, &request, now, &identity, &event))
        {
            daemon_post(peer, TESSERA_NOTICE_ERROR, 0ull, frame->declared, 0ull);
            return;
        }
        peer->identity = identity;
        peer->signum = frame->signum;
        peer->declared = frame->declared;
        peer->located = request.located;
        peer->grace_microseconds = frame->grace_microseconds;
        peer->sweep_microseconds = frame->sweep_microseconds;
        if (event.type == TESSERA_EVENT_OVER_BUDGET)
        {
            daemon_post(peer, TESSERA_NOTICE_OVER_BUDGET, identity, event.bytes, event.measured);
        }
        daemon_admit(now);
        return;
    }
    if ((frame->type == TESSERA_REQUEST_OVERRIDE) && (frame->identity == peer->identity) &&
        tessera_ledger_override(&g_daemon.ledger, peer->identity, now))
    {
        daemon_admit(now);
        return;
    }
    if ((frame->type == TESSERA_REQUEST_RELEASE) && (frame->identity == peer->identity) && (peer->identity != 0ull))
    {
        const TesseraJob *const job = tessera_ledger_job(&g_daemon.ledger, peer->identity);
        const unsigned long long reservation = (job != NULL) ? job->reservation : 0ull;
        const unsigned long long peak = (job != NULL) ? job->peak : 0ull;
        if ((job != NULL) && tessera_ledger_release(&g_daemon.ledger, peer->identity, now, 1))
        {
            daemon_history_save();
            daemon_post(peer, TESSERA_NOTICE_RELEASED, peer->identity, reservation, peak);
            peer->identity = 0ull;
            daemon_admit(now);
            return;
        }
    }
    if (frame->type == TESSERA_REQUEST_MEASURED)
    {
        // a process's own report is for its measure only where no pid is read from outside; it gets no answer
        TesseraEvent grew;
        if (tessera_measure_reported(g_daemon.measure) && (frame->identity == peer->identity) &&
            (peer->identity != 0ull) &&
            tessera_ledger_measure(&g_daemon.ledger, peer->identity, frame->measured, &grew) &&
            (grew.type == TESSERA_EVENT_GREW))
        {
            peer->grew_to = grew.measured;
            peer->has_grew = 1;
            daemon_peer_wake(peer);
        }
        daemon_admit(now);
        return;
    }
    if ((frame->type == TESSERA_REQUEST_PRECALC_SAVED) && (peer->lost_identity != 0ull) &&
        daemon_ticket_note(peer->lost_identity, peer, s_daemon_precalc))
    {
        daemon_post(peer, TESSERA_NOTICE_RELEASED, peer->lost_identity, 0ull, 0ull);
        peer->lost_identity = 0ull;
        return;
    }
    daemon_post(peer, TESSERA_NOTICE_ERROR, frame->identity, 0ull, 0ull);
}

static void daemon_fired(const TesseraEvent *event, unsigned long long now)
{
    if (event->type == TESSERA_EVENT_IDLE)
    {
        if (g_daemon.peer_count == 0ull)
        {
            daemon_history_save();
#if !defined(_WIN32)
            struct stat endpoint;
            const int ours = !g_daemon.socket_activated && (stat(g_daemon.endpoint, &endpoint) == 0) &&
                             (endpoint.st_dev == g_daemon.endpoint_device) &&
                             (endpoint.st_ino == g_daemon.endpoint_inode);
            if (ours)
            {
                unlink(g_daemon.endpoint);
            }
#endif
            exit(0);
        }
        return;
    }
    TesseraPeer *const peer = daemon_peer_of(event->identity);
    if (peer == NULL)
    {
        return;
    }
    if (event->type == TESSERA_EVENT_LOST)
    {
        const TesseraHistory *const previous = tessera_ledger_history(&g_daemon.ledger, &peer->signum);
        const unsigned long long last_peak = (previous != NULL) ? previous->peak : 0ull;
        daemon_ticket_write(peer->identity, peer, last_peak, 0ull, s_daemon_reason_grace);
        daemon_post(peer, TESSERA_NOTICE_LOST, peer->identity, peer->declared, last_peak);
        peer->lost_identity = peer->identity;
        peer->identity = 0ull;
        daemon_admit(now);
        return;
    }
    if (event->type == TESSERA_EVENT_SWEEP)
    {
        if (!tessera_process_lives(peer->process))
        {
            daemon_job_dropped(peer, now, s_daemon_reason_ended);
            return;
        }
        unsigned long long used = 0ull;
        TesseraEvent grew;
        if (tessera_measure_process(g_daemon.measure, peer->pid, &used) &&
            tessera_ledger_measure(&g_daemon.ledger, peer->identity, used, &grew) && (grew.type == TESSERA_EVENT_GREW))
        {
            peer->grew_to = grew.measured;
            peer->has_grew = 1;
            daemon_peer_wake(peer);
        }
        daemon_admit(now);
    }
}

#if defined(_WIN32)
DWORD WINAPI daemon_timer(LPVOID unused)
#else
void *daemon_timer(void *unused)
#endif
{
    (void)unused;
    daemon_lock();
    // the daemon's life: the idle teardown in daemon_fired ends the process from inside this loop
    while (g_daemon.living)
    {
        TesseraDeadline root;
        const int any = tessera_ledger_next(&g_daemon.ledger, &root);
        const unsigned long long now = daemon_now();
        if (!any || (root.when > now))
        {
            daemon_wait_until(any ? root.when : TESSERA_DAEMON_FOREVER);
            continue;
        }
        TesseraEvent event;
        while (tessera_ledger_fire(&g_daemon.ledger, now, &event))
        {
            daemon_fired(&event, now);
        }
    }
    daemon_unlock();
#if defined(_WIN32)
    return 0u;
#else
    return NULL;
#endif
}
