// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deflate_tokens.c: matching, tokens and Huffman code lengths
#include "../../platform/platform.h"
#include "deflate_internal.h"
#include "memoria_operor/memoria_operor.h"

unsigned int deflate_length_code(unsigned int length)
{
    unsigned int code = DEFLATE_LENGTH_CODES - 1u;
    while (deflate_length_base[code] > length)
    {
        code -= 1u;
    }
    return code;
}

unsigned int deflate_distance_code(unsigned int distance)
{
    unsigned int code = DEFLATE_DISTANCE_CODES - 1u;
    while (deflate_distance_base[code] > distance)
    {
        code -= 1u;
    }
    return code;
}

static unsigned long long deflate_key(const DeflateMatcher *matcher, unsigned long long at)
{
    const unsigned long long key = ((unsigned long long)matcher->in[at] << 16u) |
                                   ((unsigned long long)matcher->in[at + 1ULL] << 8u) | matcher->in[at + 2ULL];
    unsigned long long folded = 0ULL;
    for (unsigned long long rest = key; rest != 0ULL; rest >>= matcher->key_bits)
    {
        folded ^= rest & matcher->mask;
    }
    return folded;
}

static void deflate_insert(DeflateMatcher *matcher, unsigned long long at)
{
    if ((matcher->in_bytes - at) < DEFLATE_MATCH_FLOOR)
    {
        return;
    }
    const unsigned long long code = deflate_key(matcher, at);
    matcher->previous[at] = matcher->heads[code];
    matcher->heads[code] = at + 1ULL;
}

static unsigned long long deflate_find(const DeflateMatcher *matcher, unsigned long long at,
                                       unsigned long long *distance)
{
    *distance = 0ULL;
    if ((matcher->in_bytes - at) < DEFLATE_MATCH_FLOOR)
    {
        return 0ULL;
    }
    const unsigned long long limit =
        ((matcher->in_bytes - at) < DEFLATE_MATCH_CEILING) ? (matcher->in_bytes - at) : DEFLATE_MATCH_CEILING;
    unsigned long long best = 0ULL;
    unsigned long long candidate = matcher->heads[deflate_key(matcher, at)];
    while (candidate != 0ULL)
    {
        const unsigned long long earlier = candidate - 1ULL;
        if ((at - earlier) > DEFLATE_WINDOW)
        {
            break;
        }
        unsigned long long length = 0ULL;
        while ((length < limit) && (matcher->in[earlier + length] == matcher->in[at + length]))
        {
            length += 1ULL;
        }
        if (length > best)
        {
            best = length;
            *distance = at - earlier;
            if (best == limit)
            {
                break;
            }
        }
        candidate = matcher->previous[earlier];
    }
    return (best >= DEFLATE_MATCH_FLOOR) ? best : 0ULL;
}

static void deflate_token(DeflateToken *tokens, unsigned long long *count, unsigned long long length,
                          unsigned long long value)
{
    // a match length is at max 258 and a distance at max 32768, each fits in an unsigned short
    tokens[*count].length = (unsigned short)length;
    // a literal byte or a distance of at max 32768 fits in an unsigned short
    tokens[*count].value = (unsigned short)value;
    *count += 1ULL;
}

unsigned long long deflate_parse(DeflateMatcher *matcher, DeflateToken *tokens)
{
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while (at < matcher->in_bytes)
    {
        unsigned long long distance = 0ULL;
        const unsigned long long length = deflate_find(matcher, at, &distance);
        if (length == 0ULL)
        {
            deflate_token(tokens, &count, 0ULL, matcher->in[at]);
            deflate_insert(matcher, at);
            at += 1ULL;
            continue;
        }
        deflate_insert(matcher, at);
        unsigned long long later_distance = 0ULL;
        const unsigned long long later =
            ((at + 1ULL) < matcher->in_bytes) ? deflate_find(matcher, at + 1ULL, &later_distance) : 0ULL;
        if (later > length)
        {
            deflate_token(tokens, &count, 0ULL, matcher->in[at]);
            at += 1ULL;
            continue;
        }
        deflate_token(tokens, &count, length, distance);
        for (unsigned long long step = 1ULL; step < length; step += 1ULL)
        {
            deflate_insert(matcher, at + step);
        }
        at += length;
    }
    return count;
}

static unsigned int deflate_count_leaves(const DeflateNode *nodes, unsigned int node, unsigned char *lengths)
{
    if (nodes[node].left == nodes[node].right)
    {
        lengths[nodes[node].leaf] += 1u;
        return 1u;
    }
    return deflate_count_leaves(nodes, nodes[node].left, lengths) +
           deflate_count_leaves(nodes, nodes[node].right, lengths);
}

int deflate_lengths(const unsigned long long *frequency, unsigned int count, unsigned int limit, unsigned char *lengths)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = lengths, .bytes = count, .val = 0);
    unsigned int used = 0u;
    unsigned int only = 0u;
    for (unsigned int symbol = 0u; symbol < count; symbol += 1u)
    {
        used += (frequency[symbol] != 0ULL) ? 1u : 0u;
        only = (frequency[symbol] != 0ULL) ? symbol : only;
    }
    if (used < 2u)
    {
        lengths[only] = 1u;
        lengths[(only == 0u) ? 1u : 0u] = 1u;
        return 1;
    }
    const unsigned int capacity = limit * 2u * used;
    DeflateNode *const nodes = (DeflateNode *)platform_cell_taken((unsigned long long)(sizeof(DeflateNode) * capacity),
                                                                  (unsigned int)platform_place_g_program_owner, 0);
    unsigned int *const leaves = (unsigned int *)platform_cell_taken((unsigned long long)(sizeof(unsigned int) * used),
                                                                     (unsigned int)platform_place_g_program_owner, 0);
    unsigned int *const list = (unsigned int *)platform_cell_taken(
        (unsigned long long)(sizeof(unsigned int) * 2u * used), (unsigned int)platform_place_g_program_owner, 0);
    unsigned int *const next = (unsigned int *)platform_cell_taken(
        (unsigned long long)(sizeof(unsigned int) * 2u * used), (unsigned int)platform_place_g_program_owner, 0);
    if ((nodes == NULL) || (leaves == NULL) || (list == NULL) || (next == NULL))
    {
        platform_cell_released(nodes, (unsigned int)platform_place_g_program_owner);
        platform_cell_released(leaves, (unsigned int)platform_place_g_program_owner);
        platform_cell_released(list, (unsigned int)platform_place_g_program_owner);
        platform_cell_released(next, (unsigned int)platform_place_g_program_owner);
        return 0;
    }
    unsigned int made = 0u;
    for (unsigned int symbol = 0u; symbol < count; symbol += 1u)
    {
        if (frequency[symbol] == 0ULL)
        {
            continue;
        }
        unsigned int place = made;
        while ((place > 0u) && (nodes[leaves[place - 1u]].weight > frequency[symbol]))
        {
            leaves[place] = leaves[place - 1u];
            place -= 1u;
        }
        nodes[made].weight = frequency[symbol];
        nodes[made].leaf = symbol;
        nodes[made].left = 0u;
        nodes[made].right = 0u;
        leaves[place] = made;
        made += 1u;
    }
    unsigned int listed = used;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = list, .src = leaves, .bytes = sizeof(unsigned int) * used);
    for (unsigned int level = 1u; level < limit; level += 1u)
    {
        const unsigned int packages = listed / 2u;
        const unsigned int package_first = made;
        for (unsigned int package = 0u; package < packages; package += 1u)
        {
            nodes[made].weight = nodes[list[2u * package]].weight + nodes[list[(2u * package) + 1u]].weight;
            nodes[made].leaf = 0u;
            nodes[made].left = list[2u * package];
            nodes[made].right = list[(2u * package) + 1u];
            made += 1u;
        }
        unsigned int leaf_at = 0u;
        unsigned int package_at = 0u;
        listed = 0u;
        while ((leaf_at < used) || (package_at < packages))
        {
            const int take_leaf =
                (package_at == packages) ||
                ((leaf_at < used) && (nodes[leaves[leaf_at]].weight <= nodes[package_first + package_at].weight));
            next[listed] = take_leaf ? leaves[leaf_at] : (package_first + package_at);
            leaf_at += take_leaf ? 1u : 0u;
            package_at += take_leaf ? 0u : 1u;
            listed += 1u;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = list, .src = next, .bytes = sizeof(unsigned int) * listed);
    }
    for (unsigned int item = 0u; item < ((2u * used) - 2u); item += 1u)
    {
        deflate_count_leaves(nodes, list[item], lengths);
    }
    platform_cell_released(nodes, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(leaves, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(list, (unsigned int)platform_place_g_program_owner);
    platform_cell_released(next, (unsigned int)platform_place_g_program_owner);
    return 1;
}

void deflate_codes(const unsigned char *lengths, unsigned int count, unsigned int *codes)
{
    unsigned int per_length[DEFLATE_CODE_BITS + 1u];
    unsigned int next_code[DEFLATE_CODE_BITS + 1u];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = per_length, .bytes = sizeof(per_length), .val = 0);
    for (unsigned int symbol = 0u; symbol < count; symbol += 1u)
    {
        per_length[lengths[symbol]] += 1u;
    }
    per_length[0] = 0u;
    unsigned int code = 0u;
    next_code[0] = 0u;
    for (unsigned int length = 1u; length <= DEFLATE_CODE_BITS; length += 1u)
    {
        code = (code + per_length[length - 1u]) << 1u;
        next_code[length] = code;
    }
    for (unsigned int symbol = 0u; symbol < count; symbol += 1u)
    {
        const unsigned int length = lengths[symbol];
        codes[symbol] = 0u;
        if (length == 0u)
        {
            continue;
        }
        const unsigned int canonical = next_code[length];
        next_code[length] += 1u;
        for (unsigned int bit = 0u; bit < length; bit += 1u)
        {
            codes[symbol] |= ((canonical >> bit) & 1u) << (length - 1u - bit);
        }
    }
}
