// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform.h: the boundary between a program and its system. Every call into the system goes through platform_windows.c
// or platform_posix.c of this folder, and through nothing else. The model above this boundary includes no header of the
// C library and no header of the system.
//
// A program runs in one order: the process, the resource acquisitions, the callers, and the ephemeral work last.
// platform_open is the process stage. It places the one pool of the program over pages that the system gives. Every
// cell of the program comes from that pool. It also positions the block of fixed parts beside the pool.
//
// A module takes its resources from the pool on the persistent tier or as movable cells. A caller gives its storage to
// the calls below it [BORROWS]. The ephemeral work is on the temporary tier or on the local stack, and it goes back
// first.
//
// This header takes plain arguments alone, and a C++ unit includes it as it is. The guard of the pool and the owners
// are in platform_pool.h, for the C model above the boundary.
#ifndef PLATFORM_H
#define PLATFORM_H
#include "platform_places.h"
#include "verba_scribo/verba_linea.h"

EMBED_BEGIN_DECLS

// the place of each owner in the list of platform_places.h, the name that a C++ unit passes in place of the owner
#define PLATFORM_OWNER_PLACE(name_, text_) platform_place_##name_,
enum
{
    PLATFORM_OWNERS(PLATFORM_OWNER_PLACE) platform_place_count
};

// the fewest bytes that the pool holds. A request for fewer gets this many
#define PLATFORM_POOL_BYTES_LEAST (64ULL << 20u)

// The program above the boundary. The platform owns main (platform_process.c): main runs the process stage, then this,
// then the end of the stage. A program defines program_main and no main. Its answer is the exit code of the process
int program_main(int argc, char **argv);

// the bytes of the pool that the program asks for. A program that needs more than PLATFORM_POOL_BYTES_LEAST defines
// this constant, and every other program takes the default of the platform
extern const unsigned long long g_program_pool_bytes;

// the process stage, which main runs. The pool of `bytes`, at least
// PLATFORM_POOL_BYTES_LEAST, goes over pages from the system, with the regions of the temporary tier beside it. The
// movable option and the parts of the block take their places after it. 1, or 0 where
// the system gives no pages. The first call places the pool, and a later call answers 1 and changes nothing
int platform_open(unsigned long long bytes);

// the end of the process stage, which main runs after program_main returns: the mover ended and the pages given back
void platform_close(void);

// 1 from platform_open to platform_close, while the pool is there. A destructor of a static runs after main returns,
// and it reads no cell where this is 0, since the cells went back with the pages
int platform_pool_placed(void);

// `length` bytes of `text` written to the output stream of the process, or to its error stream where `errors` is 1
void platform_terminal_write(int errors, const char *text, unsigned long long length);

// 1 where the output stream of the process, or its error stream where `errors` is 1, is a terminal, and 0 where it is a
// pipe or a file
int platform_terminal_is(int errors);

// the cap of a text call of MMgr that stops at the terminator alone. The text calls count the cap and never add it to a
// pointer. A cap of all ones bounds nothing
#define PLATFORM_TEXT_TO_TERMINATOR (~(size_t)0)

// the line of the output stream: a line over the part of the block that platform_open positions. Its flush writes
// what it holds to the output stream, and so the line never fills. platform_line_out answers the same line on each call
VerbaLinea *platform_line_out(void);

// what the line of the output stream holds, written to the output stream, and the line emptied
void platform_line_flushed(void);

// the end of one write to the line of the output stream. Where the output stream is a terminal, what the line holds
// goes out, as a reader watches it. Where the stream is a pipe or a file, the line keeps it until it fills or the
// process ends
void platform_line_out_ended(void);

// Each later write to the line of the output stream goes out at its end, on a pipe or a file as on a terminal. A
// program whose reader follows its output as it runs asks for this, as setvbuf of stdout asks the C library
void platform_line_out_every_write(void);

// `length` bytes of `bytes` written to the line of the output stream as one write, which platform_line_out_ended
// ends. The answer is the count of bytes the line took, `length` where every byte went
unsigned long long platform_line_out_put(const char *bytes, unsigned long long length);

// ---- cells ----

// A cell of `bytes` from the pool, counted to the owner at `place`. An ephemeral cell is on the temporary tier and goes
// back before the work that took it ends. Any other cell is a resource on the persistent tier. NULL where the pool
// cannot meet the bytes [RETURNS OWNERSHIP]
void *platform_cell_taken(unsigned long long bytes, unsigned int place, int ephemeral);

// a cell given back to the pool by the owner that took it. Nothing goes back where the cell is NULL or after
// platform_close [TAKES OWNERSHIP]
void platform_cell_released(void *cell, unsigned int place);

// a cell of `count` elements of `size` bytes each, every byte 0, as calloc gives one. NULL where the product of `count`
// and `size` passes an unsigned long long, or where the pool cannot meet the cell
void *platform_cell_cleared(unsigned long long count, unsigned long long size, unsigned int place, int ephemeral);

// A cell grown to `bytes`. A new cell of that size comes from the same tier. The first `held` bytes of the old cell go
// into it, and the old cell goes back. NULL where the pool cannot meet the bytes, and the old cell is then as it was
// [TAKES OWNERSHIP] [RETURNS OWNERSHIP]
void *platform_cell_grown(void *cell, unsigned long long held, unsigned long long bytes, unsigned int place,
                          int ephemeral);

// ---- a movable cell ----

// A movable cell of `bytes` from the pool, counted to the owner at `place`. It is a resource that the program keeps,
// and a move of the pool can move it, which leaves no hole behind. The caller reads and writes it through the calls
// below, by its handle alone. NULL where the pool cannot meet the bytes [RETURNS OWNERSHIP]
const void *platform_movable_taken(unsigned long long bytes, unsigned int place);

// `count` bytes of `from` written into the movable cell at `offset`: 1, or 0 where the span passes the cell
int platform_movable_written(const void *cell, unsigned long long offset, const void *from, unsigned long long count);

// `count` bytes of the movable cell at `offset` read into `into`: 1, or 0 where the span passes the cell
int platform_movable_read(const void *cell, unsigned long long offset, void *into, unsigned long long count);

// 1 where the `count` bytes of the movable cell at `offset` are the `count` bytes at `bytes`
int platform_movable_same(const void *cell, unsigned long long offset, const void *bytes, unsigned long long count);

// a movable cell given back by the owner that took it. Nothing goes back where the cell is NULL or after
// platform_close [TAKES OWNERSHIP]
void platform_movable_released(const void *cell, unsigned int place);

// ---- a text that grows ----

// the tiers of a text's cell: the persistent tier, the temporary tier, and a movable cell
#define PLATFORM_TIER_PERSISTENT 0
#define PLATFORM_TIER_TEMPORARY 1
#define PLATFORM_TIER_MOVABLE 2

// the bytes that the line of a movable text holds before each flush
#define PLATFORM_TEXT_STAGE_BYTES 1024u

// A text in one cell of the pool, written through `line` with the entries of verba_linea.h. The cell starts small, and
// the flush of the line doubles it toward `max` bytes as the writes come. Past `max` the line fills, as a line with no
// flush fills. On the persistent or the temporary tier the text is whole in `cell`, terminated.
//
// A text on the movable tier holds its bytes in a movable cell, which a move of the pool can move. Its line writes into
// `stage`, and each flush puts the stage into the cell after the bytes before it. A grown cell is a new movable cell,
// and the old cell goes back. A text that grows to megabytes takes this tier, because its old cells leave no holes.
// `movable` names that cell, and `view` is the last copy that platform_text_bytes gave
typedef struct
{
    char *cell; // The cell of the text, NULL on the movable tier [BORROWS].
    const void *movable;
    char *view;
    unsigned long long capacity; // The bytes of the cell.
    unsigned long long max;      // The bytes that the cell grows to at max.
    unsigned int place;          // The place of the owner of the cell.
    int tier;                    // PLATFORM_TIER_PERSISTENT, PLATFORM_TIER_TEMPORARY or PLATFORM_TIER_MOVABLE.
    VerbaLinea line;             // The line that the writes go through.
    char stage[PLATFORM_TEXT_STAGE_BYTES];
} PlatformText;

// the text begun on `tier`, empty, before its first write
void platform_text_begun(PlatformText *text, unsigned long long max, unsigned int place, int tier);

// the bytes that the text holds, the terminator not among them
unsigned long long platform_text_size(const PlatformText *text);

// The bytes of the text, terminated [BORROWS]. For a movable text they are a copy in a cell of the temporary tier.
// The text holds that copy until its next read, reset, cut or end. NULL where the pool cannot meet the copy
const char *platform_text_bytes(PlatformText *text);

// the text emptied, with its cell kept at the size that it grew to
void platform_text_reset(PlatformText *text);

// the text cut to its first `size` bytes, where it holds more, the next write after them
void platform_text_cut(PlatformText *text, unsigned long long size);

// the cell of the text given back, after its last read
void platform_text_ended(PlatformText *text);

// ---- an array that grows ----

// `count` elements of `element` bytes each, in one cell of the pool. The room doubles as the count grows,
// and each element past the old count starts at zero. The elements are whole in `cell`, in their order
typedef struct
{
    void *cell;                 // The cell of the elements [BORROWS].
    unsigned long long count;   // The elements that the array holds.
    unsigned long long room;    // The elements that the cell holds.
    unsigned long long element; // The bytes of one element.
    unsigned int place;         // The place of the owner of the cell.
    int ephemeral;              // One where the cell is on the temporary tier.
} PlatformArray;

// the array begun, empty, with no cell until its first element
void platform_array_begun(PlatformArray *array, unsigned long long element, unsigned int place, int ephemeral);

// The array grown or cut to `count` elements, each new element zero. 1, or 0 where the pool cannot meet the room, and
// the array is then as it was
int platform_array_counted(PlatformArray *array, unsigned long long count);

// The element at `value`, `element` bytes of it, put after the last. 1, or 0 where the pool cannot meet the room
int platform_array_appended(PlatformArray *array, const void *value);

// the element at `at`, or NULL past the count [BORROWS]
void *platform_array_at(const PlatformArray *array, unsigned long long at);

// the cell of the array given back, after its last read, and the array empty
void platform_array_ended(PlatformArray *array);

// ---- the system ----

// The value of the environment variable `name` into `out` of `capacity` bytes, terminated. The answer is its length,
// or 0 where the process has no such variable, where its value is empty, or where the value does not fit
unsigned long long platform_environment(const char *name, char *out, unsigned long long capacity);

// The environment variable `name` set to `value`, for this process and the programs that it runs from now on. The
// answer is 1, or 0 where the system refused
int platform_environment_set(const char *name, const char *value);

// a handler of a signal, which hears the number of the signal
typedef void (*PlatformSignalHandler)(int number);

// The signal `number` heard by `handler` from now on, as signal sets it. SIG_IGN ignores the signal, and SIG_DFL
// gives it back to the system. The answer is the handler before, as signal answers it
PlatformSignalHandler platform_signal_handled(int number, PlatformSignalHandler handler);

// the bytes of the buffer that a value of the environment goes into, and of one that a search path goes into
#define PLATFORM_ENVIRONMENT_BYTES 4096u
#define PLATFORM_ENVIRONMENT_PATH_BYTES 32768u

// the bytes of a buffer that one path of a file goes into, its terminator among them
#define PLATFORM_PATH_BYTES 4096u

// the separator that this system writes between the folders of a path, as a text of one byte
extern const char g_platform_separator[];

// the byte that this system writes between the folders of a list of folders, as the PATH holds them
extern const char g_platform_list_separator;

// the size of the file at `path` into `bytes`: 1, or 0 where the system cannot open the file
int platform_file_size(const char *path, unsigned long long *bytes);

// The bytes of the file at `path` into `out`, at max `capacity` of them, with their count in `bytes`. 1 where the whole
// file fit, 0 where the system cannot read it or it does not fit
int platform_file_read(const char *path, void *out, unsigned long long capacity, unsigned long long *bytes);

// `count` bytes of `data` written to the file at `path`, which the call makes or replaces: 1 where every byte went
int platform_file_written(const char *path, const void *data, unsigned long long count);

// `count` bytes of `data` written after the end of the file at `path`, which is there already. The return is 1 where
// every byte went, and 0 where no file is there or a write failed
int platform_file_appended(const char *path, const void *data, unsigned long long count);

// the answers of platform_file_relocated: the file moved, the system refused, and the two paths on two volumes
#define PLATFORM_MOVE_FAILED 0
#define PLATFORM_MOVE_DONE 1
#define PLATFORM_MOVE_ACROSS 2

// The file or the folder at `from` moved to `to`, in place of any file there. The answer is PLATFORM_MOVE_DONE where
// the move succeeds. It is PLATFORM_MOVE_ACROSS where the two paths are on two volumes and the system moves no file
// between them. Where the system refused for any other reason, PLATFORM_MOVE_FAILED is the answer
int platform_file_relocated(const char *from, const char *to);

// ---- a file as a stream ----

// the bytes a file holds between the program and the system
#define PLATFORM_FILE_BUFFER_BYTES 16384u

// the answer of a write or a read of a file that did not go, as EOF is the C library's
#define PLATFORM_FILE_END (-1)

// A file open as a stream. A file written holds a line of MMgr over its buffer, and the line passes the buffer to the
// system as it fills. A file read holds the bytes it read last in the same buffer. A text file has a mode with no b.
// On Windows it writes a line's end as two bytes and reads the two back as one, as the C library does there
typedef struct
{
    VerbaLinea line;           // The line of a file written.
    unsigned long long handle; // The file of the system.
    int writing;               // One for a file written, and 0 for a file read.
    int text;                  // One for a text file on Windows.

    int failed;   // One once a write or a read of the system failed.
    int ended;    // One once a read met the end of the file.
    size_t held;  // The bytes of the buffer that a read holds.
    size_t taken; // The bytes of those that the program took.
    char buffer[PLATFORM_FILE_BUFFER_BYTES];
} PlatformFile;

// The file at `path` open in `mode`. An r reads it, a w writes it made new, and an a writes after its end. A b after
// any of them reads or writes its bytes as they are. The answer is the file, a cell of the pool. It is NULL where the
// mode is none of these or the system refused
PlatformFile *platform_file_open(const char *path, const char *mode);

// What a file written holds goes to the system, and then the file closes and its cell goes back. The answer is 0, or
// PLATFORM_FILE_END where a write or the close failed. NULL is no file and answers PLATFORM_FILE_END
int platform_file_close(PlatformFile *file);

// the line of a file written, which a program writes the file through [BORROWS]
VerbaLinea *platform_file_line(PlatformFile *file);

// `count` items of `size` bytes from `bytes` written to the file: the count of whole items that went, as fwrite answers
size_t platform_file_write_items(const void *bytes, size_t size, size_t count, PlatformFile *file);

// at max `count` items of `size` bytes read into `out`: the count of whole items read, as fread answers
size_t platform_file_read_items(void *out, size_t size, size_t count, PlatformFile *file);

// the next byte of the file read, as unsigned, or PLATFORM_FILE_END at its end or where the read failed, as fgetc
// answers
int platform_file_char_get(PlatformFile *file);

// the bytes that the file took so far, for platform_file_written_since
unsigned long long platform_file_mark(PlatformFile *file);

// The bytes that the file took since `mark`, as fprintf answers for the writes between. The answer is -1 where a write
// of the file failed
int platform_file_written_since(PlatformFile *file, unsigned long long mark);

// a terminated text written to the file: 0, or PLATFORM_FILE_END where it did not go, as fputs answers
int platform_file_text_put(const char *text, PlatformFile *file);

// one byte written to the file: the byte, or PLATFORM_FILE_END where it did not go, as fputc answers
int platform_file_char_put(int character, PlatformFile *file);

// Bytes read into `out` up to a line's end, which it keeps, or `capacity` less one of them, and a terminator after
// them. The answer is `out`, or NULL where the read met the end of the file or failed before any byte, as fgets
// answers
char *platform_file_text_read(char *out, int capacity, PlatformFile *file);

// what a file written holds passed to the system: 0, or PLATFORM_FILE_END where it did not go, as fflush answers
int platform_file_flush(PlatformFile *file);

// the three places that a move of a file counts from: the start, the place now and the end. SEEK_SET, SEEK_CUR and
// SEEK_END name the same three
#define PLATFORM_FILE_FROM_START 0
#define PLATFORM_FILE_FROM_HERE 1
#define PLATFORM_FILE_FROM_END 2

// The place of the file moved to `offset`, as fseek moves it. A `whence` of PLATFORM_FILE_FROM_START counts from the
// start, PLATFORM_FILE_FROM_HERE from the place now and PLATFORM_FILE_FROM_END from the end. The answer is 0, or -1
// where the system refused. The end of the file is no longer met after a move
int platform_file_seek(PlatformFile *file, long long offset, int whence);

// the place of the file, as ftell answers it: the bytes before the next write or read, or -1 where the system refused
long long platform_file_tell(PlatformFile *file);

// 1 once a write or a read of the file failed, as ferror answers
int platform_file_failed(const PlatformFile *file);

// 1 once a read met the end of the file, as feof answers
int platform_file_ended(const PlatformFile *file);

// the file at `path` removed: 1, or 0 where the system refused or no file was there
int platform_file_removed(const char *path);

// the folder at `path` made: 1 where it is there after the call, made now or before
int platform_folder_made(const char *path);

// 1 where `path` names a folder
int platform_folder_is(const char *path);

// Each folder along `path` made where it is not there, the last one `path` itself. `mode` holds the permission bits of
// POSIX that each folder made takes, which the umask narrows. A folder made on Windows takes the rights of the folder
// above it. 1 where the last folder is there after the call
int platform_folders_made(const char *path, unsigned int mode);

// A call that hears one entry of a folder: its name, terminated, and 1 where the entry is a folder itself
typedef void (*PlatformFolderEntry)(void *context, const char *name, int folder);

// Each entry of the folder at `path` heard by `each` with `context`, in the order that the system gives them, with
// "." and ".." left out. The answer is 1, or 0 where `path` is no folder that the system opens
int platform_folder_listed(const char *path, PlatformFolderEntry each, void *context);

// the number that the system gives the process
unsigned long long platform_process_id(void);

// the monotonic clock of the system in nanoseconds, from a start that the system chooses
unsigned long long platform_clock_ns(void);

// the clock of the calendar: the whole seconds since the start of 1970, UTC, as time answers them
unsigned long long platform_epoch_seconds(void);

// the clock of the calendar in microseconds since the start of 1970, UTC
unsigned long long platform_epoch_microseconds(void);

// the processors that the system runs threads on, 1 at the fewest
unsigned int platform_processors(void);

// The thread that calls asleep for `milliseconds`, at the least. A pause of 0 gives the processor up to any thread
// that waits for it, and the thread runs again at once where none waits
void platform_paused(unsigned long long milliseconds);

// the bytes that the system's handle of a worker takes at max
#define PLATFORM_WORKER_HANDLE_BYTES 16u

// A thread of the system that the program starts and then waits for. The caller holds the worker from its start to
// its wait, and the thread reads it in that time
typedef struct
{
    void (*run)(void *context);                                        // The call that the thread runs.
    void *context;                                                     // What the call reads [BORROWS].
    int running;                                                       // One from the start to the wait.
    EMBED_ALIGN(8) unsigned char handle[PLATFORM_WORKER_HANDLE_BYTES]; // The handle of the system.
} PlatformWorker;

// The worker started on run(context). The answer is 1, or 0 where the system refused the thread
int platform_worker_started(PlatformWorker *worker, void (*run)(void *context), void *context);

// The wait for the worker to end, and its handle given back. A worker that does not run is no wait
void platform_worker_joined(PlatformWorker *worker);

// the hooks that platform_close runs most allowed
#define PLATFORM_CLOSE_HOOKS 32u

// A hook that platform_close runs before it gives the pool back, as atexit runs one. The hooks run in the reverse
// order of the calls. The answer is 1, or 0 where PLATFORM_CLOSE_HOOKS are there already or the hook is NULL
int platform_at_close(void (*hook)(void));

// The process ended with `code` as its exit code, as exit ends it. platform_close runs first, with its hooks. The
// line goes out and the pool goes back
void platform_process_ended(int code);

// The library at `name` loaded, by the search of the system where the name holds no folder. The handle, or NULL where
// the system cannot load it
void *platform_library_opened(const char *name);

// the address of the symbol `name` in a loaded library, or NULL where the library holds no such symbol
void *platform_library_symbol(void *library, const char *name);

// a loaded library given back to the system
void platform_library_closed(void *library);

// The program `arguments[0]` run with the arguments after it, up to the NULL that ends them. Its output and its errors
// go to the file at `log`, which the call makes or replaces. The answer is its exit code, or -1 where it did not start
long platform_program_run(const char *const *arguments, const char *log);

// The program `arguments[0]` run with the arguments after it, as platform_program_run runs it, in the folder `folder`,
// or in the folder of this process where `folder` is NULL. Its output goes to the file at `output`, and its errors to
// the file at `errors`, or to `output` too where `errors` is NULL. The call makes or replaces each file, or writes
// after the end of each where `append` is 1. A path of a file counts from the folder of this process. The answer is
// the exit code of the program, or -1 where it did not start
long platform_program_run_in(const char *const *arguments, const char *folder, const char *output, const char *errors,
                             int append);

// ---- a program started and held ----

// the levels that a thread or a program runs at. WORK is the default level. RUN is the level of a run that the reader
// waits on. GUEST is the level of a guest that runs below the work of the reader. IDLE is the level of work that runs
// only where nothing else asks to run
#define PLATFORM_PRIORITY_WORK 0u
#define PLATFORM_PRIORITY_RUN 1u
#define PLATFORM_PRIORITY_GUEST 2u
#define PLATFORM_PRIORITY_IDLE 3u

// a setting of the environment of a program started: `name` set to `value`, or taken out where `value` is NULL
typedef struct
{
    const char *name;  // The name of the variable [BORROWS].
    const char *value; // Its value, or NULL to take it out [BORROWS].
} PlatformSetting;

// A program started and held. `process` is the handle of its process that the system gives, and `id` its number. `out`,
// `err` and `in` are its output, its errors and its input, each a pipe end of this process, or 0 for none. `group` is
// the group that it heads, or 0: a job on Windows, or the pseudo-console of a terminal. `waited` is 1 once a wait read
// its end. `held` holds a suspended program until platform_program_resumed: its first thread on Windows, and the pipe
// that it waits on elsewhere
typedef struct
{
    unsigned long long process;
    unsigned long long id;
    unsigned long long out;
    unsigned long long err;
    unsigned long long group;
    unsigned long long in;
    unsigned long long waited;
    unsigned long long held;
} PlatformProgram;

// Where the output and the errors of a program started go:
// - APART: each on a pipe of its own, `out` and `err`.
// - JOINED: both on the one pipe `out`.
// - ALONE: the output on `out`, and the errors to nothing.
// - NONE: both to nothing.
// - OWN: both to the output and the errors of this process.
// - FILE: both to the file at `output_path`, which the start makes or replaces.
#define PLATFORM_OUTPUT_APART 0u
#define PLATFORM_OUTPUT_JOINED 1u
#define PLATFORM_OUTPUT_ALONE 2u
#define PLATFORM_OUTPUT_NONE 3u
#define PLATFORM_OUTPUT_OWN 4u
#define PLATFORM_OUTPUT_FILE 5u

// Why a start failed. The stage is PLATFORM_REFUSED_ENDS where the start could not make a pipe, the null device or the
// output file. It is PLATFORM_REFUSED_START where the system refused the program. `code` is the code that the system
// gave: an error of Windows, or an errno
#define PLATFORM_REFUSED_ENDS 1u
#define PLATFORM_REFUSED_START 2u

typedef struct
{
    unsigned int stage;
    unsigned long long code;
} PlatformRefusal;

// what one start of a program asks for
typedef struct
{
    const char *const *words;            // The program first, and NULL after the last word [BORROWS].
    const char *folder;                  // The folder that it runs in, or NULL for the folder of this process.
    const PlatformSetting *settings;     // The changes of its environment, after the environment of this process.
    unsigned int setting_count;          // The count of the settings.
    unsigned int priority;               // PLATFORM_PRIORITY_WORK and the rest.
    unsigned int output;                 // PLATFORM_OUTPUT_APART and the rest.
    const char *output_path;             // The file of PLATFORM_OUTPUT_FILE [BORROWS].
    int detached;                        // A job of its own on Windows, a process group of its own elsewhere.
    int windowless;                      // No console of its own on Windows, as a detached program has none.
    int outlives;                        // It outlives this process: out of every job, or in a group of its own.
    int input_piped;                     // Its input a pipe that `in` feeds, else nothing.
    int input_own;                       // Its input the input of this process, where `input_piped` is 0.
    int suspended;                       // It waits before it runs until platform_program_resumed.
    int found_on_path;                   // Its program found along PATH first on Windows, as a shell finds it.
    int fault_silent;                    // A fault ends it with no dialog of Windows to wait on.
    unsigned long long processor_mask;   // The first 64 processors that it runs on, or 0 for every processor.
    unsigned long long inherited_handle; // One more handle of this process that it takes on Windows, or 0.
    VerbaLinea *err_msg;                 // Why a start failed.
    PlatformRefusal *refusal;            // Why a start failed, as the system gave it, or NULL.
} PlatformStart;

// The program of `start` started, its output and its errors where `output` sends them. Where `detached`, it heads a job
// of its own on Windows with no console, and a process group of its own elsewhere. The job ends every process in it as
// this process ends. Where `outlives`, it leaves the job of this process on Windows and heads no job, and elsewhere it
// heads a process group of its own. A processor mask puts the program on those processors, and on Windows in a job of
// its own that counts its processor time. Where the caller gives `refusal` and `suspended` is 0, a program that the
// system cannot run is a failed start on every system.
//
// The return is 1, or 0 with the reason given
int platform_program_started(const PlatformStart *start, PlatformProgram *program);

// a suspended program let run, and `held` let go
void platform_program_resumed(PlatformProgram *program);

// How a program ended. Where `signaled` is 1, a signal ended it, and `code` is the number of the signal. Else `code`
// is its exit code, which on Windows is the status of the fault that ended it where a fault did
typedef struct
{
    int signaled;
    unsigned long long code;
} PlatformEnding;

// the wait of platform_program_ended_within that has no bound
#define PLATFORM_WAIT_FOREVER (~0ULL)

// 1 when `program` ends within `microseconds` at most, and 0 where it runs on after them. Its end stays for
// platform_program_ending to read. Thus a read of its processor time still counts it
int platform_program_ended_within(const PlatformProgram *program, unsigned long long microseconds);

// the end of `program`, waited for and read into `ending`: 1, or 0 where the wait failed
int platform_program_ending(PlatformProgram *program, PlatformEnding *ending);

// every process of the job or the group of `program` ended at once, and the program with them
void platform_program_group_killed(const PlatformProgram *program);

// Every process of the tree of `program` asked to end, and each that remains after `grace` microseconds ended by force.
// Windows has no request that a console program heeds, and there the job, or the process where it heads none, ends at
// once with `code`. The count ended by force
unsigned long long platform_program_tree_stopped(PlatformProgram *program, unsigned long long grace, unsigned int code);

// The processor time that `program` and every process under it used, in microseconds: 1, or 0 where the system cannot
// give it.
// Windows reads the job of the program, and Linux reads /proc
int platform_program_processor_time(const PlatformProgram *program, unsigned long long *microseconds);

// At max `capacity` bytes that wait in the pipe end `handle` read into `piece`, with no wait: their count. `ended` is
// 1 once the pipe is at its end
unsigned long long platform_program_read_ready(unsigned long long handle, void *piece, unsigned long long capacity,
                                               int *ended);

// The program of `start` started in a pseudo-terminal of its own, `cols` wide and `rows` tall, a terminal's shell. Its
// output and its errors come from `out`, and the keys typed go to `in`. 1, or 0 with why given
int platform_program_terminal(const PlatformStart *start, unsigned int cols, unsigned int rows,
                              PlatformProgram *program);

// The pseudo-terminal of `program` given a new size, which the shell and the programs in it hear of: 1, or 0 where it
// could not be. The shell hung up on, as a terminal closed ends it. The pseudo-terminal let go, after which a reader of
// `out` meets its end, before platform_program_closed lets the rest go
int platform_program_terminal_sized(const PlatformProgram *program, unsigned int cols, unsigned int rows);
void platform_program_terminal_hung_up(const PlatformProgram *program);
void platform_program_terminal_closed(PlatformProgram *program);

// The program of `start` run on the console of this process, with the input, output and errors of this process. The
// call waits for it and puts its exit code in `code`, -1 where a signal ended it. The return is 1, or 0 with the reason
// that it did not start
int platform_program_shared(const PlatformStart *start, long long *code);

// `size` bytes of `bytes` written whole to the pipe end `handle`: 1, or 0 where the program stopped reading
int platform_program_written(unsigned long long handle, const void *bytes, unsigned long long size);

// the input of `program` closed, which the program reads as the end of its input
void platform_program_input_closed(PlatformProgram *program);

// at max `capacity` bytes read from the pipe end `handle` into `piece`: their count, and 0 at the end of the pipe
unsigned long long platform_program_read(unsigned long long handle, void *piece, unsigned long long capacity);

// The bytes that wait in the pipe end `handle`, which a read takes with no wait. On macOS a pseudo-terminal counts no
// bytes that wait, and the call answers a large count where a read returns at once
unsigned long long platform_program_waiting(unsigned long long handle);

// the end of `program` waited for: its exit code, or -1 where a signal ended it
long long platform_program_waited(PlatformProgram *program);

// 1 with the exit code of `program` in `code` where it ended, -1 where a signal ended it, and 0 where it runs on
int platform_program_polled(PlatformProgram *program, long long *code);

// every process of the group `group` ended, on a system with groups that a handle holds
void platform_program_group_ended(unsigned long long group);

// every handle of `program` let go and the record emptied. The call waits for a program that it did not wait for yet,
// once that program ends
void platform_program_closed(PlatformProgram *program);

// `program` ended at once
void platform_program_killed(const PlatformProgram *program);

// `program` asked to stop, as a key that interrupts it asks: 1, or 0 where the system has no such request
int platform_program_interrupted(const PlatformProgram *program);

// the process `id` and every process under it asked to end
void platform_program_tree_ended(unsigned long long id);

EMBED_END_DECLS

#endif
