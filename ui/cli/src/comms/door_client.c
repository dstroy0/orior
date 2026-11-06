// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_client.c: a process's end of the door: joined by its key, a thread reading what comes, each message on a channel
// it listens on queued and each answer stored for the one waiting on it, and what it requests written a frame at a
// time, the region a long body is in handed over with its frame
#include "../text/text.h"
#include "door_internal.h"

#define CLI_IPC_ANSWER (1u << 16u)
#define CLI_IPC_RECV_QUEUE 64u
struct CliIpcClient
{
    int reserved;
    CliIpcEnd end;
    char name[CLI_NAME_MAX_LEN];
    CliWake wake;
    embed_atomic_u32 read_state;
    char *answer;
    unsigned long long answer_size;
    embed_atomic_u32 answered;
    char *frame;
    CliIpcEntry recv_queue[CLI_IPC_RECV_QUEUE];
    unsigned int first;
    embed_atomic_u32 queued;
    CliThread reader;
};
// the most clients in one process; the bytes of an answer stored; and the messages queued for one client
#define CLI_IPC_CLIENTS 2u

static CliIpcClient s_clients[CLI_IPC_CLIENTS];

typedef struct
{
    unsigned long long size;
    CliIpcClient *client;
    const char *message;
    const char *request;
    const CliRegion *region;
    unsigned long long *delivered;
    VerbaLinea *err_msg;
} DoorClientCtx;

// the message of `size` bytes at `message` queued, its body in its entry or a region: a message past the queue's
// capacity or not whole is passed over
EMBED_INLINE void client_recv(const DoorClientCtx *args)
{
    unsigned long long size = args->size;
    const unsigned long long channel_end = cli_ipc_field_end(args->message, size, 0ULL);
    const unsigned long long from_end =
        (channel_end != 0ULL) ? cli_ipc_field_end(args->message, size, channel_end) : 0ULL;
    const unsigned int queued = EMBED_ATOMIC_LOAD(&args->client->queued);
    if ((queued >= CLI_IPC_RECV_QUEUE) || (from_end == 0ULL) || (from_end >= size))
    {
        return;
    }
    // the entry past the last queued is the reader's alone until it is counted
    CliIpcEntry *const entry = &args->client->recv_queue[(args->client->first + queued) % CLI_IPC_RECV_QUEUE];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    VerbaLinea channel = {entry->channel, sizeof(entry->channel), 0ULL};
    mmgr_line_put(&channel, args->message);
    (void)mmgr_line_finish(&channel);
    VerbaLinea from = {entry->from, sizeof(entry->from), 0ULL};
    mmgr_line_put(&from, args->message + channel_end);
    (void)mmgr_line_finish(&from);
    const char *const rest = args->message + from_end + 1ULL;
    const unsigned long long rest_size = size - from_end - 1ULL;
    entry->has_region = 0;
    if (args->message[from_end] == CLI_IPC_BODY_TRANSFER)
    {
        unsigned long long region_length = 0ULL;
        unsigned long long handle = 0ULL;
        if (!EMBED_CALL(cli_ipc_err_msg_size_internal.transfer, CliIpcErrMsgSizeCtx, .transfer = rest,
                        .size = rest_size, .length_pointer = &region_length, .handle_pointer = &handle,
                        .err_msg = &err_msg) ||
            !EMBED_CALL(cli_ipc_err_msg_internal.transferred_over, CliIpcErrMsgCtx, .end = &args->client->end,
                        .length = region_length, .handle = handle, .has_from = 0, .from = 0u,
                        .region_out = &entry->region, .err_msg = &err_msg))
        {
            return;
        }
        entry->has_region = 1;
        entry->size = entry->region.length;
    }
    else
    {
        if ((rest_size >= CLI_IPC_ENTRY_BODY) &&
            !EMBED_CALL(cli_transfer_region_bytes.region_with, CliTransferRegionBytesCfg, .region = &entry->region,
                        .bytes = rest, .size = rest_size, .err_msg = &err_msg))
        {
            return;
        }
        entry->has_region = (rest_size >= CLI_IPC_ENTRY_BODY);
        if (!entry->has_region)
        {
            memor.cpy(&(MemoriaCfg){.dst = entry->body, .src = rest, .bytes = (size_t)rest_size});
        }
        entry->size = rest_size;
    }
    (void)EMBED_ATOMIC_ADD_32(&args->client->queued, 1u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->client->wake);
}

// what comes read until the end breaks: each message queued, and each other frame stored as the answer waited on
static void client_read(void *context)
{
    CliIpcClient *const client = (CliIpcClient *)context;
    for (;;)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        unsigned long long size = 0ULL;
        const int read =
            EMBED_CALL(cli_ipc_err_msg_end_internal.frame_read, CliIpcErrMsgEndCtx, .end = &client->end,
                       .frame = client->frame, .capacity = CLI_IPC_FRAME, .size_pointer = &size, .err_msg = &err_msg);
        if (read == CLI_IPC_FRAME_BROKEN)
        {
            break;
        }
        if (read == CLI_IPC_FRAME_PASSED)
        {
            continue;
        }
        if ((size != 0ULL) && (client->frame[0] == CLI_IPC_KIND_MESSAGE))
        {
            EMBED_CALL(client_recv, DoorClientCtx, .client = client, .message = client->frame + 1, .size = size - 1ULL);
            continue;
        }
        const unsigned long long pending = (size < CLI_IPC_ANSWER) ? size : 0ULL;
        memor.cpy(&(MemoriaCfg){.dst = client->answer, .src = client->frame, .bytes = (size_t)pending});
        client->answer_size = pending;
        EMBED_ATOMIC_STORE(&client->answered, 1u);
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &client->wake);
    }
    EMBED_ATOMIC_STORE(&client->read_state, 0u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &client->wake);
}

// `size` bytes of `request` written, the region handed over with it where one is given, and the answer waited for: ten
// seconds, and two more for each mebibyte the region contains, as the container reads it whole first. How many
// received, where the answer reports, into `delivered`, read while no other request can take the answer's capacity: 1,
// or 0 with why given, a refusal stating why
static int client_request(const DoorClientCtx *args)
{
    unsigned long long size = args->size;
    const CliRegion *region = args->region;
    unsigned long long *delivered = args->delivered;
    EMBED_ATOMIC_STORE(&args->client->answered, 0u);
    if (!EMBED_CALL(cli_ipc_end_internal.frame_write, CliIpcEndCtx, .end = &args->client->end, .bytes = args->request,
                    .size = size, .region = region))
    {
        mmgr_line_put(args->err_msg, "the door's end closed");
        return 0;
    }
    const unsigned long long wait = 10000ULL + (2000ULL * ((region != NULL) ? (region->length >> 20u) : 0ULL));
    const unsigned long long until = cli_clock_milliseconds() + wait;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &args->client->wake);
        const unsigned long long now = cli_clock_milliseconds();
        if ((EMBED_ATOMIC_LOAD(&args->client->answered) != 0u) ||
            (EMBED_ATOMIC_LOAD(&args->client->read_state) == 0u) || (now >= until))
        {
            break;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_seen_for, CliWakeCtx, .wake = &args->client->wake, .seen = seen,
                         .milliseconds_u64 = until - now);
    }
    const int answered = EMBED_ATOMIC_LOAD(&args->client->answered) != 0u;
    if (!answered)
    {
        mmgr_line_put(args->err_msg, "the container gave no answer");
        return 0;
    }
    if ((args->client->answer_size != 0ULL) && (args->client->answer[0] == CLI_IPC_KIND_REFUSED))
    {
        mmgr_line_put_n(args->err_msg, args->client->answer + 1, args->client->answer_size - 1ULL);
        return 0;
    }
    if ((delivered != NULL) && (args->client->answer_size > 1ULL))
    {
        *delivered = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->client->answer, .at = 1ULL,
                                .size = args->client->answer_size, .end = NULL);
    }
    return 1;
}

CliIpcClient *cli_ipc_join(const CliIpcErrMsgAddressCfg *args)
{
    CliIpcClient *client = NULL;
    {
        unsigned int at = 0u;
        while ((at < CLI_IPC_CLIENTS) && (client == NULL))
        {
            client = s_clients[at].reserved ? NULL : &s_clients[at];
            at += 1u;
        }
    }
    if (client != NULL)
    {
        client->reserved = 1;
    }
    if (client == NULL)
    {
        mmgr_line_put(args->err_msg, "this process has as many ends of the door as it can");
        return NULL;
    }
    // an end's answer, frame and queued bodies stand in one cell, for as long as the end does
    client->answer =
        g_cli_guard->persistent_buf_alloc((unsigned long long)CLI_IPC_ANSWER + CLI_IPC_FRAME +
                                              ((unsigned long long)CLI_IPC_RECV_QUEUE * CLI_IPC_ENTRY_BODY),
                                          g_cli_ipc_owner);
    if (client->answer == NULL)
    {
        client->reserved = 0;
        mmgr_line_put(args->err_msg, "the cells cannot meet an end of the door");
        return NULL;
    }
    client->frame = client->answer + CLI_IPC_ANSWER;
    for (unsigned int at = 0u; at < CLI_IPC_RECV_QUEUE; at += 1u)
    {
        client->recv_queue[at].body = client->frame + CLI_IPC_FRAME + ((unsigned long long)at * CLI_IPC_ENTRY_BODY);
    }
    client->wake = (CliWake)CLI_WAKE_INIT;
    client->first = 0u;
    EMBED_ATOMIC_STORE(&client->queued, 0u);
    EMBED_ATOMIC_STORE(&client->answered, 0u);
    EMBED_ATOMIC_STORE(&client->read_state, 1u);
    char hello[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea request = {hello, sizeof(hello), 0ULL};
    mmgr_line_ch(&request, CLI_IPC_KIND_JOIN);
    mmgr_line_put(&request, args->key);
    mmgr_line_ch(&request, '\0');
    unsigned long long size = 0ULL;
    int joined = EMBED_CALL(cli_ipc_internal.connect, CliIpcCtx, .address_read = args->address_read,
                            .end = &client->end, .err_msg = args->err_msg) &&
                 EMBED_CALL(cli_ipc_end_internal.frame_write, CliIpcEndCtx, .end = &client->end, .bytes = request.out,
                            .size = request.at, .region = NULL) &&
                 (EMBED_CALL(cli_ipc_err_msg_end_internal.frame_read, CliIpcErrMsgEndCtx, .end = &client->end,
                             .frame = client->frame, .capacity = CLI_IPC_FRAME, .size_pointer = &size,
                             .err_msg = args->err_msg) == CLI_IPC_FRAME_READ) &&
                 (size != 0ULL);
    if (joined && (client->frame[0] == CLI_IPC_KIND_REFUSED))
    {
        mmgr_line_put_n(args->err_msg, client->frame + 1, size - 1ULL);
        joined = 0;
    }
    VerbaLinea named = {client->name, sizeof(client->name), 0ULL};
    mmgr_line_put_n(&named, client->frame + 1, joined ? (size - 1ULL) : 0ULL);
    (void)mmgr_line_finish(&named);
    client->reader = (CliThread){client_read, client};
    if (!joined || !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &client->reader))
    {
        EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &client->end);
        g_cli_guard->persistent_buf_release(client->answer, g_cli_ipc_owner);
        client->answer = NULL;
        client->reserved = 0;
        return NULL;
    }
    return client;
}

const char *cli_ipc_name(const CliIpcCfg *args)
{
    return args->client_read->name;
}

void cli_ipc_leave(const CliIpcClientCfg *args)
{
    EMBED_CALL(cli_ipc_internal.shut, CliIpcCtx, .end = &args->client->end);
    while (EMBED_ATOMIC_LOAD(&args->client->read_state) != 0u)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &args->client->wake);
    }
    const unsigned int queued = EMBED_ATOMIC_LOAD(&args->client->queued);
    for (unsigned int at = 0u; at < queued; at += 1u)
    {
        CliIpcEntry *const entry = &args->client->recv_queue[(args->client->first + at) % CLI_IPC_RECV_QUEUE];
        if (entry->has_region)
        {
            EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &entry->region);
        }
    }
    EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &args->client->end);
    g_cli_guard->persistent_buf_release(args->client->answer, g_cli_ipc_owner);
    args->client->answer = NULL;
    args->client->reserved = 0;
}

int cli_ipc_listen(const CliIpcErrMsgCfg *args)
{
    char request[CLI_NAME_MAX_LEN + 8u];
    VerbaLinea out = {request, sizeof(request), 0ULL};
    mmgr_line_ch(&out, CLI_IPC_KIND_LISTEN);
    mmgr_line_put(&out, args->channel);
    mmgr_line_ch(&out, '\0');
    return EMBED_CALL(client_request, DoorClientCtx, .client = args->client, .request = out.out, .size = out.at,
                      .region = NULL, .delivered = NULL, .err_msg = args->err_msg);
}

int cli_ipc_send(const CliIpcErrMsgCfg *args)
{
    unsigned long long *delivered = args->delivered;
    static char s_request[CLI_TRANSFER_SMALL + CLI_NAME_MAX_LEN * 2u + 64u];
    CliRegion region;
    const int long_body = (args->size > CLI_TRANSFER_SMALL);
    if (long_body && !EMBED_CALL(cli_transfer_region_bytes.region_with, CliTransferRegionBytesCfg, .region = &region,
                                 .bytes = args->body, .size = args->size, .err_msg = args->err_msg))
    {
        return 0;
    }
    VerbaLinea out = {s_request, sizeof(s_request), 0ULL};
    mmgr_line_ch(&out, CLI_IPC_KIND_SEND);
    mmgr_line_put(&out, args->channel);
    mmgr_line_ch(&out, '\0');
    mmgr_line_ch(&out, long_body ? CLI_IPC_BODY_TRANSFER : CLI_IPC_BODY_INLINE);
    int request = 1;
    if (long_body)
    {
        request = EMBED_CALL(cli_ipc_err_msg_region_internal.dispatch, CliIpcErrMsgRegionCtx, .region = &region,
                             .has_to = 0, .to = 0u, .out = &out, .err_msg = args->err_msg);
    }
    else
    {
        mmgr_line_put_n(&out, args->body, args->size);
    }
    *delivered = 0ULL;
    request =
        request && EMBED_CALL(client_request, DoorClientCtx, .client = args->client, .request = out.out, .size = out.at,
                              .region = long_body ? &region : NULL, .delivered = delivered, .err_msg = args->err_msg);
    if (long_body)
    {
        EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &region);
    }
    return request;
}

int cli_ipc_next(const CliIpcClientCfg *args)
{
    const unsigned long long until = cli_clock_milliseconds() + args->wait;
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &args->client->wake);
        const unsigned long long now = cli_clock_milliseconds();
        if ((EMBED_ATOMIC_LOAD(&args->client->queued) != 0u) || (EMBED_ATOMIC_LOAD(&args->client->read_state) == 0u) ||
            (now >= until))
        {
            break;
        }
        (void)EMBED_CALL(cli_wake_internal.wake_wait_seen_for, CliWakeCtx, .wake = &args->client->wake, .seen = seen,
                         .milliseconds_u64 = until - now);
    }
    const int has = (EMBED_ATOMIC_LOAD(&args->client->queued) > 0u);
    CliIpcEntry *const entry = &args->client->recv_queue[args->client->first];
    if (!has)
    {
        return 0;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = args->channel_out,
                      .src = entry->channel,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = entry->channel, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->from,
                            .src = entry->from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = entry->from, .cap = CLI_POOL_BYTES}) + 1u});
    mmgr_line_put_n(args->body_verba_linea_pointer, entry->has_region ? (const char *)entry->region.view : entry->body,
                    entry->size);
    if (entry->has_region)
    {
        EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &entry->region);
    }
    args->client->first = (args->client->first + 1u) % CLI_IPC_RECV_QUEUE;
    (void)EMBED_ATOMIC_ADD_32(&args->client->queued, (embed_u32)-1);
    return 1;
}
