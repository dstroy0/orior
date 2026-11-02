// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_pack.c: deflate written, in one block of the fixed codes, each run seen in the 32 KiB before it sent as a
// length and a distance back
#include "archive_internal.h"

// The bits of deflated data written, the low bit of each byte first
typedef struct
{
    unsigned char *out;
    unsigned long long room;
    unsigned long long size;
    unsigned long long bit_buf;
    unsigned int count;
    int full;
} CliWriter;

typedef struct
{
    CliWriter *writer;
    unsigned int value;
    unsigned int bits;
    unsigned int code;
    unsigned int symbol;
} ArchivePackCtx;
EMBED_INLINE void cli_put(const ArchivePackCtx *args)
{
    args->writer->bit_buf |= (unsigned long long)args->value << args->writer->count;
    args->writer->count += args->bits;
    while (args->writer->count >= 8u)
    {
        if (args->writer->size < args->writer->room)
        {
            // the low eight bits retained, a byte
            args->writer->out[args->writer->size] = (unsigned char)args->writer->bit_buf;
            args->writer->size += 1ull;
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
    EMBED_CALL(cli_put, ArchivePackCtx, .writer = args->writer, .value = reversed, .bits = args->bits);
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

unsigned long long cli_deflate(const CliArchiveSizeCfg *args)
{
    enum
    {
        CLI_WINDOW = 32768,
        CLI_CHAIN = 64
    };
    static unsigned long long s_head[1u << 15u];
    static unsigned long long s_previous[CLI_WINDOW];
    const unsigned long long none = ~0ull;
    for (unsigned int at = 0u; at < (1u << 15u); at += 1u)
    {
        s_head[at] = none;
    }
    for (unsigned int at = 0u; at < CLI_WINDOW; at += 1u)
    {
        s_previous[at] = none;
    }
    CliWriter writer = {args->out, args->room, 0ull, 0ull, 0u, 0};
    EMBED_CALL(cli_put, ArchivePackCtx, .writer = &writer, .value = 1u, .bits = 1u);
    EMBED_CALL(cli_put, ArchivePackCtx, .writer = &writer, .value = 1u, .bits = 2u);
    unsigned long long at = 0ull;
    while (at < args->size)
    {
        unsigned long long best = 0ull;
        unsigned long long back = 0ull;
        if ((at + 3ull) <= args->size)
        {
            const unsigned long long key = (((unsigned long long)args->data[at] << 10u) ^
                                            ((unsigned long long)args->data[at + 1ull] << 5u) ^ args->data[at + 2ull]) &
                                           ((1ull << 15u) - 1ull);
            unsigned long long candidate = s_head[key];
            unsigned int tries = 0u;
            while ((candidate != none) && ((at - candidate) <= CLI_WINDOW) && (tries < CLI_CHAIN))
            {
                const unsigned long long most = ((args->size - at) < 258ull) ? (args->size - at) : 258ull;
                unsigned long long length = 0ull;
                while ((length < most) && (args->data[candidate + length] == args->data[at + length]))
                {
                    length += 1ull;
                }
                if (length > best)
                {
                    best = length;
                    back = at - candidate;
                    if (length == most)
                    {
                        break;
                    }
                }
                candidate = s_previous[candidate % CLI_WINDOW];
                tries += 1u;
            }
        }
        unsigned long long taken = 1ull;
        if (best >= 3ull)
        {
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
            EMBED_CALL(cli_literal, ArchivePackCtx, .writer = &writer, .symbol = 257u + code);
            // a run's length past its code's base is below 32, which an unsigned int stores
            EMBED_CALL(cli_put, ArchivePackCtx, .writer = &writer,
                       .value = (unsigned int)(best - g_cli_length_base[code]), .bits = g_cli_length_extra[code]);
            EMBED_CALL(cli_code, ArchivePackCtx, .writer = &writer, .code = distance, .bits = 5u);
            // a distance past its code's base is below 8192, which an unsigned int stores
            EMBED_CALL(cli_put, ArchivePackCtx, .writer = &writer,
                       .value = (unsigned int)(back - g_cli_distance_base[distance]),
                       .bits = g_cli_distance_extra[distance]);
            taken = best;
        }
        else
        {
            EMBED_CALL(cli_literal, ArchivePackCtx, .writer = &writer, .symbol = args->data[at]);
        }
        for (unsigned long long step = at; step < (at + taken); step += 1ull)
        {
            if ((step + 3ull) <= args->size)
            {
                const unsigned long long key =
                    (((unsigned long long)args->data[step] << 10u) ^
                     ((unsigned long long)args->data[step + 1ull] << 5u) ^ args->data[step + 2ull]) &
                    ((1ull << 15u) - 1ull);
                s_previous[step % CLI_WINDOW] = s_head[key];
                s_head[key] = step;
            }
        }
        at += taken;
    }
    EMBED_CALL(cli_literal, ArchivePackCtx, .writer = &writer, .symbol = 256u);
    if (writer.count > 0u)
    {
        EMBED_CALL(cli_put, ArchivePackCtx, .writer = &writer, .value = 0u, .bits = 8u - writer.count);
    }
    return writer.full ? 0ull : writer.size;
}
