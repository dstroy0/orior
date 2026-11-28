// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// Kinkade's account of where Salishan lexical suffixes come from, tested exact on the record machine against the
// oracle tables.
//
//   Usage:  lexical_suffix_origins <input> <output>
//
// The measure is lexical_suffix_origins.py's. Per language, a same-meaning pair is a suffix and a noun whose
// meaning the suffix offers. Among those pairs, the Kinkade relation is satisfied where the noun's segments end with
// the suffix's and the noun is one or two segments longer, the tail relation where the noun is longer and ends with it,
// and a pair is identical where the noun equals the suffix. The null preserves every language's suffixes and nouns and
// shuffles which meaning each noun carries, P times. For each relation, with m the observed count, S the sum of the
// shuffles' counts and a the number of shuffles whose count is m or more,
//
//   null mean = S / P,   and   p = (a + 1) / (P + 1),
//
// both exact rationals. They are taken per language for the tail relation and pooled over the languages for both,
// a shuffle's pooled count being the sum of its count in every language. The pooled null's largest count is
// reported beside them.
//
// Programs of the record machine, each built by a request that carries only its field widths:
//   count     one lane a 32-pair word of one relation, shuffle and language, the bits of same-meaning pairs and the
//             bits where the relation is satisfied in, how many are set in both out; the sum over each run of a
//             language's words is that relation's count, and the run of the identity is the observed count;
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
// The input is what lexical_suffix_origins.py writes. The output contains every language's counts, sums and p whole,
// the pooled ones, and the tail pairs observed.
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

// the pool of the host: the counts of every relation, run and language take a whole number each
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 31u;

// the relations counted, in the order of their lanes
#define SUFFIX_PAIRS 0u
#define SUFFIX_KINKADE 1u
#define SUFFIX_TAIL 2u
#define SUFFIX_IDENTICAL 3u
#define SUFFIX_RELATIONS 4u

// the rounds of the larger program at max: each round halves the counts left, and a count of them fits 64 bits
#define SUFFIX_ROUNDS_MAX 64u

// a suffix: its segments from word[first_segment] on, and the meanings it offers from word[first_offered] on, sorted
// by their bytes once the input is read
typedef struct
{
    unsigned long long first_segment;
    unsigned long long segments;
    unsigned long long first_offered;
    unsigned long long offered;
} SuffixRow;

// a noun: its segments from word[first_segment] on, and its meaning
typedef struct
{
    unsigned long long first_segment;
    unsigned long long segments;
    const char *meaning;
} SuffixNoun;

// a language as the input names it: its suffixes from suffix[first_suffix] on and its nouns from noun[first_noun] on
typedef struct
{
    const char *name;
    unsigned long long first_suffix;
    unsigned long long suffixes;
    unsigned long long first_noun;
    unsigned long long nouns;
} SuffixRead;

// a language retained: the one read, its relation bits, `words` words a relation in a cell of the temporary tier, and
// what the machine gives it
typedef struct
{
    const SuffixRead *read;
    unsigned int *flags;
    unsigned int words;
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

// The input read once: its lines, its words, and its suffixes, nouns and languages. Each is a cell of the temporary
// tier, the words at the input's bytes and the rest at its lines, which bound them before they fill
typedef struct
{
    StageLines lines;
    const char **word;
    unsigned long long words;
    SuffixRow *suffix;
    unsigned long long suffixes;
    SuffixNoun *noun;
    unsigned long long nouns;
    SuffixRead *language;
    unsigned long long languages;
} SuffixInput;

static void suffix_released(SuffixInput *input)
{
    stage_cell_released(input->language);
    stage_cell_released(input->noun);
    stage_cell_released(input->suffix);
    stage_cell_released((void *)input->word);
    stage_lines_released(&input->lines);
}

// the words of a text split in place at `part`, each that holds a byte put at the input's next word; the count of them
static unsigned long long suffix_words(char *text, char part, SuffixInput *input)
{
    const unsigned long long first = input->words;
    char *start = text;
    for (;;)
    {
        char *const end = (char *)EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = start,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = (uint8_t)part);
        if (end != NULL)
        {
            *end = '\0';
        }
        if (*start != '\0')
        {
            input->word[input->words] = start;
            input->words += 1ull;
        }
        if (end == NULL)
        {
            return input->words - first;
        }
        start = end + 1;
    }
}

// the order of two words by their bytes, for memor.sort
static int suffix_word_order(const void *left, const void *right)
{
    return stage_text_order(*(const char *const *)left, *(const char *const *)right);
}

static int suffix_read(const char *path, SuffixSettings *settings, SuffixInput *input)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = input, .bytes = sizeof(*input), .val = 0);
    if (!stage_lines(path, &input->lines) || (input->lines.count < 4u))
    {
        return 0;
    }
    input->word = (const char **)stage_cell(input->lines.size + 1ull, sizeof(char *));
    input->suffix = (SuffixRow *)stage_cell(input->lines.count, sizeof(SuffixRow));
    input->noun = (SuffixNoun *)stage_cell(input->lines.count, sizeof(SuffixNoun));
    input->language = (SuffixRead *)stage_cell(input->lines.count, sizeof(SuffixRead));
    const char **const room = (const char **)stage_cell(input->lines.size + 1ull, sizeof(char *));
    int ok = (input->word != NULL) && (input->suffix != NULL) && (input->noun != NULL) && (input->language != NULL) &&
             (room != NULL);
    const char *heads[4] = {"permutations", "seed", "least_suffixes", "least_nouns"};
    unsigned long long *values[4] = {&settings->permutations, &settings->seed, &settings->least_suffixes,
                                     &settings->least_nouns};
    StageFields fields;
    for (unsigned long long at = 0ull; ok && (at < 4ull); at += 1ull)
    {
        stage_split(input->lines.line[at], &fields);
        ok = stage_fields_are(&fields, 2u) && stage_same(fields.field[0], heads[at]);
        *values[at] = ok ? strtoull(fields.field[1], NULL, 10) : 0ull;
    }
    unsigned long long at = 4ull;
    while (ok && (at < input->lines.count))
    {
        stage_split(input->lines.line[at], &fields);
        if (!stage_fields_are(&fields, 4u) || !stage_same(fields.field[0], "language"))
        {
            ok = 0;
            break;
        }
        SuffixRead *const language = &input->language[input->languages];
        input->languages += 1ull;
        language->name = fields.field[1];
        language->first_suffix = input->suffixes;
        language->first_noun = input->nouns;
        const unsigned long long suffixes = strtoull(fields.field[2], NULL, 10);
        const unsigned long long nouns = strtoull(fields.field[3], NULL, 10);
        at += 1ull;
        for (unsigned long long one = 0ull; ok && (one < suffixes + nouns); one += 1ull, at += 1ull)
        {
            const char *type = (one < suffixes) ? "suffix" : "noun";
            if (at >= input->lines.count)
            {
                ok = 0;
                break;
            }
            stage_split(input->lines.line[at], &fields);
            if (!stage_fields_are(&fields, 3u) || !stage_same(fields.field[0], type))
            {
                ok = 0;
                break;
            }
            if (one < suffixes)
            {
                SuffixRow *const row = &input->suffix[input->suffixes];
                input->suffixes += 1ull;
                row->first_segment = input->words;
                row->segments = suffix_words(fields.field[1], ' ', input);
                row->first_offered = input->words;
                row->offered = suffix_words(fields.field[2], ',', input);
                // the meanings offered sorted once, for the binary search of each noun's meaning
                EMBED_CALL(memor.sort, MemoriaCfg, .dst = &input->word[row->first_offered], .bytes = sizeof(char *),
                           .scratch = room, .count = row->offered, .order = suffix_word_order);
            }
            else
            {
                SuffixNoun *const noun = &input->noun[input->nouns];
                input->nouns += 1ull;
                noun->first_segment = input->words;
                noun->segments = suffix_words(fields.field[1], ' ', input);
                noun->meaning = fields.field[2];
            }
        }
        language->suffixes = input->suffixes - language->first_suffix;
        language->nouns = input->nouns - language->first_noun;
    }
    stage_cell_released((void *)room);
    return ok && (settings->permutations > 0ull);
}

// 1 where the suffix offers `meaning`, by a binary search of its sorted meanings
static int suffix_offers(const SuffixInput *input, const SuffixRow *row, const char *meaning)
{
    const char *const *const offered = &input->word[row->first_offered];
    unsigned long long low = 0ull;
    unsigned long long high = row->offered;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        const int order = stage_text_order(offered[middle], meaning);
        if (order == 0)
        {
            return 1;
        }
        low = (order < 0) ? (middle + 1ull) : low;
        high = (order < 0) ? high : middle;
    }
    return 0;
}

// 1 where the `count` words at one equal the `count` words at other, one by one
static int suffix_same_words(const char *const *one, const char *const *other, unsigned long long count)
{
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        if (!stage_same(one[at], other[at]))
        {
            return 0;
        }
    }
    return 1;
}

// whether the noun's segments end with the suffix's, the noun longer by between `least` and `max` segments
static int suffix_ends(const SuffixInput *input, const SuffixNoun *noun, const SuffixRow *suffix,
                       unsigned long long least, unsigned long long max)
{
    if ((noun->segments < suffix->segments + least) || (noun->segments > suffix->segments + max))
    {
        return 0;
    }
    return suffix_same_words(&input->word[suffix->first_segment],
                             &input->word[noun->first_segment + noun->segments - suffix->segments], suffix->segments);
}

// a count passed through, for its sum over a run of languages
static void suffix_pass_program(unsigned int bits, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int count = stage_field(program, bits);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, count, stage_constant(program, 0ull)));
}

// the larger of two counts, (x + y + |x - y|) / 2
static void suffix_larger_program(unsigned int bits, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int x = stage_field(program, bits);
    const unsigned int y = stage_field(program, bits);
    const unsigned int both = stage_step(program, ENGINE_RECORD_SUM, x, y);
    const unsigned int apart =
        stage_step(program, ENGINE_RECORD_ABSOLUTE, stage_step(program, ENGINE_RECORD_DIFFERENCE, x, y), 0u);
    const unsigned int twice = stage_step(program, ENGINE_RECORD_SUM, both, apart);
    stage_output(program, stage_step(program, ENGINE_RECORD_QUOTIENT, twice, stage_constant(program, 2ull)));
}

// p as (a + 1) / (P + 1)
static void suffix_value_program(unsigned int bits, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int above = stage_field(program, bits);
    const unsigned int shuffles = stage_field(program, bits);
    const unsigned int one = stage_constant(program, 1ull);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, above, one));
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, shuffles, one));
}

// a / b as its whole part and six digits, by the digits program; one text per value, at displayed
static int suffix_digits(SimResults *job, Stage *stage, const StageRational *values, unsigned long long count,
                         StageText *displayed, EngineError *error)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = stage, .bytes = sizeof(*stage), .val = 0);
    stage->name = "digits";
    stage_digits_program(stage_widest(values, count) + 1u, &stage->program);
    StageTable rows = {NULL, 0ull, 0u};
    int ok = stage_table_taken(&rows, count, 2u);
    for (unsigned long long at = 0ull; ok && (at < count); at += 1ull)
    {
        stage_row(&rows, at)[0] = values[at].num;
        stage_row(&rows, at)[1] = values[at].den;
    }
    StageTable out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(job, stage, &rows, &out, error);
    for (unsigned long long at = 0ull; ok && (at < out.rows); at += 1ull)
    {
        displayed[at] = stage_decimal_of(stage_row(&out, at)[0], stage_row(&out, at)[1]);
    }
    stage_table_released(&out);
    stage_table_released(&rows);
    return ok;
}

// the words at `words`, `count` of them, put one after the other
static void suffix_joined(VerbaLinea *line, const char *const *words, unsigned long long count)
{
    for (unsigned long long part = 0ull; part < count; part += 1ull)
    {
        mmgr_line_put(line, words[part]);
    }
}

int program_main(int count, char **arguments)
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &settings, .bytes = sizeof(settings), .val = 0);
    SuffixInput input;
    if (!suffix_read(arguments[1], &settings, &input))
    {
        fprintf(stderr, "  lexical_suffix_origins: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    // the languages retained, and each one's relation bits over its pairs, suffix by suffix and noun by noun
    SuffixLanguage *const languages = (SuffixLanguage *)stage_cell(input.languages, sizeof(SuffixLanguage));
    if (languages == NULL)
    {
        return 2;
    }
    unsigned long long language_count = 0ull;
    unsigned int words = 1u;
    unsigned long long all_pairs = 0ull;
    unsigned long long nouns_max = 0ull;
    for (unsigned long long at = 0ull; at < input.languages; at += 1ull)
    {
        const SuffixRead *const read = &input.language[at];
        if ((read->suffixes < settings.least_suffixes) || (read->nouns < settings.least_nouns))
        {
            continue;
        }
        SuffixLanguage *const language = &languages[language_count];
        language->read = read;
        const unsigned long long pairs = read->suffixes * read->nouns;
        const unsigned int own_words = (unsigned int)((pairs + 31u) / 32u);
        language->words = own_words;
        language->flags =
            (unsigned int *)stage_cell((unsigned long long)SUFFIX_RELATIONS * own_words, sizeof(unsigned int));
        if (language->flags == NULL)
        {
            return 2;
        }
        for (unsigned long long suffix = 0ull; suffix < read->suffixes; suffix += 1ull)
        {
            for (unsigned long long noun = 0ull; noun < read->nouns; noun += 1ull)
            {
                const unsigned long long bit = suffix * read->nouns + noun;
                const SuffixNoun *const own = &input.noun[read->first_noun + noun];
                const SuffixRow *const end = &input.suffix[read->first_suffix + suffix];
                const int same =
                    (own->segments == end->segments) &&
                    suffix_same_words(&input.word[own->first_segment], &input.word[end->first_segment], own->segments);
                const unsigned int relation_flags[SUFFIX_RELATIONS] = {
                    1u, (unsigned int)suffix_ends(&input, own, end, 1u, 2u),
                    (unsigned int)suffix_ends(&input, own, end, 1u, own->segments), (unsigned int)same};
                for (unsigned int relation = 0u; relation < SUFFIX_RELATIONS; relation += 1u)
                {
                    language->flags[relation * own_words + bit / 32u] |= relation_flags[relation] << (bit % 32u);
                }
            }
        }
        words = (own_words > words) ? own_words : words;
        nouns_max = (read->nouns > nouns_max) ? read->nouns : nouns_max;
        all_pairs += pairs;
        language_count += 1ull;
    }
    if (language_count == 0ull)
    {
        fprintf(stderr, "  lexical_suffix_origins: no language contains %llu suffixes and %llu nouns\n",
                settings.least_suffixes, settings.least_nouns);
        return 2;
    }

    // the count program: lanes ordered by relation, then run (the identity first), then language, then word
    const unsigned long long shuffles = settings.permutations;
    const unsigned long long runs = shuffles + 1ull;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    Stage count_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &count_stage, .bytes = sizeof(count_stage), .val = 0);
    count_stage.name = "count";
    stage_count_program(1, &count_stage.program);
    const unsigned long long count_lanes = SUFFIX_RELATIONS * runs * language_count * words;
    unsigned int *records = (unsigned int *)stage_cell(count_lanes * 2ull, sizeof(unsigned int));
    unsigned long long *const order = (unsigned long long *)stage_cell(nouns_max, sizeof(unsigned long long));
    unsigned int *const same = (unsigned int *)stage_cell(words, sizeof(unsigned int));
    int ok = (records != NULL) && (order != NULL) && (same != NULL);
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        const SuffixLanguage &language = languages[at];
        const SuffixRead *const read = language.read;
        const unsigned long long nouns = read->nouns;
        const unsigned long long key = sim_mix(settings.seed ^ sim_mix(at));
        for (unsigned long long run = 0ull; run < runs; run += 1ull)
        {
            for (unsigned long long noun = 0ull; noun < nouns; noun += 1ull)
            {
                order[noun] = noun;
            }
            if (run > 0ull)
            {
                for (unsigned long long place = nouns - 1ull; place > 0ull; place -= 1ull)
                {
                    const unsigned long long other = sim_draw_below(key, (run * nouns) + place, place + 1ull);
                    const unsigned long long held = order[place];
                    order[place] = order[other];
                    order[other] = held;
                }
            }
            EMBED_CALL(memor.set, MemoriaCfg, .dst = same, .bytes = words * sizeof(unsigned int), .val = 0);
            for (unsigned long long suffix = 0ull; suffix < read->suffixes; suffix += 1ull)
            {
                for (unsigned long long noun = 0ull; noun < nouns; noun += 1ull)
                {
                    const unsigned long long bit = suffix * nouns + noun;
                    const unsigned int offered =
                        (unsigned int)suffix_offers(&input, &input.suffix[read->first_suffix + suffix],
                                                    input.noun[read->first_noun + order[noun]].meaning);
                    same[bit / 32u] |= offered << (bit % 32u);
                }
            }
            for (unsigned int relation = 0u; relation < SUFFIX_RELATIONS; relation += 1u)
            {
                const unsigned long long first = ((relation * runs + run) * language_count + at) * words;
                for (unsigned int word = 0u; word < language.words; word += 1u)
                {
                    records[(first + word) * 2ull] = same[word];
                    records[(first + word) * 2ull + 1ull] = language.flags[relation * language.words + word];
                }
            }
        }
    }
    ok = ok && stage_position(&job, &count_stage, &error);
    const unsigned long long declared =
        count_lanes * (2ull + (ok ? count_stage.layout.out_limbs : 1u)) * sizeof(unsigned int);
    ok = ok && sim_job_submit(&job, "lexical_suffix_origins", count, arguments, declared);
    StageTable counted = {NULL, 0ull, 0u};
    const unsigned int first_output[1] = {0u};
    ok = ok && stage_summed(&job, &count_stage, records, count_lanes * 2ull, count_lanes, words, first_output, 1u,
                            &counted, &error);
    stage_cell_released(records);
    records = NULL;
    auto counts = [&](unsigned int relation, unsigned long long run,
                      unsigned long long language) -> const StageWhole & {
        return stage_row(&counted, 0ull)[(relation * runs + run) * language_count + language];
    };
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &pass_stage, .bytes = sizeof(pass_stage), .val = 0);
    pass_stage.name = "pass";
    suffix_pass_program(count_bits, &pass_stage.program);
    ok = ok && stage_position(&job, &pass_stage, &error);
    const unsigned long long pass_lanes = 3ull * runs * language_count;
    const unsigned int pass_limbs = (pass_stage.program.record_bits + 31u) / 32u;
    unsigned int *const pass_records = (unsigned int *)stage_cell(pass_lanes * pass_limbs, sizeof(unsigned int));
    ok = ok && (pass_records != NULL);
    for (unsigned int which = 0u; ok && (which < 3u); which += 1u)
    {
        for (unsigned long long run = 0ull; run < runs; run += 1ull)
        {
            for (unsigned long long at = 0ull; at < language_count; at += 1ull)
            {
                const unsigned long long lane = (which * runs + run) * language_count + at;
                stage_put(&pass_records[lane * pass_limbs], pass_stage.program.field_offset[0],
                          pass_stage.program.field_bits[0], counts(tested[which], run, at));
            }
        }
    }
    StageTable pooled = {NULL, 0ull, 0u};
    ok = ok && stage_summed(&job, &pass_stage, pass_records, pass_lanes * pass_limbs, pass_lanes, language_count,
                            first_output, 1u, &pooled, &error);
    const StageWhole *const pooled_row = ok ? stage_row(&pooled, 0ull) : NULL;

    // the above program: each language's shuffles and the pooled shuffles, against their observed counts
    Stage above_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &above_stage, .bytes = sizeof(above_stage), .val = 0);
    above_stage.name = "above";
    stage_above_program(count_bits + 1u, &above_stage.program);
    ok = ok && stage_position(&job, &above_stage, &error);
    const unsigned long long sets = 2ull * (language_count + 1ull);
    const unsigned long long above_lanes = sets * shuffles;
    const unsigned int above_limbs = (above_stage.program.record_bits + 31u) / 32u;
    unsigned int *const above_records = (unsigned int *)stage_cell(above_lanes * above_limbs, sizeof(unsigned int));
    ok = ok && (above_records != NULL);
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        for (unsigned long long set = 0ull; set <= language_count; set += 1ull)
        {
            const StageWhole &observed =
                (set < language_count) ? counts(tested[which], 0ull, set) : pooled_row[which * runs];
            for (unsigned long long shuffle = 0ull; shuffle < shuffles; shuffle += 1ull)
            {
                const StageWhole &drawn = (set < language_count) ? counts(tested[which], 1ull + shuffle, set)
                                                                 : pooled_row[which * runs + 1ull + shuffle];
                unsigned int *record =
                    &above_records[(((which * (language_count + 1ull)) + set) * shuffles + shuffle) * above_limbs];
                stage_put(record, above_stage.program.field_offset[0], above_stage.program.field_bits[0], drawn);
                stage_put(record, above_stage.program.field_offset[1], above_stage.program.field_bits[1], observed);
            }
        }
    }
    StageTable aboves = {NULL, 0ull, 0u};
    const unsigned int both_outputs[2] = {0u, 1u};
    ok = ok && stage_summed(&job, &above_stage, above_records, above_lanes * above_limbs, above_lanes, shuffles,
                            both_outputs, 2u, &aboves, &error);
    StageWhole pooled_above[2];
    StageWhole pooled_sum[2];
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            languages[at].above[tested[which]] = stage_row(&aboves, 0ull)[which * (language_count + 1ull) + at];
            languages[at].sum[tested[which]] = stage_row(&aboves, 1ull)[which * (language_count + 1ull) + at];
        }
        pooled_above[which] = stage_row(&aboves, 0ull)[which * (language_count + 1ull) + language_count];
        pooled_sum[which] = stage_row(&aboves, 1ull)[which * (language_count + 1ull) + language_count];
    }

    // The larger program, in rounds: the largest pooled count among the shuffles of each relation. Each round puts its
    // larger counts at the front of the counts left, in place
    StageWhole *const left[2] = {(StageWhole *)stage_cell(shuffles, sizeof(StageWhole)),
                                 (StageWhole *)stage_cell(shuffles, sizeof(StageWhole))};
    unsigned long long left_count[2] = {0ull, 0ull};
    Stage *const larger_stages = (Stage *)stage_cell(SUFFIX_ROUNDS_MAX, sizeof(Stage));
    unsigned int rounds = 0u;
    ok = ok && (left[0] != NULL) && (left[1] != NULL) && (larger_stages != NULL);
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = left[which], .src = &pooled_row[which * runs + 1ull],
                   .bytes = (size_t)(shuffles * sizeof(StageWhole)));
        left_count[which] = shuffles;
    }
    while (ok && ((left_count[0] > 1ull) || (left_count[1] > 1ull)) && (rounds < SUFFIX_ROUNDS_MAX))
    {
        StageTable rows = {NULL, 0ull, 0u};
        ok = stage_table_taken(&rows, (left_count[0] / 2ull) + (left_count[1] / 2ull), 2u);
        unsigned long long row = 0ull;
        for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
        {
            for (unsigned long long at = 0ull; (at + 1ull) < left_count[which]; at += 2ull)
            {
                stage_row(&rows, row)[0] = left[which][at];
                stage_row(&rows, row)[1] = left[which][at + 1ull];
                row += 1ull;
            }
        }
        Stage *stage = &larger_stages[rounds];
        rounds += 1u;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = stage, .bytes = sizeof(*stage), .val = 0);
        stage->name = "larger";
        suffix_larger_program(count_bits + 1u, &stage->program);
        StageTable out = {NULL, 0ull, 0u};
        ok = ok && stage_rows(&job, stage, &rows, &out, &error);
        unsigned long long lane = 0ull;
        for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
        {
            unsigned long long next = 0ull;
            for (unsigned long long at = 0ull; at < left_count[which]; at += 2ull)
            {
                if ((at + 1ull) < left_count[which])
                {
                    left[which][next] = stage_row(&out, lane)[0];
                    lane += 1ull;
                }
                else
                {
                    left[which][next] = left[which][at];
                }
                next += 1ull;
            }
            left_count[which] = next;
        }
        stage_table_released(&out);
        stage_table_released(&rows);
    }

    // the value program: every p, per language for the tail relation and pooled for both
    Stage value_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &value_stage, .bytes = sizeof(value_stage), .val = 0);
    value_stage.name = "value";
    suffix_value_program(stage_bits(stage_whole(shuffles)) + 1u, &value_stage.program);
    StageTable value_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&value_rows, language_count + 2ull, 2u);
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        stage_row(&value_rows, at)[0] = languages[at].above[SUFFIX_TAIL];
        stage_row(&value_rows, at)[1] = stage_whole(shuffles);
    }
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        stage_row(&value_rows, language_count + which)[0] = pooled_above[which];
        stage_row(&value_rows, language_count + which)[1] = stage_whole(shuffles);
    }
    StageTable values = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &value_stage, &value_rows, &values, &error);

    // the digits program: each null mean and each p
    const unsigned long long printed_count = (3ull * language_count) + 4ull;
    StageRational *const printed = (StageRational *)stage_cell(printed_count, sizeof(StageRational));
    StageText *const displayed = (StageText *)stage_cell(printed_count, sizeof(StageText));
    ok = ok && (printed != NULL) && (displayed != NULL);
    unsigned long long printed_at = 0ull;
    for (unsigned long long at = 0ull; ok && (at < language_count); at += 1ull)
    {
        printed[printed_at] = {languages[at].sum[SUFFIX_KINKADE], stage_whole(shuffles)};
        printed[printed_at + 1ull] = {languages[at].sum[SUFFIX_TAIL], stage_whole(shuffles)};
        printed[printed_at + 2ull] = stage_pair_of(stage_row(&values, at));
        printed_at += 3ull;
    }
    for (unsigned int which = 0u; ok && (which < 2u); which += 1u)
    {
        printed[printed_at] = {pooled_sum[which], stage_whole(shuffles)};
        printed[printed_at + 1ull] = stage_pair_of(stage_row(&values, language_count + which));
        printed_at += 2ull;
    }
    Stage digits_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &digits_stage, .bytes = sizeof(digits_stage), .val = 0);
    ok = ok && suffix_digits(&job, &digits_stage, printed, printed_at, displayed, &error);

    // The tail pairs observed, read from the identity run's bits: counted first, then their texts written, each in a
    // text of a fixed bound
    unsigned long long found_count = 0ull;
    StageText *found = NULL;
    int found_fits = 1;
    for (int pass = 0; ok && (pass < 2); pass += 1)
    {
        unsigned long long found_at = 0ull;
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            const SuffixLanguage &language = languages[at];
            const SuffixRead *const read = language.read;
            for (unsigned long long suffix = 0ull; suffix < read->suffixes; suffix += 1ull)
            {
                for (unsigned long long noun = 0ull; noun < read->nouns; noun += 1ull)
                {
                    const unsigned long long bit = suffix * read->nouns + noun;
                    const int tail = (language.flags[SUFFIX_TAIL * language.words + bit / 32u] >> (bit % 32u)) & 1u;
                    const int kinkade =
                        (language.flags[SUFFIX_KINKADE * language.words + bit / 32u] >> (bit % 32u)) & 1u;
                    const SuffixRow *const row = &input.suffix[read->first_suffix + suffix];
                    const SuffixNoun *const own = &input.noun[read->first_noun + noun];
                    if (!tail || !suffix_offers(&input, row, own->meaning))
                    {
                        continue;
                    }
                    if (pass == 1)
                    {
                        VerbaLinea line = mmgr_line_over(found[found_at].bytes, sizeof(found[found_at].bytes));
                        mmgr_line_put(&line, kinkade ? "Kinkade" : "tail   ");
                        mmgr_line_put(&line, "  ");
                        mmgr_line_put(&line, read->name);
                        mmgr_line_put(&line, "  ");
                        suffix_joined(&line, &input.word[own->first_segment], own->segments);
                        mmgr_line_put(&line, " ~ =");
                        suffix_joined(&line, &input.word[row->first_segment], row->segments);
                        mmgr_line_put(&line, " '");
                        mmgr_line_put(&line, own->meaning);
                        mmgr_line_put(&line, "'");
                        found_fits = found_fits && (line.at < sizeof(found[found_at].bytes));
                        (void)mmgr_line_finish(&line);
                    }
                    found_at += 1ull;
                }
            }
        }
        if (pass == 0)
        {
            found_count = found_at;
            found = (StageText *)stage_cell(found_count, sizeof(StageText));
            ok = found != NULL;
        }
    }
    if (ok && !found_fits)
    {
        fprintf(stderr, "  lexical_suffix_origins: a tail pair's text passes %u bytes\n", STAGE_TEXT_BYTES);
        ok = 0;
    }

    const char *names[2] = {"Kinkade (noun one or two segments longer)", "tail (noun longer, ends in it)"};
    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "permutations %llu seed %llu least_suffixes %llu least_nouns %llu\n", settings.permutations,
                settings.seed, settings.least_suffixes, settings.least_nouns);
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            const SuffixLanguage &language = languages[at];
            fprintf(
                out,
                "language %s suffixes %llu nouns %llu pairs %s identical %s kinkade %s kinkade_sum %s kinkade_above %s "
                "tail %s tail_sum %s tail_above %s p %s/%s\n",
                language.read->name, language.read->suffixes, language.read->nouns,
                stage_text_of(language.observed[SUFFIX_PAIRS]).bytes,
                stage_text_of(language.observed[SUFFIX_IDENTICAL]).bytes,
                stage_text_of(language.observed[SUFFIX_KINKADE]).bytes,
                stage_text_of(language.sum[SUFFIX_KINKADE]).bytes, stage_text_of(language.above[SUFFIX_KINKADE]).bytes,
                stage_text_of(language.observed[SUFFIX_TAIL]).bytes, stage_text_of(language.sum[SUFFIX_TAIL]).bytes,
                stage_text_of(language.above[SUFFIX_TAIL]).bytes, stage_text_of(stage_row(&values, at)[0]).bytes,
                stage_text_of(stage_row(&values, at)[1]).bytes);
        }
        for (unsigned int which = 0u; which < 2u; which += 1u)
        {
            fprintf(out, "pooled %s pairs %s observed %s sum %s largest %s above %s p %s/%s\n",
                    (which == 0u) ? "kinkade" : "tail", stage_text_of(pooled_row[2u * runs]).bytes,
                    stage_text_of(pooled_row[which * runs]).bytes, stage_text_of(pooled_sum[which]).bytes,
                    stage_text_of(left[which][0]).bytes, stage_text_of(pooled_above[which]).bytes,
                    stage_text_of(stage_row(&values, language_count + which)[0]).bytes,
                    stage_text_of(stage_row(&values, language_count + which)[1]).bytes);
        }
        for (unsigned long long at = 0ull; at < found_count; at += 1ull)
        {
            fprintf(out, "%s\n", found[at].bytes);
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
            mmgr_line_put_right(output_line, "LS", 4u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "nouns", 5u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "pairs", 5u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "Kinkade", 7u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "null mean", 9u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "tail", 5u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "null mean", 9u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "ident", 5u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "p (tail)", 9u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            const SuffixLanguage &language = languages[at];
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, stage_displayed(language.read->name, 18u, 1).bytes);
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal_columns(output_line, language.read->suffixes, 4u);
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal_columns(output_line, language.read->nouns, 5u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, stage_text_of(language.observed[SUFFIX_PAIRS]).bytes, 5u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, stage_text_of(language.observed[SUFFIX_KINKADE]).bytes, 7u);
                mmgr_line_put(output_line, " ");
                {
                    char linea_piece[9u + 1u];
                    VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                    (void)mmgr_line_put_max(&linea_piece_line, displayed[at * 3u].bytes, 9u);
                    (void)mmgr_line_finish(&linea_piece_line);
                    mmgr_line_put_right(output_line, linea_piece, 9u);
                }
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, stage_text_of(language.observed[SUFFIX_TAIL]).bytes, 5u);
                mmgr_line_put(output_line, " ");
                {
                    char linea_piece[9u + 1u];
                    VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                    (void)mmgr_line_put_max(&linea_piece_line, displayed[at * 3u + 1u].bytes, 9u);
                    (void)mmgr_line_finish(&linea_piece_line);
                    mmgr_line_put_right(output_line, linea_piece, 9u);
                }
                mmgr_line_put(output_line, " ");
                mmgr_line_put_right(output_line, stage_text_of(language.observed[SUFFIX_IDENTICAL]).bytes, 5u);
                mmgr_line_put(output_line, " ");
                {
                    char linea_piece[9u + 1u];
                    VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                    (void)mmgr_line_put_max(&linea_piece_line, displayed[at * 3u + 2u].bytes, 9u);
                    (void)mmgr_line_finish(&linea_piece_line);
                    mmgr_line_put_right(output_line, linea_piece, 9u);
                }
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        for (unsigned int which = 0u; which < 2u; which += 1u)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "pooled ");
            mmgr_line_put(output_line, names[which]);
            mmgr_line_put(output_line, ": observed ");
            mmgr_line_put(output_line, stage_text_of(pooled_row[which * runs]).bytes);
            mmgr_line_put(output_line, ", null mean ");
            mmgr_line_put(output_line, displayed[language_count * 3u + which * 2u].bytes);
            mmgr_line_put(output_line, ", null max ");
            mmgr_line_put(output_line, stage_text_of(left[which][0]).bytes);
            mmgr_line_put(output_line, ", p ");
            mmgr_line_put(output_line, stage_text_of(stage_row(&values, language_count + which)[0]).bytes);
            mmgr_line_put(output_line, "/");
            mmgr_line_put(output_line, stage_text_of(stage_row(&values, language_count + which)[1]).bytes);
            mmgr_line_put(output_line, " = ");
            mmgr_line_put(output_line, displayed[language_count * 3u + which * 2u + 1u].bytes);
            mmgr_line_put(output_line, " over ");
            mmgr_line_decimal(output_line, shuffles, 1u);
            mmgr_line_put(output_line, " shuffles, ");
            mmgr_line_put(output_line, stage_text_of(pooled_row[2u * runs]).bytes);
            mmgr_line_put(output_line, " same-meaning pairs");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < found_count; at += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "   ");
            mmgr_line_put(output_line, found[at].bytes);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(
                output_line,
                "each mean and p is the whole part and first six digits of the exact rational in the records");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    stage_release(&count_stage);
    stage_release(&pass_stage);
    stage_release(&above_stage);
    for (unsigned int at = 0u; at < rounds; at += 1u)
    {
        stage_release(&larger_stages[at]);
    }
    stage_release(&value_stage);
    stage_release(&digits_stage);
    stage_table_released(&values);
    stage_table_released(&value_rows);
    stage_table_released(&aboves);
    stage_table_released(&pooled);
    stage_table_released(&counted);
    void *const cells[10] = {found,   displayed,     printed,      larger_stages, left[1],
                             left[0], above_records, pass_records, same,          order};
    for (unsigned int each = 0u; each < 10u; each += 1u)
    {
        stage_cell_released(cells[each]);
    }
    for (unsigned long long at = 0ull; at < language_count; at += 1ull)
    {
        stage_cell_released(languages[at].flags);
    }
    stage_cell_released(languages);
    suffix_released(&input);
    return sim_close(&job, "lexical_suffix_origins");
}
