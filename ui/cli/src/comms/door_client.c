// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_client.c: a process's end of the door: joined by its key, a thread reading what comes, each message on a channel
// it listens on queued and each answer stored for the one waiting on it, and what it requests written a frame at a
// time, the region a long body is in handed over with its frame
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "door_internal.h"

// the most clients in one process; the bytes of an answer stored; and the messages queued for one client
#define CLI_DOOR_CLIENTS 2u
#define CLI_DOOR_ANSWER (1u << 16u)
#define CLI_DOOR_RECV_QUEUE 64u

struct CliDoorClient
{
    int taken;
    CliDoorEnd end;
    char name[CLI_NAME_MAX_LEN];
    CliLock output;
    CliLock lock;
    CliWake wake;
    int reading;
    char answer[CLI_DOOR_ANSWER];
    unsigned long long answer_size;
    int answered;
    char frame[CLI_DOOR_FRAME];
    CliDoorSlot recv_queue[CLI_DOOR_RECV_QUEUE];
    unsigned int first;
    unsigned int queued;
    CliThread reader;
};

static CliDoorClient s_clients[CLI_DOOR_CLIENTS];
static CliLock s_clients_lock = CLI_LOCK_INIT;

// a string member of the object of `size` bytes at `text` into `out`, sized for `room`, empty where it is none
static void client_string(const char *text, unsigned long long size, const char *name, char *out,
                          unsigned long long room)
{
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    out[0] = '\0';
    if (cli_lsp_member(text, size, name, &start, &length))
    {
        (void)cli_lsp_string(text + start, length, out, room);
    }
}

// the message of `size` bytes at `message` queued, its body in its slot or a region: a message past the queue's room
// or whose body contains no JSON is passed over
static void client_recv(CliDoorClient *client, const char *message, unsigned long long size)
{
    cli_lock_take(&client->lock);
    const int room = (client->queued < CLI_DOOR_RECV_QUEUE);
    cli_lock_give(&client->lock);
    if (!room)
    {
        return;
    }
    // the slot past the last queued is the reader's alone until it is counted
    CliDoorSlot *const slot = &client->recv_queue[(client->first + client->queued) % CLI_DOOR_RECV_QUEUE];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    client_string(message, size, "channel", slot->channel, sizeof(slot->channel));
    client_string(message, size, "from", slot->from, sizeof(slot->from));
    slot->has_region = 0;
    if (cli_lsp_member(message, size, "transfer", &start, &length))
    {
        unsigned long long region_length = 0ull;
        unsigned long long handle = 0ull;
        VerbaLinea none = {NULL, 0ull, 0ull};
        if (!cli_door_transfer(message + start, length, &region_length, &handle, &err_msg) ||
            !cli_door_handed_over(&client->end, region_length, handle, 0, 0u, &slot->region, &err_msg))
        {
            return;
        }
        if (!cli_json_compacted((const char *)slot->region.view, slot->region.length, &none))
        {
            cli_region_closed(&slot->region);
            return;
        }
        slot->has_region = 1;
        slot->size = slot->region.length;
    }
    else
    {
        const int has_body = cli_lsp_member(message, size, "body", &start, &length);
        const char *const body = has_body ? message + start : "null";
        const unsigned long long body_size = has_body ? length : 4ull;
        if ((body_size >= sizeof(slot->body)) && !cli_region_with(&slot->region, body, body_size, &err_msg))
        {
            return;
        }
        slot->has_region = (body_size >= sizeof(slot->body));
        if (!slot->has_region)
        {
            memor.cpy(&(MemoriaCfg){.dst = slot->body, .src = body, .bytes = (size_t)body_size});
        }
        slot->size = body_size;
    }
    cli_lock_take(&client->lock);
    client->queued += 1u;
    cli_wake_all(&client->wake);
    cli_lock_give(&client->lock);
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
        const int read = cli_door_frame_read(&client->end, client->frame, sizeof(client->frame), &size, &err_msg);
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
        if (cli_lsp_member(client->frame, size, "message", &start, &length))
        {
            client_recv(client, client->frame + start, length);
            continue;
        }
        cli_lock_take(&client->lock);
        const unsigned long long pending = (size < sizeof(client->answer)) ? size : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = client->answer, .src = client->frame, .bytes = (size_t)pending});
        client->answer_size = pending;
        client->answered = 1;
        cli_wake_all(&client->wake);
        cli_lock_give(&client->lock);
    }
    cli_lock_take(&client->lock);
    client->reading = 0;
    cli_wake_all(&client->wake);
    cli_lock_give(&client->lock);
}

// `size` bytes of `request` written, the region handed over with it where one is given, and the answer waited for: ten
// seconds, and two more for each mebibyte the region contains, as the container reads it whole first. How many
// received, where the answer reports, into `delivered`, read while no other request can take the answer's room: 1, or 0
// with why given, a refusal stating why
static int client_request(CliDoorClient *client, const char *request, unsigned long long size, const CliRegion *region,
                          unsigned long long *delivered, VerbaLinea *err_msg)
{
    cli_lock_take(&client->output);
    cli_lock_take(&client->lock);
    client->answered = 0;
    cli_lock_give(&client->lock);
    if (!cli_door_frame_written(&client->end, request, size, region))
    {
        cli_lock_give(&client->output);
        mmgr_line_put(err_msg, "the door's end closed");
        return 0;
    }
    const unsigned long long wait = 10000ull + (2000ull * ((region != NULL) ? (region->length >> 20u) : 0ull));
    const unsigned long long until = cli_clock_milliseconds() + wait;
    cli_lock_take(&client->lock);
    while (!client->answered && client->reading && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&client->wake, &client->lock, until - cli_clock_milliseconds());
    }
    const int answered = client->answered;
    cli_lock_give(&client->lock);
    if (!answered)
    {
        cli_lock_give(&client->output);
        mmgr_line_put(err_msg, "the container gave no answer");
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char why[CLI_ERR_MSG_MAX_LEN];
    if (cli_lsp_member(client->answer, client->answer_size, "refused", &start, &length) &&
        cli_lsp_string(client->answer + start, length, why, sizeof(why)))
    {
        cli_lock_give(&client->output);
        mmgr_line_put(err_msg, why);
        return 0;
    }
    if ((delivered != NULL) && cli_lsp_member(client->answer, client->answer_size, "delivered", &start, &length))
    {
        *delivered = 0ull;
        for (unsigned long long at = start;
             (at < start + length) && (client->answer[at] >= '0') && (client->answer[at] <= '9'); at += 1ull)
        {
            *delivered = (*delivered * 10ull) + (unsigned long long)(client->answer[at] - '0');
        }
    }
    cli_lock_give(&client->output);
    return 1;
}

CliDoorClient *cli_door_joined(const char *address, const char *key, VerbaLinea *err_msg)
{
    CliDoorClient *client = NULL;
    cli_lock_take(&s_clients_lock);
    for (unsigned int at = 0u; (at < CLI_DOOR_CLIENTS) && (client == NULL); at += 1u)
    {
        client = s_clients[at].taken ? NULL : &s_clients[at];
    }
    if (client != NULL)
    {
        client->taken = 1;
    }
    cli_lock_give(&s_clients_lock);
    if (client == NULL)
    {
        mmgr_line_put(err_msg, "this process has as many ends of the door as it can");
        return NULL;
    }
    client->output = (CliLock)CLI_LOCK_INIT;
    client->lock = (CliLock)CLI_LOCK_INIT;
    client->wake = (CliWake)CLI_WAKE_INIT;
    client->first = 0u;
    client->queued = 0u;
    client->answered = 0;
    client->reading = 1;
    char hello[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea request = {hello, sizeof(hello), 0ull};
    mmgr_line_put(&request, "{\"key\":");
    cli_json_quoted_lossy(&request, (const unsigned char *)key,
                          cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&request, '}');
    unsigned long long size = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    int joined = cli_door_connected(address, &client->end, err_msg) &&
                 cli_door_frame_written(&client->end, request.out, request.at, NULL) &&
                 (cli_door_frame_read(&client->end, client->frame, sizeof(client->frame), &size, err_msg) ==
                  CLI_DOOR_FRAME_READ);
    if (joined && cli_lsp_member(client->frame, size, "refused", &start, &length))
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        joined = !cli_lsp_string(client->frame + start, length, why, sizeof(why));
        mmgr_line_put(err_msg, joined ? "" : why);
    }
    client_string(client->frame, joined ? size : 0ull, "name", client->name, sizeof(client->name));
    client->reader = (CliThread){client_read, client};
    if (!joined || !cli_thread_started(&client->reader))
    {
        cli_door_closed(&client->end);
        cli_lock_take(&s_clients_lock);
        client->taken = 0;
        cli_lock_give(&s_clients_lock);
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
    if (!query_environment_read(CLI_DOOR_ADDRESS_VARIABLE, address, sizeof(address)) ||
        !query_environment_read(CLI_DOOR_KEY_VARIABLE, key, sizeof(key)))
    {
        return NULL;
    }
    return cli_door_joined(address, key, &err_msg);
}

const char *cli_door_name(const CliDoorClient *client)
{
    return client->name;
}

void cli_door_left(CliDoorClient *client)
{
    cli_door_shut(&client->end);
    cli_lock_take(&client->lock);
    while (client->reading)
    {
        cli_wake_wait(&client->wake, &client->lock);
    }
    cli_lock_give(&client->lock);
    for (unsigned int at = 0u; at < client->queued; at += 1u)
    {
        CliDoorSlot *const slot = &client->recv_queue[(client->first + at) % CLI_DOOR_RECV_QUEUE];
        if (slot->has_region)
        {
            cli_region_closed(&slot->region);
        }
    }
    cli_door_closed(&client->end);
    cli_lock_take(&s_clients_lock);
    client->taken = 0;
    cli_lock_give(&s_clients_lock);
}

int cli_door_listened(CliDoorClient *client, const char *channel, VerbaLinea *err_msg)
{
    char request[CLI_NAME_MAX_LEN + 64u];
    VerbaLinea out = {request, sizeof(request), 0ull};
    mmgr_line_put(&out, "{\"listen\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)channel,
                          cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&out, '}');
    return client_request(client, out.out, out.at, NULL, NULL, err_msg);
}

int cli_door_send(CliDoorClient *client, const char *channel, const char *body, unsigned long long size,
                  unsigned long long *delivered, VerbaLinea *err_msg)
{
    static char s_request[CLI_TRANSFER_SMALL + CLI_NAME_MAX_LEN * 2u + 64u];
    static CliLock s_request_lock = CLI_LOCK_INIT;
    CliRegion region;
    const int long_body = (size > CLI_TRANSFER_SMALL);
    if (long_body && !cli_region_with(&region, body, size, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_request_lock);
    VerbaLinea out = {s_request, sizeof(s_request), 0ull};
    mmgr_line_put(&out, "{\"send\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)channel,
                          cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, long_body ? ",\"transfer\":" : ",\"body\":");
    int request = 1;
    if (long_body)
    {
        request = cli_door_handed(&region, 0, 0u, &out, err_msg);
    }
    else
    {
        mmgr_line_put_n(&out, body, size);
    }
    mmgr_line_ch(&out, '}');
    *delivered = 0ull;
    request = request && client_request(client, out.out, out.at, long_body ? &region : NULL, delivered, err_msg);
    cli_lock_give(&s_request_lock);
    if (long_body)
    {
        cli_region_closed(&region);
    }
    return request;
}

int cli_door_next(CliDoorClient *client, unsigned long long wait, char *channel, char *from, VerbaLinea *body)
{
    const unsigned long long until = cli_clock_milliseconds() + wait;
    cli_lock_take(&client->lock);
    while ((client->queued == 0u) && client->reading && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&client->wake, &client->lock, until - cli_clock_milliseconds());
    }
    const int has = (client->queued > 0u);
    CliDoorSlot *const slot = &client->recv_queue[client->first];
    cli_lock_give(&client->lock);
    if (!has)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = channel,
                            .src = slot->channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = slot->channel, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = from,
                            .src = slot->from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = slot->from, .cap = CLI_CELLS_POOL}) + 1u});
    mmgr_line_put_n(body, slot->has_region ? (const char *)slot->region.view : slot->body, slot->size);
    if (slot->has_region)
    {
        cli_region_closed(&slot->region);
    }
    cli_lock_take(&client->lock);
    client->first = (client->first + 1u) % CLI_DOOR_RECV_QUEUE;
    client->queued -= 1u;
    cli_lock_give(&client->lock);
    return 1;
}
