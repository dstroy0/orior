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
    static int s_created;
    if (!s_created)
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
        s_created = 1;
    }
    unsigned int crc = 0xffffffffu;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
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
    unsigned long long capacity;
    unsigned long long *size;
    const CliHuffman *literals;
    const CliHuffman *distances;
    const unsigned char *data;
    unsigned long long data_size;
    unsigned char *lengths_out;
    unsigned int literal_count;
    unsigned int distance_count;
} ArchiveDeflateCtx;
// `bits` bits taken: 1 with them into `value`, or 0 where the data ends first
EMBED_INLINE int cli_alloc(const ArchiveDeflateCtx *args)
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
        args->bits->at += 1ULL;
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
        if (!EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits, .count = 1u, .value = &bit))
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
            if (*size >= args->capacity)
            {
                mmgr_line_put(args->err_msg, "the inflated data is larger than orior_cli supports");
                return 0;
            }
            // a literal is below 256, a byte
            args->out[*size] = (unsigned char)symbol;
            *size += 1ULL;
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
        if ((at >= 29u) || !EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits,
                                       .count = g_cli_length_extra[at], .value = &extra))
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
        if ((code >= 30u) || !EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits,
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
        if ((*size + length) > args->capacity)
        {
            mmgr_line_put(args->err_msg, "the inflated data is larger than orior_cli supports");
            return 0;
        }
        const unsigned long long from = *size - distance;
        for (unsigned long long step = 0ULL; step < length; step += 1ULL)
        {
            args->out[*size] = args->out[from + step];
            *size += 1ULL;
        }
    }
}

// the tables a block's codes are read through, made again for each block
static CliHuffman s_literals;
static CliHuffman s_distances;
static CliHuffman s_lengths_code;

// a stored block's bytes put after the output: 1, or 0 with why given
static int cli_block_stored(const ArchiveDeflateCtx *args)
{
    args->bits->bit_buf = 0u;
    args->bits->count = 0u;
    const unsigned long long at = args->bits->at;
    if ((at + 4ULL) > args->data_size)
    {
        mmgr_line_put(args->err_msg, "a stored block's header is cut");
        return 0;
    }
    const unsigned int length = args->data[at] | ((unsigned int)args->data[at + 1ULL] << 8u);
    const unsigned int check = args->data[at + 2ULL] | ((unsigned int)args->data[at + 3ULL] << 8u);
    if (length != ((~check) & 0xffffu))
    {
        mmgr_line_put(args->err_msg, "a stored block's length does not check");
        return 0;
    }
    if ((at + 4ULL + length) > args->data_size)
    {
        mmgr_line_put(args->err_msg, "a stored block is cut");
        return 0;
    }
    if ((*args->size + length) > args->capacity)
    {
        mmgr_line_put(args->err_msg, "the inflated data is larger than orior_cli supports");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->out + *args->size, .src = args->data + at + 4ULL, .bytes = length});
    *args->size += length;
    args->bits->at = at + 4ULL + length;
    return 1;
}

// a block of the fixed tables read: 1, or 0 with why given
static int cli_block_fixed(const ArchiveDeflateCtx *args)
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
    return EMBED_CALL(cli_codes, ArchiveDeflateCtx, .bits = args->bits, .out = args->out, .capacity = args->capacity,
                      .size = args->size, .literals = &s_literals, .distances = &s_distances, .err_msg = args->err_msg);
}

// what one symbol of the code lengths' table says into `value` and `times`, the lengths before it being `lengths_out`
// up to `count`: 1, or 0 with why given
EMBED_INLINE int cli_length_symbol(const ArchiveDeflateCtx *args)
{
    unsigned int symbol = 0u;
    unsigned int extra = 0u;
    if (!EMBED_CALL(cli_decode, ArchiveDeflateCtx, .huffman_read = &s_lengths_code, .bits = args->bits,
                    .symbol = &symbol, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (symbol < 16u)
    {
        *args->value = symbol;
        *args->symbol = 1u;
        return 1;
    }
    if ((symbol == 16u) && (args->count == 0u))
    {
        mmgr_line_put(args->err_msg, "a repeat with nothing before it");
        return 0;
    }
    static const unsigned int s_extra_bits[3] = {2u, 3u, 7u};
    static const unsigned int s_least[3] = {3u, 3u, 11u};
    // a symbol of this table is below 19, and here at 16 or past it
    const unsigned int repeat = symbol - 16u;
    if (!EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits, .count = s_extra_bits[repeat], .value = &extra))
    {
        mmgr_line_put(args->err_msg, "the deflated data ends early");
        return 0;
    }
    *args->value = (symbol == 16u) ? args->lengths_out[args->count - 1u] : 0u;
    *args->symbol = s_least[repeat] + extra;
    return 1;
}

// the lengths of the literals' and the distances' codes, `literal_count` and `distance_count` of them, read into
// `lengths_out` through the code lengths' table: 1, or 0 with why given
EMBED_INLINE int cli_lengths_read(const ArchiveDeflateCtx *args)
{
    const unsigned int whole = args->literal_count + args->distance_count;
    unsigned int count = 0u;
    while (count < whole)
    {
        unsigned int value = 0u;
        unsigned int times = 0u;
        if (!EMBED_CALL(cli_length_symbol, ArchiveDeflateCtx, .bits = args->bits, .lengths_out = args->lengths_out,
                        .count = count, .value = &value, .symbol = &times, .err_msg = args->err_msg))
        {
            return 0;
        }
        unsigned int time = 0u;
        while ((time < times) && (count < 320u))
        {
            // a code length is below 16, a byte
            args->lengths_out[count] = (unsigned char)value;
            count += 1u;
            time += 1u;
        }
        if (count > whole)
        {
            mmgr_line_put(args->err_msg, "the code lengths overrun");
            return 0;
        }
    }
    return 1;
}

// a block of its own tables read: 1, or 0 with why given
static int cli_block_dynamic(const ArchiveDeflateCtx *args)
{
    static const unsigned char s_order[19] = {16u, 17u, 18u, 0u, 8u,  7u, 9u,  6u, 10u, 5u,
                                              11u, 4u,  12u, 3u, 13u, 2u, 14u, 1u, 15u};
    unsigned int literals = 0u;
    unsigned int distances = 0u;
    unsigned int code_lengths = 0u;
    int done = EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits, .count = 5u, .value = &literals) &&
               EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits, .count = 5u, .value = &distances) &&
               EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits, .count = 4u, .value = &code_lengths);
    literals += 257u;
    distances += 1u;
    code_lengths += 4u;
    unsigned char lengths[19];
    memor.set(&(MemoriaCfg){.dst = lengths, .val = 0, .bytes = sizeof(lengths)});
    unsigned int at = 0u;
    while (done && (at < code_lengths) && (at < 19u))
    {
        unsigned int value = 0u;
        done = EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = args->bits, .count = 3u, .value = &value);
        // a code length is below 8, a byte
        lengths[s_order[at]] = (unsigned char)value;
        at += 1u;
    }
    if (!done)
    {
        mmgr_line_put(args->err_msg, "the deflated data ends early");
        return 0;
    }
    EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = lengths, .count = 19u, .huffman = &s_lengths_code);
    unsigned char all[320];
    if (!EMBED_CALL(cli_lengths_read, ArchiveDeflateCtx, .bits = args->bits, .lengths_out = all,
                    .literal_count = literals, .distance_count = distances, .err_msg = args->err_msg))
    {
        return 0;
    }
    if ((literals > 288u) || (distances > 32u))
    {
        mmgr_line_put(args->err_msg, "the code lengths overrun");
        return 0;
    }
    EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = all, .count = literals, .huffman = &s_literals);
    EMBED_CALL(cli_huffman_get, ArchiveDeflateCtx, .lengths = all + literals, .count = distances,
               .huffman = &s_distances);
    return EMBED_CALL(cli_codes, ArchiveDeflateCtx, .bits = args->bits, .out = args->out, .capacity = args->capacity,
                      .size = args->size, .literals = &s_literals, .distances = &s_distances, .err_msg = args->err_msg);
}

int cli_inflate(const CliArchiveSizeCfg *args)
{
    static int (*const s_blocks[3])(const ArchiveDeflateCtx *) = {cli_block_stored, cli_block_fixed, cli_block_dynamic};
    CliBits bits = {args->data, args->size, 0ULL, 0u, 0u};
    *args->out_size = 0ULL;
    unsigned int last = 0u;
    while (last == 0u)
    {
        unsigned int type = 0u;
        if (!EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = &bits, .count = 1u, .value = &last) ||
            !EMBED_CALL(cli_alloc, ArchiveDeflateCtx, .bits = &bits, .count = 2u, .value = &type))
        {
            mmgr_line_put(args->err_msg, "the deflated data ends early");
            return 0;
        }
        if (type >= 3u)
        {
            mmgr_line_put(args->err_msg, "a block of a kind deflate has none of");
            return 0;
        }
        if (!EMBED_CALL(s_blocks[type], ArchiveDeflateCtx, .bits = &bits, .data = args->data, .data_size = args->size,
                        .out = args->out, .capacity = args->capacity, .size = args->out_size, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    *args->used = bits.at;
    return 1;
}
