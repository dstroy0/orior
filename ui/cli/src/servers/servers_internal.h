// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_internal.h: what the servers' sources share: a server's spec as toolchains.json writes it; the servers
// running, those that failed and how often each ended; the texts and parses of the files the editor has open; the
// tree's check, its queue and what it receives; each file's diagnostics as its server last gave them, the checkers'
// findings and what a build tool and a family give; and the shapes the servers' answers are written in
#ifndef SERVERS_INTERNAL_H
#define SERVERS_INTERNAL_H
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../comms/container.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/keyed.h"
#include "../text/text.h"
#include "embed_dispatch_layout.h"
#include "embed_ticket_ring.h"
#include "lsp.h"
#include "servers.h"

typedef struct CliServersPathCtx CliServersPathCtx;
typedef struct CliServersPathErrMsgCtx CliServersPathErrMsgCtx;
typedef struct CliServersPathSizeCtx CliServersPathSizeCtx;
typedef struct CliServersPathSizeTypeCtx CliServersPathSizeTypeCtx;
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
typedef struct CliServersRootTypeCtx CliServersRootTypeCtx;

// the max servers running, toolchains that failed and that ended, words a server starts with, languages and
// extensions' names of one, and checkers named
#define CLI_SERVERS_MAX 16u
#define CLI_SERVERS_FAILED 64u
#define CLI_SERVERS_ARGS 32u
#define CLI_SERVERS_LANGUAGES 64u
#define CLI_SERVERS_CHECKERS 64u

// the times and counts of the servers:
//
// - CLI_SERVERS_REQUEST_TIMEOUT_MS and CLI_SERVERS_SEARCHING: how long the call waits on a request, and on a search of
//   the whole tree, in milliseconds
// - CLI_SERVERS_CHECKING: how many files the check has open at once
// - CLI_SERVERS_PATIENCE and CLI_SERVERS_QUIET: how long it waits for the diagnostics of a file, and for any after them
// - CLI_SERVERS_RESTARTS and CLI_SERVERS_RESTARTS_WITHIN: how often a server that ended starts again, at max that many
//   times within that many milliseconds
// - CLI_SERVERS_LARGEST: the max bytes of a file that the check reads
#define CLI_SERVERS_REQUEST_TIMEOUT_MS 5000ULL
#define CLI_SERVERS_SEARCHING 60000ULL
#define CLI_SERVERS_CHECKING 6u
#define CLI_SERVERS_PATIENCE 30000ULL
#define CLI_SERVERS_QUIET 250ULL
#define CLI_SERVERS_RESTARTS 3u
#define CLI_SERVERS_RESTARTS_WITHIN 60000ULL
#define CLI_SERVERS_LARGEST (2ULL << 20u)

// the name that stores the findings of the files that impero reads itself, beside those of the checkers
#define CLI_SERVERS_OWN "orior"

// the max bytes of a text written from the answer of one server or the findings of one file (CLI_SERVERS_ANSWER). Then
// the max bytes of what a check of the whole tree finds (CLI_SERVERS_FINDINGS), and the max nodes that a tree holds
// (CLI_SERVERS_NODES)
#define CLI_SERVERS_ANSWER (8ULL << 20u)
#define CLI_SERVERS_FINDINGS (32ULL << 20u)
#define CLI_SERVERS_NODES (1u << 18u)

// the max bytes of the params of a request about a file: its path as a URI, a place and a new name
#define CLI_SERVERS_PARAMS (CLI_PATH_MAX_LEN * 4u)

// A read of the answer of a server. `json` is the tree that it reads to, taken for the text that it reads. It goes back
// as the next read starts
typedef struct
{
    CliJson json;
} CliServersReadState;

// the tree of a reading made empty, with no nodes and no text, before the read starts (servers_shapes.c)
void cli_servers_read_state_lock(const CliServersReadStateAnswerCtx *args);

// The server of a toolchain:
//
// - `tool` and `name`: its toolchain and its name
// - `program` and `arg`: its program, and the words that it starts with
// - `language`: the languages that it serves
// - `id_extension` and `id_name`: the name in the protocol of the language of each extension
// - `setup`: the line that installs it
// - `projects`: the file that marks a project
// - `checks`: the notification that makes it check its projects
// - `blocked_args` and `trusted`: the options that it starts with, as JSON, empty for none. The first is for a tree
//   whose own code is off, the second for one whose code runs
// - `slices`: whether the editor gives it an open file as the slice of the tree for the lines in view. That is in place
//   of the whole text of the file
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

// the asks whose times a server that gets the slices of the tree keeps (CLI_SERVERS_REQUESTS). Then the quiet before
// its release, in milliseconds: the least, what each ask of the last minute adds, the max, and the minute
#define CLI_SERVERS_REQUESTS 128u
#define CLI_SERVERS_QUIET_MIN 2000ULL
#define CLI_SERVERS_QUIET_EACH 500ULL
#define CLI_SERVERS_QUIET_MAX 60000ULL
#define CLI_SERVERS_REQUESTS_WITHIN 60000ULL

// A server that runs:
//
// - `used`: whether its place is in use
// - `lsp` and `spec`: the server, and its spec
//
// For a server that gets the slices of the tree:
//
// - `waits`: the asks that wait on it
// - `requested` and `pending_requests`: the times of its last asks in a ring, and how many asks it got
// - `quiet_from`: when it last got an ask or gave an answer
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

// The servers:
//
// - `root`: the tree that they serve
// - `running`: those that run
// - `failed_tool` and `failed_msg`: the toolchains that failed, and why
// - `restart_tool` and `restarts`: how often each ended
// - `checker`: the checkers named
// - `checked_root`: the tree that they check
// - `sink` and `has_sink`: where what they report goes
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

// One file that waits for the check of the tree, apart from its path. `server` is the server that it goes to,
// referenced for it. `id` is the name of its language in the protocol
typedef struct
{
    CliLsp *server;
    char id[CLI_NAME_MAX_LEN];
} CliServersWaiting;

// A file that waits for the check of the tree, the head of one movable cell:
//
// - `server`: its server, referenced for it
// - `generation`: the generation of the check that it went into the queue in
// - `file`: its place among the files of the tree, or CLI_SERVERS_NO_FILE
// - `size`: the bytes of its path
//
// The path with its 0, then the name of its language with its 0, follow the head in the cell
typedef struct
{
    CliLsp *server;
    unsigned long long generation;
    unsigned int file;
    unsigned int size;
} CliServersQueued;
#define CLI_SERVERS_NO_FILE 0xffffffffu

// the max files of the tree that the check lists. Then the slots of its two ticket rings, as powers of two: the files
// that wait, and the files that a change puts first
#define CLI_SERVERS_FILES_MAX (1u << 16u)
#define CLI_SERVERS_CHECK_RING_SHIFT 16u
#define CLI_SERVERS_CHECK_FIRST_SHIFT 6u
// the bytes that the slots of the two rings take, those of the ring that waits first. They are the part of the check in
// the rings block
#define CLI_SERVERS_CHECK_RING_BYTES                                                                                   \
    (((1ULL << CLI_SERVERS_CHECK_RING_SHIFT) + (1ULL << CLI_SERVERS_CHECK_FIRST_SHIFT)) * sizeof(EmbedTicketRingSlot))
// the reports of the problems on their way to the thread of the view at once, a power of two. Then the bytes that the
// slots of their ring take: the part of the problems in the rings block
#define CLI_PROBLEMS_RING_SHIFT 5u
#define CLI_PROBLEMS_RING_SLOTS (1u << CLI_PROBLEMS_RING_SHIFT)
#define CLI_PROBLEMS_RING_BYTES ((unsigned long long)CLI_PROBLEMS_RING_SLOTS * sizeof(EmbedTicketRingSlot))
// the hover answers on their way to the thread of the view at once, a power of two. Then the bytes that the slots of
// their ring take: the part of the hover answers in the rings block
#define CLI_PANELS_HOVER_RING_SHIFT 2u
#define CLI_PANELS_HOVER_RING_SLOTS (1u << CLI_PANELS_HOVER_RING_SHIFT)
#define CLI_PANELS_HOVER_RING_BYTES ((unsigned long long)CLI_PANELS_HOVER_RING_SLOTS * sizeof(EmbedTicketRingSlot))
// the slots of the jobs ring as a power of two, also the max jobs in flight. Then the bytes that they take: the
// part of the jobs in the rings block
#define CLI_SERVERS_JOBS_RING_SHIFT 8u
#define CLI_SERVERS_JOBS_RING_BYTES ((1ULL << CLI_SERVERS_JOBS_RING_SHIFT) * sizeof(EmbedTicketRingSlot))

// A ring of files that wait for the check: the ticket ring and its turnstiles. Then the next ticket that a thread which
// queues takes (`ticket`), and the next that a checker takes (`taken`). A checker takes a ticket and waits for its
// push, thus the checkers pop in ticket order
typedef struct
{
    EmbedTurnstiles turnstiles;
    EmbedTicketRing ring;
    embed_atomic_u32 ticket;
    embed_atomic_u32 taken;
} CliServersCheckRing;

// The check of the tree:
//
// - `on`, `root`, `sink` and `has_sink`: whether it is on, its tree, and where what it reports goes
// - `files_cell`: the files of the tree in one movable cell, sized from them. The cell holds `file_at_max` starts of
//   eight bytes, then a byte for each file, 1 while it waits, then the texts of the paths, each with its 0.
// - `files_max` and `files_used`: the bytes of those texts, and the bytes written
// - `file_count`: the count of the files recorded
// - `waiting` and `first`: the two rings of files that wait, the files of the tree and the files that a change puts
//   first. Their slots are the part of the check in the rings block.
// - `generation`: moves on as the check lets go, thus a checker drops the files that went into the queue before
// - `total` and `done`: how many files it got, and how many it finished
// - `started` and `positioned`: whether its threads started, and whether its rings have their places
typedef struct
{
    int on;
    char root[CLI_PATH_MAX_LEN];
    CliServersSink sink;
    int has_sink;
    const CarcerMovableEntry *files_cell;
    unsigned long long files_max;
    unsigned long long files_used;
    unsigned int file_at_max;
    unsigned int file_count;
    CliServersCheckRing waiting;
    CliServersCheckRing first;
    embed_atomic_u64 generation;
    embed_atomic_u64 total;
    embed_atomic_u64 done;
    int started;
    int positioned;
} CliServersQueue;

// the places in the files cell of the check:
//
// - CLI_SERVERS_FILE_AT: the start of the file `at_`
// - CLI_SERVERS_QUEUED_AT: the byte of the file `at_` that tells whether it waits
// - CLI_SERVERS_FILES_TEXT_AT: the text at `offset_` of the bytes of the paths
#define CLI_SERVERS_FILE_AT(at_) ((unsigned long long)(at_) * sizeof(unsigned long long))
#define CLI_SERVERS_QUEUED_AT(queue_, at_) (CLI_SERVERS_FILE_AT((queue_)->file_at_max) + (unsigned long long)(at_))
#define CLI_SERVERS_FILES_TEXT_AT(queue_, offset_) (CLI_SERVERS_QUEUED_AT((queue_), (queue_)->file_at_max) + (offset_))

// the cell of the check for the files of the tree. Where it is too small, the call gives it back and takes it again at
// their size. The rings get their places once. The return is 1, or 0 where the cells cannot meet it. This call and each
// call below, down to cli_servers_check_files_append, are in servers_check_room.c
int cli_servers_check_work_state_init(const CliServersQueueFilesCtx *args);
// the byte that tells whether a file waits, written 0 for every file
void cli_servers_check_queued_cleared(const CliServersQueueFilesCtx *args);
// a file pushed on the ring that its `front` names, under a ticket of its own. The return is 1, or 0 where the ring is
// full or refuses its cell
int cli_servers_check_queue_append(const CliServersQueueCtx *args);
// the queue that a call reads
typedef struct
{
    const CliServersQueue *queue;
} CliServersQueueCfg;
// the files that wait: pushed, and not yet taken
unsigned int cli_servers_check_waiting(const CliServersQueueCfg *args);
// the files of the tree recorded: those of `files`, and those recorded before
void cli_servers_check_files_append(const CliServersQueueFilesCtx *args);

// the texts of the files that the editor has open, by their keys with their paths (servers_files.c)
extern CliKeyed g_cli_servers_texts;
// the parses as last read, by their keys with the hashes of their texts
extern CliKeyed g_cli_servers_parses;

// the diagnostics of each file as its server last gave them (servers_emit.c). Each is a JSON
// array by its key with its URI
extern CliKeyed g_cli_servers_published;
// the findings of the checkers, by the key of the file. Each is a JSON object of the
// array of each checker by its id, its ids in order, with the URI
extern CliKeyed g_cli_servers_checked;
// the names that a build tool gives the file, by its key, one to a line
extern CliKeyed g_cli_servers_input;
// what the family of each Python class sets, `class attribute` one to a line, by the key of the file
extern CliKeyed g_cli_servers_supplied;

// the max bytes of a job of the servers: the text of a file and the report on it
#define CLI_SERVERS_JOB_CAPACITY (CLI_SERVERS_LARGEST + (64ULL << 10u))

// A job of the servers that runs on one of their threads, from `size` bytes of it: its own copy of what the caller
// gave
typedef void (*CliServersJob)(const CliServersJobCfg *args);
// `size` bytes of a job, copied into a persistent cell of their size. The call pushes them on the ticket ring of the
// jobs, under a ticket of the caller. The call waits where the ring holds a lap of jobs in flight. The return is 1, or
// 0 where the call could not take the cell (servers_jobs.c)
int cli_servers_later(const CliServersRunCtx *args);
// the cell of a job, which the caller positions, pushed under a ticket. The thread that runs it pops it and gives it
// back. The return is 1, or 0 where `cell` is NULL
int cli_servers_later_dispatch(const CliServersRunSizeCtx *args);

// the spec of the server that serves `language`. The return is 1, or 0 where none does (servers_start.c)
int cli_servers_spec_for(const CliServersLanguageCtx *args);
// the server of `language` for the tree at `root`, started where it does not run, and referenced for the caller. The
// return is NULL with the reason where none could start, or where none serves it, which the call reports as nothing
CliLsp *cli_servers_server(const CliServersLanguageCtx *args);
// the server that has `path` open, referenced for the caller. The return is NULL where none does
CliLsp *cli_servers_owner_get(const CliServersPathLspCtx *args);
// whether `server` gets the slices of the tree in place of files, as its spec says
int cli_servers_slicing(const CliServersCtx *args);
// the server that gets a slice of the file at `path`, referenced for the caller (servers_keep.c). Where it gets the
// slices of the tree and the watcher let it go, it starts again. The file opens in it with its stored slice and the
// text of the editor. The return is NULL where none has the file open and none starts
CliLsp *cli_servers_owner_start(const CliServersPathLspCtx *args);
// the server to ask a request about the file at `path`, referenced for the caller. The return is NULL where none has it
// open, or where its server gets it as slices. The call asks such a server for no more than the problems it publishes
CliLsp *cli_servers_owner_answering(const CliServersPathLspCtx *args);
// an ask of `server` begun. Between it and its end, a server that gets the slices of the tree waits (servers_keep.c).
//
// A watcher lets those servers go once nothing waits on them, no slice owes its diagnostics, and the check of the tree
// does not run. Each must also be quiet for as long as its asks of the last minute earn it. The watcher runs on a
// thread of its own, which the first ask starts
void cli_servers_request_begin(const CliServersCtx *args);
// the ask ended
void cli_servers_request_end(const CliServersCtx *args);
// an ask of `server` noted that waits on nothing
void cli_servers_request_note(const CliServersCtx *args);
// the language that a file opened as, stored where its server gets it as a slice. Thus it opens again in the server
// that starts after it
void cli_servers_language_note(const CliServersPathLanguageCtx *args);
// that language let go as the file closes
void cli_servers_language_forgotten(const CliServersPathLspCtx *args);
// the name in the protocol of the language of `path`, by its extension (servers_start.c)
void cli_servers_id_get(const CliServersPathLanguageCtx *args);

// the server of `spec`, started for the tree at `root` with the options and the environment that it takes, and
// referenced once for the servers that run. The return is the server, or NULL with the reason (servers_launch.c)
CliLsp *cli_servers_launch(const CliServersRootCtx *args);
// what a server sends with no request, with its context where it goes (servers_recv.c). That is the diagnostics of a
// file, an edit to make, the settings that it requests, and the hints and tokens to request again
void cli_servers_recv(const CliLspRecvCfg *args);
// The methods that cli_servers_recv takes, CLI_SERVERS_METHODS of them. They are the five that it acts on and the two
// of the capability window.workDoneProgress. The last two are the notifications of the window that the protocol grants
// each server
#define CLI_SERVERS_METHODS 9u
extern const char *const g_cli_servers_methods[CLI_SERVERS_METHODS];
// a server stopped on a thread of its own, with its reference given back, or here where no thread is free
// (servers_running.c)
void cli_servers_stop_later(const CliServersPathLspCtx *args);

// `size` bytes of JSON, read into the tree of `read_state`, taken to their size. The call gives back the tree that it
// held. The return is its root, or CLI_JSON_NONE where it is no JSON or the cells cannot meet it (servers_shapes.c)
unsigned int cli_servers_read(const CliServersReadStateCtx *args);
// the answer of a server, read in the same way, with its message given back
unsigned int cli_servers_answer_read(const CliServersReadStateAnswerCtx *args);
// `method` sent to `server` with the params that the line `params` holds, with its answer passed over in `answer`. The
// return is 1, or 0 with the reason. The call refuses a line that did not fit its params, and reaches no server
int cli_servers_request(const CliServersErrMsgCtx *args);
// the same, with the params of a request about the place `line`, `col` of `path`, and `added` after the place where it
// gets them
int cli_servers_request_at(const CliServersPathCtx *args);
// a number as a whole number, or `otherwise` where the node is none or no number (servers_shapes.c)
unsigned long long cli_servers_json_integer(const CliServersJsonNodeOtherwiseCtx *args);
// the place `{"line", "character"}`
CliTextPosition cli_servers_position(const CliServersJsonNodeOtherwiseCtx *args);
// a path written as a JSON string as it is in `tree` (servers_shapes.c)
void cli_servers_path_write(const CliServersPathErrMsgCtx *args);
// a workspace edit, in either of the shapes that the protocol allows, as the edits of each file,
// `[{"path", "edits"}]`. The first edit of each file in the file comes first. A file made, moved or removed is not
// among them (servers_edits.c)
void cli_servers_workspace_edit_get(const CliServersJsonTreeOutCtx *args);
// every file named `name` in the tree at `root`, outside the folders that a build writes, and at max six folders down.
// The call sorts them, and writes them as a JSON array into `out` (servers_launch.c)
void cli_servers_projects_in(const CliServersRootOutCtx *args);
// the contents of a hover as Markdown, from any of the shapes that the protocol allows, with its characters as they are
void cli_servers_markdown_get(const CliServersJsonOutCtx *args);
// a completion item of the server, put among `items`
void cli_servers_item_get(const CliServersJsonOneCtx *args);
// the signature of a call, written as a panel into `store` (servers_offers.c). The return is 1, or 0 where the server
// gave none with a label
int cli_servers_signature_get(const CliServersJsonOutHelpCtx *args);
// whether the span `from`, `to` shares a place with `range`. A span with no width that touches the other counts
int cli_servers_overlaps(const CliServersJsonFromCtx *args);
// a diagnostic of the server, written as one of orior: `{"from", "to", "severity", "message", "markdown", "source",
// "code", "href"}`
void cli_servers_diagnostic_get(const CliServersJsonCtx *args);

// whether the capabilities of the server name `name` as on: not absent, null or false (servers_hints.c)
int cli_servers_capable(const CliServersServerCtx *args);
// whether the member `inner` of the capability `name` is true
int cli_servers_capable_true(const CliServersServerCtx *args);
// whether the server runs the command `command` itself, as its executeCommandProvider lists it
int cli_servers_capable_command(const CliServersServerCommandCtx *args);
// the spans of the text of `path` in the editor that hold `from` to `to` and are more than it, the least first. They
// come from the tree, off the branch under `key` where the store has one and `key` is not NULL. Else they come off the
// text that the servers got. The call writes them as `[[from, to]]`, or `[]` where neither holds a text of it or its
// grammar names none. The return is 1, or 0 with the reason
int cli_servers_selection(const CliServersPathErrMsgOutCtx *args);

// the hash of `size` bytes of `text`, FNV-1a over its bytes. It tells whether what the call read of a text is still
// what it reads (servers_parse.c)
unsigned long long cli_servers_hash_get(const CliServersReadStateCtx *args);

// a file that the editor has open, `size` bytes of `text`, given to `server` as the slice of the tree for its view
// (servers_slice.c). That is the lines 0 to 63 until the page passes one. The file opens as `id` where `id` is not
// NULL, else as its next version. The call stores the map of the slice by its key. It is in the tree at `root`, or in
// the tree that it first went in where `root` is NULL. The return is 1, or 0 with the reason
int cli_servers_slice_send(const CliServersPathSizeRootCtx *args);
// whether the server of the file at `path` gets it as a slice
int cli_servers_slice(const CliServersPathLspCtx *args);
// whether any slice that a server got owes its diagnostics: its version is above the last that its server published of
// it
int cli_servers_slices_pending(void);
// the slice of a file, let go as it closes
void cli_servers_slice_close(const CliServersPathLspCtx *args);
// the line of the slice of the file at `path` that came from its own line `line`, into `into`. The return is 1, or 0
// where none did. A file that the server got whole has its own line
int cli_servers_slice_in(const CliServersPathLineCtx *args);
// a line of the slice, moved to the file and line that it came from, with the path of the file into `from`, sized for
// CLI_PATH_MAX_LEN. The return is 1 for the own line of the file, 2 for the line of another file, and 0 for none. A
// bracket that the slice closes is none. A file that the server got whole has its own
int cli_servers_slice_out(const CliServersPathLineFromCtx *args);
// the object `one` of `json`, about the file at `path`, written with the lines of its range moved to those of the
// slice. The return is 1, or 0 with nothing written where the slice holds none of them
int cli_servers_slice_ranged_in(const CliServersPathJsonCtx *args);
// the diagnostics `all`, `size` bytes, that the server published with `params`, `params_size` bytes, about the sliced
// file at `path`. The call moves them to the own lines of the file, and leaves out those on the lines of another file.
// The return is their JSON in a cell counted to `owner` that the caller gives back, with its size into `mapped_size`.
// It is NULL where they are of a version before the stored slice, or the cells cannot meet them
char *cli_servers_slice_diagnostics(const CliServersPathSizeCtx *args);
// The slice of a window of a file that the check gives its server in slices (servers_slice.c)
typedef struct CliServersCut CliServersCut;
// what the call passes each window with: its slice, and its first and last lines
typedef struct
{
    void *context;
    const CliServersCut *cut;
    unsigned int first;
    unsigned int last;
} CliServersWindowCfg;

// what each window goes to. The return is 1 to cut the next, 0 to stop
typedef int (*CliServersWindow)(const CliServersWindowCfg *args);
// the file at `path` in the tree at `root`, `size` bytes of `text`, cut in one walk into the slices of its windows, a
// view tall each. The call passes each to `window` as the walk reaches it. The return is 1, or 0 with the reason where
// the cells cannot meet them
int cli_servers_slices_cut(const CliServersPathSizeRootErrMsgCtx *args);
// the slice of a window `cut`, stored by the key of the file with its map, and given to `server`. The file opens as
// `id` where `id` is not NULL, else as its next version. The return is 1, or 0 with the reason
int cli_servers_cut_send(const CliServersErrMsgServerCtx *args);

// The text of a file in the editor goes into a temporary cell of the servers with a 0 after it, and its size into
// `size`. The return is the cell, or NULL where the editor has no text or the pool cannot meet the cell. The call that
// asks gives the cell back (servers_files.c)
char *cli_servers_text_alloc(const CliServersPathSizeTypeCtx *args);
// the key of a file, into a line of CLI_PATH_MAX_LEN (servers_emit.c)
void cli_servers_key_get(const CliServersPathKeyCtx *args);
// the URI of a file, into a line of CLI_PATH_MAX_LEN
void cli_servers_uri_get(const CliServersPathUriCtx *args);
// the diagnostics of a file, passed on whole (servers_emit.c). First come those of its server, less some of its
// reports. Those are the reports of an attribute that the family of a class sets, and of a name that a build tool gives
// the file. Then come those of each checker
void cli_servers_emit_file(const CliServersSinkKeyCtx *args);
// the findings of a checker, stored, `[[path, [diagnostic]]]` (servers_findings.c). The call passes on the diagnostics
// of each file whose findings changed. Where `always`, it passes on every file that they name, changed or the same, as
// a file that the editor opens gets its findings
void cli_servers_findings_store(const CliServersSizeCtx *args);
// a path as it is in `tree`, its slashes forward, or whole where it is outside it or `tree` is NULL
void cli_servers_tree_path(const CliServersPathErrMsgCtx *args);

// the diagnostics of each file whose server reports that a class has no such attribute, passed on whole again
// (servers_emit.c). The call does this as what the family of a class sets moves
void cli_servers_emit_reporting_attributes(const CliServersSinkKeyCtx *args);

// a file of `language` that the editor opens, passed to each plugin server that serves the language. The return is 1
// where one took it (servers_plugins.c)
int cli_servers_plugins_open(const CliServersPathSizeLanguageCtx *args);
// the new text of the file, sent to each that has it open
void cli_servers_plugins_change(const CliServersPathSizeLanguageCtx *args);
// the close of the file, sent to each that has it open
void cli_servers_plugins_close(const CliServersPathLspCtx *args);
// the server of plugin `id`, referenced. The return is NULL with the reason where none runs (servers_plugins.c)
CliLsp *cli_servers_plugin_get(const CliServersErrMsgServerCtx *args);
// the next plugin server from the place `at` that has `path` open, referenced, with the id of its plugin into `id`,
// sized for CLI_NAME_MAX_LEN, and `at` moved past it. The return is NULL where no server remains
CliLsp *cli_servers_plugin_owner_get(const CliServersPathIdCtx *args);
// the program that the server of a plugin starts as, found by its toolchain where `server` in `json` names one, into
// `program` (servers_plugins_program.c). The folder that the call found it in goes into `folder`. Each holds
// CLI_PATH_MAX_LEN bytes. The return is 1, or 0 with the reason
int cli_servers_plugin_program(const CliServersErrMsgJsonCtx *args);
// the `length` bytes of `text`, written into `written` with `folder` for each `{folder}` in them, and escaped for a
// JSON string where `escaped`
void cli_servers_plugin_folder_append(const CliServersTextCtx *args);
// the arguments of `server`, put into `words` in the same way
void cli_servers_plugin_args(const CliServersJsonServerCtx *args);
// one file, checked with each checker named for its language, on a thread of its own (servers_checkers.c)
void cli_servers_run_checkers(const CliServersPathSizeRootSinkCtx *args);
// a file that impero reads itself, checked as its text is
void cli_servers_check_own(const CliServersPathSizeRootSinkCtx *args);
// every file of the tree that impero reads itself, checked as the disk stores it
void cli_servers_check_tree_own(const CliServersFilesCtx *args);
// each checker that reads a whole tree, run over the tree as a job of the servers
void cli_servers_check_tree_with_checkers(const CliServersFilesCtx *args);
// whether impero reads the file at `path` itself, as a build file or as JSON by its schema (servers_own.c)
int cli_servers_own_reads(const CliServersPathSizeRootSinkCtx *args);
// a run of the checker `id` over the file of `key`, counted. The return is its number. A later run of the same checker
// over the same file replaces it (servers_checkers.c)
unsigned long long cli_servers_run_count(const CliServersKeyCtx *args);
// whether the run `run` is still the last
int cli_servers_run_last(const CliServersKeyCtx *args);
// whether the check of the tree is on (servers_check.c)
int cli_servers_check_on(void);

// the queue of the check of the tree, taken to a file that the editor closes. The check takes the file, and the queue
// references the server. The return is 1 where the check is on and took it, 0 where the caller closes it
// (servers_check.c)
int cli_servers_check_allocs(const CliServersPathLspCtx *args);
// whether the check takes a file, and the editor does not (servers_check_one.c)
int cli_servers_check_open(const CliServersUriCtx *args);
// a file taken for the check, or let go
void cli_servers_check_open_set(const CliServersUriCtx *args);
// every file that the check took, let go of it (servers_check_one.c)
void cli_servers_check_open_clear(void);
// each file that `server` has open and the check does not take, given to it again, thus it reads it against what
// changed under it
void cli_servers_reread_open(const CliServersServerCommandCtx *args);
// the tree's files the check was given, by their paths in the tree, each ended by a 0, in a cell of their size counted
// to g_cli_servers_owner, their size into `size`: the cell, or NULL where the check is off or on another tree or the
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
// the file at `path` passed by the checker thread `checker` to its server as the disk stores it, or a window of its
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

struct CliServersPathSizeTypeCtx
{
    const char *path;
    unsigned long long *size_pointer;
};

struct CliServersPathSizeCtx
{
    const char *path;
    unsigned long long size;
    const char *params_text;
    unsigned long long params_size;
    const char *all;
    unsigned long long *mapped_size;
    const CellOwner *owner;
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
    unsigned int file;
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
    const CarcerMovableEntry *cell;
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
    const CellOwner *owner;
    CliSignatureStore *store;
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

struct CliServersRootTypeCtx
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
    void (*check_queued_cleared)(const CliServersQueueFilesCtx *args);
} CliServersQueueFilesNs;
EMBED_TABLE_LAYOUT(CliServersQueueFilesNs, check_work_state_init, check_files_append, check_queued_cleared);
EMBED_TABLE_STORAGE CliServersQueueFilesNs cli_servers_queue_files_internal EMBED_UNUSED = {
    .check_work_state_init = cli_servers_check_work_state_init,
    .check_files_append = cli_servers_check_files_append,
    .check_queued_cleared = cli_servers_check_queued_cleared,
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
    int (*signature_get)(const CliServersJsonOutHelpCtx *args);
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
    char *(*text_alloc)(const CliServersPathSizeTypeCtx *args);
} CliServersPathSizeTypeInternalNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeTypeInternalNs, text_alloc);
EMBED_TABLE_STORAGE CliServersPathSizeTypeInternalNs cli_servers_path_size_type_internal EMBED_UNUSED = {
    .text_alloc = cli_servers_text_alloc,
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
    void (*run_checkers)(const CliServersPathSizeRootSinkCtx *args);
    void (*check_own)(const CliServersPathSizeRootSinkCtx *args);
    int (*own_reads)(const CliServersPathSizeRootSinkCtx *args);
} CliServersPathSizeRootSinkNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeRootSinkNs, run_checkers, check_own, own_reads);
EMBED_TABLE_STORAGE CliServersPathSizeRootSinkNs cli_servers_path_size_root_sink_internal EMBED_UNUSED = {
    .run_checkers = cli_servers_run_checkers,
    .check_own = cli_servers_check_own,
    .own_reads = cli_servers_own_reads,
};

typedef struct
{
    int (*plugins_open)(const CliServersPathSizeLanguageCtx *args);
    void (*plugins_change)(const CliServersPathSizeLanguageCtx *args);
} CliServersPathSizeLanguageNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeLanguageNs, plugins_open, plugins_change);
EMBED_TABLE_STORAGE CliServersPathSizeLanguageNs cli_servers_path_size_language_internal EMBED_UNUSED = {
    .plugins_open = cli_servers_plugins_open,
    .plugins_change = cli_servers_plugins_change,
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
