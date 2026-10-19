// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// picks_test.c: the picker's check: a list held answers its best rows first, and in its order where none is asked;
// and a list of many rows matches over the cores as it would on one
#include "../../../../../ui/cli/src/editor/files.h"
#include "../../../../../ui/cli/src/editor/picks.h"
#include "cli_check.h"

#include <stdlib.h>

// the rows of a list held
#define PICKS_MANY 20000u

static char s_out[1u << 20u];
static unsigned int s_hits[CLI_FUZZY_LETTERS];

// a number member of a node of the shared tree read as a double
static double picks_number(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = cli_json_member(json, node, name);
    char text[64] = "";
    if (cli_json_is(json, number, CFG_JSON_NUMBER) && (json->node[number].length < sizeof(text)))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = text, .src = json->text + json->node[number].text, .bytes = json->node[number].length});
        text[json->node[number].length] = '\0';
    }
    return strtod(text, NULL);
}

// the score Go to File gives `text` for `query`, -1 where it does not answer
static double picks_fuzzy(const char *query, const char *text)
{
    double score = 0.0;
    unsigned int count = 0u;
    return cli_files_fuzzy(query, text, 0ull, &score, s_hits, &count) ? score : -1.0;
}

// a list held answers its best rows first, and in its order where none is asked
static void picks_few(void)
{
    static const char *const s_rows[4] = {"alpha beta", "gamma", "alphabet", "beta"};
    const unsigned long long id = cli_picks_hold("alpha beta\ngamma\nalphabet\nbeta", 30ull);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_picks_find(id, "", 3ull, &out, &said), "a list held answers");
    unsigned int found = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_out, out.at, &found);
    int in_order = (json->node[found].count == 3u);
    for (unsigned int at = 0u; in_order && (at < 3u); at += 1u)
    {
        in_order = (picks_number(json, cli_json_item(json, found, at), "at") == (double)at);
    }
    cli_check(in_order, "with no query its first rows, in order");
    out.at = 0ull;
    (void)cli_picks_find(id, "beta", 10ull, &out, &said);
    json = cli_check_parsed(s_out, out.at, &found);
    cli_check(picks_number(json, cli_json_item(json, found, 0u), "at") == 3.0,
              "the row that is the query itself first");
    int answers = 1;
    int scored = 1;
    for (unsigned int at = 0u; cli_json_item(json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, found, at);
        const unsigned int row = (unsigned int)picks_number(json, one, "at");
        answers &= (row != 1u);
        scored &= (row < 4u) && (picks_fuzzy("beta", s_rows[row]) == picks_number(json, one, "score"));
    }
    cli_check(answers, "a row that does not answer is left out");
    cli_check(scored, "each row is scored as Go to File scores it");
    cli_picks_forget(id);
    out.at = 0ull;
    said.at = 0ull;
    cli_check(!cli_picks_find(id, "beta", 10ull, &out, &said), "a list let go answers no more");
}

// a row of the many
static void picks_row(unsigned int at, char *text, unsigned long long room)
{
    CliLine line = {text, room, 0ull};
    cli_line_text(&line, "test_case_");
    cli_line_decimal(&line, at, 1u);
    cli_line_character(&line, '_');
    cli_line_decimal(&line, (at * 7919u) % PICKS_MANY, 1u);
    cli_line_text(&line, " tests/test_part_");
    cli_line_decimal(&line, at % 300u, 1u);
    cli_line_text(&line, ".py");
    (void)cli_line_closed(&line);
}

// a list of many rows matches over the cores as it would on one
static void picks_many(void)
{
    static char s_texts[PICKS_MANY * 64u];
    static unsigned int s_at[PICKS_MANY];
    static double s_score[PICKS_MANY];
    CliLine texts = {s_texts, sizeof(s_texts), 0ull};
    unsigned int alone = 0u;
    for (unsigned int at = 0u; at < PICKS_MANY; at += 1u)
    {
        char row[64];
        picks_row(at, row, sizeof(row));
        cli_line_text(&texts, (at > 0u) ? "\n" : "");
        cli_line_text(&texts, row);
        const double score = picks_fuzzy("case12", row);
        if (score >= 0.0)
        {
            unsigned int place = alone;
            while ((place > 0u) && ((s_score[place - 1u] < score)))
            {
                s_score[place] = s_score[place - 1u];
                s_at[place] = s_at[place - 1u];
                place -= 1u;
            }
            s_score[place] = score;
            s_at[place] = at;
            alone += 1u;
        }
    }
    alone = (alone < 200u) ? alone : 200u;
    const unsigned long long id = cli_picks_hold(s_texts, texts.at);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_picks_find(id, "case12", 200ull, &out, &said);
    unsigned int found = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &found);
    int same = (json->node[found].count == alone);
    for (unsigned int at = 0u; same && (at < alone); at += 1u)
    {
        const unsigned int one = cli_json_item(json, found, at);
        same = (picks_number(json, one, "at") == (double)s_at[at]) && (picks_number(json, one, "score") == s_score[at]);
    }
    cli_check(same, "the rows over the cores are those one core finds, in its order");
    cli_picks_forget(id);
}

int main(void)
{
    picks_few();
    picks_many();
    return cli_checked();
}
