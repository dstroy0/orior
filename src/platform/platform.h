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

// the cap of a text call of MMgr that stops at the terminator alone. The text calls count the cap and never add it to a
// pointer. A cap of all ones bounds nothing
#define PLATFORM_TEXT_TO_TERMINATOR (~(size_t)0)

// the line of the output stream: a line over the part of the block that platform_open positions. Its flush writes
// what it holds to the output stream, and so the line never fills. platform_line_out answers the same line on each call
VerbaLinea *platform_line_out(void);

// what the line of the output stream holds, written to the output stream, and the line emptied
void platform_line_flushed(void);

// ---- cells ----

// A cell of `bytes` from the pool, counted to the owner at `place`. An ephemeral cell is on the temporary tier and goes
// back before the work that took it ends. Any other cell is a resource on the persistent tier. NULL where the pool
// cannot meet the bytes [RETURNS OWNERSHIP]
void *platform_cell_taken(unsigned long long bytes, unsigned int place, int ephemeral);

// a cell given back to the pool by the owner that took it, and nothing where the cell is NULL [TAKES OWNERSHIP]
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

// the size of the file at `path` into `bytes`: 1, or 0 where the system cannot open the file
int platform_file_size(const char *path, unsigned long long *bytes);

// The bytes of the file at `path` into `out`, at max `capacity` of them, with their count in `bytes`. 1 where the whole
// file fit, 0 where the system cannot read it or it does not fit
int platform_file_read(const char *path, void *out, unsigned long long capacity, unsigned long long *bytes);

// `count` bytes of `data` written to the file at `path`, which the call makes or replaces: 1 where every byte went
int platform_file_written(const char *path, const void *data, unsigned long long count);

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

EMBED_END_DECLS

#endif
