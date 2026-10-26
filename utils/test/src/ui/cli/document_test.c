// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_test.c: the stored documents' check: a text is split as the page splits a file; edits are written from the
// last to the first as the page writes them; the marks show whether the commit contains the file and carry each
// change's lines as it contained them; a text is stored until it is let go; find gives each match of
// text sought with case and without; whole words beside text outside ASCII are the page's; a pattern is the page's own
// and text sought finds across a line's units; a stored text's joined lines follow its edits; and a held text is
// colored from its view out, each line as the line above it leaves it, and again after an edit; and a paged text is
// colored the same way through the pages, only the runs of the lines the page holds handed on
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/cli/src/editor/highlight.h"
#include "../../../../../ui/cli/src/editor/pages.h"
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
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (!cli_document_find_in(lines, &sought, most, &out, &err_msg))
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

// the matches of `query` in the text stored under `key`, checked against `wanted`
static void document_stored_found(const char *key, const char *query, const char *wanted, const char *what)
{
    const CliDocSought sought = document_sought(query, 0, 1, 0);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)cli_document_find(key, &sought, 100ull, &out, &err_msg);
    cli_check_text(s_out, out.at, wanted, what);
}

// edits are written from the last to the first as the page writes them
static void document_edits(void)
{
    CliShape shape;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)cli_document_store("edits", "first line\nsecond line\nthird", 28ull, &shape);
    static const CliEdit s_first[2] = {{{0u, 0u}, {0u, 5u}, "1st", 3u}, {{1u, 7u}, {2u, 2u}, "joined\nand ", 11u}};
    static const CliEdit s_second[1] = {{{2u, 0u}, {2u, 0u}, "", 0u}};
    const CliEdit *const changes[2] = {s_first, s_second};
    const unsigned int counts[2] = {2u, 1u};
    cli_check(cli_document_edit("edits", changes, counts, 2u, &shape, &err_msg) && (shape.lines == 3ull) &&
                  (shape.units == 8ull + 13ull + 7ull),
              "edits are written, the text's shape after them");
    document_stored_found("edits", "1st line", "[[0,0,8]]", "the first line written over");
    document_stored_found("edits", "second joined", "[[1,0,13]]", "lines joined");
    document_stored_found("edits", "and ird", "[[2,0,7]]", "and the last line's start");
    // the census stored with the text is of the units' bytes as the edits leave them: 28 units of ASCII, each a letter
    // low and a zero high, four of them `n`
    CliCensus census;
    cli_check(cli_document_census("edits", &census) && (census.total == 56ull) && (census.occurrences[0] == 28ull) &&
                  (census.occurrences['n'] == 4ull) && (census.occurrences['1'] == 1ull),
              "the census stored with the text counts its units' bytes as the edits leave them");
    static const CliEdit s_past[1] = {{{9u, 0u}, {9u, 0u}, "x", 1u}};
    const CliEdit *const past[1] = {s_past};
    const unsigned int one[1] = {1u};
    cli_check(!cli_document_edit("edits", past, one, 1u, &shape, &err_msg), "an edit past the text is refused");
    cli_check(!cli_document_shape("edits", &shape), "and the text is let go");
}

// the marks of the text under `key`, written compact into s_out: 1, or 0 where it gives none
static int document_marks(const char *key, int given, const char *head)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    const int marked = cli_document_marks(
        key, given, head, (head != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = head, .cap = CLI_CELLS_POOL}) : 0ull,
        &out);
    (void)mmgr_line_finish(&out);
    return marked;
}

// the marks show whether the commit contains the file and carry each change's lines as it contained them
static void document_marked(void)
{
    CliShape shape;
    (void)cli_document_store("marks", "one\nTWO\nthree", 13ull, &shape);
    cli_check(!document_marks("marks", 0, NULL), "no commit's text given yet");
    cli_check(document_marks("marks", 1, "one\ntwo\nthree") &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"head\":true", .other_cap = CLI_CELLS_POOL}) !=
                   NULL) &&
                  cli_document_has_head("marks"),
              "the commit contains the file");
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"changed\":[1]", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "\"thenLines\":[\"two\"]",
                                            .other_cap = CLI_CELLS_POOL}) != NULL),
        "and its change carries the lines it contained");
    cli_check(document_marks("marks", 0, NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"changed\":[1]", .other_cap = CLI_CELLS_POOL}) !=
                   NULL),
              "stored beside the text");
    cli_check(document_marks("marks", 1, NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_out, .other = "{\"head\":false,\"changes\":null}", .cap = CLI_CELLS_POOL})),
              "a commit that contains no such file");
    cli_document_release("marks");
}

// a grammar whose comments run over lines: `""` is its name 0, `comment` 1 and `identifier` 2
static const char s_branch_grammar[] =
    "{\"tokenizer\": {\"root\": [[\"\\\\/\\\\*\", \"comment\", \"@comment\"], [\"[a-z]+\", \"identifier\"], "
    "[\"\\\\s+\", \"\"]], \"comment\": [[\"\\\\*\\\\/\", \"comment\", \"@pop\"], [\"[^*]+\", \"comment\"], "
    "[\"\\\\*\", \"comment\"]]}}";

// the lines of the branch checked, a comment opened on the first and closed on the one before the last, and the runs
// each line was handed last, compact, and the changes the last colors handed on were worked out at
#define DOCUMENT_BRANCH_LINES 401u
static char s_branch[4u * DOCUMENT_BRANCH_LINES];
static char s_runs_of[DOCUMENT_BRANCH_LINES][32];
static unsigned long long s_seq;
static char s_folder[CLI_PATH_MAX_LEN];

// the branch's text written into s_branch: its bytes
static unsigned long long document_branch_text(void)
{
    VerbaLinea text = {s_branch, sizeof(s_branch), 0ull};
    mmgr_line_put(&text, "/*\n");
    for (unsigned int line = 1u; line < (DOCUMENT_BRANCH_LINES - 2u); line += 1u)
    {
        mmgr_line_put(&text, "b\n");
    }
    mmgr_line_put(&text, "*/\nc");
    return text.at;
}

// whether a line was handed `runs` last
static int document_handed(unsigned int line, const char *runs)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[line], .other = runs, .cap = CLI_CELLS_POOL});
}

// the lines 349 to 351 of the branch `key` handed for a call's signature, inside the comment once the walk has reached
// them: the lines joined as a JSON string, and the state the first is entered in, the comment's and not root
static void document_signature_lines(const char *key, const char *what)
{
    char state[CLI_NAME_MAX_LEN];
    VerbaLinea text = {s_out, sizeof(s_out), 0ull};
    const int written = cli_document_text_written(key, 349ull, 351ull, &text, state, sizeof(state));
    (void)mmgr_line_finish(&text);
    cli_check(written && cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "\"b\\nb\\nb\"", .cap = CLI_CELLS_POOL}) &&
                  (state[0] != '\0') && !cellul.eq(&(CatenaFinitaCfg){.src = state, .other = "root", .cap = CLI_CELLS_POOL}),
              what);
}

// the number the JSON node `node` writes, 0 where it writes none
static unsigned long long document_number(const CliJson *json, unsigned int node)
{
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; cli_json_is(json, node, CFG_JSON_NUMBER) && (at < json->node[node].length);
         at += 1ull)
    {
        value = (value * 10ull) + (unsigned long long)(json->text[json->node[node].text + at] - '0');
    }
    return value;
}

// a branch's colors, `[{"from", "runs"}]` at `colors` in `json`, each line's runs kept as the last it was handed
static void document_colors_read(const CliJson *json, unsigned int colors)
{
    const unsigned int first = cli_json_is(json, colors, CFG_JSON_ARRAY) ? json->node[colors].first : CLI_JSON_NONE;
    for (unsigned int range = first; range != CLI_JSON_NONE; range = json->node[range].next)
    {
        unsigned long long line = document_number(json, cli_json_member(json, range, "from"));
        const unsigned int runs = cli_json_member(json, range, "runs");
        const unsigned int run = cli_json_is(json, runs, CFG_JSON_ARRAY) ? json->node[runs].first : CLI_JSON_NONE;
        for (unsigned int one = run; (one != CLI_JSON_NONE) && (line < DOCUMENT_BRANCH_LINES);
             one = json->node[one].next)
        {
            VerbaLinea out = {s_runs_of[line], sizeof(s_runs_of[line]), 0ull};
            cli_json_write(json, one, &out, 0u, 0);
            (void)mmgr_line_finish(&out);
            line += 1ull;
        }
    }
}

// colors handed on as the walk works them out, `{"key", "seq", "colors"}`, read into the runs each line was handed last
static void document_colors_handed(const char *body, unsigned long long size)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(body, size, &root);
    s_seq = document_number(json, cli_json_member(json, root, "seq"));
    document_colors_read(json, cli_json_member(json, root, "colors"));
}

// a branch's lines viewed far from its first: colored at once from root as far above as a lead reaches, the comment
// above them unseen, and the walk out from them coloring every line as the line above it leaves it; an edit taking the
// comment out colors every line after it again
static void document_branch(void)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_branch_grammar, sizeof(s_branch_grammar) - 1u, &root);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_highlight_store("branch", json, root, &err_msg), "the branch's grammar is stored");
    const unsigned long long size = document_branch_text();
    cli_document_colors_to(document_colors_handed, 0);
    CliShape shape;
    (void)cli_document_store("branch", s_branch, size, &shape);
    cli_check(cli_document_colored_by("branch", "branch", 1), "a held text is colored by a grammar");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_document_viewed("branch", 350ull, 360ull, &out), "its view colored");
    json = cli_check_parsed(s_out, out.at, &root);
    document_colors_read(json, root);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[350], .other = "[0,2]", .cap = CLI_CELLS_POOL}) &&
                  (s_runs_of[49][0] == '\0') && (s_runs_of[50][0] != '\0'),
              "the view colored at once from root a lead above it, the comment above unseen");
    cli_document_walked();
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[350], .other = "[0,1]", .cap = CLI_CELLS_POOL}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[0], .other = "[0,1]", .cap = CLI_CELLS_POOL}) &&
                  cellul.eq(&(CatenaFinitaCfg){
                      .src = s_runs_of[DOCUMENT_BRANCH_LINES - 1u], .other = "[0,2]", .cap = CLI_CELLS_POOL}),
              "the walk out to the first line colors the view again as the comment above it leaves it");
    document_signature_lines("branch", "the lines handed for a call's signature, with the state the first is entered in");
    static const CliEdit s_opened[1] = {{{0u, 0u}, {0u, 2u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_opened};
    const unsigned int counts[1] = {1u};
    (void)cli_document_edit("branch", changes, counts, 1u, &shape, &err_msg);
    cli_document_walked();
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[0], .other = "[0,2]", .cap = CLI_CELLS_POOL}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[350], .other = "[0,2]", .cap = CLI_CELLS_POOL}) &&
                  (s_seq == 1ull),
              "an edit taking the comment out colors the lines after it again, handed on with the change it follows");
    cli_check(cli_document_held("branch", 100ull, 110ull) && cli_document_held("branch", 100ull, 120ull),
              "a text read whole names the lines the page holds");
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    cli_document_walked();
    cli_check((s_runs_of[99][0] == '\0') && (s_runs_of[105][0] == '\0') && document_handed(115u, "[0,2]") &&
                  (s_runs_of[120][0] == '\0'),
              "only the lines the page comes to hold are handed on again");
    VerbaLinea printed = {s_out, sizeof(s_out), 0ull};
    const int given = cli_document_runs_written("branch", 0ull, 2ull, &printed);
    (void)mmgr_line_finish(&printed);
    cli_check(given && cellul.eq(&(CatenaFinitaCfg){
                           .src = s_out, .other = "[{\"from\":0,\"runs\":[[0,2],[0,2],[0,2]]}]", .cap = CLI_CELLS_POOL}),
              "a print's lines colored and written whatever lines the page holds");
    cli_document_release("branch");
}

// a paged text's lines viewed far from its first: colored at once from root a lead above them, only the lines the page
// holds handed on; the walk colors every line as the line above it leaves it and hands on only those the page holds;
// the lines the page comes to hold handed on as the walk colors them again; and an edit the pages write colors the
// lines after it again
static void document_paged(void)
{
    const unsigned long long size = document_branch_text();
    char path[CLI_PATH_MAX_LEN];
    cli_full(s_folder, "paged.c", path);
    (void)query_file_written(path, s_branch, size);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("paged", path, &out, &err_msg), "the branch's file opens as a paged text");
    unsigned long long lines = 0ull;
    int whole = 0;
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((!cli_pages_counted("paged", &lines, &whole) || !whole) && (cli_clock_milliseconds() < until))
    {
        query_paused(1u);
    }
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    cli_check(cli_document_paged("paged", 350ull, 361ull) && cli_document_colored_by("paged", "branch", 1),
              "a paged text is held as a branch and colored by a grammar");
    out.at = 0ull;
    cli_check(cli_document_viewed("paged", 350ull, 360ull, &out), "its view colored");
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &root);
    document_colors_read(json, root);
    cli_check(document_handed(350u, "[0,2]") && (s_runs_of[349][0] == '\0') && (s_runs_of[361][0] == '\0'),
              "the view colored at once from root a lead above it, only the lines the page holds handed on");
    cli_document_walked();
    cli_check(document_handed(350u, "[0,1]") && (s_runs_of[0][0] == '\0'),
              "the walk colors the view again as the comment above it leaves it, and hands on no line the page lacks");
    cli_check(cli_document_held("paged", 0ull, DOCUMENT_BRANCH_LINES), "the page comes to hold every line");
    cli_document_walked();
    cli_check(document_handed(0u, "[0,1]") && document_handed(300u, "[0,1]") &&
                  document_handed(DOCUMENT_BRANCH_LINES - 1u, "[0,2]"),
              "the lines the page comes to hold handed on as the walk colors them again");
    document_signature_lines("paged", "a paged text's lines handed for a call's signature, read through the pages");
    static const CliEdit s_opened[1] = {{{0u, 0u}, {0u, 2u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_opened};
    const unsigned int counts[1] = {1u};
    out.at = 0ull;
    cli_check(cli_document_paged_edit("paged", changes, counts, 1u, &out, &err_msg), "the pages write the edit");
    cli_document_walked();
    cli_check(document_handed(0u, "[0,2]") && document_handed(350u, "[0,2]") && (s_seq == 1ull),
              "an edit the pages write colors the lines after it again, handed on with the change it follows");
    cli_document_release("paged");
    cli_pages_release("paged");
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
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    const CliDocSought most = document_sought("line", 0, 0, 0);
    (void)cli_document_find_in(lines, &most, 1ull, &out, &err_msg);
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
    (void)cli_cells_made(CLI_CELLS_POOL);
    const CliLines *const lines = document_lines("one\r\ntwo\nthree\r\r\n\xF0\x9F\x98\x80");
    cli_check((lines->lines == 4u) && (lines->length[2] == 6ull) && (lines->unit[lines->start[2] + 5u] == '\r') &&
                  (lines->length[3] == 2ull),
              "a text is split as the page splits a file, a char past the basic plane two units");
    document_edits();
    document_marked();
    CliShape shape;
    cli_check(cli_document_store("kept", "one\ntwo\nthree", 13ull, &shape) && (shape.lines == 3ull) &&
                  (shape.units == 11ull),
              "a text is stored, its shape given");
    cli_document_release("kept");
    cli_check(!cli_document_shape("kept", &shape), "until it is let go");
    document_find();
    (void)cli_document_store("joined", "one two\nthree", 13ull, &shape);
    document_stored_found("joined", "three", "[[1,0,5]]", "a stored text is found");
    static const CliEdit s_split[1] = {{{0u, 3u}, {1u, 0u}, "\n", 1u}};
    const CliEdit *const changes[1] = {s_split};
    const unsigned int counts[1] = {1u};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)cli_document_edit("joined", changes, counts, 1u, &shape, &err_msg);
    document_stored_found("joined", "three", "[[1,0,5]]", "and after an edit");
    document_stored_found("joined", "two", "[]", "the joined lines are made again after an edit");
    cli_document_release("joined");
    document_branch();
    cli_check_folder("document", s_folder);
    document_paged();
    return cli_checked();
}
