// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_test.c: the comms container's check: a grant names its channel and every channel under it; a key is a secret
// of the system's own and no two are alike; a listener hears what is said on its channels and nothing else; a channel
// the key does not name is refused to speaker and listener alike; a part grants what it starts only channels of its
// own; the container keeps each channel's traffic and its last messages; a listener by call hears in the speaker's
// thread in the order said; and a part that leaves hears nothing more and its key opens nothing. The checks share the
// one container a process keeps, each with parts and channels of its own
#include "../../../../../ui/cli/src/comms/comms.h"
#include "../../../../../ui/cli/src/comms/comms_internal.h"
#include "cli_check.h"

// what a listener heard: its count, and the last channel, speaker and body
typedef struct
{
    unsigned int count;
    char channel[CLI_NAME_LONGEST];
    char from[CLI_NAME_LONGEST];
    char body[256];
    char order[256];
} CommsHeard;

static char s_view[1u << 18u];

// a message heard kept, its channel put on the order heard
static int comms_heard(void *context, const char *channel, const char *from, const char *body, unsigned long long size)
{
    CommsHeard *const heard = context;
    heard->count += 1u;
    memor.cpy(&(MemoriaCfg){.dst = heard->channel,
                            .src = channel,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = heard->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned long long kept = (size < sizeof(heard->body) - 1u) ? size : sizeof(heard->body) - 1u;
    memor.cpy(&(MemoriaCfg){.dst = heard->body, .src = body, .bytes = kept});
    heard->body[kept] = '\0';
    const unsigned long long at = cellul.len(&(CatenaFinitaCfg){.src = heard->order, .cap = CLI_CELLS_POOL});
    CliLine order = {heard->order + at, sizeof(heard->order) - at, 0ull};
    cli_line_text(&order, (at > 0ull) ? "|" : "");
    cli_line_text(&order, channel);
    (void)cli_line_closed(&order);
    return 1;
}

// a part registered: 1 with its key into `key`, which holds CLI_COMMS_KEY, or 0 with why into `why`, which holds 256
static int comms_part(const char *name, int has_process, unsigned int process, const char *const *listen,
                      unsigned int listens, const char *const *speak, unsigned int speaks, char *key, char *why)
{
    CliLine said = {why, 256u, 0ull};
    const int made = cli_comms_register(name, has_process, process, listen, listens, speak, speaks, key, &said);
    (void)cli_line_closed(&said);
    return made;
}

// `body` said on `channel` as the part of `key`: how many heard it, -1 where it was refused with why into `why`
static long long comms_say(const char *key, const char *channel, const char *body, char *why)
{
    unsigned long long heard = 0ull;
    CliLine said = {why, 256u, 0ull};
    const int spoke = cli_comms_say(key, channel, body,
                                    cellul.len(&(CatenaFinitaCfg){.src = body, .cap = CLI_CELLS_POOL}), &heard, &said);
    (void)cli_line_closed(&said);
    return spoke ? (long long)heard : -1ll;
}

// `key`'s part listening on `channel`: 1, or 0 with why into `why`
static int comms_listen(const char *key, const char *channel, CommsHeard *heard, char *why)
{
    unsigned int listener = 0u;
    CliLine said = {why, 256u, 0ull};
    const int listening = cli_comms_listen(key, channel, 0, comms_heard, heard, &listener, &said);
    (void)cli_line_closed(&said);
    return listening;
}

// the item named `name` of a view, read into the shared tree: its node
static unsigned int comms_viewed(int parts, const char *name, const CliJson **json)
{
    CliLine out = {s_view, sizeof(s_view), 0ull};
    if (parts)
    {
        cli_comms_parts(&out);
    }
    else
    {
        cli_comms_channels(&out);
    }
    unsigned int list = CLI_JSON_NONE;
    *json = cli_check_parsed(s_view, out.at, &list);
    for (unsigned int at = 0u; cli_json_item(*json, list, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int item = cli_json_item(*json, list, at);
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = cli_check_member_text(item, "name"), .other = name, .cap = CLI_CELLS_POOL}))
        {
            return item;
        }
    }
    return CLI_JSON_NONE;
}

// a member of a node of the shared tree written compact into `out`, which holds 512
static const char *comms_written(const CliJson *json, unsigned int node, const char *name, char *out)
{
    CliLine line = {out, 512u, 0ull};
    cli_json_write(json, cli_json_member(json, node, name), &line, 0u, 0);
    (void)cli_line_closed(&line);
    return out;
}

// a grant names its channel and every channel under it
static void comms_grants(void)
{
    cli_check(cli_comms_grants("*", "lsp/rust") && cli_comms_grants("lsp/rust", "lsp/rust") &&
                  cli_comms_grants("lsp/*", "lsp/rust") && cli_comms_grants("lsp/*", "lsp/rust/diagnostics"),
              "a grant names its channel and every channel under it");
    cli_check(!cli_comms_grants("lsp/*", "lsp") && !cli_comms_grants("lsp/*", "lspx/rust") &&
                  !cli_comms_grants("lsp/rust", "lsp/python"),
              "and no other");
    cli_check(cli_comms_within("lsp/rust", "lsp/*") && cli_comms_within("lsp/rust/*", "lsp/*") &&
                  !cli_comms_within("lsp/*", "lsp/rust") && !cli_comms_within("*", "lsp/*") &&
                  cli_comms_within("anything/at/all", "*"),
              "a grant is within one that names every channel it names");
}

// a key is a secret of the system's own and no two are alike
static void comms_keys(void)
{
    char one[CLI_COMMS_KEY];
    char two[CLI_COMMS_KEY];
    char why[256];
    char name[CLI_NAME_LONGEST];
    (void)comms_part("one", 0, 0u, NULL, 0u, NULL, 0u, one, why);
    (void)comms_part("two", 0, 0u, NULL, 0u, NULL, 0u, two, why);
    cli_check((cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL}) == 64u) &&
                  (!cellul.eq(&(CatenaFinitaCfg){.src = one, .other = two, .cap = CLI_CELLS_POOL})),
              "a key is 64 hex digits and no two are alike");
    cli_check(cli_comms_name_of(two, name) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "two", .cap = CLI_CELLS_POOL})),
              "and names its part");
}

// a listener hears what is said on its channels alone, and a channel the key does not name is refused
static void comms_channels(void)
{
    static const char *const s_window_listens[2] = {"lsp/*", "runs"};
    static const char *const s_rust[1] = {"lsp/rust"};
    char window[CLI_COMMS_KEY];
    char server[CLI_COMMS_KEY];
    char why[256];
    static CommsHeard s_heard;
    (void)comms_part("window", 0, 0u, s_window_listens, 2u, NULL, 0u, window, why);
    (void)comms_part("rust server", 1, 42u, NULL, 0u, s_rust, 1u, server, why);
    (void)comms_listen(window, "lsp/*", &s_heard, why);
    cli_check(comms_say(server, "lsp/rust", "{\"n\":1}", why) == 1ll, "a listener hears what is said on its channels");
    unsigned int process = 0u;
    cli_check((cellul.eq(&(CatenaFinitaCfg){.src = s_heard.channel, .other = "lsp/rust", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_heard.from, .other = "rust server", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_heard.body, .other = "{\"n\":1}", .cap = CLI_CELLS_POOL})) &&
                  cli_comms_process_of(server, &process) && (process == 42u),
              "with its channel, its speaker and its body");
    cli_check(!comms_listen(window, "db/rows", &s_heard, why) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "may not listen on db/rows",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a channel the key does not name is refused to the listener");
    cli_check((comms_say(server, "db/rows", "null", why) == -1ll) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "may not speak on db/rows",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "and to the speaker");
    cli_check(comms_say("forged", "lsp/rust", "null", why) == -1ll, "a key of no part opens nothing");
    const CliJson *json = NULL;
    char text[512];
    const unsigned int rows = comms_viewed(0, "db/rows", &json);
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = comms_written(json, rows, "messages", text), .other = "0", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = comms_written(json, rows, "refused", text), .other = "1", .cap = CLI_CELLS_POOL})),
              "a channel counts what it refused");
    const unsigned int part = comms_viewed(1, "rust server", &json);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = comms_written(json, part, "refused", text), .other = "1", .cap = CLI_CELLS_POOL}),
              "and so does its speaker");
}

// a part grants what it starts only channels of its own
static void comms_under(void)
{
    static const char *const s_runs[1] = {"runs/*"};
    static const char *const s_run[1] = {"runs/7"};
    static const char *const s_rust[1] = {"lsp/rust"};
    static const char *const s_all[1] = {"*"};
    char runs[CLI_COMMS_KEY];
    char run[CLI_COMMS_KEY];
    char greedy[CLI_COMMS_KEY];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    (void)comms_part("runs", 0, 0u, s_runs, 1u, s_runs, 1u, runs, why);
    unsigned int process = 0u;
    cli_check(cli_comms_register_under(runs, "run 7", 1, 7u, s_run, 1u, s_run, 1u, run, &said) &&
                  cli_comms_process_of(run, &process) && (process == 7u),
              "a part starts another with channels of its own");
    said.at = 0ull;
    cli_check(
        !cli_comms_register_under(runs, "greedy", 0, 0u, s_rust, 1u, NULL, 0u, greedy, &said) &&
            (cli_line_closed(&said) > 0ull) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "may not grant lsp/rust", .other_cap = CLI_CELLS_POOL}) !=
             NULL),
        "and may not grant one it does not hold");
    cli_check(!cli_comms_register_under(runs, "greedy", 0, 0u, NULL, 0u, s_all, 1u, greedy, &said),
              "nor every channel");
}

// the container keeps each channel's traffic and its last messages, and a listener by call hears in the speaker's
// thread in the order said
static void comms_traffic(void)
{
    static const char *const s_all[1] = {"*"};
    static const char *const s_files[1] = {"files"};
    char window[CLI_COMMS_KEY];
    char server[CLI_COMMS_KEY];
    char why[256];
    static CommsHeard s_heard;
    (void)comms_part("files window", 0, 0u, s_all, 1u, NULL, 0u, window, why);
    (void)comms_part("files server", 0, 0u, NULL, 0u, s_files, 1u, server, why);
    (void)comms_listen(window, "files", &s_heard, why);
    for (unsigned int at = 0u; at < 25u; at += 1u)
    {
        char body[16];
        CliLine line = {body, sizeof(body), 0ull};
        cli_line_decimal(&line, at, 1u);
        (void)cli_line_closed(&line);
        (void)comms_say(server, "files", body, why);
    }
    const CliJson *json = NULL;
    char text[512];
    const unsigned int files = comms_viewed(0, "files", &json);
    const unsigned int last = cli_json_member(json, files, "last");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = comms_written(json, files, "messages", text), .other = "25", .cap = CLI_CELLS_POOL}),
              "a channel counts its messages");
    cli_check((json->node[last].count == CLI_COMMS_KEPT) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = comms_written(json, cli_json_item(json, last, 0u), "body", text),
                                                .other = "5",
                                                .cap = CLI_CELLS_POOL})),
              "and keeps the last of them, the oldest first");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = comms_written(json, files, "listeners", text),
                                           .other = "[\"files window\"]",
                                           .cap = CLI_CELLS_POOL}),
              "and its listeners");
    for (unsigned int at = 0u; at < CLI_COMMS_KEPT; at += 1u)
    {
        (void)comms_say(server, "files", "x", why);
    }
    const unsigned long long kept = g_cli_comms_cells.holds;
    for (unsigned int at = 0u; at < CLI_COMMS_KEPT; at += 1u)
    {
        (void)comms_say(server, "files", "x", why);
    }
    cli_check((kept != 0ull) && (g_cli_comms_cells.holds == kept),
              "each message kept is a cell of its own, the oldest given back as another is kept");
    char server_said[CLI_COMMS_KEY];
    char window_heard[CLI_COMMS_KEY];
    static CommsHeard s_order;
    (void)comms_part("order window", 0, 0u, s_all, 1u, NULL, 0u, window_heard, why);
    (void)comms_part("order server", 0, 0u, NULL, 0u, s_all, 1u, server_said, why);
    (void)comms_listen(window_heard, "run/*", &s_order, why);
    (void)comms_say(server_said, "run/started", "1", why);
    const unsigned int after_first = s_order.count;
    (void)comms_say(server_said, "run/line", "2", why);
    cli_check((after_first == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_order.order, .other = "run/started|run/line", .cap = CLI_CELLS_POOL})),
              "a listener by call hears in the speaker's call, in the order said");
}

// a part that leaves hears no more and its key opens nothing
static void comms_leave(void)
{
    static const char *const s_all[1] = {"*"};
    static const char *const s_files[1] = {"left"};
    char window[CLI_COMMS_KEY];
    char server[CLI_COMMS_KEY];
    char why[256];
    static CommsHeard s_heard;
    (void)comms_part("leaving window", 0, 0u, s_all, 1u, NULL, 0u, window, why);
    (void)comms_part("leaving server", 0, 0u, NULL, 0u, s_files, 1u, server, why);
    (void)comms_listen(window, "left", &s_heard, why);
    cli_comms_leave(window);
    cli_check((comms_say(server, "left", "1", why) == 0ll) && (s_heard.count == 0u),
              "a part that leaves hears no more");
    cli_check(!comms_listen(window, "left", &s_heard, why), "and its key opens nothing");
}

int main(void)
{
    comms_grants();
    comms_keys();
    comms_channels();
    comms_under();
    comms_traffic();
    comms_leave();
    return cli_checked();
}
