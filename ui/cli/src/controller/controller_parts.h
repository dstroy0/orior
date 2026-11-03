// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_parts.h: what the controller publishes of each part, a snapshot in one cell of the CLI's, its struct at
// the head and its arrays and texts after it, immutable once published. The window reads them and draws; the frame and
// the map are the editor view's (editor/view.h, editor/document.h)
#ifndef CONTROLLER_PARTS_H
#define CONTROLLER_PARTS_H
#include "../editor/view.h"
#include "../terminal/xterm.h"
#include "../text/edits.h"

// the groups a tool stands in
#define CLI_TOOL_TOP 0u
#define CLI_TOOL_FOOTER 1u

// A tool of the strip: its name, its icon's name, its label, its keys, empty where it has none, its group, whether it
// opens a menu in place of a window of its own, and whether its window is the side pane beside the editor
typedef struct
{
    const char *name;
    const char *glyph;
    const char *label;
    const char *keys;
    unsigned int group;
    int menu;
    int side;
} CliTool;

// the tools, the top group then the foot group, and how many (controller_model.c)
extern const CliTool g_cli_tools[];
extern const unsigned int g_cli_tools_count;

// One tab: the file's path from the tree's top and the name it shows, the last part of that path
typedef struct
{
    const char *path;
    const char *name;
} CliTabRow;

// The files open: how many, the one shown, -1 for none
typedef struct
{
    unsigned int count;
    int active;
    CliTabRow *row;
} CliTabs;

// what the wordmark does as the session is known: nothing yet, stand on the first start in the session, or go home at
// once
#define CLI_WORDMARK_UNKNOWN 0u
#define CLI_WORDMARK_STAND 1u
#define CLI_WORDMARK_HOME 2u

// The window's standing: the tree's name; the file shown, empty for none; whether its view is open and its lines end in
// a carriage return and a newline; whether the editor takes the keys; the strip's tool shown, -1 for none, and whether
// its Problems tool is marked; whether an ask is under way, a check among them; whether the window is to close; whether
// Preferences' window stands open; what orior holds in RAM; what the wordmark does, and whether the tree is read for
// it; and a count that moves as another file is shown
typedef struct
{
    const char *title;
    const char *path;
    int viewed;
    int crlf;
    int editing;
    int tool;
    int problems_marked;
    int busy;
    int closing;
    int preferences_displayed;
    unsigned long long memory_bytes;
    unsigned int wordmark;
    int tree_read;
    unsigned long long shown;
} CliStatus;

// One color a setting changes: its name as the palette names it and its value as the palette reads it
typedef struct
{
    char *name;
    const char *value;
} CliSettingColor;

// What orior's folder saves that the window draws with: the zoom's percent, the bar's and the editor's opacity, whether
// View's Reduce Motion is on, and the colors that differ from the scheme
typedef struct
{
    unsigned int zoom_percent;
    unsigned int bar_percent;
    unsigned int editor_percent;
    int motion_saved;
    unsigned int colors;
    CliSettingColor *color;
} CliSettings;

// a row of a menu: a line, an item, or an item with items of its own
#define CLI_MENU_ROW_LINE 0u
#define CLI_MENU_ROW_ITEM 1u
#define CLI_MENU_ROW_MORE 2u

// One row of an open menu: its kind, its label and its keys, empty where none, whether it cannot be chosen, and the
// state it shows, 1 on and 0 off, -1 where it shows none
typedef struct
{
    unsigned int kind;
    const char *label;
    const char *keys;
    int disabled;
    int checked;
} CliMenuRow;

// One open menu: its rows, the row marked and the row whose items are open under it, each -1 for none, and its serial,
// a number it takes as it opens, a new one each time and the same while it stands open
typedef struct
{
    CliMenuRow *row;
    unsigned int rows;
    int pointed;
    int opened;
    unsigned int serial;
} CliMenuLevel;

// where a menu asked for and not opened under a title stands: at its point, or at the terminal's key field, the panel's
// foot at its left, as the page's stands at the box of what has the keys
#define CLI_MENU_PLACE_POINT 0u
#define CLI_MENU_PLACE_TERMINAL_KEYS 1u

// The menu bar: every menu's title with where it stands among the menus, the title whose menu is open, -1 for none, the
// menus open, one under another, and where the first of them stands where it was asked for and not under a title: at
// the place `place` names, its top left at x, y in the frame's pixels where that is its point
typedef struct
{
    const char **title;
    unsigned int titles;
    int open;
    CliMenuLevel *level;
    unsigned int levels;
    unsigned int place;
    int x;
    int y;
} CliMenus;

// One row of the explorer: the path from the tree's top, the name, how deep it stands, whether it is a folder, whether
// git ignores it, and whether it is open
typedef struct
{
    const char *path;
    const char *name;
    unsigned int depth;
    int dir;
    int ignored;
    int open;
} CliExplorerRow;

// The explorer's rows, those whose folders above are all open, and the row of the file shown, -1 for none
typedef struct
{
    unsigned int rows;
    int displayed;
    CliExplorerRow *row;
} CliExplorerRows;

// One row of the problems pane: a file's, with its path, name and folder, its diagnostics' count and whether they are
// closed; or a diagnostic's, with its severity, message and place
typedef struct
{
    int is_file;
    const char *path;
    const char *name;
    const char *folder;
    unsigned int count;
    int closed;
    unsigned int severity;
    const char *message;
    unsigned long long line;
    unsigned long long col;
} CliProblemRow;

// One span the file shown is to have underlined: its severity, and its first place and the place after it
typedef struct
{
    unsigned int severity;
    unsigned long long from_line;
    unsigned long long from_col;
    unsigned long long to_line;
    unsigned long long to_col;
} CliProblemSpan;

// The problems: how many files, diagnostics and errors; the check's files done and total, the total 0 where none runs;
// the rows; and the spans of the file shown
typedef struct
{
    unsigned int files;
    unsigned int items;
    unsigned int errors;
    unsigned long long done;
    unsigned long long total;
    unsigned int rows;
    CliProblemRow *row;
    unsigned int spans;
    CliProblemSpan *span;
} CliProblems;

// The hover: the Markdown of its parts, `length` bytes, and the word's line and its first unit and the unit after it
typedef struct
{
    const char *text;
    unsigned long long length;
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
} CliHoverPanel;

// the rows of the completion's list shown at once
#define CLI_COMPLETION_ROWS 10u

// One row of the completion: its label and how long, its kind, its detail and how long, and how many bytes of the label
// the prefix marks
typedef struct
{
    const char *label;
    unsigned long long label_length;
    const char *kind;
    const char *detail;
    unsigned long long detail_length;
    unsigned int marked;
} CliCompletionRow;

// The completion: the line and the first unit the prefix stands at; the prefix; how many items answer it; where the
// first row shown stands among them; the rows shown in rank order, the row chosen among them always; and where the row
// chosen stands among the items. A press on a row names its place among the items
typedef struct
{
    unsigned long long line;
    unsigned long long from;
    const char *prefix;
    unsigned int count;
    unsigned int first;
    unsigned int rows;
    unsigned int chosen;
    CliCompletionRow *row;
} CliCompletionPanel;

// One tab of the terminal: its label, its number and the last folder of the place its shell reported, `shell` where it
// reported none, and whether its shell ended
typedef struct
{
    const char *label;
    int exited;
} CliTerminalTab;

// One pane the terminal's panel shows, a view of its tab's text as the editor's are: the view's frame, NULL for none,
// the rows of the text from its top with the selection, the text's line the xterm's rows start on, and the xterm's rows
// drawn over the text's from that line: their columns and count, their cells row by row, the cursor's column and row
// and whether it is shown, and whether its shell is the one the keys go to while the panel takes them
typedef struct
{
    const CliViewFrame *frame;
    unsigned long long xterm_line;
    unsigned int columns;
    unsigned int rows;
    const CliTerminalCell *cell;
    unsigned int cursor_x;
    unsigned int cursor_y;
    int cursor_shown;
    int focused;
} CliTerminalView;

// The terminal: whether its panel is open and whether it takes the keys; its height in pixels at a scale of one; its
// tabs, the tab shown and the one beside it, -1 for none; and the panes shown, the tab shown's then the one beside's, a
// pane not yet sized having no rows
typedef struct
{
    int open;
    int keys;
    unsigned int height;
    unsigned int tabs;
    CliTerminalTab *tab;
    int shown;
    int beside;
    unsigned int views;
    CliTerminalView view[2];
} CliTerminalPanel;

#endif
