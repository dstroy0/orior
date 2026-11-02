// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_picks.c: a stored list answers its best rows first, and in its order where no query is given, and a list of many rows matches
// across the cores as it does on one
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/editor/picks.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#include <stdlib.h>

// the rows of the long list
#define PICKS_MANY 20000u

static const char *const s_rows[4] = {"alpha beta", "gamma", "alphabet", "beta"};

static char s_out[1u << 20u];
static char s_why[512];
static unsigned int s_hits[CLI_FUZZY_LETTERS];
static unsigned long long s_id;
static const CliJson *s_json;

// a number member of a node of the shared tree read as a double
static double number_of(unsigned int node, const char *name)
{
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = node, .name = name);
    char text[64] = "";
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = number, .kind = CFG_JSON_NUMBER) && (s_json->node[number].length < sizeof(text)))
    {
        memor.cpy(&(MemoriaCfg){.dst = text, .src = s_json->text + s_json->node[number].text, .bytes = s_json->node[number].length});
        text[s_json->node[number].length] = '\0';
    }
    return strtod(text, NULL);
}

// the score Go to File gives `text` for `query`, -1 where it does not answer
static double fuzzy_score(const char *query, const char *text)
{
    double score = 0.0;
    unsigned int count = 0u;
    return EMBED_CALL(cli_files_query.fuzzy, CliFilesQueryCfg, .query = query, .text = text, .from_u64 = 0ull, .score = &score, .hits = s_hits, .count_pointer = &count) ? score : -1.0;
}

// what the list `id` answers `query` with, at most `most` rows: the array's node
static unsigned int found_in(unsigned long long id, const char *query, unsigned long long most)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_picks_id.find, CliPicksIdCfg, .id = id, .query = query, .most = most, .out = &out, .err_msg = &err_msg));
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// row `at` of the long list
static void long_row(unsigned int at, char *text, unsigned long long room)
{
    VerbaLinea line = {text, room, 0ull};
    mmgr_line_put(&line, "test_case_");
    mmgr_line_decimal(&line, at, 1u);
    mmgr_line_ch(&line, '_');
    mmgr_line_decimal(&line, (at * 7919u) % PICKS_MANY, 1u);
    mmgr_line_put(&line, " tests/test_part_");
    mmgr_line_decimal(&line, at % 300u, 1u);
    mmgr_line_put(&line, ".py");
    (void)mmgr_line_finish(&line);
}

void setUp(void)
{
    cli_unity_begun();
    s_id = EMBED_CALL(cli_picks.store, CliPicksCfg, .texts = "alpha beta\ngamma\nalphabet\nbeta", .length = 30ull);
}

void tearDown(void)
{
    EMBED_CALL(cli_picks_id.forget, CliPicksIdCfg, .id = s_id);
}

void test_with_no_query_a_list_answers_its_first_rows_in_order(void)
{
    const unsigned int found = found_in(s_id, "", 3ull);
    TEST_ASSERT_EQUAL_UINT(3u, s_json->node[found].count);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        TEST_ASSERT_EQUAL_DOUBLE((double)at, number_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at), "at"));
    }
}

void test_the_row_that_is_the_query_comes_first(void)
{
    const unsigned int found = found_in(s_id, "beta", 10ull);
    TEST_ASSERT_EQUAL_DOUBLE(3.0, number_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 0u), "at"));
}

void test_a_row_that_does_not_answer_is_left_out(void)
{
    const unsigned int found = found_in(s_id, "beta", 10ull);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        TEST_ASSERT_NOT_EQUAL(1.0, number_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at), "at"));
    }
}

void test_each_row_is_scored_as_go_to_file_scores_it(void)
{
    const unsigned int found = found_in(s_id, "beta", 10ull);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at);
        // a row's index is a small whole number
        const unsigned int row = (unsigned int)number_of(one, "at");
        TEST_ASSERT_LESS_THAN_UINT(4u, row);
        TEST_ASSERT_EQUAL_DOUBLE(fuzzy_score("beta", s_rows[row]), number_of(one, "score"));
    }
}

void test_a_forgotten_list_answers_no_more(void)
{
    const unsigned long long id = EMBED_CALL(cli_picks.store, CliPicksCfg, .texts = "one\ntwo", .length = 7ull);
    EMBED_CALL(cli_picks_id.forget, CliPicksIdCfg, .id = id);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_picks_id.find, CliPicksIdCfg, .id = id, .query = "one", .most = 10ull, .out = &out, .err_msg = &err_msg));
}

void test_a_long_list_answers_across_the_cores_as_on_one(void)
{
    static char s_texts[PICKS_MANY * 64u];
    static unsigned int s_at[PICKS_MANY];
    static double s_score[PICKS_MANY];
    VerbaLinea texts = {s_texts, sizeof(s_texts), 0ull};
    unsigned int matched = 0u;
    for (unsigned int at = 0u; at < PICKS_MANY; at += 1u)
    {
        char row[64];
        long_row(at, row, sizeof(row));
        mmgr_line_put(&texts, (at > 0u) ? "\n" : "");
        mmgr_line_put(&texts, row);
        const double score = fuzzy_score("case12", row);
        if (score >= 0.0)
        {
            unsigned int place = matched;
            while ((place > 0u) && (s_score[place - 1u] < score))
            {
                s_score[place] = s_score[place - 1u];
                s_at[place] = s_at[place - 1u];
                place -= 1u;
            }
            s_score[place] = score;
            s_at[place] = at;
            matched += 1u;
        }
    }
    matched = (matched < 200u) ? matched : 200u;
    const unsigned long long id = EMBED_CALL(cli_picks.store, CliPicksCfg, .texts = s_texts, .length = texts.at);
    const unsigned int found = found_in(id, "case12", 200ull);
    TEST_ASSERT_EQUAL_UINT(matched, s_json->node[found].count);
    for (unsigned int at = 0u; at < matched; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at);
        TEST_ASSERT_EQUAL_DOUBLE((double)s_at[at], number_of(one, "at"));
        TEST_ASSERT_EQUAL_DOUBLE(s_score[at], number_of(one, "score"));
    }
    EMBED_CALL(cli_picks_id.forget, CliPicksIdCfg, .id = id);
}
