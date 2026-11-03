// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_schema.c: JSON schemas: JSON reads with its positions and faults, a file is checked against the schema it names, a schema is checked
// against its own schema, a schema not stored offers its fetch, and keys and values complete from the schema
#include "ui/cli/src/code/schema_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_schema[] = "{\n  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n  \"type\": \"object\",\n"
                               "  \"required\": [\"name\", \"version\"],\n  \"additionalProperties\": false,\n"
                               "  \"definitions\": {\"level\": {\"type\": \"string\", \"enum\": [\"low\", \"medium\", \"high\"], \"description\": "
                               "\"How loud\"}},\n  \"properties\": {\n    \"$schema\": {\"type\": \"string\"},\n"
                               "    \"name\": {\"type\": \"string\", \"minLength\": 2, \"description\": \"The name\"},\n"
                               "    \"version\": {\"type\": \"string\", \"pattern\": \"^\\\\d+\\\\.\\\\d+\\\\.\\\\d+$\"},\n"
                               "    \"count\": {\"type\": \"integer\", \"minimum\": 1, \"maximum\": 10},\n"
                               "    \"level\": {\"$ref\": \"#/definitions/level\"},\n"
                               "    \"tags\": {\"type\": \"array\", \"items\": {\"type\": \"string\"}, \"uniqueItems\": true},\n"
                               "    \"old\": {\"type\": \"boolean\", \"deprecated\": true},\n"
                               "    \"server\": {\"type\": \"object\", \"properties\": {\"port\": {\"type\": \"integer\"}, \"host\": {\"type\": "
                               "\"string\"}}, \"required\": [\"port\"]},\n"
                               "    \"pairs\": {\"type\": \"array\", \"prefixItems\": [{\"enum\": [\"one\", \"two\"]}], \"items\": {\"enum\": [\"rest\"]}},\n"
                               "    \"named\": {\"type\": \"object\", \"patternProperties\": {\"^x_\": {\"enum\": [\"px\"]}}}\n  }\n}";
static const char s_faulty_text[] = "{\n  \"a\": [1, 2,],\n  \"b\": tru,\n  // a comment\n  \"a\": {}\n}";
static const char s_file_text[] = "{\n  \"$schema\": \"./schema.json\",\n  \"name\": \"x\",\n  \"version\": \"1.0\",\n  \"count\": 12,\n"
                                  "  \"level\": \"loud\",\n  \"tags\": [\"a\", \"a\"],\n  \"old\": true,\n  \"server\": {\"host\": 1},\n"
                                  "  \"nmae\": 1\n}\n";
static const char s_own_text[] = "{\n  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n  \"type\": "
                                 "\"strin\",\n  \"minLength\": -1,\n  \"required\": [\"a\", \"a\"]\n}\n";

static CliSchemaRead s_read;
static char s_out[1u << 18u];
static CliCompletions s_items;
static char s_dir[CLI_PATH_MAX_LEN];
static const CliJson *s_json;

// the findings of `text`, the file `name` of the tree at `root`, read into the shared tree: the array's node
static unsigned int findings_of(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_schema.check, CliSchemaCfg, .root = root, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .out = &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// the first fix of a finding
static unsigned int first_fix(unsigned int finding)
{
    return EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = finding, .name = "data"), .name = "orior"), .index = 0u);
}

// the findings of the sample file against the schema it names: the array's node
static unsigned int file_findings(void)
{
    cli_unity_file(s_dir, "schema.json", s_schema);
    return findings_of(s_dir, "a.json", s_file_text);
}

// what completes the end of `text`, the file a.json of the tree, with the schema written
static const CliCompletions *completed(const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    cli_unity_file(s_dir, "schema.json", s_schema);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "a.json", .joined = path);
    unsigned int line = 0u;
    unsigned int col = 0u;
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        line += (at[0] == '\n') ? 1u : 0u;
        col = (at[0] == '\n') ? 0u : col + 1u;
    }
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    EMBED_CALL(cli_schema_line.complete, CliSchemaLineCfg, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .line = line, .col = col, .items = &s_items);
    return &s_items;
}

// whether an item shows `label` and, where `insert` is not NULL, inserts it
static int offers(const CliCompletions *items, const char *label, const char *insert)
{
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const item = &items->item[at];
        const int labeled = (item->label_size == CLI_UNITY_LENGTH(label)) && (memor.cmp(&(MemoriaCfg){.src = items->text + item->label_at, .other = label, .bytes = item->label_size}) == 0);
        if (labeled && ((insert == NULL) || ((item->insert_size == CLI_UNITY_LENGTH(insert)) && (memor.cmp(&(MemoriaCfg){.src = items->text + item->insert_at, .other = insert, .bytes = item->insert_size}) == 0))))
        {
            return 1;
        }
    }
    return 0;
}

// the labels of the items, `|` between them, are `required`
static void assert_labels(const char *required, const CliCompletions *items)
{
    char text[256];
    VerbaLinea line = {text, sizeof(text), 0ull};
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put_n(&line, items->text + items->item[at].label_at, items->item[at].label_size);
    }
    CLI_TEST_ASSERT_TEXT(required, text, line.at);
}

// whether an error of the read contains `part`
static int error_contains(const char *part)
{
    for (unsigned int at = 0u; at < s_read.errors; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = &s_read, .at = s_read.error[at].message_at), .cap = CLI_CELLS_POOL, .other = part, .other_cap = CLI_CELLS_POOL}) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("schema", s_dir);
}

void tearDown(void)
{
}

void test_json_reads_with_its_faults(void)
{
    EMBED_CALL(cli_schema_read_text_internal.read, CliSchemaReadTextCtx, .text = s_faulty_text, .size = sizeof(s_faulty_text) - 1u, .read_out = &s_read);
    TEST_ASSERT_EQUAL_UINT(3u, s_read.errors);
    TEST_ASSERT_TRUE(error_contains("No item follows"));
    TEST_ASSERT_TRUE(error_contains("`tru` is no value"));
    TEST_ASSERT_TRUE(error_contains("\"a\" is a key of this object above it"));
    EMBED_CALL(cli_schema_read_text_internal.read_free, CliSchemaReadTextCtx, .read_out = &s_read);
}

void test_json_reads_with_its_positions(void)
{
    EMBED_CALL(cli_schema_read_text_internal.read, CliSchemaReadTextCtx, .text = s_faulty_text, .size = sizeof(s_faulty_text) - 1u, .read_out = &s_read);
    TEST_ASSERT_NOT_EQUAL(CLI_SCHEMA_NONE, s_read.root);
    const CliSchemaNode *const root = &s_read.node[s_read.root];
    TEST_ASSERT_EQUAL_INT(CLI_SCHEMA_OBJECT, root->kind);
    TEST_ASSERT_EQUAL_UINT(3u, root->count);
    TEST_ASSERT_EQUAL_UINT64(4ull, s_read.member[root->first].from);
    EMBED_CALL(cli_schema_read_text_internal.read_free, CliSchemaReadTextCtx, .read_out = &s_read);
}

void test_a_file_of_many_values_reads_complete_with_its_lists_in_one_cell(void)
{
    static char s_many[1u << 15u];
    VerbaLinea many = {s_many, sizeof(s_many), 0ull};
    mmgr_line_ch(&many, '[');
    for (unsigned int item = 0u; item < 3000u; item += 1u)
    {
        mmgr_line_put(&many, (item == 0u) ? "{\"k\": 1}" : ",{\"k\": 1}");
    }
    mmgr_line_ch(&many, ']');
    EMBED_CALL(cli_schema_read_text_internal.read, CliSchemaReadTextCtx, .text = s_many, .size = many.at, .read_out = &s_read);
    TEST_ASSERT_EQUAL_UINT(0u, s_read.errors);
    TEST_ASSERT_FALSE(s_read.full);
    TEST_ASSERT_EQUAL_UINT(6001u, s_read.nodes);
    TEST_ASSERT_EQUAL_UINT(3000u, s_read.members);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(s_read.nodes, s_read.nodes_max);
    EMBED_CALL(cli_schema_read_text_internal.read_free, CliSchemaReadTextCtx, .read_out = &s_read);
}

void test_a_file_is_checked_against_the_schema_it_names(void)
{
    static const char *const s_faults[9][2] = {{"schema-length", "shorter than the 2"},
                                               {"schema-pattern", "pattern"},
                                               {"schema-range", "12 is above the most the schema allows, 10"},
                                               {"schema-enum", "`\"loud\"`"},
                                               {"schema-unique", "above it"},
                                               {"schema-deprecated", "\"old\" is deprecated"},
                                               {"schema-required", "`port`"},
                                               {"schema-property", "\"nmae\" is no property"},
                                               {"schema-type", NULL}};
    const unsigned int found = file_findings();
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        TEST_ASSERT_NOT_EQUAL_MESSAGE(CLI_JSON_NONE, cli_unity_finding(found, s_faults[at][0], s_faults[at][1]), s_faults[at][0]);
    }
}

void test_a_property_adjacent_a_known_one_is_offered_it(void)
{
    const unsigned int fix = first_fix(cli_unity_finding(file_findings(), "schema-property", NULL));
    TEST_ASSERT_EQUAL_STRING("\"name\"", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = fix, .name = "edits"), .index = 0u), "text"));
}

void test_a_value_not_allowed_is_offered_the_nearest(void)
{
    const unsigned int fix = first_fix(cli_unity_finding(file_findings(), "schema-enum", NULL));
    TEST_ASSERT_EQUAL_STRING("Change to \"low\"", cli_unity_member_text(fix, "title"));
}

void test_nothing_else_is_found(void)
{
    const unsigned int found = file_findings();
    TEST_ASSERT_EQUAL_UINT(9u, s_json->node[found].count);
}

void test_a_type_no_schema_knows_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(findings_of("/t", "s.json", s_own_text), NULL, "\"strin\""));
}

void test_a_bound_below_its_min_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(findings_of("/t", "s.json", s_own_text), NULL, "-1 is below the least"));
}

void test_a_name_required_twice_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(findings_of("/t", "s.json", s_own_text), "schema-unique", NULL));
}

void test_a_2020_12_schema_is_checked_against_its_draft(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(findings_of("/t", "s.json", "{\"$schema\": \"https://json-schema.org/draft/2020-12/schema\", \"prefixItems\": 3}"), "schema-type", NULL));
}

void test_a_schema_not_stored_offers_its_fetch(void)
{
    static const char s_end[] = "' 'https://example.com/none.json'";
    const unsigned int found = findings_of("/t", "a.json", "{\"$schema\": \"https://example.com/none.json\"}");
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[found].count);
    const unsigned int missing = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 0u);
    TEST_ASSERT_EQUAL_STRING("schema-missing", cli_unity_member_text(missing, "code"));
    const char *const line = cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = first_fix(missing), .name = "run"), "line");
    const unsigned long long length = CLI_UNITY_LENGTH(line);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = line, .other = "curl -fsSL --create-dirs -o '", .cap = CLI_CELLS_POOL}));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(sizeof(s_end) - 1u, length);
    TEST_ASSERT_EQUAL_STRING(s_end, line + length - (sizeof(s_end) - 1u));
}

void test_a_key_the_schema_defines_completes(void)
{
    TEST_ASSERT_TRUE(offers(completed("{\n  \"$schema\": \"./schema.json\",\n  \"name\": \"x\",\n  "), "version", "\"version\": \"$0\""));
}

void test_a_key_the_object_has_is_not_offered(void)
{
    TEST_ASSERT_FALSE(offers(completed("{\n  \"$schema\": \"./schema.json\",\n  \"name\": \"x\",\n  "), "name", NULL));
}

void test_a_key_begun_in_its_quotes_completes_inside_them(void)
{
    TEST_ASSERT_TRUE(offers(completed("{\"$schema\": \"./schema.json\", \"ser"), "server", "server"));
}

void test_an_inner_objects_keys_complete(void)
{
    assert_labels("port|host", completed("{\"$schema\": \"./schema.json\", \"server\": {\"po"));
}

void test_a_value_completes_from_its_enum(void)
{
    assert_labels("low|medium|high", completed("{\"$schema\": \"./schema.json\", \"level\": \""));
}

void test_a_value_after_a_comment_completes_as_though_the_comment_were_not_there(void)
{
    assert_labels("low|medium|high", completed("{\"$schema\": \"./schema.json\", // \"server\": {\n\"level\": \""));
    assert_labels("low|medium|high", completed("{\"$schema\": \"./schema.json\", /* \"server\": { */ \"level\": \""));
}

void test_an_items_value_completes_from_the_schema_at_its_place(void)
{
    assert_labels("one|two", completed("{\"$schema\": \"./schema.json\", \"pairs\": [\""));
    assert_labels("rest", completed("{\"$schema\": \"./schema.json\", \"pairs\": [\"one\", \""));
}

void test_a_value_completes_from_the_pattern_its_key_matches(void)
{
    assert_labels("px", completed("{\"$schema\": \"./schema.json\", \"named\": {\"x_a\": \""));
}

void test_a_boolean_completes(void)
{
    assert_labels("true|false", completed("{\"$schema\": \"./schema.json\", \"old\": "));
}

void test_a_schema_completes_from_its_own_schema(void)
{
    TEST_ASSERT_TRUE(offers(completed("{\"$schema\": \"http://json-schema.org/draft-07/schema#\", \"type\": \""), "object", NULL));
}

void test_every_schemas_text_and_tokens_are_released(void)
{
    (void)file_findings();
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_schema_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_schema_cells.hw));
}

void test_a_files_findings_and_indexed_lines_are_released(void)
{
    (void)file_findings();
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_build_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_build_cells.hw));
}
