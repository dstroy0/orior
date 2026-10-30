// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_internal.h: what the servers' sources share: a server's spec as toolchains.json writes it; the servers
// running, those that failed and how often each ended; the texts and parses of the files the editor has open; the
// tree's check, its queue and what it receives; each file's diagnostics as its server last gave them, the inspections'
// findings, the checkers' and what a build tool and a family give; and the shapes the servers' answers are written in
#ifndef SERVERS_INTERNAL_H
#define SERVERS_INTERNAL_H

#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../comms/container.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/keyed.h"
#include "../text/pieces.h"
#include "../text/text.h"
#include "lsp.h"
#include "servers.h"

// the most servers running, toolchains that failed and that ended, words a server starts with, languages and
// extensions' names of one, and checkers named
#define CLI_SERVERS_MOST 16u
#define CLI_SERVERS_FAILED 64u
#define CLI_SERVERS_ARGS 32u
#define CLI_SERVERS_LANGUAGES 64u
#define CLI_SERVERS_CHECKERS 64u

// how long a request is waited on, and a search of the whole tree, in milliseconds; how many files the check has open
// at once; how long it waits for a file's diagnostics and for any after them; how often an ended server is started
// again, at most that many times within that many milliseconds; and the most bytes of a file the inspections and the
// check read
#define CLI_SERVERS_REQUEST_TIMEOUT_MS 5000ull
#define CLI_SERVERS_SEARCHING 60000ull
#define CLI_SERVERS_CHECKING 6u
#define CLI_SERVERS_PATIENCE 30000ull
#define CLI_SERVERS_QUIET 250ull
#define CLI_SERVERS_RESTARTS 3u
#define CLI_SERVERS_RESTARTS_WITHIN 60000ull
#define CLI_SERVERS_LARGEST (2ull << 20u)

// the name the findings of the files orior_cli reads itself are stored by, beside the checkers'
#define CLI_SERVERS_OWN "orior"

// the most bytes of a server's answer, and of the texts of its tree, and the most nodes the tree contains
#define CLI_SERVERS_ANSWER (8ull << 20u)
#define CLI_SERVERS_NODES (1u << 18u)

// the bytes of each piece a server's answer is buffered in as it comes
#define CLI_SERVERS_PIECE 4096u

// A server's answer read: its lock; the answer as the server wrote it, joined into a cell of its size, and its size;
// the tree it reads to, taken to the text it reads; and the pieces the answer is buffered in as it comes, over a piece
// of its own. The answer and the tree are given back as the next is read and as the lock is
typedef struct
{
    CliLock lock;
    char *answer;
    unsigned long long answer_size;
    CliJson json;
    CliPieces pieces;
    char piece[CLI_SERVERS_PIECE];
} CliServersReading;

// a reading's lock taken; and its answer and tree given back with the lock (servers_shapes.c)
void cli_servers_reading_locked(CliServersReading *reading);
void cli_servers_reading_released(CliServersReading *reading);
// `line` begun as the line a server's answer is written to, buffered in pieces as it comes; and the answer it contains
// joined into the reading's answer: 1, or 0 where it is not whole or the cells cannot meet it, the answer empty
void cli_servers_answer_begun(CliServersReading *reading, VerbaLinea *line);
int cli_servers_answer_joined(CliServersReading *reading, VerbaLinea *line);

// A toolchain's server: its toolchain and its name, its program, the words it starts with, the languages it serves, the
// protocol's name for each extension's language, the line that installs it, the file that marks a project, the
// notification that has it check its projects, the options it starts with in a tree whose own code is disabled and in
// one whose code runs, as JSON, empty for none, and whether a file the editor has open is given it as the tree's slice
// for the lines in view in place of the file's whole text
typedef struct
{
    char tool[CLI_NAME_MAX_LEN];
    char name[CLI_NAME_MAX_LEN];
    char program[CLI_NAME_MAX_LEN];
    char arg[CLI_SERVERS_ARGS][256];
    unsigned int args;
    char language[CLI_SERVERS_LANGUAGES][CLI_NAME_MAX_LEN];
    unsigned int languages;
    char id_extension[CLI_SERVERS_LANGUAGES][32];
    char id_name[CLI_SERVERS_LANGUAGES][CLI_NAME_MAX_LEN];
    unsigned int ids;
    char setup[1024];
    char projects[CLI_NAME_MAX_LEN];
    char checks[CLI_NAME_MAX_LEN];
    char blocked_args[8192];
    char trusted[8192];
    int slices;
} CliServerSpec;

// the asks a server given the tree's slices remembers the times of; and the quiet it is let go after, in
// milliseconds: the least, what each ask of the last minute adds, the most, and the minute
#define CLI_SERVERS_ASKS 128u
#define CLI_SERVERS_QUIET_LEAST 2000ull
#define CLI_SERVERS_QUIET_EACH 500ull
#define CLI_SERVERS_QUIET_MOST 60000ull
#define CLI_SERVERS_ASKS_WITHIN 60000ull

// A server running: whether its place is taken, the server, and its spec; and for a server given the tree's slices,
// the asks waiting on it, the times of its last asks in a ring and how many it was asked, and when it was last asked
// or answered
typedef struct
{
    int used;
    CliLsp *lsp;
    CliServerSpec spec;
    unsigned int waits;
    unsigned long long asked[CLI_SERVERS_ASKS];
    unsigned long long asks;
    unsigned long long quiet_from;
} CliServerRunning;

// The servers, under their lock: the tree they serve, those running, the toolchains that failed and why, how often
// each ended, the checkers named, the tree they check, and where what is reported goes
typedef struct
{
    CliLock lock;
    char root[CLI_PATH_MAX_LEN];
    CliServerRunning running[CLI_SERVERS_MOST];
    char failed_tool[CLI_SERVERS_FAILED][CLI_NAME_MAX_LEN];
    char failed_msg[CLI_SERVERS_FAILED][1024];
    unsigned int failed;
    char restart_tool[CLI_SERVERS_FAILED][CLI_NAME_MAX_LEN];
    CliRestarts restarts[CLI_SERVERS_FAILED];
    unsigned int restarted;
    char checker[CLI_SERVERS_CHECKERS][CLI_NAME_MAX_LEN];
    unsigned int checkers;
    char checked_root[CLI_PATH_MAX_LEN];
    CliServersSink sink;
    int has_sink;
} CliServersState;

extern CliServersState g_cli_servers;

// One file waiting for the tree's check, besides its path: the server it goes to, referenced for it, and the protocol's
// name for its language
typedef struct
{
    CliLsp *server;
    char id[CLI_NAME_MAX_LEN];
} CliServersWaiting;

// A file waiting for the tree's check: its path in the paths' room and its size, and its server and language
typedef struct
{
    unsigned long long path;
    unsigned int size;
    CliServersWaiting waiting;
} CliServersQueued;

// The tree's check: whether it is on, its tree, where what it reports goes; its room, four cells each with its most:
// the tree's files' paths, where each starts, the files waiting in order from `head` in a ring, and their paths; how
// many it was given and how many are done, and whether its threads started
typedef struct
{
    int on;
    char root[CLI_PATH_MAX_LEN];
    CliServersSink sink;
    int has_sink;
    char *files;
    unsigned long long files_most;
    unsigned long long files_used;
    unsigned long long *file_at;
    unsigned int file_at_most;
    unsigned int file_count;
    CliServersQueued *waiting;
    unsigned int waiting_most;
    unsigned int head;
    unsigned int count;
    char *room;
    unsigned long long room_most;
    unsigned long long used;
    unsigned long long total;
    unsigned long long done;
    int started;
} CliServersQueue;

// the check's room given back, where `queue` has one; taken where it has none, each part at what it begins with:
// 1, or 0 where the cells cannot meet it; the place of the `at`th file waiting; a file put in the queue, at its front
// where `front`, the room grown where it is full: 1, or 0 where it has no room; and the tree's files recorded, those of
// `files` and those recorded before. Each with the queue's lock acquired (servers_check_room.c)
void cli_servers_check_room_given(CliServersQueue *queue);
int cli_servers_check_room_made(CliServersQueue *queue);
CliServersQueued *cli_servers_check_queued_at(const CliServersQueue *queue, unsigned int at);
int cli_servers_check_queue_put(CliServersQueue *queue, const char *path, const CliServersWaiting *waiting, int front);
void cli_servers_check_files_put(CliServersQueue *queue, const char *const *files, unsigned int count);

// the texts of the files the editor has open, by their keys with their paths, and the parses as last read, by their
// keys with their texts' hashes, under the texts' lock (servers_files.c)
extern CliKeyed g_cli_servers_texts;
extern CliKeyed g_cli_servers_parses;
extern CliLock g_cli_servers_texts_lock;

// each file's diagnostics as its server last gave them, a JSON array by its key with its URI, under their own lock;
// and, by the file's key under the checks' lock, the inspections' findings, a JSON array with the file's URI; the
// checkers', a JSON object of each checker's array by its id, its ids in order, with the URI; the names a build tool
// gives the file, one to a line; and what each Python class's family sets, `class attribute` one to a line
// (servers_emit.c)
extern CliKeyed g_cli_servers_published;
extern CliLock g_cli_servers_published_lock;
extern CliKeyed g_cli_servers_inspected;
extern CliKeyed g_cli_servers_checked;
extern CliKeyed g_cli_servers_given;
extern CliKeyed g_cli_servers_supplied;
extern CliLock g_cli_servers_checks_lock;

// the most bytes of a job of the servers: a file's text and what is reported of it
#define CLI_SERVERS_JOB_ROOM (CLI_SERVERS_LARGEST + (64ull << 10u))

// A job of the servers done on one of their threads, from `size` bytes of it, its own copy of what it was handed
typedef void (*CliServersJob)(const void *job, unsigned long long size);
// `size` bytes of a job handed to the servers' threads in a cell of their size, waiting where every place is taken: 1,
// or 0 where the cell could not be taken; and a cell of `size` bytes of a job, taken by the caller, handed over whole
// and given back once the job has run: 1, or 0 where `job` is NULL (servers_jobs.c)
int cli_servers_later(CliServersJob run, const void *job, unsigned long long size);
int cli_servers_later_handed(CliServersJob run, char *job, unsigned long long size);

// the spec of the server that serves `language`: 1, or 0 where none does; the server of `language` for the tree at
// `root`, started where it is not running, referenced for the caller, NULL with why given where none could start or
// none serves it, which is reported as nothing; the server that has `path` open, referenced for the caller, NULL where
// none does; whether `server` is given the tree's slices in place of files, as its spec says; and the protocol's name
// for the language of `path` by its extension (servers_start.c)
int cli_servers_spec_for(const char *language, CliServerSpec *spec);
CliLsp *cli_servers_server(const char *root, const char *language, const CliServersSink *sink, CliServerSpec *spec,
                           VerbaLinea *err_msg);
CliLsp *cli_servers_owner_of(const char *path);
int cli_servers_slicing(const CliLsp *server);
// the server of the file at `path` a slice of it is given to, started again where it is given the tree's slices and was
// let go, the file opened in it with its stored slice and the editor's text: the server, referenced for the caller, or
// NULL where none has the file open and none starts; and the server a request about the file at `path` is asked of,
// referenced for the caller: NULL where none has it open or it is given its server as slices, as such a server is
// asked for nothing but the problems it publishes (servers_keep.c)
CliLsp *cli_servers_owner_started(const char *path);
CliLsp *cli_servers_owner_answering(const char *path);
// an ask of `server` begun and ended, a server given the tree's slices waited on between them; an ask of it noted that
// waits on nothing; and the servers given the tree's slices let go once nothing waits on them, no slice owes its
// diagnostics, the tree's check is not running and each has been quiet for as long as its asks of the last minute
// earn it, watched on a thread of their own started with the first; the language a file given its server as a slice
// opened as stored, for it to be opened again in the server started after it, and let go as it closes (servers_keep.c)
void cli_servers_ask_begun(const CliLsp *server);
void cli_servers_ask_ended(const CliLsp *server);
void cli_servers_ask_noted(const CliLsp *server);
void cli_servers_language_noted(const char *path, const char *language);
void cli_servers_language_forgotten(const char *path);
void cli_servers_id_of(const CliServerSpec *spec, const char *path, const char *language, char *id);

// the server of `spec` started for the tree at `root`, with the options and the environment it takes, referenced once
// for the servers running: it, or NULL with why given (servers_launch.c)
CliLsp *cli_servers_launch(const char *root, const CliServerSpec *spec, const CliServersSink *sink,
                           VerbaLinea *err_msg);
// what a server sends unprompted, its context where it goes: a file's diagnostics, an edit to make, the settings it
// requests, and the hints and tokens to request again (servers_recv.c)
void cli_servers_recv(void *context, const char *method, const char *params, unsigned long long size,
                      VerbaLinea *answer);
// a server stopped on a thread of its own, its reference given back, or here where no thread is free
// (servers_running.c)
void cli_servers_stop_later(CliLsp *lsp);

// `size` bytes of JSON read into `reading`'s tree, taken to their size, the tree it contained given back: its root, or
// CLI_JSON_NONE where it is no JSON or the cells cannot meet it; the reading's answer joined from `line` and read the
// same way; a number as a whole
// number, `otherwise` where the node is none or no number; and the place `{"line", "character"}` (servers_shapes.c)
unsigned int cli_servers_read(CliServersReading *reading, const char *text, unsigned long long size);
unsigned int cli_servers_answer_read(CliServersReading *reading, VerbaLinea *line);
// `method` sent to `server` with the params `params` contains in `pieces`, joined into a cell of their size and given
// back once sent, its answer written into `result`: 1, or 0 with why given; and the same with the params of a
// request about the place `line`, `col` of `path`, `added` after the place where it is given
int cli_servers_request(CliLsp *server, const char *method, CliPieces *pieces, VerbaLinea *params,
                        unsigned long long wait, VerbaLinea *result, VerbaLinea *err_msg);
int cli_servers_request_at(CliLsp *server, const char *method, const char *path, unsigned int line, unsigned int col,
                           const char *added, unsigned long long wait, VerbaLinea *result, VerbaLinea *err_msg);
unsigned long long cli_servers_whole(const CliJson *json, unsigned int node, unsigned long long otherwise);
CliTextPlace cli_servers_place(const CliJson *json, unsigned int node);
// a path written as a JSON string as it stands in `tree` (servers_shapes.c)
void cli_servers_path_written(const char *tree, const char *path, VerbaLinea *out);
// a workspace edit, in either of the shapes the protocol allows, as each file's edits, `[{"path", "edits"}]`, each
// file's first in the file first, a file made, moved or removed not among them; and the cells one occupies while it is
// written (servers_edits.c)
void cli_servers_workspace_edit_of(const CliJson *json, unsigned int edit, const char *tree, VerbaLinea *out);
extern CellOwner g_cli_servers_edits_cells;
// every file named `name` in the tree at `root`, outside the folders a build writes and at most six folders down,
// sorted, written as a JSON array into `out`; and the cells a server's start occupies while it starts
// (servers_launch.c)
void cli_servers_projects_in(const char *root, const char *name, VerbaLinea *out);
extern CellOwner g_cli_servers_launch_cells;
// a hover's contents as Markdown, from any of the shapes the protocol allows, its letters as they are
void cli_servers_markdown_of(const CliJson *json, unsigned int contents, VerbaLinea *out);
// a completion item of the server's put among `items`
void cli_servers_item_of(const CliJson *json, unsigned int one, CliCompletions *items);
// a call's signature written, `null` where the server gave none; a code action written, `{"orior_plugin", "action"}`
// its raw where the server of the plugin `plugin` offers it: 1, or 0 where it has no title; and whether the span
// `from`, `to` shares a place with `range`, a span with no width touching the other counting (servers_offers.c)
void cli_servers_signature_of(const CliJson *json, unsigned int help, VerbaLinea *out);
int cli_servers_action_of(const CliJson *json, unsigned int one, const char *plugin, VerbaLinea *out);
int cli_servers_overlaps(CliTextPlace from, CliTextPlace to, const CliJson *json, unsigned int range);
// a diagnostic of the server's written as orior's, `{"from", "to", "severity", "message", "markdown", "source",
// "code", "href"}`; and a location written as `{"path", "line", "col"}`: 1, or 0 where it names no file
void cli_servers_diagnostic_of(const CliJson *json, unsigned int one, VerbaLinea *out);
int cli_servers_found_of(const CliJson *json, unsigned int location, const char *tree, VerbaLinea *out);

// whether the server's capabilities name `name` as on, as neither absent, null nor false; and the spans of the
// editor's text of `path` that contain `from` to `to` and are more than it, the least first, the tree's, read off the
// branch under `key` where one is stored and `key` is not NULL, else off the text the servers were told, written as
// `[[from, to]]`, `[]` where neither holds a text of it or its grammar names none: 1, or 0 with why given
// (servers_hints.c)
int cli_servers_capable(CliLsp *server, const char *name);
// whether the member `inner` of the capability `name` is true
int cli_servers_capable_true(CliLsp *server, const char *name, const char *inner);
// a member of `node` written as JSON, `null` where it has none (servers_actions.c)
void cli_servers_member_written(const CliJson *json, unsigned int node, const char *name, VerbaLinea *out);
// whether the server runs the command `command` itself, as its executeCommandProvider lists it
int cli_servers_capable_command(CliLsp *server, const char *command);
int cli_servers_selection(const char *path, const char *key, CliTextPlace from, CliTextPlace to, VerbaLinea *out,
                          VerbaLinea *err_msg);

// the hash of `size` bytes of `text`, FNV-1a over its bytes, to determine whether what was read of a text is still
// what it reads (servers_parse.c)
unsigned long long cli_servers_hash_of(const char *text, unsigned long long size);

// a file the editor has open, `size` bytes of `text`, given `server` as the tree's slice for its view, the lines 0 to
// 63 until the page hands one, as the file opened as `id` where `id` is not NULL, else as its next version, the slice's
// map stored by its key, in the tree at `root`, or the tree it was first given in where `root` is NULL: 1, or 0 with
// why given; whether the file at `path` is given its server as a slice; whether any slice given a server owes its
// diagnostics, its version above the last its server published of it; and its slice let go as it closes
// (servers_slice.c)
int cli_servers_slice_sent(CliLsp *server, const char *root, const char *path, const char *id, const char *text,
                           unsigned long long size, VerbaLinea *err_msg);
int cli_servers_sliced(const char *path);
int cli_servers_slices_owed(void);
void cli_servers_slice_closed(const char *path);
// the line of the slice of the file at `path` that came from its own line `line` into `into`: 1, or 0 where none did,
// a file given whole its own line; and a line of the slice moved to the file and line it came from, `from` the file's
// path, sized for CLI_PATH_MAX_LEN: 1 where it is the file's own, 2 where another file's, 0 where none, a bracket the
// slice closes, a file given whole its own
int cli_servers_slice_in(const char *path, unsigned int line, unsigned int *into);
int cli_servers_slice_out(const char *path, unsigned int *line, char *from);
// the object `one` of `json`, about the file at `path`, written with the lines of its range moved to the slice's: 1, or
// 0 with nothing written where the slice holds none of them; and the diagnostics `all`, `size` bytes, the server
// published with `params`, `params_size` bytes, about the sliced file at `path`, moved to the file's own lines, those
// on another file's lines left out: their JSON in a cell counted to `owner` the caller gives back, its size into
// `mapped_size`, or NULL where they are of a version given before the slice stored was or the cells cannot meet them
int cli_servers_slice_ranged_in(const char *path, const CliJson *json, unsigned int one, VerbaLinea *out);
char *cli_servers_slice_diagnostics(const char *path, const char *params, unsigned long long params_size,
                                    const char *all, unsigned long long size, unsigned long long *mapped_size,
                                    CellOwner *owner);
// A window's slice of a file the check gives its server in slices; what each window is handed to with its slice and
// its first and last lines, 1 to cut the next, 0 to stop; the file at `path` in the tree at `root`, `size` bytes of
// `text`, cut in one walk into the slices of its windows, a view's height each, each handed to `window` as the walk
// reaches it: 1, or 0 with why given where the cells cannot meet them; and a window's slice `cut` stored by the file's
// key with its map and given `server` as the file opened as `id` where `id` is not NULL, else as its next version: 1,
// or 0 with why given (servers_slice.c)
typedef struct CliServersCut CliServersCut;
typedef int (*CliServersWindow)(void *context, const CliServersCut *cut, unsigned int first, unsigned int last);
int cli_servers_slices_cut(const char *root, const char *path, const char *text, unsigned long long size,
                           CliServersWindow window, void *context, VerbaLinea *err_msg);
int cli_servers_cut_sent(CliLsp *server, const char *id, const CliServersCut *cut, VerbaLinea *err_msg);

// for a file at `path` its server is given as a slice, answered from the tree whose top is `tree`: the definition in
// effect of the name at `line`, `col`, written as `[{"path", "line", "col"}]`, `[]` where the tree knows none, 1, or 0
// with why given; the usages of the name, each `{"path", "from", "to", "text"}`; whether it can be renamed, `{"from",
// "to", "name"}`; and the edits that rename it to `name`, each file's `{"path", "edits"}`. Each of these 1, 0 with
// nothing written where the tree knows no definition of the name, or -1 with why given (servers_usages.c)
int cli_servers_definition_written(const char *tree, const char *path, unsigned int line, unsigned int col,
                                   VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_usages_written(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                               VerbaLinea *err_msg);
int cli_servers_renamable_written(const char *tree, const char *path, unsigned int line, unsigned int col,
                                  VerbaLinea *out, VerbaLinea *err_msg);
int cli_servers_rename_written(const char *tree, const char *path, unsigned int line, unsigned int col,
                               const char *name, VerbaLinea *out, VerbaLinea *err_msg);

// a file's key and its URI, each into what is sized for CLI_PATH_MAX_LEN (servers_emit.c)
void cli_servers_key_of(const char *path, char *key);
void cli_servers_uri_of(const char *path, char *uri);
// a file's diagnostics passed on whole: its server's, less its reports of an attribute a class's family sets and of a
// name a build tool gives it, then the inspections', then each checker's; and a checker's findings stored, `[[path,
// [diagnostic]]]`, each file's diagnostics passed on whose findings changed (servers_emit.c)
void cli_servers_emit_whole(const char *key, const char *uri, const CliServersSink *sink);
// every file it names passed on where `always`, changed or the same, as a file the editor opens receives its findings
// (servers_findings.c)
void cli_servers_findings_stored(const char *id, const char *found, unsigned long long size, int always,
                                 const CliServersSink *sink);
// a path as it stands in `tree`, its slashes forward, or whole where it stands outside it or `tree` is NULL
void cli_servers_tree_path(const char *tree, const char *path, VerbaLinea *out);

// a file's text read into the inspections' tree, or the file taken out of it where `text` is NULL, each file's
// findings that changed passed on, the root's Python and JavaScript read first on a thread of its own where the tree
// contains another root or none; and the same for a file of the root the tree contains (servers_inspect.c)
void cli_servers_inspect(const char *root, const char *path, const char *text, unsigned long long size,
                         const CliServersSink *sink);
void cli_servers_inspect_again(const char *path, const char *text, unsigned long long size);
// the inspections' tree let go, as another tree opens; and the root the tree was read from into `root`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 where it contains none
void cli_servers_inspect_let_go(void);
int cli_servers_tree_read(char *root);
// the diagnostics of each file whose server reports an attribute a class has not passed on whole again, as what a
// class's family sets moved (servers_emit.c)
void cli_servers_emit_reporting_attributes(const CliServersSink *sink);

// a file the editor opens of `language` handed to each plugin server that serves the language: 1 where one took it;
// its new text to each that has it open, and its closing sent to each that has it open (servers_plugins.c)
int cli_servers_plugins_opened(const char *path, const char *language, const char *text, unsigned long long size);
void cli_servers_plugins_changed(const char *path, const char *text, unsigned long long size);
void cli_servers_plugins_closed(const char *path);
// the server of plugin `id`, referenced, or NULL with why given where none runs; the next plugin server from the place
// `at` that has `path` open, referenced, its plugin's id into `id`, sized for CLI_NAME_MAX_LEN, and `at` moved past it,
// or NULL where none is left; and a command of an action of the server of plugin `id` it does not run itself, `size`
// bytes of
// `{command, arguments}`, passed to the plugin as `orior/command` (servers_plugins.c)
CliLsp *cli_servers_plugin_of(const char *id, VerbaLinea *err_msg);
CliLsp *cli_servers_plugin_owner_of(unsigned int *at, const char *path, char *id);
void cli_servers_plugin_command(const char *id, const char *command, unsigned long long size);
// the program a plugin's server starts as, found by its toolchain where `server` in `json` names one, into `program`,
// and the folder it was found in into `folder`, each sized for CLI_PATH_MAX_LEN: 1, or 0 with why given; the
// `length` bytes of `text` written into `written` with each `{folder}` in them `folder`, escaped for a JSON string
// where `escaped`; and the arguments of `server` put into `words` the same way (servers_plugins_program.c)
int cli_servers_plugin_program(const CliJson *json, unsigned int server, char *program, char *folder,
                               VerbaLinea *err_msg);
void cli_servers_plugin_folder_put(VerbaLinea *written, const char *text, unsigned long long length, const char *folder,
                                   int escaped);
void cli_servers_plugin_args(const CliJson *json, unsigned int server, const char *folder, CliWords *words);
// one file checked with each checker named for its language, on a thread of its own; a file orior_cli reads itself
// checked as its text stands; and every file of the tree orior_cli reads itself checked as the disk stores it
// (servers_checkers.c)
void cli_servers_run_checkers(const char *root, const char *path, const char *text, unsigned long long size, int disk,
                              const CliServersSink *sink);
void cli_servers_check_own(const char *root, const char *path, const char *text, unsigned long long size,
                           const CliServersSink *sink);
void cli_servers_check_tree_own(const char *root, const char *const *files, unsigned int count,
                                const CliServersSink *sink);
void cli_servers_check_tree_with_checkers(const char *root, const CliServersSink *sink);
// whether orior_cli reads the file at `path` itself, as a build file or as JSON by its schema (servers_own.c)
int cli_servers_own_reads(const char *path);
// a run of the checker `id` over the file of `key` counted: its number, which a later run of the same checker over
// the same file stands in for; and whether the run `run` is still the last (servers_checkers.c)
unsigned long long cli_servers_run_counted(const char *key, const char *id);
int cli_servers_run_last(const char *key, const char *id, unsigned long long run);
// whether the tree's check is on (servers_check.c)
int cli_servers_check_on(void);

// the tree's check's queue taken to a file let go, as the editor closes one, the file taken for the check and the
// server referenced for the queue: 1 where the check is on and took it, 0 where the caller closes it; with the taken
// files' lock acquired (servers_check.c)
int cli_servers_check_takes(const char *path, CliLsp *lsp);
// the lock a server is written to about a file under, by the check or by the editor, which the check's taken files are
// guarded by; whether a file is taken for the check and not for the editor; and a file taken, or let go; each with the
// lock acquired
extern CliLock g_cli_servers_check_open_lock;
int cli_servers_check_open(const char *uri);
void cli_servers_check_open_set(const char *uri, int open);
// every file the check took let go of it; and each file `server` has open and the check does not take given to it
// again, for it to read against what changed under it; each with the taken files' lock acquired (servers_check_one.c)
void cli_servers_check_open_cleared(void);
void cli_servers_reread_open(CliLsp *server);
// each of the tree's files the check was given handed to `each`, by its path in the tree: 1, or 0 where the check is
// off or on another tree; the files a change on the disk named recorded among the tree's files, those no longer
// there let go, and where the check's word goes into `emit`: 1, or 0 where the check is off or on another tree; and the
// files `paths`, `count` of them, each with its server, referenced for it, and its language, queued behind those
// waiting, each once, the check notified it is done where none waits: `count` (servers_check.c)
int cli_servers_check_tree_files(const char *root, void (*each)(void *context, const char *file), void *context);
int cli_servers_check_files_changed(const char *root, const char *const *files, unsigned int count,
                                    CliServersSink *sink);
unsigned int cli_servers_check_queued(const char *const *paths, const CliServersWaiting *waiting, unsigned int count,
                                      const CliServersSink *sink);
// the files the editor opens and the check took let go of the check
void cli_servers_let_go_cleared(const char *key);
// the check's queue made empty and its taken files let go, as another tree opens
void cli_servers_check_let_go(void);
// the files of `files`, `count` of them, queued for the tree's check, or their servers have them check their projects:
// how many wait
unsigned int cli_servers_check_files(const char *root, const char *const *files, unsigned int count,
                                     const CliServersSink *sink);
// the file at `path` handed by the checker thread `checker` to its server as the disk stores it, or a window of its
// slices at a time where the server is given the tree's slices, the server queried for its diagnostics or, where it
// refuses to be queried, waited on to give them, and let go where the check still takes it; a file the editor has open
// is left to the editor (servers_check_one.c)
void cli_servers_check_one(unsigned int checker, const char *path, const CliServersWaiting *one,
                           const CliServersSink *sink);
// a file a server's first diagnostics wake the check's wait of, by its key
void cli_servers_woken(const char *key);
// the diagnostics `all`, `size` bytes, moved to the lines of the file of `key`, taken by the checker that gathers its
// diagnostics a window of its slices at a time, in place of those it took last, and its wait woken: 1, or 0 where no
// checker gathers them, theirs to pass on who received them; and the gathering of the file of `key` ended, as the
// editor takes it
int cli_servers_gathered(const char *key, const char *all, unsigned long long size);
void cli_servers_gathering_ended(const char *key);
// whether the check lets go of a file whose diagnostics come empty, as it closes it: 1 where they are not passed on
int cli_servers_letting_go(const char *key);
// what the servers' check reaches: the check set on or off, where it reports how far it has gone, and how far, and
// read back with how many files wait (servers_check.c); a checker made to wait on the file of `key`, whether it was
// woken since, and the file of `key` let go as the check lets go of one (servers_check_one.c); and whether a line
// that includes, imports or uses another names `size` bytes of `stem` as a word (servers_changed.c)
void cli_servers_check_set(int on, const CliServersSink *sink, unsigned long long done, unsigned long long total);
void cli_servers_check_count(int *on, unsigned int *waiting, unsigned long long *done, unsigned long long *total);
void cli_servers_wait_on(unsigned int checker, const char *key);
int cli_servers_was_woken(unsigned int checker);
void cli_servers_let_go_of(const char *key);
int cli_servers_line_names(const char *line, unsigned long long length, const char *stem, unsigned long long size);
// the attribute and the class a server's report that a class has no such attribute names, as pyright writes it:
// `Cannot access attribute "name" for class "Class*"`, each with its size: 1, or 0 where the report is none
// (servers_emit.c)
int cli_servers_attribute_of(const char *message, unsigned long long length, const char **name,
                             unsigned long long *name_size, const char **class_name, unsigned long long *class_size);

#endif
