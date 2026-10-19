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
    unsigned long long held;
    unsigned int count;
    int full;
} CliWriter;

static void cli_put(CliWriter *writer, unsigned int value, unsigned int bits)
{
    writer->held |= (unsigned long long)value << writer->count;
    writer->count += bits;
    while (writer->count >= 8u)
    {
        if (writer->size < writer->room)
        {
            // the low eight bits held, a byte
            writer->out[writer->size] = (unsigned char)writer->held;
            writer->size += 1ull;
        }
        else
        {
            writer->full = 1;
        }
        writer->held >>= 8u;
        writer->count -= 8u;
    }
}

// a Huffman code put, its first bit first
static void cli_code(CliWriter *writer, unsigned int code, unsigned int bits)
{
    unsigned int reversed = 0u;
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        reversed |= ((code >> bit) & 1u) << (bits - 1u - bit);
    }
    cli_put(writer, reversed, bits);
}

// a literal or a length's symbol put in the fixed code
static void cli_literal(CliWriter *writer, unsigned int symbol)
{
    if (symbol <= 143u)
    {
        cli_code(writer, 0x30u + symbol, 8u);
    }
    else if (symbol <= 255u)
    {
        cli_code(writer, 0x190u + symbol - 144u, 9u);
    }
    else if (symbol <= 279u)
    {
        cli_code(writer, symbol - 256u, 7u);
    }
    else
    {
        cli_code(writer, 0xC0u + symbol - 280u, 8u);
    }
}

unsigned long long cli_deflate(const unsigned char *data, unsigned long long size, unsigned char *out,
                               unsigned long long room)
{
    enum
    {
        CLI_WINDOW = 32768,
        CLI_CHAIN = 64
    };
    static unsigned long long s_head[1u << 15u];
    static unsigned long long s_previous[CLI_WINDOW];
    static CliLock s_lock = CLI_LOCK_INIT;
    const unsigned long long none = ~0ull;
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < (1u << 15u); at += 1u)
    {
        s_head[at] = none;
    }
    for (unsigned int at = 0u; at < CLI_WINDOW; at += 1u)
    {
        s_previous[at] = none;
    }
    CliWriter writer = {out, room, 0ull, 0ull, 0u, 0};
    cli_put(&writer, 1u, 1u);
    cli_put(&writer, 1u, 2u);
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned long long best = 0ull;
        unsigned long long back = 0ull;
        if ((at + 3ull) <= size)
        {
            const unsigned long long key = (((unsigned long long)data[at] << 10u) ^
                                            ((unsigned long long)data[at + 1ull] << 5u) ^ data[at + 2ull]) &
                                           ((1ull << 15u) - 1ull);
            unsigned long long candidate = s_head[key];
            unsigned int tries = 0u;
            while ((candidate != none) && ((at - candidate) <= CLI_WINDOW) && (tries < CLI_CHAIN))
            {
                const unsigned long long most = ((size - at) < 258ull) ? (size - at) : 258ull;
                unsigned long long length = 0ull;
                while ((length < most) && (data[candidate + length] == data[at + length]))
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
            cli_literal(&writer, 257u + code);
            // a run's length past its code's base is below 32, which an unsigned int holds
            cli_put(&writer, (unsigned int)(best - g_cli_length_base[code]), g_cli_length_extra[code]);
            cli_code(&writer, distance, 5u);
            // a distance past its code's base is below 8192, which an unsigned int holds
            cli_put(&writer, (unsigned int)(back - g_cli_distance_base[distance]), g_cli_distance_extra[distance]);
            taken = best;
        }
        else
        {
            cli_literal(&writer, data[at]);
        }
        for (unsigned long long step = at; step < (at + taken); step += 1ull)
        {
            if ((step + 3ull) <= size)
            {
                const unsigned long long key = (((unsigned long long)data[step] << 10u) ^
                                                ((unsigned long long)data[step + 1ull] << 5u) ^ data[step + 2ull]) &
                                               ((1ull << 15u) - 1ull);
                s_previous[step % CLI_WINDOW] = s_head[key];
                s_head[key] = step;
            }
        }
        at += taken;
    }
    cli_literal(&writer, 256u);
    if (writer.count > 0u)
    {
        cli_put(&writer, 0u, 8u - writer.count);
    }
    cli_lock_give(&s_lock);
    return writer.full ? 0ull : writer.size;
}
