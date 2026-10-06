// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// A parquet file's double columns read as the bytes they are stored as, each byte pair one 16-bit lane: lifted by the
// tower and coded by compression on the device, decoded and lowered, and the lowered lanes held against the stored
// bytes lane for lane. The lowered lanes are then the records a record program reads on the device, two limbs to a
// double, with no host step between: the program reads the mantissa, exponent and sign fields in place and writes
// x = M 2^(E - E_min) on the column's one unit, and the device's records are held word for word against the host
// reference and against x worked out apart from the record machine. Each column is laid two ways, as one line of
// lanes and as rows of a double's four lanes, and the coded bits of each are printed against the raw bits.
// The run is one job on the device's tessera daemon.
#include "../../../../../src/cu/engine/analysis/compression/compression.h"
#include "../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../../../src/cu/engine/analysis/tower/tower.h"
#include "../../../../../src/cu/engine/runtime/radix_keys/radix_keys.h"
#include "../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "../../../../../src/cu/types/integers/exact_integer.h"
#include "../../../../../src/cu/types/integerfloats/edouble/edouble_record.h"
#include "../../../../../src/cu/types/integers/exact_record/exact_record.h"
#include "../parquet/parquet.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdlib.h>
#include <string.h>

#define LANE_CHECK_COLUMNS 3u

#define LANE_CHECK_LAYOUTS 2u

// a double's 64 bits, as IEEE 754 binary64 lays them
#define LANE_CHECK_MANTISSA_BITS 52u
#define LANE_CHECK_EXPONENT_BITS 11u
#define LANE_CHECK_SIGN_OFFSET 63u
#define LANE_CHECK_EXPONENT_ALL 0x7FFull

// the decimal places tried, 0 to 17, each one floor of the places program, and the unit's exponent: on 2^(E - 1077) a
// double is 4M with its preimage's ends whole
#define LANE_CHECK_PLACES 18u
#define LANE_CHECK_PLACES_LIFT 1077ull

// a base B from 1 to 2^53: its biased exponent 1023 to 1075, and the 0 to 52 low mantissa bits a whole B leaves 0
#define LANE_CHECK_BASE_BIASED_LEAST 1023ull
#define LANE_CHECK_BASE_BIASED_MOST 1075ull
#define LANE_CHECK_BASE_DROP_BITS 7u

// a plane's lanes as a tower edge's index: every 16-bit lane is one entry
#define LANE_CHECK_EDGE_BITS 16u
#define LANE_CHECK_EDGE_ENTRIES (1u << LANE_CHECK_EDGE_BITS)

// lanes and limbs of one double
#define LANE_CHECK_LANES_PER_VALUE 4ull
#define LANE_CHECK_LIMBS_PER_VALUE 2u

// the limbs Σx is held in: x is below 2^(53 + 2^11) at the very most, and the count of values is below 2^40
#define LANE_CHECK_SUM_LIMBS 72u

static const char *const LANE_CHECK_COLUMN_PATH[LANE_CHECK_COLUMNS] = {
    "precursor_mz", "ms2_mzs.list.element", "ms2_normalized_intensities.list.element"};

static const char *const LANE_CHECK_LAYOUT_NAME[LANE_CHECK_LAYOUTS] = {"one line of lanes", "rows of four lanes"};

static CasmiParquetFooter s_footer;

// the leading records the host reference runs and is held against, 0 for every one
static unsigned long long s_host_lanes = 0ull;

// the column run, LANE_CHECK_COLUMNS for every one
static unsigned int s_column = LANE_CHECK_COLUMNS;

// a column's values, and row r's values from row_start[r] up to row_start[r + 1]
typedef struct
{
    unsigned long long values;
    unsigned char *bytes;
    unsigned long long rows;
    unsigned long long *row_start;
} LaneColumn;

typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} LaneLoaded;

static void lane_line_decimal(SimResults *results, const char *before, unsigned long long value)
{
    scriptura_text(&results->line, before);
    scriptura_decimal(&results->line, value, 1u);
}

static void lane_line_end(SimResults *results)
{
    scriptura_character(&results->line, '\n');
    sim_flush(results);
}

static void lane_error_line(SimResults *results, const char *what, const EngineError *error)
{
    scriptura_text(&results->line, "  ");
    scriptura_text(&results->line, what);
    lane_line_decimal(results, ": module ", (unsigned long long)error->module);
    lane_line_decimal(results, ", site ", (unsigned long long)error->site);
    lane_line_decimal(results, ", kind ", (unsigned long long)error->kind);
    lane_line_end(results);
}

// every value of one double column over every row group, as the stored bytes, eight to a value
static int lane_column_read(const char *path, const char *leaf_path, LaneColumn *out)
{
    memset(out, 0, sizeof(*out));
    const CasmiParquetLeafFind find = {&s_footer, leaf_path};
    const long long leaf = casmi_parquet_leaf_find(&find);
    if ((leaf < 0ll) || (s_footer.leaf[leaf].physical != CASMI_PARQUET_DOUBLE))
    {
        return 0;
    }
    unsigned long long room = 0ull;
    for (unsigned int group = 0u; group < s_footer.row_group_count; group += 1u)
    {
        room += s_footer.row_group[group].chunk[leaf].level_count;
    }
    out->bytes = (unsigned char *)malloc((size_t)(room * 8ull) + 8u);
    out->row_start = (unsigned long long *)malloc((size_t)(room * 8ull) + 8u);
    int ok = (out->bytes != NULL) && (out->row_start != NULL);
    for (unsigned int group = 0u; ok && (group < s_footer.row_group_count); group += 1u)
    {
        CasmiParquetColumn column;
        memset(&column, 0, sizeof(column));
        // the leaf is below CASMI_PARQUET_LEAVES_MOST
        const CasmiParquetColumnRead read = {path, &s_footer, group, (unsigned int)leaf, &column};
        ok = casmi_parquet_column_bound(&read) >= 0ll;
        column.row_start = ok ? (unsigned long long *)malloc((size_t)(column.row_start_room * 8ull) + 8u) : NULL;
        column.value_bytes = ok ? (unsigned char *)malloc((size_t)column.value_bytes_room + 8u) : NULL;
        column.value_offset = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
        column.value_length = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
        column.scratch = ok ? (unsigned char *)malloc((size_t)column.scratch_room + 8u) : NULL;
        ok = ok && (column.row_start != NULL) && (column.value_bytes != NULL) && (column.value_offset != NULL) &&
             (column.value_length != NULL) && (column.scratch != NULL) && (casmi_parquet_column_read(&read) >= 0ll) &&
             ((out->values + column.values) <= room) && ((out->rows + column.rows) <= room);
        if (ok)
        {
            memcpy(out->bytes + (out->values * 8ull), column.value_bytes, (size_t)(column.values * 8ull));
            for (unsigned long long row = 0ull; row < column.rows; row += 1ull)
            {
                out->row_start[out->rows + row] = out->values + column.row_start[row];
            }
            out->rows += column.rows;
            out->values += column.values;
        }
        free(column.row_start);
        free(column.value_bytes);
        free(column.value_offset);
        free(column.value_length);
        free(column.scratch);
    }
    if (ok)
    {
        out->row_start[out->rows] = out->values;
    }
    return ok;
}

// lifted and coded, then decoded and lowered: the coded bits, and the lanes where the lowered lattice and a host copy
// of it differ from the stored lanes
static int lane_round_trip(SimResults *results, const unsigned short *device_lanes, const unsigned short *host_lanes,
                           const unsigned long long extent[4], unsigned long long *bits,
                           const unsigned short **device_rebuilt, const TowerEdge *edges, unsigned int edge_count,
                           int quiet)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    const unsigned long long lanes = extent[0] * extent[1] * extent[2] * extent[3];
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    unsigned int floors = 0u;
    TowerLiftRequest lift;
    memset(&lift, 0, sizeof(lift));
    lift.device_lanes = device_lanes;
    memcpy(lift.extent, extent, sizeof(lift.extent));
    lift.coefficients = &coefficients;
    lift.scratch = &scratch;
    lift.floors = &floors;
    lift.edges = edges;
    lift.edge_count = edge_count;
    lift.error = &error;
    EngineStream stream;
    memset(&stream, 0, sizeof(stream));
    memcpy(stream.extent, extent, sizeof(stream.extent));
    CompressionEncodeRequest code;
    memset(&code, 0, sizeof(code));
    code.count = lanes;
    code.chunks = &stream.chunks;
    code.bits = &stream.bits;
    code.offsets = &stream.offsets;
    code.stream = &stream.stream;
    code.error = &error;
    const unsigned long long lift_start = engine_clock_microseconds();
    int ok = tower_lift(&lift) == 0L;
    code.device_coefficients = coefficients;
    code.device_scratch = scratch;
    ok = ok && (compression_encode(&code) == 0L);
    const unsigned long long coded = engine_clock_microseconds();
    if (!ok)
    {
        lane_error_line(results, "lift and code", &error);
        return 0;
    }
    int *decoded = NULL;
    ok = tower_capacity(extent, &decoded, &error) == 0L;
    CompressionDecodeRequest decode;
    memset(&decode, 0, sizeof(decode));
    decode.offsets = stream.offsets;
    decode.chunks = stream.chunks;
    decode.stream = stream.stream;
    decode.bits = stream.bits;
    decode.count = lanes;
    decode.device_coefficients = decoded;
    decode.error = &error;
    ok = ok && (compression_decode(&decode) == 0L);
    unsigned short *const rebuilt = (unsigned short *)malloc((size_t)(lanes * 2ull) + 2u);
    unsigned long long mismatches = ~0ull;
    TowerLowerRequest lower;
    memset(&lower, 0, sizeof(lower));
    lower.device_lanes = device_lanes;
    memcpy(lower.extent, extent, sizeof(lower.extent));
    lower.mismatches = &mismatches;
    lower.device_rebuilt = device_rebuilt;
    lower.rebuilt = rebuilt;
    lower.edges = edges;
    lower.edge_count = edge_count;
    lower.error = &error;
    ok = ok && (rebuilt != NULL) && (tower_lower(&lower) == 0L);
    const unsigned long long lowered = engine_clock_microseconds();
    if (!ok)
    {
        lane_error_line(results, "decode and lower", &error);
        free(rebuilt);
        return 0;
    }
    const int same = memcmp(rebuilt, host_lanes, (size_t)(lanes * 2ull)) == 0;
    free(rebuilt);
    *bits = stream.bits;
    sim_check(results, (mismatches == 0ull) && same, "the lowered lanes are the stored lanes");
    if (quiet != 0)
    {
        return (mismatches == 0ull) && same;
    }
    lane_line_decimal(results, "    floors ", (unsigned long long)floors);
    lane_line_decimal(results, ", chunks ", stream.chunks);
    lane_line_decimal(results, ", coded bits ", stream.bits);
    lane_line_decimal(results, " of raw ", lanes * 16ull);
    lane_line_decimal(results, ", that is ", (stream.bits * 1000ull) / (lanes * 16ull));
    scriptura_text(&results->line, " per mille, floored");
    lane_line_decimal(results, "; lift and code ", coded - lift_start);
    lane_line_decimal(results, " us, decode and lower ", lowered - coded);
    lane_line_decimal(results, " us; lanes the device's lowering counts unlike the stored ", mismatches);
    scriptura_text(&results->line, same ? "; the host copy equals the stored bytes" : "; the host copy DIFFERS");
    lane_line_end(results);
    return 1;
}

static int lane_load(ExactRecordProgram *program, const unsigned int *outputs, unsigned int output_count,
                     unsigned int in_limbs, unsigned int members, LaneLoaded *loaded, EngineError *error)
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
    // every member's records are doubles, `in_limbs` limbs each
    const unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX] = {in_limbs, (members > 1u) ? in_limbs : 0u,
                                                           (members > 2u) ? in_limbs : 0u};
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

static void lane_release(LaneLoaded *loaded)
{
    cycle_record_release(loaded->record);
    key_schedule_record_release(&loaded->layout);
    keymath_record_release(&loaded->key);
}

// the least and the most biased exponent over the column's nonzero finite values, read from the bits, and how many
// values are zero and how many are not finite
static void lane_exponents(const LaneColumn *column, unsigned long long *least, unsigned long long *most,
                           unsigned long long *zero, unsigned long long *unfinite)
{
    *least = LANE_CHECK_EXPONENT_ALL;
    *most = 0ull;
    *zero = 0ull;
    *unfinite = 0ull;
    for (unsigned long long value = 0ull; value < column->values; value += 1ull)
    {
        unsigned long long word = 0ull;
        memcpy(&word, column->bytes + (value * 8ull), 8u);
        const unsigned long long biased = (word >> LANE_CHECK_MANTISSA_BITS) & LANE_CHECK_EXPONENT_ALL;
        const unsigned long long fraction = word & ((1ull << LANE_CHECK_MANTISSA_BITS) - 1ull);
        if (biased == LANE_CHECK_EXPONENT_ALL)
        {
            *unfinite += 1ull;
            continue;
        }
        if ((biased == 0ull) && (fraction == 0ull))
        {
            *zero += 1ull;
            continue;
        }
        // a subnormal's place is that of biased exponent 1
        const unsigned long long placed = (biased == 0ull) ? 1ull : biased;
        *least = (placed < *least) ? placed : *least;
        *most = (placed > *most) ? placed : *most;
    }
}

// what the column holds, read and never summarized: its distinct values as 64-bit words, and for each of its four
// planes the distinct 16-bit lanes and how many times the most common one occurs
static int lane_reading(SimResults *results, const LaneColumn *column)
{
    const unsigned long long values = column->values;
    unsigned long long *const words = (unsigned long long *)malloc((size_t)(values * 8ull) + 8u);
    unsigned long long *const tally = (unsigned long long *)calloc(65536u, sizeof(unsigned long long));
    if ((words == NULL) || (tally == NULL) || (values == 0ull))
    {
        free(words);
        free(tally);
        return 0;
    }
    memcpy(words, column->bytes, (size_t)(values * 8ull));
    const int sorted = radix_sort_keys(words, (size_t)values);
    unsigned long long distinct = (sorted != 0) ? 1ull : 0ull;
    unsigned long long run = 1ull;
    unsigned long long longest = 1ull;
    for (unsigned long long at = 1ull; (sorted != 0) && (at < values); at += 1ull)
    {
        run = (words[at] == words[at - 1ull]) ? (run + 1ull) : 1ull;
        distinct += (words[at] != words[at - 1ull]) ? 1ull : 0ull;
        longest = (run > longest) ? run : longest;
    }
    lane_line_decimal(results, "    distinct values ", distinct);
    lane_line_decimal(results, " of ", values);
    lane_line_decimal(results, "; the most common value occurs ", longest);
    scriptura_text(&results->line, " times");
    lane_line_end(results);
    const unsigned short *const lanes = (const unsigned short *)column->bytes;
    for (unsigned long long plane = 0ull; plane < LANE_CHECK_LANES_PER_VALUE; plane += 1ull)
    {
        memset(tally, 0, 65536u * sizeof(unsigned long long));
        for (unsigned long long value = 0ull; value < values; value += 1ull)
        {
            tally[lanes[(value * LANE_CHECK_LANES_PER_VALUE) + plane]] += 1ull;
        }
        unsigned long long held = 0ull;
        unsigned long long most = 0ull;
        for (unsigned int lane = 0u; lane < 65536u; lane += 1u)
        {
            held += (tally[lane] != 0ull) ? 1ull : 0ull;
            most = (tally[lane] > most) ? tally[lane] : most;
        }
        lane_line_decimal(results, "    plane ", plane);
        lane_line_decimal(results, ": distinct lanes ", held);
        lane_line_decimal(results, " of 65536; the most common occurs ", most);
        scriptura_text(&results->line, " times");
        lane_line_end(results);
    }
    free(words);
    free(tally);
    return sorted;
}

// a plane's lanes ranked by how often each occurs, the most common first and a tie in lane order: forward[lane] is its
// rank, a permutation of the 2^16 lanes, every lane the plane never holds ranked after every one it does
static int lane_rank_edge(const unsigned short *plane, unsigned long long values, unsigned int *forward)
{
    unsigned long long *const keys = (unsigned long long *)calloc(LANE_CHECK_EDGE_ENTRIES, sizeof(unsigned long long));
    if (keys == NULL)
    {
        return 0;
    }
    for (unsigned long long value = 0ull; value < values; value += 1ull)
    {
        keys[plane[value]] += 1ull;
    }
    // the key: the count's complement below 2^40 above the lane; the order is the most common first, then the lane
    for (unsigned int lane = 0u; lane < LANE_CHECK_EDGE_ENTRIES; lane += 1u)
    {
        keys[lane] = ((((1ull << 40u) - 1ull) - keys[lane]) << LANE_CHECK_EDGE_BITS) | (unsigned long long)lane;
    }
    const int sorted = radix_sort_keys(keys, LANE_CHECK_EDGE_ENTRIES);
    for (unsigned int rank = 0u; (sorted != 0) && (rank < LANE_CHECK_EDGE_ENTRIES); rank += 1u)
    {
        forward[keys[rank] & (LANE_CHECK_EDGE_ENTRIES - 1u)] = rank;
    }
    free(keys);
    return sorted;
}

// x = (-1)^s M 2^(E - E_min), from the fields of the record in place: M the fraction with the hidden bit where the
// exponent is not 0, E the biased exponent, or 1 for a subnormal; a zero is x = 0 whatever its exponent reads
static unsigned int lane_program(ExactRecordProgram *program, unsigned long long least, unsigned int shift_bits)
{
    const unsigned int fraction_field = exact_record_member_field(program, LANE_CHECK_MANTISSA_BITS, 0u);
    const unsigned int exponent_field =
        exact_record_member_field(program, LANE_CHECK_EXPONENT_BITS, LANE_CHECK_MANTISSA_BITS);
    const unsigned int sign_field = exact_record_member_field(program, 1u, LANE_CHECK_SIGN_OFFSET);
    const unsigned int fraction = exact_record_read_unsigned(program, fraction_field, 0u);
    const unsigned int biased = exact_record_read_unsigned(program, exponent_field, 0u);
    const unsigned int sign = exact_record_read_unsigned(program, sign_field, 0u);
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int normal = exact_record_above(program, biased, zero);
    const unsigned int mantissa = exact_record_sum(
        program, fraction, exact_record_product(program, normal, exact_record_power_two(program, LANE_CHECK_MANTISSA_BITS)));
    const unsigned int placed = exact_record_sum(program, biased, exact_record_difference(program, one, normal));
    const unsigned int held = exact_record_above(program, mantissa, zero);
    const unsigned int shift = exact_record_product(
        program, held, exact_record_difference(program, placed, exact_record_constant(program, least)));
    const unsigned int lifted = exact_record_product(program, mantissa, exact_record_two_to(program, shift, shift_bits));
    const unsigned int signed_one =
        exact_record_difference(program, one, exact_record_product(program, exact_record_constant(program, 2ull), sign));
    return exact_record_product(program, lifted, signed_one);
}

static void lane_field(const unsigned int *record, unsigned int offset, unsigned int bits, AnchorExactInteger *value);

// the bits of an exact integer's magnitude, 0 for 0
static unsigned long long lane_bits(const AnchorExactInteger *value)
{
    for (unsigned int limb = ANCHOR_EXACT_LIMBS; limb > 0u; limb -= 1u)
    {
        if (value->limb[limb - 1u] != 0u)
        {
            return (32ull * (limb - 1u)) + exact_record_bits_of(value->limb[limb - 1u]);
        }
    }
    return 0ull;
}

// a double's preimage on the unit 2^(E - 1077): its ends `low` and `high`, `odd_low` and `odd_high` 1 where each end is
// open, and `divisor` 2^(1077 - E); a real r rounds to the double exactly where r 2^(1077 - E) lies between the ends
typedef struct
{
    unsigned int low;
    unsigned int high;
    unsigned int odd_low;
    unsigned int odd_high;
    unsigned int divisor;
} LanePreimage;

// a double's mantissa M, its exponent as placed, and its preimage's lower gap: 2 quarters of its last place, 1 where M
// is 2^52 above the least normal exponent
static void lane_double_read(ExactRecordProgram *program, unsigned int fraction, unsigned int biased,
                             unsigned int *mantissa, unsigned int *placed, unsigned int *gap)
{
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int two = exact_record_constant(program, 2ull);
    const unsigned int normal = exact_record_above(program, biased, zero);
    *mantissa = exact_record_sum(
        program, fraction, exact_record_product(program, normal, exact_record_power_two(program, LANE_CHECK_MANTISSA_BITS)));
    *placed = exact_record_sum(program, biased, exact_record_difference(program, one, normal));
    const unsigned int narrow =
        exact_record_product(program, exact_record_equal(program, fraction, zero), exact_record_above(program, biased, one));
    *gap = exact_record_difference(program, two, narrow);
}

// 1 where an integer is odd: v - 2 floor(v / 2)
static unsigned int lane_odd(ExactRecordProgram *program, unsigned int value)
{
    const unsigned int two = exact_record_constant(program, 2ull);
    return exact_record_difference(program, value,
                                   exact_record_product(program, two, exact_record_quotient(program, value, two)));
}

static void lane_preimage(ExactRecordProgram *program, unsigned int fraction, unsigned int biased,
                          unsigned long long most_placed, unsigned int shift_bits, LanePreimage *preimage)
{
    const unsigned int two = exact_record_constant(program, 2ull);
    const unsigned int four = exact_record_constant(program, 4ull);
    unsigned int mantissa = 0u;
    unsigned int placed = 0u;
    unsigned int gap = 0u;
    lane_double_read(program, fraction, biased, &mantissa, &placed, &gap);
    const unsigned int centre = exact_record_product(program, four, mantissa);
    preimage->low = exact_record_difference(program, centre, gap);
    preimage->high = exact_record_sum(program, centre, two);
    // closed where M is even
    preimage->odd_low = lane_odd(program, mantissa);
    preimage->odd_high = preimage->odd_low;
    const unsigned int shift = exact_record_difference(program, exact_record_constant(program, most_placed), placed);
    preimage->divisor = exact_record_two_to(
        program, exact_record_sum(program, shift, exact_record_constant(program, LANE_CHECK_PLACES_LIFT - most_placed)),
        shift_bits);
}

// the integers k with k / divisor inside the preimage scaled by `scale`: the least of them, and 1 where there is one
static void lane_preimage_integer(ExactRecordProgram *program, const LanePreimage *preimage, unsigned int scale,
                                  unsigned int *least, unsigned int *holds)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    unsigned int low_floor = 0u;
    unsigned int low_ceiling = 0u;
    unsigned int high_floor = 0u;
    unsigned int high_ceiling = 0u;
    edouble_record_floor_ceiling(program, exact_record_product(program, scale, preimage->low), preimage->divisor,
                                 &low_floor, &low_ceiling);
    edouble_record_floor_ceiling(program, exact_record_product(program, scale, preimage->high), preimage->divisor,
                                 &high_floor, &high_ceiling);
    // closed: ceil(low) to floor(high); open: floor(low) + 1 to ceil(high) - 1
    *least = exact_record_select(program, preimage->odd_low, exact_record_sum(program, low_floor, one), low_ceiling);
    const unsigned int most =
        exact_record_select(program, preimage->odd_high, exact_record_difference(program, high_ceiling, one), high_floor);
    *holds = exact_record_difference(program, one, exact_record_above(program, *least, most));
}

// The least decimal places p at which an integer k has k 10^-p inside the preimage, and the least such k. Each p is one
// floor; `least` counts the floors before the first that holds, LANE_CHECK_PLACES where none does
static void lane_places_search(ExactRecordProgram *program, const LanePreimage *preimage, unsigned int *least,
                               unsigned int *chosen)
{
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    unsigned int none = one;
    unsigned int counted = zero;
    unsigned int picked = zero;
    unsigned long long power = 1ull;
    for (unsigned int place = 0u; place < LANE_CHECK_PLACES; place += 1u)
    {
        unsigned int k_least = 0u;
        unsigned int holds = 0u;
        lane_preimage_integer(program, preimage, exact_record_constant(program, power), &k_least, &holds);
        const unsigned int first = exact_record_product(program, none, holds);
        picked = exact_record_sum(program, picked, exact_record_product(program, first, k_least));
        none = exact_record_product(program, none, exact_record_difference(program, one, holds));
        counted = exact_record_sum(program, counted, none);
        power *= 10ull;
    }
    *least = counted;
    *chosen = picked;
}

// The places program: the reals rounding to nearest, ties to even, store as this double. On the unit u = 2^(E - 1077)
// the double is 4M, its preimage runs from 4M - 2 (4M - 1 where M is 2^52 above the least exponent) to 4M + 2, closed
// where M is even and open where it is odd; k 10^-p lies in it exactly where k 2^s lies between 10^p times its ends,
// s = 1077 - E
static void lane_places_program(ExactRecordProgram *program, unsigned long long most_placed, unsigned int shift_bits,
                                unsigned int *least, unsigned int *chosen)
{
    const unsigned int fraction_field = exact_record_member_field(program, LANE_CHECK_MANTISSA_BITS, 0u);
    const unsigned int exponent_field =
        exact_record_member_field(program, LANE_CHECK_EXPONENT_BITS, LANE_CHECK_MANTISSA_BITS);
    const unsigned int fraction = exact_record_read_unsigned(program, fraction_field, 0u);
    const unsigned int biased = exact_record_read_unsigned(program, exponent_field, 0u);
    LanePreimage preimage;
    lane_preimage(program, fraction, biased, most_placed, shift_bits, &preimage);
    lane_places_search(program, &preimage, least, chosen);
}

// The double nearest an end of an interval, on the end's side: the least double at or above `value` where `above` is
// 1, the greatest at or below it where `above` is 0, strictly past it where `open` is 1. `value` is an integer of
// `bits_least` to `bits_least` + 2 bits on some unit, and the double comes out as M 2^s on that unit, M from 2^52 to
// 2^53 - 1
static void lane_double_at(ExactRecordProgram *program, unsigned int value, unsigned int bits_least, int above,
                           unsigned int open, unsigned int *mantissa, unsigned int *shift)
{
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int two = exact_record_constant(program, 2ull);
    const unsigned int least_mantissa = exact_record_power_two(program, LANE_CHECK_MANTISSA_BITS);
    const unsigned int width = LANE_CHECK_MANTISSA_BITS + 1u;
    // s = bits(value) - 53
    unsigned int s = exact_record_constant(program, (unsigned long long)(bits_least - width));
    for (unsigned int more = 0u; more < 2u; more += 1u)
    {
        const unsigned int edge =
            exact_record_difference(program, exact_record_power_two(program, bits_least + more), one);
        s = exact_record_sum(program, s, exact_record_above(program, value, edge));
    }
    unsigned int floor = 0u;
    unsigned int ceiling = 0u;
    edouble_record_floor_ceiling(program, value, exact_record_two_to(program, s, LANE_CHECK_BASE_DROP_BITS), &floor,
                                 &ceiling);
    if (above)
    {
        // closed: ceil(v / 2^s); open: floor(v / 2^s) + 1; 2^53 carries into the next binade
        const unsigned int raw = exact_record_select(program, open, exact_record_sum(program, floor, one), ceiling);
        const unsigned int over = exact_record_equal(program, raw, exact_record_power_two(program, width));
        *mantissa = exact_record_select(program, over, least_mantissa, raw);
        *shift = exact_record_sum(program, s, over);
    }
    else
    {
        // closed: floor(v / 2^s); open: ceil(v / 2^s) - 1; below 2^52 it falls into the binade under
        const unsigned int raw =
            exact_record_select(program, open, exact_record_difference(program, ceiling, one), floor);
        const unsigned int under = exact_record_difference(program, one, exact_record_above(program, raw,
                                                                  exact_record_difference(program, least_mantissa, one)));
        *mantissa = exact_record_select(program, under,
                                        exact_record_sum(program, exact_record_product(program, two, raw), one), raw);
        *shift = exact_record_difference(program, s, under);
    }
}

// The divided program: x = fl(fl(d) / B), d a decimal k 10^-p and B the constant `base`. The doubles D with
// fl(D / B) = x are those in B times x's preimage, and the reals rounding to one of them run from the lower end of the
// least one's preimage to the upper end of the greatest one's, each end closed where its D's mantissa is even. On the
// unit u / 4 those ends are (4 M_least - gap) 2^s_least and (4 M_most + 2) 2^s_most, and the places search runs on them.
// `exists` is 1 where some double D divides to x
static void lane_divided_program(ExactRecordProgram *program, unsigned long long base, unsigned long long most_placed,
                                 unsigned int shift_bits, unsigned int *least, unsigned int *chosen, unsigned int *exists)
{
    const unsigned int fraction_field = exact_record_member_field(program, LANE_CHECK_MANTISSA_BITS, 0u);
    const unsigned int exponent_field =
        exact_record_member_field(program, LANE_CHECK_EXPONENT_BITS, LANE_CHECK_MANTISSA_BITS);
    const unsigned int fraction = exact_record_read_unsigned(program, fraction_field, 0u);
    const unsigned int biased = exact_record_read_unsigned(program, exponent_field, 0u);
    const unsigned int one = exact_record_constant(program, 1ull);
    const unsigned int two = exact_record_constant(program, 2ull);
    const unsigned int four = exact_record_constant(program, 4ull);
    LanePreimage x;
    lane_preimage(program, fraction, biased, most_placed, shift_bits, &x);
    const unsigned int scale = exact_record_constant(program, base);
    // B x's preimage ends on u: from B (2^54 - 2) to B (2^55 + 2), bits(B) + 53 to bits(B) + 55 bits
    const unsigned int base_bits = exact_record_bits_of(base);
    unsigned int least_mantissa = 0u;
    unsigned int least_shift = 0u;
    unsigned int most_mantissa = 0u;
    unsigned int most_shift = 0u;
    lane_double_at(program, exact_record_product(program, scale, x.low), base_bits + LANE_CHECK_MANTISSA_BITS + 1u, 1,
                   x.odd_low, &least_mantissa, &least_shift);
    lane_double_at(program, exact_record_product(program, scale, x.high), base_bits + LANE_CHECK_MANTISSA_BITS + 1u, 0,
                   x.odd_high, &most_mantissa, &most_shift);
    const unsigned int least_two = exact_record_two_to(program, least_shift, LANE_CHECK_BASE_DROP_BITS);
    const unsigned int most_two = exact_record_two_to(program, most_shift, LANE_CHECK_BASE_DROP_BITS);
    *exists = exact_record_difference(program, one,
                                      exact_record_above(program, exact_record_product(program, least_mantissa, least_two),
                                                         exact_record_product(program, most_mantissa, most_two)));
    const unsigned int narrow = exact_record_equal(program, least_mantissa, exact_record_power_two(program,
                                                                                                   LANE_CHECK_MANTISSA_BITS));
    LanePreimage decimal;
    decimal.low = exact_record_product(
        program,
        exact_record_difference(program, exact_record_product(program, four, least_mantissa),
                                exact_record_difference(program, two, narrow)),
        least_two);
    decimal.high = exact_record_product(
        program, exact_record_sum(program, exact_record_product(program, four, most_mantissa), two), most_two);
    decimal.odd_low = lane_odd(program, least_mantissa);
    decimal.odd_high = lane_odd(program, most_mantissa);
    decimal.divisor = exact_record_product(program, four, x.divisor);
    unsigned int counted = 0u;
    unsigned int picked = 0u;
    lane_places_search(program, &decimal, &counted, &picked);
    // where no D exists the places read as none
    *least = exact_record_select(program, *exists, counted, exact_record_constant(program, LANE_CHECK_PLACES));
    *chosen = exact_record_product(program, *exists, picked);
}

// each value's least places, LANE_CHECK_PLACES where none holds, and its k, into `places_of` and `k_of`: of the value
// itself where `base` is 0, and of the decimal the value is that decimal over `base` where it is not. A k past 64 bits
// reads as held by none
static int lane_places(SimResults *results, const LaneColumn *column, const unsigned short *device_rebuilt,
                       unsigned long long base, unsigned char *places_of, unsigned long long *k_of)
{
    unsigned long long least_placed = 0ull;
    unsigned long long most_placed = 0ull;
    unsigned long long zero = 0ull;
    unsigned long long unfinite = 0ull;
    lane_exponents(column, &least_placed, &most_placed, &zero, &unfinite);
    if ((unfinite != 0ull) || (zero != 0ull) || (most_placed >= LANE_CHECK_PLACES_LIFT))
    {
        sim_check(results, 0, "the column holds no zero, no value past 2^53 and none that is not finite");
        return 0;
    }
    const unsigned int shift_bits = exact_record_bits_of(LANE_CHECK_PLACES_LIFT - least_placed) + 1u;
    ExactRecordProgram program;
    exact_record_open(&program);
    unsigned int outputs[2] = {0u, 0u};
    unsigned int exists = 0u;
    if (base == 0ull)
    {
        lane_places_program(&program, most_placed, shift_bits, &outputs[0], &outputs[1]);
    }
    else
    {
        lane_divided_program(&program, base, most_placed, shift_bits, &outputs[0], &outputs[1], &exists);
    }
    LaneLoaded loaded;
    EngineError error;
    if (lane_load(&program, outputs, 2u, LANE_CHECK_LIMBS_PER_VALUE, 1u, &loaded, &error) == 0)
    {
        lane_error_line(results, "the places program did not load", &error);
        exact_record_close(&program);
        return 0;
    }
    const unsigned int out_limbs = loaded.layout.out_limbs;
    const DeviceRecordStep least_place = loaded.layout.step_table[outputs[0]];
    const DeviceRecordStep k_place = loaded.layout.step_table[outputs[1]];
    const unsigned long long values = column->values;
    const unsigned long long checked = ((s_host_lanes != 0ull) && (s_host_lanes < values)) ? s_host_lanes : values;
    const size_t words = (size_t)(values * out_limbs);
    unsigned int *device_out = NULL;
    unsigned int *const device = (unsigned int *)malloc(words * sizeof(unsigned int) + 4u);
    unsigned int *const host = (unsigned int *)malloc((size_t)(checked * out_limbs) * sizeof(unsigned int) + 4u);
    int ok = (device != NULL) && (host != NULL) &&
             (cudaMalloc((void **)&device_out, words * sizeof(unsigned int)) == cudaSuccess);
    memset(&error, 0, sizeof(error));
    const unsigned long long run_start = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordRunRequest run = {loaded.record, {(const unsigned int *)device_rebuilt, NULL, NULL},
                                           {values, 0ull, 0ull}, NULL, values, device_out, &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
    }
    const unsigned long long run_end = engine_clock_microseconds();
    ok = ok && (cudaMemcpy(device, device_out, words * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess);
    if (ok)
    {
        const CycleRecordHostRequest run = {&loaded.layout, {(const unsigned int *)column->bytes, NULL, NULL},
                                            {values, 0ull, 0ull}, NULL, checked, host, &error, 0ull};
        ok = cycle_record_run_host(&run) != CYCLE_ERROR;
    }
    if (!ok)
    {
        lane_error_line(results, "the places program's run errored", &error);
    }
    const int same = ok && (memcmp(device, host, (size_t)(checked * out_limbs) * sizeof(unsigned int)) == 0);
    // the readings: the least places the column needs over every value, the values no p below LANE_CHECK_PLACES holds,
    // the bits of k summed, and the bits of k at the column's most places, the place every value can be read at
    unsigned long long most = 0ull;
    unsigned long long unheld = 0ull;
    unsigned long long k_bits = 0ull;
    unsigned long long apart = 0ull;
    unsigned long long at_places[LANE_CHECK_PLACES + 1u];
    memset(at_places, 0, sizeof(at_places));
    unsigned long long wide = 0ull;
    for (unsigned long long value = 0ull; ok && (value < values); value += 1ull)
    {
        AnchorExactInteger read;
        lane_field(&device[value * out_limbs], k_place.out_offset, k_place.out_bits, &read);
        const unsigned long long k = ((unsigned long long)read.limb[1] << 32u) | read.limb[0];
        const int narrow_k = lane_bits(&read) <= 64ull;
        wide += narrow_k ? 0ull : 1ull;
        lane_field(&device[value * out_limbs], least_place.out_offset, least_place.out_bits, &read);
        const unsigned long long places = narrow_k ? read.limb[0] : LANE_CHECK_PLACES;
        most = ((places < LANE_CHECK_PLACES) && (places > most)) ? places : most;
        unheld += (places >= LANE_CHECK_PLACES) ? 1ull : 0ull;
        at_places[(places < LANE_CHECK_PLACES) ? places : LANE_CHECK_PLACES] += 1ull;
        places_of[value] = (unsigned char)((places < LANE_CHECK_PLACES) ? places : LANE_CHECK_PLACES);
        k_of[value] = (places < LANE_CHECK_PLACES) ? k : 0ull;
        // strtod, correctly rounded, reads "k e-p" back, and over the base binary64 divides it, to the stored bits
        if (places < LANE_CHECK_PLACES)
        {
            k_bits += exact_record_bits_of(k);
            char text[64];
            snprintf(text, sizeof(text), "%llue-%llu", k, places);
            double read_back = strtod(text, NULL);
            read_back = (base == 0ull) ? read_back : (read_back / (double)base);
            apart += (memcmp(&read_back, column->bytes + (value * 8ull), 8u) == 0) ? 0ull : 1ull;
        }
    }
    lane_line_decimal(results, "    places over ", (base == 0ull) ? 1ull : base);
    lane_line_decimal(results, ": ", program.count);
    lane_line_decimal(results, " steps, record ", (unsigned long long)loaded.layout.out_bits);
    lane_line_decimal(results, " bits; device ", run_end - run_start);
    lane_line_decimal(results, " us over ", values);
    scriptura_text(&results->line, " lanes; device and host records ");
    scriptura_text(&results->line, same ? "equal" : "DIFFER");
    lane_line_decimal(results, " over the leading ", checked);
    lane_line_end(results);
    lane_line_decimal(results, "    the most places any value needs ", most);
    lane_line_decimal(results, "; values no place below ", (unsigned long long)LANE_CHECK_PLACES);
    lane_line_decimal(results, " holds ", unheld);
    lane_line_decimal(results, "; bits of k over the column ", k_bits);
    lane_line_decimal(results, " of raw ", values * 64ull);
    lane_line_decimal(results, ", that is ", (k_bits * 1000ull) / (values * 64ull));
    scriptura_text(&results->line, " per mille, floored");
    lane_line_decimal(results, "; strtod of k e-p unlike the stored bits ", apart);
    lane_line_end(results);
    scriptura_text(&results->line, "    values at each least places, 0 up, and none:");
    for (unsigned int place = 0u; place <= LANE_CHECK_PLACES; place += 1u)
    {
        lane_line_decimal(results, " ", at_places[place]);
    }
    lane_line_decimal(results, "; k past 64 bits, read as none ", wide);
    lane_line_end(results);
    sim_check(results, same, "the places records equal the host reference's word for word");
    sim_check(results, ok && (apart == 0ull), "every held k e-p reads back, correctly rounded, to the stored double");
    cudaFree(device_out);
    free(device);
    free(host);
    lane_release(&loaded);
    exact_record_close(&program);
    return ok;
}

// the output field at `offset`, `bits` wide, of `limbs`-limb records, as an exact integer
static void lane_field(const unsigned int *record, unsigned int offset, unsigned int bits, AnchorExactInteger *value)
{
    anchor_exact_zero(value);
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int from = offset + bit;
        value->limb[bit / 32u] |= ((record[from / 32u] >> (from % 32u)) & 1u) << (bit % 32u);
    }
    const unsigned int top = bits - 1u;
    const unsigned int negative = (value->limb[top / 32u] >> (top % 32u)) & 1u;
    const unsigned int limbs = (bits + 31u) / 32u;
    if (negative != 0u)
    {
        if ((bits % 32u) != 0u)
        {
            value->limb[limbs - 1u] |= ~0u << (bits % 32u);
        }
        unsigned long long carry = 1ull;
        for (unsigned int at = 0u; at < limbs; at += 1u)
        {
            carry += (unsigned long long)(unsigned int)~value->limb[at];
            value->limb[at] = (unsigned int)carry;
            carry >>= 32u;
        }
    }
    int any = 0;
    for (unsigned int at = 0u; at < limbs; at += 1u)
    {
        any = any || (value->limb[at] != 0u);
    }
    value->sign = any ? ((negative != 0u) ? -1 : 1) : 0;
}

// x worked out apart from the record machine, from the stored bits on the host
static void lane_expected(unsigned long long word, unsigned long long least, AnchorExactInteger *value)
{
    anchor_exact_zero(value);
    const unsigned long long biased = (word >> LANE_CHECK_MANTISSA_BITS) & LANE_CHECK_EXPONENT_ALL;
    const unsigned long long fraction = word & ((1ull << LANE_CHECK_MANTISSA_BITS) - 1ull);
    const unsigned long long mantissa = fraction | ((biased != 0ull) ? (1ull << LANE_CHECK_MANTISSA_BITS) : 0ull);
    if (mantissa == 0ull)
    {
        return;
    }
    const unsigned long long shift = ((biased == 0ull) ? 1ull : biased) - least;
    for (unsigned int bit = 0u; bit <= LANE_CHECK_MANTISSA_BITS; bit += 1u)
    {
        if (((mantissa >> bit) & 1ull) != 0ull)
        {
            const unsigned long long to = bit + shift;
            value->limb[to / 32u] |= 1u << (to % 32u);
        }
    }
    value->sign = ((word >> LANE_CHECK_SIGN_OFFSET) != 0ull) ? -1 : 1;
}

static int lane_records(SimResults *results, const LaneColumn *column, const unsigned short *device_rebuilt)
{
    unsigned long long least = 0ull;
    unsigned long long most = 0ull;
    unsigned long long zero = 0ull;
    unsigned long long unfinite = 0ull;
    lane_exponents(column, &least, &most, &zero, &unfinite);
    lane_line_decimal(results, "    biased exponents ", least);
    lane_line_decimal(results, " to ", most);
    lane_line_decimal(results, ", zero values ", zero);
    lane_line_decimal(results, ", values not finite ", unfinite);
    lane_line_end(results);
    if ((unfinite != 0ull) || (most < least))
    {
        sim_check(results, 0, "every value of the column is finite, and one is nonzero");
        return 0;
    }
    const unsigned int shift_bits = exact_record_bits_of(most - least) + 1u;
    ExactRecordProgram program;
    exact_record_open(&program);
    const unsigned int output = lane_program(&program, least, shift_bits);
    LaneLoaded loaded;
    EngineError error;
    if (lane_load(&program, &output, 1u, LANE_CHECK_LIMBS_PER_VALUE, 1u, &loaded, &error) == 0)
    {
        lane_error_line(results, "the program did not load", &error);
        exact_record_close(&program);
        return 0;
    }
    const unsigned int out_limbs = loaded.layout.out_limbs;
    const unsigned int out_offset = loaded.layout.step_table[output].out_offset;
    const unsigned int out_bits = loaded.layout.step_table[output].out_bits;
    const unsigned long long values = column->values;
    const size_t words = (size_t)(values * out_limbs);
    unsigned int *device_out = NULL;
    unsigned int *const device = (unsigned int *)malloc(words * sizeof(unsigned int) + 4u);
    unsigned int *const host = (unsigned int *)malloc(words * sizeof(unsigned int) + 4u);
    int ok = (device != NULL) && (host != NULL) &&
             (cudaMalloc((void **)&device_out, words * sizeof(unsigned int)) == cudaSuccess);
    memset(&error, 0, sizeof(error));
    // the lowered lanes are the program's records where they lie: four lanes, two limbs, one double
    const unsigned long long run_start = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordRunRequest run = {loaded.record, {(const unsigned int *)device_rebuilt, NULL, NULL},
                                           {values, 0ull, 0ull}, NULL, values, device_out, &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
    }
    const unsigned long long run_end = engine_clock_microseconds();
    // the host reference runs the leading `checked` records, and the device's sum over the same records is held
    // against the host's; the device also sums every record
    const unsigned long long checked = ((s_host_lanes != 0ull) && (s_host_lanes < values)) ? s_host_lanes : values;
    unsigned int device_sum[LANE_CHECK_SUM_LIMBS];
    unsigned int checked_sum[LANE_CHECK_SUM_LIMBS];
    unsigned int host_sum[LANE_CHECK_SUM_LIMBS];
    const unsigned int sum_limbs = ((out_bits + 40u + 31u) / 32u) + 1u;
    if (ok)
    {
        const CycleRecordSumRequest every = {device_out, values, values, out_limbs, out_offset, out_bits, sum_limbs,
                                             device_sum, &error};
        const CycleRecordSumRequest leading = {device_out, checked, checked, out_limbs, out_offset, out_bits, sum_limbs,
                                               checked_sum, &error};
        ok = (sum_limbs <= LANE_CHECK_SUM_LIMBS) && (cycle_record_sum(&every) != CYCLE_ERROR) &&
             (cycle_record_sum(&leading) != CYCLE_ERROR);
    }
    ok = ok && (cudaMemcpy(device, device_out, words * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess);
    const unsigned long long host_start = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordHostRequest run = {&loaded.layout, {(const unsigned int *)column->bytes, NULL, NULL},
                                            {values, 0ull, 0ull}, NULL, checked, host, &error, 0ull};
        ok = cycle_record_run_host(&run) != CYCLE_ERROR;
    }
    const unsigned long long host_end = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordSumRequest sum = {host, checked, checked, out_limbs, out_offset, out_bits, sum_limbs, host_sum,
                                           &error};
        ok = cycle_record_sum_host(&sum) != CYCLE_ERROR;
    }
    if (!ok)
    {
        lane_error_line(results, "the program's run or sum errored", &error);
    }
    const int same = ok && (memcmp(device, host, (size_t)(checked * out_limbs) * sizeof(unsigned int)) == 0);
    const int sums_same = ok && (memcmp(checked_sum, host_sum, sum_limbs * sizeof(unsigned int)) == 0);
    unsigned long long apart = 0ull;
    for (unsigned long long value = 0ull; ok && (value < values); value += 1ull)
    {
        unsigned long long word = 0ull;
        memcpy(&word, column->bytes + (value * 8ull), 8u);
        AnchorExactInteger read;
        AnchorExactInteger wanted;
        lane_field(&device[value * out_limbs], out_offset, out_bits, &read);
        lane_expected(word, least, &wanted);
        apart += (anchor_exact_equal(&read, &wanted) != 0) ? 0ull : 1ull;
    }
    lane_line_decimal(results, "    x: ", program.count);
    lane_line_decimal(results, " steps, record ", out_bits);
    lane_line_decimal(results, " bits in ", out_limbs);
    lane_line_decimal(results, " limbs; device ", run_end - run_start);
    lane_line_decimal(results, " us over ", values);
    lane_line_decimal(results, " lanes, host ", host_end - host_start);
    lane_line_decimal(results, " us over the leading ", checked);
    scriptura_text(&results->line, "; device and host records ");
    scriptura_text(&results->line, same ? "equal" : "DIFFER");
    scriptura_text(&results->line, ", sums ");
    scriptura_text(&results->line, sums_same ? "equal" : "DIFFER");
    lane_line_decimal(results, "; records unlike x from the bits ", apart);
    lane_line_end(results);
    scriptura_text(&results->line, "    sum of x, hexadecimal, high limb first: ");
    for (unsigned int limb = sum_limbs; limb > 0u; limb -= 1u)
    {
        scriptura_hex(&results->line, device_sum[limb - 1u], 8u);
    }
    lane_line_end(results);
    sim_check(results, same, "the device's x records equal the host reference's word for word");
    sim_check(results, sums_same, "the device's sum of x equals the host's");
    sim_check(results, ok && (apart == 0ull), "every x equals M 2^(E - E_min) worked out from the stored bits");
    cudaFree(device_out);
    free(device);
    free(host);
    lane_release(&loaded);
    exact_record_close(&program);
    return ok;
}

// the powers of ten below 2^64
#define LANE_CHECK_TENS 20u

// the form a value takes on its column's units: the integer on the first unit, a decimal on the second, a decimal
// divided by one of the LANE_CHECK_DIVISORS bases, or its stored lanes kept
#define LANE_CHECK_FORM_UNIT 0u
#define LANE_CHECK_FORM_DECIMAL 1u
#define LANE_CHECK_FORM_DIVIDED 2u
#define LANE_CHECK_FORM_KEPT (LANE_CHECK_FORM_DIVIDED + LANE_CHECK_DIVISORS)

// the bases a decimal is divided by in binary64 before it is stored, as the libraries that write percentages and
// per-mille scales divide
#define LANE_CHECK_DIVISORS 2u
static const unsigned long long LANE_CHECK_DIVISOR[LANE_CHECK_DIVISORS] = {100ull, 999ull};

// the column each intensity's row base is read from, and the intensities' column
#define LANE_CHECK_BASE_PATH "base_peak_intensity"
#define LANE_CHECK_INTENSITIES 2u

// A column on a unit: `unit` holds each value's integer on it, and a value `keep` flags is kept as its stored lanes
// besides, its integer standing among the others so their run is not broken. Each 16-bit plane of the integers, the
// flags, and the kept lanes as rows of four go through the tower and compression and back, each held against what went
// in, and the coded bits of all of them are summed into `coded` and printed against the column's raw bits
static int lane_unit_code(SimResults *results, const LaneColumn *column, const unsigned long long *unit,
                          const unsigned char *keep, unsigned long long *coded)
{
    const unsigned long long values = column->values;
    const unsigned long long raw = values * 64ull;
    unsigned short *const plane = (unsigned short *)malloc((size_t)(values * 2ull) + 2u);
    unsigned short *const kept = (unsigned short *)malloc((size_t)(values * 8ull) + 8u);
    unsigned short *device = NULL;
    int ok = (plane != NULL) && (kept != NULL) && (cudaMalloc((void **)&device, (size_t)(values * 8ull)) == cudaSuccess);
    unsigned long long kept_values = 0ull;
    unsigned long long widest = 0ull;
    unsigned int formed = 0u;
    for (unsigned long long value = 0ull; ok && (value < values); value += 1ull)
    {
        widest |= unit[value];
        formed |= keep[value];
        if (keep[value] == LANE_CHECK_FORM_KEPT)
        {
            memcpy(kept + (kept_values * LANE_CHECK_LANES_PER_VALUE), column->bytes + (value * 8ull), 8u);
            kept_values += 1ull;
        }
    }
    const unsigned int plane_count = (widest == 0ull) ? 1u : ((exact_record_bits_of(widest) + 15u) / 16u);
    const unsigned long long line[4] = {1ull, 1ull, 1ull, values};
    unsigned long long plane_bits[4] = {0ull, 0ull, 0ull, 0ull};
    *coded = 0ull;
    for (unsigned int each = 0u; ok && (each < plane_count); each += 1u)
    {
        for (unsigned long long value = 0ull; value < values; value += 1ull)
        {
            plane[value] = (unsigned short)(unit[value] >> (16u * each));
        }
        const unsigned short *rebuilt = NULL;
        ok = (cudaMemcpy(device, plane, (size_t)(values * 2ull), cudaMemcpyHostToDevice) == cudaSuccess) &&
             lane_round_trip(results, device, plane, line, &plane_bits[each], &rebuilt, NULL, 0u, 1);
        *coded += plane_bits[each];
    }
    unsigned long long flag_bits = 0ull;
    unsigned long long kept_bits = 0ull;
    if (ok && (formed != 0u))
    {
        for (unsigned long long value = 0ull; value < values; value += 1ull)
        {
            plane[value] = keep[value];
        }
        const unsigned short *rebuilt = NULL;
        ok = (cudaMemcpy(device, plane, (size_t)(values * 2ull), cudaMemcpyHostToDevice) == cudaSuccess) &&
             lane_round_trip(results, device, plane, line, &flag_bits, &rebuilt, NULL, 0u, 1);
        *coded += flag_bits;
    }
    if (ok && (kept_values != 0ull))
    {
        const unsigned short *rebuilt = NULL;
        const unsigned long long rows[4] = {1ull, 1ull, kept_values, LANE_CHECK_LANES_PER_VALUE};
        ok = (cudaMemcpy(device, kept, (size_t)(kept_values * 8ull), cudaMemcpyHostToDevice) == cudaSuccess) &&
             lane_round_trip(results, device, kept, rows, &kept_bits, &rebuilt, NULL, 0u, 1);
        *coded += kept_bits;
    }
    if (ok)
    {
        lane_line_decimal(results, " planes ", plane_count);
        scriptura_text(&results->line, " coded");
        for (unsigned int each = 0u; each < plane_count; each += 1u)
        {
            lane_line_decimal(results, " ", plane_bits[each]);
        }
        lane_line_decimal(results, " bits; kept values ", kept_values);
        lane_line_decimal(results, ", forms coded ", flag_bits);
        lane_line_decimal(results, " bits, kept lanes coded ", kept_bits);
        lane_line_decimal(results, " bits; all ", *coded);
        lane_line_decimal(results, " of raw ", raw);
        lane_line_decimal(results, ", that is ", (*coded * 1000ull) / raw);
        scriptura_text(&results->line, " per mille, floored");
        lane_line_end(results);
    }
    cudaFree(device);
    free(plane);
    free(kept);
    return ok;
}

// One unit for the column, 10^-P. A value of P places or fewer is the integer k 10^(P - p) on it; a value of more
// places, or of none below LANE_CHECK_PLACES, is kept, and laid among the integers at its floor on the unit
static int lane_decimal_trips(SimResults *results, const LaneColumn *column, const unsigned char *places_of,
                              const unsigned long long *k_of)
{
    unsigned long long ten[LANE_CHECK_TENS];
    ten[0] = 1ull;
    for (unsigned int power = 1u; power < LANE_CHECK_TENS; power += 1u)
    {
        ten[power] = ten[power - 1u] * 10ull;
    }
    const unsigned long long values = column->values;
    unsigned long long *const unit = (unsigned long long *)malloc((size_t)(values * 8ull) + 8u);
    unsigned char *const keep = (unsigned char *)malloc((size_t)values + 1u);
    int ok = (unit != NULL) && (keep != NULL);
    unsigned int most = 0u;
    for (unsigned long long value = 0ull; value < values; value += 1ull)
    {
        most = ((places_of[value] < LANE_CHECK_PLACES) && (places_of[value] > most)) ? places_of[value] : most;
    }
    unsigned long long fewest = ~0ull;
    unsigned int fewest_unit = 0u;
    for (unsigned int unit_places = 0u; ok && (unit_places <= most); unit_places += 1u)
    {
        int fits = 1;
        for (unsigned long long value = 0ull; fits && (value < values); value += 1ull)
        {
            const unsigned int places = places_of[value];
            keep[value] = (places > unit_places) ? LANE_CHECK_FORM_KEPT : LANE_CHECK_FORM_UNIT;
            if (places <= unit_places)
            {
                const unsigned long long scale = ten[unit_places - places];
                fits = k_of[value] <= (~0ull / scale);
                unit[value] = k_of[value] * scale;
            }
            else if (places < LANE_CHECK_PLACES)
            {
                unit[value] = k_of[value] / ten[places - unit_places];
            }
            else
            {
                double x = 0.0;
                memcpy(&x, column->bytes + (value * 8ull), 8u);
                const double on_unit = x * (double)ten[unit_places];
                fits = (on_unit >= 0.0) && (on_unit < 1.8e19);
                unit[value] = fits ? (unsigned long long)on_unit : 0ull;
            }
        }
        if (!fits)
        {
            break;
        }
        lane_line_decimal(results, "    on 10^-", unit_places);
        scriptura_character(&results->line, ':');
        unsigned long long coded = 0ull;
        ok = lane_unit_code(results, column, unit, keep, &coded);
        if (ok && (coded < fewest))
        {
            fewest = coded;
            fewest_unit = unit_places;
        }
    }
    if (fewest != ~0ull)
    {
        lane_line_decimal(results, "    the fewest bits on one unit: 10^-", fewest_unit);
        lane_line_decimal(results, ", ", fewest);
        lane_line_decimal(results, " bits, that is ", (fewest * 1000ull) / (values * 64ull));
        scriptura_text(&results->line, " per mille of raw, floored");
        lane_line_end(results);
    }
    sim_check(results, ok, "every unit's planes, flags and kept lanes lift, code, decode and lower");
    free(unit);
    free(keep);
    return ok;
}

// The counts program: x = c / B correctly rounded, B the row's base, an integer of member 1 read as its stored double,
// and c the integer in B times x's preimage. `holds` is 1 where B is a whole number from 1 to 2^53 and such a c
// exists, and `count` is c there and 0 elsewhere
static void lane_counts_program(ExactRecordProgram *program, unsigned long long most_placed, unsigned int shift_bits,
                                unsigned int *holds, unsigned int *count)
{
    const unsigned int fraction_field = exact_record_member_field(program, LANE_CHECK_MANTISSA_BITS, 0u);
    const unsigned int exponent_field =
        exact_record_member_field(program, LANE_CHECK_EXPONENT_BITS, LANE_CHECK_MANTISSA_BITS);
    const unsigned int fraction = exact_record_read_unsigned(program, fraction_field, 0u);
    const unsigned int biased = exact_record_read_unsigned(program, exponent_field, 0u);
    const unsigned int base_fraction = exact_record_read_unsigned(program, fraction_field, 1u);
    const unsigned int base_biased = exact_record_read_unsigned(program, exponent_field, 1u);
    const unsigned int zero = exact_record_constant(program, 0ull);
    const unsigned int one = exact_record_constant(program, 1ull);
    LanePreimage preimage;
    lane_preimage(program, fraction, biased, most_placed, shift_bits, &preimage);
    // B is 1 to 2^53 where its biased exponent is 1023 to 1075, and whole where 2^(1075 - E) divides its mantissa
    const unsigned int base_least = exact_record_constant(program, LANE_CHECK_BASE_BIASED_LEAST - 1ull);
    const unsigned int base_most = exact_record_constant(program, LANE_CHECK_BASE_BIASED_MOST);
    const unsigned int in_range = exact_record_product(program, exact_record_above(program, base_biased, base_least),
                                                       exact_record_difference(program, one,
                                                                               exact_record_above(program, base_biased,
                                                                                                  base_most)));
    const unsigned int base_mantissa = exact_record_sum(
        program, base_fraction, exact_record_power_two(program, LANE_CHECK_MANTISSA_BITS));
    const unsigned int drop = exact_record_select(program, in_range,
                                                  exact_record_difference(program, base_most, base_biased), zero);
    const unsigned int step = exact_record_two_to(program, drop, LANE_CHECK_BASE_DROP_BITS);
    const unsigned int base = exact_record_quotient(program, base_mantissa, step);
    const unsigned int whole =
        exact_record_equal(program, exact_record_product(program, base, step), base_mantissa);
    unsigned int least = 0u;
    unsigned int found = 0u;
    lane_preimage_integer(program, &preimage, base, &least, &found);
    *holds = exact_record_product(program, exact_record_product(program, in_range, whole), found);
    *count = exact_record_product(program, *holds, least);
}

// Each intensity read against its row's base: the counts program runs on the device over every value, member 0 the
// lowered lanes and member 1 the base column's stored doubles, the index naming each value's own row's base, and a
// row with no base reads a 0 laid past the column's end. The host reference runs the leading values, every held c is
// read back through c / B in binary64 against the stored double, and the counts go through lane_unit_code
static int lane_counts(SimResults *results, const LaneColumn *column, const LaneColumn *bases,
                       const unsigned short *device_rebuilt, const unsigned char *const *places_of,
                       const unsigned long long *const *k_of)
{
    if (bases->rows != column->rows)
    {
        sim_check(results, 0, "the base column has the intensities' rows");
        return 0;
    }
    unsigned long long least_placed = 0ull;
    unsigned long long most_placed = 0ull;
    unsigned long long zero = 0ull;
    unsigned long long unfinite = 0ull;
    lane_exponents(column, &least_placed, &most_placed, &zero, &unfinite);
    const unsigned int shift_bits = exact_record_bits_of(LANE_CHECK_PLACES_LIFT - least_placed) + 1u;
    ExactRecordProgram program;
    exact_record_open(&program);
    unsigned int outputs[2] = {0u, 0u};
    lane_counts_program(&program, most_placed, shift_bits, &outputs[0], &outputs[1]);
    LaneLoaded loaded;
    EngineError error;
    if (lane_load(&program, outputs, 2u, LANE_CHECK_LIMBS_PER_VALUE, 2u, &loaded, &error) == 0)
    {
        lane_error_line(results, "the counts program did not load", &error);
        exact_record_close(&program);
        return 0;
    }
    const unsigned int out_limbs = loaded.layout.out_limbs;
    const DeviceRecordStep holds_place = loaded.layout.step_table[outputs[0]];
    const DeviceRecordStep count_place = loaded.layout.step_table[outputs[1]];
    const unsigned long long values = column->values;
    const unsigned long long checked = ((s_host_lanes != 0ull) && (s_host_lanes < values)) ? s_host_lanes : values;
    // the bases, and a 0 past them for a row with none; each value's index pair, its own record and its row's base
    const unsigned long long base_records = bases->values + 1ull;
    unsigned char *const base_bytes = (unsigned char *)calloc((size_t)(base_records * 8ull), 1u);
    unsigned int *const index = (unsigned int *)malloc((size_t)(values * 2ull) * sizeof(unsigned int) + 8u);
    const size_t words = (size_t)(values * out_limbs);
    unsigned int *const device = (unsigned int *)malloc(words * sizeof(unsigned int) + 4u);
    unsigned int *const host = (unsigned int *)malloc((size_t)(checked * out_limbs) * sizeof(unsigned int) + 4u);
    unsigned int *device_out = NULL;
    unsigned int *device_bases = NULL;
    unsigned int *device_index = NULL;
    int ok = (base_bytes != NULL) && (index != NULL) && (device != NULL) && (host != NULL);
    if (ok)
    {
        memcpy(base_bytes, bases->bytes, (size_t)(bases->values * 8ull));
        for (unsigned long long row = 0ull; row < column->rows; row += 1ull)
        {
            const int present = bases->row_start[row + 1ull] > bases->row_start[row];
            const unsigned int base_record = (unsigned int)(present ? bases->row_start[row] : bases->values);
            for (unsigned long long value = column->row_start[row]; value < column->row_start[row + 1ull]; value += 1ull)
            {
                index[value * 2ull] = (unsigned int)value;
                index[(value * 2ull) + 1ull] = base_record;
            }
        }
    }
    ok = ok && (cudaMalloc((void **)&device_out, words * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_bases, (size_t)(base_records * 8ull)) == cudaSuccess) &&
         (cudaMalloc((void **)&device_index, (size_t)(values * 2ull) * sizeof(unsigned int)) == cudaSuccess) &&
         (cudaMemcpy(device_bases, base_bytes, (size_t)(base_records * 8ull), cudaMemcpyHostToDevice) == cudaSuccess) &&
         (cudaMemcpy(device_index, index, (size_t)(values * 2ull) * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
          cudaSuccess);
    memset(&error, 0, sizeof(error));
    const unsigned long long run_start = engine_clock_microseconds();
    if (ok)
    {
        const CycleRecordRunRequest run = {loaded.record,
                                           {(const unsigned int *)device_rebuilt, device_bases, NULL},
                                           {values, base_records, 0ull},
                                           device_index,
                                           values,
                                           device_out,
                                           &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
    }
    const unsigned long long run_end = engine_clock_microseconds();
    ok = ok && (cudaMemcpy(device, device_out, words * sizeof(unsigned int), cudaMemcpyDeviceToHost) == cudaSuccess);
    if (ok)
    {
        const CycleRecordHostRequest run = {&loaded.layout,
                                            {(const unsigned int *)column->bytes, (const unsigned int *)base_bytes, NULL},
                                            {values, base_records, 0ull},
                                            index,
                                            checked,
                                            host,
                                            &error,
                                            0ull};
        ok = cycle_record_run_host(&run) != CYCLE_ERROR;
    }
    if (!ok)
    {
        lane_error_line(results, "the counts program's run errored", &error);
    }
    const int same = ok && (memcmp(device, host, (size_t)(checked * out_limbs) * sizeof(unsigned int)) == 0);
    unsigned long long *const unit = (unsigned long long *)malloc((size_t)(values * 8ull) + 8u);
    unsigned char *const keep = (unsigned char *)malloc((size_t)values + 1u);
    ok = ok && (unit != NULL) && (keep != NULL);
    unsigned long long held = 0ull;
    unsigned long long apart = 0ull;
    unsigned long long count_bits = 0ull;
    unsigned long long widest = 0ull;
    for (unsigned long long value = 0ull; ok && (value < values); value += 1ull)
    {
        AnchorExactInteger read;
        lane_field(&device[value * out_limbs], holds_place.out_offset, holds_place.out_bits, &read);
        const int holding = read.limb[0] != 0u;
        lane_field(&device[value * out_limbs], count_place.out_offset, count_place.out_bits, &read);
        const unsigned long long c = ((unsigned long long)read.limb[1] << 32u) | read.limb[0];
        unit[value] = c;
        keep[value] = holding ? LANE_CHECK_FORM_UNIT : LANE_CHECK_FORM_KEPT;
        if (holding)
        {
            held += 1ull;
            count_bits += lane_bits(&read);
            widest = (c > widest) ? c : widest;
            double x = 0.0;
            double base = 0.0;
            memcpy(&x, column->bytes + (value * 8ull), 8u);
            memcpy(&base, base_bytes + ((unsigned long long)index[(value * 2ull) + 1ull] * 8ull), 8u);
            const double read_back = (double)c / base;
            apart += (memcmp(&read_back, &x, 8u) == 0) ? 0ull : 1ull;
        }
    }
    lane_line_decimal(results, "    counts: ", program.count);
    lane_line_decimal(results, " steps, record ", (unsigned long long)loaded.layout.out_bits);
    lane_line_decimal(results, " bits; device ", run_end - run_start);
    lane_line_decimal(results, " us over ", values);
    scriptura_text(&results->line, " values; device and host records ");
    scriptura_text(&results->line, same ? "equal" : "DIFFER");
    lane_line_decimal(results, " over the leading ", checked);
    lane_line_end(results);
    lane_line_decimal(results, "    rows with a base ", bases->values);
    lane_line_decimal(results, " of ", bases->rows);
    lane_line_decimal(results, "; values a whole c over the base holds ", held);
    lane_line_decimal(results, " of ", values);
    lane_line_decimal(results, "; the widest c ", widest);
    lane_line_decimal(results, ", bits of c over them ", count_bits);
    lane_line_decimal(results, "; c / B unlike the stored double ", apart);
    lane_line_end(results);
    sim_check(results, same, "the counts records equal the host reference's word for word");
    sim_check(results, ok && (apart == 0ull), "every held c over its base reads back to the stored double");
    unsigned long long coded = 0ull;
    if (ok)
    {
        scriptura_text(&results->line, "    on each row's 1 / B:");
        ok = lane_unit_code(results, column, unit, keep, &coded);
        sim_check(results, ok, "the counts' planes, flags and kept lanes lift, code, decode and lower");
    }
    if (ok)
    {
        const unsigned long long base_raw = bases->values * 64ull;
        lane_line_decimal(results, "    with the bases at their raw ", base_raw);
        lane_line_decimal(results, " bits: ", coded + base_raw);
        lane_line_decimal(results, ", that is ", ((coded + base_raw) * 1000ull) / (values * 64ull));
        scriptura_text(&results->line, " per mille of the intensities' raw, floored");
        lane_line_end(results);
    }
    // a value no whole c over a base holds, read on 10^-P where P places hold it
    unsigned long long ten[LANE_CHECK_TENS];
    ten[0] = 1ull;
    for (unsigned int power = 1u; power < LANE_CHECK_TENS; power += 1u)
    {
        ten[power] = ten[power - 1u] * 10ull;
    }
    unsigned long long *const second = (unsigned long long *)malloc((size_t)(values * 8ull) + 8u);
    unsigned char *const form = (unsigned char *)malloc((size_t)values + 1u);
    ok = ok && (second != NULL) && (form != NULL);
    unsigned long long fewest = ~0ull;
    unsigned int fewest_unit = 0u;
    for (unsigned int unit_places = 0u; ok && (unit_places < LANE_CHECK_PLACES); unit_places += 1u)
    {
        // the value itself as a decimal first, then the decimal it is over each divisor in turn
        unsigned long long taken[1u + LANE_CHECK_DIVISORS];
        memset(taken, 0, sizeof(taken));
        for (unsigned long long value = 0ull; value < values; value += 1ull)
        {
            second[value] = unit[value];
            form[value] = keep[value];
            for (unsigned int each = 0u; (form[value] == LANE_CHECK_FORM_KEPT) && (each <= LANE_CHECK_DIVISORS);
                 each += 1u)
            {
                const unsigned int places = places_of[each][value];
                if ((places <= unit_places) && (k_of[each][value] <= (~0ull / ten[unit_places - places])))
                {
                    second[value] = k_of[each][value] * ten[unit_places - places];
                    form[value] = (each == 0u) ? LANE_CHECK_FORM_DECIMAL : (LANE_CHECK_FORM_DIVIDED + each - 1u);
                    taken[each] += 1ull;
                }
            }
        }
        lane_line_decimal(results, "    on 1 / B, else 10^-", unit_places);
        lane_line_decimal(results, " for ", taken[0]);
        for (unsigned int each = 0u; each < LANE_CHECK_DIVISORS; each += 1u)
        {
            lane_line_decimal(results, ", over ", LANE_CHECK_DIVISOR[each]);
            lane_line_decimal(results, " for ", taken[1u + each]);
        }
        scriptura_text(&results->line, ":");
        unsigned long long both = 0ull;
        ok = lane_unit_code(results, column, second, form, &both);
        if (ok && (both < fewest))
        {
            fewest = both;
            fewest_unit = unit_places;
        }
    }
    if (ok && (fewest != ~0ull))
    {
        const unsigned long long base_raw = bases->values * 64ull;
        lane_line_decimal(results, "    the fewest: on 1 / B, else 10^-", fewest_unit);
        lane_line_decimal(results, ", with the bases at their raw: ", fewest + base_raw);
        lane_line_decimal(results, " bits, that is ", ((fewest + base_raw) * 1000ull) / (values * 64ull));
        scriptura_text(&results->line, " per mille of the intensities' raw, floored");
        lane_line_end(results);
    }
    sim_check(results, ok, "the counts and decimals' planes, forms and kept lanes lift, code, decode and lower");
    free(second);
    free(form);
    cudaFree(device_out);
    cudaFree(device_bases);
    cudaFree(device_index);
    free(base_bytes);
    free(index);
    free(device);
    free(host);
    free(unit);
    free(keep);
    lane_release(&loaded);
    exact_record_close(&program);
    return ok;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    if (count < 2)
    {
        scriptura_text(&results.line, "usage: lane_check FILE.parquet [HOST_LANES [COLUMN]]\n"
                                      "  HOST_LANES: the leading records the host reference checks, every one where it"
                                      " is not given or is 0\n"
                                      "  COLUMN: 0 precursor_mz, 1 ms2_mzs, 2 the intensities; every column where it"
                                      " is not given\n");
        sim_flush(&results);
        return 2;
    }
    s_host_lanes = (count > 2) ? strtoull(arguments[2], NULL, 10) : 0ull;
    s_column = (count > 3) ? (unsigned int)strtoul(arguments[3], NULL, 10) : LANE_CHECK_COLUMNS;
    const CasmiParquetFooterRead footer = {arguments[1], &s_footer};
    if (casmi_parquet_footer_read(&footer) < 0ll)
    {
        scriptura_text(&results.line, "  the parquet footer did not read\n");
        sim_flush(&results);
        return 1;
    }
    LaneColumn columns[LANE_CHECK_COLUMNS];
    unsigned long long widest = 0ull;
    for (unsigned int each = 0u; each < LANE_CHECK_COLUMNS; each += 1u)
    {
        if (lane_column_read(arguments[1], LANE_CHECK_COLUMN_PATH[each], &columns[each]) == 0)
        {
            scriptura_text(&results.line, "  a column did not read: ");
            scriptura_text(&results.line, LANE_CHECK_COLUMN_PATH[each]);
            lane_line_end(&results);
            return 1;
        }
        widest = (columns[each].values > widest) ? columns[each].values : widest;
    }
    LaneColumn bases;
    if (lane_column_read(arguments[1], LANE_CHECK_BASE_PATH, &bases) == 0)
    {
        scriptura_text(&results.line, "  a column did not read: " LANE_CHECK_BASE_PATH);
        lane_line_end(&results);
        return 1;
    }
    const unsigned long long widest_lanes = widest * LANE_CHECK_LANES_PER_VALUE;
    const unsigned long long declared = (2ull * widest_lanes * sizeof(unsigned short)) + tower_reserve_bytes(widest_lanes) +
                                        compression_reserve_bytes(widest_lanes) + (widest * 4ull * sizeof(unsigned int));
    if (sim_job_submit(&results, "lane_check", count, arguments, declared) == 0)
    {
        return sim_close(&results, "lane check");
    }
    for (unsigned int each = 0u; each < LANE_CHECK_COLUMNS; each += 1u)
    {
        if ((s_column < LANE_CHECK_COLUMNS) && (each != s_column))
        {
            free(columns[each].bytes);
            free(columns[each].row_start);
            continue;
        }
        const LaneColumn *const column = &columns[each];
        const unsigned long long lanes = column->values * LANE_CHECK_LANES_PER_VALUE;
        scriptura_text(&results.line, "  ");
        scriptura_text(&results.line, LANE_CHECK_COLUMN_PATH[each]);
        lane_line_decimal(&results, ": ", column->values);
        lane_line_decimal(&results, " values, ", lanes);
        scriptura_text(&results.line, " lanes");
        lane_line_end(&results);
        sim_check(&results, lane_reading(&results, column), "the column's values sort");
        unsigned short *device_lanes = NULL;
        const int placed = (cudaMalloc((void **)&device_lanes, (size_t)(lanes * 2ull)) == cudaSuccess) &&
                           (cudaMemcpy(device_lanes, column->bytes, (size_t)(lanes * 2ull), cudaMemcpyHostToDevice) ==
                            cudaSuccess);
        sim_check(&results, placed, "the stored lanes reach the device");
        // the places of each value itself, then, for the intensities, of the decimal it is over each divisor
        unsigned char *places_of[1u + LANE_CHECK_DIVISORS];
        unsigned long long *k_of[1u + LANE_CHECK_DIVISORS];
        int allocated = 1;
        for (unsigned int form = 0u; form <= LANE_CHECK_DIVISORS; form += 1u)
        {
            places_of[form] = (unsigned char *)malloc((size_t)column->values + 1u);
            k_of[form] = (unsigned long long *)malloc((size_t)(column->values * 8ull) + 8u);
            allocated = allocated && (places_of[form] != NULL) && (k_of[form] != NULL);
        }
        int places_read = 0;
        for (unsigned int layout = 0u; placed && (layout < LANE_CHECK_LAYOUTS); layout += 1u)
        {
            const unsigned long long line[4] = {1ull, 1ull, 1ull, lanes};
            const unsigned long long rows[4] = {1ull, 1ull, column->values, LANE_CHECK_LANES_PER_VALUE};
            const unsigned long long *const extent = (layout == 0u) ? line : rows;
            scriptura_text(&results.line, "   ");
            scriptura_text(&results.line, LANE_CHECK_LAYOUT_NAME[layout]);
            lane_line_end(&results);
            unsigned long long bits = 0ull;
            const unsigned short *device_rebuilt = NULL;
            const int tripped = lane_round_trip(&results, device_lanes, (const unsigned short *)column->bytes, extent,
                                                &bits, &device_rebuilt, NULL, 0u, 0);
            sim_check(&results, tripped, "the lattice lifts, codes, decodes and lowers");
            if (tripped && (layout == (LANE_CHECK_LAYOUTS - 1u)))
            {
                (void)lane_records(&results, column, device_rebuilt);
                places_read = allocated && lane_places(&results, column, device_rebuilt, 0ull, places_of[0], k_of[0]);
                if (places_read && (each == LANE_CHECK_INTENSITIES))
                {
                    for (unsigned int divisor = 0u; places_read && (divisor < LANE_CHECK_DIVISORS); divisor += 1u)
                    {
                        places_read = lane_places(&results, column, device_rebuilt, LANE_CHECK_DIVISOR[divisor],
                                                  places_of[1u + divisor], k_of[1u + divisor]);
                    }
                    (void)(places_read &&
                           lane_counts(&results, column, &bases, device_rebuilt, (const unsigned char *const *)places_of,
                                       (const unsigned long long *const *)k_of));
                }
            }
        }
        if (places_read)
        {
            scriptura_text(&results.line, "   the values on one decimal unit");
            lane_line_end(&results);
            (void)lane_decimal_trips(&results, column, places_of[0], k_of[0]);
        }
        for (unsigned int form = 0u; form <= LANE_CHECK_DIVISORS; form += 1u)
        {
            free(places_of[form]);
            free(k_of[form]);
        }
        // the same lanes as planes: plane j holds lane j of every value, in value order; each significance lies
        // beside its own kind; laid once as four rows and once as four lattices of their own
        unsigned short *const planes = (unsigned short *)malloc((size_t)(lanes * 2ull) + 2u);
        unsigned short *device_planes = NULL;
        int planar = placed && (planes != NULL) && (cudaMalloc((void **)&device_planes, (size_t)(lanes * 2ull)) == cudaSuccess);
        const unsigned short *const stored = (const unsigned short *)column->bytes;
        for (unsigned long long value = 0ull; planar && (value < column->values); value += 1ull)
        {
            for (unsigned long long plane = 0ull; plane < LANE_CHECK_LANES_PER_VALUE; plane += 1ull)
            {
                planes[(plane * column->values) + value] = stored[(value * LANE_CHECK_LANES_PER_VALUE) + plane];
            }
        }
        planar = planar && (cudaMemcpy(device_planes, planes, (size_t)(lanes * 2ull), cudaMemcpyHostToDevice) == cudaSuccess);
        if (planar)
        {
            const unsigned long long four_rows[4] = {1ull, 1ull, LANE_CHECK_LANES_PER_VALUE, column->values};
            scriptura_text(&results.line, "   planes as four rows");
            lane_line_end(&results);
            unsigned long long bits = 0ull;
            const unsigned short *device_rebuilt = NULL;
            sim_check(&results, lane_round_trip(&results, device_planes, planes, four_rows, &bits, &device_rebuilt, NULL, 0u, 0),
                      "the planes lift, code, decode and lower");
            unsigned long long plane_bits = 0ull;
            unsigned long long better_bits = 0ull;
            unsigned int *const forward = (unsigned int *)malloc(LANE_CHECK_EDGE_ENTRIES * sizeof(unsigned int));
            for (unsigned long long plane = 0ull; plane < LANE_CHECK_LANES_PER_VALUE; plane += 1ull)
            {
                const unsigned long long alone[4] = {1ull, 1ull, 1ull, column->values};
                lane_line_decimal(&results, "   plane ", plane);
                scriptura_text(&results.line, " alone");
                lane_line_end(&results);
                bits = 0ull;
                sim_check(&results,
                          lane_round_trip(&results, device_planes + (plane * column->values),
                                          planes + (plane * column->values), alone, &bits, &device_rebuilt, NULL, 0u, 0),
                          "a plane alone lifts, codes, decodes and lowers");
                plane_bits += bits;
                unsigned long long ranked_bits = ~0ull;
                if ((forward != NULL) && lane_rank_edge(planes + (plane * column->values), column->values, forward))
                {
                    const TowerEdge edge = {0u, LANE_CHECK_EDGE_BITS, forward};
                    lane_line_decimal(&results, "   plane ", plane);
                    scriptura_text(&results.line, " alone, its lanes ranked by count at floor 0, the ranking's ");
                    scriptura_decimal(&results.line, LANE_CHECK_EDGE_ENTRIES * LANE_CHECK_EDGE_BITS, 1u);
                    scriptura_text(&results.line, " bits charged to it");
                    lane_line_end(&results);
                    unsigned long long edged = 0ull;
                    sim_check(&results,
                              lane_round_trip(&results, device_planes + (plane * column->values),
                                              planes + (plane * column->values), alone, &edged, &device_rebuilt, &edge,
                                              1u, 0),
                              "a ranked plane lifts, codes, decodes and lowers");
                    ranked_bits = edged + (LANE_CHECK_EDGE_ENTRIES * LANE_CHECK_EDGE_BITS);
                }
                better_bits += (ranked_bits < bits) ? ranked_bits : bits;
            }
            free(forward);
            lane_line_decimal(&results, "   the four planes alone: coded bits ", plane_bits);
            lane_line_decimal(&results, " of raw ", lanes * 16ull);
            lane_line_decimal(&results, ", that is ", (plane_bits * 1000ull) / (lanes * 16ull));
            scriptura_text(&results.line, " per mille, floored");
            lane_line_end(&results);
            lane_line_decimal(&results, "   each plane at the fewer bits of the two: coded bits ", better_bits);
            lane_line_decimal(&results, ", that is ", (better_bits * 1000ull) / (lanes * 16ull));
            scriptura_text(&results.line, " per mille, floored");
            lane_line_end(&results);
        }
        cudaFree(device_planes);
        free(planes);
        cudaFree(device_lanes);
        free(columns[each].bytes);
        free(columns[each].row_start);
    }
    free(bases.bytes);
    free(bases.row_start);
    return sim_close(&results, "lane check");
}
