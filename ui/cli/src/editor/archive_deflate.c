// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_deflate.c: CRC-32, and deflate read: stored, fixed and dynamic blocks (archive_pack.c writes it)
#include "archive_internal.h"

const unsigned int g_cli_length_base[29] = {3u,  4u,  5u,  6u,   7u,   8u,   9u,   10u,  11u, 13u,
                                            15u, 17u, 19u, 23u,  27u,  31u,  35u,  43u,  51u, 59u,
                                            67u, 83u, 99u, 115u, 131u, 163u, 195u, 227u, 258u};

const unsigned int g_cli_length_extra[29] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u,
                                             2u, 3u, 3u, 3u, 3u, 4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u};

const unsigned int g_cli_distance_base[30] = {1u,    2u,    3u,    4u,    5u,    7u,    9u,    13u,    17u,    25u,
                                              33u,   49u,   65u,   97u,   129u,  193u,  257u,  385u,   513u,   769u,
                                              1025u, 1537u, 2049u, 3073u, 4097u, 6145u, 8193u, 12289u, 16385u, 24577u};

const unsigned int g_cli_distance_extra[30] = {0u, 0u, 0u, 0u, 1u, 1u, 2u, 2u,  3u,  3u,  4u,  4u,  5u,  5u,  6u,
                                               6u, 7u, 7u, 8u, 8u, 9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u};

unsigned int cli_crc32(const unsigned char *bytes, unsigned long long size)
{
    static unsigned int s_table[256];
    static int s_made;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    if (!s_made)
    {
        for (unsigned int at = 0u; at < 256u; at += 1u)
        {
            unsigned int value = at;
            for (unsigned int bit = 0u; bit < 8u; bit += 1u)
            {
                value = ((value & 1u) != 0u) ? (0xEDB88320u ^ (value >> 1u)) : (value >> 1u);
            }
            s_table[at] = value;
        }
        s_made = 1;
    }
    cli_lock_give(&s_lock);
    unsigned int crc = 0xffffffffu;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        crc = s_table[(crc ^ bytes[at]) & 0xffu] ^ (crc >> 8u);
    }
    return ~crc;
}

// The bits of deflated data read, the low bit of each byte first
typedef struct
{
    const unsigned char *data;
    unsigned long long size;
    unsigned long long at;
    unsigned int held;
    unsigned int count;
} CliBits;

// `bits` bits taken: 1 with them into `value`, or 0 where the data ends first
static int cli_take(CliBits *bits, unsigned int count, unsigned int *value)
{
    while (bits->count < count)
    {
        if (bits->at >= bits->size)
        {
            return 0;
        }
        bits->held |= (unsigned int)bits->data[bits->at] << bits->count;
        bits->at += 1ull;
        bits->count += 8u;
    }
    *value = bits->held & ((count == 32u) ? 0xffffffffu : ((1u << count) - 1u));
    bits->held = (count == 32u) ? 0u : (bits->held >> count);
    bits->count -= count;
    return 1;
}

// A canonical Huffman code: how many codes each length has, and the symbols in code order
typedef struct
{
    unsigned int counts[16];
    unsigned int symbols[320];
} CliHuffman;

static void cli_huffman_of(const unsigned char *lengths, unsigned int count, CliHuffman *huffman)
{
    memor.set(&(MemoriaCfg){.dst = huffman->counts, .val = 0, .bytes = sizeof(huffman->counts)});
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        huffman->counts[lengths[at] & 15u] += 1u;
    }
    huffman->counts[0] = 0u;
    unsigned int offsets[16];
    offsets[0] = 0u;
    offsets[1] = 0u;
    for (unsigned int length = 1u; length < 15u; length += 1u)
    {
        offsets[length + 1u] = offsets[length] + huffman->counts[length];
    }
    for (unsigned int symbol = 0u; symbol < count; symbol += 1u)
    {
        if (lengths[symbol] != 0u)
        {
            huffman->symbols[offsets[lengths[symbol]]] = symbol;
            offsets[lengths[symbol]] += 1u;
        }
    }
}

// a symbol decoded: 1 with it into `symbol`, or 0 with why said
static int cli_decode(const CliHuffman *huffman, CliBits *bits, unsigned int *symbol, CliLine *said)
{
    int code = 0;
    int first = 0;
    int index = 0;
    for (unsigned int length = 1u; length < 16u; length += 1u)
    {
        unsigned int bit = 0u;
        if (!cli_take(bits, 1u, &bit))
        {
            cli_line_text(said, "the deflated data ends early");
            return 0;
        }
        // a bit and a count are small, which an int holds
        code |= (int)bit;
        const int count = (int)huffman->counts[length];
        if ((code - count) < first)
        {
            const int place = index + code - first;
            if ((place < 0) || (place >= 320))
            {
                cli_line_text(said, "a code past the table");
                return 0;
            }
            // a place in the table, not negative as checked above
            *symbol = huffman->symbols[(unsigned int)place];
            return 1;
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    cli_line_text(said, "a code no table holds");
    return 0;
}

// a block's codes read until its end, each literal and each run put after the output
static int cli_codes(CliBits *bits, unsigned char *out, unsigned long long room, unsigned long long *size,
                     const CliHuffman *literals, const CliHuffman *distances, CliLine *said)
{
    for (;;)
    {
        unsigned int symbol = 0u;
        if (!cli_decode(literals, bits, &symbol, said))
        {
            return 0;
        }
        if (symbol < 256u)
        {
            if (*size >= room)
            {
                cli_line_text(said, "the inflated data is larger than orior holds");
                return 0;
            }
            // a literal is below 256, a byte
            out[*size] = (unsigned char)symbol;
            *size += 1ull;
            continue;
        }
        if (symbol == 256u)
        {
            return 1;
        }
        const unsigned int at = symbol - 257u;
        unsigned int extra = 0u;
        unsigned int code = 0u;
        unsigned int distance_extra = 0u;
        if ((at >= 29u) || !cli_take(bits, g_cli_length_extra[at], &extra))
        {
            cli_line_text(said, (at >= 29u) ? "a length past the table" : "the deflated data ends early");
            return 0;
        }
        const unsigned long long length = g_cli_length_base[at] + extra;
        if (!cli_decode(distances, bits, &code, said))
        {
            return 0;
        }
        if ((code >= 30u) || !cli_take(bits, g_cli_distance_extra[code], &distance_extra))
        {
            cli_line_text(said, (code >= 30u) ? "a distance past the table" : "the deflated data ends early");
            return 0;
        }
        const unsigned long long distance = g_cli_distance_base[code] + distance_extra;
        if (distance > *size)
        {
            cli_line_text(said, "a distance back past the start");
            return 0;
        }
        if ((*size + length) > room)
        {
            cli_line_text(said, "the inflated data is larger than orior holds");
            return 0;
        }
        const unsigned long long from = *size - distance;
        for (unsigned long long step = 0ull; step < length; step += 1ull)
        {
            out[*size] = out[from + step];
            *size += 1ull;
        }
    }
}

int cli_inflate(const unsigned char *data, unsigned long long size, unsigned char *out, unsigned long long room,
                unsigned long long *out_size, unsigned long long *used, CliLine *said)
{
    static CliHuffman s_literals;
    static CliHuffman s_distances;
    static CliHuffman s_lengths_code;
    static CliLock s_lock = CLI_LOCK_INIT;
    static const unsigned char s_order[19] = {16u, 17u, 18u, 0u, 8u,  7u, 9u,  6u, 10u, 5u,
                                              11u, 4u,  12u, 3u, 13u, 2u, 14u, 1u, 15u};
    CliBits bits = {data, size, 0ull, 0u, 0u};
    *out_size = 0ull;
    cli_lock_take(&s_lock);
    for (;;)
    {
        unsigned int last = 0u;
        unsigned int kind = 0u;
        if (!cli_take(&bits, 1u, &last) || !cli_take(&bits, 2u, &kind))
        {
            cli_lock_give(&s_lock);
            cli_line_text(said, "the deflated data ends early");
            return 0;
        }
        int done = 1;
        if (kind == 0u)
        {
            bits.held = 0u;
            bits.count = 0u;
            const unsigned long long at = bits.at;
            if ((at + 4ull) > size)
            {
                cli_line_text(said, "a stored block's header is cut");
                done = 0;
            }
            const unsigned int length = done ? (data[at] | ((unsigned int)data[at + 1ull] << 8u)) : 0u;
            const unsigned int check = done ? (data[at + 2ull] | ((unsigned int)data[at + 3ull] << 8u)) : 0u;
            if (done && (length != ((~check) & 0xffffu)))
            {
                cli_line_text(said, "a stored block's length does not check");
                done = 0;
            }
            if (done && (((at + 4ull + length) > size) || ((*out_size + length) > room)))
            {
                cli_line_text(said, ((at + 4ull + length) > size) ? "a stored block is cut"
                                                                  : "the inflated data is larger than orior holds");
                done = 0;
            }
            if (done)
            {
                memor.cpy(&(MemoriaCfg){.dst = out + *out_size, .src = data + at + 4ull, .bytes = length});
                *out_size += length;
                bits.at = at + 4ull + length;
            }
        }
        else if (kind == 1u)
        {
            unsigned char lengths[288];
            memor.set(&(MemoriaCfg){.dst = lengths, .val = 8, .bytes = 144u});
            memor.set(&(MemoriaCfg){.dst = lengths + 144, .val = 9, .bytes = 112u});
            memor.set(&(MemoriaCfg){.dst = lengths + 256, .val = 7, .bytes = 24u});
            memor.set(&(MemoriaCfg){.dst = lengths + 280, .val = 8, .bytes = 8u});
            unsigned char fives[30];
            memor.set(&(MemoriaCfg){.dst = fives, .val = 5, .bytes = sizeof(fives)});
            cli_huffman_of(lengths, 288u, &s_literals);
            cli_huffman_of(fives, 30u, &s_distances);
            done = cli_codes(&bits, out, room, out_size, &s_literals, &s_distances, said);
        }
        else if (kind == 2u)
        {
            unsigned int literals = 0u;
            unsigned int distances = 0u;
            unsigned int code_lengths = 0u;
            done =
                cli_take(&bits, 5u, &literals) && cli_take(&bits, 5u, &distances) && cli_take(&bits, 4u, &code_lengths);
            literals += 257u;
            distances += 1u;
            code_lengths += 4u;
            unsigned char lengths[19];
            memor.set(&(MemoriaCfg){.dst = lengths, .val = 0, .bytes = sizeof(lengths)});
            for (unsigned int at = 0u; done && (at < code_lengths) && (at < 19u); at += 1u)
            {
                unsigned int value = 0u;
                done = cli_take(&bits, 3u, &value);
                // a code length is below 8, a byte
                lengths[s_order[at]] = (unsigned char)value;
            }
            if (!done)
            {
                cli_line_text(said, "the deflated data ends early");
            }
            cli_huffman_of(lengths, 19u, &s_lengths_code);
            unsigned char all[320];
            unsigned int count = 0u;
            while (done && (count < (literals + distances)))
            {
                unsigned int symbol = 0u;
                unsigned int extra = 0u;
                done = cli_decode(&s_lengths_code, &bits, &symbol, said);
                unsigned int value = symbol;
                unsigned int times = 1u;
                if (done && (symbol == 16u))
                {
                    done = (count > 0u) && cli_take(&bits, 2u, &extra);
                    value = (count > 0u) ? all[count - 1u] : 0u;
                    times = 3u + extra;
                    if (!done)
                    {
                        cli_line_text(said, (count > 0u) ? "the deflated data ends early"
                                                         : "a repeat with nothing before it");
                    }
                }
                else if (done && (symbol >= 17u))
                {
                    done = cli_take(&bits, (symbol == 17u) ? 3u : 7u, &extra);
                    value = 0u;
                    times = ((symbol == 17u) ? 3u : 11u) + extra;
                    if (!done)
                    {
                        cli_line_text(said, "the deflated data ends early");
                    }
                }
                for (unsigned int time = 0u; done && (time < times); time += 1u)
                {
                    if (count >= 320u)
                    {
                        break;
                    }
                    // a code length is below 16, a byte
                    all[count] = (unsigned char)value;
                    count += 1u;
                }
                if (done && (count > (literals + distances)))
                {
                    cli_line_text(said, "the code lengths overrun");
                    done = 0;
                }
            }
            if (done && ((literals > 288u) || (distances > 32u)))
            {
                cli_line_text(said, "the code lengths overrun");
                done = 0;
            }
            if (done)
            {
                cli_huffman_of(all, literals, &s_literals);
                cli_huffman_of(all + literals, distances, &s_distances);
                done = cli_codes(&bits, out, room, out_size, &s_literals, &s_distances, said);
            }
        }
        else
        {
            cli_line_text(said, "a block of a kind deflate has none of");
            done = 0;
        }
        if (!done)
        {
            cli_lock_give(&s_lock);
            return 0;
        }
        if (last != 0u)
        {
            *used = bits.at;
            cli_lock_give(&s_lock);
            return 1;
        }
    }
}
