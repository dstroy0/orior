// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// How a glottalized resonant fares under reduplication, language by language, tallied and read exact on the record
// machine and set against the survey that posed the question.
//
//   Usage:  reduplication_glottalization <input> <output>
//
// The measure is reduplication_glottalization.py's. Each doubled root of a language gives one verdict per
// glottalized resonant in it, IDENTICAL where the resonant is glottalized in both copies and SPLIT where in one.
// With I and S a language's counts of each and T = I + S, the language reads
//
//   IDENTICAL where I / T >= the input's identical share,   SPLIT where I / T <= its split share,   BOTH otherwise,
//
// each an exact comparison of I against T, and its reading is set against Mellesmoen and Urbanczyk's Table 4.
//
// Programs of the record machine, each built by a request that carries only its field widths:
//   tally     one lane a verdict, its IDENTICAL bit, its SPLIT bit and whether it is the first from its paper in,
//             each bit out; the sums over each run of a language's lanes are I, S and its count of papers;
//   reading   I, S and both shares in, COMPARE(I h_den, h_num T), COMPARE(I s_den, s_num T) and T out;
//   digits    the record_stages.h program, for each share I / T printed.
// Marking a paper's first verdict is the host's, and every count, sum, product and comparison is the machine's.
//
// The input is what reduplication_glottalization.py writes. The output contains every language's I, S, papers,
// reading and Table 4 class.
//
// Checks:
//   1. each program imprints and arranges;
//   2. each program loads onto the device and every lane runs it;
//   3. the host's records of each program equal the device's word for word;
//   4. the device's sums of each run equal the host's;
//   5. the records are written out.

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

// a doubled root: its paper, and its verdicts from verdict[first] on, `count` of them, each 1 for IDENTICAL and 0 for
// SPLIT
typedef struct
{
    const char *paper;
    unsigned long long first;
    unsigned long long count;
} DoubledRoot;

// a language: its names, its roots from root[first] on, `count` of them, and what the machine reads of it
typedef struct
{
    const char *name;
    const char *table;
    unsigned long long first;
    unsigned long long count;
    StageWhole identical;
    StageWhole split;
    StageWhole papers;
    const char *reading;
    StageText share;
} DoubledLanguage;

// The input read once: its lines, and its verdicts, roots and languages, each a cell of the temporary tier at the
// count of its lines, which bounds them all before they fill
typedef struct
{
    StageLines lines;
    unsigned char *verdict;
    unsigned long long verdicts;
    DoubledRoot *root;
    unsigned long long roots;
    DoubledLanguage *language;
    unsigned long long languages;
} DoubledInput;

static void doubled_released(DoubledInput *input)
{
    stage_cell_released(input->language);
    stage_cell_released(input->root);
    stage_cell_released(input->verdict);
    stage_lines_released(&input->lines);
}

static int doubled_read(const char *path, unsigned long long shares[2][2], DoubledInput *input)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = input, .bytes = sizeof(*input), .val = 0);
    if (!stage_lines(path, &input->lines) || (input->lines.count < 2u))
    {
        return 0;
    }
    const unsigned long long bound = input->lines.size + 1ull;
    input->verdict = (unsigned char *)stage_cell(bound, 1u);
    input->root = (DoubledRoot *)stage_cell(input->lines.count, sizeof(DoubledRoot));
    input->language = (DoubledLanguage *)stage_cell(input->lines.count, sizeof(DoubledLanguage));
    if ((input->verdict == NULL) || (input->root == NULL) || (input->language == NULL))
    {
        return 0;
    }
    const char *heads[2] = {"identical", "split"};
    StageFields fields;
    for (unsigned long long at = 0ull; at < 2ull; at += 1ull)
    {
        stage_split(input->lines.line[at], &fields);
        if (!stage_fields_are(&fields, 3u) || !stage_same(fields.field[0], heads[at]))
        {
            return 0;
        }
        shares[at][0] = strtoull(fields.field[1], NULL, 10);
        shares[at][1] = strtoull(fields.field[2], NULL, 10);
    }
    unsigned long long at = 2ull;
    while (at < input->lines.count)
    {
        stage_split(input->lines.line[at], &fields);
        if (!stage_fields_are(&fields, 4u) || !stage_same(fields.field[0], "language"))
        {
            return 0;
        }
        DoubledLanguage *const language = &input->language[input->languages];
        input->languages += 1ull;
        language->name = fields.field[1];
        language->table = fields.field[2];
        language->first = input->roots;
        const unsigned long long count = strtoull(fields.field[3], NULL, 10);
        at += 1ull;
        for (unsigned long long one = 0ull; one < count; one += 1ull, at += 1ull)
        {
            if (at >= input->lines.count)
            {
                return 0;
            }
            stage_split(input->lines.line[at], &fields);
            if (!stage_fields_are(&fields, 2u))
            {
                return 0;
            }
            DoubledRoot *const root = &input->root[input->roots];
            input->roots += 1ull;
            root->paper = fields.field[0];
            root->first = input->verdicts;
            // the verdicts split in place at their commas, each IDENTICAL or SPLIT
            char *verdict = fields.field[1];
            for (;;)
            {
                char *const comma = (char *)EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = verdict,
                                                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ',');
                if (comma != NULL)
                {
                    *comma = '\0';
                }
                const int identical = stage_same(verdict, "IDENTICAL");
                if (!identical && !stage_same(verdict, "SPLIT"))
                {
                    return 0;
                }
                input->verdict[input->verdicts] = (unsigned char)identical;
                input->verdicts += 1ull;
                if (comma == NULL)
                {
                    break;
                }
                verdict = comma + 1;
            }
            root->count = input->verdicts - root->first;
        }
        language->count = input->roots - language->first;
    }
    return (shares[0][1] > 0ull) && (shares[1][1] > 0ull);
}

// the roots the order of papers sorts, for memor.sort
static const DoubledRoot *s_doubled_roots;

// the order of two roots by their papers; a stable sort keeps the roots of one paper in their order
static int doubled_paper_order(const void *left, const void *right)
{
    return stage_text_order(s_doubled_roots[*(const unsigned long long *)left].paper,
                            s_doubled_roots[*(const unsigned long long *)right].paper);
}

// each of three one-bit fields passed through, for their sums over a language's run
static void doubled_tally_program(StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int bit = 1u;
    const unsigned int identical = stage_field(program, bit);
    const unsigned int split = stage_field(program, bit);
    const unsigned int first = stage_field(program, bit);
    const unsigned int zero = stage_constant(program, 0ull);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, identical, zero));
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, split, zero));
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, first, zero));
}

// COMPARE(I h_den, h_num T) and COMPARE(I s_den, s_num T), with T = I + S, and T
static void doubled_reading_program(unsigned int bits, unsigned int share_bits, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int identical = stage_field(program, bits);
    const unsigned int split = stage_field(program, bits);
    const unsigned int high_num = stage_field(program, share_bits);
    const unsigned int high_den = stage_field(program, share_bits);
    const unsigned int low_num = stage_field(program, share_bits);
    const unsigned int low_den = stage_field(program, share_bits);
    const unsigned int total = stage_step(program, ENGINE_RECORD_SUM, identical, split);
    stage_output(program, stage_step(program, ENGINE_RECORD_COMPARE,
                                     stage_step(program, ENGINE_RECORD_PRODUCT, identical, high_den),
                                     stage_step(program, ENGINE_RECORD_PRODUCT, high_num, total)));
    stage_output(program, stage_step(program, ENGINE_RECORD_COMPARE,
                                     stage_step(program, ENGINE_RECORD_PRODUCT, identical, low_den),
                                     stage_step(program, ENGINE_RECORD_PRODUCT, low_num, total)));
    stage_output(program, total);
}

int program_main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if (count != 3)
    {
        fprintf(stderr, "  usage: reduplication_glottalization <input> <output>\n");
        return 2;
    }
    unsigned long long shares[2][2] = {{0ull, 0ull}, {0ull, 0ull}};
    DoubledInput input;
    if (!doubled_read(arguments[1], shares, &input) || (input.languages == 0ull))
    {
        fprintf(stderr, "  reduplication_glottalization: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }
    DoubledLanguage *const languages = input.language;
    const unsigned long long language_count = input.languages;

    // the tally program: each language's verdicts, padded to the longest run with lanes that contain no bit
    unsigned long long run = 1ull;
    for (unsigned long long at = 0ull; at < language_count; at += 1ull)
    {
        unsigned long long verdicts = 0ull;
        for (unsigned long long root = 0ull; root < languages[at].count; root += 1ull)
        {
            verdicts += input.root[languages[at].first + root].count;
        }
        run = (verdicts > run) ? verdicts : run;
    }
    // each root's mark, 1 where it is the first root of its paper in its language: the roots of each language sorted
    // stably by paper, and the first of each run of one paper marked
    unsigned char *const first_of_paper = (unsigned char *)stage_cell(input.roots, 1u);
    unsigned long long *const by_paper = (unsigned long long *)stage_cell(input.roots, sizeof(unsigned long long));
    unsigned long long *const by_paper_room = (unsigned long long *)stage_cell(input.roots, sizeof(unsigned long long));
    int ok = (first_of_paper != NULL) && (by_paper != NULL) && (by_paper_room != NULL);
    s_doubled_roots = input.root;
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        const unsigned long long first = languages[at].first;
        const unsigned long long roots = languages[at].count;
        for (unsigned long long root = 0ull; root < roots; root += 1ull)
        {
            by_paper[root] = first + root;
        }
        EMBED_CALL(memor.sort, MemoriaCfg, .dst = by_paper, .bytes = sizeof(unsigned long long),
                   .scratch = by_paper_room, .count = roots, .order = doubled_paper_order);
        for (unsigned long long place = 0ull; place < roots; place += 1ull)
        {
            first_of_paper[by_paper[place]] =
                (unsigned char)((place == 0ull) || !stage_same(input.root[by_paper[place - 1ull]].paper,
                                                               input.root[by_paper[place]].paper));
        }
    }
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    Stage tally_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &tally_stage, .bytes = sizeof(tally_stage), .val = 0);
    tally_stage.name = "tally";
    doubled_tally_program(&tally_stage.program);
    ok = ok && stage_position(&job, &tally_stage, &error);
    const unsigned long long lanes = language_count * run;
    const unsigned int limbs = (tally_stage.program.record_bits + 31u) / 32u;
    unsigned int *const records = (unsigned int *)stage_cell(lanes * limbs, sizeof(unsigned int));
    ok = ok && (records != NULL);
    const StageWhole one = stage_whole(1ull);
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        unsigned long long lane = at * run;
        for (unsigned long long root = 0ull; root < languages[at].count; root += 1ull)
        {
            const unsigned long long root_at = languages[at].first + root;
            const DoubledRoot *const doubled = &input.root[root_at];
            for (unsigned long long verdict = 0ull; verdict < doubled->count; verdict += 1ull, lane += 1ull)
            {
                unsigned int *record = &records[lane * limbs];
                const unsigned int field = (input.verdict[doubled->first + verdict] != 0u) ? 0u : 1u;
                stage_put(record, tally_stage.program.field_offset[field], 1u, one);
                if ((verdict == 0ull) && (first_of_paper[root_at] != 0u))
                {
                    stage_put(record, tally_stage.program.field_offset[2], 1u, one);
                }
            }
        }
    }
    ok = ok && sim_job_submit(&job, "reduplication_glottalization", count, arguments,
                              lanes * (limbs + (ok ? tally_stage.layout.out_limbs : 1u)) * sizeof(unsigned int));
    StageTable tallies = {NULL, 0ull, 0u};
    const unsigned int tally_outputs[3] = {0u, 1u, 2u};
    ok =
        ok && stage_summed(&job, &tally_stage, records, lanes * limbs, lanes, run, tally_outputs, 3u, &tallies, &error);
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        languages[at].identical = stage_row(&tallies, 0ull)[at];
        languages[at].split = stage_row(&tallies, 1ull)[at];
        languages[at].papers = stage_row(&tallies, 2ull)[at];
    }

    // the reading program: each language's share against both bounds
    Stage reading_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &reading_stage, .bytes = sizeof(reading_stage), .val = 0);
    reading_stage.name = "reading";
    unsigned int share_bits = 1u;
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const unsigned int num_bits = stage_bits(stage_whole(shares[at][0]));
        const unsigned int den_bits = stage_bits(stage_whole(shares[at][1]));
        const unsigned int wider = (num_bits > den_bits) ? num_bits : den_bits;
        share_bits = (wider > share_bits) ? wider : share_bits;
    }
    doubled_reading_program(stage_bits(stage_whole(run)) + 1u, share_bits, &reading_stage.program);
    StageTable rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&rows, language_count, 6u);
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        StageWhole *const row = stage_row(&rows, at);
        row[0] = languages[at].identical;
        row[1] = languages[at].split;
        row[2] = stage_whole(shares[0][0]);
        row[3] = stage_whole(shares[0][1]);
        row[4] = stage_whole(shares[1][0]);
        row[5] = stage_whole(shares[1][1]);
    }
    StageTable readings = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &reading_stage, &rows, &readings, &error);

    // the digits program: each share
    Stage digits_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &digits_stage, .bytes = sizeof(digits_stage), .val = 0);
    digits_stage.name = "digits";
    stage_digits_program(stage_bits(stage_whole(run)) + 1u, &digits_stage.program);
    StageTable digit_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&digit_rows, language_count, 2u);
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        stage_row(&digit_rows, at)[0] = languages[at].identical;
        stage_row(&digit_rows, at)[1] = stage_row(&readings, at)[2];
    }
    StageTable digits = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &digits_stage, &digit_rows, &digits, &error);

    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        const unsigned int high_bits = reading_stage.layout.step_table[reading_stage.program.outputs[0]].out_bits;
        const unsigned int low_bits = reading_stage.layout.step_table[reading_stage.program.outputs[1]].out_bits;
        const int high = stage_sign(stage_row(&readings, at)[0], high_bits);
        const int low = stage_sign(stage_row(&readings, at)[1], low_bits);
        languages[at].reading = (high >= 0) ? "IDENTICAL" : ((low <= 0) ? "SPLIT" : "BOTH");
        languages[at].share = stage_decimal_of(stage_row(&digits, at)[0], stage_row(&digits, at)[1]);
    }

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "identical %llu/%llu split %llu/%llu\n", shares[0][0], shares[0][1], shares[1][0], shares[1][1]);
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            const DoubledLanguage &language = languages[at];
            fprintf(out, "language %s table %s identical %s split %s papers %s reading %s\n", language.name,
                    language.table, stage_text_of(language.identical).bytes, stage_text_of(language.split).bytes,
                    stage_text_of(language.papers).bytes, language.reading);
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, stage_displayed("language", 18u, 1).bytes);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, "Table 4", 9u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "IDENTICAL", 9u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "SPLIT", 6u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "share", 9u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "papers", 6u);
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, "reading");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            const DoubledLanguage &language = languages[at];
            const int judged = !stage_same(language.table, "-") && !stage_same(language.table, "UNCLEAR");
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, stage_displayed(language.name, 18u, 1).bytes);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_columns(output_line, language.table, 9u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, stage_text_of(language.identical).bytes, 9u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, stage_text_of(language.split).bytes, 6u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, language.share.bytes, 9u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, stage_text_of(language.papers).bytes, 6u);
                mmgr_line_put(output_line, "  ");
                mmgr_line_put_columns(output_line, language.reading, 9u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put(output_line,
                              judged ? (stage_same(language.reading, language.table) ? "agrees" : "DISAGREES") : "");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }

    stage_release(&tally_stage);
    stage_release(&reading_stage);
    stage_release(&digits_stage);
    stage_table_released(&digits);
    stage_table_released(&digit_rows);
    stage_table_released(&readings);
    stage_table_released(&rows);
    stage_table_released(&tallies);
    void *const cells[4] = {records, by_paper_room, by_paper, first_of_paper};
    for (unsigned int each = 0u; each < 4u; each += 1u)
    {
        stage_cell_released(cells[each]);
    }
    doubled_released(&input);
    return sim_close(&job, "reduplication_glottalization");
}
