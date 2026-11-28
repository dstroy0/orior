// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_intercept.c: every heap block that the disassembler of the vendor lets go, or still keeps as it ends, and
// every copy that it makes, written out whole. Thus a reader can read from them the tables that it unpacks for one
// architecture. They are the instruction set, stored packed in one block, and the latencies, copied out of the buffer
// that it unpacks them into.
//
// The build makes it a shared library, which Linux loads before the C library (LD_PRELOAD). It stands in for malloc,
// calloc, realloc, free and memcpy, and records the size of each live block. It writes a record to stdout when the
// program frees a block of at least TABLES_LEAST bytes. It also writes one for each such block that is live as the
// program ends. The record is a line `<ptr 0x0 size>`, with the address of the block in hex and its byte count in
// decimal. Then the `size` bytes of the block follow.
//
// Each copy writes a record in the same way, `<dst src size>` and the bytes copied (orior_tables_funnel). The file
// reaches the own routines of the C library through dlsym, which allocates while it finds them. Those first
// allocations come from a static arena of its own, never freed.
#define _GNU_SOURCE
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_linea.h"
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
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

// 1 while the file writes a record. A copy that the write makes is no copy of the program
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
    // each address as 0x and its hex digits, 0x0 for none, as the funnel reads it (line_number)
    VerbaLinea formed_line = mmgr_line_over(head, sizeof(head));
    mmgr_line_put(&formed_line, "\n<0x");
    mmgr_line_hex(&formed_line, (uint64_t)(uintptr_t)at, 1u);
    mmgr_line_put(&formed_line, " 0x");
    mmgr_line_hex(&formed_line, (uint64_t)(uintptr_t)from, 1u);
    mmgr_line_ch(&formed_line, ' ');
    mmgr_line_decimal(&formed_line, (uint64_t)size, 1u);
    mmgr_line_put(&formed_line, ">\n");
    (void)mmgr_line_finish(&formed_line);
    const int length = mmgr_line_ok(&formed_line) ? (int)formed_line.at : 0;
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
    // before dlsym finds the copy of the C library, MMgr's copy moves the bytes
    if (s_memcpy == NULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = dest, .src = src, .bytes = num);
        return dest;
    }
    if (!s_recording)
    {
        tables_record(dest, src, num);
    }
    return s_memcpy(dest, src, num);
}

static void tables_recorded(void *at, size_t size)
{
    if ((at == NULL) || (size < TABLES_LEAST) || (s_blocks >= TABLES_BLOCKS))
    {
        return;
    }
    s_block[s_blocks].at = at;
    s_block[s_blocks].size = size;
    s_blocks += 1u;
}

// the block at `at`, taken off the tracked ones and written out. The return is 1 where the file tracked it
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

// every block still allocated written out as the program ends
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
    tables_recorded(at, size);
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
            EMBED_CALL(memor.set, MemoriaCfg, .dst = at, .bytes = count * size, .val = 0);
        }
        return at;
    }
    void *const at = s_calloc(count, size);
    tables_recorded(at, count * size);
    return at;
}

void *realloc(void *at, size_t size)
{
    tables_found();
    if (tables_in_arena(at))
    {
        // a block of the arena moved: as much of it as the arena contains past it, which is at least the block
        const size_t left = (size_t)((s_arena + TABLES_ARENA) - (unsigned char *)at);
        void *const moved = malloc(size);
        if (moved != NULL)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = moved, .src = at, .bytes = (size < left) ? size : left);
        }
        return moved;
    }
    tables_let_go(at);
    void *const grown = s_realloc(at, size);
    tables_recorded(grown, size);
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
