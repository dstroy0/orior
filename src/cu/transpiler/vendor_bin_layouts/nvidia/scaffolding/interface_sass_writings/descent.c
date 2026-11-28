// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// descent.c: the descent: the gate's descent read, the fill and the flat form, and the ruleset read
#include "../../../../../../../includes/parsers/line_read.h"
#include "../../../../../../../includes/platform/platform.h"
#include "../../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "writings_internal.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int descent_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// The arrangements a descent ran over, written by `chains` into each `into` with its verdict, read back against the
// cases the descent placed for that relation, each `cases` file a case a line as the two words and the word the
// relation gives them. An arrangement is on the part where it gives every placed case its word, and the part's
// verdict is checked against the descent's, written to `record`: 0 where every verdict agrees, 1 where one does not, 2
// where nothing could be read
int writings_descent_read(const char *record, unsigned int pairs, char **folders)
{
    static char s_chain[WRITINGS_CHAINS_MAX][512];
    static char s_verdict[WRITINGS_CHAINS_MAX][16];
    static unsigned int s_chain_answered[WRITINGS_CHAINS_MAX][WRITINGS_CASES];
    static unsigned char s_chain_ran[WRITINGS_CHAINS_MAX];
    PlatformFile *const out = platform_file_open(record, "wb");
    if (out == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the record ");
            mmgr_line_put(&engine_note.line, record);
            mmgr_line_put(&engine_note.line, " was not written");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    {
        mmgr_line_put(platform_file_line(out), "# The descent's cases put to the part\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out),
                      "Written by `interface_sass_writings.sh` whole on every run. For each relation `gate_descent` "
                      "writes every arrangement it descends over, its verdict on each and the cases it placed. Each "
                      "arrangement is written node by node from the writings `interface_sass_writings.md` found and "
                      "run on the part over the placed cases alone, and is where it gives every one its word. The "
                      "part's verdict is checked against the descent's.\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out),
                      "| relation | cases placed | arrangements | run | located on the part | located on the host | "
                      "verdicts that differ |\n|---|---|---|---|---|---|---|\n");
    }
    unsigned int differ_total = 0u;
    for (unsigned int pair = 0u; pair < pairs; pair += 1u)
    {
        const char *const into = folders[2u * pair];
        const char *const cases_path = folders[(2u * pair) + 1u];
        char path[1024];
        char line[WRITINGS_LINE];
        unsigned int placed[WRITINGS_CASES][3];
        unsigned int placed_count = 0u;
        // the descent's cases as the run stores them, the descent's run having written them through its writer
        const unsigned char *cases = NULL;
        unsigned long long cases_size = 0ULL;
        unsigned long long cases_at = 0ULL;
        const int cases_read = qry_member_file_blob(cases_path, &cases, &cases_size);
        {
            VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
            mmgr_line_put(&buffer_line, into);
            mmgr_line_put(&buffer_line, "/chains.txt");
            (void)mmgr_line_finish(&buffer_line);
        }
        PlatformFile *const chains = platform_file_open(path, "rb");
        {
            VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
            mmgr_line_put(&buffer_line, into);
            mmgr_line_put(&buffer_line, "/answers.txt");
            (void)mmgr_line_finish(&buffer_line);
        }
        PlatformFile *const answers = platform_file_open(path, "rb");
        if (!cases_read || (chains == NULL) || (answers == NULL))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "the cases ");
                mmgr_line_put(&engine_note.line, cases_path);
                mmgr_line_put(&engine_note.line, " or the chains in ");
                mmgr_line_put(&engine_note.line, into);
                mmgr_line_put(&engine_note.line, " did not read");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            platform_file_close(out);
            return 2;
        }
        while ((placed_count < WRITINGS_CASES) && line_read(cases, cases_size, &cases_at, line, sizeof(line)))
        {
            const char *read = line;
            unsigned long long word[3] = {0ULL, 0ULL, 0ULL};
            if (line_number(&read, 1, &word[0]) && line_number(&read, 1, &word[1]) && line_number(&read, 1, &word[2]) &&
                (word[0] <= 0xffffffffULL) && (word[1] <= 0xffffffffULL) && (word[2] <= 0xffffffffULL))
            {
                // the test above bounds each word inside 32 bits
                placed[placed_count][0] = (unsigned int)word[0];
                placed[placed_count][1] = (unsigned int)word[1];
                placed[placed_count][2] = (unsigned int)word[2];
                placed_count += 1u;
            }
        }
        char relation[32] = "";
        unsigned int count = 0u;
        while ((count < WRITINGS_CHAINS_MAX) && (platform_file_text_read(line, sizeof(line), chains) != NULL))
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
        platform_file_close(chains);
        const unsigned int saved = s_cases;
        s_cases = placed_count;
        unsigned int ran = 0u;
        unsigned int refused = 0u;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = s_chain_ran, .bytes = sizeof(s_chain_ran), .val = 0);
        writings_answers_read(answers, count, s_chain_answered, s_chain_ran, &ran, &refused);
        s_cases = saved;
        platform_file_close(answers);
        unsigned int located_part = 0u;
        unsigned int located_host = 0u;
        unsigned int differ = 0u;
        for (unsigned int number = 0u; number < count; number += 1u)
        {
            unsigned int agreed = 0u;
            for (unsigned int at = 0u; at < placed_count; at += 1u)
            {
                agreed += (s_chain_answered[number][at] == placed[at][2]) ? 1u : 0u;
            }
            const int part = s_chain_ran[number] && (agreed == placed_count);
            const int host = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_verdict[number],
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "located", .ci = EMBED_FALSE));
            located_part += part ? 1u : 0u;
            located_host += host ? 1u : 0u;
            if (part != host)
            {
                differ += 1u;
                {
                    mmgr_line_put(platform_file_line(out), relation);
                    mmgr_line_put(platform_file_line(out), " `");
                    mmgr_line_put(platform_file_line(out), s_chain[number]);
                    mmgr_line_put(platform_file_line(out), "`: ");
                    mmgr_line_put(platform_file_line(out), host ? "located" : "out");
                    mmgr_line_put(platform_file_line(out), " on the host, ");
                    mmgr_line_put(platform_file_line(out),
                                  s_chain_ran[number] ? (part ? "located" : "out") : "not run");
                    mmgr_line_put(platform_file_line(out), " on the part\n\n");
                }
            }
        }
        {
            mmgr_line_put(platform_file_line(out), "| ");
            mmgr_line_put(platform_file_line(out), relation);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), placed_count, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), count, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), ran, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), located_part, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), located_host, 1u);
            mmgr_line_put(platform_file_line(out), " | ");
            mmgr_line_decimal(platform_file_line(out), differ, 1u);
            mmgr_line_put(platform_file_line(out), " |\n");
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, relation, 8u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, placed_count, 1u);
            mmgr_line_put(output_line, " cases placed, ");
            mmgr_line_decimal(output_line, count, 1u);
            mmgr_line_put(output_line, " arrangements, ");
            mmgr_line_decimal(output_line, ran, 1u);
            mmgr_line_put(output_line, " run, ");
            mmgr_line_decimal(output_line, located_part, 1u);
            mmgr_line_put(output_line, " located on the part and ");
            mmgr_line_decimal(output_line, located_host, 1u);
            mmgr_line_put(output_line, " on the host, ");
            mmgr_line_decimal(output_line, differ, 1u);
            mmgr_line_put(output_line, " verdicts differ");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        differ_total += differ;
    }
    platform_file_close(out);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass descent: ");
        mmgr_line_decimal(output_line, differ_total, 1u);
        mmgr_line_put(output_line, " verdicts differ, the record written to ");
        mmgr_line_put(output_line, record);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (differ_total == 0u) ? 0 : 1;
}

// `text` with every `{name}` replaced by `put`, into `out`: 1, or 0 where `out` will not fit it
static int writings_fill(const char *text, const char *name, const char *put, char *out, size_t room)
{
    char brace[300];
    {
        VerbaLinea buffer_line = mmgr_line_over(brace, sizeof(brace));
        mmgr_line_put(&buffer_line, "{");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, "}");
        (void)mmgr_line_finish(&buffer_line);
    }
    size_t at = 0u;
    const char *walk = text;
    while ((*walk != '\0') && (at < room))
    {
        if (descent_text_order(walk, brace,
                               (size_t)(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = brace,
                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR))) == 0)
        {
            VerbaLinea formed_line_1 = mmgr_line_over(&out[at], room - at);
            mmgr_line_put(&formed_line_1, put);
            (void)mmgr_line_finish(&formed_line_1);
            at += (size_t)(int)formed_line_1.at;
            walk += EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = brace, .cap = PLATFORM_TEXT_TO_TERMINATOR);
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

// The words of the word web that are one precept over their operands, each checked against the writings the part gave:
// the ruleset's form for the word, its result put in WRITINGS_RESULT and its operands by place in WRITINGS_LEFT and
// WRITINGS_RIGHT, looked for among the forms the search ran. Where it ran, it either gives every case the precept's
// word or does not; where the search never ran that text, the record reports it. Written to `record`: 0 where every
// form found matches, 1 where one does not, 2 where nothing could be read
int writings_krs_read(const char *folder, const char *ruleset, const char *record)
{
    // the ruleset as the run stores it, or the disk where the run stores none (qry_member_file)
    const unsigned char *rules = NULL;
    unsigned long long rules_size = 0ULL;
    unsigned long long rules_at = 0ULL;
    const int rules_read = writings_load(folder) && qry_member_file_blob(ruleset, &rules, &rules_size);
    PlatformFile *const out = rules_read ? platform_file_open(record, "wb") : NULL;
    if (out == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the search in ");
            mmgr_line_put(&engine_note.line, folder);
            mmgr_line_put(&engine_note.line, ", the ruleset ");
            mmgr_line_put(&engine_note.line, ruleset);
            mmgr_line_put(&engine_note.line, " or the record ");
            mmgr_line_put(&engine_note.line, record);
            mmgr_line_put(&engine_note.line, " was not reached");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    static char s_rule[512][WRITINGS_LINE];
    unsigned int rule_count = 0u;
    while ((rule_count < 512u) && line_read(rules, rules_size, &rules_at, s_rule[rule_count], sizeof(s_rule[0])))
    {
        rule_count += (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_rule[rule_count],
                                  .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "form ", .ci = EMBED_FALSE))
                          ? 1u
                          : 0u;
    }
    {
        mmgr_line_put(platform_file_line(out), "# A ruleset's forms checked against the part's writings\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out),
                      "Written by `interface_sass_writings.sh` whole on every run. Each word of the word web that is "
                      "one precept over its operands is looked up in `");
        mmgr_line_put(platform_file_line(out), ruleset);
        mmgr_line_put(platform_file_line(out),
                      "`, its form written with its result and operands where the search puts them, and looked for "
                      "among the forms `interface_sass_writings.md` ran on the part.\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out), "| word | precept | form as run | on the part |\n|---|---|---|---|\n");
    }
    unsigned int found_count = 0u;
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
        const size_t name_length =
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = one->name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        const char *rule = NULL;
        for (unsigned int at = 0u; (rule == NULL) && (at < rule_count); at += 1u)
        {
            rule = ((descent_text_order(s_rule[at] + 5, one->name, (size_t)(name_length)) == 0) &&
                    (s_rule[at][5u + name_length] == ' '))
                       ? s_rule[at]
                       : NULL;
        }
        if (rule == NULL)
        {
            {
                mmgr_line_put(platform_file_line(out), "| `");
                mmgr_line_put(platform_file_line(out), one->name);
                mmgr_line_put(platform_file_line(out), "` | ");
                mmgr_line_put(platform_file_line(out), s_precept_text[precept]);
                mmgr_line_put(platform_file_line(out), " | no form | - |\n");
            }
            continue;
        }
        const char *const equals =
            EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = rule, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " = ",
                       .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        char params[8][32];
        unsigned int param_count = 0u;
        const char *walk = rule + 5u + name_length;
        while ((equals != NULL) && (walk < equals) && (param_count < 8u))
        {
            walk +=
                EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = walk, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " ");
            const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = walk,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " =");
            if ((length == 0u) || (walk >= equals))
            {
                break;
            }
            {
                VerbaLinea buffer_line = mmgr_line_over(params[param_count], sizeof(params[0]));
                mmgr_line_put_max(&buffer_line, walk, (size_t)((int)length));
                (void)mmgr_line_finish(&buffer_line);
            }
            param_count += 1u;
            walk += length;
        }
        char filled[WRITINGS_LINE];
        char next[WRITINGS_LINE];
        {
            VerbaLinea buffer_line = mmgr_line_over(filled, sizeof(filled));
            mmgr_line_put(&buffer_line, (equals != NULL) ? (equals + 3) : "");
            (void)mmgr_line_finish(&buffer_line);
        }
        const char *const places[3] = {WRITINGS_RESULT, WRITINGS_LEFT, WRITINGS_RIGHT};
        for (unsigned int at = 0u; (at < param_count) && (at < 3u); at += 1u)
        {
            writings_fill(filled, params[at], places[at], next, sizeof(next));
            {
                VerbaLinea buffer_line = mmgr_line_over(filled, sizeof(filled));
                mmgr_line_put(&buffer_line, next);
                (void)mmgr_line_finish(&buffer_line);
            }
        }
        char flat[512];
        writings_flat(filled, flat, sizeof(flat));
        unsigned int found = s_form_count;
        for (unsigned int number = 0u; (found == s_form_count) && (number < s_form_count); number += 1u)
        {
            found =
                (s_ran[number] && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_forms[number],
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = flat, .ci = EMBED_FALSE)))
                    ? number
                    : found;
        }
        const char *verdict = "not among the forms run";
        if (found < s_form_count)
        {
            const int agreed = s_relation_agreed[found][relation];
            verdict = agreed ? "gives every case its word" : "does not give every case its word";
            found_count += agreed ? 1u : 0u;
            differ += agreed ? 0u : 1u;
        }
        {
            mmgr_line_put(platform_file_line(out), "| `");
            mmgr_line_put(platform_file_line(out), one->name);
            mmgr_line_put(platform_file_line(out), "` | ");
            mmgr_line_put(platform_file_line(out), s_precept_text[precept]);
            mmgr_line_put(platform_file_line(out), " | `");
            mmgr_line_put(platform_file_line(out), flat);
            mmgr_line_put(platform_file_line(out), "` | ");
            mmgr_line_put(platform_file_line(out), verdict);
            mmgr_line_put(platform_file_line(out), " |\n");
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, one->name, 18u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, s_precept_text[precept], 4u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put_columns(output_line, flat, 44u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, verdict);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    platform_file_close(out);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass krs: ");
        mmgr_line_decimal(output_line, found_count, 1u);
        mmgr_line_put(output_line, " forms match, ");
        mmgr_line_decimal(output_line, differ, 1u);
        mmgr_line_put(output_line, " do not, the record written to ");
        mmgr_line_put(output_line, record);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (differ == 0u) ? 0 : 1;
}
