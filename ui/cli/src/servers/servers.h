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

#include "../text/edits.h"

// Where what the servers send unprompted goes: a function that receives each event, `lsp-diagnostics`, `lsp-edits`,
// `lsp-hints`, `lsp-parse` or `tree-check`, with its JSON, its context, and the tree its paths are written in
typedef struct
{
    void (*emit)(void *context, const char *event, const char *json, unsigned long long size);
    void *context;
    char tree[CLI_PATH_MAX_LEN];
} CliServersSink;

// the file at `path` handed to its language's server, started at `root` where it is not running, and its text to the
// inspections and the checkers: `true` or `false` written as a server took it; its new text given; and the file let go
// as the editor closes it, read again as the disk stores it. Each 1, or 0 with why given (servers_files.c)
int cli_servers_open(const char *root, const char *path, const char *language, const char *text,
                     unsigned long long size, const CliServersSink *sink, VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_change(const char *path, const char *text, unsigned long long size, VerbaLinea *err_msg);
int cli_servers_close(const char *path, VerbaLinea *err_msg);
// the view of `path` the page rests on, its lines `first` to `last` and the lines `opener`, `openers` of them, opening
// each region its first line stands inside, outermost first: where its server is given the tree's slice in place of
// the file and was given another view's, the slice cut for this view and given as the file's next version, its parse
// forgotten, `true` written, and `false` where nothing changed. 1, or 0 with why given (servers_slice.c)
int cli_servers_view(const char *path, unsigned int first, unsigned int last, const unsigned int *opener,
                     unsigned int openers, VerbaLinea *out, VerbaLinea *err_msg);

// the language server the script plugin `id` brings, as its plugin.json's `server` names it, started at `root` with
// `length` bytes of JSON `options` as the options it starts with and the `firsts` arguments `first` before those its
// plugin.json names, what it sends going where `emit` sends it, and handed every file of its languages the editor has
// open: 1, or 0 with why given; and the plugin's server stopped, where it runs. A request sent to it, its answer waited
// for up to `wait` milliseconds into `result`; a notification sent to it; and a request it sent, of the id `request`
// the window was given, answered with `length` bytes of JSON `result`. Each 1, or 0 with why given (servers_plugins.c)
int cli_servers_plugin_start(const char *root, const char *id, const char *options, unsigned long long length,
                             const char *const *first, unsigned int firsts, const CliServersSink *sink,
                             VerbaLinea *err_msg);
void cli_servers_plugin_stop(const char *id);
int cli_servers_plugin_request(const char *id, const char *method, const char *params, unsigned long long length,
                               unsigned long long wait, VerbaLinea *result, VerbaLinea *err_msg);
int cli_servers_plugin_notify(const char *id, const char *method, const char *params, unsigned long long length,
                              VerbaLinea *err_msg);
int cli_servers_plugin_answer(const char *id, const char *request, const char *result, unsigned long long length,
                              VerbaLinea *err_msg);
// the text of a file as the editor has it, where the editor has it open, into `out`: 1, or 0 where it has none; and
// the same copied into a cell of its size with a 0 after, its size into `size`: the cell, or NULL where the editor
// has none or the cell could not be taken
int cli_servers_text_of(const char *path, VerbaLinea *out);
char *cli_servers_text_taken(const char *path, unsigned long long *size);

// what the server of `path` answers at `line`, `col`: a hover as Markdown, `null` for none; the definitions, each
// `{"path", "line", "col"}`; the completions, orior's own reading's of a build file or JSON first, each an item; every
// usage, each `{"path", "from", "to", "text"}`, the declaration among them; whether the symbol can be renamed,
// `{"from", "to", "name"}` or `null`; the edits that rename it to `name`, each file's `{"path", "edits"}`; and the
// signature of the call the cursor is in, `{"label", "params", "active", "doc", "count"}` or `null`. Each 1, or 0 with
// why given (servers_request.c, servers_shapes.c)
int cli_servers_hover(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_definition(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                           VerbaLinea *err_msg);
int cli_servers_complete(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_references(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                           VerbaLinea *err_msg);
int cli_servers_renamable(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_rename(const char *tree, const char *path, unsigned int line, unsigned int col, const char *name,
                       VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_signature(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg);

// the quick fixes and refactorings for the span `from`, `to` of `path`, orior's own and the checkers' fixes of what
// touches it first, then its server's and each plugin server's that has open it, each `{"title", "kind", "preferred",
// "disabled", "raw"}` (servers_actions.c); and an action carried out from its `raw`, `size` bytes of JSON, its edits
// written as each file's `{"path", "edits"}` (servers_act.c). Each 1, or 0 with why given
int cli_servers_actions(const char *path, CliTextPlace from, CliTextPlace to, VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_act(const char *tree, const char *path, const char *raw, unsigned long long size, VerbaLinea *out,
                    VerbaLinea *err_msg);

// the lines `from` to `to` of the parse of `path`, orior's own or its server's semantic tokens, and its folds,
// `{"whole", "from", "lines", "folds"}` or `null`; the spans that contain `from` to `to` and are more than it, the
// least first; the hints its server writes on the lines `from` to `to`; and the parses that came from servers
// forgotten, for each to be requested again. Each 1, or 0 with why given (servers_parse.c)
int cli_servers_colors(const char *path, unsigned int from, unsigned int to, VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_spans(const char *path, CliTextPlace from, CliTextPlace to, VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_hints(const char *path, unsigned int from, unsigned int to, VerbaLinea *out, VerbaLinea *err_msg);
void cli_servers_forget_tokens(void);

// the function at `line`, `col` of `path` as the top of its call hierarchy, `null` where there is none; and the
// functions that call the function `item`, `size` bytes of JSON, where `incoming`, or that it calls, each `{"name",
// "detail", "path", "from", "to", "site", "at", "item"}`. Each 1, or 0 with why given (servers_calls.c)
int cli_servers_call_root(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                          VerbaLinea *err_msg);
int cli_servers_calls(const char *tree, const char *path, const char *item, unsigned long long size, int incoming,
                      VerbaLinea *out, VerbaLinea *err_msg);

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
unsigned int cli_servers_listed(CliServerListed *servers, unsigned int room);
unsigned int cli_servers_tools_for(const char *const *languages, unsigned int count, char (*tools)[CLI_NAME_MAX_LEN],
                                   unsigned int room);
void cli_servers_stop(const char *const *tools, unsigned int count, VerbaLinea *out);
void cli_servers_stop_all(void);
void cli_servers_let_go(void);
void cli_servers_settings_changed(void);

// the tree's check started over `files`, `count` of them, each by its path in the tree at `root`: how many files wait;
// the check stopped where it stands, the files waiting let go and the check reported done, on for the files that change
// after; whether it has files waiting or being checked; and a batch of `count` files changed on the disk, each by its
// path in the tree, checked again with the files whose includes or imports name one of them, a file gone losing its
// diagnostics (servers_check.c, servers_changed.c)
unsigned int cli_servers_check_tree(const char *root, const char *const *files, unsigned int count,
                                    const CliServersSink *sink);
void cli_servers_stop_check(void);
int cli_servers_checking(void);
void cli_servers_changed(const char *root, const char *const *files, unsigned int count);

// the checkers the reader named, `count` names, by name or id, each file the editor has open checked again with them
// and the tree where its check is on; the names no checker answers to written as an array: 1, or 0 with why given where
// no room for it is left (servers_named.c)
int cli_servers_set_checkers(const char *const *names, unsigned int count, const char *root, const CliServersSink *sink,
                             VerbaLinea *out, VerbaLinea *err_msg);

#endif
