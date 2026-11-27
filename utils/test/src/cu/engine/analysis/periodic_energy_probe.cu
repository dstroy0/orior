// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// Calls the functions of src/sims/cu/engine/analysis/art/periodic_energy.h on the sequences a file contains and prints
// each result in full, every exact integer in hexadecimal with its sign. It is the engine's side of
// archive/utils/test/src/python/engine/analysis/periodic_energy_test.py, which writes the file and reads these lines
// against measure/periodic_energy.py.
//
//   periodic_energy_probe <requests file>
//
// The file contains records one after another, every field little endian:
//   u32 magic 0x314E4550, u32 has_target, u64 length, u64 reach, u64 draws, u64 key, u64 period, u64 places,
//   i64 values[length], and i64 target[length] when has_target is 1.
//
// For each record it prints, in this order: energy_recover over the reach, energy_ratio at every period the recover
// scanned (from the recover's phase sums), energy_reduction at the period against the target, energy_welford at the
// period, energy_shuffle keyed by the record's key, energy_band_top over the draws, and energy_above for the recover's
// measurement against the band's top. Each measurement is also printed by energy_print, at `places` decimals where
// the probe calls sim_ratio_print itself. The results' own lines, where a device call failed, start with "results",
// and archive/utils/test/src/python/engine/analysis/periodic_energy_test.py leaves them out of the comparison.
//
// The runtime's last error is cleared before each call that launches. At 1789287 the header checks a launch with
// cudaGetLastError(), which also returns an error an earlier failed call left and sim_status_check already
// reported. Without the clearing, one request's failed allocation is read as the next call's launch failure.
// Each call is graded here on its own.
//
// The probe is a job on the device's tessera daemon (sims/cu/sim_job.cu). It reads the file once to size its
// declaration, submits the job, and then runs the records. Its exit status is the file's and the job's: a device call
// that fails is a "results" line the test grades, as before the probe was a job.

#include "../../../../../../src/cu/engine/runtime/device_pool/device_pool.h"
#include "../../../../../../includes/platform/platform.h"
#include "periodic_energy.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

#define PROBE_MAGIC 0x314E4550u

typedef struct
{
    unsigned int magic;
    unsigned int has_target;
    unsigned long long length;
    unsigned long long reach;
    unsigned long long draws;
    unsigned long long key;
    unsigned long long period;
    unsigned long long places;
} ProbeRecord;

// one byte past the results' capacity stores the end of its text
static char s_results_text[SIM_LINE_CAPACITY + 1ull];

// the record's fixed fields in the file's order; 0 at the end of the file, -1 when they are malformed
static int probe_header(FILE *file, ProbeRecord *record)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = record, .bytes = sizeof(*record), .val = 0);
    if (fread(&record->magic, sizeof(record->magic), 1u, file) != 1u)
    {
        return 0;
    }
    const int ok = (record->magic == PROBE_MAGIC) &&
                   (fread(&record->has_target, sizeof(unsigned int), 1u, file) == 1u) &&
                   (fread(&record->length, sizeof(unsigned long long), 6u, file) == 6u);
    return ok ? 1 : -1;
}

static unsigned long long probe_pages(unsigned long long bytes)
{
    return (bytes + DEVICE_POOL_PAGE_BYTES - 1ull) / DEVICE_POOL_PAGE_BYTES;
}

// The bytes the job declares: the max any one call places on the device, each of its allocations rounded up to pages.
// energy_phase_sums stores the values and the phase sums, energy_welford the values and two arrays of one entry per
// phase, and each frees them before it returns. 0 when a record is malformed.
static unsigned long long probe_declared(FILE *file)
{
    unsigned long long pages_max = 1ull;
    for (;;)
    {
        ProbeRecord record;
        const int read = probe_header(file, &record);
        if (read == 0)
        {
            break;
        }
        if (read < 0)
        {
            return 0ull;
        }
        const unsigned long long sequences = (record.has_target != 0u) ? 2ull : 1ull;
        for (unsigned long long at = 0ull; at < (record.length * sequences); at += 1ull)
        {
            long long value = 0ll;
            if (fread(&value, sizeof(value), 1u, file) != 1u)
            {
                return 0ull;
            }
        }
        const unsigned long long value_pages = probe_pages(record.length * sizeof(long long));
        // energy_recover's own bound: a range below 2 or fewer than 4 values never reaches the device
        if ((record.length >= 4ull) && (record.reach >= 2ull))
        {
            const unsigned long long top =
                (record.reach < (record.length - 1ull)) ? record.reach : (record.length - 1ull);
            const unsigned long long phase_count = energy_phase_base(top + 1ull);
            const unsigned long long pages = value_pages + probe_pages(phase_count * sizeof(long long));
            pages_max = (pages > pages_max) ? pages : pages_max;
        }
        if ((record.period != 0ull) && (record.length != 0ull))
        {
            const unsigned long long pages = value_pages + (2ull * probe_pages(record.period * sizeof(long long)));
            pages_max = (pages > pages_max) ? pages : pages_max;
        }
    }
    return pages_max * DEVICE_POOL_PAGE_BYTES;
}

static void probe_exact(const OriorExactInteger *value)
{
    if (value->sign == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "0");
            platform_line_out_ended();
        }
        return;
    }
    unsigned int top = ORIOR_EXACT_LIMBS;
    while ((top > 1u) && (value->limb[top - 1u] == 0u))
    {
        top -= 1u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (value->sign < 0) ? "-" : "");
        mmgr_line_hex(output_line, (unsigned int)value->limb[top - 1u], 1u);
        platform_line_out_ended();
    }
    for (unsigned int limb = top - 1u; limb > 0u; limb -= 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_hex(output_line, (unsigned int)value->limb[limb - 1u], 8u);
        platform_line_out_ended();
    }
}

static void probe_ratio(const OriorExactInteger *numerator, const OriorExactInteger *denominator)
{
    probe_exact(numerator);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "/");
        platform_line_out_ended();
    }
    probe_exact(denominator);
}

static void probe_measurement(const char *name, int status, const EnergyMeasurement *measurement, unsigned int places)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, " status ");
        mmgr_line_decimal_signed(output_line, status);
        mmgr_line_put(output_line, " found ");
        mmgr_line_decimal_signed(output_line, (status != 0) ? measurement->found : 0);
        platform_line_out_ended();
    }
    if ((status != 0) && (measurement->found != 0))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " period ");
            mmgr_line_decimal(output_line, measurement->period, 1u);
            mmgr_line_put(output_line, " ratio ");
            platform_line_out_ended();
        }
        probe_ratio(&measurement->numerator, &measurement->denominator);
        char text[256];
        VerbaLinea line;
        line.out = text;
        line.capacity = sizeof(text) - 1u;
        line.at = 0ull;
        energy_print(&line, measurement);
        text[line.at] = '\0';
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " print ");
            mmgr_line_put(output_line, text);
            platform_line_out_ended();
        }
        line.at = 0ull;
        sim_ratio_print(&line, &measurement->numerator, &measurement->denominator, places);
        text[line.at] = '\0';
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " places ");
            mmgr_line_put(output_line, text);
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

static void probe_results(SimResults *results)
{
    // the results' failed checks, one "results" line each
    results->line.out[results->line.at] = '\0';
    char *at = results->line.out;
    while (*at != '\0')
    {
        // the line end reached through the line itself, which the loop writes
        const char *const end_found =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\n');
        char *end = (end_found != NULL) ? &at[end_found - at] : NULL;
        if (end != NULL)
        {
            *end = '\0';
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "results ");
            mmgr_line_put(output_line, at);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (end == NULL)
        {
            break;
        }
        at = end + 1;
    }
    results->line.at = 0ull;
}

static void probe_run(SimResults *results, const ProbeRecord *record, const long long *values, const long long *target,
                      unsigned long long number)
{
    const unsigned long long length = record->length;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "case ");
        mmgr_line_decimal(output_line, number, 1u);
        mmgr_line_put(output_line, " length ");
        mmgr_line_decimal(output_line, length, 1u);
        mmgr_line_put(output_line, " reach ");
        mmgr_line_decimal(output_line, record->reach, 1u);
        mmgr_line_put(output_line, " draws ");
        mmgr_line_decimal(output_line, record->draws, 1u);
        mmgr_line_put(output_line, " period ");
        mmgr_line_decimal(output_line, record->period, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long top =
        (length >= 1ull) ? ((record->reach < (length - 1ull)) ? record->reach : (length - 1ull)) : 0ull;
    const unsigned long long phase_count = (top >= 2ull) ? energy_phase_base(top + 1ull) : 1ull;
    long long *const sums =
        (long long *)platform_cell_cleared((size_t)phase_count, sizeof(long long), platform_place_g_program_owner, 0);
    long long *const shuffled = (long long *)platform_cell_cleared((size_t)(length + 1ull), sizeof(long long),
                                                                   platform_place_g_program_owner, 0);
    const unsigned long long phases = (record->period != 0ull) ? record->period : 1ull;
    long long *const welford_numerator =
        (long long *)platform_cell_cleared((size_t)phases, sizeof(long long), platform_place_g_program_owner, 0);
    unsigned long long *const welford_denominator = (unsigned long long *)platform_cell_cleared(
        (size_t)phases, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    if ((sums == NULL) || (shuffled == NULL) || (welford_numerator == NULL) || (welford_denominator == NULL))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "host memory not allocated\nend");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_cell_released(sums, platform_place_g_program_owner);
        platform_cell_released(shuffled, platform_place_g_program_owner);
        platform_cell_released(welford_numerator, platform_place_g_program_owner);
        platform_cell_released(welford_denominator, platform_place_g_program_owner);
        return;
    }
    const unsigned int places = (unsigned int)record->places;

    EnergyMeasurement live;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &live, .bytes = sizeof(live), .val = 0);
    (void)cudaGetLastError();
    const int recovered = energy_recover(results, values, length, record->reach, sums, &live);
    probe_measurement("recover", recovered, &live, places);
    probe_results(results);

    for (unsigned long long period = 2ull; (recovered != 0) && (period <= top); period += 1ull)
    {
        OriorExactInteger numerator;
        OriorExactInteger denominator;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "ratio ");
            mmgr_line_decimal(output_line, period, 1u);
            mmgr_line_put(output_line, " ");
            platform_line_out_ended();
        }
        if (energy_ratio(values, length, sums, period, &numerator, &denominator) != 0)
        {
            probe_ratio(&numerator, &denominator);
        }
        else
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "none");
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    if ((record->has_target != 0u) && (recovered != 0) && (record->period >= 2ull) && (record->period <= top))
    {
        OriorExactInteger numerator;
        OriorExactInteger denominator;
        const int reduced = energy_reduction(values, target, length, record->period,
                                             &sums[energy_phase_base(record->period)], &numerator, &denominator);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "reduction status ");
            mmgr_line_decimal_signed(output_line, reduced);
            mmgr_line_put(output_line, " ");
            platform_line_out_ended();
        }
        probe_ratio(&numerator, &denominator);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    if ((record->period != 0ull) && (length != 0ull))
    {
        (void)cudaGetLastError();
        const int averaged =
            energy_welford(results, values, length, record->period, welford_numerator, welford_denominator);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "welford status ");
            mmgr_line_decimal_signed(output_line, averaged);
            platform_line_out_ended();
        }
        for (unsigned long long phase = 0ull; (averaged != 0) && (phase < record->period); phase += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_signed(output_line, welford_numerator[phase]);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, welford_denominator[phase], 1u);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        probe_results(results);
    }

    if (length != 0ull)
    {
        energy_shuffle(values, shuffled, length, record->key);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "shuffle");
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_signed(output_line, shuffled[at]);
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }

        EnergyMeasurement band;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &band, .bytes = sizeof(band), .val = 0);
        unsigned long long reached = 0ull;
        (void)cudaGetLastError();
        const int banded = energy_band_top(results, values, length, record->reach, record->draws, record->key, shuffled,
                                           sums, &band, &reached);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "reached ");
            mmgr_line_decimal(output_line, (banded != 0) ? reached : 0ull, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        probe_measurement("band", banded, &band, places);
        probe_results(results);
        if ((recovered != 0) && (banded != 0))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "above ");
            mmgr_line_decimal_signed(output_line, energy_above(&live, &band));
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "end");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(sums, platform_place_g_program_owner);
    platform_cell_released(shuffled, platform_place_g_program_owner);
    platform_cell_released(welford_numerator, platform_place_g_program_owner);
    platform_cell_released(welford_denominator, platform_place_g_program_owner);
}

int program_main(int count, char **arguments)
{
    if (count != 2)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "usage: periodic_energy_probe <requests file>");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    FILE *const file = fopen(arguments[1], "rb");
    if (file == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "periodic_energy_probe: ");
            mmgr_line_put(output_line, arguments[1]);
            mmgr_line_put(output_line, " not opened");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const unsigned long long declared = probe_declared(file);
    rewind(file);
    if (declared == 0ull)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "periodic_energy_probe: a record in ");
            mmgr_line_put(output_line, arguments[1]);
            mmgr_line_put(output_line, " is malformed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        fclose(file);
        return 2;
    }
    SimResults results;
    sim_open(&results, s_results_text);
    if (sim_job_submit(&results, "periodic_energy_probe", count, arguments, declared) == 0)
    {
        fclose(file);
        return sim_close(&results, "periodic energy probe");
    }
    int ok = 1;
    unsigned long long number = 0ull;
    for (;;)
    {
        ProbeRecord record;
        const int header = probe_header(file, &record);
        if (header == 0)
        {
            break;
        }
        long long *const values =
            (header > 0) ? (long long *)platform_cell_cleared((size_t)(record.length + 1ull), sizeof(long long),
                                                              platform_place_g_program_owner, 0)
                         : NULL;
        long long *const target =
            (header > 0) ? (long long *)platform_cell_cleared((size_t)(record.length + 1ull), sizeof(long long),
                                                              platform_place_g_program_owner, 0)
                         : NULL;
        const int body = (values != NULL) && (target != NULL) &&
                         (fread(values, sizeof(long long), (size_t)record.length, file) == (size_t)record.length) &&
                         ((record.has_target == 0u) ||
                          (fread(target, sizeof(long long), (size_t)record.length, file) == (size_t)record.length));
        if (!body)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "periodic_energy_probe: record ");
                mmgr_line_decimal(output_line, number, 1u);
                mmgr_line_put(output_line, " is malformed");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            platform_cell_released(values, platform_place_g_program_owner);
            platform_cell_released(target, platform_place_g_program_owner);
            ok = 0;
            break;
        }
        probe_run(&results, &record, values, target, number);
        platform_cell_released(values, platform_place_g_program_owner);
        platform_cell_released(target, platform_place_g_program_owner);
        number += 1ull;
        platform_line_flushed();
    }
    fclose(file);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "records ");
        mmgr_line_decimal(output_line, number, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long failures = results.failures;
    sim_job_release(&results);
    const int released = results.failures == failures;
    probe_results(&results);
    return (ok && released) ? 0 : 1;
}
