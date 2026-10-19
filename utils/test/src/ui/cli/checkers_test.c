// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers_test.c: the checkers' check: findings written a line each are read as flake8, mypy and pylint write them,
// ruff's findings are read with their fixes, and eslint's and shellcheck's findings are read
#include "../../../../../ui/cli/src/toolchains/checkers_internal.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static CliFindings s_findings;
static char s_said[4096];

// the findings `out` holds, read as the checker that reads `reads` writes them, each said as `file line:col sN code
// message`, `|` between them, into s_said
static const char *checkers_said(const char *reads, const char *out, const char *root, const char *path)
{
    cli_checkers_read(reads, reads, out, cellul.len(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL}), "", 0ull,
                      root, path, &s_findings);
    CliLine said = {s_said, sizeof(s_said), 0ull};
    for (unsigned int at = 0u; at < s_findings.findings; at += 1u)
    {
        const char *const file = s_findings.path[s_findings.file_of[at]];
        const char *const slash =
            cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/');
        const char *const backslash =
            cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\');
        const char *const name = (backslash > slash) ? backslash + 1 : ((slash != NULL) ? slash + 1 : file);
        unsigned int item = CLI_JSON_NONE;
        const CliJson *const json =
            cli_check_parsed(s_findings.text + s_findings.start[at], s_findings.length[at], &item);
        const unsigned int start = cli_json_member(json, cli_json_member(json, item, "range"), "start");
        cli_line_text(&said, (at > 0u) ? "|" : "");
        cli_line_text(&said, name);
        cli_line_character(&said, ' ');
        cli_json_write(json, cli_json_member(json, start, "line"), &said, 0u, 0);
        cli_line_character(&said, ':');
        cli_json_write(json, cli_json_member(json, start, "character"), &said, 0u, 0);
        cli_line_text(&said, " s");
        cli_json_write(json, cli_json_member(json, item, "severity"), &said, 0u, 0);
        cli_line_character(&said, ' ');
        cli_line_text(&said, cli_check_member_text(item, "code"));
        cli_line_character(&said, ' ');
        cli_line_text(&said, cli_check_member_text(item, "message"));
    }
    (void)cli_line_closed(&said);
    return s_said;
}

// findings written a line each are read as flake8, mypy and pylint write them
static void checkers_lines(void)
{
#if defined(_WIN32)
    static const char s_root[] = "D:/tree";
    static const char s_whole[] = "D:\\tree\\b.py";
#else
    static const char s_root[] = "/tree";
    static const char s_whole[] = "/tree/b.py";
#endif
    char out[1024];
    CliLine line = {out, sizeof(out), 0ull};
    cli_line_text(&line, "a.py:3:5: E501 line too long (90 > 79 characters)\n");
    cli_line_text(&line, s_whole);
    cli_line_text(&line, ":7:1: error: Incompatible return value type  [return-value]\nb.py:8:2: note: See the docs\n"
                         "c.py:1: [C0114(missing-module-docstring), ] Missing module docstring\nnot a finding\n");
    (void)cli_line_closed(&line);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = checkers_said("lines", out, s_root, NULL),
            .other = "a.py 2:4 s2 E501 line too long (90 > 79 characters)|b.py 6:0 s1 return-value Incompatible return "
                     "value type|b.py 7:1 s3  See the docs|c.py 0:0 s2 C0114 Missing module docstring",
            .cap = CLI_CELLS_POOL}),
        "findings written a line each are read as flake8, mypy and pylint write them");
}

// ruff's findings are read with their fixes
static void checkers_ruff(void)
{
    static const char s_out[] =
        "[{\"filename\":\"-\",\"location\":{\"row\":1,\"column\":8},\"end_location\":{\"row\":1,\"column\":10},"
        "\"code\":\"F401\",\"message\":\"`os` imported but unused\",\"fix\":{\"message\":\"Remove unused import: "
        "`os`\","
        "\"edits\":[{\"content\":\"\",\"location\":{\"row\":1,\"column\":1},\"end_location\":{\"row\":2,\"column\":1}}]"
        "}}]";
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = checkers_said("ruff", s_out, "D:/tree", "D:/tree/a.py"),
                                           .other = "a.py 0:7 s2 F401 `os` imported but unused",
                                           .cap = CLI_CELLS_POOL}),
              "ruff's findings are read");
    unsigned int item = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_findings.text + s_findings.start[0], s_findings.length[0], &item);
    const unsigned int fix =
        cli_json_item(json, cli_json_member(json, cli_json_member(json, item, "data"), "orior"), 0u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(fix, "title"),
                                           .other = "Remove unused import: `os`",
                                           .cap = CLI_CELLS_POOL}),
              "with their fixes' titles");
    char edit[256];
    CliLine line = {edit, sizeof(edit), 0ull};
    cli_json_write(json, cli_json_item(json, cli_json_member(json, fix, "edits"), 0u), &line, 0u, 0);
    cli_check_text(edit, line.at, "{\"from\":{\"line\":0,\"col\":0},\"to\":{\"line\":1,\"col\":0},\"text\":\"\"}",
                   "and their edits");
}

int main(void)
{
    checkers_lines();
    checkers_ruff();
    static const char s_eslint[] =
        "[{\"filePath\":\"D:/tree/a.js\",\"messages\":[{\"ruleId\":\"no-unused-vars\",\"severity\":2,\"message\":\"'x' "
        "is assigned a value but never used.\",\"line\":2,\"column\":7,\"endLine\":2,\"endColumn\":8}]}]";
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = checkers_said("eslint", s_eslint, "D:/tree", NULL),
                                     .other = "a.js 1:6 s1 no-unused-vars 'x' is assigned a value but never used.",
                                     .cap = CLI_CELLS_POOL}),
        "eslint's findings are read");
    static const char s_shellcheck[] =
        "{\"comments\":[{\"file\":\"-\",\"line\":3,\"endLine\":3,\"column\":6,\"endColumn\":10,\"level\":\"warning\","
        "\"code\":2086,\"message\":\"Double quote to prevent globbing and word splitting.\"}]}";
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = checkers_said("shellcheck", s_shellcheck, "D:/tree", "D:/tree/run.sh"),
                  .other = "run.sh 2:5 s2 SC2086 Double quote to prevent globbing and word splitting.",
                  .cap = CLI_CELLS_POOL}),
              "and shellcheck's");
    return cli_checked();
}
