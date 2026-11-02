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
// two cells the room takes at its first call and writes over on each one after, why it failed a text of this file's own
// where no cell could be taken for it; and the
typedef struct
{
    int taken;
    int ok;
    char *answer;
    unsigned long long answer_size;
    char *err_cell;
    const char *err_msg;
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

typedef struct
{
    const VerbaLinea *err_msg;
    const char *name;
} WindowCtx;
// a line of the errors, `orior: ` before what was written
EMBED_INLINE void window_err(const WindowCtx *args)
{
    static char s_line[CLI_WINDOW_ERR_MSG_MAX_LEN + 8u];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "orior: ");
    mmgr_line_put_n(&line, args->err_msg->out, mmgr_line_ok(args->err_msg) ? args->err_msg->at : 0ull);
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = s_line,
               .length = mmgr_line_ok(&line) ? line.at : 0ull);
}

int cli_window_command_line(int *window)
{
    // the window runs no main of orior's and enters here first: the pool every later call takes from is made here
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    static const char *s_words[CLI_LAUNCH_ARGS];
    const unsigned int count = EMBED_CALL(cli_room_count_internal.arguments, CliRoomCountCtx, .count_i32 = 0,
                                          .given = NULL, .words_read = s_words, .room_u32 = CLI_LAUNCH_ARGS);
    const char *const *const words = s_words;
    if (count == 0u)
    {
        cli_console_let_go();
        memor.set(&(MemoriaCfg){.dst = &s_outcome, .val = 0, .bytes = sizeof(s_outcome)});
        s_outcome.window = 1;
        *window = 1;
        return 0;
    }
    EMBED_CALL(cli_command_line_given.run, CliCommandLineGivenCfg, .given = words, .count = count,
               .outcome = &s_outcome);
    *window = s_outcome.window;
    return s_outcome.code;
}

void cli_window_begun(CliServeEmit emit, void *context)
{
    static char s_why[CLI_WINDOW_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    if (EMBED_CALL(cli_host_into.prepare, CliHostIntoCfg, .err_msg = &err_msg) == -1ll)
    {
        EMBED_CALL(window_err, WindowCtx, .err_msg = &err_msg);
    }
    // every process the window starts stands under it, and ends with it
    (void)cli_container_init();
    static CliLinkAddress s_address;
    int remote = 0;
    err_msg.at = 0ull;
    if (s_outcome.has_remote)
    {
        remote = EMBED_CALL(cli_link_text.address_parse, CliLinkTextCfg, .text = s_outcome.remote,
                            .address_out = &s_address, .err_msg = &err_msg);
        if (!remote)
        {
            EMBED_CALL(window_err, WindowCtx, .err_msg = &err_msg);
        }
    }
    static char s_root[CLI_PATH_MAX_LEN];
    const char *root = NULL;
    if (!remote && s_outcome.has_root)
    {
        root = s_outcome.root;
    }
    else if (!remote && EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = s_root))
    {
        root = s_root;
    }
    char home[CLI_PATH_MAX_LEN];
    char detached_dir[CLI_PATH_MAX_LEN];
    const int has_home = EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home);
    if (has_home)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "runs", .joined = detached_dir);
    }
    EMBED_CALL(cli_serve_root_table.begin, CliServeRootCfg, .root = root,
               .detached_dir = has_home ? detached_dir : NULL);
    EMBED_CALL(cli_serve_emit.emit_set, CliServeEmitCfg, .emit = emit, .context = context);
    // the door of the comms layer is open from the start, for every process the container starts to join
    char address[CLI_PATH_MAX_LEN];
    err_msg.at = 0ull;
    (void)EMBED_CALL(cli_serve_err_msg_address.open_door, CliServeErrMsgAddressCfg, .address = address,
                     .err_msg = &err_msg);
    err_msg.at = 0ull;
    (void)EMBED_CALL(cli_serve_err_msg_address.open_to_command_line, CliServeErrMsgAddressCfg, .err_msg = &err_msg);
    cli_serve_watch();
    err_msg.at = 0ull;
    if (remote && !EMBED_CALL(cli_link_err_msg_address.open, CliLinkErrMsgAddressCfg, .address = &s_address,
                              .emit = emit, .context = context, .err_msg = &err_msg))
    {
        EMBED_CALL(window_err, WindowCtx, .err_msg = &err_msg);
    }
}

void cli_window_launch_take(const CliWindowOutRequestCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const int has = s_outcome.window && (s_outcome.command[0] != '\0');
    if (has)
    {
        mmgr_line_put(args->out, "{\"root\":");
        if (s_outcome.has_root)
        {
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = args->out,
                       .text = s_outcome.root);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_put(args->out, ",\"remote\":");
        if (s_outcome.has_remote)
        {
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = args->out,
                       .text = s_outcome.remote);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_put(args->out, ",\"menu\":");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = args->out, .text = s_outcome.menu);
        mmgr_line_put(args->out, ",\"command\":");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = args->out,
                   .text = s_outcome.command);
        mmgr_line_put(args->out, ",\"args\":[");
        for (unsigned int at = 0u; at < s_outcome.args; at += 1u)
        {
            mmgr_line_put(args->out, (at > 0u) ? "," : "");
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = args->out,
                       .text = EMBED_CALL(cli_command_line_table.launch_arg, CliCommandLineCfg, .launch = &s_outcome,
                                          .at = at));
        }
        mmgr_line_put(args->out, "]}");
        s_outcome.command[0] = '\0';
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

void cli_window_ended(void)
{
    // a language server orior_cli started ends with it: on Windows a child outlives its parent
    cli_reach_forget();
    cli_serve_stop_all();
    CliLinkAddress address;
    if (EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        cli_link_end();
    }
    cli_container_members_end();
}

// the window's own call `name`: its answer, or NULL where none answers it
EMBED_INLINE CliServeAnswer window_answer_get(const WindowCtx *args)
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
            if (cellul.eq(&(CatenaFinitaCfg){
                    .src = s_tables[table].call[at].name, .other = args->name, .cap = CLI_CELLS_POOL}))
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
int cli_window_tree_call(const CliWindowOutSizeCtx *args)
{
    const char *name = args->name;
    const char *arguments = args->args_text;
    unsigned long long size = args->size;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    CliLinkAddress address;
    if (!EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        return EMBED_CALL(cli_serve_err_msg.call, CliServeErrMsgCfg, .name = name, .request = arguments, .size = size,
                          .out = out, .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "files_bring", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(cli_serve_size_internal.answer, CliServeSizeCtx, .answer = cli_serve_files_bring_over,
                          .args_text = arguments, .size = size, .out = out, .err_msg = err_msg);
    }
    return EMBED_CALL(cli_link_size_out.call, CliLinkSizeOutCfg, .name = name, .request = arguments, .size = size,
                      .out = out, .err_msg = err_msg);
}

// a room taken, waited for where every one is: its place
static unsigned int window_room_take(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
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
            EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_wake, .lock = &s_lock);
        }
    }
    s_room[at].taken = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return at;
}

unsigned int cli_window_called(int own, const char *name, const char *args, unsigned long long size)
{
    const unsigned int at = window_room_take();
    CliWindowRoom *const room = &s_room[at];
    if (room->answer == NULL)
    {
        room->answer = g_cli_cells->persistent_buf_alloc(CLI_WINDOW_ANSWER_ROOM, &g_cli_window_cells);
    }
    if (room->err_cell == NULL)
    {
        room->err_cell = g_cli_cells->persistent_buf_alloc(CLI_WINDOW_ERR_MSG_MAX_LEN, &g_cli_window_cells);
    }
    if ((room->answer == NULL) || (room->err_cell == NULL))
    {
        room->ok = 0;
        room->err_msg = s_refused;
        room->err_msg_size = sizeof(s_refused) - 1u;
        return at;
    }
    VerbaLinea out = {room->answer, CLI_WINDOW_ANSWER_ROOM, 0ull};
    room->err_msg = room->err_cell;
    VerbaLinea err_line = {room->err_cell, CLI_WINDOW_ERR_MSG_MAX_LEN, 0ull};
    // a name of the window's is answered here on whichever lane the window called it on; a name not the window's
    // is the tree's, where the window called it as the tree's
    const CliServeAnswer answer = EMBED_CALL(window_answer_get, WindowCtx, .name = name);
    if (answer != NULL)
    {
        room->ok = EMBED_CALL(cli_serve_size_internal.answer, CliServeSizeCtx, .answer = answer, .args_text = args,
                              .size = size, .out = &out, .err_msg = &err_line);
    }
    else if (own)
    {
        mmgr_line_put(&err_line, name);
        mmgr_line_put(&err_line, " is not a command of the window's");
        room->ok = 0;
    }
    else
    {
        room->ok = EMBED_CALL(cli_window_out_size_internal.tree_call, CliWindowOutSizeCtx, .name = name,
                              .args_text = args, .size = size, .out = &out, .err_msg = &err_line);
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
    entry->err_msg = NULL;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_room[room].taken = 0;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

int cli_window_terminal_key(const CliWindowKeyAddressCtx *args)
{
    char *key = args->key_out;
    char *address = args->address;
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    static const char *const s_runs[1] = {"runs/*"};
    return EMBED_CALL(cli_serve_err_msg_address.open_door, CliServeErrMsgAddressCfg, .address = address,
                      .err_msg = &err_msg) &&
           EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "terminal", .has_process = 0,
                      .process = 0u, .listen = s_runs, .listens = 1u, .send_grant = s_runs, .send_grants = 1u,
                      .key_out = key, .err_msg = &err_msg);
}

void cli_window_terminal_bind(const CliWindowKeyCtx *args)
{
    const char *key = args->key;
    unsigned int process = args->process;
    EMBED_CALL(cli_comms_process.bind, CliCommsProcessCfg, .key = key, .process = process);
}

void cli_window_terminal_leave(const CliWindowKeyCtx *args)
{
    const char *key = args->key;
    EMBED_CALL(cli_comms_process.leave, CliCommsProcessCfg, .key = key);
}

void cli_window_quote(const CliWindowOutTextCtx *args)
{
    VerbaLinea *out = args->out;
    const char *text = args->text;
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

void cli_window_count(const CliWindowOutTextCtx *args)
{
    VerbaLinea *out = args->out;
    const char *text = args->text;
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)text, .length = args->size);
}

void cli_window_strings(const CliWindowOutItemsCtx *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = args->out, .text = args->items[at]);
    }
    mmgr_line_ch(args->out, ']');
}

int cli_window_home(const CliWindowCtx *args)
{
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = args->folder))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    return 1;
}

void cli_window_arg_write(const CliWindowOutRequestCtx *args)
{
    const CliServeArgs *arguments = args->request;
    const char *key = args->key;
    VerbaLinea *out = args->out;
    const unsigned int value =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = key);
    if (value == CLI_JSON_NONE)
    {
        mmgr_line_put(out, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = arguments->json, .node = value,
               .out = out, .indent = 0u, .sorted = 0);
}
