// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_complete.c: what completes the word before the cursor in a Gradle script: after a catalog's accessor, its
// libraries' aliases and its versions', plugins' and bundles' under their sections; and at the head of a statement,
// what the block it stands in takes, a settings script's own words or a build script's at the top, and in
// `dependencies` the configurations, what the block calls beside them, and those the script declares. Nothing completes
// in a string or a comment
#include "../text/json_tree.h"
#include "gradle_internal.h"
#include "version_catalog_internal.h"

// the most lines of a script indexed for its statements, and the most bytes of a catalog read
#define CLI_GRADLE_LINES (1u << 20u)
#define CLI_GRADLE_CATALOG_TEXT (1u << 22u)

// what a settings script names at its top
static const char *const s_settings_top[9] = {"rootProject",
                                              "include",
                                              "includeBuild",
                                              "pluginManagement",
                                              "dependencyResolutionManagement",
                                              "enableFeaturePreview",
                                              "buildCache",
                                              "plugins",
                                              "gradle"};

// whether a letter goes in a catalog's accessor chain
static int complete_chain_letter(unsigned int letter)
{
    return cli_build_word_letter(letter) || (letter == '.');
}

// the aliases of the catalog `first` of the build of the script at `path`, put after `rest`
static void complete_catalog(const char *root, const char *path, const char *first, unsigned long long first_size,
                             const char *rest, unsigned long long rest_size, CliCompletions *items)
{
    static CliGradleCatalogs s_catalogs;
    static char s_value[1u << 12u];
    char folder[CLI_PATH_LONGEST];
    cli_gradle_build_root(root, path, folder);
    cli_gradle_catalogs(folder, &s_catalogs);
    unsigned int chosen = s_catalogs.count;
    for (unsigned int at = 0u; (chosen == s_catalogs.count) && (at < s_catalogs.count); at += 1u)
    {
        chosen = ((cellul.len(&(CatenaFinitaCfg){.src = s_catalogs.name[at], .cap = CLI_CELLS_POOL}) == first_size) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_catalogs.name[at], .other = first, .bytes = first_size}) == 0))
                     ? at
                     : chosen;
    }
    if (chosen == s_catalogs.count)
    {
        return;
    }
    // the catalog read where it lies, mapped while its aliases are read and let go after
    unsigned long long read = 0ull;
    const unsigned char *const catalog = qry_member_file_mapped(s_catalogs.path[chosen], &read);
    CliVersionsAliases aliases;
    const int whole = (catalog != NULL) && (read <= CLI_GRADLE_CATALOG_TEXT) &&
                      cli_utf8_whole((const char *)catalog, read) &&
                      cli_versions_aliases((const char *)catalog, read, &aliases);
    qry_member_file_unmapped(catalog, read);
    if (!whole)
    {
        return;
    }
    static const unsigned int s_order[4] = {CLI_VERSIONS_LIBRARIES, CLI_VERSIONS_VERSIONS, CLI_VERSIONS_PLUGINS,
                                            CLI_VERSIONS_BUNDLES};
    static const char *const s_leads[4] = {"", "versions.", "plugins.", "bundles."};
    static const char *const s_kinds[4] = {"module", "constant", "module", "module"};
    for (unsigned int one = 0u; one < 4u; one += 1u)
    {
        for (unsigned int at = 0u; at < aliases.count; at += 1u)
        {
            const CliVersionsAlias *const alias = &aliases.alias[at];
            if (alias->section != s_order[one])
            {
                continue;
            }
            CliLine value = {s_value, sizeof(s_value), 0ull};
            cli_line_text(&value, s_leads[one]);
            cli_versions_accessor(aliases.text + alias->alias_at, alias->alias_size, &value);
            if (cli_line_fits(&value))
            {
                cli_build_relative(items, rest, rest_size, s_value, value.at, s_kinds[one],
                                   aliases.text + alias->about_at, alias->about_size);
            }
        }
    }
    cli_versions_aliases_given(&aliases);
}

// whether the file at `path` is a settings script, by its name in lower case
static int complete_settings(const char *path)
{
    const char *name = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? at + 1 : name;
    }
    static const char s_settings[] = "settings.gradle";
    for (unsigned int at = 0u; at < sizeof(s_settings) - 1u; at += 1u)
    {
        const char one = ((name[at] >= 'A') && (name[at] <= 'Z')) ? (char)(name[at] - 'A' + 'a') : name[at];
        if (one != s_settings[at])
        {
            return 0;
        }
    }
    return 1;
}

// a configuration the script declares put as an item
static void complete_declared(CliCompletions *items)
{
    for (unsigned int at = 0u; at < g_cli_gradle.declares; at += 1u)
    {
        (void)cli_completion_put(items, g_cli_gradle.declared[at], g_cli_gradle.declared_size[at], "function",
                                 "declared configuration", 22u, g_cli_gradle.declared[at],
                                 g_cli_gradle.declared_size[at], 0);
    }
}

void cli_gradle_complete(const char *root, const char *path, const char *text, unsigned long long size,
                         unsigned long long at, CliCompletions *items)
{
    if (at > CLI_GRADLE_TEXT)
    {
        return;
    }
    cli_lock_take(&g_cli_gradle_lock);
    // the bytes before the cursor classed in a cell of their own, given back once the last of them is read
    unsigned char *const before =
        cli_cell_taken_by(&g_cli_gradle_cells, at + 1ull, "a script's classes before the cursor");
    if (before == NULL)
    {
        cli_lock_give(&g_cli_gradle_lock);
        return;
    }
    cli_gradle_classes(text, at, before);
    const int open = (at > 0u) && (before[at - 1u] != CLI_GRADLE_CODE) && (text[at - 1u] != '\n');
    cli_cell_given_by(&g_cli_gradle_cells, before);
    const unsigned long long token = cli_build_run_before(text, at, complete_chain_letter);
    const char *const dot = memor.chr(&(MemoriaCfg){.src = text + token, .val = '.', .bytes = (size_t)(at - token)});
    if (open)
    {
        cli_lock_give(&g_cli_gradle_lock);
        return;
    }
    if (dot != NULL)
    {
        const unsigned long long first = (unsigned long long)(dot - text);
        complete_catalog(root, path, text + token, first - token, dot + 1, at - first - 1u, items);
        cli_lock_give(&g_cli_gradle_lock);
        return;
    }
    unsigned long long line_start = token;
    while ((line_start > 0u) && (text[line_start - 1u] != '\n'))
    {
        line_start -= 1u;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(text + line_start, token - line_start, &from, &to);
    const char last = (to > from) ? text[line_start + to - 1u] : '\0';
    if ((to > from) && (last != '{') && (last != ';'))
    {
        cli_lock_give(&g_cli_gradle_lock);
        return;
    }
    if (!cli_gradle_read(text, size))
    {
        cli_lock_give(&g_cli_gradle_lock);
        return;
    }
    const unsigned int inner = cli_gradle_block_at(at);
    if (inner == CLI_GRADLE_BLOCKS)
    {
        if (complete_settings(path))
        {
            cli_build_items(items, s_settings_top, 9u, "keyword", "settings");
        }
        else
        {
            unsigned int count = 0u;
            const char *const *const words = cli_gradle_block_words("allprojects", 11u, &count);
            cli_build_items(items, words, count, "keyword", "Gradle");
        }
    }
    else if (cli_gradle_block_named(&g_cli_gradle.block[inner], "dependencies"))
    {
        cli_build_items(items, g_cli_gradle_configurations, 30u, "function", "configuration");
        cli_build_items(items, g_cli_gradle_helpers, 12u, "function", "dependencies");
        // the script's lines a cell of their count, each line's start, given back once the declared are read
        unsigned long long count = 1ull;
        for (unsigned long long byte = 0ull; byte < size; byte += 1ull)
        {
            count += (text[byte] == '\n') ? 1ull : 0ull;
        }
        count = (count < CLI_GRADLE_LINES) ? count : CLI_GRADLE_LINES;
        unsigned int *const starts =
            cli_cell_taken_by(&g_cli_gradle_cells, count * sizeof(unsigned int), "a script's lines");
        if (starts != NULL)
        {
            CliBuildLines lines;
            // a count no more than CLI_GRADLE_LINES, which an unsigned int holds
            cli_build_lines_of(text, size, starts, (unsigned int)count, &lines);
            cli_gradle_declared(&lines);
            complete_declared(items);
        }
        cli_cell_given_by(&g_cli_gradle_cells, starts);
    }
    else
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[inner];
        static char s_name[1u << 12u];
        const unsigned long long name_size = (block->name_size < sizeof(s_name)) ? block->name_size : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = s_name, .src = text + block->name, .bytes = (size_t)name_size});
        s_name[name_size] = '\0';
        unsigned int count = 0u;
        const char *const *const words = cli_gradle_block_words(s_name, name_size, &count);
        cli_build_items(items, words, count, "keyword", s_name);
    }
    cli_gradle_released();
    cli_lock_give(&g_cli_gradle_lock);
}
