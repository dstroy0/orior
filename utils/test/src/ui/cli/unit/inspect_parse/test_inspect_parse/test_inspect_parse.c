// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_inspect_parse.c: a Python file and a JavaScript file are colored, folded and spanned from their parse
#include "ui/cli/src/inspect/inspect.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_python[] = "import os.path as p\n\n\n@cache\ndef area(width, height=2):\n    \"\"\"Area.\"\"\"  # "
                               "the area\n    return Shape(width).scale(height, by=p.sep) + self.size\n\n\nclass "
                               "Shape:\n    pass\n";
static const char s_javascript[] = "// the area\nexport function area(width, height = 2) {\n  const shape = new Shape(width);\n  return "
                                   "shape.scale(height) + `${width}px` + /a\\/b/.source;\n}\n\nclass Shape {\n  scale(by) {\n    return { by, "
                                   "size: 1 };\n  }\n}\n";

static CliInspectRun s_runs[4096];
static unsigned int s_line_first[256];
static unsigned int s_line_runs[256];
static unsigned int s_fold_first[256];
static unsigned int s_fold_last[256];
static CliInspectSpan s_ranges[4096];
static CliParsed s_parsed;
static char s_classes[2048];
static char s_word[256];

// `text` in `language` parsed into s_parsed
static void parsed(const char *language, const char *text)
{
    EMBED_CALL(cli_inspect_parsed.parsed_begin, CliInspectParsedCfg, .parsed = &s_parsed, .runs = s_runs, .runs_max = 4096u, .line_first = s_line_first, .line_runs = s_line_runs, .lines_max = 256u, .fold_first = s_fold_first, .fold_last = s_fold_last, .folds_max = 256u, .ranges = s_ranges, .ranges_max = 4096u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_language_text.parse, CliInspectLanguageTextCfg, .language = language, .text = text, .size = CLI_UNITY_LENGTH(text), .parsed = &s_parsed));
}

// the class the parse gives each word of line `line` of `text`, `word=class` joined by `|`, delimiters and operators left out where
// `plain`; the text is ASCII, and a column is a byte
static const char *classes_of(const char *text, unsigned int line, int plain)
{
    const char *start = text;
    for (unsigned int at = 0u; (at < line) && (cellul.chr(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .byte = '\n'}) != NULL); at += 1u)
    {
        start = cellul.chr(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .byte = '\n'}) + 1;
    }
    const char *const newline = cellul.chr(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .byte = '\n'});
    const char *const end = (newline != NULL) ? newline : (start + CLI_UNITY_LENGTH(start));
    // the line's end lies at or past its start
    const unsigned long long length = (unsigned long long)(end - start);
    VerbaLinea out = {s_classes, sizeof(s_classes), 0ull};
    for (unsigned int index = 0u; index < s_parsed.line_runs[line]; index += 1u)
    {
        const CliInspectRun *const run = &s_parsed.run[s_parsed.line_first[line] + index];
        unsigned long long from = (run->from < length) ? run->from : length;
        unsigned long long to = (run->to < length) ? run->to : length;
        while ((from < to) && ((start[from] == ' ') || (start[from] == '\t')))
        {
            from += 1ull;
        }
        while ((to > from) && ((start[to - 1u] == ' ') || (start[to - 1u] == '\t')))
        {
            to -= 1ull;
        }
        const int skipped = plain && ((run->class_of == CLI_INSPECT_DELIMITER) || (run->class_of == CLI_INSPECT_OPERATOR));
        if ((from == to) || skipped)
        {
            continue;
        }
        mmgr_line_put(&out, (out.at > 0ull) ? "|" : "");
        mmgr_line_put_n(&out, start + from, to - from);
        mmgr_line_ch(&out, '=');
        mmgr_line_put(&out, g_cli_inspect_classes[run->class_of]);
    }
    (void)mmgr_line_finish(&out);
    return s_classes;
}

// the word at place `index` of `classes`, `word=class`, into s_word
static const char *word_at(const char *classes, unsigned int index)
{
    const char *at = classes;
    for (unsigned int skip = 0u; (skip < index) && (cellul.chr(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL, .byte = '|'}) != NULL); skip += 1u)
    {
        at = cellul.chr(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL, .byte = '|'}) + 1;
    }
    const char *const bar = cellul.chr(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL, .byte = '|'});
    const char *const end = (bar != NULL) ? bar : (at + CLI_UNITY_LENGTH(at));
    VerbaLinea line = {s_word, sizeof(s_word), 0ull};
    // the word's end lies at or past its start
    mmgr_line_put_n(&line, at, (unsigned long long)(end - at));
    (void)mmgr_line_finish(&line);
    return s_word;
}

// whether `classes` contains the word `required`
static int contains_word(const char *classes, const char *required)
{
    for (unsigned int index = 0u; index < 64u; index += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = word_at(classes, index), .other = required, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the folds, each `first-last` joined by `|`
static const char *folds(void)
{
    VerbaLinea out = {s_classes, sizeof(s_classes), 0ull};
    for (unsigned int at = 0u; at < s_parsed.folds; at += 1u)
    {
        mmgr_line_put(&out, (at > 0u) ? "|" : "");
        mmgr_line_decimal(&out, s_parsed.fold_first[at], 1u);
        mmgr_line_ch(&out, '-');
        mmgr_line_decimal(&out, s_parsed.fold_last[at], 1u);
    }
    (void)mmgr_line_finish(&out);
    return s_classes;
}

// the spans that contain the position `line`:`col`, the smallest first, at most `most`, each `line:col-line:col` joined by `|`
static const char *spans_at(unsigned int line, unsigned int col, unsigned int most)
{
    static CliInspectSpan s_spans[64];
    static const char *const s_marks[4] = {"", ":", "-", ":"};
    const CliInspectPosition place = {line, col};
    const unsigned int count = EMBED_CALL(cli_inspect_ranges.ranges_at, CliInspectRangesCfg, .ranges_read = s_parsed.range, .count = s_parsed.ranges, .from = place, .to = place, .out_inspect_span_pointer = s_spans, .most = 64u);
    VerbaLinea out = {s_classes, sizeof(s_classes), 0ull};
    for (unsigned int at = 0u; (at < count) && (at < most); at += 1u)
    {
        const unsigned int positions[4] = {s_spans[at].from.line, s_spans[at].from.col, s_spans[at].to.line, s_spans[at].to.col};
        mmgr_line_put(&out, (at > 0u) ? "|" : "");
        for (unsigned int part = 0u; part < 4u; part += 1u)
        {
            mmgr_line_put(&out, s_marks[part]);
            mmgr_line_decimal(&out, positions[part], 1u);
        }
    }
    (void)mmgr_line_finish(&out);
    return s_classes;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_python_an_imports_words(void)
{
    parsed("python", s_python);
    TEST_ASSERT_EQUAL_STRING("import=t-keyword|os=t-module|.=t-delimiter|path=t-module|as=t-keyword|p=t-module", classes_of(s_python, 0u, 0));
}

void test_python_a_decorator(void)
{
    parsed("python", s_python);
    TEST_ASSERT_EQUAL_STRING("@cache=t-attribute", classes_of(s_python, 3u, 0));
}

void test_python_a_functions_name_and_its_parameters(void)
{
    parsed("python", s_python);
    TEST_ASSERT_EQUAL_STRING("area=t-function", word_at(classes_of(s_python, 4u, 0), 1u));
    TEST_ASSERT_EQUAL_STRING("width=t-parameter", word_at(classes_of(s_python, 4u, 0), 3u));
}

void test_python_a_docstring_and_a_comment(void)
{
    parsed("python", s_python);
    TEST_ASSERT_EQUAL_STRING("\"\"\"Area.\"\"\"=t-string|# the area=t-comment", classes_of(s_python, 5u, 0));
}

void test_python_a_name_is_classed_by_what_it_is(void)
{
    parsed("python", s_python);
    TEST_ASSERT_EQUAL_STRING("return=t-keyword|Shape=t-type|width=t-parameter|scale=t-function|height=t-parameter|by=t-parameter|p=|sep=t-property|self=t-predefined|size=t-property", classes_of(s_python, 6u, 1));
}

void test_python_the_regions_that_fold(void)
{
    parsed("python", s_python);
    TEST_ASSERT_EQUAL_STRING("4-6|9-10", folds());
}

void test_python_spans_grow_from_the_brackets_inside_to_the_blocks_body(void)
{
    parsed("python", s_python);
    TEST_ASSERT_EQUAL_STRING("6:17-6:22|6:16-6:23|6:4-6:59|5:4-6:59", spans_at(6u, 19u, 4u));
}

void test_javascript_a_comment(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_EQUAL_STRING("// the area=t-comment", classes_of(s_javascript, 0u, 0));
}

void test_javascript_a_functions_name_and_its_parameters(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_EQUAL_STRING("area=t-function", word_at(classes_of(s_javascript, 1u, 0), 2u));
    TEST_ASSERT_EQUAL_STRING("width=t-parameter", word_at(classes_of(s_javascript, 1u, 0), 4u));
}

void test_javascript_a_templates_parts_and_a_pattern(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_EQUAL_STRING("return=t-keyword|shape=|scale=t-function|height=t-parameter|`${=t-string|width=t-parameter|}px`=t-string|/a\\/b/=t-string|source=t-property", classes_of(s_javascript, 3u, 1));
}

void test_javascript_a_classs_name(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_TRUE(contains_word(classes_of(s_javascript, 6u, 0), "Shape=t-type"));
}

void test_javascript_a_method_and_its_parameter(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_TRUE(contains_word(classes_of(s_javascript, 7u, 0), "scale=t-function"));
    TEST_ASSERT_TRUE(contains_word(classes_of(s_javascript, 7u, 0), "by=t-parameter"));
}

void test_javascript_an_objects_key(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_TRUE(contains_word(classes_of(s_javascript, 8u, 0), "size=t-property"));
}

void test_javascript_the_regions_that_fold(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_EQUAL_STRING("1-4|6-10|7-9", folds());
}

void test_javascript_spans_are_inside_the_calls_brackets_then_with_them(void)
{
    parsed("javascript", s_javascript);
    TEST_ASSERT_EQUAL_STRING("3:21-3:27|3:20-3:28", spans_at(3u, 21u, 2u));
}
