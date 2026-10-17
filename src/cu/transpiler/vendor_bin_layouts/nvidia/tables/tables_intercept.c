// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_intercept.c: every heap block the vendor's disassembler lets go, or still holds as it ends, and every copy
// it makes, written out whole, so that the tables it unpacks for one architecture are read off them: the
// instruction set, held packed in one block, and the latencies, copied out of the buffer they are unpacked into
//
// Built as a shared library and loaded before the C library on Linux (LD_PRELOAD), it stands in for malloc, calloc,
// realloc, free and memcpy. It keeps each live block's size and writes a record to stdout as a block of at least
// TABLES_LEAST bytes is freed, and for each such block still held as the program ends: a line `<ptr 0x0 size>` with
// the block's address in hex and its byte count in decimal, then the block's `size` bytes. Each copy writes a record
// the same way, `<dst src size>` and the bytes copied (tables_funnel). The C library's own routines are reached
// through dlsym, which allocates while they are found: those first allocations come off a static arena of its own,
// never freed.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TABLES_LEAST 4096u
#define TABLES_BLOCKS 65536u
#define TABLES_ARENA 65536u

typedef struct
{
    void *at;
    size_t size;
} TablesBlock;

static TablesBlock s_block[TABLES_BLOCKS];
static unsigned int s_blocks;
static unsigned char s_arena[TABLES_ARENA];
static size_t s_arena_used;
static int s_finding;

static void *(*s_malloc)(size_t);
static void *(*s_calloc)(size_t, size_t);
static void *(*s_realloc)(void *, size_t);
static void (*s_free)(void *);
static void *(*s_memcpy)(void *restrict, const void *restrict, size_t);

// 1 while a record is being written: a copy the writing makes is no copy of the program's
static int s_recording;

static void tables_found(void)
{
    if (s_malloc != NULL || s_finding)
    {
        return;
    }
    s_finding = 1;
    *(void **)(&s_malloc) = dlsym(RTLD_NEXT, "malloc");
    *(void **)(&s_calloc) = dlsym(RTLD_NEXT, "calloc");
    *(void **)(&s_realloc) = dlsym(RTLD_NEXT, "realloc");
    *(void **)(&s_free) = dlsym(RTLD_NEXT, "free");
    *(void **)(&s_memcpy) = dlsym(RTLD_NEXT, "memcpy");
    s_finding = 0;
}

// the whole of `bytes`, `count` of them, written to stdout, however many writes it takes
static void tables_written(const void *bytes, size_t count)
{
    const unsigned char *at = (const unsigned char *)bytes;
    while (count != 0u)
    {
        const ssize_t wrote = write(1, at, count);
        if (wrote <= 0)
        {
            return;
        }
        at += wrote;
        count -= (size_t)wrote;
    }
}

// a record written: the line naming the copy's destination `at`, its source `from`, 0x0 for a block, and the byte
// count, then the bytes at `from`
static void tables_record(const void *at, const void *from, size_t size)
{
    char head[80];
    s_recording = 1;
    const int length = snprintf(head, sizeof(head), "\n<%p %p %zu>\n", at, from, size);
    if (length > 0)
    {
        tables_written(head, (size_t)length);
        tables_written(from, size);
    }
    s_recording = 0;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t num)
{
    tables_found();
    if (s_memcpy == NULL)
    {
        unsigned char *to = (unsigned char *)dest;
        const unsigned char *at = (const unsigned char *)src;
        for (size_t byte = 0u; byte < num; byte += 1u)
        {
            to[byte] = at[byte];
        }
        return dest;
    }
    if (!s_recording)
    {
        tables_record(dest, src, num);
    }
    return s_memcpy(dest, src, num);
}

static void tables_kept(void *at, size_t size)
{
    if ((at == NULL) || (size < TABLES_LEAST) || (s_blocks >= TABLES_BLOCKS))
    {
        return;
    }
    s_block[s_blocks].at = at;
    s_block[s_blocks].size = size;
    s_blocks += 1u;
}

// the block at `at` taken off the kept ones and written out: 1 where it was kept
static int tables_let_go(void *at)
{
    for (unsigned int block = 0u; block < s_blocks; block += 1u)
    {
        if (s_block[block].at == at)
        {
            tables_record(at, at, s_block[block].size);
            s_block[block] = s_block[s_blocks - 1u];
            s_blocks -= 1u;
            return 1;
        }
    }
    return 0;
}

// every block still held written out as the program ends
__attribute__((destructor)) static void tables_ended(void)
{
    for (unsigned int block = 0u; block < s_blocks; block += 1u)
    {
        tables_record(s_block[block].at, s_block[block].at, s_block[block].size);
    }
}

static void *tables_arena(size_t size)
{
    const size_t rounded = (size + 15u) & ~(size_t)15u;
    if (s_arena_used + rounded > TABLES_ARENA)
    {
        return NULL;
    }
    void *const at = s_arena + s_arena_used;
    s_arena_used += rounded;
    return at;
}

static int tables_in_arena(const void *at)
{
    return (at >= (const void *)s_arena) && (at < (const void *)(s_arena + TABLES_ARENA));
}

void *malloc(size_t size)
{
    tables_found();
    if (s_malloc == NULL)
    {
        return tables_arena(size);
    }
    void *const at = s_malloc(size);
    tables_kept(at, size);
    return at;
}

void *calloc(size_t count, size_t size)
{
    tables_found();
    if (s_calloc == NULL)
    {
        void *const at = tables_arena(count * size);
        if (at != NULL)
        {
            memset(at, 0, count * size);
        }
        return at;
    }
    void *const at = s_calloc(count, size);
    tables_kept(at, count * size);
    return at;
}

void *realloc(void *at, size_t size)
{
    tables_found();
    if (tables_in_arena(at))
    {
        // a block of the arena moved: as much of it as the arena holds past it, which is at least the block
        const size_t left = (size_t)((s_arena + TABLES_ARENA) - (unsigned char *)at);
        void *const moved = malloc(size);
        if (moved != NULL)
        {
            memcpy(moved, at, (size < left) ? size : left);
        }
        return moved;
    }
    tables_let_go(at);
    void *const grown = s_realloc(at, size);
    tables_kept(grown, size);
    return grown;
}

void free(void *at)
{
    tables_found();
    if ((at == NULL) || tables_in_arena(at))
    {
        return;
    }
    tables_let_go(at);
    s_free(at);
}
