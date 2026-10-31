// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_serve.c: the door's container side: the processes that join, each in a place of its own with a thread reading
// what it requests and a thread sending it what it receives, the messages on its channels queued between them, a body
// past the room a message is queued in stored in a region of the transfer layer
#include "door_serve.h"

#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "container.h"

CliDoorServed g_cli_door_served = {.lock = CLI_LOCK_INIT};

// the listener the door is joined through
static CliDoorListener s_listener;

// an answer written to the process: 1, or 0 where its end is broken
static int serve_answer(CliDoorJoined *joined, const char *json, unsigned long long size)
{
    cli_lock_take(&joined->output);
    const int written = cli_door_frame_written(&joined->end, json, size, NULL);
    cli_lock_give(&joined->output);
    return written;
}

// a refusal answered, why it was refused given
static int serve_refused(CliDoorJoined *joined, const char *why, unsigned long long size)
{
    char answer[CLI_ERR_MSG_MAX_LEN + 64u];
    VerbaLinea out = {answer, sizeof(answer), 0ull};
    mmgr_line_put(&out, "{\"refused\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)why, size);
    mmgr_line_ch(&out, '}');
    return serve_answer(joined, out.out, out.at);
}

// a message sent on a channel the process listens on queued for it, under the comms layer's lock: 1, 0 where the
// process is gone, -1 where its queue has no room
static int serve_recv(void *context, const char *channel, const char *from, const char *body, unsigned long long size)
{
    CliDoorJoined *const joined = (CliDoorJoined *)context;
    cli_lock_take(&joined->queue_lock);
    if (!joined->reading)
    {
        cli_lock_give(&joined->queue_lock);
        return 0;
    }
    if (joined->queued == CLI_DOOR_SLOTS)
    {
        cli_lock_give(&joined->queue_lock);
        return -1;
    }
    CliDoorSlot *const slot = &joined->slots[(joined->first + joined->queued) % CLI_DOOR_SLOTS];
    memor.cpy(&(MemoriaCfg){.dst = slot->channel,
                            .src = channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = slot->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    slot->size = size;
    slot->has_region = (size > sizeof(slot->body));
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (slot->has_region && !cli_region_with(&slot->region, body, size, &err_msg))
    {
        cli_lock_give(&joined->queue_lock);
        return -1;
    }
    if (!slot->has_region)
    {
        memor.cpy(&(MemoriaCfg){.dst = slot->body, .src = body, .bytes = (size_t)size});
    }
    joined->queued += 1u;
    cli_wake_all(&joined->queue_wake);
    cli_lock_give(&joined->queue_lock);
    return 1;
}

// a queued message sent to the process, through the transfer layer where its body is long: 1, or 0 where it did not go
static int serve_send(CliDoorJoined *joined, CliDoorSlot *slot)
{
    char head[CLI_NAME_MAX_LEN * 3u + 512u];
    VerbaLinea prefix = {head, sizeof(head), 0ull};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int inline_body = (slot->size <= CLI_TRANSFER_SMALL);
    const char *const body = slot->has_region ? (const char *)slot->region.view : slot->body;
    mmgr_line_put(&prefix, "{\"message\":{\"channel\":");
    cli_json_quoted_lossy(&prefix, (const unsigned char *)slot->channel,
                          cellul.len(&(CatenaFinitaCfg){.src = slot->channel, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&prefix, ",\"from\":");
    cli_json_quoted_lossy(&prefix, (const unsigned char *)slot->from,
                          cellul.len(&(CatenaFinitaCfg){.src = slot->from, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&prefix, inline_body ? ",\"body\":" : ",\"transfer\":");
    if (!inline_body && !cli_door_handed(&slot->region, joined->has_peer, joined->peer, &prefix, &err_msg))
    {
        return 0;
    }
    if (!mmgr_line_ok(&prefix))
    {
        return 0;
    }
    const unsigned long long size = prefix.at + (inline_body ? slot->size : 0ull) + 2ull;
    const unsigned char length[4] = {(unsigned char)(size & 0xffu), (unsigned char)((size >> 8u) & 0xffu),
                                     (unsigned char)((size >> 16u) & 0xffu), (unsigned char)((size >> 24u) & 0xffu)};
    cli_lock_take(&joined->output);
    const int sent = cli_door_written(&joined->end, length, sizeof(length), inline_body ? NULL : &slot->region) &&
                     cli_door_written(&joined->end, prefix.out, prefix.at, NULL) &&
                     (!inline_body || cli_door_written(&joined->end, body, slot->size, NULL)) &&
                     cli_door_written(&joined->end, "}}", 2ull, NULL);
    cli_lock_give(&joined->output);
    if (sent && !inline_body)
    {
        cli_comms_transferred(slot->channel, slot->size);
    }
    return sent;
}

// a thread's end in the process's place: the last of the two lets the end go and gives the place back
static void serve_thread_ended(CliDoorJoined *joined)
{
    cli_lock_take(&g_cli_door_served.lock);
    joined->going -= 1u;
    const int last = (joined->going == 0u);
    cli_lock_give(&g_cli_door_served.lock);
    if (!last)
    {
        return;
    }
    cli_door_closed(&joined->end);
    cli_lock_take(&g_cli_door_served.lock);
    g_cli_door_served.joined[joined->place] = NULL;
    cli_lock_give(&g_cli_door_served.lock);
    g_cli_cells->persistent_buf_release(joined, &g_cli_door_cells);
}

// the messages queued sent to the process, one at a time, until the process is gone and its queue is empty; a message
// that does not go ends the sending, its channels let go
static void serve_drained(void *context)
{
    CliDoorJoined *const joined = (CliDoorJoined *)context;
    int sending = 1;
    for (;;)
    {
        cli_lock_take(&joined->queue_lock);
        while (joined->reading && (joined->queued == 0u))
        {
            cli_wake_wait(&joined->queue_wake, &joined->queue_lock);
        }
        if (joined->queued == 0u)
        {
            cli_lock_give(&joined->queue_lock);
            break;
        }
        CliDoorSlot *const slot = &joined->slots[joined->first];
        cli_lock_give(&joined->queue_lock);
        sending = sending && serve_send(joined, slot);
        if (slot->has_region)
        {
            cli_region_closed(&slot->region);
        }
        cli_lock_take(&joined->queue_lock);
        joined->first = (joined->first + 1u) % CLI_DOOR_SLOTS;
        joined->queued -= 1u;
        if (!sending)
        {
            joined->reading = 0;
        }
        cli_lock_give(&joined->queue_lock);
        if (!sending)
        {
            cli_door_shut(&joined->end);
        }
    }
    serve_thread_ended(joined);
}

// whether the process at the other end may take the key's part: a persistent part's key from any process, else only the
// process the key was given to or one it stands under; refused where it may not
static int serve_may_join(CliDoorJoined *joined, const char *key, const char *name)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned int given = 0u;
    if (cli_comms_is_persistent(key))
    {
        return 1;
    }
    if (!cli_comms_process_of(key, &given))
    {
        mmgr_line_put(&err_msg, "the key of ");
        mmgr_line_put(&err_msg, name);
        mmgr_line_put(&err_msg, " was given to no process");
        (void)serve_refused(joined, err_msg.out, err_msg.at);
        return 0;
    }
    unsigned int count = 0u;
    CliProcess *const parents = cli_processes_taken(&g_cli_door_cells, &count);
    const int under = joined->has_peer && ((joined->peer == given) || cli_under(given, joined->peer, parents, count));
    g_cli_cells->persistent_buf_release(parents, &g_cli_door_cells);
    if (!under)
    {
        mmgr_line_put(&err_msg, "the key of ");
        mmgr_line_put(&err_msg, name);
        mmgr_line_put(&err_msg, " was given to another process");
        (void)serve_refused(joined, err_msg.out, err_msg.at);
    }
    return under;
}

// a joined process served: its first frame names its key, and each frame after requests to listen or to send, until its
// end breaks; its channels are let go and its queue drained as it goes
static void serve_read(void *context)
{
    CliDoorJoined *const joined = (CliDoorJoined *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long size = 0ull;
    if (cli_door_frame_read(&joined->end, joined->frame, sizeof(joined->frame), &size, &err_msg) == CLI_DOOR_FRAME_READ)
    {
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        joined->key[0] = '\0';
        if (cli_lsp_member(joined->frame, size, "key", &start, &length))
        {
            (void)cli_lsp_string(joined->frame + start, length, joined->key, sizeof(joined->key));
        }
        char name[CLI_NAME_MAX_LEN];
        if (!cli_comms_name_of(joined->key, name))
        {
            (void)serve_refused(joined, "that key is no part's", 21ull);
        }
        else if (serve_may_join(joined, joined->key, name))
        {
            char answer[CLI_NAME_MAX_LEN + 64u];
            VerbaLinea out = {answer, sizeof(answer), 0ull};
            mmgr_line_put(&out, "{\"ok\":\"joined\",\"name\":");
            cli_json_quoted_lossy(&out, (const unsigned char *)name,
                                  cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
            mmgr_line_ch(&out, '}');
            if (serve_answer(joined, out.out, out.at))
            {
                cli_door_requests_served(joined);
            }
        }
    }
    for (unsigned int at = 0u; at < joined->listens; at += 1u)
    {
        cli_comms_unlisten(joined->listeners[at]);
    }
    cli_lock_take(&joined->queue_lock);
    joined->reading = 0;
    cli_wake_all(&joined->queue_wake);
    cli_lock_give(&joined->queue_lock);
    serve_thread_ended(joined);
}

// the processes that join taken, each into a place of its own, a cell, with its two threads, one refused where every
// place is taken or the cellblock cannot meet one
static void serve_accepted(void *context)
{
    (void)context;
    for (;;)
    {
        CliDoorEnd end;
        int has_peer = 0;
        unsigned int peer = 0u;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        if (!cli_door_accepted(&s_listener, &end, &has_peer, &peer, &err_msg))
        {
            query_paused(100u);
            continue;
        }
        CliDoorJoined *joined = NULL;
        cli_lock_take(&g_cli_door_served.lock);
        unsigned int place = CLI_DOOR_JOINED;
        for (unsigned int at = 0u; (at < CLI_DOOR_JOINED) && (place == CLI_DOOR_JOINED); at += 1u)
        {
            place = (g_cli_door_served.joined[at] == NULL) ? at : place;
        }
        joined = (place < CLI_DOOR_JOINED) ? g_cli_cells->persistent_buf_alloc(sizeof(CliDoorJoined), &g_cli_door_cells)
                                           : NULL;
        if (joined != NULL)
        {
            joined->place = place;
            joined->going = 2u;
            g_cli_door_served.joined[place] = joined;
        }
        cli_lock_give(&g_cli_door_served.lock);
        if (joined == NULL)
        {
            static const char s_full[] = "{\"refused\":\"the door has as many processes as it can\"}";
            (void)cli_door_frame_written(&end, s_full, sizeof(s_full) - 1u, NULL);
            cli_door_closed(&end);
            continue;
        }
        joined->end = end;
        joined->has_peer = has_peer;
        joined->peer = peer;
        joined->output = (CliLock)CLI_LOCK_INIT;
        joined->queue_lock = (CliLock)CLI_LOCK_INIT;
        joined->queue_wake = (CliWake)CLI_WAKE_INIT;
        joined->first = 0u;
        joined->queued = 0u;
        joined->listens = 0u;
        joined->reading = 1;
        joined->reader = (CliThread){serve_read, joined};
        joined->teller = (CliThread){serve_drained, joined};
        if (!cli_thread_started(&joined->teller))
        {
            serve_thread_ended(joined);
        }
        if (!cli_thread_started(&joined->reader))
        {
            serve_read(joined);
        }
    }
}

int cli_door_open(char *address, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_door_served.lock);
    if (g_cli_door_served.open)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = address,
            .src = g_cli_door_served.address,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_door_served.address, .cap = CLI_CELLS_POOL}) + 1u});
        cli_lock_give(&g_cli_door_served.lock);
        return 1;
    }
    if (!cli_door_listening(&s_listener, g_cli_door_served.address, err_msg))
    {
        cli_lock_give(&g_cli_door_served.lock);
        return 0;
    }
    g_cli_door_served.accepter = (CliThread){serve_accepted, NULL};
    if (!cli_thread_started(&g_cli_door_served.accepter))
    {
        cli_lock_give(&g_cli_door_served.lock);
        mmgr_line_put(err_msg, "the door's thread did not start");
        return 0;
    }
    g_cli_door_served.open = 1;
    memor.cpy(&(MemoriaCfg){
        .dst = address,
        .src = g_cli_door_served.address,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_door_served.address, .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&g_cli_door_served.lock);
    return 1;
}

int cli_door_listen_requested(CliDoorJoined *joined, const char *channel, VerbaLinea *err_msg)
{
    unsigned int listener = 0u;
    if (joined->listens == CLI_DOOR_LISTENS)
    {
        mmgr_line_put(err_msg, "a process listens on at most 32 channels");
        return 0;
    }
    if (!cli_comms_listen(joined->key, channel, 1, serve_recv, joined, &listener, err_msg))
    {
        return 0;
    }
    joined->listeners[joined->listens] = listener;
    joined->listens += 1u;
    return 1;
}

int cli_door_refused(CliDoorJoined *joined, const char *why, unsigned long long size)
{
    return serve_refused(joined, why, size);
}

int cli_door_answered(CliDoorJoined *joined, const char *json, unsigned long long size)
{
    return serve_answer(joined, json, size);
}
