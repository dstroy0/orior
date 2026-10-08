// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// casmi_driver: the CASMI library and its ranking on the engine. --ingest reads a parquet file one row group at a time,
// each double column as the bytes it is stored as, each byte pair one 16-bit lane, and reads every value into its form
// on the device: a count over its row's base, else a decimal on the column's unit, the decimal itself or divided by a
// base, else the stored lanes kept. The record program writes each value's integer on the unit as 16-bit planes and its
// form beside them; the planes are gathered out of the records by strided copies on the device and each sealed as a
// crystal of the set through engine_crystal_write, beside the kept values' lanes, gathered by an index, and each row's
// value count. The run is one job on the device's tessera daemon.
#include "../../../../../src/cu/engine/analysis/compression/compression.h"
#include "../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../../../src/cu/engine/analysis/tower/tower.h"
#include "../../../../../src/cu/engine/engine.h"
#include "../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "../forms/forms.h"
#include "../parquet/parquet.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the columns read: the precursor and the peaks' m/z on a decimal unit, the intensities over their row's base, and
// the base column itself kept as it is stored
#define DRIVER_COLUMNS 4u
#define DRIVER_PRECURSOR 0u
#define DRIVER_PEAKS 1u
#define DRIVER_INTENSITIES 2u
#define DRIVER_BASES 3u

static const char *const DRIVER_COLUMN_PATH[DRIVER_COLUMNS] = {
    "precursor_mz", "ms2_mzs.list.element", "ms2_normalized_intensities.list.element", "base_peak_intensity"};

// the text columns: each sealed as its distinct strings and each row's index among them; a file without one of them
// has it left out
#define DRIVER_TEXTS 7u
static const char *const DRIVER_TEXT_PATH[DRIVER_TEXTS] = {"ingest_lib",        "ionization_mode", "adduct",
                                                           "molecular_formula", "inchikey14",      "normalized_smiles",
                                                           "molecule_id"};

// a sealed part's name: a double column, or a text column past them
static const char *driver_part_path(unsigned int column)
{
    return (column < DRIVER_COLUMNS) ? DRIVER_COLUMN_PATH[column] : DRIVER_TEXT_PATH[column - DRIVER_COLUMNS];
}

// each column's unit, 10^-places; the bases have none and are kept
static const unsigned int DRIVER_UNIT_PLACES[DRIVER_COLUMNS] = {4u, 4u, 6u, 0u};

// the bases a decimal intensity is divided by in binary64 before it is stored
#define DRIVER_DIVISORS 2u
static const unsigned long long DRIVER_DIVISOR[DRIVER_DIVISORS] = {100ull, 999ull};

// a value's form: a count over its row's base, a decimal on the unit, a decimal divided by each divisor, or kept
#define DRIVER_FORM_COUNT 0u
#define DRIVER_FORM_DECIMAL 1u
#define DRIVER_FORM_DIVIDED 2u
#define DRIVER_FORM_KEPT (DRIVER_FORM_DIVIDED + DRIVER_DIVISORS)

// lanes and limbs of one double. An integer is written as planes of 15 bits, each a 16-bit lane: a record output is
// its register's width and a sign bit, and a value below 2^15 is an output exactly 16 bits wide, its lane. A value on
// the unit takes DRIVER_PLANES planes and a kept value's 64 stored bits DRIVER_KEPT_PLANES
#define DRIVER_LANES_PER_VALUE 4ull
#define DRIVER_LIMBS_PER_VALUE 2u
#define DRIVER_PLANES 4u
#define DRIVER_KEPT_PLANES 5u
#define DRIVER_PLANE_BITS 15u
#define DRIVER_LANE_BITS 16u
#define DRIVER_WORD_BITS 32u

#define DRIVER_SAMPLE_BYTES 256u

static CasmiParquetFooter s_footer;

typedef struct
{
    unsigned long long values;
    unsigned char *bytes;
    unsigned long long rows;
    unsigned long long *row_start;
} DriverColumn;

typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} DriverLoaded;

// what one column of one row group sealed: the planes that hold anything, whether its form plane is sealed, and its
// kept values
typedef struct
{
    unsigned int planes;
    int form;
    unsigned long long kept;
} DriverSealed;

// what one row group's sealing came to, summed over the run
typedef struct
{
    unsigned long long crystals;
    unsigned long long crystal_bytes[DRIVER_COLUMNS + DRIVER_TEXTS];
    unsigned long long raw_bytes[DRIVER_COLUMNS + DRIVER_TEXTS];
    unsigned long long forms[DRIVER_COLUMNS + DRIVER_TEXTS][DRIVER_FORM_KEPT + 1u];
} DriverTally;

static void driver_line_decimal(SimResults *results, const char *before, unsigned long long value)
{
    scriptura_text(&results->line, before);
    scriptura_decimal(&results->line, value, 1u);
}

static void driver_line_end(SimResults *results)
{
    scriptura_character(&results->line, '\n');
    sim_flush(results);
}

static void driver_error_line(SimResults *results, const char *what, const EngineError *error)
{
    scriptura_text(&results->line, "  ");
    scriptura_text(&results->line, what);
    driver_line_decimal(results, ": module ", (unsigned long long)error->module);
    driver_line_decimal(results, ", site ", (unsigned long long)error->site);
    driver_line_decimal(results, ", kind ", (unsigned long long)error->kind);
    driver_line_end(results);
}

static void driver_column_release(DriverColumn *column)
{
    free(column->bytes);
    free(column->row_start);
    memset(column, 0, sizeof(*column));
}

// one row group's values of one double column, as the stored bytes, eight to a value, and row r's values from
// row_start[r] up to row_start[r + 1]
static int driver_column_read(const char *path, unsigned int group, const char *leaf_path, DriverColumn *out)
{
    memset(out, 0, sizeof(*out));
    const CasmiParquetLeafFind find = {&s_footer, leaf_path};
    const long long leaf = casmi_parquet_leaf_find(&find);
    if ((leaf < 0ll) || (s_footer.leaf[leaf].physical != CASMI_PARQUET_DOUBLE))
    {
        return 0;
    }
    CasmiParquetColumn column;
    memset(&column, 0, sizeof(column));
    // the leaf is below CASMI_PARQUET_LEAVES_MOST
    const CasmiParquetColumnRead read = {path, &s_footer, group, (unsigned int)leaf, &column};
    int ok = casmi_parquet_column_bound(&read) >= 0ll;
    column.row_start = ok ? (unsigned long long *)malloc((size_t)(column.row_start_room * 8ull) + 8u) : NULL;
    column.value_bytes = ok ? (unsigned char *)malloc((size_t)column.value_bytes_room + 8u) : NULL;
    column.value_offset = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
    column.value_length = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
    column.scratch = ok ? (unsigned char *)malloc((size_t)column.scratch_room + 8u) : NULL;
    ok = ok && (column.row_start != NULL) && (column.value_bytes != NULL) && (column.value_offset != NULL) &&
         (column.value_length != NULL) && (column.scratch != NULL) && (casmi_parquet_column_read(&read) >= 0ll);
    free(column.value_offset);
    free(column.value_length);
    free(column.scratch);
    if (!ok)
    {
        free(column.row_start);
        free(column.value_bytes);
        return 0;
    }
    out->values = column.values;
    out->bytes = column.value_bytes;
    out->rows = column.rows;
    out->row_start = column.row_start;
    out->row_start[out->rows] = out->values;
    return 1;
}

static int driver_load(ExactRecordProgram *program, const unsigned int *outputs, unsigned int output_count,
                       const unsigned int *member_limbs, unsigned int members, DriverLoaded *loaded, EngineError *error)
{
    memset(error, 0, sizeof(*error));
    memset(loaded, 0, sizeof(*loaded));
    const KeymathRecordRequest encode = {program->steps,  program->count, program->field_bits,   program->fields,
                                         members,         outputs,        output_count,          program->tables,
                                         program->table_count, &loaded->key, error};
    if ((program->failed != 0) || (keymath_record_encode(&encode) == KEYMATH_ERROR))
    {
        return 0;
    }
    unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {0u, 0u, 0u};
    for (unsigned int member = 0u; member < members; member += 1u)
    {
        limbs[member] = member_limbs[member];
    }
    const KeyScheduleRecordRequest layout = {&loaded->key, program->field_offset, program->fields, limbs, 1,
                                             &loaded->layout, error};
    if (key_schedule_record_layout(&layout) == KEY_SCHEDULE_ERROR)
    {
        keymath_record_release(&loaded->key);
        return 0;
    }
    if (cycle_record_load(&loaded->layout, &loaded->record, error) == CYCLE_ERROR)
    {
        key_schedule_record_release(&loaded->layout);
        keymath_record_release(&loaded->key);
        return 0;
    }
    return 1;
}

static void driver_release(DriverLoaded *loaded)
{
    cycle_record_release(loaded->record);
    key_schedule_record_release(&loaded->layout);
    keymath_record_release(&loaded->key);
}

// the least and most exponents as placed over a column's values, 1 for a subnormal, read from the stored bits
static void driver_exponents(const DriverColumn *column, unsigned long long *least, unsigned long long *most)
{
    *least = ~0ull;
    *most = 0ull;
    for (unsigned long long value = 0ull; value < column->values; value += 1ull)
    {
        unsigned long long word = 0ull;
        memcpy(&word, column->bytes + (value * 8ull), 8u);
        unsigned long long biased = (word >> FORMS_MANTISSA_BITS) & ((1ull << FORMS_EXPONENT_BITS) - 1ull);
        biased = (biased == 0ull) ? 1ull : biased;
        *least = (biased < *least) ? biased : *least;
        *most = (biased > *most) ? biased : *most;
    }
    *least = (*least == ~0ull) ? 1ull : *least;
}

// A value's low DRIVER_PLANE_BITS bits as an output exactly one lane wide. The and with 2^15 - 1 is no wider than
// its narrower operand; 2^15 is added first: the sum is wider than the mask, and its low bits are the value's
static unsigned int driver_lane(ExactRecordProgram *program, unsigned int value)
{
    return exact_record_narrow(
        program, exact_record_sum(program, value, exact_record_power_two(program, DRIVER_PLANE_BITS)), DRIVER_PLANE_BITS);
}

// a value never negative written as `planes` planes, its low DRIVER_PLANE_BITS bits first, each an output one lane wide
static void driver_planes(ExactRecordProgram *program, unsigned int value, unsigned int planes, unsigned int *outputs)
{
    const unsigned int plane_size = exact_record_power_two(program, DRIVER_PLANE_BITS);
    unsigned int rest = value;
    for (unsigned int plane = 0u; plane < planes; plane += 1u)
    {
        outputs[plane] = driver_lane(program, rest);
        rest = exact_record_quotient(program, rest, plane_size);
    }
}

// The forms program of a column: each value's integer on the unit as DRIVER_PLANES planes, the outputs laid first so
// plane j sits at bit 16 j of every record, and its form after them, one lane. A column on a decimal unit
// reads each value as a decimal of at most the unit's places, else kept; the intensities read a count over their
// row's base, member 1, first, then a decimal, then a decimal divided by each divisor. A kept value's integer is the
// floor of the value on the unit; the planes keep their run
static void driver_forms_program(ExactRecordProgram *program, unsigned int column, unsigned long long most_placed,
                                 unsigned int shift_bits, unsigned int *outputs)
{
    const unsigned int places = DRIVER_UNIT_PLACES[column];
    FormsFields fields;
    forms_fields(program, &fields);
    FormsDouble value;
    forms_double(program, &fields, 0u, &value);
    FormsPreimage preimage;
    forms_preimage(program, &value, most_placed, shift_bits, &preimage);
    unsigned int unit = forms_floor(program, &preimage, places);
    unsigned int form = exact_record_constant(program, DRIVER_FORM_KEPT);
    // the forms in reverse of their order, each taking the value where it holds; the first that holds stands
    unsigned int candidate[1u + 1u + DRIVER_DIVISORS];
    unsigned int candidate_held[1u + 1u + DRIVER_DIVISORS];
    unsigned int candidate_form[1u + 1u + DRIVER_DIVISORS];
    unsigned int candidates = 0u;
    if (column == DRIVER_INTENSITIES)
    {
        FormsDouble base;
        forms_double(program, &fields, 1u, &base);
        forms_count(program, &preimage, &base, &candidate_held[candidates], &candidate[candidates]);
        candidate_form[candidates] = DRIVER_FORM_COUNT;
        candidates += 1u;
    }
    forms_unit(program, &preimage, places, &candidate_held[candidates], &candidate[candidates]);
    candidate_form[candidates] = DRIVER_FORM_DECIMAL;
    candidates += 1u;
    if (column == DRIVER_INTENSITIES)
    {
        for (unsigned int divisor = 0u; divisor < DRIVER_DIVISORS; divisor += 1u)
        {
            FormsPreimage divided;
            unsigned int exists = 0u;
            forms_divided(program, &preimage, DRIVER_DIVISOR[divisor], &divided, &exists);
            unsigned int held = 0u;
            forms_unit(program, &divided, places, &held, &candidate[candidates]);
            candidate_held[candidates] = exact_record_product(program, exists, held);
            candidate_form[candidates] = DRIVER_FORM_DIVIDED + divisor;
            candidates += 1u;
        }
    }
    for (unsigned int each = candidates; each > 0u; each -= 1u)
    {
        const unsigned int held = candidate_held[each - 1u];
        unit = exact_record_select(program, held, candidate[each - 1u], unit);
        form = exact_record_select(program, held, exact_record_constant(program, candidate_form[each - 1u]), form);
    }
    driver_planes(program, unit, DRIVER_PLANES, outputs);
    outputs[DRIVER_PLANES] = driver_lane(program, form);
}

// the kept values' program: each record's 64 stored bits, its two 32-bit words, as DRIVER_KEPT_PLANES planes
static void driver_kept_program(ExactRecordProgram *program, unsigned int *outputs)
{
    const unsigned int low = exact_record_member_field(program, DRIVER_WORD_BITS, 0u);
    const unsigned int high = exact_record_member_field(program, DRIVER_WORD_BITS, DRIVER_WORD_BITS);
    const unsigned int stored = exact_record_sum(
        program, exact_record_read_unsigned(program, low, 0u),
        exact_record_product(program, exact_record_read_unsigned(program, high, 0u),
                             exact_record_power_two(program, DRIVER_WORD_BITS)));
    driver_planes(program, stored, DRIVER_KEPT_PLANES, outputs);
}

// 1 where output `output` of a loaded program sits at bit `offset` and is `bits` wide
static int driver_output_at(const DriverLoaded *loaded, unsigned int output, unsigned int offset, unsigned int bits)
{
    const DeviceRecordStep place = loaded->layout.step_table[output];
    return (place.out_offset == offset) && (place.out_bits == bits);
}

// 1 where output j of a loaded program sits at bit 16 j and is one lane wide, for each of `count` outputs; the outputs'
// places printed where they do not
static int driver_laid(SimResults *results, const char *program, const DriverLoaded *loaded,
                       const unsigned int *outputs, unsigned int count)
{
    int laid = 1;
    for (unsigned int output = 0u; output < count; output += 1u)
    {
        laid = laid && driver_output_at(loaded, outputs[output], output * DRIVER_LANE_BITS, DRIVER_LANE_BITS);
    }
    if (!laid)
    {
        scriptura_text(&results->line, "    ");
        scriptura_text(&results->line, program);
        scriptura_text(&results->line, "'s outputs, bit and width:");
        for (unsigned int output = 0u; output < count; output += 1u)
        {
            const DeviceRecordStep place = loaded->layout.step_table[outputs[output]];
            driver_line_decimal(results, " ", (unsigned long long)place.out_offset);
            driver_line_decimal(results, "/", (unsigned long long)place.out_bits);
        }
        driver_line_decimal(results, "; record ", (unsigned long long)loaded->layout.out_bits);
        driver_line_end(results);
    }
    return laid;
}

static int driver_sample_name(char *out, unsigned int group, unsigned int column, const char *part)
{
    const int written = snprintf(out, DRIVER_SAMPLE_BYTES, "row_group_%u/%s.%s", group, driver_part_path(column), part);
    return (written > 0) && (written < (int)DRIVER_SAMPLE_BYTES);
}

// one lattice of device lanes sealed as a crystal of the set, its bytes added to the column's tally
static int driver_seal(SimResults *results, const char *set, const char *sample, const unsigned short *device_lanes,
                       const unsigned long long extent[4], unsigned int column, DriverTally *tally)
{
    EngineSampleRecord record;
    memset(&record, 0, sizeof(record));
    EngineError error;
    memset(&error, 0, sizeof(error));
    const EngineCrystalRequest write = {set,  sample, device_lanes, {extent[0], extent[1], extent[2], extent[3]},
                                        NULL, 0ull,   NULL,         &record,
                                        &error};
    const int ok = engine_crystal_write(&write) == 0L;
    if (!ok)
    {
        driver_error_line(results, "a crystal did not seal", &error);
        return 0;
    }
    tally->crystals += 1ull;
    tally->crystal_bytes[column] += record.crystal_bytes;
    scriptura_text(&results->line, "    ");
    scriptura_text(&results->line, sample);
    driver_line_decimal(results, ": ", record.crystal_bytes);
    driver_line_decimal(results, " bytes of ", record.raw_bytes);
    driver_line_end(results);
    return 1;
}

// One column of one row group read into its forms on the device and sealed: its planes that hold anything, its form
// plane where any value is not in the column's first form, and its kept values' lanes as four planes
static int driver_column_forms(SimResults *results, const char *set, unsigned int group, unsigned int column,
                               const DriverColumn *values, const DriverColumn *bases, DriverTally *tally,
                               DriverSealed *sealed)
{
    memset(sealed, 0, sizeof(*sealed));
    const unsigned long long count = values->values;
    if (count == 0ull)
    {
        return 1;
    }
    unsigned long long least_placed = 0ull;
    unsigned long long most_placed = 0ull;
    driver_exponents(values, &least_placed, &most_placed);
    if (most_placed >= FORMS_LIFT)
    {
        sim_check(results, 0, "the column holds no value past 2^53");
        return 0;
    }
    const unsigned int shift_bits = exact_record_bits_of(FORMS_LIFT - least_placed) + 1u;
    ExactRecordProgram program;
    exact_record_open(&program);
    unsigned int outputs[DRIVER_PLANES + 1u];
    driver_forms_program(&program, column, most_placed, shift_bits, outputs);
    const unsigned int members = (column == DRIVER_INTENSITIES) ? 2u : 1u;
    DriverLoaded loaded;
    EngineError error;
    const unsigned int doubles[ENGINE_RECORD_MEMBERS_MAX] = {DRIVER_LIMBS_PER_VALUE, DRIVER_LIMBS_PER_VALUE,
                                                             DRIVER_LIMBS_PER_VALUE};
    if (driver_load(&program, outputs, DRIVER_PLANES + 1u, doubles, members, &loaded, &error) == 0)
    {
        driver_error_line(results, "the forms program did not load", &error);
        exact_record_close(&program);
        return 0;
    }
    const int laid = driver_laid(results, "the forms program", &loaded, outputs, DRIVER_PLANES + 1u);
    sim_check(results, laid, "the forms program lays each plane at its own 16 bits");
    const unsigned int out_limbs = loaded.layout.out_limbs;
    const size_t lane_bytes = sizeof(unsigned short);
    const size_t record_bytes = (size_t)out_limbs * sizeof(unsigned int);
    // the bases, and a 0 past them for a row with none; each value's index pair, its own record and its row's base
    const unsigned long long base_records = (bases != NULL) ? (bases->values + 1ull) : 0ull;
    unsigned int *index = NULL;
    unsigned int *device_index = NULL;
    unsigned int *device_bases = NULL;
    unsigned int *device_values = NULL;
    unsigned int *device_out = NULL;
    unsigned short *device_planes = NULL;
    int ok = laid && (cudaMalloc((void **)&device_values, (size_t)(count * 8ull)) == cudaSuccess) &&
             (cudaMemcpy(device_values, values->bytes, (size_t)(count * 8ull), cudaMemcpyHostToDevice) == cudaSuccess) &&
             (cudaMalloc((void **)&device_out, (size_t)count * record_bytes) == cudaSuccess) &&
             (cudaMalloc((void **)&device_planes, (size_t)(count * (DRIVER_PLANES + 1u)) * lane_bytes) == cudaSuccess);
    if (ok && (column == DRIVER_INTENSITIES))
    {
        index = (unsigned int *)malloc((size_t)(count * 2ull) * sizeof(unsigned int));
        unsigned char *const base_bytes = (unsigned char *)calloc((size_t)(base_records * 8ull), 1u);
        ok = (index != NULL) && (base_bytes != NULL) && (bases->rows == values->rows);
        if (ok)
        {
            memcpy(base_bytes, bases->bytes, (size_t)(bases->values * 8ull));
            for (unsigned long long row = 0ull; row < values->rows; row += 1ull)
            {
                const int present = bases->row_start[row + 1ull] > bases->row_start[row];
                const unsigned int base_record = (unsigned int)(present ? bases->row_start[row] : bases->values);
                for (unsigned long long value = values->row_start[row]; value < values->row_start[row + 1ull];
                     value += 1ull)
                {
                    index[value * 2ull] = (unsigned int)value;
                    index[(value * 2ull) + 1ull] = base_record;
                }
            }
        }
        ok = ok && (cudaMalloc((void **)&device_bases, (size_t)(base_records * 8ull)) == cudaSuccess) &&
             (cudaMemcpy(device_bases, base_bytes, (size_t)(base_records * 8ull), cudaMemcpyHostToDevice) ==
              cudaSuccess) &&
             (cudaMalloc((void **)&device_index, (size_t)(count * 2ull) * sizeof(unsigned int)) == cudaSuccess) &&
             (cudaMemcpy(device_index, index, (size_t)(count * 2ull) * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
              cudaSuccess);
        free(base_bytes);
    }
    memset(&error, 0, sizeof(error));
    const unsigned long long run_start = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordRunRequest run = {loaded.record,
                                           {device_values, device_bases, NULL},
                                           {count, base_records, 0ull},
                                           device_index,
                                           count,
                                           device_out,
                                           &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
        if (!ok)
        {
            driver_error_line(results, "the forms program's run errored", &error);
        }
    }
    const unsigned long long run_end = engine_clock_microseconds();
    // plane j of every record, gathered beside the others: one strided copy on the device a plane
    for (unsigned int plane = 0u; ok && (plane <= DRIVER_PLANES); plane += 1u)
    {
        ok = cudaMemcpy2D(device_planes + (plane * count), lane_bytes,
                          (const unsigned char *)device_out + (plane * lane_bytes), record_bytes, lane_bytes,
                          (size_t)count, cudaMemcpyDeviceToDevice) == cudaSuccess;
    }
    cudaFree(device_out);
    cudaFree(device_bases);
    cudaFree(device_index);
    free(index);
    // the form plane comes back to name the kept values, and each plane's highest lane to name the planes that hold
    unsigned short *const forms = (unsigned short *)malloc((size_t)count * lane_bytes + 2u);
    ok = ok && (forms != NULL) &&
         (cudaMemcpy(forms, device_planes + (DRIVER_PLANES * count), (size_t)count * lane_bytes, cudaMemcpyDeviceToHost) ==
          cudaSuccess);
    unsigned long long form_count[DRIVER_FORM_KEPT + 1u];
    memset(form_count, 0, sizeof(form_count));
    for (unsigned long long value = 0ull; ok && (value < count); value += 1ull)
    {
        form_count[(forms[value] <= DRIVER_FORM_KEPT) ? forms[value] : DRIVER_FORM_KEPT] += 1ull;
    }
    for (unsigned int form = 0u; form <= DRIVER_FORM_KEPT; form += 1u)
    {
        tally->forms[column][form] += form_count[form];
    }
    const unsigned int first_form = (column == DRIVER_INTENSITIES) ? DRIVER_FORM_COUNT : DRIVER_FORM_DECIMAL;
    char sample[DRIVER_SAMPLE_BYTES];
    const unsigned long long line[4] = {1ull, 1ull, 1ull, count};
    for (unsigned int plane = 0u; ok && (plane < DRIVER_PLANES); plane += 1u)
    {
        // a plane past the integers' width holds only zeros and is not sealed
        unsigned short *const lanes = (unsigned short *)malloc((size_t)count * lane_bytes + 2u);
        int holds = 0;
        ok = (lanes != NULL) && (cudaMemcpy(lanes, device_planes + (plane * count), (size_t)count * lane_bytes,
                                            cudaMemcpyDeviceToHost) == cudaSuccess);
        for (unsigned long long value = 0ull; ok && !holds && (value < count); value += 1ull)
        {
            holds = lanes[value] != 0u;
        }
        free(lanes);
        char part[16];
        snprintf(part, sizeof(part), "unit%u", plane);
        ok = ok && (!holds || (driver_sample_name(sample, group, column, part) &&
                               driver_seal(results, set, sample, device_planes + (plane * count), line, column, tally)));
        sealed->planes |= (ok && holds) ? (1u << plane) : 0u;
    }
    if (ok && (form_count[first_form] != count))
    {
        ok = driver_sample_name(sample, group, column, "form") &&
             driver_seal(results, set, sample, device_planes + (DRIVER_PLANES * count), line, column, tally);
        sealed->form = ok;
    }
    sealed->kept = form_count[DRIVER_FORM_KEPT];
    const unsigned long long kept = form_count[DRIVER_FORM_KEPT];
    if (ok && (kept != 0ull))
    {
        // the kept values' records, named by an index, written as their stored bits' planes and gathered
        unsigned int *const kept_index = (unsigned int *)malloc((size_t)kept * sizeof(unsigned int));
        unsigned long long at = 0ull;
        for (unsigned long long value = 0ull; (kept_index != NULL) && (value < count); value += 1ull)
        {
            if (forms[value] == DRIVER_FORM_KEPT)
            {
                kept_index[at] = (unsigned int)value;
                at += 1ull;
            }
        }
        ExactRecordProgram copy;
        exact_record_open(&copy);
        unsigned int copy_outputs[DRIVER_KEPT_PLANES];
        driver_kept_program(&copy, copy_outputs);
        DriverLoaded copied;
        unsigned int *device_kept_index = NULL;
        unsigned int *device_kept = NULL;
        unsigned short *device_kept_planes = NULL;
        const int copy_loaded =
            (kept_index != NULL) && driver_load(&copy, copy_outputs, DRIVER_KEPT_PLANES, doubles, 1u, &copied, &error);
        const int copy_laid =
            copy_loaded && driver_laid(results, "the kept values' program", &copied, copy_outputs, DRIVER_KEPT_PLANES);
        sim_check(results, copy_laid, "the kept values' program lays each plane at its own 16 bits");
        const size_t kept_record_bytes = copy_loaded ? ((size_t)copied.layout.out_limbs * sizeof(unsigned int)) : 0u;
        ok = copy_laid && (cudaMalloc((void **)&device_kept_index, (size_t)kept * sizeof(unsigned int)) == cudaSuccess) &&
             (cudaMemcpy(device_kept_index, kept_index, (size_t)kept * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
              cudaSuccess) &&
             (cudaMalloc((void **)&device_kept, (size_t)kept * kept_record_bytes) == cudaSuccess) &&
             (cudaMalloc((void **)&device_kept_planes, (size_t)(kept * DRIVER_KEPT_PLANES) * lane_bytes) == cudaSuccess);
        if (ok)
        {
            const CycleRecordRunRequest run = {copied.record, {device_values, NULL, NULL}, {count, 0ull, 0ull},
                                               device_kept_index, kept, device_kept, &error};
            ok = cycle_record_run(&run) != CYCLE_ERROR;
        }
        for (unsigned long long plane = 0ull; ok && (plane < DRIVER_KEPT_PLANES); plane += 1ull)
        {
            ok = cudaMemcpy2D(device_kept_planes + (plane * kept), lane_bytes,
                              (const unsigned char *)device_kept + (plane * lane_bytes), kept_record_bytes, lane_bytes,
                              (size_t)kept, cudaMemcpyDeviceToDevice) == cudaSuccess;
        }
        const unsigned long long kept_extent[4] = {1ull, 1ull, DRIVER_KEPT_PLANES, kept};
        ok = ok && driver_sample_name(sample, group, column, "kept") &&
             driver_seal(results, set, sample, device_kept_planes, kept_extent, column, tally);
        if (copy_loaded)
        {
            driver_release(&copied);
        }
        exact_record_close(&copy);
        cudaFree(device_kept_index);
        cudaFree(device_kept);
        cudaFree(device_kept_planes);
        free(kept_index);
        sim_check(results, ok, "the kept values' lanes are gathered and sealed");
    }
    tally->raw_bytes[column] += count * 8ull;
    scriptura_text(&results->line, "   ");
    scriptura_text(&results->line, DRIVER_COLUMN_PATH[column]);
    driver_line_decimal(results, ": ", count);
    driver_line_decimal(results, " values, forms program ", program.count);
    driver_line_decimal(results, " steps, device ", run_end - run_start);
    scriptura_text(&results->line, " us; forms");
    for (unsigned int form = 0u; form <= DRIVER_FORM_KEPT; form += 1u)
    {
        driver_line_decimal(results, " ", form_count[form]);
    }
    driver_line_end(results);
    free(forms);
    cudaFree(device_values);
    cudaFree(device_planes);
    driver_release(&loaded);
    exact_record_close(&program);
    return ok;
}

// The check program: each value's stored double, member 0, against what the set's crystals hold for it, member 1, its
// planes and its form one lane each, and for the intensities its row's base, member 2. The integer u the planes make
// is held against the double's preimage in the value's form: u / 10^places a decimal, u / B a count, u / 10^places a
// decimal the double is that over a divisor. A kept value reads 1 here and is held against its lanes apart
static void driver_check_program(ExactRecordProgram *program, unsigned int column, unsigned long long most_placed,
                                 unsigned int shift_bits, unsigned int *output)
{
    const unsigned int places = DRIVER_UNIT_PLACES[column];
    FormsFields fields;
    forms_fields(program, &fields);
    FormsDouble value;
    forms_double(program, &fields, 0u, &value);
    FormsPreimage preimage;
    forms_preimage(program, &value, most_placed, shift_bits, &preimage);
    const unsigned int plane_size = exact_record_power_two(program, DRIVER_PLANE_BITS);
    unsigned int unit = exact_record_constant(program, 0ull);
    unsigned int weight = exact_record_constant(program, 1ull);
    for (unsigned int plane = 0u; plane < DRIVER_PLANES; plane += 1u)
    {
        const unsigned int field = exact_record_member_field(program, DRIVER_PLANE_BITS, plane * DRIVER_LANE_BITS);
        unit = exact_record_sum(program, unit,
                                exact_record_product(program, exact_record_read_unsigned(program, field, 1u), weight));
        weight = exact_record_product(program, weight, plane_size);
    }
    const unsigned int form_field = exact_record_member_field(program, DRIVER_PLANE_BITS, DRIVER_PLANES * DRIVER_LANE_BITS);
    const unsigned int form = exact_record_read_unsigned(program, form_field, 1u);
    const unsigned int ten = exact_record_constant(program, forms_ten(places));
    unsigned int verdict = exact_record_product(
        program, exact_record_equal(program, form, exact_record_constant(program, DRIVER_FORM_DECIMAL)),
        forms_contains(program, &preimage, ten, unit));
    verdict = exact_record_sum(
        program, verdict, exact_record_equal(program, form, exact_record_constant(program, DRIVER_FORM_KEPT)));
    if (column == DRIVER_INTENSITIES)
    {
        FormsDouble base;
        forms_double(program, &fields, 2u, &base);
        unsigned int whole_base = 0u;
        unsigned int whole = 0u;
        forms_whole(program, &base, &whole_base, &whole);
        verdict = exact_record_sum(
            program, verdict,
            exact_record_product(
                program, exact_record_equal(program, form, exact_record_constant(program, DRIVER_FORM_COUNT)),
                exact_record_product(program, whole, forms_contains(program, &preimage, whole_base, unit))));
        for (unsigned int divisor = 0u; divisor < DRIVER_DIVISORS; divisor += 1u)
        {
            FormsPreimage divided;
            unsigned int exists = 0u;
            forms_divided(program, &preimage, DRIVER_DIVISOR[divisor], &divided, &exists);
            verdict = exact_record_sum(
                program, verdict,
                exact_record_product(
                    program,
                    exact_record_equal(program, form, exact_record_constant(program, DRIVER_FORM_DIVIDED + divisor)),
                    exact_record_product(program, exists, forms_contains(program, &divided, ten, unit))));
        }
    }
    *output = driver_lane(program, verdict);
}

// One column of one row group read back from the set: its crystals loaded, each value's planes and form laid beside
// each other into a record of its own on the device by strided copies, and the check program run over every value
// against its stored double, the verdicts summed on the device. The kept values' lanes are rebuilt from their crystal
// and held against the stored bytes of the values whose form is kept
static int driver_column_check(SimResults *results, const char *set, unsigned int group, unsigned int column,
                               const DriverColumn *values, const DriverColumn *bases, const DriverSealed *sealed)
{
    const unsigned long long count = values->values;
    if (count == 0ull)
    {
        return 1;
    }
    const size_t lane_bytes = sizeof(unsigned short);
    const unsigned int part_limbs = (((DRIVER_PLANES + 1u) * DRIVER_LANE_BITS) + DRIVER_WORD_BITS - 1u) / DRIVER_WORD_BITS;
    const size_t part_bytes = (size_t)part_limbs * sizeof(unsigned int);
    const unsigned int first_form = (column == DRIVER_INTENSITIES) ? DRIVER_FORM_COUNT : DRIVER_FORM_DECIMAL;
    char sample[DRIVER_SAMPLE_BYTES];
    // the parts as planes: each loaded crystal's lanes, 0 for a plane not sealed and the first form for a form plane
    unsigned short *const parts = (unsigned short *)calloc((size_t)(count * (DRIVER_PLANES + 1u)), lane_bytes);
    int ok = parts != NULL;
    for (unsigned long long value = 0ull; ok && !sealed->form && (value < count); value += 1ull)
    {
        parts[(DRIVER_PLANES * count) + value] = (unsigned short)first_form;
    }
    const unsigned long long started = engine_clock_microseconds();
    for (unsigned int plane = 0u; ok && (plane <= DRIVER_PLANES); plane += 1u)
    {
        const int held = (plane < DRIVER_PLANES) ? ((sealed->planes >> plane) & 1u) : sealed->form;
        if (!held)
        {
            continue;
        }
        char part[16];
        if (plane < DRIVER_PLANES)
        {
            snprintf(part, sizeof(part), "unit%u", plane);
        }
        else
        {
            snprintf(part, sizeof(part), "form");
        }
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        unsigned short *volume = NULL;
        EngineSignum root;
        EngineError error;
        memset(&error, 0, sizeof(error));
        ok = driver_sample_name(sample, group, column, part) &&
             (engine_iapx_load(set, sample, extent, &volume, &root, NULL, &error) == 0L) && (extent[3] == count);
        if (ok)
        {
            memcpy(parts + (plane * count), volume, (size_t)count * lane_bytes);
        }
        else
        {
            driver_error_line(results, "a crystal did not load", &error);
        }
        free(volume);
    }
    const unsigned long long loaded_at = engine_clock_microseconds();
    // the kept values, from their crystal's five planes, against the stored bytes of the values kept
    unsigned long long kept_apart = 0ull;
    if (ok && (sealed->kept != 0ull))
    {
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        unsigned short *volume = NULL;
        EngineSignum root;
        EngineError error;
        memset(&error, 0, sizeof(error));
        ok = driver_sample_name(sample, group, column, "kept") &&
             (engine_iapx_load(set, sample, extent, &volume, &root, NULL, &error) == 0L) &&
             (extent[2] == DRIVER_KEPT_PLANES) && (extent[3] == sealed->kept);
        unsigned long long at = 0ull;
        for (unsigned long long value = 0ull; ok && (value < count); value += 1ull)
        {
            if (parts[(DRIVER_PLANES * count) + value] != DRIVER_FORM_KEPT)
            {
                continue;
            }
            unsigned long long stored = 0ull;
            for (unsigned int plane = DRIVER_KEPT_PLANES; plane > 0u; plane -= 1u)
            {
                stored = (stored << DRIVER_PLANE_BITS) | volume[((plane - 1u) * sealed->kept) + at];
            }
            kept_apart += (memcmp(&stored, values->bytes + (value * 8ull), 8u) == 0) ? 0ull : 1ull;
            at += 1ull;
        }
        ok = ok && (at == sealed->kept);
        free(volume);
    }
    // each value's parts laid into its record on the device
    unsigned short *device_parts = NULL;
    unsigned int *device_records = NULL;
    unsigned int *device_values = NULL;
    unsigned int *device_bases = NULL;
    unsigned int *device_index = NULL;
    unsigned int *device_out = NULL;
    ok = ok && (cudaMalloc((void **)&device_parts, (size_t)(count * (DRIVER_PLANES + 1u)) * lane_bytes) == cudaSuccess) &&
         (cudaMemcpy(device_parts, parts, (size_t)(count * (DRIVER_PLANES + 1u)) * lane_bytes, cudaMemcpyHostToDevice) ==
          cudaSuccess) &&
         (cudaMalloc((void **)&device_records, (size_t)count * part_bytes) == cudaSuccess) &&
         (cudaMemset(device_records, 0, (size_t)count * part_bytes) == cudaSuccess);
    for (unsigned int plane = 0u; ok && (plane <= DRIVER_PLANES); plane += 1u)
    {
        ok = cudaMemcpy2D((unsigned char *)device_records + (plane * lane_bytes), part_bytes,
                          device_parts + (plane * count), lane_bytes, lane_bytes, (size_t)count,
                          cudaMemcpyDeviceToDevice) == cudaSuccess;
    }
    cudaFree(device_parts);
    free(parts);
    unsigned long long least_placed = 0ull;
    unsigned long long most_placed = 0ull;
    driver_exponents(values, &least_placed, &most_placed);
    const unsigned int shift_bits = exact_record_bits_of(FORMS_LIFT - least_placed) + 1u;
    ExactRecordProgram program;
    exact_record_open(&program);
    unsigned int output = 0u;
    driver_check_program(&program, column, most_placed, shift_bits, &output);
    const unsigned int members = (column == DRIVER_INTENSITIES) ? 3u : 2u;
    const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {DRIVER_LIMBS_PER_VALUE, part_limbs, DRIVER_LIMBS_PER_VALUE};
    DriverLoaded loaded;
    EngineError error;
    const int program_loaded = ok && driver_load(&program, &output, 1u, limbs, members, &loaded, &error);
    ok = program_loaded;
    if (!ok)
    {
        driver_error_line(results, "the check program did not load", &error);
    }
    // for the intensities, each value's index triple: its own record, its own parts and its row's base
    const unsigned long long base_records = (bases != NULL) ? (bases->values + 1ull) : 0ull;
    if (ok && (column == DRIVER_INTENSITIES))
    {
        unsigned int *const index = (unsigned int *)malloc((size_t)(count * 3ull) * sizeof(unsigned int));
        unsigned char *const base_bytes = (unsigned char *)calloc((size_t)(base_records * 8ull), 1u);
        ok = (index != NULL) && (base_bytes != NULL) && (bases->rows == values->rows);
        if (ok)
        {
            memcpy(base_bytes, bases->bytes, (size_t)(bases->values * 8ull));
            for (unsigned long long row = 0ull; row < values->rows; row += 1ull)
            {
                const int present = bases->row_start[row + 1ull] > bases->row_start[row];
                const unsigned int base_record = (unsigned int)(present ? bases->row_start[row] : bases->values);
                for (unsigned long long value = values->row_start[row]; value < values->row_start[row + 1ull];
                     value += 1ull)
                {
                    index[value * 3ull] = (unsigned int)value;
                    index[(value * 3ull) + 1ull] = (unsigned int)value;
                    index[(value * 3ull) + 2ull] = base_record;
                }
            }
        }
        ok = ok && (cudaMalloc((void **)&device_bases, (size_t)(base_records * 8ull)) == cudaSuccess) &&
             (cudaMemcpy(device_bases, base_bytes, (size_t)(base_records * 8ull), cudaMemcpyHostToDevice) ==
              cudaSuccess) &&
             (cudaMalloc((void **)&device_index, (size_t)(count * 3ull) * sizeof(unsigned int)) == cudaSuccess) &&
             (cudaMemcpy(device_index, index, (size_t)(count * 3ull) * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
              cudaSuccess);
        free(index);
        free(base_bytes);
    }
    const unsigned int out_limbs = program_loaded ? loaded.layout.out_limbs : 0u;
    ok = ok && (cudaMalloc((void **)&device_values, (size_t)(count * 8ull)) == cudaSuccess) &&
         (cudaMemcpy(device_values, values->bytes, (size_t)(count * 8ull), cudaMemcpyHostToDevice) == cudaSuccess) &&
         (cudaMalloc((void **)&device_out, (size_t)count * out_limbs * sizeof(unsigned int)) == cudaSuccess);
    memset(&error, 0, sizeof(error));
    if (ok)
    {
        const CycleRecordRunRequest run = {loaded.record,
                                           {device_values, device_records, device_bases},
                                           {count, count, base_records},
                                           device_index,
                                           count,
                                           device_out,
                                           &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
        if (!ok)
        {
            driver_error_line(results, "the check program's run errored", &error);
        }
    }
    // the verdicts summed on the device: every value whose parts read back to its double counts 1
    unsigned int sum[2] = {0u, 0u};
    if (ok)
    {
        const DeviceRecordStep place = loaded.layout.step_table[output];
        const CycleRecordSumRequest total = {device_out,    count, count, out_limbs, place.out_offset, place.out_bits,
                                             2u,            sum,   &error};
        ok = cycle_record_sum(&total) != CYCLE_ERROR;
        if (!ok)
        {
            driver_error_line(results, "the verdicts' sum errored", &error);
        }
    }
    const unsigned long long checked_at = engine_clock_microseconds();
    const unsigned long long read_back = ((unsigned long long)sum[1] << 32u) | sum[0];
    cudaFree(device_records);
    cudaFree(device_values);
    cudaFree(device_bases);
    cudaFree(device_index);
    cudaFree(device_out);
    if (program_loaded)
    {
        driver_release(&loaded);
    }
    exact_record_close(&program);
    scriptura_text(&results->line, "   ");
    scriptura_text(&results->line, DRIVER_COLUMN_PATH[column]);
    driver_line_decimal(results, " read back: ", read_back);
    driver_line_decimal(results, " of ", count);
    driver_line_decimal(results, " values hold their stored double; kept lanes apart ", kept_apart);
    driver_line_decimal(results, "; loaded in ", loaded_at - started);
    driver_line_decimal(results, " us, checked in ", checked_at - loaded_at);
    scriptura_text(&results->line, " us");
    driver_line_end(results);
    return ok && (read_back == count) && (kept_apart == 0ull);
}

// A text column of one row group: its distinct strings, their bytes one after another from start[d] up to
// start[d + 1], and each row's string as its index among them, DRIVER_TEXT_NONE for a row with none
typedef struct
{
    unsigned long long rows;
    unsigned long long distinct;
    unsigned long long raw_bytes;
    unsigned char *bytes;
    unsigned long long *start;
    unsigned int *index;
} DriverText;

#define DRIVER_TEXT_NONE 0xFFFFFFFFu

static void driver_text_release(DriverText *text)
{
    free(text->bytes);
    free(text->start);
    free(text->index);
    memset(text, 0, sizeof(*text));
}

// FNV-1a over a string's bytes, the table's place for it
static unsigned long long driver_text_hash(const unsigned char *bytes, unsigned long long length)
{
    unsigned long long hash = 0xCBF29CE484222325ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        hash = (hash ^ bytes[at]) * 0x100000001B3ull;
    }
    return hash;
}

// One row group's text column read as its distinct strings and each row's index among them; 0 where the file has no
// such column or it did not read
static int driver_text_read(const char *path, unsigned int group, const char *leaf_path, DriverText *out)
{
    memset(out, 0, sizeof(*out));
    const CasmiParquetLeafFind find = {&s_footer, leaf_path};
    const long long leaf = casmi_parquet_leaf_find(&find);
    if ((leaf < 0ll) || (s_footer.leaf[leaf].physical != CASMI_PARQUET_BYTE_ARRAY))
    {
        return 0;
    }
    CasmiParquetColumn column;
    memset(&column, 0, sizeof(column));
    // the leaf is below CASMI_PARQUET_LEAVES_MOST
    const CasmiParquetColumnRead read = {path, &s_footer, group, (unsigned int)leaf, &column};
    int ok = casmi_parquet_column_bound(&read) >= 0ll;
    column.row_start = ok ? (unsigned long long *)malloc((size_t)(column.row_start_room * 8ull) + 8u) : NULL;
    column.value_bytes = ok ? (unsigned char *)malloc((size_t)column.value_bytes_room + 8u) : NULL;
    column.value_offset = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
    column.value_length = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
    column.scratch = ok ? (unsigned char *)malloc((size_t)column.scratch_room + 8u) : NULL;
    ok = ok && (column.row_start != NULL) && (column.value_bytes != NULL) && (column.value_offset != NULL) &&
         (column.value_length != NULL) && (column.scratch != NULL) && (casmi_parquet_column_read(&read) >= 0ll);
    if (ok)
    {
        column.row_start[column.rows] = column.values;
    }
    // the distinct strings, by an open table twice the values' count, a power of two
    unsigned long long slots = 2ull;
    while (ok && (slots < (2ull * column.values) + 2ull))
    {
        slots *= 2ull;
    }
    unsigned int *const table = ok ? (unsigned int *)malloc((size_t)slots * sizeof(unsigned int)) : NULL;
    unsigned long long *const first = ok ? (unsigned long long *)malloc((size_t)(column.values + 1ull) * 8u) : NULL;
    out->index = ok ? (unsigned int *)malloc((size_t)(column.rows + 1ull) * sizeof(unsigned int)) : NULL;
    out->start = ok ? (unsigned long long *)malloc((size_t)(column.values + 2ull) * 8u) : NULL;
    ok = ok && (table != NULL) && (first != NULL) && (out->index != NULL) && (out->start != NULL) &&
         (column.values < DRIVER_TEXT_NONE);
    if (ok)
    {
        memset(table, 0xFF, (size_t)slots * sizeof(unsigned int));
    }
    unsigned long long distinct_bytes = 0ull;
    for (unsigned long long row = 0ull; ok && (row < column.rows); row += 1ull)
    {
        out->index[row] = DRIVER_TEXT_NONE;
        if (column.row_start[row + 1ull] == column.row_start[row])
        {
            continue;
        }
        const unsigned long long value = column.row_start[row];
        const unsigned char *const bytes = column.value_bytes + column.value_offset[value];
        const unsigned long long length = column.value_length[value];
        out->raw_bytes += length;
        unsigned long long slot = driver_text_hash(bytes, length) & (slots - 1ull);
        while (table[slot] != DRIVER_TEXT_NONE)
        {
            const unsigned long long held = first[table[slot]];
            if ((column.value_length[held] == length) &&
                (memcmp(column.value_bytes + column.value_offset[held], bytes, (size_t)length) == 0))
            {
                break;
            }
            slot = (slot + 1ull) & (slots - 1ull);
        }
        if (table[slot] == DRIVER_TEXT_NONE)
        {
            table[slot] = (unsigned int)out->distinct;
            first[out->distinct] = value;
            distinct_bytes += length;
            out->distinct += 1ull;
        }
        out->index[row] = table[slot];
    }
    // the distinct strings' bytes, in the order they first appear
    out->bytes = ok ? (unsigned char *)malloc((size_t)distinct_bytes + 2u) : NULL;
    ok = ok && (out->bytes != NULL);
    unsigned long long at = 0ull;
    for (unsigned long long each = 0ull; ok && (each < out->distinct); each += 1ull)
    {
        out->start[each] = at;
        const unsigned long long value = first[each];
        memcpy(out->bytes + at, column.value_bytes + column.value_offset[value], (size_t)column.value_length[value]);
        at += column.value_length[value];
    }
    if (ok)
    {
        out->start[out->distinct] = at;
        out->rows = column.rows;
    }
    free(table);
    free(first);
    free(column.row_start);
    free(column.value_bytes);
    free(column.value_offset);
    free(column.value_length);
    free(column.scratch);
    if (!ok)
    {
        driver_text_release(out);
    }
    return ok;
}

// host lanes sealed as a crystal of the set through the device
static int driver_seal_host(SimResults *results, const char *set, const char *sample, const unsigned short *lanes,
                            const unsigned long long extent[4], unsigned int column, DriverTally *tally)
{
    const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
    unsigned short *device_lanes = NULL;
    const int ok = (cudaMalloc((void **)&device_lanes, (size_t)count * sizeof(unsigned short)) == cudaSuccess) &&
                   (cudaMemcpy(device_lanes, lanes, (size_t)count * sizeof(unsigned short), cudaMemcpyHostToDevice) ==
                    cudaSuccess) &&
                   driver_seal(results, set, sample, device_lanes, extent, column, tally);
    cudaFree(device_lanes);
    return ok;
}

// One text column of one row group sealed: its distinct strings' bytes, two to a lane, their lengths, one lane each,
// and each row's index, one past it and 0 for none, as two planes of 16 bits
static int driver_text_seal(SimResults *results, const char *set, unsigned int group, unsigned int text,
                            const DriverText *read, DriverTally *tally)
{
    const unsigned int column = DRIVER_COLUMNS + text;
    const unsigned long long byte_count = read->start[read->distinct];
    const unsigned long long lanes = (byte_count + 1ull) / 2ull;
    unsigned short *const bytes = (unsigned short *)calloc((size_t)lanes + 1u, sizeof(unsigned short));
    unsigned short *const lengths = (unsigned short *)malloc((size_t)read->distinct * sizeof(unsigned short) + 2u);
    unsigned short *const index = (unsigned short *)malloc((size_t)(read->rows * 2ull) * sizeof(unsigned short) + 2u);
    int ok = (bytes != NULL) && (lengths != NULL) && (index != NULL) && (read->distinct != 0ull);
    if (ok)
    {
        memcpy(bytes, read->bytes, (size_t)byte_count);
    }
    for (unsigned long long each = 0ull; ok && (each < read->distinct); each += 1ull)
    {
        const unsigned long long length = read->start[each + 1ull] - read->start[each];
        ok = length < (1ull << DRIVER_LANE_BITS);
        lengths[each] = (unsigned short)length;
    }
    for (unsigned long long row = 0ull; ok && (row < read->rows); row += 1ull)
    {
        const unsigned long long named = (read->index[row] == DRIVER_TEXT_NONE) ? 0ull : (read->index[row] + 1ull);
        index[row] = (unsigned short)(named & 0xFFFFull);
        index[read->rows + row] = (unsigned short)(named >> DRIVER_LANE_BITS);
    }
    char sample[DRIVER_SAMPLE_BYTES];
    const unsigned long long byte_extent[4] = {1ull, 1ull, 1ull, (lanes != 0ull) ? lanes : 1ull};
    const unsigned long long length_extent[4] = {1ull, 1ull, 1ull, read->distinct};
    const unsigned long long index_planes = ((read->distinct + 1ull) >> DRIVER_LANE_BITS) != 0ull ? 2ull : 1ull;
    const unsigned long long index_extent[4] = {1ull, 1ull, index_planes, read->rows};
    ok = ok && driver_sample_name(sample, group, column, "text") &&
         driver_seal_host(results, set, sample, bytes, byte_extent, column, tally) &&
         driver_sample_name(sample, group, column, "lengths") &&
         driver_seal_host(results, set, sample, lengths, length_extent, column, tally) &&
         driver_sample_name(sample, group, column, "index") &&
         driver_seal_host(results, set, sample, index, index_extent, column, tally);
    tally->raw_bytes[column] += read->raw_bytes;
    tally->forms[column][0] += read->distinct;
    tally->forms[column][1] += read->rows;
    free(bytes);
    free(lengths);
    free(index);
    return ok;
}

// each row's value count as one plane of 16-bit lanes, sealed where every count fits one
static int driver_rows_seal(SimResults *results, const char *set, unsigned int group, unsigned int column,
                            const DriverColumn *values, DriverTally *tally)
{
    unsigned short *const counts = (unsigned short *)malloc((size_t)values->rows * sizeof(unsigned short) + 2u);
    int ok = counts != NULL;
    for (unsigned long long row = 0ull; ok && (row < values->rows); row += 1ull)
    {
        const unsigned long long held = values->row_start[row + 1ull] - values->row_start[row];
        ok = held < (1ull << DRIVER_LANE_BITS);
        counts[row] = (unsigned short)held;
    }
    unsigned short *device_counts = NULL;
    ok = ok && (cudaMalloc((void **)&device_counts, (size_t)values->rows * sizeof(unsigned short)) == cudaSuccess) &&
         (cudaMemcpy(device_counts, counts, (size_t)values->rows * sizeof(unsigned short), cudaMemcpyHostToDevice) ==
          cudaSuccess);
    char sample[DRIVER_SAMPLE_BYTES];
    const unsigned long long line[4] = {1ull, 1ull, 1ull, values->rows};
    ok = ok && driver_sample_name(sample, group, column, "rows") &&
         driver_seal(results, set, sample, device_counts, line, column, tally);
    cudaFree(device_counts);
    free(counts);
    sim_check(results, ok, "each row's value count seals");
    return ok;
}

// the device bytes the widest row group holds: its values, records, planes and bases, and the tower's and the coder's
// pools for the widest plane
static unsigned long long driver_declared(void)
{
    unsigned long long widest = 0ull;
    for (unsigned int group = 0u; group < s_footer.row_group_count; group += 1u)
    {
        for (unsigned int leaf = 0u; leaf < s_footer.leaf_count; leaf += 1u)
        {
            const unsigned long long levels = s_footer.row_group[group].chunk[leaf].level_count;
            widest = (levels > widest) ? levels : widest;
        }
    }
    const unsigned long long lanes = widest * DRIVER_LANES_PER_VALUE;
    return (widest * (8ull + 16ull + (2ull * (DRIVER_PLANES + 1u)) + 8ull + 8ull)) + tower_reserve_bytes(lanes) +
           compression_reserve_bytes(lanes);
}

static int driver_ingest(SimResults *results, int count, char **arguments)
{
    const char *const path = arguments[2];
    const char *const set = arguments[3];
    const CasmiParquetFooterRead footer = {path, &s_footer};
    if (casmi_parquet_footer_read(&footer) < 0ll)
    {
        scriptura_text(&results->line, "  the parquet footer did not read");
        driver_line_end(results);
        return 0;
    }
    if (sim_job_submit(results, "casmi_driver", count, arguments, driver_declared()) == 0)
    {
        return 0;
    }
    DriverTally tally;
    memset(&tally, 0, sizeof(tally));
    const unsigned long long started = engine_clock_microseconds();
    int ok = 1;
    for (unsigned int group = 0u; ok && (group < s_footer.row_group_count); group += 1u)
    {
        driver_line_decimal(results, "  row group ", group);
        driver_line_end(results);
        DriverColumn columns[DRIVER_COLUMNS];
        memset(columns, 0, sizeof(columns));
        for (unsigned int column = 0u; ok && (column < DRIVER_COLUMNS); column += 1u)
        {
            ok = driver_column_read(path, group, DRIVER_COLUMN_PATH[column], &columns[column]);
            sim_check(results, ok, "the column reads as its stored bytes");
        }
        for (unsigned int column = 0u; ok && (column < DRIVER_BASES); column += 1u)
        {
            const DriverColumn *const bases = (column == DRIVER_INTENSITIES) ? &columns[DRIVER_BASES] : NULL;
            DriverSealed sealed;
            ok = driver_column_forms(results, set, group, column, &columns[column], bases, &tally, &sealed);
            sim_check(results, ok, "the column's forms are read on the device and sealed");
            const int read_back = ok && driver_column_check(results, set, group, column, &columns[column], bases, &sealed);
            sim_check(results, read_back, "every value read back from the set's crystals is its stored double");
            ok = ok && read_back;
        }
        // the bases as they are stored, a value's lanes as four planes, and which rows hold one
        if (ok && (columns[DRIVER_BASES].values != 0ull))
        {
            const DriverColumn *const bases = &columns[DRIVER_BASES];
            unsigned short *device_lanes = NULL;
            unsigned short *device_planes = NULL;
            const size_t lane_bytes = sizeof(unsigned short);
            ok = (cudaMalloc((void **)&device_lanes, (size_t)(bases->values * 8ull)) == cudaSuccess) &&
                 (cudaMalloc((void **)&device_planes, (size_t)(bases->values * 8ull)) == cudaSuccess) &&
                 (cudaMemcpy(device_lanes, bases->bytes, (size_t)(bases->values * 8ull), cudaMemcpyHostToDevice) ==
                  cudaSuccess);
            for (unsigned long long plane = 0ull; ok && (plane < DRIVER_LANES_PER_VALUE); plane += 1ull)
            {
                ok = cudaMemcpy2D(device_planes + (plane * bases->values), lane_bytes, device_lanes + plane,
                                  (size_t)DRIVER_LANES_PER_VALUE * lane_bytes, lane_bytes, (size_t)bases->values,
                                  cudaMemcpyDeviceToDevice) == cudaSuccess;
            }
            char sample[DRIVER_SAMPLE_BYTES];
            const unsigned long long extent[4] = {1ull, 1ull, DRIVER_LANES_PER_VALUE, bases->values};
            ok = ok && driver_sample_name(sample, group, DRIVER_BASES, "kept") &&
                 driver_seal(results, set, sample, device_planes, extent, DRIVER_BASES, &tally) &&
                 driver_rows_seal(results, set, group, DRIVER_BASES, bases, &tally);
            tally.raw_bytes[DRIVER_BASES] += bases->values * 8ull;
            cudaFree(device_lanes);
            cudaFree(device_planes);
        }
        ok = ok && driver_rows_seal(results, set, group, DRIVER_PEAKS, &columns[DRIVER_PEAKS], &tally);
        for (unsigned int each = 0u; ok && (each < DRIVER_TEXTS); each += 1u)
        {
            DriverText read;
            if (!driver_text_read(path, group, DRIVER_TEXT_PATH[each], &read))
            {
                continue;
            }
            ok = driver_text_seal(results, set, group, each, &read, &tally);
            sim_check(results, ok, "the text column seals as its distinct strings and each row's index");
            driver_text_release(&read);
        }
        for (unsigned int column = 0u; column < DRIVER_COLUMNS; column += 1u)
        {
            driver_column_release(&columns[column]);
        }
    }
    const unsigned long long finished = engine_clock_microseconds();
    unsigned long long crystal_total = 0ull;
    unsigned long long raw_total = 0ull;
    for (unsigned int column = 0u; column < DRIVER_COLUMNS + DRIVER_TEXTS; column += 1u)
    {
        if (tally.raw_bytes[column] == 0ull)
        {
            continue;
        }
        scriptura_text(&results->line, "  ");
        scriptura_text(&results->line, driver_part_path(column));
        driver_line_decimal(results, ": crystals ", tally.crystal_bytes[column]);
        driver_line_decimal(results, " bytes of ", tally.raw_bytes[column]);
        driver_line_decimal(results, " raw, that is ",
                            (tally.raw_bytes[column] != 0ull)
                                ? ((tally.crystal_bytes[column] * 1000ull) / tally.raw_bytes[column])
                                : 0ull);
        if (column < DRIVER_COLUMNS)
        {
            scriptura_text(&results->line, " per mille, floored; forms");
            for (unsigned int form = 0u; form <= DRIVER_FORM_KEPT; form += 1u)
            {
                driver_line_decimal(results, " ", tally.forms[column][form]);
            }
        }
        else
        {
            driver_line_decimal(results, " per mille, floored; distinct strings ", tally.forms[column][0]);
            driver_line_decimal(results, " of rows ", tally.forms[column][1]);
        }
        driver_line_end(results);
        crystal_total += tally.crystal_bytes[column];
        raw_total += tally.raw_bytes[column];
    }
    driver_line_decimal(results, "  the set: ", tally.crystals);
    driver_line_decimal(results, " crystals, ", crystal_total);
    driver_line_decimal(results, " bytes of ", raw_total);
    driver_line_decimal(results, " raw, that is ", (raw_total != 0ull) ? ((crystal_total * 1000ull) / raw_total) : 0ull);
    driver_line_decimal(results, " per mille, floored; ingested in ", finished - started);
    scriptura_text(&results->line, " us");
    driver_line_end(results);
    return ok;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    if ((count != 4) || (strcmp(arguments[1], "--ingest") != 0))
    {
        scriptura_text(&results.line, "usage: casmi_driver --ingest FILE.parquet SET\n"
                                      "  --ingest: read the file's double columns into their forms on the device and seal"
                                      " them as crystals of the set SET\n");
        sim_flush(&results);
        return 2;
    }
    (void)driver_ingest(&results, count, arguments);
    return sim_close(&results, "casmi driver");
}
