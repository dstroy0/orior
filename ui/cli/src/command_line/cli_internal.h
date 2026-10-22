// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_internal.h: what the modules of the command line share: the query's platform calls it is built on, its own
// platform's calls (platform/windows.c, platform/posix.c), its bounds, and the functions one module reads from
// another
#ifndef CLI_INTERNAL_H
#define CLI_INTERNAL_H

#include "../../../../src/cu/includes/file_defs/ksg/ksg_writing.h"
#include "../../../query/src/query_internal.h"

// the longest path and the longest line the command line handles, as the query's
#define CLI_PATH_LONGEST QUERY_PATH_LONGEST

#define CLI_SAID_LONGEST QUERY_SAID_LONGEST

// orior's version
#define CLI_VERSION "0.1.0"

// the most processes the system lists, the most mounts a window holds, and the longest name of one
#define CLI_PROCESSES_MOST 65536u

#define CLI_MOUNTS_MOST 64u

#define CLI_NAME_LONGEST 256u

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
int cli_container_held(void);
int cli_started_outside(const char *const *words, unsigned int *id);
unsigned int cli_container_members(unsigned int *ids, unsigned int room);
unsigned int cli_processes_listed(CliProcess *processes, unsigned int room);
void cli_container_members_ended(void);
unsigned int cli_process_own(void);
int cli_program_own(char *path, size_t room);
// the folder this process stands in, into `path`
int cli_folder_current(char *path, size_t room);
// 1 where this process's errors go to a terminal, and where its output does (platform/windows.c, platform/posix.c)
int cli_errors_terminal(void);
int cli_output_terminal(void);
// the words this program was given after its name, at most `room` of them, into `words`, each UTF-8: on Windows as the
// system holds them, wide, and elsewhere the `count` words of `given`: their count
unsigned int cli_arguments(int count, char **given, const char **words, unsigned int room);
// the file at `path` made one its owner runs, on POSIX
int cli_file_runnable(const char *path);
// 1 where the file at `path` is there and is a file the system calls a link to elsewhere, never followed
int cli_is_link(const char *path);
// the milliseconds a clock that only goes forward reads
unsigned long long cli_clock_milliseconds(void);
// the size of the file at `path` into `size`: 1, or 0 where it is not there; and the milliseconds since 1970 the wall
// clock reads (platform/windows_files.c, platform/posix_files.c)
int cli_file_size(const char *path, unsigned long long *size);
// the size of the file at `path` and the milliseconds since 1970 it last changed at: 1, or 0 where it is not there
int cli_file_stamp(const char *path, unsigned long long *size, unsigned long long *changed);
// the whole file at `path` read in one opening: its size measured on the open file and handed to `room`, which gives
// back where the bytes go, or NULL to refuse; the bytes read into it, their count into `got`: 1, or 0 where the file
// could not be opened, measured or given room
int cli_file_read_whole(const char *path, unsigned char *(*room)(void *context, unsigned long long size), void *context,
                        unsigned long long *got);
// `count` bytes of the file at `path` from its byte `from` read into `bytes`, their count into `got`, fewer where the
// file ends first: 1, or 0 where the file could not be opened or read
int cli_file_read_at(const char *path, unsigned long long from, unsigned char *bytes, unsigned long long count,
                     unsigned long long *got);
// The bytes of a file in a cell of the cellblock, where every read of them stays: the file at `path` read whole, its
// size into `*size`, or `count` bytes of it from `from`, their count into `*size`, fewer where the file ends first; a
// word past the bytes is readable and zero, as the walks that load whole words over a text's last bytes take it.
// NULL where the file could not be read or the cells cannot meet it; a file of no bytes is bytes of none. And the
// bytes let go (machine/file_held.c)
const unsigned char *cli_file_held(const char *path, unsigned long long *size);
// the file at `path` held as cli_file_held holds it where its bytes read as UTF-8 whole: its bytes, or NULL, the
// cell given back, where a byte starts no letter (file_held.c)
const unsigned char *cli_file_held_utf8(const char *path, unsigned long long *size);
const unsigned char *cli_file_piece_held(const char *path, unsigned long long from, unsigned long long count,
                                         unsigned long long *size);
void cli_file_let_go(const unsigned char *bytes, unsigned long long size);
// the bytes of every file under `root`, the folders of version control and of cargo's build passed over: the ask a
// tree's pool is made for (machine/file_held.c)
unsigned long long cli_tree_bytes(const char *root);
// the system's number for why the call just made failed, read before any other call; and its reason written after
// `said`, as ": Access is denied. (os error 5)" (platform/windows_files.c, platform/posix_files.c)
unsigned int cli_system_error(void);
void cli_system_reason(unsigned int error, CliLine *said);
// `secret` kept for `account` of `service`, such as "database" or "plugin sonarqube", in the system's keychain, on
// this machine alone, under the service's name "orior " and `service`; the secret kept for `account` of `service` into
// `out`, which holds `room`: 1, or 0 where none is kept; and the secret taken out (platform/windows_keychain.c,
// platform/posix_keychain.c)
int cli_keychain_keep(const char *service, const char *account, const char *secret, CliLine *said);
int cli_keychain_read(const char *service, const char *account, char *out, unsigned long long room);
int cli_keychain_forget(const char *service, const char *account, CliLine *said);
unsigned long long cli_wall_milliseconds(void);
// one letter written in UTF-8 (words.c)
void cli_utf8_put(CliLine *line, unsigned int letter);
// the staged file `from` copied over `to` with the rights the system asks the reader for: on Windows orior's own
// program run again as `orior_cli write-raised`, raised; on Linux cp through pkexec; on macOS cp with the system's
// authorization. 1, or 0 with why said
int cli_raised_platform(const char *from, const char *to, CliLine *said);

// the most words of one command a program is run with, and the most bytes they take
#define CLI_WORDS_MOST 512u

#define CLI_WORDS_TEXT (1u << 16u)

// The words of one command a program is run with, NULL after the last, and the bytes they take; 1 in `full` where a
// word did not fit
typedef struct
{
    const char *word[CLI_WORDS_MOST + 1u];
    unsigned int count;
    char text[CLI_WORDS_TEXT];
    unsigned int used;
    int full;
} CliWords;

// `word` put after the words so far (words.c)
void cli_words_put(CliWords *words, const char *word);

// A lock one thread holds at a time: the system's slim reader-writer lock on Windows, its first and only word, and a
// POSIX mutex elsewhere. CLI_LOCK_INIT makes one free as it is declared
#if defined(_WIN32)
typedef struct
{
    void *slot;
} CliLock;

#define CLI_LOCK_INIT {NULL}
#else
#include <pthread.h>

typedef struct
{
    pthread_mutex_t mutex;
} CliLock;

#define CLI_LOCK_INIT {PTHREAD_MUTEX_INITIALIZER}
#endif

// the lock taken, the calling thread waiting until it is free; and given back (platform/windows_threads.c,
// platform/posix_threads.c)
void cli_lock_take(CliLock *lock);
void cli_lock_give(CliLock *lock);

// A wake threads wait on under a lock until another thread wakes them: the system's condition variable, one word on
// Windows. CLI_WAKE_INIT makes one as it is declared
#if defined(_WIN32)
typedef struct
{
    void *slot;
} CliWake;

#define CLI_WAKE_INIT {NULL}
#else
typedef struct
{
    pthread_cond_t condition;
} CliWake;

#define CLI_WAKE_INIT {PTHREAD_COND_INITIALIZER}
#endif

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
// was made the head of on Windows, 0 for none, and the pipe its standard input is fed by, 0 for none
typedef struct
{
    unsigned long long process;
    unsigned long long id;
    unsigned long long out;
    unsigned long long err;
    unsigned long long group;
    unsigned long long in;
} CliSpawned;

// a process started with `words`, the program first and NULL after the last, in `folder`, its environment this
// process's with `settings` after, `count` of them, its standard input nothing: where `detached`, with no console of
// its own and a job of its own on Windows, a process group of its own elsewhere. 1, or 0 with why said
// (platform/windows_spawn.c, platform/posix_spawn.c)
int cli_spawn(CliSpawned *spawned, const char *const *words, const char *folder, const CliSetting *settings,
              unsigned int count, int detached, CliLine *said);
// a process started as cli_spawn starts one, at `priority`, one of CLI_PRIORITY_WORK and the rest; cli_spawn starts
// one at work
int cli_spawn_as(unsigned int priority, CliSpawned *spawned, const char *const *words, const char *folder,
                 const CliSetting *settings, unsigned int count, int detached, CliLine *said);
// a process started as cli_spawn starts one, its standard input a pipe this process feeds through `in`, at work or at
// `priority`; `size` bytes written whole to that pipe: 1, or 0 where the process stopped reading; and the pipe closed,
// which the process reads as its input's end
int cli_spawn_fed(CliSpawned *spawned, const char *const *words, const char *folder, const CliSetting *settings,
                  unsigned int count, int detached, CliLine *said);
int cli_spawn_fed_as(unsigned int priority, CliSpawned *spawned, const char *const *words, const char *folder,
                     const CliSetting *settings, unsigned int count, int detached, CliLine *said);
int cli_spawn_written(unsigned long long pipe, const void *bytes, unsigned long long size);
// a process started as cli_spawn starts one, sharing this process's console, its input, output and errors this
// process's, and waited on: its exit code into `code`, -1 where a signal ended it, 1, or 0 with why it did not start
// said
int cli_spawn_shared(const char *const *words, const CliSetting *settings, unsigned int count, long long *code,
                     CliLine *said);
void cli_spawn_input_closed(CliSpawned *spawned);

// What a program run gave: what it wrote to its output and to its errors, each a cell the size of what came and
// ended by a 0, given back with the run, 0 in `whole` where a cell could not be taken and what came is not all kept,
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
} CliRan;

// what one program run gave taken from those kept, waiting while each is in use, and given back; and a program run
// as cli_spawn starts one, detached, its errors read on a thread of their own while its output is read, and waited on:
// what it gave into `ran`, 1, or 0 with why it did not start said (ran.c)
CliRan *cli_ran_taken(void);
void cli_ran_given(CliRan *ran);
int cli_ran(const char *const *words, const char *folder, const CliSetting *settings, unsigned int count, CliRan *ran,
            CliLine *said);
// a program run as cli_ran runs one, `length` bytes of `input` written to its standard input on a thread of their own
int cli_ran_fed(const char *const *words, const char *folder, const CliSetting *settings, unsigned int count,
                const char *input, unsigned long long length, CliRan *ran, CliLine *said);
// a program run as cli_ran_fed runs one, ended with every process under it where it runs past `milliseconds`
int cli_ran_within(const char *const *words, const char *folder, const CliSetting *settings, unsigned int count,
                   const char *input, unsigned long long length, unsigned long long milliseconds, CliRan *ran,
                   CliLine *said);

// `size` bytes of `data` added to the Windows program at `path` as the RCDATA resource `name`: 1, or 0 with why said,
// off Windows always (platform/windows_files.c, platform/posix_files.c)
int cli_resource_added(const char *path, const char *name, const void *data, unsigned long long size, CliLine *said);

// A piece of a file written: `size` bytes of `bytes`, or of zeros where `bytes` is NULL
typedef struct
{
    const void *bytes;
    unsigned long long size;
} CliFilePiece;

// the file at `path` written as `count` pieces in turn, any file there replaced whole or not at all: 1, or 0 where it
// could not be (file_writing.c)
int cli_file_pieces_written(const char *path, const CliFilePiece *pieces, unsigned int count);

// the bytes a file is written in: one view of the Windows cache manager, VACB_MAPPING_GRANULARITY in ntifs.h, written
// at an offset that is a whole number of them, each a whole number of 4 KiB pages
#define CLI_STAGE_BYTES (256u << 10u)

_Static_assert((CLI_STAGE_BYTES % 4096u) == 0u, "CLI_STAGE_BYTES is a whole number of 4 KiB pages");
_Static_assert(((256u << 10u) % CLI_STAGE_BYTES) == 0u, "CLI_STAGE_BYTES divides the cache manager's 256 KiB view");

// A file being written whole, and sealed where it is of a kind ksg seals (ksg_writing.h): written beside its place
// under a name of this process's own, made durable and put in its place, so a write that fails leaves the file it
// would have replaced as it was
typedef KsgWriting CliFileWriting;

_Static_assert(CLI_PATH_LONGEST <= KSG_PATH_LONGEST, "a path of the command line's fits a sealed writer's");

// the file `path` begun beside its place, with its seal where it is sealed: 1, or 0 where it could not be made; `size`
// bytes at `bytes` written after what the file begun holds, the shape of a line's flush: 1, or 0 where they were not;
// and the file begun on the disk and put in place of `path` where `whole` is 1, or taken away where it is 0: 1, or 0
// where it was not put in place (file_writing.c)
int cli_file_begun(CliFileWriting *file, const char *path);
int cli_file_put(void *context, const char *bytes, unsigned long long size);
int cli_file_ended(CliFileWriting *file, int whole);

// a line over `stage`, which holds CLI_STAGE_BYTES + 1, handing each whole stage to the file begun; and the line's
// last bytes handed to it, the file put in place where every byte went and taken away where one did not: 1, or 0
// where it was not put in place (file_writing.c)
CliLine cli_file_line(CliFileWriting *file, char *stage);
int cli_file_line_ended(CliFileWriting *file, CliLine *line);

// what a move came to: it failed, the file or folder moved, or it failed for the two paths being on different drives
#define CLI_MOVE_FAILED 0
#define CLI_MOVE_DONE 1
#define CLI_MOVE_ACROSS 2

// the file or folder at `from` moved to `to`, a file there replaced: what the move came to; and `size` bytes of
// `bytes` written at the end of the file at `path`, which is made anew, empty, first where `fresh` is 1 and must be
// there already where it is 0: 1, or 0 where they could not be (platform/windows_files.c, platform/posix_files.c)
int cli_moved(const char *from, const char *to);
int cli_file_appended(const char *path, const void *bytes, unsigned long long size, int fresh);
// a piece of a started process's output or errors read, 0 at its end; the process waited on, its exit code, or -1
// where a signal ended it; its job ended; its job and pipes let go; and the process group `leader` leads ended, asked
// first, or on Windows the process `id` and every process under it
unsigned long long cli_spawn_read(unsigned long long pipe, char *piece, unsigned long long room);
long long cli_spawn_wait(CliSpawned *spawned);
// whether a started process has ended, without waiting: 1 with its exit code into `code`, -1 where a signal ended it,
// or 0 where it goes on
int cli_spawn_poll(CliSpawned *spawned, long long *code);
// the system's PATH and the reader's as the registry holds them now, each into `out`, which holds `room`; a variable of
// the reader's own read, and set as text whose %NAME% parts expand; every open program told the environment changed;
// and a text's %NAME% parts expanded. Each 1, or 0 where there is none or off Windows (platform/windows_registry.c,
// platform/posix_registry.c)
int cli_registry_machine_path(char *out, unsigned long long room);
int cli_registry_user_value(const char *name, char *out, unsigned long long room);
int cli_registry_set_user_value(const char *name, const char *value, CliLine *said);
void cli_registry_announce(void);
void cli_registry_expand(const char *text, char *out, unsigned long long room);
void cli_spawn_group_end(unsigned long long group);
void cli_spawn_closed(CliSpawned *spawned);
void cli_spawn_tree_end(unsigned long long id);
// a started process ended at once, by SIGKILL on POSIX; and interrupted by SIGINT: 1, or 0 where it could not be, as
// on Windows, where no signal interrupts a process
void cli_spawn_killed(const CliSpawned *spawned);
int cli_spawn_interrupted(const CliSpawned *spawned);
// the folders of the PATH this process runs with, each into `folders`, which holds `room`: their count; and the PATH
// those folders, `count` of them, make, into `path`, which holds `size`
unsigned int cli_path_folders(char (*folders)[CLI_PATH_LONGEST], unsigned int room);
void cli_path_joined(const char (*folders)[CLI_PATH_LONGEST], unsigned int count, char *path, unsigned long long size);

// What a thread runs: the function and its context, kept by its starter for as long as the thread lives, and the
// priority it runs at, one of CLI_PRIORITY_WORK and the rest, work where none is set
typedef struct
{
    void (*run)(void *context);
    void *context;
    unsigned int priority;
} CliThread;

// the lock, which the caller holds, given back while the thread waits on the wake, and taken again as it wakes; every
// thread waiting woken; and a thread started on `thread`, let go to end on its own: 1, or 0 where it could not start
void cli_wake_wait(CliWake *wake, CliLock *lock);
void cli_wake_all(CliWake *wake);
// the same wait, given up once `milliseconds` have passed: 1 where it was woken, 0 where the time ran out
int cli_wake_wait_for(CliWake *wake, CliLock *lock, unsigned long long milliseconds);

// what happened to a path the system tells of: made or moved there, taken out or moved away, or written; and that the
// system's record of changes ran over, so any file may have changed
#define CLI_CHANGE_ADDED 0u
#define CLI_CHANGE_REMOVED 1u
#define CLI_CHANGE_MODIFIED 2u
#define CLI_CHANGE_LOST 3u

// a change the system tells of: its path from the tree's top with forward slashes, empty where changes were lost, and
// what happened to it
typedef void (*CliChangeTold)(void *context, const char *path, unsigned int kind);

// the folder `root` and all under it watched by the system, each change handed to `told` on a thread of the watch's
// own as it is made: 1, or 0 where the system cannot watch it, as off Windows; and the watch stopped, its thread
// waited for (platform/windows_watch.c, platform/posix_watch.c)
int cli_watch_system_start(const char *root, CliChangeTold told, void *context);
void cli_watch_system_stop(void);
int cli_thread_started(CliThread *thread);
// the calling thread put at `priority`, one of CLI_PRIORITY_WORK and the rest (platform/windows_threads.c,
// platform/posix_threads.c)
void cli_priority_own(unsigned int priority);
// the processors the machine has, one at the fewest
unsigned int cli_processors(void);

// A connection over TCP: the system's handle of it, 0 for none
typedef struct
{
    unsigned long long handle;
} CliSocket;

// what a read that takes what came says beside a count
#define CLI_SOCKET_CLOSED 0ll

#define CLI_SOCKET_WAITED (-2ll)

#define CLI_SOCKET_FAILED (-1ll)

// a connection made to `host`:`port`, its delay of small writes off; `size` bytes written whole; `size` bytes read
// whole, 0 where the peer closed or the wait ran out; the wait of a read set, 0 for none; the connection ended both
// ways, its handle kept, so that a thread reading it wakes to its end; and the connection let go. One thread may read a
// connection while another writes it
int cli_socket_connected(CliSocket *socket, const char *host, unsigned int port, CliLine *said);
int cli_socket_written(const CliSocket *socket, const void *bytes, unsigned long long size);
int cli_socket_read(const CliSocket *socket, void *bytes, unsigned long long size);
// what came, at most `room` bytes, read into `bytes`: its count, CLI_SOCKET_CLOSED where the peer closed,
// CLI_SOCKET_WAITED where the wait ran out, CLI_SOCKET_FAILED where the read failed
long long cli_socket_some(const CliSocket *socket, void *bytes, unsigned long long room);
void cli_socket_waited(const CliSocket *socket, unsigned int milliseconds);
// the wait of a write set, 0 for none
void cli_socket_write_waited(const CliSocket *socket, unsigned int milliseconds);
void cli_socket_ended(const CliSocket *socket);
void cli_socket_closed(CliSocket *socket);
// a port on the loopback no one holds now into `port`: 1, or 0 with why said (platform/windows.c, platform/posix.c)
int cli_port_free(unsigned int *port, CliLine *said);
// a connection made to the first address `host`:`port` names that takes it, each given up where it is not made
// within `milliseconds`, its delay of small writes off: 1, or 0 with why said; the same with the microseconds finding
// the addresses took into `resolved`; the microseconds a clock that only goes forward reads; a socket listening on a
// port of the loopback no one held, the port into `port`: 1, or 0 with why said; and the next connection it takes,
// waited for, into `peer`: 1, or 0 where it took none (platform/windows_connect.c, platform/posix_connect.c)
int cli_socket_connected_within(CliSocket *socket, const char *host, unsigned int port, unsigned int milliseconds,
                                CliLine *said);
int cli_socket_connected_timed(CliSocket *socket, const char *host, unsigned int port, unsigned int milliseconds,
                               unsigned long long *resolved, CliLine *said);
unsigned long long cli_clock_microseconds(void);
int cli_socket_listening(CliSocket *listener, unsigned int *port, CliLine *said);
int cli_socket_accepted(const CliSocket *listener, CliSocket *peer);

// `size` bytes of the system's own randomness into `bytes`: 1, or 0 where it gave none; the folder the system keeps
// passing files in into `folder`, which holds CLI_PATH_LONGEST, with no separator after it; an event a started process
// inherits, made unsignaled, its handle, 0 where none was made and off Windows; the event signaled: 1, or 0 where it
// could not be, and off Windows; and the event let go (platform/windows_system.c, platform/posix_system.c)
int cli_random_bytes(void *bytes, unsigned long long size);
// a folder made with every folder above it, readable by the reader alone, and a file written whole, made anew,
// readable by the reader alone: each 1, or 0 where it could not be (platform/windows_system.c,
// platform/posix_system.c)
int cli_private_folder_made(const char *folder);
int cli_private_file_written(const char *path, const void *bytes, unsigned long long size);
// each process of the machine handed to `each`: its number, its program's name, and the line it was started with where
// the system says, else empty (platform/windows_processes.c, platform/posix_processes.c)
void cli_processes_each(void (*each)(void *context, unsigned int pid, const char *name, const char *command),
                        void *context);
// whether the process `pid` still runs, as the system says: on Windows its exit code is still active, on Linux /proc
// lists it, and elsewhere, with no /proc to ask, it is taken to run
int cli_process_running(unsigned int pid);
void cli_temp_folder(char *folder);
unsigned long long cli_event_inheritable(void);
int cli_event_set(unsigned long long event);
void cli_event_closed(unsigned long long event);

// the tree (root.c): 1 where `folder` holds the engine's tree; the tree ORIOR_ROOT names, else the one found walking up
// from the working folder or the program; a folder mounted beside the tree under a name it is given; a mount taken
// away, or all; a path inside the tree or a mount, refused where it climbs out; a path written relative to the tree, a
// whole path kept as it is
int cli_holds_tree(const char *folder);
int cli_tree_at(const char *path, char *full, CliLine *said);
int cli_tree_found(char *tree);
int cli_mounted(const char *path, char *name, CliLine *said);
void cli_unmounted(const char *name);
void cli_unmounted_all(void);
unsigned int cli_mounts(const char **names, const char **paths, unsigned int room);
int cli_inside(const char *root, const char *relative, char *joined, CliLine *said);
void cli_full(const char *root, const char *relative, char *joined);
void cli_relative(const char *root, const char *path, char *relative);
// the folder above `path` into `out`, which holds CLI_PATH_LONGEST: 1, or 0 where `path` is a root or a bare name
// and has none
int cli_path_parent(const char *path, char *out);

// orior's own folder (home.c): the folder, its plugins, the reader's stylesheet, menus and languages; each made where
// it is not there; and a file or a folder opened as the system opens it
int cli_home_folder(char *folder);
int cli_home_plugins(char *folder);
int cli_home_user_css(char *path);
int cli_home_menus(char *path);
int cli_home_languages(char *folder);
int cli_home_ensure_menus(char *path, CliLine *said);
int cli_home_ensure_user_css(char *path, CliLine *said);
int cli_home_reveal(const char *path, CliLine *said);

#endif
