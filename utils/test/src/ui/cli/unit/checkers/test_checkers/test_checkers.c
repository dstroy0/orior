// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_checkers.c: findings written a line each are read as flake8, mypy and pylint write them, ruff's are read with their fixes, and
// eslint's and shellcheck's findings are read
#include "ui/cli/src/toolchains/checkers_internal.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// a tree's root and a whole path under it as the platform writes them
#if defined(_WIN32)
#define CHECKERS_ROOT "D:/tree"
#define CHECKERS_WHOLE "D:\\tree\\b.py"
#else
#define CHECKERS_ROOT "/tree"
#define CHECKERS_WHOLE "/tree/b.py"
#endif

static const char s_ruff[] = "[{\"filename\":\"-\",\"location\":{\"row\":1,\"column\":8},\"end_location\":{\"row\":1,\"column\":10},"
                             "\"code\":\"F401\",\"message\":\"`os` imported but unused\",\"fix\":{\"message\":\"Remove unused import: "
                             "`os`\","
                             "\"edits\":[{\"content\":\"\",\"location\":{\"row\":1,\"column\":1},\"end_location\":{\"row\":2,\"column\":1}}]"
                             "}}]";

static CliFindings s_findings;
static char s_findings_text[4096];

// the findings `out` contains, read as the checker `reads` writes them, each written as `file line:col sN code message`, `|` between
static const char *findings_written(const char *reads, const char *out, const char *root, const char *path)
{
    EMBED_CALL(cli_checkers_name_internal.read, CliCheckersNameCtx, .name = reads, .reads = reads, .out = out, .out_size = CLI_UNITY_LENGTH(out), .err = "", .err_size = 0ull, .root = root, .path = path, .findings = &s_findings);
    VerbaLinea line = {s_findings_text, sizeof(s_findings_text), 0ull};
    for (unsigned int at = 0u; at < s_findings.findings; at += 1u)
    {
        const char *const file = s_findings.path[s_findings.file_of[at]];
        const char *const slash = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = file, .length_value = CLI_UNITY_LENGTH(file), .byte = '/');
        const char *const backslash = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = file, .length_value = CLI_UNITY_LENGTH(file), .byte = '\\');
        const char *const name = (backslash > slash) ? backslash + 1 : ((slash != NULL) ? slash + 1 : file);
        unsigned int item = CLI_JSON_NONE;
        const CliJson *const json = cli_unity_parsed(s_findings.text + s_findings.start[at], s_findings.length[at], &item);
        const unsigned int start = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = item, .name = "range"), .name = "start");
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, name);
        mmgr_line_ch(&line, ' ');
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = start, .name = "line"), .out = &line, .indent = 0u, .sorted = 0);
        mmgr_line_ch(&line, ':');
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = start, .name = "character"), .out = &line, .indent = 0u, .sorted = 0);
        mmgr_line_put(&line, " s");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = item, .name = "severity"), .out = &line, .indent = 0u, .sorted = 0);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, cli_unity_member_text(item, "code"));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, cli_unity_member_text(item, "message"));
    }
    (void)mmgr_line_finish(&line);
    return s_findings_text;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_findings_written_a_line_each_are_read(void)
{
    static const char s_out[] = "a.py:3:5: E501 line too long (90 > 79 characters)\n" CHECKERS_WHOLE ":7:1: error: Incompatible return value type  [return-value]\nb.py:8:2: note: See the docs\n"
                                "c.py:1: [C0114(missing-module-docstring), ] Missing module docstring\nnot a finding\n";
    TEST_ASSERT_EQUAL_STRING("a.py 2:4 s2 E501 line too long (90 > 79 characters)|b.py 6:0 s1 return-value Incompatible return "
                             "value type|b.py 7:1 s3  See the docs|c.py 0:0 s2 C0114 Missing module docstring",
                             findings_written("lines", s_out, CHECKERS_ROOT, NULL));
}

void test_ruffs_findings_are_read(void)
{
    TEST_ASSERT_EQUAL_STRING("a.py 0:7 s2 F401 `os` imported but unused", findings_written("ruff", s_ruff, CHECKERS_ROOT, CHECKERS_ROOT "/a.py"));
}

void test_ruffs_fix_title_is_read(void)
{
    (void)findings_written("ruff", s_ruff, CHECKERS_ROOT, CHECKERS_ROOT "/a.py");
    unsigned int item = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_findings.text + s_findings.start[0], s_findings.length[0], &item);
    const unsigned int fix = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = item, .name = "data"), .name = "orior"), .index = 0u);
    TEST_ASSERT_EQUAL_STRING("Remove unused import: `os`", cli_unity_member_text(fix, "title"));
}

void test_ruffs_fix_edits_are_read(void)
{
    (void)findings_written("ruff", s_ruff, CHECKERS_ROOT, CHECKERS_ROOT "/a.py");
    unsigned int item = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_findings.text + s_findings.start[0], s_findings.length[0], &item);
    const unsigned int fix = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = item, .name = "data"), .name = "orior"), .index = 0u);
    char edit[256];
    VerbaLinea line = {edit, sizeof(edit), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = fix, .name = "edits"), .index = 0u), .out = &line, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT("{\"from\":{\"line\":0,\"col\":0},\"to\":{\"line\":1,\"col\":0},\"text\":\"\"}", edit, line.at);
}

void test_eslints_findings_are_read(void)
{
    static const char s_eslint[] = "[{\"filePath\":\"" CHECKERS_ROOT "/a.js\",\"messages\":[{\"ruleId\":\"no-unused-vars\",\"severity\":2,\"message\":\"'x' "
                                   "is assigned a value but never used.\",\"line\":2,\"column\":7,\"endLine\":2,\"endColumn\":8}]}]";
    TEST_ASSERT_EQUAL_STRING("a.js 1:6 s1 no-unused-vars 'x' is assigned a value but never used.", findings_written("eslint", s_eslint, CHECKERS_ROOT, NULL));
}

void test_shellchecks_findings_are_read(void)
{
    static const char s_shellcheck[] = "{\"comments\":[{\"file\":\"-\",\"line\":3,\"endLine\":3,\"column\":6,\"endColumn\":10,\"level\":\"warning\","
                                       "\"code\":2086,\"message\":\"Double quote to prevent globbing and word splitting.\"}]}";
    TEST_ASSERT_EQUAL_STRING("run.sh 2:5 s2 SC2086 Double quote to prevent globbing and word splitting.", findings_written("shellcheck", s_shellcheck, CHECKERS_ROOT, CHECKERS_ROOT "/run.sh"));
}
