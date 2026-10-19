// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugin_catalog_test.c: the plugin catalog's check: a plugin is found in a catalog and installed only as its entry
// names it
#include "../../../../../ui/cli/src/plugins/plugin_catalog.h"
#include "../../../../../ui/cli/src/text/digest.h"
#include "cli_check.h"

static char s_root[CLI_PATH_LONGEST];

// `text` written to the file `name` under the root
static void catalog_written(const char *name, const char *text, unsigned long long size)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    (void)query_file_written(path, text, size);
}

// the entry of counts with `version` and `sha256` installed: 1, or 0 with why said into `why`
static int catalog_installed(const char *address, const char *version, const char *sha256, CliLine *why)
{
    char plugins[CLI_PATH_LONGEST];
    char entry[512];
    char folder[CLI_PATH_LONGEST];
    CliLine line = {entry, sizeof(entry), 0ull};
    cli_line_text(&line, "{\"id\":\"counts\",\"name\":\"Counts\",\"version\":\"");
    cli_line_text(&line, version);
    cli_line_text(&line, "\",\"package\":\"packages/counts.json\",\"sha256\":\"");
    cli_line_text(&line, sha256);
    cli_line_text(&line, "\",\"reads\":[\"*.md\"]}");
    CliLine made = {folder, sizeof(folder), 0ull};
    cli_full(s_root, "plugins", plugins);
    const int installed = cli_plugin_catalog_install_into(plugins, address, entry, line.at, &made, why);
    (void)cli_line_closed(why);
    return installed;
}

int main(void)
{
    static const char s_package[] =
        "{\"orior\":\"plugin\",\"files\":{\"plugin.json\":\"{\\\"id\\\": \\\"counts\\\", "
        "\\\"version\\\": \\\"1.1.0\\\", \\\"kind\\\": \\\"script\\\", \\\"api\\\": 3, "
        "\\\"reads\\\": [\\\"*.md\\\"]}\",\"plugin.js\":\"orior.on(\\\"open\\\", () => {});\"}}";
    char packages[CLI_PATH_LONGEST];
    char plugins[CLI_PATH_LONGEST];
    char address[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("plugin_catalog", s_root);
    cli_full(s_root, "packages", packages);
    cli_full(s_root, "plugins", plugins);
    (void)query_folder_made(packages);
    (void)query_folder_made(plugins);
    catalog_written("packages/counts.json", s_package, sizeof(s_package) - 1u);
    char hash[72];
    CliLine hashed = {hash, sizeof(hash), 0ull};
    cli_sha256_text(s_package, sizeof(s_package) - 1u, &hashed);
    (void)cli_line_closed(&hashed);
    static char s_catalog[2048];
    CliLine catalog = {s_catalog, sizeof(s_catalog), 0ull};
    cli_line_text(&catalog, "{\"orior\":\"plugin catalog\",\"plugins\":[{\"id\":\"counts\",\"name\":\"Counts\","
                            "\"version\":\"1.1.0\",\"package\":\"packages/counts.json\",\"sha256\":\"");
    cli_line_text(&catalog, hash);
    cli_line_text(&catalog, "\",\"reads\":[\"*.md\"]},{\"id\":\"../out\",\"name\":\"Out\",\"version\":\"1\","
                            "\"package\":\"x.json\"}]}");
    catalog_written("catalog.json", s_catalog, catalog.at);
    cli_full(s_root, "catalog.json", address);
    static char s_entries[8192];
    CliLine entries = {s_entries, sizeof(s_entries), 0ull};
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    const int read = cli_plugin_catalog_read(address, &entries, &said);
    unsigned int list = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_entries, entries.at, &list);
    cli_check(read && cli_json_is(json, list, CFG_JSON_ARRAY) && (json->node[list].count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, list, 0u), "id"),
                                                .other = "counts",
                                                .cap = CLI_CELLS_POOL})),
              "an id that is no folder's name is left out");
    said.at = 0ull;
    cli_check(catalog_installed(address, "1.1.0", hash, &said), "the entry installs");
    cli_full(plugins, "counts/plugin.js", path);
    const int js = query_is_file(path);
    cli_full(plugins, "counts/plugin.json", path);
    cli_check(js && query_is_file(path), "its files are in its folder");
    said.at = 0ull;
    cli_check(
        !catalog_installed(address, "1.1.0", "0000000000000000000000000000000000000000000000000000000000000000",
                           &said) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "hash differs", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a package whose hash differs is refused");
    said.at = 0ull;
    cli_check(!catalog_installed(address, "1.0.0", "", &said) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "the catalog names counts 1.0.0",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a plugin.json of another version is refused");
    catalog_written("not.json", "{\"plugins\": []}", 15ull);
    cli_full(s_root, "not.json", address);
    said.at = 0ull;
    entries.at = 0ull;
    const int no_catalog = !cli_plugin_catalog_read(address, &entries, &said);
    (void)cli_line_closed(&said);
    cli_check(no_catalog &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "no catalog", .other_cap = CLI_CELLS_POOL}) != NULL),
              "a file that is no catalog is refused as one");
    char beside[CLI_PATH_LONGEST];
    CliLine near = {beside, sizeof(beside), 0ull};
    cli_plugin_catalog_beside("https://example.com/orior/catalog.json", "p/a.json", &near);
    cli_check_text(beside, near.at, "https://example.com/orior/p/a.json", "a relative package is beside its catalog");
    near.at = 0ull;
    cli_plugin_catalog_beside("https://example.com/catalog.json", "https://cdn.example.com/a.json", &near);
    cli_check_text(beside, near.at, "https://cdn.example.com/a.json", "a whole address is as it is");
    return cli_checked();
}
