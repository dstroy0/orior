// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_rank_test.c: the quick open and the editor's windows: a name as typed stands above every looser match and above
// the files opened lately; with fuzzy matching off only the text as typed answers; a quick open answers from the
// recent files until the tree is read; a window and its slices are whole numbered lines; and a name typed matches its
// letters in the name, each score the palette's own, a query torn into too many pieces matching nothing
#include "../../../../../ui/cli/src/editor/files.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static char s_out[1u << 18u];
static char s_text[1u << 18u];
static char s_shown[1u << 16u];

// the paths a ranking wrote, joined by `|`, its first's hits after a `#`, and whether it was reading into `reading`
static const char *rank_paths(unsigned long long size, int *reading)
{
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, size, &read);
    const unsigned int found = cli_json_member(json, read, "found");
    *reading = cli_json_is(json, cli_json_member(json, read, "reading"), CFG_JSON_TRUE);
    CliLine line = {s_shown, sizeof(s_shown), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_check_member_text(cli_json_item(json, found, at), "path"));
    }
    const unsigned int hits = cli_json_member(json, cli_json_item(json, found, 0u), "hits");
    cli_line_character(&line, '#');
    for (unsigned int at = 0u; cli_json_item(json, hits, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int hit = cli_json_item(json, hits, at);
        cli_line_text(&line, (at > 0u) ? "," : "");
        cli_line_counted(&line, json->text + json->node[hit].text, json->node[hit].length);
    }
    (void)cli_line_closed(&line);
    return s_shown;
}

// `query` ranked over the `count` paths of `files`: the paths found and the first's hits, as rank_paths writes them
static const char *rank_in(const char *const *files, unsigned int count, const char *query, const char *const *recent,
                           unsigned int recents, int fuzzy)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_files_ranked_in(files, count, query, recent, recents, 10u, fuzzy, &out);
    int reading = 0;
    return rank_paths(out.at, &reading);
}

// a name as typed stands above every looser match and above the files opened lately; and with fuzzy matching off only
// the text as typed answers
static void rank_going(void)
{
    static const char *const s_files[4] = {"src/view_tests/view_helpers.rs", "src/review.rs", "docs/view.md",
                                           "src/view.rs"};
    static const char *const s_recent[2] = {"src/review.rs", "src/view_tests/view_helpers.rs"};
    const char *const found = rank_in(s_files, 4u, "view", s_recent, 2u, 1);
    cli_check(
        cellul.starts(&(CatenaFinitaCfg){.src = found, .other = "src/view.rs|docs/view.md|", .cap = CLI_CELLS_POOL}),
        "named as typed, with or without its extension, first");
    static const char *const s_typed[3] = {"src/review.rs", "src/vim_ew.rs", "src/view.rs"};
    const char *const fuzzy = rank_in(s_typed, 3u, "view", NULL, 0u, 1);
    unsigned int paths = 1u;
    for (const char *at = fuzzy; (at[0] != '\0') && (at[0] != '#'); at += 1)
    {
        paths += (at[0] == '|') ? 1u : 0u;
    }
    cli_check(paths == 3u, "fuzzy: the letters in order anywhere");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = rank_in(s_typed, 3u, "view", NULL, 0u, 0),
                                           .other = "src/view.rs|src/review.rs#4,5,6,7",
                                           .cap = CLI_CELLS_POOL}),
              "with fuzzy matching off, only the text as typed, its hits where it stands");
}

// a quick open answers from the recent files until the tree is read
static void rank_reading(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("files_reading", root);
    cli_full(root, "alpha.txt", path);
    (void)query_file_written(path, "one", 3ull);
    cli_full(root, "apple.txt", path);
    (void)query_file_written(path, "two", 3ull);
    static const char *const s_recent[1] = {"apple.txt"};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_files_ranked(root, "a", s_recent, 1u, 10u, 1, &out);
    int reading = 0;
    const char *const first = rank_paths(out.at, &reading);
    cli_check(reading, "the tree is not read yet");
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = first, .other = "apple.txt#", .cap = CLI_CELLS_POOL}),
              "the recent files alone answer");
    for (unsigned int tries = 0u; cli_files_unread(root) && (tries < 100u); tries += 1u)
    {
        query_paused(50u);
    }
    out.at = 0ull;
    cli_files_ranked(root, "alpha", NULL, 0u, 10u, 1, &out);
    const char *const read = rank_paths(out.at, &reading);
    cli_check(!reading, "the tree is read");
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = read, .other = "alpha.txt", .cap = CLI_CELLS_POOL}),
              "and answers");
}

// A window's members as it was written
typedef struct
{
    unsigned long long start;
    unsigned long long end;
    unsigned long long size;
    unsigned long long line;
    char text[1u << 16u];
} RankWindow;

// a whole number member of the shared tree's node
static unsigned long long rank_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int member = cli_json_member(json, node, name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; (member != CLI_JSON_NONE) && (at < json->node[member].length); at += 1ull)
    {
        value = (value * 10ull) + (unsigned long long)(json->text[json->node[member].text + at] - '0');
    }
    return value;
}

// a window or a slice written into `out` read into `window`, the line read only once the call that wrote it is made: 1,
// or 0 where it was not written
static int rank_window(int written, const CliLine *out, RankWindow *window)
{
    if (!written)
    {
        return 0;
    }
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out->at, &read);
    window->start = rank_whole(json, read, "start");
    window->end = rank_whole(json, read, "end");
    window->size = rank_whole(json, read, "size");
    window->line = rank_whole(json, read, "line");
    const char *const text = cli_check_member_text(read, "text");
    memor.cpy(&(MemoriaCfg){.dst = window->text,
                            .src = text,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// line `n` of the file, as written
static void rank_line(CliLine *out, unsigned long long n)
{
    cli_line_text(out, "line ");
    cli_line_decimal(out, n, 1u);
    cli_line_character(out, ' ');
    for (unsigned long long x = 0ull; x < (n % 37ull); x += 1ull)
    {
        cli_line_character(out, 'x');
    }
    cli_line_character(out, '\n');
}

// a window and its slices are whole numbered lines
static void rank_windows(void)
{
    static RankWindow s_window;
    static RankWindow s_below;
    static RankWindow s_above;
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[1024];
    char wanted[128];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("files_window", root);
    CliLine text = {s_text, sizeof(s_text), 0ull};
    for (unsigned long long n = 0ull; n < 5000ull; n += 1ull)
    {
        rank_line(&text, n);
    }
    cli_full(root, "lines.txt", path);
    (void)query_file_written(path, s_text, text.at);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(rank_window(cli_files_window(root, "lines.txt", 2500ull, 3000ull, &out, &said), &out, &s_window),
              "a window is read");
    CliLine first = {wanted, sizeof(wanted), 0ull};
    rank_line(&first, s_window.line);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_window.text, .cap = CLI_CELLS_POOL});
    cli_check(
        cli_text_same(s_window.text, wanted, first.at) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_window.text, .cap = CLI_CELLS_POOL, .other = "\nline 2500 ", .other_cap = CLI_CELLS_POOL}) !=
             NULL) &&
            (length > 0ull) && (s_window.text[length - 1u] == '\n'),
        "its whole lines, numbered from its first");
    out.at = 0ull;
    cli_check(rank_window(cli_files_slice(root, "lines.txt", s_window.end, s_window.end + 2000ull, &out, &said), &out,
                          &s_below) &&
                  (s_below.start == s_window.end),
              "the slice below starts where it ends");
    unsigned long long lines = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        lines += (s_window.text[at] == '\n') ? 1ull : 0ull;
    }
    CliLine next = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&next, "line ");
    cli_line_decimal(&next, s_window.line + lines, 1u);
    cli_line_character(&next, ' ');
    cli_check(cli_text_same(s_below.text, wanted, next.at), "with the line after its last");
    out.at = 0ull;
    const unsigned long long from = (s_window.start > 2000ull) ? (s_window.start - 2000ull) : 0ull;
    cli_check(rank_window(cli_files_slice(root, "lines.txt", from, s_window.start, &out, &said), &out, &s_above) &&
                  (s_above.end == s_window.start),
              "the slice above ends where it starts");
    CliLine before = {wanted, sizeof(wanted), 0ull};
    rank_line(&before, s_window.line - 1ull);
    const unsigned long long above = cellul.len(&(CatenaFinitaCfg){.src = s_above.text, .cap = CLI_CELLS_POOL});
    cli_check((above >= before.at) &&
                  (memor.cmp(&(MemoriaCfg){
                       .src = s_above.text + above - before.at, .other = wanted, .bytes = before.at}) == 0),
              "with the line before its first");
    out.at = 0ull;
    cli_check(rank_window(cli_files_window(root, "lines.txt", 99999ull, 100ull, &out, &said), &out, &s_window) &&
                  (s_window.end == s_window.size),
              "a window past the end ends at the file's");
}

// whether `query` matches `text` past `from` with `score`, any where it is below 0, and the hits `wanted`, any where it
// is NULL, `-` for no match
static int rank_fuzzy(const char *query, const char *text, unsigned long long from, double score, const char *wanted)
{
    static unsigned int s_hits[CLI_FUZZY_LETTERS];
    double got = 0.0;
    unsigned int count = 0u;
    if (!cli_files_fuzzy(query, text, from, &got, s_hits, &count))
    {
        return (wanted != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = wanted, .other = "-", .cap = CLI_CELLS_POOL}));
    }
    char hits[256];
    CliLine line = {hits, sizeof(hits), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "," : "");
        cli_line_decimal(&line, s_hits[at], 1u);
    }
    (void)cli_line_closed(&line);
    const double apart = (got > score) ? (got - score) : (score - got);
    return ((score < 0.0) || (apart < 1e-9)) &&
           ((wanted == NULL) || (cellul.eq(&(CatenaFinitaCfg){.src = hits, .other = wanted, .cap = CLI_CELLS_POOL})));
}

int main(void)
{
    rank_going();
    rank_reading();
    rank_windows();
    cli_check(rank_fuzzy("edit", "src/ui/src/edit.js", 11ull, -1.0, "11,12,13,14"),
              "a name typed matches its letters in the name");
    cli_check(rank_fuzzy("xyz", "src/ui/src/edit.js", 11ull, -1.0, "-"), "and nothing where it has them not");
    cli_check(rank_fuzzy("", "a.rs", 0ull, -1.0, ""), "an empty query matches with no hits");
    cli_check(rank_fuzzy("view", "src/ui/src/editor/view.js", 18ull, 42.2, NULL), "each score the palette's own");
    cli_check(rank_fuzzy("edit", "src/ui/src/edit.js", 11ull, 42.34, NULL), "and another");
    cli_check(rank_fuzzy("view", "src/vendor/item/eventwatch.js", 21ull, -1.0, "-"), "a match too torn is none");
    cli_check(rank_fuzzy("abcd", "a/x/b/x/c/x/d", 12ull, -1.0, "-"), "a query torn into too many pieces");
    return cli_checked();
}
