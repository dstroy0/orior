// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_shape.c: structural search: an expression placeholder matches a whole call and writes its text back, a name placeholder
// matches one name, statements match past spaces and comments, and a last expression takes the rest of its statement
#include "ui/cli/src/code/shape_internal.h"
#include "ui/cli/src/code/symbols_internal.h"
#include "ui/cli/src/text/edits.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_calls[] = "x = len( items ) + len(other(1, 2))\nprint(len(z))\n";
static const char s_names[] = "a = a + 1\nb = c + 1\nd.e = d.e + 1\n";
static const char s_script[] = "if (ready) {\n  start(); // go\n  log(\"started\");\n}\nif(done){finish();}\n";
static const char s_total[] = "total = a + b * c\nname = \"a + b\"\n";

static CliSymbolsLanguages *s_languages;
static CliTextEdit s_edits[64];
static char s_with[1u << 14u];
static unsigned int s_count;
static unsigned int s_used;
static char s_after[1u << 14u];

// a match recorded with its span and its rewritten text
static void match_recorded(const CliShapeEachCfg *args)
{
    if ((s_count >= 64u) || (s_used + args->found->with_size > sizeof(s_with)))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_with + s_used, .src = args->found->has_with ? args->found->with : "", .bytes = args->found->has_with ? args->found->with_size : 0ull});
    s_edits[s_count] = (CliTextEdit){args->found->from, args->found->to, s_with + s_used, args->found->has_with ? args->found->with_size : 0ull};
    // the recorded texts stay below the buffer's size, which an unsigned int represents
    s_used += (unsigned int)s_edits[s_count].size;
    s_count += 1u;
}

// how many matches of `pattern` `src` has, each rewritten with `template_text` where that is not NULL
static unsigned int matches_of(const char *language, const char *src, const char *pattern, const char *template_text)
{
    s_count = 0u;
    s_used = 0u;
    EMBED_CALL(cli_shape_languages.search, CliShapeLanguagesCfg, .languages = s_languages, .language = language, .src = src, .size = CLI_UNITY_LENGTH(src), .pattern = pattern, .template_text = template_text, .each = match_recorded, .context = NULL);
    return s_count;
}

// `src` with each match of `pattern` rewritten as `template_text` is `required`
static void assert_replaced(const char *required, const char *language, const char *src, const char *pattern, const char *template_text)
{
    (void)matches_of(language, src, pattern, template_text);
    VerbaLinea after = {s_after, sizeof(s_after), 0ull};
    (void)EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = src, .size = CLI_UNITY_LENGTH(src), .edits = s_edits, .count = s_count, .out = &after);
    CLI_TEST_ASSERT_TEXT(required, s_after, after.at);
}

void setUp(void)
{
    cli_unity_begun();
    s_languages = cli_symbols_languages_read();
}

void tearDown(void)
{
    if (s_languages != NULL)
    {
        g_cli_guard->persistent_buf_release(s_languages->languages.pair, g_cli_plugins_owner);
        *(&s_languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(s_languages, g_cli_code_owner);
}

void test_an_expression_placeholder_matches_each_complete_call(void)
{
    TEST_ASSERT_EQUAL_UINT(3u, matches_of("python", s_calls, "len($a$)", NULL));
    TEST_ASSERT_EQUAL_UINT(0u, s_edits[0].from.line);
    TEST_ASSERT_EQUAL_UINT(4u, s_edits[0].from.col);
    TEST_ASSERT_EQUAL_UINT(16u, s_edits[0].to.col);
    TEST_ASSERT_EQUAL_UINT(0u, s_edits[1].from.line);
    TEST_ASSERT_EQUAL_UINT(19u, s_edits[1].from.col);
    TEST_ASSERT_EQUAL_UINT(35u, s_edits[1].to.col);
    TEST_ASSERT_EQUAL_UINT(1u, s_edits[2].from.line);
    TEST_ASSERT_EQUAL_UINT(6u, s_edits[2].from.col);
    TEST_ASSERT_EQUAL_UINT(12u, s_edits[2].to.col);
}

void test_an_expression_placeholder_writes_its_text_back(void)
{
    assert_replaced("x = count(items) + count(other(1, 2))\nprint(count(z))\n", "python", s_calls, "len($a$)", "count($a$)");
}

void test_a_name_placeholder_matches_one_name_and_the_same_both_times(void)
{
    assert_replaced("a += 1\nb = c + 1\nd.e = d.e + 1\n", "python", s_names, "$x:name$ = $x:name$ + 1", "$x$ += 1");
}

void test_an_expression_named_twice_matches_the_same_text_both_times(void)
{
    assert_replaced("a += 1\nb = c + 1\nd.e += 1\n", "python", s_names, "$x$ = $x$ + 1", "$x$ += 1");
}

void test_statements_match_end_spaces_and_comments(void)
{
    TEST_ASSERT_EQUAL_UINT(2u, matches_of("javascript", s_script, "if ($c$) { $body:stmt$ }", NULL));
}

void test_statements_are_written_back_unchanged(void)
{
    assert_replaced("when(ready, () => { start(); // go\n  log(\"started\"); })\nwhen(done, () => { finish(); })\n", "javascript", s_script,
                    "if ($c$) { $body:stmt$ }", "when($c$, () => { $body$ })");
}

void test_an_expression_last_in_a_pattern_allocs_the_remainder_of_its_statement(void)
{
    assert_replaced("total = (a + b * c)\nname = \"a + b\"\n", "python", s_total, "total = $v$", "total = ($v$)");
}

void test_a_string_is_one_token(void)
{
    TEST_ASSERT_EQUAL_UINT(1u, matches_of("python", s_total, "a + b", NULL));
}

void test_a_search_sizes_its_memory_to_its_text_and_releases_it(void)
{
    (void)matches_of("python", s_total, "a + b", NULL);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_shape_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_shape_owner));
    TEST_ASSERT_LESS_THAN_UINT64(1ull << 16u, mmgr_owner_hw(g_cli_shape_owner));
}
