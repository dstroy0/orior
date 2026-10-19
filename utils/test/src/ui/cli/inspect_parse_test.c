// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_parse_test.c: a file's parse: a Python file and a JavaScript file are colored, folded and spanned from their
// parse
#include "../../../../../ui/cli/src/inspect/inspect.h"
#include "cli_check.h"

static CliInspectRun s_runs[4096];
static unsigned int s_line_first[256];
static unsigned int s_line_runs[256];
static unsigned int s_fold_first[256];
static unsigned int s_fold_last[256];
static CliInspectSpan s_ranges[4096];
static CliParsed s_parsed;
static char s_classes[2048];

// `text` in `language` parsed into s_parsed: 1, or 0
static int parse_read(const char *language, const char *text)
{
    cli_parsed_begun(&s_parsed, s_runs, 4096u, s_line_first, s_line_runs, 256u, s_fold_first, s_fold_last, 256u,
                     s_ranges, 4096u);
    return cli_inspect_parse(language, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                             &s_parsed);
}

// the class the parse gives each word of line `line` of `text`, `word=class` joined by `|`, the delimiters and
// operators left out where `plain`; the text is ASCII, so a column is a byte
static const char *parse_classes(const char *text, unsigned int line, int plain)
{
    const char *start = text;
    for (unsigned int at = 0u;
         (at < line) && (cellul.chr(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .byte = '\n'}) != NULL);
         at += 1u)
    {
        start = cellul.chr(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .byte = '\n'}) + 1;
    }
    const char *const end = (cellul.chr(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .byte = '\n'}) != NULL)
                                ? cellul.chr(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL, .byte = '\n'})
                                : (start + cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL}));
    const unsigned long long length = (unsigned long long)(end - start);
    CliLine out = {s_classes, sizeof(s_classes), 0ull};
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
        const int skipped =
            plain && ((run->class_of == CLI_INSPECT_DELIMITER) || (run->class_of == CLI_INSPECT_OPERATOR));
        if ((from == to) || skipped)
        {
            continue;
        }
        cli_line_text(&out, (out.at > 0ull) ? "|" : "");
        cli_line_counted(&out, start + from, to - from);
        cli_line_character(&out, '=');
        cli_line_text(&out, g_cli_inspect_classes[run->class_of]);
    }
    (void)cli_line_closed(&out);
    return s_classes;
}

// the word at place `index` of `classes`, `word=class` as parse_classes writes it, into `out`, which holds `room`
static const char *parse_word(const char *classes, unsigned int index, char *out, unsigned long long room)
{
    const char *at = classes;
    for (unsigned int skip = 0u;
         (skip < index) && (cellul.chr(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL, .byte = '|'}) != NULL);
         skip += 1u)
    {
        at = cellul.chr(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL, .byte = '|'}) + 1;
    }
    const char *const end = (cellul.chr(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL, .byte = '|'}) != NULL)
                                ? cellul.chr(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL, .byte = '|'})
                                : (at + cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}));
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, at, (unsigned long long)(end - at));
    (void)cli_line_closed(&line);
    return out;
}

// the folds, each `first-last` joined by `|`
static const char *parse_folds(void)
{
    CliLine out = {s_classes, sizeof(s_classes), 0ull};
    for (unsigned int at = 0u; at < s_parsed.folds; at += 1u)
    {
        cli_line_text(&out, (at > 0u) ? "|" : "");
        cli_line_decimal(&out, s_parsed.fold_first[at], 1u);
        cli_line_character(&out, '-');
        cli_line_decimal(&out, s_parsed.fold_last[at], 1u);
    }
    (void)cli_line_closed(&out);
    return s_classes;
}

// the spans that hold the place `line`:`col`, the least first, at most `most`, each `line:col-line:col` joined by `|`
static const char *parse_spans(unsigned int line, unsigned int col, unsigned int most)
{
    static CliInspectSpan s_spans[64];
    const CliInspectPlace place = {line, col};
    const unsigned int count = cli_inspect_ranges_at(s_parsed.range, s_parsed.ranges, place, place, s_spans, 64u);
    CliLine out = {s_classes, sizeof(s_classes), 0ull};
    for (unsigned int at = 0u; (at < count) && (at < most); at += 1u)
    {
        const unsigned int places[4] = {s_spans[at].from.line, s_spans[at].from.col, s_spans[at].to.line,
                                        s_spans[at].to.col};
        static const char *const s_marks[4] = {"", ":", "-", ":"};
        cli_line_text(&out, (at > 0u) ? "|" : "");
        for (unsigned int part = 0u; part < 4u; part += 1u)
        {
            cli_line_text(&out, s_marks[part]);
            cli_line_decimal(&out, places[part], 1u);
        }
    }
    (void)cli_line_closed(&out);
    return s_classes;
}

// a Python file is colored, folded and spanned from its parse
static void parse_python(void)
{
    static const char s_text[] =
        "import os.path as p\n\n\n@cache\ndef area(width, height=2):\n    \"\"\"Area.\"\"\"  # "
        "the area\n    return Shape(width).scale(height, by=p.sep) + self.size\n\n\nclass "
        "Shape:\n    pass\n";
    char word[256];
    cli_check(parse_read("python", s_text), "a Python file is parsed");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = parse_classes(s_text, 0u, 0),
                  .other = "import=t-keyword|os=t-module|.=t-delimiter|path=t-module|as=t-keyword|p=t-module",
                  .cap = CLI_CELLS_POOL}),
              "an import's words");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = parse_classes(s_text, 3u, 0), .other = "@cache=t-attribute", .cap = CLI_CELLS_POOL}),
              "a decorator");
    cli_check((cellul.eq(&(CatenaFinitaCfg){.src = parse_word(parse_classes(s_text, 4u, 0), 1u, word, sizeof(word)),
                                            .other = "area=t-function",
                                            .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = parse_word(parse_classes(s_text, 4u, 0), 3u, word, sizeof(word)),
                                                .other = "width=t-parameter",
                                                .cap = CLI_CELLS_POOL})),
              "a function's name and its parameters");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = parse_classes(s_text, 5u, 0),
                                           .other = "\"\"\"Area.\"\"\"=t-string|# the area=t-comment",
                                           .cap = CLI_CELLS_POOL}),
              "a docstring and a comment");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = parse_classes(s_text, 6u, 1),
                  .other = "return=t-keyword|Shape=t-type|width=t-parameter|scale=t-function|height=t-parameter|by=t-"
                           "parameter|p=|sep=t-property|self=t-predefined|size=t-property",
                  .cap = CLI_CELLS_POOL}),
              "a name by what it is");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = parse_folds(), .other = "4-6|9-10", .cap = CLI_CELLS_POOL}),
              "the regions that fold");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = parse_spans(6u, 19u, 4u),
                                           .other = "6:17-6:22|6:16-6:23|6:4-6:59|5:4-6:59",
                                           .cap = CLI_CELLS_POOL}),
              "the inside of the brackets, the brackets, the statement, the block's body");
}

// whether `classes` holds the word `wanted`
static int parse_holds(const char *classes, const char *wanted)
{
    char word[256];
    for (unsigned int index = 0u; index < 64u; index += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = parse_word(classes, index, word, sizeof(word)), .other = wanted, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// a JavaScript file is colored, folded and spanned from its parse
static void parse_javascript(void)
{
    static const char s_text[] =
        "// the area\nexport function area(width, height = 2) {\n  const shape = new Shape(width);\n  return "
        "shape.scale(height) + `${width}px` + /a\\/b/.source;\n}\n\nclass Shape {\n  scale(by) {\n    return { by, "
        "size: 1 };\n  }\n}\n";
    char word[256];
    cli_check(parse_read("javascript", s_text), "a JavaScript file is parsed");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = parse_classes(s_text, 0u, 0), .other = "// the area=t-comment", .cap = CLI_CELLS_POOL}),
              "a comment");
    cli_check((cellul.eq(&(CatenaFinitaCfg){.src = parse_word(parse_classes(s_text, 1u, 0), 2u, word, sizeof(word)),
                                            .other = "area=t-function",
                                            .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = parse_word(parse_classes(s_text, 1u, 0), 4u, word, sizeof(word)),
                                                .other = "width=t-parameter",
                                                .cap = CLI_CELLS_POOL})),
              "a function's name and its parameters");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = parse_classes(s_text, 3u, 1),
            .other =
                "return=t-keyword|shape=|scale=t-function|height=t-parameter|`${=t-string|width=t-parameter|}px`=t-"
                "string|/a\\/b/=t-string|source=t-property",
            .cap = CLI_CELLS_POOL}),
        "a template's parts and a pattern");
    cli_check(parse_holds(parse_classes(s_text, 6u, 0), "Shape=t-type"), "a class's name");
    cli_check(parse_holds(parse_classes(s_text, 7u, 0), "scale=t-function") &&
                  parse_holds(parse_classes(s_text, 7u, 0), "by=t-parameter"),
              "a method and its parameter");
    cli_check(parse_holds(parse_classes(s_text, 8u, 0), "size=t-property"), "an object's key");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = parse_folds(), .other = "1-4|6-10|7-9", .cap = CLI_CELLS_POOL}),
              "the regions that fold");
    cli_check(cellul.starts(&(CatenaFinitaCfg){
                  .src = parse_spans(3u, 21u, 2u), .other = "3:21-3:27|3:20-3:28", .cap = CLI_CELLS_POOL}),
              "inside the call's brackets, then with them");
}

int main(void)
{
    parse_python();
    parse_javascript();
    return cli_checked();
}
