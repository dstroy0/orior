// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_pack.c: deflate written, in one block of the fixed codes, each run seen in the 32 KiB before it sent as a
// length and a distance back
#include "archive_internal.h"

// The bits of deflated data written, the low bit of each byte first
typedef struct
{
    unsigned char *out;
    unsigned long long capacity;
    unsigned long long size;
    unsigned long long bit_buf;
    unsigned int count;
    int full;
} CliWriter;

// the bytes a run looks back over, and the max earlier places of a key it tries
#define CLI_WINDOW 32768ULL
#define CLI_CHAIN 64u

// the last place each key of three bytes was seen, and the place each place's key was seen before it, by the place
// within the window; CLI_PACK_NONE where none was
#define CLI_PACK_NONE (~0ULL)
static unsigned long long s_head[1u << 15u];
static unsigned long long s_previous[CLI_WINDOW];

typedef struct
{
    CliWriter *writer;
    unsigned int value;
    unsigned int bits;
    unsigned int code;
    unsigned int symbol;
    const unsigned char *data;
    unsigned long long size;
    unsigned long long at;
    unsigned long long *back_out;
    unsigned long long best;
    unsigned long long back;
    unsigned long long reserved;
} ArchivePackCtx;
EMBED_INLINE void cli_append(const ArchivePackCtx *args)
{
    args->writer->bit_buf |= (unsigned long long)args->value << args->writer->count;
    args->writer->count += args->bits;
    while (args->writer->count >= 8u)
    {
        if (args->writer->size < args->writer->capacity)
        {
            // the low eight bits retained, a byte
            args->writer->out[args->writer->size] = (unsigned char)args->writer->bit_buf;
            args->writer->size += 1ULL;
        }
        else
        {
            args->writer->full = 1;
        }
        args->writer->bit_buf >>= 8u;
        args->writer->count -= 8u;
    }
}

// a Huffman code put, its first bit first
EMBED_INLINE void cli_code(const ArchivePackCtx *args)
{
    unsigned int reversed = 0u;
    for (unsigned int bit = 0u; bit < args->bits; bit += 1u)
    {
        reversed |= ((args->code >> bit) & 1u) << (args->bits - 1u - bit);
    }
    EMBED_CALL(cli_append, ArchivePackCtx, .writer = args->writer, .value = reversed, .bits = args->bits);
}

// a literal or a length's symbol put in the fixed code
EMBED_INLINE void cli_literal(const ArchivePackCtx *args)
{
    if (args->symbol <= 143u)
    {
        EMBED_CALL(cli_code, ArchivePackCtx, .writer = args->writer, .code = 0x30u + args->symbol, .bits = 8u);
    }
    else if (args->symbol <= 255u)
    {
        EMBED_CALL(cli_code, ArchivePackCtx, .writer = args->writer, .code = 0x190u + args->symbol - 144u, .bits = 9u);
    }
    else if (args->symbol <= 279u)
    {
        EMBED_CALL(cli_code, ArchivePackCtx, .writer = args->writer, .code = args->symbol - 256u, .bits = 7u);
    }
    else
    {
        EMBED_CALL(cli_code, ArchivePackCtx, .writer = args->writer, .code = 0xC0u + args->symbol - 280u, .bits = 8u);
    }
}

// the key of the three bytes of `data` at `at`, below 1 << 15
EMBED_INLINE unsigned long long cli_key(const ArchivePackCtx *args)
{
    const unsigned char *const data = args->data;
    const unsigned long long at = args->at;
    return (((unsigned long long)data[at] << 10u) ^ ((unsigned long long)data[at + 1ULL] << 5u) ^ data[at + 2ULL]) &
           ((1ULL << 15u) - 1ULL);
}

// the longest run at `at` of the `size` bytes of `data` that is within the window before it, of the CLI_CHAIN
// latest places its key was seen, at max 258 bytes, and how far back it is into `back_out`: 0 where none is
EMBED_INLINE unsigned long long cli_longest_run(const ArchivePackCtx *args)
{
    const unsigned char *const data = args->data;
    const unsigned long long at = args->at;
    unsigned long long best = 0ULL;
    *args->back_out = 0ULL;
    if ((at + 3ULL) > args->size)
    {
        return 0ULL;
    }
    const unsigned long long max = ((args->size - at) < 258ULL) ? (args->size - at) : 258ULL;
    unsigned long long candidate = s_head[EMBED_CALL(cli_key, ArchivePackCtx, .data = data, .at = at)];
    unsigned int tries = 0u;
    while ((candidate != CLI_PACK_NONE) && ((at - candidate) <= CLI_WINDOW) && (tries < CLI_CHAIN) && (best < max))
    {
        unsigned long long length = 0ULL;
        while ((length < max) && (data[candidate + length] == data[at + length]))
        {
            length += 1ULL;
        }
        if (length > best)
        {
            best = length;
            *args->back_out = at - candidate;
        }
        candidate = s_previous[candidate % CLI_WINDOW];
        tries += 1u;
    }
    return best;
}

// a run of `best` bytes `back` bytes back put: its length's symbol and extra bits, then its distance's code and extra
// bits
EMBED_INLINE void cli_run_put(const ArchivePackCtx *args)
{
    const unsigned long long best = args->best;
    const unsigned long long back = args->back;
    unsigned int code = 0u;
    for (unsigned int each = 0u; each < 29u; each += 1u)
    {
        code = (g_cli_length_base[each] <= best) ? each : code;
    }
    unsigned int distance = 0u;
    for (unsigned int each = 0u; each < 30u; each += 1u)
    {
        distance = (g_cli_distance_base[each] <= back) ? each : distance;
    }
    EMBED_CALL(cli_literal, ArchivePackCtx, .writer = args->writer, .symbol = 257u + code);
    // a run's length past its code's base is below 32, which an unsigned int stores
    EMBED_CALL(cli_append, ArchivePackCtx, .writer = args->writer,
               .value = (unsigned int)(best - g_cli_length_base[code]), .bits = g_cli_length_extra[code]);
    EMBED_CALL(cli_code, ArchivePackCtx, .writer = args->writer, .code = distance, .bits = 5u);
    // a distance past its code's base is below 8192, which an unsigned int stores
    EMBED_CALL(cli_append, ArchivePackCtx, .writer = args->writer,
               .value = (unsigned int)(back - g_cli_distance_base[distance]), .bits = g_cli_distance_extra[distance]);
}

// each place from `at` up to `at` + `reserved` of the `size` bytes of `data` with three bytes from it recorded as the
// latest seen of its key
EMBED_INLINE void cli_places_seen(const ArchivePackCtx *args)
{
    for (unsigned long long step = args->at; (step < (args->at + args->reserved)) && ((step + 3ULL) <= args->size);
         step += 1ULL)
    {
        const unsigned long long key = EMBED_CALL(cli_key, ArchivePackCtx, .data = args->data, .at = step);
        s_previous[step % CLI_WINDOW] = s_head[key];
        s_head[key] = step;
    }
}

unsigned long long cli_deflate(const CliArchiveSizeCfg *args)
{
    for (unsigned int at = 0u; at < (1u << 15u); at += 1u)
    {
        s_head[at] = CLI_PACK_NONE;
    }
    for (unsigned long long at = 0ULL; at < CLI_WINDOW; at += 1ULL)
    {
        s_previous[at] = CLI_PACK_NONE;
    }
    CliWriter writer = {args->out, args->capacity, 0ULL, 0ULL, 0u, 0};
    EMBED_CALL(cli_append, ArchivePackCtx, .writer = &writer, .value = 1u, .bits = 1u);
    EMBED_CALL(cli_append, ArchivePackCtx, .writer = &writer, .value = 1u, .bits = 2u);
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        unsigned long long back = 0ULL;
        const unsigned long long best = EMBED_CALL(cli_longest_run, ArchivePackCtx, .data = args->data,
                                                   .size = args->size, .at = at, .back_out = &back);
        unsigned long long reserved = 1ULL;
        if (best >= 3ULL)
        {
            EMBED_CALL(cli_run_put, ArchivePackCtx, .writer = &writer, .best = best, .back = back);
            reserved = best;
        }
        else
        {
            EMBED_CALL(cli_literal, ArchivePackCtx, .writer = &writer, .symbol = args->data[at]);
        }
        EMBED_CALL(cli_places_seen, ArchivePackCtx, .data = args->data, .size = args->size, .at = at,
                   .reserved = reserved);
        at += reserved;
    }
    EMBED_CALL(cli_literal, ArchivePackCtx, .writer = &writer, .symbol = 256u);
    if (writer.count > 0u)
    {
        EMBED_CALL(cli_append, ArchivePackCtx, .writer = &writer, .value = 0u, .bits = 8u - writer.count);
    }
    return writer.full ? 0ULL : writer.size;
}
