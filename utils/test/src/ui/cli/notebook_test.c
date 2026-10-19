// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_test.c: the notebooks' check: a notebook reads as cells and writes back as it was, a percent script does the
// same, a merge takes each side's changes and holds the cells both changed apart, and a diff reads sources and outputs
// and no run counts
#include "../../../../../ui/cli/src/notebooks/notebook.h"
#include "cli_check.h"

static const char s_book[] =
    "{\n \"cells\": [\n  {\n   \"cell_type\": \"code\",\n   \"execution_count\": 3,\n   \"id\": \"a1\",\n"
    "   \"metadata\": {},\n   \"outputs\": [\n    {\n     \"name\": \"stdout\",\n     \"output_type\": \"stream\",\n"
    "     \"text\": [\n      \"hi\\n\"\n     ]\n    }\n   ],\n   \"source\": [\n    \"print('hi')\\n\",\n"
    "    \"x = 1\"\n   ]\n  },\n  {\n   \"cell_type\": \"markdown\",\n   \"id\": \"b2\",\n   \"metadata\": {},\n"
    "   \"source\": [\n    \"# Title\"\n   ]\n  }\n ],\n \"metadata\": {\n  \"kernelspec\": {\n"
    "   \"display_name\": \"Python 3\",\n   \"language\": \"python\",\n   \"name\": \"python3\"\n  }\n },\n"
    " \"nbformat\": 4,\n \"nbformat_minor\": 5\n}\n";

static CliJsonNode s_node[1u << 14u];
static char s_text[1u << 18u];
static CfgJsonToken s_token[1u << 14u];
static CliJson s_json;
static char s_out[1u << 16u];

// the tree made empty
static CliJson *notebook_tree(void)
{
    cli_json_begun(&s_json, s_node, sizeof(s_node) / sizeof(s_node[0]), s_text, sizeof(s_text));
    return &s_json;
}

// a string reached from `node` by `count` members or items, each a name or, where it starts with `#`, an index of one
// digit; the empty text where it has none
static const char *notebook_at(unsigned int node, const char *const *path, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        node = (path[at][0] == '#') ? cli_json_item(&s_json, node, (unsigned int)(path[at][1] - '0'))
                                    : cli_json_member(&s_json, node, path[at]);
    }
    const char *const text = cli_json_string(&s_json, node, NULL);
    return (text != NULL) ? text : "";
}

// a notebook reads as cells and writes back as it was
static void notebook_book(void)
{
    static const char *const s_source[3] = {"cells", "#0", "source"};
    static const char *const s_output[5] = {"cells", "#0", "outputs", "#0", "text"};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliJson *const json = notebook_tree();
    const unsigned int book = cli_notebook_read(json, "a.ipynb", s_book, sizeof(s_book) - 1u, s_token,
                                                sizeof(s_token) / sizeof(s_token[0]), &said);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = notebook_at(book, s_source, 3u), .other = "print('hi')\nx = 1", .cap = CLI_CELLS_POOL}),
              "a cell's source reads as one text");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = notebook_at(book, s_output, 5u), .other = "hi\n", .cap = CLI_CELLS_POOL}),
        "and its output's text");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_notebook_write(json, "a.ipynb", book, &out);
    cli_check_text(s_out, out.at, s_book, "a notebook writes back as it was");
}

// a percent script reads as cells and writes back as it was
static void notebook_script(void)
{
    static const char s_script[] = "import os\n\n# %% Load\nx = 1\nprint(x)\n\n# %% [markdown]\n# A note\n#\n"
                                   "# on two lines\n\n# %%\ny = x + 1\n";
    static const char *const s_header[1] = {"header"};
    static const char *const s_title[4] = {"cells", "#0", "metadata", "title"};
    static const char *const s_kind[3] = {"cells", "#1", "cell_type"};
    static const char *const s_source[3] = {"cells", "#1", "source"};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliJson *const json = notebook_tree();
    const unsigned int book = cli_notebook_read(json, "s.py", s_script, sizeof(s_script) - 1u, s_token,
                                                sizeof(s_token) / sizeof(s_token[0]), &said);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = notebook_at(book, s_header, 1u), .other = "import os\n\n", .cap = CLI_CELLS_POOL}),
              "a script keeps its lines before a cell");
    cli_check(json->node[cli_json_member(json, book, "cells")].count == 3u, "each `# %%` begins a cell");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = notebook_at(book, s_title, 4u), .other = "Load", .cap = CLI_CELLS_POOL}),
        "a cell's mark names its title");
    cli_check(
        (cellul.eq(
            &(CatenaFinitaCfg){.src = notebook_at(book, s_kind, 3u), .other = "markdown", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = notebook_at(book, s_source, 3u), .other = "A note\n\non two lines", .cap = CLI_CELLS_POOL})),
        "a markdown cell reads from behind its comments");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_notebook_write(json, "s.py", book, &out);
    cli_check_text(s_out, out.at, s_script, "a script writes back as it was");
    cli_check(cli_notebook_is("s.py", s_script, sizeof(s_script) - 1u), "a script of cells is a notebook");
    cli_check(!cli_notebook_is("plain.py", "print(1)\n", 9ull), "and a plain script is not");
}

// a notebook of `count` cells, each a kind and a source, read into the tree: its node
static unsigned int notebook_of(const char *const (*cells)[2], unsigned int count)
{
    char text[2048];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(&line, "{\"cells\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? ",{\"cell_type\":" : "{\"cell_type\":");
        cli_json_quoted(&line, cells[at][0],
                        cellul.len(&(CatenaFinitaCfg){.src = cells[at][0], .cap = CLI_CELLS_POOL}));
        cli_line_text(&line, ",\"source\":");
        cli_json_quoted(&line, cells[at][1],
                        cellul.len(&(CatenaFinitaCfg){.src = cells[at][1], .cap = CLI_CELLS_POOL}));
        cli_line_text(&line, ",\"metadata\":{}}");
    }
    cli_line_text(&line, "]}");
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_json_read(&s_json, text, line.at, s_token, sizeof(s_token) / sizeof(s_token[0]), &said);
}

// a cell's source as one text into `line`, a list of lines joined
static void notebook_source(unsigned int cell, CliLine *line)
{
    const unsigned int source = cli_json_member(&s_json, cell, "source");
    if (source == CLI_JSON_NONE)
    {
        cli_line_text(line, "gone");
        return;
    }
    if (cli_json_is(&s_json, source, CFG_JSON_STRING))
    {
        cli_line_text(line, cli_json_string(&s_json, source, NULL));
        return;
    }
    for (unsigned int at = 0u; cli_json_item(&s_json, source, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(line, cli_json_string(&s_json, cli_json_item(&s_json, source, at), NULL));
    }
}

// a merge's entries shown, each taken cell's source or `<ours|theirs>`, `;` between them, checked against `wanted`
static void notebook_shown(unsigned int merged, const char *wanted, const char *what)
{
    char text[1024];
    CliLine line = {text, sizeof(text), 0ull};
    const unsigned int entries = cli_json_member(&s_json, merged, "entries");
    for (unsigned int at = 0u; cli_json_item(&s_json, entries, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int entry = cli_json_item(&s_json, entries, at);
        const unsigned int take = cli_json_member(&s_json, entry, "take");
        cli_line_text(&line, (at > 0u) ? ";" : "");
        if (take != CLI_JSON_NONE)
        {
            notebook_source(take, &line);
            continue;
        }
        const unsigned int conflict = cli_json_member(&s_json, entry, "conflict");
        cli_line_character(&line, '<');
        notebook_source(cli_json_member(&s_json, conflict, "ours"), &line);
        cli_line_character(&line, '|');
        notebook_source(cli_json_member(&s_json, conflict, "theirs"), &line);
        cli_line_character(&line, '>');
    }
    cli_check_text(text, line.at, wanted, what);
}

// a merge takes each side's changes and holds the cells both changed apart
static void notebook_merge(void)
{
    static const char *const s_base[4][2] = {
        {"code", "a = 1"}, {"code", "b = 2\nprint(b)"}, {"code", "c = 3\nprint(c)"}, {"markdown", "note"}};
    static const char *const s_ours[5][2] = {{"code", "a = 1"},
                                             {"code", "b = 20\nprint(b)"},
                                             {"code", "c = 3\nprint(c)"},
                                             {"markdown", "note"},
                                             {"code", "d = 4"}};
    static const char *const s_theirs[5][2] = {{"code", "e = 5"},
                                               {"code", "a = 1"},
                                               {"code", "b = 2\nprint(b)"},
                                               {"code", "c = 30\nprint(c)"},
                                               {"markdown", "note"}};
    static const char *const s_both[3][2] = {
        {"code", "a = 1"}, {"code", "b = 22\nprint(b)"}, {"code", "c = 3\nprint(c)"}};
    static const char *const s_deleted[2][2] = {{"code", "a = 1"}, {"code", "b = 2\nprint(b)"}};
    static const char *const s_changed_c[4][2] = {
        {"code", "a = 1"}, {"code", "b = 2\nprint(b)"}, {"code", "c = 33\nprint(c)"}, {"markdown", "note"}};
    (void)notebook_tree();
    const unsigned int base = notebook_of(s_base, 4u);
    const unsigned int ours = notebook_of(s_ours, 5u);
    const unsigned int theirs = notebook_of(s_theirs, 5u);
    notebook_shown(cli_notebook_merge(&s_json, base, ours, theirs),
                   "e = 5;a = 1;b = 20\nprint(b);c = 30\nprint(c);note;d = 4", "a merge takes each side's changes");
    const unsigned int both = notebook_of(s_both, 3u);
    notebook_shown(cli_notebook_merge(&s_json, base, ours, both),
                   "a = 1;<b = 20\nprint(b)|b = 22\nprint(b)>;c = 3\nprint(c);d = 4",
                   "and holds a cell both changed apart");
    const unsigned int deleted = notebook_of(s_deleted, 2u);
    const unsigned int changed_c = notebook_of(s_changed_c, 4u);
    notebook_shown(cli_notebook_merge(&s_json, base, changed_c, deleted),
                   "a = 1;b = 2\nprint(b);<c = 33\nprint(c)|gone>", "and a cell one changed and the other deleted");
}

int main(void)
{
    notebook_book();
    notebook_script();
    notebook_merge();
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_notebook_for_diff(notebook_tree(), s_book, sizeof(s_book) - 1u, s_token,
                                sizeof(s_token) / sizeof(s_token[0]), &out, &said);
    cli_check_text(s_out, out.at, "# cell 1 (code)\nprint('hi')\nx = 1\n## stream\nhi\n# cell 2 (markdown)\n# Title\n",
                   "a diff reads sources and outputs");
    out.at = 0ull;
    (void)cli_notebook_without_outputs(notebook_tree(), s_book, sizeof(s_book) - 1u, s_token,
                                       sizeof(s_token) / sizeof(s_token[0]), &out, &said);
    (void)cli_line_closed(&out);
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"execution_count\": null", .other_cap = CLI_CELLS_POOL}) !=
         NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"outputs\": []", .other_cap = CLI_CELLS_POOL}) !=
             NULL),
        "and no run counts");
    return cli_checked();
}
