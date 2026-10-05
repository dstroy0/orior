// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// Kinkade's account of where Salishan lexical suffixes come from, tested exact on the record machine against the
// oracle tables.
//
//   Usage:  lexical_suffix_origins <input> <output>
//
// The measure is lexical_suffix_origins.py's. Per language, a same-meaning pair is a suffix and a noun whose
// meaning the suffix offers. Among those pairs, the Kinkade relation holds where the noun's segments end with the
// suffix's and the noun is one or two segments longer, the tail relation where the noun is longer and ends with
// it, and a pair is identical where the noun equals the suffix. The null keeps every language's suffixes and nouns
// and shuffles which meaning each noun carries, P times. For each relation, with m the observed count, S the sum of
// the shuffles' counts and a the number of shuffles whose count is m or more,
//
//   null mean = S / P,   and   p = (a + 1) / (P + 1),
//
// both exact rationals. They are taken per language for the tail relation and pooled over the languages for both,
// a shuffle's pooled count being the sum of its count in every language. The pooled null's largest count is
// reported beside them.
//
// Programs of the record machine, each built by a request that carries only its field widths:
//   count     one lane a 32-pair word of one relation, shuffle and language, the bits of same-meaning pairs and the
//             bits where the relation holds in, how many are set in both out; the sum over each run of a language's
//             words is that relation's count, and the run of the identity is the observed count;
//   above     one lane a shuffle, its count and the observed count in, [m_k >= m] and m_k out; the sums over each
//             run of shuffles are a and S;
//   pass      one lane a language's count in one shuffle, the count out; the sum over each run of languages is the
//             pooled count;
//   larger    two counts in, (x + y + |x - y|) / 2 out, run in rounds until one count is left;
//   value     a and P in, a + 1 and P + 1 out;
//   digits    the record_stages.h program, for every mean and p printed.
// Building the relations and drawing the shuffles are the host's, and every count, sum, comparison and quotient is
// the machine's. The shuffles are drawn with sim_draw, keyed by the input's seed, the language and the shuffle. They
// are not Python's draws, and a p here is a p over different shuffles of the same null.
//
// The input is what lexical_suffix_origins.py writes. The output holds every language's counts, sums and p whole,
// the pooled ones, and the tail pairs observed.
//
// Checks:
//   1. each program imprints and lays out;
//   2. each program loads onto the device and every lane runs it;
//   3. the host's records of each program equal the device's word for word;
//   4. the device's sums of each run equal the host's;
//   5. the records are written out.

#include "../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../src/cu/engine/runtime/scriptura/scriptura.h"
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

// the relations counted, in the order of their lanes
#define SUFFIX_PAIRS 0u
#define SUFFIX_KINKADE 1u
#define SUFFIX_TAIL 2u
#define SUFFIX_IDENTICAL 3u
#define SUFFIX_RELATIONS 4u

typedef struct
{
    std::vector<std::string> segments;
    std::set<std::string> offered;
} SuffixRow;

typedef struct
{
    std::vector<std::string> segments;
    std::string meaning;
} SuffixNoun;

typedef struct
{
    std::string name;
    std::vector<SuffixRow> suffixes;
    std::vector<SuffixNoun> nouns;
    std::vector<std::vector<unsigned int>> flags;
    StageWhole observed[SUFFIX_RELATIONS];
    StageWhole sum[SUFFIX_RELATIONS];
    StageWhole above[SUFFIX_RELATIONS];
} SuffixLanguage;

typedef struct
{
    unsigned long long permutations;
    unsigned long long seed;
    unsigned long long least_suffixes;
    unsigned long long least_nouns;
} SuffixSettings;

static std::vector<std::string> suffix_words(const std::string &text, char part)
{
    std::vector<std::string> words;
    size_t start = 0u;
    while (start <= text.size())
    {
        size_t end = text.find(part, start);
        end = (end == std::string::npos) ? text.size() : end;
        if (end > start)
        {
            words.push_back(text.substr(start, end - start));
        }
        start = end + 1u;
    }
    return words;
}

static int suffix_read(const char *path, SuffixSettings *settings, std::vector<SuffixLanguage> *languages)
{
    std::vector<std::string> lines;
    if (!stage_lines(path, &lines) || (lines.size() < 4u))
    {
        return 0;
    }
    const char *heads[4] = {"permutations", "seed", "least_suffixes", "least_nouns"};
    unsigned long long *values[4] = {&settings->permutations, &settings->seed, &settings->least_suffixes,
                                     &settings->least_nouns};
    for (size_t at = 0u; at < 4u; at += 1u)
    {
        const std::vector<std::string> fields = stage_split(lines[at]);
        if ((fields.size() != 2u) || (fields[0] != heads[at]))
        {
            return 0;
        }
        *values[at] = strtoull(fields[1].c_str(), NULL, 10);
    }
    size_t at = 4u;
    while (at < lines.size())
    {
        const std::vector<std::string> head = stage_split(lines[at]);
        if ((head.size() != 4u) || (head[0] != "language"))
        {
            return 0;
        }
        SuffixLanguage language;
        language.name = head[1];
        const unsigned long long suffixes = strtoull(head[2].c_str(), NULL, 10);
        const unsigned long long nouns = strtoull(head[3].c_str(), NULL, 10);
        at += 1u;
        for (unsigned long long one = 0ull; one < suffixes + nouns; one += 1ull, at += 1u)
        {
            const std::vector<std::string> fields = (at < lines.size()) ? stage_split(lines[at]) : std::vector<std::string>();
            const char *kind = (one < suffixes) ? "suffix" : "noun";
            if ((fields.size() != 3u) || (fields[0] != kind))
            {
                return 0;
            }
            if (one < suffixes)
            {
                const std::vector<std::string> offered = suffix_words(fields[2], ',');
                language.suffixes.push_back({suffix_words(fields[1], ' '), std::set<std::string>(offered.begin(), offered.end())});
            }
            else
            {
                language.nouns.push_back({suffix_words(fields[1], ' '), fields[2]});
            }
        }
        languages->push_back(language);
    }
    return settings->permutations > 0ull;
}

// whether the noun's segments end with the suffix's, the noun longer by between `least` and `most` segments
static int suffix_ends(const std::vector<std::string> &noun, const std::vector<std::string> &suffix, size_t least,
                       size_t most)
{
    if ((noun.size() < suffix.size() + least) || (noun.size() > suffix.size() + most))
    {
        return 0;
    }
    return std::equal(suffix.begin(), suffix.end(), noun.end() - (long long)suffix.size());
}

// a count passed through, for its sum over a run of languages
static void suffix_pass_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int count = stage_field(program, bits);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, count, stage_constant(program, 0ull)));
}

// the larger of two counts, (x + y + |x - y|) / 2
static void suffix_larger_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int x = stage_field(program, bits);
    const unsigned int y = stage_field(program, bits);
    const unsigned int both = stage_step(program, ENGINE_RECORD_SUM, x, y);
    const unsigned int apart = stage_step(program, ENGINE_RECORD_ABSOLUTE,
                                          stage_step(program, ENGINE_RECORD_DIFFERENCE, x, y), 0u);
    const unsigned int twice = stage_step(program, ENGINE_RECORD_SUM, both, apart);
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, twice, stage_constant(program, 2ull)));
}

// p as (a + 1) / (P + 1)
static void suffix_value_program(unsigned int bits, StageProgram *program)
{
    memset(program, 0, sizeof(*program));
    const unsigned int above = stage_field(program, bits);
    const unsigned int shuffles = stage_field(program, bits);
    const unsigned int one = stage_constant(program, 1ull);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, above, one));
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, shuffles, one));
}

// a / b as its whole part and six digits, by the digits program; one text per value
static int suffix_digits(SimResults *job, Stage *stage, const std::vector<StageRational> &values,
                         std::vector<std::string> *shown, EngineError *error)
{
    memset(stage, 0, sizeof(*stage));
    stage->name = "digits";
    stage_digits_program(stage_widest(values) + 1u, &stage->program);
    StageRows rows;
    for (size_t at = 0u; at < values.size(); at += 1u)
    {
        rows.push_back({values[at].num, values[at].den});
    }
    std::vector<std::vector<StageWhole>> out;
    const int ok = stage_rows(job, stage, rows, &out, error);
    shown->clear();
    for (size_t at = 0u; ok && (at < out.size()); at += 1u)
    {
        char fraction[16];
        snprintf(fraction, sizeof(fraction), "%06u", out[at][1][0]);
        shown->push_back(stage_text_of(out[at][0]) + "." + fraction);
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
        fprintf(stderr, "  usage: lexical_suffix_origins <input> <output>\n");
        return 2;
    }
    SuffixSettings settings;
    memset(&settings, 0, sizeof(settings));
    std::vector<SuffixLanguage> read;
    if (!suffix_read(arguments[1], &settings, &read))
    {
        fprintf(stderr, "  lexical_suffix_origins: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    // the languages held, and each one's relation bits over its pairs, suffix by suffix and noun by noun
    std::vector<SuffixLanguage> languages;
    unsigned int words = 1u;
    unsigned long long all_pairs = 0ull;
    for (size_t at = 0u; at < read.size(); at += 1u)
    {
        SuffixLanguage language = read[at];
        if ((language.suffixes.size() < settings.least_suffixes) || (language.nouns.size() < settings.least_nouns))
        {
            continue;
        }
        const size_t pairs = language.suffixes.size() * language.nouns.size();
        const unsigned int own_words = (unsigned int)((pairs + 31u) / 32u);
        language.flags.assign(SUFFIX_RELATIONS, std::vector<unsigned int>(own_words, 0u));
        for (size_t suffix = 0u; suffix < language.suffixes.size(); suffix += 1u)
        {
            for (size_t noun = 0u; noun < language.nouns.size(); noun += 1u)
            {
                const size_t bit = suffix * language.nouns.size() + noun;
                const std::vector<std::string> &own = language.nouns[noun].segments;
                const std::vector<std::string> &end = language.suffixes[suffix].segments;
                const unsigned int holds[SUFFIX_RELATIONS] = {1u, (unsigned int)suffix_ends(own, end, 1u, 2u),
                                                              (unsigned int)suffix_ends(own, end, 1u, own.size()),
                                                              (unsigned int)(own == end)};
                for (unsigned int relation = 0u; relation < SUFFIX_RELATIONS; relation += 1u)
                {
                    language.flags[relation][bit / 32u] |= holds[relation] << (bit % 32u);
                }
            }
        }
        words = std::max(words, own_words);
        all_pairs += pairs;
        languages.push_back(language);
    }
    if (languages.empty())
    {
        fprintf(stderr, "  lexical_suffix_origins: no language holds %llu suffixes and %llu nouns\n",
                settings.least_suffixes, settings.least_nouns);
        return 2;
    }

    // the count program: lanes ordered by relation, then run (the identity first), then language, then word
    const unsigned long long shuffles = settings.permutations;
    const unsigned long long runs = shuffles + 1ull;
    const unsigned long long held = languages.size();
    EngineError error;
    memset(&error, 0, sizeof(error));
    Stage count_stage;
    memset(&count_stage, 0, sizeof(count_stage));
    count_stage.name = "count";
    stage_count_program(1, &count_stage.program);
    const unsigned long long count_lanes = SUFFIX_RELATIONS * runs * held * words;
    std::vector<unsigned int> records((size_t)(count_lanes * 2ull), 0u);
    std::vector<size_t> order;
    std::vector<unsigned int> same(words, 0u);
    for (size_t at = 0u; at < languages.size(); at += 1u)
    {
        const SuffixLanguage &language = languages[at];
        const size_t nouns = language.nouns.size();
        const unsigned long long key = sim_mix(settings.seed ^ sim_mix((unsigned long long)at));
        for (unsigned long long run = 0ull; run < runs; run += 1ull)
        {
            order = stage_run(0u, nouns, 1u);
            if (run > 0ull)
            {
                for (size_t place = nouns - 1u; place > 0u; place -= 1u)
                {
                    const size_t other = (size_t)sim_draw_below(key, (run * (unsigned long long)nouns) + place,
                                                                (unsigned long long)place + 1ull);
                    std::swap(order[place], order[other]);
                }
            }
            std::fill(same.begin(), same.end(), 0u);
            for (size_t suffix = 0u; suffix < language.suffixes.size(); suffix += 1u)
            {
                for (size_t noun = 0u; noun < nouns; noun += 1u)
                {
                    const size_t bit = suffix * nouns + noun;
                    const unsigned int offered =
                        (unsigned int)language.suffixes[suffix].offered.count(language.nouns[order[noun]].meaning);
                    same[bit / 32u] |= offered << (bit % 32u);
                }
            }
            for (unsigned int relation = 0u; relation < SUFFIX_RELATIONS; relation += 1u)
            {
                const unsigned long long first = ((relation * runs + run) * held + at) * words;
                for (size_t word = 0u; word < language.flags[relation].size(); word += 1u)
                {
                    records[(size_t)((first + word) * 2ull)] = same[word];
                    records[(size_t)((first + word) * 2ull + 1ull)] = language.flags[relation][word];
                }
            }
        }
    }
    int ok = stage_lay(&job, &count_stage, &error);
    const unsigned long long declared =
        count_lanes * (2ull + (ok ? count_stage.layout.out_limbs : 1u)) * sizeof(unsigned int);
    ok = ok && sim_job_submit(&job, "lexical_suffix_origins", count, arguments, declared);
    std::vector<std::vector<StageWhole>> counted;
    ok = ok && stage_summed(&job, &count_stage, records, count_lanes, words, std::vector<unsigned int>(1, 0u),
                            &counted, &error);
    records.clear();
    records.shrink_to_fit();
    auto counts = [&](unsigned int relation, unsigned long long run, size_t language) -> const StageWhole & {
        return counted[0][(size_t)((relation * runs + run) * held + language)];
    };
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        for (unsigned int relation = 0u; relation < SUFFIX_RELATIONS; relation += 1u)
        {
            languages[at].observed[relation] = counts(relation, 0ull, at);
        }
    }

    // the pass program: each shuffle's Kinkade, tail and same-meaning counts summed over the languages
    const unsigned int count_bits = stage_bits(stage_whole(all_pairs));
    const unsigned int tested[3] = {SUFFIX_KINKADE, SUFFIX_TAIL, SUFFIX_PAIRS};
    Stage pass_stage;
    memset(&pass_stage, 0, sizeof(pass_stage));
    pass_stage.name = "pass";
    suffix_pass_program(count_bits, &pass_stage.program);
    ok = ok && stage_lay(&job, &pass_stage, &error);
    const unsigned long long pass_lanes = 3ull * runs * held;
    const unsigned int pass_limbs = (pass_stage.program.record_bits + 31u) / 32u;
    std::vector<unsigned int> pass_records((size_t)(pass_lanes * pass_limbs), 0u);
    for (unsigned int which = 0u; ok && (which < 3u); which += 1u)
    {
        for (unsigned long long run = 0ull; run < runs; run += 1ull)
        {
            for (size_t at = 0u; at < languages.size(); at += 1u)
            {
                const unsigned long long lane = (which * runs + run) * held + at;
                stage_put(&pass_records[(size_t)(lane * pass_limbs)], pass_stage.program.field_offset[0],
                          pass_stage.program.field_bits[0], counts(tested[which], run, at));
            }
        }
    }
    std::vector<std::vector<StageWhole>> pooled;
    ok = ok && stage_summed(&job, &pass_stage, pass_records, pass_lanes, held, std::vector<unsigned int>(1, 0u),
                            &pooled, &error);

    // the above program: each language's shuffles and the pooled shuffles, against their observed counts
    Stage above_stage;
    memset(&above_stage, 0, sizeof(above_stage));
    above_stage.name = "above";
    stage_above_program(count_bits + 1u, &above_stage.program);
    ok = ok && stage_lay(&job, &above_stage, &error);
    const unsigned long long sets = 2ull * (held + 1ull);
    const unsigned long long above_lanes = sets * shuffles;
    const unsigned int above_limbs = (above_stage.program.record_bits + 31u) / 32u;
    std::vector<unsigned int> above_records((size_t)(above_lanes * above_limbs), 0u);
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        for (unsigned long long set = 0ull; set <= held; set += 1ull)
        {
            const StageWhole &observed = (set < held) ? counts(tested[which], 0ull, (size_t)set) : pooled[0][which * runs];
            for (unsigned long long shuffle = 0ull; shuffle < shuffles; shuffle += 1ull)
            {
                const StageWhole &drawn = (set < held) ? counts(tested[which], 1ull + shuffle, (size_t)set)
                                                       : pooled[0][which * runs + 1ull + shuffle];
                unsigned int *record =
                    &above_records[(size_t)((((which * (held + 1ull)) + set) * shuffles + shuffle) * above_limbs)];
                stage_put(record, above_stage.program.field_offset[0], above_stage.program.field_bits[0], drawn);
                stage_put(record, above_stage.program.field_offset[1], above_stage.program.field_bits[1], observed);
            }
        }
    }
    std::vector<std::vector<StageWhole>> aboves;
    ok = ok && stage_summed(&job, &above_stage, above_records, above_lanes, shuffles, {0u, 1u}, &aboves, &error);
    StageWhole pooled_above[2];
    StageWhole pooled_sum[2];
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        for (size_t at = 0u; at < languages.size(); at += 1u)
        {
            languages[at].above[tested[which]] = aboves[0][which * (held + 1ull) + at];
            languages[at].sum[tested[which]] = aboves[1][which * (held + 1ull) + at];
        }
        pooled_above[which] = aboves[0][which * (held + 1ull) + held];
        pooled_sum[which] = aboves[1][which * (held + 1ull) + held];
    }

    // the larger program, in rounds: the largest pooled count among the shuffles of each relation
    std::vector<std::vector<StageWhole>> left(2);
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        left[which].assign(pooled[0].begin() + (long long)(which * runs + 1ull),
                           pooled[0].begin() + (long long)((which + 1ull) * runs));
    }
    std::vector<Stage> larger_stages;
    while (ok && ((left[0].size() > 1u) || (left[1].size() > 1u)))
    {
        StageRows rows;
        for (unsigned int which = 0u; which < 2u; which += 1u)
        {
            for (size_t at = 0u; (at + 1u) < left[which].size(); at += 2u)
            {
                rows.push_back({left[which][at], left[which][at + 1u]});
            }
        }
        larger_stages.push_back(Stage());
        Stage *stage = &larger_stages.back();
        memset(stage, 0, sizeof(*stage));
        stage->name = "larger";
        suffix_larger_program(count_bits + 1u, &stage->program);
        std::vector<std::vector<StageWhole>> out;
        ok = stage_rows(&job, stage, rows, &out, &error);
        size_t lane = 0u;
        for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
        {
            std::vector<StageWhole> next;
            for (size_t at = 0u; at < left[which].size(); at += 2u)
            {
                if ((at + 1u) < left[which].size())
                {
                    next.push_back(out[lane][0]);
                    lane += 1u;
                }
                else
                {
                    next.push_back(left[which][at]);
                }
            }
            left[which] = next;
        }
    }

    // the value program: every p, per language for the tail relation and pooled for both
    Stage value_stage;
    memset(&value_stage, 0, sizeof(value_stage));
    value_stage.name = "value";
    suffix_value_program(stage_bits(stage_whole(shuffles)) + 1u, &value_stage.program);
    StageRows value_rows;
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        value_rows.push_back({languages[at].above[SUFFIX_TAIL], stage_whole(shuffles)});
    }
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        value_rows.push_back({pooled_above[which], stage_whole(shuffles)});
    }
    std::vector<std::vector<StageWhole>> values;
    ok = ok && stage_rows(&job, &value_stage, value_rows, &values, &error);

    // the digits program: each null mean and each p
    std::vector<StageRational> printed;
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        printed.push_back({languages[at].sum[SUFFIX_KINKADE], stage_whole(shuffles)});
        printed.push_back({languages[at].sum[SUFFIX_TAIL], stage_whole(shuffles)});
        printed.push_back(stage_pair_of(values[at]));
    }
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        printed.push_back({pooled_sum[which], stage_whole(shuffles)});
        printed.push_back(stage_pair_of(values[held + which]));
    }
    Stage digits_stage;
    memset(&digits_stage, 0, sizeof(digits_stage));
    std::vector<std::string> shown;
    ok = ok && suffix_digits(&job, &digits_stage, printed, &shown, &error);

    // the tail pairs observed, read from the identity run's bits
    std::vector<std::string> found;
    for (size_t at = 0u; ok && (at < languages.size()); at += 1u)
    {
        const SuffixLanguage &language = languages[at];
        for (size_t suffix = 0u; suffix < language.suffixes.size(); suffix += 1u)
        {
            for (size_t noun = 0u; noun < language.nouns.size(); noun += 1u)
            {
                const size_t bit = suffix * language.nouns.size() + noun;
                const int tail = (language.flags[SUFFIX_TAIL][bit / 32u] >> (bit % 32u)) & 1u;
                const int kinkade = (language.flags[SUFFIX_KINKADE][bit / 32u] >> (bit % 32u)) & 1u;
                if (!tail || (language.suffixes[suffix].offered.count(language.nouns[noun].meaning) == 0u))
                {
                    continue;
                }
                std::string noun_text;
                std::string suffix_text;
                for (size_t part = 0u; part < language.nouns[noun].segments.size(); part += 1u)
                {
                    noun_text += language.nouns[noun].segments[part];
                }
                for (size_t part = 0u; part < language.suffixes[suffix].segments.size(); part += 1u)
                {
                    suffix_text += language.suffixes[suffix].segments[part];
                }
                found.push_back(std::string(kinkade ? "Kinkade" : "tail   ") + "  " + language.name + "  " +
                                noun_text + " ~ =" + suffix_text + " '" + language.nouns[noun].meaning + "'");
            }
        }
    }

    const char *names[2] = {"Kinkade (noun one or two segments longer)", "tail (noun longer, ends in it)"};
    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "permutations %llu seed %llu least_suffixes %llu least_nouns %llu\n", settings.permutations,
                settings.seed, settings.least_suffixes, settings.least_nouns);
        for (size_t at = 0u; at < languages.size(); at += 1u)
        {
            const SuffixLanguage &language = languages[at];
            fprintf(out,
                    "language %s suffixes %zu nouns %zu pairs %s identical %s kinkade %s kinkade_sum %s kinkade_above %s "
                    "tail %s tail_sum %s tail_above %s p %s/%s\n",
                    language.name.c_str(), language.suffixes.size(), language.nouns.size(),
                    stage_text_of(language.observed[SUFFIX_PAIRS]).c_str(),
                    stage_text_of(language.observed[SUFFIX_IDENTICAL]).c_str(),
                    stage_text_of(language.observed[SUFFIX_KINKADE]).c_str(),
                    stage_text_of(language.sum[SUFFIX_KINKADE]).c_str(),
                    stage_text_of(language.above[SUFFIX_KINKADE]).c_str(),
                    stage_text_of(language.observed[SUFFIX_TAIL]).c_str(),
                    stage_text_of(language.sum[SUFFIX_TAIL]).c_str(), stage_text_of(language.above[SUFFIX_TAIL]).c_str(),
                    stage_text_of(values[at][0]).c_str(), stage_text_of(values[at][1]).c_str());
        }
        for (unsigned int which = 0u; which < 2u; which += 1u)
        {
            fprintf(out, "pooled %s pairs %s observed %s sum %s largest %s above %s p %s/%s\n",
                    (which == 0u) ? "kinkade" : "tail", stage_text_of(pooled[0][2u * runs]).c_str(),
                    stage_text_of(pooled[0][which * runs]).c_str(), stage_text_of(pooled_sum[which]).c_str(),
                    stage_text_of(left[which][0]).c_str(), stage_text_of(pooled_above[which]).c_str(),
                    stage_text_of(values[held + which][0]).c_str(), stage_text_of(values[held + which][1]).c_str());
        }
        for (size_t at = 0u; at < found.size(); at += 1u)
        {
            fprintf(out, "%s\n", found[at].c_str());
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        printf("%s %4s %5s %5s %7s %9s %5s %9s %5s %9s\n", stage_shown("language", 18u, 1).c_str(), "LS", "nouns",
               "pairs", "Kinkade", "null mean", "tail", "null mean", "ident", "p (tail)");
        for (size_t at = 0u; at < languages.size(); at += 1u)
        {
            const SuffixLanguage &language = languages[at];
            printf("%s %4zu %5zu %5s %7s %9.9s %5s %9.9s %5s %9.9s\n", stage_shown(language.name, 18u, 1).c_str(),
                   language.suffixes.size(), language.nouns.size(),
                   stage_text_of(language.observed[SUFFIX_PAIRS]).c_str(),
                   stage_text_of(language.observed[SUFFIX_KINKADE]).c_str(), shown[at * 3u].c_str(),
                   stage_text_of(language.observed[SUFFIX_TAIL]).c_str(), shown[at * 3u + 1u].c_str(),
                   stage_text_of(language.observed[SUFFIX_IDENTICAL]).c_str(), shown[at * 3u + 2u].c_str());
        }
        for (unsigned int which = 0u; which < 2u; which += 1u)
        {
            printf("pooled %s: observed %s, null mean %s, null max %s, p %s/%s = %s over %llu shuffles, %s same-meaning "
                   "pairs\n",
                   names[which], stage_text_of(pooled[0][which * runs]).c_str(), shown[held * 3u + which * 2u].c_str(),
                   stage_text_of(left[which][0]).c_str(), stage_text_of(values[held + which][0]).c_str(),
                   stage_text_of(values[held + which][1]).c_str(), shown[held * 3u + which * 2u + 1u].c_str(),
                   shuffles, stage_text_of(pooled[0][2u * runs]).c_str());
        }
        for (size_t at = 0u; at < found.size(); at += 1u)
        {
            printf("   %s\n", found[at].c_str());
        }
        printf("each mean and p is the whole part and first six digits of the exact rational in the records\n");
    }

    stage_release(&count_stage);
    stage_release(&pass_stage);
    stage_release(&above_stage);
    for (size_t at = 0u; at < larger_stages.size(); at += 1u)
    {
        stage_release(&larger_stages[at]);
    }
    stage_release(&value_stage);
    stage_release(&digits_stage);
    return sim_close(&job, "lexical_suffix_origins");
}
