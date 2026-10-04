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
// holding no pair, has split-half distance 1, as self_distance gives it.
//
// What is read:
//   a pair of corpora reads where D clears the larger split-half distance of the two by the input's margin m;
//   a paper is read only where it holds at least the input's least count of pairs. Its distances to the language
//     corpora, English left out, are ordered with ties broken by name. The gap is the runner-up's distance less the
//     nearest's, and the paper reads where the gap clears m times its own midpoint split-half distance, as
//     language_check.py asks it;
//   the medians of the gaps and of the split-half distances over the papers held and not read, each the middle
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
// The output holds the records whole. The figures, where named, are the TeX macros the Salishan research paper reads.
//
// Checks:
//   1. each program imprints and lays out;
//   2. each program loads onto the device and every lane runs it;
//   3. the host's records of each program equal the device's word for word;
//   4. the device's sum of each comparison's run equals the host's;
//   5. every distance lies in [0, 1], its numerator at most its denominator;
//   6. the records are written out.

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
#include <string>
#include <vector>

#define GOLD_CELLS 65536u

// the widest total a field holds, so that 2 A B fits 64 bits
#define GOLD_TOTAL_BITS_MOST 31u


typedef struct
{
    std::string name;
    std::string language;
    std::vector<std::string> lines;
} GoldText;

// a text's pair counts over the 2^16 cells, and how many pairs there are in all
typedef struct
{
    std::vector<unsigned int> counts;
    unsigned long long total;
} GoldTable;

// two tables measured against each other, and the distance the distance program gives them
typedef struct
{
    std::string name;
    size_t left;
    size_t right;
    StageRational distance;
} GoldComparison;


// a paper's reading
typedef struct
{
    size_t text;
    size_t whole;
    int held;
    std::vector<size_t> against;
    size_t own;
    std::vector<size_t> order;
    StageRational floor;
    StageRational gap;
    int reads;
} GoldPaper;


// |a B - b A| and 2 A B for one cell of one comparison
static void gold_distance_program(unsigned int count_bits, unsigned int total_bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
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


// the pairs of the lines `pick` lists, each line's bytes read alone
static GoldTable gold_squash(const std::vector<std::string> &lines, const std::vector<size_t> &pick)
{
    GoldTable table;
    table.counts.assign(GOLD_CELLS, 0u);
    table.total = 0ull;
    for (size_t at = 0u; at < pick.size(); at += 1u)
    {
        const std::string &line = lines[pick[at]];
        for (size_t place = 0u; (place + 1u) < line.size(); place += 1u)
        {
            const unsigned int cell = ((unsigned int)(unsigned char)line[place] << 8u) |
                                      (unsigned int)(unsigned char)line[place + 1u];
            table.counts[cell] += 1u;
            table.total += 1ull;
        }
    }
    return table;
}


static int gold_block(const std::vector<std::string> &lines, size_t *at, const char *kind, int with_language,
                      GoldText *text)
{
    const std::string &head = lines[*at];
    const std::string opening = std::string(kind) + " ";
    if (head.compare(0u, opening.size(), opening) != 0)
    {
        return 0;
    }
    const size_t last = head.rfind(' ');
    if (last <= opening.size())
    {
        return 0;
    }
    std::string named = head.substr(opening.size(), last - opening.size());
    if (with_language)
    {
        const size_t space = named.find(' ');
        if (space == std::string::npos)
        {
            return 0;
        }
        text->language = named.substr(space + 1u);
        named = named.substr(0u, space);
    }
    text->name = named;
    const unsigned long long count = strtoull(head.c_str() + last + 1u, NULL, 10);
    *at += 1u;
    if ((*at + count) > lines.size())
    {
        return 0;
    }
    text->lines.assign(lines.begin() + (long long)*at, lines.begin() + (long long)(*at + count));
    *at += (size_t)count;
    return 1;
}

static int gold_read(const char *path, std::vector<GoldText> *corpora, std::vector<GoldText> *papers,
                     unsigned long long margin[2], unsigned long long *least)
{
    FILE *in = fopen(path, "rb");
    if (in == NULL)
    {
        return 0;
    }
    std::string text;
    char chunk[65536];
    size_t got = 0u;
    while ((got = fread(chunk, 1u, sizeof(chunk), in)) > 0u)
    {
        text.append(chunk, got);
    }
    fclose(in);
    std::vector<std::string> lines;
    size_t start = 0u;
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
        {
            end = text.size();
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1u;
    }
    if ((lines.size() < 2u) || (sscanf(lines[0].c_str(), "margin %llu %llu", &margin[0], &margin[1]) != 2) ||
        (margin[0] == 0ull) || (margin[1] == 0ull) || (sscanf(lines[1].c_str(), "least %llu", least) != 1))
    {
        return 0;
    }
    size_t at = 2u;
    while (at < lines.size())
    {
        GoldText one;
        if (lines[at].compare(0u, 7u, "corpus ") == 0)
        {
            if (!gold_block(lines, &at, "corpus", 0, &one))
            {
                return 0;
            }
            corpora->push_back(one);
        }
        else if (lines[at].compare(0u, 6u, "paper ") == 0)
        {
            if (!gold_block(lines, &at, "paper", 1, &one))
            {
                return 0;
            }
            papers->push_back(one);
        }
        else
        {
            return 0;
        }
    }
    return corpora->size() >= 2u;
}


int main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if ((count != 3) && (count != 4))
    {
        fprintf(stderr, "  usage: gold_readings <input> <output> [<figures>]\n");
        return 2;
    }
    std::vector<GoldText> corpora;
    std::vector<GoldText> texts;
    unsigned long long margin[2] = {0ull, 0ull};
    unsigned long long least = 0ull;
    if (!gold_read(arguments[1], &corpora, &texts, margin, &least))
    {
        fprintf(stderr, "  gold_readings: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }
    const unsigned long long unit_margin[2] = {1ull, 1ull};

    // the tables: each corpus whole, its midpoint halves and its alternating halves, then each paper whole and its
    // midpoint halves
    const size_t named = corpora.size();
    std::vector<GoldTable> tables;
    for (size_t at = 0u; at < named; at += 1u)
    {
        tables.push_back(gold_squash(corpora[at].lines, stage_run(0u, corpora[at].lines.size(), 1u)));
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        const size_t lines = corpora[at].lines.size();
        tables.push_back(gold_squash(corpora[at].lines, stage_run(0u, lines / 2u, 1u)));
        tables.push_back(gold_squash(corpora[at].lines, stage_run(lines / 2u, lines, 1u)));
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        const size_t lines = corpora[at].lines.size();
        tables.push_back(gold_squash(corpora[at].lines, stage_run(0u, lines, 2u)));
        tables.push_back(gold_squash(corpora[at].lines, stage_run(1u, lines, 2u)));
    }
    std::vector<size_t> anchors;
    for (size_t at = 0u; at < named; at += 1u)
    {
        if (corpora[at].name != "English")
        {
            anchors.push_back(at);
        }
    }

    std::vector<GoldComparison> comparisons;
    for (size_t one = 0u; one < named; one += 1u)
    {
        for (size_t two = one + 1u; two < named; two += 1u)
        {
            comparisons.push_back({corpora[one].name + " to " + corpora[two].name, one, two, {}});
        }
    }
    const size_t pairs = comparisons.size();
    for (size_t at = 0u; at < named; at += 1u)
    {
        comparisons.push_back({corpora[at].name + " midpoint", named + (2u * at), named + (2u * at) + 1u, {}});
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        comparisons.push_back(
            {corpora[at].name + " alternating", (3u * named) + (2u * at), (3u * named) + (2u * at) + 1u, {}});
    }

    std::vector<GoldPaper> papers;
    for (size_t at = 0u; at < texts.size(); at += 1u)
    {
        GoldPaper paper;
        paper.text = at;
        paper.whole = tables.size();
        paper.own = (size_t)-1;
        paper.reads = 0;
        const size_t lines = texts[at].lines.size();
        tables.push_back(gold_squash(texts[at].lines, stage_run(0u, lines, 1u)));
        paper.held = tables.back().total >= least;
        if (paper.held)
        {
            for (size_t one = 0u; one < anchors.size(); one += 1u)
            {
                paper.against.push_back(comparisons.size());
                comparisons.push_back({texts[at].name + " to " + corpora[anchors[one]].name, paper.whole,
                                       anchors[one], {}});
            }
            const size_t first = tables.size();
            tables.push_back(gold_squash(texts[at].lines, stage_run(0u, lines / 2u, 1u)));
            tables.push_back(gold_squash(texts[at].lines, stage_run(lines / 2u, lines, 1u)));
            if ((lines >= 2u) && (tables[first].total > 0ull) && (tables[first + 1u].total > 0ull))
            {
                paper.own = comparisons.size();
                comparisons.push_back({texts[at].name + " midpoint", first, first + 1u, {}});
            }
        }
        papers.push_back(paper);
    }

    unsigned int most_count = 0u;
    unsigned long long most_total = 0ull;
    size_t run = 1u;
    int whole = 1;
    std::vector<std::vector<unsigned int>> cells(comparisons.size());
    for (size_t at = 0u; at < comparisons.size(); at += 1u)
    {
        const GoldTable &left = tables[comparisons[at].left];
        const GoldTable &right = tables[comparisons[at].right];
        for (unsigned int cell = 0u; cell < GOLD_CELLS; cell += 1u)
        {
            if ((left.counts[cell] != 0u) || (right.counts[cell] != 0u))
            {
                cells[at].push_back(cell);
                most_count = std::max(most_count, std::max(left.counts[cell], right.counts[cell]));
            }
        }
        run = std::max(run, cells[at].size());
        most_total = std::max(most_total, std::max(left.total, right.total));
        whole = whole && (left.total > 0ull) && (right.total > 0ull);
    }
    const unsigned int count_bits = stage_bits(stage_whole(most_count));
    const unsigned int total_bits = stage_bits(stage_whole(most_total));
    if (!whole || (total_bits > GOLD_TOTAL_BITS_MOST))
    {
        fprintf(stderr, "  gold_readings: a table is empty or holds more than 2^%u pairs\n", GOLD_TOTAL_BITS_MOST);
        return 2;
    }

    // the distance program: each comparison a run of `run` lanes, its occupied cells first and zeros after
    EngineError error;
    memset(&error, 0, sizeof(error));
    Stage distance_stage;
    memset(&distance_stage, 0, sizeof(distance_stage));
    distance_stage.name = "distance";
    gold_distance_program(count_bits, total_bits, &distance_stage.program);
    const unsigned long long lanes = (unsigned long long)comparisons.size() * run;
    const unsigned int in_limbs = (distance_stage.program.record_bits + 31u) / 32u;
    std::vector<unsigned int> records((size_t)(lanes * in_limbs), 0u);
    const unsigned int *field_bits = distance_stage.program.field_bits;
    const unsigned int *field_offset = distance_stage.program.field_offset;
    for (size_t at = 0u; at < comparisons.size(); at += 1u)
    {
        const GoldTable &left = tables[comparisons[at].left];
        const GoldTable &right = tables[comparisons[at].right];
        for (size_t place = 0u; place < run; place += 1u)
        {
            unsigned int *record = &records[(size_t)(((unsigned long long)at * run + place) * in_limbs)];
            if (place < cells[at].size())
            {
                stage_put(record, field_offset[0], field_bits[0], stage_whole(left.counts[cells[at][place]]));
                stage_put(record, field_offset[1], field_bits[1], stage_whole(right.counts[cells[at][place]]));
            }
            stage_put(record, field_offset[2], field_bits[2], stage_whole(left.total));
            stage_put(record, field_offset[3], field_bits[3], stage_whole(right.total));
        }
    }

    int ok = stage_lay(&job, &distance_stage, &error);
    // the buffers the program names for itself: the distance stage's records in and out, the most it holds at once
    const unsigned long long declared =
        lanes * ((unsigned long long)in_limbs + distance_stage.layout.out_limbs) * sizeof(unsigned int);
    ok = ok && sim_job_submit(&job, "gold_readings", count, arguments, declared);
    std::vector<unsigned int> distance_out;
    unsigned int *device_out = NULL;
    ok = ok && stage_sweep(&job, &distance_stage, records, lanes, &distance_out, &device_out, &error);
    records.clear();
    records.shrink_to_fit();

    const DeviceRecordStep *apart = ok ? &distance_stage.layout.step_table[distance_stage.program.outputs[0]] : NULL;
    const DeviceRecordStep *twice = ok ? &distance_stage.layout.step_table[distance_stage.program.outputs[1]] : NULL;
    const unsigned int sum_limbs = ok ? ((apart->out_bits + stage_bits(stage_whole(run)) + 31u) / 32u) + 1u : 0u;
    std::vector<unsigned int> device_sums(comparisons.size() * sum_limbs, 0u);
    std::vector<unsigned int> host_sums(comparisons.size() * sum_limbs, 0u);
    if (ok)
    {
        const unsigned int out_limbs = distance_stage.layout.out_limbs;
        const CycleRecordSumRequest on_device = {device_out, lanes, run, out_limbs, apart->out_offset, apart->out_bits,
                                                 sum_limbs, device_sums.data(), &error};
        const CycleRecordSumRequest on_host = {distance_out.data(), lanes, run, out_limbs, apart->out_offset,
                                               apart->out_bits, sum_limbs, host_sums.data(), &error};
        ok = (cycle_record_sum(&on_device) != CYCLE_ERROR) && (cycle_record_sum_host(&on_host) != CYCLE_ERROR);
        ok = ok && (memcmp(device_sums.data(), host_sums.data(), device_sums.size() * sizeof(unsigned int)) == 0);
    }
    sim_check(&job, ok, "the device's sum of each comparison equals the host's");
    if (device_out != NULL)
    {
        cudaFree(device_out);
    }

    int bounded = ok;
    for (size_t at = 0u; ok && (at < comparisons.size()); at += 1u)
    {
        comparisons[at].distance.num.assign(device_sums.begin() + (long long)(at * sum_limbs),
                                            device_sums.begin() + (long long)((at + 1u) * sum_limbs));
        const unsigned int *first = &distance_out[(size_t)((unsigned long long)at * run * distance_stage.layout.out_limbs)];
        comparisons[at].distance.den = stage_take(first, twice->out_offset, twice->out_bits);
        bounded = bounded && (stage_sign(comparisons[at].distance.num, 32u * sum_limbs) >= 0) &&
                  (stage_order_of(comparisons[at].distance.num, comparisons[at].distance.den) <= 0);
    }
    sim_check(&job, bounded, "every distance lies in [0, 1]");
    distance_out.clear();
    distance_out.shrink_to_fit();
    ok = ok && bounded;

    // the ratio of each corpus's midpoint split-half distance to its alternating one
    Stage ratio_stage;
    memset(&ratio_stage, 0, sizeof(ratio_stage));
    ratio_stage.name = "ratio";
    std::vector<StageRational> halves;
    for (size_t at = 0u; at < (2u * named); at += 1u)
    {
        halves.push_back(comparisons[pairs + at].distance);
    }
    stage_ratio_program(stage_widest(halves), &ratio_stage.program);
    StageRows ratio_rows;
    for (size_t at = 0u; at < named; at += 1u)
    {
        const StageRational &mid = comparisons[pairs + at].distance;
        const StageRational &alt = comparisons[pairs + named + at].distance;
        ratio_rows.push_back({mid.num, mid.den, alt.num, alt.den});
    }
    std::vector<std::vector<StageWhole>> ratio_out;
    ok = ok && stage_rows(&job, &ratio_stage, ratio_rows, &ratio_out, &error);
    std::vector<StageRational> ratios;
    for (size_t at = 0u; ok && (at < named); at += 1u)
    {
        ratios.push_back(stage_pair_of(ratio_out[at]));
    }

    // the first compare: each pair against each split-half distance, the orders the ranges need, and each paper's
    // distances to the language corpora
    std::vector<StageRational> xs;
    std::vector<StageRational> ys;
    std::vector<int> margined;
    for (size_t at = 0u; at < pairs; at += 1u)
    {
        for (size_t kind = 0u; kind < 2u; kind += 1u)
        {
            const size_t sides[2] = {comparisons[at].left, comparisons[at].right};
            for (size_t side = 0u; side < 2u; side += 1u)
            {
                xs.push_back(comparisons[at].distance);
                ys.push_back(comparisons[pairs + (kind * named) + sides[side]].distance);
                margined.push_back(1);
            }
        }
    }
    StageOrder pair_order;
    StageOrder midpoint_order;
    StageOrder alternating_order;
    StageOrder ratio_order;
    for (size_t at = 0u; at < pairs; at += 1u)
    {
        pair_order.values.push_back(comparisons[at].distance);
    }
    for (size_t at = 0u; at < named; at += 1u)
    {
        midpoint_order.values.push_back(comparisons[pairs + at].distance);
        alternating_order.values.push_back(comparisons[pairs + named + at].distance);
    }
    ratio_order.values = ratios;
    StageOrder *orders[4] = {&pair_order, &midpoint_order, &alternating_order, &ratio_order};
    for (size_t at = 0u; at < 4u; at += 1u)
    {
        stage_order_lanes(orders[at], &xs, &ys);
    }
    std::vector<StageOrder> paper_orders(papers.size());
    for (size_t at = 0u; at < papers.size(); at += 1u)
    {
        if (!papers[at].held)
        {
            continue;
        }
        for (size_t one = 0u; one < anchors.size(); one += 1u)
        {
            paper_orders[at].values.push_back(comparisons[papers[at].against[one]].distance);
            paper_orders[at].names.push_back(corpora[anchors[one]].name);
        }
        stage_order_lanes(&paper_orders[at], &xs, &ys);
    }
    margined.resize(xs.size(), 0);
    Stage first_stage;
    memset(&first_stage, 0, sizeof(first_stage));
    std::vector<int> first_signs;
    ok = ok && stage_compare(&job, &first_stage, "first compare", xs, ys, margined, margin, &first_signs, &error);
    for (size_t at = 0u; ok && (at < 4u); at += 1u)
    {
        stage_order_read(orders[at], first_signs);
    }
    for (size_t at = 0u; ok && (at < papers.size()); at += 1u)
    {
        if (papers[at].held)
        {
            stage_order_read(&paper_orders[at], first_signs);
            for (size_t one = 0u; one < paper_orders[at].sorted.size(); one += 1u)
            {
                papers[at].order.push_back(anchors[paper_orders[at].sorted[one]]);
            }
        }
    }

    // each held paper's gap between its nearest corpus and the runner-up
    Stage gap_stage;
    memset(&gap_stage, 0, sizeof(gap_stage));
    gap_stage.name = "gap";
    std::vector<StageRational> near;
    StageRows gap_rows;
    std::vector<size_t> gapped;
    for (size_t at = 0u; ok && (at < papers.size()); at += 1u)
    {
        if (papers[at].held && (papers[at].order.size() >= 2u))
        {
            const StageRational &one = comparisons[papers[at].against[paper_orders[at].sorted[0]]].distance;
            const StageRational &two = comparisons[papers[at].against[paper_orders[at].sorted[1]]].distance;
            near.push_back(one);
            near.push_back(two);
            gap_rows.push_back({one.num, one.den, two.num, two.den});
            gapped.push_back(at);
        }
    }
    stage_gap_program(stage_widest(near), &gap_stage.program);
    std::vector<std::vector<StageWhole>> gap_out;
    ok = ok && stage_rows(&job, &gap_stage, gap_rows, &gap_out, &error);
    for (size_t at = 0u; ok && (at < gapped.size()); at += 1u)
    {
        GoldPaper &paper = papers[gapped[at]];
        paper.gap = stage_pair_of(gap_out[at]);
        paper.floor = (paper.own != (size_t)-1) ? comparisons[paper.own].distance : stage_unit();
    }

    // the second compare: each gap against the margin times its paper's split-half distance
    std::vector<StageRational> gap_xs;
    std::vector<StageRational> gap_ys;
    for (size_t at = 0u; at < gapped.size(); at += 1u)
    {
        gap_xs.push_back(papers[gapped[at]].gap);
        gap_ys.push_back(papers[gapped[at]].floor);
    }
    Stage second_stage;
    memset(&second_stage, 0, sizeof(second_stage));
    std::vector<int> second_signs;
    ok = ok && stage_compare(&job, &second_stage, "second compare", gap_xs, gap_ys, std::vector<int>(gap_xs.size(), 1),
                            margin, &second_signs, &error);
    for (size_t at = 0u; ok && (at < gapped.size()); at += 1u)
    {
        papers[gapped[at]].reads = second_signs[at] == 1;
    }

    // the third compare: the orders of the gaps and of the split-half distances over the papers held and not read
    StageOrder gap_order;
    StageOrder floor_order;
    for (size_t at = 0u; at < gapped.size(); at += 1u)
    {
        if (!papers[gapped[at]].reads)
        {
            gap_order.values.push_back(papers[gapped[at]].gap);
            floor_order.values.push_back(papers[gapped[at]].floor);
        }
    }
    std::vector<StageRational> third_xs;
    std::vector<StageRational> third_ys;
    stage_order_lanes(&gap_order, &third_xs, &third_ys);
    stage_order_lanes(&floor_order, &third_xs, &third_ys);
    Stage third_stage;
    memset(&third_stage, 0, sizeof(third_stage));
    std::vector<int> third_signs;
    ok = ok && stage_compare(&job, &third_stage, "third compare", third_xs, third_ys,
                            std::vector<int>(third_xs.size(), 0), unit_margin, &third_signs, &error);
    if (ok)
    {
        stage_order_read(&gap_order, third_signs);
        stage_order_read(&floor_order, third_signs);
    }

    // the medians, a middle value or the mean of the middle two
    Stage mean_stage;
    memset(&mean_stage, 0, sizeof(mean_stage));
    mean_stage.name = "mean";
    StageOrder *medians_of[2] = {&gap_order, &floor_order};
    StageRational medians[2] = {stage_unit(), stage_unit()};
    int median_held[2] = {0, 0};
    StageRows mean_rows;
    std::vector<size_t> mean_for;
    std::vector<StageRational> mean_values;
    for (size_t at = 0u; ok && (at < 2u); at += 1u)
    {
        const size_t held = medians_of[at]->sorted.size();
        if (held == 0u)
        {
            continue;
        }
        median_held[at] = 1;
        const StageRational &upper = medians_of[at]->values[medians_of[at]->sorted[held / 2u]];
        if ((held % 2u) == 1u)
        {
            medians[at] = upper;
            continue;
        }
        const StageRational &lower = medians_of[at]->values[medians_of[at]->sorted[(held / 2u) - 1u]];
        mean_rows.push_back({lower.num, lower.den, upper.num, upper.den});
        mean_values.push_back(lower);
        mean_values.push_back(upper);
        mean_for.push_back(at);
    }
    stage_mean_program(stage_widest(mean_values), &mean_stage.program);
    std::vector<std::vector<StageWhole>> mean_out;
    ok = ok && stage_rows(&job, &mean_stage, mean_rows, &mean_out, &error);
    for (size_t at = 0u; ok && (at < mean_for.size()); at += 1u)
    {
        medians[mean_for[at]] = stage_pair_of(mean_out[at]);
    }

    // every value printed, as its whole part and six digits
    std::vector<StageRational> printed;
    for (size_t at = 0u; at < comparisons.size(); at += 1u)
    {
        printed.push_back(comparisons[at].distance);
    }
    const size_t ratio_at = printed.size();
    printed.insert(printed.end(), ratios.begin(), ratios.end());

    for (size_t at = 0u; at < gapped.size(); at += 1u)
    {
        printed.push_back(papers[gapped[at]].gap);
        printed.push_back(papers[gapped[at]].floor);
    }
    const size_t median_at = printed.size();
    printed.push_back(medians[0]);
    printed.push_back(medians[1]);

    // how many times the median gap fits inside the median split-half distance
    Stage noise_stage;
    memset(&noise_stage, 0, sizeof(noise_stage));
    noise_stage.name = "noise ratio";
    std::vector<StageRational> noise_values = {medians[0], medians[1]};
    stage_ratio_program(stage_widest(noise_values), &noise_stage.program);
    StageRows noise_rows;
    if (median_held[0] && median_held[1] && (stage_sign(medians[0].num, 32u * (unsigned int)medians[0].num.size()) > 0))
    {
        noise_rows.push_back({medians[1].num, medians[1].den, medians[0].num, medians[0].den});
    }
    std::vector<std::vector<StageWhole>> noise_out;
    ok = ok && stage_rows(&job, &noise_stage, noise_rows, &noise_out, &error);
    const size_t noise_at = printed.size();
    if (ok && !noise_out.empty())
    {
        printed.push_back(stage_pair_of(noise_out[0]));
    }
    Stage digits_stage;
    memset(&digits_stage, 0, sizeof(digits_stage));
    digits_stage.name = "digits";
    stage_digits_program(stage_widest(printed), &digits_stage.program);
    StageRows digit_rows;
    for (size_t at = 0u; at < printed.size(); at += 1u)
    {
        digit_rows.push_back({printed[at].num, printed[at].den});
    }
    std::vector<std::vector<StageWhole>> digit_out;
    ok = ok && stage_rows(&job, &digits_stage, digit_rows, &digit_out, &error);
    std::vector<std::string> decimal(printed.size());
    for (size_t at = 0u; ok && (at < printed.size()); at += 1u)
    {
        // the six digits are below 10^6, which the first limb holds
        char fraction[32];
        snprintf(fraction, sizeof(fraction), "%06u", digit_out[at][1][0]);
        decimal[at] = stage_text_of(digit_out[at][0]) + "." + fraction;
    }

    // the counts the readings come to
    unsigned int pairs_read[2] = {0u, 0u};
    std::vector<int> pair_reads(pairs * 2u, 0);
    for (size_t at = 0u; ok && (at < pairs); at += 1u)
    {
        for (size_t kind = 0u; kind < 2u; kind += 1u)
        {
            const int reads = (first_signs[(at * 4u) + (kind * 2u)] == 1) &&
                              (first_signs[(at * 4u) + (kind * 2u) + 1u] == 1);
            pair_reads[(at * 2u) + kind] = reads;
            pairs_read[kind] += (unsigned int)reads;
        }
    }
    unsigned int small = 0u;
    unsigned int read = 0u;
    unsigned int agreed = 0u;
    for (size_t at = 0u; at < papers.size(); at += 1u)
    {
        small += (unsigned int)!papers[at].held;
        if (papers[at].reads)
        {
            read += 1u;
            agreed += (unsigned int)(corpora[papers[at].order[0]].name == texts[papers[at].text].language);
        }
    }
    const unsigned int unread = (unsigned int)papers.size() - small - read;

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "margin %llu/%llu least %llu\n", margin[0], margin[1], least);
        for (size_t at = 0u; at < named; at += 1u)
        {
            fprintf(out, "corpus %s lines %zu pairs %llu\n", corpora[at].name.c_str(), corpora[at].lines.size(),
                    tables[at].total);
        }
        for (size_t at = 0u; at < comparisons.size(); at += 1u)
        {
            fprintf(out, "distance %s = %s / %s\n", comparisons[at].name.c_str(),
                    stage_text_of(comparisons[at].distance.num).c_str(),
                    stage_text_of(comparisons[at].distance.den).c_str());
        }
        for (size_t at = 0u; at < named; at += 1u)
        {
            fprintf(out, "ratio %s midpoint to alternating = %s / %s\n", corpora[at].name.c_str(),
                    stage_text_of(ratios[at].num).c_str(), stage_text_of(ratios[at].den).c_str());
        }
        for (size_t at = 0u; at < pairs; at += 1u)
        {
            fprintf(out, "verdict %s midpoint %d %d alternating %d %d\n", comparisons[at].name.c_str(),
                    first_signs[at * 4u], first_signs[(at * 4u) + 1u], first_signs[(at * 4u) + 2u],
                    first_signs[(at * 4u) + 3u]);
        }
        size_t gap_place = 0u;
        for (size_t at = 0u; at < papers.size(); at += 1u)
        {
            const GoldPaper &paper = papers[at];
            const GoldText &text = texts[paper.text];
            if (!paper.held)
            {
                fprintf(out, "paper %s says %s pairs %llu held 0\n", text.name.c_str(), text.language.c_str(),
                        tables[paper.whole].total);
                continue;
            }
            const int gapped_here = (gap_place < gapped.size()) && (gapped[gap_place] == at);
            fprintf(out, "paper %s says %s pairs %llu held 1 nearest %s", text.name.c_str(), text.language.c_str(),
                    tables[paper.whole].total, paper.order.empty() ? "-" : corpora[paper.order[0]].name.c_str());
            if (gapped_here)
            {
                fprintf(out, " gap %s / %s split %s / %s reads %d", stage_text_of(paper.gap.num).c_str(),
                        stage_text_of(paper.gap.den).c_str(), stage_text_of(paper.floor.num).c_str(),
                        stage_text_of(paper.floor.den).c_str(), paper.reads);
                gap_place += 1u;
            }
            fprintf(out, "\n");
        }
        for (size_t at = 0u; at < 2u; at += 1u)
        {
            if (median_held[at])
            {
                fprintf(out, "median %s = %s / %s\n", (at == 0u) ? "gap" : "split", stage_text_of(medians[at].num).c_str(),
                        stage_text_of(medians[at].den).c_str());
            }
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        printf("  %-16s %8s %10s  %-12s %-12s %-10s\n", "corpus", "lines", "pairs", "midpoint", "alternating", "ratio");
        for (size_t at = 0u; at < named; at += 1u)
        {
            printf("  %-16s %8zu %10llu  %-12s %-12s %-10s\n", corpora[at].name.c_str(), corpora[at].lines.size(),
                   tables[at].total, decimal[pairs + at].c_str(), decimal[pairs + named + at].c_str(),
                   decimal[ratio_at + at].c_str());
        }
        printf("\n  %-34s %-10s %-9s %-9s\n", "pair", "distance", "midpoint", "alternating");
        for (size_t at = 0u; at < pairs; at += 1u)
        {
            printf("  %-34s %-10s %-9s %-9s\n", comparisons[at].name.c_str(), decimal[at].c_str(),
                   pair_reads[at * 2u] ? "reads" : "does not", pair_reads[(at * 2u) + 1u] ? "reads" : "does not");
        }
        printf("\n  %u of %zu pairs read against the midpoint split-half distances, %u against the alternating\n",
               pairs_read[0], pairs, pairs_read[1]);
        printf("  %zu papers name a language the corpora cover: %u hold fewer than %llu pairs, %u read and agree with"
               " their prose on %u, %u do not read\n",
               papers.size(), small, least, read, agreed, unread);
        if (median_held[0])
        {
            printf("  over the %u that do not read, the median gap is %s and the median split-half distance %s\n",
                   unread, decimal[median_at].c_str(), decimal[median_at + 1u].c_str());
        }
        printf("  each decimal is the whole part and first six digits of the exact rational in the records\n");
    }

    if (ok && (count == 4))
    {
        FILE *figures = fopen(arguments[3], "w");
        if (figures != NULL)
        {
            const std::string *least_pair = &decimal[pair_order.sorted.front()];
            const std::string *most_pair = &decimal[pair_order.sorted.back()];
            fprintf(figures, "\\newcommand{\\GoldProfiles}{%zu}\n", named);
            fprintf(figures, "\\newcommand{\\GoldLanguages}{%zu}\n", anchors.size());
            fprintf(figures, "\\newcommand{\\GoldPairs}{%zu}\n", pairs);
            fprintf(figures, "\\newcommand{\\GoldPairsReadMidpoint}{%u}\n", pairs_read[0]);
            fprintf(figures, "\\newcommand{\\GoldPairsReadAlternating}{%u}\n", pairs_read[1]);
            fprintf(figures, "\\newcommand{\\GoldPairLeast}{%.5s}\n", least_pair->c_str());
            fprintf(figures, "\\newcommand{\\GoldPairMost}{%.5s}\n", most_pair->c_str());
            fprintf(figures, "\\newcommand{\\GoldSplitLeast}{%.5s}\n",
                    decimal[pairs + midpoint_order.sorted.front()].c_str());
            fprintf(figures, "\\newcommand{\\GoldSplitMost}{%.5s}\n",
                    decimal[pairs + midpoint_order.sorted.back()].c_str());
            fprintf(figures, "\\newcommand{\\GoldAlternatingLeast}{%.5s}\n",
                    decimal[pairs + named + alternating_order.sorted.front()].c_str());
            fprintf(figures, "\\newcommand{\\GoldAlternatingMost}{%.5s}\n",
                    decimal[pairs + named + alternating_order.sorted.back()].c_str());
            fprintf(figures, "\\newcommand{\\GoldRatioLeast}{%.4s}\n", decimal[ratio_at + ratio_order.sorted.front()].c_str());
            fprintf(figures, "\\newcommand{\\GoldRatioMost}{%.4s}\n", decimal[ratio_at + ratio_order.sorted.back()].c_str());
            unsigned int unread_pairs = 0u;
            for (size_t at = 0u; at < pairs; at += 1u)
            {
                if (pair_reads[at * 2u])
                {
                    continue;
                }
                unread_pairs += 1u;
                const GoldComparison &pair = comparisons[at];
                // the side whose midpoint split-half distance the pair did not clear
                const size_t wider = (first_signs[at * 4u] != 1) ? pair.left : pair.right;
                fprintf(figures, "\\newcommand{\\GoldUnreadLeft}{%s}\n", corpora[pair.left].name.c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadRight}{%s}\n", corpora[pair.right].name.c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadDistance}{%.5s}\n", decimal[at].c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadWider}{%s}\n", corpora[wider].name.c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadWiderLines}{%zu}\n", corpora[wider].lines.size());
                fprintf(figures, "\\newcommand{\\GoldUnreadSplit}{%.5s}\n", decimal[pairs + wider].c_str());
                fprintf(figures, "\\newcommand{\\GoldUnreadAlternating}{%.5s}\n", decimal[pairs + named + wider].c_str());
            }
            fprintf(figures, "\\newcommand{\\GoldPairsUnread}{%u}\n", unread_pairs);
            fprintf(figures, "\\newcommand{\\GoldPapers}{%zu}\n", papers.size());
            fprintf(figures, "\\newcommand{\\GoldPapersSmall}{%u}\n", small);
            fprintf(figures, "\\newcommand{\\GoldPapersLeast}{%llu}\n", least);
            fprintf(figures, "\\newcommand{\\GoldPapersRead}{%u}\n", read);
            fprintf(figures, "\\newcommand{\\GoldPapersAgreed}{%u}\n", agreed);
            fprintf(figures, "\\newcommand{\\GoldPapersUnread}{%u}\n", unread);
            fprintf(figures, "\\newcommand{\\GoldPapersNotRead}{%u}\n", unread + small);
            fprintf(figures, "\\newcommand{\\GoldMedianGap}{%.6s}\n", median_held[0] ? decimal[median_at].c_str() : "--");
            fprintf(figures, "\\newcommand{\\GoldMedianSplit}{%.6s}\n",
                    median_held[1] ? decimal[median_at + 1u].c_str() : "--");
            const std::string noise = (noise_at < decimal.size()) ? decimal[noise_at] : std::string("--");
            fprintf(figures, "\\newcommand{\\GoldNoiseTimes}{%s}\n", noise.substr(0u, noise.find('.')).c_str());
            // the pairs of languages alone, English left out: how many, how many read alternating, and how many
            // change verdict between the midpoint reading and the alternating one
            unsigned int language_pairs = 0u;
            unsigned int language_read = 0u;
            unsigned int language_changed = 0u;
            for (size_t at = 0u; at < pairs; at += 1u)
            {
                if ((corpora[comparisons[at].left].name == "English") ||
                    (corpora[comparisons[at].right].name == "English"))
                {
                    continue;
                }
                language_pairs += 1u;
                language_read += (unsigned int)pair_reads[(at * 2u) + 1u];
                language_changed += (unsigned int)(pair_reads[at * 2u] != pair_reads[(at * 2u) + 1u]);
            }
            fprintf(figures, "\\newcommand{\\GoldLanguagePairs}{%u}\n", language_pairs);
            fprintf(figures, "\\newcommand{\\GoldLanguagePairsReadAlternating}{%u}\n", language_read);
            fprintf(figures, "\\newcommand{\\GoldLanguagePairsChanged}{%u}\n", language_changed);
            size_t language_lines = 0u;
            for (size_t at = 0u; at < anchors.size(); at += 1u)
            {
                language_lines += corpora[anchors[at]].lines.size();
            }
            fprintf(figures, "\\newcommand{\\GoldLanguageLines}{%zu}\n", language_lines);
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
    return sim_close(&job, "gold_readings");
}
