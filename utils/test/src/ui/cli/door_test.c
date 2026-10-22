// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door_test.c: the comms door's check: a process joins by its key and hears and speaks on its channels; a key given to
// another process or to no part is refused at the door; a held key is taken from any process and grants its own
// channels alone; a key given to a process is taken from every process under it; a body past the most a frame takes
// goes through the transfer layer each way; a frame past the most a frame takes is not read; and every place a process
// joined into is given back to the cellblock once its process is gone
#include "../../../../../ui/cli/src/comms/comms.h"
#include "../../../../../ui/cli/src/comms/door.h"
#include "../../../../../ui/cli/src/comms/door_internal.h"
#include "../../../../../ui/cli/src/comms/door_serve.h"
#include "cli_check.h"

// what a part's listener heard: the last channel, speaker and body, and the last body's size
typedef struct
{
    CliLock lock;
    char channel[CLI_NAME_LONGEST];
    char from[CLI_NAME_LONGEST];
    char body[256];
    unsigned long long size;
    unsigned int count;
} DoorHeard;

static char s_address[CLI_PATH_LONGEST];
static DoorHeard s_heard = {CLI_LOCK_INIT, "", "", "", 0ull, 0u};
static char s_big[CLI_DOOR_MOST + (CLI_DOOR_MOST / 5u) + 16u];
static char s_next[CLI_DOOR_MOST + (CLI_DOOR_MOST / 5u) + 16u];
static char s_view[1u << 16u];

// a message a part heard kept
static int door_heard(void *context, const char *channel, const char *from, const char *body, unsigned long long size)
{
    DoorHeard *const heard = context;
    cli_lock_take(&heard->lock);
    memor.cpy(&(MemoriaCfg){.dst = heard->channel,
                            .src = channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = heard->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned long long kept = (size < sizeof(heard->body) - 1u) ? size : sizeof(heard->body) - 1u;
    memor.cpy(&(MemoriaCfg){.dst = heard->body, .src = body, .bytes = kept});
    heard->body[kept] = '\0';
    heard->size = size;
    heard->count += 1u;
    cli_lock_give(&heard->lock);
    return 1;
}

// the count of messages heard, waited on up to `wait` milliseconds until it passes `count`
static unsigned int door_heard_past(unsigned int count, unsigned long long wait)
{
    const unsigned long long until = cli_clock_milliseconds() + wait;
    for (;;)
    {
        cli_lock_take(&s_heard.lock);
        const unsigned int now = s_heard.count;
        cli_lock_give(&s_heard.lock);
        if ((now > count) || (cli_clock_milliseconds() > until))
        {
            return now;
        }
        query_paused(20u);
    }
}

// a part registered: its key into `key`, which holds CLI_COMMS_KEY
static void door_part(const char *name, int has_process, unsigned int process, const char *const *listen,
                      unsigned int listens, const char *const *speak, unsigned int speaks, char *key)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_comms_register(name, has_process, process, listen, listens, speak, speaks, key, &said), name);
}

// the door joined with `key`: the client, NULL with why into `why`, which holds 256
static CliDoorClient *door_joined(const char *key, char *why)
{
    CliLine said = {why, 256u, 0ull};
    CliDoorClient *const client = cli_door_joined(s_address, key, &said);
    (void)cli_line_closed(&said);
    return client;
}

// a process joins by its key and hears and speaks on its channels
static void door_channels(void)
{
    static const char *const s_runs[1] = {"runs/*"};
    static const char *const s_notes[1] = {"runs/notes"};
    char here[CLI_COMMS_KEY];
    char runner[CLI_COMMS_KEY];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    door_part("tracker", 1, cli_process_own(), s_runs, 1u, s_notes, 1u, here);
    door_part("runner", 0, 0u, s_notes, 1u, s_runs, 1u, runner);
    unsigned int listener = 0u;
    (void)cli_comms_listen(runner, "runs/notes", 0, door_heard, &s_heard, &listener, &said);
    CliDoorClient *const client = door_joined(here, why);
    cli_check((client != NULL) && (cellul.eq(&(CatenaFinitaCfg){
                                      .src = cli_door_name(client), .other = "tracker", .cap = CLI_CELLS_POOL})),
              "a process joins by its key");
    if (client == NULL)
    {
        return;
    }
    cli_check(cli_door_listened(client, "runs/*", &said), "and listens on its channels");
    unsigned long long heard = 0ull;
    (void)cli_comms_say(runner, "runs/7", "{\"state\":\"live\"}", 16ull, &heard, &said);
    cli_check(heard == 1ull, "a message on its channel reaches it");
    char channel[CLI_NAME_LONGEST];
    char from[CLI_NAME_LONGEST];
    CliLine body = {s_next, sizeof(s_next), 0ull};
    cli_check(cli_door_next(client, 5000ull, channel, from, &body) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = channel, .other = "runs/7", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "runner", .cap = CLI_CELLS_POOL})) &&
                  (body.at == 16ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_next, .other = "{\"state\":\"live\"}", .bytes = 16u}) == 0),
              "and is heard with its speaker");
    // the tracker listens on runs/*, and so hears its own note beside the runner
    const unsigned int before = s_heard.count;
    cli_check(cli_door_said(client, "runs/notes", "\"seen\"", 6ull, &heard, &said) && (heard == 2ull),
              "a note is heard by both its listeners");
    (void)door_heard_past(before, 5000ull);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_heard.from, .other = "tracker", .cap = CLI_CELLS_POOL}),
              "the runner hears the tracker");
    body.at = 0ull;
    cli_check(cli_door_next(client, 5000ull, channel, from, &body) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = channel, .other = "runs/notes", .cap = CLI_CELLS_POOL})),
              "and the tracker its own note");
    said.at = 0ull;
    cli_check(
        !cli_door_said(client, "runs/7", "null", 4ull, &heard, &said) && (cli_line_closed(&said) > 0ull) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "may not speak on runs/7", .other_cap = CLI_CELLS_POOL}) !=
             NULL),
        "a part may not speak where its key does not let it");
    said.at = 0ull;
    cli_check(!cli_door_listened(client, "lsp/rust", &said) && (cli_line_closed(&said) > 0ull) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "may not listen on lsp/rust",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "nor listen");
    cli_door_left(client);
}

// a key given to another process or to no part is refused at the door
static void door_refused(void)
{
    static const char *const s_all[1] = {"*"};
    char elsewhere[CLI_COMMS_KEY];
    char window[CLI_COMMS_KEY];
    char why[256];
    door_part("someone else's", 1, cli_process_own() + 1u, s_all, 1u, s_all, 1u, elsewhere);
    cli_check((door_joined(elsewhere, why) == NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "given to another process",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a key given to another process is refused");
    cli_check((door_joined("not a key", why) == NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "no part's", .other_cap = CLI_CELLS_POOL}) != NULL),
              "a key of no part is refused");
    door_part("the window", 0, 0u, s_all, 1u, NULL, 0u, window);
    cli_check(
        (door_joined(window, why) == NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "given to no process", .other_cap = CLI_CELLS_POOL}) !=
             NULL),
        "a key of the container's own is refused");
}

// a kept key is taken from any process and grants its own channels alone
static void door_held(void)
{
    static const char *const s_command[1] = {"command"};
    static const char *const s_answer[1] = {"answer/*"};
    char window[CLI_COMMS_KEY];
    char kept[CLI_COMMS_KEY];
    char other[CLI_COMMS_KEY];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    door_part("window", 0, 0u, s_command, 1u, s_answer, 1u, window);
    unsigned int listener = 0u;
    (void)cli_comms_listen(window, "command", 0, door_heard, &s_heard, &listener, &said);
    cli_check(cli_comms_register_kept("command line", s_answer, 1u, s_command, 1u, kept, &said), "a kept key is made");
    CliDoorClient *const client = door_joined(kept, why);
    cli_check((client != NULL) && (cellul.eq(&(CatenaFinitaCfg){
                                      .src = cli_door_name(client), .other = "command line", .cap = CLI_CELLS_POOL})),
              "and taken from any process");
    if (client == NULL)
    {
        return;
    }
    (void)cli_door_listened(client, "answer/1", &said);
    unsigned long long heard = 0ull;
    const unsigned int before = s_heard.count;
    cli_check(cli_door_said(client, "command", "{\"id\":\"1\"}", 10ull, &heard, &said) && (heard == 1ull),
              "it speaks on its own channel");
    (void)door_heard_past(before, 5000ull);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_heard.body, .other = "{\"id\":\"1\"}", .cap = CLI_CELLS_POOL}),
              "and is heard there");
    (void)cli_comms_say(window, "answer/1", "{\"done\":true}", 13ull, &heard, &said);
    char channel[CLI_NAME_LONGEST];
    char from[CLI_NAME_LONGEST];
    CliLine body = {s_next, sizeof(s_next), 0ull};
    cli_check(cli_door_next(client, 5000ull, channel, from, &body) && (body.at == 13ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_next, .other = "{\"done\":true}", .bytes = 13u}) == 0),
              "and hears its answer");
    cli_check(!cli_door_said(client, "runs/1", "{}", 2ull, &heard, &said),
              "a channel the key does not name is refused");
    door_part("other", 0, 0u, NULL, 0u, s_command, 1u, other);
    cli_check(door_joined(other, why) == NULL, "a key not kept is taken from its own process alone");
    cli_door_left(client);
}

// a key given to a process is taken from every process under it
static void door_under(void)
{
    static const char *const s_runs[1] = {"runs/*"};
    static CliProcess s_processes[4096];
    const unsigned int count = cli_processes_listed(s_processes, 4096u);
    const unsigned int own = cli_process_own();
    unsigned int parent = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        parent = (s_processes[at].id == own) ? s_processes[at].parent : parent;
    }
    char shell[CLI_COMMS_KEY];
    char why[256];
    door_part("terminal", 0, 0u, s_runs, 1u, s_runs, 1u, shell);
    cli_comms_bind(shell, parent);
    CliDoorClient *const client = door_joined(shell, why);
    cli_check(
        (parent != 0u) && (client != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_door_name(client), .other = "terminal", .cap = CLI_CELLS_POOL})),
        "a key given to a process is taken from every process under it");
    if (client != NULL)
    {
        cli_door_left(client);
    }
}

// the bytes the channel `name` has carried through the transfer layer, as the view lists it
static unsigned long long door_moved(const char *name)
{
    CliLine out = {s_view, sizeof(s_view), 0ull};
    cli_comms_channels(&out);
    unsigned int channels = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_view, out.at, &channels);
    for (unsigned int at = 0u; cli_json_item(json, channels, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int channel = cli_json_item(json, channels, at);
        const unsigned int moved = cli_json_member(json, channel, "transferred");
        if ((cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(channel, "name"), .other = name, .cap = CLI_CELLS_POOL})) &&
            cli_json_is(json, moved, CFG_JSON_NUMBER))
        {
            unsigned long long value = 0ull;
            for (unsigned long long digit = 0ull; digit < json->node[moved].length; digit += 1ull)
            {
                value = value * 10ull + (unsigned long long)(json->text[json->node[moved].text + digit] - '0');
            }
            return value;
        }
    }
    return 0ull;
}

// a body past the most a frame takes goes through the transfer layer each way
static void door_big(void)
{
    static const char *const s_runs[1] = {"runs/*"};
    static const char *const s_all[1] = {"*"};
    char tracker[CLI_COMMS_KEY];
    char window[CLI_COMMS_KEY];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    door_part("tracker", 0, 0u, s_runs, 1u, s_runs, 1u, tracker);
    cli_comms_bind(tracker, cli_process_own());
    door_part("window", 0, 0u, s_all, 1u, s_all, 1u, window);
    unsigned int listener = 0u;
    (void)cli_comms_listen(window, "runs/big", 0, door_heard, &s_heard, &listener, &said);
    CliDoorClient *const client = door_joined(tracker, why);
    if (client == NULL)
    {
        cli_check(0, "the tracker joins");
        return;
    }
    (void)cli_door_listened(client, "runs/back", &said);
    CliLine big = {s_big, sizeof(s_big), 0ull};
    cli_line_character(&big, '"');
    for (unsigned int at = 0u; at < CLI_DOOR_MOST / 5u; at += 1u)
    {
        cli_line_text(&big, "orior ");
    }
    cli_line_character(&big, '"');
    cli_check(big.at > CLI_DOOR_MOST, "the body is past the most a frame takes");
    unsigned long long heard = 0ull;
    const unsigned int before = s_heard.count;
    cli_check(cli_door_said(client, "runs/big", s_big, big.at, &heard, &said) && (heard == 1ull), "it is said");
    // a system run in an emulator of another processor reads twenty mebibytes slowly
    (void)door_heard_past(before, 120000ull);
    cli_check(s_heard.size == big.at, "and heard whole");
    (void)cli_comms_say(window, "runs/back", s_big, big.at, &heard, &said);
    char channel[CLI_NAME_LONGEST];
    char from[CLI_NAME_LONGEST];
    CliLine body = {s_next, sizeof(s_next), 0ull};
    cli_check(cli_door_next(client, 120000ull, channel, from, &body) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = channel, .other = "runs/back", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "window", .cap = CLI_CELLS_POOL})) &&
                  (body.at == big.at),
              "and the other way");
    cli_check(cli_door_said(client, "runs/big", "\"small\"", 7ull, &heard, &said) && (heard == 1ull),
              "a short body goes in its frame");
    cli_check((door_moved("runs/big") == big.at) && (door_moved("runs/back") == big.at),
              "each long body is counted through the transfer layer");
    cli_door_left(client);
}

// the door's other end of a frame test, accepted on a thread of its own
static CliDoorEnd s_server;
static int s_accepted;

static void door_accept(void *context)
{
    CliDoorListener *const listener = context;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    int has_peer = 0;
    unsigned int peer = 0u;
    s_accepted = cli_door_accepted(listener, &s_server, &has_peer, &peer, &said);
}

// a frame past the most a frame takes is not read
static void door_frame(void)
{
    char address[CLI_PATH_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliDoorEnd client;
    memor.set(&(MemoriaCfg){.dst = &client, .val = 0, .bytes = sizeof(client)});
    static CliDoorListener s_listener;
    if (!cli_door_listening(&s_listener, address, &said))
    {
        cli_check(0, "a listener is made");
        return;
    }
    static CliThread s_thread = {door_accept, &s_listener};
    (void)cli_thread_started(&s_thread);
    cli_check(cli_door_connected(address, &client, &said), "an end joins the listener");
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (!s_accepted && (cli_clock_milliseconds() < until))
    {
        query_paused(20u);
    }
    unsigned char head[6];
    const unsigned long long most = CLI_DOOR_MOST + 1u;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        // a byte of the length, its low eight bits kept
        head[at] = (unsigned char)((most >> (8u * at)) & 0xffu);
    }
    memor.cpy(&(MemoriaCfg){.dst = head + 4, .src = "{}", .bytes = 2u});
    (void)cli_door_written(&client, head, sizeof(head), NULL);
    static char s_frame[64];
    unsigned long long size = 0ull;
    cli_check(cli_door_frame_read(&s_server, s_frame, sizeof(s_frame), &size, &said) == CLI_DOOR_FRAME_BROKEN,
              "a frame past the most a frame takes is not read");
    cli_door_closed(&client);
    cli_door_closed(&s_server);
}

// a frame written reads back as itself, over a second pair of ends
static void door_frame_back(void)
{
    char address[CLI_PATH_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliDoorEnd client;
    memor.set(&(MemoriaCfg){.dst = &client, .val = 0, .bytes = sizeof(client)});
    s_accepted = 0;
    static CliDoorListener s_listener;
    if (!cli_door_listening(&s_listener, address, &said))
    {
        return;
    }
    static CliThread s_thread = {door_accept, &s_listener};
    (void)cli_thread_started(&s_thread);
    (void)cli_door_connected(address, &client, &said);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (!s_accepted && (cli_clock_milliseconds() < until))
    {
        query_paused(20u);
    }
    (void)cli_door_frame_written(&client, "{\"a\":1}", 7ull, NULL);
    static char s_frame[64];
    unsigned long long size = 0ull;
    cli_check((cli_door_frame_read(&s_server, s_frame, sizeof(s_frame), &size, &said) == CLI_DOOR_FRAME_READ) &&
                  (size == 7ull) && (memor.cmp(&(MemoriaCfg){.src = s_frame, .other = "{\"a\":1}", .bytes = 7u}) == 0),
              "a frame written reads back as itself");
    cli_door_closed(&client);
    cli_door_closed(&s_server);
}

int main(void)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_door_open(s_address, &said), "the door opens");
    door_channels();
    door_refused();
    door_held();
    door_under();
    door_big();
    door_frame();
    door_frame_back();
    // a place's threads end after its process does; its cell is therefore waited for up to five seconds
    for (unsigned int waited = 0u; (g_cli_door_cells.holds != 0ull) && (waited < 250u); waited += 1u)
    {
        query_paused(20u);
    }
    cli_check((g_cli_door_cells.holds == 0ull) && (g_cli_door_cells.hw != 0ull),
              "every place is given back once its process is gone, the most the places kept marked");
    return cli_checked();
}
