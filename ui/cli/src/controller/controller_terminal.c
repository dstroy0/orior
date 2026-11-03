// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_terminal.c: the terminal's model, on the controller's thread (controller_internal.h): its tabs, each a
// screen and the shell drawing on it in a cell of their own; the tab shown and the one beside it where the panel is
// split; the one the keys go to; whether the panel is open and takes the keys; and its height. Every shell is drained
// each step whether its tab shows or not. A tab closed waits, its shell hung up on, until the shell ends before its
// cell goes. A shell starts the first time its tab shows, in the tree's top folder; one that ended starts again at the
// next key typed into it
#include "controller_internal.h"

#include "../command_line/cells.h"
#include "../command_line/commands.h"
#include "../terminal/keys.h"
#include "../terminal/shells.h"

// the panel's height at first, in pixels at a scale of one; the tabs closed that wait for their shells to end, at the
// most; how long the window's end waits for every shell; the most bytes a paste sends; and the most bytes a key sends
#define TERMINAL_HEIGHT 256u
#define TERMINAL_CLOSING 16u
#define TERMINAL_END_MS 2000ULL
#define TERMINAL_PASTE_MAX (1024u * 1024u)
#define TERMINAL_KEY_BYTES 32u

// One tab: its screen, its shell, and whether its screen has a size yet
typedef struct
{
    CliTerminalScreen screen;
    CliTerminalShell shell;
    int sized;
} TerminalTab;

// The terminal: its tabs and how many the cell of them has places for; the tabs closed whose shells have yet to end;
// the tab shown, the one beside it and the one the keys go to, each -1 for none; whether the panel is open, whether it
// takes the keys and whether the editor took them before it did; its height; and the columns and rows the window last
// said its two views have, 0 before it said
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
    int open;
    int keys;
    int editing_before;
    unsigned int height;
    unsigned int columns[2];
    unsigned int rows[2];
} TerminalModel;

static TerminalModel s_terminal = {.shown = -1, .beside = -1, .focused = -1, .height = TERMINAL_HEIGHT};

typedef struct
{
    int at;
    int taken_out;
    unsigned int view;
    TerminalTab *tab;
    const char *bytes;
    unsigned long long size;
    int on;
    const char *place;
    VerbaLinea *out;
} TerminalCtx;

// the terminal's part marked to be published again
EMBED_INLINE void terminal_dirty(void)
{
    g_cli_model.dirty |= CLI_MODEL_DIRTY(CLI_SNAPSHOT_TERMINAL);
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

// the shell of `tab` started in the tree's top folder, or why it could not be printed on its screen
EMBED_INLINE void terminal_start(const TerminalCtx *args)
{
    TerminalTab *const tab = args->tab;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    mmgr_line_put(&err_msg, "\r\n");
    if (!EMBED_CALL(cli_terminal_shell.start, CliTerminalShellCfg, .shell = &tab->shell, .screen = &tab->screen,
                    .words = NULL, .folder = g_cli_model.root, .err_msg = &err_msg))
    {
        mmgr_line_put(&err_msg, "\r\n");
        EMBED_CALL(cli_terminal_screen_bytes.write, CliTerminalScreenBytesCfg, .screen = &tab->screen,
                   .bytes = (const unsigned char *)why, .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
    }
    terminal_dirty();
}

// the tab the view `view` shows sized as the window said the view is, its shell told; a shell never started started
// where the panel is open
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
    if (!tab->sized)
    {
        tab->sized = EMBED_CALL(cli_terminal_screen_size.init, CliTerminalScreenSizeCfg, .screen = &tab->screen,
                                .columns = columns, .rows = rows);
    }
    else if (((tab->screen.columns != columns) || (tab->screen.rows != rows)) &&
             EMBED_CALL(cli_terminal_screen_size.resize, CliTerminalScreenSizeCfg, .screen = &tab->screen,
                        .columns = columns, .rows = rows))
    {
        EMBED_CALL(cli_terminal_shell.resize, CliTerminalShellCfg, .shell = &tab->shell, .screen = &tab->screen);
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

// a tab added after the rest, its shell not started: its place, or -1 where no cell could be had for it
EMBED_INLINE int terminal_add(void)
{
    if (s_terminal.tabs == s_terminal.capacity)
    {
        const unsigned int capacity = (s_terminal.capacity == 0u) ? 4u : (s_terminal.capacity * 2u);
        TerminalTab **const grown = g_cli_cells->persistent_buf_alloc(capacity * sizeof(*grown), &g_cli_terminal_cells);
        if (grown == NULL)
        {
            return -1;
        }
        if (s_terminal.tab != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_terminal.tab, .bytes = s_terminal.tabs * sizeof(*grown)});
            g_cli_cells->persistent_buf_release(s_terminal.tab, &g_cli_terminal_cells);
        }
        s_terminal.tab = grown;
        s_terminal.capacity = capacity;
    }
    TerminalTab *const tab = g_cli_cells->persistent_buf_alloc(sizeof(*tab), &g_cli_terminal_cells);
    if (tab == NULL)
    {
        return -1;
    }
    memor.set(&(MemoriaCfg){.dst = tab, .val = 0, .bytes = sizeof(*tab)});
    s_terminal.tab[s_terminal.tabs] = tab;
    s_terminal.tabs += 1u;
    terminal_dirty();
    // the tabs are far fewer than an int counts
    return (int)s_terminal.tabs - 1;
}

// a tab whose shell ended let go: its screen's cells and its own
EMBED_INLINE void terminal_free(const TerminalCtx *args)
{
    EMBED_CALL(cli_terminal_screen.free, CliTerminalScreenCfg, .screen = &args->tab->screen);
    g_cli_cells->persistent_buf_release(args->tab, &g_cli_terminal_cells);
}

// the keys sent to the shell of the tab `at`, the editor letting them go
EMBED_INLINE void terminal_focus(const TerminalCtx *args)
{
    s_terminal.focused = args->at;
    if (!s_terminal.keys)
    {
        s_terminal.editing_before = g_cli_model.editing;
    }
    s_terminal.keys = 1;
    EMBED_CALL(cli_model.editing, CliModelCtx, .on = 0);
    terminal_fit_all();
}

// the panel opened, with a tab where there is none, its shells started as they show, taking the keys where `on` is 2;
// or closed where `on` is 0, the keys handed back to the editor where it took them before
EMBED_INLINE void terminal_toggle(const TerminalCtx *args)
{
    s_terminal.open = args->on != 0;
    terminal_dirty();
    if (!s_terminal.open)
    {
        if (s_terminal.keys && s_terminal.editing_before)
        {
            EMBED_CALL(cli_model.editing, CliModelCtx, .on = 1);
        }
        s_terminal.keys = 0;
        return;
    }
    if (s_terminal.tabs == 0u)
    {
        s_terminal.shown = terminal_add();
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
        (void)EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &args->tab->shell, .screen = NULL,
                         .now_ms = cli_clock_milliseconds());
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause,
                         .milliseconds_u64 = CLI_TERMINAL_LOOK_MS);
    }
    if (args->tab->shell.state != CLI_TERMINAL_SHELL_RUNNING)
    {
        EMBED_CALL(terminal_free, TerminalCtx, .tab = args->tab);
    }
}

// the tab `at` closed, its shell hung up on and its cell let go once the shell ends; the last tab closed closes the
// panel and a fresh one waits in its place
EMBED_INLINE void terminal_close(const TerminalCtx *args)
{
    // the tab is among the tabs
    const unsigned int out = (unsigned int)args->at;
    TerminalTab *const tab = s_terminal.tab[out];
    EMBED_CALL(cli_terminal_shell.end, CliTerminalShellCfg, .shell = &tab->shell, .now_ms = cli_clock_milliseconds());
    if (tab->shell.state != CLI_TERMINAL_SHELL_RUNNING)
    {
        EMBED_CALL(terminal_free, TerminalCtx, .tab = tab);
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
        s_terminal.shown = terminal_add();
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
    other = (other >= 0) ? other : terminal_add();
    if (other >= 0)
    {
        s_terminal.beside = other;
        EMBED_CALL(terminal_focus, TerminalCtx, .at = other);
    }
}

// `size` bytes of `bytes` sent to the shell the keys go to, a shell that ended started again first
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
    EMBED_CALL(cli_terminal_shell.write, CliTerminalShellCfg, .shell = &tab->shell,
               .bytes = (const unsigned char *)args->bytes, .size = args->size);
}

// a paste sent as a shell takes one: `size` bytes of `bytes` or, where `bytes` is NULL, the clipboard's text, each line
// ended as Enter ends it, and marked as a paste where the program in the terminal asked for that
EMBED_INLINE void terminal_paste(const TerminalCtx *args)
{
    const int at = terminal_target();
    char *const text = (at >= 0) ? g_cli_cells->persistent_buf_alloc(TERMINAL_PASTE_MAX, &g_cli_terminal_cells) : NULL;
    if (text == NULL)
    {
        return;
    }
    VerbaLinea line = {text, TERMINAL_PASTE_MAX, 0ULL};
    const int bracketed = s_terminal.tab[at]->screen.bracketed_paste;
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
    g_cli_cells->persistent_buf_release(text, &g_cli_terminal_cells);
}

int cli_model_terminal_command(const CliModelCtx *args)
{
    static const char *const s_commands[9] = {"terminal-view", "toggle", "terminal-close", "new",          "split",
                                              "clear",         "kill",   "terminal-paste", "terminal-copy"};
    unsigned int command = 0u;
    while ((command < 9u) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = s_commands[command], .cap = CLI_LABEL_MAX_LEN}))
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
        const int added = terminal_add();
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
            EMBED_CALL(terminal_close, TerminalCtx, .at = at);
        }
        return 1;
    case 7u: {
        // the words handed to the paste, a space between each, else the clipboard's text
        char words[CLI_PATH_MAX_LEN];
        VerbaLinea line = {words, sizeof(words), 0ULL};
        for (unsigned int word = 0u; word < args->at; word += 1u)
        {
            mmgr_line_put(&line, (word != 0u) ? " " : "");
            mmgr_line_put(&line, args->words[word]);
        }
        EMBED_CALL(terminal_paste, TerminalCtx, .bytes = (args->at != 0u) ? words : NULL,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        return 1;
    }
    case 8u:
        return 1;
    default:
        return 0;
    }
}

int cli_model_terminal_key(const CliModelCtx *args)
{
    if (!s_terminal.open || !s_terminal.keys)
    {
        return 0;
    }
    const char *name = args->path;
    unsigned int mods = 0u;
    for (unsigned int read = 0u; read < 3u; read += 1u)
    {
        const unsigned int mod =
            cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Ctrl+", .cap = 64ULL})    ? CLI_INPUT_MOD_CONTROL
            : cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Mod+", .cap = 64ULL})   ? CLI_INPUT_MOD_CONTROL
            : cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Alt+", .cap = 64ULL})   ? CLI_INPUT_MOD_ALT
            : cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "Shift+", .cap = 64ULL}) ? CLI_INPUT_MOD_SHIFT
                                                                                              : 0u;
        const unsigned int skipped = (mod == CLI_INPUT_MOD_SHIFT) ? 6u
                                                                  : ((mod == 0u)        ? 0u
                                                                     : (name[0] == 'C') ? 5u
                                                                                        : 4u);
        mods |= mod;
        name += skipped;
    }
    const int control = (mods & CLI_INPUT_MOD_CONTROL) != 0u;
    const int alt = (mods & CLI_INPUT_MOD_ALT) != 0u;
    const int shift = (mods & CLI_INPUT_MOD_SHIFT) != 0u;
    if (control && shift && !alt && ((name[0] == 'V') || (name[0] == 'C')) && (name[1] == '\0'))
    {
        if (name[0] == 'V')
        {
            EMBED_CALL(terminal_paste, TerminalCtx, .bytes = NULL);
        }
        return 1;
    }
    const int at = terminal_target();
    char sent[TERMINAL_KEY_BYTES];
    VerbaLinea out = {sent, sizeof(sent), 0ULL};
    const int application = (at >= 0) && s_terminal.tab[at]->sized && s_terminal.tab[at]->screen.application_cursor;
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

int cli_model_terminal_text(const CliModelCtx *args)
{
    if (!s_terminal.open || !s_terminal.keys)
    {
        return 0;
    }
    EMBED_CALL(terminal_send, TerminalCtx, .bytes = args->path, .size = args->length);
    return 1;
}

void cli_model_terminal_press(const CliModelCtx *args)
{
    const CliInput *const input = (const CliInput *)args->input;
    // a place pressed among the tabs is far inside what an int counts
    const int at = (input->index < s_terminal.tabs) ? (int)input->index : -1;
    switch (input->hit)
    {
    case CLI_HIT_TERMINAL: {
        const int viewed = EMBED_CALL(terminal_viewed, TerminalCtx, .view = (input->index != 0u) ? 1u : 0u);
        if (viewed >= 0)
        {
            EMBED_CALL(terminal_focus, TerminalCtx, .at = viewed);
        }
        break;
    }
    case CLI_HIT_TERMINAL_TAB:
        if (at >= 0)
        {
            EMBED_CALL(terminal_show, TerminalCtx, .at = at);
        }
        break;
    case CLI_HIT_TERMINAL_TAB_CLOSE:
        if (at >= 0)
        {
            EMBED_CALL(terminal_close, TerminalCtx, .at = at);
        }
        break;
    case CLI_HIT_TERMINAL_NEW:
        (void)cli_model_terminal_command(&(CliModelCtx){.path = "new"});
        break;
    case CLI_HIT_TERMINAL_SPLIT:
        terminal_split();
        break;
    default:
        s_terminal.keys = 0;
        terminal_dirty();
        break;
    }
}

void cli_model_terminal_size(const CliModelCtx *args)
{
    const CliInput *const input = (const CliInput *)args->input;
    if (input->kind == CLI_INPUT_TERMINAL_HEIGHT)
    {
        s_terminal.height = input->rows;
        terminal_dirty();
        return;
    }
    const unsigned int view = (input->index != 0u) ? 1u : 0u;
    s_terminal.columns[view] = input->columns;
    s_terminal.rows[view] = input->rows;
    EMBED_CALL(terminal_fit, TerminalCtx, .view = view);
}

int cli_model_terminal_step(const CliModelCtx *args)
{
    (void)args;
    const unsigned long long now_ms = cli_clock_milliseconds();
    int changed = 0;
    for (unsigned int at = 0u; at < s_terminal.tabs; at += 1u)
    {
        TerminalTab *const tab = s_terminal.tab[at];
        changed = EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &tab->shell,
                             .screen = tab->sized ? &tab->screen : NULL, .now_ms = now_ms) ||
                  changed;
        changed = tab->screen.place_reported || changed;
        tab->screen.place_reported = 0;
    }
    unsigned int left = 0u;
    for (unsigned int at = 0u; at < s_terminal.closings; at += 1u)
    {
        TerminalTab *const tab = s_terminal.closing[at];
        (void)EMBED_CALL(cli_terminal_shell.drain, CliTerminalShellCfg, .shell = &tab->shell, .screen = NULL,
                         .now_ms = now_ms);
        if (tab->shell.state == CLI_TERMINAL_SHELL_RUNNING)
        {
            s_terminal.closing[left] = tab;
            left += 1u;
        }
        else
        {
            EMBED_CALL(terminal_free, TerminalCtx, .tab = tab);
        }
    }
    s_terminal.closings = left;
    if (changed)
    {
        terminal_dirty();
    }
    return changed;
}

unsigned long long cli_model_terminal_next_ms(const CliModelCtx *args)
{
    (void)args;
    const unsigned long long now_ms = cli_clock_milliseconds();
    unsigned long long earliest = 0ULL;
    for (unsigned int at = 0u; at < (s_terminal.tabs + s_terminal.closings); at += 1u)
    {
        TerminalTab *const tab = (at < s_terminal.tabs) ? s_terminal.tab[at] : s_terminal.closing[at - s_terminal.tabs];
        const unsigned long long next =
            EMBED_CALL(cli_terminal_shell.next_ms, CliTerminalShellCfg, .shell = &tab->shell, .now_ms = now_ms);
        earliest = ((next != 0ULL) && ((earliest == 0ULL) || (next < earliest))) ? next : earliest;
    }
    return earliest;
}

// a hexadecimal digit's value, or 16 where `at` is none
EMBED_INLINE unsigned int terminal_hex(const TerminalCtx *args)
{
    const char digit = args->bytes[args->at];
    return ((digit >= '0') && (digit <= '9'))   ? (unsigned int)(digit - '0')
           : ((digit >= 'a') && (digit <= 'f')) ? (unsigned int)(digit - 'a' + 10)
           : ((digit >= 'A') && (digit <= 'F')) ? (unsigned int)(digit - 'A' + 10)
                                                : 16u;
}

// the last folder of the place `place` a shell reported, `file://host/path`, its %XX escapes read, put after `out`: 1,
// or 0 where it names none
EMBED_INLINE int terminal_folder(const TerminalCtx *args)
{
    const char *const place = args->place;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_TERMINAL_PLACE_MAX});
    unsigned long long from = 0ULL;
    if (cellul.starts(&(CatenaFinitaCfg){.src = place, .other = "file://", .cap = CLI_TERMINAL_PLACE_MAX}))
    {
        from = 7ULL;
        while ((from < size) && (place[from] != '/'))
        {
            from += 1ULL;
        }
    }
    unsigned long long end = size;
    while ((end > from) && ((place[end - 1ULL] == '/') || (place[end - 1ULL] == '\\')))
    {
        end -= 1ULL;
    }
    unsigned long long start = end;
    while ((start > from) && (place[start - 1ULL] != '/') && (place[start - 1ULL] != '\\'))
    {
        start -= 1ULL;
    }
    for (unsigned long long at = start; at < end; at += 1ULL)
    {
        // a place's length is far inside what an int counts
        const unsigned int high =
            ((at + 2ULL) < end) ? EMBED_CALL(terminal_hex, TerminalCtx, .bytes = place, .at = (int)at + 1) : 16u;
        const unsigned int low =
            (high < 16u) ? EMBED_CALL(terminal_hex, TerminalCtx, .bytes = place, .at = (int)at + 2) : 16u;
        const int escaped = (place[at] == '%') && (low < 16u);
        // two hexadecimal digits make a byte
        mmgr_line_ch(args->out, escaped ? (char)((high * 16u) + low) : place[at]);
        at += escaped ? 2ULL : 0ULL;
    }
    return end > start;
}

void *cli_model_terminal_alloc(const CliModelCtx *args)
{
    (void)args;
    const unsigned int views = (s_terminal.beside >= 0) ? 2u : ((s_terminal.shown >= 0) ? 1u : 0u);
    unsigned long long cells = 0ULL;
    for (unsigned int view = 0u; view < views; view += 1u)
    {
        const TerminalTab *const tab = s_terminal.tab[EMBED_CALL(terminal_viewed, TerminalCtx, .view = view)];
        cells += tab->sized ? ((unsigned long long)tab->screen.columns * tab->screen.rows) : 0ULL;
    }
    unsigned long long label_bytes = 0ULL;
    for (unsigned int at = 0u; at < s_terminal.tabs; at += 1u)
    {
        const TerminalTab *const tab = s_terminal.tab[at];
        label_bytes +=
            24ULL + (tab->sized
                         ? cellul.len(&(CatenaFinitaCfg){.src = tab->screen.place, .cap = CLI_TERMINAL_PLACE_MAX})
                         : 0ULL);
    }
    CliTerminalPanel *const panel = g_cli_cells->persistent_buf_alloc(
        sizeof(*panel) + (s_terminal.tabs * sizeof(CliTerminalTab)) + (cells * sizeof(CliTerminalCell)) + label_bytes,
        &g_cli_controller_cells);
    if (panel == NULL)
    {
        return NULL;
    }
    *panel = (CliTerminalPanel){.open = s_terminal.open,
                                .keys = s_terminal.keys,
                                .height = s_terminal.height,
                                .tabs = s_terminal.tabs,
                                .tab = (CliTerminalTab *)(void *)(panel + 1),
                                .shown = s_terminal.shown,
                                .beside = s_terminal.beside,
                                .views = views};
    CliTerminalCell *cell = (CliTerminalCell *)(void *)(panel->tab + s_terminal.tabs);
    for (unsigned int view = 0u; view < views; view += 1u)
    {
        const int at = EMBED_CALL(terminal_viewed, TerminalCtx, .view = view);
        const CliTerminalScreen *const screen = &s_terminal.tab[at]->screen;
        const unsigned long long count =
            s_terminal.tab[at]->sized ? ((unsigned long long)screen->columns * screen->rows) : 0ULL;
        panel->view[view] = (CliTerminalView){.columns = (count != 0ULL) ? screen->columns : 0u,
                                              .rows = (count != 0ULL) ? screen->rows : 0u,
                                              .cell = cell,
                                              .cursor_x = screen->cursor.x,
                                              .cursor_y = screen->cursor.y,
                                              .cursor_shown = screen->cursor_shown,
                                              .focused = at == s_terminal.focused};
        if (count != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = cell, .src = screen->shown, .bytes = count * sizeof(*cell)});
        }
        cell += count;
    }
    char *text = (char *)(void *)cell;
    for (unsigned int at = 0u; at < s_terminal.tabs; at += 1u)
    {
        const TerminalTab *const tab = s_terminal.tab[at];
        const unsigned long long left = label_bytes - (unsigned long long)(text - (char *)(void *)cell);
        VerbaLinea label = {text, left, 0ULL};
        mmgr_line_decimal(&label, at + 1ULL, 1u);
        mmgr_line_ch(&label, ' ');
        if (!tab->sized || !EMBED_CALL(terminal_folder, TerminalCtx, .place = tab->screen.place, .out = &label))
        {
            mmgr_line_put(&label, "shell");
        }
        (void)mmgr_line_finish(&label);
        panel->tab[at] = (CliTerminalTab){.label = text, .exited = tab->shell.state == CLI_TERMINAL_SHELL_EXITED};
        text += label.at + 1ULL;
    }
    return panel;
}

void cli_model_terminal_end(const CliModelCtx *args)
{
    (void)args;
    const unsigned long long now_ms = cli_clock_milliseconds();
    while (s_terminal.tabs != 0u)
    {
        EMBED_CALL(terminal_close, TerminalCtx, .at = (int)s_terminal.tabs - 1);
        // the last tab closed leaves a fresh one, whose shell never started
        if ((s_terminal.tabs == 1u) && (s_terminal.tab[0]->shell.state == CLI_TERMINAL_SHELL_IDLE))
        {
            EMBED_CALL(terminal_free, TerminalCtx, .tab = s_terminal.tab[0]);
            s_terminal.tabs = 0u;
        }
    }
    CliWake pause = CLI_WAKE_INIT;
    while ((s_terminal.closings != 0u) && (cli_clock_milliseconds() < (now_ms + TERMINAL_END_MS)))
    {
        (void)cli_model_terminal_step(args);
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause,
                         .milliseconds_u64 = CLI_TERMINAL_LOOK_MS);
    }
    g_cli_cells->persistent_buf_release(s_terminal.tab, &g_cli_terminal_cells);
    s_terminal = (TerminalModel){.shown = -1, .beside = -1, .focused = -1, .height = TERMINAL_HEIGHT};
}
