// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The known-answer run of the Salishan experiments, exact on the record machine: does meaning-matched vocabulary
// from the oracle tables recover the accepted Salish subgrouping?
//
//   Usage:  subgrouping <input> <output>
//
// The measure is subgrouping_check.py's. Two forms of one meaning match when their first two Dolgopolsky classes
// agree and they come from different papers. For a pair of languages with at least the input's least count of
// shared meanings, m is how many of the n shared meanings match. Each of P shuffles pairs every meaning of one
// language with a meaning of the other drawn without replacement, and m_k is that shuffle's count. Then
//
//   excess = m / n - S / (n P), with S the sum of the m_k,   and   p = (a + 1) / (P + 1),
//
// with a the number of shuffles where m_k >= m. Both are exact rationals. The excess is carried as excess + 1,
// (m P - S + n P) / (n P), which is never negative since each m_k is at most n. Orders and means are unchanged by
// the shift, and the excess is printed with its sign by the signed digits program.
//
// The criteria are subgrouping_check.py's, fixed before its first run, with (d) as it records the change:
//   (a) each Interior language's highest-excess Salish partner is Interior;
//   (b) each Central Salish language's highest-excess Salish partner is Central Salish;
//   (c) the mean excess among Interior pairs and among Central pairs each exceeds the mean between an Interior and
//       a Central language;
//   (d) no pair of an outside language and a Salish language has p below the input's p_below.
// A highest partner is taken as Python's max takes it: the largest excess, and among equals the last name.
// Nuxalk's mean excess with each branch is reported beside them, which is P5.
//
// Programs of the record machine, each built by a request that carries only its field widths:
//   count       one lane a 32-meaning word of one shuffle of one pair, the bits of matching meanings in, how many
//               are set out; the sum over each run of a shuffle's words is m_k, and the run of the identity is m;
//   above       one lane a shuffle, m_k and m in, [m_k >= m] and m_k out; the sums over each pair's run are a and S;
//   value       one lane a pair, m, S, n, P and a in, excess + 1 and p out;
//   add         a / b and c / d in, (a d + c b) / (b d) out, run as a tree over each group's values;
//   scale       a / b and k in, a / (b k) out, a group's sum made its mean;
//   compare     the record_stages.h program, for the partners' orders, the means and (d);
//   signed      a value in, the sign, whole part and first six digits of the value less one out.
// Drawing the shuffles and building the match bits are the host's, and every count, sum, product, quotient and
// comparison is the machine's. The shuffles are drawn with sim_draw, keyed by the input's seed, the pair and the
// shuffle. They are not Python's draws, and a p here is a p over different shuffles of the same null.
//
// The input is what subgrouping.py writes. The output holds every pair's m, S, n, P and a, its excess + 1 and p
// whole, and the verdicts.
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
#include <map>
#include <set>
#include <string>
#include <vector>

// a form of one meaning: its first two classes and the paper it came from
typedef struct
{
    std::string classes;
    std::string paper;
} GroupForm;

typedef struct
{
    std::string branch;
    std::string name;
    std::map<std::string, std::vector<GroupForm>> meanings;
} GroupLanguage;

// one pair of languages compared, and what the machine gives it
typedef struct
{
    size_t left;
    size_t right;
    size_t shared;
    std::vector<std::vector<unsigned char>> matches;
    StageWhole matched;
    StageWhole shuffled_sum;
    StageWhole above;
    StageRational lifted;
    StageRational chance;
    std::string excess_text;
} GroupPair;

typedef struct
{
    unsigned long long permutations;
    unsigned long long seed;
    unsigned long long least_shared;
    unsigned long long least_meanings;
    unsigned long long p_below[2];
} GroupSettings;

static int group_read(const char *path, GroupSettings *settings, std::vector<GroupLanguage> *languages)
{
    std::vector<std::string> lines;
    if (!stage_lines(path, &lines))
    {
        return 0;
    }
    const char *heads[5] = {"permutations", "seed", "least_shared", "least_meanings", "p_below"};
    unsigned long long *values[4] = {&settings->permutations, &settings->seed, &settings->least_shared,
                                     &settings->least_meanings};
    if (lines.size() < 5u)
    {
        return 0;
    }
    for (size_t at = 0u; at < 5u; at += 1u)
    {
        const std::vector<std::string> fields = stage_split(lines[at]);
        if ((fields.size() < 2u) || (fields[0] != heads[at]))
        {
            return 0;
        }
        if (at < 4u)
        {
            *values[at] = strtoull(fields[1].c_str(), NULL, 10);
        }
        else if (fields.size() == 3u)
        {
            settings->p_below[0] = strtoull(fields[1].c_str(), NULL, 10);
            settings->p_below[1] = strtoull(fields[2].c_str(), NULL, 10);
        }
        else
        {
            return 0;
        }
    }
    size_t at = 5u;
    while (at < lines.size())
    {
        const std::vector<std::string> head = stage_split(lines[at]);
        if ((head.size() != 4u) || (head[0] != "language"))
        {
            return 0;
        }
        GroupLanguage language;
        language.branch = head[1];
        language.name = head[2];
        const unsigned long long count = strtoull(head[3].c_str(), NULL, 10);
        at += 1u;
        for (unsigned long long one = 0ull; one < count; one += 1ull, at += 1u)
        {
            if (at >= lines.size())
            {
                return 0;
            }
            const std::vector<std::string> fields = stage_split(lines[at]);
            std::vector<GroupForm> forms;
            for (size_t field = 1u; field < fields.size(); field += 1u)
            {
                const size_t bar = fields[field].find('|');
                if (bar == std::string::npos)
                {
                    return 0;
                }
                forms.push_back({fields[field].substr(0u, bar), fields[field].substr(bar + 1u)});
            }
            language.meanings[fields[0]] = forms;
        }
        languages->push_back(language);
    }
    return (settings->permutations > 0ull) && (settings->p_below[1] > 0ull);
}

// two forms lists match where some form of each agrees on its two classes and the two come from different papers
static int group_match(const std::vector<GroupForm> &first, const std::vector<GroupForm> &second)
{
    for (size_t one = 0u; one < first.size(); one += 1u)
    {
        for (size_t two = 0u; two < second.size(); two += 1u)
        {
            if ((first[one].classes == second[two].classes) && (first[one].paper != second[two].paper))
            {
                return 1;
            }
        }
    }
    return 0;
}

// excess + 1 as (m P - S + n P) / (n P), and p as (a + 1) / (P + 1)
static void group_value_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int matched = stage_field(program, bits);
    const unsigned int sum = stage_field(program, bits);
    const unsigned int shared = stage_field(program, bits);
    const unsigned int shuffles = stage_field(program, bits);
    const unsigned int above = stage_field(program, bits);
    const unsigned int observed = stage_step(program, ENGINE_RECORD_PRODUCT, matched, shuffles);
    const unsigned int whole = stage_step(program, ENGINE_RECORD_PRODUCT, shared, shuffles);
    const unsigned int apart = stage_step(program, ENGINE_RECORD_DIFFERENCE, observed, sum);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, apart, whole));
    stage_output(program, whole);
    const unsigned int one = stage_constant(program, 1ull);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, above, one));
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, shuffles, one));
}

// a / b + c / d as (a d + c b) / (b d)
static void group_add_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int a = stage_field(program, bits);
    const unsigned int b = stage_field(program, bits);
    const unsigned int c = stage_field(program, bits);
    const unsigned int d = stage_field(program, bits);
    const unsigned int left = stage_step(program, ENGINE_RECORD_PRODUCT, a, d);
    const unsigned int right = stage_step(program, ENGINE_RECORD_PRODUCT, c, b);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, left, right));
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, b, d));
}

// a / b over k as a / (b k)
static void group_scale_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int a = stage_field(program, bits);
    const unsigned int b = stage_field(program, bits);
    const unsigned int k = stage_field(program, bits);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, a, stage_constant(program, 0ull)));
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, b, k));
}

// a / b less one, as its sign, whole part and first six digits after the point
static void group_signed_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int a = stage_field(program, bits);
    const unsigned int b = stage_field(program, bits);
    const unsigned int less = stage_step(program, ENGINE_RECORD_DIFFERENCE, a, b);
    stage_output(program, stage_step(program, ENGINE_RECORD_COMPARE, less, stage_constant(program, 0ull)));
    const unsigned int size = stage_step(program, ENGINE_RECORD_ABSOLUTE, less, 0u);
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, size, b));
    const unsigned int rest = stage_step(program, ENGINE_RECORD_REMAINDER, size, b);
    const unsigned int scaled =
        stage_step(program, ENGINE_RECORD_PRODUCT, rest, stage_constant(program, STAGE_DIGITS_SCALE));
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, scaled, b));
}

// the sum of each group of values, by rounds of the add program over pairs, and each sum scaled to its mean
static int group_means(SimResults *job, const std::vector<std::vector<StageRational>> &groups,
                       std::vector<StageRational> *means, std::vector<Stage> *stages, EngineError *error)
{
    std::vector<std::vector<StageRational>> held = groups;
    int ok = 1;
    while (ok)
    {
        StageRows rows;
        std::vector<StageRational> widths;
        for (size_t group = 0u; group < held.size(); group += 1u)
        {
            for (size_t at = 0u; (at + 1u) < held[group].size(); at += 2u)
            {
                rows.push_back({held[group][at].num, held[group][at].den, held[group][at + 1u].num,
                                held[group][at + 1u].den});
                widths.push_back(held[group][at]);
                widths.push_back(held[group][at + 1u]);
            }
        }
        if (rows.empty())
        {
            break;
        }
        stages->push_back(Stage());
        Stage *stage = &stages->back();
        memset(stage, 0, sizeof(*stage));
        stage->name = "add";
        group_add_program(stage_widest(widths), &stage->program);
        std::vector<std::vector<StageWhole>> out;
        ok = stage_rows(job, stage, rows, &out, error);
        std::vector<std::vector<StageRational>> next(held.size());
        size_t lane = 0u;
        for (size_t group = 0u; ok && (group < held.size()); group += 1u)
        {
            for (size_t at = 0u; at < held[group].size(); at += 2u)
            {
                if ((at + 1u) < held[group].size())
                {
                    next[group].push_back(stage_pair_of(out[lane]));
                    lane += 1u;
                }
                else
                {
                    next[group].push_back(held[group][at]);
                }
            }
        }
        held = next;
    }
    StageRows rows;
    std::vector<StageRational> widths;
    for (size_t group = 0u; group < held.size(); group += 1u)
    {
        const StageRational sum = held[group].empty() ? stage_unit() : held[group][0];
        rows.push_back({sum.num, sum.den, stage_whole(groups[group].empty() ? 1ull : groups[group].size())});
        widths.push_back(sum);
    }
    stages->push_back(Stage());
    Stage *stage = &stages->back();
    memset(stage, 0, sizeof(*stage));
    stage->name = "scale";
    group_scale_program(stage_widest(widths), &stage->program);
    std::vector<std::vector<StageWhole>> out;
    ok = ok && stage_rows(job, stage, rows, &out, error);
    means->clear();
    for (size_t group = 0u; ok && (group < held.size()); group += 1u)
    {
        means->push_back(stage_pair_of(out[group]));
    }
    return ok;
}

int main(int count, char **arguments)
{
    SimResults job;
    char job_capacity[SIM_LINE_CAPACITY];
    sim_open(&job, job_capacity);
    if (count != 3)
    {
        fprintf(stderr, "  usage: subgrouping <input> <output>\n");
        return 2;
    }
    GroupSettings settings;
    memset(&settings, 0, sizeof(settings));
    std::vector<GroupLanguage> read;
    if (!group_read(arguments[1], &settings, &read))
    {
        fprintf(stderr, "  subgrouping: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    // the languages held: enough meanings, not a reconstruction, ordered by branch and then name
    std::vector<GroupLanguage> languages;
    for (size_t at = 0u; at < read.size(); at += 1u)
    {
        if ((read[at].meanings.size() >= settings.least_meanings) && (read[at].branch != "PROTO"))
        {
            languages.push_back(read[at]);
        }
    }
    std::sort(languages.begin(), languages.end(), [](const GroupLanguage &one, const GroupLanguage &two) {
        return (one.branch != two.branch) ? (one.branch < two.branch) : (one.name < two.name);
    });

    // the pairs with enough shared meanings, and each pair's match bits over its shared meanings
    std::vector<GroupPair> pairs;
    size_t widest = 1u;
    for (size_t left = 0u; left < languages.size(); left += 1u)
    {
        for (size_t right = left + 1u; right < languages.size(); right += 1u)
        {
            std::vector<std::string> shared;
            for (const auto &meaning : languages[left].meanings)
            {
                if (languages[right].meanings.count(meaning.first) != 0u)
                {
                    shared.push_back(meaning.first);
                }
            }
            if (shared.size() < settings.least_shared)
            {
                continue;
            }
            GroupPair pair;
            pair.left = left;
            pair.right = right;
            pair.shared = shared.size();
            pair.matches.assign(shared.size(), std::vector<unsigned char>(shared.size(), 0u));
            for (size_t one = 0u; one < shared.size(); one += 1u)
            {
                for (size_t two = 0u; two < shared.size(); two += 1u)
                {
                    pair.matches[one][two] = (unsigned char)group_match(languages[left].meanings[shared[one]],
                                                                        languages[right].meanings[shared[two]]);
                }
            }
            widest = std::max(widest, shared.size());
            pairs.push_back(pair);
        }
    }
    if (pairs.empty())
    {
        fprintf(stderr, "  subgrouping: no pair of languages shares %llu meanings\n", settings.least_shared);
        return 2;
    }

    // the count program: each pair's runs, the identity first and then each shuffle, of `words` 32-bit words
    const unsigned long long shuffles = settings.permutations;
    const unsigned long long runs = shuffles + 1ull;
    const unsigned int words = (unsigned int)((widest + 31u) / 32u);
    EngineError error;
    memset(&error, 0, sizeof(error));
    Stage count_stage;
    memset(&count_stage, 0, sizeof(count_stage));
    count_stage.name = "count";
    stage_count_program(0, &count_stage.program);
    const unsigned long long count_lanes = (unsigned long long)pairs.size() * runs * words;
    std::vector<unsigned int> records((size_t)count_lanes, 0u);
    std::vector<size_t> order;
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        const size_t n = pairs[at].shared;
        const unsigned long long key = sim_mix(settings.seed ^ sim_mix((unsigned long long)at));
        for (unsigned long long run = 0ull; run < runs; run += 1ull)
        {
            order = stage_run(0u, n, 1u);
            if (run > 0ull)
            {
                for (size_t place = n - 1u; place > 0u; place -= 1u)
                {
                    const size_t other =
                        (size_t)sim_draw_below(key, (run * (unsigned long long)n) + place, (unsigned long long)place + 1ull);
                    std::swap(order[place], order[other]);
                }
            }
            unsigned int *word = &records[(size_t)(((unsigned long long)at * runs + run) * words)];
            for (size_t meaning = 0u; meaning < n; meaning += 1u)
            {
                if (pairs[at].matches[meaning][order[meaning]] != 0u)
                {
                    word[meaning / 32u] |= 1u << (meaning % 32u);
                }
            }
        }
    }

    int ok = stage_lay(&job, &count_stage, &error);
    const unsigned long long declared =
        count_lanes * (1ull + (ok ? count_stage.layout.out_limbs : 1u)) * sizeof(unsigned int);
    ok = ok && sim_job_submit(&job, "subgrouping", count, arguments, declared);
    std::vector<std::vector<StageWhole>> counted;
    ok = ok && stage_summed(&job, &count_stage, records, count_lanes, words, std::vector<unsigned int>(1, 0u),
                            &counted, &error);
    records.clear();
    records.shrink_to_fit();

    // the above program: each shuffle's count against the pair's own, summed over each pair's shuffles
    Stage above_stage;
    memset(&above_stage, 0, sizeof(above_stage));
    above_stage.name = "above";
    const unsigned int count_bits = stage_bits(stage_whole(widest));
    stage_above_program(count_bits, &above_stage.program);
    ok = ok && stage_lay(&job, &above_stage, &error);
    const unsigned int above_limbs = (above_stage.program.record_bits + 31u) / 32u;
    const unsigned long long above_lanes = (unsigned long long)pairs.size() * shuffles;
    std::vector<unsigned int> above_records((size_t)(above_lanes * above_limbs), 0u);
    for (size_t at = 0u; ok && (at < pairs.size()); at += 1u)
    {
        pairs[at].matched = counted[0][at * runs];
        for (unsigned long long shuffle = 0ull; shuffle < shuffles; shuffle += 1ull)
        {
            unsigned int *record = &above_records[(size_t)(((unsigned long long)at * shuffles + shuffle) * above_limbs)];
            stage_put(record, above_stage.program.field_offset[0], above_stage.program.field_bits[0],
                      counted[0][at * runs + 1ull + shuffle]);
            stage_put(record, above_stage.program.field_offset[1], above_stage.program.field_bits[1], pairs[at].matched);
        }
    }
    std::vector<std::vector<StageWhole>> aboves;
    std::vector<unsigned int> both_outputs = {0u, 1u};
    ok = ok && stage_summed(&job, &above_stage, above_records, above_lanes, shuffles, both_outputs, &aboves, &error);
    for (size_t at = 0u; ok && (at < pairs.size()); at += 1u)
    {
        pairs[at].above = aboves[0][at];
        pairs[at].shuffled_sum = aboves[1][at];
    }

    // the value program: each pair's excess + 1 and p
    Stage value_stage;
    memset(&value_stage, 0, sizeof(value_stage));
    value_stage.name = "value";
    StageRows value_rows;
    unsigned int value_bits = 1u;
    for (size_t at = 0u; ok && (at < pairs.size()); at += 1u)
    {
        value_rows.push_back({pairs[at].matched, pairs[at].shuffled_sum, stage_whole(pairs[at].shared),
                              stage_whole(shuffles), pairs[at].above});
        for (size_t field = 0u; field < value_rows.back().size(); field += 1u)
        {
            value_bits = std::max(value_bits, stage_bits(value_rows.back()[field]));
        }
    }
    group_value_program(value_bits + 1u, &value_stage.program);
    std::vector<std::vector<StageWhole>> values;
    ok = ok && stage_rows(&job, &value_stage, value_rows, &values, &error);
    for (size_t at = 0u; ok && (at < pairs.size()); at += 1u)
    {
        pairs[at].lifted = {values[at][0], values[at][1]};
        pairs[at].chance = {values[at][2], values[at][3]};
    }

    // the first compare: each language's partners ordered by excess, and each outside pair's p against p_below
    std::map<std::pair<size_t, size_t>, size_t> table;
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        table[{pairs[at].left, pairs[at].right}] = at;
        table[{pairs[at].right, pairs[at].left}] = at;
    }
    const std::set<std::string> salish = {"CS", "NIS", "SIS", "NUX", "TS", "TI"};
    std::vector<StageRational> xs;
    std::vector<StageRational> ys;
    std::vector<StageOrder> partner_orders(languages.size());
    std::vector<std::vector<size_t>> partner_of(languages.size());
    for (size_t left = 0u; left < languages.size(); left += 1u)
    {
        for (size_t right = 0u; right < languages.size(); right += 1u)
        {
            const auto found = table.find({left, right});
            if ((found == table.end()) || (salish.count(languages[right].branch) == 0u))
            {
                continue;
            }
            partner_orders[left].values.push_back(pairs[found->second].lifted);
            partner_orders[left].names.push_back(languages[right].name);
            partner_of[left].push_back(right);
        }
        stage_order_lanes(&partner_orders[left], &xs, &ys);
    }
    const size_t chance_at = xs.size();
    std::vector<size_t> outside;
    const StageRational p_below = {stage_whole(settings.p_below[0]), stage_whole(settings.p_below[1])};
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        const std::string &one = languages[pairs[at].left].branch;
        const std::string &two = languages[pairs[at].right].branch;
        if (((one == "OUT") && (salish.count(two) != 0u)) || ((two == "OUT") && (salish.count(one) != 0u)))
        {
            xs.push_back(p_below);
            ys.push_back(pairs[at].chance);
            outside.push_back(at);
        }
    }
    const unsigned long long unit_margin[2] = {1ull, 1ull};
    Stage first_stage;
    memset(&first_stage, 0, sizeof(first_stage));
    std::vector<int> first_signs;
    ok = ok && stage_compare(&job, &first_stage, "first compare", xs, ys, std::vector<int>(xs.size(), 0), unit_margin,
                             &first_signs, &error);
    for (size_t left = 0u; ok && (left < languages.size()); left += 1u)
    {
        stage_order_read(&partner_orders[left], first_signs);
    }

    // the group means: Interior, Central and between, and Nuxalk with each branch
    auto in = [&](size_t language, const std::set<std::string> &branches) {
        return branches.count(languages[language].branch) != 0u;
    };
    const std::set<std::string> interior = {"NIS", "SIS"};
    const std::set<std::string> central = {"CS"};
    const std::set<std::string> tsamosan = {"TS"};
    const std::set<std::string> nuxalk = {"NUX"};
    std::vector<std::vector<StageRational>> groups(6);
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        const size_t one = pairs[at].left;
        const size_t two = pairs[at].right;
        const StageRational &value = pairs[at].lifted;
        if (in(one, interior) && in(two, interior))
        {
            groups[0].push_back(value);
        }
        if (in(one, central) && in(two, central))
        {
            groups[1].push_back(value);
        }
        if ((in(one, interior) && in(two, central)) || (in(one, central) && in(two, interior)))
        {
            groups[2].push_back(value);
        }
        const std::set<std::string> *with[3] = {&interior, &central, &tsamosan};
        for (size_t branch = 0u; branch < 3u; branch += 1u)
        {
            if ((in(one, nuxalk) && in(two, *with[branch])) || (in(two, nuxalk) && in(one, *with[branch])))
            {
                groups[3u + branch].push_back(value);
            }
        }
    }
    std::vector<StageRational> means;
    std::vector<Stage> mean_stages;
    ok = ok && group_means(&job, groups, &means, &mean_stages, &error);
    std::vector<StageRational> mean_xs;
    std::vector<StageRational> mean_ys;
    if (ok && !groups[0].empty() && !groups[1].empty() && !groups[2].empty())
    {
        mean_xs = {means[0], means[1]};
        mean_ys = {means[2], means[2]};
    }
    Stage second_stage;
    memset(&second_stage, 0, sizeof(second_stage));
    std::vector<int> second_signs;
    ok = ok && stage_compare(&job, &second_stage, "second compare", mean_xs, mean_ys,
                             std::vector<int>(mean_xs.size(), 0), unit_margin, &second_signs, &error);

    // every excess and mean printed with its sign, from excess + 1
    std::vector<StageRational> printed;
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        printed.push_back(pairs[at].lifted);
    }
    const size_t means_at = printed.size();
    printed.insert(printed.end(), means.begin(), means.end());
    Stage signed_stage;
    memset(&signed_stage, 0, sizeof(signed_stage));
    signed_stage.name = "signed";
    group_signed_program(stage_widest(printed) + 1u, &signed_stage.program);
    StageRows signed_rows;
    for (size_t at = 0u; at < printed.size(); at += 1u)
    {
        signed_rows.push_back({printed[at].num, printed[at].den});
    }
    std::vector<std::vector<StageWhole>> signed_out;
    ok = ok && stage_rows(&job, &signed_stage, signed_rows, &signed_out, &error);
    std::vector<std::string> shown(printed.size());
    for (size_t at = 0u; ok && (at < printed.size()); at += 1u)
    {
        const DeviceRecordStep *sign_step = &signed_stage.layout.step_table[signed_stage.program.outputs[0]];
        const int sign = stage_sign(signed_out[at][0], sign_step->out_bits);
        char fraction[16];
        snprintf(fraction, sizeof(fraction), "%06u", signed_out[at][2][0]);
        shown[at] = std::string((sign < 0) ? "-" : "+") + stage_text_of(signed_out[at][1]) + "." + fraction;
    }
    for (size_t at = 0u; ok && (at < pairs.size()); at += 1u)
    {
        pairs[at].excess_text = shown[at];
    }

    // the verdicts
    std::vector<std::string> lines;
    for (int pass = 0; ok && (pass < 2); pass += 1)
    {
        const std::set<std::string> &group = (pass == 0) ? interior : central;
        for (size_t left = 0u; left < languages.size(); left += 1u)
        {
            if (!in(left, group) || partner_orders[left].sorted.empty())
            {
                continue;
            }
            const size_t best = partner_of[left][partner_orders[left].sorted.back()];
            const int holds = in(best, group);
            char line[256];
            snprintf(line, sizeof(line), "(%c) %s best Salish partner %s %s", (pass == 0) ? 'a' : 'b',
                     stage_shown(languages[left].name, 18u, 1).c_str(),
                     stage_shown(languages[best].name, 18u, 1).c_str(), holds ? "pass" : "FAIL");
            lines.push_back(line);
        }
    }
    const int means_hold = ok && (second_signs.size() == 2u) && (second_signs[0] == 1) && (second_signs[1] == 1);
    std::vector<std::string> failures;
    for (size_t at = 0u; ok && (at < outside.size()); at += 1u)
    {
        if (first_signs[chance_at + at] == 1)
        {
            const GroupPair &pair = pairs[outside[at]];
            failures.push_back(languages[pair.left].name + "-" + languages[pair.right].name + " " + pair.excess_text +
                               " p " + stage_text_of(pair.chance.num) + "/" + stage_text_of(pair.chance.den) + " [" +
                               std::to_string(pair.shared) + "]");
        }
    }

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "permutations %llu seed %llu least_shared %llu least_meanings %llu p_below %llu/%llu\n",
                settings.permutations, settings.seed, settings.least_shared, settings.least_meanings,
                settings.p_below[0], settings.p_below[1]);
        for (size_t at = 0u; at < languages.size(); at += 1u)
        {
            fprintf(out, "language %s %s meanings %zu\n", languages[at].branch.c_str(), languages[at].name.c_str(),
                    languages[at].meanings.size());
        }
        for (size_t at = 0u; at < pairs.size(); at += 1u)
        {
            const GroupPair &pair = pairs[at];
            fprintf(out, "pair %s | %s shared %zu matched %s shuffled_sum %s above %s excess+1 %s/%s p %s/%s excess %s\n",
                    languages[pair.left].name.c_str(), languages[pair.right].name.c_str(), pair.shared,
                    stage_text_of(pair.matched).c_str(), stage_text_of(pair.shuffled_sum).c_str(),
                    stage_text_of(pair.above).c_str(), stage_text_of(pair.lifted.num).c_str(),
                    stage_text_of(pair.lifted.den).c_str(), stage_text_of(pair.chance.num).c_str(),
                    stage_text_of(pair.chance.den).c_str(), pair.excess_text.c_str());
        }
        for (size_t at = 0u; at < lines.size(); at += 1u)
        {
            fprintf(out, "%s\n", lines[at].c_str());
        }
        fprintf(out, "(c) %s\n", means_hold ? "pass" : "FAIL");
        fprintf(out, "(d) %s\n", failures.empty() ? "pass" : "FAIL");
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        printf("languages with %llu or more meanings:\n", settings.least_meanings);
        for (size_t at = 0u; at < languages.size(); at += 1u)
        {
            printf("   %-5s %s %5zu meanings\n", languages[at].branch.c_str(),
                   stage_shown(languages[at].name, 18u, 1).c_str(), languages[at].meanings.size());
        }
        printf("\nexcess match rate (observed minus shuffled), shared meanings in brackets\n");
        for (size_t left = 0u; left < languages.size(); left += 1u)
        {
            std::vector<std::pair<size_t, size_t>> ranked;
            for (size_t place = partner_orders[left].sorted.size(); place > 0u; place -= 1u)
            {
                const size_t right = partner_of[left][partner_orders[left].sorted[place - 1u]];
                ranked.push_back({right, table[{left, right}]});
            }
            printf("%-5s %s", languages[left].branch.c_str(), stage_shown(languages[left].name, 18u, 1).c_str());
            for (size_t at = 0u; (at < ranked.size()) && (at < 5u); at += 1u)
            {
                printf("  %s %.6s[%zu]", stage_shown(languages[ranked[at].first].name, 10u, 0).c_str(),
                       pairs[ranked[at].second].excess_text.c_str(), pairs[ranked[at].second].shared);
            }
            printf("\n");
        }
        for (size_t at = 0u; at < lines.size(); at += 1u)
        {
            printf("%s\n", lines[at].c_str());
        }
        printf("(c) mean excess: Interior pairs %s (%zu), Central pairs %s (%zu), Interior-Central %s (%zu): %s\n",
               shown[means_at].c_str(), groups[0].size(), shown[means_at + 1u].c_str(), groups[1].size(),
               shown[means_at + 2u].c_str(), groups[2].size(), means_hold ? "pass" : "FAIL");
        printf("(d) outside-Salish pairs with shuffle p below %llu/%llu: ", settings.p_below[0], settings.p_below[1]);
        if (failures.empty())
        {
            printf("none, pass\n");
        }
        for (size_t at = 0u; at < failures.size(); at += 1u)
        {
            printf("%s%s", (at == 0u) ? "" : "; ", failures[at].c_str());
        }
        if (!failures.empty())
        {
            printf("\n");
        }
        const char *names[3] = {"Interior", "Central", "Tsamosan"};
        for (size_t branch = 0u; branch < 3u; branch += 1u)
        {
            if (!groups[3u + branch].empty())
            {
                printf("Nuxalk mean excess with %-9s %s over %zu languages\n", names[branch],
                       shown[means_at + 3u + branch].c_str(), groups[3u + branch].size());
            }
        }
        printf("each excess is the sign, whole part and first six digits of the exact rational in the records\n");
    }

    stage_release(&count_stage);
    stage_release(&above_stage);
    stage_release(&value_stage);
    stage_release(&first_stage);
    for (size_t at = 0u; at < mean_stages.size(); at += 1u)
    {
        stage_release(&mean_stages[at]);
    }
    stage_release(&second_stage);
    stage_release(&signed_stage);
    return sim_close(&job, "subgrouping");
}
