// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_test.c: the stored documents' check: a text is split as the page splits a file; edits are written from the
// last to the first as the page writes them; the marks show whether the commit contains the file and carry each
// change's lines as it contained them; a text is stored until it is let go; find gives each match of text sought with
// case and without, a pattern's by the regular expressions the colorer reads, and a small text's while the pool holds
// little room; a stored text's joined lines follow its edits; a branch is colored from its view out, each line as the
// line above it leaves it, and again after an edit; its folds, its uses and its brackets read off its grammar's roles;
// texts colored at once by walkers on threads of their own, views, prints and an edit beside them, as one walked alone
// is; and a paged text is colored the same way through the pages, only the runs of the lines the page mirrors handed on
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/cli/src/editor/highlight.h"
#include "../../../../../ui/cli/src/editor/pages.h"
#include "../../../../../ui/cli/src/plugins/plugins.h"
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
    const CliDocSought sought = {query, regex, match_case, word};
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

// the cells the pool is taken in for a search in little room, and the most of them
#define DOCUMENT_TAKEN_CELL (8ull << 20u)
#define DOCUMENT_TAKEN_MOST 64u

// a small text's search takes cells sized to it: the pool taken in cells of DOCUMENT_TAKEN_CELL until it gives no
// more, one of them given back, the search still answers
static void document_found_in_little_room(void)
{
    static void *s_taken[DOCUMENT_TAKEN_MOST];
    unsigned int taken = 0u;
    while (taken < DOCUMENT_TAKEN_MOST)
    {
        s_taken[taken] = g_cli_cells->persistent_buf_alloc(DOCUMENT_TAKEN_CELL, &g_checks_cells);
        if (s_taken[taken] == NULL)
        {
            break;
        }
        taken += 1u;
    }
    if (taken != 0u)
    {
        taken -= 1u;
        g_cli_cells->persistent_buf_release(s_taken[taken], &g_checks_cells);
    }
    CliShape shape;
    (void)cli_document_store("little", "one line\ntwo lines", 18ull, &shape);
    document_stored_found("little", "line", "[[0,4,8],[1,4,8]]",
                          "a small text is searched while the pool holds little room");
    cli_document_release("little");
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        g_cli_cells->persistent_buf_release(s_taken[at], &g_checks_cells);
    }
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

// an edit of a large text is written in the text's own room: edits that each put in a letter raise the cellblock's
// peak by less than a room of the text, though the text's room is near the size of all the cells taken before them
static void document_edits_in_place(void)
{
    // 100000 lines of 12 letters, in a cell given back once the text is stored
    const unsigned long long lines = 100000ull;
    const unsigned long long size = lines * 13ull;
    char *const text = g_cli_cells->persistent_buf_alloc(size, &g_checks_cells);
    cli_check(text != NULL, "the large text's cell is taken");
    if (text == NULL)
    {
        return;
    }
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        text[at] = ((at % 13ull) == 12ull) ? '\n' : 'a';
    }
    CliShape shape;
    (void)cli_document_store("large", text, size, &shape);
    g_cli_cells->persistent_buf_release(text, &g_checks_cells);
    unsigned long long pool = 0ull;
    unsigned long long before = 0ull;
    unsigned long long temporary = 0ull;
    cli_cells_report(&pool, &before, &temporary);
    static const CliEdit s_letter[1] = {{{50000u, 0u}, {50000u, 0u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_letter};
    const unsigned int counts[1] = {1u};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    int edited = 1;
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        edited = edited && cli_document_edit("large", changes, counts, 1u, &shape, &err_msg);
    }
    unsigned long long after = 0ull;
    cli_cells_report(&pool, &after, &temporary);
    cli_check(edited && (shape.lines == lines + 1ull) && (shape.units == (lines * 12ull) + 20ull),
              "a large text takes letters put in one edit at a time");
    document_stored_found("large", "xxxxxxxxxxxxxxxxxxxxa", "[[50000,0,21]]", "each letter where its edit puts it");
    cli_check((before == 0ull) || ((after - before) < (size * 2ull)),
              "and no edit takes a room of the text: the cellblock's peak rises by less than one");
    cli_document_release("large");
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
static char s_runs_of[DOCUMENT_BRANCH_LINES][64];
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
                  (state[0] != '\0') &&
                  !cellul.eq(&(CatenaFinitaCfg){.src = state, .other = "root", .cap = CLI_CELLS_POOL}),
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

// the folds the walk handed on last, compact
static char s_folds[1024];

// colors handed on as the walk works them out, `{"key", "seq", "colors"}`, read into the runs each line was handed
// last, and folds handed on, `{"key", "seq", "folds"}`, kept as the folds handed on last
static void document_colors_handed(const char *body, unsigned long long size)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(body, size, &root);
    s_seq = document_number(json, cli_json_member(json, root, "seq"));
    document_colors_read(json, cli_json_member(json, root, "colors"));
    const unsigned int folds = cli_json_member(json, root, "folds");
    if (folds != CLI_JSON_NONE)
    {
        VerbaLinea out = {s_folds, sizeof(s_folds), 0ull};
        cli_json_write(json, folds, &out, 0u, 0);
        (void)mmgr_line_finish(&out);
    }
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
    cli_check(cli_document_colored_by("branch", "branch", 1), "a branch is colored by a grammar");
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
    document_signature_lines("branch",
                             "the lines handed for a call's signature, with the state the first is entered in");
    static const CliEdit s_opened[1] = {{{0u, 0u}, {0u, 2u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_opened};
    const unsigned int counts[1] = {1u};
    (void)cli_document_edit("branch", changes, counts, 1u, &shape, &err_msg);
    cli_document_walked();
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[0], .other = "[0,2]", .cap = CLI_CELLS_POOL}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[350], .other = "[0,2]", .cap = CLI_CELLS_POOL}) &&
                  (s_seq == 1ull),
              "an edit taking the comment out colors the lines after it again, handed on with the change it follows");
    cli_check(cli_document_held("branch", 100ull, 110ull), "a text read whole names the lines the page holds");
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    cli_check(cli_document_held("branch", 100ull, 120ull) && (s_runs_of[99][0] == '\0') &&
                  (s_runs_of[105][0] == '\0') && document_handed(115u, "[0,2]") && (s_runs_of[120][0] == '\0'),
              "only the lines the page comes to hold are handed on again, at once with the runs the walk kept");
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    cli_document_walked();
    cli_check(s_runs_of[115][0] == '\0', "the walk colors none of them again");
    VerbaLinea printed = {s_out, sizeof(s_out), 0ull};
    const int given = cli_document_runs_written("branch", 0ull, 2ull, 0, &printed);
    (void)mmgr_line_finish(&printed);
    cli_check(given && cellul.eq(&(CatenaFinitaCfg){
                           .src = s_out,
                           .other = "[{\"from\":0,\"runs\":[[0,2],[0,2],[0,2]]}]",
                           .cap = CLI_CELLS_POOL}),
              "a print's lines colored and written whatever lines the page holds");
    cli_document_release("branch");
}

// lines' runs handed as far as a view draws them, each line colored whole from left to right and the next entered in
// the state it ends in, the walk coloring neither again; and a view drawing farther has the lines the page mirrors
// colored again and handed as far
static void document_columns(void)
{
    static const char s_text[] = "abc def ghi\nxy";
    CliShape shape;
    (void)cli_document_store("columns", s_text, sizeof(s_text) - 1u, &shape);
    cli_check(cli_document_colored_by("columns", "branch", 1), "a text of a long line and a short one is colored");
    cli_check(cli_document_columns("columns", 5ull), "the view draws five letters of each line");
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    const int read = cli_document_runs_written("columns", 0ull, 1ull, 1, &out);
    (void)mmgr_line_finish(&out);
    cli_check(read && cellul.eq(&(CatenaFinitaCfg){
                          .src = s_out,
                          .other = "[{\"from\":0,\"runs\":[[0,2,3,0,4,2],[0,2]]}]",
                          .cap = CLI_CELLS_POOL}),
              "the long line's runs handed as far as its first five letters, the short one's whole");
    cli_document_walked();
    cli_check((s_runs_of[0][0] == '\0') && (s_runs_of[1][0] == '\0'), "the walk colors neither line again");
    cli_check(cli_document_columns("columns", 0ull), "the view comes to draw every letter, its lines wrapped");
    cli_document_walked();
    cli_check(document_handed(0u, "[0,2,3,0,4,2,7,0,8,2]") && document_handed(1u, "[0,2]"),
              "the lines the page mirrors colored again whole and handed on");
    cli_document_release("columns");
    static const char s_opened[] = "abcdef /*\nxy";
    (void)cli_document_store("opened", s_opened, sizeof(s_opened) - 1u, &shape);
    (void)cli_document_colored_by("opened", "branch", 1);
    (void)cli_document_columns("opened", 5ull);
    out.at = 0ull;
    const int opened = cli_document_runs_written("opened", 0ull, 1ull, 1, &out);
    (void)mmgr_line_finish(&out);
    cli_check(opened && cellul.eq(&(CatenaFinitaCfg){
                            .src = s_out, .other = "[{\"from\":0,\"runs\":[[0,2],[0,1]]}]", .cap = CLI_CELLS_POOL}),
              "a comment a line opens past the letters the view draws carried into the line after it");
    cli_document_release("opened");
    static char s_long[21007];
    memor.set(&(MemoriaCfg){.dst = s_long, .val = 'a', .bytes = 21000u});
    memor.cpy(&(MemoriaCfg){.dst = s_long + 21000u, .src = " /*\nxy", .bytes = 7u});
    (void)cli_document_store("long", s_long, sizeof(s_long) - 1u, &shape);
    (void)cli_document_colored_by("long", "branch", 1);
    out.at = 0ull;
    const int read_long = cli_document_runs_written("long", 0ull, 1ull, 0, &out);
    (void)mmgr_line_finish(&out);
    cli_check(read_long && cellul.eq(&(CatenaFinitaCfg){
                               .src = s_out,
                               .other = "[{\"from\":0,\"runs\":[[0,2,21000,0,21001,1],[0,1]]}]",
                               .cap = CLI_CELLS_POOL}),
              "a line of more than 20000 letters colored to its end, the comment it opens there carried on");
    cli_document_release("long");
}

// a grammar whose brackets open and close regions and whose `;` ends a statement
static const char s_bracket_grammar[] =
    "{\"tokenizer\": {\"root\": [[\"\\\\{\", \"delimiter.bracket.region.open\"], "
    "[\"\\\\}\", \"delimiter.bracket.region.close\"], [\";\", \"delimiter.statement\"], [\"[a-z]+\", \"identifier\"], "
    "[\"\\\\s+\", \"\"]]}}";

// a grammar whose `:` at a line's end opens a region its indent runs, a mark of no letters closing it
static const char s_indent_grammar[] =
    "{\"tokenizer\": {\"root\": [[\":(?=\\\\s*$)\", \"operator.region.open\", \"@block\"], "
    "[\"[a-z]+\", \"identifier\"], [\"\\\\s+\", \"\"]], "
    "\"block\": [[\":(?=\\\\s*$)\", \"operator.region.open\", \"@block\"], [\"[a-z]+\", \"identifier\"], "
    "[\"\\\\s+\", \"\"]]}, \"indent\": {\"states\": {\"block\": \"operator.region.close\"}, \"tab\": 8, "
    "\"skip\": \"^\\\\s*$\", \"begins\": [\"root\", \"block\"]}}";

// the grammar `grammar` stored under `key` and the text `text` colored by it whole, its folds handed on checked
// `wanted`
static void document_folds_of(const char *key, const char *grammar, const char *text, const char *wanted,
                              const char *what)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json =
        cli_check_parsed(grammar, cellul.len(&(CatenaFinitaCfg){.src = grammar, .cap = CLI_CELLS_POOL}), &root);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)cli_highlight_store(key, json, root, &err_msg);
    CliShape shape;
    (void)cli_document_store(key, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &shape);
    (void)cli_document_colored_by(key, key, 1);
    s_folds[0] = '\0';
    cli_document_walked();
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_folds, .other = wanted, .cap = CLI_CELLS_POOL}), what);
}

// folds read off the regions a grammar's roles open and close as the walk colors the text: a bracket's region from its
// line to the line before its close where a leaf stands before the close or after it, and to its close, shown, where
// the close's line holds closes alone; a pair on one line no fold; a mark's region to the last line before it that
// holds a leaf, or to its own line where a leaf stands before it; an edit hands the folds on again moved
static void document_folds(void)
{
    document_folds_of("brackets", s_bracket_grammar, "f {\n  a;\n  g {\n    b;\n  }\n} x\nh { c; }\nk {\n  d;\n};",
                      "[[0,4,0],[2,4,1],[7,9,1]]", "a bracket's folds read off its regions");
    static const CliEdit s_above[1] = {{{0u, 0u}, {0u, 0u}, "y;\n", 3u}};
    const CliEdit *const changes[1] = {s_above};
    const unsigned int counts[1] = {1u};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliShape shape;
    s_folds[0] = '\0';
    (void)cli_document_edit("brackets", changes, counts, 1u, &shape, &err_msg);
    cli_document_walked();
    static const char s_moved[] = "[[1,5,0],[3,5,1],[8,10,1]]";
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_folds, .other = s_moved, .cap = CLI_CELLS_POOL}),
              "a line put above them hands the folds on again, moved");
    s_folds[0] = '\0';
    cli_document_walked();
    cli_check(s_folds[0] == '\0', "folds whose regions did not move go on no second time");
    cli_document_release("brackets");
    document_folds_of("indents", s_indent_grammar, "def:\n    a\n\n    b\n\nc\ne:\n    f", "[[0,3,0],[6,7,0]]",
                      "a mark's folds end on the last line that holds a leaf");
    cli_document_release("indents");
}

// a plain text's folds read off its plugin's grammar: each line that holds a letter opens a region its indent runs,
// every space and tab of the indent counted one: a line folds the deeper lines under it, the blank lines after
// them left out, and a line with none under it folds nothing
static void document_plain_folds(void)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_plugins_grammar_stored("plaintext", key, &err_msg), "the plain text plugin's grammar is stored");
    CliShape shape;
    static const char s_plain[] = "a\n\tb\n\t\tc\n\n\td\ne\n  f\n  g";
    (void)cli_document_store("plain", s_plain, sizeof(s_plain) - 1u, &shape);
    (void)cli_document_colored_by("plain", key, 1);
    s_folds[0] = '\0';
    cli_document_walked();
    cli_check_text(s_folds, cellul.len(&(CatenaFinitaCfg){.src = s_folds, .cap = CLI_CELLS_POOL}),
                   "[[0,4,0],[1,2,0],[5,7,0]]",
                   "a plain text folds each line over the deeper lines under it, tabs and spaces each one");
    cli_document_release("plain");
    cli_check(cli_plugins_grammar_stored("javascript", key, &err_msg), "the JavaScript plugin's grammar is stored");
    static const char s_script[] = "function f() {\n  return 1;\n}\n";
    (void)cli_document_store("script", s_script, sizeof(s_script) - 1u, &shape);
    (void)cli_document_colored_by("script", key, 1);
    s_folds[0] = '\0';
    cli_document_walked();
    cli_check_text(s_folds, cellul.len(&(CatenaFinitaCfg){.src = s_folds, .cap = CLI_CELLS_POOL}), "[[0,2,1]]",
                   "a JavaScript function folds its body, its closing brace shown");
    cli_document_release("script");
    cli_check(cli_plugins_grammar_stored("tsv", key, &err_msg), "the tables plugin's grammar is stored");
    static const char s_table[] = "a\tx\n\tb\ty\tz\n\t\tc\td\ne\tf\n";
    (void)cli_document_store("table", s_table, sizeof(s_table) - 1u, &shape);
    (void)cli_document_colored_by("table", key, 1);
    s_folds[0] = '\0';
    cli_document_walked();
    cli_check_text(s_folds, cellul.len(&(CatenaFinitaCfg){.src = s_folds, .cap = CLI_CELLS_POOL}), "[[0,2,0],[1,2,0]]",
                   "a table's row folds the rows indented under it, its columns colored between");
    cli_document_release("table");
}

// the place among the names of the grammar under `key` of the name `name`, CLI_JSON_NONE where it takes none
static unsigned int document_name_place(const char *key, const char *name)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)cli_highlight_names_written(key, &out);
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &root);
    unsigned int place = 0u;
    for (unsigned int one = json->node[root].first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, one, &size);
        if ((text != NULL) && (size == cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL})) &&
            (memor.cmp(&(MemoriaCfg){.src = text, .other = name, .bytes = size}) == 0))
        {
            return place;
        }
        place += 1u;
    }
    return CLI_JSON_NONE;
}

// whether the line `line` of the colored text under `key` holds a run at `col` named `name` among the names of the
// grammar under `grammar`
static int document_run_named(const char *key, const char *grammar, unsigned int line, unsigned int col,
                              const char *name)
{
    const unsigned int place = document_name_place(grammar, name);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)cli_document_runs_written(key, line, line, 0, &out);
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &root);
    const unsigned int range = json->node[root].first;
    const unsigned int runs = (range != CLI_JSON_NONE) ? cli_json_member(json, range, "runs") : CLI_JSON_NONE;
    const unsigned int flat = (runs != CLI_JSON_NONE) ? json->node[runs].first : CLI_JSON_NONE;
    unsigned int at = (flat != CLI_JSON_NONE) ? json->node[flat].first : CLI_JSON_NONE;
    while ((at != CLI_JSON_NONE) && (json->node[at].next != CLI_JSON_NONE))
    {
        const unsigned int named = json->node[at].next;
        if ((document_number(json, at) == col) && (document_number(json, named) == place))
        {
            return 1;
        }
        at = json->node[named].next;
    }
    return 0;
}

// a C text's uses named as the walk reads its scopes off the grammar's roles: a use of a parameter or a local of the
// function it stands in takes its declaration's name, a name reached through the operator before it keeps its own,
// and a use outside the function's region, its names let go there, keeps its own
static void document_uses(void)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_plugins_grammar_stored("c", key, &err_msg), "the C plugin's grammar is stored");
    static const char s_code[] = "int f(int count)\n{\n    int total = 0;\n    total = count + other;\n"
                                 "    view->total = 1;\n}\nint g(void)\n{\n    total = 2;\n}\n";
    CliShape shape;
    (void)cli_document_store("uses", s_code, sizeof(s_code) - 1u, &shape);
    (void)cli_document_colored_by("uses", key, 1);
    cli_document_walked();
    cli_check(document_run_named("uses", key, 3u, 4u, "identifier.local") &&
                  document_run_named("uses", key, 3u, 12u, "identifier.parameter") &&
                  document_run_named("uses", key, 3u, 20u, "identifier"),
              "a use of a local and of a parameter takes its declaration's name, an undeclared name its own");
    cli_check(document_run_named("uses", key, 4u, 10u, "identifier"), "a name reached through -> keeps its own");
    cli_check(document_run_named("uses", key, 8u, 4u, "identifier"), "a name past the function's region keeps its own");
    cli_document_release("uses");
}

// whether the run at `col` of the runs `runs`, compact `[column, name, ...]`, takes the name `wanted` among the names a
// branch colored by the grammar under `grammar` takes
static int document_run_takes(const char *runs, const char *grammar, unsigned int col, const char *wanted)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *json =
        cli_check_parsed(runs, cellul.len(&(CatenaFinitaCfg){.src = runs, .cap = CLI_CELLS_POOL}), &root);
    unsigned long long place = CLI_JSON_NONE;
    for (unsigned int at = (root != CLI_JSON_NONE) ? json->node[root].first : CLI_JSON_NONE;
         (at != CLI_JSON_NONE) && (json->node[at].next != CLI_JSON_NONE);
         at = json->node[json->node[at].next].next)
    {
        place = (document_number(json, at) == col) ? document_number(json, json->node[at].next) : place;
    }
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_document_names_written(grammar, &out);
    json = cli_check_parsed(s_out, out.at, &root);
    unsigned long long at_place = 0ull;
    for (unsigned int one = json->node[root].first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, one, &size);
        if ((at_place == place) && (text != NULL))
        {
            return (size == cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL})) &&
                   (memor.cmp(&(MemoriaCfg){.src = text, .other = wanted, .bytes = size}) == 0);
        }
        at_place += 1ull;
    }
    return 0;
}

// whether the run at `col` of the line `line` of the text under `key`, as a print of it reads it, takes the name
// `wanted` among the names a branch colored by the grammar under `grammar` takes
static int document_printed_takes(const char *key, const char *grammar, unsigned int line, unsigned int col,
                                  const char *wanted)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)cli_document_runs_written(key, line, line, 0, &out);
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &root);
    const unsigned int range = (root != CLI_JSON_NONE) ? json->node[root].first : CLI_JSON_NONE;
    const unsigned int runs = (range != CLI_JSON_NONE) ? cli_json_member(json, range, "runs") : CLI_JSON_NONE;
    const unsigned int flat = (runs != CLI_JSON_NONE) ? json->node[runs].first : CLI_JSON_NONE;
    char compact[256];
    VerbaLinea written = {compact, sizeof(compact), 0ull};
    if (flat != CLI_JSON_NONE)
    {
        cli_json_write(json, flat, &written, 0u, 0);
    }
    (void)mmgr_line_finish(&written);
    return document_run_takes(compact, grammar, col, wanted);
}

// a grammar whose brackets count depth, the JSON plugin's `@brackets` named by its `bracketToken`: the walk names each
// bracket's run by the depth its pair stands at as it colors the line, the depth carried from line to line in the
// state each ends in; and an edit that opens a bracket above them has the walk color the lines after it again, the
// lines the page holds handed on at their new depths
static void document_brackets(void)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_plugins_grammar_stored("json", key, &err_msg), "the JSON plugin's grammar is stored");
    static const char s_json[] = "[\n  {\n    \"a\": [1]\n  }\n]";
    CliShape shape;
    (void)cli_document_store("brackets", s_json, sizeof(s_json) - 1u, &shape);
    (void)cli_document_colored_by("brackets", key, 1);
    cli_document_walked();
    cli_check(document_printed_takes("brackets", key, 0u, 0u, "delimiter.bracket.depth.open.0") &&
                  document_printed_takes("brackets", key, 1u, 2u, "delimiter.bracket.depth.open.1") &&
                  document_printed_takes("brackets", key, 2u, 9u, "delimiter.bracket.depth.open.2") &&
                  document_printed_takes("brackets", key, 2u, 11u, "delimiter.bracket.depth.close.2") &&
                  document_printed_takes("brackets", key, 3u, 2u, "delimiter.bracket.depth.close.1") &&
                  document_printed_takes("brackets", key, 4u, 0u, "delimiter.bracket.depth.close.0"),
              "each bracket named by the depth its pair stands at, a pair's two the same");
    static const CliEdit s_opened[1] = {{{0u, 0u}, {0u, 0u}, "[", 1u}};
    const CliEdit *const changes[1] = {s_opened};
    const unsigned int counts[1] = {1u};
    (void)cli_document_held("brackets", 2ull, 5ull);
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    (void)cli_document_edit("brackets", changes, counts, 1u, &shape, &err_msg);
    cli_document_walked();
    cli_check(document_run_takes(s_runs_of[2], key, 9u, "delimiter.bracket.depth.open.3") &&
                  document_run_takes(s_runs_of[4], key, 0u, "delimiter.bracket.depth.close.1"),
              "a bracket opened above the lines the page holds: the walk colors them again at their new depths");
    cli_check(document_printed_takes("brackets", key, 0u, 1u, "delimiter.bracket.depth.open.1"),
              "and the bracket opened before them takes the depth after it");
    cli_document_release("brackets");
}

// the lines of the text an edit moves the state of every line after, and the room the text is written in
#define DOCUMENT_ONWARD_LINES 40000ull
#define DOCUMENT_ONWARD_ROOM (4ull << 20u)

// a text whose every line declares a name, its view's columns set, walked whole, then an edit opening a string near
// its top that moves the state of every line after it: the walk colors those lines many to a step, no slower than its
// first walk of the whole text twice over, a little past it for a walk too short to time
static void document_walk_onward(void)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_plugins_grammar_stored("python", key, &err_msg), "the Python plugin's grammar is stored");
    char *const text = g_cli_cells->persistent_buf_alloc(DOCUMENT_ONWARD_ROOM, &g_checks_cells);
    VerbaLinea out = {text, DOCUMENT_ONWARD_ROOM, 0ull};
    for (unsigned long long line = 0ull; line < DOCUMENT_ONWARD_LINES; line += 1ull)
    {
        mmgr_line_put(&out, "x_");
        mmgr_line_decimal(&out, line, 1u);
        mmgr_line_put(&out, " = ");
        mmgr_line_decimal(&out, line, 1u);
        mmgr_line_put(&out, " * 2  # line ");
        mmgr_line_decimal(&out, line, 1u);
        mmgr_line_ch(&out, '\n');
    }
    CliShape shape;
    (void)cli_document_store("onward", text, out.at, &shape);
    (void)cli_document_colored_by("onward", key, 1);
    (void)cli_document_columns("onward", 120ull);
    const unsigned long long began = cli_clock_milliseconds();
    cli_document_walked();
    const unsigned long long walked = cli_clock_milliseconds();
    static const CliEdit s_quoted[1] = {{{1000u, 0u}, {1000u, 0u}, "\"\"\"", 3u}};
    const CliEdit *const changes[1] = {s_quoted};
    const unsigned int counts[1] = {1u};
    (void)cli_document_edit("onward", changes, counts, 1u, &shape, &err_msg);
    cli_document_walked();
    const unsigned long long walked_again = cli_clock_milliseconds();
    cli_check((walked_again - walked) <= ((2ull * (walked - began)) + 100ull),
              "an edit that moves the state of every line after it is walked many lines to a step");
    cli_document_release("onward");
    g_cli_cells->persistent_buf_release(text, &g_checks_cells);
}

// a paged text's lines viewed far from its first: colored at once from root at the first line the page holds, only
// those lines handed on; the walk colors the lines the page holds alone, the comment above them unread; a read's lines
// colored with it and mirrored from then on, the walk handing them on no second time; the lines the page comes to hold
// handed on as the walk colors them again; and an edit the pages write colors the lines after it again
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
              "a paged text is a branch and colored by a grammar");
    out.at = 0ull;
    cli_check(cli_document_viewed("paged", 350ull, 360ull, &out), "its view colored");
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &root);
    document_colors_read(json, root);
    cli_check(document_handed(350u, "[0,2]") && (s_runs_of[349][0] == '\0') && (s_runs_of[361][0] == '\0'),
              "the view colored at once from root at the first line the page holds, only those lines handed on");
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    cli_document_walked();
    cli_check((s_runs_of[350][0] == '\0') && (s_runs_of[0][0] == '\0'),
              "the walk colors the lines the page holds alone, the comment above them unread");
    out.at = 0ull;
    const int read = cli_document_runs_written("paged", 361ull, 370ull, 1, &out);
    (void)mmgr_line_finish(&out);
    static const char s_read_begins[] = "[{\"from\":361,\"runs\":[[0,2]";
    cli_check(read && (memor.cmp(&(MemoriaCfg){.src = s_out, .other = s_read_begins,
                                                .bytes = sizeof(s_read_begins) - 1ull}) == 0),
              "a read's lines colored and written with it, as the view's lines above it leave them");
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
    cli_check(cli_document_held("paged", 350ull, 371ull), "the page names the lines it read among those it mirrors");
    cli_document_walked();
    cli_check((s_runs_of[361][0] == '\0') && (s_runs_of[370][0] == '\0'),
              "the walk hands a read's lines on no second time: the page mirrors them since the read");
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
    lines = document_lines("Caf\xC3\xA9 caf\xC3\x89\nnone");
    (void)document_found(lines, document_sought("\xC3\xA9", 0, 0, 0), 100ull, "[[0,3,4],[0,8,9]]",
                         "without case, a letter outside ASCII found in either case");
    lines = document_lines("a line\xD9\x8B here, \xC3\xA9line");
    (void)document_found(lines, document_sought("line", 0, 1, 1), 100ull, "[[0,2,6]]",
                         "whole words beside text outside ASCII: a mark ends a word, a letter does not");
    lines = document_lines("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                           "ab");
    (void)document_found(lines, document_sought("x_\\d+", 1, 1, 0), 100ull, "[[0,0,3],[1,0,4]]",
                         "a pattern found by the regular expressions the colorer reads");
    (void)document_found(lines, document_sought("[a-z]b", 1, 1, 0), 100ull, "[[1,10,12]]",
                         "a pattern's cols in UTF-16 units past a letter outside the basic plane");
    (void)document_found(lines, document_sought("(", 1, 1, 0), 100ull, NULL, "a pattern that does not read is refused");
    (void)document_found(lines, document_sought("ab", 0, 1, 0), 100ull, "[[1,10,12]]", "cols in UTF-16 units");
    (void)document_found(lines, document_sought("X_", 0, 0, 0), 100ull, "[[0,0,2],[1,0,2]]", "without case");
    (void)document_found(lines, document_sought("10\nx", 0, 1, 0), 100ull, "[]", "nothing runs over a line's end");
}

// the colors walkers hand on from threads of their own counted, under a lock of the check's own
static CliLock s_counted_lock = CLI_LOCK_INIT;
static unsigned int s_counted;

static void document_colors_counted(const char *body, unsigned long long size)
{
    (void)body;
    (void)size;
    cli_lock_take(&s_counted_lock);
    s_counted += 1u;
    cli_lock_give(&s_counted_lock);
}

// the lines of the texts colored at once, a comment opened on the first and closed on the middle one, and how many
// texts; the text, and each text's print, the one walked alone's first
#define DOCUMENT_AT_ONCE_LINES 3000u
#define DOCUMENT_AT_ONCE_TEXTS 4u
static char s_at_once[4u * DOCUMENT_AT_ONCE_LINES];
static char s_printed[DOCUMENT_AT_ONCE_TEXTS + 1u][1u << 16u];

// the text written into s_at_once: its bytes
static unsigned long long document_at_once_text(void)
{
    VerbaLinea text = {s_at_once, sizeof(s_at_once), 0ull};
    mmgr_line_put(&text, "/*\n");
    for (unsigned int line = 1u; line < (DOCUMENT_AT_ONCE_LINES - 1u); line += 1u)
    {
        const unsigned int middle = DOCUMENT_AT_ONCE_LINES / 2u;
        mmgr_line_put(&text, (line == middle) ? "*/\n" : ((line < middle) ? "b\n" : "c\n"));
    }
    mmgr_line_put(&text, "c");
    return text.at;
}

// every line of the text under `key` printed into `out`, `room` bytes: 1 where it was printed whole
static int document_printed(const char *key, char *out, unsigned long long room)
{
    VerbaLinea printed = {out, room, 0ull};
    const int given = cli_document_runs_written(key, 0ull, DOCUMENT_AT_ONCE_LINES - 1u, 0, &printed);
    return given && (mmgr_line_finish(&printed) != 0ull);
}

// the walkers waited for until none walks, ten seconds at the most: 1 where none walks
static int document_walkers_ended(void)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    static CliWake s_wake = CLI_WAKE_INIT;
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    cli_lock_take(&s_lock);
    while ((cli_document_walkers() != 0u) && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&s_wake, &s_lock, 5ull);
    }
    cli_lock_give(&s_lock);
    return cli_document_walkers() == 0u;
}

// texts colored at once by walkers on threads of their own while views and prints of them run beside them and one of
// them is edited: each text's print the same as one walked alone on the calling thread, while the walkers walk and once
// they end, and the walkers ending once every text is colored
static void document_at_once(void)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliShape shape;
    const unsigned long long size = document_at_once_text();
    cli_document_colors_to(document_colors_handed, 0);
    (void)cli_document_store("alone", s_at_once, size, &shape);
    (void)cli_document_colored_by("alone", "branch", 1);
    cli_document_walked();
    const int alone = document_printed("alone", s_printed[0], sizeof(s_printed[0]));
    cli_document_release("alone");
    cli_document_colors_to(document_colors_counted, 1);
    static const char *const s_keys[DOCUMENT_AT_ONCE_TEXTS] = {"at_once_0", "at_once_1", "at_once_2", "at_once_3"};
    for (unsigned int text = 0u; text < DOCUMENT_AT_ONCE_TEXTS; text += 1u)
    {
        (void)cli_document_store(s_keys[text], s_at_once, size, &shape);
        (void)cli_document_colored_by(s_keys[text], "branch", 1);
    }
    // views of each text as the walkers color them, near its middle, its end and its first lines
    static const unsigned long long s_views[3] = {1600ull, 2900ull, 10ull};
    for (unsigned int view = 0u; view < 3u; view += 1u)
    {
        for (unsigned int text = 0u; text < DOCUMENT_AT_ONCE_TEXTS; text += 1u)
        {
            (void)cli_document_viewed(s_keys[text], s_views[view], s_views[view] + 40ull, NULL);
        }
    }
    // the first text's first line written again over itself as the walkers color it
    static const CliEdit s_again[1] = {{{0u, 0u}, {0u, 2u}, "/*", 2u}};
    const CliEdit *const changes[1] = {s_again};
    const unsigned int counts[1] = {1u};
    (void)cli_document_edit(s_keys[0], changes, counts, 1u, &shape, &err_msg);
    int same = alone;
    for (unsigned int text = 0u; text < DOCUMENT_AT_ONCE_TEXTS; text += 1u)
    {
        same = same && document_printed(s_keys[text], s_printed[text + 1u], sizeof(s_printed[0])) &&
               cellul.eq(&(CatenaFinitaCfg){.src = s_printed[text + 1u], .other = s_printed[0], .cap = CLI_CELLS_POOL});
    }
    cli_check(same,
              "texts colored at once by walkers, views and an edit beside them, each printed as one walked alone");
    const int ended = document_walkers_ended();
    cli_lock_take(&s_counted_lock);
    const unsigned int counted = s_counted;
    cli_lock_give(&s_counted_lock);
    cli_check(ended && (counted != 0u), "the walkers hand their colors on and end once every text is colored");
    same = 1;
    for (unsigned int text = 0u; text < DOCUMENT_AT_ONCE_TEXTS; text += 1u)
    {
        same = same && document_printed(s_keys[text], s_printed[text + 1u], sizeof(s_printed[0])) &&
               cellul.eq(&(CatenaFinitaCfg){.src = s_printed[text + 1u], .other = s_printed[0], .cap = CLI_CELLS_POOL});
        cli_document_release(s_keys[text]);
    }
    cli_check(same, "and each text they walked is printed as the one walked alone");
    cli_document_colors_to(document_colors_handed, 0);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    const CliLines *const lines = document_lines("one\r\ntwo\nthree\r\r\n\xF0\x9F\x98\x80");
    cli_check((lines->lines == 4u) && (lines->length[2] == 6ull) && (lines->unit[lines->start[2] + 5u] == '\r') &&
                  (lines->length[3] == 2ull),
              "a text is split as the page splits a file, a char past the basic plane two units");
    document_edits_in_place();
    document_edits();
    document_marked();
    CliShape shape;
    cli_check(cli_document_store("kept", "one\ntwo\nthree", 13ull, &shape) && (shape.lines == 3ull) &&
                  (shape.units == 11ull),
              "a text is stored, its shape given");
    cli_document_release("kept");
    cli_check(!cli_document_shape("kept", &shape), "until it is let go");
    document_find();
    document_found_in_little_room();
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
    document_columns();
    document_folds();
    document_plain_folds();
    document_uses();
    document_brackets();
    document_walk_onward();
    document_at_once();
    cli_check_folder("document", s_folder);
    document_paged();
    return cli_checked();
}
