// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_test.c: the version catalog's check: a catalog reads its sections, is checked against itself, and
// completes its sections, keys and aliases, and every catalog read is given back to the cellblock
#include "../../../../../ui/cli/src/builds/version_catalog_internal.h"
#include "cli_check.h"

static const char s_catalog[] =
    "[versions]\ngroovy = \"3.0.5\"\ncheckstyle = \"8.37\"\nmine = \"1.2-SNAPSHOT\"\n\n[libraries]\n"
    "groovy-core = { module = \"org.codehaus.groovy:groovy\", version.ref = \"groovy\" }\n"
    "groovy-json = { module = \"org.codehaus.groovy:groovy-json\", version.ref = \"grovy\" }\n"
    "commons-lang3 = { group = \"org.apache.commons\", name = \"commons-lang3\", version = { strictly = \"[3.8, "
    "4.0[\", "
    "prefer = \"3.9\" } }\nbad = \"only:two\"\n\n[bundles]\ngroovy = [\"groovy-core\", \"groovy-jsn\"]\n\n[plugins]\n"
    "versions = { id = \"com.github.ben-manes.versions\", version = \"0.45.0\" }\n\n[librarys]\n";

static CliVersionsAliases s_aliases;
static char s_out[1u << 18u];

// the first alias of the section `section`, NULL where it has none
static const CliVersionsAlias *catalog_first(unsigned int section)
{
    for (unsigned int at = 0u; at < s_aliases.count; at += 1u)
    {
        if (s_aliases.alias[at].section == section)
        {
            return &s_aliases.alias[at];
        }
    }
    return NULL;
}

// whether `size` bytes at `at` of the aliases' text are `wanted`
static int catalog_said(unsigned int at, unsigned int size, const char *wanted)
{
    return (size == cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = s_aliases.text + at, .other = wanted, .bytes = size}) == 0);
}

// a catalog reads its sections
static void catalog_sections(void)
{
    static const char *const s_names[5] = {"versions", "libraries", "bundles", "plugins", "librarys"};
    cli_lock_take(&g_cli_versions_lock);
    int named = cli_versions_parse(s_catalog, sizeof(s_catalog) - 1u) && (g_cli_versions.errors == 0u) &&
                (g_cli_versions.sections == 5u);
    for (unsigned int at = 0u; named && (at < 5u); at += 1u)
    {
        named = (g_cli_versions.section[at].name_size ==
                 cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL})) &&
                (memor.cmp(&(MemoriaCfg){.src = cli_versions_text(g_cli_versions.section[at].name_at),
                                         .other = s_names[at],
                                         .bytes = g_cli_versions.section[at].name_size}) == 0);
    }
    cli_versions_released();
    cli_lock_give(&g_cli_versions_lock);
    cli_check(named, "a catalog reads its sections, in order and with no error");
    cli_check(cli_versions_aliases(s_catalog, sizeof(s_catalog) - 1u, &s_aliases), "a catalog's aliases are read");
    const CliVersionsAlias *const library = catalog_first(CLI_VERSIONS_LIBRARIES);
    cli_check((library != NULL) && catalog_said(library->alias_at, library->alias_size, "groovy-core") &&
                  catalog_said(library->about_at, library->about_size, "org.codehaus.groovy:groovy, version groovy"),
              "a library's alias stands for its module and version");
    const CliVersionsAlias *const bundle = catalog_first(CLI_VERSIONS_BUNDLES);
    cli_check((bundle != NULL) && catalog_said(bundle->about_at, bundle->about_size, "groovy-core, groovy-jsn"),
              "a bundle's alias stands for its libraries");
    cli_versions_aliases_given(&s_aliases);
}

// a catalog is checked against itself
static void catalog_checked(void)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_build_check("/tree", "/tree/gradle/libs.versions.toml", s_catalog, sizeof(s_catalog) - 1u, &out);
    unsigned int found = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &found);
    const unsigned int grovy = cli_check_finding(found, "catalog-reference", "`grovy`");
    cli_check(grovy != CLI_JSON_NONE, "a version reference no version names is found");
    cli_check(cli_check_finding(found, "catalog-reference", "`groovy-jsn`") != CLI_JSON_NONE,
              "a bundle's library no library names is found");
    cli_check(cli_check_finding(found, "catalog-library", "only:two") != CLI_JSON_NONE,
              "a library of two parts is found");
    const unsigned int snapshot = cli_check_finding(found, "snapshot", NULL);
    cli_check((snapshot != CLI_JSON_NONE) &&
                  cli_json_is(json, cli_json_member(json, snapshot, "severity"), CFG_JSON_NUMBER) &&
                  (json->text[json->node[cli_json_member(json, snapshot, "severity")].text] == '3'),
              "a snapshot is a note");
    cli_check(cli_check_finding(found, "catalog-section", NULL) != CLI_JSON_NONE, "a section no catalog has is found");
    const unsigned int fixes = cli_json_member(json, cli_json_member(json, grovy, "data"), "orior");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, fixes, 0u), "title"),
                                           .other = "Change to groovy",
                                           .cap = CLI_CELLS_POOL}),
              "the nearest version is offered");
    const unsigned int refresh =
        cli_json_item(json, cli_json_member(json, cli_json_member(json, snapshot, "data"), "orior"), 0u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, refresh, "run"), "line"),
                                           .other = "gradle --refresh-dependencies dependencies",
                                           .cap = CLI_CELLS_POOL}),
              "a snapshot offers the line that refreshes it");
    cli_check(json->node[found].count == 5u, "and nothing more is found");
}

// what completes the end of `text`
static const CliCompletions *catalog_completed(const char *text)
{
    return cli_check_build_completed("/tree", "/tree/gradle/libs.versions.toml", text);
}

// a catalog completes its sections, keys and aliases
static void catalog_completes(void)
{
    cli_check(cli_check_completes(catalog_completed("[versions]\na = \"1\"\n[lib"), "libraries"),
              "a section's name completes");
    const CliCompletions *items = catalog_completed("[versions]\ngroovy = \"1\"\n[libraries]\nx = { module = \"a:b\", "
                                                    "version.ref = \"gr");
    cli_check((items->count == 1u) && cli_check_completes(items, "groovy"), "a version reference completes");
    cli_check(cli_check_completes(catalog_completed("[libraries]\nx = { mod"), "module"), "a table's key completes");
    items = catalog_completed("[libraries]\nx = { module = \"a:b\", version.");
    cli_check((items->count == 1u) && cli_check_completes(items, "ref"), "and after a dot");
    items = catalog_completed("[libraries]\ngroovy-core = \"a:b:1\"\n[bundles]\nall = [\"gro");
    cli_check((items->count == 1u) && cli_check_completes(items, "groovy-core"), "a bundle's library completes");
}

int main(void)
{
    catalog_sections();
    catalog_checked();
    catalog_completes();
    cli_check((g_cli_versions_cells.holds == 0ull) && (g_cli_versions_cells.hw != 0ull),
              "every catalog read is given back, the most the catalogs held marked");
    return cli_checked();
}
