// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_plugin_catalog.c: a plugin is found in a catalog and installed only as its entry names it
#include "ui/cli/src/plugins/plugin_catalog.h"
#include "ui/cli/src/text/digest.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_package[] = "{\"orior\":\"plugin\",\"files\":{\"plugin.json\":\"{\\\"id\\\": \\\"counts\\\", "
                                "\\\"version\\\": \\\"1.1.0\\\", \\\"kind\\\": \\\"script\\\", \\\"api\\\": 3, "
                                "\\\"reads\\\": [\\\"*.md\\\"]}\",\"plugin.js\":\"orior.on(\\\"open\\\", () => {});\"}}";

static char s_root[CLI_PATH_MAX_LEN];
static char s_plugins[CLI_PATH_MAX_LEN];
static char s_address[CLI_PATH_MAX_LEN];
static char s_hash[72];
static char s_catalog[2048];
static char s_entries[8192];
static char s_why[1024];

// the entry of counts with `version` and `sha256` installed: 1, or 0 with the reason NUL-terminated in s_why
static int entry_installed(const char *version, const char *sha256)
{
    char entry[512];
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {entry, sizeof(entry), 0ull};
    mmgr_line_put(&line, "{\"id\":\"counts\",\"name\":\"Counts\",\"version\":\"");
    mmgr_line_put(&line, version);
    mmgr_line_put(&line, "\",\"package\":\"packages/counts.json\",\"sha256\":\"");
    mmgr_line_put(&line, sha256);
    mmgr_line_put(&line, "\",\"reads\":[\"*.md\"]}");
    VerbaLinea made = {folder, sizeof(folder), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    const int installed = EMBED_CALL(cli_plugin_catalog_address.install_into, CliPluginCatalogAddressCfg, .plugins = s_plugins, .address = s_address, .entry = entry, .size = line.at, .out = &made, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    return installed;
}

void setUp(void)
{
    char packages[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    cli_unity_folder("plugin_catalog", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "packages", .joined = packages);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "plugins", .joined = s_plugins);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = packages));
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_plugins));
    cli_unity_file(s_root, "packages/counts.json", s_package);
    VerbaLinea hashed = {s_hash, sizeof(s_hash), 0ull};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = s_package, .size = sizeof(s_package) - 1u, .text = &hashed);
    (void)mmgr_line_finish(&hashed);
    VerbaLinea catalog = {s_catalog, sizeof(s_catalog), 0ull};
    mmgr_line_put(&catalog, "{\"orior\":\"plugin catalog\",\"plugins\":[{\"id\":\"counts\",\"name\":\"Counts\","
                            "\"version\":\"1.1.0\",\"package\":\"packages/counts.json\",\"sha256\":\"");
    mmgr_line_put(&catalog, s_hash);
    mmgr_line_put(&catalog, "\",\"reads\":[\"*.md\"]},{\"id\":\"../out\",\"name\":\"Out\",\"version\":\"1\","
                            "\"package\":\"x.json\"}]}");
    (void)mmgr_line_finish(&catalog);
    cli_unity_file(s_root, "catalog.json", s_catalog);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "catalog.json", .joined = s_address);
}

void tearDown(void)
{
}

void test_an_id_that_is_not_a_folder_name_is_left_out(void)
{
    VerbaLinea entries = {s_entries, sizeof(s_entries), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugin_catalog_address.read, CliPluginCatalogAddressCfg, .address = s_address, .out = &entries, .err_msg = &err_msg));
    unsigned int list = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_entries, entries.at, &list);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = list, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_EQUAL_UINT(1u, json->node[list].count);
    TEST_ASSERT_EQUAL_STRING("counts", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = list, .index = 0u), "id"));
}

void test_an_entry_installs(void)
{
    TEST_ASSERT_TRUE_MESSAGE(entry_installed("1.1.0", s_hash), s_why);
}

void test_the_installed_files_are_in_the_plugins_folder(void)
{
    char path[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE_MESSAGE(entry_installed("1.1.0", s_hash), s_why);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_plugins, .relative = "counts/plugin.js", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_plugins, .relative = "counts/plugin.json", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}

void test_a_package_whose_hash_differs_is_refused(void)
{
    TEST_ASSERT_FALSE(entry_installed("1.1.0", "0000000000000000000000000000000000000000000000000000000000000000"));
    CLI_TEST_ASSERT_CONTAINS("hash differs", s_why);
}

void test_a_plugin_json_of_another_version_is_refused(void)
{
    TEST_ASSERT_FALSE(entry_installed("1.0.0", ""));
    CLI_TEST_ASSERT_CONTAINS("the catalog names counts 1.0.0", s_why);
}

void test_a_file_that_is_not_a_catalog_is_refused(void)
{
    char path[CLI_PATH_MAX_LEN];
    cli_unity_file(s_root, "not.json", "{\"plugins\": []}");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "not.json", .joined = path);
    VerbaLinea entries = {s_entries, sizeof(s_entries), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_plugin_catalog_address.read, CliPluginCatalogAddressCfg, .address = path, .out = &entries, .err_msg = &err_msg));
    (void)mmgr_line_finish(&err_msg);
    CLI_TEST_ASSERT_CONTAINS("no catalog", s_why);
}

void test_a_relative_package_is_beside_its_catalog(void)
{
    char beside[CLI_PATH_MAX_LEN];
    VerbaLinea near = {beside, sizeof(beside), 0ull};
    EMBED_CALL(cli_plugin_catalog.beside, CliPluginCatalogCfg, .base = "https://example.com/orior/catalog.json", .address = "p/a.json", .out = &near);
    CLI_TEST_ASSERT_TEXT("https://example.com/orior/p/a.json", beside, near.at);
}

void test_a_full_address_is_unchanged(void)
{
    char beside[CLI_PATH_MAX_LEN];
    VerbaLinea near = {beside, sizeof(beside), 0ull};
    EMBED_CALL(cli_plugin_catalog.beside, CliPluginCatalogCfg, .base = "https://example.com/catalog.json", .address = "https://cdn.example.com/a.json", .out = &near);
    CLI_TEST_ASSERT_TEXT("https://cdn.example.com/a.json", beside, near.at);
}
