// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_gradle.c: a Gradle script is checked, braces left open are found, a script completes by its block and its catalog, and every
// script read is released
#include "ui/cli/src/builds/gradle_internal.h"
#include "ui/cli/src/builds/version_catalog_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_script[] = "plugins {\n    id(\"java\")\n}\n\ndependencies {\n    implementation(libs.groovy.core)\n"
                               "    implementation(libs.groovy.cor)\n    compile(\"a:b:1\")\n    implmentation(\"a:b:1\")\n"
                               "    testImplementation(\"org.junit:junit:4.13\")\n    api(\"onlyone\")\n    "
                               "runtimeOnly(\"g:a:2.0-SNAPSHOT\")\n"
                               "    implementation(libs.versions.groovy.get())\n    integrationTestImplementation(\"x:y:1\")\n"
                               "    constraints {\n        implementation(\"c:d:1\")\n    }\n}\n// } a brace in a comment\nval s = \"{\"\n";

static char s_root[CLI_PATH_MAX_LEN];
static char s_path[CLI_PATH_MAX_LEN];
static char s_out[1u << 18u];
static const CliJson *s_json;

// the findings of the script at `path` of the tree at `root`, `text` its text: the array's node in the shared tree
static unsigned int findings_of(const char *root, const char *path, const char *text)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_builds_path.build_check, CliBuildsPathCfg, .root = root, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .out = &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// the findings of the sample script in a tree with a catalog: the array's node
static unsigned int script_findings(void)
{
    char gradle[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "gradle", .joined = gradle);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = gradle));
    cli_unity_file(s_root, "settings.gradle.kts", "rootProject.name = \"x\"\n");
    cli_unity_file(s_root, "gradle/libs.versions.toml", "[versions]\ngroovy = \"3\"\n[libraries]\ngroovy-core = { module = \"g:groovy\", version.ref = \"groovy\" }\n");
    cli_unity_file(s_root, "build.gradle.kts", s_script);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build.gradle.kts", .joined = s_path);
    return findings_of(s_root, s_path, s_script);
}

// a tree with a catalog of aliases, versions and plugins, s_path its build script
static void catalog_written(void)
{
    char gradle[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "gradle", .joined = gradle);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = gradle));
    cli_unity_file(s_root, "gradle/libs.versions.toml",
                   "[versions]\ngroovy = \"3\"\n[libraries]\ngroovy-core = \"g:groovy:3\"\ngroovy-json = \"g:json:3\"\n"
                   "[plugins]\nshadow = \"com.github.johnrengelman.shadow:8\"\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "build.gradle", .joined = s_path);
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("gradle_check", s_root);
}

void tearDown(void)
{
}

void test_an_alias_the_catalog_does_not_define_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(script_findings(), "gradle-catalog", "libs.groovy.cor"));
}

void test_a_configuration_gradle_removed_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(script_findings(), "gradle-configuration", "`compile`"));
}

void test_a_configuration_no_plugin_makes_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(script_findings(), "gradle-configuration", "`implmentation`"));
}

void test_a_dependency_of_one_part_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(script_findings(), "gradle-notation", "onlyone"));
}

void test_a_snapshot_is_found(void)
{
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(script_findings(), "snapshot", NULL));
}

void test_nothing_else_is_found(void)
{
    const unsigned int found = script_findings();
    TEST_ASSERT_EQUAL_UINT(5u, s_json->node[found].count);
}

void test_the_nearest_configuration_is_offered(void)
{
    const unsigned int typo = cli_unity_finding(script_findings(), "gradle-configuration", "`implmentation`");
    const unsigned int fix = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = typo, .name = "data"), .name = "orior"), .index = 0u);
    TEST_ASSERT_EQUAL_STRING("Change to implementation", cli_unity_member_text(fix, "title"));
}

void test_the_nearest_alias_replaces_the_wrong_one(void)
{
    const unsigned int cor = cli_unity_finding(script_findings(), "gradle-catalog", "libs.groovy.cor");
    const unsigned int fix = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = cor, .name = "data"), .name = "orior"), .index = 0u);
    TEST_ASSERT_EQUAL_STRING("groovy.core", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = fix, .name = "edits"), .index = 0u), "text"));
}

void test_a_block_left_open_is_the_only_finding(void)
{
    const unsigned int found = findings_of("/none", "/none/build.gradle", "dependencies {\n  implementation 'a:b:1'\n");
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[found].count);
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(found, NULL, "`dependencies`"));
}

void test_a_catalogs_aliases_complete_after_a_dot(void)
{
    catalog_written();
    const CliCompletions *const items = cli_unity_build_completed(s_root, s_path, "dependencies {\n    implementation libs.groovy.");
    TEST_ASSERT_EQUAL_UINT(2u, items->count);
    TEST_ASSERT_TRUE(cli_unity_completes(items, "core"));
    TEST_ASSERT_TRUE(cli_unity_completes(items, "json"));
}

void test_a_catalogs_versions_complete(void)
{
    catalog_written();
    TEST_ASSERT_TRUE(cli_unity_completes(cli_unity_build_completed(s_root, s_path, "dependencies {\n    implementation libs."), "versions.groovy"));
}

void test_a_catalogs_plugins_complete(void)
{
    catalog_written();
    TEST_ASSERT_TRUE(cli_unity_completes(cli_unity_build_completed(s_root, s_path, "plugins {\n    alias(libs.plugins.sh"), "shadow"));
}

void test_a_blocks_configurations_complete(void)
{
    catalog_written();
    TEST_ASSERT_TRUE(cli_unity_completes(cli_unity_build_completed(s_root, s_path, "dependencies {\n    impl"), "implementation"));
}

void test_a_blocks_own_calls_complete(void)
{
    catalog_written();
    TEST_ASSERT_TRUE(cli_unity_completes(cli_unity_build_completed(s_root, s_path, "repositories {\n    mav"), "mavenCentral"));
}

void test_the_scripts_blocks_complete(void)
{
    catalog_written();
    TEST_ASSERT_TRUE(cli_unity_completes(cli_unity_build_completed(s_root, s_path, "dep"), "dependencies"));
}

void test_nothing_completes_in_a_string(void)
{
    catalog_written();
    TEST_ASSERT_EQUAL_UINT(0u, cli_unity_build_completed(s_root, s_path, "x = \"dep")->count);
}

void test_every_scripts_memory_is_released(void)
{
    (void)script_findings();
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_gradle_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_gradle_cells.hw));
}

void test_every_catalogs_memory_is_released(void)
{
    (void)script_findings();
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_versions_cells.in_use));
}

void test_the_findings_memory_is_released_once_written(void)
{
    (void)script_findings();
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_build_cells.in_use));
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_build_cells.hw));
}
