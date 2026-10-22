// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: what the window calls of orior: the command line run first, and the window it leaves to open kept; the
// window opened, its tree's side made and the link opened where the tree is on another machine; each call answered in
// a room of its own, one of those the calls answered at once share, a call waiting where every room is taken, the
// tree's over the link where one is open; a terminal's key to the comms layer; and every part ended as the window ends
#include "../command_line/cells.h"
#include "../command_line/command_line.h"
#include "../comms/comms.h"
#include "../comms/link.h"
#include "../machine/host.h"
#include "../machine/reach.h"
#include "serve_internal.h"
#include "window_internal.h"

// A room a call is answered in: whether it is taken, whether its call was answered, and its answer and why it failed,
// cells given back with the room, why it failed a text of this file's own where no cell could be taken for it
typedef struct
{
    int taken;
    int ok;
    char *answer;
    unsigned long long answer_size;
    char *said;
    unsigned long long said_size;
} CliWindowRoom;

// why a call failed where its cells could not be taken, which is no cell
static char s_refused[] = "the call's cells could not be taken";

static CliWindowRoom s_room[CLI_WINDOW_ROOMS];
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static CliOutcome s_outcome;

// One table of the window's calls and its count
typedef struct
{
    const CliServeCall *call;
    const unsigned int *count;
} CliWindowTable;

// a line of the errors, `orior: ` before what was said
static void window_err(const CliLine *said)
{
    static char s_line[CLI_WINDOW_SAID + 8u];
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, "orior: ");
    cli_line_counted(&line, said->out, cli_line_fits(said) ? said->at : 0ull);
    cli_line_character(&line, '\n');
    query_terminal_written(1, s_line, cli_line_fits(&line) ? line.at : 0ull);
}

int cli_window_command_line(const char *const *words, unsigned int count, int *window)
{
    if (count == 0u)
    {
        cli_console_let_go();
        memor.set(&(MemoriaCfg){.dst = &s_outcome, .val = 0, .bytes = sizeof(s_outcome)});
        s_outcome.window = 1;
        *window = 1;
        return 0;
    }
    cli_command_line_run(words, count, &s_outcome);
    *window = s_outcome.window;
    return s_outcome.code;
}

void cli_window_begun(CliServeEmit emit, void *context)
{
    static char s_why[CLI_WINDOW_SAID];
    CliLine said = {s_why, sizeof(s_why), 0ull};
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    if (cli_host_prepared(&said) == -1ll)
    {
        window_err(&said);
    }
    // every process the window starts stands under it, and ends with it
    (void)cli_container_held();
    static CliLinkAddress s_address;
    int remote = 0;
    said.at = 0ull;
    if (s_outcome.has_remote)
    {
        remote = cli_link_address_parse(s_outcome.remote, &s_address, &said);
        if (!remote)
        {
            window_err(&said);
        }
    }
    static char s_root[CLI_PATH_LONGEST];
    const char *root = NULL;
    if (!remote && s_outcome.has_root)
    {
        root = s_outcome.root;
    }
    else if (!remote && cli_tree_found(s_root))
    {
        root = s_root;
    }
    char home[CLI_PATH_LONGEST];
    char keeping[CLI_PATH_LONGEST];
    const int has_home = cli_home_folder(home);
    if (has_home)
    {
        cli_full(home, "runs", keeping);
    }
    cli_serve_begun(root, has_home ? keeping : NULL);
    cli_serve_tell_to(emit, context);
    // the door of the comms layer is open from the start, for every process the container starts to join
    char address[CLI_PATH_LONGEST];
    said.at = 0ull;
    (void)cli_serve_open_door(address, &said);
    said.at = 0ull;
    (void)cli_serve_open_to_command_line(&said);
    cli_serve_watch();
    said.at = 0ull;
    if (remote && !cli_link_open(&s_address, emit, context, &said))
    {
        window_err(&said);
    }
}

void cli_window_launch_taken(CliLine *out)
{
    cli_lock_take(&s_lock);
    const int has = s_outcome.window && (s_outcome.command[0] != '\0');
    if (has)
    {
        cli_line_text(out, "{\"root\":");
        if (s_outcome.has_root)
        {
            cli_window_quoted(out, s_outcome.root);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_text(out, ",\"remote\":");
        if (s_outcome.has_remote)
        {
            cli_window_quoted(out, s_outcome.remote);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_text(out, ",\"menu\":");
        cli_window_quoted(out, s_outcome.menu);
        cli_line_text(out, ",\"command\":");
        cli_window_quoted(out, s_outcome.command);
        cli_line_text(out, ",\"args\":[");
        for (unsigned int at = 0u; at < s_outcome.args; at += 1u)
        {
            cli_line_text(out, (at > 0u) ? "," : "");
            cli_window_quoted(out, cli_launch_arg(&s_outcome, at));
        }
        cli_line_text(out, "]}");
        s_outcome.command[0] = '\0';
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_lock_give(&s_lock);
}

void cli_window_ended(void)
{
    // a language server orior_cli started ends with it: on Windows a child outlives its parent
    cli_reach_forget();
    cli_serve_stop_all();
    CliLinkAddress address;
    if (cli_link_address(&address))
    {
        cli_link_end();
    }
    cli_container_members_ended();
}

// the window's own call `name`: its answer, or NULL where none answers it
static CliServeAnswer window_answer_of(const char *name)
{
    static const CliWindowTable s_tables[] = {
        {g_cli_window_home_calls, &g_cli_window_home_count}, {g_cli_window_places_calls, &g_cli_window_places_count},
        {g_cli_window_net_calls, &g_cli_window_net_count},   {g_cli_window_report_calls, &g_cli_window_report_count},
        {g_cli_window_text_calls, &g_cli_window_text_count}, {g_cli_window_code_calls, &g_cli_window_code_count},
        {g_cli_window_tree_calls, &g_cli_window_tree_count},
    };
    for (unsigned int table = 0u; table < sizeof(s_tables) / sizeof(s_tables[0]); table += 1u)
    {
        for (unsigned int at = 0u; at < *s_tables[table].count; at += 1u)
        {
            if (cellul.eq(
                    &(CatenaFinitaCfg){.src = s_tables[table].call[at].name, .other = name, .cap = CLI_CELLS_POOL}))
            {
                return s_tables[table].call[at].answer;
            }
        }
    }
    return NULL;
}

// `answer` given `size` bytes of its arguments, read into a tree in a cell sized to them and given back once it has
// answered: its answer into `out`, 1, or 0 with why said
int cli_window_tree_call(const char *name, const char *args, unsigned long long size, CliLine *out, CliLine *said)
{
    CliLinkAddress address;
    return cli_link_address(&address) ? cli_link_call(name, args, size, out, said)
                                      : cli_serve_call(name, args, size, out, said);
}

// a room taken, waited for where every one is: its place
static unsigned int window_room_taken(void)
{
    cli_lock_take(&s_lock);
    unsigned int at = CLI_WINDOW_ROOMS;
    while (at == CLI_WINDOW_ROOMS)
    {
        at = 0u;
        while ((at < CLI_WINDOW_ROOMS) && s_room[at].taken)
        {
            at += 1u;
        }
        if (at == CLI_WINDOW_ROOMS)
        {
            cli_wake_wait(&s_wake, &s_lock);
        }
    }
    s_room[at].taken = 1;
    cli_lock_give(&s_lock);
    return at;
}

unsigned int cli_window_called(int own, const char *name, const char *args, unsigned long long size)
{
    const unsigned int at = window_room_taken();
    CliWindowRoom *const room = &s_room[at];
    CliLine out;
    (void)cli_line_growing(&out, CLI_WINDOW_ANSWER_FIRST, "a window call's answer");
    room->answer = out.out;
    room->said = cli_cell_taken(CLI_WINDOW_SAID, "why a window call failed");
    if ((room->answer == NULL) || (room->said == NULL))
    {
        cli_cell_given(room->said);
        room->ok = 0;
        room->said = s_refused;
        room->said_size = sizeof(s_refused) - 1u;
        return at;
    }
    CliLine said = {room->said, CLI_WINDOW_SAID, 0ull};
    if (own)
    {
        const CliServeAnswer answer = window_answer_of(name);
        if (answer == NULL)
        {
            cli_line_text(&said, name);
            cli_line_text(&said, " is not a command of the window's");
        }
        room->ok = (answer != NULL) && cli_serve_answered(answer, args, size, &out, &said);
    }
    else
    {
        room->ok = cli_window_tree_call(name, args, size, &out, &said);
    }
    if (room->ok && !cli_line_fits(&out))
    {
        said.at = 0ull;
        cli_line_text(&said, "the answer does not fit the room it is made in");
        room->ok = 0;
    }
    if (room->ok && (out.at == 0ull))
    {
        cli_line_text(&out, "null");
    }
    room->answer = out.out;
    room->answer_size = out.at;
    room->said_size = cli_line_fits(&said) ? said.at : 0ull;
    return at;
}

int cli_window_answer(unsigned int room, const char **text, unsigned long long *size)
{
    const CliWindowRoom *const held = &s_room[room];
    *text = held->ok ? held->answer : held->said;
    *size = held->ok ? held->answer_size : held->said_size;
    return held->ok;
}

void cli_window_given(unsigned int room)
{
    CliWindowRoom *const held = &s_room[room];
    if (held->said != s_refused)
    {
        cli_cell_given(held->said);
    }
    cli_cell_given(held->answer);
    held->answer = NULL;
    held->said = NULL;
    cli_lock_take(&s_lock);
    s_room[room].taken = 0;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

int cli_window_terminal_key(char *key, char *address)
{
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    static const char *const s_runs[1] = {"runs/*"};
    return cli_serve_open_door(address, &said) &&
           cli_comms_register("terminal", 0, 0u, s_runs, 1u, s_runs, 1u, key, &said);
}

void cli_window_terminal_bound(const char *key, unsigned int process)
{
    cli_comms_bind(key, process);
}

void cli_window_terminal_left(const char *key)
{
    cli_comms_leave(key);
}

void cli_window_quoted(CliLine *out, const char *text)
{
    cli_json_quoted_lossy(out, (const unsigned char *)text,
                          cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_window_counted(CliLine *out, const char *text, unsigned long long size)
{
    cli_json_quoted_lossy(out, (const unsigned char *)text, size);
}

void cli_window_strings(CliLine *out, const char *const *items, unsigned int count)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, items[at]);
    }
    cli_line_character(out, ']');
}

int cli_window_home(char *folder, CliLine *said)
{
    if (!cli_home_folder(folder))
    {
        cli_line_text(said, "orior_cli has no folder of its own");
        return 0;
    }
    return 1;
}

void cli_window_arg_written(const CliServeArgs *args, const char *key, CliLine *out)
{
    const unsigned int value = cli_serve_arg(args, key);
    if (value == CLI_JSON_NONE)
    {
        cli_line_text(out, "null");
        return;
    }
    cli_json_write(args->json, value, out, 0u, 0);
}
