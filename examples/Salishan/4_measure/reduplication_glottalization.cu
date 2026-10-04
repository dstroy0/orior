// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// How a glottalized resonant fares under reduplication, language by language, tallied and read exact on the record
// machine and set against the survey that asked.
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
// The input is what reduplication_glottalization.py writes. The output holds every language's I, S, papers,
// reading and Table 4 class.
//
// Checks:
//   1. each program imprints and lays out;
//   2. each program loads onto the device and every lane runs it;
//   3. the host's records of each program equal the device's word for word;
//   4. the device's sums of each run equal the host's;
//   5. the records are written out.

#include "../../../src/c/engine/analysis/cycle/cycle.h"
#include "../../../src/c/engine/analysis/key_schedule/key_schedule.h"
#include "../../../src/c/engine/analysis/keymath/keymath.h"
#include "../../../src/c/engine/runtime/scriptura/scriptura.h"
#include "sim.h"

#include "record_stages.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

typedef struct
{
    std::string paper;
    std::vector<std::string> verdicts;
} DoubledRoot;

typedef struct
{
    std::string name;
    std::string table;
    std::vector<DoubledRoot> roots;
    StageWhole identical;
    StageWhole split;
    StageWhole papers;
    std::string reading;
    std::string share;
} DoubledLanguage;

static int doubled_read(const char *path, unsigned long long shares[2][2], std::vector<DoubledLanguage> *languages)
{
    std::vector<std::string> lines;
    if (!stage_lines(path, &lines) || (lines.size() < 2u))
    {
        return 0;
    }
    const char *heads[2] = {"identical", "split"};
    for (size_t at = 0u; at < 2u; at += 1u)
    {
        const std::vector<std::string> fields = stage_split(lines[at]);
        if ((fields.size() != 3u) || (fields[0] != heads[at]))
        {
            return 0;
        }
        shares[at][0] = strtoull(fields[1].c_str(), NULL, 10);
        shares[at][1] = strtoull(fields[2].c_str(), NULL, 10);
    }
    size_t at = 2u;
    while (at < lines.size())
    {
        const std::vector<std::string> head = stage_split(lines[at]);
        if ((head.size() != 4u) || (head[0] != "language"))
        {
            return 0;
        }
        DoubledLanguage language;
        language.name = head[1];
        language.table = head[2];
        const unsigned long long count = strtoull(head[3].c_str(), NULL, 10);
        at += 1u;
        for (unsigned long long one = 0ull; one < count; one += 1ull, at += 1u)
        {
            const std::vector<std::string> fields = (at < lines.size()) ? stage_split(lines[at]) : std::vector<std::string>();
            if (fields.size() != 2u)
            {
                return 0;
            }
            DoubledRoot root;
            root.paper = fields[0];
            size_t start = 0u;
            while (start <= fields[1].size())
            {
                size_t end = fields[1].find(',', start);
                end = (end == std::string::npos) ? fields[1].size() : end;
                const std::string verdict = fields[1].substr(start, end - start);
                if ((verdict != "IDENTICAL") && (verdict != "SPLIT"))
                {
                    return 0;
                }
                root.verdicts.push_back(verdict);
                start = end + 1u;
            }
            language.roots.push_back(root);
        }
        languages->push_back(language);
    }
    return (shares[0][1] > 0ull) && (shares[1][1] > 0ull);
}

// each of three one-bit fields passed through, for their sums over a language's run
static void doubled_tally_program(StageProgram *program)
{
    memset(program, 0, sizeof(*program));
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
    memset(program, 0, sizeof(*program));
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

int main(int count, char **arguments)
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
    std::vector<DoubledLanguage> languages;
    if (!doubled_read(arguments[1], shares, &languages) || languages.empty())
    {
        fprintf(stderr, "  reduplication_glottalization: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    // the tally program: each language's verdicts, padded to the longest run with lanes that hold no bit
    size_t run = 1u;
    for (size_t at = 0u; at < languages.size(); at += 1u)
    {
        size_t verdicts = 0u;
        for (size_t root = 0u; root < languages[at].roots.size(); root += 1u)
        {
            verdicts += languages[at].roots[root].verdicts.size();
        }
        run = std::max(run, verdicts);
    }
    EngineError error;
    memset(&error, 0, sizeof(error));
    Stage tally_stage;
    memset(&tally_stage, 0, sizeof(tally_stage));
    tally_stage.name = "tally";
    doubled_tally_program(&tally_stage.program);
    int ok = stage_lay(&job, &tally_stage, &error);
    const unsigned long long lanes = (unsigned long long)languages.size() * run;
    const unsigned int limbs = (tally_stage.program.record_bits + 31u) / 32u;
    std::vector<unsigned int> records((size_t)(lanes * limbs), 0u);
    const StageWhole one = stage_whole(1ull);
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        std::set<std::string> seen;
        size_t lane = at * run;
        for (size_t root = 0u; root < languages[at].roots.size(); root += 1u)
        {
            const DoubledRoot &doubled = languages[at].roots[root];
            for (size_t verdict = 0u; verdict < doubled.verdicts.size(); verdict += 1u, lane += 1u)
            {
                unsigned int *record = &records[lane * limbs];
                const unsigned int field = (doubled.verdicts[verdict] == "IDENTICAL") ? 0u : 1u;
                stage_put(record, tally_stage.program.field_offset[field], 1u, one);
                if (seen.insert(doubled.paper).second)
                {
                    stage_put(record, tally_stage.program.field_offset[2], 1u, one);
                }
            }
        }
    }
    ok = ok && sim_job_submit(&job, "reduplication_glottalization", count, arguments,
                              lanes * (limbs + (ok ? tally_stage.layout.out_limbs : 1u)) * sizeof(unsigned int));
    std::vector<std::vector<StageWhole>> tallies;
    ok = ok && stage_summed(&job, &tally_stage, records, lanes, run, {0u, 1u, 2u}, &tallies, &error);
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        languages[at].identical = tallies[0][at];
        languages[at].split = tallies[1][at];
        languages[at].papers = tallies[2][at];
    }

    // the reading program: each language's share against both bounds
    Stage reading_stage;
    memset(&reading_stage, 0, sizeof(reading_stage));
    reading_stage.name = "reading";
    unsigned int share_bits = 1u;
    for (size_t at = 0u; at < 2u; at += 1u)
    {
        share_bits = std::max(share_bits, std::max(stage_bits(stage_whole(shares[at][0])), stage_bits(stage_whole(shares[at][1]))));
    }
    doubled_reading_program(stage_bits(stage_whole(run)) + 1u, share_bits, &reading_stage.program);
    StageRows rows;
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        rows.push_back({languages[at].identical, languages[at].split, stage_whole(shares[0][0]),
                        stage_whole(shares[0][1]), stage_whole(shares[1][0]), stage_whole(shares[1][1])});
    }
    std::vector<std::vector<StageWhole>> readings;
    ok = ok && stage_rows(&job, &reading_stage, rows, &readings, &error);

    // the digits program: each share
    Stage digits_stage;
    memset(&digits_stage, 0, sizeof(digits_stage));
    digits_stage.name = "digits";
    stage_digits_program(stage_bits(stage_whole(run)) + 1u, &digits_stage.program);
    StageRows digit_rows;
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        digit_rows.push_back({languages[at].identical, readings[at][2]});
    }
    std::vector<std::vector<StageWhole>> digits;
    ok = ok && stage_rows(&job, &digits_stage, digit_rows, &digits, &error);

    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        const unsigned int high_bits = reading_stage.layout.step_table[reading_stage.program.outputs[0]].out_bits;
        const unsigned int low_bits = reading_stage.layout.step_table[reading_stage.program.outputs[1]].out_bits;
        const int high = stage_sign(readings[at][0], high_bits);
        const int low = stage_sign(readings[at][1], low_bits);
        languages[at].reading = (high >= 0) ? "IDENTICAL" : ((low <= 0) ? "SPLIT" : "BOTH");
        char fraction[16];
        snprintf(fraction, sizeof(fraction), "%06u", digits[at][1][0]);
        languages[at].share = stage_text_of(digits[at][0]) + "." + fraction;
    }

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "identical %llu/%llu split %llu/%llu\n", shares[0][0], shares[0][1], shares[1][0], shares[1][1]);
        for (size_t at = 0u; at < languages.size(); at += 1u)
        {
            const DoubledLanguage &language = languages[at];
            fprintf(out, "language %s table %s identical %s split %s papers %s reading %s\n", language.name.c_str(),
                    language.table.c_str(), stage_text_of(language.identical).c_str(),
                    stage_text_of(language.split).c_str(), stage_text_of(language.papers).c_str(),
                    language.reading.c_str());
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        printf("%s %-9s %9s %6s %9s %6s  %s\n", stage_shown("language", 18u, 1).c_str(), "Table 4", "IDENTICAL",
               "SPLIT", "share", "papers", "reading");
        for (size_t at = 0u; at < languages.size(); at += 1u)
        {
            const DoubledLanguage &language = languages[at];
            const int judged = (language.table != "-") && (language.table != "UNCLEAR");
            printf("%s %-9s %9s %6s %9s %6s  %-9s %s\n", stage_shown(language.name, 18u, 1).c_str(),
                   language.table.c_str(), stage_text_of(language.identical).c_str(),
                   stage_text_of(language.split).c_str(), language.share.c_str(),
                   stage_text_of(language.papers).c_str(), language.reading.c_str(),
                   judged ? ((language.reading == language.table) ? "agrees" : "DISAGREES") : "");
        }
    }

    stage_release(&tally_stage);
    stage_release(&reading_stage);
    stage_release(&digits_stage);
    return sim_close(&job, "reduplication_glottalization");
}
