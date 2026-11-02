// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_client.c: a process's end of the door: joined by its key, a thread reading what comes, each message on a channel
// it listens on queued and each answer stored for the one waiting on it, and what it requests written a frame at a
// time, the region a long body is in handed over with its frame
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "door_internal.h"

#define CLI_DOOR_ANSWER (1u << 16u)
#define CLI_DOOR_RECV_QUEUE 64u
struct CliDoorClient
{
    int taken;
    CliDoorEnd end;
    char name[CLI_NAME_MAX_LEN];
    CliWake wake;
    int reading;
    char answer[CLI_DOOR_ANSWER];
    unsigned long long answer_size;
    int answered;
    char frame[CLI_DOOR_FRAME];
    CliDoorEntry recv_queue[CLI_DOOR_RECV_QUEUE];
    unsigned int first;
    unsigned int queued;
    CliThread reader;
};
// the most clients in one process; the bytes of an answer stored; and the messages queued for one client
#define CLI_DOOR_CLIENTS 2u

static CliDoorClient s_clients[CLI_DOOR_CLIENTS];

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *name;
    char *out;
    unsigned long long room;
    CliDoorClient *client;
    const char *message;
    const char *request;
    const CliRegion *region;
    unsigned long long *delivered;
    VerbaLinea *err_msg;
} DoorClientCtx;
// a string member of the object of `size` bytes at `text` into `out`, sized for `room`, empty where it is none
EMBED_INLINE void client_string(const DoorClientCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *name = args->name;
    char *out = args->out;
    unsigned long long room = args->room;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    out[0] = '\0';
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = text, .size_value = size, .name = name, .start = &start,
                   .length_pointer = &length))
    {
        (void)EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = text + start, .length = length, .out_room = out,
                         .room = room);
    }
}

// the message of `size` bytes at `message` queued, its body in its entry or a region: a message past the queue's room
// or whose body contains no JSON is passed over
EMBED_INLINE void client_recv(const DoorClientCtx *args)
{
    unsigned long long size = args->size;
    const int room = (args->client->queued < CLI_DOOR_RECV_QUEUE);
    if (!room)
    {
        return;
    }
    // the entry past the last queued is the reader's alone until it is counted
    CliDoorEntry *const entry =
        &args->client->recv_queue[(args->client->first + args->client->queued) % CLI_DOOR_RECV_QUEUE];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    EMBED_CALL(client_string, DoorClientCtx, .text = args->message, .size = size, .name = "channel",
               .out = entry->channel, .room = sizeof(entry->channel));
    EMBED_CALL(client_string, DoorClientCtx, .text = args->message, .size = size, .name = "from", .out = entry->from,
               .room = sizeof(entry->from));
    entry->has_region = 0;
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message, .size_value = size, .name = "transfer",
                   .start = &start, .length_pointer = &length))
    {
        unsigned long long region_length = 0ull;
        unsigned long long handle = 0ull;
        VerbaLinea none = {NULL, 0ull, 0ull};
        if (!EMBED_CALL(cli_door_err_msg_size_internal.transfer, CliDoorErrMsgSizeCtx,
                        .transfer = args->message + start, .size = length, .length_pointer = &region_length,
                        .handle_pointer = &handle, .err_msg = &err_msg) ||
            !EMBED_CALL(cli_door_err_msg_internal.handed_over, CliDoorErrMsgCtx, .end = &args->client->end,
                        .length = region_length, .handle = handle, .has_from = 0, .from = 0u,
                        .region_out = &entry->region, .err_msg = &err_msg))
        {
            return;
        }
        if (!EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg, .text = (const char *)entry->region.view,
                        .size = entry->region.length, .out = &none))
        {
            EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &entry->region);
            return;
        }
        entry->has_region = 1;
        entry->size = entry->region.length;
    }
    else
    {
        const int has_body = EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->message, .size_value = size,
                                        .name = "body", .start = &start, .length_pointer = &length);
        const char *const body = has_body ? args->message + start : "null";
        const unsigned long long body_size = has_body ? length : 4ull;
        if ((body_size >= sizeof(entry->body)) &&
            !EMBED_CALL(cli_transfer_region_bytes.region_with, CliTransferRegionBytesCfg, .region = &entry->region,
                        .bytes = body, .size = body_size, .err_msg = &err_msg))
        {
            return;
        }
        entry->has_region = (body_size >= sizeof(entry->body));
        if (!entry->has_region)
        {
            memor.cpy(&(MemoriaCfg){.dst = entry->body, .src = body, .bytes = (size_t)body_size});
        }
        entry->size = body_size;
    }
    args->client->queued += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &args->client->wake);
}

// what comes read until the end breaks: each message queued, and each other frame stored as the answer waited on
static void client_read(void *context)
{
    CliDoorClient *const client = (CliDoorClient *)context;
    for (;;)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        unsigned long long size = 0ull;
        const int read = EMBED_CALL(cli_door_err_msg_end_internal.frame_read, CliDoorErrMsgEndCtx, .end = &client->end,
                                    .frame = client->frame, .room = sizeof(client->frame), .size_pointer = &size,
                                    .err_msg = &err_msg);
        if (read == CLI_DOOR_FRAME_BROKEN)
        {
            break;
        }
        if (read == CLI_DOOR_FRAME_PASSED)
        {
            continue;
        }
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = client->frame, .size_value = size, .name = "message",
                       .start = &start, .length_pointer = &length))
        {
            EMBED_CALL(client_recv, DoorClientCtx, .client = client, .message = client->frame + start, .size = length);
            continue;
        }
        const unsigned long long pending = (size < sizeof(client->answer)) ? size : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = client->answer, .src = client->frame, .bytes = (size_t)pending});
        client->answer_size = pending;
        client->answered = 1;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &client->wake);
    }
    client->reading = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &client->wake);
}

// `size` bytes of `request` written, the region handed over with it where one is given, and the answer waited for: ten
// seconds, and two more for each mebibyte the region contains, as the container reads it whole first. How many
// received, where the answer reports, into `delivered`, read while no other request can take the answer's room: 1, or 0
// with why given, a refusal stating why
static int client_request(const DoorClientCtx *args)
{
    unsigned long long size = args->size;
    const CliRegion *region = args->region;
    unsigned long long *delivered = args->delivered;
    args->client->answered = 0;
    if (!EMBED_CALL(cli_door_end_internal.frame_write, CliDoorEndCtx, .end = &args->client->end, .json = args->request,
                    .size = size, .region = region))
    {
        mmgr_line_put(args->err_msg, "the door's end closed");
        return 0;
    }
    const unsigned long long wait = 10000ull + (2000ull * ((region != NULL) ? (region->length >> 20u) : 0ull));
    const unsigned long long until = cli_clock_milliseconds() + wait;
    while (!args->client->answered && args->client->reading && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->client->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int answered = args->client->answered;
    if (!answered)
    {
        mmgr_line_put(args->err_msg, "the container gave no answer");
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char why[CLI_ERR_MSG_MAX_LEN];
    if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->client->answer,
                   .size_value = args->client->answer_size, .name = "refused", .start = &start,
                   .length_pointer = &length) &&
        EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = args->client->answer + start, .length = length,
                   .out_room = why, .room = sizeof(why)))
    {
        mmgr_line_put(args->err_msg, why);
        return 0;
    }
    if ((delivered != NULL) && EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = args->client->answer,
                                          .size_value = args->client->answer_size, .name = "delivered", .start = &start,
                                          .length_pointer = &length))
    {
        *delivered = 0ull;
        *delivered = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = args->client->answer, .at = start,
                                .size = start + length, .end = NULL);
    }
    return 1;
}

CliDoorClient *cli_door_join(const CliDoorErrMsgAddressCfg *args)
{
    CliDoorClient *client = NULL;
    for (unsigned int at = 0u; (at < CLI_DOOR_CLIENTS) && (client == NULL); at += 1u)
    {
        client = s_clients[at].taken ? NULL : &s_clients[at];
    }
    if (client != NULL)
    {
        client->taken = 1;
    }
    if (client == NULL)
    {
        mmgr_line_put(args->err_msg, "this process has as many ends of the door as it can");
        return NULL;
    }
    client->wake = (CliWake)CLI_WAKE_INIT;
    client->first = 0u;
    client->queued = 0u;
    client->answered = 0;
    client->reading = 1;
    char hello[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea request = {hello, sizeof(hello), 0ull};
    mmgr_line_put(&request, "{\"key\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &request,
               .text_bytes = (const unsigned char *)args->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&request, '}');
    unsigned long long size = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    int joined = EMBED_CALL(cli_door_internal.connect, CliDoorCtx, .address_read = args->address_read,
                            .end = &client->end, .err_msg = args->err_msg) &&
                 EMBED_CALL(cli_door_end_internal.frame_write, CliDoorEndCtx, .end = &client->end, .json = request.out,
                            .size = request.at, .region = NULL) &&
                 (EMBED_CALL(cli_door_err_msg_end_internal.frame_read, CliDoorErrMsgEndCtx, .end = &client->end,
                             .frame = client->frame, .room = sizeof(client->frame), .size_pointer = &size,
                             .err_msg = args->err_msg) == CLI_DOOR_FRAME_READ);
    if (joined && EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = client->frame, .size_value = size,
                             .name = "refused", .start = &start, .length_pointer = &length))
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        joined = !EMBED_CALL(cli_lsp_room.string, CliLspRoomCfg, .text = client->frame + start, .length = length,
                             .out_room = why, .room = sizeof(why));
        mmgr_line_put(args->err_msg, joined ? "" : why);
    }
    EMBED_CALL(client_string, DoorClientCtx, .text = client->frame, .size = joined ? size : 0ull, .name = "name",
               .out = client->name, .room = sizeof(client->name));
    client->reader = (CliThread){client_read, client};
    if (!joined || !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &client->reader))
    {
        EMBED_CALL(cli_door_internal.close, CliDoorCtx, .end = &client->end);
        client->taken = 0;
        return NULL;
    }
    return client;
}

CliDoorClient *cli_door_from_environment(void)
{
    char address[CLI_PATH_MAX_LEN];
    char key[CLI_NAME_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = CLI_DOOR_ADDRESS_VARIABLE,
                    .value_out = address, .room = sizeof(address)) ||
        !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = CLI_DOOR_KEY_VARIABLE,
                    .value_out = key, .room = sizeof(key)))
    {
        return NULL;
    }
    return EMBED_CALL(cli_door_err_msg_address.join, CliDoorErrMsgAddressCfg, .address_read = address, .key = key,
                      .err_msg = &err_msg);
}

const char *cli_door_name(const CliDoorCfg *args)
{
    return args->client_read->name;
}

void cli_door_leave(const CliDoorClientCfg *args)
{
    EMBED_CALL(cli_door_internal.shut, CliDoorCtx, .end = &args->client->end);
    while (args->client->reading)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &args->client->wake);
    }
    for (unsigned int at = 0u; at < args->client->queued; at += 1u)
    {
        CliDoorEntry *const entry = &args->client->recv_queue[(args->client->first + at) % CLI_DOOR_RECV_QUEUE];
        if (entry->has_region)
        {
            EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &entry->region);
        }
    }
    EMBED_CALL(cli_door_internal.close, CliDoorCtx, .end = &args->client->end);
    args->client->taken = 0;
}

int cli_door_listen(const CliDoorErrMsgCfg *args)
{
    char request[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea out = {request, sizeof(request), 0ull};
    mmgr_line_put(&out, "{\"listen\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->channel,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&out, '}');
    return EMBED_CALL(client_request, DoorClientCtx, .client = args->client, .request = out.out, .size = out.at,
                      .region = NULL, .delivered = NULL, .err_msg = args->err_msg);
}

int cli_door_send(const CliDoorErrMsgCfg *args)
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
    VerbaLinea out = {s_request, sizeof(s_request), 0ull};
    mmgr_line_put(&out, "{\"send\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->channel,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->channel, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, long_body ? ",\"transfer\":" : ",\"body\":");
    int request = 1;
    if (long_body)
    {
        request = EMBED_CALL(cli_door_err_msg_region_internal.dispatch, CliDoorErrMsgRegionCtx, .region = &region,
                             .has_to = 0, .to = 0u, .out = &out, .err_msg = args->err_msg);
    }
    else
    {
        mmgr_line_put_n(&out, args->body, args->size);
    }
    mmgr_line_ch(&out, '}');
    *delivered = 0ull;
    request =
        request && EMBED_CALL(client_request, DoorClientCtx, .client = args->client, .request = out.out, .size = out.at,
                              .region = long_body ? &region : NULL, .delivered = delivered, .err_msg = args->err_msg);
    if (long_body)
    {
        EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &region);
    }
    return request;
}

int cli_door_next(const CliDoorClientCfg *args)
{
    const unsigned long long until = cli_clock_milliseconds() + args->wait;
    while ((args->client->queued == 0u) && args->client->reading && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &args->client->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int has = (args->client->queued > 0u);
    CliDoorEntry *const entry = &args->client->recv_queue[args->client->first];
    if (!has)
    {
        return 0;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = args->channel_out,
                      .src = entry->channel,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = entry->channel, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->from,
                            .src = entry->from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = entry->from, .cap = CLI_CELLS_POOL}) + 1u});
    mmgr_line_put_n(args->body_verba_linea_pointer, entry->has_region ? (const char *)entry->region.view : entry->body,
                    entry->size);
    if (entry->has_region)
    {
        EMBED_CALL(cli_transfer_region_bytes.region_close, CliTransferRegionBytesCfg, .region = &entry->region);
    }
    args->client->first = (args->client->first + 1u) % CLI_DOOR_RECV_QUEUE;
    args->client->queued -= 1u;
    return 1;
}
