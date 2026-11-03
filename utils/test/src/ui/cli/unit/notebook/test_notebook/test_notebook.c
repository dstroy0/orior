// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_notebook.c: a notebook reads as cells and writes back unchanged, a percent script does the same, a merge takes each side's changes
// and separates the cells both changed, and a diff reads sources and outputs and no run counts
#include "ui/cli/src/notebooks/notebook.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_notebook[] = "{\n \"cells\": [\n  {\n   \"cell_type\": \"code\",\n   \"execution_count\": 3,\n   \"id\": \"a1\",\n"
                                 "   \"metadata\": {},\n   \"outputs\": [\n    {\n     \"name\": \"stdout\",\n     \"output_type\": \"stream\",\n"
                                 "     \"text\": [\n      \"hi\\n\"\n     ]\n    }\n   ],\n   \"source\": [\n    \"print('hi')\\n\",\n"
                                 "    \"x = 1\"\n   ]\n  },\n  {\n   \"cell_type\": \"markdown\",\n   \"id\": \"b2\",\n   \"metadata\": {},\n"
                                 "   \"source\": [\n    \"# Title\"\n   ]\n  }\n ],\n \"metadata\": {\n  \"kernelspec\": {\n"
                                 "   \"display_name\": \"Python 3\",\n   \"language\": \"python\",\n   \"name\": \"python3\"\n  }\n },\n"
                                 " \"nbformat\": 4,\n \"nbformat_minor\": 5\n}\n";
static const char s_script[] = "import os\n\n# %% Load\nx = 1\nprint(x)\n\n# %% [markdown]\n# A note\n#\n"
                               "# on two lines\n\n# %%\ny = x + 1\n";
static const char *const s_base[4][2] = {{"code", "a = 1"}, {"code", "b = 2\nprint(b)"}, {"code", "c = 3\nprint(c)"}, {"markdown", "note"}};
static const char *const s_ours[5][2] = {{"code", "a = 1"}, {"code", "b = 20\nprint(b)"}, {"code", "c = 3\nprint(c)"}, {"markdown", "note"}, {"code", "d = 4"}};
static const char *const s_theirs[5][2] = {{"code", "e = 5"}, {"code", "a = 1"}, {"code", "b = 2\nprint(b)"}, {"code", "c = 30\nprint(c)"}, {"markdown", "note"}};
static const char *const s_both[3][2] = {{"code", "a = 1"}, {"code", "b = 22\nprint(b)"}, {"code", "c = 3\nprint(c)"}};
static const char *const s_deleted[2][2] = {{"code", "a = 1"}, {"code", "b = 2\nprint(b)"}};
static const char *const s_changed_c[4][2] = {{"code", "a = 1"}, {"code", "b = 2\nprint(b)"}, {"code", "c = 33\nprint(c)"}, {"markdown", "note"}};

static CliJsonNode s_node[1u << 14u];
static char s_text[1u << 18u];
static CfgJsonToken s_token[1u << 14u];
static CliJson s_json;
static char s_out[1u << 16u];
static char s_error[256];

// the tree emptied
static CliJson *tree_emptied(void)
{
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &s_json, .node_json_node_pointer = s_node, .nodes_max = sizeof(s_node) / sizeof(s_node[0]), .text_out = s_text, .text_max = sizeof(s_text));
    return &s_json;
}

// the text `text` of the file `path` read into the tree: its node
static unsigned int read_as(const char *path, const char *text, unsigned long long size)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    return EMBED_CALL(cli_notebook_path.read, CliNotebookPathCfg, .json = tree_emptied(), .path = path, .text = text, .size = size, .tokens = s_token, .tokens_max = sizeof(s_token) / sizeof(s_token[0]), .err_msg = &err_msg);
}

// a string reached from `node` by `count` members or items, each a name or, where it starts with `#`, an index of one digit; the empty
// string where it has none
static const char *string_at(unsigned int node, const char *const *path, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        // a digit's value is below 10
        node = (path[at][0] == '#') ? EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = node, .index = (unsigned int)(path[at][1] - '0')) : EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = node, .name = path[at]);
    }
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = node, .length_pointer = NULL);
    return (text != NULL) ? text : "";
}

// a notebook of `count` cells, each a kind and a source, read into the tree: its node
static unsigned int notebook_of(const char *const (*cells)[2], unsigned int count)
{
    char text[2048];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "{\"cells\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? ",{\"cell_type\":" : "{\"cell_type\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = cells[at][0], .length = CLI_UNITY_LENGTH(cells[at][0]));
        mmgr_line_put(&line, ",\"source\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = cells[at][1], .length = CLI_UNITY_LENGTH(cells[at][1]));
        mmgr_line_put(&line, ",\"metadata\":{}}");
    }
    mmgr_line_put(&line, "]}");
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    return EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &s_json, .text = text, .size = line.at, .tokens = s_token, .tokens_max = sizeof(s_token) / sizeof(s_token[0]), .err_msg = &err_msg);
}

// a cell's source as one string into `line`, a list of lines joined
static void source_written(unsigned int cell, VerbaLinea *line)
{
    const unsigned int source = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = cell, .name = "source");
    if (source == CLI_JSON_NONE)
    {
        mmgr_line_put(line, "gone");
        return;
    }
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = source, .kind = CFG_JSON_STRING))
    {
        mmgr_line_put(line, EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = source, .length_pointer = NULL));
        return;
    }
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = source, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        mmgr_line_put(line, EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = source, .index = at), .length_pointer = NULL));
    }
}

// a merge's entries, each taken cell's source or `<ours|theirs>`, `;` between them, are `required`
static void assert_merged(const char *required, unsigned int merged)
{
    char text[1024];
    VerbaLinea line = {text, sizeof(text), 0ull};
    const unsigned int entries = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = merged, .name = "entries");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = entries, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int entry = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = entries, .index = at);
        const unsigned int take = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = entry, .name = "take");
        mmgr_line_put(&line, (at > 0u) ? ";" : "");
        if (take != CLI_JSON_NONE)
        {
            source_written(take, &line);
            continue;
        }
        const unsigned int conflict = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = entry, .name = "conflict");
        mmgr_line_ch(&line, '<');
        source_written(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = conflict, .name = "ours"), &line);
        mmgr_line_ch(&line, '|');
        source_written(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = conflict, .name = "theirs"), &line);
        mmgr_line_ch(&line, '>');
    }
    CLI_TEST_ASSERT_TEXT(required, text, line.at);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_cells_source_reads_as_one_string(void)
{
    static const char *const s_path[3] = {"cells", "#0", "source"};
    const unsigned int notebook = read_as("a.ipynb", s_notebook, sizeof(s_notebook) - 1u);
    TEST_ASSERT_EQUAL_STRING("print('hi')\nx = 1", string_at(notebook, s_path, 3u));
}

void test_a_cells_output_text_reads_as_one_string(void)
{
    static const char *const s_path[5] = {"cells", "#0", "outputs", "#0", "text"};
    const unsigned int notebook = read_as("a.ipynb", s_notebook, sizeof(s_notebook) - 1u);
    TEST_ASSERT_EQUAL_STRING("hi\n", string_at(notebook, s_path, 5u));
}

void test_a_notebook_writes_back_unchanged(void)
{
    const unsigned int notebook = read_as("a.ipynb", s_notebook, sizeof(s_notebook) - 1u);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_notebook_path_book.write, CliNotebookPathBookCfg, .json = &s_json, .path = "a.ipynb", .book = notebook, .out = &out);
    CLI_TEST_ASSERT_TEXT(s_notebook, s_out, out.at);
}

void test_a_script_stores_its_lines_before_a_cell(void)
{
    static const char *const s_path[1] = {"header"};
    const unsigned int notebook = read_as("s.py", s_script, sizeof(s_script) - 1u);
    TEST_ASSERT_EQUAL_STRING("import os\n\n", string_at(notebook, s_path, 1u));
}

void test_each_percent_mark_begins_a_cell(void)
{
    const unsigned int notebook = read_as("s.py", s_script, sizeof(s_script) - 1u);
    TEST_ASSERT_EQUAL_UINT(3u, s_json.node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = notebook, .name = "cells")].count);
}

void test_a_cells_mark_names_its_title(void)
{
    static const char *const s_path[4] = {"cells", "#0", "metadata", "title"};
    const unsigned int notebook = read_as("s.py", s_script, sizeof(s_script) - 1u);
    TEST_ASSERT_EQUAL_STRING("Load", string_at(notebook, s_path, 4u));
}

void test_a_markdown_cell_reads_from_behind_its_comments(void)
{
    static const char *const s_kind[3] = {"cells", "#1", "cell_type"};
    static const char *const s_source[3] = {"cells", "#1", "source"};
    const unsigned int notebook = read_as("s.py", s_script, sizeof(s_script) - 1u);
    TEST_ASSERT_EQUAL_STRING("markdown", string_at(notebook, s_kind, 3u));
    TEST_ASSERT_EQUAL_STRING("A note\n\non two lines", string_at(notebook, s_source, 3u));
}

void test_a_script_writes_back_unchanged(void)
{
    const unsigned int notebook = read_as("s.py", s_script, sizeof(s_script) - 1u);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_notebook_path_book.write, CliNotebookPathBookCfg, .json = &s_json, .path = "s.py", .book = notebook, .out = &out);
    CLI_TEST_ASSERT_TEXT(s_script, s_out, out.at);
}

void test_a_script_of_cells_is_a_notebook(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_notebook_path.is, CliNotebookPathCfg, .path = "s.py", .text = s_script, .size = sizeof(s_script) - 1u));
}

void test_a_plain_script_is_not_a_notebook(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_notebook_path.is, CliNotebookPathCfg, .path = "plain.py", .text = "print(1)\n", .size = 9ull));
}

void test_a_merge_allocs_each_sides_changes(void)
{
    (void)tree_emptied();
    const unsigned int base = notebook_of(s_base, 4u);
    const unsigned int ours = notebook_of(s_ours, 5u);
    const unsigned int theirs = notebook_of(s_theirs, 5u);
    assert_merged("e = 5;a = 1;b = 20\nprint(b);c = 30\nprint(c);note;d = 4", EMBED_CALL(cli_notebook.merge, CliNotebookCfg, .json = &s_json, .base = base, .ours = ours, .theirs = theirs));
}

void test_a_merge_separates_a_cell_both_sides_changed(void)
{
    (void)tree_emptied();
    const unsigned int base = notebook_of(s_base, 4u);
    const unsigned int ours = notebook_of(s_ours, 5u);
    const unsigned int both = notebook_of(s_both, 3u);
    assert_merged("a = 1;<b = 20\nprint(b)|b = 22\nprint(b)>;c = 3\nprint(c);d = 4", EMBED_CALL(cli_notebook.merge, CliNotebookCfg, .json = &s_json, .base = base, .ours = ours, .theirs = both));
}

void test_a_merge_separates_a_cell_one_changed_and_the_other_deleted(void)
{
    (void)tree_emptied();
    const unsigned int base = notebook_of(s_base, 4u);
    const unsigned int changed = notebook_of(s_changed_c, 4u);
    const unsigned int deleted = notebook_of(s_deleted, 2u);
    assert_merged("a = 1;b = 2\nprint(b);<c = 33\nprint(c)|gone>", EMBED_CALL(cli_notebook.merge, CliNotebookCfg, .json = &s_json, .base = base, .ours = changed, .theirs = deleted));
}

void test_a_merges_memory_is_sized_to_its_notebooks_and_released(void)
{
    (void)tree_emptied();
    const unsigned int base = notebook_of(s_base, 4u);
    const unsigned int ours = notebook_of(s_ours, 5u);
    const unsigned int theirs = notebook_of(s_theirs, 5u);
    (void)EMBED_CALL(cli_notebook.merge, CliNotebookCfg, .json = &s_json, .base = base, .ours = ours, .theirs = theirs);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_notebook_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_notebook_cells.hw));
    TEST_ASSERT_LESS_THAN_UINT64(1024ull, embed_atomic_usize_load(&g_cli_notebook_cells.hw));
}

void test_a_diff_reads_sources_and_outputs(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_notebook_text.for_diff, CliNotebookTextCfg, .json = tree_emptied(), .text = s_notebook, .size = sizeof(s_notebook) - 1u, .tokens = s_token, .tokens_max = sizeof(s_token) / sizeof(s_token[0]), .out = &out, .err_msg = &err_msg);
    CLI_TEST_ASSERT_TEXT("# cell 1 (code)\nprint('hi')\nx = 1\n## stream\nhi\n# cell 2 (markdown)\n# Title\n", s_out, out.at);
}

void test_a_diff_reads_a_result_errors_and_the_kinds_of_an_outputs_data(void)
{
    static const char outputs[] = "{\"cells\": [{\"cell_type\": \"code\", \"metadata\": {}, \"source\": [\"x\"], \"outputs\": ["
                                  "{\"output_type\": \"execute_result\", \"data\": {\"text/plain\": \"1\"}}, "
                                  "{\"output_type\": \"error\", \"ename\": \"ValueError\", \"evalue\": \"bad\"}, "
                                  "{\"output_type\": \"error\", \"evalue\": \"worse\"}, "
                                  "{\"output_type\": \"display_data\", \"data\": {\"image/png\": \"AAA\", \"text/html\": \"<b>\"}}]}], "
                                  "\"metadata\": {}, \"nbformat\": 4, \"nbformat_minor\": 5}\n";
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_notebook_text.for_diff, CliNotebookTextCfg, .json = tree_emptied(), .text = outputs, .size = sizeof(outputs) - 1u, .tokens = s_token, .tokens_max = sizeof(s_token) / sizeof(s_token[0]), .out = &out, .err_msg = &err_msg);
    CLI_TEST_ASSERT_TEXT("# cell 1 (code)\nx\n## execute_result\n1\n## error\nValueError: bad\n## error\nerror: worse\n## display_data: image/png, text/html\n", s_out, out.at);
}

void test_a_notebook_without_outputs_has_no_run_counts_or_outputs(void)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_notebook_text.without_outputs, CliNotebookTextCfg, .json = tree_emptied(), .text = s_notebook, .size = sizeof(s_notebook) - 1u, .tokens = s_token, .tokens_max = sizeof(s_token) / sizeof(s_token[0]), .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    CLI_TEST_ASSERT_CONTAINS("\"execution_count\": null", s_out);
    CLI_TEST_ASSERT_CONTAINS("\"outputs\": []", s_out);
}
