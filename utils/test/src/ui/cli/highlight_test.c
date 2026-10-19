// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_test.c: the editor's colors' check: a JSON line holding a string of thousands of characters colors; a line
// is colored by its rules; a state carries to the next line; columns count as the page counts them; a rule reads its
// groups and a grammar its includes; and what matches no rule has no token
#include "../../../../../ui/cli/src/editor/highlight.h"
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

// the grammar of `size` bytes of JSON, its `member` where it is not NULL, kept under `key`: 1, or 0 where it is refused
static int highlight_kept(const char *key, const char *text, unsigned long long size, const char *member)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(text, size, &root);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_highlight_keep(key, json, (member != NULL) ? cli_json_member(json, root, member) : root, &said);
}

// `line` colored from `state` with the grammar under `key`, its runs `column name` joined by `|` into `out`, which
// holds `room`, and the state it ends in into s_state
static const char *highlight_line(const char *key, const char *line, const char *state, char *out,
                                  unsigned long long room)
{
    const char *const lines[1] = {line};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine colored = {s_out, sizeof(s_out), 0ull};
    CliLine runs_line = {out, room, 0ull};
    s_state[0] = '\0';
    if (cli_highlight_color(key, state, lines, 1u, &colored, &said))
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
            cli_line_text(&runs_line, (at > 0u) ? "|" : "");
            cli_line_counted(&runs_line, json->text + json->node[column].text, json->node[column].length);
            cli_line_character(&runs_line, ' ');
            cli_line_text(&runs_line, (named != NULL) ? named : "?");
        }
        const char *const ended =
            cli_json_string(json, cli_json_item(json, cli_json_member(json, found, "states"), 0u), NULL);
        memor.cpy(&(MemoriaCfg){
            .dst = s_state,
            .src = (ended != NULL) ? ended : "",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = (ended != NULL) ? ended : "", .cap = CLI_CELLS_POOL}) + 1u});
    }
    (void)cli_line_closed(&runs_line);
    return out;
}

// a JSON line holding a string of thousands of characters colors
static void highlight_long(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (!cli_tree_found(root))
    {
        cli_check(1, "no tree is found, and JSON's grammar is not read");
        return;
    }
    cli_full(root, "ui/plugins/json/plugin.json", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_plugin, sizeof(s_plugin));
    cli_check((size != QRY_MEMBER_UNREAD) && highlight_kept("json", s_plugin, size, "grammar"),
              "JSON's grammar is kept");
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, "{\"body\": \"");
    unsigned int length = 0u;
    for (unsigned int at = 0u; at < 15000u; at += 1u)
    {
        cli_line_text(&line, ((at % 1000u) == 0u) ? "\\\"" : "x");
        length += ((at % 1000u) == 0u) ? 2u : 1u;
    }
    cli_line_text(&line, "\", \"n\": 1}");
    (void)cli_line_closed(&line);
    static char s_runs[1u << 16u];
    highlight_line("json", s_line, "root", s_runs, sizeof(s_runs));
    const char *const string = cellul.find(
        &(CatenaFinitaCfg){.src = s_runs, .cap = CLI_CELLS_POOL, .other = "|9 ", .other_cap = CLI_CELLS_POOL});
    char wanted[32];
    CliLine next = {wanted, sizeof(wanted), 0ull};
    cli_line_character(&next, '|');
    cli_line_decimal(&next, 9u + length + 2u, 1u);
    cli_line_character(&next, ' ');
    (void)cli_line_closed(&next);
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

int main(void)
{
    char runs[1024];
    highlight_long();
    cli_check(highlight_kept("test", s_grammar, sizeof(s_grammar) - 1u, NULL), "a grammar is kept");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = highlight_line("test", "if (x) return 12", "root", runs, sizeof(runs)),
            .other = "0 keyword|2 |3 delimiter.bracket|4 identifier|5 delimiter.bracket|6 |7 keyword|13 |14 number",
            .cap = CLI_CELLS_POOL}),
        "a line is colored by its rules");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_state, .other = "root", .cap = CLI_CELLS_POOL}),
              "and ends in root");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("test", "a /* open", "root", runs, sizeof(runs)),
                                           .other = "0 identifier|1 |2 comment",
                                           .cap = CLI_CELLS_POOL}),
              "a comment opened colors");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_state, .other = "root/comment", .cap = CLI_CELLS_POOL}),
              "and its state carries");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = highlight_line("test", "still */ b", "root/comment", runs, sizeof(runs)),
                                     .other = "0 comment|8 |9 identifier",
                                     .cap = CLI_CELLS_POOL}),
        "to the next line");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = s_state, .other = "root", .cap = CLI_CELLS_POOL}), "until it closes");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = highlight_line("test", "\"\xF0\x9F\x98\x80\" 7", "root", runs, sizeof(runs)),
                  .other = "0 string|4 |5 number",
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
    static const char s_bad[] = "{\"tokenizer\": {\"root\": [[{\"pattern\": \"(?<=a)b\"}, \"x\"]]}}";
    cli_check(!highlight_kept("bad", s_bad, sizeof(s_bad) - 1u, NULL), "a pattern not read here is refused");
    // the grammars report their own mark: the most the grammars kept and the rooms of a read and a coloring held of
    // the cellblock at once
    char report[160];
    CliLine said = {report, sizeof(report), 0ull};
    cli_line_text(&said, "the grammars count what they and their rooms hold in the cellblock, their report ");
    const unsigned long long numbers = said.at;
    cli_highlight_cells_said(&said);
    (void)cli_line_closed(&said);
    const char *const mark =
        cellul.chr(&(CatenaFinitaCfg){.src = report + numbers, .cap = CLI_CELLS_POOL, .byte = ' '});
    cli_check((mark != NULL) && (mark[1] != '0'), report);
    return cli_checked();
}
