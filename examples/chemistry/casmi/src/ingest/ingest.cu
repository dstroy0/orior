// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ingest.cu: casmi_driver --ingest. A parquet file is read one row group at a time, each double column as the bytes it
// is stored as, each byte pair one 16-bit lane, and every value is read into its form on the device in two passes. The
// first finds the forms each value can take: a count over its row's base, and the least places of the decimal it is,
// itself or divided by a base. A list column's row then takes the first form that holds every value of the row, at the
// most places any of them needs, its term; a row of no one form has each value take its own on the column's unit,
// else kept. The second pass writes each value's integer on its row's unit as 15-bit planes and its form beside them;
// the planes are gathered out of the records by strided copies on the device and each sealed as a crystal of the set
// through engine_crystal_write, beside the rows' terms, the kept values' lanes, each row's value count and the text
// columns' distinct strings. Every value is then read back from the crystals and held against its stored double
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
#define INGEST_COLUMNS 4u
#define INGEST_PRECURSOR 0u
#define INGEST_PEAKS 1u
#define INGEST_INTENSITIES 2u
#define INGEST_BASES 3u

static const char *const INGEST_COLUMN_PATH[INGEST_COLUMNS] = {
    "precursor_mz", "ms2_mzs.list.element", "ms2_normalized_intensities.list.element", "base_peak_intensity"};

// the text columns: each sealed as its distinct strings and each row's index among them; a file without one of them
// has it left out
#define INGEST_TEXTS 7u
static const char *const INGEST_TEXT_PATH[INGEST_TEXTS] = {"ingest_lib",        "ionization_mode", "adduct",
                                                           "molecular_formula", "inchikey14",      "normalized_smiles",
                                                           "molecule_id"};

// a sealed part's name: a double column, or a text column past them
static const char *ingest_part_path(unsigned int column)
{
    return (column < INGEST_COLUMNS) ? INGEST_COLUMN_PATH[column] : INGEST_TEXT_PATH[column - INGEST_COLUMNS];
}

// each column's unit, 10^-places, where a row's values take no one form, and the most places a row's decimal may
// take: a value on a row's places is below 2^60 at them, the m/z below 10^4 and the intensities at most 1; the bases
// have none and are kept
static const unsigned int INGEST_UNIT_PLACES[INGEST_COLUMNS] = {4u, 4u, 6u, 0u};
static const unsigned int INGEST_PLACES_MOST[INGEST_COLUMNS] = {12u, 12u, 17u, 0u};

// the most places a decimal divided by a base may take: it is at most 1000 where the value is at most 1
#define INGEST_DIVIDED_PLACES_MOST 15u

// the bases a decimal intensity is divided by in binary64 before it is stored
#define INGEST_DIVISORS 2u
static const unsigned long long INGEST_DIVISOR[INGEST_DIVISORS] = {100ull, 999ull};

// a value's form: a count over its row's base, a decimal on the unit, a decimal divided by each divisor, or kept
#define INGEST_FORM_COUNT 0u
#define INGEST_FORM_DECIMAL 1u
#define INGEST_FORM_DIVIDED 2u
#define INGEST_FORM_KEPT (INGEST_FORM_DIVIDED + INGEST_DIVISORS)

// the decimal kinds, the decimal itself and divided by each divisor, and the places program's outputs: the count's
// verdict and each kind's places, one lane each, then the count, each kind's k and the floor on the column's unit
#define INGEST_DECIMALS (1u + INGEST_DIVISORS)
#define INGEST_ROW_COUNT 0u
#define INGEST_PLACES_LANES (1u + INGEST_DECIMALS)
#define INGEST_PLACES_OUTPUTS (INGEST_PLACES_LANES + 1u + INGEST_DECIMALS + 1u)

// an integer on a unit is below 2^60, its INGEST_PLANES planes of 15 bits; a power of ten's exponent is below 2^5
#define INGEST_UNIT_BITS 60u
#define INGEST_PLACES_BITS 5u

// A row's term, one lane: its form and INGEST_TERM_FORMS times its places, INGEST_ROW_EACH where its values take no one
// form. A row's record is its base's two limbs, 0 where it has none, and its term
#define INGEST_TERM_FORMS 16u
#define INGEST_ROW_EACH 15u
#define INGEST_ROW_LIMBS 3u

// lanes and limbs of one double. An integer is written as planes of 15 bits, each a 16-bit lane: a record output is
// its register's width and a sign bit, and a value below 2^15 is an output exactly 16 bits wide, its lane. A value on
// the unit takes INGEST_PLANES planes and a kept value's 64 stored bits INGEST_KEPT_PLANES
#define INGEST_LANES_PER_VALUE 4ull
#define INGEST_LIMBS_PER_VALUE 2u
#define INGEST_PLANES 4u
#define INGEST_KEPT_PLANES 5u
#define INGEST_PLANE_BITS 15u
#define INGEST_LANE_BITS 16u
#define INGEST_WORD_BITS 32u

#define INGEST_SAMPLE_BYTES 256u

// the device bytes a value of the widest row group holds at once: its stored double, its places record of up to 30
// limbs, its record and its planes from the units program, its index and the check's record of its parts
#define INGEST_DECLARED_PER_VALUE (8ull + 120ull + 12ull + (2ull * (INGEST_PLANES + 1u)) + 12ull + 12ull)

static CasmiParquetFooter s_footer;

typedef struct
{
    unsigned long long values;
    unsigned char *bytes;
    unsigned long long rows;
    unsigned long long *row_start;
} IngestColumn;

typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} IngestLoaded;

// what one column of one row group sealed: the planes that hold anything, whether its form plane is sealed, and its
// kept values
typedef struct
{
    unsigned int planes;
    int form;
    int rows;
    unsigned long long kept;
} IngestSealed;

// what one row group's sealing came to, summed over the run
typedef struct
{
    unsigned long long crystals;
    unsigned long long crystal_bytes[INGEST_COLUMNS + INGEST_TEXTS];
    unsigned long long raw_bytes[INGEST_COLUMNS + INGEST_TEXTS];
    unsigned long long forms[INGEST_COLUMNS + INGEST_TEXTS][INGEST_FORM_KEPT + 1u];
} IngestTally;

static void ingest_line_decimal(SimResults *results, const char *before, unsigned long long value)
{
    scriptura_text(&results->line, before);
    scriptura_decimal(&results->line, value, 1u);
}

static void ingest_line_end(SimResults *results)
{
    scriptura_character(&results->line, '\n');
    sim_flush(results);
}

static void ingest_error_line(SimResults *results, const char *what, const EngineError *error)
{
    scriptura_text(&results->line, "  ");
    scriptura_text(&results->line, what);
    ingest_line_decimal(results, ": module ", (unsigned long long)error->module);
    ingest_line_decimal(results, ", site ", (unsigned long long)error->site);
    ingest_line_decimal(results, ", kind ", (unsigned long long)error->kind);
    ingest_line_end(results);
}

static void ingest_column_release(IngestColumn *column)
{
    free(column->bytes);
    free(column->row_start);
    memset(column, 0, sizeof(*column));
}

// one row group's values of one double column, as the stored bytes, eight to a value, and row r's values from
// row_start[r] up to row_start[r + 1]
static int ingest_column_read(const char *path, unsigned int group, const char *leaf_path, IngestColumn *out)
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

static int ingest_load(ExactRecordProgram *program, const unsigned int *outputs, unsigned int output_count,
                       const unsigned int *member_limbs, unsigned int members, IngestLoaded *loaded, EngineError *error)
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

static void ingest_release(IngestLoaded *loaded)
{
    cycle_record_release(loaded->record);
    key_schedule_record_release(&loaded->layout);
    keymath_record_release(&loaded->key);
}

// the least and most exponents as placed over a column's values, 1 for a subnormal, read from the stored bits
static void ingest_exponents(const IngestColumn *column, unsigned long long *least, unsigned long long *most)
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

// 0 as a register the program does not know: a value it reads less itself. A step over known values is folded to
// their constant, as narrow as the constant is; a lane over this 0 stays a register and keeps its width
static unsigned int ingest_unknown_zero(ExactRecordProgram *program, unsigned int read)
{
    return exact_record_difference(program, read, read);
}

// A value's low INGEST_PLANE_BITS bits as an output exactly one lane wide. The and with 2^15 - 1 is no wider than
// its narrower operand; 2^15 and the unknown 0 are added first: the sum is wider than the mask and no constant, and
// its low bits are the value's
static unsigned int ingest_lane(ExactRecordProgram *program, unsigned int value, unsigned int zero)
{
    const unsigned int padded = exact_record_sum(
        program, exact_record_sum(program, value, zero), exact_record_power_two(program, INGEST_PLANE_BITS));
    return exact_record_narrow(program, padded, INGEST_PLANE_BITS);
}

// a value never negative written as `planes` planes, its low INGEST_PLANE_BITS bits first, each an output one lane wide
static void ingest_planes(ExactRecordProgram *program, unsigned int value, unsigned int planes, unsigned int zero,
                          unsigned int *outputs)
{
    const unsigned int plane_size = exact_record_power_two(program, INGEST_PLANE_BITS);
    unsigned int rest = value;
    for (unsigned int plane = 0u; plane < planes; plane += 1u)
    {
        outputs[plane] = ingest_lane(program, rest, zero);
        rest = exact_record_quotient(program, rest, plane_size);
    }
}

// each row's record: its base's stored double where the column reads one and the row holds it, and its term 0
static unsigned int *ingest_rows_build(const IngestColumn *values, const IngestColumn *bases)
{
    unsigned int *const rows = (unsigned int *)calloc((size_t)(values->rows * INGEST_ROW_LIMBS) + 1u, sizeof(unsigned int));
    if ((rows == NULL) || ((bases != NULL) && (bases->rows != values->rows)))
    {
        free(rows);
        return NULL;
    }
    for (unsigned long long row = 0ull; (bases != NULL) && (row < values->rows); row += 1ull)
    {
        if (bases->row_start[row + 1ull] > bases->row_start[row])
        {
            memcpy(&rows[row * INGEST_ROW_LIMBS], bases->bytes + (bases->row_start[row] * 8ull), 8u);
        }
    }
    return rows;
}

// each value's index: `members` record numbers, the value's own for every member before the last and its row's for
// the last
static unsigned int *ingest_index_build(const IngestColumn *values, unsigned int members)
{
    unsigned int *const index = (unsigned int *)malloc((size_t)(values->values * members) * sizeof(unsigned int) + 4u);
    for (unsigned long long row = 0ull; (index != NULL) && (row < values->rows); row += 1ull)
    {
        for (unsigned long long value = values->row_start[row]; value < values->row_start[row + 1ull]; value += 1ull)
        {
            for (unsigned int member = 0u; member < members; member += 1u)
            {
                index[(value * members) + member] = (unsigned int)(((member + 1u) == members) ? row : value);
            }
        }
    }
    return index;
}

// the most places decimal kind `kind` may take in a column: the decimal itself, or one divided by a base
static unsigned int ingest_kind_most(unsigned int column, unsigned int kind)
{
    return (kind == 0u) ? INGEST_PLACES_MOST[column] : INGEST_DIVIDED_PLACES_MOST;
}

// The places program, the first pass over a column: for each value, member 0, the forms it can take and where. The
// outputs are first the count's verdict and each decimal kind's least places, one lane each, laid at bit 16 j so they
// gather into planes, and then the count, each decimal kind's k and the value's floor on the column's unit. A decimal
// kind holds a value at its places p where p is at most the kind's most places and k 10^(most - p) is below 2^60;
// the value fits its planes at any places a row may take; otherwise its places read as none, the most places and 1.
// The intensities read their row's base from member 1
static void ingest_places_program(ExactRecordProgram *program, unsigned int column, unsigned long long most_placed,
                                  unsigned int shift_bits, unsigned int *outputs)
{
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int width = exact_record_power_two(program, INGEST_UNIT_BITS);
    FormsFields fields;
    forms_fields(program, &fields);
    FormsDouble value;
    forms_double(program, &fields, 0u, &value);
    FormsPreimage preimage;
    forms_preimage(program, &value, most_placed, shift_bits, &preimage);
    const unsigned int unknown = ingest_unknown_zero(program, value.fraction);
    unsigned int count_held = zero;
    unsigned int count = zero;
    if (column == INGEST_INTENSITIES)
    {
        FormsDouble base;
        forms_double(program, &fields, 1u, &base);
        forms_count(program, &preimage, &base, &count_held, &count);
        count_held = exact_record_product(program, count_held, exact_record_above(program, width, count));
        count = exact_record_product(program, count_held, count);
    }
    outputs[INGEST_ROW_COUNT] = ingest_lane(program, count_held, unknown);
    for (unsigned int kind = 0u; kind < INGEST_DECIMALS; kind += 1u)
    {
        const unsigned int most = ingest_kind_most(column, kind);
        const unsigned int none = exact_record_constant(program, (unsigned long long)most + 1ull);
        unsigned int places = none;
        unsigned int k = zero;
        if ((kind == 0u) || (column == INGEST_INTENSITIES))
        {
            FormsPreimage divided;
            unsigned int exists = exact_record_constant(program, 1ull);
            if (kind != 0u)
            {
                forms_divided(program, &preimage, INGEST_DIVISOR[kind - 1u], &divided, &exists);
            }
            forms_places(program, (kind == 0u) ? &preimage : &divided, most, &places, &k);
            // held where some places hold, a D divides to the value, and k at the kind's most places is below 2^60
            const unsigned int found = exact_record_product(program, exists, exact_record_above(program, none, places));
            const unsigned int lift = exact_record_select(
                program, found, exact_record_difference(program, exact_record_constant(program, most), places), zero);
            const unsigned int widest =
                exact_record_product(program, k, exact_record_power_of(program, 10ull, lift, INGEST_PLACES_BITS));
            const unsigned int held = exact_record_product(program, found, exact_record_above(program, width, widest));
            places = exact_record_select(program, held, places, none);
            k = exact_record_product(program, held, k);
        }
        outputs[INGEST_ROW_COUNT + 1u + kind] = ingest_lane(program, places, unknown);
        outputs[INGEST_PLACES_LANES + 1u + kind] = exact_record_narrow(program, k, INGEST_UNIT_BITS);
    }
    outputs[INGEST_PLACES_LANES] = exact_record_narrow(program, count, INGEST_UNIT_BITS);
    const unsigned int floor = forms_floor(program, &preimage, INGEST_UNIT_PLACES[column]);
    outputs[INGEST_PLACES_LANES + 1u + INGEST_DECIMALS] =
        exact_record_narrow(program, exact_record_product(program, floor, exact_record_above(program, width, floor)),
                            INGEST_UNIT_BITS);
}

// The units program, the second pass: each value's places record, member 0, and its row's record, member 1, whose
// term names the row's form and places. A row of one form takes it at the row's places: a count as it is, a decimal
// kind's k times 10^(row places - p). A row of no one form has each value take the first form that holds it on the
// column's unit, a count, then the decimal kinds at the unit's places, else kept at its floor on the unit. Outputs
// the integer as INGEST_PLANES planes and the value's form after them, one lane each
static void ingest_units_program(ExactRecordProgram *program, unsigned int column, const IngestLoaded *places_loaded,
                                 const unsigned int *places_outputs, unsigned int *outputs)
{
    const unsigned int unit_places = INGEST_UNIT_PLACES[column];
    unsigned int read[INGEST_PLACES_OUTPUTS];
    for (unsigned int output = 0u; output < INGEST_PLACES_OUTPUTS; output += 1u)
    {
        const DeviceRecordStep place = places_loaded->layout.step_table[places_outputs[output]];
        const unsigned int field = exact_record_member_field(program, place.out_bits, place.out_offset);
        read[output] = exact_record_read_unsigned(program, field, 0u);
    }
    const unsigned int term_field =
        exact_record_member_field(program, INGEST_WORD_BITS, INGEST_LIMBS_PER_VALUE * INGEST_WORD_BITS);
    const unsigned int term = exact_record_read_unsigned(program, term_field, 1u);
    const unsigned int sixteen = exact_record_constant(program, INGEST_TERM_FORMS);
    const unsigned int row_form = exact_record_remainder(program, term, sixteen);
    const unsigned int row_places = exact_record_quotient(program, term, sixteen);
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int each = exact_record_equal(program, row_form, exact_record_constant(program, INGEST_ROW_EACH));
    // a value on its own: the first form that holds it on the column's unit, in reverse so the first stands
    unsigned int each_unit = read[INGEST_PLACES_LANES + 1u + INGEST_DECIMALS];
    unsigned int each_form = exact_record_constant(program, INGEST_FORM_KEPT);
    const unsigned int on_unit = exact_record_constant(program, unit_places);
    for (unsigned int kind = INGEST_DECIMALS; kind > 0u; kind -= 1u)
    {
        const unsigned int places = read[INGEST_ROW_COUNT + kind];
        const unsigned int held = exact_record_difference(program, one, exact_record_above(program, places, on_unit));
        const unsigned int lift = exact_record_select(program, held, exact_record_difference(program, on_unit, places), zero);
        const unsigned int scaled = exact_record_product(program, read[INGEST_PLACES_LANES + kind],
                                                         exact_record_power_of(program, 10ull, lift, INGEST_PLACES_BITS));
        each_unit = exact_record_select(program, held, scaled, each_unit);
        each_form = exact_record_select(program, held, exact_record_constant(program, INGEST_FORM_DECIMAL + kind - 1u),
                                        each_form);
    }
    const unsigned int count_held = read[INGEST_ROW_COUNT];
    each_unit = exact_record_select(program, count_held, read[INGEST_PLACES_LANES], each_unit);
    each_form = exact_record_select(program, count_held, exact_record_constant(program, INGEST_FORM_COUNT), each_form);
    // the row's form at the row's places
    unsigned int row_unit = exact_record_product(
        program, exact_record_equal(program, row_form, exact_record_constant(program, INGEST_FORM_COUNT)),
        read[INGEST_PLACES_LANES]);
    for (unsigned int kind = 0u; kind < INGEST_DECIMALS; kind += 1u)
    {
        const unsigned int named =
            exact_record_equal(program, row_form, exact_record_constant(program, INGEST_FORM_DECIMAL + kind));
        const unsigned int lift = exact_record_select(
            program, named, exact_record_difference(program, row_places, read[INGEST_ROW_COUNT + 1u + kind]), zero);
        row_unit = exact_record_sum(
            program, row_unit,
            exact_record_product(program, named,
                                 exact_record_product(program, read[INGEST_PLACES_LANES + 1u + kind],
                                                      exact_record_power_of(program, 10ull, lift, INGEST_PLACES_BITS))));
    }
    const unsigned int unit = exact_record_select(program, each, each_unit, row_unit);
    const unsigned int form = exact_record_select(program, each, each_form, row_form);
    const unsigned int unknown = ingest_unknown_zero(program, term);
    ingest_planes(program, unit, INGEST_PLANES, unknown, outputs);
    outputs[INGEST_PLANES] = ingest_lane(program, form, unknown);
}

// the kept values' program: each record's 64 stored bits, its two 32-bit words, as INGEST_KEPT_PLANES planes
static void ingest_kept_program(ExactRecordProgram *program, unsigned int *outputs)
{
    const unsigned int low = exact_record_member_field(program, INGEST_WORD_BITS, 0u);
    const unsigned int high = exact_record_member_field(program, INGEST_WORD_BITS, INGEST_WORD_BITS);
    const unsigned int stored = exact_record_sum(
        program, exact_record_read_unsigned(program, low, 0u),
        exact_record_product(program, exact_record_read_unsigned(program, high, 0u),
                             exact_record_power_two(program, INGEST_WORD_BITS)));
    ingest_planes(program, stored, INGEST_KEPT_PLANES, ingest_unknown_zero(program, stored), outputs);
}

// 1 where output `output` of a loaded program sits at bit `offset` and is `bits` wide
static int ingest_output_at(const IngestLoaded *loaded, unsigned int output, unsigned int offset, unsigned int bits)
{
    const DeviceRecordStep place = loaded->layout.step_table[output];
    return (place.out_offset == offset) && (place.out_bits == bits);
}

// 1 where output j of a loaded program sits at bit 16 j and is one lane wide, for each of `count` outputs; the outputs'
// places printed where they do not
static int ingest_laid(SimResults *results, const char *program, const IngestLoaded *loaded,
                       const unsigned int *outputs, unsigned int count)
{
    int laid = 1;
    for (unsigned int output = 0u; output < count; output += 1u)
    {
        laid = laid && ingest_output_at(loaded, outputs[output], output * INGEST_LANE_BITS, INGEST_LANE_BITS);
    }
    if (!laid)
    {
        scriptura_text(&results->line, "    ");
        scriptura_text(&results->line, program);
        scriptura_text(&results->line, "'s outputs, bit and width:");
        for (unsigned int output = 0u; output < count; output += 1u)
        {
            const DeviceRecordStep place = loaded->layout.step_table[outputs[output]];
            ingest_line_decimal(results, " ", (unsigned long long)place.out_offset);
            ingest_line_decimal(results, "/", (unsigned long long)place.out_bits);
        }
        ingest_line_decimal(results, "; record ", (unsigned long long)loaded->layout.out_bits);
        ingest_line_end(results);
    }
    return laid;
}

static int ingest_sample_name(char *out, unsigned int group, unsigned int column, const char *part)
{
    const int written = snprintf(out, INGEST_SAMPLE_BYTES, "row_group_%u.%s.%s", group, ingest_part_path(column), part);
    return (written > 0) && (written < (int)INGEST_SAMPLE_BYTES);
}

static int ingest_seal_host(SimResults *results, const char *set, const char *sample, const unsigned short *lanes,
                            const unsigned long long extent[4], unsigned int column, IngestTally *tally);

// one lattice of device lanes sealed as a crystal of the set, its bytes added to the column's tally
static int ingest_seal(SimResults *results, const char *set, const char *sample, const unsigned short *device_lanes,
                       const unsigned long long extent[4], unsigned int column, IngestTally *tally)
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
        ingest_error_line(results, "a crystal did not seal", &error);
        return 0;
    }
    tally->crystals += 1ull;
    tally->crystal_bytes[column] += record.crystal_bytes;
    scriptura_text(&results->line, "    ");
    scriptura_text(&results->line, sample);
    ingest_line_decimal(results, ": ", record.crystal_bytes);
    ingest_line_decimal(results, " bytes of ", record.raw_bytes);
    ingest_line_end(results);
    return 1;
}

// One column of one row group read into its forms on the device and sealed: its planes that hold anything, its form
// plane where any value is not in the column's first form, and its kept values' lanes as four planes
static int ingest_column_forms(SimResults *results, const char *set, unsigned int group, unsigned int column,
                               const IngestColumn *values, const IngestColumn *bases, IngestTally *tally,
                               IngestSealed *sealed)
{
    memset(sealed, 0, sizeof(*sealed));
    const unsigned long long count = values->values;
    if (count == 0ull)
    {
        return 1;
    }
    unsigned long long least_placed = 0ull;
    unsigned long long most_placed = 0ull;
    ingest_exponents(values, &least_placed, &most_placed);
    if (most_placed >= FORMS_LIFT)
    {
        sim_check(results, 0, "the column holds no value past 2^53");
        return 0;
    }
    const unsigned int shift_bits = exact_record_bits_of(FORMS_LIFT - least_placed) + 1u;
    const size_t lane_bytes = sizeof(unsigned short);
    // each row's record, its base and its term, and each value's index pair, its own record and its row's
    unsigned int *const rows = ingest_rows_build(values, bases);
    unsigned int *const index = ingest_index_build(values, 2u);
    unsigned int *device_rows = NULL;
    unsigned int *device_index = NULL;
    unsigned int *device_values = NULL;
    const size_t rows_bytes = (size_t)values->rows * INGEST_ROW_LIMBS * sizeof(unsigned int);
    int ok = (rows != NULL) && (index != NULL) &&
             (cudaMalloc((void **)&device_values, (size_t)(count * 8ull)) == cudaSuccess) &&
             (cudaMemcpy(device_values, values->bytes, (size_t)(count * 8ull), cudaMemcpyHostToDevice) == cudaSuccess) &&
             (cudaMalloc((void **)&device_rows, rows_bytes) == cudaSuccess) &&
             (cudaMemcpy(device_rows, rows, rows_bytes, cudaMemcpyHostToDevice) == cudaSuccess) &&
             (cudaMalloc((void **)&device_index, (size_t)(count * 2ull) * sizeof(unsigned int)) == cudaSuccess) &&
             (cudaMemcpy(device_index, index, (size_t)(count * 2ull) * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
              cudaSuccess);
    // the first pass: each value's places for every form
    ExactRecordProgram places_program;
    exact_record_open(&places_program);
    unsigned int places_outputs[INGEST_PLACES_OUTPUTS];
    ingest_places_program(&places_program, column, most_placed, shift_bits, places_outputs);
    const unsigned int members_limbs[ENGINE_RECORD_MEMBERS_MAX] = {INGEST_LIMBS_PER_VALUE, INGEST_ROW_LIMBS,
                                                                   INGEST_ROW_LIMBS};
    IngestLoaded places_loaded;
    EngineError error;
    const int places_held =
        ok && ingest_load(&places_program, places_outputs, INGEST_PLACES_OUTPUTS, members_limbs, 2u, &places_loaded, &error);
    if (ok && !places_held)
    {
        ingest_error_line(results, "the places program did not load", &error);
    }
    ok = places_held && ingest_laid(results, "the places program", &places_loaded, places_outputs, INGEST_PLACES_LANES);
    sim_check(results, ok, "the places program lays each value's places at its own 16 bits");
    const size_t places_record_bytes = places_held ? ((size_t)places_loaded.layout.out_limbs * sizeof(unsigned int)) : 0u;
    unsigned int *device_places = NULL;
    ok = ok && (cudaMalloc((void **)&device_places, (size_t)count * places_record_bytes) == cudaSuccess);
    memset(&error, 0, sizeof(error));
    const unsigned long long run_start = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordRunRequest run = {places_loaded.record, {device_values, device_rows, NULL},
                                           {count, values->rows, 0ull},  device_index,
                                           count,                       device_places,
                                           &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
        if (!ok)
        {
            ingest_error_line(results, "the places program's run errored", &error);
        }
    }
    // each row's form: the first that holds every value of the row, at the most places any of them needs
    unsigned short *const lanes = (unsigned short *)malloc((size_t)(count * INGEST_PLACES_LANES) * lane_bytes + 2u);
    ok = ok && (lanes != NULL) &&
         (cudaMemcpy2D(lanes, INGEST_PLACES_LANES * lane_bytes, device_places, places_record_bytes,
                       INGEST_PLACES_LANES * lane_bytes, (size_t)count, cudaMemcpyDeviceToHost) == cudaSuccess);
    unsigned long long row_forms[INGEST_TERM_FORMS];
    memset(row_forms, 0, sizeof(row_forms));
    unsigned short *const terms = (unsigned short *)malloc((size_t)values->rows * lane_bytes + 2u);
    ok = ok && (terms != NULL);
    for (unsigned long long row = 0ull; ok && (row < values->rows); row += 1ull)
    {
        unsigned int term = INGEST_ROW_EACH;
        const unsigned long long first = values->row_start[row];
        const unsigned long long past = values->row_start[row + 1ull];
        for (unsigned int form = 0u; (column != INGEST_PRECURSOR) && (past > first) && (term == INGEST_ROW_EACH) &&
                                     (form <= INGEST_DECIMALS);
             form += 1u)
        {
            int all = 1;
            unsigned int places = 0u;
            for (unsigned long long value = first; all && (value < past); value += 1ull)
            {
                const unsigned int lane = lanes[(value * INGEST_PLACES_LANES) + form];
                all = (form == INGEST_FORM_COUNT) ? (lane == 1u) : (lane <= ingest_kind_most(column, form - 1u));
                places = ((form != INGEST_FORM_COUNT) && (lane > places)) ? lane : places;
            }
            term = all ? (form + (INGEST_TERM_FORMS * places)) : term;
        }
        terms[row] = (unsigned short)term;
        rows[(row * INGEST_ROW_LIMBS) + INGEST_LIMBS_PER_VALUE] = term;
        row_forms[term % INGEST_TERM_FORMS] += 1ull;
    }
    free(lanes);
    ok = ok && (cudaMemcpy(device_rows, rows, rows_bytes, cudaMemcpyHostToDevice) == cudaSuccess);
    // the second pass: each value's integer on its row's unit, or on the column's where the row holds no one form
    ExactRecordProgram program;
    exact_record_open(&program);
    unsigned int outputs[INGEST_PLANES + 1u];
    if (places_held)
    {
        ingest_units_program(&program, column, &places_loaded, places_outputs, outputs);
    }
    const unsigned int units_limbs[ENGINE_RECORD_MEMBERS_MAX] = {places_held ? places_loaded.layout.out_limbs : 0u,
                                                                 INGEST_ROW_LIMBS, 0u};
    IngestLoaded loaded;
    const int units_held = ok && ingest_load(&program, outputs, INGEST_PLANES + 1u, units_limbs, 2u, &loaded, &error);
    if (ok && !units_held)
    {
        ingest_error_line(results, "the units program did not load", &error);
    }
    ok = units_held && ingest_laid(results, "the units program", &loaded, outputs, INGEST_PLANES + 1u);
    sim_check(results, ok, "the units program lays each plane at its own 16 bits");
    const unsigned int out_limbs = units_held ? loaded.layout.out_limbs : 0u;
    const size_t record_bytes = (size_t)out_limbs * sizeof(unsigned int);
    unsigned int *device_out = NULL;
    unsigned short *device_planes = NULL;
    ok = ok && (cudaMalloc((void **)&device_out, (size_t)count * record_bytes) == cudaSuccess) &&
         (cudaMalloc((void **)&device_planes, (size_t)(count * (INGEST_PLANES + 1u)) * lane_bytes) == cudaSuccess);
    if (ok)
    {
        const CycleRecordRunRequest run = {loaded.record, {device_places, device_rows, NULL},
                                           {count, values->rows, 0ull}, device_index,
                                           count,                       device_out,
                                           &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
        if (!ok)
        {
            ingest_error_line(results, "the units program's run errored", &error);
        }
    }
    const unsigned long long run_end = engine_clock_microseconds();
    // plane j of every record, gathered beside the others: one strided copy on the device a plane
    for (unsigned int plane = 0u; ok && (plane <= INGEST_PLANES); plane += 1u)
    {
        ok = cudaMemcpy2D(device_planes + (plane * count), lane_bytes,
                          (const unsigned char *)device_out + (plane * lane_bytes), record_bytes, lane_bytes,
                          (size_t)count, cudaMemcpyDeviceToDevice) == cudaSuccess;
    }
    cudaFree(device_out);
    cudaFree(device_places);
    cudaFree(device_rows);
    cudaFree(device_index);
    free(rows);
    free(index);
    if (places_held)
    {
        ingest_release(&places_loaded);
    }
    exact_record_close(&places_program);
    // the rows' terms, for a list column, sealed beside its planes
    char sample[INGEST_SAMPLE_BYTES];
    if (ok && (column != INGEST_PRECURSOR))
    {
        const unsigned long long row_line[4] = {1ull, 1ull, 1ull, values->rows};
        ok = ingest_sample_name(sample, group, column, "row") &&
             ingest_seal_host(results, set, sample, terms, row_line, column, tally);
        sealed->rows = ok;
        scriptura_text(&results->line, "   ");
        scriptura_text(&results->line, INGEST_COLUMN_PATH[column]);
        scriptura_text(&results->line, " rows by their form, count, decimal and each divided from the lowest, then each"
                                       " value its own:");
        for (unsigned int form = 0u; form < INGEST_TERM_FORMS; form += 1u)
        {
            if (row_forms[form] != 0ull)
            {
                ingest_line_decimal(results, " ", (unsigned long long)form);
                ingest_line_decimal(results, ":", row_forms[form]);
            }
        }
        ingest_line_end(results);
    }
    free(terms);
    // the form plane comes back to name the kept values, and each plane's highest lane to name the planes that hold
    unsigned short *const forms = (unsigned short *)malloc((size_t)count * lane_bytes + 2u);
    ok = ok && (forms != NULL) &&
         (cudaMemcpy(forms, device_planes + (INGEST_PLANES * count), (size_t)count * lane_bytes, cudaMemcpyDeviceToHost) ==
          cudaSuccess);
    unsigned long long form_count[INGEST_FORM_KEPT + 1u];
    memset(form_count, 0, sizeof(form_count));
    for (unsigned long long value = 0ull; ok && (value < count); value += 1ull)
    {
        form_count[(forms[value] <= INGEST_FORM_KEPT) ? forms[value] : INGEST_FORM_KEPT] += 1ull;
    }
    for (unsigned int form = 0u; form <= INGEST_FORM_KEPT; form += 1u)
    {
        tally->forms[column][form] += form_count[form];
    }
    // each row's forms, as one bit a form: the rows whose values take one form, and the mixes the others take
    if (ok && (column != INGEST_PRECURSOR))
    {
        unsigned long long mixes[1u << (INGEST_FORM_KEPT + 1u)];
        memset(mixes, 0, sizeof(mixes));
        for (unsigned long long row = 0ull; row < values->rows; row += 1ull)
        {
            unsigned int mask = 0u;
            for (unsigned long long value = values->row_start[row]; value < values->row_start[row + 1ull]; value += 1ull)
            {
                mask |= 1u << ((forms[value] <= INGEST_FORM_KEPT) ? forms[value] : INGEST_FORM_KEPT);
            }
            mixes[mask] += 1ull;
        }
        scriptura_text(&results->line, "   ");
        scriptura_text(&results->line, INGEST_COLUMN_PATH[column]);
        scriptura_text(&results->line, " rows by the forms their values take, each form a bit, count, decimal, divided"
                                       " and kept from the lowest:");
        for (unsigned int mask = 0u; mask < (1u << (INGEST_FORM_KEPT + 1u)); mask += 1u)
        {
            if (mixes[mask] != 0ull)
            {
                ingest_line_decimal(results, " ", (unsigned long long)mask);
                ingest_line_decimal(results, ":", mixes[mask]);
            }
        }
        ingest_line_end(results);
    }
    const unsigned int first_form = (column == INGEST_INTENSITIES) ? INGEST_FORM_COUNT : INGEST_FORM_DECIMAL;
    const unsigned long long line[4] = {1ull, 1ull, 1ull, count};
    for (unsigned int plane = 0u; ok && (plane < INGEST_PLANES); plane += 1u)
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
        ok = ok && (!holds || (ingest_sample_name(sample, group, column, part) &&
                               ingest_seal(results, set, sample, device_planes + (plane * count), line, column, tally)));
        sealed->planes |= (ok && holds) ? (1u << plane) : 0u;
    }
    if (ok && (form_count[first_form] != count))
    {
        ok = ingest_sample_name(sample, group, column, "form") &&
             ingest_seal(results, set, sample, device_planes + (INGEST_PLANES * count), line, column, tally);
        sealed->form = ok;
    }
    sealed->kept = form_count[INGEST_FORM_KEPT];
    const unsigned long long kept = form_count[INGEST_FORM_KEPT];
    if (ok && (kept != 0ull))
    {
        // the kept values' records, named by an index, written as their stored bits' planes and gathered
        unsigned int *const kept_index = (unsigned int *)malloc((size_t)kept * sizeof(unsigned int));
        unsigned long long at = 0ull;
        for (unsigned long long value = 0ull; (kept_index != NULL) && (value < count); value += 1ull)
        {
            if (forms[value] == INGEST_FORM_KEPT)
            {
                kept_index[at] = (unsigned int)value;
                at += 1ull;
            }
        }
        ExactRecordProgram copy;
        exact_record_open(&copy);
        const unsigned int doubles[ENGINE_RECORD_MEMBERS_MAX] = {INGEST_LIMBS_PER_VALUE, 0u, 0u};
        unsigned int copy_outputs[INGEST_KEPT_PLANES];
        ingest_kept_program(&copy, copy_outputs);
        IngestLoaded copied;
        unsigned int *device_kept_index = NULL;
        unsigned int *device_kept = NULL;
        unsigned short *device_kept_planes = NULL;
        const int copy_loaded =
            (kept_index != NULL) && ingest_load(&copy, copy_outputs, INGEST_KEPT_PLANES, doubles, 1u, &copied, &error);
        const int copy_laid =
            copy_loaded && ingest_laid(results, "the kept values' program", &copied, copy_outputs, INGEST_KEPT_PLANES);
        sim_check(results, copy_laid, "the kept values' program lays each plane at its own 16 bits");
        const size_t kept_record_bytes = copy_loaded ? ((size_t)copied.layout.out_limbs * sizeof(unsigned int)) : 0u;
        ok = copy_laid && (cudaMalloc((void **)&device_kept_index, (size_t)kept * sizeof(unsigned int)) == cudaSuccess) &&
             (cudaMemcpy(device_kept_index, kept_index, (size_t)kept * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
              cudaSuccess) &&
             (cudaMalloc((void **)&device_kept, (size_t)kept * kept_record_bytes) == cudaSuccess) &&
             (cudaMalloc((void **)&device_kept_planes, (size_t)(kept * INGEST_KEPT_PLANES) * lane_bytes) == cudaSuccess);
        if (ok)
        {
            const CycleRecordRunRequest run = {copied.record, {device_values, NULL, NULL}, {count, 0ull, 0ull},
                                               device_kept_index, kept, device_kept, &error};
            ok = cycle_record_run(&run) != CYCLE_ERROR;
        }
        for (unsigned long long plane = 0ull; ok && (plane < INGEST_KEPT_PLANES); plane += 1ull)
        {
            ok = cudaMemcpy2D(device_kept_planes + (plane * kept), lane_bytes,
                              (const unsigned char *)device_kept + (plane * lane_bytes), kept_record_bytes, lane_bytes,
                              (size_t)kept, cudaMemcpyDeviceToDevice) == cudaSuccess;
        }
        const unsigned long long kept_extent[4] = {1ull, 1ull, INGEST_KEPT_PLANES, kept};
        ok = ok && ingest_sample_name(sample, group, column, "kept") &&
             ingest_seal(results, set, sample, device_kept_planes, kept_extent, column, tally);
        if (copy_loaded)
        {
            ingest_release(&copied);
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
    scriptura_text(&results->line, INGEST_COLUMN_PATH[column]);
    ingest_line_decimal(results, ": ", count);
    ingest_line_decimal(results, " values, forms program ", program.count);
    ingest_line_decimal(results, " steps, device ", run_end - run_start);
    scriptura_text(&results->line, " us; forms");
    for (unsigned int form = 0u; form <= INGEST_FORM_KEPT; form += 1u)
    {
        ingest_line_decimal(results, " ", form_count[form]);
    }
    ingest_line_end(results);
    free(forms);
    cudaFree(device_values);
    cudaFree(device_planes);
    if (units_held)
    {
        ingest_release(&loaded);
    }
    exact_record_close(&program);
    return ok;
}

// The check program: each value's stored double, member 0, against what the set's crystals hold for it, member 1, its
// planes and its form one lane each, and for the intensities its row's base, member 2. The integer u the planes make
// is held against the double's preimage in the value's form: u / 10^places a decimal, u / B a count, u / 10^places a
// decimal the double is that over a divisor. A kept value reads 1 here and is held against its lanes apart
static void ingest_check_program(ExactRecordProgram *program, unsigned int column, unsigned long long most_placed,
                                 unsigned int shift_bits, unsigned int *output)
{
    const unsigned int places = INGEST_UNIT_PLACES[column];
    FormsFields fields;
    forms_fields(program, &fields);
    FormsDouble value;
    forms_double(program, &fields, 0u, &value);
    FormsPreimage preimage;
    forms_preimage(program, &value, most_placed, shift_bits, &preimage);
    const unsigned int plane_size = exact_record_power_two(program, INGEST_PLANE_BITS);
    unsigned int unit = exact_record_constant(program, 0ull);
    unsigned int weight = exact_record_constant(program, 1ull);
    for (unsigned int plane = 0u; plane < INGEST_PLANES; plane += 1u)
    {
        const unsigned int field = exact_record_member_field(program, INGEST_PLANE_BITS, plane * INGEST_LANE_BITS);
        unit = exact_record_sum(program, unit,
                                exact_record_product(program, exact_record_read_unsigned(program, field, 1u), weight));
        weight = exact_record_product(program, weight, plane_size);
    }
    const unsigned int form_field = exact_record_member_field(program, INGEST_PLANE_BITS, INGEST_PLANES * INGEST_LANE_BITS);
    const unsigned int form = exact_record_read_unsigned(program, form_field, 1u);
    // the row's term: its places where it holds one form, the column's unit where its values take each their own
    const unsigned int term_field =
        exact_record_member_field(program, INGEST_WORD_BITS, INGEST_LIMBS_PER_VALUE * INGEST_WORD_BITS);
    const unsigned int term = exact_record_read_unsigned(program, term_field, 2u);
    const unsigned int sixteen = exact_record_constant(program, INGEST_TERM_FORMS);
    const unsigned int each = exact_record_equal(program, exact_record_remainder(program, term, sixteen),
                                                 exact_record_constant(program, INGEST_ROW_EACH));
    const unsigned int ten =
        exact_record_select(program, each, exact_record_constant(program, forms_ten(places)),
                            exact_record_power_of(program, 10ull, exact_record_quotient(program, term, sixteen),
                                                  INGEST_PLACES_BITS));
    unsigned int verdict = exact_record_product(
        program, exact_record_equal(program, form, exact_record_constant(program, INGEST_FORM_DECIMAL)),
        forms_contains(program, &preimage, ten, unit));
    verdict = exact_record_sum(
        program, verdict, exact_record_equal(program, form, exact_record_constant(program, INGEST_FORM_KEPT)));
    if (column == INGEST_INTENSITIES)
    {
        FormsDouble base;
        forms_double(program, &fields, 2u, &base);
        unsigned int whole_base = 0u;
        unsigned int whole = 0u;
        forms_whole(program, &base, &whole_base, &whole);
        verdict = exact_record_sum(
            program, verdict,
            exact_record_product(
                program, exact_record_equal(program, form, exact_record_constant(program, INGEST_FORM_COUNT)),
                exact_record_product(program, whole, forms_contains(program, &preimage, whole_base, unit))));
        for (unsigned int divisor = 0u; divisor < INGEST_DIVISORS; divisor += 1u)
        {
            FormsPreimage divided;
            unsigned int exists = 0u;
            forms_divided(program, &preimage, INGEST_DIVISOR[divisor], &divided, &exists);
            verdict = exact_record_sum(
                program, verdict,
                exact_record_product(
                    program,
                    exact_record_equal(program, form, exact_record_constant(program, INGEST_FORM_DIVIDED + divisor)),
                    exact_record_product(program, exists, forms_contains(program, &divided, ten, unit))));
        }
    }
    *output = ingest_lane(program, verdict, ingest_unknown_zero(program, unit));
}

// One column of one row group read back from the set: its crystals loaded, each value's planes and form laid beside
// each other into a record of its own on the device by strided copies, and the check program run over every value
// against its stored double, the verdicts summed on the device. The kept values' lanes are rebuilt from their crystal
// and held against the stored bytes of the values whose form is kept
static int ingest_column_check(SimResults *results, const char *set, unsigned int group, unsigned int column,
                               const IngestColumn *values, const IngestColumn *bases, const IngestSealed *sealed)
{
    const unsigned long long count = values->values;
    if (count == 0ull)
    {
        return 1;
    }
    const size_t lane_bytes = sizeof(unsigned short);
    const unsigned int part_limbs = (((INGEST_PLANES + 1u) * INGEST_LANE_BITS) + INGEST_WORD_BITS - 1u) / INGEST_WORD_BITS;
    const size_t part_bytes = (size_t)part_limbs * sizeof(unsigned int);
    const unsigned int first_form = (column == INGEST_INTENSITIES) ? INGEST_FORM_COUNT : INGEST_FORM_DECIMAL;
    char sample[INGEST_SAMPLE_BYTES];
    // the parts as planes: each loaded crystal's lanes, 0 for a plane not sealed and the first form for a form plane
    unsigned short *const parts = (unsigned short *)calloc((size_t)(count * (INGEST_PLANES + 1u)), lane_bytes);
    int ok = parts != NULL;
    for (unsigned long long value = 0ull; ok && !sealed->form && (value < count); value += 1ull)
    {
        parts[(INGEST_PLANES * count) + value] = (unsigned short)first_form;
    }
    const unsigned long long started = engine_clock_microseconds();
    for (unsigned int plane = 0u; ok && (plane <= INGEST_PLANES); plane += 1u)
    {
        const int held = (plane < INGEST_PLANES) ? ((sealed->planes >> plane) & 1u) : sealed->form;
        if (!held)
        {
            continue;
        }
        char part[16];
        if (plane < INGEST_PLANES)
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
        ok = ingest_sample_name(sample, group, column, part) &&
             (engine_iapx_load(set, sample, extent, &volume, &root, NULL, &error) == 0L) && (extent[3] == count);
        if (ok)
        {
            memcpy(parts + (plane * count), volume, (size_t)count * lane_bytes);
        }
        else
        {
            ingest_error_line(results, "a crystal did not load", &error);
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
        ok = ingest_sample_name(sample, group, column, "kept") &&
             (engine_iapx_load(set, sample, extent, &volume, &root, NULL, &error) == 0L) &&
             (extent[2] == INGEST_KEPT_PLANES) && (extent[3] == sealed->kept);
        unsigned long long at = 0ull;
        for (unsigned long long value = 0ull; ok && (value < count); value += 1ull)
        {
            if (parts[(INGEST_PLANES * count) + value] != INGEST_FORM_KEPT)
            {
                continue;
            }
            unsigned long long stored = 0ull;
            for (unsigned int plane = INGEST_KEPT_PLANES; plane > 0u; plane -= 1u)
            {
                stored = (stored << INGEST_PLANE_BITS) | volume[((plane - 1u) * sealed->kept) + at];
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
    ok = ok && (cudaMalloc((void **)&device_parts, (size_t)(count * (INGEST_PLANES + 1u)) * lane_bytes) == cudaSuccess) &&
         (cudaMemcpy(device_parts, parts, (size_t)(count * (INGEST_PLANES + 1u)) * lane_bytes, cudaMemcpyHostToDevice) ==
          cudaSuccess) &&
         (cudaMalloc((void **)&device_records, (size_t)count * part_bytes) == cudaSuccess) &&
         (cudaMemset(device_records, 0, (size_t)count * part_bytes) == cudaSuccess);
    for (unsigned int plane = 0u; ok && (plane <= INGEST_PLANES); plane += 1u)
    {
        ok = cudaMemcpy2D((unsigned char *)device_records + (plane * lane_bytes), part_bytes,
                          device_parts + (plane * count), lane_bytes, lane_bytes, (size_t)count,
                          cudaMemcpyDeviceToDevice) == cudaSuccess;
    }
    cudaFree(device_parts);
    free(parts);
    unsigned long long least_placed = 0ull;
    unsigned long long most_placed = 0ull;
    ingest_exponents(values, &least_placed, &most_placed);
    const unsigned int shift_bits = exact_record_bits_of(FORMS_LIFT - least_placed) + 1u;
    ExactRecordProgram program;
    exact_record_open(&program);
    unsigned int output = 0u;
    ingest_check_program(&program, column, most_placed, shift_bits, &output);
    const unsigned int members = 3u;
    const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {INGEST_LIMBS_PER_VALUE, part_limbs, INGEST_ROW_LIMBS};
    IngestLoaded loaded;
    EngineError error;
    const int program_loaded = ok && ingest_load(&program, &output, 1u, limbs, members, &loaded, &error);
    ok = program_loaded;
    if (!ok)
    {
        ingest_error_line(results, "the check program did not load", &error);
    }
    // each row's record, its base and its term as the row's crystal holds it, and each value's index triple: its own
    // record, its own parts and its row's record
    unsigned int *const rows = ok ? ingest_rows_build(values, bases) : NULL;
    unsigned int *const index = ok ? ingest_index_build(values, 3u) : NULL;
    ok = ok && (rows != NULL) && (index != NULL);
    for (unsigned long long row = 0ull; ok && !sealed->rows && (row < values->rows); row += 1ull)
    {
        rows[(row * INGEST_ROW_LIMBS) + INGEST_LIMBS_PER_VALUE] = INGEST_ROW_EACH;
    }
    if (ok && sealed->rows)
    {
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        unsigned short *volume = NULL;
        EngineSignum root;
        memset(&error, 0, sizeof(error));
        ok = ingest_sample_name(sample, group, column, "row") &&
             (engine_iapx_load(set, sample, extent, &volume, &root, NULL, &error) == 0L) && (extent[3] == values->rows);
        for (unsigned long long row = 0ull; ok && (row < values->rows); row += 1ull)
        {
            rows[(row * INGEST_ROW_LIMBS) + INGEST_LIMBS_PER_VALUE] = volume[row];
        }
        free(volume);
    }
    const size_t rows_bytes = (size_t)values->rows * INGEST_ROW_LIMBS * sizeof(unsigned int);
    ok = ok && (cudaMalloc((void **)&device_bases, rows_bytes) == cudaSuccess) &&
         (cudaMemcpy(device_bases, rows, rows_bytes, cudaMemcpyHostToDevice) == cudaSuccess) &&
         (cudaMalloc((void **)&device_index, (size_t)(count * 3ull) * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_index, index, (size_t)(count * 3ull) * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
          cudaSuccess);
    free(rows);
    free(index);
    const unsigned long long base_records = values->rows;
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
            ingest_error_line(results, "the check program's run errored", &error);
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
            ingest_error_line(results, "the verdicts' sum errored", &error);
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
        ingest_release(&loaded);
    }
    exact_record_close(&program);
    scriptura_text(&results->line, "   ");
    scriptura_text(&results->line, INGEST_COLUMN_PATH[column]);
    ingest_line_decimal(results, " read back: ", read_back);
    ingest_line_decimal(results, " of ", count);
    ingest_line_decimal(results, " values hold their stored double; kept lanes apart ", kept_apart);
    ingest_line_decimal(results, "; loaded in ", loaded_at - started);
    ingest_line_decimal(results, " us, checked in ", checked_at - loaded_at);
    scriptura_text(&results->line, " us");
    ingest_line_end(results);
    return ok && (read_back == count) && (kept_apart == 0ull);
}

// A text column of one row group: its distinct strings, their bytes one after another from start[d] up to
// start[d + 1], and each row's string as its index among them, INGEST_TEXT_NONE for a row with none
typedef struct
{
    unsigned long long rows;
    unsigned long long distinct;
    unsigned long long raw_bytes;
    unsigned char *bytes;
    unsigned long long *start;
    unsigned int *index;
} IngestText;

#define INGEST_TEXT_NONE 0xFFFFFFFFu

static void ingest_text_release(IngestText *text)
{
    free(text->bytes);
    free(text->start);
    free(text->index);
    memset(text, 0, sizeof(*text));
}

// FNV-1a over a string's bytes, the table's place for it
static unsigned long long ingest_text_hash(const unsigned char *bytes, unsigned long long length)
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
static int ingest_text_read(const char *path, unsigned int group, const char *leaf_path, IngestText *out)
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
         (column.values < INGEST_TEXT_NONE);
    if (ok)
    {
        memset(table, 0xFF, (size_t)slots * sizeof(unsigned int));
    }
    unsigned long long distinct_bytes = 0ull;
    for (unsigned long long row = 0ull; ok && (row < column.rows); row += 1ull)
    {
        out->index[row] = INGEST_TEXT_NONE;
        if (column.row_start[row + 1ull] == column.row_start[row])
        {
            continue;
        }
        const unsigned long long value = column.row_start[row];
        const unsigned char *const bytes = column.value_bytes + column.value_offset[value];
        const unsigned long long length = column.value_length[value];
        out->raw_bytes += length;
        unsigned long long slot = ingest_text_hash(bytes, length) & (slots - 1ull);
        while (table[slot] != INGEST_TEXT_NONE)
        {
            const unsigned long long held = first[table[slot]];
            if ((column.value_length[held] == length) &&
                (memcmp(column.value_bytes + column.value_offset[held], bytes, (size_t)length) == 0))
            {
                break;
            }
            slot = (slot + 1ull) & (slots - 1ull);
        }
        if (table[slot] == INGEST_TEXT_NONE)
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
        ingest_text_release(out);
    }
    return ok;
}

// host lanes sealed as a crystal of the set through the device
static int ingest_seal_host(SimResults *results, const char *set, const char *sample, const unsigned short *lanes,
                            const unsigned long long extent[4], unsigned int column, IngestTally *tally)
{
    const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
    unsigned short *device_lanes = NULL;
    const int ok = (cudaMalloc((void **)&device_lanes, (size_t)count * sizeof(unsigned short)) == cudaSuccess) &&
                   (cudaMemcpy(device_lanes, lanes, (size_t)count * sizeof(unsigned short), cudaMemcpyHostToDevice) ==
                    cudaSuccess) &&
                   ingest_seal(results, set, sample, device_lanes, extent, column, tally);
    cudaFree(device_lanes);
    return ok;
}

// One text column of one row group sealed: its distinct strings' bytes, two to a lane, their lengths, one lane each,
// and each row's index, one past it and 0 for none, as two planes of 16 bits
static int ingest_text_seal(SimResults *results, const char *set, unsigned int group, unsigned int text,
                            const IngestText *read, IngestTally *tally)
{
    const unsigned int column = INGEST_COLUMNS + text;
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
        ok = length < (1ull << INGEST_LANE_BITS);
        lengths[each] = (unsigned short)length;
    }
    for (unsigned long long row = 0ull; ok && (row < read->rows); row += 1ull)
    {
        const unsigned long long named = (read->index[row] == INGEST_TEXT_NONE) ? 0ull : (read->index[row] + 1ull);
        index[row] = (unsigned short)(named & 0xFFFFull);
        index[read->rows + row] = (unsigned short)(named >> INGEST_LANE_BITS);
    }
    char sample[INGEST_SAMPLE_BYTES];
    const unsigned long long byte_extent[4] = {1ull, 1ull, 1ull, (lanes != 0ull) ? lanes : 1ull};
    const unsigned long long length_extent[4] = {1ull, 1ull, 1ull, read->distinct};
    const unsigned long long index_planes = ((read->distinct + 1ull) >> INGEST_LANE_BITS) != 0ull ? 2ull : 1ull;
    const unsigned long long index_extent[4] = {1ull, 1ull, index_planes, read->rows};
    ok = ok && ingest_sample_name(sample, group, column, "text") &&
         ingest_seal_host(results, set, sample, bytes, byte_extent, column, tally) &&
         ingest_sample_name(sample, group, column, "lengths") &&
         ingest_seal_host(results, set, sample, lengths, length_extent, column, tally) &&
         ingest_sample_name(sample, group, column, "index") &&
         ingest_seal_host(results, set, sample, index, index_extent, column, tally);
    tally->raw_bytes[column] += read->raw_bytes;
    tally->forms[column][0] += read->distinct;
    tally->forms[column][1] += read->rows;
    free(bytes);
    free(lengths);
    free(index);
    return ok;
}

// each row's value count as one plane of 16-bit lanes, sealed where every count fits one
static int ingest_rows_seal(SimResults *results, const char *set, unsigned int group, unsigned int column,
                            const IngestColumn *values, IngestTally *tally)
{
    unsigned short *const counts = (unsigned short *)malloc((size_t)values->rows * sizeof(unsigned short) + 2u);
    int ok = counts != NULL;
    for (unsigned long long row = 0ull; ok && (row < values->rows); row += 1ull)
    {
        const unsigned long long held = values->row_start[row + 1ull] - values->row_start[row];
        ok = held < (1ull << INGEST_LANE_BITS);
        counts[row] = (unsigned short)held;
    }
    unsigned short *device_counts = NULL;
    ok = ok && (cudaMalloc((void **)&device_counts, (size_t)values->rows * sizeof(unsigned short)) == cudaSuccess) &&
         (cudaMemcpy(device_counts, counts, (size_t)values->rows * sizeof(unsigned short), cudaMemcpyHostToDevice) ==
          cudaSuccess);
    char sample[INGEST_SAMPLE_BYTES];
    const unsigned long long line[4] = {1ull, 1ull, 1ull, values->rows};
    ok = ok && ingest_sample_name(sample, group, column, "rows") &&
         ingest_seal(results, set, sample, device_counts, line, column, tally);
    cudaFree(device_counts);
    free(counts);
    sim_check(results, ok, "each row's value count seals");
    return ok;
}

// the device bytes the widest row group holds: its values, records, planes and bases, and the tower's and the coder's
// pools for the widest plane
static unsigned long long ingest_declared(void)
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
    const unsigned long long lanes = widest * INGEST_LANES_PER_VALUE;
    return (widest * INGEST_DECLARED_PER_VALUE) + tower_reserve_bytes(lanes) + compression_reserve_bytes(lanes);
}

int ingest_run(SimResults *results, int count, char **arguments)
{
    const char *const path = arguments[2];
    const char *const set = arguments[3];
    const CasmiParquetFooterRead footer = {path, &s_footer};
    if (casmi_parquet_footer_read(&footer) < 0ll)
    {
        scriptura_text(&results->line, "  the parquet footer did not read");
        ingest_line_end(results);
        return 0;
    }
    if (sim_job_submit(results, "casmi_driver", count, arguments, ingest_declared()) == 0)
    {
        return 0;
    }
    IngestTally tally;
    memset(&tally, 0, sizeof(tally));
    const unsigned long long started = engine_clock_microseconds();
    int ok = 1;
    for (unsigned int group = 0u; ok && (group < s_footer.row_group_count); group += 1u)
    {
        ingest_line_decimal(results, "  row group ", group);
        ingest_line_end(results);
        IngestColumn columns[INGEST_COLUMNS];
        memset(columns, 0, sizeof(columns));
        for (unsigned int column = 0u; ok && (column < INGEST_COLUMNS); column += 1u)
        {
            ok = ingest_column_read(path, group, INGEST_COLUMN_PATH[column], &columns[column]);
            sim_check(results, ok, "the column reads as its stored bytes");
        }
        for (unsigned int column = 0u; ok && (column < INGEST_BASES); column += 1u)
        {
            const IngestColumn *const bases = (column == INGEST_INTENSITIES) ? &columns[INGEST_BASES] : NULL;
            IngestSealed sealed;
            ok = ingest_column_forms(results, set, group, column, &columns[column], bases, &tally, &sealed);
            sim_check(results, ok, "the column's forms are read on the device and sealed");
            const int read_back = ok && ingest_column_check(results, set, group, column, &columns[column], bases, &sealed);
            sim_check(results, read_back, "every value read back from the set's crystals is its stored double");
            ok = ok && read_back;
        }
        // the bases as they are stored, a value's lanes as four planes, and which rows hold one
        if (ok && (columns[INGEST_BASES].values != 0ull))
        {
            const IngestColumn *const bases = &columns[INGEST_BASES];
            unsigned short *device_lanes = NULL;
            unsigned short *device_planes = NULL;
            const size_t lane_bytes = sizeof(unsigned short);
            ok = (cudaMalloc((void **)&device_lanes, (size_t)(bases->values * 8ull)) == cudaSuccess) &&
                 (cudaMalloc((void **)&device_planes, (size_t)(bases->values * 8ull)) == cudaSuccess) &&
                 (cudaMemcpy(device_lanes, bases->bytes, (size_t)(bases->values * 8ull), cudaMemcpyHostToDevice) ==
                  cudaSuccess);
            for (unsigned long long plane = 0ull; ok && (plane < INGEST_LANES_PER_VALUE); plane += 1ull)
            {
                ok = cudaMemcpy2D(device_planes + (plane * bases->values), lane_bytes, device_lanes + plane,
                                  (size_t)INGEST_LANES_PER_VALUE * lane_bytes, lane_bytes, (size_t)bases->values,
                                  cudaMemcpyDeviceToDevice) == cudaSuccess;
            }
            char sample[INGEST_SAMPLE_BYTES];
            const unsigned long long extent[4] = {1ull, 1ull, INGEST_LANES_PER_VALUE, bases->values};
            ok = ok && ingest_sample_name(sample, group, INGEST_BASES, "kept") &&
                 ingest_seal(results, set, sample, device_planes, extent, INGEST_BASES, &tally) &&
                 ingest_rows_seal(results, set, group, INGEST_BASES, bases, &tally);
            tally.raw_bytes[INGEST_BASES] += bases->values * 8ull;
            cudaFree(device_lanes);
            cudaFree(device_planes);
        }
        ok = ok && ingest_rows_seal(results, set, group, INGEST_PEAKS, &columns[INGEST_PEAKS], &tally);
        for (unsigned int each = 0u; ok && (each < INGEST_TEXTS); each += 1u)
        {
            IngestText read;
            if (!ingest_text_read(path, group, INGEST_TEXT_PATH[each], &read))
            {
                continue;
            }
            ok = ingest_text_seal(results, set, group, each, &read, &tally);
            sim_check(results, ok, "the text column seals as its distinct strings and each row's index");
            ingest_text_release(&read);
        }
        for (unsigned int column = 0u; column < INGEST_COLUMNS; column += 1u)
        {
            ingest_column_release(&columns[column]);
        }
    }
    const unsigned long long finished = engine_clock_microseconds();
    unsigned long long crystal_total = 0ull;
    unsigned long long raw_total = 0ull;
    for (unsigned int column = 0u; column < INGEST_COLUMNS + INGEST_TEXTS; column += 1u)
    {
        if (tally.raw_bytes[column] == 0ull)
        {
            continue;
        }
        scriptura_text(&results->line, "  ");
        scriptura_text(&results->line, ingest_part_path(column));
        ingest_line_decimal(results, ": crystals ", tally.crystal_bytes[column]);
        ingest_line_decimal(results, " bytes of ", tally.raw_bytes[column]);
        ingest_line_decimal(results, " raw, that is ",
                            (tally.raw_bytes[column] != 0ull)
                                ? ((tally.crystal_bytes[column] * 1000ull) / tally.raw_bytes[column])
                                : 0ull);
        if (column < INGEST_COLUMNS)
        {
            scriptura_text(&results->line, " per mille, floored; forms");
            for (unsigned int form = 0u; form <= INGEST_FORM_KEPT; form += 1u)
            {
                ingest_line_decimal(results, " ", tally.forms[column][form]);
            }
        }
        else
        {
            ingest_line_decimal(results, " per mille, floored; distinct strings ", tally.forms[column][0]);
            ingest_line_decimal(results, " of rows ", tally.forms[column][1]);
        }
        ingest_line_end(results);
        crystal_total += tally.crystal_bytes[column];
        raw_total += tally.raw_bytes[column];
    }
    ingest_line_decimal(results, "  the set: ", tally.crystals);
    ingest_line_decimal(results, " crystals, ", crystal_total);
    ingest_line_decimal(results, " bytes of ", raw_total);
    ingest_line_decimal(results, " raw, that is ", (raw_total != 0ull) ? ((crystal_total * 1000ull) / raw_total) : 0ull);
    ingest_line_decimal(results, " per mille, floored; ingested in ", finished - started);
    scriptura_text(&results->line, " us");
    ingest_line_end(results);
    return ok;
}

