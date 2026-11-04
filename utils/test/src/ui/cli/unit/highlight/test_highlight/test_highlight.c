// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_highlight.c: a JSON line containing a string of thousands of characters colors; a line is colored by its rules, each token a run of its own; a
// state carries to the next line; columns count as the page counts them; a rule reads its groups and a grammar its includes; what matches no rule has no
// token; a grammar's leaves are returned a line at a time; colorings run at once, beside one another and within one another, and a grammar stored again
// while one reads it; a language plugin's grammar is stored under its id's key; and the C, Python, Rust and JavaScript grammars name a function's
// parameters, their types and a body's locals
#include "ui/cli/src/editor/highlight.h"
#include "ui/cli/src/plugins/plugins.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_grammar[] = "{\"keywords\": [\"if\", \"return\"], \"tokenizer\": {\"root\": ["
                                "[{\"pattern\": \"(\\\\w+)(:)\", \"flags\": \"\"}, [\"label\", \"delimiter\"]],"
                                "[{\"pattern\": \"[a-z_]\\\\w*\", \"flags\": \"\"}, {\"cases\": {\"@keywords\": \"keyword\", \"@default\": "
                                "\"identifier\"}}],"
                                "[{\"pattern\": \"\\\\d+\", \"flags\": \"\"}, \"number\"],"
                                "[{\"pattern\": \"\\\"\", \"flags\": \"\"}, \"string\", \"@string\"],"
                                "[{\"pattern\": \"\\\\/\\\\*\", \"flags\": \"\"}, \"comment\", \"@comment\"],"
                                "[{\"pattern\": \"[()]\", \"flags\": \"\"}, \"@brackets\"],"
                                "{\"include\": \"@space\"}],"
                                "\"space\": [[{\"pattern\": \"\\\\s+\", \"flags\": \"\"}, \"\"]],"
                                "\"string\": [[{\"pattern\": \"[^\\\"]+\", \"flags\": \"\"}, \"string\"], [{\"pattern\": \"\\\"\", \"flags\": "
                                "\"\"}, "
                                "\"string\", \"@pop\"]],"
                                "\"comment\": [[{\"pattern\": \"[^*]+\", \"flags\": \"\"}, \"comment\"], [{\"pattern\": \"\\\\*\\\\/\", \"flags\": "
                                "\"\"}, \"comment\", \"@pop\"], [{\"pattern\": \"\\\\*\", \"flags\": \"\"}, \"comment\"]]}}";

// a grammar that names every word `word`
static const char s_words[] = "{\"tokenizer\": {\"root\": [[\"[a-z]+\", \"word\"], [\"\\\\s+\", \"\"]]}}";

// a grammar whose `indent` runs a block by indent, a tab to the next fourth column, lines of blanks or a `;` note alone skipped, and statements written in root and in a block
static const char s_indented[] = "{\"indent\": {\"states\": {\"block\": \"close\"}, \"tab\": 4, \"skip\": \"^\\\\s*(?:;.*)?$\", "
                                 "\"begins\": [\"root\", \"block\"]}, \"tokenizer\": {"
                                 "\"root\": [[\"$\", \"end\"], [\":$\", \"open\", \"@block\"], {\"include\": \"@code\"}],"
                                 "\"block\": [[\"$\", \"end\"], [\":$\", \"open\", \"@block\"], {\"include\": \"@code\"}],"
                                 "\"code\": [[\"[a-z]+\", \"word\"], [\";.*$\", \"note\"], [\"\\\\s+\", \"\"]]}}";

static const char s_one_line[] = "0 keyword|2 |3 delimiter.bracket|4 identifier|5 delimiter.bracket/";

static char s_out[1u << 20u];
static char s_line[1u << 15u];
static char s_plugin[1u << 18u];
static char s_state[256];
static char s_runs[1u << 16u];

// Work done beside a coloring under way, on a thread of its own: what it does, the lock and wake its end is reported under, 1 where it ended and did its
// work, -1 where it ended and did not, 0 before it ends, and the leaves it colored
static int (*s_aside_work)(void);
static CliWake s_aside_wake = CLI_WAKE_INIT;
static int s_aside_ended;
static char s_aside_leaves[256];
static int s_test_grammar_stored;

// the grammar of `size` bytes of JSON, its `member` where it is not NULL, stored under `key`: 1, or 0 where it is refused
static int grammar_stored(const char *key, const char *text, unsigned long long size, const char *member)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(text, size, &root);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    return EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = key, .json = json, .def = (member != NULL) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = member) : root, .err_msg = &err_msg);
}

// `line` colored from `state` with the grammar under `key`, its runs `column name` joined by `|` in s_runs, and the state it ends in in s_state
static const char *line_colored(const char *key, const char *line, const char *state)
{
    const char *const lines[1] = {line};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea colored = {s_out, sizeof(s_out), 0ull};
    VerbaLinea runs_line = {s_runs, sizeof(s_runs), 0ull};
    s_state[0] = '\0';
    if (EMBED_CALL(cli_highlight_key_err_msg.color, CliHighlightKeyErrMsgCfg, .key = key, .state = state, .lines = lines, .count = 1u, .out = &colored, .err_msg = &err_msg))
    {
        unsigned int found = CLI_JSON_NONE;
        const CliJson *const json = cli_unity_parsed(s_out, colored.at, &found);
        const unsigned int names = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = found, .name = "names");
        const unsigned int runs = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = found, .name = "runs"), .index = 0u);
        for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = runs, .index = at) != CLI_JSON_NONE; at += 1u)
        {
            const unsigned int run = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = runs, .index = at);
            const unsigned int column = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = run, .index = 0u);
            const unsigned int name = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = run, .index = 1u);
            unsigned int index = 0u;
            for (unsigned long long digit = 0ull; digit < json->node[name].length; digit += 1ull)
            {
                // a digit's value is below 10
                index = index * 10u + (unsigned int)(json->text[json->node[name].text + digit] - '0');
            }
            const char *const named = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = names, .index = index), .length_pointer = NULL);
            mmgr_line_put(&runs_line, (at > 0u) ? "|" : "");
            mmgr_line_put_n(&runs_line, json->text + json->node[column].text, json->node[column].length);
            mmgr_line_ch(&runs_line, ' ');
            mmgr_line_put(&runs_line, (named != NULL) ? named : "?");
        }
        const char *const ended = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = found, .name = "states"), .index = 0u), .length_pointer = NULL);
        const char *const copied = (ended != NULL) ? ended : "";
        memor.cpy(&(MemoriaCfg){.dst = s_state, .src = copied, .bytes = CLI_UNITY_LENGTH(copied) + 1u});
    }
    (void)mmgr_line_finish(&runs_line);
    return s_runs;
}

// each line's leaves written after those before, `col name` joined by `|`, each line ended by `/`
static void leaves_recorded(const CliLeavesEachCfg *args)
{
    VerbaLinea *const out = args->context;
    for (unsigned int run = 0u; run < args->leaves->runs; run += 1u)
    {
        mmgr_line_put(out, (run != 0u) ? "|" : "");
        mmgr_line_decimal(out, args->leaves->col[run], 1u);
        mmgr_line_ch(out, ' ');
        mmgr_line_put(out, args->leaves->text + args->leaves->name_at[args->leaves->name[run]]);
    }
    mmgr_line_ch(out, '/');
}

// a line colored by the grammar `test` beside a coloring under way: 1 where it colored
static int aside_colored(void)
{
    static const char *const s_lines[1] = {"if (x)"};
    char ended[64];
    char why[256];
    VerbaLinea out = {s_aside_leaves, sizeof(s_aside_leaves), 0ull};
    VerbaLinea ended_line = {ended, sizeof(ended), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int colored = EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = "test", .state = "root", .lines = s_lines, .count = 1u, .ends = 0, .each = leaves_recorded, .context = &out, .ended = &ended_line, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    return colored;
}

// the grammar `swapped` stored again beside a coloring that reads it, another in its place: 1 where it was stored
static int aside_stored(void)
{
    return grammar_stored("swapped", s_words, sizeof(s_words) - 1u, NULL);
}

// the aside's work done and its end reported
static void aside_run(void *context)
{
    (void)context;
    const int done = s_aside_work();
    s_aside_ended = done ? 1 : -1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_aside_wake);
}

// the first line of a coloring recorded, and the aside's work started on a thread of its own and waited for, five seconds at the most, before the coloring
// goes on; each line after recorded alone
static void leaves_waiting(const CliLeavesEachCfg *args)
{
    static CliThread s_thread = {aside_run, NULL, CLI_PRIORITY_WORK};
    leaves_recorded(args);
    const int first = s_aside_ended == 0;
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    if (first && EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_thread))
    {
        for (unsigned long long now = cli_clock_milliseconds(); (s_aside_ended == 0) && (now < until); now = cli_clock_milliseconds())
        {
            (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_aside_wake, .milliseconds_u64 = until - now);
        }
    }
    s_aside_ended = (first && (s_aside_ended == 0)) ? -2 : s_aside_ended;
}

// two lines of the grammar under `key` colored, the aside's work `work` done beside the first, their leaves into `leaves`, `capacity` bytes: 1 where they
// colored and the aside ended while the first waited and did its work
static int colored_adjacent(const char *key, int (*work)(void), char *leaves, unsigned long long capacity)
{
    static const char *const s_lines[2] = {"if (x)", "if (x)"};
    char ended[64];
    char why[256];
    VerbaLinea out = {leaves, capacity, 0ull};
    VerbaLinea ended_line = {ended, sizeof(ended), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    s_aside_work = work;
    s_aside_ended = 0;
    const int returned = EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = key, .state = "root", .lines = s_lines, .count = 2u, .ends = 0, .each = leaves_waiting, .context = &out, .ended = &ended_line, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    return returned && (s_aside_ended == 1);
}

// a line colored and leaves recorded on the same thread as a coloring under way returns them, the one under way going on in its own capacity
static void leaves_nested(const CliLeavesEachCfg *args)
{
    leaves_recorded(args);
    if (s_aside_leaves[0] == '\0')
    {
        (void)aside_colored();
    }
}

// the line `text` colored by the grammar of the plugin `id` from root, asserted against `required`
static void assert_plugin_line(const char *id, const char *text, const char *required)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = id, .key = key, .err_msg = &err_msg), id);
    TEST_ASSERT_EQUAL_STRING(required, line_colored(key, text, "root"));
}

void setUp(void)
{
    cli_unity_begun();
    if (!s_test_grammar_stored)
    {
        s_test_grammar_stored = 1;
        TEST_ASSERT_TRUE(grammar_stored("test", s_grammar, sizeof(s_grammar) - 1u, NULL));
    }
}

void tearDown(void)
{
}

void test_a_string_of_thousands_of_characters_runs_to_its_closing_quote(void)
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char required[32];
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = root))
    {
        TEST_IGNORE_MESSAGE("no tree is found: JSON's grammar is not read");
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "ui/plugins/json/plugin.json", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_plugin, sizeof(s_plugin));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    TEST_ASSERT_TRUE(grammar_stored("json", s_plugin, size, "grammar"));
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "{\"body\": \"");
    unsigned int length = 0u;
    for (unsigned int at = 0u; at < 15000u; at += 1u)
    {
        mmgr_line_put(&line, ((at % 1000u) == 0u) ? "\\\"" : "x");
        length += ((at % 1000u) == 0u) ? 2u : 1u;
    }
    mmgr_line_put(&line, "\", \"n\": 1}");
    (void)mmgr_line_finish(&line);
    (void)line_colored("json", s_line, "root");
    const char *const string = cellul.find(&(CatenaFinitaCfg){.src = s_runs, .cap = CLI_CELLS_POOL, .other = "|9 ", .other_cap = CLI_CELLS_POOL});
    TEST_ASSERT_NOT_NULL(string);
    CLI_TEST_ASSERT_CONTAINS("string", string);
    VerbaLinea next = {required, sizeof(required), 0ull};
    mmgr_line_ch(&next, '|');
    mmgr_line_decimal(&next, 9u + length + 2u, 1u);
    mmgr_line_ch(&next, ' ');
    (void)mmgr_line_finish(&next);
    const char *const bar = cellul.chr(&(CatenaFinitaCfg){.src = string + 1, .cap = CLI_CELLS_POOL, .byte = '|'});
    TEST_ASSERT_NOT_NULL(bar);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = bar, .other_pointer = required, .size = CLI_UNITY_LENGTH(required)));
}

void test_a_line_is_colored_by_its_rules_and_ends_in_root(void)
{
    TEST_ASSERT_EQUAL_STRING("0 keyword|2 |3 delimiter.bracket|4 identifier|5 delimiter.bracket|6 |7 keyword|13 |14 number", line_colored("test", "if (x) return 12", "root"));
    TEST_ASSERT_EQUAL_STRING("root", s_state);
}

void test_each_bracket_is_a_token_of_its_own(void)
{
    TEST_ASSERT_EQUAL_STRING("0 delimiter.bracket|1 delimiter.bracket|2 delimiter.bracket|3 delimiter.bracket|4 "
                             "delimiter.bracket|5 delimiter.bracket|6 delimiter.bracket|7 delimiter.bracket",
                             line_colored("test", "(((())))", "root"));
}

void test_a_comment_opened_colors_and_its_state_transfers(void)
{
    TEST_ASSERT_EQUAL_STRING("0 identifier|1 |2 comment|4 comment", line_colored("test", "a /* open", "root"));
    TEST_ASSERT_EQUAL_STRING("root/comment", s_state);
}

void test_a_comments_state_transfers_to_the_next_line_until_it_closes(void)
{
    TEST_ASSERT_EQUAL_STRING("0 comment|6 comment|8 |9 identifier", line_colored("test", "still */ b", "root/comment"));
    TEST_ASSERT_EQUAL_STRING("root", s_state);
}

void test_columns_count_as_the_page_counts_them(void)
{
    TEST_ASSERT_EQUAL_STRING("0 string|1 string|3 string|4 |5 number", line_colored("test", "\"\xF0\x9F\x98\x80\" 7", "root"));
}

void test_a_rule_reads_its_groups_and_a_grammar_its_includes(void)
{
    TEST_ASSERT_EQUAL_STRING("0 label|3 delimiter|4 |5 number", line_colored("test", "key: 1", "root"));
}

void test_what_matches_no_rule_has_no_token(void)
{
    TEST_ASSERT_EQUAL_STRING("0 ", line_colored("test", "#", "root"));
}

void test_a_hundred_grammars_are_stored_each_under_its_key(void)
{
    for (unsigned int at = 0u; at < 100u; at += 1u)
    {
        char key[32];
        VerbaLinea named = {key, sizeof(key), 0ull};
        mmgr_line_put(&named, "many_");
        mmgr_line_decimal(&named, at, 1u);
        (void)mmgr_line_finish(&named);
        TEST_ASSERT_TRUE_MESSAGE(grammar_stored(key, s_grammar, sizeof(s_grammar) - 1u, NULL), key);
    }
    TEST_ASSERT_EQUAL_STRING("0 keyword", line_colored("many_0", "if", "root"));
    TEST_ASSERT_EQUAL_STRING("0 keyword", line_colored("many_99", "if", "root"));
}

void test_a_grammars_leaves_are_returned_a_line_at_a_time(void)
{
    static const char *const s_lines[3] = {"if (x)", "a /* open", "still */ b"};
    char leaves[512];
    char ended[64];
    char why[256];
    VerbaLinea out = {leaves, sizeof(leaves), 0ull};
    VerbaLinea ended_line = {ended, sizeof(ended), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = "test", .state = "root", .lines = s_lines, .count = 3u, .ends = 0, .each = leaves_recorded, .context = &out, .ended = &ended_line, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    (void)mmgr_line_finish(&ended_line);
    TEST_ASSERT_EQUAL_STRING("0 keyword|2 |3 delimiter.bracket|4 identifier|5 "
                             "delimiter.bracket/0 identifier|1 |2 comment|4 comment/0 "
                             "comment|6 comment|8 |9 identifier/",
                             leaves);
    TEST_ASSERT_EQUAL_STRING("root", ended);
}

void test_a_plugins_grammar_is_stored_under_its_key_and_colors(void)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "c", .key = key, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING("plugin:c", key);
    TEST_ASSERT_EQUAL_STRING("0 type|3 |4 identifier|5 delimiter.statement", line_colored(key, "int x;", "root"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "c", .key = key, .err_msg = &err_msg));
}

void test_a_plugin_there_is_none_of_is_refused_with_error(void)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "no_such_plugin", .key = key, .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, err_msg.at);
}

void test_a_line_is_colored_while_a_coloring_under_way_waits_on_it(void)
{
    char leaves[512];
    s_aside_leaves[0] = '\0';
    TEST_ASSERT_TRUE(colored_adjacent("test", aside_colored, leaves, sizeof(leaves)));
    TEST_ASSERT_EQUAL_STRING(s_one_line, s_aside_leaves);
    TEST_ASSERT_EQUAL_STRING("0 keyword|2 |3 delimiter.bracket|4 identifier|5 "
                             "delimiter.bracket/0 keyword|2 |3 delimiter.bracket|4 "
                             "identifier|5 delimiter.bracket/",
                             leaves);
}

void test_a_grammar_stored_again_leaves_a_coloring_read_state_the_one_it_found_and_the_next_the_new(void)
{
    char leaves[512];
    TEST_ASSERT_TRUE(grammar_stored("swapped", s_grammar, sizeof(s_grammar) - 1u, NULL));
    TEST_ASSERT_TRUE(colored_adjacent("swapped", aside_stored, leaves, sizeof(leaves)));
    TEST_ASSERT_EQUAL_STRING("0 keyword|2 |3 delimiter.bracket|4 identifier|5 "
                             "delimiter.bracket/0 keyword|2 |3 delimiter.bracket|4 "
                             "identifier|5 delimiter.bracket/",
                             leaves);
    TEST_ASSERT_EQUAL_STRING("0 word|2 |3 word", line_colored("swapped", "if x", "root"));
}

void test_a_coloring_begun_within_another_on_its_thread_colors_and_the_other_goes_on(void)
{
    static const char *const s_lines[2] = {"a /* open", "still */ b"};
    char leaves[512];
    char ended[64];
    char why[256];
    VerbaLinea out = {leaves, sizeof(leaves), 0ull};
    VerbaLinea ended_line = {ended, sizeof(ended), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    s_aside_leaves[0] = '\0';
    TEST_ASSERT_TRUE(EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = "test", .state = "root", .lines = s_lines, .count = 2u, .ends = 0, .each = leaves_nested, .context = &out, .ended = &ended_line, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    TEST_ASSERT_EQUAL_STRING(s_one_line, s_aside_leaves);
    TEST_ASSERT_EQUAL_STRING("0 identifier|1 |2 comment|4 comment/0 comment|6 comment|8 |9 identifier/", leaves);
}

void test_a_grammars_indent_runs_its_block_each_close_a_mark(void)
{
    static const char *const s_lines[6] = {"a:", "\tb", "; note", "c", "d:", "  e"};
    char leaves[512];
    char ended[64];
    char why[256];
    VerbaLinea out = {leaves, sizeof(leaves), 0ull};
    VerbaLinea ended_line = {ended, sizeof(ended), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(grammar_stored("indented", s_indented, sizeof(s_indented) - 1u, NULL));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_highlight_key.leaves, CliHighlightKeyCfg, .key = "indented", .state = "root", .lines = s_lines, .count = 6u, .ends = 1, .each = leaves_recorded, .context = &out, .ended = &ended_line, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    (void)mmgr_line_finish(&ended_line);
    TEST_ASSERT_EQUAL_STRING("0 word|1 open|2 end/0 |1 word|2 end/0 note|6 end/0 "
                             "close|0 word|1 end/0 word|1 open|2 end/0 |2 word|3 "
                             "end|3 close/",
                             leaves);
    TEST_ASSERT_EQUAL_STRING("2|root", ended);
}

void test_cs_prototype_names_its_parameters_and_their_types(void)
{
    assert_plugin_line("c", "static int add(const char *name, unsigned int count);",
                       "0 keyword|6 |7 type|10 |11 identifier.declaration.function|14 "
                       "delimiter.bracket.parameter.depth.open|15 keyword|20 |21 type.string|25 |26 operator.memory|27 "
                       "identifier.parameter|31 delimiter.separator|32 |33 type.number|41 |42 type.number|45 |46 "
                       "identifier.parameter|51 delimiter.bracket.parameter.depth.close|52 delimiter.statement");
}

void test_a_c_local_and_its_type_are_named(void)
{
    assert_plugin_line("c", "    unsigned int total = 0u;",
                       "0 |4 type.number|12 |13 type.number|16 |17 identifier.local|22 |23 operator.assign|24 |25 "
                       "number|26 number.suffix|27 delimiter.statement");
}

void test_pythons_head_names_its_parameters_and_an_annotations_type(void)
{
    assert_plugin_line("python", "def total(first: int, second=\"x\"):",
                       "0 keyword|3 |4 identifier.declaration.function|9 delimiter.bracket.parameter.depth.open|10 "
                       "identifier.parameter|15 delimiter|16 |17 type.number|20 delimiter.separator|21 |22 "
                       "identifier.parameter|28 operator.assign|29 string|32 delimiter.bracket.parameter.depth.close|33 "
                       "operator.region.open|34 delimiter.statement");
}

void test_a_python_local_with_its_annotation_is_named(void)
{
    assert_plugin_line("python", "    count: int = 0",
                       "0 |4 identifier.local|9 delimiter|10 |11 type.number|14 |15 operator.assign|16 |17 "
                       "number|18 delimiter.statement");
}

void test_a_python_local_without_an_annotation_is_named(void)
{
    assert_plugin_line("python", "    total = 4", "0 |4 identifier.local|9 |10 operator.assign|11 |12 number|13 delimiter.statement");
}

void test_rusts_head_names_its_parameters_and_their_types(void)
{
    assert_plugin_line("rust", "fn add(name: &str, count: u32) -> u32 {",
                       "0 keyword|2 |3 identifier.declaration.function|6 delimiter.bracket.parameter.depth.open|7 "
                       "identifier.parameter|11 delimiter|12 |13 operator.memory|14 type.string|17 "
                       "delimiter.separator|18 |19 identifier.parameter|24 delimiter|25 |26 type.number|29 "
                       "delimiter.bracket.parameter.depth.close|30 |31 operator|33 |34 type|37 |38 "
                       "delimiter.bracket.region.depth.open");
}

void test_a_rust_local_and_its_type_are_named(void)
{
    assert_plugin_line("rust", "    let mut total: u64 = 0;",
                       "0 |4 keyword|7 |8 keyword|12 identifier.local|17 delimiter|18 |19 type.number|22 |23 "
                       "operator.assign|24 |25 number|26 delimiter.statement");
}

void test_javascripts_head_names_its_parameters(void)
{
    assert_plugin_line("javascript", "function add(name, count = 1) {",
                       "0 keyword|8 |9 identifier.declaration.function|12 delimiter.bracket.parameter.depth.open|13 "
                       "identifier.parameter|17 delimiter.separator|18 |19 identifier.parameter|24 |25 "
                       "operator.assign|26 |27 number|28 delimiter.bracket.parameter.depth.close|29 |30 "
                       "delimiter.bracket.region.depth.open");
}

void test_a_javascript_local_is_named(void)
{
    assert_plugin_line("javascript", "  const total = 4;",
                       "0 |2 keyword|7 |8 identifier.local|13 |14 operator.assign|15 |16 number|17 "
                       "delimiter.statement");
}

void test_a_terminal_line_colors_its_text_by_the_sgr_before_it_and_conceals_the_sequences(void)
{
    assert_plugin_line("xterm", "\x1b[31mred\x1b[0m plain", "0 conceal|5 terminal.1|8 conceal|12 ");
}

void test_a_terminal_color_among_other_numbers_and_a_bright_one_switched_to_are_read(void)
{
    assert_plugin_line("xterm", "\x1b[01;34mdir\x1b[0m", "0 conceal|8 terminal.4|11 conceal");
    assert_plugin_line("xterm", "\x1b[32mok\x1b[91m!", "0 conceal|5 terminal.2|7 conceal|12 terminal.9");
}

void test_a_terminal_color_of_the_256_leaves_the_text_plain(void)
{
    assert_plugin_line("xterm", "\x1b[38;5;31mx", "0 conceal|10 ");
}

void test_a_pattern_not_read_here_is_refused(void)
{
    static const char s_bad[] = "{\"tokenizer\": {\"root\": [[{\"pattern\": \"(?<=a)b\"}, \"x\"]]}}";
    TEST_ASSERT_FALSE(grammar_stored("bad", s_bad, sizeof(s_bad) - 1u, NULL));
}

void test_the_grammars_report_the_max_they_and_their_capacities_used_of_the_cellblock(void)
{
    char report[160];
    VerbaLinea report_line = {report, sizeof(report), 0ull};
    EMBED_CALL(cli_highlight.cells_usage_write, CliHighlightCfg, .out = &report_line);
    (void)mmgr_line_finish(&report_line);
    const char *const mark = cellul.chr(&(CatenaFinitaCfg){.src = report, .cap = CLI_CELLS_POOL, .byte = ' '});
    TEST_ASSERT_NOT_NULL_MESSAGE(mark, report);
    TEST_ASSERT_NOT_EQUAL_MESSAGE('0', mark[1], report);
}
