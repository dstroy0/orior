// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_internal.h: what the modules of the command line share: the query's platform calls it is built on, its own
// platform's calls (platform/windows.c, platform/posix.c), its bounds, and the functions one module reads from
// another
#ifndef CLI_INTERNAL_H
#define CLI_INTERNAL_H
#include "../../../../includes/file_defs/ksg/ksg_writing.h"
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
typedef struct CliCapacityNamesCtx CliCapacityNamesCtx;
typedef struct CliCapacityPidCtx CliCapacityPidCtx;
typedef struct CliErrMsgFromCtx CliErrMsgFromCtx;
typedef struct CliOutCtx CliOutCtx;
typedef struct CliErrMsgNameCtx CliErrMsgNameCtx;
typedef struct CliOutCapacityCtx CliOutCapacityCtx;
typedef struct CliPathRootCtx CliPathRootCtx;
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
typedef struct CliTreeCtx CliTreeCtx;

// the longest path and the longest line the command line handles, as the query's
#define CLI_PATH_MAX_LEN QUERY_PATH_MAX_LEN

#define CLI_ERR_MSG_MAX_LEN QUERY_LINE_MAX_LEN

// orior's version
#define CLI_VERSION "0.1.0"

// the most processes the system lists, the most mounts a window lists, and the longest name of one
#define CLI_PROCESSES_MAX 65536u

#define CLI_MOUNTS_MAX 64u

#define CLI_NAME_MAX_LEN 256u

// A process the system lists: its id and its parent's
typedef struct
{
    unsigned int id;
    unsigned int parent;
} CliProcess;

// the command line's platform calls: this process put at the head of its container, the job on Windows; a process
// started outside the container, its id; every process in the container but this one, by id; every process the system
// lists with its parent; every process in the container but this one ended; a process group's members signaled on
// POSIX; this process's id; the program this process runs, its path
int cli_container_init(void);
int cli_started_outside(const CliWordsIdCtx *args);
unsigned int cli_container_members(const CliIdsCtx *args);
unsigned int cli_processes_list(const CliCapacityProcessesCtx *args);
void cli_container_members_end(void);
unsigned int cli_process_own(void);
int cli_program_own(const CliPathCapacityCtx *args);
// the folder this process stands in, into `path`
int cli_folder_current(const CliPathCapacityCtx *args);
// 1 where this process's errors go to a terminal, and where its output does (platform/windows.c, platform/posix.c)
int cli_errors_terminal(void);
int cli_output_terminal(void);
// the words this program was given after its name, at most `capacity` of them, into `words`, each UTF-8: on Windows as
// the system stores them, wide, and elsewhere the `count` words of `supplied`, or as the system saves them where
// `supplied` is NULL: their count
unsigned int cli_arguments(const CliCapacityCountCtx *args);
// the file at `path` made one its owner runs, on POSIX
int cli_file_runnable(const CliPathFileCtx *args);
// whether the file's owner may run it, its rights unchanged; 0 on a system whose files carry no such right
int cli_file_runs(const CliPathFileCtx *args);
// 1 where the file at `path` is there and is a file the system calls a link to elsewhere, never followed
int cli_is_link(const CliPathFileCtx *args);
// the milliseconds a clock that only goes forward reads
unsigned long long cli_clock_milliseconds(void);
// the seconds since 1970 the machine started at, as this reading makes it, a few either way from one reading to the
// next, 0 where the system does not say; and the reader's log in as the system names it, every log in a new name, into
// `logon`, sized for `capacity`, empty where the system names none (platform/windows_system.c, platform/posix_system.c)
unsigned long long cli_booted_seconds(void);
void cli_logon_by_name(const CliCapacityLogonCtx *args);
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
// the calendar reading of `seconds` since 1970 in the machine's own zone into `civil_out`, 1970's first moment where
// the system does not read it (platform/windows_system.c, platform/posix_system.c)
void cli_clock_civil(const CliClockCivilCtx *args);
// the size of the file at `path` into `size`: 1, or 0 where it is not there; and the milliseconds since 1970 the wall
// clock reads (platform/windows_files.c, platform/posix_files.c)
int cli_file_size(const CliPathSizeCtx *args);
// the size of the file at `path` and the milliseconds since 1970 it last changed at: 1, or 0 where it is not there
int cli_file_stamp(const CliPathSizeCtx *args);
// `count` bytes of the file at `path` from its byte `from` read into `bytes`, their count into `got`, fewer where the
// file ends first: 1, or 0 where the file could not be opened or read
int cli_file_read_at(const CliPathFromCtx *args);
// the bytes past a file's stored in its cell, zero: a word a walk loads over a text's last bytes, and the terminator
#define CLI_FILE_SLACK 64ull
// The bytes of a file in one temporary cell of their size and the slack, counted to `owner`: the file at `path` read
// whole, its size into `*size`; the same where its bytes read as UTF-8 whole; or `count` bytes of it from `from`, their
// count into `*size`, fewer where the file ends first. NULL where the file could not be read or the cells cannot meet
// it; a file of no bytes is bytes of none. And a cell of a file's bytes given back to `owner`, NULL being none
// (machine/file_cell.c)
unsigned char *cli_file_alloc(const CliPathCtx *args);
unsigned char *cli_file_alloc_utf8(const CliPathCtx *args);
unsigned char *cli_file_piece_alloc(const CliPathCtx *args);
// the bytes of every file under `root`, the folders of version control and of cargo's build passed over: the request a
// tree's pool is made for (machine/file_cell.c)
unsigned long long cli_tree_bytes(const CliPathRootCtx *args);
// the system's number for why the call just made failed, read before any other call; and its reason written after
// `err_msg`, as ": Access is denied. (os error 5)" (platform/windows_files.c, platform/posix_files.c)
unsigned int cli_system_error(void);
void cli_system_reason(const CliErrorCtx *args);
// `secret` stored for `account` of `service`, such as "database" or "plugin sonarqube", in the system's keychain, on
// this machine alone, under the service's name "orior " and `service`; the secret stored for `account` of `service`
// into `out`, sized for `capacity`: 1, or 0 where none is stored; and the secret taken out
// (platform/windows_keychain.c, platform/posix_keychain.c)
int cli_keychain_store(const CliErrMsgServiceCtx *args);
int cli_keychain_read(const CliServiceCtx *args);
int cli_keychain_forget(const CliErrMsgServiceCtx *args);
unsigned long long cli_wall_milliseconds(void);
// one letter written in UTF-8 (words.c)
void cli_utf8_append(const CliLineLetterCtx *args);
// the staged file `from` copied over `to` with the rights the system requires of the reader: on Windows orior's own
// program run again as `imp write-raised`, raised; on Linux cp through pkexec; on macOS cp with the system's
// authorization. 1, or 0 with why given
int cli_raised_platform(const CliErrMsgFromCtx *args);

// the most words of one command a program is run with, and the most bytes they take
#define CLI_WORDS_MAX 512u

#define CLI_WORDS_TEXT (1u << 16u)

// The words of one command a program is run with, NULL after the last, and the bytes they take; 1 in `full` where a
// word did not fit
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

// A wake threads wait on: a generation word cli_wake_all moves on, a wait sleeping on the word's address until it
// moves or the wait's time is up. A wait is free to return early, so a waiter tests what it waits for in a loop
typedef struct
{
    embed_atomic_word generation;
} CliWake;

#define CLI_WAKE_INIT {EMBED_ATOMIC_INIT(0u)}

// A gate one thread holds at a time: its word's held bit and its sleeper bit. A thread that finds the gate held sleeps
// on the word with the turnstile's backoff until the holder opens it and wakes the word
typedef struct
{
    embed_atomic_word word;
} CliGate;

#define CLI_GATE_INIT {EMBED_ATOMIC_INIT(0u)}
#define CLI_GATE_HELD ((embed_atomic_word_value)1u)
#define CLI_GATE_SLEEPER ((embed_atomic_word_value)2u)

// the gate shut by this thread, once the holder before it has opened it
EMBED_INLINE void cli_gate_shut(CliGate *gate)
{
    EmbedTurnstileBackoff backoff;
    embed_atomic_word_value was = embed_atomic_word_fetch_or(&gate->word, CLI_GATE_HELD);
    embed_turnstile_backoff_begin(&backoff);
    while ((was & CLI_GATE_HELD) != 0u)
    {
        embed_turnstile_backed_off(&gate->word, was, CLI_GATE_SLEEPER, &backoff, 1u);
        was = embed_atomic_word_fetch_or(&gate->word, CLI_GATE_HELD);
    }
    EMBED_ATOMIC_ORDERED();
}

// the gate opened by its holder, and a thread asleep on it woken
EMBED_INLINE void cli_gate_opened(CliGate *gate)
{
    EMBED_ATOMIC_ORDERED();
    const embed_atomic_word_value was = embed_atomic_word_fetch_and(&gate->word, ~(CLI_GATE_HELD | CLI_GATE_SLEEPER));
    if ((was & CLI_GATE_SLEEPER) != 0u)
    {
        embed_word_wake(&gate->word);
    }
}

// the epoch a ticket ring's stamp carries: the clock in steps of about a second, in the stamp's sixteen bits
EMBED_INLINE embed_u16 cli_ticket_epoch(void)
{
    // Explicit cast keeps the low sixteen bits of the clock's steps
    return (embed_u16)(embed_clock_ns() >> 30u);
}

// whether the slot of the ticket `next` carries that ticket with its push landed and no pop yet, its stamp with the
// flag bits clear into `stamp_out`: what a consumer tests before it pops the next ticket in order
EMBED_INLINE int cli_ticket_pushed(const EmbedTicketRing *ring, embed_u32 next, embed_u64 *stamp_out)
{
    const embed_u64 current = embed_atomic_u64_load(&embed_ticket_ring_slot(ring, (embed_u64)next)->stamp);
    *stamp_out = embed_ticket_fields_of_stamp(current);
    return (embed_ticket_of_stamp(current) == next) && ((current & EMBED_TICKET_PUSHED) != 0u) &&
           ((current & EMBED_TICKET_POPPED) == 0u);
}

// Whether the slot of the ticket `next` is ready for its grant: a zero word on the ring's first lap, else the
// released word of the ticket one lap before. A producer that grants only then keeps every live ticket within one
// lap of the ring, which the ring's proofs need and the consumer's pop in ticket order relies on
EMBED_INLINE int cli_ticket_room(const EmbedTicketRing *ring, embed_u32 next)
{
    const embed_u64 current = embed_atomic_u64_load(&embed_ticket_ring_slot(ring, (embed_u64)next)->stamp);
    // Explicit cast narrows the slot count, at most 2^31, to the ticket's width
    const embed_u32 lap = (embed_u32)(ring->mask + 1u);
    return ((current == 0u) && (next < lap)) ||
           (((current & EMBED_TICKET_RELEASED) != 0u) && (embed_ticket_of_stamp(current) == next - lap));
}

// A setting of a started process's environment: `name` set to `value`, or taken out where `value` is NULL
typedef struct
{
    const char *name;
    const char *value;
} CliSetting;

// The priority a thread or a program starts at, so that what must not wait does not, and what runs as the host's
// guest gives the processor back the moment the host wants it. The window's own threads, which answer it, run at the
// process's level and are not set here. Work is that level, and the default: language servers, git, formatters.
// Run stands above it: a run's writer and the channels that hand to it. A guest stands below it: builds, tests,
// checkers, clones, installs and scans. Idle runs only in time nothing else wants: caches warmed ahead
#define CLI_PRIORITY_WORK 0u
#define CLI_PRIORITY_RUN 1u
#define CLI_PRIORITY_GUEST 2u
#define CLI_PRIORITY_IDLE 3u

// A process a run starts: the system's handle of it and its id, its output and its errors each piped back, the job it
// was made the head of on Windows, 0 for none, the pipe its standard input is fed by, 0 for none, and 1 once it was
// waited on to its end
typedef struct
{
    unsigned long long process;
    unsigned long long id;
    unsigned long long out;
    unsigned long long err;
    unsigned long long group;
    unsigned long long in;
    unsigned long long waited;
} CliSpawned;

// a process started with `words`, the program first and NULL after the last, in `folder`, its environment this
// process's with `settings` after, `count` of them, its standard input nothing: where `detached`, with no console of
// its own and a job of its own on Windows, a process group of its own elsewhere. 1, or 0 with why given
// (platform/windows_spawn.c, platform/posix_spawn.c)
int cli_spawn(const CliWordsErrMsgCtx *args);
// a process started as cli_spawn starts one, at `priority`, one of CLI_PRIORITY_WORK and the rest; cli_spawn starts
// one at work
int cli_spawn_as(const CliWordsErrMsgCtx *args);
// a process started as cli_spawn starts one, its standard input a pipe this process feeds through `in`, at work or at
// `priority`; `size` bytes written whole to that pipe: 1, or 0 where the process stopped reading; and the pipe closed,
// which the process reads as its input's end
int cli_spawn_feed(const CliWordsErrMsgCtx *args);
int cli_spawn_piped_as(const CliWordsErrMsgCtx *args);
int cli_spawn_write(const CliPipeHandleCtx *args);
// a process started as cli_spawn starts one, in a pseudo-terminal `cols` wide and `rows` tall of its own, a terminal's
// shell: its output and errors read from `out` and the keys typed written to `in`, the pseudo-console in `group` on
// Windows: 1, or 0 with why given; the pseudo-terminal given a new size, which the shell and the programs in it are
// told of: 1, or 0 where it could not be; the shell hung up on, as a terminal closed ends it; and the pseudo-terminal
// let go, after which a reader of `out` meets its end, before cli_spawn_close lets the rest go
// (platform/windows_spawn.c, platform/posix_spawn.c)
int cli_spawn_terminal(const CliWordsErrMsgSpawnedCtx *args);
int cli_spawn_terminal_resize(const CliColsCtx *args);
void cli_spawn_terminal_hung_up(const CliColsCtx *args);
void cli_spawn_terminal_close(const CliSpawnedCtx *args);
// a process started as cli_spawn starts one, sharing this process's console, its input, output and errors this
// process's, and waited on: its exit code into `code`, -1 where a signal ended it, 1, or 0 with why it did not start
// given
int cli_spawn_share(const CliWordsErrMsgSettingsCtx *args);
void cli_spawn_input_close(const CliSpawnedCtx *args);

// What a program run gave: what it wrote to its output and to its errors, each a cell the size of what came and
// ended by a 0, given back with the run, 0 in `whole` where a cell could not be taken and what came is not all stored,
// its exit code, -1 where a signal ended it, and 1 in `stopped` where it ran past its time and was ended
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

// what one program run gave taken from those retained, waiting while each is in use, and given back; and a program run
// as cli_spawn starts one, detached, its errors read on a thread of their own while its output is read, and waited on:
// what it gave into `run_result`, 1, or 0 with why it did not start given (ran.c)
CliRunResult *cli_run_result_alloc(void);
void cli_run_result_free(const CliWordsCtx *args);
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

// the file at `path` written as `count` pieces in turn, any file there replaced whole or not at all: 1, or 0 where it
// could not be; and the file at `path` written with what `put` puts into the file writer's line, handed `context`, the
// same way: 1, or 0 where `put` returned 0 or the file could not be written (file_writing.c)
int cli_file_pieces_write(const CliPathCountCtx *args);
int cli_file_put_write(const CliPathPutCtx *args);

// the bytes a file is written in: one view of the Windows cache manager, VACB_MAPPING_GRANULARITY in ntifs.h, written
// at an offset that is a whole number of them, each a whole number of 4 KiB pages
#define CLI_STAGE_BYTES (256u << 10u)

_Static_assert((CLI_STAGE_BYTES % 4096u) == 0u, "CLI_STAGE_BYTES is a whole number of 4 KiB pages");
_Static_assert(((256u << 10u) % CLI_STAGE_BYTES) == 0u, "CLI_STAGE_BYTES divides the cache manager's 256 KiB view");

// A file being written whole, and sealed where it is of a type ksg seals (ksg_writing.h): written beside its place
// under a name of this process's own, made durable and put in its place, and a write that fails leaves the file it
// would have replaced as it was
typedef KsgWriting CliFileWriting;

_Static_assert(CLI_PATH_MAX_LEN <= KSG_PATH_MAX_LEN, "a path of the command line's fits a sealed writer's");

// the file `path` begun beside its place, with its seal where it is sealed: 1, or 0 where it could not be made; `size`
// bytes at `bytes` written after what the file begun contains, the shape of a line's write-through: 1, or 0 where they
// were not; and the file begun on the disk and put in place of `path` where `whole` is 1, or taken away where it is 0:
// 1, or 0 where it was not put in place (file_writing.c)
int cli_file_begin(const CliPathFileCtx *args);
int cli_file_write_chunk(void *context, const char *bytes, embed_word size);
int cli_file_end(const CliFileCtx *args);

// a line over `stage`, which is sized for CLI_STAGE_BYTES + 1, handing each whole stage to the file begun; and the
// line's last bytes handed to it, the file put in place where every byte went and taken away where one did not: 1, or 0
// where it was not put in place (file_writing.c)
VerbaLinea cli_file_line(const CliFileStageCtx *args);
int cli_file_line_end(const CliLineCtx *args);

// what a move came to: it failed, the file or folder moved, or it failed for the two paths being on different drives
#define CLI_MOVE_FAILED 0
#define CLI_MOVE_DONE 1
#define CLI_MOVE_ACROSS 2

// the file or folder at `from` moved to `to`, a file there replaced: what the move came to; and `size` bytes of
// `bytes` written at the end of the file at `path`, which is made anew, empty, first where `fresh` is 1 and must be
// there already where it is 0: 1, or 0 where they could not be (platform/windows_files.c, platform/posix_files.c)
int cli_move(const CliErrMsgFromCtx *args);
int cli_file_append(const CliPathBytesCtx *args);

// A file of this process's own that the system deletes as it is let go or the process ends, so that a crash leaves
// nothing behind: the system's handle to it
typedef struct
{
    unsigned long long handle;
} CliTemporaryFile;

// a temporary file made, empty, in the folder the system passes files in: 1, or 0 where it could not be; `size` bytes
// of `bytes` written at its byte `at`: 1, or 0 where they could not all be; `size` bytes from its byte `at` read into
// `bytes`: 1, or 0 where they could not all be; and the file let go, which deletes it (platform/windows_files.c,
// platform/posix_files.c)
int cli_temporary_file_init(const CliFileTemporaryFileCtx *args);
int cli_temporary_file_write(const CliBytesCtx *args);
int cli_temporary_file_read(const CliSizeCtx *args);
void cli_temporary_file_let_go(const CliFileTemporaryFileCtx *args);

// the system's clipboard: its text written into `text` as UTF-8, nothing where it holds none: 1, or 0 where it could
// not be read; `size` bytes of UTF-8 `text` put on it: 1, or 0 where it did not take them; the files `paths`, `count`
// of them, put on it as the file manager puts them, to be moved where they are pasted where `cut` and copied otherwise:
// 1, or 0 where it did not take them; each file it holds handed to `each`, 1 in `cut` where they were cut, 0 where they
// were copied and -1 where the clipboard does not say: their count; and the clipboard emptied
// (platform/windows_clipboard.c, platform/posix_clipboard.c)
// what the system's picker picks: a file to open, a folder, or a file to save
#define CLI_PICK_FILE 0u
#define CLI_PICK_FOLDER 1u
#define CLI_PICK_SAVE 2u

// the system's picker shown for the reader to pick `type`, waited on: the path chosen into `path`, sized for
// `capacity`, 1, or 0 where none was chosen (platform/windows_picker.c, platform/posix_picker.c)
int cli_file_pick(const CliPathCapacityCtx *args);

int cli_clipboard_text_read(const CliCtx *args);
int cli_clipboard_text_write(const CliSizeTextCtx *args);
int cli_clipboard_files_append(const CliCountCtx *args);
unsigned int cli_clipboard_files_read(const CliEachCtx *args);
void cli_clipboard_clear(void);
// a piece of a started process's output or errors read, 0 at its end; the process waited on, its exit code, or -1
// where a signal ended it; its job ended; its job and pipes let go; and the process group `leader` leads ended,
// requested first, or on Windows the process `id` and every process under it
unsigned long long cli_spawn_read(const CliCapacityPipeHandleCtx *args);
// the bytes waiting to be read from `pipe_handle`, which a read of no more than that many takes without waiting; 0
// where none wait or the pipe has ended
unsigned long long cli_spawn_waiting(const CliCapacityPipeHandleCtx *args);
// what `pipe_handle` writes read until it ends, line by line into `line`, sized for `capacity`, each line handed to
// `each` with `context` and its bytes, its end left off, a line longer than the capacity handed in pieces of the
// capacity's length, and what stands after the last end handed as it ends (spawn_lines.c)
void cli_spawn_lines_read(const CliSpawnLinesCtx *args);
long long cli_spawn_wait(const CliSpawnedCtx *args);
// whether a started process has ended, without waiting: 1 with its exit code into `code`, -1 where a signal ended it,
// or 0 where it goes on
int cli_spawn_poll(const CliSpawnedCtx *args);
// the system's PATH and the reader's as the registry stores them now, each into `out`, sized for `capacity`; a variable
// of the reader's own read, and set as text whose %NAME% parts expand; every open program notified the environment
// changed; and a text's %NAME% parts expanded. Each 1, or 0 where there is none or off Windows
// (platform/windows_registry.c, platform/posix_registry.c)
int cli_registry_machine_path(const CliOutCtx *args);
int cli_registry_user_value(const CliOutCapacityCtx *args);
int cli_registry_set_user_value(const CliErrMsgNameCtx *args);
void cli_registry_announce(void);
void cli_registry_expand(const CliOutCtx *args);
void cli_spawn_group_end(const CliGroupCtx *args);
void cli_spawn_close(const CliSpawnedCtx *args);
void cli_spawn_tree_end(const CliIdCtx *args);
// a started process ended at once, by SIGKILL on POSIX; and interrupted by SIGINT: 1, or 0 where it could not be, as
// on Windows, where no signal interrupts a process
void cli_spawn_kill(const CliColsCtx *args);
int cli_spawn_interrupt(const CliColsCtx *args);
// the folders of the PATH this process runs with, each into `folders`, sized for `capacity`: their count; and the PATH
// those folders, `count` of them, make, into `path`, sized for `size`
unsigned int cli_path_folders(const CliCapacityFoldersCtx *args);
void cli_path_join(const CliPathCountSizeCtx *args);

// What a thread runs: the function and its context, retained by its starter for as long as the thread lives, and the
// priority it runs at, one of CLI_PRIORITY_WORK and the rest, work where none is set
typedef struct
{
    void (*run)(void *context);
    void *context;
    unsigned int priority;
} CliThread;

// the lock, which the caller owns, given back while the thread waits on the wake, and taken again as it wakes; every
// thread waiting woken; and a thread started on `thread`, let go to end on its own: 1, or 0 where it could not start
void cli_wake_wait(const CliWakeCtx *args);
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

// the folder `root` and all under it watched by the system, each change handed to `emit` on a thread of the watch's
// own as it is made: 1, or 0 where the system cannot watch it, as off Windows; and the watch stopped, its thread
// waited for (platform/windows_watch.c, platform/posix_watch.c)
int cli_watch_system_start(const CliRootContextCtx *args);
void cli_watch_system_stop(void);
int cli_thread_start(const CliThreadCtx *args);
// the calling thread put at `priority`, one of CLI_PRIORITY_WORK and the rest; its id, which no other live thread has;
// and the processor given up to any thread waiting for it (platform/windows_threads.c, platform/posix_threads.c)
void cli_priority_own(const CliWordsErrMsgCtx *args);
embed_word cli_thread_id(void);
void cli_thread_yield(void);
// the processors the machine has, one at the fewest
unsigned int cli_processors(void);

// A connection over TCP: the system's handle of it, 0 for none
typedef struct
{
    unsigned long long handle;
} CliSocket;

// what a read that takes what came returns beside a count
#define CLI_SOCKET_CLOSED 0ll

#define CLI_SOCKET_WAITED (-2ll)

#define CLI_SOCKET_FAILED (-1ll)

// a connection made to `host`:`port`, its delay of small writes off; `size` bytes written whole; `size` bytes read
// whole, 0 where the peer closed or the wait ran out; the wait of a read set, 0 for none; the connection ended both
// ways, its handle retained, so that a thread reading it wakes to its end; and the connection let go. One thread may
// read a connection while another writes it
int cli_socket_connect(const CliErrMsgCtx *args);
int cli_socket_write(const CliBytesSizeCtx *args);
int cli_socket_read(const CliSizeBytesCtx *args);
// what came, at most `capacity` bytes, read into `bytes`: its count, CLI_SOCKET_CLOSED where the peer closed,
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
// `size` bytes of pages from the system's own page allocator, zero, one allocation a graphics device can read from
// where the system lets it: the pages, or NULL where it gives none; `size` bytes of pages reserved and none of them
// committed, on the same terms; `size` bytes of reserved pages at `bytes_out` committed, zero: 1, or 0 where the system
// refuses; and pages so taken or reserved at `bytes_out`, `size` of them, given back
void *cli_pages_alloc(const CliSizeBytesCtx *args);
void *cli_pages_reserve(const CliSizeBytesCtx *args);
int cli_pages_commit(const CliSizeBytesCtx *args);
void cli_pages_free(const CliSizeBytesCtx *args);
// a folder made with every folder above it, readable by the reader alone, and a file written whole, made anew,
// readable by the reader alone: each 1, or 0 where it could not be (platform/windows_system.c,
// platform/posix_system.c)
int cli_private_folder_init(const CliWordsErrMsgCtx *args);
int cli_private_file_write(const CliPathBytesCtx *args);
// each process of the machine handed to `each`: its number, its program's name, and the line it was started with where
// the system reports, else empty (platform/windows_processes.c, platform/posix_processes.c)
void cli_processes_each(const CliContextCtx *args);
// whether the process `pid` still runs, as the system reports: on Windows its exit code is still active, on Linux /proc
// lists it, and elsewhere, with no /proc to query, it is taken to run
int cli_process_running(const CliPidCtx *args);

// What a process holds in memory, in bytes: its working set, the memory only it uses that is in RAM, and its commit,
// the private memory it has reserved whether in RAM or paged out; and when it started, in the system's own ticks, 0
// where the system does not say. On Linux the working set is its private pages in RAM and the commit its anonymous
// pages in RAM and swapped out; on macOS both are its physical footprint, as the system counts it against its limit
typedef struct
{
    unsigned long long working;
    unsigned long long commit;
    unsigned long long started;
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
unsigned int cli_mounts(const CliCapacityNamesCtx *args);
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
    const CliSetting *settings;
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
    CliSpawned *spawned;
    const char *folder;
    const CliSetting *settings;
    unsigned int count;
    int detached;
    unsigned int priority;
    // a handle the child inherits beside its pipe ends, 0 for none. Only Windows reads it
    unsigned long long inherited_handle;
};

struct CliWordsErrMsgSpawnedCtx
{
    const char *const *words;
    VerbaLinea *err_msg;
    CliSpawned *spawned;
    const char *folder;
    const CliSetting *settings;
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
    const CliSetting *settings;
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

struct CliCapacityNamesCtx
{
    unsigned int capacity_u32;
    const char **names;
    const char **paths_read;
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
    void (*each)(void *context, char *line, unsigned long long used);
    void *context;
};

struct CliColsCtx
{
    unsigned int cols;
    unsigned int rows;
    const CliSpawned *spawned_read;
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
    CliSpawned *spawned;
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
    unsigned int letter;
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
} CliPathNs;
EMBED_TABLE_LAYOUT(CliPathNs, file_alloc, file_alloc_utf8, file_piece_alloc);
EMBED_TABLE_STORAGE CliPathNs cli_path_internal EMBED_UNUSED = {
    .file_alloc = cli_file_alloc,
    .file_alloc_utf8 = cli_file_alloc_utf8,
    .file_piece_alloc = cli_file_piece_alloc,
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
EMBED_TABLE_STORAGE CliLineLetterNs cli_line_letter_internal EMBED_UNUSED = {
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
    void (*wake_wait)(const CliWakeCtx *args);
    void (*wake_all)(const CliWakeCtx *args);
    unsigned int (*wake_read)(const CliWakeCtx *args);
    void (*wake_wait_seen)(const CliWakeCtx *args);
    int (*wake_wait_for)(const CliWakeCtx *args);
    int (*wake_wait_seen_for)(const CliWakeCtx *args);
} CliWakeNs;
EMBED_TABLE_LAYOUT(CliWakeNs, wake_wait, wake_all, wake_read, wake_wait_seen, wake_wait_for, wake_wait_seen_for);
EMBED_TABLE_STORAGE CliWakeNs cli_wake_internal EMBED_UNUSED = {
    .wake_wait = cli_wake_wait,
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
} CliColsNs;
EMBED_TABLE_LAYOUT(CliColsNs, spawn_terminal_resize, spawn_terminal_hung_up, spawn_kill, spawn_interrupt);
EMBED_TABLE_STORAGE CliColsNs cli_cols_internal EMBED_UNUSED = {
    .spawn_terminal_resize = cli_spawn_terminal_resize,
    .spawn_terminal_hung_up = cli_spawn_terminal_hung_up,
    .spawn_kill = cli_spawn_kill,
    .spawn_interrupt = cli_spawn_interrupt,
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
    void *(*pages_alloc)(const CliSizeBytesCtx *args);
    void *(*pages_reserve)(const CliSizeBytesCtx *args);
    int (*pages_commit)(const CliSizeBytesCtx *args);
    void (*pages_free)(const CliSizeBytesCtx *args);
} CliSizeBytesNs;
EMBED_TABLE_LAYOUT(CliSizeBytesNs, socket_read, random_bytes, pages_alloc, pages_reserve, pages_commit, pages_free);
EMBED_TABLE_STORAGE CliSizeBytesNs cli_size_bytes_internal EMBED_UNUSED = {
    .socket_read = cli_socket_read,
    .random_bytes = cli_random_bytes,
    .pages_alloc = cli_pages_alloc,
    .pages_reserve = cli_pages_reserve,
    .pages_commit = cli_pages_commit,
    .pages_free = cli_pages_free,
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
    unsigned int (*mounts)(const CliCapacityNamesCtx *args);
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
