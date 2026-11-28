// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_internal.h: what the modules of the command line share: the query's platform calls it is built on, its own
// platform's calls (platform/windows.c, platform/posix.c), its bounds, and the functions one module reads from
// another
#ifndef CLI_INTERNAL_H
#define CLI_INTERNAL_H
#include "../../../../includes/file_defs/ksg/ksg_writing.h"
#include "../../../../includes/platform/platform_internal.h"
#include "../../../query/src/query_internal.h"
#include "callback_cfg.h"
#include "embed_dispatch_layout.h"
#include "embed_ticket_ring.h"

typedef struct CliWordsCtx CliWordsCtx;
typedef struct CliWordsErrMsgCtx CliWordsErrMsgCtx;
typedef struct CliWordsErrMsgSpawnedCtx CliWordsErrMsgSpawnedCtx;
typedef struct CliErrMsgCtx CliErrMsgCtx;
typedef struct CliPathCtx CliPathCtx;
typedef struct CliPathFromCtx CliPathFromCtx;
typedef struct CliPathErrMsgCtx CliPathErrMsgCtx;
typedef struct CliWordsErrMsgSettingsCtx CliWordsErrMsgSettingsCtx;
typedef struct CliCapacityCountCtx CliCapacityCountCtx;
typedef struct CliPathBytesCtx CliPathBytesCtx;
typedef struct CliRootCtx CliRootCtx;
typedef struct CliServiceCtx CliServiceCtx;
typedef struct CliErrMsgServiceCtx CliErrMsgServiceCtx;
typedef struct CliPathCountSizeCtx CliPathCountSizeCtx;
typedef struct CliSizeCtx CliSizeCtx;
typedef struct CliBytesCtx CliBytesCtx;
typedef struct CliCountCtx CliCountCtx;
typedef struct CliEachCtx CliEachCtx;
typedef struct CliPathCapacityCtx CliPathCapacityCtx;
typedef struct CliPathCountCtx CliPathCountCtx;
typedef struct CliPathPutCtx CliPathPutCtx;
typedef struct CliPathSizeCtx CliPathSizeCtx;
typedef struct CliPathErrMsgNameCtx CliPathErrMsgNameCtx;
typedef struct CliCapacityPidCtx CliCapacityPidCtx;
typedef struct CliErrMsgFromCtx CliErrMsgFromCtx;
typedef struct CliOutCtx CliOutCtx;
typedef struct CliErrMsgNameCtx CliErrMsgNameCtx;
typedef struct CliOutCapacityCtx CliOutCapacityCtx;
typedef struct CliPathRootCtx CliPathRootCtx;
typedef struct CliPathJoinCtx CliPathJoinCtx;
typedef struct CliErrMsgPortCtx CliErrMsgPortCtx;
typedef struct CliSizeBytesCtx CliSizeBytesCtx;
typedef struct CliCapacityCtx CliCapacityCtx;
typedef struct CliBytesSizeCtx CliBytesSizeCtx;
typedef struct CliCapacityPipeHandleCtx CliCapacityPipeHandleCtx;
typedef struct CliSpawnLinesCtx CliSpawnLinesCtx;
typedef struct CliColsCtx CliColsCtx;
typedef struct CliPipeHandleCtx CliPipeHandleCtx;
typedef struct CliPathErrMsgFullCtx CliPathErrMsgFullCtx;
typedef struct CliWakeCtx CliWakeCtx;
typedef struct CliRootContextCtx CliRootContextCtx;
typedef struct CliSizeTextCtx CliSizeTextCtx;
typedef struct CliClipboardUnitsCtx CliClipboardUnitsCtx;
typedef struct CliIdsCtx CliIdsCtx;
typedef struct CliPathFileCtx CliPathFileCtx;
typedef struct CliFileCtx CliFileCtx;
typedef struct CliOwnerCtx CliOwnerCtx;
typedef struct CliFileStageCtx CliFileStageCtx;
typedef struct CliLineCtx CliLineCtx;
typedef struct CliPathErrMsgTypeCtx CliPathErrMsgTypeCtx;
typedef struct CliCapacityLogonCtx CliCapacityLogonCtx;
typedef struct CliClockCivilCtx CliClockCivilCtx;
typedef struct CliCapacityFoldersCtx CliCapacityFoldersCtx;
typedef struct CliPathOutTypeCtx CliPathOutTypeCtx;
typedef struct CliPidCtx CliPidCtx;
typedef struct CliContextCtx CliContextCtx;
typedef struct CliCapacityProcessesCtx CliCapacityProcessesCtx;
typedef struct CliListenerCtx CliListenerCtx;
typedef struct CliSockCtx CliSockCtx;
typedef struct CliSpawnedCtx CliSpawnedCtx;
typedef struct CliWordsIdCtx CliWordsIdCtx;
typedef struct CliErrorCtx CliErrorCtx;
typedef struct CliLineLetterCtx CliLineLetterCtx;
typedef struct CliWordsWordsCtx CliWordsWordsCtx;
typedef struct CliCtx CliCtx;
typedef struct CliEventCtx CliEventCtx;
typedef struct CliFolderCtx CliFolderCtx;
typedef struct CliGroupCtx CliGroupCtx;
typedef struct CliIdCtx CliIdCtx;
typedef struct CliFileTemporaryFileCtx CliFileTemporaryFileCtx;
typedef struct CliThreadCtx CliThreadCtx;
typedef struct CliFolderWatchCtx CliFolderWatchCtx;
typedef struct CliTreeCtx CliTreeCtx;

// the longest path and the longest line the command line handles, as the query's
#define CLI_PATH_MAX_LEN QUERY_PATH_MAX_LEN

#define CLI_ERR_MSG_MAX_LEN QUERY_LINE_MAX_LEN

// orior's version
#define CLI_VERSION "1.10.0"

// the max processes the system lists, the max mounts a window lists, and the longest name of one
#define CLI_PROCESSES_MAX 65536u

#define CLI_NAME_MAX_LEN 256u

// A folder or file mounted beside the tree, under its name
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
} CliMount;

// the mounts: records in a movable cell that the process holds. Readers get them as the token of the cell and the count
// of the records
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int count;
} CliMounts;

// A process the system lists: its id and its parent's
typedef struct
{
    unsigned int id;
    unsigned int parent;
} CliProcess;

// the platform calls of the command line. This one puts this process at the head of its container, the job on Windows
int cli_container_init(void);
// a process started outside the container, with its id
int cli_started_outside(const CliWordsIdCtx *args);
// the ids of the processes started outside the container, which the container leaves out with every process under
// them. The return is their count, with the ids in `ids` (programs.c)
unsigned int cli_started_outside_listed(const unsigned int **ids);
// every process in the container but this one, by id
unsigned int cli_container_members(const CliIdsCtx *args);
// every process that the system lists, with its parent
unsigned int cli_processes_list(const CliCapacityProcessesCtx *args);
// every process in the container but this one, ended
void cli_container_members_end(void);
// the id of this process
unsigned int cli_process_own(void);
// the path of the program that this process runs
int cli_program_own(const CliPathCapacityCtx *args);
// the folder this process is in, into `path`
int cli_folder_current(const CliPathCapacityCtx *args);
// 1 where this process's errors go to a terminal, and where its output does (platform/windows.c, platform/posix.c)
int cli_errors_terminal(void);
int cli_output_terminal(void);
// this process's own input and output as pipe handles cli_spawn_read and cli_spawn_write take, 0 where the system
// gives none (platform/windows.c, platform/posix.c)
unsigned long long cli_standard_input(void);
unsigned long long cli_standard_output(void);
// this machine's name, as the system names it, put on `out`: 1, or 0 where the system gives none (platform/windows.c,
// platform/posix.c)
int cli_machine_name(VerbaLinea *out);
// the words that this program got after its name, at max `capacity` of them, into `words`, each UTF-8. On Windows they
// are as the system stores them, wide. Elsewhere they are the `count` words of `supplied`, or as the system saves them
// where `supplied` is NULL. The return is their count
unsigned int cli_arguments(const CliCapacityCountCtx *args);
// the file at `path` made one its owner runs, on POSIX
int cli_file_runnable(const CliPathFileCtx *args);
// whether the file's owner may run it, its rights unchanged; 0 on a system whose files carry no such right
int cli_file_runs(const CliPathFileCtx *args);
// 1 where the file at `path` is there and is a file the system calls a link to elsewhere, never followed
int cli_is_link(const CliPathFileCtx *args);
// the milliseconds a clock that only goes forward reads
unsigned long long cli_clock_milliseconds(void);
// the seconds since 1970 that the machine started at, as this reading makes it, a few either way from one reading to
// the next. The return is 0 where the system does not say (platform/windows_system.c, platform/posix_system.c)
unsigned long long cli_booted_seconds(void);
// the log in of the reader as the system names it, every log in a new name, into `logon`, sized for `capacity`. It is
// empty where the system names none (platform/windows_system.c, platform/posix_system.c)
void cli_logon_by_name(const CliCapacityLogonCtx *args);
// What this system is, as programs and tools name it (platform/windows_system.c, platform/posix_system.c).
// `program_end` ends a program's file name, "" where nothing ends it, and `program_ends` are every ending a name on the
// PATH answers to, `program_end_count` of them. `list_separator` stands between the folders of a
// list such as PATH, and `list_quoted` is 1 where the system takes quotes out of the list's entries. `path_separator`
// is the byte the system writes between a path's parts, and `path_separators`
// every byte it reads there. `case_folded` is 1 where two paths that differ in case alone name one file.
//
// `host_variable` names the variable that names the machine, and `home_variable` the one that names the reader's home.
// `system_name` is the system's own short name, and `rust_processor` the name Rust gives the processor. `go_system`
// and `go_processor` are the names Go gives this system and processor, and `dotnet_runtime` the name .NET gives both.
// `env_bin` is the folder of a Python environment's programs. `opener` opens a file or an address in the reader's own
// program, with `opener_word` before the address where it is not NULL. `on_linux` is 1 on Linux alone.
//
// `font_folders` are the folders the system saves its fonts in, `font_folder_count` of them. `fonts_nested` is 1 where
// a font can be in a folder of its own under them, to a depth of four. `path_in_profiles` is 1 where the reader's PATH
// is in their shell profiles, and 0 where it is in the registry. `zsh_default` is 1 where zsh is the reader's shell
// unless they choose another.
//
// `config_folders` are the folders that can hold the reader's settings, `config_folder_count` of them, the first that
// the system sets first. `file_opener` opens a file or a folder in the reader's own program. `python_name` names
// Python 3's program. `signals` is 1 where a signal
// can end a process. `lldb_watch_by_script` is 1 where lldb-dap ends at a watchpoint's stop, and lldb's script sets
// the watchpoint instead
typedef struct
{
    // the variable that names the folder's base, NULL for a folder that `path` names whole; and the path after the base
    const char *variable;
    const char *path;
} CliSystemFolder;
typedef struct
{
    const char *program_end;
    const char *const *program_ends;
    unsigned int program_end_count;
    char list_separator;
    int list_quoted;
    char path_separator;
    const char *path_separators;
    int case_folded;
    const char *host_variable;
    const char *home_variable;
    const char *system_name;
    const char *rust_processor;
    const char *env_bin;
    const char *opener;
    const char *opener_word;
    const CliSystemFolder *font_folders;
    unsigned int font_folder_count;
    int fonts_nested;
    int path_in_profiles;
    int zsh_default;
    const CliSystemFolder *config_folders;
    unsigned int config_folder_count;
    const char *file_opener;
    const char *python_name;
    int signals;
    int lldb_watch_by_script;
    const char *go_system;
    const char *go_processor;
    const char *dotnet_runtime;
    int on_linux;
} CliSystemFacts;
extern const CliSystemFacts g_cli_system_facts;
// A calendar reading in the machine's own zone: the year, the month and the day of the month counted from 1, the hour
// and the minute
typedef struct
{
    unsigned int year;
    unsigned int month;
    unsigned int day;
    unsigned int hour;
    unsigned int minute;
} CliCivil;
// the calendar reading of `seconds` since 1970 in the own zone of the machine, into `civil_out`. It is the first moment
// of 1970 where the system does not read it (platform/windows_system.c, platform/posix_system.c)
void cli_clock_civil(const CliClockCivilCtx *args);
// the size of the file at `path`, into `size`. The return is 1, or 0 where it is not there (system_calls.c, through
// the platform)
int cli_file_size(const CliPathSizeCtx *args);
// the size of the file at `path` and the milliseconds since 1970 it last changed at: 1, or 0 where it is not there
int cli_file_stamp(const CliPathSizeCtx *args);
// `count` bytes of the file at `path` from its byte `from`, read into `bytes`, with their count into `got`. They are
// fewer where the file ends first. The return is 1, or 0 where the call could not open or read the file
int cli_file_read_at(const CliPathFromCtx *args);
// the bytes past those of a file stored in its cell, zero. They are a word that a walk loads over the last bytes of a
// text, and the terminator
#define CLI_FILE_SLACK 64ULL
// The bytes of a file in one temporary cell of their size and the slack, counted to `owner` (machine/file_cell.c). This
// one reads the file at `path` whole, with its size into `*size`. The return is NULL where the call could not read the
// file, or the cells cannot meet it. A file of no bytes gives bytes of none
unsigned char *cli_file_alloc(const CliPathCtx *args);
// the same, where the bytes of the file read as UTF-8 whole
unsigned char *cli_file_alloc_utf8(const CliPathCtx *args);
// `count` bytes of the file from `from`, with their count into `*size`, fewer where the file ends first
unsigned char *cli_file_piece_alloc(const CliPathCtx *args);
// the bytes of every file under `root`, with the folders of version control and of the build of cargo passed over. It
// is the request that sizes the pool of a tree (machine/file_cell.c)
unsigned long long cli_tree_bytes(const CliPathRootCtx *args);
// The bytes of the top of `path`, where a whole path starts. On Windows the top is a drive, `C:` and the separator
// after it, or one separator. Elsewhere it is one `/`. 0 where `path` is no whole path (platform/windows_files.c,
// platform/posix_files.c)
unsigned long long cli_path_top(const CliPathCtx *args);
// Whether `path` is whole on its own, as Rust's Path::is_absolute has it on this system. On Windows that is a drive and
// a separator after it, or two separators that start a share. Elsewhere it is one `/` (platform/windows_files.c,
// platform/posix_files.c)
int cli_path_absolute(const CliPathCtx *args);
// `path` joined onto the folder `root` as Rust's Path::join joins them on this system, written on `out`. On Windows a
// path with a drive or a share stands alone, and a path from a separator takes the drive of `root`. Elsewhere a path
// from `/` stands alone (platform/windows_files.c, platform/posix_files.c)
void cli_path_onto(const CliPathJoinCtx *args);
// the number of the system for why the last call failed, read before any other call (platform/windows_files.c,
// platform/posix_files.c)
unsigned int cli_system_error(void);
// the reason for that failure, written after `err_msg`, as `": Access is denied. (os error 5)"`
void cli_system_reason(const CliErrorCtx *args);
// `secret` stored for `account` of `service`, such as "database" or "plugin sonarqube", in the keychain of the system,
// on this machine alone. It goes under the service name "orior " and `service` (platform/windows_keychain.c,
// platform/posix_keychain.c)
int cli_keychain_store(const CliErrMsgServiceCtx *args);
// the secret stored for `account` of `service`, into `out`, sized for `capacity`. The return is 1, or 0 where the
// keychain holds none
int cli_keychain_read(const CliServiceCtx *args);
// the secret, taken out of the keychain
int cli_keychain_forget(const CliErrMsgServiceCtx *args);
// the milliseconds since 1970 that the wall clock reads (windows_files.c, posix_files.c)
unsigned long long cli_wall_milliseconds(void);
// one character written in UTF-8 (words.c)
void cli_utf8_append(const CliLineLetterCtx *args);
// the staged file `from`, copied over `to` with the rights that the system requires of the reader. On Windows, the own
// program of orior runs again as `imp write-raised`, raised. On Linux, cp runs through pkexec, and on macOS, cp runs
// with the authorization of the system. The return is 1, or 0 with the reason
int cli_raised_platform(const CliErrMsgFromCtx *args);

// the max words of one command that a program runs with
#define CLI_WORDS_MAX 512u

// the max bytes that the words of one command take
#define CLI_WORDS_TEXT (1u << 16u)

// The words of one command that a program runs with, NULL after the last, and the bytes that they take. `full` is 1
// where a word did not fit
typedef struct
{
    const char *word_text[CLI_WORDS_MAX + 1u];
    unsigned int count;
    char text[CLI_WORDS_TEXT];
    unsigned int used;
    int full;
} CliWords;

// `word` put after the words so far (words.c)
void cli_words_append(const CliWordsWordsCtx *args);

// A wake that threads wait on: a generation word that cli_wake_all moves on. A wait sleeps on the address of the word
// until it moves, or the time of the wait ends. A wait can return early, thus a waiter tests what it waits for in a
// loop
typedef struct
{
    embed_atomic_word generation;
} CliWake;

#define CLI_WAKE_INIT {EMBED_ATOMIC_INIT(0u)}

// A gate one thread holds at a time: its word's held bit and its sleeper bit. A thread that finds the gate held sleeps
// on the word with the turnstile's backoff until the holder opens it and wakes the word
typedef struct
{
    embed_atomic_word state;
} CliGate;

#define CLI_GATE_INIT {EMBED_ATOMIC_INIT(0u)}
#define CLI_GATE_HELD ((embed_atomic_word_value)1u)
#define CLI_GATE_SLEEPER ((embed_atomic_word_value)2u)

// the gate shut by this thread, after the holder before it opened it
EMBED_INLINE void cli_gate_shut(CliGate *gate)
{
    EmbedTurnstileBackoff backoff;
    embed_atomic_word_value was = EMBED_CALL(embed_atomic_word_table.fetch_or, EmbedAtomicWordFetchOrCfg,
                                             .word = &gate->state, .value = CLI_GATE_HELD);
    EMBED_CALL(embed_turnstile_backoff_begin, EmbedTurnstileBackoffBeginCfg, &backoff);
    while ((was & CLI_GATE_HELD) != 0u)
    {
        EMBED_CALL(embed_turnstile_backoff_step, EmbedTurnstileBackoffStepCfg, &gate->state, was, CLI_GATE_SLEEPER,
                   &backoff, 1u);
        was = EMBED_CALL(embed_atomic_word_table.fetch_or, EmbedAtomicWordFetchOrCfg, .word = &gate->state,
                         .value = CLI_GATE_HELD);
    }
    EMBED_ATOMIC_ORDERED();
}

// the gate opened by its holder, and a thread asleep on it woken
EMBED_INLINE void cli_gate_opened(CliGate *gate)
{
    EMBED_ATOMIC_ORDERED();
    const embed_atomic_word_value was = EMBED_CALL(embed_atomic_word_table.fetch_and, EmbedAtomicWordFetchAndCfg,
                                                   .word = &gate->state, .value = ~(CLI_GATE_HELD | CLI_GATE_SLEEPER));
    if ((was & CLI_GATE_SLEEPER) != 0u)
    {
        EMBED_CALL(embed_word_wake, EmbedWordWakeCfg, .word = &gate->state);
    }
}

// the epoch a ticket ring's stamp carries: the clock in steps of about a second, in the stamp's sixteen bits
EMBED_INLINE embed_u16 cli_ticket_epoch(void)
{
    // Explicit cast retains the low sixteen bits of the clock's steps
    return (embed_u16)(embed_clock_ns() >> 30u);
}

// whether the slot of the ticket `next` carries that ticket, with its push landed and no pop yet. Its stamp, with the
// flag bits clear, goes into `stamp_out`. A consumer tests this before it pops the next ticket in order
EMBED_INLINE int cli_ticket_pushed(const EmbedTicketRing *ring, embed_u32 next, embed_word *stamp_out)
{
    const embed_u64 current =
        EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg,
                   .word = &EMBED_CALL(embed_ticket_ring_slot, EmbedTicketRingSlotCfg, ring, (embed_u64)next)->stamp);
    *stamp_out = EMBED_CALL(embed_ticket_fields_of_stamp, EmbedTicketFieldsOfStampCfg, current);
    return (EMBED_CALL(embed_ticket_of_stamp, EmbedTicketOfStampCfg, current) == next) &&
           ((current & EMBED_TICKET_PUSHED_BIT) != 0u) && ((current & EMBED_TICKET_POPPED_BIT) == 0u);
}

// Whether the slot of the ticket `next` is ready for its grant. That is a zero word on the first lap of the ring, else
// the released word of the ticket one lap before. A producer that grants only then keeps every live ticket within one
// lap of the ring. The proofs of the ring need that, and the pop of the consumer in ticket order relies on it
EMBED_INLINE int cli_ticket_room(const EmbedTicketRing *ring, embed_u32 next)
{
    const embed_u64 current =
        EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg,
                   .word = &EMBED_CALL(embed_ticket_ring_slot, EmbedTicketRingSlotCfg, ring, (embed_u64)next)->stamp);
    // Explicit cast narrows the slot count, at max 2^31, to the ticket's width
    const embed_u32 lap = (embed_u32)(ring->mask + 1u);
    return ((current == 0u) && (next < lap)) ||
           (((current & EMBED_TICKET_RELEASED_BIT) != 0u) &&
            (EMBED_CALL(embed_ticket_of_stamp, EmbedTicketOfStampCfg, current) == next - lap));
}

// The priority that a thread or a program starts at (PLATFORM_PRIORITY_WORK and the rest, platform.h). Thus what must
// not wait does not wait. What runs as the guest of the host gives the processor back when the host wants it. The own
// threads of the window, which answer it, run at the level of the process, and this does not set them.
//
// Work is that level, and the default: language servers, git, formatters. Run is above it: the writer of a run, and
// the channels that hand to it. A guest is below it: builds, tests, checkers, clones, installs and scans. Idle runs
// only in time that nothing else wants: caches warmed ahead. The process of a run is the program of the platform, and
// its settings are the settings of the platform

// a process started with `words`, the program first and NULL after the last, in `folder`. Its environment is that of
// this process with `settings` after, `count` of them, and its standard input is nothing. Where `detached`, it has no
// console of its own and a job of its own on Windows, and a process group of its own elsewhere. The return is 1, or 0
// with the reason. These calls go to the programs of the platform (programs.c)
int cli_spawn(const CliWordsErrMsgCtx *args);
// a process started as cli_spawn starts one, at `priority`, one of PLATFORM_PRIORITY_WORK and the rest; cli_spawn
// starts one at work
int cli_spawn_as(const CliWordsErrMsgCtx *args);
// a process started as cli_spawn starts one, with its standard input a pipe that this process feeds through `in`, at
// work
int cli_spawn_feed(const CliWordsErrMsgCtx *args);
// the same, at `priority`
int cli_spawn_piped_as(const CliWordsErrMsgCtx *args);
// `size` bytes written whole to that pipe. The return is 1, or 0 where the process stopped its read
int cli_spawn_write(const CliPipeHandleCtx *args);
// a process started as cli_spawn starts one, in a pseudo-terminal of its own `cols` wide and `rows` tall: the shell of
// a terminal (programs.c). The call reads its output and errors from `out`, and writes the keys typed to `in`. On
// Windows, the pseudo-console goes in `group`. The return is 1, or 0 with the reason
int cli_spawn_terminal(const CliWordsErrMsgSpawnedCtx *args);
// the pseudo-terminal given a new size, which the shell and the programs in it learn of. The return is 1, or 0 where it
// could not be
int cli_spawn_terminal_resize(const CliColsCtx *args);
// the shell hung up on, as a closed terminal ends it
void cli_spawn_terminal_hung_up(const CliColsCtx *args);
// the pseudo-terminal let go. After it, a reader of `out` meets its end, before cli_spawn_close lets the rest go
void cli_spawn_terminal_close(const CliSpawnedCtx *args);
// a process started as cli_spawn starts one, which shares the console of this process: its input, output and errors
// are those of this process. The call waits on it, and puts its exit code into `code`, -1 where a signal ended it. The
// return is 1, or 0 with the reason that it did not start
int cli_spawn_share(const CliWordsErrMsgSettingsCtx *args);
// the input pipe of a started process closed, which the process reads as the end of its input
void cli_spawn_input_close(const CliSpawnedCtx *args);

// What a program run gave:
//
// - `out` and `err`: what it wrote to its output and to its errors. Each is a cell with the size of what came, ended by
//   a 0, and given back with the run
// - `whole`: 0 where the call could not take a cell, and the store does not hold all that came
// - `code`: its exit code, -1 where a signal ended it
// - `stopped`: 1 where it ran past its time, and the call ended it
typedef struct
{
    char *out;
    unsigned long long out_size;
    char *err;
    unsigned long long err_size;
    int whole;
    long long code;
    int stopped;
} CliRunResult;

// the result of one program run, taken from those kept, with a wait while each is in use (ran.c)
CliRunResult *cli_run_result_alloc(void);
// the result of a program run, given back
void cli_run_result_free(const CliWordsCtx *args);
// a program run as cli_spawn starts one, detached, with its errors read on a thread of their own while the call reads
// its output. The call waits on it, and puts what it gave into `run_result`. The return is 1, or 0 with the reason that
// it did not start
int cli_on_run(const CliWordsCtx *args);
// a program run as cli_on_run runs one, `length` bytes of `input` written to its standard input on a thread of their
// own
int cli_run_result_feed(const CliWordsCtx *args);
// a program run as cli_run_result_feed runs one, ended with every process under it where it runs past `milliseconds`
int cli_run_result_within(const CliWordsCtx *args);

// `size` bytes of `data` added to the Windows program at `path` as the RCDATA resource `name`: 1, or 0 with why given,
// off Windows always (platform/windows_files.c, platform/posix_files.c)
int cli_resource_add(const CliPathErrMsgCtx *args);

// A piece of a file written: `size` bytes of `bytes`, or of zeros where `bytes` is NULL
typedef struct
{
    const void *bytes;
    unsigned long long size;
} CliFilePiece;

// the file at `path`, written as `count` pieces in turn. The write replaces any file there whole, or not at all. The
// return is 1, or 0 where the write failed (file_writing.c)
int cli_file_pieces_write(const CliPathCountCtx *args);
// the file at `path`, written in the same way with what `put` puts into the line of the file writer, passed `context`.
// The return is 1, or 0 where `put` returned 0 or the write failed
int cli_file_put_write(const CliPathPutCtx *args);

// the bytes that a write of a file goes in: one view of the Windows cache manager, VACB_MAPPING_GRANULARITY in ntifs.h.
// The write goes at an offset that is a whole number of them, each a whole number of 4 KiB pages
#define CLI_STAGE_BYTES (256u << 10u)

_Static_assert((CLI_STAGE_BYTES % 4096u) == 0u, "CLI_STAGE_BYTES is a whole number of 4 KiB pages");
_Static_assert(((256u << 10u) % CLI_STAGE_BYTES) == 0u, "CLI_STAGE_BYTES divides the cache manager's 256 KiB view");

// A file that the call writes whole, and seals where it is of a type that ksg seals (ksg_writing.h). The call writes it
// beside its place under an own name of this process, makes it durable, and puts it in its place. A write that fails
// leaves the file that it would replace as it was
typedef KsgWriting CliFileWriting;

_Static_assert(CLI_PATH_MAX_LEN <= KSG_PATH_MAX_LEN, "a path of the command line's fits a sealed writer's");

// the file `path` begun beside its place, with its seal where it has one. The return is 1, or 0 where the call could
// not make it (file_writing.c)
int cli_file_begin(const CliPathFileCtx *args);
// `size` bytes at `bytes`, written after what the file begun holds, in the shape of the write-through of a line. The
// return is 1, or 0 where they did not go
int cli_file_write_chunk(void *context, const char *bytes, size_t size);
// the file begun, on the disk, and put in place of `path` where `whole` is 1, or taken away where it is 0. The return
// is 1, or 0 where the call did not put it in place
int cli_file_end(const CliFileCtx *args);

// a line over `stage`, which has room for `CLI_STAGE_BYTES + 1`, and hands each whole stage to the file begun
// (file_writing.c)
VerbaLinea cli_file_line(const CliFileStageCtx *args);
// the last bytes of the line, passed to the file. The call puts the file in place where every byte went, and takes it
// away where one did not. The return is 1, or 0 where it did not put it in place
int cli_file_line_end(const CliLineCtx *args);

// the file or folder at `from`, moved to `to`, with a file there replaced. The return is what the move came to
// (system_calls.c, through the calls of the platform)
int cli_move(const CliErrMsgFromCtx *args);
// `size` bytes of `bytes`, written at the end of the file at `path`. Where `fresh` is 1, the call first makes the file
// again, empty. Where it is 0, the file must be there already. The return is 1, or 0 where the write failed
int cli_file_append(const CliPathBytesCtx *args);

// An own file of this process that the system deletes as the process lets it go or ends. Thus a crash leaves nothing
// behind. It is the handle of the system to it
typedef struct
{
    unsigned long long handle;
} CliTemporaryFile;

// a temporary file, made empty, in the folder that the system passes files in. The return is 1, or 0 where the call
// could not make it (platform/windows_files.c, platform/posix_files.c)
int cli_temporary_file_init(const CliFileTemporaryFileCtx *args);
// `size` bytes of `bytes`, written at its byte `at`. The return is 1, or 0 where the call could not write all of them
int cli_temporary_file_write(const CliBytesCtx *args);
// `size` bytes from its byte `at`, read into `bytes`. The return is 1, or 0 where the call could not read all of them
int cli_temporary_file_read(const CliSizeCtx *args);
// the file let go, which deletes it
void cli_temporary_file_let_go(const CliFileTemporaryFileCtx *args);

// what the picker of the system picks: a file to open, a folder, or a file to save
#define CLI_PICK_FILE 0u
#define CLI_PICK_FOLDER 1u
#define CLI_PICK_SAVE 2u

// the picker of the system, shown for the reader to pick `type`, and waited on. The call puts the path chosen into
// `path`, sized for `capacity`. The return is 1, or 0 where the reader chose none (platform/windows_picker.c,
// platform/posix_picker.c)
int cli_file_pick(const CliPathCapacityCtx *args);

// the text of the clipboard of the system, written into `text` as UTF-8, nothing where it holds none
// (platform/windows_clipboard.c, platform/posix_clipboard.c). The return is 1, or 0 where the read failed
int cli_clipboard_text_read(const CliCtx *args);
// `size` bytes of UTF-8 `text`, put on the clipboard. The return is 1, or 0 where it did not take them
int cli_clipboard_text_write(const CliSizeTextCtx *args);
// the files `paths`, `count` of them, put on the clipboard as the file manager puts them. A paste moves them where
// `cut`, and copies them otherwise. The return is 1, or 0 where it did not take them
int cli_clipboard_files_append(const CliCountCtx *args);
// each file that the clipboard holds, passed to `each`. `cut` is 1 for a cut, 0 for a copy, and -1 where the clipboard
// does not say. The return is their count
unsigned int cli_clipboard_files_read(const CliEachCtx *args);
// the clipboard emptied
void cli_clipboard_clear(void);
// The clipboard text of the system as UTF-16 units, in the own memory of the system (platform/windows_clipboard.c,
// platform/posix_clipboard.c):
//
// - units_take takes memory for `count` units and `breaks` line ends.
// - units_append reads `bytes` of the units of a movable cell at `offset` into that memory, after what it holds.
// - units_break adds the own line end of the system.
// - units_put puts what the memory holds on the clipboard.
// - units_open gives the units that the clipboard holds, and their count into `count_out`, or NULL where it holds none.
//   They stay readable until units_close.
//
// Each of the first four gives 1, or 0 where the system or the cell refuses. Only Windows has these calls. The window
// does not use the clipboard of the system as units on X11 or macOS, and there each call refuses
int cli_clipboard_units_take(const CliClipboardUnitsCtx *args);
int cli_clipboard_units_append(const CarcerMovableCfg *args);
// The `size` bytes of UTF-8 at `text` go after the units of the take, as UTF-16. A byte that starts no character is one
// unit. The return is 1, or 0 where no take is out or the units do not fit (windows_clipboard.c, posix_clipboard.c)
int cli_clipboard_units_append_text(const CliSizeTextCtx *args);
int cli_clipboard_units_break(const CliClipboardUnitsCtx *args);
int cli_clipboard_units_put(const CliClipboardUnitsCtx *args);
const unsigned short *cli_clipboard_units_open(const CliClipboardUnitsCtx *args);
void cli_clipboard_units_close(const CliClipboardUnitsCtx *args);
// a piece of the output or errors of a started process, read. The return is 0 at its end
unsigned long long cli_spawn_read(const CliCapacityPipeHandleCtx *args);
// the bytes that wait for a read from `pipe_handle`, which a read of no more than that many takes with no wait. The
// return is 0 where none wait or the pipe ended
unsigned long long cli_spawn_waiting(const CliCapacityPipeHandleCtx *args);
// what `pipe_handle` writes, read until it ends, line by line into `line`, sized for `capacity` (spawn_lines.c). The
// call passes each line to `each` with `context` and its bytes, with its end left off. It passes a line longer than the
// capacity in pieces of the length of the capacity. It passes what is after the last end as it ends
void cli_spawn_lines_read(const CliSpawnLinesCtx *args);
// the started process, waited on. The return is its exit code, or -1 where a signal ended it
long long cli_spawn_wait(const CliSpawnedCtx *args);
// whether a started process ended, with no wait. The return is 1 with its exit code into `code`, -1 where a signal
// ended it, or 0 where it goes on
int cli_spawn_poll(const CliSpawnedCtx *args);
// the PATH of the system as the registry stores it now, into `out`, sized for `capacity` (platform/windows_registry.c,
// platform/posix_registry.c). The return is 1, or 0 where there is none, or off Windows. The calls below give the same
int cli_registry_machine_path(const CliOutCtx *args);
// an own variable of the reader, read as the registry stores it now, into `out`, sized for `capacity`. The PATH of the
// reader is one of them
int cli_registry_user_value(const CliOutCapacityCtx *args);
// an own variable of the reader, set as text whose %NAME% parts expand
int cli_registry_set_user_value(const CliErrMsgNameCtx *args);
// every open program, told that the environment changed
void cli_registry_announce(void);
// the %NAME% parts of a text, expanded
void cli_registry_expand(const CliOutCtx *args);
// the job of a started process, ended
void cli_spawn_group_end(const CliGroupCtx *args);
// the job and pipes of a started process, let go
void cli_spawn_close(const CliSpawnedCtx *args);
// the process group that `leader` leads, ended, with a request first. On Windows, it is the process `id` and every
// process under it
void cli_spawn_tree_end(const CliIdCtx *args);
// a started process, ended at once, by SIGKILL on POSIX
void cli_spawn_kill(const CliColsCtx *args);
// a started process, interrupted by SIGINT. The return is 1, or 0 where it could not be, as on Windows, where no signal
// interrupts a process
int cli_spawn_interrupt(const CliColsCtx *args);
// a started process that heads a job or a group of its own ended at once, with every process in that job or group
void cli_spawn_group_kill(const CliColsCtx *args);
// the folders of the PATH that this process runs with, each into `folders`, sized for `capacity`. The return is their
// count (programs.c)
unsigned int cli_path_folders(const CliCapacityFoldersCtx *args);
// the PATH that those folders make, `count` of them, into `path`, sized for `size`
void cli_path_join(const CliPathCountSizeCtx *args);

// What a thread runs: the function and its context, kept by its starter for as long as the thread lives. Then the
// priority that it runs at, one of PLATFORM_PRIORITY_WORK and the rest, work where the starter sets none. `started` is
// the record that the platform starts the thread on (platform_internal.h)
typedef struct
{
    void (*run)(void *context);
    void *context;
    unsigned int priority;
    PlatformThread started;
} CliThread;

// each thread that waits on the wake, woken; and a thread started on `thread`, let go to end on its own: 1, or 0 where
// it could not start. An untimed wait has no form that reads the generation itself. That read comes after the test of
// the waiter, and a wake between the two is lost to a sleep that no timer ends
void cli_wake_all(const CliWakeCtx *args);
// the wake's generation, read before a waiter tests its condition; and a sleep until the generation moves off `seen`,
// which no wake landing after that read can be lost to
unsigned int cli_wake_read(const CliWakeCtx *args);
void cli_wake_wait_seen(const CliWakeCtx *args);
// the same wait, given up once `milliseconds` have passed: 1 where it was woken, 0 where the time ran out
int cli_wake_wait_for(const CliWakeCtx *args);
// a sleep until the generation moves off `seen`, given up once `milliseconds` have passed: 1 where it moved, 0 where
// the time ran out
int cli_wake_wait_seen_for(const CliWakeCtx *args);

// what happened to a path the system reports: made or moved there, taken out or moved away, or written; and that the
// system's record of changes ran over, and any file may have changed
#define CLI_CHANGE_ADDED 0u
#define CLI_CHANGE_REMOVED 1u
#define CLI_CHANGE_MODIFIED 2u
#define CLI_CHANGE_LOST 3u

// a change the system reports: its path from the tree's top with forward slashes, empty where changes were lost, and
// what happened to it
typedef void (*CliChangeFn)(const CliEachPathTypeCfg *args);

// the folder `root` and all under it watched by the system, each change passed to `emit` on a thread of the watch's
// own as it is made: 1, or 0 where the system cannot watch it, as off Windows; and the watch stopped, its thread
// waited for (platform/windows_watch.c, platform/posix_watch.c)
int cli_watch_system_start(const CliRootContextCtx *args);
void cli_watch_system_stop(void);
// the folder `folder` and all under it kept open as one of the program's own files from its start to its end, `changed`
// called on the watch's own thread once a change rests: 1, or 0 where every watch is in use or the system refuses;
// and every such watch stopped, each thread waited for (platform/folder_watch.c)
int cli_folder_watch_start(const CliFolderWatchCtx *args);
void cli_folder_watch_stop(void);
int cli_thread_start(const CliThreadCtx *args);
// the calling thread put at `priority`, one of PLATFORM_PRIORITY_WORK and the rest (threads.c, through the platform). A
// thread's id, the yield and the count of processors are the platform's (platform_internal.h, platform.h)
void cli_priority_own(const CliWordsErrMsgCtx *args);

// A connection over TCP: the system's handle of it, 0 for none
typedef struct
{
    unsigned long long handle;
} CliSocket;

// what a read that takes what came returns beside a count
#define CLI_SOCKET_CLOSED 0LL

#define CLI_SOCKET_WAITED (-2LL)

#define CLI_SOCKET_FAILED (-1LL)

// a connection made to `host`:`port`, its delay of small writes off; `size` bytes written whole; `size` bytes read
// whole, 0 where the peer closed or the wait ran out; the wait of a read set, 0 for none; the connection ended both
// ways, its handle retained, so that a thread reading it wakes to its end; and the connection let go. One thread may
// read a connection while another writes it
int cli_socket_connect(const CliErrMsgCtx *args);
int cli_socket_write(const CliBytesSizeCtx *args);
int cli_socket_read(const CliSizeBytesCtx *args);
// what came, at max `capacity` bytes, read into `bytes`: its count, CLI_SOCKET_CLOSED where the peer closed,
// CLI_SOCKET_WAITED where the wait ran out, CLI_SOCKET_FAILED where the read failed
long long cli_socket_some(const CliCapacityCtx *args);
void cli_socket_wait(const CliSockCtx *args);
// the wait of a write set, 0 for none
void cli_socket_write_wait(const CliSockCtx *args);
void cli_socket_end(const CliSockCtx *args);
void cli_socket_close(const CliErrMsgCtx *args);
// a port on the loopback no one uses now into `port`: 1, or 0 with why given (platform/windows.c, platform/posix.c)
int cli_port_free(const CliErrMsgPortCtx *args);
// a connection made to the first address `host`:`port` names that takes it, each given up where it is not made
// within `milliseconds`, its delay of small writes off: 1, or 0 with why given; the same with the microseconds finding
// the addresses took into `resolved`; the microseconds a clock that only goes forward reads; a socket listening on a
// port of the loopback no one used, the port into `port`: 1, or 0 with why given; and the next connection it takes,
// waited for, into `peer`: 1, or 0 where it took none (platform/windows_connect.c, platform/posix_connect.c)
int cli_socket_connected_within(const CliErrMsgCtx *args);
int cli_socket_connected_time(const CliErrMsgCtx *args);
unsigned long long cli_clock_microseconds(void);
int cli_socket_listening(const CliErrMsgPortCtx *args);
int cli_socket_accept(const CliListenerCtx *args);

// `size` bytes of the system's own randomness into `bytes`: 1, or 0 where it gave none; the folder the system uses to
// pass files, into `folder`, sized for CLI_PATH_MAX_LEN, with no separator after it; an event a started process
// inherits, made unsignaled, its handle, 0 where none was made and off Windows; the event signaled: 1, or 0 where it
// could not be, and off Windows; and the event let go (platform/windows_system.c, platform/posix_system.c)
int cli_random_bytes(const CliSizeBytesCtx *args);
// a folder made with every folder above it, readable by the reader alone, and a file written whole, made anew,
// readable by the reader alone: each 1, or 0 where it could not be (platform/windows_system.c,
// platform/posix_system.c)
int cli_private_folder_init(const CliWordsErrMsgCtx *args);
int cli_private_file_write(const CliPathBytesCtx *args);
// each process of the machine passed to `each`: its number, its program's name, and the line it was started with where
// the system reports, else empty (platform/windows_processes.c, platform/posix_processes.c)
void cli_processes_each(const CliContextCtx *args);
// whether the process `pid` still runs, as the system reports: on Windows its exit code is still active, on Linux /proc
// lists it, and elsewhere, with no /proc to query, it is taken to run
int cli_process_running(const CliPidCtx *args);

// What a process holds in memory, in bytes: its working set, the memory only it uses that is in RAM, and its commit,
// the private memory it has reserved whether in RAM or paged out; and when it started, in the system's own ticks, 0
// where the system does not say. On Linux the working set is its private pages in RAM and the commit its anonymous
// pages in RAM and swapped out; on macOS both are its physical footprint, as the system counts it against its limit.
// `processor_us` is the processor time that the process used since its start, in microseconds, user and kernel together
typedef struct
{
    unsigned long long working;
    unsigned long long commit;
    unsigned long long started;
    unsigned long long processor_us;
} CliProcessMemory;

// what the process `pid` holds into `memory`: 1, or 0 where the system does not say; and the name of the program it
// runs, its file's name without its folder or `.exe`, into `name`, sized for `capacity`: 1, or 0 where the system does
// not say (platform/windows_processes.c, platform/posix_processes.c)
int cli_process_memory(const CliPidCtx *args);
int cli_process_by_name(const CliCapacityPidCtx *args);
void cli_temp_folder(const CliFolderCtx *args);
unsigned long long cli_event_inheritable(void);
int cli_event_set(const CliEventCtx *args);
void cli_event_close(const CliEventCtx *args);

// the tree (root.c): 1 where `folder` contains the engine's tree; the tree ORIOR_ROOT names, else the one found walking
// up from the working folder or the program; a folder mounted beside the tree under a name it is given; a mount taken
// away, or all; a path inside the tree or a mount, refused where it climbs out; a path written relative to the tree, a
// whole path retained as it is
int cli_is_tree_root(const CliWordsErrMsgCtx *args);
int cli_tree_at(const CliPathErrMsgFullCtx *args);
int cli_tree_on_find(const CliTreeCtx *args);
int cli_mount(const CliPathErrMsgNameCtx *args);
void cli_unmount(const CliErrMsgNameCtx *args);
void cli_unmounted_all(void);
CliMounts cli_mounts(void);
int cli_inside(const CliRootCtx *args);
void cli_full(const CliRootCtx *args);
void cli_relative(const CliPathRootCtx *args);
// the folder above `path` into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 where `path` is a root or a bare name
// and has none
int cli_path_parent(const CliPathOutTypeCtx *args);

// orior's own folder (home.c): the folder, its plugins, the reader's stylesheet, menus and languages; each made where
// it is not there; and a file or a folder opened as the system opens it
int cli_home_folder(const CliFolderCtx *args);
int cli_home_plugins(const CliFolderCtx *args);
int cli_home_user_css(const CliPathErrMsgTypeCtx *args);
int cli_home_menus(const CliPathErrMsgTypeCtx *args);
int cli_home_languages(const CliFolderCtx *args);
int cli_home_ensure_menus(const CliPathErrMsgTypeCtx *args);
int cli_home_ensure_user_css(const CliPathErrMsgTypeCtx *args);
// the settings file at `path_out` made, an empty object, its folder with it, where it is not there: 1, or 0 with why
// given (home.c)
int cli_settings_ensure(const CliPathErrMsgTypeCtx *args);
int cli_home_reveal(const CliPathErrMsgNameCtx *args);

struct CliWordsCtx
{
    const char *const *words;
    VerbaLinea *err_msg;
    const char *folder;
    const PlatformSetting *settings;
    unsigned int count;
    CliRunResult *run_result;
    const char *input;
    unsigned long long length;
    unsigned long long milliseconds_u64;
};

struct CliWordsErrMsgCtx
{
    const char *const *words;
    VerbaLinea *err_msg;
    PlatformProgram *spawned;
    const char *folder;
    const PlatformSetting *settings;
    unsigned int count;
    int detached;
    unsigned int priority;
    // a handle the child inherits beside its pipe ends, 0 for none. Only Windows reads it
    unsigned long long inherited_handle;
    // Where `confined`, each program that the child runs past its own waits on `child_decided` with `label`
    // (platform.h)
    int confined;
    int (*child_decided)(const char *label, const char *program);
    const char *label;
};

struct CliWordsErrMsgSpawnedCtx
{
    const char *const *words;
    VerbaLinea *err_msg;
    PlatformProgram *spawned;
    const char *folder;
    const PlatformSetting *settings;
    unsigned int count;
    unsigned int cols;
    unsigned int rows;
};

struct CliErrMsgCtx
{
    VerbaLinea *err_msg;
    CliSocket *sock_out;
    const char *host;
    unsigned int port;
    unsigned int milliseconds;
    unsigned long long *resolved;
};

struct CliPathCtx
{
    const char *path;
    unsigned long long *size_pointer;
    unsigned long long from;
    unsigned long long count_u64;
    const CellOwner *owner;
};

struct CliPathFromCtx
{
    const char *path;
    unsigned long long from;
    unsigned char *bytes_bytes;
    unsigned long long count_u64;
    unsigned long long *got;
};

struct CliPathErrMsgCtx
{
    const char *path;
    VerbaLinea *err_msg;
    unsigned long long size;
    const char *name;
    const void *data;
};

struct CliWordsErrMsgSettingsCtx
{
    const char *const *words;
    VerbaLinea *err_msg;
    const PlatformSetting *settings;
    unsigned int count;
    long long *code;
};

struct CliCapacityCountCtx
{
    unsigned int capacity_u32;
    int count_i32;
    char **supplied;
    const char **words_read;
};

struct CliPathBytesCtx
{
    const char *path;
    const void *bytes;
    unsigned long long size;
    int fresh;
};

struct CliRootCtx
{
    const char *root;
    VerbaLinea *err_msg;
    const char *relative;
    char *joined;
};

struct CliServiceCtx
{
    const char *service;
    const char *account;
    char *out;
    unsigned long long capacity;
};

struct CliErrMsgServiceCtx
{
    VerbaLinea *err_msg;
    const char *service;
    const char *account;
    const char *secret;
};

struct CliPathCountSizeCtx
{
    char *path_out;
    unsigned int count;
    unsigned long long size;
    const char (*folders_callback)[CLI_PATH_MAX_LEN];
};

struct CliSizeCtx
{
    unsigned long long size;
    const CliTemporaryFile *file_temporary_file_pointer_other;
    unsigned long long at;
    void *bytes_out;
};

struct CliBytesCtx
{
    const void *bytes;
    unsigned long long size;
    const CliTemporaryFile *file_temporary_file_pointer_other;
    unsigned long long at;
};

struct CliCountCtx
{
    unsigned int count;
    const char *const *paths;
    int cut;
};

struct CliEachCtx
{
    void (*each)(const CliEachPathCfg *args);
    void *context;
    int *cut_pointer;
};

struct CliPathCapacityCtx
{
    char *path_out;
    embed_word capacity_bytes;
    unsigned int type;
};

struct CliPathCountCtx
{
    const char *path;
    unsigned int count;
    const CliFilePiece *pieces;
};

struct CliPathPutCtx
{
    const char *path;
    int (*put)(VerbaLinea *line, void *context);
    void *context;
};

struct CliPathSizeCtx
{
    const char *path;
    unsigned long long *size_pointer;
    unsigned long long *changed;
};

struct CliPathErrMsgNameCtx
{
    const char *path;
    VerbaLinea *err_msg;
    char *name_out;
};

struct CliCapacityPidCtx
{
    embed_word capacity_bytes;
    unsigned int pid;
    char *name_out;
};

struct CliErrMsgFromCtx
{
    VerbaLinea *err_msg;
    const char *from_text;
    const char *to;
};

struct CliOutCtx
{
    char *out;
    unsigned long long capacity;
    const char *text;
};

struct CliErrMsgNameCtx
{
    VerbaLinea *err_msg;
    const char *name;
    const char *value;
};

struct CliOutCapacityCtx
{
    char *out;
    unsigned long long capacity;
    const char *name;
};

struct CliPathJoinCtx
{
    const char *root;
    const char *path;
    VerbaLinea *out;
};

struct CliPathRootCtx
{
    const char *path;
    const char *root;
    char *relative_out;
};

struct CliErrMsgPortCtx
{
    VerbaLinea *err_msg;
    unsigned int *port_pointer;
    CliSocket *listener;
};

struct CliSizeBytesCtx
{
    unsigned long long size;
    void *bytes_out;
    const CliSocket *sock;
};

struct CliCapacityCtx
{
    unsigned long long capacity;
    void *bytes_out;
    const CliSocket *sock;
};

struct CliBytesSizeCtx
{
    const void *bytes;
    unsigned long long size;
    const CliSocket *sock;
};

struct CliCapacityPipeHandleCtx
{
    unsigned long long capacity;
    unsigned long long pipe_handle;
    char *piece;
};

struct CliSpawnLinesCtx
{
    unsigned long long pipe_handle;
    char *line;
    unsigned long long capacity;
    void (*each)(void *context, const char *line, unsigned long long used);
    void *context;
};

struct CliColsCtx
{
    unsigned int cols;
    unsigned int rows;
    const PlatformProgram *spawned_read;
};

struct CliPipeHandleCtx
{
    unsigned long long pipe_handle;
    const void *bytes;
    unsigned long long size;
};

struct CliPathErrMsgFullCtx
{
    const char *path;
    VerbaLinea *err_msg;
    char *full;
};

struct CliWakeCtx
{
    unsigned int seen;
    unsigned long long milliseconds_u64;
    CliWake *wake;
};

struct CliRootContextCtx
{
    const char *root;
    void *context;
    CliChangeFn emit;
};

struct CliSizeTextCtx
{
    unsigned long long size;
    const char *text;
};

struct CliClipboardUnitsCtx
{
    unsigned long long count;
    unsigned long long breaks;
    unsigned long long *count_out;
};

struct CliIdsCtx
{
    unsigned int *ids;
    unsigned int capacity_u32;
};

struct CliPathFileCtx
{
    const char *path;
    CliFileWriting *file;
};

struct CliFileCtx
{
    CliFileWriting *file;
    int whole;
};

struct CliOwnerCtx
{
    const CellOwner *owner;
    const unsigned char *bytes_bytes_other;
};

struct CliFileStageCtx
{
    CliFileWriting *file;
    char *stage;
};

struct CliLineCtx
{
    VerbaLinea *line;
    CliFileWriting *file;
};

struct CliPathErrMsgTypeCtx
{
    char *path_out;
    VerbaLinea *err_msg;
};

struct CliCapacityLogonCtx
{
    embed_word capacity_bytes;
    char *logon;
};

struct CliClockCivilCtx
{
    long long seconds;
    CliCivil *civil_out;
};

struct CliCapacityFoldersCtx
{
    unsigned int capacity_u32;
    char (*folders)[CLI_PATH_MAX_LEN];
};

struct CliPathOutTypeCtx
{
    const char *path;
    char *out;
};

struct CliPidCtx
{
    unsigned int pid;
    CliProcessMemory *memory;
};

struct CliContextCtx
{
    void *context;
    void (*each_callback)(const CliEachPidNameCommandCfg *args);
};

struct CliCapacityProcessesCtx
{
    unsigned int capacity_u32;
    CliProcess *processes;
};

struct CliListenerCtx
{
    const CliSocket *listener_read;
    CliSocket *peer;
};

struct CliSockCtx
{
    const CliSocket *sock;
    unsigned int milliseconds;
};

struct CliSpawnedCtx
{
    PlatformProgram *spawned;
    long long *code;
};

struct CliWordsIdCtx
{
    const char *const *words;
    unsigned int *id;
};

struct CliErrorCtx
{
    unsigned int error;
    VerbaLinea *err_msg;
};

struct CliLineLetterCtx
{
    VerbaLinea *line;
    unsigned int character;
};

struct CliWordsWordsCtx
{
    CliWords *words_words_pointer;
    const char *word_text;
};

struct CliCtx
{
    VerbaLinea *text_verba_linea_pointer;
};

struct CliEventCtx
{
    unsigned long long event;
};

struct CliFolderCtx
{
    char *folder_out;
};

struct CliGroupCtx
{
    unsigned long long group;
};

struct CliIdCtx
{
    unsigned long long id_u64;
};

struct CliFileTemporaryFileCtx
{
    CliTemporaryFile *file_temporary_file_pointer;
};

struct CliFolderWatchCtx
{
    const char *folder;
    void (*changed)(void);
};

struct CliThreadCtx
{
    CliThread *thread;
};

struct CliTreeCtx
{
    char *tree;
};

typedef struct
{
    int (*started_outside)(const CliWordsIdCtx *args);
} CliWordsIdNs;
EMBED_TABLE_LAYOUT(CliWordsIdNs, started_outside);
EMBED_TABLE_STORAGE CliWordsIdNs cli_words_id_internal EMBED_UNUSED = {
    .started_outside = cli_started_outside,
};

typedef struct
{
    unsigned int (*container_members)(const CliIdsCtx *args);
} CliIdsNs;
EMBED_TABLE_LAYOUT(CliIdsNs, container_members);
EMBED_TABLE_STORAGE CliIdsNs cli_ids_internal EMBED_UNUSED = {
    .container_members = cli_container_members,
};

typedef struct
{
    unsigned int (*processes_list)(const CliCapacityProcessesCtx *args);
} CliCapacityProcessesNs;
EMBED_TABLE_LAYOUT(CliCapacityProcessesNs, processes_list);
EMBED_TABLE_STORAGE CliCapacityProcessesNs cli_capacity_processes_internal EMBED_UNUSED = {
    .processes_list = cli_processes_list,
};

typedef struct
{
    int (*program_own)(const CliPathCapacityCtx *args);
    int (*folder_current)(const CliPathCapacityCtx *args);
    int (*file_pick)(const CliPathCapacityCtx *args);
} CliPathCapacityNs;
EMBED_TABLE_LAYOUT(CliPathCapacityNs, program_own, folder_current, file_pick);
EMBED_TABLE_STORAGE CliPathCapacityNs cli_path_capacity_internal EMBED_UNUSED = {
    .program_own = cli_program_own,
    .folder_current = cli_folder_current,
    .file_pick = cli_file_pick,
};

typedef struct
{
    unsigned int (*arguments)(const CliCapacityCountCtx *args);
} CliCapacityCountNs;
EMBED_TABLE_LAYOUT(CliCapacityCountNs, arguments);
EMBED_TABLE_STORAGE CliCapacityCountNs cli_capacity_count_internal EMBED_UNUSED = {
    .arguments = cli_arguments,
};

typedef struct
{
    int (*file_runnable)(const CliPathFileCtx *args);
    int (*file_runs)(const CliPathFileCtx *args);
    int (*is_link)(const CliPathFileCtx *args);
    int (*file_begin)(const CliPathFileCtx *args);
} CliPathFileNs;
EMBED_TABLE_LAYOUT(CliPathFileNs, file_runnable, file_runs, is_link, file_begin);
EMBED_TABLE_STORAGE CliPathFileNs cli_path_file_internal EMBED_UNUSED = {
    .file_runnable = cli_file_runnable,
    .file_runs = cli_file_runs,
    .is_link = cli_is_link,
    .file_begin = cli_file_begin,
};

typedef struct
{
    void (*logon_by_name)(const CliCapacityLogonCtx *args);
} CliCapacityLogonNs;
EMBED_TABLE_LAYOUT(CliCapacityLogonNs, logon_by_name);
EMBED_TABLE_STORAGE CliCapacityLogonNs cli_capacity_logon_internal EMBED_UNUSED = {
    .logon_by_name = cli_logon_by_name,
};

typedef struct
{
    int (*file_size)(const CliPathSizeCtx *args);
    int (*file_stamp)(const CliPathSizeCtx *args);
} CliPathSizeNs;
EMBED_TABLE_LAYOUT(CliPathSizeNs, file_size, file_stamp);
EMBED_TABLE_STORAGE CliPathSizeNs cli_path_size_internal EMBED_UNUSED = {
    .file_size = cli_file_size,
    .file_stamp = cli_file_stamp,
};

typedef struct
{
    int (*file_read_at)(const CliPathFromCtx *args);
} CliPathFromNs;
EMBED_TABLE_LAYOUT(CliPathFromNs, file_read_at);
EMBED_TABLE_STORAGE CliPathFromNs cli_path_from_internal EMBED_UNUSED = {
    .file_read_at = cli_file_read_at,
};

typedef struct
{
    unsigned char *(*file_alloc)(const CliPathCtx *args);
    unsigned char *(*file_alloc_utf8)(const CliPathCtx *args);
    unsigned char *(*file_piece_alloc)(const CliPathCtx *args);
    unsigned long long (*top)(const CliPathCtx *args);
    int (*absolute)(const CliPathCtx *args);
} CliPathNs;
EMBED_TABLE_LAYOUT(CliPathNs, file_alloc, file_alloc_utf8, file_piece_alloc, top, absolute);
EMBED_TABLE_STORAGE CliPathNs cli_path_internal EMBED_UNUSED = {
    .file_alloc = cli_file_alloc,
    .file_alloc_utf8 = cli_file_alloc_utf8,
    .file_piece_alloc = cli_file_piece_alloc,
    .top = cli_path_top,
    .absolute = cli_path_absolute,
};

typedef struct
{
    unsigned long long (*tree_bytes)(const CliPathRootCtx *args);
    void (*relative)(const CliPathRootCtx *args);
} CliPathRootNs;
EMBED_TABLE_LAYOUT(CliPathRootNs, tree_bytes, relative);
EMBED_TABLE_STORAGE CliPathRootNs cli_path_root_internal EMBED_UNUSED = {
    .tree_bytes = cli_tree_bytes,
    .relative = cli_relative,
};

typedef struct
{
    void (*onto)(const CliPathJoinCtx *args);
} CliPathJoinNs;
EMBED_TABLE_LAYOUT(CliPathJoinNs, onto);
EMBED_TABLE_STORAGE CliPathJoinNs cli_path_join_internal EMBED_UNUSED = {
    .onto = cli_path_onto,
};

typedef struct
{
    void (*system_reason)(const CliErrorCtx *args);
} CliErrorNs;
EMBED_TABLE_LAYOUT(CliErrorNs, system_reason);
EMBED_TABLE_STORAGE CliErrorNs cli_error_internal EMBED_UNUSED = {
    .system_reason = cli_system_reason,
};

typedef struct
{
    int (*keychain_store)(const CliErrMsgServiceCtx *args);
    int (*keychain_forget)(const CliErrMsgServiceCtx *args);
} CliErrMsgServiceNs;
EMBED_TABLE_LAYOUT(CliErrMsgServiceNs, keychain_store, keychain_forget);
EMBED_TABLE_STORAGE CliErrMsgServiceNs cli_err_msg_service_internal EMBED_UNUSED = {
    .keychain_store = cli_keychain_store,
    .keychain_forget = cli_keychain_forget,
};

typedef struct
{
    int (*keychain_read)(const CliServiceCtx *args);
} CliServiceNs;
EMBED_TABLE_LAYOUT(CliServiceNs, keychain_read);
EMBED_TABLE_STORAGE CliServiceNs cli_service_internal EMBED_UNUSED = {
    .keychain_read = cli_keychain_read,
};

typedef struct
{
    void (*utf8_append)(const CliLineLetterCtx *args);
} CliLineLetterNs;
EMBED_TABLE_LAYOUT(CliLineLetterNs, utf8_append);
EMBED_TABLE_STORAGE CliLineLetterNs cli_line_character_internal EMBED_UNUSED = {
    .utf8_append = cli_utf8_append,
};

typedef struct
{
    int (*raised_platform)(const CliErrMsgFromCtx *args);
    int (*move)(const CliErrMsgFromCtx *args);
} CliErrMsgFromNs;
EMBED_TABLE_LAYOUT(CliErrMsgFromNs, raised_platform, move);
EMBED_TABLE_STORAGE CliErrMsgFromNs cli_err_msg_from_internal EMBED_UNUSED = {
    .raised_platform = cli_raised_platform,
    .move = cli_move,
};

typedef struct
{
    void (*words_append)(const CliWordsWordsCtx *args);
} CliWordsWordsNs;
EMBED_TABLE_LAYOUT(CliWordsWordsNs, words_append);
EMBED_TABLE_STORAGE CliWordsWordsNs cli_words_words_internal EMBED_UNUSED = {
    .words_append = cli_words_append,
};

typedef struct
{
    void (*wake_all)(const CliWakeCtx *args);
    unsigned int (*wake_read)(const CliWakeCtx *args);
    void (*wake_wait_seen)(const CliWakeCtx *args);
    int (*wake_wait_for)(const CliWakeCtx *args);
    int (*wake_wait_seen_for)(const CliWakeCtx *args);
} CliWakeNs;
EMBED_TABLE_LAYOUT(CliWakeNs, wake_all, wake_read, wake_wait_seen, wake_wait_for, wake_wait_seen_for);
EMBED_TABLE_STORAGE CliWakeNs cli_wake_internal EMBED_UNUSED = {
    .wake_all = cli_wake_all,
    .wake_read = cli_wake_read,
    .wake_wait_seen = cli_wake_wait_seen,
    .wake_wait_for = cli_wake_wait_for,
    .wake_wait_seen_for = cli_wake_wait_seen_for,
};

typedef struct
{
    int (*spawn)(const CliWordsErrMsgCtx *args);
    int (*spawn_as)(const CliWordsErrMsgCtx *args);
    int (*spawn_feed)(const CliWordsErrMsgCtx *args);
    int (*spawn_piped_as)(const CliWordsErrMsgCtx *args);
    void (*priority_own)(const CliWordsErrMsgCtx *args);
    int (*private_folder_init)(const CliWordsErrMsgCtx *args);
    int (*is_tree_root)(const CliWordsErrMsgCtx *args);
} CliWordsErrMsgNs;
EMBED_TABLE_LAYOUT(CliWordsErrMsgNs, spawn, spawn_as, spawn_feed, spawn_piped_as, priority_own, private_folder_init,
                   is_tree_root);
EMBED_TABLE_STORAGE CliWordsErrMsgNs cli_words_err_msg_internal EMBED_UNUSED = {
    .spawn = cli_spawn,
    .spawn_as = cli_spawn_as,
    .spawn_feed = cli_spawn_feed,
    .spawn_piped_as = cli_spawn_piped_as,
    .priority_own = cli_priority_own,
    .private_folder_init = cli_private_folder_init,
    .is_tree_root = cli_is_tree_root,
};

typedef struct
{
    int (*spawn_write)(const CliPipeHandleCtx *args);
} CliPipeHandleNs;
EMBED_TABLE_LAYOUT(CliPipeHandleNs, spawn_write);
EMBED_TABLE_STORAGE CliPipeHandleNs cli_pipe_handle_internal EMBED_UNUSED = {
    .spawn_write = cli_spawn_write,
};

typedef struct
{
    int (*spawn_terminal)(const CliWordsErrMsgSpawnedCtx *args);
} CliWordsErrMsgSpawnedNs;
EMBED_TABLE_LAYOUT(CliWordsErrMsgSpawnedNs, spawn_terminal);
EMBED_TABLE_STORAGE CliWordsErrMsgSpawnedNs cli_words_err_msg_spawned_internal EMBED_UNUSED = {
    .spawn_terminal = cli_spawn_terminal,
};

typedef struct
{
    int (*spawn_terminal_resize)(const CliColsCtx *args);
    void (*spawn_terminal_hung_up)(const CliColsCtx *args);
    void (*spawn_kill)(const CliColsCtx *args);
    int (*spawn_interrupt)(const CliColsCtx *args);
    void (*spawn_group_kill)(const CliColsCtx *args);
} CliColsNs;
EMBED_TABLE_LAYOUT(CliColsNs, spawn_terminal_resize, spawn_terminal_hung_up, spawn_kill, spawn_interrupt,
                   spawn_group_kill);
EMBED_TABLE_STORAGE CliColsNs cli_cols_internal EMBED_UNUSED = {
    .spawn_terminal_resize = cli_spawn_terminal_resize,
    .spawn_terminal_hung_up = cli_spawn_terminal_hung_up,
    .spawn_kill = cli_spawn_kill,
    .spawn_interrupt = cli_spawn_interrupt,
    .spawn_group_kill = cli_spawn_group_kill,
};

typedef struct
{
    void (*spawn_terminal_close)(const CliSpawnedCtx *args);
    void (*spawn_input_close)(const CliSpawnedCtx *args);
    long long (*spawn_wait)(const CliSpawnedCtx *args);
    int (*spawn_poll)(const CliSpawnedCtx *args);
    void (*spawn_close)(const CliSpawnedCtx *args);
} CliSpawnedNs;
EMBED_TABLE_LAYOUT(CliSpawnedNs, spawn_terminal_close, spawn_input_close, spawn_wait, spawn_poll, spawn_close);
EMBED_TABLE_STORAGE CliSpawnedNs cli_spawned_internal EMBED_UNUSED = {
    .spawn_terminal_close = cli_spawn_terminal_close,
    .spawn_input_close = cli_spawn_input_close,
    .spawn_wait = cli_spawn_wait,
    .spawn_poll = cli_spawn_poll,
    .spawn_close = cli_spawn_close,
};

typedef struct
{
    int (*spawn_share)(const CliWordsErrMsgSettingsCtx *args);
} CliWordsErrMsgSettingsNs;
EMBED_TABLE_LAYOUT(CliWordsErrMsgSettingsNs, spawn_share);
EMBED_TABLE_STORAGE CliWordsErrMsgSettingsNs cli_words_err_msg_settings_internal EMBED_UNUSED = {
    .spawn_share = cli_spawn_share,
};

typedef struct
{
    void (*run_result_free)(const CliWordsCtx *args);
    int (*run)(const CliWordsCtx *args);
    int (*run_result_feed)(const CliWordsCtx *args);
    int (*run_result_within)(const CliWordsCtx *args);
} CliWordsNs;
EMBED_TABLE_LAYOUT(CliWordsNs, run_result_free, run, run_result_feed, run_result_within);
EMBED_TABLE_STORAGE CliWordsNs cli_words_internal EMBED_UNUSED = {
    .run_result_free = cli_run_result_free,
    .run = cli_on_run,
    .run_result_feed = cli_run_result_feed,
    .run_result_within = cli_run_result_within,
};

typedef struct
{
    int (*resource_add)(const CliPathErrMsgCtx *args);
} CliPathErrMsgNs;
EMBED_TABLE_LAYOUT(CliPathErrMsgNs, resource_add);
EMBED_TABLE_STORAGE CliPathErrMsgNs cli_path_err_msg_internal EMBED_UNUSED = {
    .resource_add = cli_resource_add,
};

typedef struct
{
    int (*file_pieces_write)(const CliPathCountCtx *args);
} CliPathCountNs;
EMBED_TABLE_LAYOUT(CliPathCountNs, file_pieces_write);
EMBED_TABLE_STORAGE CliPathCountNs cli_path_count_internal EMBED_UNUSED = {
    .file_pieces_write = cli_file_pieces_write,
};

typedef struct
{
    int (*file_put_write)(const CliPathPutCtx *args);
} CliPathPutNs;
EMBED_TABLE_LAYOUT(CliPathPutNs, file_put_write);
EMBED_TABLE_STORAGE CliPathPutNs cli_path_put_internal EMBED_UNUSED = {
    .file_put_write = cli_file_put_write,
};

typedef struct
{
    int (*file_end)(const CliFileCtx *args);
} CliFileNs;
EMBED_TABLE_LAYOUT(CliFileNs, file_end);
EMBED_TABLE_STORAGE CliFileNs cli_file_internal EMBED_UNUSED = {
    .file_end = cli_file_end,
};

typedef struct
{
    VerbaLinea (*file_line)(const CliFileStageCtx *args);
} CliFileStageNs;
EMBED_TABLE_LAYOUT(CliFileStageNs, file_line);
EMBED_TABLE_STORAGE CliFileStageNs cli_file_stage_internal EMBED_UNUSED = {
    .file_line = cli_file_line,
};

typedef struct
{
    int (*file_line_end)(const CliLineCtx *args);
} CliLineNs;
EMBED_TABLE_LAYOUT(CliLineNs, file_line_end);
EMBED_TABLE_STORAGE CliLineNs cli_line_internal EMBED_UNUSED = {
    .file_line_end = cli_file_line_end,
};

typedef struct
{
    int (*file_append)(const CliPathBytesCtx *args);
    int (*private_file_write)(const CliPathBytesCtx *args);
} CliPathBytesNs;
EMBED_TABLE_LAYOUT(CliPathBytesNs, file_append, private_file_write);
EMBED_TABLE_STORAGE CliPathBytesNs cli_path_bytes_internal EMBED_UNUSED = {
    .file_append = cli_file_append,
    .private_file_write = cli_private_file_write,
};

typedef struct
{
    int (*temporary_file_init)(const CliFileTemporaryFileCtx *args);
    void (*temporary_file_let_go)(const CliFileTemporaryFileCtx *args);
} CliFileTemporaryFileNs;
EMBED_TABLE_LAYOUT(CliFileTemporaryFileNs, temporary_file_init, temporary_file_let_go);
EMBED_TABLE_STORAGE CliFileTemporaryFileNs cli_file_temporary_file_internal EMBED_UNUSED = {
    .temporary_file_init = cli_temporary_file_init,
    .temporary_file_let_go = cli_temporary_file_let_go,
};

typedef struct
{
    int (*temporary_file_write)(const CliBytesCtx *args);
} CliBytesNs;
EMBED_TABLE_LAYOUT(CliBytesNs, temporary_file_write);
EMBED_TABLE_STORAGE CliBytesNs cli_bytes_internal EMBED_UNUSED = {
    .temporary_file_write = cli_temporary_file_write,
};

typedef struct
{
    int (*temporary_file_read)(const CliSizeCtx *args);
} CliSizeNs;
EMBED_TABLE_LAYOUT(CliSizeNs, temporary_file_read);
EMBED_TABLE_STORAGE CliSizeNs cli_size_internal EMBED_UNUSED = {
    .temporary_file_read = cli_temporary_file_read,
};

typedef struct
{
    int (*clipboard_text_read)(const CliCtx *args);
} CliNs;
EMBED_TABLE_LAYOUT(CliNs, clipboard_text_read);
EMBED_TABLE_STORAGE CliNs cli_internal_table EMBED_UNUSED = {
    .clipboard_text_read = cli_clipboard_text_read,
};

typedef struct
{
    int (*clipboard_text_write)(const CliSizeTextCtx *args);
} CliSizeTextNs;
EMBED_TABLE_LAYOUT(CliSizeTextNs, clipboard_text_write);
EMBED_TABLE_STORAGE CliSizeTextNs cli_size_text_internal EMBED_UNUSED = {
    .clipboard_text_write = cli_clipboard_text_write,
};

typedef struct
{
    int (*clipboard_files_append)(const CliCountCtx *args);
} CliCountNs;
EMBED_TABLE_LAYOUT(CliCountNs, clipboard_files_append);
EMBED_TABLE_STORAGE CliCountNs cli_count_internal EMBED_UNUSED = {
    .clipboard_files_append = cli_clipboard_files_append,
};

typedef struct
{
    unsigned int (*clipboard_files_read)(const CliEachCtx *args);
} CliEachNs;
EMBED_TABLE_LAYOUT(CliEachNs, clipboard_files_read);
EMBED_TABLE_STORAGE CliEachNs cli_each_internal EMBED_UNUSED = {
    .clipboard_files_read = cli_clipboard_files_read,
};

typedef struct
{
    unsigned long long (*spawn_read)(const CliCapacityPipeHandleCtx *args);
    unsigned long long (*spawn_waiting)(const CliCapacityPipeHandleCtx *args);
} CliCapacityPipeHandleNs;
EMBED_TABLE_LAYOUT(CliCapacityPipeHandleNs, spawn_read, spawn_waiting);
EMBED_TABLE_STORAGE CliCapacityPipeHandleNs cli_capacity_pipe_handle_internal EMBED_UNUSED = {
    .spawn_read = cli_spawn_read,
    .spawn_waiting = cli_spawn_waiting,
};

typedef struct
{
    void (*read)(const CliSpawnLinesCtx *args);
} CliSpawnLinesNs;
EMBED_TABLE_LAYOUT(CliSpawnLinesNs, read);
EMBED_TABLE_STORAGE CliSpawnLinesNs cli_spawn_lines_internal EMBED_UNUSED = {
    .read = cli_spawn_lines_read,
};

typedef struct
{
    int (*registry_machine_path)(const CliOutCtx *args);
    void (*registry_expand)(const CliOutCtx *args);
} CliOutNs;
EMBED_TABLE_LAYOUT(CliOutNs, registry_machine_path, registry_expand);
EMBED_TABLE_STORAGE CliOutNs cli_out_internal EMBED_UNUSED = {
    .registry_machine_path = cli_registry_machine_path,
    .registry_expand = cli_registry_expand,
};

typedef struct
{
    int (*registry_user_value)(const CliOutCapacityCtx *args);
} CliOutCapacityNs;
EMBED_TABLE_LAYOUT(CliOutCapacityNs, registry_user_value);
EMBED_TABLE_STORAGE CliOutCapacityNs cli_out_capacity_internal EMBED_UNUSED = {
    .registry_user_value = cli_registry_user_value,
};

typedef struct
{
    int (*registry_set_user_value)(const CliErrMsgNameCtx *args);
    void (*unmount)(const CliErrMsgNameCtx *args);
} CliErrMsgNameNs;
EMBED_TABLE_LAYOUT(CliErrMsgNameNs, registry_set_user_value, unmount);
EMBED_TABLE_STORAGE CliErrMsgNameNs cli_err_msg_name_internal EMBED_UNUSED = {
    .registry_set_user_value = cli_registry_set_user_value,
    .unmount = cli_unmount,
};

typedef struct
{
    void (*spawn_group_end)(const CliGroupCtx *args);
} CliGroupNs;
EMBED_TABLE_LAYOUT(CliGroupNs, spawn_group_end);
EMBED_TABLE_STORAGE CliGroupNs cli_group_internal EMBED_UNUSED = {
    .spawn_group_end = cli_spawn_group_end,
};

typedef struct
{
    void (*spawn_tree_end)(const CliIdCtx *args);
} CliIdNs;
EMBED_TABLE_LAYOUT(CliIdNs, spawn_tree_end);
EMBED_TABLE_STORAGE CliIdNs cli_id_internal EMBED_UNUSED = {
    .spawn_tree_end = cli_spawn_tree_end,
};

typedef struct
{
    unsigned int (*path_folders)(const CliCapacityFoldersCtx *args);
} CliCapacityFoldersNs;
EMBED_TABLE_LAYOUT(CliCapacityFoldersNs, path_folders);
EMBED_TABLE_STORAGE CliCapacityFoldersNs cli_capacity_folders_internal EMBED_UNUSED = {
    .path_folders = cli_path_folders,
};

typedef struct
{
    void (*path_join)(const CliPathCountSizeCtx *args);
} CliPathCountSizeNs;
EMBED_TABLE_LAYOUT(CliPathCountSizeNs, path_join);
EMBED_TABLE_STORAGE CliPathCountSizeNs cli_path_count_size_internal EMBED_UNUSED = {
    .path_join = cli_path_join,
};

typedef struct
{
    int (*watch_system_start)(const CliRootContextCtx *args);
} CliRootContextNs;
EMBED_TABLE_LAYOUT(CliRootContextNs, watch_system_start);
EMBED_TABLE_STORAGE CliRootContextNs cli_root_context_internal EMBED_UNUSED = {
    .watch_system_start = cli_watch_system_start,
};

typedef struct
{
    int (*thread_start)(const CliThreadCtx *args);
} CliThreadNs;
EMBED_TABLE_LAYOUT(CliThreadNs, thread_start);
EMBED_TABLE_STORAGE CliThreadNs cli_thread_internal EMBED_UNUSED = {
    .thread_start = cli_thread_start,
};

typedef struct
{
    int (*start)(const CliFolderWatchCtx *args);
} CliFolderWatchNs;
EMBED_TABLE_LAYOUT(CliFolderWatchNs, start);
EMBED_TABLE_STORAGE CliFolderWatchNs cli_folder_watch_internal EMBED_UNUSED = {
    .start = cli_folder_watch_start,
};

typedef struct
{
    int (*socket_connect)(const CliErrMsgCtx *args);
    void (*socket_close)(const CliErrMsgCtx *args);
    int (*socket_connected_within)(const CliErrMsgCtx *args);
    int (*socket_connected_time)(const CliErrMsgCtx *args);
} CliErrMsgNs;
EMBED_TABLE_LAYOUT(CliErrMsgNs, socket_connect, socket_close, socket_connected_within, socket_connected_time);
EMBED_TABLE_STORAGE CliErrMsgNs cli_err_msg_internal EMBED_UNUSED = {
    .socket_connect = cli_socket_connect,
    .socket_close = cli_socket_close,
    .socket_connected_within = cli_socket_connected_within,
    .socket_connected_time = cli_socket_connected_time,
};

typedef struct
{
    int (*socket_write)(const CliBytesSizeCtx *args);
} CliBytesSizeNs;
EMBED_TABLE_LAYOUT(CliBytesSizeNs, socket_write);
EMBED_TABLE_STORAGE CliBytesSizeNs cli_bytes_size_internal EMBED_UNUSED = {
    .socket_write = cli_socket_write,
};

typedef struct
{
    int (*socket_read)(const CliSizeBytesCtx *args);
    int (*random_bytes)(const CliSizeBytesCtx *args);
} CliSizeBytesNs;
EMBED_TABLE_LAYOUT(CliSizeBytesNs, socket_read, random_bytes);
EMBED_TABLE_STORAGE CliSizeBytesNs cli_size_bytes_internal EMBED_UNUSED = {
    .socket_read = cli_socket_read,
    .random_bytes = cli_random_bytes,
};

typedef struct
{
    long long (*socket_some)(const CliCapacityCtx *args);
} CliCapacityNs;
EMBED_TABLE_LAYOUT(CliCapacityNs, socket_some);
EMBED_TABLE_STORAGE CliCapacityNs cli_capacity_internal EMBED_UNUSED = {
    .socket_some = cli_socket_some,
};

typedef struct
{
    void (*socket_wait)(const CliSockCtx *args);
    void (*socket_write_wait)(const CliSockCtx *args);
    void (*socket_end)(const CliSockCtx *args);
} CliSockNs;
EMBED_TABLE_LAYOUT(CliSockNs, socket_wait, socket_write_wait, socket_end);
EMBED_TABLE_STORAGE CliSockNs cli_sock_internal EMBED_UNUSED = {
    .socket_wait = cli_socket_wait,
    .socket_write_wait = cli_socket_write_wait,
    .socket_end = cli_socket_end,
};

typedef struct
{
    int (*port_free)(const CliErrMsgPortCtx *args);
    int (*socket_listening)(const CliErrMsgPortCtx *args);
} CliErrMsgPortNs;
EMBED_TABLE_LAYOUT(CliErrMsgPortNs, port_free, socket_listening);
EMBED_TABLE_STORAGE CliErrMsgPortNs cli_err_msg_port_internal EMBED_UNUSED = {
    .port_free = cli_port_free,
    .socket_listening = cli_socket_listening,
};

typedef struct
{
    int (*socket_accept)(const CliListenerCtx *args);
} CliListenerNs;
EMBED_TABLE_LAYOUT(CliListenerNs, socket_accept);
EMBED_TABLE_STORAGE CliListenerNs cli_listener_internal EMBED_UNUSED = {
    .socket_accept = cli_socket_accept,
};

typedef struct
{
    void (*processes_each)(const CliContextCtx *args);
} CliContextNs;
EMBED_TABLE_LAYOUT(CliContextNs, processes_each);
EMBED_TABLE_STORAGE CliContextNs cli_context_internal EMBED_UNUSED = {
    .processes_each = cli_processes_each,
};

typedef struct
{
    int (*process_running)(const CliPidCtx *args);
    int (*process_memory)(const CliPidCtx *args);
} CliPidNs;
EMBED_TABLE_LAYOUT(CliPidNs, process_running, process_memory);
EMBED_TABLE_STORAGE CliPidNs cli_pid_internal EMBED_UNUSED = {
    .process_running = cli_process_running,
    .process_memory = cli_process_memory,
};

typedef struct
{
    int (*process_by_name)(const CliCapacityPidCtx *args);
} CliCapacityPidNs;
EMBED_TABLE_LAYOUT(CliCapacityPidNs, process_by_name);
EMBED_TABLE_STORAGE CliCapacityPidNs cli_capacity_pid_internal EMBED_UNUSED = {
    .process_by_name = cli_process_by_name,
};

typedef struct
{
    void (*temp_folder)(const CliFolderCtx *args);
    int (*home_folder)(const CliFolderCtx *args);
    int (*home_plugins)(const CliFolderCtx *args);
    int (*home_languages)(const CliFolderCtx *args);
} CliFolderNs;
EMBED_TABLE_LAYOUT(CliFolderNs, temp_folder, home_folder, home_plugins, home_languages);
EMBED_TABLE_STORAGE CliFolderNs cli_folder_internal EMBED_UNUSED = {
    .temp_folder = cli_temp_folder,
    .home_folder = cli_home_folder,
    .home_plugins = cli_home_plugins,
    .home_languages = cli_home_languages,
};

typedef struct
{
    int (*event_set)(const CliEventCtx *args);
    void (*event_close)(const CliEventCtx *args);
} CliEventNs;
EMBED_TABLE_LAYOUT(CliEventNs, event_set, event_close);
EMBED_TABLE_STORAGE CliEventNs cli_event_internal EMBED_UNUSED = {
    .event_set = cli_event_set,
    .event_close = cli_event_close,
};

typedef struct
{
    int (*tree_at)(const CliPathErrMsgFullCtx *args);
} CliPathErrMsgFullNs;
EMBED_TABLE_LAYOUT(CliPathErrMsgFullNs, tree_at);
EMBED_TABLE_STORAGE CliPathErrMsgFullNs cli_path_err_msg_full_internal EMBED_UNUSED = {
    .tree_at = cli_tree_at,
};

typedef struct
{
    int (*tree_find)(const CliTreeCtx *args);
} CliTreeNs;
EMBED_TABLE_LAYOUT(CliTreeNs, tree_find);
EMBED_TABLE_STORAGE CliTreeNs cli_tree_internal EMBED_UNUSED = {
    .tree_find = cli_tree_on_find,
};

typedef struct
{
    int (*mount)(const CliPathErrMsgNameCtx *args);
    int (*home_reveal)(const CliPathErrMsgNameCtx *args);
} CliPathErrMsgNameNs;
EMBED_TABLE_LAYOUT(CliPathErrMsgNameNs, mount, home_reveal);
EMBED_TABLE_STORAGE CliPathErrMsgNameNs cli_path_err_msg_name_internal EMBED_UNUSED = {
    .mount = cli_mount,
    .home_reveal = cli_home_reveal,
};

typedef struct
{
    CliMounts (*mounts)(void);
} CliCapacityNamesNs;
EMBED_TABLE_LAYOUT(CliCapacityNamesNs, mounts);
EMBED_TABLE_STORAGE CliCapacityNamesNs cli_capacity_names_internal EMBED_UNUSED = {
    .mounts = cli_mounts,
};

typedef struct
{
    int (*inside)(const CliRootCtx *args);
    void (*full)(const CliRootCtx *args);
} CliRootNs;
EMBED_TABLE_LAYOUT(CliRootNs, inside, full);
EMBED_TABLE_STORAGE CliRootNs cli_root_internal EMBED_UNUSED = {
    .inside = cli_inside,
    .full = cli_full,
};

typedef struct
{
    int (*path_parent)(const CliPathOutTypeCtx *args);
} CliPathOutTypeNs;
EMBED_TABLE_LAYOUT(CliPathOutTypeNs, path_parent);
EMBED_TABLE_STORAGE CliPathOutTypeNs cli_path_out_type_internal EMBED_UNUSED = {
    .path_parent = cli_path_parent,
};

typedef struct
{
    int (*home_user_css)(const CliPathErrMsgTypeCtx *args);
    int (*home_menus)(const CliPathErrMsgTypeCtx *args);
    int (*home_ensure_menus)(const CliPathErrMsgTypeCtx *args);
    int (*home_ensure_user_css)(const CliPathErrMsgTypeCtx *args);
    int (*settings_ensure)(const CliPathErrMsgTypeCtx *args);
} CliPathErrMsgTypeNs;
EMBED_TABLE_LAYOUT(CliPathErrMsgTypeNs, home_user_css, home_menus, home_ensure_menus, home_ensure_user_css,
                   settings_ensure);
EMBED_TABLE_STORAGE CliPathErrMsgTypeNs cli_path_err_msg_type_internal EMBED_UNUSED = {
    .home_user_css = cli_home_user_css,
    .home_menus = cli_home_menus,
    .home_ensure_menus = cli_home_ensure_menus,
    .home_ensure_user_css = cli_home_ensure_user_css,
    .settings_ensure = cli_settings_ensure,
};

#endif
