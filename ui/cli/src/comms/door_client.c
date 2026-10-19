// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_client.c: a process's end of the door: joined by its key, a thread reading what comes, each message on a channel
// it listens on queued and each answer kept for the one waiting on it, and what it asks written a frame at a time, the
// region a long body is in handed over with its frame
#include "../servers/lsp.h"
#include "../text/json_tree.h"
#include "door_internal.h"

// the most clients in one process; the bytes of an answer kept; and the messages queued for one client
#define CLI_DOOR_CLIENTS 2u
#define CLI_DOOR_ANSWER (1u << 16u)
#define CLI_DOOR_HEARD 64u

struct CliDoorClient
{
    int taken;
    CliDoorEnd end;
    char name[CLI_NAME_LONGEST];
    CliLock output;
    CliLock lock;
    CliWake wake;
    int reading;
    char answer[CLI_DOOR_ANSWER];
    unsigned long long answer_size;
    int answered;
    char frame[CLI_DOOR_FRAME];
    CliDoorSlot heard[CLI_DOOR_HEARD];
    unsigned int first;
    unsigned int queued;
    CliThread reader;
};

static CliDoorClient s_clients[CLI_DOOR_CLIENTS];
static CliLock s_clients_lock = CLI_LOCK_INIT;

// a string member of the object of `size` bytes at `text` into `out`, which holds `room`, empty where it is none
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
// or whose body holds no JSON is passed over
static void client_heard(CliDoorClient *client, const char *message, unsigned long long size)
{
    cli_lock_take(&client->lock);
    const int room = (client->queued < CLI_DOOR_HEARD);
    cli_lock_give(&client->lock);
    if (!room)
    {
        return;
    }
    // the slot past the last queued is the reader's alone until it is counted
    CliDoorSlot *const slot = &client->heard[(client->first + client->queued) % CLI_DOOR_HEARD];
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    client_string(message, size, "channel", slot->channel, sizeof(slot->channel));
    client_string(message, size, "from", slot->from, sizeof(slot->from));
    slot->has_region = 0;
    if (cli_lsp_member(message, size, "transfer", &start, &length))
    {
        unsigned long long region_length = 0ull;
        unsigned long long handle = 0ull;
        CliLine none = {NULL, 0ull, 0ull};
        if (!cli_door_transfer(message + start, length, &region_length, &handle, &said) ||
            !cli_door_handed_over(&client->end, region_length, handle, 0, 0u, &slot->region, &said))
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
        if ((body_size >= sizeof(slot->body)) && !cli_region_with(&slot->region, body, body_size, &said))
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

// what comes read until the end breaks: each message queued, and each other frame kept as the answer waited on
static void client_read(void *context)
{
    CliDoorClient *const client = (CliDoorClient *)context;
    for (;;)
    {
        char why[CLI_SAID_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        unsigned long long size = 0ull;
        const int read = cli_door_frame_read(&client->end, client->frame, sizeof(client->frame), &size, &said);
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
            client_heard(client, client->frame + start, length);
            continue;
        }
        cli_lock_take(&client->lock);
        const unsigned long long kept = (size < sizeof(client->answer)) ? size : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = client->answer, .src = client->frame, .bytes = (size_t)kept});
        client->answer_size = kept;
        client->answered = 1;
        cli_wake_all(&client->wake);
        cli_lock_give(&client->lock);
    }
    cli_lock_take(&client->lock);
    client->reading = 0;
    cli_wake_all(&client->wake);
    cli_lock_give(&client->lock);
}

// `size` bytes of `asked` written, the region handed over with it where one is given, and the answer waited for: ten
// seconds, and two more for each mebibyte the region holds, as the container reads it whole first. How many heard,
// where the answer says, into `heard`, read while no other ask can take the answer's room: 1, or 0 with why said, a
// refusal saying why
static int client_ask(CliDoorClient *client, const char *asked, unsigned long long size, const CliRegion *region,
                      unsigned long long *heard, CliLine *said)
{
    cli_lock_take(&client->output);
    cli_lock_take(&client->lock);
    client->answered = 0;
    cli_lock_give(&client->lock);
    if (!cli_door_frame_written(&client->end, asked, size, region))
    {
        cli_lock_give(&client->output);
        cli_line_text(said, "the door's end closed");
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
        cli_line_text(said, "the container gave no answer");
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    char why[CLI_SAID_LONGEST];
    if (cli_lsp_member(client->answer, client->answer_size, "refused", &start, &length) &&
        cli_lsp_string(client->answer + start, length, why, sizeof(why)))
    {
        cli_lock_give(&client->output);
        cli_line_text(said, why);
        return 0;
    }
    if ((heard != NULL) && cli_lsp_member(client->answer, client->answer_size, "heard", &start, &length))
    {
        *heard = 0ull;
        for (unsigned long long at = start;
             (at < start + length) && (client->answer[at] >= '0') && (client->answer[at] <= '9'); at += 1ull)
        {
            *heard = (*heard * 10ull) + (unsigned long long)(client->answer[at] - '0');
        }
    }
    cli_lock_give(&client->output);
    return 1;
}

CliDoorClient *cli_door_joined(const char *address, const char *key, CliLine *said)
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
        cli_line_text(said, "this process holds as many ends of the door as it can");
        return NULL;
    }
    client->output = (CliLock)CLI_LOCK_INIT;
    client->lock = (CliLock)CLI_LOCK_INIT;
    client->wake = (CliWake)CLI_WAKE_INIT;
    client->first = 0u;
    client->queued = 0u;
    client->answered = 0;
    client->reading = 1;
    char hello[CLI_NAME_LONGEST * 2u];
    CliLine asked = {hello, sizeof(hello), 0ull};
    cli_line_text(&asked, "{\"key\":");
    cli_json_quoted_lossy(&asked, (const unsigned char *)key,
                          cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
    cli_line_character(&asked, '}');
    unsigned long long size = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    int joined =
        cli_door_connected(address, &client->end, said) &&
        cli_door_frame_written(&client->end, asked.out, asked.at, NULL) &&
        (cli_door_frame_read(&client->end, client->frame, sizeof(client->frame), &size, said) == CLI_DOOR_FRAME_READ);
    if (joined && cli_lsp_member(client->frame, size, "refused", &start, &length))
    {
        char why[CLI_SAID_LONGEST];
        joined = !cli_lsp_string(client->frame + start, length, why, sizeof(why));
        cli_line_text(said, joined ? "" : why);
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
    char address[CLI_PATH_LONGEST];
    char key[CLI_NAME_LONGEST];
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    if (!query_environment_read(CLI_DOOR_ADDRESS_VARIABLE, address, sizeof(address)) ||
        !query_environment_read(CLI_DOOR_KEY_VARIABLE, key, sizeof(key)))
    {
        return NULL;
    }
    return cli_door_joined(address, key, &said);
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
        CliDoorSlot *const slot = &client->heard[(client->first + at) % CLI_DOOR_HEARD];
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

int cli_door_listened(CliDoorClient *client, const char *channel, CliLine *said)
{
    char asked[CLI_NAME_LONGEST + 64u];
    CliLine out = {asked, sizeof(asked), 0ull};
    cli_line_text(&out, "{\"listen\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)channel,
                          cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}));
    cli_line_character(&out, '}');
    return client_ask(client, out.out, out.at, NULL, NULL, said);
}

int cli_door_said(CliDoorClient *client, const char *channel, const char *body, unsigned long long size,
                  unsigned long long *heard, CliLine *said)
{
    static char s_asked[CLI_TRANSFER_SMALL + CLI_NAME_LONGEST * 2u + 64u];
    static CliLock s_asked_lock = CLI_LOCK_INIT;
    CliRegion region;
    const int long_body = (size > CLI_TRANSFER_SMALL);
    if (long_body && !cli_region_with(&region, body, size, said))
    {
        return 0;
    }
    cli_lock_take(&s_asked_lock);
    CliLine out = {s_asked, sizeof(s_asked), 0ull};
    cli_line_text(&out, "{\"say\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)channel,
                          cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}));
    cli_line_text(&out, long_body ? ",\"transfer\":" : ",\"body\":");
    int asked = 1;
    if (long_body)
    {
        asked = cli_door_handed(&region, 0, 0u, &out, said);
    }
    else
    {
        cli_line_counted(&out, body, size);
    }
    cli_line_character(&out, '}');
    *heard = 0ull;
    asked = asked && client_ask(client, out.out, out.at, long_body ? &region : NULL, heard, said);
    cli_lock_give(&s_asked_lock);
    if (long_body)
    {
        cli_region_closed(&region);
    }
    return asked;
}

int cli_door_next(CliDoorClient *client, unsigned long long wait, char *channel, char *from, CliLine *body)
{
    const unsigned long long until = cli_clock_milliseconds() + wait;
    cli_lock_take(&client->lock);
    while ((client->queued == 0u) && client->reading && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&client->wake, &client->lock, until - cli_clock_milliseconds());
    }
    const int has = (client->queued > 0u);
    CliDoorSlot *const slot = &client->heard[client->first];
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
    cli_line_counted(body, slot->has_region ? (const char *)slot->region.view : slot->body, slot->size);
    if (slot->has_region)
    {
        cli_region_closed(&slot->region);
    }
    cli_lock_take(&client->lock);
    client->first = (client->first + 1u) % CLI_DOOR_HEARD;
    client->queued -= 1u;
    cli_lock_give(&client->lock);
    return 1;
}
