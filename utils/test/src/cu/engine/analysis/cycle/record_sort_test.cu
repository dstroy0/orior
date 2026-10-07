// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The sort across lanes. cycle_record_sort reads a field of every record in device memory and writes, for each run of
// consecutive records, the run's lanes in the order of their fields read as magnitudes, equal fields in lane order: a
// run that fits one thread block's shared memory by a bitonic network over (field, lane), a longer run by a radix sort
// of four bits a pass. cycle_record_sort_host merges the same records in host memory. The two must agree word for word,
// and each run's order must be a permutation of its own lanes whose fields never fall and whose equal fields keep their
// lanes rising:
// - random records of nine limbs, fields one bit wide, four bits across two limbs, a whole limb, 37 bits across two
//   limbs, two hundred bits and the whole record, over runs of 1, 3, 1024, 16384 and every record, so that both the
//   network and the radix sort run, the one-bit field holding a tie in nearly every pair;
// - the order as an index: a record program sweeps the records through it and writes each run's fields rising.
// A request whose count is not a whole number of runs, whose count passes 2^32 records, or whose field passes its
// record errors on both routes before a record is read. The test is one job on the device's tessera daemon.
#include "../../../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SORT_TEST_LINE 8192ull

// three times 2^14, so that runs of 3 divide them and a run of every record passes the shared memory
#define SORT_TEST_RECORDS (3ull * (1ull << 14u))

#define SORT_TEST_RECORD_LIMBS 9u

#define SORT_TEST_FIELDS 6u

#define SORT_TEST_GROUPS 5u

// the most the test puts on the device at once: the records, the order, the swept records, and room beside for the
// sort's spare order and counts and the index program's compiled module
#define SORT_TEST_DECLARED ((SORT_TEST_RECORDS * (SORT_TEST_RECORD_LIMBS + 4ull) * sizeof(unsigned int)) + (96ull << 20u))

typedef struct
{
    unsigned long long checks;
    unsigned long long failures;
    ScripturaLine line;
} SortResults;

static unsigned long long s_sort_state = 0x5EED5027C0FFEE11ull;

static unsigned long long sort_random(void)
{
    s_sort_state ^= s_sort_state << 13u;
    s_sort_state ^= s_sort_state >> 7u;
    s_sort_state ^= s_sort_state << 17u;
    return s_sort_state;
}

static void sort_check(SortResults *results, int passed, const char *what)
{
    results->checks += 1ull;
    if (passed == 0)
    {
        results->failures += 1ull;
        scriptura_text(&results->line, "  FAILED: ");
        scriptura_text(&results->line, what);
        scriptura_character(&results->line, '\n');
    }
}

static long sort_run(int device, const unsigned int *records, unsigned long long count, unsigned long long group,
                     unsigned int out_limbs, unsigned int offset, unsigned int bits, unsigned int *order,
                     EngineError *error)
{
    const CycleRecordSortRequest request = {records, count, group, out_limbs, offset, bits, order, error};
    return (device != 0) ? cycle_record_sort(&request) : cycle_record_sort_host(&request);
}

// the order of two records' fields read as magnitudes, bit by bit from the top: -1, 0 or 1
static int sort_field_order(const unsigned int *records, unsigned int out_limbs, unsigned int offset,
                            unsigned int bits, unsigned int one, unsigned int other)
{
    const unsigned int *const first = &records[(unsigned long long)one * out_limbs];
    const unsigned int *const second = &records[(unsigned long long)other * out_limbs];
    for (unsigned int bit = bits; bit > 0u; bit -= 1u)
    {
        const unsigned int at = offset + bit - 1u;
        const unsigned int left = (first[at / 32u] >> (at % 32u)) & 1u;
        const unsigned int right = (second[at / 32u] >> (at % 32u)) & 1u;
        if (left != right)
        {
            return (left > right) ? 1 : -1;
        }
    }
    return 0;
}

// 1 where each run's order is a permutation of its own lanes, its fields never fall, and equal fields keep their lanes
// rising
static int sort_ordered(const unsigned int *records, const unsigned int *order, unsigned long long count,
                        unsigned long long group, unsigned int out_limbs, unsigned int offset, unsigned int bits,
                        unsigned char *seen)
{
    memset(seen, 0, (size_t)count);
    int ok = 1;
    for (unsigned long long at = 0ull; (ok != 0) && (at < count); at += 1ull)
    {
        const unsigned long long run = at / group;
        const unsigned int lane = order[at];
        ok = (lane < count) && ((lane / group) == run) && (seen[lane] == 0u);
        if (ok != 0)
        {
            seen[lane] = 1u;
        }
        if ((ok != 0) && ((at % group) != 0ull))
        {
            const unsigned int before = order[at - 1ull];
            const int compared = sort_field_order(records, out_limbs, offset, bits, before, lane);
            ok = (compared < 0) || ((compared == 0) && (before < lane));
        }
    }
    return ok;
}

// random records of nine limbs, sorted on the device and the host
static void sort_records(SortResults *results, unsigned int *records, unsigned int *device_records)
{
    const size_t words = (size_t)SORT_TEST_RECORDS * SORT_TEST_RECORD_LIMBS;
    unsigned int *const device_order = (unsigned int *)malloc((size_t)SORT_TEST_RECORDS * sizeof(unsigned int));
    unsigned int *const host_order = (unsigned int *)malloc((size_t)SORT_TEST_RECORDS * sizeof(unsigned int));
    unsigned char *const seen = (unsigned char *)malloc((size_t)SORT_TEST_RECORDS);
    unsigned int *order_on_device = NULL;
    int ok = (device_order != NULL) && (host_order != NULL) && (seen != NULL) &&
             (cudaMalloc((void **)&order_on_device, (size_t)SORT_TEST_RECORDS * sizeof(unsigned int)) == cudaSuccess);
    sort_check(results, ok, "the orders are held on the host and the device");
    const unsigned int offsets[SORT_TEST_FIELDS] = {0u, 30u, 32u, 7u, 7u, 0u};
    const unsigned int bits[SORT_TEST_FIELDS] = {1u, 4u, 32u, 37u, 200u, 32u * SORT_TEST_RECORD_LIMBS};
    const unsigned long long groups[SORT_TEST_GROUPS] = {1ull, 3ull, 1024ull, 16384ull, SORT_TEST_RECORDS};
    for (unsigned int field = 0u; (ok != 0) && (field < SORT_TEST_FIELDS); field += 1u)
    {
        for (size_t word = 0u; word < words; word += 1u)
        {
            records[word] = (unsigned int)sort_random();
        }
        ok = cudaMemcpy(device_records, records, words * sizeof(unsigned int), cudaMemcpyHostToDevice) == cudaSuccess;
        for (unsigned int g = 0u; (ok != 0) && (g < SORT_TEST_GROUPS); g += 1u)
        {
            EngineError error;
            memset(&error, 0, sizeof(error));
            const int both =
                (sort_run(1, device_records, SORT_TEST_RECORDS, groups[g], SORT_TEST_RECORD_LIMBS, offsets[field],
                          bits[field], order_on_device, &error) != CYCLE_ERROR) &&
                (cudaMemcpy(device_order, order_on_device, (size_t)SORT_TEST_RECORDS * sizeof(unsigned int),
                            cudaMemcpyDeviceToHost) == cudaSuccess) &&
                (sort_run(0, records, SORT_TEST_RECORDS, groups[g], SORT_TEST_RECORD_LIMBS, offsets[field],
                          bits[field], host_order, &error) != CYCLE_ERROR);
            const int same =
                both && (memcmp(device_order, host_order, (size_t)SORT_TEST_RECORDS * sizeof(unsigned int)) == 0);
            char what[200];
            snprintf(what, sizeof(what), "fields of %u bits at bit %u, runs of %llu: the device equals the host word "
                                         "for word", bits[field], offsets[field], groups[g]);
            sort_check(results, same, what);
            const int ordered = both && sort_ordered(records, host_order, SORT_TEST_RECORDS, groups[g],
                                                     SORT_TEST_RECORD_LIMBS, offsets[field], bits[field], seen);
            snprintf(what, sizeof(what), "fields of %u bits at bit %u, runs of %llu: each run is its own lanes, its "
                                         "fields rising and its ties in lane order", bits[field], offsets[field],
                     groups[g]);
            sort_check(results, ordered, what);
        }
    }
    // the refusals, on both routes
    for (int device = 0; (ok != 0) && (device < 2); device += 1)
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        const unsigned int *const source = (device != 0) ? device_records : records;
        unsigned int *const target = (device != 0) ? order_on_device : host_order;
        sort_check(results,
                   sort_run(device, source, SORT_TEST_RECORDS, 1000ull, SORT_TEST_RECORD_LIMBS, 0u, 32u, target,
                            &error) == CYCLE_ERROR,
                   (device != 0) ? "the device refuses a count that is not a whole number of runs"
                                 : "the host refuses a count that is not a whole number of runs");
        sort_check(results,
                   sort_run(device, source, (1ull << 32u) + 1ull, (1ull << 32u) + 1ull, SORT_TEST_RECORD_LIMBS, 0u,
                            32u, target, &error) == CYCLE_ERROR,
                   (device != 0) ? "the device refuses a count past 2^32 records"
                                 : "the host refuses a count past 2^32 records");
        sort_check(results,
                   sort_run(device, source, SORT_TEST_RECORDS, SORT_TEST_RECORDS, SORT_TEST_RECORD_LIMBS, 1u,
                            32u * SORT_TEST_RECORD_LIMBS, target, &error) == CYCLE_ERROR,
                   (device != 0) ? "the device refuses a field that passes its record"
                                 : "the host refuses a field that passes its record");
    }
    cudaFree(order_on_device);
    free(device_order);
    free(host_order);
    free(seen);
}

// the order as an index: a program reads member 0's field 0, 32 bits at bit 32, through the order and writes it out.
// Each run's outputs must rise, and equal the records' fields in the order's sequence
static void sort_index(SortResults *results, unsigned int *records, unsigned int *device_records)
{
    const unsigned long long group = 1024ull;
    const size_t words = (size_t)SORT_TEST_RECORDS * SORT_TEST_RECORD_LIMBS;
    for (size_t word = 0u; word < words; word += 1u)
    {
        records[word] = (unsigned int)sort_random();
    }
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record = NULL;
    EngineError error;
    memset(&key, 0, sizeof(key));
    memset(&layout, 0, sizeof(layout));
    memset(&error, 0, sizeof(error));
    EngineRecordStep steps[1] = {EngineRecordStep{ENGINE_RECORD_FIELD, 0u, 0u, 0u}};
    const unsigned int outputs[1] = {0u};
    const unsigned int field_bits[1] = {32u};
    const unsigned int field_offset[1] = {32u};
    unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {SORT_TEST_RECORD_LIMBS, 0u, 0u};
    const KeymathRecordRequest encode = {steps, 1u, field_bits, 1u, 1u, outputs, 1u, NULL, 0u, &key, &error};
    const KeyScheduleRecordRequest lay = {&key, field_offset, 1u, in_limbs, 1, &layout, &error};
    int ok = (keymath_record_encode(&encode) != KEYMATH_ERROR) &&
             (key_schedule_record_layout(&lay) != KEY_SCHEDULE_ERROR) &&
             (cycle_record_load(&layout, &record, &error) != CYCLE_ERROR);
    sort_check(results, ok, "the index program is encoded, laid out and loaded");
    const unsigned int out_limbs = layout.out_limbs;
    unsigned int *const order = (unsigned int *)malloc((size_t)SORT_TEST_RECORDS * sizeof(unsigned int));
    unsigned int *const out = (unsigned int *)malloc((size_t)SORT_TEST_RECORDS * out_limbs * sizeof(unsigned int));
    unsigned int *device_order = NULL;
    unsigned int *device_out = NULL;
    ok = ok && (order != NULL) && (out != NULL) &&
         (cudaMemcpy(device_records, records, words * sizeof(unsigned int), cudaMemcpyHostToDevice) == cudaSuccess) &&
         (cudaMalloc((void **)&device_order, (size_t)SORT_TEST_RECORDS * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_out, (size_t)SORT_TEST_RECORDS * out_limbs * sizeof(unsigned int)) ==
          cudaSuccess) &&
         (sort_run(1, device_records, SORT_TEST_RECORDS, group, SORT_TEST_RECORD_LIMBS, 32u, 32u, device_order,
                   &error) != CYCLE_ERROR);
    CycleRecordRunRequest sweep;
    memset(&sweep, 0, sizeof(sweep));
    sweep.record = record;
    sweep.device_in[0] = device_records;
    sweep.bodies[0] = SORT_TEST_RECORDS;
    sweep.device_index = device_order;
    sweep.count = SORT_TEST_RECORDS;
    sweep.device_out = device_out;
    sweep.error = &error;
    ok = ok && (cycle_record_run(&sweep) != CYCLE_ERROR) &&
         (cudaMemcpy(out, device_out, (size_t)SORT_TEST_RECORDS * out_limbs * sizeof(unsigned int),
                     cudaMemcpyDeviceToHost) == cudaSuccess) &&
         (cudaMemcpy(order, device_order, (size_t)SORT_TEST_RECORDS * sizeof(unsigned int), cudaMemcpyDeviceToHost) ==
          cudaSuccess);
    sort_check(results, ok, "the program sweeps the records through the order as its index");
    int rising = ok;
    int matching = ok;
    const DeviceRecordStep *const step = &layout.step_table[0];
    for (unsigned long long at = 0ull; (ok != 0) && (at < SORT_TEST_RECORDS); at += 1ull)
    {
        // the output is the 32-bit field, laid at its offset in 33 bits; a field below 2^32 fills its low 32
        const unsigned int *const written = &out[at * out_limbs];
        unsigned long long value = 0ull;
        for (unsigned int bit = 0u; bit < 32u; bit += 1u)
        {
            const unsigned int from = step->out_offset + bit;
            value |= (unsigned long long)((written[from / 32u] >> (from % 32u)) & 1u) << bit;
        }
        matching = matching && (value == records[((unsigned long long)order[at] * SORT_TEST_RECORD_LIMBS) + 1u]);
        if ((at % group) != 0ull)
        {
            const unsigned int *const before = &out[(at - 1ull) * out_limbs];
            unsigned long long previous = 0ull;
            for (unsigned int bit = 0u; bit < 32u; bit += 1u)
            {
                const unsigned int from = step->out_offset + bit;
                previous |= (unsigned long long)((before[from / 32u] >> (from % 32u)) & 1u) << bit;
            }
            rising = rising && (previous <= value);
        }
    }
    sort_check(results, matching, "each swept output is the field of the lane the order names");
    sort_check(results, rising, "each run of 1024 swept outputs rises");
    cudaFree(device_order);
    cudaFree(device_out);
    free(order);
    free(out);
    if (record != NULL)
    {
        cycle_record_release(record);
    }
    key_schedule_record_release(&layout);
    keymath_record_release(&key);
}

int main(int count, char **arguments)
{
    SortResults results;
    results.checks = 0ull;
    results.failures = 0ull;
    results.line.capacity = SORT_TEST_LINE;
    results.line.out = (char *)malloc((size_t)SORT_TEST_LINE);
    results.line.at = 0ull;
    if (results.line.out == NULL)
    {
        return 2;
    }
    char job_capacity[SIM_LINE_CAPACITY];
    SimResults job;
    sim_open(&job, job_capacity);
    const int admitted = sim_job_submit(&job, "record_sort_test", count, arguments, SORT_TEST_DECLARED);
    if (admitted != 0)
    {
        const size_t words = (size_t)SORT_TEST_RECORDS * SORT_TEST_RECORD_LIMBS;
        unsigned int *const records = (unsigned int *)malloc(words * sizeof(unsigned int));
        unsigned int *device_records = NULL;
        const int held = (records != NULL) &&
                         (cudaMalloc((void **)&device_records, words * sizeof(unsigned int)) == cudaSuccess);
        sort_check(&results, held, "the random records are held on the host and the device");
        if (held != 0)
        {
            sort_records(&results, records, device_records);
            sort_index(&results, records, device_records);
        }
        cudaFree(device_records);
        free(records);
    }
    sim_job_release(&job);
    sim_flush(&job);
    sort_check(&results, (admitted != 0) && (job.failures == 0ull),
               "tessera: the device's daemon admits the test's job and it releases");
    scriptura_text(&results.line, "  record sort test: ");
    scriptura_decimal(&results.line, results.checks, 1u);
    scriptura_text(&results.line, " checks, ");
    scriptura_decimal(&results.line, results.failures, 1u);
    scriptura_text(&results.line, " failed\n");
    scriptura_write(&results.line, stdout);
    free(results.line.out);
    return (results.failures == 0ull) ? 0 : 1;
}
