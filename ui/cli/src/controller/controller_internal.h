// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_internal.h: what the controller's thread and its model share: the model itself, one record the
// controller's thread alone reads and writes, each part it publishes marked dirty until no input waits (controller.c,
// controller_*.c)
#ifndef CONTROLLER_INTERNAL_H
#define CONTROLLER_INTERNAL_H
#include "../command_line/command_line.h"
#include "../editor/document.h"
#include "../editor/watch.h"
#include "../servers/servers.h"
#include "controller.h"

typedef struct CliModelCtx CliModelCtx;

// a part marked to be published again
#define CLI_MODEL_DIRTY(part_) (1u << (part_))

// One file open as a tab, its path and the key its text is stored under after it in the tab's own cell: whether a view
// stands on it and whether its language's server took it
typedef struct
{
    char *path;
    char *key;
    int viewed;
    int served;
} CliModelTab;

// A menu open: the catalog's list its rows come from, its rows in a cell, how many, the row marked and the row whose
// items are open under it, each -1 for none
typedef struct
{
    struct CliItemRows *rows;
    int pointed;
    int opened;
} CliModelMenu;

// The model. The tree's root and name, each in a cell of its own, NULL with no tree; the tabs, the one shown and the
// one shown before; the editor's rows and columns as the window last said; whether the editor takes the keys, and the
// strip's tool shown; the lines the map stands for and whether the window has said; the place wanted for the next frame
// of the tab shown; what orior's folder has been asked for the session; the asks under way and whether the tree's check
// runs; the parts to publish; and when the next timer is due
typedef struct
{
    char *root;
    char *title;
    CliModelTab **tab;
    unsigned int tabs;
    unsigned int tabs_capacity;
    int active;
    int last_active;
    unsigned int rows;
    unsigned int columns;
    int editing;
    int tool;
    unsigned long long map_first;
    unsigned long long map_last;
    int map_said;
    CliPosition position_wanted;
    int position_pending;
    int session_known;
    int restored;
    int closing;
    int preferences_displayed;
    int problems_marked;
    unsigned int busy;
    int checking;
    unsigned int wordmark;
    int tree_read;
    unsigned int zoom_percent;
    unsigned int bar_percent;
    unsigned int editor_percent;
    int motion_saved;
    unsigned long long memory_bytes;
    unsigned long long memory_due_ms;
    unsigned long long shown;
    unsigned int dirty;
} CliModel;

extern CliModel g_cli_model;

// from the controller's thread: a snapshot of `part` published to the view, the view's thread woken; where the part's
// ring is full it waits as the part's newest, in place of any that waited, until the view moves on. From any thread:
// the controller's thread woken. And a snapshot's cells released, by its part (controller_model.c)
void cli_controller_publish(unsigned int part, void *snapshot);
void cli_controller_wake(void);
void cli_controller_snapshot_release(unsigned int part, void *snapshot);

// the tree the window is open on, a cell that stands until the window ends, NULL with none; and what the command line
// left the window to run (controller_tree.c)
const char *cli_controller_root(void);
const CliOutcome *cli_controller_launch(void);

// the model, on the controller's thread: begun, one input taken with its bytes, one step of what it has to do with no
// input waiting, 1 where it did something; the milliseconds until its next timer, 0 for none; and ended
void cli_controller_model_begin(void);
void cli_controller_model_input(const CliInput *input, const char *bytes);
int cli_controller_model_step(void);
unsigned long long cli_controller_model_due_ms(void);
void cli_controller_model_end(void);

// the tabs (controller_tabs.c): the file at `path`, a path from the tree's top or a whole one, opened as a tab shown,
// or its tab shown where it is open already, its cursor put at `line`, `col` where `placed` is 1; the tab `at` shown;
// the tab `at` closed, its view and its text let go and the tab beside it shown; every tab closed; the tab shown before
// shown again; the key of the tab shown, NULL for none; and the parts that follow the tab shown marked dirty
void cli_model_file_open(const CliModelCtx *args);
void cli_model_tab_show(const CliModelCtx *args);
void cli_model_tab_close(const CliModelCtx *args);
void cli_model_tabs_close(const CliModelCtx *args);
void cli_model_tab_last(const CliModelCtx *args);
const char *cli_model_key(const CliModelCtx *args);
void cli_model_shown_dirty(const CliModelCtx *args);
// orior's folder (controller_tabs.c): `value` saved under the key `path` on the files' lane, here where `on` is 1, the
// window ending; the session as it stands saved the same way; and what `input`, a CliStateEntries, saves of the tree's
// tabs and the strip's group taken as the window begins
void cli_model_value_save(const CliModelCtx *args);
void cli_model_session_save(const CliModelCtx *args);
void cli_model_session_restore(const CliModelCtx *args);
// the commands (controller_commands.c): the command `path` run, its keys `value`, whoever pressed what; the key `path`
// taken: the menus', and where `on` is 0 a command's or a tool's, else the view's; the catalog read; a press and a move
// on the menu bar or its menus; a menu's rest; and the snapshot of the menus
int cli_model_catalog_read(const CliModelCtx *args);
void cli_model_command_run(const CliModelCtx *args);
// the command `path` run as the menus run it with its `at` `words`, whoever named it: `file` opening its first word, a
// path with a line and a column after it each a colon and digits: 1, or 0 where it is none of the window's
int cli_model_command_named(const CliModelCtx *args);
void cli_model_key_take(const CliModelCtx *args);
void cli_model_preferences_key(const CliModelCtx *args);
void cli_model_menu_input(const CliModelCtx *args);
void cli_model_menu_bar_size(const CliModelCtx *args);
unsigned long long cli_model_menu_due_ms(const CliModelCtx *args);
int cli_model_menu_step(const CliModelCtx *args);
void *cli_model_menus_alloc(const CliModelCtx *args);
// the editor taking the keys or letting them go, `on` 1 or 0, which the window's claim follows; one CliViewInput,
// `input`, handed to the view of the tab shown, and the parts it moves marked dirty; the strip's tool `at` shown or
// closed, and the side told (controller_model.c)
void cli_model_editing(const CliModelCtx *args);
void cli_model_view_input(const CliModelCtx *args);
void cli_model_tool_toggle(const CliModelCtx *args);
// the explorer (controller_explorer.c): the top folder's listing asked for where the side first shows; a press on the
// row `at`; and the snapshot of the rows
void cli_model_explorer_show(const CliModelCtx *args);
void cli_model_explorer_press(const CliModelCtx *args);
void *cli_model_explorer_alloc(const CliModelCtx *args);
// the panels (controller_panels.c): every panel hidden and none wanted; the pointer moved, `input` a CliInput; text
// typed `path`, `length` bytes; a key's effect on the panels, 1 where the key was theirs; the completion's row `at`
// accepted; their timers; and each panel's snapshot
void cli_model_panels_hide(const CliModelCtx *args);
void cli_model_panels_pointer(const CliModelCtx *args);
void cli_model_panels_text(const CliModelCtx *args);
int cli_model_panels_key(const CliModelCtx *args);
void cli_model_panels_frame(const CliModelCtx *args);
void cli_model_completion_accept(const CliModelCtx *args);
void cli_model_completion_wheel(const CliModelCtx *args);
unsigned long long cli_model_panels_due_ms(const CliModelCtx *args);
int cli_model_panels_step(const CliModelCtx *args);
void *cli_model_hover_alloc(const CliModelCtx *args);
void *cli_model_completions_alloc(const CliModelCtx *args);
void *cli_model_signature_alloc(const CliModelCtx *args);
// the language server (controller_panels.c): the tab `at`'s view handed to its file's server; the tab `at`'s view's
// server let go; the server told the text changed; and the server told where the view rests
void cli_model_server_serve(const CliModelCtx *args);
void cli_model_server_close(const CliModelCtx *args);
void cli_model_server_changed(const CliModelCtx *args);
// the problems (controller_problems.c): where the servers report for the tree `path`; begun; a press on the row `at` or
// the check's mark; the reports taken; and the snapshot of the rows, the spans the tab shown's
CliServersSink cli_model_problems_sink(const CliModelCtx *args);
void cli_model_problems_begin(const CliModelCtx *args);
void cli_model_problems_end(const CliModelCtx *args);
void cli_model_problems_press(const CliModelCtx *args);
void cli_model_problems_check(const CliModelCtx *args);
int cli_model_problems_step(const CliModelCtx *args);
void *cli_model_problems_alloc(const CliModelCtx *args);
// the door (controller_door.c): opened, the part that takes the command line's commands made and the window stored for
// the tree `path`, none where it is NULL: 1, or 0 with why given; the commands waiting run, 1 where any were; its key's
// cell released; and what the command line left the window to run, run
int cli_model_door_open(const CliModelCtx *args);
int cli_model_door_step(const CliModelCtx *args);
void cli_model_door_end(const CliModelCtx *args);
void cli_model_launch_run(const CliModelCtx *args);
// the watch (controller_watch.c): a batch taken on the watch's thread; the batches that came handed on, 1 where any
// did; and the watch stopped
void cli_controller_watch_taken(const CliWatchBatchCfg *args);
int cli_model_watch_step(const CliModelCtx *args);
void cli_model_watch_end(const CliModelCtx *args);
// the terminal (controller_terminal.c): the terminal's command `path` run, `value` the words it is handed or NULL: 1,
// or 0 where it is none of the terminal's; the key `path` taken where the panel takes the keys: 1, or 0 where it goes
// on to the rest of the window; `length` bytes of text `path` typed where the panel takes the keys: 1, or 0 where it
// does not; a press on the terminal, `input` a CliInput, or elsewhere, which takes the keys from it; the pointer moved
// or let go after a press on a pane, handed to the pane's view: 1, or 0 where no press on a pane came before it; the
// wheel over a pane, its view scrolled; the view `input` names sized to its rows and columns, or the panel's height
// where `input` is a CLI_INPUT_TERMINAL_HEIGHT; every shell drained and the place stored once its time comes, 1 where
// an xterm changed; the clock's reading of its next timer, 0 for none; its snapshot, and a snapshot's frames and cell
// released; the place read back from the values `input` holds, a CliStateEntries or NULL for none, as the window
// starts; the place stored, every shell hung up on and its tabs let go; and whether the terminal's state `path` is
// on, terminal-split while a shell stands beside the one shown: 1, or 0 where it is off or none of the terminal's
int cli_model_terminal_command(const CliModelCtx *args);
int cli_model_terminal_key(const CliModelCtx *args);
int cli_model_terminal_text(const CliModelCtx *args);
void cli_model_terminal_press(const CliModelCtx *args);
int cli_model_terminal_pointer(const CliModelCtx *args);
void cli_model_terminal_wheel(const CliModelCtx *args);
void cli_model_terminal_size(const CliModelCtx *args);
int cli_model_terminal_step(const CliModelCtx *args);
unsigned long long cli_model_terminal_next_ms(const CliModelCtx *args);
void *cli_model_terminal_alloc(const CliModelCtx *args);
void cli_model_terminal_release(CliTerminalPanel *panel);
void cli_model_terminal_restore(const CliModelCtx *args);
void cli_model_terminal_end(const CliModelCtx *args);
int cli_model_terminal_checked(const CliModelCtx *args);

struct CliModelCtx
{
    const char *path;
    const char *value;
    unsigned long long length;
    int placed;
    unsigned long long line;
    unsigned long long col;
    unsigned int at;
    int on;
    const void *input;
    const char *const *words;
    VerbaLinea *err_msg;
};

typedef struct
{
    void (*file_open)(const CliModelCtx *args);
    void (*tab_show)(const CliModelCtx *args);
    void (*tab_close)(const CliModelCtx *args);
    void (*tabs_close)(const CliModelCtx *args);
    void (*tab_last)(const CliModelCtx *args);
    const char *(*key)(const CliModelCtx *args);
    void (*shown_dirty)(const CliModelCtx *args);
    void (*value_save)(const CliModelCtx *args);
    void (*session_save)(const CliModelCtx *args);
    void (*session_restore)(const CliModelCtx *args);
    void (*editing)(const CliModelCtx *args);
    void (*view_input)(const CliModelCtx *args);
    void (*tool_toggle)(const CliModelCtx *args);
    void (*explorer_show)(const CliModelCtx *args);
    void (*panels_hide)(const CliModelCtx *args);
    void (*server_serve)(const CliModelCtx *args);
    void (*server_close)(const CliModelCtx *args);
    void (*server_changed)(const CliModelCtx *args);
} CliModelNs;
EMBED_TABLE_LAYOUT(CliModelNs, file_open, tab_show, tab_close, tabs_close, tab_last, key, shown_dirty, value_save,
                   session_save, session_restore, editing, view_input, tool_toggle, explorer_show, panels_hide,
                   server_serve, server_close, server_changed);
EMBED_TABLE_STORAGE CliModelNs cli_model EMBED_UNUSED = {
    .file_open = cli_model_file_open,
    .tab_show = cli_model_tab_show,
    .tab_close = cli_model_tab_close,
    .tabs_close = cli_model_tabs_close,
    .tab_last = cli_model_tab_last,
    .key = cli_model_key,
    .shown_dirty = cli_model_shown_dirty,
    .value_save = cli_model_value_save,
    .session_save = cli_model_session_save,
    .session_restore = cli_model_session_restore,
    .editing = cli_model_editing,
    .view_input = cli_model_view_input,
    .tool_toggle = cli_model_tool_toggle,
    .explorer_show = cli_model_explorer_show,
    .panels_hide = cli_model_panels_hide,
    .server_serve = cli_model_server_serve,
    .server_close = cli_model_server_close,
    .server_changed = cli_model_server_changed,
};

typedef struct
{
    int (*catalog_read)(const CliModelCtx *args);
    void (*command_run)(const CliModelCtx *args);
    int (*command_named)(const CliModelCtx *args);
    void (*key_take)(const CliModelCtx *args);
    void (*preferences_key)(const CliModelCtx *args);
    void (*menu_input)(const CliModelCtx *args);
    unsigned long long (*menu_due_ms)(const CliModelCtx *args);
    int (*menu_step)(const CliModelCtx *args);
    void *(*menus_alloc)(const CliModelCtx *args);
} CliModelCommandsNs;
EMBED_TABLE_LAYOUT(CliModelCommandsNs, catalog_read, command_run, command_named, key_take, preferences_key, menu_input,
                   menu_due_ms, menu_step, menus_alloc);
EMBED_TABLE_STORAGE CliModelCommandsNs cli_model_commands EMBED_UNUSED = {
    .catalog_read = cli_model_catalog_read,
    .command_run = cli_model_command_run,
    .command_named = cli_model_command_named,
    .key_take = cli_model_key_take,
    .preferences_key = cli_model_preferences_key,
    .menu_input = cli_model_menu_input,
    .menu_due_ms = cli_model_menu_due_ms,
    .menu_step = cli_model_menu_step,
    .menus_alloc = cli_model_menus_alloc,
};

typedef struct
{
    void (*press)(const CliModelCtx *args);
    void *(*rows_alloc)(const CliModelCtx *args);
} CliModelExplorerNs;
EMBED_TABLE_LAYOUT(CliModelExplorerNs, press, rows_alloc);
EMBED_TABLE_STORAGE CliModelExplorerNs cli_model_explorer EMBED_UNUSED = {
    .press = cli_model_explorer_press,
    .rows_alloc = cli_model_explorer_alloc,
};

typedef struct
{
    void (*pointer)(const CliModelCtx *args);
    void (*text)(const CliModelCtx *args);
    int (*key)(const CliModelCtx *args);
    void (*frame)(const CliModelCtx *args);
    void (*completion_accept)(const CliModelCtx *args);
    void (*completion_wheel)(const CliModelCtx *args);
    unsigned long long (*due_ms)(const CliModelCtx *args);
    int (*step)(const CliModelCtx *args);
    void *(*hover_alloc)(const CliModelCtx *args);
    void *(*completions_alloc)(const CliModelCtx *args);
    void *(*signature_alloc)(const CliModelCtx *args);
} CliModelPanelsNs;
EMBED_TABLE_LAYOUT(CliModelPanelsNs, pointer, text, key, frame, completion_accept, completion_wheel, due_ms, step,
                   hover_alloc, completions_alloc, signature_alloc);
EMBED_TABLE_STORAGE CliModelPanelsNs cli_model_panels EMBED_UNUSED = {
    .pointer = cli_model_panels_pointer,
    .text = cli_model_panels_text,
    .key = cli_model_panels_key,
    .frame = cli_model_panels_frame,
    .completion_accept = cli_model_completion_accept,
    .completion_wheel = cli_model_completion_wheel,
    .due_ms = cli_model_panels_due_ms,
    .step = cli_model_panels_step,
    .hover_alloc = cli_model_hover_alloc,
    .completions_alloc = cli_model_completions_alloc,
    .signature_alloc = cli_model_signature_alloc,
};

typedef struct
{
    CliServersSink (*sink)(const CliModelCtx *args);
    void (*begin)(const CliModelCtx *args);
    void (*end)(const CliModelCtx *args);
    void (*press)(const CliModelCtx *args);
    void (*check)(const CliModelCtx *args);
    int (*step)(const CliModelCtx *args);
    void *(*rows_alloc)(const CliModelCtx *args);
} CliModelProblemsNs;
EMBED_TABLE_LAYOUT(CliModelProblemsNs, sink, begin, end, press, check, step, rows_alloc);
EMBED_TABLE_STORAGE CliModelProblemsNs cli_model_problems EMBED_UNUSED = {
    .sink = cli_model_problems_sink,
    .begin = cli_model_problems_begin,
    .end = cli_model_problems_end,
    .press = cli_model_problems_press,
    .check = cli_model_problems_check,
    .step = cli_model_problems_step,
    .rows_alloc = cli_model_problems_alloc,
};

typedef struct
{
    int (*open)(const CliModelCtx *args);
    int (*step)(const CliModelCtx *args);
    void (*end)(const CliModelCtx *args);
    void (*launch_run)(const CliModelCtx *args);
} CliModelDoorNs;
EMBED_TABLE_LAYOUT(CliModelDoorNs, open, step, end, launch_run);
EMBED_TABLE_STORAGE CliModelDoorNs cli_model_door EMBED_UNUSED = {
    .open = cli_model_door_open,
    .step = cli_model_door_step,
    .end = cli_model_door_end,
    .launch_run = cli_model_launch_run,
};

typedef struct
{
    int (*step)(const CliModelCtx *args);
    void (*end)(const CliModelCtx *args);
} CliModelWatchNs;
EMBED_TABLE_LAYOUT(CliModelWatchNs, step, end);
EMBED_TABLE_STORAGE CliModelWatchNs cli_model_watch EMBED_UNUSED = {
    .step = cli_model_watch_step,
    .end = cli_model_watch_end,
};

typedef struct
{
    int (*command)(const CliModelCtx *args);
    int (*key)(const CliModelCtx *args);
    int (*text)(const CliModelCtx *args);
    void (*press)(const CliModelCtx *args);
    int (*pointer)(const CliModelCtx *args);
    void (*wheel)(const CliModelCtx *args);
    void (*size)(const CliModelCtx *args);
    int (*step)(const CliModelCtx *args);
    unsigned long long (*next_ms)(const CliModelCtx *args);
    void *(*alloc)(const CliModelCtx *args);
    void (*restore)(const CliModelCtx *args);
    void (*end)(const CliModelCtx *args);
    int (*checked)(const CliModelCtx *args);
} CliModelTerminalNs;
EMBED_TABLE_LAYOUT(CliModelTerminalNs, command, key, text, press, pointer, wheel, size, step, next_ms, alloc, restore,
                   end, checked);
EMBED_TABLE_STORAGE CliModelTerminalNs cli_model_terminal EMBED_UNUSED = {
    .command = cli_model_terminal_command,
    .key = cli_model_terminal_key,
    .text = cli_model_terminal_text,
    .press = cli_model_terminal_press,
    .pointer = cli_model_terminal_pointer,
    .wheel = cli_model_terminal_wheel,
    .size = cli_model_terminal_size,
    .step = cli_model_terminal_step,
    .next_ms = cli_model_terminal_next_ms,
    .alloc = cli_model_terminal_alloc,
    .restore = cli_model_terminal_restore,
    .end = cli_model_terminal_end,
    .checked = cli_model_terminal_checked,
};

#endif
