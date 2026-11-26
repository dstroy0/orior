// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef TOWER_H
#define TOWER_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TOWER_ERROR (-1L)

// A reversible lookup edge is between two tower floors. It permutes the low `index_bits` of every
// approximation coefficient at floor `floor` through `forward`, a permutation of [0, 1 << index_bits);
// the high bits pass through. The map is a bijection on the coefficient word and tower_lower replays
// its inverse in reverse order. This is the nonlinear edge of the engine (kolmogorov_arnold.md) in the reversible
// tower. The tower keeps the crystal exactly invertible, and the edge makes it universal over the coefficient
// alphabet. `floor` runs 0..floors: floor k acts on the approximation just before the lifting of level k, and
// floor == floors acts on the fully collapsed floor.
#define TOWER_EDGE_INDEX_BITS_MAX 20u

    typedef struct
    {
        unsigned int floor;
        unsigned int index_bits;
        const unsigned int *forward;
    } TowerEdge;

    // The lift reads the lattice `device_lanes`, 16-bit samples. Where `device_values` is not NULL, it reads that
    // lattice of ints in their place, such as a residual below zero. Its values past ENGINE_COEFFICIENT_LIMIT error
    // on the lift as the values of the coefficients do.
    typedef struct
    {
        const unsigned short *device_lanes;
        const int *device_values;
        unsigned long long extent[4];
        const int **coefficients;
        unsigned int **scratch;
        unsigned int *floors;
        const TowerEdge *edges;
        unsigned int edge_count;
        EngineError *error;
    } TowerLiftRequest;

    long tower_lift(const TowerLiftRequest *request);

    long tower_capacity(const unsigned long long extent[4], int **coefficients, EngineError *error);

    // The device bytes that the tower takes for a lattice of `lanes` values, rounded to the page. That is its one
    // pool: the coefficients and the scratch as ints, the flag and the mismatch count. 0 for no lanes or a count that
    // no pool can use. The tower keeps the pool at the most lanes that a call asked for. A job that lifts or lowers
    // several lattices keeps it for the largest.
    unsigned long long tower_reserve_bytes(unsigned long long lanes);

    // frees the pool and the edge table the tower retains between calls
    void tower_resident_release(void);

    typedef struct
    {
        const unsigned short *device_lanes;
        unsigned long long extent[4];
        unsigned long long *mismatches;
        const unsigned short **device_rebuilt;
        unsigned short *rebuilt;
        const TowerEdge *edges;
        unsigned int edge_count;
        EngineError *error;
    } TowerLowerRequest;

    long tower_lower(const TowerLowerRequest *request);

    // A lift as one job of the device (tessera_job.h). The lattice, `lanes` or `values` on the host, and the tables of
    // the edges cross in. One cooperative launch lifts the lattice on the device, the scratch a cell that the job takes
    // there. The coefficients cross out into `coefficients`, a room of the lanes of the extent on the host. A value
    // past ENGINE_COEFFICIENT_LIMIT errors as tower_lift errors on it.
    typedef struct
    {
        const unsigned short *lanes;
        const int *values;
        unsigned long long extent[4];
        const TowerEdge *edges;
        unsigned int edge_count;
        int *coefficients;
        unsigned int *floors;
        EngineError *error;
    } TowerLiftJobRequest;

    long tower_lift_job(const TowerLiftJobRequest *request);

// The ruleset of the wave transform. A line of values splits into two bands, the lows (its even values) and the highs
// (its odd values). A lifting step moves one band by a rounded sum over the other:
//     `target[a] += sign * floor((rounding + sum_k weight_k * other[a + offset_k]) / 2^shift)`.
// An index past either end of `other` reads that end. A ruleset is its lifting steps in order.
//
// The transform runs the steps along every line of every level, and its inverse runs them last first with each sign
// turned. Every ruleset is invertible exactly. The 5/3 of the kernels is two steps: the highs less floor((x_2j +
// x_(2j+2)) / 2), then the lows plus floor((d_(j-1) + d_j + 2) / 4).
#define TOWER_BAND_LOW 0u

#define TOWER_BAND_HIGH 1u

#define TOWER_RULE_TAPS_MAX 8u

#define TOWER_RULE_SHIFT_MAX 30u

    typedef struct
    {
        unsigned int target;
        int sign;
        unsigned int taps;
        int offset[TOWER_RULE_TAPS_MAX];
        int weight[TOWER_RULE_TAPS_MAX];
        unsigned int rounding;
        unsigned int shift;
    } TowerLiftingStep;

    // T and T^-1 as record floors (engine_table.md item 8, A13). The call writes the ruleset over the extent into the
    // program of the caller, one record step at a time. A program runs T, T^-1 or an operation read through T^-1 as
    // one stack on the record machine. Each lane holds one block of the lattice. `rules` and `rule_count` are the
    // ruleset.
    // With both 0, it is the 5/3 that tower_lift and tower_lower run as kernels.
    //
    // `extent` is the extent of the block, t, z, y and x, and the registers of the positions go in that order. Each
    // register of `in_registers` holds a value that the program takes in, from an earlier step. Each register of
    // `out_registers` holds a value that the program gives out. The call tower_record_lift leaves the crystal where
    // tower_lift leaves its coefficients: the lows of each level first along every axis, and its highs after them.
    // The call tower_record_lower leaves the samples.
    //
    // A weight other than 1 is a product by a constant. A floor v / 2^k is EXACT_QUOTIENT(v - AND(v, 2^k - 1), 2^k),
    // toward minus infinity as the shift of the kernels. Each constant is one step, which the call puts where a step
    // first reads it. The call puts no edge. The steps go in at `*count`, and the call moves the count past them.
    //
    // Where `steps` is NULL, the call writes the count and the out registers alone, which sizes a program before the
    // caller stores it. Where `steps` is not NULL, a program past `step_capacity` errors, and the count stays where it
    // was.
    typedef struct
    {
        unsigned long long extent[4];
        const TowerLiftingStep *rules;
        unsigned int rule_count;
        const unsigned int *in_registers;
        unsigned int *out_registers;
        EngineRecordStep *steps;
        unsigned int step_capacity;
        unsigned int *count;
        EngineError *error;
    } TowerRecordRequest;

    long tower_record_lift(const TowerRecordRequest *request);

    long tower_record_lower(const TowerRecordRequest *request);

#ifdef __cplusplus
}
#endif

#endif
