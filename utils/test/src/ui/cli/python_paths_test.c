// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_test.c: the Python paths' check: the folders a script puts on its path are read, and expressions read
// as Python writes them
#include "../../../../../ui/cli/src/code/python_paths_internal.h"
#include "cli_check.h"

static char s_dir[CLI_PATH_LONGEST];
static char s_file[CLI_PATH_LONGEST];

// what the script `text` puts on its path, each made whole and kept where it is a folder, where `folders`, checked
// against the folders `wanted` of the tree, `count` of them, in order
static void paths_checked(const char *text, int folders, const char *const *wanted, unsigned int count,
                          const char *what)
{
    cli_py_paths_in(s_file, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    unsigned int kept = 0u;
    int same = 1;
    for (unsigned int at = 0u; at < g_cli_py.founds; at += 1u)
    {
        char full[CLI_PATH_LONGEST];
        if (!query_path_full(cli_py_text(g_cli_py.found[at]), full, sizeof(full)) ||
            (folders && !query_is_folder(full)))
        {
            continue;
        }
        char expected[CLI_PATH_LONGEST];
        char joined[CLI_PATH_LONGEST];
        if (kept < count)
        {
            cli_full(s_dir, wanted[kept], joined);
            (void)query_path_full(joined, expected, sizeof(expected));
        }
        same &=
            (kept < count) && (cellul.eq(&(CatenaFinitaCfg){.src = full, .other = expected, .cap = CLI_CELLS_POOL}));
        kept += 1u;
    }
    cli_check(same && (kept == count), what);
}

// the parent of `path` checked against `wanted`
static void parent_checked(const char *path, const char *wanted)
{
    char up[CLI_PATH_LONGEST];
    CliLine line = {up, sizeof(up), 0ull};
    cli_py_parent(path, &line);
    char what[128];
    CliLine named = {what, sizeof(what), 0ull};
    cli_line_text(&named, "the folder of ");
    cli_line_text(&named, path);
    (void)cli_line_closed(&named);
    cli_check_text(up, line.at, wanted, what);
}

int main(void)
{
    static const char *const s_folders[6] = {"src/python/pkg",    "examples/art/measure", "examples/proofing",
                                             "tools/plugins/one", "tools/plugins/two",    "shared"};
    cli_check_folder("python_paths", s_dir);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        char folder[CLI_PATH_LONGEST];
        cli_full(s_dir, s_folders[at], folder);
        (void)query_folder_made(folder);
    }
    cli_full(s_dir, "examples/art/measure/a.py", s_file);
    static const char *const s_walked_wanted[1] = {"src/python"};
    paths_checked("import os, sys\nROOT = os.path.dirname(os.path.abspath(__file__))\nwhile not "
                  "os.path.isdir(os.path.join(ROOT, \"src\", \"python\")):\n    ROOT = os.path.dirname(ROOT)\n"
                  "sys.path.insert(0, os.path.join(ROOT, \"src\", \"python\"))\n",
                  1, s_walked_wanted, 1u, "a folder walked up to is read");
    static const char *const s_named_wanted[2] = {"examples/art/measure", "examples/proofing"};
    paths_checked("HERE = os.path.dirname(os.path.abspath(__file__))\nROOT = os.path.dirname(os.path.dirname(HERE))\n"
                  "PROOFING = os.path.join(ROOT, \"proofing\")\nfor _where in (HERE, PROOFING):\n    if _where not in "
                  "sys.path:\n        sys.path.insert(0, _where)\n",
                  0, s_named_wanted, 2u, "the folders a loop over named folders puts are read");
    static const char *const s_pathlib_wanted[2] = {"examples/proofing", "examples/art/measure"};
    paths_checked("from pathlib import Path\nsys.path.append(str(Path(__file__).resolve().parents[2] / \"proofing\"))\n"
                  "sys.path.insert(\n    0,\n    __file__.rsplit(\"\\\\\", 1)[0].rsplit(\"/\", 1)[0],\n)\n",
                  0, s_pathlib_wanted, 2u, "pathlib's parents and a split path are read");
    char plugins[CLI_PATH_LONGEST];
    cli_full(s_dir, "tools/plugins", plugins);
    static char s_scanned[CLI_PATH_LONGEST * 2u];
    CliLine scanned = {s_scanned, sizeof(s_scanned), 0ull};
    cli_line_text(&scanned, "for _category in os.scandir(");
    cli_json_quoted(&scanned, plugins, cellul.len(&(CatenaFinitaCfg){.src = plugins, .cap = CLI_CELLS_POOL}));
    cli_line_text(&scanned, "):\n    if _category.is_dir():\n        sys.path.insert(0, _category.path)\n");
    (void)cli_line_closed(&scanned);
    static const char *const s_scanned_wanted[2] = {"tools/plugins/one", "tools/plugins/two"};
    paths_checked(s_scanned, 0, s_scanned_wanted, 2u, "the folders a scan of a folder finds are read");
    static const char s_call[] = "os.path.join(ROOT, 'a', \"b\")";
    unsigned int expr = cli_py_parse(s_call, sizeof(s_call) - 1u);
    cli_check((expr != CLI_PY_NONE) && (g_cli_py.expr[expr].kind == CLI_EXPR_CALL) && (g_cli_py.expr[expr].count == 3u),
              "a call reads with its three arguments");
    cli_check(cli_py_parse("f\"{ROOT}/x\"", 11ull) == CLI_PY_NONE, "an f-string is not read");
    expr = cli_py_parse("(HERE, PROOFING)", 16ull);
    cli_check((expr != CLI_PY_NONE) && (g_cli_py.expr[expr].kind == CLI_EXPR_MANY) && (g_cli_py.expr[expr].count == 2u),
              "a tuple reads with its two items");
    parent_checked("D:\\a\\b", "D:\\a");
    parent_checked("D:\\a", "D:\\");
    parent_checked("/a/b", "/a");
    return cli_checked();
}
