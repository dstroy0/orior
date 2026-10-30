// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_test.c: the plugin catalog's check: a plugin is found in a catalog and installed only as its entry
// names it
#include "../../../../../ui/cli/src/plugins/plugin_catalog.h"
#include "../../../../../ui/cli/src/text/digest.h"
#include "cli_check.h"

static char s_root[CLI_PATH_MAX_LEN];

// `text` written to the file `name` under the root
static void catalog_written(const char *name, const char *text, unsigned long long size)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(s_root, name, path);
    (void)query_file_written(path, text, size);
}

// the entry of counts with `version` and `sha256` installed: 1, or 0 with why written into `why`
static int catalog_installed(const char *address, const char *version, const char *sha256, VerbaLinea *why)
{
    char plugins[CLI_PATH_MAX_LEN];
    char entry[512];
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea line = {entry, sizeof(entry), 0ull};
    mmgr_line_put(&line, "{\"id\":\"counts\",\"name\":\"Counts\",\"version\":\"");
    mmgr_line_put(&line, version);
    mmgr_line_put(&line, "\",\"package\":\"packages/counts.json\",\"sha256\":\"");
    mmgr_line_put(&line, sha256);
    mmgr_line_put(&line, "\",\"reads\":[\"*.md\"]}");
    VerbaLinea made = {folder, sizeof(folder), 0ull};
    cli_full(s_root, "plugins", plugins);
    const int installed = cli_plugin_catalog_install_into(plugins, address, entry, line.at, &made, why);
    (void)mmgr_line_finish(why);
    return installed;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    static const char s_package[] =
        "{\"orior\":\"plugin\",\"files\":{\"plugin.json\":\"{\\\"id\\\": \\\"counts\\\", "
        "\\\"version\\\": \\\"1.1.0\\\", \\\"kind\\\": \\\"script\\\", \\\"api\\\": 3, "
        "\\\"reads\\\": [\\\"*.md\\\"]}\",\"plugin.js\":\"orior.on(\\\"open\\\", () => {});\"}}";
    char packages[CLI_PATH_MAX_LEN];
    char plugins[CLI_PATH_MAX_LEN];
    char address[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_check_folder("plugin_catalog", s_root);
    cli_full(s_root, "packages", packages);
    cli_full(s_root, "plugins", plugins);
    (void)query_folder_made(packages);
    (void)query_folder_made(plugins);
    catalog_written("packages/counts.json", s_package, sizeof(s_package) - 1u);
    char hash[72];
    VerbaLinea hashed = {hash, sizeof(hash), 0ull};
    cli_sha256_text(s_package, sizeof(s_package) - 1u, &hashed);
    (void)mmgr_line_finish(&hashed);
    static char s_catalog[2048];
    VerbaLinea catalog = {s_catalog, sizeof(s_catalog), 0ull};
    mmgr_line_put(&catalog, "{\"orior\":\"plugin catalog\",\"plugins\":[{\"id\":\"counts\",\"name\":\"Counts\","
                            "\"version\":\"1.1.0\",\"package\":\"packages/counts.json\",\"sha256\":\"");
    mmgr_line_put(&catalog, hash);
    mmgr_line_put(&catalog, "\",\"reads\":[\"*.md\"]},{\"id\":\"../out\",\"name\":\"Out\",\"version\":\"1\","
                            "\"package\":\"x.json\"}]}");
    catalog_written("catalog.json", s_catalog, catalog.at);
    cli_full(s_root, "catalog.json", address);
    static char s_entries[8192];
    VerbaLinea entries = {s_entries, sizeof(s_entries), 0ull};
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int read = cli_plugin_catalog_read(address, &entries, &err_msg);
    unsigned int list = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_entries, entries.at, &list);
    cli_check(read && cli_json_is(json, list, CFG_JSON_ARRAY) && (json->node[list].count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, list, 0u), "id"),
                                                .other = "counts",
                                                .cap = CLI_CELLS_POOL})),
              "an id that is no folder's name is left out");
    err_msg.at = 0ull;
    cli_check(catalog_installed(address, "1.1.0", hash, &err_msg), "the entry installs");
    cli_full(plugins, "counts/plugin.js", path);
    const int js = query_is_file(path);
    cli_full(plugins, "counts/plugin.json", path);
    cli_check(js && query_is_file(path), "its files are in its folder");
    err_msg.at = 0ull;
    cli_check(
        !catalog_installed(address, "1.1.0", "0000000000000000000000000000000000000000000000000000000000000000",
                           &err_msg) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "hash differs", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a package whose hash differs is refused");
    err_msg.at = 0ull;
    cli_check(!catalog_installed(address, "1.0.0", "", &err_msg) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "the catalog names counts 1.0.0",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a plugin.json of another version is refused");
    catalog_written("not.json", "{\"plugins\": []}", 15ull);
    cli_full(s_root, "not.json", address);
    err_msg.at = 0ull;
    entries.at = 0ull;
    const int no_catalog = !cli_plugin_catalog_read(address, &entries, &err_msg);
    (void)mmgr_line_finish(&err_msg);
    cli_check(no_catalog &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "no catalog", .other_cap = CLI_CELLS_POOL}) != NULL),
              "a file that is no catalog is refused as one");
    char beside[CLI_PATH_MAX_LEN];
    VerbaLinea near = {beside, sizeof(beside), 0ull};
    cli_plugin_catalog_beside("https://example.com/orior/catalog.json", "p/a.json", &near);
    cli_check_text(beside, near.at, "https://example.com/orior/p/a.json", "a relative package is beside its catalog");
    near.at = 0ull;
    cli_plugin_catalog_beside("https://example.com/catalog.json", "https://cdn.example.com/a.json", &near);
    cli_check_text(beside, near.at, "https://cdn.example.com/a.json", "a whole address is as it is");
    return cli_checked();
}
