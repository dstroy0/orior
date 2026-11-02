// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_python_paths.c: the folders a script puts on its path are read, expressions parse as Python writes them, and a path's parent
// reads in both Windows and POSIX forms
#include "ui/cli/src/code/python_paths_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_dir[CLI_PATH_MAX_LEN];
static char s_file[CLI_PATH_MAX_LEN];

// what the script `text` puts on its path is the `count` folders `wanted` of the tree, in order; `folders` skips what is not a folder
static void assert_paths(const char *const *wanted, unsigned int count, const char *text, int folders)
{
    EMBED_CALL(cli_python_paths_size_internal.py_paths_in, CliPythonPathsSizeCtx, .file = s_file, .text = text, .size = CLI_UNITY_LENGTH(text));
    unsigned int matched = 0u;
    for (unsigned int at = 0u; at < g_cli_py.founds; at += 1u)
    {
        char full[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = g_cli_py.found[at]), .full = full, .room = sizeof(full)) || (folders && !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = full)))
        {
            continue;
        }
        TEST_ASSERT_LESS_THAN_UINT(count, matched);
        char joined[CLI_PATH_MAX_LEN];
        char expected[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = wanted[matched], .joined = joined);
        TEST_ASSERT_TRUE(EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = joined, .full = expected, .room = sizeof(expected)));
        TEST_ASSERT_EQUAL_STRING(expected, full);
        matched += 1u;
    }
    cli_py_reading_free();
    TEST_ASSERT_EQUAL_UINT(count, matched);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_python_cells.in_use));
}

// the parent of `path` is `wanted`
static void assert_parent(const char *wanted, const char *path)
{
    char up[CLI_PATH_MAX_LEN];
    VerbaLinea line = {up, sizeof(up), 0ull};
    EMBED_CALL(cli_python_paths_out_internal.py_parent, CliPythonPathsOutCtx, .path = path, .out = &line);
    CLI_TEST_ASSERT_TEXT(wanted, up, line.at);
}

void setUp(void)
{
    static const char *const s_folders[6] = {"src/python/pkg", "examples/art/measure", "examples/proofing", "tools/plugins/one", "tools/plugins/two", "shared"};
    cli_unity_begun();
    cli_unity_folder("python_paths", s_dir);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        char folder[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = s_folders[at], .joined = folder);
        TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder));
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "examples/art/measure/a.py", .joined = s_file);
}

void tearDown(void)
{
}

void test_a_folder_found_by_walking_up_is_read(void)
{
    static const char *const s_wanted[1] = {"src/python"};
    assert_paths(s_wanted, 1u,
                 "import os, sys\nROOT = os.path.dirname(os.path.abspath(__file__))\nwhile not "
                 "os.path.isdir(os.path.join(ROOT, \"src\", \"python\")):\n    ROOT = os.path.dirname(ROOT)\n"
                 "sys.path.insert(0, os.path.join(ROOT, \"src\", \"python\"))\n",
                 1);
}

void test_the_folders_of_a_loop_over_named_folders_are_read(void)
{
    static const char *const s_wanted[2] = {"examples/art/measure", "examples/proofing"};
    assert_paths(s_wanted, 2u,
                 "HERE = os.path.dirname(os.path.abspath(__file__))\nROOT = os.path.dirname(os.path.dirname(HERE))\n"
                 "PROOFING = os.path.join(ROOT, \"proofing\")\nfor _where in (HERE, PROOFING):\n    if _where not in "
                 "sys.path:\n        sys.path.insert(0, _where)\n",
                 0);
}

void test_pathlib_parents_and_a_split_path_are_read(void)
{
    static const char *const s_wanted[2] = {"examples/proofing", "examples/art/measure"};
    assert_paths(s_wanted, 2u,
                 "from pathlib import Path\nsys.path.append(str(Path(__file__).resolve().parents[2] / \"proofing\"))\n"
                 "sys.path.insert(\n    0,\n    __file__.rsplit(\"\\\\\", 1)[0].rsplit(\"/\", 1)[0],\n)\n",
                 0);
}

void test_the_folders_a_folder_scan_finds_are_read(void)
{
    static const char *const s_wanted[2] = {"tools/plugins/one", "tools/plugins/two"};
    static char s_scanned[CLI_PATH_MAX_LEN * 2u];
    char plugins[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "tools/plugins", .joined = plugins);
    VerbaLinea scanned = {s_scanned, sizeof(s_scanned), 0ull};
    mmgr_line_put(&scanned, "for _category in os.scandir(");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &scanned, .text = plugins, .length = CLI_UNITY_LENGTH(plugins));
    mmgr_line_put(&scanned, "):\n    if _category.is_dir():\n        sys.path.insert(0, _category.path)\n");
    (void)mmgr_line_finish(&scanned);
    assert_paths(s_wanted, 2u, s_scanned, 0);
}

void test_a_call_parses_with_its_three_arguments(void)
{
    static const char s_call[] = "os.path.join(ROOT, 'a', \"b\")";
    TEST_ASSERT_TRUE(EMBED_CALL(cli_python_paths_size_internal.py_reading_take, CliPythonPathsSizeCtx, .size = sizeof(s_call)));
    const unsigned int expr = EMBED_CALL(cli_python_paths_text_internal.py_parse, CliPythonPathsTextCtx, .text = s_call, .length = sizeof(s_call) - 1u);
    TEST_ASSERT_NOT_EQUAL(CLI_PY_NONE, expr);
    TEST_ASSERT_EQUAL_INT(CLI_EXPR_CALL, g_cli_py.expr[expr].kind);
    TEST_ASSERT_EQUAL_UINT(3u, g_cli_py.expr[expr].count);
    cli_py_reading_free();
}

void test_an_f_string_does_not_parse(void)
{
    static const char s_text[] = "f\"{ROOT}/x\"";
    TEST_ASSERT_TRUE(EMBED_CALL(cli_python_paths_size_internal.py_reading_take, CliPythonPathsSizeCtx, .size = sizeof(s_text)));
    TEST_ASSERT_EQUAL_UINT(CLI_PY_NONE, EMBED_CALL(cli_python_paths_text_internal.py_parse, CliPythonPathsTextCtx, .text = s_text, .length = sizeof(s_text) - 1u));
    cli_py_reading_free();
}

void test_a_tuple_parses_with_its_two_items(void)
{
    static const char s_text[] = "(HERE, PROOFING)";
    TEST_ASSERT_TRUE(EMBED_CALL(cli_python_paths_size_internal.py_reading_take, CliPythonPathsSizeCtx, .size = sizeof(s_text)));
    const unsigned int expr = EMBED_CALL(cli_python_paths_text_internal.py_parse, CliPythonPathsTextCtx, .text = s_text, .length = sizeof(s_text) - 1u);
    TEST_ASSERT_NOT_EQUAL(CLI_PY_NONE, expr);
    TEST_ASSERT_EQUAL_INT(CLI_EXPR_MANY, g_cli_py.expr[expr].kind);
    TEST_ASSERT_EQUAL_UINT(2u, g_cli_py.expr[expr].count);
    cli_py_reading_free();
}

void test_the_parent_of_a_windows_path(void)
{
    assert_parent("D:\\a", "D:\\a\\b");
}

void test_the_parent_of_a_folder_on_a_windows_drive_is_the_drive(void)
{
    assert_parent("D:\\", "D:\\a");
}

void test_the_parent_of_a_posix_path(void)
{
    assert_parent("/a", "/a/b");
}
