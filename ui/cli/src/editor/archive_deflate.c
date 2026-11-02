// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_deflate.c: CRC-32, and deflate read: stored, fixed and dynamic blocks (archive_pack.c writes it)
#include "archive_internal.h"

// The bits of deflated data read, the low bit of each byte first
typedef struct
{
    const unsigned char *data;
    unsigned long long size;
    unsigned long long at;
    unsigned int bit_buf;
    unsigned int count;
} CliBits;
// A canonical Huffman code: how many codes each length has, and the symbols in code order
typedef struct
{
    unsigned int counts[16];
    unsigned int symbols[320];
} CliHuffman;
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

unsigned int cli_crc32(const CliArchiveBytesSizeCfg *args)
{
    static unsigned int s_table[256];
    static int s_made;
    static CliLock s_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
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
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    unsigned int crc = 0xffffffffu;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        crc = s_table[(crc ^ args->bytes[at]) & 0xffu] ^ (crc >> 8u);
    }
    return ~crc;
}

typedef struct
{
    CliBits *bits;
    unsigned int count;
    unsigned int *value;
    const unsigned char *lengths;
    CliHuffman *huffman;
    const CliHuffman *huffman_read;
    unsigned int *symbol;
    VerbaLinea *err_msg;
    unsigned char *out;
    unsigned long long room;
    unsigned long long *size;
    const CliHuffman *literals;
    const CliHuffman *distances;
} ArchiveDeflateCtx;
// `bits` bits taken: 1 with them into `value`, or 0 where the data ends first
EMBED_INLINE int cli_take(const ArchiveDeflateCtx *args)
{
    unsigned int count = args->count;
    unsigned int *value = args->value;
    while (args->bits->count < count)
    {
        if (args->bits->at >= args->bits->size)
        {
            return 0;
        }
        args->bits->bit_buf |= (unsigned int)args->bits->data[args->bits->at] << args->bits->count;
        args->bits->at += 1ull;
        args->bits->count += 8u;
    }
    *value = args->bits->bit_buf & ((count == 32u) ? 0xffffffffu : ((1u << count) - 1u));
    args->bits->bit_buf = (count == 32u) ? 0u : (args->bits->bit_buf >> count);
    args->bits->count -= count;
    return 1;
}

EMBED_INLINE void cli_huffman_get(const ArchiveDeflateCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->huffman->counts, .val = 0, .bytes = sizeof(args->huffman->counts)});
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        args->huffman->counts[args->lengths[at] & 15u] += 1u;
    }
    args->huffman->counts[0] = 0u;
    unsigned int offsets[16];
    offsets[0] = 0u;
    offsets[1] = 0u;
    for (unsigned int length = 1u; length < 15u; length += 1u)
    {
        offsets[length + 1u] = offsets[length] + args->huffman->counts[length];
    }
    for (unsigned int symbol = 0u; symbol < args->count; symbol += 1u)
    {
        if (args->lengths[symbol] != 0u)
        {
            args->huffman->symbols[offsets[args->lengths[symbol]]] = symbol;
            offsets[args->lengths[symbol]] += 1u;
        }
    }
}

// a symbol decoded: 1 with it into `symbol`, or 0 with why given
EMBED_INLINE int cli_decode(const ArchiveDeflateCtx *args)
{
    unsigned int *symbol = args->symbol;
    int code = 0;
    int first = 0;
    int index = 0;
    for (unsigned int length = 1u; length < 16u; length += 1u)
    {
        unsigned int bit = 0u;
        if (!EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = args->bits, .count = 1u, .value = &bit))
        {
            mmgr_line_put(args->err_msg, "the deflated data ends early");
            return 0;
        }
        // a bit and a count are small, which an int stores
        code |= (int)bit;
        const int count = (int)args->huffman_read->counts[length];
        if ((code - count) < first)
        {
            const int place = index + code - first;
            if ((place < 0) || (place >= 320))
            {
                mmgr_line_put(args->err_msg, "a code past the table");
                return 0;
            }
            // a place in the table, not negative as checked above
            *symbol = args->huffman_read->symbols[(unsigned int)place];
            return 1;
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    mmgr_line_put(args->err_msg, "a code no table contains");
    return 0;
}

// a block's codes read until its end, each literal and each run put after the output
static int cli_codes(const ArchiveDeflateCtx *args)
{
    unsigned long long *size = args->size;
    for (;;)
    {
        unsigned int symbol = 0u;
        if (!EMBED_CALL(cli_decode, ArchiveDeflateCtx, .huffman_read = args->literals, .bits = args->bits,
                        .symbol = &symbol, .err_msg = args->err_msg))
        {
            return 0;
        }
        if (symbol < 256u)
        {
            if (*size >= args->room)
            {
                mmgr_line_put(args->err_msg, "the inflated data is larger than orior_cli supports");
                return 0;
            }
            // a literal is below 256, a byte
            args->out[*size] = (unsigned char)symbol;
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
        if ((at >= 29u) || !EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = args->bits, .count = g_cli_length_extra[at],
                                       .value = &extra))
        {
            mmgr_line_put(args->err_msg, (at >= 29u) ? "a length past the table" : "the deflated data ends early");
            return 0;
        }
        const unsigned long long length = g_cli_length_base[at] + extra;
        if (!EMBED_CALL(cli_decode, ArchiveDeflateCtx, .huffman_read = args->distances, .bits = args->bits,
                        .symbol = &code, .err_msg = args->err_msg))
        {
            return 0;
        }
        if ((code >= 30u) || !EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = args->bits,
                                         .count = g_cli_distance_extra[code], .value = &distance_extra))
        {
            mmgr_line_put(args->err_msg, (code >= 30u) ? "a distance past the table" : "the deflated data ends early");
            return 0;
        }
        const unsigned long long distance = g_cli_distance_base[code] + distance_extra;
        if (distance > *size)
        {
            mmgr_line_put(args->err_msg, "a distance back past the start");
            return 0;
        }
        if ((*size + length) > args->room)
        {
            mmgr_line_put(args->err_msg, "the inflated data is larger than orior_cli supports");
            return 0;
        }
        const unsigned long long from = *size - distance;
        for (unsigned long long step = 0ull; step < length; step += 1ull)
        {
            args->out[*size] = args->out[from + step];
            *size += 1ull;
        }
    }
}

int cli_inflate(const CliArchiveSizeCfg *args)
{
    unsigned long long *out_size = args->out_size;
    unsigned long long *used = args->used;
    static CliHuffman s_literals;
    static CliHuffman s_distances;
    static CliHuffman s_lengths_code;
    static CliLock s_lock = CLI_LOCK_INIT;
    static const unsigned char s_order[19] = {16u, 17u, 18u, 0u, 8u,  7u, 9u,  6u, 10u, 5u,
                                              11u, 4u,  12u, 3u, 13u, 2u, 14u, 1u, 15u};
    CliBits bits = {args->data, args->size, 0ull, 0u, 0u};
    *out_size = 0ull;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    for (;;)
    {
        unsigned int last = 0u;
        unsigned int kind = 0u;
        if (!EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = 1u, .value = &last) ||
            !EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = 2u, .value = &kind))
        {
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
            mmgr_line_put(args->err_msg, "the deflated data ends early");
            return 0;
        }
        int done = 1;
        if (kind == 0u)
        {
            bits.bit_buf = 0u;
            bits.count = 0u;
            const unsigned long long at = bits.at;
            if ((at + 4ull) > args->size)
            {
                mmgr_line_put(args->err_msg, "a stored block's header is cut");
                done = 0;
            }
            const unsigned int length = done ? (args->data[at] | ((unsigned int)args->data[at + 1ull] << 8u)) : 0u;
            const unsigned int check =
                done ? (args->data[at + 2ull] | ((unsigned int)args->data[at + 3ull] << 8u)) : 0u;
            if (done && (length != ((~check) & 0xffffu)))
            {
                mmgr_line_put(args->err_msg, "a stored block's length does not check");
                done = 0;
            }
            if (done && (((at + 4ull + length) > args->size) || ((*out_size + length) > args->room)))
            {
                mmgr_line_put(args->err_msg, ((at + 4ull + length) > args->size)
                                                 ? "a stored block is cut"
                                                 : "the inflated data is larger than orior_cli supports");
                done = 0;
            }
            if (done)
            {
                memor.cpy(&(MemoriaCfg){.dst = args->out + *out_size, .src = args->data + at + 4ull, .bytes = length});
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
            EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = lengths, .count = 288u, .huffman = &s_literals);
            EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = fives, .count = 30u, .huffman = &s_distances);
            done = EMBED_CALL(cli_codes, ArchiveDeflateCtx, .bits = &bits, .out = args->out, .room = args->room,
                              .size = out_size, .literals = &s_literals, .distances = &s_distances,
                              .err_msg = args->err_msg);
        }
        else if (kind == 2u)
        {
            unsigned int literals = 0u;
            unsigned int distances = 0u;
            unsigned int code_lengths = 0u;
            done = EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = 5u, .value = &literals) &&
                   EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = 5u, .value = &distances) &&
                   EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = 4u, .value = &code_lengths);
            literals += 257u;
            distances += 1u;
            code_lengths += 4u;
            unsigned char lengths[19];
            memor.set(&(MemoriaCfg){.dst = lengths, .val = 0, .bytes = sizeof(lengths)});
            for (unsigned int at = 0u; done && (at < code_lengths) && (at < 19u); at += 1u)
            {
                unsigned int value = 0u;
                done = EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = 3u, .value = &value);
                // a code length is below 8, a byte
                lengths[s_order[at]] = (unsigned char)value;
            }
            if (!done)
            {
                mmgr_line_put(args->err_msg, "the deflated data ends early");
            }
            EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = lengths, .count = 19u,
                       .huffman = &s_lengths_code);
            unsigned char all[320];
            unsigned int count = 0u;
            while (done && (count < (literals + distances)))
            {
                unsigned int symbol = 0u;
                unsigned int extra = 0u;
                done = EMBED_CALL(cli_decode, ArchiveDeflateCtx, .huffman_read = &s_lengths_code, .bits = &bits,
                                  .symbol = &symbol, .err_msg = args->err_msg);
                unsigned int value = symbol;
                unsigned int times = 1u;
                if (done && (symbol == 16u))
                {
                    done = (count > 0u) &&
                           EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = 2u, .value = &extra);
                    value = (count > 0u) ? all[count - 1u] : 0u;
                    times = 3u + extra;
                    if (!done)
                    {
                        mmgr_line_put(args->err_msg, (count > 0u) ? "the deflated data ends early"
                                                                  : "a repeat with nothing before it");
                    }
                }
                else if (done && (symbol >= 17u))
                {
                    done = EMBED_CALL(cli_take, ArchiveDeflateCtx, .bits = &bits, .count = (symbol == 17u) ? 3u : 7u,
                                      .value = &extra);
                    value = 0u;
                    times = ((symbol == 17u) ? 3u : 11u) + extra;
                    if (!done)
                    {
                        mmgr_line_put(args->err_msg, "the deflated data ends early");
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
                    mmgr_line_put(args->err_msg, "the code lengths overrun");
                    done = 0;
                }
            }
            if (done && ((literals > 288u) || (distances > 32u)))
            {
                mmgr_line_put(args->err_msg, "the code lengths overrun");
                done = 0;
            }
            if (done)
            {
                EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = all, .count = literals,
                           .huffman = &s_literals);
                EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = all + literals, .count = distances,
                           .huffman = &s_distances);
                done = EMBED_CALL(cli_codes, ArchiveDeflateCtx, .bits = &bits, .out = args->out, .room = args->room,
                                  .size = out_size, .literals = &s_literals, .distances = &s_distances,
                                  .err_msg = args->err_msg);
            }
        }
        else
        {
            mmgr_line_put(args->err_msg, "a block of a kind deflate has none of");
            done = 0;
        }
        if (!done)
        {
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
            return 0;
        }
        if (last != 0u)
        {
            *used = bits.at;
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
            return 1;
        }
    }
}
