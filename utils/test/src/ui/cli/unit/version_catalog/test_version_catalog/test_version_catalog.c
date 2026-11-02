// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_version_catalog.c: a version catalog reads its sections, is checked against itself, completes its sections, keys and aliases,
// and every catalog read is released
#include "ui/cli/src/builds/version_catalog_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define CATALOG_PATH "/tree/gradle/libs.versions.toml"

static const char s_catalog[] = "[versions]\ngroovy = \"3.0.5\"\ncheckstyle = \"8.37\"\nmine = \"1.2-SNAPSHOT\"\n\n[libraries]\n"
                                "groovy-core = { module = \"org.codehaus.groovy:groovy\", version.ref = \"groovy\" }\n"
                                "groovy-json = { module = \"org.codehaus.groovy:groovy-json\", version.ref = \"grovy\" }\n"
                                "commons-lang3 = { group = \"org.apache.commons\", name = \"commons-lang3\", version = { strictly = \"[3.8, "
                                "4.0[\", "
                                "prefer = \"3.9\" } }\nbad = \"only:two\"\n\n[bundles]\ngroovy = [\"groovy-core\", \"groovy-jsn\"]\n\n[plugins]\n"
                                "versions = { id = \"com.github.ben-manes.versions\", version = \"0.45.0\" }\n\n[librarys]\n";

static CliVersionsAliases s_aliases;
static char s_out[1u << 18u];
static const CliJson *s_json;

// the first alias of the section `section`, the test failed where it has none
static const CliVersionsAlias *first_alias(unsigned int section)
{
    for (unsigned int at = 0u; at < s_aliases.count; at += 1u)
    {
        if (s_aliases.alias[at].section == section)
        {
            return &s_aliases.alias[at];
        }
    }
    TEST_FAIL_MESSAGE("the section has no alias");
    return NULL;
}

// the findings of the catalog: the array's node in the shared tree
static unsigned int catalog_findings(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_builds_path.build_check, CliBuildsPathCfg, .root = "/tree", .path = CATALOG_PATH, .text = s_catalog, .size = sizeof(s_catalog) - 1u, .out = &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// what completes the end of `text`
static const CliCompletions *completed(const char *text)
{
    return cli_unity_build_completed("/tree", CATALOG_PATH, text);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_catalog_reads_its_sections_in_order_with_no_error(void)
{
    static const char *const s_names[5] = {"versions", "libraries", "bundles", "plugins", "librarys"};
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_versions_lock);
    const int parsed = EMBED_CALL(cli_version_catalog_text_internal.versions_parse, CliVersionCatalogTextCtx, .text = s_catalog, .size = sizeof(s_catalog) - 1u);
    const unsigned int errors = g_cli_versions.errors;
    const unsigned int sections = g_cli_versions.sections;
    int named = 1;
    for (unsigned int at = 0u; named && (at < 5u) && (at < sections); at += 1u)
    {
        named = (g_cli_versions.section[at].name_size == CLI_UNITY_LENGTH(s_names[at])) &&
                (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = g_cli_versions.section[at].name_at), .other = s_names[at], .bytes = g_cli_versions.section[at].name_size}) == 0);
    }
    cli_versions_release();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_versions_lock);
    TEST_ASSERT_TRUE(parsed);
    TEST_ASSERT_EQUAL_UINT(0u, errors);
    TEST_ASSERT_EQUAL_UINT(5u, sections);
    TEST_ASSERT_TRUE(named);
}

void test_a_librarys_alias_reads_its_module_and_version(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases, CliVersionCatalogTextSizeAliasesCtx, .text = s_catalog, .size = sizeof(s_catalog) - 1u, .aliases = &s_aliases));
    const CliVersionsAlias *const library = first_alias(CLI_VERSIONS_LIBRARIES);
    CLI_TEST_ASSERT_TEXT("groovy-core", s_aliases.text + library->alias_at, library->alias_size);
    CLI_TEST_ASSERT_TEXT("org.codehaus.groovy:groovy, version groovy", s_aliases.text + library->about_at, library->about_size);
    EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases_free, CliVersionCatalogTextSizeAliasesCtx, .aliases = &s_aliases);
}

void test_a_bundles_alias_reads_its_libraries(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases, CliVersionCatalogTextSizeAliasesCtx, .text = s_catalog, .size = sizeof(s_catalog) - 1u, .aliases = &s_aliases));
    const CliVersionsAlias *const bundle = first_alias(CLI_VERSIONS_BUNDLES);
    CLI_TEST_ASSERT_TEXT("groovy-core, groovy-jsn", s_aliases.text + bundle->about_at, bundle->about_size);
    EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases_free, CliVersionCatalogTextSizeAliasesCtx, .aliases = &s_aliases);
}

void test_a_version_reference_no_version_names_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(catalog_findings(), "catalog-reference", "`grovy`"));
}

void test_a_bundles_library_no_library_names_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(catalog_findings(), "catalog-reference", "`groovy-jsn`"));
}

void test_a_library_of_two_parts_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(catalog_findings(), "catalog-library", "only:two"));
}

void test_a_snapshot_is_a_note(void)
{
    const unsigned int snapshot = cli_unity_finding(catalog_findings(), "snapshot", NULL);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, snapshot);
    const unsigned int severity = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = snapshot, .name = "severity");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = severity, .kind = CFG_JSON_NUMBER));
    TEST_ASSERT_EQUAL_CHAR('3', s_json->text[s_json->node[severity].text]);
}

void test_a_section_no_catalog_has_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(catalog_findings(), "catalog-section", NULL));
}

void test_the_nearest_version_is_offered(void)
{
    const unsigned int grovy = cli_unity_finding(catalog_findings(), "catalog-reference", "`grovy`");
    const unsigned int fixes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = grovy, .name = "data"), .name = "orior");
    TEST_ASSERT_EQUAL_STRING("Change to groovy", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = fixes, .index = 0u), "title"));
}

void test_a_snapshot_offers_the_command_that_refreshes_it(void)
{
    const unsigned int snapshot = cli_unity_finding(catalog_findings(), "snapshot", NULL);
    const unsigned int refresh = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = snapshot, .name = "data"), .name = "orior"), .index = 0u);
    TEST_ASSERT_EQUAL_STRING("gradle --refresh-dependencies dependencies", cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = refresh, .name = "run"), "line"));
}

void test_nothing_else_is_found(void)
{
    const unsigned int found = catalog_findings();
    TEST_ASSERT_EQUAL_UINT(5u, s_json->node[found].count);
}

void test_a_sections_name_completes(void)
{
    TEST_ASSERT_TRUE(cli_unity_completes(completed("[versions]\na = \"1\"\n[lib"), "libraries"));
}

void test_a_version_reference_completes(void)
{
    const CliCompletions *const items = completed("[versions]\ngroovy = \"1\"\n[libraries]\nx = { module = \"a:b\", version.ref = \"gr");
    TEST_ASSERT_EQUAL_UINT(1u, items->count);
    TEST_ASSERT_TRUE(cli_unity_completes(items, "groovy"));
}

void test_a_tables_key_completes(void)
{
    TEST_ASSERT_TRUE(cli_unity_completes(completed("[libraries]\nx = { mod"), "module"));
}

void test_a_tables_key_completes_after_a_dot(void)
{
    const CliCompletions *const items = completed("[libraries]\nx = { module = \"a:b\", version.");
    TEST_ASSERT_EQUAL_UINT(1u, items->count);
    TEST_ASSERT_TRUE(cli_unity_completes(items, "ref"));
}

void test_a_bundles_library_completes(void)
{
    const CliCompletions *const items = completed("[libraries]\ngroovy-core = \"a:b:1\"\n[bundles]\nall = [\"gro");
    TEST_ASSERT_EQUAL_UINT(1u, items->count);
    TEST_ASSERT_TRUE(cli_unity_completes(items, "groovy-core"));
}

void test_every_catalogs_memory_is_released(void)
{
    (void)catalog_findings();
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_versions_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_versions_cells.hw));
}
