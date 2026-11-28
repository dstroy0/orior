// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The gold standard corpora read against each other and the papers read against them on the record machine, every
// distance an exact rational, and every verdict, order and median taken without a float.
//
//   Usage:  gold_readings <input> <output> [<figures>]
//
// The measure is orior.py's, sections 1 to 3. A text is its count of adjacent byte pairs, a_k in cell
// k = 256 b_i + b_(i+1) over each line's UTF-8 bytes, A of them in all. Two texts are apart by their total
// variation, which over the counts is the exact rational
//
//   D = sum over k of |a_k B - b_k A| / (2 A B).
//
// A text's split-half distance is D between its two halves, cut at the midpoint (lines [0, n/2) and [n/2, n)) and,
// for a corpus, alternating as well (the even lines and the odd). A text of fewer than two lines, or with a half
// containing no pair, has split-half distance 1, as self_distance gives it.
//
// What is read:
//   a pair of corpora reads where D clears the larger split-half distance of the two by the input's margin m;
//   a paper is read only where it contains at least the input's least count of pairs. Its distances to the language
//     corpora, English left out, are ordered with ties broken by name. The gap is the runner-up's distance less the
//     nearest's, and the paper reads where the gap clears m times its own midpoint split-half distance, as
//     language_check.py requests it;
//   the medians of the gaps and of the split-half distances over the papers reserved and not read, each the middle
//     value, or the mean of the middle two where the count is even.
//
// Eight programs of the record machine, each built by a request that carries only its field widths:
//   distance  one lane a cell of one comparison, a, b, A and B in, |a B - b A| and 2 A B out; the sum over each run
//             of a comparison's lanes is the numerator of its D;
//   ratio     x and y in, x / y out as (x_num y_den) / (x_den y_num);
//   compare   x, y and m in, the sign of x_num y_den m_den - y_num x_den m_num out, +1 where x > m y;
//   gap       nearest and runner-up in, (n2 d1 - n1 d2) / (d1 d2) out;
//   mean      two values in, (a d + c b) / (2 b d) out;
//   digits    a value in, its whole part and the first six digits after the point out, by quotient and remainder.
// The compare program runs three times, on the corpora, on the papers' gaps, and on the orders the medians need.
// Counting the pairs is the host's, and every product, difference, sum, quotient and comparison is the machine's.
//
// The input is what gold_readings.py writes: "margin <numerator> <denominator>", "least <pairs>", each corpus as
// "corpus <name> <lines>" and that many lines, then each paper as "paper <stem> <language> <lines>" and its lines.
//
// The output contains the records whole. The figures, where named, are the TeX macros the Salishan research paper
// reads.
//
// Checks:
//   1. each program imprints and arranges;
//   2. each program loads onto the device and every lane runs it;
//   3. the host's records of each program equal the device's word for word;
//   4. the device's sum of each comparison's run equals the host's;
//   5. every distance lies in [0, 1], its numerator at max its denominator;
//   6. the records are written out.

#include "../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "sim.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>

#include "record_stages.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>

// the pool of the host: the pair tables, a cell of GOLD_CELLS counts each, and the distance stage's records
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 31u;

#define GOLD_CELLS 65536u

// the widest total a field stores, so that 2 A B fits 64 bits
#define GOLD_TOTAL_BITS_MAX 31u

// an absent comparison of a paper
#define GOLD_ABSENT (~0ull)

// a text of the input: its name, its language where it is a paper, and its lines from line[first] on, `count` of them
typedef struct
{
    const char *name;
    const char *language;
    unsigned long long first;
    unsigned long long count;
} GoldText;

// a text's pair counts over the 2^16 cells, a cell of the temporary tier, and how many pairs there are in all
typedef struct
{
    unsigned int *counts;
    unsigned long long total;
} GoldTable;

// two tables measured against each other, and the distance the distance program gives them
typedef struct
{
    StageText name;
    unsigned long long left;
    unsigned long long right;
    StageRational distance;
} GoldComparison;

// a paper's reading: its comparisons to the anchors from comparison[first_against] on, one an anchor
typedef struct
{
    unsigned long long text;
    unsigned long long whole;
    int qualified;
    unsigned long long first_against;
    unsigned long long own;
    StageRational floor;
    StageRational gap;
    int reads;
} GoldPaper;

// |a B - b A| and 2 A B for one cell of one comparison
static void gold_distance_program(unsigned int count_bits, unsigned int total_bits, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int a = stage_field(program, count_bits);
    const unsigned int b = stage_field(program, count_bits);
    const unsigned int big_a = stage_field(program, total_bits);
    const unsigned int big_b = stage_field(program, total_bits);
    const unsigned int a_by_b = stage_step(program, ENGINE_RECORD_PRODUCT, a, big_b);
    const unsigned int b_by_a = stage_step(program, ENGINE_RECORD_PRODUCT, b, big_a);
    const unsigned int apart = stage_step(program, ENGINE_RECORD_DIFFERENCE, a_by_b, b_by_a);
    stage_output(program, stage_step(program, ENGINE_RECORD_ABSOLUTE, apart, 0u));
    const unsigned int both = stage_step(program, ENGINE_RECORD_PRODUCT, big_a, big_b);
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, both, stage_constant(program, 2ull)));
}

// The pairs of a text's lines from `from` up to `until` at a step of `step`, each line's bytes read alone, into the
// table at `table`, its counts a cell of the temporary tier. A line's bytes run to its line break, a NUL byte inside it
// counted. 0 where the pool cannot meet the cell
static int gold_squash(const StageLines *lines, const GoldText *text, unsigned long long from, unsigned long long until,
                       unsigned long long step, GoldTable *table)
{
    table->counts = (unsigned int *)stage_cell(GOLD_CELLS, sizeof(unsigned int));
    table->total = 0ull;
    if (table->counts == NULL)
    {
        return 0;
    }
    for (unsigned long long at = from; at < until; at += step)
    {
        const char *const line = lines->line[text->first + at];
        const unsigned long long length = lines->length[text->first + at];
        // each byte is a step of the count: it starts the pair of itself and the byte after it
        for (unsigned long long place = 0ull; (place + 1ull) < length; place += 1ull)
        {
            const unsigned int cell =
                ((unsigned int)(unsigned char)line[place] << 8u) | (unsigned int)(unsigned char)line[place + 1u];
            table->counts[cell] += 1u;
            table->total += 1ull;
        }
    }
    return 1;
}

// The block of a text that starts at line *at: its head "<type> <name> <lines>", or "<type> <name> <language> <lines>"
// where `with_language`, split in place, and the lines after it. 0 where the head or the count does not read
static int gold_block(const StageLines *lines, unsigned long long *at, const char *type, int with_language,
                      GoldText *text)
{
    char *const head = lines->line[*at];
    const size_t opening =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u;
    char *const last = (char *)EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = head,
                                          .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ' ');
    if ((last == NULL) || ((size_t)(last - head) <= opening))
    {
        return 0;
    }
    const unsigned long long count = strtoull(last + 1, NULL, 10);
    *last = '\0';
    char *const named = head + opening;
    text->language = NULL;
    if (with_language)
    {
        char *const space = (char *)EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = named,
                                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ' ');
        if (space == NULL)
        {
            return 0;
        }
        *space = '\0';
        text->language = space + 1;
    }
    text->name = named;
    *at += 1ull;
    if ((*at + count) > lines->count)
    {
        return 0;
    }
    text->first = *at;
    text->count = count;
    *at += count;
    return 1;
}

// the input read once: its lines, and its corpora and papers, each a cell of the temporary tier at the count of its
// lines, which bounds them before they fill
typedef struct
{
    StageLines lines;
    GoldText *corpus;
    unsigned long long corpora;
    GoldText *paper;
    unsigned long long papers;
} GoldInput;

static int gold_read(const char *path, GoldInput *input, unsigned long long margin[2], unsigned long long *least)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = input, .bytes = sizeof(*input), .val = 0);
    if (!stage_lines(path, &input->lines))
    {
        return 0;
    }
    StageLines *const lines = &input->lines;
    if ((lines->count < 2u) || (sscanf(lines->line[0], "margin %llu %llu", &margin[0], &margin[1]) != 2) ||
        (margin[0] == 0ull) || (margin[1] == 0ull) || (sscanf(lines->line[1], "least %llu", least) != 1))
    {
        return 0;
    }
    input->corpus = (GoldText *)stage_cell(lines->count, sizeof(GoldText));
    input->paper = (GoldText *)stage_cell(lines->count, sizeof(GoldText));
    if ((input->corpus == NULL) || (input->paper == NULL))
    {
        return 0;
    }
    unsigned long long at = 2ull;
    while (at < lines->count)
    {
        const char *const head = lines->line[at];
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = head, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "corpus "))
        {
            if (!gold_block(lines, &at, "corpus", 0, &input->corpus[input->corpora]))
            {
                return 0;
            }
            input->corpora += 1ull;
        }
        else if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = head, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "paper "))
        {
            if (!gold_block(lines, &at, "paper", 1, &input->paper[input->papers]))
            {
                return 0;
            }
            input->papers += 1ull;
        }
        else
        {
            return 0;
        }
    }
    return input->corpora >= 2u;
}

// a comparison's name: `left`, `joint` and `right` put one after the other; 0 where they pass the name's bound
static int gold_named(StageText *name, const char *left, const char *joint, const char *right)
{
    VerbaLinea line = mmgr_line_over(name->bytes, sizeof(name->bytes));
    mmgr_line_put(&line, left);
    mmgr_line_put(&line, joint);
    mmgr_line_put(&line, right);
    const int fits = line.at < sizeof(name->bytes);
    (void)mmgr_line_finish(&line);
    return fits;
}

// the bytes of a decimal text before its point, all of it where it holds none
static int gold_whole_length(const char *text)
{
    const char *const point =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    return (point != NULL)
               ? (int)(point - text)
               : (int)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
}

int program_main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if ((count != 3) && (count != 4))
    {
        fprintf(stderr, "  usage: gold_readings <input> <output> [<figures>]\n");
        return 2;
    }
    GoldInput input;
    unsigned long long margin[2] = {0ull, 0ull};
    unsigned long long least = 0ull;
    if (!gold_read(arguments[1], &input, margin, &least))
    {
        fprintf(stderr, "  gold_readings: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }
    const GoldText *const corpora = input.corpus;
    const GoldText *const texts = input.paper;
    const unsigned long long paper_count = input.papers;
    const unsigned long long unit_margin[2] = {1ull, 1ull};

    // the anchors: every corpus but English
    const unsigned long long named = input.corpora;
    unsigned long long *const anchors = (unsigned long long *)stage_cell(named, sizeof(unsigned long long));
    if (anchors == NULL)
    {
        return 2;
    }
    unsigned long long anchor_count = 0ull;
    for (unsigned long long at = 0ull; at < named; at += 1ull)
    {
        if (!stage_same(corpora[at].name, "English"))
        {
            anchors[anchor_count] = at;
            anchor_count += 1ull;
        }
    }

    // The tables: each corpus whole, its midpoint halves and its alternating halves, then each paper whole and its
    // midpoint halves. The comparisons: every pair of corpora, each corpus's halves, and each paper's
    const unsigned long long pairs = stage_order_pairs(named);
    GoldTable *const tables = (GoldTable *)stage_cell((5ull * named) + (3ull * paper_count), sizeof(GoldTable));
    GoldComparison *const comparisons = (GoldComparison *)stage_cell(
        pairs + (2ull * named) + (paper_count * (anchor_count + 1ull)), sizeof(GoldComparison));
    GoldPaper *const papers = (GoldPaper *)stage_cell(paper_count, sizeof(GoldPaper));
    if ((tables == NULL) || (comparisons == NULL) || (papers == NULL))
    {
        return 2;
    }
    unsigned long long table_count = 0ull;
    int ok = 1;
    for (unsigned long long at = 0ull; ok && (at < named); at += 1ull)
    {
        ok = gold_squash(&input.lines, &corpora[at], 0ull, corpora[at].count, 1ull, &tables[table_count]);
        table_count += 1ull;
    }
    for (unsigned long long at = 0ull; ok && (at < named); at += 1ull)
    {
        const unsigned long long count_of = corpora[at].count;
        ok = gold_squash(&input.lines, &corpora[at], 0ull, count_of / 2ull, 1ull, &tables[table_count]) &&
             gold_squash(&input.lines, &corpora[at], count_of / 2ull, count_of, 1ull, &tables[table_count + 1ull]);
        table_count += 2ull;
    }
    for (unsigned long long at = 0ull; ok && (at < named); at += 1ull)
    {
        const unsigned long long count_of = corpora[at].count;
        ok = gold_squash(&input.lines, &corpora[at], 0ull, count_of, 2ull, &tables[table_count]) &&
             gold_squash(&input.lines, &corpora[at], 1ull, count_of, 2ull, &tables[table_count + 1ull]);
        table_count += 2ull;
    }
    unsigned long long comparison_count = 0ull;
    int names_fit = 1;
    for (unsigned long long one = 0ull; one < named; one += 1ull)
    {
        for (unsigned long long two = one + 1ull; two < named; two += 1ull)
        {
            GoldComparison *const comparison = &comparisons[comparison_count];
            names_fit = names_fit && gold_named(&comparison->name, corpora[one].name, " to ", corpora[two].name);
            comparison->left = one;
            comparison->right = two;
            comparison_count += 1ull;
        }
    }
    for (unsigned long long at = 0ull; at < named; at += 1ull)
    {
        GoldComparison *const comparison = &comparisons[comparison_count];
        names_fit = names_fit && gold_named(&comparison->name, corpora[at].name, " midpoint", "");
        comparison->left = named + (2ull * at);
        comparison->right = named + (2ull * at) + 1ull;
        comparison_count += 1ull;
    }
    for (unsigned long long at = 0ull; at < named; at += 1ull)
    {
        GoldComparison *const comparison = &comparisons[comparison_count];
        names_fit = names_fit && gold_named(&comparison->name, corpora[at].name, " alternating", "");
        comparison->left = (3ull * named) + (2ull * at);
        comparison->right = (3ull * named) + (2ull * at) + 1ull;
        comparison_count += 1ull;
    }

    for (unsigned long long at = 0ull; ok && (at < paper_count); at += 1ull)
    {
        GoldPaper *const paper = &papers[at];
        paper->text = at;
        paper->whole = table_count;
        paper->own = GOLD_ABSENT;
        paper->reads = 0;
        const unsigned long long count_of = texts[at].count;
        ok = gold_squash(&input.lines, &texts[at], 0ull, count_of, 1ull, &tables[table_count]);
        table_count += 1ull;
        paper->qualified = ok && (tables[paper->whole].total >= least);
        if (paper->qualified)
        {
            paper->first_against = comparison_count;
            for (unsigned long long one = 0ull; one < anchor_count; one += 1ull)
            {
                GoldComparison *const comparison = &comparisons[comparison_count];
                names_fit =
                    names_fit && gold_named(&comparison->name, texts[at].name, " to ", corpora[anchors[one]].name);
                comparison->left = paper->whole;
                comparison->right = anchors[one];
                comparison_count += 1ull;
            }
            const unsigned long long first = table_count;
            ok = gold_squash(&input.lines, &texts[at], 0ull, count_of / 2ull, 1ull, &tables[table_count]) &&
                 gold_squash(&input.lines, &texts[at], count_of / 2ull, count_of, 1ull, &tables[table_count + 1ull]);
            table_count += 2ull;
            if (ok && (count_of >= 2u) && (tables[first].total > 0ull) && (tables[first + 1u].total > 0ull))
            {
                paper->own = comparison_count;
                GoldComparison *const comparison = &comparisons[comparison_count];
                names_fit = names_fit && gold_named(&comparison->name, texts[at].name, " midpoint", "");
                comparison->left = first;
                comparison->right = first + 1ull;
                comparison_count += 1ull;
            }
        }
    }
    if (!ok || !names_fit)
    {
        fprintf(stderr, "  gold_readings: the pool cannot meet a table, or a comparison's name passes %u bytes\n",
                STAGE_TEXT_BYTES);
        return 2;
    }

    // each comparison's cells that either table holds a pair in, counted
    unsigned int max_count = 0u;
    unsigned long long max_total = 0ull;
    unsigned long long run = 1ull;
    int whole = 1;
    for (unsigned long long at = 0ull; at < comparison_count; at += 1ull)
    {
        const GoldTable &left = tables[comparisons[at].left];
        const GoldTable &right = tables[comparisons[at].right];
        unsigned long long occupied = 0ull;
        for (unsigned int cell = 0u; cell < GOLD_CELLS; cell += 1u)
        {
            if ((left.counts[cell] != 0u) || (right.counts[cell] != 0u))
            {
                occupied += 1ull;
                const unsigned int larger =
                    (left.counts[cell] > right.counts[cell]) ? left.counts[cell] : right.counts[cell];
                max_count = (larger > max_count) ? larger : max_count;
            }
        }
        run = (occupied > run) ? occupied : run;
        const unsigned long long total = (left.total > right.total) ? left.total : right.total;
        max_total = (total > max_total) ? total : max_total;
        whole = whole && (left.total > 0ull) && (right.total > 0ull);
    }
    const unsigned int count_bits = stage_bits(stage_whole(max_count));
    const unsigned int total_bits = stage_bits(stage_whole(max_total));
    if (!whole || (total_bits > GOLD_TOTAL_BITS_MAX))
    {
        fprintf(stderr, "  gold_readings: a table is empty or contains more than 2^%u pairs\n", GOLD_TOTAL_BITS_MAX);
        return 2;
    }

    // the distance program: each comparison a run of `run` lanes, its occupied cells first and zeros after
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    Stage distance_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &distance_stage, .bytes = sizeof(distance_stage), .val = 0);
    distance_stage.name = "distance";
    gold_distance_program(count_bits, total_bits, &distance_stage.program);
    const unsigned long long lanes = comparison_count * run;
    const unsigned int in_limbs = (distance_stage.program.record_bits + 31u) / 32u;
    unsigned int *records = (unsigned int *)stage_cell(lanes * in_limbs, sizeof(unsigned int));
    if (records == NULL)
    {
        return 2;
    }
    const unsigned int *field_bits = distance_stage.program.field_bits;
    const unsigned int *field_offset = distance_stage.program.field_offset;
    for (unsigned long long at = 0ull; at < comparison_count; at += 1ull)
    {
        const GoldTable &left = tables[comparisons[at].left];
        const GoldTable &right = tables[comparisons[at].right];
        unsigned long long place = 0ull;
        for (unsigned int cell = 0u; cell < GOLD_CELLS; cell += 1u)
        {
            if ((left.counts[cell] != 0u) || (right.counts[cell] != 0u))
            {
                unsigned int *record = &records[(at * run + place) * in_limbs];
                stage_put(record, field_offset[0], field_bits[0], stage_whole(left.counts[cell]));
                stage_put(record, field_offset[1], field_bits[1], stage_whole(right.counts[cell]));
                place += 1ull;
            }
        }
        for (place = 0ull; place < run; place += 1ull)
        {
            unsigned int *record = &records[(at * run + place) * in_limbs];
            stage_put(record, field_offset[2], field_bits[2], stage_whole(left.total));
            stage_put(record, field_offset[3], field_bits[3], stage_whole(right.total));
        }
    }

    ok = stage_position(&job, &distance_stage, &error);
    // the buffers the program names for itself: the distance stage's records in and out, the max it stores at once
    const unsigned long long declared =
        lanes * ((unsigned long long)in_limbs + distance_stage.layout.out_limbs) * sizeof(unsigned int);
    ok = ok && sim_job_submit(&job, "gold_readings", count, arguments, declared);
    unsigned int *distance_out = NULL;
    unsigned int *device_out = NULL;
    ok = ok && stage_sweep(&job, &distance_stage, records, lanes * in_limbs, lanes, &distance_out, &device_out, &error);
    stage_cell_released(records);
    records = NULL;

    const DeviceRecordStep *apart = ok ? &distance_stage.layout.step_table[distance_stage.program.outputs[0]] : NULL;
    const DeviceRecordStep *twice = ok ? &distance_stage.layout.step_table[distance_stage.program.outputs[1]] : NULL;
    const unsigned int sum_limbs = ok ? ((apart->out_bits + stage_bits(stage_whole(run)) + 31u) / 32u) + 1u : 0u;
    unsigned int *const device_sums = (unsigned int *)stage_cell(comparison_count * sum_limbs, sizeof(unsigned int));
    unsigned int *const host_sums = (unsigned int *)stage_cell(comparison_count * sum_limbs, sizeof(unsigned int));
    ok = ok && (device_sums != NULL) && (host_sums != NULL);
    if (ok)
    {
        const unsigned int out_limbs = distance_stage.layout.out_limbs;
        const CycleRecordSumRequest on_device = {device_out,      lanes,     run,         out_limbs, apart->out_offset,
                                                 apart->out_bits, sum_limbs, device_sums, &error};
        const CycleRecordSumRequest on_host = {distance_out,    lanes,     run,       out_limbs, apart->out_offset,
                                               apart->out_bits, sum_limbs, host_sums, &error};
        ok = (cycle_record_sum(&on_device) != CYCLE_ERROR) && (cycle_record_sum_host(&on_host) != CYCLE_ERROR);
        ok = ok && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device_sums, .other = host_sums,
                               .bytes = comparison_count * sum_limbs * sizeof(unsigned int)) == 0);
    }
    sim_check(&job, ok, "the device's sum of each comparison equals the host's");
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }

    int bounded = ok;
    for (unsigned long long at = 0ull; ok && (at < comparison_count); at += 1ull)
    {
        comparisons[at].distance.num = stage_whole_of(&device_sums[at * sum_limbs], sum_limbs);
        const unsigned int *first = &distance_out[at * run * distance_stage.layout.out_limbs];
        comparisons[at].distance.den = stage_take(first, twice->out_offset, twice->out_bits);
        bounded = bounded && (stage_sign(comparisons[at].distance.num, 32u * sum_limbs) >= 0) &&
                  (stage_order_of(comparisons[at].distance.num, comparisons[at].distance.den) <= 0);
    }
    bounded = bounded && stage_fits("distance");
    sim_check(&job, bounded, "every distance lies in [0, 1]");
    stage_cell_released(distance_out);
    distance_out = NULL;
    ok = ok && bounded;

    // the ratio of each corpus's midpoint split-half distance to its alternating one
    Stage ratio_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &ratio_stage, .bytes = sizeof(ratio_stage), .val = 0);
    ratio_stage.name = "ratio";
    StageRational *const halves = (StageRational *)stage_cell(2ull * named, sizeof(StageRational));
    StageRational *const ratios = (StageRational *)stage_cell(named, sizeof(StageRational));
    ok = ok && (halves != NULL) && (ratios != NULL);
    for (unsigned long long at = 0ull; ok && (at < (2ull * named)); at += 1ull)
    {
        halves[at] = comparisons[pairs + at].distance;
    }
    stage_ratio_program(ok ? stage_widest(halves, 2ull * named) : 1u, &ratio_stage.program);
    StageTable ratio_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&ratio_rows, named, 4u);
    for (unsigned long long at = 0ull; ok && (at < named); at += 1ull)
    {
        const StageRational &mid = comparisons[pairs + at].distance;
        const StageRational &alt = comparisons[pairs + named + at].distance;
        StageWhole *const row = stage_row(&ratio_rows, at);
        row[0] = mid.num;
        row[1] = mid.den;
        row[2] = alt.num;
        row[3] = alt.den;
    }
    StageTable ratio_out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &ratio_stage, &ratio_rows, &ratio_out, &error);
    for (unsigned long long at = 0ull; ok && (at < named); at += 1ull)
    {
        ratios[at] = stage_pair_of(stage_row(&ratio_out, at));
    }

    // The first compare: each pair against each split-half distance, the orders the ranges need, and each paper's
    // distances to the language corpora. The values and names of the orders are cells at their counts
    StageRational *const pair_values = (StageRational *)stage_cell(pairs, sizeof(StageRational));
    StageRational *const midpoint_values = (StageRational *)stage_cell(named, sizeof(StageRational));
    StageRational *const alternating_values = (StageRational *)stage_cell(named, sizeof(StageRational));
    StageRational *const paper_values = (StageRational *)stage_cell(paper_count * anchor_count, sizeof(StageRational));
    const char **const anchor_names = (const char **)stage_cell(anchor_count, sizeof(char *));
    StageOrder *const paper_orders = (StageOrder *)stage_cell(paper_count, sizeof(StageOrder));
    ok = ok && (pair_values != NULL) && (midpoint_values != NULL) && (alternating_values != NULL) &&
         (paper_values != NULL) && (anchor_names != NULL) && (paper_orders != NULL);
    StageOrder pair_order = {pair_values, NULL, pairs, 0ull, NULL};
    StageOrder midpoint_order = {midpoint_values, NULL, named, 0ull, NULL};
    StageOrder alternating_order = {alternating_values, NULL, named, 0ull, NULL};
    StageOrder ratio_order = {ratios, NULL, named, 0ull, NULL};
    StageOrder *orders[4] = {&pair_order, &midpoint_order, &alternating_order, &ratio_order};
    unsigned long long first_count = (4ull * pairs) + stage_order_pairs(pairs) + (3ull * stage_order_pairs(named));
    for (unsigned long long at = 0ull; ok && (at < pairs); at += 1ull)
    {
        pair_values[at] = comparisons[at].distance;
    }
    for (unsigned long long at = 0ull; ok && (at < named); at += 1ull)
    {
        midpoint_values[at] = comparisons[pairs + at].distance;
        alternating_values[at] = comparisons[pairs + named + at].distance;
    }
    for (unsigned long long one = 0ull; ok && (one < anchor_count); one += 1ull)
    {
        anchor_names[one] = corpora[anchors[one]].name;
    }
    for (unsigned long long at = 0ull; ok && (at < paper_count); at += 1ull)
    {
        StageOrder *const paper_order = &paper_orders[at];
        paper_order->values = &paper_values[at * anchor_count];
        paper_order->names = anchor_names;
        paper_order->count = papers[at].qualified ? anchor_count : 0ull;
        for (unsigned long long one = 0ull; one < paper_order->count; one += 1ull)
        {
            paper_values[at * anchor_count + one] = comparisons[papers[at].first_against + one].distance;
        }
        first_count += stage_order_pairs(paper_order->count);
    }
    StageRational *const xs = (StageRational *)stage_cell(first_count, sizeof(StageRational));
    StageRational *const ys = (StageRational *)stage_cell(first_count, sizeof(StageRational));
    int *const margined = (int *)stage_cell(first_count, sizeof(int));
    int *const first_signs = (int *)stage_cell(first_count, sizeof(int));
    ok = ok && (xs != NULL) && (ys != NULL) && (margined != NULL) && (first_signs != NULL);
    unsigned long long first_lanes = 0ull;
    for (unsigned long long at = 0ull; ok && (at < pairs); at += 1ull)
    {
        for (unsigned long long type = 0ull; type < 2ull; type += 1ull)
        {
            const unsigned long long sides[2] = {comparisons[at].left, comparisons[at].right};
            for (unsigned long long side = 0ull; side < 2ull; side += 1ull)
            {
                xs[first_lanes] = comparisons[at].distance;
                ys[first_lanes] = comparisons[pairs + (type * named) + sides[side]].distance;
                margined[first_lanes] = 1;
                first_lanes += 1ull;
            }
        }
    }
    for (unsigned int at = 0u; ok && (at < 4u); at += 1u)
    {
        stage_order_lanes(orders[at], xs, ys, &first_lanes);
    }
    for (unsigned long long at = 0ull; ok && (at < paper_count); at += 1ull)
    {
        if (papers[at].qualified)
        {
            stage_order_lanes(&paper_orders[at], xs, ys, &first_lanes);
        }
    }
    Stage first_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &first_stage, .bytes = sizeof(first_stage), .val = 0);
    ok = ok &&
         stage_compare(&job, &first_stage, "first compare", xs, ys, margined, first_lanes, margin, first_signs, &error);
    for (unsigned int at = 0u; ok && (at < 4u); at += 1u)
    {
        ok = stage_order_read(orders[at], first_signs);
    }
    for (unsigned long long at = 0ull; ok && (at < paper_count); at += 1ull)
    {
        if (papers[at].qualified)
        {
            ok = stage_order_read(&paper_orders[at], first_signs);
        }
    }
    // a qualified paper's order: its anchors, nearest first
    auto paper_anchor = [&](unsigned long long paper, unsigned long long place) -> unsigned long long {
        return anchors[paper_orders[paper].sorted[place]];
    };

    // each reserved paper's gap between its nearest corpus and the runner-up
    Stage gap_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &gap_stage, .bytes = sizeof(gap_stage), .val = 0);
    gap_stage.name = "gap";
    StageRational *const near = (StageRational *)stage_cell(2ull * paper_count, sizeof(StageRational));
    unsigned long long *const gapped = (unsigned long long *)stage_cell(paper_count, sizeof(unsigned long long));
    ok = ok && (near != NULL) && (gapped != NULL);
    unsigned long long gapped_count = 0ull;
    for (unsigned long long at = 0ull; ok && (at < paper_count); at += 1ull)
    {
        if (papers[at].qualified && (anchor_count >= 2ull))
        {
            near[2ull * gapped_count] = comparisons[papers[at].first_against + paper_orders[at].sorted[0]].distance;
            near[2ull * gapped_count + 1ull] =
                comparisons[papers[at].first_against + paper_orders[at].sorted[1]].distance;
            gapped[gapped_count] = at;
            gapped_count += 1ull;
        }
    }
    StageTable gap_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&gap_rows, gapped_count, 4u);
    for (unsigned long long at = 0ull; ok && (at < gapped_count); at += 1ull)
    {
        StageWhole *const row = stage_row(&gap_rows, at);
        row[0] = near[2ull * at].num;
        row[1] = near[2ull * at].den;
        row[2] = near[2ull * at + 1ull].num;
        row[3] = near[2ull * at + 1ull].den;
    }
    stage_gap_program(ok ? stage_widest(near, 2ull * gapped_count) : 1u, &gap_stage.program);
    StageTable gap_out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &gap_stage, &gap_rows, &gap_out, &error);
    for (unsigned long long at = 0ull; ok && (at < gapped_count); at += 1ull)
    {
        GoldPaper &paper = papers[gapped[at]];
        paper.gap = stage_pair_of(stage_row(&gap_out, at));
        paper.floor = (paper.own != GOLD_ABSENT) ? comparisons[paper.own].distance : stage_unit();
    }

    // the second compare: each gap against the margin times its paper's split-half distance
    StageRational *const gap_xs = (StageRational *)stage_cell(gapped_count, sizeof(StageRational));
    StageRational *const gap_ys = (StageRational *)stage_cell(gapped_count, sizeof(StageRational));
    int *const gap_margined = (int *)stage_cell(gapped_count, sizeof(int));
    int *const second_signs = (int *)stage_cell(gapped_count, sizeof(int));
    ok = ok && (gap_xs != NULL) && (gap_ys != NULL) && (gap_margined != NULL) && (second_signs != NULL);
    for (unsigned long long at = 0ull; ok && (at < gapped_count); at += 1ull)
    {
        gap_xs[at] = papers[gapped[at]].gap;
        gap_ys[at] = papers[gapped[at]].floor;
        gap_margined[at] = 1;
    }
    Stage second_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &second_stage, .bytes = sizeof(second_stage), .val = 0);
    ok = ok && stage_compare(&job, &second_stage, "second compare", gap_xs, gap_ys, gap_margined, gapped_count, margin,
                             second_signs, &error);
    for (unsigned long long at = 0ull; ok && (at < gapped_count); at += 1ull)
    {
        papers[gapped[at]].reads = second_signs[at] == 1;
    }

    // the third compare: the orders of the gaps and of the split-half distances over the papers reserved and not read
    StageRational *const gap_values = (StageRational *)stage_cell(gapped_count, sizeof(StageRational));
    StageRational *const floor_values = (StageRational *)stage_cell(gapped_count, sizeof(StageRational));
    ok = ok && (gap_values != NULL) && (floor_values != NULL);
    StageOrder gap_order = {gap_values, NULL, 0ull, 0ull, NULL};
    StageOrder floor_order = {floor_values, NULL, 0ull, 0ull, NULL};
    for (unsigned long long at = 0ull; ok && (at < gapped_count); at += 1ull)
    {
        if (!papers[gapped[at]].reads)
        {
            gap_values[gap_order.count] = papers[gapped[at]].gap;
            floor_values[floor_order.count] = papers[gapped[at]].floor;
            gap_order.count += 1ull;
            floor_order.count += 1ull;
        }
    }
    const unsigned long long third_count = stage_order_pairs(gap_order.count) + stage_order_pairs(floor_order.count);
    StageRational *const third_xs = (StageRational *)stage_cell(third_count, sizeof(StageRational));
    StageRational *const third_ys = (StageRational *)stage_cell(third_count, sizeof(StageRational));
    int *const third_margined = (int *)stage_cell(third_count, sizeof(int));
    int *const third_signs = (int *)stage_cell(third_count, sizeof(int));
    ok = ok && (third_xs != NULL) && (third_ys != NULL) && (third_margined != NULL) && (third_signs != NULL);
    unsigned long long third_lanes = 0ull;
    if (ok)
    {
        stage_order_lanes(&gap_order, third_xs, third_ys, &third_lanes);
        stage_order_lanes(&floor_order, third_xs, third_ys, &third_lanes);
    }
    Stage third_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &third_stage, .bytes = sizeof(third_stage), .val = 0);
    ok = ok && stage_compare(&job, &third_stage, "third compare", third_xs, third_ys, third_margined, third_lanes,
                             unit_margin, third_signs, &error);
    if (ok)
    {
        ok = stage_order_read(&gap_order, third_signs) && stage_order_read(&floor_order, third_signs);
    }

    // the medians, a middle value or the mean of the middle two
    Stage mean_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &mean_stage, .bytes = sizeof(mean_stage), .val = 0);
    mean_stage.name = "mean";
    StageOrder *medians_of[2] = {&gap_order, &floor_order};
    StageRational medians[2] = {stage_unit(), stage_unit()};
    int median_qualified[2] = {0, 0};
    StageTable mean_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&mean_rows, 2ull, 4u);
    unsigned long long mean_count = 0ull;
    unsigned long long mean_for[2] = {0ull, 0ull};
    StageRational mean_values[4];
    for (unsigned int at = 0u; ok && (at < 2u); at += 1u)
    {
        const unsigned long long sorted_size = medians_of[at]->count;
        if (sorted_size == 0ull)
        {
            continue;
        }
        median_qualified[at] = 1;
        const StageRational &upper = medians_of[at]->values[medians_of[at]->sorted[sorted_size / 2ull]];
        if ((sorted_size % 2ull) == 1ull)
        {
            medians[at] = upper;
            continue;
        }
        const StageRational &lower = medians_of[at]->values[medians_of[at]->sorted[(sorted_size / 2ull) - 1ull]];
        StageWhole *const row = stage_row(&mean_rows, mean_count);
        row[0] = lower.num;
        row[1] = lower.den;
        row[2] = upper.num;
        row[3] = upper.den;
        mean_values[2ull * mean_count] = lower;
        mean_values[2ull * mean_count + 1ull] = upper;
        mean_for[mean_count] = at;
        mean_count += 1ull;
    }
    mean_rows.rows = mean_count;
    stage_mean_program(stage_widest(mean_values, 2ull * mean_count), &mean_stage.program);
    StageTable mean_out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &mean_stage, &mean_rows, &mean_out, &error);
    for (unsigned long long at = 0ull; ok && (at < mean_count); at += 1ull)
    {
        medians[mean_for[at]] = stage_pair_of(stage_row(&mean_out, at));
    }

    // every value printed, as its whole part and six digits
    const unsigned long long printed_room = comparison_count + named + (2ull * gapped_count) + 3ull;
    StageRational *const printed = (StageRational *)stage_cell(printed_room, sizeof(StageRational));
    StageText *const decimal = (StageText *)stage_cell(printed_room, sizeof(StageText));
    ok = ok && (printed != NULL) && (decimal != NULL);
    unsigned long long printed_count = 0ull;
    for (unsigned long long at = 0ull; ok && (at < comparison_count); at += 1ull)
    {
        printed[printed_count] = comparisons[at].distance;
        printed_count += 1ull;
    }
    const unsigned long long ratio_at = printed_count;
    for (unsigned long long at = 0ull; ok && (at < named); at += 1ull)
    {
        printed[printed_count] = ratios[at];
        printed_count += 1ull;
    }
    for (unsigned long long at = 0ull; ok && (at < gapped_count); at += 1ull)
    {
        printed[printed_count] = papers[gapped[at]].gap;
        printed[printed_count + 1ull] = papers[gapped[at]].floor;
        printed_count += 2ull;
    }
    const unsigned long long median_at = printed_count;
    if (ok)
    {
        printed[printed_count] = medians[0];
        printed[printed_count + 1ull] = medians[1];
        printed_count += 2ull;
    }

    // how many times the median gap fits inside the median split-half distance
    Stage noise_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &noise_stage, .bytes = sizeof(noise_stage), .val = 0);
    noise_stage.name = "noise ratio";
    stage_ratio_program(stage_widest(medians, 2ull), &noise_stage.program);
    StageTable noise_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&noise_rows, 1ull, 4u);
    noise_rows.rows = 0ull;
    if (ok && median_qualified[0] && median_qualified[1] &&
        (stage_sign(medians[0].num, 32u * medians[0].num.count) > 0))
    {
        StageWhole *const row = stage_row(&noise_rows, 0ull);
        row[0] = medians[1].num;
        row[1] = medians[1].den;
        row[2] = medians[0].num;
        row[3] = medians[0].den;
        noise_rows.rows = 1ull;
    }
    StageTable noise_out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &noise_stage, &noise_rows, &noise_out, &error);
    const unsigned long long noise_at = printed_count;
    if (ok && (noise_out.rows != 0ull))
    {
        printed[printed_count] = stage_pair_of(stage_row(&noise_out, 0ull));
        printed_count += 1ull;
    }
    Stage digits_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &digits_stage, .bytes = sizeof(digits_stage), .val = 0);
    digits_stage.name = "digits";
    stage_digits_program(ok ? stage_widest(printed, printed_count) : 1u, &digits_stage.program);
    StageTable digit_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&digit_rows, printed_count, 2u);
    for (unsigned long long at = 0ull; ok && (at < printed_count); at += 1ull)
    {
        stage_row(&digit_rows, at)[0] = printed[at].num;
        stage_row(&digit_rows, at)[1] = printed[at].den;
    }
    StageTable digit_out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &digits_stage, &digit_rows, &digit_out, &error);
    for (unsigned long long at = 0ull; ok && (at < printed_count); at += 1ull)
    {
        decimal[at] = stage_decimal_of(stage_row(&digit_out, at)[0], stage_row(&digit_out, at)[1]);
    }

    // the counts the readings come to
    unsigned int pairs_read[2] = {0u, 0u};
    int *const pair_reads = (int *)stage_cell(pairs * 2ull, sizeof(int));
    ok = ok && (pair_reads != NULL);
    for (unsigned long long at = 0ull; ok && (at < pairs); at += 1ull)
    {
        for (unsigned long long type = 0ull; type < 2ull; type += 1ull)
        {
            const int reads = (first_signs[(at * 4ull) + (type * 2ull)] == 1) &&
                              (first_signs[(at * 4ull) + (type * 2ull) + 1ull] == 1);
            pair_reads[(at * 2ull) + type] = reads;
            pairs_read[type] += (unsigned int)reads;
        }
    }
    unsigned int small = 0u;
    unsigned int read = 0u;
    unsigned int agreed = 0u;
    for (unsigned long long at = 0ull; ok && (at < paper_count); at += 1ull)
    {
        small += (unsigned int)!papers[at].qualified;
        if (papers[at].reads)
        {
            read += 1u;
            agreed += (unsigned int)stage_same(corpora[paper_anchor(at, 0ull)].name, texts[papers[at].text].language);
        }
    }
    const unsigned int unread = (unsigned int)paper_count - small - read;

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "margin %llu/%llu least %llu\n", margin[0], margin[1], least);
        for (unsigned long long at = 0ull; at < named; at += 1ull)
        {
            fprintf(out, "corpus %s lines %llu pairs %llu\n", corpora[at].name, corpora[at].count, tables[at].total);
        }
        for (unsigned long long at = 0ull; at < comparison_count; at += 1ull)
        {
            fprintf(out, "distance %s = %s / %s\n", comparisons[at].name.bytes,
                    stage_text_of(comparisons[at].distance.num).bytes,
                    stage_text_of(comparisons[at].distance.den).bytes);
        }
        for (unsigned long long at = 0ull; at < named; at += 1ull)
        {
            fprintf(out, "ratio %s midpoint to alternating = %s / %s\n", corpora[at].name,
                    stage_text_of(ratios[at].num).bytes, stage_text_of(ratios[at].den).bytes);
        }
        for (unsigned long long at = 0ull; at < pairs; at += 1ull)
        {
            fprintf(out, "verdict %s midpoint %d %d alternating %d %d\n", comparisons[at].name.bytes,
                    first_signs[at * 4ull], first_signs[(at * 4ull) + 1ull], first_signs[(at * 4ull) + 2ull],
                    first_signs[(at * 4ull) + 3ull]);
        }
        unsigned long long gap_place = 0ull;
        for (unsigned long long at = 0ull; at < paper_count; at += 1ull)
        {
            const GoldPaper &paper = papers[at];
            const GoldText &text = texts[paper.text];
            if (!paper.qualified)
            {
                fprintf(out, "paper %s attributes %s pairs %llu reserved 0\n", text.name, text.language,
                        tables[paper.whole].total);
                continue;
            }
            const int gapped_here = (gap_place < gapped_count) && (gapped[gap_place] == at);
            fprintf(out, "paper %s attributes %s pairs %llu reserved 1 nearest %s", text.name, text.language,
                    tables[paper.whole].total, (anchor_count == 0ull) ? "-" : corpora[paper_anchor(at, 0ull)].name);
            if (gapped_here)
            {
                fprintf(out, " gap %s / %s split %s / %s reads %d", stage_text_of(paper.gap.num).bytes,
                        stage_text_of(paper.gap.den).bytes, stage_text_of(paper.floor.num).bytes,
                        stage_text_of(paper.floor.den).bytes, paper.reads);
                gap_place += 1ull;
            }
            fprintf(out, "\n");
        }
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            if (median_qualified[at])
            {
                fprintf(out, "median %s = %s / %s\n", (at == 0u) ? "gap" : "split",
                        stage_text_of(medians[at].num).bytes, stage_text_of(medians[at].den).bytes);
            }
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, "corpus", 16u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "lines", 8u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "pairs", 10u);
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, "midpoint", 12u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, "alternating", 12u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, "ratio", 10u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < named; at += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, corpora[at].name, 16u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, corpora[at].count, 8u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, tables[at].total, 10u);
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, decimal[pairs + at].bytes, 12u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, decimal[pairs + named + at].bytes, 12u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, decimal[ratio_at + at].bytes, 10u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\n  ");
            mmgr_line_put_columns(output_line, "pair", 34u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, "distance", 10u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, "midpoint", 9u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, "alternating", 9u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < pairs; at += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, comparisons[at].name.bytes, 34u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, decimal[at].bytes, 10u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, pair_reads[at * 2ull] ? "reads" : "does not", 9u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, pair_reads[(at * 2ull) + 1ull] ? "reads" : "does not", 9u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\n  ");
            mmgr_line_decimal(output_line, pairs_read[0], 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, pairs, 1u);
            mmgr_line_put(output_line, " pairs read against the midpoint split-half distances, ");
            mmgr_line_decimal(output_line, pairs_read[1], 1u);
            mmgr_line_put(output_line, " against the alternating");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_decimal(output_line, paper_count, 1u);
            mmgr_line_put(output_line, " papers name a language the corpora cover: ");
            mmgr_line_decimal(output_line, small, 1u);
            mmgr_line_put(output_line, " contain fewer than ");
            mmgr_line_decimal(output_line, least, 1u);
            mmgr_line_put(output_line, " pairs, ");
            mmgr_line_decimal(output_line, read, 1u);
            mmgr_line_put(output_line, " read and agree with their prose on ");
            mmgr_line_decimal(output_line, agreed, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, unread, 1u);
            mmgr_line_put(output_line, " do not read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (median_qualified[0])
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  over the ");
            mmgr_line_decimal(output_line, unread, 1u);
            mmgr_line_put(output_line, " that do not read, the median gap is ");
            mmgr_line_put(output_line, decimal[median_at].bytes);
            mmgr_line_put(output_line, " and the median split-half distance ");
            mmgr_line_put(output_line, decimal[median_at + 1u].bytes);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line,
                          "  each decimal is the whole part and first six digits of the exact rational in the records");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    if (ok && (count == 4))
    {
        FILE *figures = fopen(arguments[3], "w");
        if (figures != NULL)
        {
            const char *const least_pair = decimal[pair_order.sorted[0]].bytes;
            const char *const max_pair = decimal[pair_order.sorted[pairs - 1ull]].bytes;
            fprintf(figures, "\\newcommand{\\GoldProfiles}{%llu}\n", named);
            fprintf(figures, "\\newcommand{\\GoldLanguages}{%llu}\n", anchor_count);
            fprintf(figures, "\\newcommand{\\GoldPairs}{%llu}\n", pairs);
            fprintf(figures, "\\newcommand{\\GoldPairsReadMidpoint}{%u}\n", pairs_read[0]);
            fprintf(figures, "\\newcommand{\\GoldPairsReadAlternating}{%u}\n", pairs_read[1]);
            fprintf(figures, "\\newcommand{\\GoldPairLeast}{%.5s}\n", least_pair);
            fprintf(figures, "\\newcommand{\\GoldPairMost}{%.5s}\n", max_pair);
            fprintf(figures, "\\newcommand{\\GoldSplitLeast}{%.5s}\n", decimal[pairs + midpoint_order.sorted[0]].bytes);
            fprintf(figures, "\\newcommand{\\GoldSplitMost}{%.5s}\n",
                    decimal[pairs + midpoint_order.sorted[named - 1ull]].bytes);
            fprintf(figures, "\\newcommand{\\GoldAlternatingLeast}{%.5s}\n",
                    decimal[pairs + named + alternating_order.sorted[0]].bytes);
            fprintf(figures, "\\newcommand{\\GoldAlternatingMost}{%.5s}\n",
                    decimal[pairs + named + alternating_order.sorted[named - 1ull]].bytes);
            fprintf(figures, "\\newcommand{\\GoldRatioLeast}{%.4s}\n", decimal[ratio_at + ratio_order.sorted[0]].bytes);
            fprintf(figures, "\\newcommand{\\GoldRatioMost}{%.4s}\n",
                    decimal[ratio_at + ratio_order.sorted[named - 1ull]].bytes);
            unsigned int unread_pairs = 0u;
            for (unsigned long long at = 0ull; at < pairs; at += 1ull)
            {
                if (pair_reads[at * 2ull])
                {
                    continue;
                }
                unread_pairs += 1u;
                const GoldComparison &pair = comparisons[at];
                // the side whose midpoint split-half distance the pair did not clear
                const unsigned long long wider = (first_signs[at * 4ull] != 1) ? pair.left : pair.right;
                fprintf(figures, "\\newcommand{\\GoldUnreadLeft}{%s}\n", corpora[pair.left].name);
                fprintf(figures, "\\newcommand{\\GoldUnreadRight}{%s}\n", corpora[pair.right].name);
                fprintf(figures, "\\newcommand{\\GoldUnreadDistance}{%.5s}\n", decimal[at].bytes);
                fprintf(figures, "\\newcommand{\\GoldUnreadWider}{%s}\n", corpora[wider].name);
                fprintf(figures, "\\newcommand{\\GoldUnreadWiderLines}{%llu}\n", corpora[wider].count);
                fprintf(figures, "\\newcommand{\\GoldUnreadSplit}{%.5s}\n", decimal[pairs + wider].bytes);
                fprintf(figures, "\\newcommand{\\GoldUnreadAlternating}{%.5s}\n", decimal[pairs + named + wider].bytes);
            }
            fprintf(figures, "\\newcommand{\\GoldPairsUnread}{%u}\n", unread_pairs);
            fprintf(figures, "\\newcommand{\\GoldPapers}{%llu}\n", paper_count);
            fprintf(figures, "\\newcommand{\\GoldPapersSmall}{%u}\n", small);
            fprintf(figures, "\\newcommand{\\GoldPapersLeast}{%llu}\n", least);
            fprintf(figures, "\\newcommand{\\GoldPapersRead}{%u}\n", read);
            fprintf(figures, "\\newcommand{\\GoldPapersAgreed}{%u}\n", agreed);
            fprintf(figures, "\\newcommand{\\GoldPapersUnread}{%u}\n", unread);
            fprintf(figures, "\\newcommand{\\GoldPapersNotRead}{%u}\n", unread + small);
            fprintf(figures, "\\newcommand{\\GoldMedianGap}{%.6s}\n",
                    median_qualified[0] ? decimal[median_at].bytes : "--");
            fprintf(figures, "\\newcommand{\\GoldMedianSplit}{%.6s}\n",
                    median_qualified[1] ? decimal[median_at + 1u].bytes : "--");
            const char *const noise = (noise_at < printed_count) ? decimal[noise_at].bytes : "--";
            fprintf(figures, "\\newcommand{\\GoldNoiseTimes}{%.*s}\n", gold_whole_length(noise), noise);
            // the pairs of languages alone, English left out: how many, how many read alternating, and how many
            // change verdict between the midpoint reading and the alternating one
            unsigned int language_pairs = 0u;
            unsigned int language_read = 0u;
            unsigned int language_changed = 0u;
            for (unsigned long long at = 0ull; at < pairs; at += 1ull)
            {
                if (stage_same(corpora[comparisons[at].left].name, "English") ||
                    stage_same(corpora[comparisons[at].right].name, "English"))
                {
                    continue;
                }
                language_pairs += 1u;
                language_read += (unsigned int)pair_reads[(at * 2ull) + 1ull];
                language_changed += (unsigned int)(pair_reads[at * 2ull] != pair_reads[(at * 2ull) + 1ull]);
            }
            fprintf(figures, "\\newcommand{\\GoldLanguagePairs}{%u}\n", language_pairs);
            fprintf(figures, "\\newcommand{\\GoldLanguagePairsReadAlternating}{%u}\n", language_read);
            fprintf(figures, "\\newcommand{\\GoldLanguagePairsChanged}{%u}\n", language_changed);
            unsigned long long language_lines = 0ull;
            for (unsigned long long at = 0ull; at < anchor_count; at += 1ull)
            {
                language_lines += corpora[anchors[at]].count;
            }
            fprintf(figures, "\\newcommand{\\GoldLanguageLines}{%llu}\n", language_lines);
            fclose(figures);
        }
        sim_check(&job, figures != NULL, "the figures are written out");
    }

    stage_release(&distance_stage);
    stage_release(&ratio_stage);
    stage_release(&first_stage);
    stage_release(&gap_stage);
    stage_release(&second_stage);
    stage_release(&third_stage);
    stage_release(&mean_stage);
    stage_release(&noise_stage);
    stage_release(&digits_stage);
    StageTable *const used_tables[] = {&digit_out, &digit_rows, &noise_out, &noise_rows, &mean_out,
                                       &mean_rows, &gap_out,    &gap_rows,  &ratio_out,  &ratio_rows};
    for (unsigned int each = 0u; each < (unsigned int)(sizeof(used_tables) / sizeof(used_tables[0])); each += 1u)
    {
        stage_table_released(used_tables[each]);
    }
    StageOrder *const used_orders[] = {&floor_order,       &gap_order,      &ratio_order,
                                       &alternating_order, &midpoint_order, &pair_order};
    for (unsigned int each = 0u; each < (unsigned int)(sizeof(used_orders) / sizeof(used_orders[0])); each += 1u)
    {
        stage_order_released(used_orders[each]);
    }
    for (unsigned long long at = 0ull; (paper_orders != NULL) && (at < paper_count); at += 1ull)
    {
        stage_order_released(&paper_orders[at]);
    }
    void *const cells[] = {pair_reads,
                           decimal,
                           printed,
                           third_signs,
                           third_margined,
                           third_ys,
                           third_xs,
                           floor_values,
                           gap_values,
                           second_signs,
                           gap_margined,
                           gap_ys,
                           gap_xs,
                           gapped,
                           near,
                           first_signs,
                           margined,
                           ys,
                           xs,
                           paper_orders,
                           (void *)anchor_names,
                           paper_values,
                           alternating_values,
                           midpoint_values,
                           pair_values,
                           ratios,
                           halves,
                           host_sums,
                           device_sums};
    for (unsigned int each = 0u; each < (unsigned int)(sizeof(cells) / sizeof(cells[0])); each += 1u)
    {
        stage_cell_released(cells[each]);
    }
    for (unsigned long long at = 0ull; at < table_count; at += 1ull)
    {
        stage_cell_released(tables[at].counts);
    }
    void *const read_cells[] = {papers, comparisons, tables, anchors, input.paper, input.corpus};
    for (unsigned int each = 0u; each < (unsigned int)(sizeof(read_cells) / sizeof(read_cells[0])); each += 1u)
    {
        stage_cell_released(read_cells[each]);
    }
    stage_lines_released(&input.lines);
    return sim_close(&job, "gold_readings");
}
