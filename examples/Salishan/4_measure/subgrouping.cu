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
// (m P - S + n P) / (n P), which is never negative since each m_k is at max n. Orders and means are unchanged by
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
// The input is what subgrouping.py writes. The output contains every pair's m, S, n, P and a, its excess + 1 and p
// whole, and the verdicts.
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

// the pool of the host: the counts of every shuffle of every pair take a whole number each
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 31u;

// an absent entry of the table of pairs
#define GROUP_ABSENT (~0ull)

// the groups of means: Interior, Central, between them, and Nuxalk with each of three branches
#define GROUP_MEANS 6u

// the stages the means take at max: each round of the add program halves the values left, then one scale
#define GROUP_MEAN_STAGES 66u

// a form of one meaning: its first two classes and the paper it came from, both split in place from its field
typedef struct
{
    const char *classes;
    const char *paper;
} GroupForm;

// a meaning of a language: its name, and its forms from form[first] on, `count` of them
typedef struct
{
    const char *meaning;
    unsigned long long first;
    unsigned long long count;
} GroupMeaning;

// a language: its branch and name, and its meanings from meaning[first] on, `count` of them, sorted by name, each once
typedef struct
{
    const char *branch;
    const char *name;
    unsigned long long first;
    unsigned long long count;
} GroupLanguage;

// one pair of languages compared, its match bits over its shared meanings in a cell of `shared` x `shared` bytes, and
// what the machine gives it
typedef struct
{
    unsigned long long left;
    unsigned long long right;
    unsigned long long shared;
    unsigned char *matches;
    StageWhole matched;
    StageWhole shuffled_sum;
    StageWhole above;
    StageRational lifted;
    StageRational chance;
    StageText excess_text;
} GroupPair;

typedef struct
{
    unsigned long long permutations;
    unsigned long long seed;
    unsigned long long least_shared;
    unsigned long long least_meanings;
    unsigned long long p_below[2];
} GroupSettings;

// The input read once: its lines, and its forms, meanings and languages. Each is a cell of the temporary tier, the
// forms at the input's bytes and the rest at its lines, which bound them before they fill
typedef struct
{
    StageLines lines;
    GroupForm *form;
    unsigned long long forms;
    GroupMeaning *meaning;
    unsigned long long meanings;
    GroupLanguage *language;
    unsigned long long languages;
} GroupInput;

static void group_released(GroupInput *input)
{
    stage_cell_released(input->language);
    stage_cell_released(input->meaning);
    stage_cell_released(input->form);
    stage_lines_released(&input->lines);
}

// the order of two meanings by their names, for memor.sort, which keeps equal names in the order they were read
static int group_meaning_order(const void *left, const void *right)
{
    return stage_text_order(((const GroupMeaning *)left)->meaning, ((const GroupMeaning *)right)->meaning);
}

// A language's meanings sorted by name, and of each run of one name the last read kept, as a map's assignment keeps
// it. 0 where the pool cannot meet the sort's room
static int group_meanings_settled(GroupInput *input, GroupLanguage *language)
{
    GroupMeaning *const meanings = &input->meaning[language->first];
    GroupMeaning *const room = (GroupMeaning *)stage_cell(language->count, sizeof(GroupMeaning));
    if (room == NULL)
    {
        return 0;
    }
    EMBED_CALL(memor.sort, MemoriaCfg, .dst = meanings, .bytes = sizeof(GroupMeaning), .scratch = room,
               .count = language->count, .order = group_meaning_order);
    stage_cell_released(room);
    unsigned long long kept = 0ull;
    for (unsigned long long at = 0ull; at < language->count; at += 1ull)
    {
        const int last =
            ((at + 1ull) == language->count) || !stage_same(meanings[at].meaning, meanings[at + 1ull].meaning);
        if (last)
        {
            meanings[kept] = meanings[at];
            kept += 1ull;
        }
    }
    language->count = kept;
    return 1;
}

static int group_read(const char *path, GroupSettings *settings, GroupInput *input)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = input, .bytes = sizeof(*input), .val = 0);
    if (!stage_lines(path, &input->lines) || (input->lines.count < 5u))
    {
        return 0;
    }
    input->form = (GroupForm *)stage_cell(input->lines.size + 1ull, sizeof(GroupForm));
    input->meaning = (GroupMeaning *)stage_cell(input->lines.count, sizeof(GroupMeaning));
    input->language = (GroupLanguage *)stage_cell(input->lines.count, sizeof(GroupLanguage));
    if ((input->form == NULL) || (input->meaning == NULL) || (input->language == NULL))
    {
        return 0;
    }
    const char *heads[5] = {"permutations", "seed", "least_shared", "least_meanings", "p_below"};
    unsigned long long *values[4] = {&settings->permutations, &settings->seed, &settings->least_shared,
                                     &settings->least_meanings};
    StageFields fields;
    for (unsigned long long at = 0ull; at < 5ull; at += 1ull)
    {
        stage_split(input->lines.line[at], &fields);
        if ((fields.count < 2u) || (fields.count > STAGE_LINE_FIELDS) || !stage_same(fields.field[0], heads[at]))
        {
            return 0;
        }
        if (at < 4ull)
        {
            *values[at] = strtoull(fields.field[1], NULL, 10);
        }
        else if (fields.count == 3u)
        {
            settings->p_below[0] = strtoull(fields.field[1], NULL, 10);
            settings->p_below[1] = strtoull(fields.field[2], NULL, 10);
        }
        else
        {
            return 0;
        }
    }
    unsigned long long at = 5ull;
    while (at < input->lines.count)
    {
        stage_split(input->lines.line[at], &fields);
        if (!stage_fields_are(&fields, 4u) || !stage_same(fields.field[0], "language"))
        {
            return 0;
        }
        GroupLanguage *const language = &input->language[input->languages];
        input->languages += 1ull;
        language->branch = fields.field[1];
        language->name = fields.field[2];
        language->first = input->meanings;
        const unsigned long long count = strtoull(fields.field[3], NULL, 10);
        at += 1ull;
        for (unsigned long long one = 0ull; one < count; one += 1ull, at += 1ull)
        {
            if (at >= input->lines.count)
            {
                return 0;
            }
            stage_split(input->lines.line[at], &fields);
            if (fields.count > STAGE_LINE_FIELDS)
            {
                fprintf(stderr, "  subgrouping: a meaning's line holds more than %u fields\n", STAGE_LINE_FIELDS);
                return 0;
            }
            GroupMeaning *const meaning = &input->meaning[input->meanings];
            input->meanings += 1ull;
            meaning->meaning = fields.field[0];
            meaning->first = input->forms;
            for (unsigned int field = 1u; field < fields.count; field += 1u)
            {
                char *const bar = (char *)EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = fields.field[field],
                                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '|');
                if (bar == NULL)
                {
                    return 0;
                }
                *bar = '\0';
                input->form[input->forms] = GroupForm{fields.field[field], bar + 1};
                input->forms += 1ull;
            }
            meaning->count = input->forms - meaning->first;
        }
        language->count = input->meanings - language->first;
        if (!group_meanings_settled(input, language))
        {
            return 0;
        }
    }
    return (settings->permutations > 0ull) && (settings->p_below[1] > 0ull);
}

// a language's meaning of the name `wanted`, by a binary search of its sorted meanings; NULL where it has none
static const GroupMeaning *group_meaning_of(const GroupInput *input, const GroupLanguage *language, const char *wanted)
{
    const GroupMeaning *const meanings = &input->meaning[language->first];
    unsigned long long low = 0ull;
    unsigned long long high = language->count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        const int order = stage_text_order(meanings[middle].meaning, wanted);
        if (order == 0)
        {
            return &meanings[middle];
        }
        low = (order < 0) ? (middle + 1ull) : low;
        high = (order < 0) ? high : middle;
    }
    return NULL;
}

// two forms lists match where some form of each agrees on its two classes and the two come from different papers
static int group_match(const GroupInput *input, const GroupMeaning *first, const GroupMeaning *second)
{
    for (unsigned long long one = 0ull; one < first->count; one += 1ull)
    {
        for (unsigned long long two = 0ull; two < second->count; two += 1ull)
        {
            const GroupForm *const left = &input->form[first->first + one];
            const GroupForm *const right = &input->form[second->first + two];
            if (stage_same(left->classes, right->classes) && !stage_same(left->paper, right->paper))
            {
                return 1;
            }
        }
    }
    return 0;
}

// 1 where `branch` is one of the `count` branches at `branches`
static int group_branch_in(const char *branch, const char *const *branches, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (stage_same(branch, branches[at]))
        {
            return 1;
        }
    }
    return 0;
}

// the order of two retained languages by branch, then name, for memor.sort
static const GroupLanguage *s_group_languages;

static int group_language_order(const void *left, const void *right)
{
    const GroupLanguage *const one = &s_group_languages[*(const unsigned long long *)left];
    const GroupLanguage *const two = &s_group_languages[*(const unsigned long long *)right];
    const int branch = stage_text_order(one->branch, two->branch);
    return (branch != 0) ? branch : stage_text_order(one->name, two->name);
}

// excess + 1 as (m P - S + n P) / (n P), and p as (a + 1) / (P + 1)
static void group_value_program(unsigned int bits, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
    const unsigned int a = stage_field(program, bits);
    const unsigned int b = stage_field(program, bits);
    const unsigned int k = stage_field(program, bits);
    stage_output(program, stage_step(program, ENGINE_RECORD_SUM, a, stage_constant(program, 0ull)));
    stage_output(program, stage_step(program, ENGINE_RECORD_PRODUCT, b, k));
}

// a / b less one, as its sign, whole part and first six digits after the point
static void group_signed_program(unsigned int bits, StageProgram *program)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = program, .bytes = sizeof(*program), .val = 0);
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

// The groups of values for the means: group g's values at value[g], count[g] of them, each a cell of the temporary
// tier at the count of pairs, which bounds every group
typedef struct
{
    StageRational *value[GROUP_MEANS];
    unsigned long long count[GROUP_MEANS];
} GroupGroups;

// The sum of each group of values, by rounds of the add program over pairs, and each sum scaled to its mean. Each
// round puts its sums at the front of the values left, in place, in a copy of the groups. Each stage goes at stages,
// *stage_count of them
static int group_means(SimResults *job, const GroupGroups *groups, StageRational *means, Stage *stages,
                       unsigned int *stage_count, EngineError *error)
{
    StageRational *copied[GROUP_MEANS];
    unsigned long long left[GROUP_MEANS];
    unsigned long long widths_room = 0ull;
    int ok = 1;
    for (unsigned int group = 0u; group < GROUP_MEANS; group += 1u)
    {
        copied[group] = (StageRational *)stage_cell(groups->count[group], sizeof(StageRational));
        ok = ok && (copied[group] != NULL);
        if (copied[group] != NULL)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copied[group], .src = groups->value[group],
                       .bytes = (size_t)(groups->count[group] * sizeof(StageRational)));
        }
        left[group] = groups->count[group];
        widths_room += groups->count[group];
    }
    StageRational *const widths = (StageRational *)stage_cell(widths_room, sizeof(StageRational));
    ok = ok && (widths != NULL);
    while (ok && (*stage_count < (GROUP_MEAN_STAGES - 1u)))
    {
        unsigned long long row_count = 0ull;
        for (unsigned int group = 0u; group < GROUP_MEANS; group += 1u)
        {
            row_count += left[group] / 2ull;
        }
        if (row_count == 0ull)
        {
            break;
        }
        StageTable rows = {NULL, 0ull, 0u};
        ok = stage_table_taken(&rows, row_count, 4u);
        unsigned long long row = 0ull;
        for (unsigned int group = 0u; ok && (group < GROUP_MEANS); group += 1u)
        {
            for (unsigned long long at = 0ull; (at + 1ull) < left[group]; at += 2ull)
            {
                StageWhole *const entry = stage_row(&rows, row);
                entry[0] = copied[group][at].num;
                entry[1] = copied[group][at].den;
                entry[2] = copied[group][at + 1ull].num;
                entry[3] = copied[group][at + 1ull].den;
                widths[2ull * row] = copied[group][at];
                widths[2ull * row + 1ull] = copied[group][at + 1ull];
                row += 1ull;
            }
        }
        Stage *stage = &stages[*stage_count];
        *stage_count += 1u;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = stage, .bytes = sizeof(*stage), .val = 0);
        stage->name = "add";
        group_add_program(stage_widest(widths, 2ull * row_count), &stage->program);
        StageTable out = {NULL, 0ull, 0u};
        ok = ok && stage_rows(job, stage, &rows, &out, error);
        unsigned long long lane = 0ull;
        for (unsigned int group = 0u; ok && (group < GROUP_MEANS); group += 1u)
        {
            unsigned long long next = 0ull;
            for (unsigned long long at = 0ull; at < left[group]; at += 2ull)
            {
                if ((at + 1ull) < left[group])
                {
                    copied[group][next] = stage_pair_of(stage_row(&out, lane));
                    lane += 1ull;
                }
                else
                {
                    copied[group][next] = copied[group][at];
                }
                next += 1ull;
            }
            left[group] = next;
        }
        stage_table_released(&out);
        stage_table_released(&rows);
    }
    StageTable rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&rows, GROUP_MEANS, 3u);
    for (unsigned int group = 0u; ok && (group < GROUP_MEANS); group += 1u)
    {
        const StageRational sum = (left[group] == 0ull) ? stage_unit() : copied[group][0];
        stage_row(&rows, group)[0] = sum.num;
        stage_row(&rows, group)[1] = sum.den;
        stage_row(&rows, group)[2] = stage_whole((groups->count[group] == 0ull) ? 1ull : groups->count[group]);
        widths[group] = sum;
    }
    Stage *stage = &stages[*stage_count];
    *stage_count += 1u;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = stage, .bytes = sizeof(*stage), .val = 0);
    stage->name = "scale";
    group_scale_program(stage_widest(widths, GROUP_MEANS), &stage->program);
    StageTable out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(job, stage, &rows, &out, error);
    for (unsigned int group = 0u; ok && (group < GROUP_MEANS); group += 1u)
    {
        means[group] = stage_pair_of(stage_row(&out, group));
    }
    stage_table_released(&out);
    stage_table_released(&rows);
    stage_cell_released(widths);
    for (unsigned int group = 0u; group < GROUP_MEANS; group += 1u)
    {
        stage_cell_released(copied[group]);
    }
    return ok;
}

int program_main(int count, char **arguments)
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &settings, .bytes = sizeof(settings), .val = 0);
    GroupInput input;
    if (!group_read(arguments[1], &settings, &input))
    {
        fprintf(stderr, "  subgrouping: %s is not an input this program reads\n", arguments[1]);
        return 2;
    }

    // the languages retained: enough meanings, not a reconstruction, ordered by branch and then name
    unsigned long long *const retained = (unsigned long long *)stage_cell(input.languages, sizeof(unsigned long long));
    unsigned long long *const retained_room =
        (unsigned long long *)stage_cell(input.languages, sizeof(unsigned long long));
    GroupLanguage *const languages = (GroupLanguage *)stage_cell(input.languages, sizeof(GroupLanguage));
    if ((retained == NULL) || (retained_room == NULL) || (languages == NULL))
    {
        return 2;
    }
    unsigned long long language_count = 0ull;
    for (unsigned long long at = 0ull; at < input.languages; at += 1ull)
    {
        if ((input.language[at].count >= settings.least_meanings) && !stage_same(input.language[at].branch, "PROTO"))
        {
            retained[language_count] = at;
            language_count += 1ull;
        }
    }
    s_group_languages = input.language;
    EMBED_CALL(memor.sort, MemoriaCfg, .dst = retained, .bytes = sizeof(unsigned long long), .scratch = retained_room,
               .count = language_count, .order = group_language_order);
    for (unsigned long long at = 0ull; at < language_count; at += 1ull)
    {
        languages[at] = input.language[retained[at]];
    }

    // The pairs with enough shared meanings, and each pair's match bits over its shared meanings. The table of pairs
    // names each pair of languages, both ways, GROUP_ABSENT where the two share too few
    const unsigned long long pair_room = stage_order_pairs(language_count);
    GroupPair *const pairs = (GroupPair *)stage_cell(pair_room, sizeof(GroupPair));
    unsigned long long *const table =
        (unsigned long long *)stage_cell(language_count * language_count, sizeof(unsigned long long));
    unsigned long long meanings_max = 1ull;
    for (unsigned long long at = 0ull; at < language_count; at += 1ull)
    {
        meanings_max = (languages[at].count > meanings_max) ? languages[at].count : meanings_max;
    }
    const GroupMeaning **const shared = (const GroupMeaning **)stage_cell(2ull * meanings_max, sizeof(GroupMeaning *));
    if ((pairs == NULL) || (table == NULL) || (shared == NULL))
    {
        return 2;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = table,
               .bytes = (size_t)(language_count * language_count * sizeof(unsigned long long)), .val = 0xFF);
    unsigned long long pair_count = 0ull;
    unsigned long long widest = 1ull;
    for (unsigned long long left = 0ull; left < language_count; left += 1ull)
    {
        for (unsigned long long right = left + 1ull; right < language_count; right += 1ull)
        {
            unsigned long long shared_count = 0ull;
            for (unsigned long long one = 0ull; one < languages[left].count; one += 1ull)
            {
                const GroupMeaning *const meaning = &input.meaning[languages[left].first + one];
                const GroupMeaning *const other = group_meaning_of(&input, &languages[right], meaning->meaning);
                if (other != NULL)
                {
                    shared[2ull * shared_count] = meaning;
                    shared[2ull * shared_count + 1ull] = other;
                    shared_count += 1ull;
                }
            }
            if (shared_count < settings.least_shared)
            {
                continue;
            }
            GroupPair *const pair = &pairs[pair_count];
            pair->left = left;
            pair->right = right;
            pair->shared = shared_count;
            pair->matches = (unsigned char *)stage_cell(shared_count * shared_count, 1u);
            if (pair->matches == NULL)
            {
                return 2;
            }
            for (unsigned long long one = 0ull; one < shared_count; one += 1ull)
            {
                for (unsigned long long two = 0ull; two < shared_count; two += 1ull)
                {
                    pair->matches[one * shared_count + two] =
                        (unsigned char)group_match(&input, shared[2ull * one], shared[2ull * two + 1ull]);
                }
            }
            table[left * language_count + right] = pair_count;
            table[right * language_count + left] = pair_count;
            widest = (shared_count > widest) ? shared_count : widest;
            pair_count += 1ull;
        }
    }
    if (pair_count == 0ull)
    {
        fprintf(stderr, "  subgrouping: no pair of languages shares %llu meanings\n", settings.least_shared);
        return 2;
    }

    // the count program: each pair's runs, the identity first and then each shuffle, of `words` 32-bit words
    const unsigned long long shuffles = settings.permutations;
    const unsigned long long runs = shuffles + 1ull;
    const unsigned int words = (unsigned int)((widest + 31u) / 32u);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    Stage count_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &count_stage, .bytes = sizeof(count_stage), .val = 0);
    count_stage.name = "count";
    stage_count_program(0, &count_stage.program);
    const unsigned long long count_lanes = pair_count * runs * words;
    unsigned int *records = (unsigned int *)stage_cell(count_lanes, sizeof(unsigned int));
    unsigned long long *const order = (unsigned long long *)stage_cell(widest, sizeof(unsigned long long));
    int ok = (records != NULL) && (order != NULL);
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        const unsigned long long n = pairs[at].shared;
        const unsigned long long key = sim_mix(settings.seed ^ sim_mix(at));
        for (unsigned long long run = 0ull; run < runs; run += 1ull)
        {
            for (unsigned long long meaning = 0ull; meaning < n; meaning += 1ull)
            {
                order[meaning] = meaning;
            }
            if (run > 0ull)
            {
                for (unsigned long long place = n - 1ull; place > 0ull; place -= 1ull)
                {
                    const unsigned long long other = sim_draw_below(key, (run * n) + place, place + 1ull);
                    const unsigned long long held = order[place];
                    order[place] = order[other];
                    order[other] = held;
                }
            }
            unsigned int *word = &records[(at * runs + run) * words];
            for (unsigned long long meaning = 0ull; meaning < n; meaning += 1ull)
            {
                if (pairs[at].matches[meaning * n + order[meaning]] != 0u)
                {
                    word[meaning / 32u] |= 1u << (meaning % 32u);
                }
            }
        }
    }

    ok = ok && stage_position(&job, &count_stage, &error);
    const unsigned long long declared =
        count_lanes * (1ull + (ok ? count_stage.layout.out_limbs : 1u)) * sizeof(unsigned int);
    ok = ok && sim_job_submit(&job, "subgrouping", count, arguments, declared);
    StageTable counted = {NULL, 0ull, 0u};
    const unsigned int first_output[1] = {0u};
    ok = ok &&
         stage_summed(&job, &count_stage, records, count_lanes, count_lanes, words, first_output, 1u, &counted, &error);
    stage_cell_released(records);
    records = NULL;

    // the above program: each shuffle's count against the pair's own, summed over each pair's shuffles
    Stage above_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &above_stage, .bytes = sizeof(above_stage), .val = 0);
    above_stage.name = "above";
    const unsigned int count_bits = stage_bits(stage_whole(widest));
    stage_above_program(count_bits, &above_stage.program);
    ok = ok && stage_position(&job, &above_stage, &error);
    const unsigned int above_limbs = (above_stage.program.record_bits + 31u) / 32u;
    const unsigned long long above_lanes = pair_count * shuffles;
    unsigned int *const above_records = (unsigned int *)stage_cell(above_lanes * above_limbs, sizeof(unsigned int));
    ok = ok && (above_records != NULL);
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        pairs[at].matched = stage_row(&counted, 0ull)[at * runs];
        for (unsigned long long shuffle = 0ull; shuffle < shuffles; shuffle += 1ull)
        {
            unsigned int *record = &above_records[(at * shuffles + shuffle) * above_limbs];
            stage_put(record, above_stage.program.field_offset[0], above_stage.program.field_bits[0],
                      stage_row(&counted, 0ull)[at * runs + 1ull + shuffle]);
            stage_put(record, above_stage.program.field_offset[1], above_stage.program.field_bits[1],
                      pairs[at].matched);
        }
    }
    StageTable aboves = {NULL, 0ull, 0u};
    const unsigned int both_outputs[2] = {0u, 1u};
    ok = ok && stage_summed(&job, &above_stage, above_records, above_lanes * above_limbs, above_lanes, shuffles,
                            both_outputs, 2u, &aboves, &error);
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        pairs[at].above = stage_row(&aboves, 0ull)[at];
        pairs[at].shuffled_sum = stage_row(&aboves, 1ull)[at];
    }

    // the value program: each pair's excess + 1 and p
    Stage value_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &value_stage, .bytes = sizeof(value_stage), .val = 0);
    value_stage.name = "value";
    StageTable value_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&value_rows, pair_count, 5u);
    unsigned int value_bits = 1u;
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        StageWhole *const row = stage_row(&value_rows, at);
        row[0] = pairs[at].matched;
        row[1] = pairs[at].shuffled_sum;
        row[2] = stage_whole(pairs[at].shared);
        row[3] = stage_whole(shuffles);
        row[4] = pairs[at].above;
        for (unsigned int field = 0u; field < 5u; field += 1u)
        {
            const unsigned int bits = stage_bits(row[field]);
            value_bits = (bits > value_bits) ? bits : value_bits;
        }
    }
    group_value_program(value_bits + 1u, &value_stage.program);
    StageTable values = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &value_stage, &value_rows, &values, &error);
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        pairs[at].lifted = {stage_row(&values, at)[0], stage_row(&values, at)[1]};
        pairs[at].chance = {stage_row(&values, at)[2], stage_row(&values, at)[3]};
    }

    // the first compare: each language's partners ordered by excess, and each outside pair's p against p_below
    static const char *const s_salish[6] = {"CS", "NIS", "SIS", "NUX", "TS", "TI"};
    StageOrder *const partner_orders = (StageOrder *)stage_cell(language_count, sizeof(StageOrder));
    StageRational *const partner_values =
        (StageRational *)stage_cell(language_count * language_count, sizeof(StageRational));
    const char **const partner_names = (const char **)stage_cell(language_count * language_count, sizeof(char *));
    unsigned long long *const partner_of =
        (unsigned long long *)stage_cell(language_count * language_count, sizeof(unsigned long long));
    ok = ok && (partner_orders != NULL) && (partner_values != NULL) && (partner_names != NULL) && (partner_of != NULL);
    unsigned long long lane_count = 0ull;
    unsigned long long outside_count = 0ull;
    for (unsigned long long left = 0ull; ok && (left < language_count); left += 1ull)
    {
        StageOrder *const partner_order = &partner_orders[left];
        partner_order->values = &partner_values[left * language_count];
        partner_order->names = &partner_names[left * language_count];
        for (unsigned long long right = 0ull; right < language_count; right += 1ull)
        {
            const unsigned long long found = table[left * language_count + right];
            if ((found == GROUP_ABSENT) || !group_branch_in(languages[right].branch, s_salish, 6u))
            {
                continue;
            }
            partner_values[left * language_count + partner_order->count] = pairs[found].lifted;
            partner_names[left * language_count + partner_order->count] = languages[right].name;
            partner_of[left * language_count + partner_order->count] = right;
            partner_order->count += 1ull;
        }
        lane_count += stage_order_pairs(partner_order->count);
    }
    for (unsigned long long at = 0ull; at < pair_count; at += 1ull)
    {
        const char *const one = languages[pairs[at].left].branch;
        const char *const two = languages[pairs[at].right].branch;
        outside_count += ((stage_same(one, "OUT") && group_branch_in(two, s_salish, 6u)) ||
                          (stage_same(two, "OUT") && group_branch_in(one, s_salish, 6u)))
                             ? 1ull
                             : 0ull;
    }
    StageRational *const xs = (StageRational *)stage_cell(lane_count + outside_count, sizeof(StageRational));
    StageRational *const ys = (StageRational *)stage_cell(lane_count + outside_count, sizeof(StageRational));
    int *const unmargined = (int *)stage_cell(lane_count + outside_count, sizeof(int));
    int *const first_signs = (int *)stage_cell(lane_count + outside_count, sizeof(int));
    unsigned long long *const outside = (unsigned long long *)stage_cell(outside_count, sizeof(unsigned long long));
    ok = ok && (xs != NULL) && (ys != NULL) && (unmargined != NULL) && (first_signs != NULL) && (outside != NULL);
    unsigned long long lanes = 0ull;
    for (unsigned long long left = 0ull; ok && (left < language_count); left += 1ull)
    {
        stage_order_lanes(&partner_orders[left], xs, ys, &lanes);
    }
    const unsigned long long chance_at = lanes;
    const StageRational p_below = {stage_whole(settings.p_below[0]), stage_whole(settings.p_below[1])};
    unsigned long long outside_at = 0ull;
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        const char *const one = languages[pairs[at].left].branch;
        const char *const two = languages[pairs[at].right].branch;
        if ((stage_same(one, "OUT") && group_branch_in(two, s_salish, 6u)) ||
            (stage_same(two, "OUT") && group_branch_in(one, s_salish, 6u)))
        {
            xs[lanes] = p_below;
            ys[lanes] = pairs[at].chance;
            lanes += 1ull;
            outside[outside_at] = at;
            outside_at += 1ull;
        }
    }
    const unsigned long long unit_margin[2] = {1ull, 1ull};
    Stage first_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &first_stage, .bytes = sizeof(first_stage), .val = 0);
    ok = ok && stage_compare(&job, &first_stage, "first compare", xs, ys, unmargined, lanes, unit_margin, first_signs,
                             &error);
    for (unsigned long long left = 0ull; ok && (left < language_count); left += 1ull)
    {
        ok = stage_order_read(&partner_orders[left], first_signs);
    }

    // the group means: Interior, Central and between, and Nuxalk with each branch
    static const char *const s_interior[2] = {"NIS", "SIS"};
    static const char *const s_central[1] = {"CS"};
    static const char *const s_tsamosan[1] = {"TS"};
    static const char *const s_nuxalk[1] = {"NUX"};
    auto in = [&](unsigned long long language, const char *const *branches, unsigned int branch_count) {
        return group_branch_in(languages[language].branch, branches, branch_count);
    };
    GroupGroups groups;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &groups, .bytes = sizeof(groups), .val = 0);
    for (unsigned int group = 0u; group < GROUP_MEANS; group += 1u)
    {
        groups.value[group] = (StageRational *)stage_cell(pair_count, sizeof(StageRational));
        ok = ok && (groups.value[group] != NULL);
    }
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        const unsigned long long one = pairs[at].left;
        const unsigned long long two = pairs[at].right;
        const StageRational &value = pairs[at].lifted;
        const int put[GROUP_MEANS] = {
            in(one, s_interior, 2u) && in(two, s_interior, 2u),
            in(one, s_central, 1u) && in(two, s_central, 1u),
            (in(one, s_interior, 2u) && in(two, s_central, 1u)) || (in(one, s_central, 1u) && in(two, s_interior, 2u)),
            (in(one, s_nuxalk, 1u) && in(two, s_interior, 2u)) || (in(two, s_nuxalk, 1u) && in(one, s_interior, 2u)),
            (in(one, s_nuxalk, 1u) && in(two, s_central, 1u)) || (in(two, s_nuxalk, 1u) && in(one, s_central, 1u)),
            (in(one, s_nuxalk, 1u) && in(two, s_tsamosan, 1u)) || (in(two, s_nuxalk, 1u) && in(one, s_tsamosan, 1u))};
        for (unsigned int group = 0u; group < GROUP_MEANS; group += 1u)
        {
            if (put[group])
            {
                groups.value[group][groups.count[group]] = value;
                groups.count[group] += 1ull;
            }
        }
    }
    StageRational means[GROUP_MEANS];
    Stage *const mean_stages = (Stage *)stage_cell(GROUP_MEAN_STAGES, sizeof(Stage));
    unsigned int mean_stage_count = 0u;
    ok = ok && (mean_stages != NULL) && group_means(&job, &groups, means, mean_stages, &mean_stage_count, &error);
    StageRational mean_xs[2];
    StageRational mean_ys[2];
    unsigned long long mean_count = 0ull;
    if (ok && (groups.count[0] != 0ull) && (groups.count[1] != 0ull) && (groups.count[2] != 0ull))
    {
        mean_xs[0] = means[0];
        mean_xs[1] = means[1];
        mean_ys[0] = means[2];
        mean_ys[1] = means[2];
        mean_count = 2ull;
    }
    const int mean_margined[2] = {0, 0};
    int second_signs[2] = {0, 0};
    Stage second_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &second_stage, .bytes = sizeof(second_stage), .val = 0);
    ok = ok && stage_compare(&job, &second_stage, "second compare", mean_xs, mean_ys, mean_margined, mean_count,
                             unit_margin, second_signs, &error);

    // every excess and mean printed with its sign, from excess + 1
    const unsigned long long printed_count = pair_count + GROUP_MEANS;
    StageRational *const printed = (StageRational *)stage_cell(printed_count, sizeof(StageRational));
    StageText *const displayed = (StageText *)stage_cell(printed_count, sizeof(StageText));
    ok = ok && (printed != NULL) && (displayed != NULL);
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        printed[at] = pairs[at].lifted;
    }
    const unsigned long long means_at = pair_count;
    for (unsigned int group = 0u; ok && (group < GROUP_MEANS); group += 1u)
    {
        printed[means_at + group] = means[group];
    }
    Stage signed_stage;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &signed_stage, .bytes = sizeof(signed_stage), .val = 0);
    signed_stage.name = "signed";
    group_signed_program((ok ? stage_widest(printed, printed_count) : 1u) + 1u, &signed_stage.program);
    StageTable signed_rows = {NULL, 0ull, 0u};
    ok = ok && stage_table_taken(&signed_rows, printed_count, 2u);
    for (unsigned long long at = 0ull; ok && (at < printed_count); at += 1ull)
    {
        stage_row(&signed_rows, at)[0] = printed[at].num;
        stage_row(&signed_rows, at)[1] = printed[at].den;
    }
    StageTable signed_out = {NULL, 0ull, 0u};
    ok = ok && stage_rows(&job, &signed_stage, &signed_rows, &signed_out, &error);
    for (unsigned long long at = 0ull; ok && (at < printed_count); at += 1ull)
    {
        const DeviceRecordStep *sign_step = &signed_stage.layout.step_table[signed_stage.program.outputs[0]];
        const StageWhole *const row = stage_row(&signed_out, at);
        const int sign = stage_sign(row[0], sign_step->out_bits);
        VerbaLinea line = mmgr_line_over(displayed[at].bytes, sizeof(displayed[at].bytes));
        mmgr_line_put(&line, (sign < 0) ? "-" : "+");
        mmgr_line_put(&line, stage_decimal_of(row[1], row[2]).bytes);
        (void)mmgr_line_finish(&line);
    }
    for (unsigned long long at = 0ull; ok && (at < pair_count); at += 1ull)
    {
        pairs[at].excess_text = displayed[at];
    }

    // the verdicts, each a text of a fixed bound
    StageText *const verdicts = (StageText *)stage_cell(2ull * language_count, sizeof(StageText));
    unsigned long long verdict_count = 0ull;
    ok = ok && (verdicts != NULL);
    for (int pass = 0; ok && (pass < 2); pass += 1)
    {
        const char *const *group = (pass == 0) ? s_interior : s_central;
        const unsigned int group_count = (pass == 0) ? 2u : 1u;
        for (unsigned long long left = 0ull; left < language_count; left += 1ull)
        {
            if (!in(left, group, group_count) || (partner_orders[left].count == 0ull))
            {
                continue;
            }
            const unsigned long long best =
                partner_of[left * language_count + partner_orders[left].sorted[partner_orders[left].count - 1ull]];
            const int in_group = in(best, group, group_count);
            snprintf(verdicts[verdict_count].bytes, sizeof(verdicts[verdict_count].bytes),
                     "(%c) %s best Salish partner %s %s", (pass == 0) ? 'a' : 'b',
                     stage_displayed(languages[left].name, 18u, 1).bytes,
                     stage_displayed(languages[best].name, 18u, 1).bytes, in_group ? "pass" : "FAIL");
            verdict_count += 1ull;
        }
    }
    const int means_agree = ok && (mean_count == 2ull) && (second_signs[0] == 1) && (second_signs[1] == 1);
    StageText *const failures = (StageText *)stage_cell(outside_count, sizeof(StageText));
    unsigned long long failure_count = 0ull;
    int failures_fit = 1;
    ok = ok && (failures != NULL);
    for (unsigned long long at = 0ull; ok && (at < outside_count); at += 1ull)
    {
        if (first_signs[chance_at + at] == 1)
        {
            const GroupPair &pair = pairs[outside[at]];
            VerbaLinea line = mmgr_line_over(failures[failure_count].bytes, sizeof(failures[failure_count].bytes));
            mmgr_line_put(&line, languages[pair.left].name);
            mmgr_line_put(&line, "-");
            mmgr_line_put(&line, languages[pair.right].name);
            mmgr_line_put(&line, " ");
            mmgr_line_put(&line, pair.excess_text.bytes);
            mmgr_line_put(&line, " p ");
            mmgr_line_put(&line, stage_text_of(pair.chance.num).bytes);
            mmgr_line_put(&line, "/");
            mmgr_line_put(&line, stage_text_of(pair.chance.den).bytes);
            mmgr_line_put(&line, " [");
            mmgr_line_decimal(&line, pair.shared, 1u);
            mmgr_line_put(&line, "]");
            failures_fit = failures_fit && (line.at < sizeof(failures[failure_count].bytes));
            (void)mmgr_line_finish(&line);
            failure_count += 1ull;
        }
    }
    if (ok && !failures_fit)
    {
        fprintf(stderr, "  subgrouping: a failure's text passes %u bytes\n", STAGE_TEXT_BYTES);
        ok = 0;
    }

    FILE *out = ok ? fopen(arguments[2], "w") : NULL;
    if (out != NULL)
    {
        fprintf(out, "permutations %llu seed %llu least_shared %llu least_meanings %llu p_below %llu/%llu\n",
                settings.permutations, settings.seed, settings.least_shared, settings.least_meanings,
                settings.p_below[0], settings.p_below[1]);
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            fprintf(out, "language %s %s meanings %llu\n", languages[at].branch, languages[at].name,
                    languages[at].count);
        }
        for (unsigned long long at = 0ull; at < pair_count; at += 1ull)
        {
            const GroupPair &pair = pairs[at];
            fprintf(
                out, "pair %s | %s shared %llu matched %s shuffled_sum %s above %s excess+1 %s/%s p %s/%s excess %s\n",
                languages[pair.left].name, languages[pair.right].name, pair.shared, stage_text_of(pair.matched).bytes,
                stage_text_of(pair.shuffled_sum).bytes, stage_text_of(pair.above).bytes,
                stage_text_of(pair.lifted.num).bytes, stage_text_of(pair.lifted.den).bytes,
                stage_text_of(pair.chance.num).bytes, stage_text_of(pair.chance.den).bytes, pair.excess_text.bytes);
        }
        for (unsigned long long at = 0ull; at < verdict_count; at += 1ull)
        {
            fprintf(out, "%s\n", verdicts[at].bytes);
        }
        fprintf(out, "(c) %s\n", means_agree ? "pass" : "FAIL");
        fprintf(out, "(d) %s\n", (failure_count == 0ull) ? "pass" : "FAIL");
        const char *types[GROUP_MEANS] = {"interior",        "central",        "interior-central",
                                          "nuxalk-interior", "nuxalk-central", "nuxalk-tsamosan"};
        for (unsigned int group = 0u; group < GROUP_MEANS; group += 1u)
        {
            if (groups.count[group] != 0ull)
            {
                fprintf(out, "mean %s pairs %llu excess+1 %s/%s excess %s\n", types[group], groups.count[group],
                        stage_text_of(means[group].num).bytes, stage_text_of(means[group].den).bytes,
                        displayed[means_at + group].bytes);
            }
        }
        fclose(out);
    }
    sim_check(&job, out != NULL, "the records are written out");

    if (ok)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "languages with ");
            mmgr_line_decimal(output_line, settings.least_meanings, 1u);
            mmgr_line_put(output_line, " or more meanings:");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < language_count; at += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "   ");
            mmgr_line_put_columns(output_line, languages[at].branch, 5u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, stage_displayed(languages[at].name, 18u, 1).bytes);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, languages[at].count, 5u);
            mmgr_line_put(output_line, " meanings");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\nexcess match rate (observed minus shuffled), shared meanings in brackets");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long left = 0ull; left < language_count; left += 1ull)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put_columns(output_line, languages[left].branch, 5u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put(output_line, stage_displayed(languages[left].name, 18u, 1).bytes);
                platform_line_out_ended();
            }
            // the partners ranked from the last of the order, the five with the highest excess
            const StageOrder *const partner_order = &partner_orders[left];
            for (unsigned long long at = 0ull; (at < partner_order->count) && (at < 5ull); at += 1ull)
            {
                const unsigned long long right =
                    partner_of[left * language_count + partner_order->sorted[partner_order->count - 1ull - at]];
                const GroupPair &pair = pairs[table[left * language_count + right]];
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, stage_displayed(languages[right].name, 10u, 0).bytes);
                mmgr_line_put(output_line, " ");
                mmgr_line_put_max(output_line, pair.excess_text.bytes, 6u);
                mmgr_line_put(output_line, "[");
                mmgr_line_decimal(output_line, pair.shared, 1u);
                mmgr_line_put(output_line, "]");
                platform_line_out_ended();
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        for (unsigned long long at = 0ull; at < verdict_count; at += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, verdicts[at].bytes);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "(c) mean excess: Interior pairs ");
            mmgr_line_put(output_line, displayed[means_at].bytes);
            mmgr_line_put(output_line, " (");
            mmgr_line_decimal(output_line, groups.count[0], 1u);
            mmgr_line_put(output_line, "), Central pairs ");
            mmgr_line_put(output_line, displayed[means_at + 1u].bytes);
            mmgr_line_put(output_line, " (");
            mmgr_line_decimal(output_line, groups.count[1], 1u);
            mmgr_line_put(output_line, "), Interior-Central ");
            mmgr_line_put(output_line, displayed[means_at + 2u].bytes);
            mmgr_line_put(output_line, " (");
            mmgr_line_decimal(output_line, groups.count[2], 1u);
            mmgr_line_put(output_line, "): ");
            mmgr_line_put(output_line, means_agree ? "pass" : "FAIL");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "(d) outside-Salish pairs with shuffle p below ");
            mmgr_line_decimal(output_line, settings.p_below[0], 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, settings.p_below[1], 1u);
            mmgr_line_put(output_line, ": ");
            platform_line_out_ended();
        }
        if (failure_count == 0ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "none, pass");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long at = 0ull; at < failure_count; at += 1ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (at == 0ull) ? "" : "; ");
            mmgr_line_put(output_line, failures[at].bytes);
            platform_line_out_ended();
        }
        if (failure_count != 0ull)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        const char *names[3] = {"Interior", "Central", "Tsamosan"};
        for (unsigned int branch = 0u; branch < 3u; branch += 1u)
        {
            if (groups.count[3u + branch] != 0ull)
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "Nuxalk mean excess with ");
                mmgr_line_put_columns(output_line, names[branch], 9u);
                mmgr_line_put(output_line, " ");
                mmgr_line_put(output_line, displayed[means_at + 3u + branch].bytes);
                mmgr_line_put(output_line, " over ");
                mmgr_line_decimal(output_line, groups.count[3u + branch], 1u);
                mmgr_line_put(output_line, " languages");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(
                output_line,
                "each excess is the sign, whole part and first six digits of the exact rational in the records");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    stage_release(&count_stage);
    stage_release(&above_stage);
    stage_release(&value_stage);
    stage_release(&first_stage);
    for (unsigned int at = 0u; at < mean_stage_count; at += 1u)
    {
        stage_release(&mean_stages[at]);
    }
    stage_release(&second_stage);
    stage_release(&signed_stage);
    stage_table_released(&signed_out);
    stage_table_released(&signed_rows);
    stage_table_released(&values);
    stage_table_released(&value_rows);
    stage_table_released(&aboves);
    stage_table_released(&counted);
    for (unsigned long long left = 0ull; partner_orders != NULL && (left < language_count); left += 1ull)
    {
        stage_order_released(&partner_orders[left]);
    }
    for (unsigned int group = 0u; group < GROUP_MEANS; group += 1u)
    {
        stage_cell_released(groups.value[group]);
    }
    for (unsigned long long at = 0ull; at < pair_count; at += 1ull)
    {
        stage_cell_released(pairs[at].matches);
    }
    void *const cells[] = {failures,
                           verdicts,
                           displayed,
                           printed,
                           mean_stages,
                           outside,
                           first_signs,
                           unmargined,
                           ys,
                           xs,
                           partner_of,
                           (void *)partner_names,
                           partner_values,
                           partner_orders,
                           above_records,
                           order,
                           (void *)shared,
                           table,
                           pairs,
                           languages,
                           retained_room,
                           retained};
    for (unsigned int each = 0u; each < (unsigned int)(sizeof(cells) / sizeof(cells[0])); each += 1u)
    {
        stage_cell_released(cells[each]);
    }
    group_released(&input);
    return sim_close(&job, "subgrouping");
}
