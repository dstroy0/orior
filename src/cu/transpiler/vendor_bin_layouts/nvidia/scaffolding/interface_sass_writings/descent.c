// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// descent.c: the descent: the gate's descent read, the fill and the flat form, and the ruleset read
#include "writings_internal.h"
#include "../../../../../includes/parsers/line_read.h"

// The arrangements a descent ran over, written by `chains` into each `into` with its verdict, read back against the
// cases the descent placed for that relation, each `cases` file a case a line as the two words and the word the
// relation gives them. An arrangement stands on the part where it gives every placed case its word, and the part's
// verdict is held to the descent's, written to `record`: 0 where every verdict agrees, 1 where one does not, 2 where
// nothing could be read
int writings_descent_read(const char *record, unsigned int pairs, char **folders)
{
    static char s_chain[WRITINGS_CHAINS_MOST][512];
    static char s_verdict[WRITINGS_CHAINS_MOST][16];
    static unsigned int s_chain_answered[WRITINGS_CHAINS_MOST][WRITINGS_CASES];
    static unsigned char s_chain_ran[WRITINGS_CHAINS_MOST];
    FILE *const out = fopen(record, "wb");
    if (out == NULL)
    {
        fprintf(stderr, "the record %s was not written\n", record);
        return 2;
    }
    fprintf(out, "# The descent's cases put to the part\n\n");
    fprintf(out, "Written by `interface_sass_writings.sh` whole on every run. For each relation `gate_descent` writes "
                 "every arrangement it descends over, its verdict on each and the cases it placed. Each arrangement "
                 "is written node by node from the writings `interface_sass_writings.md` found and run on the part "
                 "over the placed cases alone, and stands where it gives every one its word. The part's verdict is "
                 "held to the descent's.\n\n");
    fprintf(out, "| relation | cases placed | arrangements | run | standing on the part | standing on the host | "
                 "verdicts that differ |\n|---|---|---|---|---|---|---|\n");
    unsigned int differ_total = 0u;
    for (unsigned int pair = 0u; pair < pairs; pair += 1u)
    {
        const char *const into = folders[2u * pair];
        const char *const cases_path = folders[(2u * pair) + 1u];
        char path[1024];
        char line[WRITINGS_LINE];
        unsigned int placed[WRITINGS_CASES][3];
        unsigned int placed_count = 0u;
        // the descent's cases as the run holds them, the descent's run having written them through its writer
        const unsigned char *cases = NULL;
        unsigned long long cases_size = 0ull;
        unsigned long long cases_at = 0ull;
        const int cases_read = qry_member_file_blob(cases_path, &cases, &cases_size);
        snprintf(path, sizeof(path), "%s/chains.txt", into);
        FILE *const chains = fopen(path, "rb");
        snprintf(path, sizeof(path), "%s/answers.txt", into);
        FILE *const answers = fopen(path, "rb");
        if (!cases_read || (chains == NULL) || (answers == NULL))
        {
            fprintf(stderr, "the cases %s or the chains in %s did not read\n", cases_path, into);
            fclose(out);
            return 2;
        }
        while ((placed_count < WRITINGS_CASES) && line_read(cases, cases_size, &cases_at, line, sizeof(line)))
        {
            const char *read = line;
            unsigned long long word[3] = {0ull, 0ull, 0ull};
            if (line_number(&read, 1, &word[0]) && line_number(&read, 1, &word[1]) && line_number(&read, 1, &word[2]) &&
                (word[0] <= 0xffffffffull) && (word[1] <= 0xffffffffull) && (word[2] <= 0xffffffffull))
            {
                // the test above holds each word inside 32 bits
                placed[placed_count][0] = (unsigned int)word[0];
                placed[placed_count][1] = (unsigned int)word[1];
                placed[placed_count][2] = (unsigned int)word[2];
                placed_count += 1u;
            }
        }
        char relation[32] = "";
        unsigned int count = 0u;
        while ((count < WRITINGS_CHAINS_MOST) && (fgets(line, sizeof(line), chains) != NULL))
        {
            unsigned int number = 0u;
            const char *read = line;
            int whole = line_unsigned(&read, &number) && (*read == '\t');
            read += whole ? 1 : 0;
            whole = whole && line_until(&read, '\t', relation, sizeof(relation)) && (*read == '\t');
            read += whole ? 1 : 0;
            whole = whole && line_until(&read, '\t', s_chain[count], sizeof(s_chain[count])) && (*read == '\t');
            read += whole ? 1 : 0;
            if (whole && line_word(&read, s_verdict[count], sizeof(s_verdict[count])))
            {
                count += 1u;
            }
        }
        fclose(chains);
        const unsigned int saved = s_cases;
        s_cases = placed_count;
        unsigned int ran = 0u;
        unsigned int refused = 0u;
        memset(s_chain_ran, 0, sizeof(s_chain_ran));
        writings_answers_read(answers, count, s_chain_answered, s_chain_ran, &ran, &refused);
        s_cases = saved;
        fclose(answers);
        unsigned int standing_part = 0u;
        unsigned int standing_host = 0u;
        unsigned int differ = 0u;
        for (unsigned int number = 0u; number < count; number += 1u)
        {
            unsigned int agreed = 0u;
            for (unsigned int at = 0u; at < placed_count; at += 1u)
            {
                agreed += (s_chain_answered[number][at] == placed[at][2]) ? 1u : 0u;
            }
            const int part = s_chain_ran[number] && (agreed == placed_count);
            const int host = (strcmp(s_verdict[number], "standing") == 0);
            standing_part += part ? 1u : 0u;
            standing_host += host ? 1u : 0u;
            if (part != host)
            {
                differ += 1u;
                fprintf(out, "%s `%s`: %s on the host, %s on the part\n\n", relation, s_chain[number],
                        host ? "standing" : "out", s_chain_ran[number] ? (part ? "standing" : "out") : "not run");
            }
        }
        fprintf(out, "| %s | %u | %u | %u | %u | %u | %u |\n", relation, placed_count, count, ran, standing_part,
                standing_host, differ);
        printf("  %-8s %u cases placed, %u arrangements, %u run, %u standing on the part and %u on the host, %u "
               "verdicts differ\n",
               relation, placed_count, count, ran, standing_part, standing_host, differ);
        differ_total += differ;
    }
    fclose(out);
    printf("interface sass descent: %u verdicts differ, the record written to %s\n", differ_total, record);
    return (differ_total == 0u) ? 0 : 1;
}

// `text` with every `{name}` replaced by `put`, into `out`: 1, or 0 where `out` will not hold it
static int writings_fill(const char *text, const char *name, const char *put, char *out, size_t room)
{
    char brace[300];
    snprintf(brace, sizeof(brace), "{%s}", name);
    size_t at = 0u;
    const char *walk = text;
    while ((*walk != '\0') && (at < room))
    {
        if (strncmp(walk, brace, strlen(brace)) == 0)
        {
            at += (size_t)snprintf(&out[at], room - at, "%s", put);
            walk += strlen(brace);
            continue;
        }
        out[at] = *walk;
        at += 1u;
        walk += 1;
    }
    if (at >= room)
    {
        return 0;
    }
    out[at] = '\0';
    return 1;
}

// `text`, a form's text as a ruleset writes it, written to `out` as one instruction the way the search prints one: each
// written tab and space run one space, the closing `;` and written newline dropped, and nothing at either end
static void writings_flat(const char *text, char *out, size_t room)
{
    size_t at = 0u;
    int space = 1;
    for (const char *walk = text; (*walk != '\0') && ((at + 1u) < room); walk += 1)
    {
        if ((walk[0] == '\\') && ((walk[1] == 't') || (walk[1] == 'n')))
        {
            walk += 1;
            space = space || (at != 0u);
            continue;
        }
        if ((*walk == ';') || (*walk == ' ') || (*walk == '\t') || (*walk == '\r') || (*walk == '\n'))
        {
            space = space || ((*walk == ' ') && (at != 0u));
            continue;
        }
        if (space && (at != 0u))
        {
            out[at] = ' ';
            at += 1u;
        }
        space = 0;
        out[at] = *walk;
        at += 1u;
    }
    out[at] = '\0';
}

// The words of the word web that are one precept over their operands, each held to the writings the part gave: the
// ruleset's form for the word, its result put in WRITINGS_RESULT and its operands by place in WRITINGS_LEFT and
// WRITINGS_RIGHT, looked for among the forms the search ran. Where it ran, it either gives every case the precept's word
// or does not; where the search never ran that text, the record says so. Written to `record`: 0 where every form found
// holds, 1 where one does not, 2 where nothing could be read
int writings_krs_read(const char *folder, const char *ruleset, const char *record)
{
    // the ruleset as the run holds it, or the disk where the run holds none (qry_member_file)
    const unsigned char *rules = NULL;
    unsigned long long rules_size = 0ull;
    unsigned long long rules_at = 0ull;
    const int rules_read = writings_load(folder) && qry_member_file_blob(ruleset, &rules, &rules_size);
    FILE *const out = rules_read ? fopen(record, "wb") : NULL;
    if (out == NULL)
    {
        fprintf(stderr, "the search in %s, the ruleset %s or the record %s was not reached\n", folder, ruleset, record);
        return 2;
    }
    static char s_rule[512][WRITINGS_LINE];
    unsigned int rule_count = 0u;
    while ((rule_count < 512u) && line_read(rules, rules_size, &rules_at, s_rule[rule_count], sizeof(s_rule[0])))
    {
        rule_count += (strncmp(s_rule[rule_count], "form ", 5u) == 0) ? 1u : 0u;
    }
    fprintf(out, "# A ruleset's forms held to the part's writings\n\n");
    fprintf(out, "Written by `interface_sass_writings.sh` whole on every run. Each word of the word web that is one "
                 "precept over its operands is looked up in `%s`, its form written with its result and operands where "
                 "the search puts them, and looked for among the forms `interface_sass_writings.md` ran on the part.\n\n",
            ruleset);
    fprintf(out, "| word | precept | form as run | on the part |\n|---|---|---|---|\n");
    unsigned int held = 0u;
    unsigned int differ = 0u;
    for (unsigned int word = 0u; word < WORD_WEB_COUNT; word += 1u)
    {
        const Word *const one = &s_word_web[word];
        const unsigned int precept = one->node[0].precept;
        const unsigned int relation_none = s_relations;
        unsigned int relation = relation_none;
        for (unsigned int at = 0u; at < s_relations; at += 1u)
        {
            relation = (s_relation[at].precept && (s_relation[at].which == precept)) ? at : relation;
        }
        if ((one->nodes != 1u) || (relation == relation_none))
        {
            continue;
        }
        const size_t name_length = strlen(one->name);
        const char *rule = NULL;
        for (unsigned int at = 0u; (rule == NULL) && (at < rule_count); at += 1u)
        {
            rule = ((strncmp(s_rule[at] + 5, one->name, name_length) == 0) && (s_rule[at][5u + name_length] == ' '))
                       ? s_rule[at]
                       : NULL;
        }
        if (rule == NULL)
        {
            fprintf(out, "| `%s` | %s | no form | - |\n", one->name, s_precept_text[precept]);
            continue;
        }
        const char *const equals = strstr(rule, " = ");
        char params[8][32];
        unsigned int param_count = 0u;
        const char *walk = rule + 5u + name_length;
        while ((equals != NULL) && (walk < equals) && (param_count < 8u))
        {
            walk += strspn(walk, " ");
            const size_t length = strcspn(walk, " =");
            if ((length == 0u) || (walk >= equals))
            {
                break;
            }
            snprintf(params[param_count], sizeof(params[0]), "%.*s", (int)length, walk);
            param_count += 1u;
            walk += length;
        }
        char filled[WRITINGS_LINE];
        char next[WRITINGS_LINE];
        snprintf(filled, sizeof(filled), "%s", (equals != NULL) ? (equals + 3) : "");
        const char *const places[3] = {WRITINGS_RESULT, WRITINGS_LEFT, WRITINGS_RIGHT};
        for (unsigned int at = 0u; (at < param_count) && (at < 3u); at += 1u)
        {
            writings_fill(filled, params[at], places[at], next, sizeof(next));
            snprintf(filled, sizeof(filled), "%s", next);
        }
        char flat[512];
        writings_flat(filled, flat, sizeof(flat));
        unsigned int found = s_form_count;
        for (unsigned int number = 0u; (found == s_form_count) && (number < s_form_count); number += 1u)
        {
            found = (s_ran[number] && (strcmp(s_forms[number], flat) == 0)) ? number : found;
        }
        const char *verdict = "not among the forms run";
        if (found < s_form_count)
        {
            const int holds = s_holds[found][relation];
            verdict = holds ? "gives every case its word" : "does not give every case its word";
            held += holds ? 1u : 0u;
            differ += holds ? 0u : 1u;
        }
        fprintf(out, "| `%s` | %s | `%s` | %s |\n", one->name, s_precept_text[precept], flat, verdict);
        printf("  %-18s %-4s %-44s %s\n", one->name, s_precept_text[precept], flat, verdict);
    }
    fclose(out);
    printf("interface sass krs: %u forms hold, %u do not, the record written to %s\n", held, differ, record);
    return (differ == 0u) ? 0 : 1;
}
