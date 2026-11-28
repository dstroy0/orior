// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "answer_key.h"
#include "../../../../includes/platform/platform.h"

#include "track.h"

#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    long long identity;
    int place[4];
} AnswerKeyNode;

long node_index_of(const AnswerKey *key, long long identity)
{
    unsigned int low = 0u;
    unsigned int high = key->node_count;
    while (low < high)
    {
        const unsigned int middle = low + (high - low) / 2u;
        if (key->node_identity[middle] < identity)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < key->node_count) && (key->node_identity[low] == identity)) ? (long)low : -1L;
}

static int answer_key_order(const void *left, const void *right)
{
    const long long one = ((const AnswerKeyNode *)left)->identity;
    const long long other = ((const AnswerKeyNode *)right)->identity;
    return (one > other) - (one < other);
}

int answer_key_build(const AnswerKeyTruth *truth, AnswerKey *key)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = key, .bytes = sizeof(*key), .val = 0);
    const unsigned long long identity_limit = 0x7FFFFFFFFFFFFFFFull;
    int ok = (truth->nodes < 0xFFFFFFFFull) && (truth->edges < 0xFFFFFFFFull);
    AnswerKeyNode *const nodes =
        ok ? (AnswerKeyNode *)platform_cell_taken(((size_t)truth->nodes + 1u) * sizeof(AnswerKeyNode),
                                                  platform_place_g_program_owner, 0)
           : NULL;
    ok = ok && (nodes != NULL);
    for (unsigned long long node = 0ull; ok && (node < truth->nodes); node += 1ull)
    {
        ok = (truth->node_identity[node] <= identity_limit);
        nodes[node].identity = (long long)truth->node_identity[node];
        for (unsigned int axis = 0u; ok && (axis < 4u); axis += 1u)
        {
            const long long place = truth->node_place[(node * 4ull) + axis];
            ok = (place >= 0ll) && (place <= 0x7FFFFFFFll);
            nodes[node].place[axis] = (int)place;
        }
    }
    if (ok)
    {
        qsort(nodes, (size_t)truth->nodes, sizeof(AnswerKeyNode), answer_key_order);
    }
    for (unsigned long long node = 1ull; ok && (node < truth->nodes); node += 1ull)
    {
        ok = (nodes[node - 1ull].identity != nodes[node].identity);
    }
    key->node_count = ok ? (unsigned int)truth->nodes : 0u;
    key->edge_count = ok ? (unsigned int)truth->edges : 0u;
    key->node_identity = ok ? (long long *)platform_cell_taken(((size_t)key->node_count + 1u) * sizeof(long long),
                                                               platform_place_g_program_owner, 0)
                            : NULL;
    key->node_coordinates = ok ? (int *)platform_cell_taken(((size_t)key->node_count + 1u) * 4u * sizeof(int),
                                                            platform_place_g_program_owner, 0)
                               : NULL;
    key->edge_ends = ok ? (long long *)platform_cell_taken(((size_t)key->edge_count + 1u) * 2u * sizeof(long long),
                                                           platform_place_g_program_owner, 0)
                        : NULL;
    ok = ok && (key->node_identity != NULL) && (key->node_coordinates != NULL) && (key->edge_ends != NULL);
    for (unsigned int node = 0u; ok && (node < key->node_count); node += 1u)
    {
        key->node_identity[node] = nodes[node].identity;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &key->node_coordinates[(size_t)node * 4u], .src = nodes[node].place,
                   .bytes = sizeof(nodes[node].place));
    }
    for (size_t end = 0u; ok && (end < (size_t)key->edge_count * 2u); end += 1u)
    {
        ok = (truth->edge_ends[end] <= identity_limit);
        key->edge_ends[end] = (long long)truth->edge_ends[end];
    }
    platform_cell_released(nodes, platform_place_g_program_owner);
    if (ok == 0)
    {
        release_answer_key(key);
    }
    return ok;
}

void release_answer_key(AnswerKey *key)
{
    platform_cell_released(key->node_identity, platform_place_g_program_owner);
    platform_cell_released(key->node_coordinates, platform_place_g_program_owner);
    platform_cell_released(key->edge_ends, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = key, .bytes = sizeof(*key), .val = 0);
}
