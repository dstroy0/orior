// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// terminal_tabs.h: the terminal's tabs, on the window's thread: each a shell, its xterm and a text stored with a view
// on it, the panel that shows one or two of them, and the place each window starts from (terminal_tabs.c)
#ifndef CLI_TERMINAL_TABS_H
#define CLI_TERMINAL_TABS_H
#include "../command_line/commands.h"
#include "../machine/state.h"
#include "shells.h"
#include "xterm.h"

typedef struct CliTerminalTabsPressCfg CliTerminalTabsPressCfg;

// what a press on the panel stands on: a pane, `index` 0 the one shown and 1 the one beside it; a tab or its ×,
// `index` its place; New Terminal; Split; and the panel on none of these
#define CLI_TERMINAL_HIT_PANE 1u
#define CLI_TERMINAL_HIT_TAB 2u
#define CLI_TERMINAL_HIT_TAB_CLOSE 3u
#define CLI_TERMINAL_HIT_NEW 4u
#define CLI_TERMINAL_HIT_SPLIT 5u
#define CLI_TERMINAL_HIT_PANEL 6u

// the rows of the panel's menu
#define CLI_TERMINAL_MENU_ROWS 7u

// The panel: whether it is open and whether it takes the keys; its height in pixels at a scale of one; how many tabs
// it has; the tab shown and the one beside it, -1 for none; and how many panes it shows
typedef struct
{
    int open;
    int keys;
    unsigned int height;
    unsigned int tabs;
    int shown;
    int beside;
    unsigned int views;
} CliTerminalPanel;

// One pane: the key of its tab's text, NULL before it is stored; the text's line the xterm's rows start on; the
// xterm's columns and rows, 0 before it is sized, its cells row by row as it stands, the cursor's column and row and
// whether it is shown; and whether its shell is the one the keys go to
typedef struct
{
    const char *key;
    unsigned long long xterm_line;
    unsigned int columns;
    unsigned int rows;
    const CliTerminalCell *cell;
    unsigned int cursor_x;
    unsigned int cursor_y;
    int cursor_shown;
    int focused;
} CliTerminalView;

// the place read back from `entries`, NULL for none, the tree's top folder `root` the folder a shell with none of its
// own starts in; and every shell hung up on and its tabs let go, the place and each tab's rows stored for the next
// start
void cli_terminal_tabs_restore(const CliStateEntries *entries, const char *root);
void cli_terminal_tabs_end(void);
// the terminal's command `name` run with its `count` `words`: 1, or 0 where it is none of the terminal's; whether its
// state `name` is on, terminal-split while a shell stands beside the one shown, terminal-chosen while text is chosen in
// the pane the keys go to and terminal-keys while the panel takes the keys; and the panel's menu, its
// CLI_TERMINAL_MENU_ROWS rows, standing as long as the program runs
int cli_terminal_tabs_command(const char *name, const char *const *words, unsigned int count);
int cli_terminal_tabs_checked(const char *name);
const CliItem *cli_terminal_tabs_menu(void);
// the key `name` taken where the panel takes the keys: 1, or 0 where it goes on to the rest of the window; and
// `length` bytes of text typed where the panel takes the keys: 1, or 0 where it does not
int cli_terminal_tabs_key(const char *name);
int cli_terminal_tabs_text(const char *bytes, unsigned long long length);
// a press on what `hit` names, a CLI_TERMINAL_HIT_*, elsewhere taking the keys from the panel; the pointer moved or let
// go after a press on a pane, handed to the pane's view: 1, or 0 where no press on a pane came before it; the wheel's
// `rows` over the pane `view`; the pane `view` sized to `columns` and `rows`; and the panel's height
void cli_terminal_tabs_press(const CliTerminalTabsPressCfg *args);
int cli_terminal_tabs_pointer(const CliTerminalTabsPressCfg *args);
void cli_terminal_tabs_wheel(unsigned int view, int rows);
void cli_terminal_tabs_size(unsigned int view, unsigned int columns, unsigned int rows);
void cli_terminal_tabs_height(unsigned int height);
// every shell drained and the place stored once its time comes: 1 where an xterm changed; and the clock's reading of
// its next timer, 0 for none
int cli_terminal_tabs_step(void);
unsigned long long cli_terminal_tabs_next_ms(void);
// the panel as it stands; the pane `view` into `out`: 1, or 0 where it shows no tab; and the label of the tab `at`
// put after `out`: 1 where its shell ended, else 0
CliTerminalPanel cli_terminal_tabs_panel(void);
int cli_terminal_tabs_view(unsigned int view, CliTerminalView *out);
int cli_terminal_tabs_label(unsigned int at, VerbaLinea *out);

struct CliTerminalTabsPressCfg
{
    unsigned int hit;
    unsigned int index;
    unsigned long long row;
    unsigned long long column;
    unsigned int clicks;
    int shift;
    int up;
};

#endif
