// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// terminal_tabs.c: the terminal's tabs, on the window's thread. Each tab is a pane as the editor's are: a text stored
// under a key of its own with a view on it, which scrolls it, chooses in it and is drawn from, colored by the xterm
// grammar plugin; its shell; and the xterm writing the shell's output into the rows at the text's foot (xterm.h). A
// line that leaves the main screen's top joins the history above the rows where it stands; the history is written as
// the file of the tab's number in orior's folder as the window ends, and a tab of that number reads it back as it
// starts. Besides the tabs:
// the tab shown and the one beside it where the panel is split; the one the keys go to; whether the panel is open and
// takes the keys; and its height. Every shell is drained each step whether its tab shows or not. A tab closed waits,
// its shell hung up on, until the shell ends before its cell goes. A shell starts the first time its tab shows, in the
// folder its tab last stood in where that is a folder and else in the tree's top folder; one that ended starts again
// at the next key typed into it. The place, whether the panel is open, the tab shown and the one beside it, the height
// and each tab's number and folder, is stored as a value of orior's folder TERMINAL_PLACE_REST_MS after the last change
// and as the window ends, and read back as the window starts
#include "terminal_tabs.h"

#include "../command_line/pool.h"
#include "../editor/view.h"
#include "../machine/state.h"
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "keys.h"

// the panel's height at first, in pixels at a scale of one; the tabs closed that wait for their shells to end, at the
// most; how long the window's end waits for every shell; the most bytes a paste sends; the most bytes a key sends; the
// lines of a tab's history read back as it starts; the head of a tab's key; the folder of orior's the histories stand
// in; and the key the place is stored under and how long after the last change it is stored
#define TERMINAL_HEIGHT 256u
#define TERMINAL_CLOSING 16u
#define TERMINAL_END_MS 2000ULL
#define TERMINAL_PASTE_MAX (1024u * 1024u)
#define TERMINAL_KEY_BYTES 32u
#define TERMINAL_RESTORED 300ULL
#define TERMINAL_KEY_HEAD "terminal:"
#define TERMINAL_FOLDER "terminals"
#define TERMINAL_PLACE_KEY "terminal.place"
#define TERMINAL_PLACE_REST_MS 800ULL

// One tab: its xterm, which knows where in the text its rows and its history stand, and its shell; whether the xterm
// has a size yet and whether its text is stored with its view open; its number, which its key and its history are
// called by; the key and the history's path; and the folder its shell last reported standing in, or the one read back
// for it, empty for none
typedef struct
{
    CliTerminalXterm xterm;
    CliTerminalShell shell;
    int sized;
    int stored;
    unsigned int number;
    char key[TERMINAL_KEY_BYTES];
    char history_path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
} TerminalTab;

// The terminal: its tabs and how many the cell of them has places for; the tabs closed whose shells have yet to end;
// the tab shown, the one beside it, the one the keys go to and the one the pointer chooses in, each -1 for none;
// whether the panel is open and whether it takes the keys; its height; the columns and rows the window last sized its
// two views to, 0 before it sized them; the clock's reading the place is stored at, 0 for none; whether the place was
// read back, before which nothing is stored; and the tree's top folder a shell with no folder of its own starts in
typedef struct
{
    TerminalTab **tab;
    unsigned int tabs;
    unsigned int capacity;
    TerminalTab *closing[TERMINAL_CLOSING];
    unsigned int closings;
    int shown;
    int beside;
    int focused;
    int choosing;
    int open;
    int keys;
    unsigned int height;
    unsigned int columns[2];
    unsigned int rows[2];
    unsigned long long place_ms;
    int restored;
    const char *root;
} TerminalModel;

static TerminalModel s_terminal = {.shown = -1, .beside = -1, .focused = -1, .choosing = -1, .height = TERMINAL_HEIGHT};

typedef struct
{
    int at;
    int taken_out;
    unsigned int view;
    TerminalTab *tab;
    const char *bytes;
    unsigned long long size;
    unsigned long long first;
    unsigned int columns;
    unsigned long long row;
    unsigned long long column;
    unsigned int clicks;
    int shift;
    unsigned int pointer;
    int on;
    const char *place;
    VerbaLinea *out;
    unsigned int number;
    unsigned long long *read_at;
} TerminalCtx;

// the terminal's place stored TERMINAL_PLACE_REST_MS after this change where no other comes before then
EMBED_INLINE void terminal_dirty(void)
{
    s_terminal.place_ms = cli_clock_milliseconds() + TERMINAL_PLACE_REST_MS;
}

// the tab the view `view` shows, 0 the one shown and 1 the one beside it: -1 for none
EMBED_INLINE int terminal_viewed(const TerminalCtx *args)
{
    return (args->view == 0u) ? s_terminal.shown : s_terminal.beside;
}

// the tab the keys go to, else the one shown, else the first: -1 where there are none
EMBED_INLINE int terminal_target(void)
{
    if (s_terminal.focused >= 0)
    {
        return s_terminal.focused;
    }
    return (s_terminal.shown >= 0) ? s_terminal.shown : ((s_terminal.tabs != 0u) ? 0 : -1);
}

// the last folder of the path `place` put after `out`: 1, or 0 where the path has none
EMBED_INLINE int terminal_folder(const TerminalCtx *args)
{
    const char *const place = args->place;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_PATH_MAX_LEN});
    while ((end > 0ULL) && ((place[end - 1ULL] == '/') || (place[end - 1ULL] == '\\')))
    {
        end -= 1ULL;
    }
    unsigned long long start = end;
    while ((start > 0ULL) && (place[start - 1ULL] != '/') && (place[start - 1ULL] != '\\'))
    {
        start -= 1ULL;
    }
    mmgr_line_put_n(args->out, place + start, end - start);
    return end > start;
}

// the folder the shell of `tab` starts in put after `out`: the tab's folder where it is one, as it is written or, where
// Git's bash wrote it as /d/rest, as d:/rest; 1, or 0 where neither is a folder and the shell starts in the tree's top
EMBED_INLINE int terminal_start_folder(const TerminalCtx *args)
{
    const char *const folder = args->tab->folder;
    if (folder[0] == '\0')
    {
        return 0;
    }
    if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = folder))
    {
        mmgr_line_put(args->out, folder);
        return mmgr_line_ok(args->out);
    }
    const int letter = ((folder[1] >= 'a') && (folder[1] <= 'z')) || ((folder[1] >= 'A') && (folder[1] <= 'Z'));
    if ((folder[0] != '/') || !letter || ((folder[2] != '/') && (folder[2] != '\0')))
    {
        return 0;
    }
    const unsigned long long began = args->out->at;
    mmgr_line_ch(args->out, folder[1]);
    mmgr_line_ch(args->out, ':');
    mmgr_line_put(args->out, (folder[2] == '/') ? (folder + 2) : "/");
    const unsigned long long length = args->out->at;
    const int folder_found =
        mmgr_line_ok(args->out) && (mmgr_line_finish(args->out) != 0ULL) &&
        EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->out->out + began);
    args->out->at = folder_found ? length : began;
    return folder_found;
}

// the tab's view scrolled to the line `first` at its top
EMBED_INLINE void terminal_scroll(const TerminalCtx *args)
{
    const CliViewInput scrolled = {.kind = CLI_VIEW_SCROLL, .top = args->first};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = args->tab->key, .input = &scrolled,
                     .err_msg = &err_msg);
}

// whether the tab's view stands on the screen drawn on, following the output, and not scrolled back over the lines
// above it
EMBED_INLINE int terminal_follows(const TerminalCtx *args)
{
    return args->tab->stored && (cli_view_top(args->tab->key) >= args->tab->xterm.base);
}

// the tab's text stored and its view opened: its history's last TERMINAL_RESTORED lines read back from its file, which
// is written again with them alone, and the xterm's rows after them, blank, the view on the rows. The lines read back
// stand above the rows and not on them: a pseudo-console clears its screen as its shell starts
EMBED_INLINE void terminal_store(const TerminalCtx *args)
{
    TerminalTab *const tab = args->tab;
    char folder[CLI_PATH_MAX_LEN];
    if (tab->stored || !EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        return;
    }
    VerbaLinea folder_line = {folder, sizeof(folder),
                              cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = sizeof(folder)})};
    mmgr_line_put(&folder_line, "/" TERMINAL_FOLDER);
    (void)mmgr_line_finish(&folder_line);
    (void)EMBED_CALL(cli_words_err_msg_internal.private_folder_init, CliWordsErrMsgCtx, .folder = folder);
    VerbaLinea path = {tab->history_path, sizeof(tab->history_path), 0ULL};
    mmgr_line_put(&path, folder);
    mmgr_line_ch(&path, '/');
    mmgr_line_decimal(&path, tab->number, 1u);
    (void)mmgr_line_finish(&path);
    unsigned long long size = 0ULL;
    const unsigned char *const file_bytes =
        EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = tab->history_path, .size_pointer = &size,
                   .owner = g_cli_terminal_owner);
    const unsigned long long read = (file_bytes != NULL) ? size : 0ULL;
    // the history read back from the start of its last TERMINAL_RESTORED lines, a last line with no newline ended
    const int ended = (read == 0ULL) || (file_bytes[read - 1ULL] == (unsigned char)'\n');
    unsigned long long lines = ended ? 0ULL : 1ULL;
    for (unsigned long long at = 0ULL; at < read; at += 1ULL)
    {
        lines += (file_bytes[at] == (unsigned char)'\n') ? 1ULL : 0ULL;
    }
    unsigned long long skipped = (lines > TERMINAL_RESTORED) ? (lines - TERMINAL_RESTORED) : 0ULL;
    unsigned long long from = 0ULL;
    while ((skipped > 0ULL) && (from < read))
    {
        skipped -= (file_bytes[from] == (unsigned char)'\n') ? 1ULL : 0ULL;
        from += 1ULL;
    }
    const unsigned long long restored = read - from;
    const unsigned int rows = tab->xterm.rows;
    const unsigned long long bytes = restored + 1ULL + rows + 1ULL;
    char *const text = g_cli_guard->persistent_buf_alloc(bytes, g_cli_terminal_owner);
    if (text == NULL)
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)file_bytes, (g_cli_terminal_owner));
        return;
    }
    VerbaLinea out = {text, bytes, 0ULL};
    if (restored != 0ULL)
    {
        mmgr_line_put_n(&out, (const char *)file_bytes + from, restored);
    }
    if (!ended)
    {
        mmgr_line_ch(&out, '\n');
    }
    (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = tab->history_path, .bytes = text,
                     .size = out.at, .fresh = 1);
    unsigned long long history = 0ULL;
    for (unsigned long long at = 0ULL; at < out.at; at += 1ULL)
    {
        history += (text[at] == '\n') ? 1ULL : 0ULL;
    }
    // the screen's rows, each a line, the first the one the history's last line end opens
    for (unsigned int row = 1u; row < rows; row += 1u)
    {
        mmgr_line_ch(&out, '\n');
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)file_bytes, (g_cli_terminal_owner));
    VerbaLinea key = {tab->key, sizeof(tab->key), 0ULL};
    mmgr_line_put(&key, TERMINAL_KEY_HEAD);
    mmgr_line_decimal(&key, tab->number, 1u);
    (void)mmgr_line_finish(&key);
    CliShape shape = {0ULL, 0ULL};
    const int stored = mmgr_line_ok(&out) &&
                       EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .text = text, .size = out.at,
                                  .key = tab->key, .shape = &shape) &&
                       EMBED_CALL(cli_view_table.open, CliViewCfg, .key = tab->key, .path = "");
    g_cli_guard->persistent_buf_release(text, g_cli_terminal_owner);
    if (!stored)
    {
        EMBED_CALL(cli_document.release, CliDocumentCfg, .key = tab->key);
        return;
    }
    tab->stored = 1;
    tab->xterm.key = tab->key;
    tab->xterm.history = history;
    tab->xterm.base = history;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    // the text colored by the xterm grammar plugin, as a file is by its language's
    char grammar[CLI_NAME_MAX_LEN];
    grammar[0] = '\0';
    if (!EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "xterm", .key = grammar,
                    .err_msg = &err_msg))
    {
        grammar[0] = '\0';
    }
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = tab->key, .grammar = grammar,
                     .named = 0);
    const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = rows, .columns = tab->xterm.columns};
    err_msg.at = 0ULL;
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &sized,
                     .err_msg = &err_msg);
    EMBED_CALL(terminal_scroll, TerminalCtx, .tab = tab, .first = history);
}

// the tab's view closed and its text let go, its history's file removed where `on` is 1 and left for a tab of its
// number to read back where it is 0
EMBED_INLINE void terminal_unstore(const TerminalCtx *args)
{
    TerminalTab *const tab = args->tab;
    if (!tab->stored)
    {
        return;
    }
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = tab->key);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = tab->key);
    tab->stored = 0;
    if (args->on)
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = tab->history_path);
    }
}

// the selection of the tab's view written to the clipboard, the selection let go: 1, or 0 where nothing is chosen
EMBED_INLINE int terminal_copy(const TerminalCtx *args)
{
    TerminalTab *const tab = args->tab;
    CliPosition cursor = {0ULL, 0ULL};
    CliPosition anchor = {0ULL, 0ULL};
    if ((tab == NULL) || !tab->stored ||
        !EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = tab->key, .cursor = &cursor,
                    .anchor = &anchor) ||
        ((cursor.line == anchor.line) && (cursor.col == anchor.col)))
    {
        return 0;
    }
    const int forward = (anchor.line < cursor.line) || ((anchor.line == cursor.line) && (anchor.col < cursor.col));
    const CliPosition from = forward ? anchor : cursor;
    const CliPosition to = forward ? cursor : anchor;
    char state[8];
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = tab->key, .first = from.line,
                                  .last = to.line, .size_pointer = &size, .state = state, .capacity = sizeof(state),
                                  .owner = g_cli_terminal_owner);
    if (text == NULL)
    {
        return 0;
    }
    // the bytes from `from`'s unit on its line up to `to`'s on its own, each line's units counted from its start and
    // a letter past the basic plane counting two
    unsigned long long start = size;
    unsigned long long end = size;
    unsigned long long line = from.line;
    unsigned long long units = 0ULL;
    unsigned long long at = 0ULL;
    while (end == size)
    {
        start = ((start == size) && (line == from.line) && (units == from.col)) ? at : start;
        if (((line == to.line) && (units >= to.col)) || (at >= size))
        {
            end = at;
            break;
        }
        unsigned long long consumed = 0ULL;
        const unsigned int letter = (text[at] == '\n') ? (unsigned int)'\n'
                                                       : EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text,
                                                                    .size = size, .at = at, .length = &consumed);
        units = (letter == (unsigned int)'\n') ? 0ULL : (units + ((letter >= 0x10000u) ? 2ULL : 1ULL));
        line += (letter == (unsigned int)'\n') ? 1ULL : 0ULL;
        at += (consumed != 0ULL) ? consumed : 1ULL;
    }
    // the SGR sequences the text carries for its colors are no letters of the copy; the bytes read as unsigned are the
    // same bytes
    const unsigned long long copied = (end > start)
                                          ? EMBED_CALL(cli_terminal_xterm_plain, CliTerminalXtermPlainCfg,
                                                       .bytes = (unsigned char *)(text + start), .size = end - start)
                                          : 0ULL;
    (void)EMBED_CALL(cli_size_text_internal.clipboard_text_write, CliSizeTextCtx, .size = copied,
                     .text = text + ((start < size) ? start : size));
    g_cli_guard->persistent_buf_release(text, g_cli_terminal_owner);
    const CliViewInput placed = {.kind = CLI_VIEW_POSITION, .row = cursor.line, .column = cursor.col};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &placed,
                     .err_msg = &err_msg);
    terminal_dirty();
    return 1;
}

// the shell of `tab` started in its folder, or the tree's top where it has none, or why it could not be printed on its
// xterm
EMBED_INLINE void terminal_start(const TerminalCtx *args)
{
    TerminalTab *const tab = args->tab;
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea folder_line = {folder, sizeof(folder), 0ULL};
    const int own_folder = EMBED_CALL(terminal_start_folder, TerminalCtx, .tab = tab, .out = &folder_line) &&
                           (mmgr_line_finish(&folder_line) != 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    mmgr_line_put(&err_msg, "\r\n");
    if (!EMBED_CALL(cli_terminal_shell.start, CliTerminalShellCfg, .shell = &tab->shell, .xterm = &tab->xterm,
                    .words = NULL, .folder = own_folder ? folder : s_terminal.root, .err_msg = &err_msg))
    {
        mmgr_line_put(&err_msg, "\r\n");
        EMBED_CALL(cli_terminal_xterm_bytes.write, CliTerminalXtermBytesCfg, .xterm = &tab->xterm,
                   .bytes = (const unsigned char *)why, .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
    }
    terminal_dirty();
}

// the tab the view `view` shows sized as the window said the view is, its shell told and its text's rows and view
// sized with it; its text stored the first time; a shell never started started where the panel is open
EMBED_INLINE void terminal_fit(const TerminalCtx *args)
{
    const int at = EMBED_CALL(terminal_viewed, TerminalCtx, .view = args->view);
    const unsigned int columns = s_terminal.columns[args->view];
    const unsigned int rows = s_terminal.rows[args->view];
    if ((at < 0) || (columns == 0u) || (rows == 0u))
    {
        return;
    }
    TerminalTab *const tab = s_terminal.tab[at];
    const int follows = EMBED_CALL(terminal_follows, TerminalCtx, .tab = tab);
    if (!tab->sized)
    {
        // the text the xterm writes into is stored once its rows are known, which sets where it writes; a folder the
        // shell reports is written into the tab's own
        tab->sized = EMBED_CALL(cli_terminal_xterm_size.init, CliTerminalXtermSizeCfg, .xterm = &tab->xterm,
                                .columns = columns, .rows = rows, .key = NULL, .history = 0ULL, .folder = tab->folder,
                                .folder_size = sizeof(tab->folder));
        EMBED_CALL(terminal_store, TerminalCtx, .tab = tab);
    }
    else if (((tab->xterm.columns != columns) || (tab->xterm.rows != rows)) &&
             EMBED_CALL(cli_terminal_xterm_size.resize, CliTerminalXtermSizeCfg, .xterm = &tab->xterm,
                        .columns = columns, .rows = rows))
    {
        EMBED_CALL(cli_terminal_shell.resize, CliTerminalShellCfg, .shell = &tab->shell, .xterm = &tab->xterm);
        const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = rows, .columns = columns};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &sized,
                         .err_msg = &err_msg);
        if (follows)
        {
            EMBED_CALL(terminal_scroll, TerminalCtx, .tab = tab, .first = tab->xterm.base);
        }
    }
    if (tab->sized && s_terminal.open && (tab->shell.state == CLI_TERMINAL_SHELL_IDLE))
    {
        EMBED_CALL(terminal_start, TerminalCtx, .tab = tab);
    }
    terminal_dirty();
}

// both views fitted
EMBED_INLINE void terminal_fit_all(void)
{
    EMBED_CALL(terminal_fit, TerminalCtx, .view = 0u);
    EMBED_CALL(terminal_fit, TerminalCtx, .view = 1u);
}

// a tab added after the rest, its shell not started, its number `number` or, where that is 0, the least no other tab
// has: its place, or -1 where no cell could be had for it
EMBED_INLINE int terminal_add(const TerminalCtx *args)
{
    if (s_terminal.tabs == s_terminal.capacity)
    {
        const unsigned int capacity = (s_terminal.capacity == 0u) ? 4u : (s_terminal.capacity * 2u);
        TerminalTab **const grown = g_cli_guard->persistent_buf_alloc(capacity * sizeof(*grown), g_cli_terminal_owner);
        if (grown == NULL)
        {
            return -1;
        }
        if (s_terminal.tab != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_terminal.tab, .bytes = s_terminal.tabs * sizeof(*grown)});
            g_cli_guard->persistent_buf_release(s_terminal.tab, g_cli_terminal_owner);
        }
        s_terminal.tab = grown;
        s_terminal.capacity = capacity;
    }
    TerminalTab *const tab = g_cli_guard->persistent_buf_alloc(sizeof(*tab), g_cli_terminal_owner);
    if (tab == NULL)
    {
        return -1;
    }
    memor.set(&(MemoriaCfg){.dst = tab, .val = 0, .bytes = sizeof(*tab)});
    unsigned int number = 0u;
    int taken = args->number == 0u;
    while (taken)
    {
        number += 1u;
        taken = 0;
        for (unsigned int at = 0u; at < s_terminal.tabs; at += 1u)
        {
            taken = taken || (s_terminal.tab[at]->number == number);
        }
    }
    tab->number = (args->number != 0u) ? args->number : number;
    s_terminal.tab[s_terminal.tabs] = tab;
    s_terminal.tabs += 1u;
    terminal_dirty();
    // the tabs are far fewer than an int counts
    return (int)s_terminal.tabs - 1;
}

// the keys sent to the shell of the tab `at`
EMBED_INLINE void terminal_focus(const TerminalCtx *args)
{
    s_terminal.focused = args->at;
    s_terminal.keys = 1;
    terminal_fit_all();
}

// the panel opened, with a tab where there is none, its shells started as they show, taking the keys where `on` is 2;
// or closed where `on` is 0, the keys let go
EMBED_INLINE void terminal_toggle(const TerminalCtx *args)
{
    s_terminal.open = args->on != 0;
    terminal_dirty();
    if (!s_terminal.open)
    {
        s_terminal.keys = 0;
        return;
    }
    if (s_terminal.tabs == 0u)
    {
        s_terminal.shown = EMBED_CALL(terminal_add, TerminalCtx, .number = 0u);
        s_terminal.focused = s_terminal.shown;
    }
    if (args->on == 2)
    {
        EMBED_CALL(terminal_focus, TerminalCtx, .at = terminal_target());
        return;
    }
    terminal_fit_all();
}

// the tab `at` shown: in the place of the one the keys go to where the panel is split, else alone; the panel open and
// taking the keys
EMBED_INLINE void terminal_show(const TerminalCtx *args)
{
    if ((args->at != s_terminal.shown) && (args->at != s_terminal.beside))
    {
        if ((s_terminal.beside >= 0) && (s_terminal.focused == s_terminal.beside))
        {
            s_terminal.beside = args->at;
        }
        else
        {
            s_terminal.shown = args->at;
        }
    }
    EMBED_CALL(terminal_toggle, TerminalCtx, .on = 1);
    EMBED_CALL(terminal_focus, TerminalCtx, .at = args->at);
}

// a place `at` among the tabs after the tab at `taken_out` is taken out: the same where it stood before it, one less
// after it, and -1 for the tab taken out or none
EMBED_INLINE int terminal_after(const TerminalCtx *args)
{
    const int out = args->taken_out;
    return ((args->at < 0) || (args->at == out)) ? -1 : ((args->at > out) ? (args->at - 1) : args->at);
}

// a tab whose shell was hung up on drained until the shell ends or the window's end waits no longer, its cell let go
// once it ends
EMBED_INLINE void terminal_wait_free(const TerminalCtx *args)
{
    const unsigned long long last_ms = cli_clock_milliseconds() + TERMINAL_END_MS;
    CliWake pause = CLI_WAKE_INIT;
    while ((args->tab->shell.state == CLI_TERMINAL_SHELL_RUNNING) && (cli_clock_milliseconds() < last_ms))
    {
        (void)EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &args->tab->shell, .xterm = NULL,
                         .now_ms = cli_clock_milliseconds());
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause,
                         .milliseconds_u64 = CLI_TERMINAL_LOOK_MS);
    }
    if (args->tab->shell.state != CLI_TERMINAL_SHELL_RUNNING)
    {
        g_cli_guard->persistent_buf_release(args->tab->xterm.cell, g_cli_terminal_owner);
        args->tab->xterm.cell = NULL;
        g_cli_guard->persistent_buf_release(args->tab, g_cli_terminal_owner);
    }
}

// the tab `at` closed, its view and text let go, its history removed where `on` is 1, its shell hung up on and its cell
// let go once the shell ends; the last tab closed closes the panel and a fresh one waits in its place
EMBED_INLINE void terminal_close(const TerminalCtx *args)
{
    // the tab is among the tabs
    const unsigned int out = (unsigned int)args->at;
    TerminalTab *const tab = s_terminal.tab[out];
    EMBED_CALL(terminal_unstore, TerminalCtx, .tab = tab, .on = args->on);
    EMBED_CALL(cli_terminal_shell.end, CliTerminalShellCfg, .shell = &tab->shell, .now_ms = cli_clock_milliseconds());
    if (tab->shell.state != CLI_TERMINAL_SHELL_RUNNING)
    {
        g_cli_guard->persistent_buf_release(tab->xterm.cell, g_cli_terminal_owner);
        tab->xterm.cell = NULL;
        g_cli_guard->persistent_buf_release(tab, g_cli_terminal_owner);
    }
    else if (s_terminal.closings < TERMINAL_CLOSING)
    {
        s_terminal.closing[s_terminal.closings] = tab;
        s_terminal.closings += 1u;
    }
    else
    {
        EMBED_CALL(terminal_wait_free, TerminalCtx, .tab = tab);
    }
    for (unsigned int at = out + 1u; at < s_terminal.tabs; at += 1u)
    {
        s_terminal.tab[at - 1u] = s_terminal.tab[at];
    }
    s_terminal.tabs -= 1u;
    s_terminal.choosing = -1;
    int beside = EMBED_CALL(terminal_after, TerminalCtx, .at = s_terminal.beside, .taken_out = args->at);
    int shown = EMBED_CALL(terminal_after, TerminalCtx, .at = s_terminal.shown, .taken_out = args->at);
    const int focused = EMBED_CALL(terminal_after, TerminalCtx, .at = s_terminal.focused, .taken_out = args->at);
    if (shown < 0)
    {
        // the tabs left are far fewer than an int counts
        shown = (beside >= 0) ? beside : ((int)s_terminal.tabs - 1);
        beside = (beside == shown) ? -1 : beside;
    }
    s_terminal.shown = shown;
    s_terminal.beside = beside;
    s_terminal.focused = (focused >= 0) ? focused : shown;
    if (s_terminal.tabs == 0u)
    {
        EMBED_CALL(terminal_toggle, TerminalCtx, .on = 0);
        s_terminal.shown = EMBED_CALL(terminal_add, TerminalCtx, .number = 0u);
        s_terminal.focused = s_terminal.shown;
    }
    terminal_fit_all();
}

// Split: a second shell shown beside the one shown, a new one where every other is shown already; pressed again, one
// shell shown
EMBED_INLINE void terminal_split(void)
{
    if (s_terminal.beside >= 0)
    {
        s_terminal.beside = -1;
        s_terminal.focused = s_terminal.shown;
        terminal_fit_all();
        return;
    }
    EMBED_CALL(terminal_toggle, TerminalCtx, .on = 1);
    int other = -1;
    for (unsigned int at = 0u; (at < s_terminal.tabs) && (other < 0); at += 1u)
    {
        // the tabs are far fewer than an int counts
        other = ((int)at != s_terminal.shown) ? (int)at : -1;
    }
    other = (other >= 0) ? other : EMBED_CALL(terminal_add, TerminalCtx, .number = 0u);
    if (other >= 0)
    {
        s_terminal.beside = other;
        EMBED_CALL(terminal_focus, TerminalCtx, .at = other);
    }
}

// `size` bytes of `bytes` sent to the shell the keys go to, a shell that ended started again first, its view taken to
// the xterm's rows
EMBED_INLINE void terminal_send(const TerminalCtx *args)
{
    const int at = terminal_target();
    if ((at < 0) || (args->size == 0ULL))
    {
        return;
    }
    TerminalTab *const tab = s_terminal.tab[at];
    if ((tab->shell.state != CLI_TERMINAL_SHELL_RUNNING) && tab->sized)
    {
        EMBED_CALL(terminal_start, TerminalCtx, .tab = tab);
    }
    if (tab->stored && !EMBED_CALL(terminal_follows, TerminalCtx, .tab = tab))
    {
        EMBED_CALL(terminal_scroll, TerminalCtx, .tab = tab, .first = tab->xterm.base);
        terminal_dirty();
    }
    EMBED_CALL(cli_terminal_shell.write, CliTerminalShellCfg, .shell = &tab->shell,
               .bytes = (const unsigned char *)args->bytes, .size = args->size);
}

// a paste sent as a shell takes one: `size` bytes of `bytes` or, where `bytes` is NULL, the clipboard's text, each line
// ended as Enter ends it, and marked as a paste where the program in the terminal asked for that
EMBED_INLINE void terminal_paste(const TerminalCtx *args)
{
    const int at = terminal_target();
    char *const text = (at >= 0) ? g_cli_guard->persistent_buf_alloc(TERMINAL_PASTE_MAX, g_cli_terminal_owner) : NULL;
    if (text == NULL)
    {
        return;
    }
    VerbaLinea line = {text, TERMINAL_PASTE_MAX, 0ULL};
    const int bracketed = s_terminal.tab[at]->xterm.bracketed_paste;
    mmgr_line_put(&line, bracketed ? "\x1b[200~" : "");
    const unsigned long long from = line.at;
    if (args->bytes != NULL)
    {
        mmgr_line_put_n(&line, args->bytes, args->size);
    }
    else
    {
        (void)EMBED_CALL(cli_internal_table.clipboard_text_read, CliCtx, .text_verba_linea_pointer = &line);
    }
    // a line's end, CR LF or LF alone, read as one CR, the text closing up behind it
    unsigned long long written = from;
    for (unsigned long long read = from; read < line.at; read += 1ULL)
    {
        const int pair = (text[read] == '\r') && ((read + 1ULL) < line.at) && (text[read + 1ULL] == '\n');
        text[written] = (text[read] == '\n') ? '\r' : text[read];
        written += 1ULL;
        read += pair ? 1ULL : 0ULL;
    }
    const int pasted = written > from;
    line.at = written;
    mmgr_line_put(&line, bracketed ? "\x1b[201~" : "");
    if (pasted && mmgr_line_ok(&line))
    {
        EMBED_CALL(terminal_send, TerminalCtx, .bytes = text, .size = line.at);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_terminal_owner);
}

int cli_terminal_tabs_command(const char *name, const char *const *words, unsigned int count)
{
    static const char *const s_commands[9] = {"terminal-view", "toggle", "terminal-close", "new",          "split",
                                              "clear",         "kill",   "terminal-paste", "terminal-copy"};
    unsigned int command = 0u;
    while ((command < 9u) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_commands[command], .cap = CLI_LABEL_MAX_LEN}))
    {
        command += 1u;
    }
    const int at = terminal_target();
    switch (command)
    {
    case 0u:
    case 1u:
        EMBED_CALL(terminal_toggle, TerminalCtx, .on = s_terminal.open ? 0 : 2);
        return 1;
    case 2u:
        EMBED_CALL(terminal_toggle, TerminalCtx, .on = 0);
        return 1;
    case 3u: {
        const int added = EMBED_CALL(terminal_add, TerminalCtx, .number = 0u);
        if (added >= 0)
        {
            EMBED_CALL(terminal_show, TerminalCtx, .at = added);
        }
        return 1;
    }
    case 4u:
        terminal_split();
        return 1;
    case 5u:
        if ((at >= 0) && (s_terminal.tab[at]->shell.state == CLI_TERMINAL_SHELL_RUNNING))
        {
            EMBED_CALL(terminal_send, TerminalCtx, .bytes = "\x0c", .size = 1ULL);
        }
        return 1;
    case 6u:
        if (at >= 0)
        {
            EMBED_CALL(terminal_close, TerminalCtx, .at = at, .on = 1);
        }
        return 1;
    case 7u: {
        // the words handed to the paste, a space between each, else the clipboard's text
        char text_words[CLI_PATH_MAX_LEN];
        VerbaLinea line = {text_words, sizeof(text_words), 0ULL};
        for (unsigned int word = 0u; word < count; word += 1u)
        {
            mmgr_line_put(&line, (word != 0u) ? " " : "");
            mmgr_line_put(&line, words[word]);
        }
        EMBED_CALL(terminal_paste, TerminalCtx, .bytes = (count != 0u) ? text_words : NULL,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        return 1;
    }
    case 8u:
        (void)EMBED_CALL(terminal_copy, TerminalCtx, .tab = (at >= 0) ? s_terminal.tab[at] : NULL);
        return 1;
    default:
        return 0;
    }
}

// the panel's menu as terminal.js gives it: Copy, which needs text chosen, Paste, a line, Clear, New Terminal, Split
// Terminal, on while a shell stands beside the one shown, and Close
static const CliItem s_terminal_menu[CLI_TERMINAL_MENU_ROWS] = {
    {.label = "Copy",
     .command = "terminal-copy",
     .keys = "Ctrl+Shift+C",
     .args = "",
     .needs = "terminal-chosen",
     .checks = ""},
    {.label = "Paste", .command = "terminal-paste", .keys = "Ctrl+Shift+V", .args = "", .needs = "", .checks = ""},
    {.label = "", .command = "", .keys = "", .args = "", .needs = "", .checks = "", .line = 1},
    {.label = "Clear", .command = "clear", .keys = "Ctrl+L", .args = "", .needs = "", .checks = ""},
    {.label = "New Terminal", .command = "new", .keys = "Ctrl+Shift+`", .args = "", .needs = "", .checks = ""},
    {.label = "Split Terminal", .command = "split", .keys = "", .args = "", .needs = "", .checks = "terminal-split"},
    {.label = "Close", .command = "terminal-close", .keys = "Ctrl+`", .args = "", .needs = "", .checks = ""}};

// whether text is chosen in the pane of the tab `tab`: its view's cursor and anchor apart
EMBED_INLINE int terminal_chosen(const TerminalCtx *args)
{
    CliPosition cursor = {0ULL, 0ULL};
    CliPosition anchor = {0ULL, 0ULL};
    return (args->tab != NULL) && args->tab->stored &&
           EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = args->tab->key, .cursor = &cursor,
                      .anchor = &anchor) &&
           ((cursor.line != anchor.line) || (cursor.col != anchor.col));
}

int cli_terminal_tabs_checked(const char *name)
{
    const int at = terminal_target();
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "terminal-chosen", .cap = CLI_LABEL_MAX_LEN}))
    {
        return EMBED_CALL(terminal_chosen, TerminalCtx, .tab = (at >= 0) ? s_terminal.tab[at] : NULL);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "terminal-keys", .cap = CLI_LABEL_MAX_LEN}))
    {
        return s_terminal.open && s_terminal.keys;
    }
    return cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "terminal-split", .cap = CLI_LABEL_MAX_LEN}) &&
           (s_terminal.beside >= 0);
}

const CliItem *cli_terminal_tabs_menu(void)
{
    return s_terminal_menu;
}

int cli_terminal_tabs_key(const char *key)
{
    if (!s_terminal.open || !s_terminal.keys)
    {
        return 0;
    }
    const char *name = key;
    unsigned int mods = 0u;
    for (unsigned int read = 0u; read < 3u; read += 1u)
    {
        const unsigned int mod =
            cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Ctrl+", .cap = 64ULL})    ? CLI_TERMINAL_MOD_CONTROL
            : cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Mod+", .cap = 64ULL})   ? CLI_TERMINAL_MOD_CONTROL
            : cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Alt+", .cap = 64ULL})   ? CLI_TERMINAL_MOD_ALT
            : cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Shift+", .cap = 64ULL}) ? CLI_TERMINAL_MOD_SHIFT
                                                                                              : 0u;
        const unsigned int skipped = (mod == CLI_TERMINAL_MOD_SHIFT) ? 6u
                                                                     : ((mod == 0u)        ? 0u
                                                                        : (name[0] == 'C') ? 5u
                                                                                           : 4u);
        mods |= mod;
        name += skipped;
    }
    const int control = (mods & CLI_TERMINAL_MOD_CONTROL) != 0u;
    const int alt = (mods & CLI_TERMINAL_MOD_ALT) != 0u;
    const int shift = (mods & CLI_TERMINAL_MOD_SHIFT) != 0u;
    const int at = terminal_target();
    TerminalTab *const target = (at >= 0) ? s_terminal.tab[at] : NULL;
    // Ctrl+Shift+V pastes and Ctrl+Shift+C copies what is chosen; Ctrl+C copies where something is chosen, and else
    // goes on to the shell as the interrupt
    if (control && shift && !alt && ((name[0] == 'V') || (name[0] == 'C')) && (name[1] == '\0'))
    {
        if (name[0] == 'V')
        {
            EMBED_CALL(terminal_paste, TerminalCtx, .bytes = NULL);
        }
        (void)((name[0] == 'C') && EMBED_CALL(terminal_copy, TerminalCtx, .tab = target));
        return 1;
    }
    if (control && !shift && !alt && (name[0] == 'C') && (name[1] == '\0') &&
        EMBED_CALL(terminal_copy, TerminalCtx, .tab = target))
    {
        return 1;
    }
    char sent[TERMINAL_KEY_BYTES];
    VerbaLinea out = {sent, sizeof(sent), 0ULL};
    const int application = (target != NULL) && target->sized && target->xterm.application_cursor;
    if (EMBED_CALL(cli_terminal_key.bytes, CliTerminalKeyCfg, .name = name,
                   // a key's name is far shorter than an unsigned int counts
                   .name_size = (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = name, .cap = 64ULL}), .mods = mods,
                   .application_cursor = application, .out = &out) &&
        mmgr_line_ok(&out))
    {
        EMBED_CALL(terminal_send, TerminalCtx, .bytes = sent, .size = out.at);
        return 1;
    }
    // a key with Control the terminal sends nothing for goes on to the rest of the window; any other comes as text
    return !control || alt;
}

int cli_terminal_tabs_text(const char *bytes, unsigned long long length)
{
    if (!s_terminal.open || !s_terminal.keys)
    {
        return 0;
    }
    EMBED_CALL(terminal_send, TerminalCtx, .bytes = bytes, .size = length);
    return 1;
}

// the pointer's event `pointer` at the row `row` and the column `column` handed to the view of the tab `at`
EMBED_INLINE void terminal_point(const TerminalCtx *args)
{
    const TerminalTab *const tab = s_terminal.tab[args->at];
    if (!tab->stored)
    {
        return;
    }
    // the pane's column counts letters alone; the view's counts the line's units, the SGR sequences among them too
    unsigned long long count = 0ULL;
    const unsigned short *const units =
        EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = tab->key,
                   .line = cli_view_top(tab->key) + args->row, .count = &count);
    // a pane's column is far inside what an unsigned int counts
    const unsigned long long column =
        (units != NULL) ? EMBED_CALL(cli_terminal_xterm_column_unit, CliTerminalXtermUnitsCfg, .units = units,
                                     .count = count, .column = (unsigned int)args->column)
                        : args->column;
    const CliViewInput pointed = {.kind = CLI_VIEW_POINTER,
                                  .pointer = args->pointer,
                                  .row = args->row,
                                  .column = column,
                                  .clicks = args->clicks,
                                  .shift = args->shift};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &pointed,
                     .err_msg = &err_msg);
    terminal_dirty();
}

void cli_terminal_tabs_press(const CliTerminalTabsPressCfg *args)
{
    // a place pressed among the tabs is far inside what an int counts
    const int at = (args->index < s_terminal.tabs) ? (int)args->index : -1;
    switch (args->hit)
    {
    case CLI_TERMINAL_HIT_PANE: {
        const int viewed = EMBED_CALL(terminal_viewed, TerminalCtx, .view = (args->index != 0u) ? 1u : 0u);
        if (viewed >= 0)
        {
            EMBED_CALL(terminal_focus, TerminalCtx, .at = viewed);
            s_terminal.choosing = viewed;
            EMBED_CALL(terminal_point, TerminalCtx, .at = viewed, .row = args->row, .column = args->column,
                       .clicks = args->clicks, .shift = args->shift, .pointer = CLI_VIEW_POINTER_DOWN);
        }
        break;
    }
    case CLI_TERMINAL_HIT_TAB:
        if (at >= 0)
        {
            EMBED_CALL(terminal_show, TerminalCtx, .at = at);
        }
        break;
    case CLI_TERMINAL_HIT_TAB_CLOSE:
        if (at >= 0)
        {
            EMBED_CALL(terminal_close, TerminalCtx, .at = at, .on = 1);
        }
        break;
    case CLI_TERMINAL_HIT_NEW:
        (void)cli_terminal_tabs_command("new", NULL, 0u);
        break;
    case CLI_TERMINAL_HIT_SPLIT:
        terminal_split();
        break;
    default:
        s_terminal.keys = 0;
        terminal_dirty();
        break;
    }
}

int cli_terminal_tabs_pointer(const CliTerminalTabsPressCfg *args)
{
    if ((s_terminal.choosing < 0) || (s_terminal.choosing >= (int)s_terminal.tabs))
    {
        return 0;
    }
    EMBED_CALL(terminal_point, TerminalCtx, .at = s_terminal.choosing, .row = args->row, .column = args->column,
               .clicks = args->clicks, .shift = args->shift,
               .pointer = args->up ? CLI_VIEW_POINTER_UP : CLI_VIEW_POINTER_MOVED);
    s_terminal.choosing = args->up ? -1 : s_terminal.choosing;
    return 1;
}

void cli_terminal_tabs_wheel(unsigned int view, int rows)
{
    const int at = EMBED_CALL(terminal_viewed, TerminalCtx, .view = (view != 0u) ? 1u : 0u);
    if ((at < 0) || !s_terminal.tab[at]->stored)
    {
        return;
    }
    TerminalTab *const tab = s_terminal.tab[at];
    // the view's top moved by the wheel's rows, down where positive, between the text's first line and the first line
    // of the screen drawn on
    const CliViewInput wheeled = {.kind = CLI_VIEW_WHEEL, .wheel_rows = rows, .top = tab->xterm.base};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = tab->key, .input = &wheeled,
                     .err_msg = &err_msg);
    terminal_dirty();
}

void cli_terminal_tabs_size(unsigned int view, unsigned int columns, unsigned int rows)
{
    const unsigned int at = (view != 0u) ? 1u : 0u;
    s_terminal.columns[at] = columns;
    s_terminal.rows[at] = rows;
    EMBED_CALL(terminal_fit, TerminalCtx, .view = at);
}

void cli_terminal_tabs_height(unsigned int height)
{
    s_terminal.height = height;
    terminal_dirty();
}

// the place stored as a value of orior's folder: whether the panel is open, the tab shown and the one beside it, each
// one past its place and 0 for none, and the height on the first line, then each tab's number and folder a line.
// Nothing is stored before the place was read back, which it would write over
EMBED_INLINE void terminal_place_save(void)
{
    s_terminal.place_ms = 0ULL;
    if (!s_terminal.restored)
    {
        return;
    }
    const unsigned long long bytes = 64ULL + ((unsigned long long)s_terminal.tabs * (CLI_PATH_MAX_LEN + 16ULL));
    char *const text = g_cli_guard->persistent_buf_alloc(bytes, g_cli_terminal_owner);
    if (text == NULL)
    {
        return;
    }
    VerbaLinea out = {text, bytes, 0ULL};
    mmgr_line_decimal(&out, s_terminal.open ? 1ULL : 0ULL, 1u);
    mmgr_line_ch(&out, ' ');
    // a place among the tabs, or -1 for none, one past it is far inside what an unsigned long long counts
    mmgr_line_decimal(&out, (unsigned long long)(s_terminal.shown + 1), 1u);
    mmgr_line_ch(&out, ' ');
    mmgr_line_decimal(&out, (unsigned long long)(s_terminal.beside + 1), 1u);
    mmgr_line_ch(&out, ' ');
    mmgr_line_decimal(&out, s_terminal.height, 1u);
    mmgr_line_ch(&out, '\n');
    for (unsigned int at = 0u; at < s_terminal.tabs; at += 1u)
    {
        mmgr_line_decimal(&out, s_terminal.tab[at]->number, 1u);
        mmgr_line_ch(&out, ' ');
        mmgr_line_put(&out, s_terminal.tab[at]->folder);
        mmgr_line_ch(&out, '\n');
    }
    char folder[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (mmgr_line_ok(&out) && (mmgr_line_finish(&out) != 0ULL) &&
        EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        (void)cli_state_value_save(
            &(CliStateValueCfg){.folder = folder, .key = TERMINAL_PLACE_KEY, .value = text, .err_msg = &err_msg});
    }
    g_cli_guard->persistent_buf_release(text, g_cli_terminal_owner);
}

// the tab's text as the window ends written as its history's file, from its first line to the main screen's foot, the
// empty rows at that foot left off, so the tab of its number reads them back with the rest
EMBED_INLINE void terminal_rows_file(const TerminalCtx *args)
{
    TerminalTab *const tab = args->tab;
    const CliTerminalXterm *const xterm = &tab->xterm;
    if (!tab->sized || !tab->stored)
    {
        return;
    }
    char state[8];
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.lines_alloc, CliDocumentKeyCfg, .key = tab->key, .first = 0ULL,
                                  .last = xterm->history + xterm->rows - 1ULL, .size_pointer = &size, .state = state,
                                  .capacity = sizeof(state), .owner = g_cli_terminal_owner);
    if (text == NULL)
    {
        return;
    }
    while ((size > 0ULL) && (text[size - 1ULL] == '\n'))
    {
        size -= 1ULL;
    }
    (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = tab->history_path, .bytes = text,
                     .size = size, .fresh = 1);
    if (size != 0ULL)
    {
        (void)EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = tab->history_path, .bytes = "\n",
                         .size = 1ULL, .fresh = 0);
    }
    g_cli_guard->persistent_buf_release(text, g_cli_terminal_owner);
}

int cli_terminal_tabs_step(void)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if ((s_terminal.place_ms != 0ULL) && (now_ms >= s_terminal.place_ms))
    {
        terminal_place_save();
    }
    int changed = 0;
    for (unsigned int at = 0u; at < s_terminal.tabs; at += 1u)
    {
        TerminalTab *const tab = s_terminal.tab[at];
        const int follows = EMBED_CALL(terminal_follows, TerminalCtx, .tab = tab);
        const int drained = EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &tab->shell,
                                       .xterm = tab->sized ? &tab->xterm : NULL, .now_ms = now_ms);
        const int moved = tab->sized && tab->xterm.moved;
        tab->xterm.moved = 0;
        if (moved && follows)
        {
            EMBED_CALL(terminal_scroll, TerminalCtx, .tab = tab, .first = tab->xterm.base);
        }
        // a folder the shell reported stands in the tab's folder already, the xterm having written it there
        changed = drained || moved || tab->xterm.folder_reported || changed;
        tab->xterm.folder_reported = 0;
    }
    unsigned int left = 0u;
    for (unsigned int at = 0u; at < s_terminal.closings; at += 1u)
    {
        TerminalTab *const tab = s_terminal.closing[at];
        (void)EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &tab->shell, .xterm = NULL,
                         .now_ms = now_ms);
        if (tab->shell.state == CLI_TERMINAL_SHELL_RUNNING)
        {
            s_terminal.closing[left] = tab;
            left += 1u;
        }
        else
        {
            g_cli_guard->persistent_buf_release(tab->xterm.cell, g_cli_terminal_owner);
            tab->xterm.cell = NULL;
            g_cli_guard->persistent_buf_release(tab, g_cli_terminal_owner);
        }
    }
    s_terminal.closings = left;
    if (changed)
    {
        terminal_dirty();
    }
    return changed;
}

unsigned long long cli_terminal_tabs_next_ms(void)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    unsigned long long earliest = s_terminal.place_ms;
    for (unsigned int at = 0u; at < (s_terminal.tabs + s_terminal.closings); at += 1u)
    {
        TerminalTab *const tab = (at < s_terminal.tabs) ? s_terminal.tab[at] : s_terminal.closing[at - s_terminal.tabs];
        const unsigned long long next =
            EMBED_CALL(cli_terminal_shell.next_ms, CliTerminalShellCfg, .shell = &tab->shell, .now_ms = now_ms);
        earliest = ((next != 0ULL) && ((earliest == 0ULL) || (next < earliest))) ? next : earliest;
    }
    return earliest;
}

CliTerminalPanel cli_terminal_tabs_panel(void)
{
    const CliTerminalPanel panel = {s_terminal.open,
                                    s_terminal.keys,
                                    s_terminal.height,
                                    s_terminal.tabs,
                                    s_terminal.shown,
                                    s_terminal.beside,
                                    (s_terminal.beside >= 0) ? 2u : ((s_terminal.shown >= 0) ? 1u : 0u)};
    return panel;
}

int cli_terminal_tabs_view(unsigned int view, CliTerminalView *out)
{
    const int at = EMBED_CALL(terminal_viewed, TerminalCtx, .view = (view != 0u) ? 1u : 0u);
    if (at < 0)
    {
        return 0;
    }
    const TerminalTab *const tab = s_terminal.tab[at];
    const CliTerminalXterm *const xterm = &tab->xterm;
    *out = (CliTerminalView){.key = tab->stored ? tab->key : NULL,
                             .xterm_line = xterm->base,
                             .columns = tab->sized ? xterm->columns : 0u,
                             .rows = tab->sized ? xterm->rows : 0u,
                             .cursor_x = xterm->cursor.x,
                             .cursor_y = xterm->cursor.y,
                             .cursor_shown = xterm->cursor_shown,
                             .focused = at == s_terminal.focused};
    return 1;
}

int cli_terminal_tabs_label(unsigned int at, VerbaLinea *out)
{
    if (at >= s_terminal.tabs)
    {
        return 0;
    }
    const TerminalTab *const tab = s_terminal.tab[at];
    mmgr_line_decimal(out, at + 1ULL, 1u);
    mmgr_line_ch(out, ' ');
    if (!tab->sized || !tab->xterm.folder_known ||
        !EMBED_CALL(terminal_folder, TerminalCtx, .place = tab->folder, .out = out))
    {
        mmgr_line_put(out, "shell");
    }
    return tab->shell.state == CLI_TERMINAL_SHELL_EXITED;
}

// a decimal number read from `size` bytes of `bytes` at `*read_at`, `*read_at` moved past it and one space after it:
// the number, 0 where no digit stands there
EMBED_INLINE unsigned long long terminal_number_read(const TerminalCtx *args)
{
    unsigned long long *const at = args->read_at;
    unsigned long long number = 0ULL;
    while ((*at < args->size) && (args->bytes[*at] >= '0') && (args->bytes[*at] <= '9') && (number < 0xFFFFFFFFULL))
    {
        // a digit's value is far inside what an unsigned long long counts
        number = (number * 10ULL) + (unsigned long long)(args->bytes[*at] - '0');
        *at += 1ULL;
    }
    *at += ((*at < args->size) && (args->bytes[*at] == ' ')) ? 1ULL : 0ULL;
    return number;
}

void cli_terminal_tabs_restore(const CliStateEntries *entries, const char *root)
{
    const char *const place = (entries != NULL) ? cli_state_entry_value(entries, TERMINAL_PLACE_KEY) : NULL;
    s_terminal.restored = 1;
    s_terminal.root = root;
    if (place == NULL)
    {
        return;
    }
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_POOL_BYTES});
    unsigned long long at = 0ULL;
    const unsigned long long open =
        EMBED_CALL(terminal_number_read, TerminalCtx, .bytes = place, .size = size, .read_at = &at);
    const unsigned long long shown =
        EMBED_CALL(terminal_number_read, TerminalCtx, .bytes = place, .size = size, .read_at = &at);
    const unsigned long long beside =
        EMBED_CALL(terminal_number_read, TerminalCtx, .bytes = place, .size = size, .read_at = &at);
    const unsigned long long height =
        EMBED_CALL(terminal_number_read, TerminalCtx, .bytes = place, .size = size, .read_at = &at);
    while ((at < size) && (place[at] != '\n'))
    {
        at += 1ULL;
    }
    at += 1ULL;
    while (at < size)
    {
        const unsigned long long number =
            EMBED_CALL(terminal_number_read, TerminalCtx, .bytes = place, .size = size, .read_at = &at);
        unsigned long long end = at;
        while ((end < size) && (place[end] != '\n'))
        {
            end += 1ULL;
        }
        int taken = (number == 0ULL) || (number > 0xFFFFULL);
        for (unsigned int tab = 0u; tab < s_terminal.tabs; tab += 1u)
        {
            taken = taken || (s_terminal.tab[tab]->number == number);
        }
        // a tab's number is far inside what an unsigned int counts
        const int added = taken ? -1 : EMBED_CALL(terminal_add, TerminalCtx, .number = (unsigned int)number);
        if (added >= 0)
        {
            TerminalTab *const tab = s_terminal.tab[added];
            VerbaLinea folder = {tab->folder, sizeof(tab->folder), 0ULL};
            mmgr_line_put_n(&folder, place + at, end - at);
            const int folder_read = mmgr_line_ok(&folder) && (mmgr_line_finish(&folder) != 0ULL);
            tab->folder[0] = folder_read ? tab->folder[0] : '\0';
        }
        at = end + 1ULL;
    }
    // a place among the tabs read back one past it, 0 for none, and the tabs, are far inside what an int counts
    const int tabs = (int)s_terminal.tabs;
    const int first = (tabs != 0) ? 0 : -1;
    s_terminal.shown = ((shown != 0ULL) && (shown <= (unsigned long long)tabs)) ? ((int)shown - 1) : first;
    const int beside_at = ((beside != 0ULL) && (beside <= (unsigned long long)tabs)) ? ((int)beside - 1) : -1;
    s_terminal.beside = (beside_at != s_terminal.shown) ? beside_at : -1;
    s_terminal.focused = s_terminal.shown;
    // a height read back is far inside what an unsigned int counts
    s_terminal.height = ((height != 0ULL) && (height <= 0xFFFFULL)) ? (unsigned int)height : TERMINAL_HEIGHT;
    if (open != 0ULL)
    {
        EMBED_CALL(terminal_toggle, TerminalCtx, .on = 1);
    }
    terminal_dirty();
}

void cli_terminal_tabs_end(void)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    terminal_place_save();
    for (unsigned int at = 0u; at < s_terminal.tabs; at += 1u)
    {
        EMBED_CALL(terminal_rows_file, TerminalCtx, .tab = s_terminal.tab[at]);
    }
    // nothing is fitted as the tabs close, so the fresh tab the last close leaves reads back no history its number's
    // file holds
    s_terminal.columns[0] = 0u;
    s_terminal.columns[1] = 0u;
    while (s_terminal.tabs != 0u)
    {
        // each history stays for the tab of its number to read back
        EMBED_CALL(terminal_close, TerminalCtx, .at = (int)s_terminal.tabs - 1, .on = 0);
        // the last tab closed leaves a fresh one, whose shell never started
        if ((s_terminal.tabs == 1u) && (s_terminal.tab[0]->shell.state == CLI_TERMINAL_SHELL_IDLE))
        {
            EMBED_CALL(terminal_unstore, TerminalCtx, .tab = s_terminal.tab[0], .on = 0);
            g_cli_guard->persistent_buf_release((s_terminal.tab[0])->xterm.cell, g_cli_terminal_owner);
            (s_terminal.tab[0])->xterm.cell = NULL;
            g_cli_guard->persistent_buf_release((s_terminal.tab[0]), g_cli_terminal_owner);
            s_terminal.tabs = 0u;
        }
    }
    CliWake pause = CLI_WAKE_INIT;
    while ((s_terminal.closings != 0u) && (cli_clock_milliseconds() < (now_ms + TERMINAL_END_MS)))
    {
        (void)cli_terminal_tabs_step();
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause,
                         .milliseconds_u64 = CLI_TERMINAL_LOOK_MS);
    }
    g_cli_guard->persistent_buf_release(s_terminal.tab, g_cli_terminal_owner);
    s_terminal = (TerminalModel){.shown = -1, .beside = -1, .focused = -1, .choosing = -1, .height = TERMINAL_HEIGHT};
}
