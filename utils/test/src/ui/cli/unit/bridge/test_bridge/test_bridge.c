// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_bridge.c: a pair stores its verdict and every line under it, a map reads a name and its key separately, and the tree's files
// are read into the bridge
#include "ui/cli/src/editor/bridge.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_klq[] = "klq L*\nkey word_copy\npair word_copy thread_idx_x\nopen 3\nqualifier_coherence\n"
                            "concept_coherence 00ff\nkey word_set\ntext_identity a = b\nclosed @x 1->0\n";
static const char s_klm[] = "klm cu\nkey lane_body lane_core\nkind lane_body nop\nbreaks a b\n";

static CliJson s_json;

// a string member of a node, the empty string where it has none
static const char *text_of(unsigned int node, const char *name)
{
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = node, .name = name), .length_pointer = NULL);
    return (text != NULL) ? text : "";
}

// whether the string `whole` ends in `end`
static int text_ends(const char *whole, const char *end)
{
    const unsigned long long size = CLI_UNITY_LENGTH(whole);
    const unsigned long long end_size = CLI_UNITY_LENGTH(end);
    return (size >= end_size) && cellul.eq(&(CatenaFinitaCfg){.src = whole + size - end_size, .other = end, .cap = CLI_CELLS_POOL});
}

// the sample's first pair
static unsigned int first_pair(unsigned int read)
{
    const unsigned int keys = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = read, .name = "keys");
    return EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = keys, .name = "word_copy"), .name = "pairs"), .index = 0u);
}

// the sample's Lstar file read: its node
static unsigned int klq_read(void)
{
    return EMBED_CALL(cli_bridge_path.klq, CliBridgePathCfg, .json = &s_json, .path = "Lstar.klq", .text = s_klq, .size = sizeof(s_klq) - 1u);
}

// the sample's map's names: their node
static unsigned int map_names(void)
{
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_bridge_path.klm, CliBridgePathCfg, .json = &s_json, .path = "cu.klm", .text = s_klm, .size = sizeof(s_klm) - 1u), .name = "names");
}

// the tree's bridge read: its node, the test ignored where no tree is found
static unsigned int tree_bridge(void)
{
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = root))
    {
        TEST_IGNORE_MESSAGE("no tree is found");
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    return EMBED_CALL(cli_bridge.read, CliBridgeCfg, .json = &s_json, .root = root);
}

void setUp(void)
{
    cli_unity_begun();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &s_json, .nodes_max = 1024u, .text_max = 1ull << 16u, .what = "a bridge test's tree", .owner = &g_checks_cells));
}

void tearDown(void)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
}

void test_a_pair_reads_its_two_names(void)
{
    const unsigned int pair = first_pair(klq_read());
    TEST_ASSERT_EQUAL_STRING("word_copy", text_of(pair, "first"));
    TEST_ASSERT_EQUAL_STRING("thread_idx_x", text_of(pair, "second"));
}

void test_a_pair_reads_its_verdict(void)
{
    const unsigned int verdict = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = first_pair(klq_read()), .name = "verdict");
    TEST_ASSERT_EQUAL_STRING("open", text_of(verdict, "word"));
    TEST_ASSERT_EQUAL_STRING("3", text_of(verdict, "rest"));
}

void test_every_other_line_under_a_pair_is_a_note(void)
{
    const unsigned int notes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = first_pair(klq_read()), .name = "notes");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = notes, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_EQUAL_UINT(2u, s_json.node[notes].count);
    TEST_ASSERT_EQUAL_STRING("qualifier_coherence", text_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = notes, .index = 0u), "word"));
    TEST_ASSERT_EQUAL_STRING("concept_coherence", text_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = notes, .index = 1u), "word"));
}

void test_a_key_with_no_pair_has_none(void)
{
    const unsigned int keys = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = klq_read(), .name = "keys");
    const unsigned int pairs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = keys, .name = "word_set"), .name = "pairs");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json, .node = pairs, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_EQUAL_UINT(0u, s_json.node[pairs].count);
}

void test_a_line_under_no_pair_belongs_to_the_key(void)
{
    const unsigned int identity = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = klq_read(), .name = "identities"), .index = 0u);
    TEST_ASSERT_TRUE(text_ends(text_of(identity, "rest"), "closed @x 1->0"));
}

void test_a_map_names_its_language(void)
{
    TEST_ASSERT_EQUAL_STRING("cu", text_of(EMBED_CALL(cli_bridge_path.klm, CliBridgePathCfg, .json = &s_json, .path = "cu.klm", .text = s_klm, .size = sizeof(s_klm) - 1u), "language"));
}

void test_a_name_and_its_key_are_read_separately(void)
{
    TEST_ASSERT_EQUAL_STRING("lane_core", text_of(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = map_names(), .name = "lane_body"), "key"));
}

void test_a_name_stores_its_kind(void)
{
    const unsigned int body = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = map_names(), .name = "lane_body");
    TEST_ASSERT_EQUAL_STRING("nop", text_of(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = body, .name = "kind"), "rest"));
}

void test_a_name_stores_what_it_breaks(void)
{
    const unsigned int breaks = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = map_names(), .name = "a"), .name = "breaks");
    TEST_ASSERT_EQUAL_STRING("b", text_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = breaks, .index = 0u), "rest"));
}

void test_the_trees_lstar_file_contains_lane_open(void)
{
    const unsigned int klq = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = tree_bridge(), .name = "klq");
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = klq, .name = "keys"), .name = "lane_open"));
}

void test_the_trees_cu_map_names_lane_open(void)
{
    const unsigned int maps = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = tree_bridge(), .name = "maps");
    int mapped = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = maps, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int map = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = maps, .index = at);
        mapped |= cellul.eq(&(CatenaFinitaCfg){.src = text_of(map, "language"), .other = "cu", .cap = CLI_CELLS_POOL}) && (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = map, .name = "names"), .name = "lane_open") != CLI_JSON_NONE);
    }
    TEST_ASSERT_TRUE(mapped);
}

void test_the_trees_cu_ruleset_writes_lane_open(void)
{
    const unsigned int rulesets = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = tree_bridge(), .name = "rulesets");
    int written = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = rulesets, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int ruleset = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = rulesets, .index = at);
        written |= text_ends(text_of(ruleset, "path"), "cu.krs") && (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = ruleset, .name = "entries"), .name = "lane_open") != CLI_JSON_NONE);
    }
    TEST_ASSERT_TRUE(written);
}
