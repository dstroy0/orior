// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// max_tree_nodes.c: nodes and probes
#include "max_tree_internal.h"
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"

static void max_tree_part(const unsigned int *residual, const MaxTree *theirs, const MaxTree *mine)
{
    unsigned int *const place =
        (unsigned int *)platform_cell_taken((unsigned long long)((size_t)theirs->voxels * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, 0);
    if (place == NULL)
    {
        return;
    }
    for (unsigned int at = 0u; at < theirs->admitted; at += 1u)
    {
        place[theirs->order[at]] = at;
    }
    unsigned int differing = 0u;
    unsigned int first = MAX_TREE_ABSENT;
    for (unsigned int at = 0u; at < theirs->admitted; at += 1u)
    {
        const unsigned int voxel = theirs->order[at];
        const unsigned int parts = (unsigned int)(theirs->parent[voxel] != mine->parent[voxel]);
        differing += parts;
        first = ((parts != 0u) && (first == MAX_TREE_ABSENT)) ? voxel : first;
    }
    if (first != MAX_TREE_ABSENT)
    {
        const unsigned int their_parent = theirs->parent[first];
        const unsigned int my_parent = mine->parent[first];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    part: ");
            mmgr_line_decimal(output_line, differing, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, theirs->admitted, 1u);
            mmgr_line_put(output_line, " voxels differ; first voxel ");
            mmgr_line_decimal(output_line, first, 1u);
            mmgr_line_put(output_line, " at ");
            mmgr_line_decimal(output_line, place[first], 1u);
            mmgr_line_put(output_line, ", reference parent ");
            mmgr_line_decimal(output_line, their_parent, 1u);
            mmgr_line_put(output_line, " at ");
            mmgr_line_decimal(output_line, place[their_parent], 1u);
            mmgr_line_put(output_line, ", queried parent ");
            mmgr_line_decimal(output_line, my_parent, 1u);
            mmgr_line_put(output_line, " at ");
            mmgr_line_decimal(output_line, place[my_parent], 1u);
            mmgr_line_put(output_line, "; voxel level equals reference parent ");
            mmgr_line_decimal_signed(output_line, max_tree_same(residual, first, their_parent));
            mmgr_line_put(output_line, ", queried parent ");
            mmgr_line_decimal_signed(output_line, max_tree_same(residual, first, my_parent));
            mmgr_line_put(output_line, "; parents equal ");
            mmgr_line_decimal_signed(output_line, max_tree_same(residual, their_parent, my_parent));
            mmgr_line_put(output_line, "; reference parent's parent ");
            mmgr_line_decimal(output_line, theirs->parent[their_parent], 1u);
            mmgr_line_put(output_line, ", queried parent's parent ");
            mmgr_line_decimal(output_line, mine->parent[my_parent], 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    platform_cell_released(place, (unsigned int)platform_place_g_engine_owner);
}

static void max_tree_key_encode(const unsigned int *residual, unsigned int weaker, unsigned int name, unsigned int *key)
{
    const unsigned int *const limbs = &residual[(size_t)weaker * ENGINE_RESIDUAL_LIMBS];
    key[0] = ~name;
    for (unsigned int limb = 0u; limb < ENGINE_RESIDUAL_LIMBS; limb += 1u)
    {
        key[limb + 1u] = limbs[limb];
    }
}

static void max_tree_key_exact(const unsigned int *key, OriorExactInteger *value)
{
    unsigned int any = 0u;
    for (unsigned int limb = 0u; limb < MAX_TREE_KEY_LIMBS; limb += 1u)
    {
        value->limb[limb] = key[limb];
        any |= key[limb];
    }
    value->sign = (any != 0u) ? 1 : 0;
}

static int max_tree_stronger(const unsigned int *keys, unsigned int face, unsigned int located)
{
    max_tree_compare_ready();
    max_tree_key_exact(&keys[(size_t)face * MAX_TREE_KEY_LIMBS], &g_compare_left);
    max_tree_key_exact(&keys[(size_t)located * MAX_TREE_KEY_LIMBS], &g_compare_right);
    return (orior_exact_compare(&g_compare_left, &g_compare_right) > 0) ? 1 : 0;
}

int max_tree_poc(const unsigned int *residual, unsigned int depth, unsigned int height, unsigned int width,
                 unsigned char *bound, unsigned int *rounds)
{
    MaxTree reference;
    if ((bound == NULL) || (max_tree_build(residual, depth, height, width, &reference) < 0L))
    {
        return 0;
    }
    const size_t voxels = (size_t)depth * height * width;
    const size_t plane = (size_t)height * width;
    mmgr_bytes_set(bound, 0, voxels * 3u);
    unsigned int *const left = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * 3u * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const right = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * 3u * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const axes = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * 3u * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const keys = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * 3u * MAX_TREE_KEY_LIMBS * sizeof(unsigned int)),
        (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const belongs = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const strongest = (unsigned int *)platform_cell_taken(
        (unsigned long long)(voxels * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    int ok = (left != NULL) && (right != NULL) && (axes != NULL) && (keys != NULL) && (belongs != NULL) &&
             (strongest != NULL);
    unsigned int faces = 0u;
    for (size_t voxel = 0u; (ok != 0) && (voxel < voxels); voxel += 1u)
    {
        const unsigned int here = (unsigned int)voxel;
        if (max_tree_selects(residual, here) == 0)
        {
            continue;
        }
        const long long z = (long long)(voxel / plane);
        const long long y = (long long)((voxel % plane) / width);
        const long long x = (long long)((voxel % plane) % width);
        const long long steps[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        for (unsigned int step = 0u; step < 3u; step += 1u)
        {
            const long long near_z = z + steps[step][0];
            const long long near_y = y + steps[step][1];
            const long long near_x = x + steps[step][2];
            if ((near_z >= (long long)depth) || (near_y >= (long long)height) || (near_x >= (long long)width))
            {
                continue;
            }
            const unsigned int near =
                (unsigned int)(((near_z * (long long)height) + near_y) * (long long)width + near_x);
            if (max_tree_selects(residual, near) == 0)
            {
                continue;
            }
            left[faces] = here;
            right[faces] = near;
            axes[faces] = step;
            const unsigned int weaker = (max_tree_before(residual, here, near) < 0) ? near : here;
            max_tree_key_encode(residual, weaker, (here * 3u) + step, &keys[(size_t)faces * MAX_TREE_KEY_LIMBS]);
            faces += 1u;
        }
    }
    for (size_t voxel = 0u; (ok != 0) && (voxel < voxels); voxel += 1u)
    {
        belongs[voxel] = (unsigned int)voxel;
    }
    unsigned int turns = 0u;
    unsigned int moving = 1u;
    while ((ok != 0) && (moving != 0u))
    {
        moving = 0u;
        turns += 1u;
        for (size_t voxel = 0u; voxel < voxels; voxel += 1u)
        {
            strongest[voxel] = MAX_TREE_ABSENT;
        }
        for (unsigned int face = 0u; face < faces; face += 1u)
        {
            const unsigned int one = max_tree_root(belongs, left[face]);
            const unsigned int other = max_tree_root(belongs, right[face]);
            if (one == other)
            {
                continue;
            }
            const unsigned int located_one = strongest[one];
            const unsigned int located_other = strongest[other];
            if ((located_one == MAX_TREE_ABSENT) || (max_tree_stronger(keys, face, located_one) != 0))
            {
                strongest[one] = face;
            }
            if ((located_other == MAX_TREE_ABSENT) || (max_tree_stronger(keys, face, located_other) != 0))
            {
                strongest[other] = face;
            }
        }
        for (size_t voxel = 0u; voxel < voxels; voxel += 1u)
        {
            if (strongest[voxel] == MAX_TREE_ABSENT)
            {
                continue;
            }
            const unsigned int face = strongest[voxel];
            const unsigned int one = max_tree_root(belongs, left[face]);
            const unsigned int other = max_tree_root(belongs, right[face]);
            if (one == other)
            {
                continue;
            }
            belongs[(one < other) ? other : one] = (one < other) ? one : other;
            bound[((size_t)left[face] * 3u) + axes[face]] = 1u;
            moving += 1u;
        }
    }
    if (rounds != NULL)
    {
        *rounds = turns;
    }

    MaxTree tree;
    mmgr_bytes_set(&tree, 0, sizeof(tree));
    ok = (ok != 0) && (max_tree_build_bound(residual, depth, height, width, bound, &tree) >= 0L);
    const int built = ok;
    ok = (ok != 0) && (max_tree_equal(&reference, &tree) != 0);
    if ((built != 0) && (ok == 0))
    {
        max_tree_part(residual, &reference, &tree);
    }
    max_tree_release(&tree);
    platform_cell_released(left, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(right, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(axes, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(keys, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(belongs, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(strongest, (unsigned int)platform_place_g_engine_owner);
    max_tree_release(&reference);
    return ok;
}

int max_tree_key_compare(const void *left, const void *right)
{
    const unsigned long long one = *(const unsigned long long *)left;
    const unsigned long long other = *(const unsigned long long *)right;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

void max_tree_nodes_release(MaxTreeNodes *nodes)
{
    if (nodes == NULL)
    {
        return;
    }
    platform_cell_released(nodes->voxel, platform_place_g_program_owner);
    platform_cell_released(nodes->parent, platform_place_g_program_owner);
    platform_cell_released(nodes->level, platform_place_g_program_owner);
    platform_cell_released(nodes->subtree_end, platform_place_g_program_owner);
    platform_cell_released(nodes->root, platform_place_g_program_owner);
    platform_cell_released(nodes->own, platform_place_g_program_owner);
    mmgr_bytes_set(nodes, 0, sizeof(*nodes));
}

// the node arrays numbered in DFS order from the canonical voxels: `node_of` names each canonical voxel's place among
// them in voxel order, `rank` each admitted voxel's dense rank; returns 0 where a voxel's parent is not canonical
static int max_tree_nodes_build(const MaxTree *tree, const unsigned int *node_of, const unsigned int *rank,
                                unsigned int count, MaxTreeNodes *nodes, EngineError *error)
{
    const size_t voxels = tree->voxels;
    unsigned int *const starts = (unsigned int *)platform_cell_cleared((unsigned long long)((size_t)count + 2u),
                                                                       (unsigned long long)(sizeof(unsigned int)),
                                                                       (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const children =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)count + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const canonical =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)count + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const filled =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)count + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const placed =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)count + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const waiting =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)count + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, 0);
    int ok = MAX_TREE_HOST_CHECK((starts != NULL) && (children != NULL) && (canonical != NULL) && (filled != NULL) &&
                                     (placed != NULL) && (waiting != NULL),
                                 tree, error, ENGINE_ERROR_RESOURCE);
    for (size_t voxel = 0u; (ok != 0) && (voxel < voxels); voxel += 1u)
    {
        if (node_of[voxel] == MAX_TREE_ABSENT)
        {
            continue;
        }
        // a voxel index is below the tree's voxel count, which max_tree_grow bounds below 2^32 - 1
        canonical[node_of[voxel]] = (unsigned int)voxel;
        const unsigned int above = tree->parent[voxel];
        if (above == voxel)
        {
            continue;
        }
        // a canonical voxel's parent is the canonical voxel of the node below it
        ok = MAX_TREE_HOST_CHECK(node_of[above] != MAX_TREE_ABSENT, &tree->parent[voxel], error, ENGINE_ERROR_LOGIC);
        starts[node_of[above] + 1u] += (ok != 0) ? 1u : 0u;
    }
    for (unsigned int node = 0u; (ok != 0) && (node < count); node += 1u)
    {
        starts[node + 1u] += starts[node];
    }
    for (unsigned int node = 0u; (ok != 0) && (node < count); node += 1u)
    {
        filled[node] = starts[node];
    }
    for (unsigned int node = 0u; (ok != 0) && (node < count); node += 1u)
    {
        const unsigned int voxel = canonical[node];
        const unsigned int above = tree->parent[voxel];
        if (above != voxel)
        {
            children[filled[node_of[above]]] = node;
            filled[node_of[above]] += 1u;
        }
    }
    unsigned int waiting_count = 0u;
    for (unsigned int node = count; (ok != 0) && (node > 0u); node -= 1u)
    {
        if (tree->parent[canonical[node - 1u]] == canonical[node - 1u])
        {
            waiting[waiting_count] = node - 1u;
            waiting_count += 1u;
        }
    }
    unsigned int next = 0u;
    while ((ok != 0) && (waiting_count != 0u))
    {
        waiting_count -= 1u;
        const unsigned int node = waiting[waiting_count];
        placed[node] = next;
        next += 1u;
        for (unsigned int child = starts[node + 1u]; child > starts[node]; child -= 1u)
        {
            waiting[waiting_count] = children[child - 1u];
            waiting_count += 1u;
        }
    }
    ok = (ok != 0) && MAX_TREE_HOST_CHECK(next == count, tree, error, ENGINE_ERROR_LOGIC);
    for (unsigned int node = 0u; (ok != 0) && (node < count); node += 1u)
    {
        const unsigned int voxel = canonical[node];
        const unsigned int at = placed[node];
        const unsigned int above = tree->parent[voxel];
        nodes->voxel[at] = voxel;
        nodes->level[at] = rank[voxel];
        nodes->parent[at] = (above == voxel) ? at : placed[node_of[above]];
        nodes->subtree_end[at] = 1u;
    }
    for (unsigned int at = count; (ok != 0) && (at > 0u); at -= 1u)
    {
        const unsigned int node = at - 1u;
        if (nodes->parent[node] != node)
        {
            nodes->subtree_end[nodes->parent[node]] += nodes->subtree_end[node];
        }
    }
    for (unsigned int node = 0u; (ok != 0) && (node < count); node += 1u)
    {
        nodes->subtree_end[node] += node;
        nodes->root[node] = (nodes->parent[node] == node) ? node : nodes->root[nodes->parent[node]];
    }
    for (size_t voxel = 0u; (ok != 0) && (voxel < voxels); voxel += 1u)
    {
        const unsigned int above = tree->parent[voxel];
        if (above == MAX_TREE_ABSENT)
        {
            nodes->own[voxel] = MAX_TREE_ABSENT;
            continue;
        }
        const unsigned int node_at = (node_of[voxel] != MAX_TREE_ABSENT) ? node_of[voxel] : node_of[above];
        // an admitted voxel that is not canonical points at its node's canonical voxel
        ok = MAX_TREE_HOST_CHECK(node_at != MAX_TREE_ABSENT, &tree->parent[voxel], error, ENGINE_ERROR_LOGIC);
        nodes->own[voxel] = (ok != 0) ? placed[node_at] : MAX_TREE_ABSENT;
    }
    platform_cell_released(starts, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(children, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(canonical, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(filled, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(placed, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(waiting, (unsigned int)platform_place_g_engine_owner);
    return ok;
}

long max_tree_nodes(const MaxTreeNodesRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return MAX_TREE_ERROR;
    }
    EngineError *const error = request->error;
    if (!MAX_TREE_HOST_CHECK((request->residual != NULL) && (request->tree != NULL) && (request->nodes != NULL) &&
                                 (request->tree->parent != NULL) && (request->tree->order != NULL),
                             request, error, ENGINE_ERROR_REQUEST))
    {
        return MAX_TREE_ERROR;
    }
    const unsigned int *const residual = request->residual;
    const MaxTree *const tree = request->tree;
    MaxTreeNodes *const nodes = request->nodes;
    mmgr_bytes_set(nodes, 0, sizeof(*nodes));
    const size_t voxels = tree->voxels;
    const unsigned int admitted = tree->admitted;
    unsigned int *const rank = (unsigned int *)platform_cell_taken(
        (unsigned long long)((voxels + 1u) * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    unsigned int *const node_of = (unsigned int *)platform_cell_taken(
        (unsigned long long)((voxels + 1u) * sizeof(unsigned int)), (unsigned int)platform_place_g_engine_owner, 0);
    int ok = MAX_TREE_HOST_CHECK((rank != NULL) && (node_of != NULL), request, error, ENGINE_ERROR_RESOURCE);
    unsigned int levels = 0u;
    for (unsigned int at = admitted; (ok != 0) && (at > 0u); at -= 1u)
    {
        const unsigned int voxel = tree->order[at - 1u];
        levels += ((at == admitted) || (max_tree_same(residual, voxel, tree->order[at]) == 0)) ? 1u : 0u;
        rank[voxel] = levels;
    }
    unsigned int count = 0u;
    for (size_t voxel = 0u; (ok != 0) && (voxel < voxels); voxel += 1u)
    {
        const unsigned int above = tree->parent[voxel];
        // a voxel index is below the tree's voxel count, which max_tree_grow bounds below 2^32 - 1
        const unsigned int here = (unsigned int)voxel;
        const int canonical =
            (above != MAX_TREE_ABSENT) && ((above == here) || (max_tree_same(residual, here, above) == 0));
        node_of[voxel] = (canonical != 0) ? count : MAX_TREE_ABSENT;
        count += (canonical != 0) ? 1u : 0u;
    }
    if (ok != 0)
    {
        nodes->voxel = (unsigned int *)platform_cell_taken(((size_t)count + 1u) * sizeof(unsigned int),
                                                           platform_place_g_program_owner, 0);
        nodes->parent = (unsigned int *)platform_cell_taken(((size_t)count + 1u) * sizeof(unsigned int),
                                                            platform_place_g_program_owner, 0);
        nodes->level = (unsigned int *)platform_cell_taken(((size_t)count + 1u) * sizeof(unsigned int),
                                                           platform_place_g_program_owner, 0);
        nodes->subtree_end = (unsigned int *)platform_cell_taken(((size_t)count + 1u) * sizeof(unsigned int),
                                                                 platform_place_g_program_owner, 0);
        nodes->root = (unsigned int *)platform_cell_taken(((size_t)count + 1u) * sizeof(unsigned int),
                                                          platform_place_g_program_owner, 0);
        nodes->own = (unsigned int *)platform_cell_taken((voxels + 1u) * sizeof(unsigned int),
                                                         platform_place_g_program_owner, 0);
        ok = MAX_TREE_HOST_CHECK((nodes->voxel != NULL) && (nodes->parent != NULL) && (nodes->level != NULL) &&
                                     (nodes->subtree_end != NULL) && (nodes->root != NULL) && (nodes->own != NULL),
                                 request, error, ENGINE_ERROR_RESOURCE);
    }
    ok = (ok != 0) && max_tree_nodes_build(tree, node_of, rank, count, nodes, error);
    platform_cell_released(rank, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(node_of, (unsigned int)platform_place_g_engine_owner);
    if (ok == 0)
    {
        max_tree_nodes_release(nodes);
        return MAX_TREE_ERROR;
    }
    nodes->count = count;
    nodes->levels = levels;
    nodes->voxels = tree->voxels;
    nodes->depth = tree->depth;
    nodes->height = tree->height;
    nodes->width = tree->width;
    return (long)count;
}

unsigned int max_tree_node_at(const MaxTreeNodes *nodes, unsigned int node, unsigned int level)
{
    if ((nodes == NULL) || (node >= nodes->count) || (nodes->level[node] < level))
    {
        return MAX_TREE_ABSENT;
    }
    unsigned int at = node;
    while ((nodes->parent[at] != at) && (nodes->level[nodes->parent[at]] >= level))
    {
        at = nodes->parent[at];
    }
    return at;
}

long max_tree_probe_levels(const MaxTreeProbeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return MAX_TREE_ERROR;
    }
    EngineError *const error = request->error;
    const MaxTreeNodes *const nodes = request->nodes;
    const unsigned int probes = request->probe_count;
    if (!MAX_TREE_HOST_CHECK((nodes != NULL) && (nodes->own != NULL) &&
                                 ((probes == 0u) || ((request->probe_voxels != NULL) && (request->sorted != NULL) &&
                                                     (request->own_level != NULL) && (request->joined != NULL))),
                             request, error, ENGINE_ERROR_REQUEST))
    {
        return MAX_TREE_ERROR;
    }
    for (unsigned int probe = 0u; probe < probes; probe += 1u)
    {
        if (!MAX_TREE_HOST_CHECK(request->probe_voxels[probe] < nodes->voxels, &request->probe_voxels[probe], error,
                                 ENGINE_ERROR_REQUEST))
        {
            return MAX_TREE_ERROR;
        }
    }
    if (probes == 0u)
    {
        return 0L;
    }
    unsigned long long *const keys =
        (unsigned long long *)platform_cell_taken((unsigned long long)((size_t)probes * sizeof(unsigned long long)),
                                                  (unsigned int)platform_place_g_engine_owner, 0);
    if (!MAX_TREE_HOST_CHECK(keys != NULL, request, error, ENGINE_ERROR_RESOURCE))
    {
        return MAX_TREE_ERROR;
    }
    for (unsigned int probe = 0u; probe < probes; probe += 1u)
    {
        const unsigned int own = nodes->own[request->probe_voxels[probe]];
        request->own_level[probe] = (own == MAX_TREE_ABSENT) ? 0u : nodes->level[own];
        // MAX_TREE_ABSENT in the high word sorts a probe that is not admitted after every node
        keys[probe] = ((unsigned long long)own << 32u) | probe;
    }
    qsort(keys, probes, sizeof(unsigned long long), max_tree_key_compare);
    for (unsigned int at = 0u; at < probes; at += 1u)
    {
        // the low word of a key is the probe's index
        request->sorted[at] = (unsigned int)(keys[at] & 0xFFFFFFFFULL);
    }
    for (unsigned int at = 0u; (at + 1u) < probes; at += 1u)
    {
        const unsigned int one = nodes->own[request->probe_voxels[request->sorted[at]]];
        const unsigned int other = nodes->own[request->probe_voxels[request->sorted[at + 1u]]];
        unsigned int joined = 0u;
        if ((one != MAX_TREE_ABSENT) && (other != MAX_TREE_ABSENT) && (one == other))
        {
            joined = nodes->level[one];
        }
        else if ((one != MAX_TREE_ABSENT) && (other != MAX_TREE_ABSENT) && (nodes->root[one] == nodes->root[other]))
        {
            // every node in (one, other] lies under the LCA, and the LCA's child toward other is one of them
            unsigned int lowest = nodes->level[nodes->parent[one + 1u]];
            for (unsigned int node = one + 2u; node <= other; node += 1u)
            {
                const unsigned int above = nodes->level[nodes->parent[node]];
                lowest = (above < lowest) ? above : lowest;
            }
            joined = lowest;
        }
        request->joined[at] = joined;
    }
    platform_cell_released(keys, (unsigned int)platform_place_g_engine_owner);
    return (long)probes;
}
