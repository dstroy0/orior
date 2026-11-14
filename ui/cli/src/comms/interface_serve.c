// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_serve.c: the interface's container side: the processes that join, each in a place of its own with a thread reading
// what it requests and a thread sending it what it receives, the messages on its channels queued between them, a body
// past the capacity a message is queued in stored in a region of the transfer layer
#include "interface_serve.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/rings.h"

#include "container.h"

CliIpcServed g_cli_ipc_served = {0};

// the listener the interface is joined through
static CliIpcListener s_listener;

typedef struct
{
    CliIpcJoined *joined;
    const char *bytes;
    unsigned long long size;
    const char *why;
    CliIpcEntry *entry;
    const char *key;
    const char *name;
} InterfaceServeCtx;
// an answer passed to the writer of the process's end, once the one before it has gone: 1, or 0 where its end is broken
EMBED_INLINE int serve_answer(const InterfaceServeCtx *args)
{
    CliIpcJoined *const joined = args->joined;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &joined->queue_wake);
        if (EMBED_ATOMIC_LOAD(&joined->sending) == 0u)
        {
            return 0;
        }
        if (EMBED_ATOMIC_LOAD(&joined->answer_waits) == 0u)
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &joined->queue_wake, .seen = seen);
    }
    const unsigned long long size = (args->size < CLI_IPC_SERVE_ANSWER) ? args->size : CLI_IPC_SERVE_ANSWER;
    memor.cpy(&(MemoriaCfg){.dst = joined->answer, .src = args->bytes, .bytes = (embed_word)size});
    joined->answer_size = size;
    EMBED_ATOMIC_STORE(&joined->answer_waits, 1u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &joined->queue_wake);
    return 1;
}

// a refusal answered, why it was refused given
EMBED_INLINE int serve_reject(const InterfaceServeCtx *args)
{
    char answer[CLI_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea out = {answer, sizeof(answer), 0ULL};
    mmgr_line_ch(&out, CLI_IPC_TYPE_REFUSED);
    mmgr_line_put_n(&out, args->why, args->size);
    return EMBED_CALL(serve_answer, InterfaceServeCtx, .joined = args->joined, .bytes = out.out, .size = out.at);
}

// a message sent on a channel the process listens on queued for it, from any thread, a ticket taken and its slot
// granted before the entry is written and pushed: 1, 0 where the process is gone, -1 where every slot is in flight
static int serve_recv(const CliCommsRecvCfg *args)
{
    unsigned long long size = args->size;
    CliIpcJoined *const joined = (CliIpcJoined *)args->context;
    if (EMBED_ATOMIC_LOAD(&joined->read_state) == 0u)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliRegion region;
    const int has_region = (size > CLI_IPC_ENTRY_BODY);
    if (has_region && !EMBED_CALL(cli_transfer_region_bytes.region_with, CliTransferRegionBytesCfg, .region = &region,
                                  .bytes = args->body, .size = size, .err_msg = &err_msg))
    {
        return -1;
    }
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&joined->claimed, 1u);
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), ticket);
    CliIpcEntry *const entry = &joined->entries[ticket & (CLI_IPC_ENTRIES - 1u)];
    // Explicit cast carries the entry's address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)entry;
    embed_u64 seen = 0u;
    // the slot is granted once the entry a lap before it is sent and released; this thread sleeps on the queue's
    // wake until then, and a message that comes once a thread of the place has ended is let go
    for (;;)
    {
        const unsigned int room = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &joined->queue_wake);
        if (cli_ticket_room(&joined->ring, ticket) &&
            embed_ticket_ring_granted(&joined->turnstiles, address, &joined->ring, stamp, &seen))
        {
            break;
        }
        if (EMBED_ATOMIC_LOAD(&joined->going) != 2u)
        {
            if (has_region)
            {
                EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &region);
            }
            return -1;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &joined->queue_wake, .seen = room);
    }
    memor.cpy(&(MemoriaCfg){.dst = entry->channel,
                            .src = args->channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = entry->from,
                            .src = args->from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->from, .cap = CLI_POOL_BYTES}) + 1u});
    entry->size = size;
    entry->has_region = has_region;
    if (has_region)
    {
        entry->region = region;
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = entry->body, .src = args->body, .bytes = (embed_word)size});
    }
    (void)embed_ticket_ring_pushed(&joined->turnstiles, address, &joined->ring, stamp, entry, &seen);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &joined->queue_wake);
    return 1;
}

// a queued message sent to the process, through the transfer layer where its body is long: 1, or 0 where it did not go
EMBED_INLINE int serve_send(const InterfaceServeCtx *args)
{
    char head[CLI_NAME_MAX_LEN * 3u + 512u];
    VerbaLinea prefix = {head, sizeof(head), 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int inline_body = (args->entry->size <= CLI_TRANSFER_SMALL);
    const char *const body = args->entry->has_region ? (const char *)args->entry->region.view : args->entry->body;
    mmgr_line_ch(&prefix, CLI_IPC_TYPE_MESSAGE);
    mmgr_line_put(&prefix, args->entry->channel);
    mmgr_line_ch(&prefix, '\0');
    mmgr_line_put(&prefix, args->entry->from);
    mmgr_line_ch(&prefix, '\0');
    mmgr_line_ch(&prefix, inline_body ? CLI_IPC_BODY_INLINE : CLI_IPC_BODY_TRANSFER);
    if (!inline_body &&
        !EMBED_CALL(cli_ipc_err_msg_region_internal.dispatch, CliIpcErrMsgRegionCtx, .region = &args->entry->region,
                    .has_to = args->joined->has_peer, .to = args->joined->peer, .out = &prefix, .err_msg = &err_msg))
    {
        return 0;
    }
    if (!mmgr_line_ok(&prefix))
    {
        return 0;
    }
    const unsigned long long size = prefix.at + (inline_body ? args->entry->size : 0ULL);
    const unsigned char length[4] = {(unsigned char)(size & 0xffu), (unsigned char)((size >> 8u) & 0xffu),
                                     (unsigned char)((size >> 16u) & 0xffu), (unsigned char)((size >> 24u) & 0xffu)};
    const int sent =
        EMBED_CALL(cli_ipc_end_size_internal.write, CliIpcEndSizeCtx, .end = &args->joined->end, .bytes_read = length,
                   .size = sizeof(length), .region = inline_body ? NULL : &args->entry->region) &&
        EMBED_CALL(cli_ipc_end_size_internal.write, CliIpcEndSizeCtx, .end = &args->joined->end,
                   .bytes_read = prefix.out, .size = prefix.at, .region = NULL) &&
        (!inline_body || EMBED_CALL(cli_ipc_end_size_internal.write, CliIpcEndSizeCtx, .end = &args->joined->end,
                                    .bytes_read = body, .size = args->entry->size, .region = NULL));
    if (sent && !inline_body)
    {
        EMBED_CALL(cli_comms_channel_bytes.transfer, CliCommsChannelBytesCfg, .channel = args->entry->channel,
                   .bytes = args->entry->size);
    }
    return sent;
}

// a thread's end in the process's place: the last of the two lets the end go and gives the place back
EMBED_INLINE void serve_thread_end(const InterfaceServeCtx *args)
{
    const int last = (EMBED_ATOMIC_ADD_32(&args->joined->going, (embed_u32)-1) == 1u);
    if (!last)
    {
        return;
    }
    EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &args->joined->end);
    g_cli_ipc_served.joined[args->joined->place] = NULL;
}

// what the process's end has stopped taking told: its reader let go, and nothing more written
EMBED_INLINE void serve_end_broken(const InterfaceServeCtx *args)
{
    EMBED_ATOMIC_STORE(&args->joined->sending, 0u);
    EMBED_ATOMIC_STORE(&args->joined->read_state, 0u);
    EMBED_CALL(cli_ipc_internal.shut, CliIpcCtx, .end = &args->joined->end);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->joined->queue_wake);
}

// the end's one writer: the answer waiting and the messages queued sent to the process, one at a time, until the
// process is gone and nothing waits; what does not go ends the sending, and what waits after it is let go
static void serve_drain(void *context)
{
    CliIpcJoined *const joined = (CliIpcJoined *)context;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &joined->queue_wake);
        if (EMBED_ATOMIC_LOAD(&joined->answer_waits) != 0u)
        {
            if ((EMBED_ATOMIC_LOAD(&joined->sending) != 0u) &&
                !EMBED_CALL(cli_ipc_end_internal.frame_write, CliIpcEndCtx, .end = &joined->end,
                            .bytes = joined->answer, .size = joined->answer_size, .region = NULL))
            {
                EMBED_CALL(serve_end_broken, InterfaceServeCtx, .joined = joined);
            }
            EMBED_ATOMIC_STORE(&joined->answer_waits, 0u);
            EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &joined->queue_wake);
            continue;
        }
        // the next entry pushed popped, in the order of the tickets, sent, and its slot released
        embed_u64 stamp = 0u;
        void *job = NULL;
        embed_u64 found = 0u;
        if (cli_ticket_pushed(&joined->ring, joined->taken, &stamp) &&
            // Explicit cast carries the place's address as the pass's entry
            embed_ticket_ring_popped(&joined->turnstiles, (embed_atomic_uptr_value)(embed_word)joined, &joined->ring,
                                     stamp, &job, &found))
        {
            CliIpcEntry *const entry = job;
            if ((EMBED_ATOMIC_LOAD(&joined->sending) != 0u) &&
                !EMBED_CALL(serve_send, InterfaceServeCtx, .joined = joined, .entry = entry))
            {
                EMBED_CALL(serve_end_broken, InterfaceServeCtx, .joined = joined);
            }
            if (entry->has_region)
            {
                EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &entry->region);
            }
            (void)embed_ticket_ring_released(&joined->turnstiles, &joined->ring, stamp, &found);
            joined->taken += 1u;
            // a reading thread asleep for the slot's room wakes to it
            EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &joined->queue_wake);
            continue;
        }
        if ((EMBED_ATOMIC_LOAD(&joined->read_state) == 0u) && (EMBED_ATOMIC_LOAD(&joined->claimed) == joined->taken))
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &joined->queue_wake, .seen = seen);
    }
    EMBED_CALL(serve_thread_end, InterfaceServeCtx, .joined = joined);
}

// whether the process at the other end may take the key's part: a persistent part's key from any process, else only the
// process the key was given to or one it is under; refused where it may not
EMBED_INLINE int serve_may_join(const InterfaceServeCtx *args)
{
    const char *key = args->key;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    unsigned int supplied = 0u;
    if (EMBED_CALL(cli_comms_process.is_persistent, CliCommsProcessCfg, .key = key))
    {
        return 1;
    }
    if (!EMBED_CALL(cli_comms_key_process.process_get, CliCommsKeyProcessCfg, .key = key, .process_pointer = &supplied))
    {
        mmgr_line_put(&err_msg, "the key of ");
        mmgr_line_put(&err_msg, args->name);
        mmgr_line_put(&err_msg, " was given to no process");
        (void)EMBED_CALL(serve_reject, InterfaceServeCtx, .joined = args->joined, .why = err_msg.out, .size = err_msg.at);
        return 0;
    }
    unsigned int count = 0u;
    CliProcess *const parents =
        EMBED_CALL(cli_container.processes_alloc, CliContainerCfg, .owner = g_cli_ipc_owner, .count_pointer = &count);
    const int under =
        args->joined->has_peer && ((args->joined->peer == supplied) ||
                                   EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = supplied,
                                              .id = args->joined->peer, .parents = parents, .count = count));
    g_cli_guard->persistent_buf_release(parents, g_cli_ipc_owner);
    if (!under)
    {
        mmgr_line_put(&err_msg, "the key of ");
        mmgr_line_put(&err_msg, args->name);
        mmgr_line_put(&err_msg, " was given to another process");
        (void)EMBED_CALL(serve_reject, InterfaceServeCtx, .joined = args->joined, .why = err_msg.out, .size = err_msg.at);
    }
    return under;
}

// a joined process served: its first frame names its key, and each frame after requests to listen or to send, until its
// end breaks; its channels are let go and its queue drained as it goes
static void serve_read(void *context)
{
    CliIpcJoined *const joined = (CliIpcJoined *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    unsigned long long size = 0ULL;
    if (EMBED_CALL(cli_ipc_err_msg_end_internal.frame_read, CliIpcErrMsgEndCtx, .end = &joined->end,
                   .frame = joined->frame, .capacity = CLI_IPC_FRAME, .size_pointer = &size,
                   .err_msg = &err_msg) == CLI_IPC_FRAME_READ)
    {
        joined->key[0] = '\0';
        if ((size != 0ULL) && (joined->frame[0] == CLI_IPC_TYPE_JOIN) &&
            (cli_ipc_field_end(joined->frame, size, 1ULL) != 0ULL))
        {
            VerbaLinea key = {joined->key, sizeof(joined->key), 0ULL};
            mmgr_line_put(&key, joined->frame + 1);
            (void)mmgr_line_finish(&key);
        }
        char name[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(cli_comms_key.name_get, CliCommsKeyCfg, .key = joined->key, .name_out = name))
        {
            (void)EMBED_CALL(serve_reject, InterfaceServeCtx, .joined = joined, .why = "that key is no part's",
                             .size = 21ULL);
        }
        else if (EMBED_CALL(serve_may_join, InterfaceServeCtx, .joined = joined, .key = joined->key, .name = name))
        {
            char answer[CLI_NAME_MAX_LEN + 8u];
            VerbaLinea out = {answer, sizeof(answer), 0ULL};
            mmgr_line_ch(&out, CLI_IPC_TYPE_ANSWER);
            mmgr_line_put(&out, name);
            if (EMBED_CALL(serve_answer, InterfaceServeCtx, .joined = joined, .bytes = out.out, .size = out.at))
            {
                EMBED_CALL(cli_ipc_serve.ipc_requests_serve, CliIpcServeCfg, .joined = joined);
            }
        }
    }
    for (unsigned int at = 0u; at < joined->listens; at += 1u)
    {
        EMBED_CALL(cli_comms_listener.unlisten, CliCommsListenerCfg, .listener_value = joined->listeners[at]);
    }
    EMBED_ATOMIC_STORE(&joined->read_state, 0u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &joined->queue_wake);
    EMBED_CALL(serve_thread_end, InterfaceServeCtx, .joined = joined);
}

// the processes that join taken, each into a place of its own, a cell, with its two threads, one refused where every
// place is taken or the cellblock cannot meet one
static void serve_accept(void *context)
{
    (void)context;
    for (;;)
    {
        CliIpcEnd end;
        int has_peer = 0;
        unsigned int peer = 0u;
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if (!EMBED_CALL(cli_ipc_listener_internal.accept, CliIpcListenerCtx, .listener = &s_listener, .end = &end,
                        .has_peer = &has_peer, .peer = &peer, .err_msg = &err_msg))
        {
            EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
            continue;
        }
        CliIpcJoined *joined = NULL;
        unsigned int place = CLI_IPC_JOINED;
        {
            unsigned int at = 0u;
            while ((at < CLI_IPC_JOINED) && (place == CLI_IPC_JOINED))
            {
                place = (g_cli_ipc_served.joined[at] == NULL) ? at : place;
                at += 1u;
            }
        }
        // a joined process's record, frame, queued bodies and answer are its two threads' buffers, so they are
        // fixed outside the pool, one set for each place, where they punch no hole in the persistent tier
        static CliIpcJoined s_places[CLI_IPC_JOINED];
        static char s_bytes[CLI_IPC_JOINED]
                           [CLI_IPC_FRAME + (CLI_IPC_ENTRIES * CLI_IPC_ENTRY_BODY) + CLI_IPC_SERVE_ANSWER];
        joined = (place < CLI_IPC_JOINED) ? &s_places[place] : NULL;
        if (joined != NULL)
        {
            memor.set(&(MemoriaCfg){.dst = joined, .val = 0, .bytes = sizeof(*joined)});
            joined->frame = s_bytes[place];
            for (unsigned int at = 0u; at < CLI_IPC_ENTRIES; at += 1u)
            {
                joined->entries[at].body =
                    joined->frame + CLI_IPC_FRAME + ((unsigned long long)at * CLI_IPC_ENTRY_BODY);
            }
            memor.set(&(MemoriaCfg){.dst = &joined->turnstiles, .val = 0, .bytes = sizeof(joined->turnstiles)});
            // the ring's slots, this place's run of the joined part of the rings block
            EmbedTicketRingSlot *const slots =
                (EmbedTicketRingSlot *)(void *)(EMBED_CALL(cli_rings.part, CliRingsPartCfg,
                                                           .part = CLI_RINGS_INTERFACE_JOINED) +
                                                ((unsigned long long)place * CLI_IPC_ENTRIES *
                                                 sizeof(EmbedTicketRingSlot)));
            memor.set(&(MemoriaCfg){
                .dst = slots, .val = 0, .bytes = (unsigned long long)CLI_IPC_ENTRIES * sizeof(EmbedTicketRingSlot)});
            embed_ticket_ring_begin(&joined->ring, slots, CLI_IPC_ENTRIES_SHIFT);
            joined->answer = joined->frame + CLI_IPC_FRAME + ((unsigned long long)CLI_IPC_ENTRIES * CLI_IPC_ENTRY_BODY);
            joined->place = place;
            EMBED_ATOMIC_STORE(&joined->going, 2u);
            g_cli_ipc_served.joined[place] = joined;
        }
        if (joined == NULL)
        {
            static const char s_full[] = "Rthe interface has as many processes as it can";
            (void)EMBED_CALL(cli_ipc_end_internal.frame_write, CliIpcEndCtx, .end = &end, .bytes = s_full,
                             .size = sizeof(s_full) - 1u, .region = NULL);
            EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &end);
            continue;
        }
        joined->end = end;
        joined->has_peer = has_peer;
        joined->peer = peer;
        joined->queue_wake = (CliWake)CLI_WAKE_INIT;
        EMBED_ATOMIC_STORE(&joined->claimed, 0u);
        joined->taken = 0u;
        EMBED_ATOMIC_STORE(&joined->answer_waits, 0u);
        joined->listens = 0u;
        EMBED_ATOMIC_STORE(&joined->read_state, 1u);
        EMBED_ATOMIC_STORE(&joined->sending, 1u);
        joined->reader = (CliThread){serve_read, joined};
        joined->reporter = (CliThread){serve_drain, joined};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &joined->reporter))
        {
            EMBED_ATOMIC_STORE(&joined->sending, 0u);
            EMBED_CALL(serve_thread_end, InterfaceServeCtx, .joined = joined);
        }
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &joined->reader))
        {
            serve_read(joined);
        }
    }
}

int cli_ipc_open(const CliIpcAddressCfg *args)
{
    if (g_cli_ipc_served.open)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->address,
            .src = g_cli_ipc_served.address,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_ipc_served.address, .cap = CLI_POOL_BYTES}) + 1u});
        return 1;
    }
    if (!EMBED_CALL(cli_ipc_listener_address_internal.listening, CliIpcListenerAddressCtx, .listener = &s_listener,
                    .address = g_cli_ipc_served.address, .err_msg = args->err_msg))
    {
        return 0;
    }
    g_cli_ipc_served.accepter = (CliThread){serve_accept, NULL};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &g_cli_ipc_served.accepter))
    {
        mmgr_line_put(args->err_msg, "the interface's thread did not start");
        return 0;
    }
    g_cli_ipc_served.open = 1;
    memor.cpy(&(MemoriaCfg){
        .dst = args->address,
        .src = g_cli_ipc_served.address,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_ipc_served.address, .cap = CLI_POOL_BYTES}) + 1u});
    return 1;
}

int cli_ipc_listen_request(const CliIpcServeChannelCfg *args)
{
    const char *channel = args->channel;
    VerbaLinea *err_msg = args->err_msg;
    unsigned int listener = 0u;
    if (args->joined->listens == CLI_IPC_LISTENS)
    {
        mmgr_line_put(err_msg, "a process listens on at max 32 channels");
        return 0;
    }
    if (!EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = args->joined->key, .channel = channel,
                    .queued = 1, .recv = serve_recv, .context = args->joined, .listener = &listener,
                    .err_msg = err_msg))
    {
        return 0;
    }
    args->joined->listeners[args->joined->listens] = listener;
    args->joined->listens += 1u;
    return 1;
}

int cli_ipc_reject(const CliIpcServeErrorCfg *args)
{
    return EMBED_CALL(serve_reject, InterfaceServeCtx, .joined = args->joined, .why = args->why, .size = args->size);
}

int cli_ipc_answer(const CliIpcServeCfg *args)
{
    return EMBED_CALL(serve_answer, InterfaceServeCtx, .joined = args->joined, .bytes = args->bytes, .size = args->size);
}
