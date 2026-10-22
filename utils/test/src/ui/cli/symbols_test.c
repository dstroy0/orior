// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_test.c: the tree's symbols' check: Python's, Rust's, scripts', C's and the other languages' declarations are
// read; scores rank the name itself first; a first reading says how far it has gone and a stop keeps what it read; and
// the index reads a tree and reads a file again once it changes
#include "../../../../../ui/cli/src/code/symbols.h"
#include "cli_check.h"

static char s_out[1u << 20u];

// a declaration put after the others, `name kind line` joined by `|`
static void symbols_kept(void *context, const char *name, unsigned long long length, const char *kind,
                         unsigned int line)
{
    CliLine *const out = context;
    cli_line_text(out, (out->at > 0ull) ? "|" : "");
    cli_line_counted(out, name, length);
    cli_line_character(out, ' ');
    cli_line_text(out, kind);
    cli_line_character(out, ' ');
    cli_line_decimal(out, line, 1u);
}

// the declarations of `text` read as the file `path`, checked against `wanted`
static void symbols_names(const char *path, const char *text, const char *wanted, const char *what)
{
    char out[1024];
    CliLine line = {out, sizeof(out), 0ull};
    cli_symbols_declared(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), symbols_kept,
                         &line);
    cli_check_text(out, line.at, wanted, what);
}

// the score `query` gives `name`, -1 where it does not name it
static long long symbols_score(const char *query, const char *name)
{
    long long points = 0ll;
    return cli_symbols_score(query, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), &points)
               ? points
               : -1ll;
}

// the symbols answering `query`, at most `most`, read into the shared tree: the array's node, `reading` set where the
// index was still being read
static unsigned int symbols_found(const char *query, unsigned int most, const CliJson **json, int *reading)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_symbols_find(query, most, &out);
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    *reading = cli_json_is(*json, cli_json_member(*json, found, "reading"), CFG_JSON_TRUE);
    return cli_json_member(*json, found, "symbols");
}

// a first reading says how far it has gone and a stop keeps what it read
static void symbols_stop(void)
{
    static char s_names[3000][16];
    static const char *s_paths[3000];
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("symbols_stop", root);
    for (unsigned int at = 0u; at < 3000u; at += 1u)
    {
        CliLine name = {s_names[at], sizeof(s_names[at]), 0ull};
        cli_line_character(&name, 'f');
        cli_line_decimal(&name, at, 4u);
        cli_line_text(&name, ".py");
        (void)cli_line_closed(&name);
        s_paths[at] = s_names[at];
        char text[64];
        CliLine line = {text, sizeof(text), 0ull};
        cli_line_text(&line, "def step_");
        cli_line_decimal(&line, at, 1u);
        cli_line_text(&line, "():\n    pass\n");
        cli_full(root, s_names[at], path);
        (void)query_file_written(path, text, line.at);
    }
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 3000u);
    unsigned long long done = 0ull;
    unsigned long long total = 0ull;
    int seen = 0;
    for (unsigned int tries = 0u; !seen && (tries < 500u); tries += 1u)
    {
        seen = cli_symbols_progress(&done, &total) && (done > 0ull);
        query_paused(1u);
    }
    cli_check(seen && (total == 3000ull) && (done <= total), "the reading says how far it has gone");
    cli_symbols_stop();
    for (unsigned int tries = 0u; cli_symbols_progress(&done, &total) && (tries < 500u); tries += 1u)
    {
        query_paused(2u);
    }
    cli_check(!cli_symbols_progress(&done, &total), "the reading ended");
    cli_check(!cli_symbols_unread(), "and is no longer the first reading");
    const CliJson *json = NULL;
    int reading = 0;
    const unsigned int kept = symbols_found("step_", 3000u, &json, &reading);
    cli_check((json->node[kept].count > 0u) && (json->node[kept].count < 3000u), "what it read stays, and no further");
    cli_check(json->node[symbols_found("step_0", 1u, &json, &reading)].count == 1u, "Go to Symbol answers from it");
}

// the index waited for until it is no longer being read
static void symbols_read_waited(void)
{
    const CliJson *json = NULL;
    int reading = 1;
    for (unsigned int tries = 0u; tries < 200u; tries += 1u)
    {
        (void)symbols_found("", 1u, &json, &reading);
        if (!reading)
        {
            return;
        }
        query_paused(10u);
    }
}

// the index reads a tree and reads a file again once it changes
static void symbols_index(void)
{
    static const char *const s_first[2] = {"a.py", "notes.txt"};
    static const char *const s_again[1] = {"a.py"};
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char text[512];
    cli_check_folder("symbols_index", root);
    cli_full(root, "a.py", path);
    (void)query_file_written(path, "def first():\n    pass\n", 22ull);
    cli_symbols_forget();
    cli_symbols_refresh(root, s_first, 2u);
    symbols_read_waited();
    const CliJson *json = NULL;
    int reading = 0;
    unsigned int found = symbols_found("first", 10u, &json, &reading);
    CliLine line = {text, sizeof(text), 0ull};
    cli_json_write(json, found, &line, 0u, 0);
    cli_check_text(
        text, line.at,
        "[{\"name\":\"first\",\"kind\":\"function\",\"path\":\"a.py\",\"line\":0,\"text\":\"def first():\"}]",
        "the index reads a tree");
    query_paused(20u);
    (void)query_file_written(path, "\ndef second():\n    pass\n", 24ull);
    // a tree is brought up to date again once four seconds have passed since the last
    query_paused(4100u);
    cli_symbols_refresh(root, s_again, 1u);
    symbols_read_waited();
    cli_check(json->node[symbols_found("first", 10u, &json, &reading)].count == 0u,
              "a file that changed is read again");
    found = symbols_found("second", 10u, &json, &reading);
    line.at = 0ull;
    cli_json_write(json, cli_json_member(json, cli_json_item(json, found, 0u), "line"), &line, 0u, 0);
    cli_check_text(text, line.at, "1", "with its new declarations");
}

int main(void)
{
    symbols_names(
        "x.py",
        "import os\nLIMIT = 3\nlimit = 4\nX: int = 5\nclass Page:\n    def read(self):\n        pass\nasync def "
        "go():\n    if a == b:\n        pass\n",
        "LIMIT constant 1|X constant 3|Page class 4|read function 5|go function 7", "Python's declarations");
    symbols_names(
        "x.rs",
        "pub(crate) fn alpha() {}\npub struct Beta;\nconst GAMMA: u32 = 1;\nmacro_rules! delta {\n}\npub async "
        "unsafe fn epsilon() {}\nmod zeta;\nimpl Beta {\nextern \"C\" fn eta() {}\nstatic mut THETA: u8 = 0;\n",
        "alpha function 0|Beta class 1|GAMMA constant 2|delta function 3|epsilon function 5|zeta module 6|eta "
        "function 8|THETA constant 9",
        "Rust's declarations");
    symbols_names("x.js",
                  "export async function load(path) {\nclass Editor {\n  paint() {\n    if (x) {\n  }\nconst STEP = "
                  "4;\nexport const go = (a) => a;\nlet run = async () => 1;\n",
                  "load function 0|Editor class 1|paint method 2|STEP constant 5|go function 6|run function 7",
                  "a script's declarations");
    symbols_names(
        "x.c",
        "#define LIMIT 4\n#  define TWICE(x) ((x) * 2)\nstruct grid {\ntypedef enum shade\nstatic int "
        "count_cells(const struct grid *g) {\nint scale(int a,\n  return helper(1);\nint value = make(2);\nvoid "
        "declared(int);\nif (x) {\n",
        "LIMIT constant 0|TWICE constant 1|grid class 2|shade class 3|count_cells function 4|scale function 5",
        "C's declarations");
    symbols_names("x.sh", "build() {\nfunction clean {\necho hi\n", "build function 0|clean function 1", "a shell's");
    symbols_names("x.ps1", "Function Get-Thing {\n", "Get-Thing function 0", "PowerShell's");
    symbols_names("x.toml", "[package]\n[[bin]]\nname = 1\n", "package module 0|bin module 1", "TOML's tables");
    symbols_names("x.md", "# Title\n```\n# not this\n```\n## Part two ##\n", "Title heading 0|Part two heading 4",
                  "Markdown's headings, none in code");
    symbols_names("x.tex", "\\section*{Start}\n\\subsection[short]{Long}\n", "Start heading 0|Long heading 1",
                  "TeX's sections");
    const long long exact = symbols_score("paint", "paint");
    const long long begins = symbols_score("paint", "paintStrip");
    const long long holds = symbols_score("paint", "repaint");
    const long long initials = symbols_score("gtf", "go_to_file");
    const long long camel = symbols_score("gtf", "goToFile");
    const long long letters = symbols_score("pnt", "paint");
    cli_check((exact > begins) && (begins > holds) && (holds > initials) && (initials > letters),
              "scores rank the name itself first");
    cli_check((initials == symbols_score("gtf", "go_to_file")) && (camel > letters),
              "a word's start ranks over letters");
    cli_check(symbols_score("xyz", "paint") == -1ll, "and a query that names none does not answer");
    symbols_stop();
    symbols_index();
    return cli_checked();
}
