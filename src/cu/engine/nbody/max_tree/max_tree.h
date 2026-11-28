// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef MAX_TREE_H
#define MAX_TREE_H

#include "../../engine_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_TREE_ERROR (-1)

#define MAX_TREE_ABSENT 0xFFFFFFFFu

#define MAX_TREE_KEY_LIMBS (ENGINE_RESIDUAL_LIMBS + 1u)

    typedef struct
    {
        unsigned int voxels;
        unsigned int depth;
        unsigned int height;
        unsigned int width;
        unsigned int admitted;
        unsigned int *parent;
        unsigned int *order;
    } MaxTree;

    typedef struct
    {
        const unsigned int *residual;
        unsigned int depth;
        unsigned int height;
        unsigned int width;
        const unsigned int *levels;
        unsigned int level_count;
        unsigned char *bound;
        unsigned int *rounds;
        unsigned int *levels_equal;
        unsigned long long *bound_microseconds;
        unsigned long long *proved_microseconds;
        EngineError *error;
    } MaxTreeBindRequest;

    long max_tree_build(const unsigned int *residual, unsigned int depth, unsigned int height, unsigned int width,
                        MaxTree *tree);

    void max_tree_release(MaxTree *tree);

    int max_tree_valid(const unsigned int *residual, const MaxTree *tree, unsigned int levels);

    long max_tree_build_bound(const unsigned int *residual, unsigned int depth, unsigned int height, unsigned int width,
                              const unsigned char *bound, MaxTree *tree);

    int max_tree_equal(const MaxTree *left, const MaxTree *right);

    int max_tree_poc(const unsigned int *residual, unsigned int depth, unsigned int height, unsigned int width,
                     unsigned char *bound, unsigned int *rounds);

    int max_tree_order_agrees(const unsigned int *residual, unsigned int depth, unsigned int height, unsigned int width,
                              EngineError *error);

    long max_tree_bind(const MaxTreeBindRequest *request);

    // The tree as nodes (A2). A node is one component of one upper level set, the nodes of a tree that max_tree_build
    // builds. The level of a node is the dense rank of its value among the admitted values, 1 the least, and `levels`
    // is the max. The nodes have numbers in DFS order: the subtree of a node is a single range
    // `[node, subtree_end[node])`. Its parent is the node of the next lower level that holds it. A root is its own
    // parent, and `root` names the root of each node.
    //
    // The level of a child is above that of its parent. `own` names the own node of each voxel, the node at its own
    // level that holds it. It is MAX_TREE_ABSENT for a voxel outside the admitted set. The component at level r that
    // holds voxel x is the ancestor a of `own[x]` with `level[a] >= r > level[parent[a]]`: max_tree_node_at.
    typedef struct
    {
        unsigned int count;
        unsigned int levels;
        unsigned int voxels;
        unsigned int depth;
        unsigned int height;
        unsigned int width;
        unsigned int *voxel;
        unsigned int *parent;
        unsigned int *level;
        unsigned int *subtree_end;
        unsigned int *root;
        unsigned int *own;
    } MaxTreeNodes;

    typedef struct
    {
        const unsigned int *residual;
        const MaxTree *tree;
        MaxTreeNodes *nodes;
        EngineError *error;
    } MaxTreeNodesRequest;

    // the nodes of `tree`, built over `residual`; returns the node count, or MAX_TREE_ERROR with the error filled and
    // `nodes` left empty
    long max_tree_nodes(const MaxTreeNodesRequest *request);

    void max_tree_nodes_release(MaxTreeNodes *nodes);

    // the component at `level` that contains `node`'s voxels: `node` itself or its ancestor, or MAX_TREE_ABSENT where
    // the node's level is below `level`
    unsigned int max_tree_node_at(const MaxTreeNodes *nodes, unsigned int node, unsigned int level);

    // The probe partition at every level at once (A2). `sorted` lists the probes by the DFS order of their own nodes,
    // with a probe whose voxel is outside the admitted set last. `own_level` is the own level of each probe, 0 where
    // its voxel is outside the admitted set. `joined[i]` is the level of the LCA of sorted probes i and `i + 1`, 0
    // where no level holds both.
    //
    // At level r, a probe is present where its own level is r or more. Two present probes are in one component exactly
    // where every joined level between them in the order is r or more. The call reads each joined level in one scan of
    // the DFS order. For u before v in one tree, the level of the LCA is the least level among the parents of the nodes
    // in `(u, v]`. Each of those nodes lies under the LCA, and the child of the LCA toward v is one of them.
    typedef struct
    {
        const MaxTreeNodes *nodes;
        const unsigned int *probe_voxels;
        unsigned int probe_count;
        unsigned int *sorted;
        unsigned int *own_level;
        unsigned int *joined;
        EngineError *error;
    } MaxTreeProbeRequest;

    // returns the probe count, or MAX_TREE_ERROR with the error filled
    long max_tree_probe_levels(const MaxTreeProbeRequest *request);

    // the partition of one level from max_tree_probe_levels. It is the part of each probe, numbered from 0 in the
    // sorted order. It is MAX_TREE_ABSENT where the probe is not present at `level`. Returns the number of parts
    unsigned int max_tree_probe_partition(const unsigned int *sorted, const unsigned int *own_level,
                                          const unsigned int *joined, unsigned int probe_count, unsigned int level,
                                          unsigned int *part);

    // The own-node pair table (A2). It is for the nodes of an earlier and a later frame over one extent, and a lag v
    // (z, y, x). Entry i counts the voxels x whose own node in the earlier frame is `earlier[i]`. For each, `x + v` is
    // in the admitted set of the later frame, with own node `later[i]`. The entries go in order of the earlier node,
    // then of the later one, each pair once.
    typedef struct
    {
        unsigned int count;
        unsigned int *earlier;
        unsigned int *later;
        unsigned long long *voxels;
    } MaxTreePairs;

    typedef struct
    {
        const MaxTreeNodes *earlier;
        const MaxTreeNodes *later;
        int lag[3];
        MaxTreePairs *pairs;
        EngineError *error;
    } MaxTreePairsRequest;

    // returns the entry count, or MAX_TREE_ERROR with the error filled and `pairs` left empty
    long max_tree_pairs(const MaxTreePairsRequest *request);

    void max_tree_pairs_release(MaxTreePairs *pairs);

    // The overlap at every pair of levels from the single table (A2). Take each requested pair of nodes, a of the
    // earlier frame and b of the later. `sums` is the table summed over the subtree of a and the subtree of b. That is
    // `C_{r,r'}[A, B](v)` for the components A and B that those nodes are, at any level where each is a component. The
    // call answers the sums together in one sweep of the table.
    typedef struct
    {
        const MaxTreeNodes *earlier;
        const MaxTreeNodes *later;
        const MaxTreePairs *pairs;
        const unsigned int *earlier_nodes;
        const unsigned int *later_nodes;
        unsigned int pair_count;
        unsigned long long *sums;
        EngineError *error;
    } MaxTreeOverlapSumsRequest;

    // returns the number of sums, or MAX_TREE_ERROR with the error filled
    long max_tree_overlap_sums(const MaxTreeOverlapSumsRequest *request);

    typedef struct
    {
        const unsigned int *device_residual;
        unsigned int depth;
        unsigned int height;
        unsigned int width;
        unsigned int capacity;
        EngineBody *bodies;
        unsigned int *labels;
        unsigned long long *positive_words;
        unsigned int grade;
        unsigned int *faces_differ;
        unsigned int *level_code;
        unsigned int *proof_count;
        EngineError *error;
        // the limbs that hold a lane of `device_residual`, as those of EngineResidualRequest: 0 is
        // ENGINE_RESIDUAL_LIMBS. A graded request, or one made while the program keeps frames, uses
        // ENGINE_RESIDUAL_LIMBS
        unsigned int limbs;
    } MaxTreeObjectsRequest;

    long max_tree_objects(const MaxTreeObjectsRequest *request);

    // the device bytes that max_tree_objects allocates over `voxels` lanes with no frames kept, and the room of its
    // bodies for `bodies` with them. Each allocation is in whole pages (DEVICE_POOL_PAGE_BYTES). It is 0 where the
    // caller gives no workspace for the sort
    unsigned long long max_tree_objects_bytes(size_t voxels, size_t bodies);

    // frees the buffers max_tree_objects allocates between calls
    void max_tree_resident_release(void);

#define MAX_TREE_FIELD_MOMENT_ZZ 0u
#define MAX_TREE_FIELD_MOMENT_YY 1u
#define MAX_TREE_FIELD_MOMENT_XX 2u
#define MAX_TREE_FIELD_MOMENT_ZY 3u
#define MAX_TREE_FIELD_MOMENT_ZX 4u
#define MAX_TREE_FIELD_MOMENT_YX 5u
#define MAX_TREE_FIELD_SUM_Z 6u
#define MAX_TREE_FIELD_SUM_Y 7u
#define MAX_TREE_FIELD_SUM_X 8u
#define MAX_TREE_FIELD_PEAK 9u
#define MAX_TREE_FIELD_TOUCHES 10u
#define MAX_TREE_FIELD_MASS 11u
#define MAX_TREE_FIELD_LEVEL 12u
#define MAX_TREE_FIELD_FRAME 13u
#define MAX_TREE_FIELD_SAMPLE 14u
#define MAX_TREE_FIELDS 15u

    typedef struct
    {
        unsigned int bits[MAX_TREE_FIELDS];
        unsigned int offset[MAX_TREE_FIELDS];
        unsigned int total_bits;
        unsigned int limbs;
    } MaxTreeLayout;

    void max_tree_layout(unsigned int depth, unsigned int height, unsigned int width, unsigned int frames,
                         unsigned int samples, MaxTreeLayout *layout);

    typedef struct
    {
        const MaxTreeLayout *layout;
        unsigned int sample;
        unsigned int frame;
        unsigned int *device_magnitudes;
        unsigned long long *mismatches;
        EngineError *error;
    } MaxTreePackRequest;

    long max_tree_pack(const MaxTreePackRequest *request);

    typedef struct
    {
        unsigned int level;
        unsigned int components;
    } MaxTreeSlideStep;

    typedef struct
    {
        const unsigned int *probe_voxels;
        unsigned int probe_count;
        MaxTreeSlideStep *steps;
        unsigned int *partitions;
        unsigned int step_capacity;
        unsigned int *step_count;
        unsigned int *top_level;
        unsigned int *threshold;
        unsigned int *labelings;
        EngineError *error;
    } MaxTreeSlideRequest;

    long max_tree_slide(const MaxTreeSlideRequest *request);

    int max_tree_retain_frames(void);

    typedef struct
    {
        unsigned int origin;
        unsigned int level;
        unsigned int earlier_code;
        unsigned int later_code;
        unsigned int earlier_components;
        unsigned int later_components;
        unsigned int earlier_one;
        unsigned int later_one;
        unsigned int earlier_unpaired;
        unsigned int earlier_mutual;
        unsigned int earlier_forked;
        unsigned int later_unpaired;
        unsigned int later_mutual;
        unsigned int later_forked;
    } MaxTreeOverlapStep;

    typedef struct
    {
        unsigned int root;
        unsigned int degree;
        unsigned int backs;
    } MaxTreeOverlapProbe;

    typedef struct
    {
        int lag[3];
        const unsigned int *earlier_levels;
        unsigned int earlier_level_count;
        const unsigned int *later_levels;
        unsigned int later_level_count;
        const unsigned int *probe_voxels[2];
        unsigned int probe_counts[2];
        const unsigned int *probe_links;
        unsigned int link_count;
        MaxTreeOverlapProbe *probes;
        unsigned char *links_present;
        MaxTreeOverlapStep *steps;
        unsigned int step_capacity;
        unsigned int *step_count;
        EngineError *error;
    } MaxTreeOverlapRequest;

    long max_tree_overlap(const MaxTreeOverlapRequest *request);

    void max_tree_profile_report(void);

#ifdef __cplusplus
}
#endif

#endif
