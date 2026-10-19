// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_internal.h: what the servers' sources share: a server's spec as toolchains.json writes it; the servers
// running, those that failed and how often each ended; the texts and parses of the files the editor holds; the tree's
// check, its queue and what it hears; each file's diagnostics as its server last gave them, the inspections' findings,
// the checkers' and what a build tool and a family give; and the shapes the servers' answers are written in
#ifndef SERVERS_INTERNAL_H
#define SERVERS_INTERNAL_H

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

// how long a request is waited on, and a search of the whole tree, in milliseconds; how many files the check holds at
// once; how long it waits for a file's diagnostics and for any after them; how often an ended server is started again,
// at most that many times within that many milliseconds; and the most bytes of a file the inspections and the check
// read
#define CLI_SERVERS_ASKING 5000ull
#define CLI_SERVERS_SEARCHING 60000ull
#define CLI_SERVERS_CHECKING 6u
#define CLI_SERVERS_PATIENCE 30000ull
#define CLI_SERVERS_QUIET 250ull
#define CLI_SERVERS_RESTARTS 3u
#define CLI_SERVERS_RESTARTS_WITHIN 60000ull
#define CLI_SERVERS_LARGEST (2ull << 20u)

// the name the findings of the files orior reads itself are kept by, beside the checkers'
#define CLI_SERVERS_OWN "orior"

// the most bytes of a server's answer, and of the texts of its tree, and the most nodes the tree holds
#define CLI_SERVERS_ANSWER (8ull << 20u)
#define CLI_SERVERS_NODES (1u << 18u)

// the bytes of each piece a server's answer is held in as it comes
#define CLI_SERVERS_PIECE 4096u

// A server's answer read: its lock; the answer as the server wrote it, joined into a cell of its size, and its size;
// the tree it reads to, taken to the text it reads; and the pieces the answer is held in as it comes, over a piece of
// its own. The answer and the tree are given back as the next is read and as the lock is
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
void cli_servers_reading_held(CliServersReading *reading);
void cli_servers_reading_released(CliServersReading *reading);
// `line` begun as the line a server's answer is written to, held in pieces as it comes; and the answer it holds
// joined into the reading's answer: 1, or 0 where it is not whole or the cells cannot meet it, the answer empty
void cli_servers_answer_begun(CliServersReading *reading, CliLine *line);
int cli_servers_answer_joined(CliServersReading *reading, CliLine *line);

// A toolchain's server: its toolchain and its name, its program, the words it starts with, the languages it serves, the
// protocol's name for each extension's language, the line that installs it, the file that marks a project, the
// notification that has it check its projects, and the options it starts with in a tree whose own code is held back and
// in one whose code runs, as JSON, empty for none
typedef struct
{
    char tool[CLI_NAME_LONGEST];
    char name[CLI_NAME_LONGEST];
    char program[CLI_NAME_LONGEST];
    char arg[CLI_SERVERS_ARGS][256];
    unsigned int args;
    char language[CLI_SERVERS_LANGUAGES][CLI_NAME_LONGEST];
    unsigned int languages;
    char id_extension[CLI_SERVERS_LANGUAGES][32];
    char id_name[CLI_SERVERS_LANGUAGES][CLI_NAME_LONGEST];
    unsigned int ids;
    char setup[1024];
    char projects[CLI_NAME_LONGEST];
    char checks[CLI_NAME_LONGEST];
    char held[8192];
    char trusted[8192];
} CliServerSpec;

// A server running: whether its place is taken, the server, and its spec
typedef struct
{
    int used;
    CliLsp *lsp;
    CliServerSpec spec;
} CliServerRunning;

// The servers, under their lock: the tree they serve, those running, the toolchains that failed and why, how often
// each ended, the checkers named, the tree they check, and where what is said goes
typedef struct
{
    CliLock lock;
    char root[CLI_PATH_LONGEST];
    CliServerRunning running[CLI_SERVERS_MOST];
    char failed_tool[CLI_SERVERS_FAILED][CLI_NAME_LONGEST];
    char failed_said[CLI_SERVERS_FAILED][1024];
    unsigned int failed;
    char restart_tool[CLI_SERVERS_FAILED][CLI_NAME_LONGEST];
    CliRestarts restarts[CLI_SERVERS_FAILED];
    unsigned int restarted;
    char checker[CLI_SERVERS_CHECKERS][CLI_NAME_LONGEST];
    unsigned int checkers;
    char checked_root[CLI_PATH_LONGEST];
    CliServersEmit told;
    int has_told;
} CliServersState;

extern CliServersState g_cli_servers;

// One file waiting for the tree's check, besides its path: the server it goes to, held for it, and the protocol's name
// for its language
typedef struct
{
    CliLsp *server;
    char id[CLI_NAME_LONGEST];
} CliServersWaiting;

// the texts of the files the editor holds, by their keys with their paths, and the parses as last read, by their keys
// with their texts' hashes, under the texts' lock (servers_files.c)
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

// the most bytes of a job of the servers: a file's text and what is said of it
#define CLI_SERVERS_JOB_ROOM (CLI_SERVERS_LARGEST + (64ull << 10u))

// A job of the servers done on one of their threads, from `size` bytes of it, its own copy of what it was handed
typedef void (*CliServersJob)(const void *job, unsigned long long size);
// `size` bytes of a job handed to the servers' threads in a cell of their size, waiting where every place is taken: 1,
// or 0 where the cell could not be taken; and a cell of `size` bytes of a job, taken by the caller, handed over whole
// and given back once the job has run: 1, or 0 where `job` is NULL (servers_jobs.c)
int cli_servers_later(CliServersJob run, const void *job, unsigned long long size);
int cli_servers_later_handed(CliServersJob run, char *job, unsigned long long size);

// the spec of the server that serves `language`: 1, or 0 where none does; the server of `language` for the tree at
// `root`, started where it is not running, held for the caller, NULL with why said where none could start or none
// serves it, which is said as nothing; the server that has `path` open, held for the caller, NULL where none does; and
// the protocol's name for the language of `path` by its extension (servers_start.c)
int cli_servers_spec_for(const char *language, CliServerSpec *spec);
CliLsp *cli_servers_server(const char *root, const char *language, const CliServersEmit *emit, CliServerSpec *spec,
                           CliLine *said);
CliLsp *cli_servers_holding(const char *path);
void cli_servers_id_of(const CliServerSpec *spec, const char *path, const char *language, char *id);

// the server of `spec` started for the tree at `root`, with the options and the environment it takes, held once for
// the servers running: it, or NULL with why said (servers_launch.c)
CliLsp *cli_servers_launch(const char *root, const CliServerSpec *spec, const CliServersEmit *emit, CliLine *said);
// what a server says on its own, its context where it goes: a file's diagnostics, an edit to make, the settings it
// asks for, and the hints and tokens to ask for again (servers_heard.c)
void cli_servers_heard(void *context, const char *method, const char *params, unsigned long long size, CliLine *answer);
// a server stopped on a thread of its own, its hold given back, or here where no thread is free (servers_running.c)
void cli_servers_stop_later(CliLsp *lsp);

// `size` bytes of JSON read into `reading`'s tree, taken to their size, the tree it held given back: its root, or
// CLI_JSON_NONE where it is no JSON or the cells cannot meet it; the reading's answer joined from `line` and read the
// same way; a number as a whole
// number, `otherwise` where the node is none or no number; and the place `{"line", "character"}` (servers_shapes.c)
unsigned int cli_servers_read(CliServersReading *reading, const char *text, unsigned long long size);
unsigned int cli_servers_answer_read(CliServersReading *reading, CliLine *line);
// `method` asked of `server` with the params `params` holds in `pieces`, joined into a cell of their size and given
// back once asked, its answer written into `result`: 1, or 0 with why said; and the same with the params of a
// request about the place `line`, `col` of `path`, `added` after the place where it is given
int cli_servers_asked(CliLsp *server, const char *method, CliPieces *pieces, CliLine *params, unsigned long long wait,
                      CliLine *result, CliLine *said);
int cli_servers_asked_at(CliLsp *server, const char *method, const char *path, unsigned int line, unsigned int col,
                         const char *added, unsigned long long wait, CliLine *result, CliLine *said);
unsigned long long cli_servers_whole(const CliJson *json, unsigned int node, unsigned long long otherwise);
CliTextPlace cli_servers_place(const CliJson *json, unsigned int node);
// a workspace edit, in either of the shapes the protocol allows, as each file's edits, `[{"path", "edits"}]`, each
// file's first in the file first, a file made, moved or removed not among them
void cli_servers_workspace_edit_of(const CliJson *json, unsigned int edit, const char *tree, CliLine *out);
// a hover's contents as Markdown, from any of the shapes the protocol allows, its letters as they are
void cli_servers_markdown_of(const CliJson *json, unsigned int contents, CliLine *out);
// a completion item of the server's put among `items`
void cli_servers_item_of(const CliJson *json, unsigned int one, CliCompletions *items);
// a call's signature written, `null` where the server gave none; a code action written: 1, or 0 where it has no title;
// and whether the span `from`, `to` shares a place with `range`, a span with no width touching the other counting
// (servers_offers.c)
void cli_servers_signature_of(const CliJson *json, unsigned int help, CliLine *out);
int cli_servers_action_of(const CliJson *json, unsigned int one, CliLine *out);
int cli_servers_overlaps(CliTextPlace from, CliTextPlace to, const CliJson *json, unsigned int range);
// a diagnostic of the server's written as orior's, `{"from", "to", "severity", "message", "markdown", "source",
// "code", "href"}`; and a location written as `{"path", "line", "col"}`: 1, or 0 where it names no file
void cli_servers_diagnostic_of(const CliJson *json, unsigned int one, CliLine *out);
int cli_servers_found_of(const CliJson *json, unsigned int location, const char *tree, CliLine *out);

// whether the server's capabilities name `name` as on, as neither absent, null nor false; and the spans of `path`
// that hold `from` to `to` and are more than it, the least first, from its server's selection ranges, written as
// `[[from, to]]`, `[]` where it gives none: 1, or 0 with why said (servers_hints.c)
int cli_servers_capable(CliLsp *server, const char *name);
// whether the member `inner` of the capability `name` is true
int cli_servers_capable_true(CliLsp *server, const char *name, const char *inner);
int cli_servers_selection(const char *path, CliTextPlace from, CliTextPlace to, CliLine *out, CliLine *said);

// a file's key and its URI, each into what holds CLI_PATH_LONGEST (servers_emit.c)
void cli_servers_key_of(const char *path, char *key);
void cli_servers_uri_of(const char *path, char *uri);
// a file's diagnostics passed on whole: its server's, less its reports of an attribute a class's family sets and of a
// name a build tool gives it, then the inspections', then each checker's; and a checker's findings kept, `[[path,
// [diagnostic]]]`, each file's diagnostics passed on whose findings changed (servers_emit.c)
void cli_servers_emit_whole(const char *key, const char *uri, const CliServersEmit *emit);
// every file it names passed on where `always`, changed or the same, as a file the editor opens hears its findings
// (servers_kept.c)
void cli_servers_keep_checked(const char *id, const char *found, unsigned long long size, int always,
                              const CliServersEmit *emit);
// a path as it stands in `tree`, its slashes forward, or whole where it stands outside it or `tree` is NULL
void cli_servers_tree_path(const char *tree, const char *path, CliLine *out);

// a file's text read into the inspections' index, or the file taken out of it where `text` is NULL, each file's
// findings that changed passed on, the tree's Python and JavaScript read first on a thread of its own where the index
// holds another tree or none; and the same for a file of the tree the index holds (servers_inspect.c)
void cli_servers_inspect(const char *root, const char *path, const char *text, unsigned long long size,
                         const CliServersEmit *emit);
void cli_servers_inspect_again(const char *path, const char *text, unsigned long long size);
// the inspections' index let go, as another tree opens; and the tree the index was read from into `root`, which holds
// CLI_PATH_LONGEST: 1, or 0 where it holds none
void cli_servers_inspect_let_go(void);
int cli_servers_indexed(char *root);
// the diagnostics of each file whose server reports an attribute a class has not passed on whole again, as what a
// class's family sets moved (servers_emit.c)
void cli_servers_emit_reporting_attributes(const CliServersEmit *emit);

// one file checked with each checker named for its language, on a thread of its own; a file orior reads itself
// checked as its text stands; and every file of the tree orior reads itself checked as the disk holds it
// (servers_checkers.c)
void cli_servers_run_checkers(const char *root, const char *path, const char *text, unsigned long long size, int disk,
                              const CliServersEmit *emit);
void cli_servers_check_own(const char *root, const char *path, const char *text, unsigned long long size,
                           const CliServersEmit *emit);
void cli_servers_check_tree_own(const char *root, const char *const *files, unsigned int count,
                                const CliServersEmit *emit);
void cli_servers_check_tree_with_checkers(const char *root, const CliServersEmit *emit);
// whether orior reads the file at `path` itself, as a build file or as JSON by its schema (servers_own.c)
int cli_servers_own_reads(const char *path);
// a run of the checker `id` over the file of `key` counted: its number, which a later run of the same checker over
// the same file stands in for; and whether the run `run` is still the last (servers_checkers.c)
unsigned long long cli_servers_run_counted(const char *key, const char *id);
int cli_servers_run_last(const char *key, const char *id, unsigned long long run);
// whether the tree's check is on (servers_check.c)
int cli_servers_check_on(void);

// the tree's check's queue taken to a file let go, as the editor closes one, the file held for the check and the
// server held for the queue: 1 where the check is on and took it, 0 where the caller closes it; with the held files'
// lock held (servers_check.c)
int cli_servers_check_takes(const char *path, CliLsp *lsp);
// the lock a server is written to about a file under, by the check or by the editor, which the check's held files are
// kept under; whether a file is held for the check and not for the editor; and a file held, or let go; each with the
// lock held
extern CliLock g_cli_servers_held_lock;
int cli_servers_held(const char *uri);
void cli_servers_hold(const char *uri, int held);
// every file the check held let go of it; and each file `server` has open and the check does not hold given to it
// again, for it to read against what changed under it; each with the held files' lock held (servers_check_one.c)
void cli_servers_held_cleared(void);
void cli_servers_read_again_held(CliLsp *server);
// each of the tree's files the check was given handed to `each`, by its path in the tree: 1, or 0 where the check is
// off or on another tree; the files of a batch changed on the disk kept among the tree's files, those no longer there
// let go, and where the check's word goes into `emit`: 1, or 0 where the check is off or on another tree; and the
// files `paths`, `count` of them, each with its server, held for it, and its language, queued behind those waiting,
// each once, the check told it is done where none waits: `count` (servers_check.c)
int cli_servers_check_tree_files(const char *root, void (*each)(void *context, const char *file), void *context);
int cli_servers_check_files_changed(const char *root, const char *const *files, unsigned int count,
                                    CliServersEmit *emit);
unsigned int cli_servers_check_queued(const char *const *paths, const CliServersWaiting *waiting, unsigned int count,
                                      const CliServersEmit *emit);
// the files the editor opens and the check held let go of the check
void cli_servers_heard_opened(const char *key);
// the check's queue made empty and its held files let go, as another tree opens
void cli_servers_check_let_go(void);
// the files of `files`, `count` of them, queued for the tree's check, or their servers have them check their projects:
// how many wait
unsigned int cli_servers_check_files(const char *root, const char *const *files, unsigned int count,
                                     const CliServersEmit *emit);
// the file at `path` handed by the checker thread `checker` to its server as the disk holds it, the server asked for
// its diagnostics or, where it refuses to be asked, waited on to give them, and let go where the check still holds it;
// a file the editor holds is left to the editor (servers_check_one.c)
void cli_servers_check_one(unsigned int checker, const char *path, const CliServersWaiting *one,
                           const CliServersEmit *emit);
// a file a server's first diagnostics wake the check's wait of, by its key
void cli_servers_woken(const char *key);
// whether the check lets go of a file whose diagnostics come empty, as it closes it: 1 where they are not passed on
int cli_servers_letting_go(const char *key);
// what the servers' check reaches: the check set on or off, where it says how far it has gone, and how far, and
// read back with how many files wait (servers_check.c); a checker made to wait on the file of `key`, whether it was
// woken since, and the file of `key` let go as the check lets go of one (servers_check_one.c); and whether a line
// that includes, imports or uses another names `size` bytes of `stem` as a word (servers_changed.c)
void cli_servers_check_set(int on, const CliServersEmit *emit, unsigned long long done, unsigned long long total);
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
