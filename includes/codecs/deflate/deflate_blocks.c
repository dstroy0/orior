// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deflate_blocks.c: runs, trees and writing the blocks
#include "../../platform/platform.h"
#include "deflate_internal.h"
#include "memoria_operor/memoria_operor.h"

static const unsigned char deflate_length_extra[DEFLATE_LENGTH_CODES] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 3u, 3u, 3u, 3u, 4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u};

static const unsigned char deflate_distance_extra[DEFLATE_DISTANCE_CODES] = {
    0u, 0u, 0u, 0u, 1u, 1u, 2u, 2u,  3u,  3u,  4u,  4u,  5u,  5u,  6u,
    6u, 7u, 7u, 8u, 8u, 9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u};

static const unsigned char deflate_code_length_order[DEFLATE_CODE_LENGTH_CODES] = {
    16u, 17u, 18u, 0u, 8u, 7u, 9u, 6u, 10u, 5u, 11u, 4u, 12u, 3u, 13u, 2u, 14u, 1u, 15u};

static void deflate_run(DeflateTrees *trees, unsigned int symbol, unsigned int extra)
{
    // a code length symbol is below 19 and its extra value below 128, each fits in an unsigned char
    trees->run_symbols[trees->run_count] = (unsigned char)symbol;
    // the extra value is below 128 and fits in an unsigned char
    trees->run_extras[trees->run_count] = (unsigned char)extra;
    trees->run_count += 1u;
}

static void deflate_runs(DeflateTrees *trees)
{
    const unsigned int total = trees->literal_count + trees->distance_count;
    unsigned char joined[DEFLATE_LITERAL_CODES + DEFLATE_DISTANCE_CODES];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = joined, .src = trees->lengths, .bytes = trees->literal_count);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = joined + trees->literal_count,
               .src = trees->lengths + DEFLATE_LITERAL_CODES, .bytes = trees->distance_count);
    trees->run_count = 0u;
    unsigned int at = 0u;
    while (at < total)
    {
        const unsigned int value = joined[at];
        unsigned int run = 1u;
        while (((at + run) < total) && (joined[at + run] == value))
        {
            run += 1u;
        }
        unsigned int rest = run;
        if (value == 0u)
        {
            while (rest >= 11u)
            {
                const unsigned int take = (rest < 138u) ? rest : 138u;
                deflate_run(trees, 18u, take - 11u);
                rest -= take;
            }
            if (rest >= 3u)
            {
                deflate_run(trees, 17u, rest - 3u);
                rest = 0u;
            }
        }
        else
        {
            deflate_run(trees, value, 0u);
            rest -= 1u;
            while (rest >= 3u)
            {
                const unsigned int take = (rest < 6u) ? rest : 6u;
                deflate_run(trees, 16u, take - 3u);
                rest -= take;
            }
        }
        while (rest > 0u)
        {
            deflate_run(trees, value, 0u);
            rest -= 1u;
        }
        at += run;
    }
}

static unsigned int deflate_run_extra_bits(unsigned int symbol)
{
    return (symbol == 16u) ? 2u : ((symbol == 17u) ? 3u : ((symbol == 18u) ? 7u : 0u));
}

static int deflate_trees(const DeflateToken *tokens, unsigned long long count, DeflateTrees *trees)
{
    unsigned long long literal_frequency[DEFLATE_LITERAL_CODES];
    unsigned long long distance_frequency[DEFLATE_DISTANCE_CODES];
    unsigned long long code_length_frequency[DEFLATE_CODE_LENGTH_CODES];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = literal_frequency, .bytes = sizeof(literal_frequency), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = distance_frequency, .bytes = sizeof(distance_frequency), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = code_length_frequency, .bytes = sizeof(code_length_frequency), .val = 0);
    for (unsigned long long token = 0ULL; token < count; token += 1ULL)
    {
        if (tokens[token].length == 0u)
        {
            literal_frequency[tokens[token].value] += 1ULL;
            continue;
        }
        literal_frequency[DEFLATE_END_OF_BLOCK + 1u + deflate_length_code(tokens[token].length)] += 1ULL;
        distance_frequency[deflate_distance_code(tokens[token].value)] += 1ULL;
    }
    literal_frequency[DEFLATE_END_OF_BLOCK] += 1ULL;
    if (!deflate_lengths(literal_frequency, DEFLATE_LITERAL_CODES, DEFLATE_CODE_BITS, trees->lengths) ||
        !deflate_lengths(distance_frequency, DEFLATE_DISTANCE_CODES, DEFLATE_CODE_BITS,
                         trees->lengths + DEFLATE_LITERAL_CODES))
    {
        return 0;
    }
    trees->literal_count = DEFLATE_LITERAL_CODES;
    while ((trees->literal_count > (DEFLATE_END_OF_BLOCK + 1u)) && (trees->lengths[trees->literal_count - 1u] == 0u))
    {
        trees->literal_count -= 1u;
    }
    trees->distance_count = DEFLATE_DISTANCE_CODES;
    while ((trees->distance_count > 1u) && (trees->lengths[DEFLATE_LITERAL_CODES + trees->distance_count - 1u] == 0u))
    {
        trees->distance_count -= 1u;
    }
    deflate_codes(trees->lengths, DEFLATE_LITERAL_CODES, trees->codes);
    deflate_codes(trees->lengths + DEFLATE_LITERAL_CODES, DEFLATE_DISTANCE_CODES, trees->codes + DEFLATE_LITERAL_CODES);
    deflate_runs(trees);
    for (unsigned int run = 0u; run < trees->run_count; run += 1u)
    {
        code_length_frequency[trees->run_symbols[run]] += 1ULL;
    }
    if (!deflate_lengths(code_length_frequency, DEFLATE_CODE_LENGTH_CODES, DEFLATE_CODE_LENGTH_BITS,
                         trees->code_length_lengths))
    {
        return 0;
    }
    deflate_codes(trees->code_length_lengths, DEFLATE_CODE_LENGTH_CODES, trees->code_length_codes);
    trees->code_length_count = DEFLATE_CODE_LENGTH_CODES;
    while ((trees->code_length_count > 4u) &&
           (trees->code_length_lengths[deflate_code_length_order[trees->code_length_count - 1u]] == 0u))
    {
        trees->code_length_count -= 1u;
    }
    return 1;
}

static unsigned long long deflate_symbol_bits(const DeflateToken *tokens, unsigned long long count,
                                              const DeflateTrees *trees)
{
    unsigned long long bits = trees->lengths[DEFLATE_END_OF_BLOCK];
    for (unsigned long long token = 0ULL; token < count; token += 1ULL)
    {
        if (tokens[token].length == 0u)
        {
            bits += trees->lengths[tokens[token].value];
            continue;
        }
        const unsigned int length_code = deflate_length_code(tokens[token].length);
        const unsigned int distance_code = deflate_distance_code(tokens[token].value);
        bits += trees->lengths[DEFLATE_END_OF_BLOCK + 1u + length_code] + deflate_length_extra[length_code] +
                trees->lengths[DEFLATE_LITERAL_CODES + distance_code] + deflate_distance_extra[distance_code];
    }
    return bits;
}

static unsigned long long deflate_dynamic_bits(const DeflateToken *tokens, unsigned long long count,
                                               const DeflateTrees *trees)
{
    unsigned long long bits = 3ULL + 5ULL + 5ULL + 4ULL + (3ULL * trees->code_length_count);
    for (unsigned int run = 0u; run < trees->run_count; run += 1u)
    {
        bits += trees->code_length_lengths[trees->run_symbols[run]] + deflate_run_extra_bits(trees->run_symbols[run]);
    }
    return bits + deflate_symbol_bits(tokens, count, trees);
}

static void deflate_fixed_trees(DeflateTrees *fixed)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = fixed, .bytes = sizeof(*fixed), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = fixed->lengths, .bytes = 144u, .val = 8);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = fixed->lengths + 144u, .bytes = 112u, .val = 9);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = fixed->lengths + 256u, .bytes = 24u, .val = 7);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = fixed->lengths + 280u, .bytes = DEFLATE_LITERAL_CODES - 280u, .val = 8);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = fixed->lengths + DEFLATE_LITERAL_CODES, .bytes = DEFLATE_DISTANCE_CODES,
               .val = 5);
    unsigned char literal_lengths[DEFLATE_LITERAL_CODES + 2u];
    unsigned int literal_codes[DEFLATE_LITERAL_CODES + 2u];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = literal_lengths, .src = fixed->lengths, .bytes = DEFLATE_LITERAL_CODES);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = literal_lengths + DEFLATE_LITERAL_CODES, .bytes = 2u, .val = 8);
    deflate_codes(literal_lengths, DEFLATE_LITERAL_CODES + 2u, literal_codes);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = fixed->codes, .src = literal_codes,
               .bytes = sizeof(unsigned int) * DEFLATE_LITERAL_CODES);
    deflate_codes(fixed->lengths + DEFLATE_LITERAL_CODES, DEFLATE_DISTANCE_CODES, fixed->codes + DEFLATE_LITERAL_CODES);
}

static void deflate_put(DeflateWriter *writer, unsigned int value, unsigned int count)
{
    writer->bits |= (unsigned long long)value << writer->count;
    writer->count += count;
    while (writer->count >= 8u)
    {
        if (writer->written == writer->capacity)
        {
            writer->overflow = 1;
            return;
        }
        // the low byte of the accumulator is taken whole
        writer->out[writer->written] = (unsigned char)(writer->bits & 0xFFULL);
        writer->written += 1ULL;
        writer->bits >>= 8u;
        writer->count -= 8u;
    }
}

static void deflate_align(DeflateWriter *writer)
{
    if (writer->count != 0u)
    {
        deflate_put(writer, 0u, 8u - writer->count);
    }
}

static void deflate_write_symbols(DeflateWriter *writer, const DeflateToken *tokens, unsigned long long count,
                                  const DeflateTrees *trees)
{
    for (unsigned long long token = 0ULL; (token < count) && (writer->overflow == 0); token += 1ULL)
    {
        if (tokens[token].length == 0u)
        {
            deflate_put(writer, trees->codes[tokens[token].value], trees->lengths[tokens[token].value]);
            continue;
        }
        const unsigned int length_code = deflate_length_code(tokens[token].length);
        const unsigned int distance_code = deflate_distance_code(tokens[token].value);
        const unsigned int length_symbol = DEFLATE_END_OF_BLOCK + 1u + length_code;
        const unsigned int distance_symbol = DEFLATE_LITERAL_CODES + distance_code;
        deflate_put(writer, trees->codes[length_symbol], trees->lengths[length_symbol]);
        deflate_put(writer, tokens[token].length - deflate_length_base[length_code], deflate_length_extra[length_code]);
        deflate_put(writer, trees->codes[distance_symbol], trees->lengths[distance_symbol]);
        deflate_put(writer, tokens[token].value - deflate_distance_base[distance_code],
                    deflate_distance_extra[distance_code]);
    }
    deflate_put(writer, trees->codes[DEFLATE_END_OF_BLOCK], trees->lengths[DEFLATE_END_OF_BLOCK]);
    deflate_align(writer);
}

static void deflate_write_dynamic(DeflateWriter *writer, const DeflateToken *tokens, unsigned long long count,
                                  const DeflateTrees *trees)
{
    deflate_put(writer, 1u, 1u);
    deflate_put(writer, 2u, 2u);
    deflate_put(writer, trees->literal_count - 257u, 5u);
    deflate_put(writer, trees->distance_count - 1u, 5u);
    deflate_put(writer, trees->code_length_count - 4u, 4u);
    for (unsigned int at = 0u; at < trees->code_length_count; at += 1u)
    {
        deflate_put(writer, trees->code_length_lengths[deflate_code_length_order[at]], 3u);
    }
    for (unsigned int run = 0u; run < trees->run_count; run += 1u)
    {
        const unsigned int symbol = trees->run_symbols[run];
        deflate_put(writer, trees->code_length_codes[symbol], trees->code_length_lengths[symbol]);
        deflate_put(writer, trees->run_extras[run], deflate_run_extra_bits(symbol));
    }
    deflate_write_symbols(writer, tokens, count, trees);
}

static void deflate_write_fixed(DeflateWriter *writer, const DeflateToken *tokens, unsigned long long count,
                                const DeflateTrees *fixed)
{
    deflate_put(writer, 1u, 1u);
    deflate_put(writer, 1u, 2u);
    deflate_write_symbols(writer, tokens, count, fixed);
}

static void deflate_write_stored(DeflateWriter *writer, const unsigned char *in, unsigned long long in_bytes)
{
    unsigned long long at = 0ULL;
    do
    {
        const unsigned long long take =
            ((in_bytes - at) < DEFLATE_STORED_CEILING) ? (in_bytes - at) : DEFLATE_STORED_CEILING;
        const unsigned int last = ((at + take) == in_bytes) ? 1u : 0u;
        deflate_put(writer, last, 1u);
        deflate_put(writer, 0u, 2u);
        deflate_align(writer);
        // a stored block's length is at max 65535, which fits in an unsigned int
        const unsigned int length = (unsigned int)take;
        deflate_put(writer, length, 16u);
        deflate_put(writer, length ^ 0xFFFFu, 16u);
        if ((writer->capacity - writer->written) < take)
        {
            writer->overflow = 1;
            return;
        }
        if (take != 0ULL)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = writer->out + writer->written, .src = in + at,
                       .bytes = (size_t)take);
        }
        writer->written += take;
        at += take;
    } while ((at < in_bytes) && (writer->overflow == 0));
}

unsigned long long deflate_raw_bound(unsigned long long in_bytes)
{
    const unsigned long long blocks = (in_bytes == 0ULL) ? 1ULL : (((in_bytes - 1ULL) / DEFLATE_STORED_CEILING) + 1ULL);
    return in_bytes + (DEFLATE_STORED_FRAME * blocks);
}

long long deflate_raw_encode(const EngineBytesRequest *request)
{
    if ((request == NULL) || ((request->in == NULL) && (request->in_bytes != 0ULL)) || (request->out == NULL))
    {
        return ENGINE_BYTES_ERROR;
    }
    const unsigned long long in_bytes = request->in_bytes;
    const unsigned long long range = (in_bytes < DEFLATE_KEYS) ? in_bytes : DEFLATE_KEYS;
    unsigned int key_bits = 0u;
    while ((1ULL << key_bits) < range)
    {
        key_bits += 1u;
    }
    const unsigned long long slots = 1ULL << key_bits;
    DeflateMatcher matcher;
    matcher.in = request->in;
    matcher.in_bytes = in_bytes;
    matcher.heads = (unsigned long long *)platform_cell_cleared((size_t)slots, sizeof(unsigned long long),
                                                                platform_place_g_program_owner, 0);
    matcher.previous = (unsigned long long *)platform_cell_cleared((size_t)in_bytes + 1u, sizeof(unsigned long long),
                                                                   platform_place_g_program_owner, 0);
    matcher.mask = slots - 1ULL;
    matcher.key_bits = (key_bits == 0u) ? 1u : key_bits;
    DeflateToken *const tokens =
        (DeflateToken *)platform_cell_taken((unsigned long long)(((size_t)in_bytes + 1u) * sizeof(DeflateToken)),
                                            (unsigned int)platform_place_g_program_owner, 0);
    DeflateTrees *const trees =
        (DeflateTrees *)platform_cell_cleared((unsigned long long)(1u), (unsigned long long)(sizeof(DeflateTrees)),
                                              (unsigned int)platform_place_g_program_owner, 0);
    DeflateTrees *const fixed =
        (DeflateTrees *)platform_cell_cleared((unsigned long long)(1u), (unsigned long long)(sizeof(DeflateTrees)),
                                              (unsigned int)platform_place_g_program_owner, 0);
    int ok =
        (matcher.heads != NULL) && (matcher.previous != NULL) && (tokens != NULL) && (trees != NULL) && (fixed != NULL);
    const unsigned long long count = ok ? deflate_parse(&matcher, tokens) : 0ULL;
    ok = ok && deflate_trees(tokens, count, trees);
    if (ok)
    {
        deflate_fixed_trees(fixed);
    }
    const unsigned long long dynamic_bytes = ok ? ((deflate_dynamic_bits(tokens, count, trees) + 7ULL) / 8ULL) : 0ULL;
    const unsigned long long fixed_bytes =
        ok ? ((3ULL + deflate_symbol_bits(tokens, count, fixed) + 7ULL) / 8ULL) : 0ULL;
    const unsigned long long stored_bytes = deflate_raw_bound(in_bytes);
    const int use_fixed = (fixed_bytes < dynamic_bytes);
    const unsigned long long coded_bytes = use_fixed ? fixed_bytes : dynamic_bytes;
    const int stored = (stored_bytes < coded_bytes);
    const unsigned long long chosen = stored ? stored_bytes : coded_bytes;
    ok = ok && (chosen <= request->out_capacity);
    DeflateWriter writer = {request->out, request->out_capacity, 0ULL, 0ULL, 0u, 0};
    if (ok && stored)
    {
        deflate_write_stored(&writer, request->in, in_bytes);
    }
    else if (ok && use_fixed)
    {
        deflate_write_fixed(&writer, tokens, count, fixed);
    }
    else if (ok)
    {
        deflate_write_dynamic(&writer, tokens, count, trees);
    }
    ok = ok && (writer.overflow == 0) && (writer.written == chosen);
    platform_cell_released(matcher.heads, platform_place_g_program_owner);
    platform_cell_released(matcher.previous, platform_place_g_program_owner);
    platform_cell_released(tokens, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(trees, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(fixed, (unsigned int)platform_place_g_program_owner);
    // the written count is at max the caller's capacity, a size in memory, and fits a long long
    return ok ? (long long)writer.written : ENGINE_BYTES_ERROR;
}
