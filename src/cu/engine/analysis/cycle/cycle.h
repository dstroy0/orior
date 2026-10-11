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

    // 1 where the loaded program runs as its own compiled kernel, its resident written after its lane; 0 where it runs
    // on the interpreter (a step the compiler does not hold, NVRTC or nvJitLink not found, or CYCLE_RECORD_INTERPRET=1)
    int cycle_record_compiled(const CycleRecord *record);

    // the host's copy of the loaded program's block, read back from the device as each launch ends: where it stands,
    // sealed whenever no launch holds it. A compiled program runs resident and writes it on the device; a program on
    // the interpreter leaves it laid out
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
        // the lane the run starts at: it runs lanes first to first + count - 1, each reading its members and its
        // number as on the device, and writes their count records to `out` from its start; 0 where it is not set
        unsigned long long first;
    } CycleRecordHostRequest;

    long cycle_record_run_host(const CycleRecordHostRequest *request);

// the latch's answer where no lane's output holds: the minimum over no lane, infinity
#define CYCLE_LATCH_NONE 0xFFFFFFFFFFFFFFFFull

    // The latch: the first of `count` records whose output at bit `offset`, `bits` wide, is not zero, the least such
    // lane, or CYCLE_LATCH_NONE where no lane's is; the output is a program's condition, and it holds where it is not
    // zero. The minimum is associative, commutative and idempotent. Any grouping returns the lane a serial scan from
    // lane 0 does. cycle_record_latch reads records in device memory by a tree over each warp and one atomic minimum
    // over the device, and only the lane comes back to the host; cycle_record_latch_host reads records in host memory
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

    // The sum: over each run of `group` consecutive records of `count`, the output at bit `offset`, `bits` wide, read
    // as two's complement, summed exactly into `sum_limbs` limbs of two's complement at `sums`, in host memory, one sum
    // a run. `count` is a whole number of runs, a run holds at most 2^32 records, and sum_limbs * 32 holds `bits` and
    // the bits of `group` beside them, and no sum wraps: a request that cannot hold its sums errors before a record is
    // read. The sum is associative and commutative, and any grouping of the records returns the serial sum.
    // cycle_record_sum reads records in device memory: each 32-bit limb of the field is summed over a run into 64 bits,
    // which hold 2^32 of them, the negative fields are counted beside, and the host takes the carries and subtracts
    // 2^bits a negative field once a run; only those totals come back. cycle_record_sum_host reads records in host
    // memory and adds them in order, each sign-extended to `sum_limbs` limbs.
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

    // 1 where a sum request is whole, the check both routes make before a record is read
    int cycle_record_sum_valid(const CycleRecordSumRequest *request);

    long cycle_record_sum_host(const CycleRecordSumRequest *request);

    // cycle_record_sum keeps its device scratch between calls, grown to the most a sum has asked; this gives it back
    void cycle_record_sum_release(void);

    // cycle_run keeps its scratch, its lanes' table and its folds on the device between calls; this gives them back
    void cycle_resident_release(void);

    // The sort: within each run of `group` consecutive records of `count`, the lanes in the order of their output at
    // bit `offset`, `bits` wide, read as a magnitude, least first, and records whose outputs are equal in the order of
    // their lanes. `order` takes `count` lanes, 32 bits each: entry i of a run is the lane of the run's record that
    // stands i-th. `order` is an index a sweep reads its records through. `count` is a whole number of runs and at
    // most 2^32 records. cycle_record_sort reads records and writes the order in device memory: a run whose records
    // and lanes fit one thread block's shared memory is sorted there by a bitonic network over (output, lane), and a
    // longer run by a radix sort of four bits a pass over the device, the output's bits and then the run's number.
    // cycle_record_sort_host reads and writes them in host memory, by a merge that keeps equal outputs in lane order.
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

    // 1 where a sort request is whole, the check both routes make before a record is read
    int cycle_record_sort_valid(const CycleRecordSortRequest *request);

    long cycle_record_sort_host(const CycleRecordSortRequest *request);

#ifdef __cplusplus
}
#endif

#endif
