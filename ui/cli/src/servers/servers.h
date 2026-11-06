// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers.h: the language servers the editor talks to, one for each toolchain whose `server` in toolchains.json names a
// program, such as clangd for C, C++ and CUDA. A server starts with the first file of its languages the editor opens,
// at the tree's top folder, and stops when another tree is opened or orior_cli ends. One that cannot be found or does
// not start is not tried again until the tree changes, and the file opens without it.
//
// What the servers return comes back as orior's own shapes: a hover as Markdown, a definition as a path and a place, a
// usage as a span with its line's text, a completion as the editor's items, a file's diagnostics as spans with a
// severity, 1 an error, 2 a warning, 3 a note and 4 a hint, and an edit, from a rename, a quick fix or the server's own
// requesting, as each file's spans and their new text. A quick fix goes to the editor whole, as the server gave it, and
// comes back to be carried out.
//
// The tree's check hands every file of the tree the editor does not have open to its language's server, six at a
// time, as the editor would open it, and lets it go once the server has reported what is wrong in it, the server's
// clearing of a file it lets go not passed on. A server that checks its projects as a whole, as rust-analyzer does with
// cargo check, is instructed to check them all in place of that. A file is checked again when it changes on the disk,
// with the files whose includes or imports name it, and when the editor closes it, as the disk stores it.
//
// The checkers the reader names check a file as it opens; one that reads a file's text on its input checks it again as
// the editor changes it, and every one as it changes on the disk; and, with the tree's check, each that reads a tree
// whole checks the tree. A file's diagnostics contain each checker's findings, a checker's fixes offered as quick
// fixes.
//
// The inspections read every Python and JavaScript file of the tree once a file of the tree is handed over, on a thread
// of their own, and each file again as the editor changes it, as it changes on the disk, and as the editor closes it. A
// file's diagnostics go to the editor whole: its server's, then the inspections'. A finding's fix is offered with the
// server's quick fixes.
//
// Every answer is written as JSON into the line handed for it; a path in it is written as it stands in the tree where a
// tree is given, and whole otherwise (servers_*.c)
#ifndef SERVERS_H
#define SERVERS_H
#include "../command_line/callback_cfg.h"
#include "../text/edits.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliServersRootCfg CliServersRootCfg;
typedef struct CliServersRootSinkCfg CliServersRootSinkCfg;
typedef struct CliServersPathCfg CliServersPathCfg;
typedef struct CliServersErrMsgCfg CliServersErrMsgCfg;
typedef struct CliServersPathOutCfg CliServersPathOutCfg;
typedef struct CliServersPathOutErrMsgCfg CliServersPathOutErrMsgCfg;
typedef struct CliServersPathSizeCfg CliServersPathSizeCfg;
typedef struct CliServersRootSinkOutCfg CliServersRootSinkOutCfg;
typedef struct CliServersPathOutErrMsgFromCfg CliServersPathOutErrMsgFromCfg;
typedef struct CliServersPathOutErrMsgFromToCfg CliServersPathOutErrMsgFromToCfg;
typedef struct CliServersErrMsgIdCfg CliServersErrMsgIdCfg;
typedef struct CliServersRootSinkCountCfg CliServersRootSinkCountCfg;
typedef struct CliServersCapacityCfg CliServersCapacityCfg;
typedef struct CliServersOutCfg CliServersOutCfg;
typedef struct CliServersCfg CliServersCfg;
typedef struct CliServersPathSizeKindCfg CliServersPathSizeKindCfg;

// One diagnostic of a file: its span's first place and the place after it, each a line and a column in units, its
// severity, 1 an error, 2 a warning, 3 and 4 notes, and its message, `message_length` bytes in the texts of the report
// it stands in
typedef struct
{
    unsigned long long from_line;
    unsigned long long from_col;
    unsigned long long to_line;
    unsigned long long to_col;
    unsigned int severity;
    const char *message;
    unsigned long long message_length;
} CliServerDiagnostic;

// A file's report decoded once where the servers store it: the tree its stored texts are read into, the server's, the
// inspections' and the checkers' in that order, its path as the sink's tree writes it, and its diagnostics, whose
// messages lie in the tree's texts. The head, the diagnostics and the path are one cell, and the tree is a cell of its
// own; cli_servers_report_release releases both
typedef struct
{
    CliJson json;
    const char *path;
    unsigned int count;
    CliServerDiagnostic item[];
} CliServerReport;

// A file's diagnostics handed to the sink's function, in place of those it stood with, on the thread that found
// them: its path, its diagnostics and the report they stand in, NULL with none. A sink that reads the report after the
// call returns sets `*taken` to 1 and releases the report itself; the report of any other sink is released as the call
// returns
typedef struct
{
    void *context;
    const char *path;
    const CliServerDiagnostic *item;
    unsigned int count;
    CliServerReport *report;
    int *taken;
} CliServerDiagnosticsCfg;

// a report's cell and its tree's cell released (servers_emit.c)
void cli_servers_report_release(CliServerReport *report);

// How far the tree's check has gone, the total 0 where it is over
typedef struct
{
    void *context;
    unsigned long long done;
    unsigned long long total;
} CliServerCheckCfg;

// Where what the servers send unprompted goes: a function that receives each event, `lsp-diagnostics`, `lsp-edits`,
// `lsp-hints`, `lsp-parse` or `tree-check`, with its JSON, its context, and the tree its paths are written in
typedef struct
{
    void (*emit)(const CliEachEventJsonSizeCfg *args);
    void (*diagnostics)(const CliServerDiagnosticsCfg *args);
    void (*check_moved)(const CliServerCheckCfg *args);
    void *context;
    char tree[CLI_PATH_MAX_LEN];
} CliServersSink;

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
// first row shown stands among them; the rows shown; and where the row chosen stands among the items
typedef struct
{
    unsigned long long line;
    unsigned long long from;
    const char *prefix;
    unsigned int count;
    unsigned int first;
    unsigned int rows;
    unsigned int chosen;
} CliCompletionPanel;

// The pointer moved: whether it stands on the editor's text, at the row `row` from the view's top and the cell
// `column`, and whether it stands on the hover's box
typedef struct
{
    int in_text;
    unsigned long long row;
    unsigned long long column;
    int on_hover;
} CliServersPanelsPointerCfg;

// the panels over the editor of the tree `root`, on the window's thread (servers_panels.c): begun, watching the views,
// each view opened on a file handed to its language's server, each about to close let go of it, and each frame of the
// view shown followed, and ended; every panel hidden; the pointer moved; text typed, `length` bytes; a key's effect on
// the panels, 1 where the key was theirs; the completion's row `at` accepted; the completion's rows turned by the
// wheel; their timers taken, 1 where any came, and the clock's reading of the next, 0 for none; and what they show: the
// hover, the completion and its row `row` shown, each 1 where it stands, else 0, and the signature, NULL for none
void cli_servers_panels_begin(const char *root);
void cli_servers_panels_end(void);
void cli_servers_panels_hide(void);
void cli_servers_panels_pointer(const CliServersPanelsPointerCfg *args);
void cli_servers_panels_text(const char *text, unsigned long long length);
int cli_servers_panels_key(const char *name);
void cli_servers_panels_accept(unsigned int at);
void cli_servers_panels_wheel(int rows);
int cli_servers_panels_step(void);
unsigned long long cli_servers_panels_due_ms(void);
int cli_servers_panels_hover(CliHoverPanel *panel);
int cli_servers_panels_completion(CliCompletionPanel *panel);
int cli_servers_panels_completion_row(unsigned int row, CliCompletionRow *out);
const CliSignaturePanel *cli_servers_panels_signature(void);

// One row of the problems: a file's, with its path, its name and how many bytes of its path its folder takes, its
// diagnostics' count and whether they are closed; or a diagnostic's, with its severity, its message and the bytes of
// its first line, and its place. Each text lies in the file's report and stands until the reading thread's next step
typedef struct
{
    int is_file;
    const char *path;
    const char *name;
    unsigned long long folder_length;
    unsigned int count;
    int closed;
    unsigned int severity;
    const char *message;
    unsigned long long message_length;
    unsigned long long line;
    unsigned long long col;
} CliProblemRow;

// One span a file's diagnostic underlines: its severity, and its first place and the place after it
typedef struct
{
    unsigned int severity;
    unsigned long long from_line;
    unsigned long long from_col;
    unsigned long long to_line;
    unsigned long long to_col;
} CliProblemSpan;

// The problems counted: the files, the diagnostics and the errors; the check's files done and total, the total 0 where
// none runs; and the rows shown
typedef struct
{
    unsigned int files;
    unsigned int items;
    unsigned int errors;
    unsigned long long done;
    unsigned long long total;
    unsigned int rows;
} CliProblemsCounts;

// The problems (servers_problems.c): begun, `woken` called from the thread a report comes on; where the servers report
// for the tree `root`; and every report released as they end, the check stopped. A report comes in on the thread that
// read it and is published whole with an atomic swap, and what it replaced is released once no reader reads it. A step
// ends the calling thread's pass through the problems and begins its next, reading what was published last: 1 where it
// differs from what its last step read or the check moved. From one step to the next, on that thread: the problems
// counted; whether the tree's check runs; the row `at` into `row`: 1, or 0 past them; the span `at` of the file
// `path`'s diagnostics into `span`: 1, or 0 past them; a press on the row `at`, a file's rows closed or opened, or a
// diagnostic's row into `row`: 1 where it was a diagnostic's, its file to be opened at its place; and the tree's check
// of `root` started, or stopped where it runs
void cli_servers_problems_begin(void (*woken)(void));
CliServersSink cli_servers_problems_sink(const char *root);
int cli_servers_problems_step(void);
void cli_servers_problems_end(void);
CliProblemsCounts cli_servers_problems_counts(void);
int cli_servers_problems_checking(void);
int cli_servers_problems_row(unsigned int at, CliProblemRow *row);
int cli_servers_problems_span(const char *path, unsigned int at, CliProblemSpan *span);
int cli_servers_problems_press(unsigned int at, CliProblemRow *row);
void cli_servers_problems_check(const char *root);

// the file at `path` handed to its language's server, started at `root` where it is not running, and its text to the
// inspections and the checkers: `true` or `false` written as a server took it; its new text given; and the file let go
// as the editor closes it, read again as the disk stores it. Each 1, or 0 with why given (servers_files.c)
int cli_servers_open(const CliServersRootCfg *args);
int cli_servers_change(const CliServersRootCfg *args);
int cli_servers_close(const CliServersPathOutErrMsgFromToCfg *args);
// the view of `path` the page rests on, its lines `first` to `last` and the lines `opener`, `openers` of them, opening
// each region its first line stands inside, outermost first: where its server is given the tree's slice in place of
// the file and was given another view's, the slice cut for this view and given as the file's next version, its parse
// forgotten, `true` written, and `false` where nothing changed. 1, or 0 with why given (servers_slice.c)
int cli_servers_view(const CliServersPathOutErrMsgCfg *args);

// the language server the script plugin `id` brings, as its plugin.json's `server` names it, started at `root` with
// `length` bytes of JSON `options` as the options it starts with and the `firsts` arguments `first` before those its
// plugin.json names, what it sends going where `emit` sends it, and handed every file of its languages the editor has
// open: 1, or 0 with why given; and the plugin's server stopped, where it runs. A request sent to it, its answer waited
// for up to `wait` milliseconds into `result`; a notification sent to it; and a request it sent, of the id `request`
// the window was given, answered with `length` bytes of JSON `result`. Each 1, or 0 with why given (servers_plugins.c)
void cli_servers_plugin_stop(const CliServersRootSinkCfg *args);
int cli_servers_plugin_request(const CliServersErrMsgCfg *args);
int cli_servers_plugin_notify(const CliServersErrMsgCfg *args);
// the text of a file as the editor has it, where the editor has it open, into `out`: 1, or 0 where it has none; and
// the same copied into a cell of its size with a 0 after, its size into `size`: the cell, or NULL where the editor
// has none or the cell could not be taken
int cli_servers_text_get(const CliServersPathOutErrMsgFromToCfg *args);
char *cli_servers_text_alloc(const CliServersPathSizeKindCfg *args);

// what the server of `path` answers at `line`, `col`: a hover as Markdown, `null` for none; the definitions, each
// `{"path", "line", "col"}`; the completions, orior's own reading's of a build file or JSON first, each an item; every
// usage, each `{"path", "from", "to", "text"}`, the declaration among them; whether the symbol can be renamed,
// `{"from", "to", "name"}` or `null`; the edits that rename it to `name`, each file's `{"path", "edits"}`; and the
// signature of the call the cursor is in, `{"label", "params", "active", "doc", "count"}` or `null`. Each 1, or 0 with
// why given (servers_request.c, servers_shapes.c)
int cli_servers_hover(const CliServersPathOutCfg *args);
// One item a completion answers: what it shows, its kind, what it states of itself and what it inserts, each ended by a
// 0, and whether what it inserts is a snippet
typedef struct
{
    const char *label;
    const char *kind;
    const char *detail;
    const char *insert;
    int snippet;
} CliServerItem;
// The items of one completion in one cell
typedef struct
{
    unsigned int count;
    CliServerItem *item;
} CliServerItems;
// the completion items at `line`, `col` of the file at `path`, those of the tree where the file is a slice's, of a
// build's or a schema's where it is one, and its server's, in a cell of `owner`: the cell, or NULL where none is to be
// asked or the cells cannot meet it, with why given. And the signature at the place, the server's, in a cell of
// `owner`: the cell, or NULL where it gives none
typedef struct
{
    const char *path;
    unsigned int line;
    unsigned int col;
    const CellOwner *owner;
    VerbaLinea *err_msg;
} CliServersAskCfg;
CliServerItems *cli_servers_items_alloc(const CliServersAskCfg *args);
CliSignaturePanel *cli_servers_signature_alloc(const CliServersAskCfg *args);

// the lines `from` to `to` of the parse of `path`, orior's own or its server's semantic tokens, and its folds,
// `{"whole", "from", "lines", "folds"}` or `null`; the spans that contain `from` to `to` and are more than it, the
// least first, the parse's where it reads the file whole, else the tree's read off the branch under `key`, NULL for
// none, or the text the servers were told; the hints its server writes on the lines `from` to `to`; and the parses
// that came from servers forgotten, for each to be requested again. Each 1, or 0 with why given (servers_parse.c)
void cli_servers_forget_tokens(void);

// A server running: its toolchain's id, its program's name, and its process
typedef struct
{
    char tool[CLI_NAME_MAX_LEN];
    char program[CLI_NAME_MAX_LEN];
    unsigned int pid;
} CliServerListing;

// every server running into `servers`, sized for `capacity`: their count; the toolchains whose servers serve any of
// `languages`, `count` of them, into `tools`, sized for `capacity`: their count; the servers of `tools`, `count` of
// them, stopped without waiting, the files they had open written as an array; every server stopped and waited on, as
// the app closes; every server let go as another tree opens; and each server notified its settings changed
// (servers_running.c)
unsigned int cli_servers_list(const CliServersCfg *args);
unsigned int cli_servers_tools_for(const CliServersCapacityCfg *args);
void cli_servers_stop_all(void);
void cli_servers_let_go(void);

// the tree's check started over `files`, `count` of them, each by its path in the tree at `root`: how many files wait;
// the check stopped where it stands, the files waiting let go and the check reported done, on for the files that change
// after; whether it has files waiting or being checked; and a batch of `count` files changed on the disk, each by its
// path in the tree, checked again with the files whose includes or imports name one of them, a file gone losing its
// diagnostics (servers_check.c, servers_changed.c)
unsigned int cli_servers_check_tree(const CliServersRootSinkCountCfg *args);
void cli_servers_stop_check(void);
int cli_servers_checking(void);
void cli_servers_on_change(const CliServersRootSinkCountCfg *args);

struct CliServersRootCfg
{
    const char *root;
    const char *path;
    const char *language;
    const char *text;
    unsigned long long size;
    const CliServersSink *sink;
    int *served;
    VerbaLinea *err_msg;
};

struct CliServersRootSinkCfg
{
    const char *root;
    const CliServersSink *sink;
    VerbaLinea *err_msg;
    const char *id;
    const char *options;
    unsigned long long length;
    const char *const *first_letters;
    unsigned int firsts;
};

struct CliServersPathCfg
{
    const char *path;
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *tree;
    const char *item;
    int incoming;
};

struct CliServersErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *id;
    unsigned long long length;
    const char *method;
    const char *params;
    unsigned long long wait;
    VerbaLinea *result;
};

struct CliServersPathOutCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int line;
    unsigned int col;
    const char *tree;
    const char *name;
};

struct CliServersPathOutErrMsgCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int first;
    unsigned int last;
    const unsigned int *opener;
    unsigned int openers;
};

struct CliServersPathSizeCfg
{
    const char *path;
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *tree;
    const char *raw;
};

struct CliServersRootSinkOutCfg
{
    const char *root;
    const CliServersSink *sink;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int count;
    const char *const *names;
};

struct CliServersPathOutErrMsgFromCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    CliTextPosition from;
    CliTextPosition to;
    const char *key;
};

struct CliServersPathOutErrMsgFromToCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int from_u32;
    unsigned int to_u32;
};

struct CliServersErrMsgIdCfg
{
    VerbaLinea *err_msg;
    const char *id;
    unsigned long long length;
    const char *request;
    const char *result_text;
};

struct CliServersRootSinkCountCfg
{
    const char *root;
    const CliServersSink *sink;
    unsigned int count;
    const char *const *files;
};

struct CliServersCapacityCfg
{
    unsigned int capacity;
    const char *const *languages;
    unsigned int count;
    char (*tools)[CLI_NAME_MAX_LEN];
};

struct CliServersOutCfg
{
    VerbaLinea *out;
    unsigned int count;
    const char *const *tools_letters;
};

struct CliServersCfg
{
    CliServerListing *servers;
    unsigned int capacity;
};

struct CliServersPathSizeKindCfg
{
    const char *path;
    unsigned long long *size_pointer;
};

typedef struct
{
    int (*open)(const CliServersRootCfg *args);
    int (*change)(const CliServersRootCfg *args);
} CliServersRootNs;
EMBED_TABLE_LAYOUT(CliServersRootNs, open, change);
EMBED_TABLE_STORAGE CliServersRootNs cli_servers_root EMBED_UNUSED = {
    .open = cli_servers_open,
    .change = cli_servers_change,
};

typedef struct
{
    int (*close)(const CliServersPathOutErrMsgFromToCfg *args);
    int (*text_get)(const CliServersPathOutErrMsgFromToCfg *args);
} CliServersPathOutErrMsgFromToNs;
EMBED_TABLE_LAYOUT(CliServersPathOutErrMsgFromToNs, close, text_get);
EMBED_TABLE_STORAGE CliServersPathOutErrMsgFromToNs cli_servers_path_out_err_msg_from_to EMBED_UNUSED = {
    .close = cli_servers_close,
    .text_get = cli_servers_text_get,
};

typedef struct
{
    int (*view)(const CliServersPathOutErrMsgCfg *args);
} CliServersPathOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliServersPathOutErrMsgNs, view);
EMBED_TABLE_STORAGE CliServersPathOutErrMsgNs cli_servers_path_out_err_msg EMBED_UNUSED = {
    .view = cli_servers_view,
};

typedef struct
{
    void (*plugin_stop)(const CliServersRootSinkCfg *args);
} CliServersRootSinkNs;
EMBED_TABLE_LAYOUT(CliServersRootSinkNs, plugin_stop);
EMBED_TABLE_STORAGE CliServersRootSinkNs cli_servers_root_sink EMBED_UNUSED = {
    .plugin_stop = cli_servers_plugin_stop,
};

typedef struct
{
    int (*plugin_request)(const CliServersErrMsgCfg *args);
    int (*plugin_notify)(const CliServersErrMsgCfg *args);
} CliServersErrMsgNs;
EMBED_TABLE_LAYOUT(CliServersErrMsgNs, plugin_request, plugin_notify);
EMBED_TABLE_STORAGE CliServersErrMsgNs cli_servers_err_msg EMBED_UNUSED = {
    .plugin_request = cli_servers_plugin_request,
    .plugin_notify = cli_servers_plugin_notify,
};

typedef struct
{
    char *(*text_alloc)(const CliServersPathSizeKindCfg *args);
} CliServersPathSizeKindNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeKindNs, text_alloc);
EMBED_TABLE_STORAGE CliServersPathSizeKindNs cli_servers_path_size_kind EMBED_UNUSED = {
    .text_alloc = cli_servers_text_alloc,
};

typedef struct
{
    int (*hover)(const CliServersPathOutCfg *args);
} CliServersPathOutNs;
EMBED_TABLE_LAYOUT(CliServersPathOutNs, hover);
EMBED_TABLE_STORAGE CliServersPathOutNs cli_servers_path_out EMBED_UNUSED = {
    .hover = cli_servers_hover,
};

typedef struct
{
    unsigned int (*list)(const CliServersCfg *args);
} CliServersNs;
EMBED_TABLE_LAYOUT(CliServersNs, list);
EMBED_TABLE_STORAGE CliServersNs cli_servers_table EMBED_UNUSED = {
    .list = cli_servers_list,
};

typedef struct
{
    unsigned int (*tools_for)(const CliServersCapacityCfg *args);
} CliServersCapacityNs;
EMBED_TABLE_LAYOUT(CliServersCapacityNs, tools_for);
EMBED_TABLE_STORAGE CliServersCapacityNs cli_servers_capacity EMBED_UNUSED = {
    .tools_for = cli_servers_tools_for,
};

typedef struct
{
    unsigned int (*check_tree)(const CliServersRootSinkCountCfg *args);
    void (*change)(const CliServersRootSinkCountCfg *args);
} CliServersRootSinkCountNs;
EMBED_TABLE_LAYOUT(CliServersRootSinkCountNs, check_tree, change);
EMBED_TABLE_STORAGE CliServersRootSinkCountNs cli_servers_root_sink_count EMBED_UNUSED = {
    .check_tree = cli_servers_check_tree,
    .change = cli_servers_on_change,
};

typedef struct
{
    CliServerItems *(*items_alloc)(const CliServersAskCfg *args);
    CliSignaturePanel *(*signature_alloc)(const CliServersAskCfg *args);
} CliServersAskNs;
EMBED_TABLE_LAYOUT(CliServersAskNs, items_alloc, signature_alloc);
EMBED_TABLE_STORAGE CliServersAskNs cli_servers_ask EMBED_UNUSED = {
    .items_alloc = cli_servers_items_alloc,
    .signature_alloc = cli_servers_signature_alloc,
};

#endif
