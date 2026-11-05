// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_internal.h: what the servers' sources share: a server's spec as toolchains.json writes it; the servers
// running, those that failed and how often each ended; the texts and parses of the files the editor has open; the
// tree's check, its queue and what it receives; each file's diagnostics as its server last gave them, the inspections'
// findings, the checkers' and what a build tool and a family give; and the shapes the servers' answers are written in
#ifndef SERVERS_INTERNAL_H
#define SERVERS_INTERNAL_H
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../comms/container.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/keyed.h"
#include "../text/text.h"
#include "embed_dispatch_layout.h"
#include "lsp.h"
#include "servers.h"

typedef struct CliServersPathCtx CliServersPathCtx;
typedef struct CliServersPathErrMsgCtx CliServersPathErrMsgCtx;
typedef struct CliServersPathSizeCtx CliServersPathSizeCtx;
typedef struct CliServersPathSizeRootCtx CliServersPathSizeRootCtx;
typedef struct CliServersPathSizeRootErrMsgCtx CliServersPathSizeRootErrMsgCtx;
typedef struct CliServersLengthCtx CliServersLengthCtx;
typedef struct CliServersErrMsgCtx CliServersErrMsgCtx;
typedef struct CliServersPathSizeRootSinkCtx CliServersPathSizeRootSinkCtx;
typedef struct CliServersPathErrMsgOutCtx CliServersPathErrMsgOutCtx;
typedef struct CliServersSizeCtx CliServersSizeCtx;
typedef struct CliServersTextCtx CliServersTextCtx;
typedef struct CliServersErrMsgJsonCtx CliServersErrMsgJsonCtx;
typedef struct CliServersLanguageCtx CliServersLanguageCtx;
typedef struct CliServersJsonCtx CliServersJsonCtx;
typedef struct CliServersOnCtx CliServersOnCtx;
typedef struct CliServersFilesCtx CliServersFilesCtx;
typedef struct CliServersFilesCountCtx CliServersFilesCountCtx;
typedef struct CliServersPathSinkCtx CliServersPathSinkCtx;
typedef struct CliServersQueueCtx CliServersQueueCtx;
typedef struct CliServersCountCtx CliServersCountCtx;
typedef struct CliServersSinkCtx CliServersSinkCtx;
typedef struct CliServersErrMsgServerCtx CliServersErrMsgServerCtx;
typedef struct CliServersJsonTreeCtx CliServersJsonTreeCtx;
typedef struct CliServersPathLanguageCtx CliServersPathLanguageCtx;
typedef struct CliServersRootCtx CliServersRootCtx;
typedef struct CliServersSizeLengthCtx CliServersSizeLengthCtx;
typedef struct CliServersJsonNodeCtx CliServersJsonNodeCtx;
typedef struct CliServersJsonFromCtx CliServersJsonFromCtx;
typedef struct CliServersJsonServerCtx CliServersJsonServerCtx;
typedef struct CliServersPathSizeLanguageCtx CliServersPathSizeLanguageCtx;
typedef struct CliServersPathJsonCtx CliServersPathJsonCtx;
typedef struct CliServersJsonTreeOutCtx CliServersJsonTreeOutCtx;
typedef struct CliServersServerCtx CliServersServerCtx;
typedef struct CliServersQueueFilesCtx CliServersQueueFilesCtx;
typedef struct CliServersSinkKeyCtx CliServersSinkKeyCtx;
typedef struct CliServersSizeKeyCtx CliServersSizeKeyCtx;
typedef struct CliServersJsonOneCtx CliServersJsonOneCtx;
typedef struct CliServersRunCtx CliServersRunCtx;
typedef struct CliServersRunSizeCtx CliServersRunSizeCtx;
typedef struct CliServersJsonOutCtx CliServersJsonOutCtx;
typedef struct CliServersSizeCommandCtx CliServersSizeCommandCtx;
typedef struct CliServersPathIdCtx CliServersPathIdCtx;
typedef struct CliServersRootOutCtx CliServersRootOutCtx;
typedef struct CliServersReadStateCtx CliServersReadStateCtx;
typedef struct CliServersKeyCtx CliServersKeyCtx;
typedef struct CliServersJsonOutHelpCtx CliServersJsonOutHelpCtx;
typedef struct CliServersPathLineCtx CliServersPathLineCtx;
typedef struct CliServersPathLineFromCtx CliServersPathLineFromCtx;
typedef struct CliServersJsonNodeOtherwiseCtx CliServersJsonNodeOtherwiseCtx;
typedef struct CliServersReadStateAnswerCtx CliServersReadStateAnswerCtx;
typedef struct CliServersServerCommandCtx CliServersServerCommandCtx;
typedef struct CliServersUriCtx CliServersUriCtx;
typedef struct CliServersQueueAtCtx CliServersQueueAtCtx;
typedef struct CliServersPathLspCtx CliServersPathLspCtx;
typedef struct CliServersRootSizeCtx CliServersRootSizeCtx;
typedef struct CliServersPathKeyCtx CliServersPathKeyCtx;
typedef struct CliServersPathUriCtx CliServersPathUriCtx;
typedef struct CliServersKeyCheckerCtx CliServersKeyCheckerCtx;
typedef struct CliServersCtx CliServersCtx;
typedef struct CliServersRootKindCtx CliServersRootKindCtx;

// the most servers running, toolchains that failed and that ended, words a server starts with, languages and
// extensions' names of one, and checkers named
#define CLI_SERVERS_MAX 16u
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

// the most bytes of a text written from one server's answer or one file's findings, and of what a check of the whole
// tree finds; and the most nodes a tree contains
#define CLI_SERVERS_ANSWER (8ull << 20u)
#define CLI_SERVERS_FINDINGS (32ull << 20u)
#define CLI_SERVERS_NODES (1u << 18u)

// the most bytes of a request's params about a file: its path as a URI, a place and a new name
#define CLI_SERVERS_PARAMS (CLI_PATH_MAX_LEN * 4u)

// A server's answer read: its lock, and the tree it reads to, taken to the text it reads, given back as the next is
// read and as the lock is
typedef struct
{
    CliJson json;
} CliServersReadState;

// a reading's lock taken; and its tree given back with the lock (servers_shapes.c)
void cli_servers_read_state_lock(const CliServersReadStateAnswerCtx *args);

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
#define CLI_SERVERS_REQUESTS 128u
#define CLI_SERVERS_QUIET_MIN 2000ull
#define CLI_SERVERS_QUIET_EACH 500ull
#define CLI_SERVERS_QUIET_MAX 60000ull
#define CLI_SERVERS_REQUESTS_WITHIN 60000ull

// A server running: whether its place is taken, the server, and its spec; and for a server given the tree's slices,
// the asks waiting on it, the times of its last asks in a ring and how many it was asked, and when it was last asked
// or answered
typedef struct
{
    int used;
    CliLsp *lsp;
    CliServerSpec spec;
    unsigned int waits;
    unsigned long long requested[CLI_SERVERS_REQUESTS];
    unsigned long long pending_requests;
    unsigned long long quiet_from;
} CliServerRunning;

// The servers, under their lock: the tree they serve, those running, the toolchains that failed and why, how often
// each ended, the checkers named, the tree they check, and where what is reported goes
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    CliServerRunning running[CLI_SERVERS_MAX];
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

// A file waiting for the tree's check: its path in the paths' capacity and its size, and its server and language
typedef struct
{
    unsigned long long path;
    unsigned int size;
    unsigned int integer;
    CliLsp *server;
} CliServersQueued;

// The tree's check: whether it is on, its tree, where what it reports goes; its capacity, four cells each with its
// most: the tree's files' paths, where each starts, the files waiting in order from `head` in a ring, and their paths;
// how many it was given and how many are done, and whether its threads started
typedef struct
{
    int on;
    char root[CLI_PATH_MAX_LEN];
    CliServersSink sink;
    int has_sink;
    char *files;
    unsigned long long files_max;
    unsigned long long files_used;
    unsigned long long *file_at;
    unsigned int file_at_max;
    unsigned int file_count;
    CliServersQueued *waiting;
    unsigned int waiting_max;
    unsigned int head;
    unsigned int count;
    char *capacity;
    unsigned long long capacity_max;
    unsigned long long used;
    unsigned long long total;
    unsigned long long done;
    int started;
} CliServersQueue;

// the check's capacity given back, where `queue` has one; taken where it has none, sized from the tree's files: 1, or 0
// where the cells cannot meet it; the place of the `at`th file waiting; a file put in the queue, at its front where
// `front`: 1, or 0 where it has no capacity; and the tree's files recorded, those of `files` and those recorded before.
// Each with the queue's lock acquired (servers_check_capacity.c)
int cli_servers_check_work_state_init(const CliServersQueueFilesCtx *args);
CliServersQueued *cli_servers_check_queued_at(const CliServersQueueAtCtx *args);
int cli_servers_check_queue_append(const CliServersQueueCtx *args);
void cli_servers_check_files_append(const CliServersQueueFilesCtx *args);

// the texts of the files the editor has open, by their keys with their paths, and the parses as last read, by their
// keys with their texts' hashes, under the texts' lock (servers_files.c)
extern CliKeyed g_cli_servers_texts;
extern CliKeyed g_cli_servers_parses;

// each file's diagnostics as its server last gave them, a JSON array by its key with its URI, under their own lock;
// and, by the file's key under the checks' lock, the inspections' findings, a JSON array with the file's URI; the
// checkers', a JSON object of each checker's array by its id, its ids in order, with the URI; the names a build tool
// gives the file, one to a line; and what each Python class's family sets, `class attribute` one to a line
// (servers_emit.c)
extern CliKeyed g_cli_servers_published;
extern CliKeyed g_cli_servers_inspected;
extern CliKeyed g_cli_servers_checked;
extern CliKeyed g_cli_servers_input;
extern CliKeyed g_cli_servers_supplied;

// the most bytes of a job of the servers: a file's text and what is reported of it
#define CLI_SERVERS_JOB_CAPACITY (CLI_SERVERS_LARGEST + (64ull << 10u))

// A job of the servers done on one of their threads, from `size` bytes of it, its own copy of what it was handed
typedef void (*CliServersJob)(const CliServersJobCfg *args);
// `size` bytes of a job handed to the servers' threads in a cell of their size, waiting where every place is taken: 1,
// or 0 where the cell could not be taken; and a cell of `size` bytes of a job, taken by the caller, handed over whole
// and given back once the job has run: 1, or 0 where `job` is NULL (servers_jobs.c)
int cli_servers_later(const CliServersRunCtx *args);
int cli_servers_later_dispatch(const CliServersRunSizeCtx *args);

// the spec of the server that serves `language`: 1, or 0 where none does; the server of `language` for the tree at
// `root`, started where it is not running, referenced for the caller, NULL with why given where none could start or
// none serves it, which is reported as nothing; the server that has `path` open, referenced for the caller, NULL where
// none does; whether `server` is given the tree's slices in place of files, as its spec says; and the protocol's name
// for the language of `path` by its extension (servers_start.c)
int cli_servers_spec_for(const CliServersLanguageCtx *args);
CliLsp *cli_servers_server(const CliServersLanguageCtx *args);
CliLsp *cli_servers_owner_get(const CliServersPathLspCtx *args);
int cli_servers_slicing(const CliServersCtx *args);
// the server of the file at `path` a slice of it is given to, started again where it is given the tree's slices and was
// let go, the file opened in it with its stored slice and the editor's text: the server, referenced for the caller, or
// NULL where none has the file open and none starts; and the server a request about the file at `path` is asked of,
// referenced for the caller: NULL where none has it open or it is given its server as slices, as such a server is
// asked for nothing but the problems it publishes (servers_save.c)
CliLsp *cli_servers_owner_start(const CliServersPathLspCtx *args);
CliLsp *cli_servers_owner_answering(const CliServersPathLspCtx *args);
// an ask of `server` begun and ended, a server given the tree's slices waited on between them; an ask of it noted that
// waits on nothing; and the servers given the tree's slices let go once nothing waits on them, no slice owes its
// diagnostics, the tree's check is not running and each has been quiet for as long as its asks of the last minute
// earn it, watched on a thread of their own started with the first; the language a file given its server as a slice
// opened as stored, for it to be opened again in the server started after it, and let go as it closes (servers_save.c)
void cli_servers_request_begin(const CliServersCtx *args);
void cli_servers_request_end(const CliServersCtx *args);
void cli_servers_request_note(const CliServersCtx *args);
void cli_servers_language_note(const CliServersPathLanguageCtx *args);
void cli_servers_language_forgotten(const CliServersPathLspCtx *args);
void cli_servers_id_get(const CliServersPathLanguageCtx *args);

// the server of `spec` started for the tree at `root`, with the options and the environment it takes, referenced once
// for the servers running: it, or NULL with why given (servers_launch.c)
CliLsp *cli_servers_launch(const CliServersRootCtx *args);
// what a server sends unprompted, its context where it goes: a file's diagnostics, an edit to make, the settings it
// requests, and the hints and tokens to request again (servers_recv.c)
void cli_servers_recv(const CliLspRecvCfg *args);
// a server stopped on a thread of its own, its reference given back, or here where no thread is free
// (servers_running.c)
void cli_servers_stop_later(const CliServersPathLspCtx *args);

// `size` bytes of JSON read into `read_state`'s tree, taken to their size, the tree it contained given back: its root,
// or CLI_JSON_NONE where it is no JSON or the cells cannot meet it; a server's answer read the same way and its message
// given back; a number as a whole number, `otherwise` where the node is none or no number; and the place
// `{"line", "character"}` (servers_shapes.c)
unsigned int cli_servers_read(const CliServersReadStateCtx *args);
unsigned int cli_servers_answer_read(const CliServersReadStateAnswerCtx *args);
// `method` sent to `server` with the params the line `params` holds, its answer handed over in `answer`: 1, or 0 with
// why given, a line that did not fit its params refused with no server reached; and the same with the params of a
// request about the place `line`, `col` of `path`, `added` after the place where it is given
int cli_servers_request(const CliServersErrMsgCtx *args);
int cli_servers_request_at(const CliServersPathCtx *args);
unsigned long long cli_servers_json_integer(const CliServersJsonNodeOtherwiseCtx *args);
CliTextPosition cli_servers_position(const CliServersJsonNodeOtherwiseCtx *args);
// a path written as a JSON string as it stands in `tree` (servers_shapes.c)
void cli_servers_path_write(const CliServersPathErrMsgCtx *args);
// a workspace edit, in either of the shapes the protocol allows, as each file's edits, `[{"path", "edits"}]`, each
// file's first in the file first, a file made, moved or removed not among them; and the cells one occupies while it is
// written (servers_edits.c)
void cli_servers_workspace_edit_get(const CliServersJsonTreeOutCtx *args);
extern CellOwner g_cli_servers_edits_cells;
// every file named `name` in the tree at `root`, outside the folders a build writes and at most six folders down,
// sorted, written as a JSON array into `out`; and the cells a server's start occupies while it starts
// (servers_launch.c)
void cli_servers_projects_in(const CliServersRootOutCtx *args);
extern CellOwner g_cli_servers_launch_cells;
// a hover's contents as Markdown, from any of the shapes the protocol allows, its letters as they are
void cli_servers_markdown_get(const CliServersJsonOutCtx *args);
// a completion item of the server's put among `items`
void cli_servers_item_get(const CliServersJsonOneCtx *args);
// a call's signature written, `null` where the server gave none; a code action written, `{"orior_plugin", "action"}`
// its raw where the server of the plugin `plugin` offers it: 1, or 0 where it has no title; and whether the span
// `from`, `to` shares a place with `range`, a span with no width touching the other counting (servers_offers.c)
CliSignaturePanel *cli_servers_signature_get(const CliServersJsonOutHelpCtx *args);
int cli_servers_overlaps(const CliServersJsonFromCtx *args);
// a diagnostic of the server's written as orior's, `{"from", "to", "severity", "message", "markdown", "source",
// "code", "href"}`; and a location written as `{"path", "line", "col"}`: 1, or 0 where it names no file
void cli_servers_diagnostic_get(const CliServersJsonCtx *args);

// whether the server's capabilities name `name` as on, as neither absent, null nor false; and the spans of the
// editor's text of `path` that contain `from` to `to` and are more than it, the least first, the tree's, read off the
// branch under `key` where one is stored and `key` is not NULL, else off the text the servers were told, written as
// `[[from, to]]`, `[]` where neither holds a text of it or its grammar names none: 1, or 0 with why given
// (servers_hints.c)
int cli_servers_capable(const CliServersServerCtx *args);
// whether the member `inner` of the capability `name` is true
int cli_servers_capable_true(const CliServersServerCtx *args);
// whether the server runs the command `command` itself, as its executeCommandProvider lists it
int cli_servers_capable_command(const CliServersServerCommandCtx *args);
int cli_servers_selection(const CliServersPathErrMsgOutCtx *args);

// the hash of `size` bytes of `text`, FNV-1a over its bytes, to determine whether what was read of a text is still
// what it reads (servers_parse.c)
unsigned long long cli_servers_hash_get(const CliServersReadStateCtx *args);

// a file the editor has open, `size` bytes of `text`, given `server` as the tree's slice for its view, the lines 0 to
// 63 until the page hands one, as the file opened as `id` where `id` is not NULL, else as its next version, the slice's
// map stored by its key, in the tree at `root`, or the tree it was first given in where `root` is NULL: 1, or 0 with
// why given; whether the file at `path` is given its server as a slice; whether any slice given a server owes its
// diagnostics, its version above the last its server published of it; and its slice let go as it closes
// (servers_slice.c)
int cli_servers_slice_send(const CliServersPathSizeRootCtx *args);
int cli_servers_slice(const CliServersPathLspCtx *args);
int cli_servers_slices_pending(void);
void cli_servers_slice_close(const CliServersPathLspCtx *args);
// the line of the slice of the file at `path` that came from its own line `line` into `into`: 1, or 0 where none did,
// a file given whole its own line; and a line of the slice moved to the file and line it came from, `from` the file's
// path, sized for CLI_PATH_MAX_LEN: 1 where it is the file's own, 2 where another file's, 0 where none, a bracket the
// slice closes, a file given whole its own
int cli_servers_slice_in(const CliServersPathLineCtx *args);
int cli_servers_slice_out(const CliServersPathLineFromCtx *args);
// the object `one` of `json`, about the file at `path`, written with the lines of its range moved to the slice's: 1, or
// 0 with nothing written where the slice holds none of them; and the diagnostics `all`, `size` bytes, the server
// published with `params`, `params_size` bytes, about the sliced file at `path`, moved to the file's own lines, those
// on another file's lines left out: their JSON in a cell counted to `owner` the caller gives back, its size into
// `mapped_size`, or NULL where they are of a version given before the slice stored was or the cells cannot meet them
int cli_servers_slice_ranged_in(const CliServersPathJsonCtx *args);
char *cli_servers_slice_diagnostics(const CliServersPathSizeCtx *args);
// A window's slice of a file the check gives its server in slices; what each window is handed to with its slice and
// its first and last lines, 1 to cut the next, 0 to stop; the file at `path` in the tree at `root`, `size` bytes of
// `text`, cut in one walk into the slices of its windows, a view's height each, each handed to `window` as the walk
// reaches it: 1, or 0 with why given where the cells cannot meet them; and a window's slice `cut` stored by the file's
// key with its map and given `server` as the file opened as `id` where `id` is not NULL, else as its next version: 1,
// or 0 with why given (servers_slice.c)
typedef struct CliServersCut CliServersCut;
typedef struct
{
    void *context;
    const CliServersCut *cut;
    unsigned int first;
    unsigned int last;
} CliServersWindowCfg;

typedef int (*CliServersWindow)(const CliServersWindowCfg *args);
int cli_servers_slices_cut(const CliServersPathSizeRootErrMsgCtx *args);
int cli_servers_cut_send(const CliServersErrMsgServerCtx *args);

// a file's key and its URI, each into what is sized for CLI_PATH_MAX_LEN (servers_emit.c)
void cli_servers_key_get(const CliServersPathKeyCtx *args);
void cli_servers_uri_get(const CliServersPathUriCtx *args);
// a file's diagnostics passed on whole: its server's, less its reports of an attribute a class's family sets and of a
// name a build tool gives it, then the inspections', then each checker's; and a checker's findings stored, `[[path,
// [diagnostic]]]`, each file's diagnostics passed on whose findings changed (servers_emit.c)
void cli_servers_emit_file(const CliServersSinkKeyCtx *args);
// every file it names passed on where `always`, changed or the same, as a file the editor opens receives its findings
// (servers_findings.c)
void cli_servers_findings_store(const CliServersSizeCtx *args);
// a path as it stands in `tree`, its slashes forward, or whole where it stands outside it or `tree` is NULL
void cli_servers_tree_path(const CliServersPathErrMsgCtx *args);

// a file's text read into the inspections' tree, or the file taken out of it where `text` is NULL, each file's
// findings that changed passed on, the root's Python and JavaScript read first on a thread of its own where the tree
// contains another root or none; and the same for a file of the root the tree contains (servers_inspect.c)
void cli_servers_inspect(const CliServersPathSizeRootSinkCtx *args);
void cli_servers_inspect_again(const CliServersPathSizeLanguageCtx *args);
// the inspections' tree let go, as another tree opens; and the root the tree was read from into `root`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 where it contains none
void cli_servers_inspect_let_go(void);
int cli_servers_tree_read(const CliServersRootKindCtx *args);
// the diagnostics of each file whose server reports an attribute a class has not passed on whole again, as what a
// class's family sets moved (servers_emit.c)
void cli_servers_emit_reporting_attributes(const CliServersSinkKeyCtx *args);

// a file the editor opens of `language` handed to each plugin server that serves the language: 1 where one took it;
// its new text to each that has it open, and its closing sent to each that has it open (servers_plugins.c)
int cli_servers_plugins_open(const CliServersPathSizeLanguageCtx *args);
void cli_servers_plugins_change(const CliServersPathSizeLanguageCtx *args);
void cli_servers_plugins_close(const CliServersPathLspCtx *args);
// the server of plugin `id`, referenced, or NULL with why given where none runs; the next plugin server from the place
// `at` that has `path` open, referenced, its plugin's id into `id`, sized for CLI_NAME_MAX_LEN, and `at` moved past it,
// or NULL where none is left; and a command of an action of the server of plugin `id` it does not run itself, `size`
// bytes of
// `{command, arguments}`, passed to the plugin as `orior/command` (servers_plugins.c)
CliLsp *cli_servers_plugin_get(const CliServersErrMsgServerCtx *args);
CliLsp *cli_servers_plugin_owner_get(const CliServersPathIdCtx *args);
// the program a plugin's server starts as, found by its toolchain where `server` in `json` names one, into `program`,
// and the folder it was found in into `folder`, each sized for CLI_PATH_MAX_LEN: 1, or 0 with why given; the
// `length` bytes of `text` written into `written` with each `{folder}` in them `folder`, escaped for a JSON string
// where `escaped`; and the arguments of `server` put into `words` the same way (servers_plugins_program.c)
int cli_servers_plugin_program(const CliServersErrMsgJsonCtx *args);
void cli_servers_plugin_folder_append(const CliServersTextCtx *args);
void cli_servers_plugin_args(const CliServersJsonServerCtx *args);
// one file checked with each checker named for its language, on a thread of its own; a file orior_cli reads itself
// checked as its text stands; and every file of the tree orior_cli reads itself checked as the disk stores it
// (servers_checkers.c)
void cli_servers_run_checkers(const CliServersPathSizeRootSinkCtx *args);
void cli_servers_check_own(const CliServersPathSizeRootSinkCtx *args);
void cli_servers_check_tree_own(const CliServersFilesCtx *args);
void cli_servers_check_tree_with_checkers(const CliServersFilesCtx *args);
// whether orior_cli reads the file at `path` itself, as a build file or as JSON by its schema (servers_own.c)
int cli_servers_own_reads(const CliServersPathSizeRootSinkCtx *args);
// a run of the checker `id` over the file of `key` counted: its number, which a later run of the same checker over
// the same file stands in for; and whether the run `run` is still the last (servers_checkers.c)
unsigned long long cli_servers_run_count(const CliServersKeyCtx *args);
int cli_servers_run_last(const CliServersKeyCtx *args);
// whether the tree's check is on (servers_check.c)
int cli_servers_check_on(void);

// the tree's check's queue taken to a file let go, as the editor closes one, the file taken for the check and the
// server referenced for the queue: 1 where the check is on and took it, 0 where the caller closes it; with the taken
// files' lock acquired (servers_check.c)
int cli_servers_check_allocs(const CliServersPathLspCtx *args);
// the lock a server is written to about a file under, by the check or by the editor, which the check's taken files are
// guarded by; whether a file is taken for the check and not for the editor; and a file taken, or let go; each with the
// lock acquired
int cli_servers_check_open(const CliServersUriCtx *args);
void cli_servers_check_open_set(const CliServersUriCtx *args);
// every file the check took let go of it; and each file `server` has open and the check does not take given to it
// again, for it to read against what changed under it; each with the taken files' lock acquired (servers_check_one.c)
void cli_servers_check_open_clear(void);
void cli_servers_reread_open(const CliServersServerCommandCtx *args);
// the tree's files the check was given, by their paths in the tree, each ended by a 0, in a cell of their size counted
// to g_cli_servers_cells, their size into `size`: the cell, or NULL where the check is off or on another tree or the
// cell cannot be taken; the files a change on the disk named recorded among the tree's files, those no longer there let
// go, and where the check's word goes into `emit`: 1, or 0 where the check is off or on another tree; and the files
// `paths`, `count` of them, each with its server, referenced for it, and its language, queued behind those waiting,
// each once, the check notified it is done where none waits: `count` (servers_check.c)
char *cli_servers_check_tree_files(const CliServersRootSizeCtx *args);
int cli_servers_check_files_change(const CliServersFilesCountCtx *args);
unsigned int cli_servers_check_queue(const CliServersCountCtx *args);
// the files the editor opens and the check took let go of the check
void cli_servers_let_go_clear(const CliServersKeyCheckerCtx *args);
// the check's queue made empty and its taken files let go, as another tree opens
void cli_servers_check_let_go(void);
// the files of `files`, `count` of them, queued for the tree's check, or their servers have them check their projects:
// how many wait
unsigned int cli_servers_check_files(const CliServersFilesCtx *args);
// the file at `path` handed by the checker thread `checker` to its server as the disk stores it, or a window of its
// slices at a time where the server is given the tree's slices, the server queried for its diagnostics or, where it
// refuses to be queried, waited on to give them, and let go where the check still takes it; a file the editor has open
// is left to the editor (servers_check_one.c)
void cli_servers_check_one(const CliServersPathSinkCtx *args);
// a file a server's first diagnostics wake the check's wait of, by its key
void cli_servers_wake(const CliServersKeyCheckerCtx *args);
// the diagnostics `all`, `size` bytes, moved to the lines of the file of `key`, taken by the checker that gathers its
// diagnostics a window of its slices at a time, in place of those it took last, and its wait woken: 1, or 0 where no
// checker gathers them, theirs to pass on who received them; and the gathering of the file of `key` ended, as the
// editor takes it
int cli_servers_gather(const CliServersSizeKeyCtx *args);
void cli_servers_gathering_end(const CliServersKeyCheckerCtx *args);
// whether the check lets go of a file whose diagnostics come empty, as it closes it: 1 where they are not passed on
int cli_servers_letting_go(const CliServersKeyCheckerCtx *args);
// what the servers' check reaches: the check set on or off, where it reports how far it has gone, and how far, and
// read back with how many files wait (servers_check.c); a checker made to wait on the file of `key`, whether it was
// woken since, and the file of `key` let go as the check lets go of one (servers_check_one.c); and whether a line
// that includes, imports or uses another names `size` bytes of `stem` as a word (servers_changed.c)
void cli_servers_check_set(const CliServersSinkCtx *args);
void cli_servers_check_count(const CliServersOnCtx *args);
void cli_servers_wait_on(const CliServersKeyCheckerCtx *args);
int cli_servers_was_wake(const CliServersKeyCheckerCtx *args);
void cli_servers_let_go_get(const CliServersKeyCheckerCtx *args);
int cli_servers_line_names(const CliServersSizeLengthCtx *args);
// the attribute and the class a server's report that a class has no such attribute names, as pyright writes it:
// `Cannot access attribute "name" for class "Class*"`, each with its size: 1, or 0 where the report is none
// (servers_emit.c)
int cli_servers_attribute_get(const CliServersLengthCtx *args);

struct CliServersPathCtx
{
    const char *path;
    VerbaLinea *err_msg;
    CliLspAnswer *answer;
    CliLsp *server;
    const char *method;
    unsigned long long wait;
    unsigned int line;
    unsigned int col;
    const char *added;
};

struct CliServersPathErrMsgCtx
{
    const char *path;
    VerbaLinea *err_msg;
    unsigned int line;
    unsigned int col;
    const char *tree;
    VerbaLinea *out;
    const char *name;
};

struct CliServersPathSizeCtx
{
    const char *path;
    unsigned long long size;
    const char *params_text;
    unsigned long long params_size;
    const char *all;
    unsigned long long *mapped_size;
    CellOwner *owner;
};

struct CliServersPathSizeRootCtx
{
    const char *path;
    unsigned long long size;
    const char *root;
    VerbaLinea *err_msg;
    const char *text;
    CliLsp *server;
    const char *id;
};

struct CliServersPathSizeRootErrMsgCtx
{
    const char *path;
    unsigned long long size;
    const char *root;
    VerbaLinea *err_msg;
    const char *text;
    CliServersWindow window;
    void *context;
};

struct CliServersLengthCtx
{
    unsigned long long length;
    const char *message;
    const char **name_pointer;
    unsigned long long *name_size;
    const char **class_name;
    unsigned long long *class_size;
};

struct CliServersErrMsgCtx
{
    VerbaLinea *err_msg;
    CliLspAnswer *answer;
    CliLsp *server;
    const char *method;
    const VerbaLinea *params;
    unsigned long long wait;
};

struct CliServersPathSizeRootSinkCtx
{
    const char *path;
    unsigned long long size;
    const char *root;
    const CliServersSink *sink;
    const char *text;
    int disk;
};

struct CliServersPathErrMsgOutCtx
{
    const char *path;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    CliTextPosition from;
    CliTextPosition to;
    const char *key;
};

struct CliServersSizeCtx
{
    unsigned long long size;
    const CliServersSink *sink;
    const char *id;
    const char *found;
    int always;
};

struct CliServersTextCtx
{
    const char *text;
    VerbaLinea *written;
    unsigned long long length;
    const char *folder;
    int escaped;
};

struct CliServersErrMsgJsonCtx
{
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int server_u32;
    char *program;
    char *folder_out;
};

struct CliServersLanguageCtx
{
    const char *language;
    CliServerSpec *spec;
    const char *root;
    const CliServersSink *sink;
    VerbaLinea *err_msg;
};

struct CliServersJsonCtx
{
    const CliJson *json;
    VerbaLinea *out;
    unsigned int one;
    const char *plugin;
    CliServerDiagnostic *diagnostic;
};

struct CliServersOnCtx
{
    int *on_pointer;
    unsigned int *waiting_u32s;
    unsigned long long *done_pointer;
    unsigned long long *total_pointer;
};

struct CliServersFilesCtx
{
    const char *const *files;
    unsigned int count;
    const char *root;
    const CliServersSink *sink;
};

struct CliServersFilesCountCtx
{
    const char *const *files;
    unsigned int count;
    const char *root;
    CliServersSink *sink_out;
};

struct CliServersPathSinkCtx
{
    const char *path;
    const CliServersSink *sink;
    unsigned int checker;
    const CliServersWaiting *one_servers_waiting_pointer;
};

struct CliServersQueueCtx
{
    CliServersQueue *queue;
    const char *path;
    const CliServersWaiting *waiting;
    int front;
};

struct CliServersCountCtx
{
    unsigned int count;
    const CliServersWaiting *waiting;
    const CliServersSink *sink;
    const char *const *paths;
};

struct CliServersSinkCtx
{
    const CliServersSink *sink;
    int on;
    unsigned long long done;
    unsigned long long total;
};

struct CliServersErrMsgServerCtx
{
    VerbaLinea *err_msg;
    CliLsp *server;
    const char *id;
    const CliServersCut *cut;
};

struct CliServersJsonTreeCtx
{
    const CliJson *json;
    const char *tree;
    VerbaLinea *out;
    unsigned int location;
};

struct CliServersPathLanguageCtx
{
    const char *path;
    const char *language;
    const CliServerSpec *spec_read;
    char *id_out;
};

struct CliServersRootCtx
{
    const char *root;
    const CliServersSink *sink;
    VerbaLinea *err_msg;
    const CliServerSpec *spec_read;
};

struct CliServersSizeLengthCtx
{
    unsigned long long size;
    unsigned long long length;
    const char *line_text;
    const char *stem;
};

struct CliServersJsonNodeCtx
{
    const CliJson *json;
    unsigned int node;
    VerbaLinea *out;
    const char *name;
};

struct CliServersJsonFromCtx
{
    const CliJson *json;
    CliTextPosition from;
    CliTextPosition to;
    unsigned int range;
};

struct CliServersJsonServerCtx
{
    const CliJson *json;
    unsigned int server_u32;
    const char *folder;
    CliWords *words;
};

struct CliServersPathSizeLanguageCtx
{
    const char *path;
    unsigned long long size;
    const char *language;
    const char *text;
};

struct CliServersPathJsonCtx
{
    const char *path;
    const CliJson *json;
    VerbaLinea *out;
    unsigned int one;
};

struct CliServersJsonTreeOutCtx
{
    const CliJson *json;
    const char *tree;
    VerbaLinea *out;
    unsigned int edit;
};

struct CliServersServerCtx
{
    CliLsp *server;
    const char *name;
    const char *inner;
};

struct CliServersQueueFilesCtx
{
    CliServersQueue *queue;
    const char *const *files;
    unsigned int count;
};

struct CliServersSinkKeyCtx
{
    const CliServersSink *sink;
    const char *key;
    const char *uri;
};

struct CliServersSizeKeyCtx
{
    unsigned long long size;
    const char *key;
    const char *all;
};

struct CliServersJsonOneCtx
{
    const CliJson *json;
    unsigned int one;
    CliCompletions *items;
};

struct CliServersRunCtx
{
    CliServersJob run;
    const void *job;
    unsigned long long size;
};

struct CliServersRunSizeCtx
{
    CliServersJob run;
    unsigned long long size;
    char *bytes;
};

struct CliServersJsonOutCtx
{
    const CliJson *json;
    VerbaLinea *out;
    unsigned int contents;
};

struct CliServersSizeCommandCtx
{
    unsigned long long size;
    const char *command;
    const char *id;
};

struct CliServersPathIdCtx
{
    const char *path;
    char *id_out;
    unsigned int *at_pointer;
};

struct CliServersRootOutCtx
{
    const char *root;
    VerbaLinea *out;
    const char *name;
};

struct CliServersReadStateCtx
{
    CliServersReadState *read_state;
    unsigned long long size;
    const char *text;
};

struct CliServersKeyCtx
{
    const char *key;
    const char *id;
    unsigned long long run_u64;
};

struct CliServersJsonOutHelpCtx
{
    const CliJson *json;
    unsigned int help;
    CellOwner *owner;
};

struct CliServersPathLineCtx
{
    const char *path;
    unsigned int line;
    unsigned int *into;
};

struct CliServersPathLineFromCtx
{
    const char *path;
    unsigned int *line_pointer;
    char *from_capacity;
};

struct CliServersJsonNodeOtherwiseCtx
{
    const CliJson *json;
    unsigned int node;
    unsigned long long otherwise;
};

struct CliServersReadStateAnswerCtx
{
    CliServersReadState *read_state;
    CliLspAnswer *answer;
};

struct CliServersServerCommandCtx
{
    CliLsp *server;
    const char *command;
};

struct CliServersUriCtx
{
    const char *uri;
    int open;
};

struct CliServersQueueAtCtx
{
    const CliServersQueue *queue_read;
    unsigned int at;
};

struct CliServersPathLspCtx
{
    const char *path;
    CliLsp *lsp;
};

struct CliServersRootSizeCtx
{
    const char *root;
    unsigned long long *size_pointer;
};

struct CliServersPathKeyCtx
{
    const char *path;
    char *key_out;
};

struct CliServersPathUriCtx
{
    const char *path;
    char *uri_out;
};

struct CliServersKeyCheckerCtx
{
    const char *key;
    unsigned int checker;
};

struct CliServersCtx
{
    const CliLsp *server_read;
};

struct CliServersRootKindCtx
{
    char *root_out;
};

typedef struct
{
    void (*read_state_lock)(const CliServersReadStateAnswerCtx *args);
    unsigned int (*answer_read)(const CliServersReadStateAnswerCtx *args);
} CliServersReadStateAnswerNs;
EMBED_TABLE_LAYOUT(CliServersReadStateAnswerNs, read_state_lock, answer_read);
EMBED_TABLE_STORAGE CliServersReadStateAnswerNs cli_servers_read_state_answer_internal EMBED_UNUSED = {
    .read_state_lock = cli_servers_read_state_lock,
    .answer_read = cli_servers_answer_read,
};

typedef struct
{
    int (*check_work_state_init)(const CliServersQueueFilesCtx *args);
    void (*check_files_append)(const CliServersQueueFilesCtx *args);
} CliServersQueueFilesNs;
EMBED_TABLE_LAYOUT(CliServersQueueFilesNs, check_work_state_init, check_files_append);
EMBED_TABLE_STORAGE CliServersQueueFilesNs cli_servers_queue_files_internal EMBED_UNUSED = {
    .check_work_state_init = cli_servers_check_work_state_init,
    .check_files_append = cli_servers_check_files_append,
};

typedef struct
{
    CliServersQueued *(*check_queued_at)(const CliServersQueueAtCtx *args);
} CliServersQueueAtNs;
EMBED_TABLE_LAYOUT(CliServersQueueAtNs, check_queued_at);
EMBED_TABLE_STORAGE CliServersQueueAtNs cli_servers_queue_at_internal EMBED_UNUSED = {
    .check_queued_at = cli_servers_check_queued_at,
};

typedef struct
{
    int (*check_queue_append)(const CliServersQueueCtx *args);
} CliServersQueueNs;
EMBED_TABLE_LAYOUT(CliServersQueueNs, check_queue_append);
EMBED_TABLE_STORAGE CliServersQueueNs cli_servers_queue_internal EMBED_UNUSED = {
    .check_queue_append = cli_servers_check_queue_append,
};

typedef struct
{
    int (*later)(const CliServersRunCtx *args);
} CliServersRunNs;
EMBED_TABLE_LAYOUT(CliServersRunNs, later);
EMBED_TABLE_STORAGE CliServersRunNs cli_servers_run_internal EMBED_UNUSED = {
    .later = cli_servers_later,
};

typedef struct
{
    int (*later_dispatch)(const CliServersRunSizeCtx *args);
} CliServersRunSizeNs;
EMBED_TABLE_LAYOUT(CliServersRunSizeNs, later_dispatch);
EMBED_TABLE_STORAGE CliServersRunSizeNs cli_servers_run_size_internal EMBED_UNUSED = {
    .later_dispatch = cli_servers_later_dispatch,
};

typedef struct
{
    int (*spec_for)(const CliServersLanguageCtx *args);
    CliLsp *(*server)(const CliServersLanguageCtx *args);
} CliServersLanguageNs;
EMBED_TABLE_LAYOUT(CliServersLanguageNs, spec_for, server);
EMBED_TABLE_STORAGE CliServersLanguageNs cli_servers_language_internal EMBED_UNUSED = {
    .spec_for = cli_servers_spec_for,
    .server = cli_servers_server,
};

typedef struct
{
    CliLsp *(*owner_get)(const CliServersPathLspCtx *args);
    CliLsp *(*owner_start)(const CliServersPathLspCtx *args);
    CliLsp *(*owner_answering)(const CliServersPathLspCtx *args);
    void (*language_forgotten)(const CliServersPathLspCtx *args);
    void (*stop_later)(const CliServersPathLspCtx *args);
    int (*slice)(const CliServersPathLspCtx *args);
    void (*slice_close)(const CliServersPathLspCtx *args);
    void (*plugins_close)(const CliServersPathLspCtx *args);
    int (*check_allocs)(const CliServersPathLspCtx *args);
} CliServersPathLspNs;
EMBED_TABLE_LAYOUT(CliServersPathLspNs, owner_get, owner_start, owner_answering, language_forgotten, stop_later, slice,
                   slice_close, plugins_close, check_allocs);
EMBED_TABLE_STORAGE CliServersPathLspNs cli_servers_path_lsp_internal EMBED_UNUSED = {
    .owner_get = cli_servers_owner_get,
    .owner_start = cli_servers_owner_start,
    .owner_answering = cli_servers_owner_answering,
    .language_forgotten = cli_servers_language_forgotten,
    .stop_later = cli_servers_stop_later,
    .slice = cli_servers_slice,
    .slice_close = cli_servers_slice_close,
    .plugins_close = cli_servers_plugins_close,
    .check_allocs = cli_servers_check_allocs,
};

typedef struct
{
    int (*slicing)(const CliServersCtx *args);
    void (*request_begin)(const CliServersCtx *args);
    void (*request_end)(const CliServersCtx *args);
    void (*request_note)(const CliServersCtx *args);
} CliServersNsTable;
EMBED_TABLE_LAYOUT(CliServersNsTable, slicing, request_begin, request_end, request_note);
EMBED_TABLE_STORAGE CliServersNsTable cli_servers_internal EMBED_UNUSED = {
    .slicing = cli_servers_slicing,
    .request_begin = cli_servers_request_begin,
    .request_end = cli_servers_request_end,
    .request_note = cli_servers_request_note,
};

typedef struct
{
    void (*language_note)(const CliServersPathLanguageCtx *args);
    void (*id_get)(const CliServersPathLanguageCtx *args);
} CliServersPathLanguageNs;
EMBED_TABLE_LAYOUT(CliServersPathLanguageNs, language_note, id_get);
EMBED_TABLE_STORAGE CliServersPathLanguageNs cli_servers_path_language_internal EMBED_UNUSED = {
    .language_note = cli_servers_language_note,
    .id_get = cli_servers_id_get,
};

typedef struct
{
    CliLsp *(*launch)(const CliServersRootCtx *args);
} CliServersRootNsTable;
EMBED_TABLE_LAYOUT(CliServersRootNsTable, launch);
EMBED_TABLE_STORAGE CliServersRootNsTable cli_servers_root_internal EMBED_UNUSED = {
    .launch = cli_servers_launch,
};

typedef struct
{
    void (*recv)(const CliLspRecvCfg *args);
} CliLspRecvNs;
EMBED_TABLE_LAYOUT(CliLspRecvNs, recv);
EMBED_TABLE_STORAGE CliLspRecvNs cli_lsp_recv EMBED_UNUSED = {
    .recv = cli_servers_recv,
};

typedef struct
{
    unsigned int (*read)(const CliServersReadStateCtx *args);
    unsigned long long (*hash_get)(const CliServersReadStateCtx *args);
} CliServersReadStateNs;
EMBED_TABLE_LAYOUT(CliServersReadStateNs, read, hash_get);
EMBED_TABLE_STORAGE CliServersReadStateNs cli_servers_read_state_internal EMBED_UNUSED = {
    .read = cli_servers_read,
    .hash_get = cli_servers_hash_get,
};

typedef struct
{
    int (*request)(const CliServersErrMsgCtx *args);
} CliServersErrMsgNsTable;
EMBED_TABLE_LAYOUT(CliServersErrMsgNsTable, request);
EMBED_TABLE_STORAGE CliServersErrMsgNsTable cli_servers_err_msg_internal EMBED_UNUSED = {
    .request = cli_servers_request,
};

typedef struct
{
    int (*request_at)(const CliServersPathCtx *args);
} CliServersPathNsTable;
EMBED_TABLE_LAYOUT(CliServersPathNsTable, request_at);
EMBED_TABLE_STORAGE CliServersPathNsTable cli_servers_path_internal EMBED_UNUSED = {
    .request_at = cli_servers_request_at,
};

typedef struct
{
    unsigned long long (*integer)(const CliServersJsonNodeOtherwiseCtx *args);
    CliTextPosition (*place)(const CliServersJsonNodeOtherwiseCtx *args);
} CliServersJsonNodeOtherwiseNs;
EMBED_TABLE_LAYOUT(CliServersJsonNodeOtherwiseNs, integer, place);
EMBED_TABLE_STORAGE CliServersJsonNodeOtherwiseNs cli_servers_json_node_otherwise_internal EMBED_UNUSED = {
    .integer = cli_servers_json_integer,
    .place = cli_servers_position,
};

typedef struct
{
    void (*path_write)(const CliServersPathErrMsgCtx *args);
    void (*tree_path)(const CliServersPathErrMsgCtx *args);
} CliServersPathErrMsgNs;
EMBED_TABLE_LAYOUT(CliServersPathErrMsgNs, path_write, tree_path);
EMBED_TABLE_STORAGE CliServersPathErrMsgNs cli_servers_path_err_msg_internal EMBED_UNUSED = {
    .path_write = cli_servers_path_write,
    .tree_path = cli_servers_tree_path,
};

typedef struct
{
    void (*workspace_edit_get)(const CliServersJsonTreeOutCtx *args);
} CliServersJsonTreeOutNs;
EMBED_TABLE_LAYOUT(CliServersJsonTreeOutNs, workspace_edit_get);
EMBED_TABLE_STORAGE CliServersJsonTreeOutNs cli_servers_json_tree_out_internal EMBED_UNUSED = {
    .workspace_edit_get = cli_servers_workspace_edit_get,
};

typedef struct
{
    void (*projects_in)(const CliServersRootOutCtx *args);
} CliServersRootOutNs;
EMBED_TABLE_LAYOUT(CliServersRootOutNs, projects_in);
EMBED_TABLE_STORAGE CliServersRootOutNs cli_servers_root_out_internal EMBED_UNUSED = {
    .projects_in = cli_servers_projects_in,
};

typedef struct
{
    void (*markdown_get)(const CliServersJsonOutCtx *args);
} CliServersJsonOutNs;
EMBED_TABLE_LAYOUT(CliServersJsonOutNs, markdown_get);
EMBED_TABLE_STORAGE CliServersJsonOutNs cli_servers_json_out_internal EMBED_UNUSED = {
    .markdown_get = cli_servers_markdown_get,
};

typedef struct
{
    void (*item_get)(const CliServersJsonOneCtx *args);
} CliServersJsonOneNs;
EMBED_TABLE_LAYOUT(CliServersJsonOneNs, item_get);
EMBED_TABLE_STORAGE CliServersJsonOneNs cli_servers_json_one_internal EMBED_UNUSED = {
    .item_get = cli_servers_item_get,
};

typedef struct
{
    CliSignaturePanel *(*signature_get)(const CliServersJsonOutHelpCtx *args);
} CliServersJsonOutHelpNs;
EMBED_TABLE_LAYOUT(CliServersJsonOutHelpNs, signature_get);
EMBED_TABLE_STORAGE CliServersJsonOutHelpNs cli_servers_json_out_help_internal EMBED_UNUSED = {
    .signature_get = cli_servers_signature_get,
};

typedef struct
{
    void (*diagnostic_get)(const CliServersJsonCtx *args);
} CliServersJsonNs;
EMBED_TABLE_LAYOUT(CliServersJsonNs, diagnostic_get);
EMBED_TABLE_STORAGE CliServersJsonNs cli_servers_json_internal EMBED_UNUSED = {
    .diagnostic_get = cli_servers_diagnostic_get,
};

typedef struct
{
    int (*overlaps)(const CliServersJsonFromCtx *args);
} CliServersJsonFromNs;
EMBED_TABLE_LAYOUT(CliServersJsonFromNs, overlaps);
EMBED_TABLE_STORAGE CliServersJsonFromNs cli_servers_json_from_internal EMBED_UNUSED = {
    .overlaps = cli_servers_overlaps,
};

typedef struct
{
    int (*capable)(const CliServersServerCtx *args);
    int (*capable_true)(const CliServersServerCtx *args);
} CliServersServerNs;
EMBED_TABLE_LAYOUT(CliServersServerNs, capable, capable_true);
EMBED_TABLE_STORAGE CliServersServerNs cli_servers_server_internal EMBED_UNUSED = {
    .capable = cli_servers_capable,
    .capable_true = cli_servers_capable_true,
};

typedef struct
{
    int (*capable_command)(const CliServersServerCommandCtx *args);
    void (*reread_open)(const CliServersServerCommandCtx *args);
} CliServersServerCommandNs;
EMBED_TABLE_LAYOUT(CliServersServerCommandNs, capable_command, reread_open);
EMBED_TABLE_STORAGE CliServersServerCommandNs cli_servers_server_command_internal EMBED_UNUSED = {
    .capable_command = cli_servers_capable_command,
    .reread_open = cli_servers_reread_open,
};

typedef struct
{
    int (*selection)(const CliServersPathErrMsgOutCtx *args);
} CliServersPathErrMsgOutNs;
EMBED_TABLE_LAYOUT(CliServersPathErrMsgOutNs, selection);
EMBED_TABLE_STORAGE CliServersPathErrMsgOutNs cli_servers_path_err_msg_out_internal EMBED_UNUSED = {
    .selection = cli_servers_selection,
};

typedef struct
{
    int (*slice_send)(const CliServersPathSizeRootCtx *args);
} CliServersPathSizeRootNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeRootNs, slice_send);
EMBED_TABLE_STORAGE CliServersPathSizeRootNs cli_servers_path_size_root_internal EMBED_UNUSED = {
    .slice_send = cli_servers_slice_send,
};

typedef struct
{
    int (*slice_in)(const CliServersPathLineCtx *args);
} CliServersPathLineNs;
EMBED_TABLE_LAYOUT(CliServersPathLineNs, slice_in);
EMBED_TABLE_STORAGE CliServersPathLineNs cli_servers_path_line_internal EMBED_UNUSED = {
    .slice_in = cli_servers_slice_in,
};

typedef struct
{
    int (*slice_out)(const CliServersPathLineFromCtx *args);
} CliServersPathLineFromNs;
EMBED_TABLE_LAYOUT(CliServersPathLineFromNs, slice_out);
EMBED_TABLE_STORAGE CliServersPathLineFromNs cli_servers_path_line_from_internal EMBED_UNUSED = {
    .slice_out = cli_servers_slice_out,
};

typedef struct
{
    int (*slice_ranged_in)(const CliServersPathJsonCtx *args);
} CliServersPathJsonNs;
EMBED_TABLE_LAYOUT(CliServersPathJsonNs, slice_ranged_in);
EMBED_TABLE_STORAGE CliServersPathJsonNs cli_servers_path_json_internal EMBED_UNUSED = {
    .slice_ranged_in = cli_servers_slice_ranged_in,
};

typedef struct
{
    char *(*slice_diagnostics)(const CliServersPathSizeCtx *args);
} CliServersPathSizeNsTable;
EMBED_TABLE_LAYOUT(CliServersPathSizeNsTable, slice_diagnostics);
EMBED_TABLE_STORAGE CliServersPathSizeNsTable cli_servers_path_size_internal EMBED_UNUSED = {
    .slice_diagnostics = cli_servers_slice_diagnostics,
};

typedef struct
{
    int (*slices_cut)(const CliServersPathSizeRootErrMsgCtx *args);
} CliServersPathSizeRootErrMsgNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeRootErrMsgNs, slices_cut);
EMBED_TABLE_STORAGE CliServersPathSizeRootErrMsgNs cli_servers_path_size_root_err_msg_internal EMBED_UNUSED = {
    .slices_cut = cli_servers_slices_cut,
};

typedef struct
{
    int (*cut_send)(const CliServersErrMsgServerCtx *args);
    CliLsp *(*plugin_get)(const CliServersErrMsgServerCtx *args);
} CliServersErrMsgServerNs;
EMBED_TABLE_LAYOUT(CliServersErrMsgServerNs, cut_send, plugin_get);
EMBED_TABLE_STORAGE CliServersErrMsgServerNs cli_servers_err_msg_server_internal EMBED_UNUSED = {
    .cut_send = cli_servers_cut_send,
    .plugin_get = cli_servers_plugin_get,
};

typedef struct
{
    void (*key_get)(const CliServersPathKeyCtx *args);
} CliServersPathKeyNs;
EMBED_TABLE_LAYOUT(CliServersPathKeyNs, key_get);
EMBED_TABLE_STORAGE CliServersPathKeyNs cli_servers_path_key_internal EMBED_UNUSED = {
    .key_get = cli_servers_key_get,
};

typedef struct
{
    void (*uri_get)(const CliServersPathUriCtx *args);
} CliServersPathUriNs;
EMBED_TABLE_LAYOUT(CliServersPathUriNs, uri_get);
EMBED_TABLE_STORAGE CliServersPathUriNs cli_servers_path_uri_internal EMBED_UNUSED = {
    .uri_get = cli_servers_uri_get,
};

typedef struct
{
    void (*emit_file)(const CliServersSinkKeyCtx *args);
    void (*emit_reporting_attributes)(const CliServersSinkKeyCtx *args);
} CliServersSinkKeyNs;
EMBED_TABLE_LAYOUT(CliServersSinkKeyNs, emit_file, emit_reporting_attributes);
EMBED_TABLE_STORAGE CliServersSinkKeyNs cli_servers_sink_key_internal EMBED_UNUSED = {
    .emit_file = cli_servers_emit_file,
    .emit_reporting_attributes = cli_servers_emit_reporting_attributes,
};

typedef struct
{
    void (*findings_store)(const CliServersSizeCtx *args);
} CliServersSizeNs;
EMBED_TABLE_LAYOUT(CliServersSizeNs, findings_store);
EMBED_TABLE_STORAGE CliServersSizeNs cli_servers_size_internal EMBED_UNUSED = {
    .findings_store = cli_servers_findings_store,
};

typedef struct
{
    void (*inspect)(const CliServersPathSizeRootSinkCtx *args);
    void (*run_checkers)(const CliServersPathSizeRootSinkCtx *args);
    void (*check_own)(const CliServersPathSizeRootSinkCtx *args);
    int (*own_reads)(const CliServersPathSizeRootSinkCtx *args);
} CliServersPathSizeRootSinkNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeRootSinkNs, inspect, run_checkers, check_own, own_reads);
EMBED_TABLE_STORAGE CliServersPathSizeRootSinkNs cli_servers_path_size_root_sink_internal EMBED_UNUSED = {
    .inspect = cli_servers_inspect,
    .run_checkers = cli_servers_run_checkers,
    .check_own = cli_servers_check_own,
    .own_reads = cli_servers_own_reads,
};

typedef struct
{
    void (*inspect_again)(const CliServersPathSizeLanguageCtx *args);
    int (*plugins_open)(const CliServersPathSizeLanguageCtx *args);
    void (*plugins_change)(const CliServersPathSizeLanguageCtx *args);
} CliServersPathSizeLanguageNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeLanguageNs, inspect_again, plugins_open, plugins_change);
EMBED_TABLE_STORAGE CliServersPathSizeLanguageNs cli_servers_path_size_language_internal EMBED_UNUSED = {
    .inspect_again = cli_servers_inspect_again,
    .plugins_open = cli_servers_plugins_open,
    .plugins_change = cli_servers_plugins_change,
};

typedef struct
{
    int (*tree_read)(const CliServersRootKindCtx *args);
} CliServersRootKindNs;
EMBED_TABLE_LAYOUT(CliServersRootKindNs, tree_read);
EMBED_TABLE_STORAGE CliServersRootKindNs cli_servers_root_kind_internal EMBED_UNUSED = {
    .tree_read = cli_servers_tree_read,
};

typedef struct
{
    CliLsp *(*plugin_owner_get)(const CliServersPathIdCtx *args);
} CliServersPathIdNs;
EMBED_TABLE_LAYOUT(CliServersPathIdNs, plugin_owner_get);
EMBED_TABLE_STORAGE CliServersPathIdNs cli_servers_path_id_internal EMBED_UNUSED = {
    .plugin_owner_get = cli_servers_plugin_owner_get,
};

typedef struct
{
    int (*plugin_program)(const CliServersErrMsgJsonCtx *args);
} CliServersErrMsgJsonNs;
EMBED_TABLE_LAYOUT(CliServersErrMsgJsonNs, plugin_program);
EMBED_TABLE_STORAGE CliServersErrMsgJsonNs cli_servers_err_msg_json_internal EMBED_UNUSED = {
    .plugin_program = cli_servers_plugin_program,
};

typedef struct
{
    void (*plugin_folder_append)(const CliServersTextCtx *args);
} CliServersTextNs;
EMBED_TABLE_LAYOUT(CliServersTextNs, plugin_folder_append);
EMBED_TABLE_STORAGE CliServersTextNs cli_servers_text_internal EMBED_UNUSED = {
    .plugin_folder_append = cli_servers_plugin_folder_append,
};

typedef struct
{
    void (*plugin_args)(const CliServersJsonServerCtx *args);
} CliServersJsonServerNs;
EMBED_TABLE_LAYOUT(CliServersJsonServerNs, plugin_args);
EMBED_TABLE_STORAGE CliServersJsonServerNs cli_servers_json_server_internal EMBED_UNUSED = {
    .plugin_args = cli_servers_plugin_args,
};

typedef struct
{
    void (*check_tree_own)(const CliServersFilesCtx *args);
    void (*check_tree_with_checkers)(const CliServersFilesCtx *args);
    unsigned int (*check_files)(const CliServersFilesCtx *args);
} CliServersFilesNs;
EMBED_TABLE_LAYOUT(CliServersFilesNs, check_tree_own, check_tree_with_checkers, check_files);
EMBED_TABLE_STORAGE CliServersFilesNs cli_servers_files_internal EMBED_UNUSED = {
    .check_tree_own = cli_servers_check_tree_own,
    .check_tree_with_checkers = cli_servers_check_tree_with_checkers,
    .check_files = cli_servers_check_files,
};

typedef struct
{
    unsigned long long (*run_count)(const CliServersKeyCtx *args);
    int (*run_last)(const CliServersKeyCtx *args);
} CliServersKeyNs;
EMBED_TABLE_LAYOUT(CliServersKeyNs, run_count, run_last);
EMBED_TABLE_STORAGE CliServersKeyNs cli_servers_key_internal EMBED_UNUSED = {
    .run_count = cli_servers_run_count,
    .run_last = cli_servers_run_last,
};

typedef struct
{
    int (*check_open)(const CliServersUriCtx *args);
    void (*check_open_set)(const CliServersUriCtx *args);
} CliServersUriNs;
EMBED_TABLE_LAYOUT(CliServersUriNs, check_open, check_open_set);
EMBED_TABLE_STORAGE CliServersUriNs cli_servers_uri_internal EMBED_UNUSED = {
    .check_open = cli_servers_check_open,
    .check_open_set = cli_servers_check_open_set,
};

typedef struct
{
    char *(*check_tree_files)(const CliServersRootSizeCtx *args);
} CliServersRootSizeNs;
EMBED_TABLE_LAYOUT(CliServersRootSizeNs, check_tree_files);
EMBED_TABLE_STORAGE CliServersRootSizeNs cli_servers_root_size_internal EMBED_UNUSED = {
    .check_tree_files = cli_servers_check_tree_files,
};

typedef struct
{
    int (*check_files_change)(const CliServersFilesCountCtx *args);
} CliServersFilesCountNs;
EMBED_TABLE_LAYOUT(CliServersFilesCountNs, check_files_change);
EMBED_TABLE_STORAGE CliServersFilesCountNs cli_servers_files_count_internal EMBED_UNUSED = {
    .check_files_change = cli_servers_check_files_change,
};

typedef struct
{
    unsigned int (*check_queue)(const CliServersCountCtx *args);
} CliServersCountNs;
EMBED_TABLE_LAYOUT(CliServersCountNs, check_queue);
EMBED_TABLE_STORAGE CliServersCountNs cli_servers_count_internal EMBED_UNUSED = {
    .check_queue = cli_servers_check_queue,
};

typedef struct
{
    void (*let_go_clear)(const CliServersKeyCheckerCtx *args);
    void (*wake)(const CliServersKeyCheckerCtx *args);
    void (*gathering_end)(const CliServersKeyCheckerCtx *args);
    int (*letting_go)(const CliServersKeyCheckerCtx *args);
    void (*wait_on)(const CliServersKeyCheckerCtx *args);
    int (*was_wake)(const CliServersKeyCheckerCtx *args);
    void (*let_go_get)(const CliServersKeyCheckerCtx *args);
} CliServersKeyCheckerNs;
EMBED_TABLE_LAYOUT(CliServersKeyCheckerNs, let_go_clear, wake, gathering_end, letting_go, wait_on, was_wake,
                   let_go_get);
EMBED_TABLE_STORAGE CliServersKeyCheckerNs cli_servers_key_checker_internal EMBED_UNUSED = {
    .let_go_clear = cli_servers_let_go_clear,
    .wake = cli_servers_wake,
    .gathering_end = cli_servers_gathering_end,
    .letting_go = cli_servers_letting_go,
    .wait_on = cli_servers_wait_on,
    .was_wake = cli_servers_was_wake,
    .let_go_get = cli_servers_let_go_get,
};

typedef struct
{
    void (*check_one)(const CliServersPathSinkCtx *args);
} CliServersPathSinkNs;
EMBED_TABLE_LAYOUT(CliServersPathSinkNs, check_one);
EMBED_TABLE_STORAGE CliServersPathSinkNs cli_servers_path_sink_internal EMBED_UNUSED = {
    .check_one = cli_servers_check_one,
};

typedef struct
{
    int (*gather)(const CliServersSizeKeyCtx *args);
} CliServersSizeKeyNs;
EMBED_TABLE_LAYOUT(CliServersSizeKeyNs, gather);
EMBED_TABLE_STORAGE CliServersSizeKeyNs cli_servers_size_key_internal EMBED_UNUSED = {
    .gather = cli_servers_gather,
};

typedef struct
{
    void (*check_set)(const CliServersSinkCtx *args);
} CliServersSinkNs;
EMBED_TABLE_LAYOUT(CliServersSinkNs, check_set);
EMBED_TABLE_STORAGE CliServersSinkNs cli_servers_sink_internal EMBED_UNUSED = {
    .check_set = cli_servers_check_set,
};

typedef struct
{
    void (*check_count)(const CliServersOnCtx *args);
} CliServersOnNs;
EMBED_TABLE_LAYOUT(CliServersOnNs, check_count);
EMBED_TABLE_STORAGE CliServersOnNs cli_servers_on_internal EMBED_UNUSED = {
    .check_count = cli_servers_check_count,
};

typedef struct
{
    int (*line_names)(const CliServersSizeLengthCtx *args);
} CliServersSizeLengthNs;
EMBED_TABLE_LAYOUT(CliServersSizeLengthNs, line_names);
EMBED_TABLE_STORAGE CliServersSizeLengthNs cli_servers_size_length_internal EMBED_UNUSED = {
    .line_names = cli_servers_line_names,
};

typedef struct
{
    int (*attribute_get)(const CliServersLengthCtx *args);
} CliServersLengthNs;
EMBED_TABLE_LAYOUT(CliServersLengthNs, attribute_get);
EMBED_TABLE_STORAGE CliServersLengthNs cli_servers_length_internal EMBED_UNUSED = {
    .attribute_get = cli_servers_attribute_get,
};

#endif
