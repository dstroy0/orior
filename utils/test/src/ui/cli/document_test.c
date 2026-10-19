// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_test.c: the held documents' check: a text is split as the page splits a file; edits are written from the
// last to the first as the page writes them; the marks say whether the commit holds the file and carry each change's
// lines as it held them; a text given in parts is held once the last comes; find gives each match of text sought with
// case and without; whole words beside text outside ASCII are the page's; a pattern is the page's own and text sought
// finds across a line's units; and a held text's joined lines follow its edits
#include "../../../../../ui/cli/src/editor/document.h"
#include "cli_check.h"

static unsigned short s_units[1u << 12u];
static unsigned long long s_start[256];
static unsigned long long s_length[256];
static CliLines s_lines;
static char s_out[1u << 14u];

// `text` split into s_lines as the page splits a file
static const CliLines *document_lines(const char *text)
{
    s_lines = (CliLines){s_units, sizeof(s_units) / sizeof(s_units[0]), 0ull, s_start, s_length, 256u, 0u, 0};
    cli_document_lines_of(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &s_lines);
    return &s_lines;
}

// what is sought, as the find bar sets it
static CliDocSought document_sought(const char *query, int regex, int match_case, int word)
{
    const CliDocSought sought = {query, regex, match_case, word, 1};
    return sought;
}

// the matches of `query` in the lines, written compact into s_out: 1, or 0 where the search is the page's own
static int document_found(const CliLines *lines, CliDocSought sought, unsigned long long most, const char *wanted,
                          const char *what)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    if (!cli_document_find_in(lines, &sought, most, &out, &said))
    {
        cli_check(wanted == NULL, what);
        return 0;
    }
    if (wanted != NULL)
    {
        cli_check_text(s_out, out.at, wanted, what);
    }
    else
    {
        cli_check(0, what);
    }
    return 1;
}

// the matches of `query` in the text held under `key`, checked against `wanted`
static void document_held_found(const char *key, const char *query, const char *wanted, const char *what)
{
    const CliDocSought sought = document_sought(query, 0, 1, 0);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_document_find(key, &sought, 100ull, &out, &said);
    cli_check_text(s_out, out.at, wanted, what);
}

// edits are written from the last to the first as the page writes them
static void document_edits(void)
{
    CliShape shape;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_document_hold("edits", "first line\nsecond line\nthird", 28ull, &shape);
    static const CliEdit s_first[2] = {{{0u, 0u}, {0u, 5u}, "1st", 3u}, {{1u, 7u}, {2u, 2u}, "joined\nand ", 11u}};
    static const CliEdit s_second[1] = {{{2u, 0u}, {2u, 0u}, "", 0u}};
    const CliEdit *const changes[2] = {s_first, s_second};
    const unsigned int counts[2] = {2u, 1u};
    cli_check(cli_document_edit("edits", changes, counts, 2u, &shape, &said) && (shape.lines == 3ull) &&
                  (shape.units == 8ull + 13ull + 7ull),
              "edits are written, the text's shape after them");
    document_held_found("edits", "1st line", "[[0,0,8]]", "the first line written over");
    document_held_found("edits", "second joined", "[[1,0,13]]", "lines joined");
    document_held_found("edits", "and ird", "[[2,0,7]]", "and the last line's start");
    static const CliEdit s_past[1] = {{{9u, 0u}, {9u, 0u}, "x", 1u}};
    const CliEdit *const past[1] = {s_past};
    const unsigned int one[1] = {1u};
    cli_check(!cli_document_edit("edits", past, one, 1u, &shape, &said), "an edit past the text is refused");
    cli_check(!cli_document_shape("edits", &shape), "and the text is let go");
}

// the marks of the text under `key`, written compact into s_out: 1, or 0 where it gives none
static int document_marks(const char *key, int given, const char *head)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    const int marked = cli_document_marks(
        key, given, head, (head != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = head, .cap = CLI_CELLS_POOL}) : 0ull,
        &out);
    (void)cli_line_closed(&out);
    return marked;
}

// the marks say whether the commit holds the file and carry each change's lines as it held them
static void document_marked(void)
{
    CliShape shape;
    (void)cli_document_hold("marks", "one\nTWO\nthree", 13ull, &shape);
    cli_check(!document_marks("marks", 0, NULL), "no commit's text given yet");
    cli_check(document_marks("marks", 1, "one\ntwo\nthree") &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"head\":true", .other_cap = CLI_CELLS_POOL}) !=
                   NULL) &&
                  cli_document_has_head("marks"),
              "the commit holds the file");
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"changed\":[1]", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "\"thenLines\":[\"two\"]",
                                            .other_cap = CLI_CELLS_POOL}) != NULL),
        "and its change carries the lines it held");
    cli_check(document_marks("marks", 0, NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"changed\":[1]", .other_cap = CLI_CELLS_POOL}) !=
                   NULL),
              "kept beside the text");
    cli_check(document_marks("marks", 1, NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_out, .other = "{\"head\":false,\"changes\":null}", .cap = CLI_CELLS_POOL})),
              "a commit that holds no such file");
    cli_document_release("marks");
}

// find gives each match of text sought with case and without
static void document_find(void)
{
    const CliLines *lines = document_lines("Line one line\nnoline LINE\n\xC5\xBFtar KING");
    (void)document_found(lines, document_sought("line", 0, 1, 0), 100ull, "[[0,9,13],[1,2,6]]", "with case");
    (void)document_found(lines, document_sought("line", 0, 0, 0), 100ull, "[[0,0,4],[0,9,13],[1,2,6],[1,7,11]]",
                         "without case");
    (void)document_found(lines, document_sought("line", 0, 0, 1), 100ull, "[[0,0,4],[0,9,13],[1,7,11]]", "whole words");
    (void)document_found(lines, document_sought("star king", 0, 0, 0), 100ull, "[[2,0,9]]",
                         "the long s and the Kelvin sign fold");
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    const CliDocSought most = document_sought("line", 0, 0, 0);
    (void)cli_document_find_in(lines, &most, 1ull, &out, &said);
    unsigned int found = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &found);
    cli_check(json->node[found].count == 2u, "the line that reaches the most is searched whole");
    (void)document_found(lines, document_sought("\xC3\xA9", 0, 0, 0), 100ull, NULL,
                         "without case, text outside ASCII is the page's");
    lines = document_lines("a line\xD9\x8B here");
    (void)document_found(lines, document_sought("line", 0, 1, 1), 100ull, NULL,
                         "whole words beside text outside ASCII are the page's");
    lines = document_lines("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                           "ab");
    (void)document_found(lines, document_sought("x_\\d+", 1, 1, 0), 100ull, NULL, "a pattern is the page's own");
    (void)document_found(lines, document_sought("ab", 0, 1, 0), 100ull, "[[1,10,12]]", "cols in UTF-16 units");
    (void)document_found(lines, document_sought("X_", 0, 0, 0), 100ull, "[[0,0,2],[1,0,2]]", "without case");
    (void)document_found(lines, document_sought("10\nx", 0, 1, 0), 100ull, "[]", "nothing runs over a line's end");
}

int main(void)
{
    const CliLines *const lines = document_lines("one\r\ntwo\nthree\r\r\n\xF0\x9F\x98\x80");
    cli_check((lines->lines == 4u) && (lines->length[2] == 6ull) && (lines->unit[lines->start[2] + 5u] == '\r') &&
                  (lines->length[3] == 2ull),
              "a text is split as the page splits a file, a char past the basic plane two units");
    document_edits();
    document_marked();
    CliShape shape;
    cli_check(!cli_document_hold_part("parts", "one\ntw", 6ull, 1, 0, &shape), "a part is kept");
    cli_check(cli_document_hold_part("parts", "o\nthree", 7ull, 0, 1, &shape) && (shape.lines == 3ull) &&
                  (shape.units == 11ull),
              "and the text is held once the last comes");
    cli_document_release("parts");
    cli_check(!cli_document_shape("parts", &shape), "until it is let go");
    document_find();
    (void)cli_document_hold("joined", "one two\nthree", 13ull, &shape);
    document_held_found("joined", "three", "[[1,0,5]]", "a held text is found");
    static const CliEdit s_split[1] = {{{0u, 3u}, {1u, 0u}, "\n", 1u}};
    const CliEdit *const changes[1] = {s_split};
    const unsigned int counts[1] = {1u};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_document_edit("joined", changes, counts, 1u, &shape, &said);
    document_held_found("joined", "three", "[[1,0,5]]", "and after an edit");
    document_held_found("joined", "two", "[]", "the joined lines are made again after an edit");
    cli_document_release("joined");
    return cli_checked();
}
