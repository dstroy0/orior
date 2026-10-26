// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_test.c: the editor's colors' check: a JSON line containing a string of thousands of characters colors; a
// line is colored by its rules, each token a run of its own; a state carries to the next line; columns count as the
// page counts them; a rule reads
// its groups and a grammar its includes; what matches no rule has no token; a grammar's leaves are handed over a line
// at a time; a language plugin's grammar is stored under its id's key from its plugin.json; and the C, Python, Rust and
// JavaScript plugins' grammars name a function's parameters, their types and a body's locals
#include "../../../../../ui/cli/src/editor/highlight.h"
#include "../../../../../ui/cli/src/plugins/plugins.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static const char s_grammar[] =
    "{\"keywords\": [\"if\", \"return\"], \"tokenizer\": {\"root\": ["
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

static char s_out[1u << 20u];
static char s_line[1u << 15u];
static char s_plugin[1u << 18u];
static char s_state[256];

// the grammar of `size` bytes of JSON, its `member` where it is not NULL, stored under `key`: 1, or 0 where it is
// refused
static int highlight_grammar_store(const char *key, const char *text, unsigned long long size, const char *member)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(text, size, &root);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    return cli_highlight_store(key, json, (member != NULL) ? cli_json_member(json, root, member) : root, &err_msg);
}

// `line` colored from `state` with the grammar under `key`, its runs `column name` joined by `|` into `out`, which
// is sized for `room`, and the state it ends in into s_state
static const char *highlight_line(const char *key, const char *line, const char *state, char *out,
                                  unsigned long long room)
{
    const char *const lines[1] = {line};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea colored = {s_out, sizeof(s_out), 0ull};
    VerbaLinea runs_line = {out, room, 0ull};
    s_state[0] = '\0';
    if (cli_highlight_color(key, state, lines, 1u, &colored, &err_msg))
    {
        unsigned int found = CLI_JSON_NONE;
        const CliJson *const json = cli_check_parsed(s_out, colored.at, &found);
        const unsigned int names = cli_json_member(json, found, "names");
        const unsigned int runs = cli_json_item(json, cli_json_member(json, found, "runs"), 0u);
        for (unsigned int at = 0u; cli_json_item(json, runs, at) != CLI_JSON_NONE; at += 1u)
        {
            const unsigned int run = cli_json_item(json, runs, at);
            const unsigned int column = cli_json_item(json, run, 0u);
            const unsigned int name = cli_json_item(json, run, 1u);
            unsigned int index = 0u;
            for (unsigned long long digit = 0ull; digit < json->node[name].length; digit += 1ull)
            {
                index = index * 10u + (unsigned int)(json->text[json->node[name].text + digit] - '0');
            }
            const char *const named = cli_json_string(json, cli_json_item(json, names, index), NULL);
            mmgr_line_put(&runs_line, (at > 0u) ? "|" : "");
            mmgr_line_put_n(&runs_line, json->text + json->node[column].text, json->node[column].length);
            mmgr_line_ch(&runs_line, ' ');
            mmgr_line_put(&runs_line, (named != NULL) ? named : "?");
        }
        const char *const ended =
            cli_json_string(json, cli_json_item(json, cli_json_member(json, found, "states"), 0u), NULL);
        memor.cpy(&(MemoriaCfg){
            .dst = s_state,
            .src = (ended != NULL) ? ended : "",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = (ended != NULL) ? ended : "", .cap = CLI_CELLS_POOL}) + 1u});
    }
    (void)mmgr_line_finish(&runs_line);
    return out;
}

// a JSON line containing a string of thousands of characters colors
static void highlight_long(void)
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!cli_tree_found(root))
    {
        cli_check(1, "no tree is found, and JSON's grammar is not read");
        return;
    }
    cli_full(root, "ui/plugins/json/plugin.json", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_plugin, sizeof(s_plugin));
    cli_check((size != QRY_MEMBER_UNREAD) && highlight_grammar_store("json", s_plugin, size, "grammar"),
              "JSON's grammar is stored");
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
    static char s_runs[1u << 16u];
    highlight_line("json", s_line, "root", s_runs, sizeof(s_runs));
    const char *const string = cellul.find(
        &(CatenaFinitaCfg){.src = s_runs, .cap = CLI_CELLS_POOL, .other = "|9 ", .other_cap = CLI_CELLS_POOL});
    char wanted[32];
    VerbaLinea next = {wanted, sizeof(wanted), 0ull};
    mmgr_line_ch(&next, '|');
    mmgr_line_decimal(&next, 9u + length + 2u, 1u);
    mmgr_line_ch(&next, ' ');
    (void)mmgr_line_finish(&next);
    const char *const bar =
        (string != NULL) ? cellul.chr(&(CatenaFinitaCfg){.src = string + 1, .cap = CLI_CELLS_POOL, .byte = '|'}) : NULL;
    cli_check(
        (string != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = string, .cap = CLI_CELLS_POOL, .other = "string", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (bar != NULL) &&
            (cli_text_same(bar, wanted, cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL}))),
        "a string of thousands of characters runs to its closing quote");
}

// each line's leaves put after those before, `col name` joined by `|`, each line ended by `/`
static void highlight_leaves_capture(void *context, const CliLeaves *leaves)
{
    VerbaLinea *const out = context;
    for (unsigned int run = 0u; run < leaves->runs; run += 1u)
    {
        mmgr_line_put(out, (run != 0u) ? "|" : "");
        mmgr_line_decimal(out, leaves->col[run], 1u);
        mmgr_line_ch(out, ' ');
        mmgr_line_put(out, leaves->text + leaves->name_at[leaves->name[run]]);
    }
    mmgr_line_ch(out, '/');
}

// a stored grammar's leaves handed over a line at a time, the state carried from line to line and the last line's
// given back; and a language plugin's grammar stored under its id's key from its plugin.json, read once
static void highlight_leaves(void)
{
    static const char *const s_lines[3] = {"if (x)", "a /* open", "still */ b"};
    char leaves[512];
    char ended[64];
    char why[256];
    VerbaLinea out = {leaves, sizeof(leaves), 0ull};
    VerbaLinea ended_line = {ended, sizeof(ended), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int handed = cli_highlight_leaves("test", "root", s_lines, 3u, highlight_leaves_capture, &out, &ended_line,
                                            &err_msg);
    (void)mmgr_line_finish(&out);
    (void)mmgr_line_finish(&ended_line);
    cli_check(handed && cellul.eq(&(CatenaFinitaCfg){.src = leaves,
                                                     .other = "0 keyword|2 |3 delimiter.bracket|4 identifier|5 "
                                                              "delimiter.bracket/0 identifier|1 |2 comment|4 comment/0 "
                                                              "comment|6 comment|8 |9 identifier/",
                                                     .cap = CLI_CELLS_POOL}),
              "a stored grammar's leaves are handed over a line at a time, each token a leaf of its own");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = ended, .other = "root", .cap = CLI_CELLS_POOL}),
              "and the state the last line ends in given back");
    char key[CLI_NAME_MAX_LEN];
    cli_check(cli_plugins_grammar_stored("c", key, &err_msg) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "plugin:c", .cap = CLI_CELLS_POOL}),
              "the C plugin's grammar is stored under plugin:c");
    char runs[256];
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = highlight_line(key, "int x;", "root", runs, sizeof(runs)),
                                           .other = "0 type|3 |4 identifier|5 delimiter.statement",
                                           .cap = CLI_CELLS_POOL}),
              "and colors as the plugin's grammar gives");
    cli_check(cli_plugins_grammar_stored("c", key, &err_msg), "and is stored again only where its text changed");
    err_msg.at = 0ull;
    cli_check(!cli_plugins_grammar_stored("no_such_plugin", key, &err_msg) && (err_msg.at != 0ull),
              "a plugin there is none of is refused with why");
}

// the line `text` colored by the grammar of the plugin `id` from root, checked against `wanted`
static void highlight_plugin_line(const char *id, const char *text, const char *wanted, const char *what)
{
    char key[CLI_NAME_MAX_LEN];
    char why[256];
    char runs[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_plugins_grammar_stored(id, key, &err_msg) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = highlight_line(key, text, "root", runs, sizeof(runs)),
                                               .other = wanted,
                                               .cap = CLI_CELLS_POOL}),
              what);
}

// a function's parameters, their types and a body's locals named by the C, Python, Rust and JavaScript plugins'
// grammars
static void highlight_declarations(void)
{
    highlight_plugin_line("c", "static int add(const char *name, unsigned int count);",
                          "0 keyword|6 |7 type|10 |11 identifier.declaration.function|14 "
                          "delimiter.bracket.parameter.open|15 keyword|20 |21 type.string|25 |26 operator.memory|27 "
                          "identifier.parameter|31 delimiter.separator|32 |33 type.number|41 |42 type.number|45 |46 "
                          "identifier.parameter|51 delimiter.bracket.parameter.close|52 delimiter.statement",
                          "C's prototype names its parameters and their types");
    highlight_plugin_line("c", "    unsigned int total = 0u;",
                          "0 |4 type.number|12 |13 type.number|16 |17 identifier.local|22 |23 operator.assign|24 |25 "
                          "number|26 number.suffix|27 delimiter.statement",
                          "a C local and its type");
    highlight_plugin_line("python", "def total(first: int, second=\"x\"):",
                          "0 keyword|3 |4 identifier.declaration.function|9 delimiter.bracket.parameter.open|10 "
                          "identifier.parameter|15 delimiter|16 |17 type.number|20 delimiter.separator|21 |22 "
                          "identifier.parameter|28 operator.assign|29 string|32 delimiter.bracket.parameter.close|33 "
                          "operator",
                          "Python's head names its parameters and an annotation's type");
    highlight_plugin_line("python", "    count: int = 0",
                          "0 |4 identifier.local|9 delimiter|10 |11 type.number|14 |15 operator.assign|16 |17 number",
                          "a Python local with its annotation");
    highlight_plugin_line("python", "    total = 4", "0 |4 identifier.local|9 |10 operator.assign|11 |12 number",
                          "and one without");
    highlight_plugin_line("rust", "fn add(name: &str, count: u32) -> u32 {",
                          "0 keyword|2 |3 identifier.declaration.function|6 delimiter.bracket.parameter.open|7 "
                          "identifier.parameter|11 delimiter|12 |13 operator.memory|14 type.string|17 "
                          "delimiter.separator|18 |19 identifier.parameter|24 delimiter|25 |26 type.number|29 "
                          "delimiter.bracket.parameter.close|30 |31 operator|33 |34 type|37 |38 "
                          "delimiter.bracket.region.open",
                          "Rust's head names its parameters and their types");
    highlight_plugin_line("rust", "    let mut total: u64 = 0;",
                          "0 |4 keyword|7 |8 keyword|12 identifier.local|17 delimiter|18 |19 type.number|22 |23 "
                          "operator.assign|24 |25 number|26 delimiter.statement",
                          "a Rust local and its type");
    highlight_plugin_line("javascript", "function add(name, count = 1) {",
                          "0 keyword|8 |9 identifier.declaration.function|12 delimiter.bracket.parameter.open|13 "
                          "identifier.parameter|17 delimiter.separator|18 |19 identifier.parameter|24 |25 "
                          "operator.assign|26 |27 number|28 delimiter.bracket.parameter.close|29 |30 "
                          "delimiter.bracket.region.open",
                          "JavaScript's head names its parameters");
    highlight_plugin_line("javascript", "  const total = 4;",
                          "0 |2 keyword|7 |8 identifier.local|13 |14 operator.assign|15 |16 number|17 "
                          "delimiter.statement",
                          "a JavaScript local");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char runs[1024];
    highlight_long();
    cli_check(highlight_grammar_store("test", s_grammar, sizeof(s_grammar) - 1u, NULL), "a grammar is stored");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = highlight_line("test", "if (x) return 12", "root", runs, sizeof(runs)),
            .other = "0 keyword|2 |3 delimiter.bracket|4 identifier|5 delimiter.bracket|6 |7 keyword|13 |14 number",
            .cap = CLI_CELLS_POOL}),
        "a line is colored by its rules");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_state, .other = "root", .cap = CLI_CELLS_POOL}),
              "and ends in root");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = highlight_line("test", "(((())))", "root", runs, sizeof(runs)),
                  .other = "0 delimiter.bracket|1 delimiter.bracket|2 delimiter.bracket|3 delimiter.bracket|4 "
                           "delimiter.bracket|5 delimiter.bracket|6 delimiter.bracket|7 delimiter.bracket",
                  .cap = CLI_CELLS_POOL}),
              "each bracket is a token of its own, never joined to the bracket beside it");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("test", "a /* open", "root", runs, sizeof(runs)),
                                           .other = "0 identifier|1 |2 comment|4 comment",
                                           .cap = CLI_CELLS_POOL}),
              "a comment opened colors, each of its tokens a run of its own");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_state, .other = "root/comment", .cap = CLI_CELLS_POOL}),
              "and its state carries");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("test", "still */ b", "root/comment", runs, sizeof(runs)),
                                     .other = "0 comment|6 comment|8 |9 identifier",
                                     .cap = CLI_CELLS_POOL}),
        "to the next line");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_state, .other = "root", .cap = CLI_CELLS_POOL}), "until it closes");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = highlight_line("test", "\"\xF0\x9F\x98\x80\" 7", "root", runs, sizeof(runs)),
                  .other = "0 string|1 string|3 string|4 |5 number",
                  .cap = CLI_CELLS_POOL}),
              "columns count as the page counts them");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("test", "key: 1", "root", runs, sizeof(runs)),
                                           .other = "0 label|3 delimiter|4 |5 number",
                                           .cap = CLI_CELLS_POOL}),
              "a rule reads its groups and a grammar its includes");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("test", "#", "root", runs, sizeof(runs)),
                                           .other = "0 ",
                                           .cap = CLI_CELLS_POOL}),
              "what matches no rule has no token");
    // grammars stored under many keys, each stored as the list of them grows, the first still colors after the last
    int stored = 1;
    for (unsigned int at = 0u; at < 100u; at += 1u)
    {
        char key[32];
        VerbaLinea named = {key, sizeof(key), 0ull};
        mmgr_line_put(&named, "many_");
        mmgr_line_decimal(&named, at, 1u);
        (void)mmgr_line_finish(&named);
        stored &= highlight_grammar_store(key, s_grammar, sizeof(s_grammar) - 1u, NULL);
    }
    cli_check(stored &&
                  cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("many_0", "if", "root", runs, sizeof(runs)),
                                               .other = "0 keyword",
                                               .cap = CLI_CELLS_POOL}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("many_99", "if", "root", runs, sizeof(runs)),
                                               .other = "0 keyword",
                                               .cap = CLI_CELLS_POOL}),
              "a hundred grammars are stored, each under its key");
    highlight_leaves();
    highlight_declarations();
    static const char s_bad[] = "{\"tokenizer\": {\"root\": [[{\"pattern\": \"(?<=a)b\"}, \"x\"]]}}";
    cli_check(!highlight_grammar_store("bad", s_bad, sizeof(s_bad) - 1u, NULL), "a pattern not read here is refused");
    // the grammars report their own mark: the most the grammars used and the rooms of a read and a coloring used of
    // the cellblock at once
    char report[160];
    VerbaLinea report_line = {report, sizeof(report), 0ull};
    mmgr_line_put(&report_line, "the grammars count what they and their rooms occupy in the cellblock, their report ");
    const unsigned long long numbers = report_line.at;
    cli_highlight_cells_usage_written(&report_line);
    (void)mmgr_line_finish(&report_line);
    const char *const mark =
        cellul.chr(&(CatenaFinitaCfg){.src = report + numbers, .cap = CLI_CELLS_POOL, .byte = ' '});
    cli_check((mark != NULL) && (mark[1] != '0'), report);
    return cli_checked();
}
