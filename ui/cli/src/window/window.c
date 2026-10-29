// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.c: what the window calls of orior: the command line run first, and the window it leaves to open recorded; the
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
    char *err_msg;
    unsigned long long err_msg_size;
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

// a line of the errors, `orior: ` before what was written
static void window_err(const VerbaLinea *err_msg)
{
    static char s_line[CLI_WINDOW_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put_n(&line, err_msg->out, mmgr_line_ok(err_msg) ? err_msg->at : 0ull);
    mmgr_line_ch(&line, '\n');
    query_terminal_written(1, s_line, mmgr_line_ok(&line) ? line.at : 0ull);
}

int cli_window_command_line(int *window)
{
    // the window runs no main of orior's and enters here first: the pool every later call takes from is made here
    (void)cli_cells_made(CLI_CELLS_POOL);
    static const char *s_words[CLI_LAUNCH_ARGS];
    const unsigned int count = cli_arguments(0, NULL, s_words, CLI_LAUNCH_ARGS);
    const char *const *const words = s_words;
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
    static char s_why[CLI_WINDOW_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    if (cli_host_prepared(&err_msg) == -1ll)
    {
        window_err(&err_msg);
    }
    // every process the window starts stands under it, and ends with it
    (void)cli_container_made();
    static CliLinkAddress s_address;
    int remote = 0;
    err_msg.at = 0ull;
    if (s_outcome.has_remote)
    {
        remote = cli_link_address_parse(s_outcome.remote, &s_address, &err_msg);
        if (!remote)
        {
            window_err(&err_msg);
        }
    }
    static char s_root[CLI_PATH_MAX_LEN];
    const char *root = NULL;
    if (!remote && s_outcome.has_root)
    {
        root = s_outcome.root;
    }
    else if (!remote && cli_tree_found(s_root))
    {
        root = s_root;
    }
    char home[CLI_PATH_MAX_LEN];
    char detached_dir[CLI_PATH_MAX_LEN];
    const int has_home = cli_home_folder(home);
    if (has_home)
    {
        cli_full(home, "runs", detached_dir);
    }
    cli_serve_begun(root, has_home ? detached_dir : NULL);
    cli_serve_emit_set(emit, context);
    // the door of the comms layer is open from the start, for every process the container starts to join
    char address[CLI_PATH_MAX_LEN];
    err_msg.at = 0ull;
    (void)cli_serve_open_door(address, &err_msg);
    err_msg.at = 0ull;
    (void)cli_serve_open_to_command_line(&err_msg);
    cli_serve_watch();
    err_msg.at = 0ull;
    if (remote && !cli_link_open(&s_address, emit, context, &err_msg))
    {
        window_err(&err_msg);
    }
}

void cli_window_launch_taken(VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    const int has = s_outcome.window && (s_outcome.command[0] != '\0');
    if (has)
    {
        mmgr_line_put(out, "{\"root\":");
        if (s_outcome.has_root)
        {
            cli_window_quoted(out, s_outcome.root);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ",\"remote\":");
        if (s_outcome.has_remote)
        {
            cli_window_quoted(out, s_outcome.remote);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ",\"menu\":");
        cli_window_quoted(out, s_outcome.menu);
        mmgr_line_put(out, ",\"command\":");
        cli_window_quoted(out, s_outcome.command);
        mmgr_line_put(out, ",\"args\":[");
        for (unsigned int at = 0u; at < s_outcome.args; at += 1u)
        {
            mmgr_line_put(out, (at > 0u) ? "," : "");
            cli_window_quoted(out, cli_launch_arg(&s_outcome, at));
        }
        mmgr_line_put(out, "]}");
        s_outcome.command[0] = '\0';
    }
    else
    {
        mmgr_line_put(out, "null");
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
        {g_cli_window_home_calls, &g_cli_window_home_count},
        {g_cli_window_places_calls, &g_cli_window_places_count},
        {g_cli_window_net_calls, &g_cli_window_net_count},
        {g_cli_window_report_calls, &g_cli_window_report_count},
        {g_cli_window_text_calls, &g_cli_window_text_count},
        {g_cli_window_branch_calls, &g_cli_window_branch_count},
        {g_cli_window_code_calls, &g_cli_window_code_count},
        {g_cli_window_tree_calls, &g_cli_window_tree_count},
        {g_cli_window_memory_calls, &g_cli_window_memory_count},
        {g_cli_window_machine_calls, &g_cli_window_machine_count},
        {g_cli_window_scrollback_calls, &g_cli_window_scrollback_count},
        {g_cli_window_terminal_calls, &g_cli_window_terminal_count},
        {g_cli_window_clipboard_calls, &g_cli_window_clipboard_count},
        {g_cli_window_view_calls, &g_cli_window_view_count},
        {g_cli_window_editor_calls, &g_cli_window_editor_count},
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
// answered: its answer into `out`, 1, or 0 with why given. Files brought into a tree on another machine are this
// machine's, read here and written over the link
int cli_window_tree_call(const char *name, const char *args, unsigned long long size, VerbaLinea *out,
                         VerbaLinea *err_msg)
{
    CliLinkAddress address;
    if (!cli_link_address(&address))
    {
        return cli_serve_call(name, args, size, out, err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "files_bring", .cap = CLI_CELLS_POOL}))
    {
        return cli_serve_answered(cli_serve_files_bring_over, args, size, out, err_msg);
    }
    return cli_link_call(name, args, size, out, err_msg);
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
    VerbaLinea out;
    (void)cli_growing_line_made(&out, CLI_WINDOW_ANSWER_FIRST);
    room->answer = out.out;
    room->err_msg = g_cli_cells->persistent_buf_alloc(CLI_WINDOW_ERR_MSG_MAX_LEN, NULL);
    if ((room->answer == NULL) || (room->err_msg == NULL))
    {
        g_cli_cells->persistent_buf_release(room->err_msg, NULL);
        room->ok = 0;
        room->err_msg = s_refused;
        room->err_msg_size = sizeof(s_refused) - 1u;
        return at;
    }
    VerbaLinea err_line = {room->err_msg, CLI_WINDOW_ERR_MSG_MAX_LEN, 0ull};
    // a name of the window's is answered here on whichever lane the window called it on; a name not the window's
    // is the tree's, where the window called it as the tree's
    const CliServeAnswer answer = window_answer_of(name);
    if (answer != NULL)
    {
        room->ok = cli_serve_answered(answer, args, size, &out, &err_line);
    }
    else if (own)
    {
        mmgr_line_put(&err_line, name);
        mmgr_line_put(&err_line, " is not a command of the window's");
        room->ok = 0;
    }
    else
    {
        room->ok = cli_window_tree_call(name, args, size, &out, &err_line);
    }
    if (room->ok && !mmgr_line_ok(&out))
    {
        err_line.at = 0ull;
        mmgr_line_put(&err_line, "the answer does not fit the room it is made in");
        room->ok = 0;
    }
    if (room->ok && (out.at == 0ull))
    {
        mmgr_line_put(&out, "null");
    }
    room->answer = out.out;
    room->answer_size = out.at;
    room->err_msg_size = mmgr_line_ok(&err_line) ? err_line.at : 0ull;
    return at;
}

int cli_window_answer(unsigned int room, const char **text, unsigned long long *size)
{
    const CliWindowRoom *const entry = &s_room[room];
    *text = entry->ok ? entry->answer : entry->err_msg;
    *size = entry->ok ? entry->answer_size : entry->err_msg_size;
    return entry->ok;
}

void cli_window_given(unsigned int room)
{
    CliWindowRoom *const entry = &s_room[room];
    if (entry->err_msg != s_refused)
    {
        g_cli_cells->persistent_buf_release(entry->err_msg, NULL);
    }
    g_cli_cells->persistent_buf_release(entry->answer, NULL);
    entry->answer = NULL;
    entry->err_msg = NULL;
    cli_lock_take(&s_lock);
    s_room[room].taken = 0;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

int cli_window_terminal_key(char *key, char *address)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    static const char *const s_runs[1] = {"runs/*"};
    return cli_serve_open_door(address, &err_msg) &&
           cli_comms_register("terminal", 0, 0u, s_runs, 1u, s_runs, 1u, key, &err_msg);
}

void cli_window_terminal_bound(const char *key, unsigned int process)
{
    cli_comms_bind(key, process);
}

void cli_window_terminal_left(const char *key)
{
    cli_comms_leave(key);
}

void cli_window_quoted(VerbaLinea *out, const char *text)
{
    cli_json_quoted_lossy(out, (const unsigned char *)text,
                          cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_window_counted(VerbaLinea *out, const char *text, unsigned long long size)
{
    cli_json_quoted_lossy(out, (const unsigned char *)text, size);
}

void cli_window_strings(VerbaLinea *out, const char *const *items, unsigned int count)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, items[at]);
    }
    mmgr_line_ch(out, ']');
}

int cli_window_home(char *folder, VerbaLinea *err_msg)
{
    if (!cli_home_folder(folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    return 1;
}

void cli_window_arg_written(const CliServeArgs *args, const char *key, VerbaLinea *out)
{
    const unsigned int value = cli_serve_arg(args, key);
    if (value == CLI_JSON_NONE)
    {
        mmgr_line_put(out, "null");
        return;
    }
    cli_json_write(args->json, value, out, 0u, 0);
}
