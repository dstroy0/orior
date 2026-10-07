// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef PERIOD_H
#define PERIOD_H

#include "../../engine_config.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PERIOD_ERROR (-1L)

    typedef struct
    {
        unsigned long long numerator;
        unsigned long long denominator;
    } PeriodMargin;

    typedef struct
    {
        unsigned long long extent;
        unsigned long long period;
        unsigned long long candidate;
        unsigned long long lags;
        unsigned long long pairs_per_lag;
        unsigned long long agreement_at_candidate;
        unsigned long long agreement_beside_candidate;
        unsigned long long agreement_at_double;
        unsigned long long agreement_beside_double;
        PeriodMargin margin;
        unsigned long long band_count;
        PeriodMargin band_bottom;
        PeriodMargin band_top;
    } PeriodAxis;

    typedef struct
    {
        unsigned int rank;
        unsigned long long voxels;
        unsigned long long collisions;
        unsigned long long draws;
        PeriodAxis axis[ENGINE_ARRAY_RANK];
    } PeriodMeasurement;

    typedef struct
    {
        const unsigned short *device_lanes;
        unsigned int rank;
        unsigned long long extent[ENGINE_ARRAY_RANK];
        unsigned long long draws;
        EngineSignum content;
        const PeriodMargin *null_top;
        unsigned long long *agreement;
        unsigned long long agreement_capacity;
        PeriodMargin *band;
        unsigned long long band_capacity;
        PeriodMeasurement *measurement;
        EngineError *error;
    } PeriodRequest;

    unsigned long long period_agreement_entries(unsigned int rank, const unsigned long long *extent);

    // The bytes of the device pool period_read and period_draw keep after they return, for `voxels` lanes and `entries`
    // agreement counts (period_agreement_entries): the histogram, the agreement and a shuffled copy of the lanes. The
    // pool grows to the most voxels and the most entries asked so far, and a job over several extents declares it for
    // the most of each. 0 for no voxels or more than 2^32 - 1, which the calls error.
    unsigned long long period_reserve_bytes(unsigned long long voxels, unsigned long long entries);

    long period_read(const PeriodRequest *request);

    long period_draw(const PeriodRequest *request, unsigned long long draw, PeriodMargin *heights);

    int period_print(const PeriodMeasurement *measurement, FILE *file);

    // The period on the record machine, from the same request to the same measurement as period_read and period_draw.
    // Each lag's agreement is a record program swept over the pairs that lag reaches and summed by cycle_record_sum,
    // and the counts equal period_read's word for word. An axis's null keys every voxel of each line along it by
    // period's line hash of the content and the draw, cycle_record_sort orders each line by its keys, and two sweeps
    // gather the line through that order. The programs are made once a process and read every extent from a shared
    // record. A lattice of more than 2^31 voxels errors: a sweep's index names a voxel by an output one bit wider than
    // its register, and 32 bits hold it.
    long period_record_read(const PeriodRequest *request);

    long period_record_draw(const PeriodRequest *request, unsigned long long draw, PeriodMargin *heights);

    // The bytes of the device pool period_record_read and period_record_draw keep after they return, for `voxels` lanes:
    // the lanes widened to a limb, the shuffled line, each voxel's key and place, the order and the places it gathers,
    // and the histogram. 0 for no voxels or more than 2^31.
    unsigned long long period_record_reserve_bytes(unsigned long long voxels);

    // the programs and the pool given back
    void period_record_release(void);

#ifdef __cplusplus
}
#endif

#endif
