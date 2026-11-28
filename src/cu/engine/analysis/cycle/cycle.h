// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CYCLE_H
#define CYCLE_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CYCLE_ERROR (-1L)

    long cycle_key_load(const EngineKeyLayout *layout, CycleKey **key, EngineError *error);

    void cycle_key_release(CycleKey *key);

    unsigned int cycle_key_scratch_limbs(const CycleKey *key);

    typedef struct
    {
        const CycleKey *key;
        const Atom *atoms;
        unsigned long long count;
        unsigned int limbs;
        unsigned int *device_out;
        EngineError *error;
    } CycleRunRequest;

    long cycle_run(const CycleRunRequest *request);

    long cycle_record_load(const EngineRecordLayout *layout, CycleRecord **record, EngineError *error);

    void cycle_record_release(CycleRecord *record);

    unsigned int cycle_record_out_limbs(const CycleRecord *record);

    unsigned int cycle_record_members(const CycleRecord *record);

    unsigned int cycle_record_in_limbs(const CycleRecord *record, unsigned int member);

    // 1 where the loaded program runs as its own compiled kernel, with its resident written after its lane. 0 where it
    // runs on the interpreter: a step that the compiler does not support, no NVRTC or nvJitLink, or
    // CYCLE_RECORD_INTERPRET=1
    int cycle_record_compiled(const CycleRecord *record);

    // the copy that the host has of the block of the loaded program, read back from the device as each launch ends. It
    // tells where the program is, and the seal is on it when no launch refers to it. A compiled program runs resident,
    // and writes it on the device. A program on the interpreter leaves it as arranged
    const EngineProgramBlock *cycle_record_block(const CycleRecord *record);

    typedef struct
    {
        const CycleRecord *record;
        const unsigned int *device_in[ENGINE_RECORD_MEMBERS_MAX];
        unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX];
        const unsigned int *device_index;
        unsigned long long count;
        unsigned int *device_out;
        EngineError *error;
    } CycleRecordRunRequest;

    long cycle_record_run(const CycleRecordRunRequest *request);

    typedef struct
    {
        const EngineRecordLayout *layout;
        const unsigned int *in[ENGINE_RECORD_MEMBERS_MAX];
        unsigned long long bodies[ENGINE_RECORD_MEMBERS_MAX];
        const unsigned int *index;
        unsigned long long count;
        unsigned int *out;
        EngineError *error;
        // the lane that the run starts at. It runs lanes `first` to `first + count - 1`, and each reads its members and
        // its number as on the device. It writes their `count` records to `out` from its start. It is 0 where the
        // caller does not set it
        unsigned long long first;
    } CycleRecordHostRequest;

    long cycle_record_run_host(const CycleRecordHostRequest *request);

// the latch's answer where no lane's output is true: the minimum over no lane, infinity
#define CYCLE_LATCH_NONE 0xFFFFFFFFFFFFFFFFULL

    // The latch: the first of `count` records whose output at bit `offset`, `bits` wide, is not zero. That is the least
    // such lane, or CYCLE_LATCH_NONE where the output of no lane is. The output is the condition of a program, and it
    // is true where it is not zero. The minimum is associative, commutative and idempotent. Any group returns the lane
    // that a serial scan from lane 0 does.
    //
    // The call cycle_record_latch reads records in device memory by a tree over each warp and one atomic minimum over
    // the device. Only the lane comes back to the host. The call cycle_record_latch_host reads records in host memory
    // by that serial scan.
    typedef struct
    {
        const unsigned int *records;
        unsigned long long count;
        unsigned int out_limbs;
        unsigned int offset;
        unsigned int bits;
        unsigned long long *first;
        EngineError *error;
    } CycleRecordLatchRequest;

    long cycle_record_latch(const CycleRecordLatchRequest *request);

    long cycle_record_latch_host(const CycleRecordLatchRequest *request);

    // The sum, over each run of `group` consecutive records of `count`. The call reads the output at bit `offset`,
    // `bits` wide, as two's complement. It sums it exactly into `sum_limbs` limbs of two's complement at `sums`, in
    // host memory, one sum for each run.
    //
    // `count` is a whole number of runs, and a run holds at max `2^32` records. `sum_limbs * 32` holds `bits` and the
    // bits of `group` beside them, and no sum wraps. A request that cannot hold its sums gives an error before the call
    // reads a record. The sum is associative and commutative, and any group of the records returns the serial sum.
    //
    // The call cycle_record_sum reads records in device memory. It sums each 32-bit limb of the field over a run into
    // 64 bits, which hold `2^32` of them, and counts the negative fields beside. The host takes the carries, and
    // subtracts `2^bits` for each negative field once for each run. Only those totals come back. The call
    // cycle_record_sum_host reads records in host memory, and adds them in order, each sign-extended to `sum_limbs`
    // limbs.
    typedef struct
    {
        const unsigned int *records;
        unsigned long long count;
        unsigned long long group;
        unsigned int out_limbs;
        unsigned int offset;
        unsigned int bits;
        unsigned int sum_limbs;
        unsigned int *sums;
        EngineError *error;
    } CycleRecordSumRequest;

    long cycle_record_sum(const CycleRecordSumRequest *request);

    // 1 where a sum request is whole, the check that both routes make before they read a record
    int cycle_record_sum_valid(const CycleRecordSumRequest *request);

    long cycle_record_sum_host(const CycleRecordSumRequest *request);

    // cycle_record_sum retains its device scratch between calls, grown to the max a sum requested; this gives it
    // back
    void cycle_record_sum_release(void);

    // cycle_run retains its scratch, its lanes' table and its folds on the device between calls; this gives them back
    void cycle_resident_release(void);

    // The sort, within each run of `group` consecutive records of `count`. The lanes go in the order of their output at
    // bit `offset`, `bits` wide, read as a magnitude, least first. Records whose outputs are equal go in the order of
    // their lanes. `order` takes `count` lanes, 32 bits each, and entry i of a run is the lane of the i-th record of
    // the run. `order` is an index that a sweep reads its records through. `count` is a whole number of runs, and at
    // max `2^32` records.
    //
    // The call cycle_record_sort reads records and writes the order in device memory. A run whose records and lanes fit
    // the shared memory of one thread block sorts there by a bitonic network over (output, lane). A longer run sorts by
    // a radix sort of four bits for each pass over the device. The sort takes the bits of the output, then the number
    // of the run. The call cycle_record_sort_host reads and writes them in host memory, by a merge that keeps equal
    // outputs in lane order.
    typedef struct
    {
        const unsigned int *records;
        unsigned long long count;
        unsigned long long group;
        unsigned int out_limbs;
        unsigned int offset;
        unsigned int bits;
        unsigned int *order;
        EngineError *error;
    } CycleRecordSortRequest;

    long cycle_record_sort(const CycleRecordSortRequest *request);

    // 1 where a sort request is whole, the check that both routes make before they read a record
    int cycle_record_sort_valid(const CycleRecordSortRequest *request);

    long cycle_record_sort_host(const CycleRecordSortRequest *request);

#ifdef __cplusplus
}
#endif

#endif
