// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_defs.c: the file definitions read from the tree: each ruleset file type with its columns, the record header, and the
// language tables
#include "ui/cli/src/command_line/cells.h"
#include "ui/cli/src/editor/defs.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_read[1u << 22u];
static const CliJson *s_json_read;
static unsigned int s_root;

// the type of the extension `ext` has exactly the five ruleset columns, in order
static void assert_ruleset_columns(const char *ext)
{
    static const char *const s_columns[5] = {"where", "who", "kind", "form", "gloss"};
    const unsigned int types = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json_read, .object = s_root, .name = "types");
    unsigned int found = CLI_JSON_NONE;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json_read, .array = types, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int type = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json_read, .array = types, .index = at);
        if (cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(type, "ext"), .other = ext, .cap = CLI_CELLS_POOL}))
        {
            found = type;
        }
    }
    TEST_ASSERT_NOT_EQUAL_MESSAGE(CLI_JSON_NONE, found, ext);
    const unsigned int columns = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json_read, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json_read, .object = found, .name = "table"), .name = "columns");
    TEST_ASSERT_EQUAL_MESSAGE(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json_read, .array = columns, .index = 5u), ext);
    for (unsigned int column = 0u; column < 5u; column += 1u)
    {
        TEST_ASSERT_EQUAL_STRING_MESSAGE(s_columns[column], EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json_read, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json_read, .array = columns, .index = column), .length_pointer = NULL), ext);
    }
}

void setUp(void)
{
    cli_unity_begun();
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = root))
    {
        TEST_IGNORE_MESSAGE("no tree is found from the working directory");
    }
    VerbaLinea out = {s_read, sizeof(s_read), 0ull};
    EMBED_CALL(cli_defs_table.read, CliDefsCfg, .root = root, .out = &out);
    s_json_read = cli_unity_parsed(s_read, out.at, &s_root);
}

void tearDown(void)
{
}

void test_reading_releases_everything_it_allocated(void)
{
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_defs_cells.in_use));
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, embed_atomic_usize_load(&g_cli_defs_cells.hw));
}

void test_the_g_table_has_its_columns(void)
{
    assert_ruleset_columns("g");
}

void test_the_gsm_table_has_its_columns(void)
{
    assert_ruleset_columns("gsm");
}

void test_the_krs_table_has_its_columns(void)
{
    assert_ruleset_columns("krs");
}

void test_the_kcr_table_has_its_columns(void)
{
    assert_ruleset_columns("kcr");
}

void test_the_record_header_is_16_bytes_with_magic_krep(void)
{
    const unsigned int head = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json_read, .object = s_root, .name = "head");
    const unsigned int bytes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json_read, .object = head, .name = "bytes");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json_read, .node = bytes, .kind = CFG_JSON_NUMBER));
    CLI_TEST_ASSERT_TEXT("16", s_json_read->text + s_json_read->node[bytes].text, s_json_read->node[bytes].length);
    TEST_ASSERT_EQUAL_STRING("KREP", cli_unity_member_text(head, "magic"));
}

void test_the_languages_include_the_assembler_table(void)
{
    const unsigned int languages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json_read, .object = s_root, .name = "languages");
    int found = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json_read, .array = languages, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        found |= cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json_read, .array = languages, .index = at), "name"), .other = "gnascor_asm_lng.tsv", .cap = CLI_CELLS_POOL});
    }
    TEST_ASSERT_TRUE(found);
}
