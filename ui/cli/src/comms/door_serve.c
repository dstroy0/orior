// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_serve.c: the door's container side: the processes that join, each in a place of its own with a thread reading
// what it requests and a thread sending it what it receives, the messages on its channels queued between them, a body
// past the capacity a message is queued in stored in a region of the transfer layer
#include "door_serve.h"
#include "../command_line/callback_cfg.h"

#include "container.h"

CliIpcServed g_cli_ipc_served = {0};

// the listener the door is joined through
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
} DoorServeCtx;
// an answer written to the process: 1, or 0 where its end is broken
EMBED_INLINE int serve_answer(const DoorServeCtx *args)
{
    return EMBED_CALL(cli_ipc_end_internal.frame_write, CliIpcEndCtx, .end = &args->joined->end, .bytes = args->bytes,
                      .size = args->size, .region = NULL);
}

// a refusal answered, why it was refused given
EMBED_INLINE int serve_reject(const DoorServeCtx *args)
{
    char answer[CLI_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea out = {answer, sizeof(answer), 0ull};
    mmgr_line_ch(&out, CLI_IPC_KIND_REFUSED);
    mmgr_line_put_n(&out, args->why, args->size);
    return EMBED_CALL(serve_answer, DoorServeCtx, .joined = args->joined, .bytes = out.out, .size = out.at);
}

// a message sent on a channel the process listens on queued for it, under the comms layer's lock: 1, 0 where the
// process is gone, -1 where its queue has no capacity
static int serve_recv(const CliCommsRecvCfg *args)
{
    unsigned long long size = args->size;
    CliIpcJoined *const joined = (CliIpcJoined *)args->context;
    if (!joined->read_state)
    {
        return 0;
    }
    if (joined->queued == CLI_IPC_ENTRIES)
    {
        return -1;
    }
    CliIpcEntry *const entry = &joined->entries[(joined->first + joined->queued) % CLI_IPC_ENTRIES];
    memor.cpy(&(MemoriaCfg){.dst = entry->channel,
                            .src = args->channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = entry->from,
                            .src = args->from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->from, .cap = CLI_CELLS_POOL}) + 1u});
    entry->size = size;
    entry->has_region = (size > CLI_IPC_ENTRY_BODY);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (entry->has_region &&
        !EMBED_CALL(cli_transfer_region_bytes.region_with, CliTransferRegionBytesCfg, .region = &entry->region,
                    .bytes = args->body, .size = size, .err_msg = &err_msg))
    {
        return -1;
    }
    if (!entry->has_region)
    {
        memor.cpy(&(MemoriaCfg){.dst = entry->body, .src = args->body, .bytes = (size_t)size});
    }
    joined->queued += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &joined->queue_wake);
    return 1;
}

// a queued message sent to the process, through the transfer layer where its body is long: 1, or 0 where it did not go
EMBED_INLINE int serve_send(const DoorServeCtx *args)
{
    char head[CLI_NAME_MAX_LEN * 3u + 512u];
    VerbaLinea prefix = {head, sizeof(head), 0ull};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int inline_body = (args->entry->size <= CLI_TRANSFER_SMALL);
    const char *const body = args->entry->has_region ? (const char *)args->entry->region.view : args->entry->body;
    mmgr_line_ch(&prefix, CLI_IPC_KIND_MESSAGE);
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
    const unsigned long long size = prefix.at + (inline_body ? args->entry->size : 0ull);
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
EMBED_INLINE void serve_thread_end(const DoorServeCtx *args)
{
    args->joined->going -= 1u;
    const int last = (args->joined->going == 0u);
    if (!last)
    {
        return;
    }
    EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &args->joined->end);
    g_cli_ipc_served.joined[args->joined->place] = NULL;
    g_cli_cells->persistent_buf_release(args->joined, &g_cli_ipc_cells);
}

// the messages queued sent to the process, one at a time, until the process is gone and its queue is empty; a message
// that does not go ends the sending, its channels let go
static void serve_drain(void *context)
{
    CliIpcJoined *const joined = (CliIpcJoined *)context;
    int sending = 1;
    for (;;)
    {
        while (joined->read_state && (joined->queued == 0u))
        {
            EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &joined->queue_wake);
        }
        if (joined->queued == 0u)
        {
            break;
        }
        CliIpcEntry *const entry = &joined->entries[joined->first];
        sending = sending && EMBED_CALL(serve_send, DoorServeCtx, .joined = joined, .entry = entry);
        if (entry->has_region)
        {
            EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &entry->region);
        }
        joined->first = (joined->first + 1u) % CLI_IPC_ENTRIES;
        joined->queued -= 1u;
        if (!sending)
        {
            joined->read_state = 0;
        }
        if (!sending)
        {
            EMBED_CALL(cli_ipc_internal.shut, CliIpcCtx, .end = &joined->end);
        }
    }
    EMBED_CALL(serve_thread_end, DoorServeCtx, .joined = joined);
}

// whether the process at the other end may take the key's part: a persistent part's key from any process, else only the
// process the key was given to or one it stands under; refused where it may not
EMBED_INLINE int serve_may_join(const DoorServeCtx *args)
{
    const char *key = args->key;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
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
        (void)EMBED_CALL(serve_reject, DoorServeCtx, .joined = args->joined, .why = err_msg.out, .size = err_msg.at);
        return 0;
    }
    unsigned int count = 0u;
    CliProcess *const parents =
        EMBED_CALL(cli_container.processes_alloc, CliContainerCfg, .owner = &g_cli_ipc_cells, .count_pointer = &count);
    const int under =
        args->joined->has_peer && ((args->joined->peer == supplied) ||
                                   EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = supplied,
                                              .id = args->joined->peer, .parents = parents, .count = count));
    g_cli_cells->persistent_buf_release(parents, &g_cli_ipc_cells);
    if (!under)
    {
        mmgr_line_put(&err_msg, "the key of ");
        mmgr_line_put(&err_msg, args->name);
        mmgr_line_put(&err_msg, " was given to another process");
        (void)EMBED_CALL(serve_reject, DoorServeCtx, .joined = args->joined, .why = err_msg.out, .size = err_msg.at);
    }
    return under;
}

// a joined process served: its first frame names its key, and each frame after requests to listen or to send, until its
// end breaks; its channels are let go and its queue drained as it goes
static void serve_read(void *context)
{
    CliIpcJoined *const joined = (CliIpcJoined *)context;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long size = 0ull;
    if (EMBED_CALL(cli_ipc_err_msg_end_internal.frame_read, CliIpcErrMsgEndCtx, .end = &joined->end,
                   .frame = joined->frame, .capacity = CLI_IPC_FRAME, .size_pointer = &size,
                   .err_msg = &err_msg) == CLI_IPC_FRAME_READ)
    {
        joined->key[0] = '\0';
        if ((size != 0ull) && (joined->frame[0] == CLI_IPC_KIND_JOIN) &&
            (cli_ipc_field_end(joined->frame, size, 1ull) != 0ull))
        {
            VerbaLinea key = {joined->key, sizeof(joined->key), 0ull};
            mmgr_line_put(&key, joined->frame + 1);
            (void)mmgr_line_finish(&key);
        }
        char name[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(cli_comms_key.name_get, CliCommsKeyCfg, .key = joined->key, .name_out = name))
        {
            (void)EMBED_CALL(serve_reject, DoorServeCtx, .joined = joined, .why = "that key is no part's",
                             .size = 21ull);
        }
        else if (EMBED_CALL(serve_may_join, DoorServeCtx, .joined = joined, .key = joined->key, .name = name))
        {
            char answer[CLI_NAME_MAX_LEN + 8u];
            VerbaLinea out = {answer, sizeof(answer), 0ull};
            mmgr_line_ch(&out, CLI_IPC_KIND_ANSWER);
            mmgr_line_put(&out, name);
            if (EMBED_CALL(serve_answer, DoorServeCtx, .joined = joined, .bytes = out.out, .size = out.at))
            {
                EMBED_CALL(cli_ipc_serve.ipc_requests_serve, CliIpcServeCfg, .joined = joined);
            }
        }
    }
    for (unsigned int at = 0u; at < joined->listens; at += 1u)
    {
        EMBED_CALL(cli_comms_listener.unlisten, CliCommsListenerCfg, .listener_value = joined->listeners[at]);
    }
    joined->read_state = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &joined->queue_wake);
    EMBED_CALL(serve_thread_end, DoorServeCtx, .joined = joined);
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
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        if (!EMBED_CALL(cli_ipc_listener_internal.accept, CliIpcListenerCtx, .listener = &s_listener, .end = &end,
                        .has_peer = &has_peer, .peer = &peer, .err_msg = &err_msg))
        {
            EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
            continue;
        }
        CliIpcJoined *joined = NULL;
        unsigned int place = CLI_IPC_JOINED;
        for (unsigned int at = 0u; (at < CLI_IPC_JOINED) && (place == CLI_IPC_JOINED); at += 1u)
        {
            place = (g_cli_ipc_served.joined[at] == NULL) ? at : place;
        }
        // a joined process's frame and queued bodies stand in its cell, after it
        joined = (place < CLI_IPC_JOINED)
                     ? g_cli_cells->persistent_buf_alloc(sizeof(CliIpcJoined) + CLI_IPC_FRAME +
                                                             ((unsigned long long)CLI_IPC_ENTRIES * CLI_IPC_ENTRY_BODY),
                                                         &g_cli_ipc_cells)
                     : NULL;
        if (joined != NULL)
        {
            joined->frame = (char *)(void *)(joined + 1);
            for (unsigned int at = 0u; at < CLI_IPC_ENTRIES; at += 1u)
            {
                joined->entries[at].body =
                    joined->frame + CLI_IPC_FRAME + ((unsigned long long)at * CLI_IPC_ENTRY_BODY);
            }
            joined->place = place;
            joined->going = 2u;
            g_cli_ipc_served.joined[place] = joined;
        }
        if (joined == NULL)
        {
            static const char s_full[] = "Rthe door has as many processes as it can";
            (void)EMBED_CALL(cli_ipc_end_internal.frame_write, CliIpcEndCtx, .end = &end, .bytes = s_full,
                             .size = sizeof(s_full) - 1u, .region = NULL);
            EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &end);
            continue;
        }
        joined->end = end;
        joined->has_peer = has_peer;
        joined->peer = peer;
        joined->queue_wake = (CliWake)CLI_WAKE_INIT;
        joined->first = 0u;
        joined->queued = 0u;
        joined->listens = 0u;
        joined->read_state = 1;
        joined->reader = (CliThread){serve_read, joined};
        joined->reporter = (CliThread){serve_drain, joined};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &joined->reporter))
        {
            EMBED_CALL(serve_thread_end, DoorServeCtx, .joined = joined);
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
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_ipc_served.address, .cap = CLI_CELLS_POOL}) + 1u});
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
        mmgr_line_put(args->err_msg, "the door's thread did not start");
        return 0;
    }
    g_cli_ipc_served.open = 1;
    memor.cpy(&(MemoriaCfg){
        .dst = args->address,
        .src = g_cli_ipc_served.address,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_ipc_served.address, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

int cli_ipc_listen_request(const CliIpcServeChannelCfg *args)
{
    const char *channel = args->channel;
    VerbaLinea *err_msg = args->err_msg;
    unsigned int listener = 0u;
    if (args->joined->listens == CLI_IPC_LISTENS)
    {
        mmgr_line_put(err_msg, "a process listens on at most 32 channels");
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
    return EMBED_CALL(serve_reject, DoorServeCtx, .joined = args->joined, .why = args->why, .size = args->size);
}

int cli_ipc_answer(const CliIpcServeCfg *args)
{
    return EMBED_CALL(serve_answer, DoorServeCtx, .joined = args->joined, .bytes = args->bytes, .size = args->size);
}
