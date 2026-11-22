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

// ---- a text that grows ----

// A text in one cell of the pool, written through `line` with the entries of verba_linea.h. The cell starts small, and
// the flush of the line doubles it toward `max` bytes as the writes come. Past `max` the line fills, as a line with no
// flush fills. The text is whole in `cell`, terminated
typedef struct
{
    char *cell;                  // The cell of the text [BORROWS].
    unsigned long long capacity; // The bytes of the cell.
    unsigned long long max;      // The bytes that the cell grows to at max.
    unsigned int place;          // The place of the owner of the cell.
    int ephemeral;               // One where the cell is on the temporary tier.
    VerbaLinea line;             // The line that the writes go through.
} PlatformText;

// the text begun, empty, before its first write
void platform_text_begun(PlatformText *text, unsigned long long max, unsigned int place, int ephemeral);

// the bytes that the text holds, the terminator not among them
unsigned long long platform_text_size(const PlatformText *text);

// the bytes of the text, terminated [BORROWS]
const char *platform_text_bytes(PlatformText *text);

// the text emptied, with its cell kept at the size that it grew to
void platform_text_reset(PlatformText *text);

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

// the size of the file at `path` into `bytes`: 1, or 0 where the system cannot open the file
int platform_file_size(const char *path, unsigned long long *bytes);

// The bytes of the file at `path` into `out`, at max `capacity` of them, with their count in `bytes`. 1 where the whole
// file fit, 0 where the system cannot read it or it does not fit
int platform_file_read(const char *path, void *out, unsigned long long capacity, unsigned long long *bytes);

// `count` bytes of `data` written to the file at `path`, which the call makes or replaces: 1 where every byte went
int platform_file_written(const char *path, const void *data, unsigned long long count);

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

// the file at `from` moved to `to`, in place of any file there: 1, or 0 where the system refused
int platform_file_moved(const char *from, const char *to);

// the file at `path` removed: 1, or 0 where the system refused or no file was there
int platform_file_removed(const char *path);

// the folder at `path` made: 1 where it is there after the call, made now or before
int platform_folder_made(const char *path);

// the number that the system gives the process
unsigned long long platform_process_id(void);

// the monotonic clock of the system in nanoseconds, from a start that the system chooses
unsigned long long platform_clock_ns(void);

// the clock of the calendar: the whole seconds since the start of 1970, UTC, as time answers them
unsigned long long platform_epoch_seconds(void);

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

EMBED_END_DECLS

#endif
