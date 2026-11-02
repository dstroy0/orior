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
typedef struct CliServersRoomCfg CliServersRoomCfg;
typedef struct CliServersOutCfg CliServersOutCfg;
typedef struct CliServersCfg CliServersCfg;
typedef struct CliServersPathSizeKindCfg CliServersPathSizeKindCfg;

#include "../command_line/callback_cfg.h"
#include "../text/edits.h"
#include "embed_dispatch_layout.h"

// Where what the servers send unprompted goes: a function that receives each event, `lsp-diagnostics`, `lsp-edits`,
// `lsp-hints`, `lsp-parse` or `tree-check`, with its JSON, its context, and the tree its paths are written in
typedef struct
{
    void (*emit)(const CliEachEventJsonSizeCfg *args);
    void *context;
    char tree[CLI_PATH_MAX_LEN];
} CliServersSink;

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
int cli_servers_plugin_start(const CliServersRootSinkCfg *args);
void cli_servers_plugin_stop(const CliServersRootSinkCfg *args);
int cli_servers_plugin_request(const CliServersErrMsgCfg *args);
int cli_servers_plugin_notify(const CliServersErrMsgCfg *args);
int cli_servers_plugin_answer(const CliServersErrMsgIdCfg *args);
// the text of a file as the editor has it, where the editor has it open, into `out`: 1, or 0 where it has none; and
// the same copied into a cell of its size with a 0 after, its size into `size`: the cell, or NULL where the editor
// has none or the cell could not be taken
int cli_servers_text_get(const CliServersPathOutErrMsgFromToCfg *args);
char *cli_servers_text_take(const CliServersPathSizeKindCfg *args);

// what the server of `path` answers at `line`, `col`: a hover as Markdown, `null` for none; the definitions, each
// `{"path", "line", "col"}`; the completions, orior's own reading's of a build file or JSON first, each an item; every
// usage, each `{"path", "from", "to", "text"}`, the declaration among them; whether the symbol can be renamed,
// `{"from", "to", "name"}` or `null`; the edits that rename it to `name`, each file's `{"path", "edits"}`; and the
// signature of the call the cursor is in, `{"label", "params", "active", "doc", "count"}` or `null`. Each 1, or 0 with
// why given (servers_request.c, servers_shapes.c)
int cli_servers_hover(const CliServersPathOutCfg *args);
int cli_servers_definition(const CliServersPathOutCfg *args);
int cli_servers_complete(const CliServersPathOutCfg *args);
int cli_servers_references(const CliServersPathOutCfg *args);
int cli_servers_renamable(const CliServersPathOutCfg *args);
int cli_servers_rename(const CliServersPathOutCfg *args);
int cli_servers_signature(const CliServersPathOutCfg *args);

// the quick fixes and refactorings for the span `from`, `to` of `path`, orior's own and the checkers' fixes of what
// touches it first, then its server's and each plugin server's that has open it, each `{"title", "kind", "preferred",
// "disabled", "raw"}` (servers_actions.c); and an action carried out from its `raw`, `size` bytes of JSON, its edits
// written as each file's `{"path", "edits"}` (servers_act.c). Each 1, or 0 with why given
int cli_servers_actions(const CliServersPathOutErrMsgFromCfg *args);
int cli_servers_act(const CliServersPathSizeCfg *args);

// the lines `from` to `to` of the parse of `path`, orior's own or its server's semantic tokens, and its folds,
// `{"whole", "from", "lines", "folds"}` or `null`; the spans that contain `from` to `to` and are more than it, the
// least first, the parse's where it reads the file whole, else the tree's read off the branch under `key`, NULL for
// none, or the text the servers were told; the hints its server writes on the lines `from` to `to`; and the parses
// that came from servers forgotten, for each to be requested again. Each 1, or 0 with why given (servers_parse.c)
int cli_servers_colors(const CliServersPathOutErrMsgFromToCfg *args);
int cli_servers_spans(const CliServersPathOutErrMsgFromCfg *args);
int cli_servers_hints(const CliServersPathOutErrMsgFromToCfg *args);
void cli_servers_forget_tokens(void);

// the function at `line`, `col` of `path` as the top of its call hierarchy, `null` where there is none; and the
// functions that call the function `item`, `size` bytes of JSON, where `incoming`, or that it calls, each `{"name",
// "detail", "path", "from", "to", "site", "at", "item"}`. Each 1, or 0 with why given (servers_calls.c)
int cli_servers_call_root(const CliServersPathOutCfg *args);
int cli_servers_calls(const CliServersPathCfg *args);

// A server running: its toolchain's id, its program's name, and its process
typedef struct
{
    char tool[CLI_NAME_MAX_LEN];
    char program[CLI_NAME_MAX_LEN];
    unsigned int pid;
} CliServerListed;

// every server running into `servers`, sized for `room`: their count; the toolchains whose servers serve any of
// `languages`, `count` of them, into `tools`, sized for `room`: their count; the servers of `tools`, `count` of them,
// stopped without waiting, the files they had open written as an array; every server stopped and waited on, as the app
// closes; every server let go as another tree opens; and each server notified its settings changed (servers_running.c)
unsigned int cli_servers_list(const CliServersCfg *args);
unsigned int cli_servers_tools_for(const CliServersRoomCfg *args);
void cli_servers_stop(const CliServersOutCfg *args);
void cli_servers_stop_all(void);
void cli_servers_let_go(void);
void cli_servers_settings_change(void);

// the tree's check started over `files`, `count` of them, each by its path in the tree at `root`: how many files wait;
// the check stopped where it stands, the files waiting let go and the check reported done, on for the files that change
// after; whether it has files waiting or being checked; and a batch of `count` files changed on the disk, each by its
// path in the tree, checked again with the files whose includes or imports name one of them, a file gone losing its
// diagnostics (servers_check.c, servers_changed.c)
unsigned int cli_servers_check_tree(const CliServersRootSinkCountCfg *args);
void cli_servers_stop_check(void);
int cli_servers_checking(void);
void cli_servers_on_change(const CliServersRootSinkCountCfg *args);

// the checkers the reader named, `count` names, by name or id, each file the editor has open checked again with them
// and the tree where its check is on; the names no checker answers to written as an array: 1, or 0 with why given where
// no room for it is left (servers_named.c)
int cli_servers_set_checkers(const CliServersRootSinkOutCfg *args);

struct CliServersRootCfg
{
    const char *root;
    const char *path;
    const char *language;
    const char *text;
    unsigned long long size;
    const CliServersSink *sink;
    VerbaLinea *out;
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
    CliTextPlace from;
    CliTextPlace to;
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

struct CliServersRoomCfg
{
    unsigned int room;
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
    CliServerListed *servers;
    unsigned int room;
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
    int (*colors)(const CliServersPathOutErrMsgFromToCfg *args);
    int (*hints)(const CliServersPathOutErrMsgFromToCfg *args);
} CliServersPathOutErrMsgFromToNs;
EMBED_TABLE_LAYOUT(CliServersPathOutErrMsgFromToNs, close, text_get, colors, hints);
EMBED_TABLE_STORAGE CliServersPathOutErrMsgFromToNs cli_servers_path_out_err_msg_from_to EMBED_UNUSED = {
    .close = cli_servers_close,
    .text_get = cli_servers_text_get,
    .colors = cli_servers_colors,
    .hints = cli_servers_hints,
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
    int (*plugin_start)(const CliServersRootSinkCfg *args);
    void (*plugin_stop)(const CliServersRootSinkCfg *args);
} CliServersRootSinkNs;
EMBED_TABLE_LAYOUT(CliServersRootSinkNs, plugin_start, plugin_stop);
EMBED_TABLE_STORAGE CliServersRootSinkNs cli_servers_root_sink EMBED_UNUSED = {
    .plugin_start = cli_servers_plugin_start,
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
    int (*plugin_answer)(const CliServersErrMsgIdCfg *args);
} CliServersErrMsgIdNs;
EMBED_TABLE_LAYOUT(CliServersErrMsgIdNs, plugin_answer);
EMBED_TABLE_STORAGE CliServersErrMsgIdNs cli_servers_err_msg_id EMBED_UNUSED = {
    .plugin_answer = cli_servers_plugin_answer,
};

typedef struct
{
    char *(*text_take)(const CliServersPathSizeKindCfg *args);
} CliServersPathSizeKindNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeKindNs, text_take);
EMBED_TABLE_STORAGE CliServersPathSizeKindNs cli_servers_path_size_kind EMBED_UNUSED = {
    .text_take = cli_servers_text_take,
};

typedef struct
{
    int (*hover)(const CliServersPathOutCfg *args);
    int (*definition)(const CliServersPathOutCfg *args);
    int (*complete)(const CliServersPathOutCfg *args);
    int (*references)(const CliServersPathOutCfg *args);
    int (*renamable)(const CliServersPathOutCfg *args);
    int (*rename)(const CliServersPathOutCfg *args);
    int (*signature)(const CliServersPathOutCfg *args);
    int (*call_root)(const CliServersPathOutCfg *args);
} CliServersPathOutNs;
EMBED_TABLE_LAYOUT(CliServersPathOutNs, hover, definition, complete, references, renamable, rename, signature,
                   call_root);
EMBED_TABLE_STORAGE CliServersPathOutNs cli_servers_path_out EMBED_UNUSED = {
    .hover = cli_servers_hover,
    .definition = cli_servers_definition,
    .complete = cli_servers_complete,
    .references = cli_servers_references,
    .renamable = cli_servers_renamable,
    .rename = cli_servers_rename,
    .signature = cli_servers_signature,
    .call_root = cli_servers_call_root,
};

typedef struct
{
    int (*actions)(const CliServersPathOutErrMsgFromCfg *args);
    int (*spans)(const CliServersPathOutErrMsgFromCfg *args);
} CliServersPathOutErrMsgFromNs;
EMBED_TABLE_LAYOUT(CliServersPathOutErrMsgFromNs, actions, spans);
EMBED_TABLE_STORAGE CliServersPathOutErrMsgFromNs cli_servers_path_out_err_msg_from EMBED_UNUSED = {
    .actions = cli_servers_actions,
    .spans = cli_servers_spans,
};

typedef struct
{
    int (*act)(const CliServersPathSizeCfg *args);
} CliServersPathSizeNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeNs, act);
EMBED_TABLE_STORAGE CliServersPathSizeNs cli_servers_path_size EMBED_UNUSED = {
    .act = cli_servers_act,
};

typedef struct
{
    int (*calls)(const CliServersPathCfg *args);
} CliServersPathNs;
EMBED_TABLE_LAYOUT(CliServersPathNs, calls);
EMBED_TABLE_STORAGE CliServersPathNs cli_servers_path EMBED_UNUSED = {
    .calls = cli_servers_calls,
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
    unsigned int (*tools_for)(const CliServersRoomCfg *args);
} CliServersRoomNs;
EMBED_TABLE_LAYOUT(CliServersRoomNs, tools_for);
EMBED_TABLE_STORAGE CliServersRoomNs cli_servers_room EMBED_UNUSED = {
    .tools_for = cli_servers_tools_for,
};

typedef struct
{
    void (*stop)(const CliServersOutCfg *args);
} CliServersOutNs;
EMBED_TABLE_LAYOUT(CliServersOutNs, stop);
EMBED_TABLE_STORAGE CliServersOutNs cli_servers_out EMBED_UNUSED = {
    .stop = cli_servers_stop,
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
    int (*set_checkers)(const CliServersRootSinkOutCfg *args);
} CliServersRootSinkOutNs;
EMBED_TABLE_LAYOUT(CliServersRootSinkOutNs, set_checkers);
EMBED_TABLE_STORAGE CliServersRootSinkOutNs cli_servers_root_sink_out EMBED_UNUSED = {
    .set_checkers = cli_servers_set_checkers,
};

#endif
