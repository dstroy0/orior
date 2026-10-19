// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_references.c: each name a catalog's accessor leads in a Gradle script read, checked against the catalog's
// aliases: the chain of words after the accessor, less the call it ends in, names an alias of the section its first
// word names, and a chain that names none is a finding with the aliases near it as its fixes
#include "gradle_internal.h"
#include "version_catalog_internal.h"

// the most words of one chain a catalog's accessor leads
#define CLI_GRADLE_SEGMENTS 64u

// the calls a chain of a catalog's accessor ends in that name no alias
static const char *const s_getters[9] = {"get",       "asProvider", "orNull", "getOrNull", "orElse",
                                         "isPresent", "present",    "map",    "flatMap"};

// the run of word letters from `at`: its size
static unsigned long long references_word(unsigned long long at)
{
    unsigned long long end = at;
    while (end < g_cli_gradle.size)
    {
        unsigned long long length = 0ull;
        if (!cli_build_word_letter(cli_text_letter(g_cli_gradle.text, g_cli_gradle.size, end, &length)))
        {
            break;
        }
        end += length;
    }
    return end - at;
}

// the words of the chain a catalog's accessor leads from `start`, each one's span: their count
static unsigned int references_chain(unsigned long long start, unsigned long long *from, unsigned long long *to)
{
    const char *const text = g_cli_gradle.text;
    unsigned int count = 0u;
    for (;;)
    {
        const unsigned long long length = references_word(start);
        if ((length == 0u) || (count == CLI_GRADLE_SEGMENTS))
        {
            break;
        }
        from[count] = start;
        to[count] = start + length;
        count += 1u;
        if ((start + length < g_cli_gradle.size) && (text[start + length] == '.'))
        {
            start += length + 1u;
            continue;
        }
        break;
    }
    if (count > 0u)
    {
        const unsigned long long end = to[count - 1u];
        const unsigned long long lead = end + cli_text_lead(text + end, g_cli_gradle.size - end);
        count -= ((lead < g_cli_gradle.size) && (text[lead] == '(')) ? 1u : 0u;
    }
    for (int popped = 1; popped && (count > 0u);)
    {
        popped = 0;
        for (unsigned int at = 0u; at < 9u; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = s_getters[at], .cap = CLI_CELLS_POOL}) ==
                 to[count - 1u] - from[count - 1u]) &&
                (memor.cmp(&(MemoriaCfg){.src = s_getters[at],
                                         .other = text + from[count - 1u],
                                         .bytes = (size_t)(to[count - 1u] - from[count - 1u])}) == 0))
            {
                popped = 1;
            }
        }
        count -= popped ? 1u : 0u;
    }
    return count;
}

// the alias sections of a chain's first word: the section, the kind it names, and where the alias's path starts
static void references_section_of(const char *word, unsigned long long size, unsigned int *section, const char **kind,
                                  unsigned int *path_from)
{
    static const char *const s_names[3] = {"versions", "plugins", "bundles"};
    static const char *const s_kinds[3] = {"version", "plugin", "bundle"};
    static const unsigned int s_sections[3] = {CLI_VERSIONS_VERSIONS, CLI_VERSIONS_PLUGINS, CLI_VERSIONS_BUNDLES};
    *section = CLI_VERSIONS_LIBRARIES;
    *kind = "library";
    *path_from = 0u;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_names[at], .other = word, .bytes = (size_t)size}) == 0))
        {
            *section = s_sections[at];
            *kind = s_kinds[at];
            *path_from = 1u;
        }
    }
}

// a chain of words of the catalog `name`, at `shown` in the tree, checked against the aliases of the section its first
// word names
static void references_one(const CliBuildLines *lines, CliBuildFindings *found, const char *name, const char *shown,
                           const CliVersionsAliases *aliases, const unsigned long long *from,
                           const unsigned long long *to, unsigned int count)
{
    const char *const text = g_cli_gradle.text;
    unsigned int section = 0u;
    const char *kind = NULL;
    unsigned int path_from = 0u;
    references_section_of(text + from[0], to[0] - from[0], &section, &kind, &path_from);
    if (path_from >= count)
    {
        return;
    }
    const char *const path = text + from[path_from];
    const unsigned long long path_size = to[count - 1u] - from[path_from];
    CliBuildNear near;
    cli_build_near_begun(&near, path, path_size);
    static char s_known[1u << 12u];
    for (unsigned int at = 0u; at < aliases->count; at += 1u)
    {
        const CliVersionsAlias *const alias = &aliases->alias[at];
        if (alias->section != section)
        {
            continue;
        }
        CliLine known = {s_known, sizeof(s_known), 0ull};
        cli_versions_accessor(aliases->text + alias->alias_at, alias->alias_size, &known);
        if ((known.at == path_size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_known, .other = path, .bytes = (size_t)path_size}) == 0))
        {
            return;
        }
    }
    CliLine said = cli_build_said(found);
    cli_line_character(&said, '`');
    cli_line_text(&said, name);
    cli_line_character(&said, '.');
    cli_line_counted(&said, text + from[0], to[count - 1u] - from[0]);
    cli_line_text(&said, "` names no ");
    cli_line_text(&said, kind);
    cli_line_text(&said, " of `");
    cli_line_text(&said, shown);
    cli_line_character(&said, '`');
    const unsigned int finding =
        cli_build_finding(found, &said, lines, from[path_from], to[count - 1u], CLI_BUILD_ERROR, "gradle-catalog");
    // each accessor is as long as its alias, so a cell of the aliases' texts holds those the nearest are chosen from
    const unsigned long long room = aliases->used + 1ull;
    char *const accessors = cli_cell_taken_by(&g_cli_gradle_cells, room, "a catalog's accessors");
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; (accessors != NULL) && (at < aliases->count); at += 1u)
    {
        const CliVersionsAlias *const alias = &aliases->alias[at];
        CliLine known = {accessors + used, room - used, 0ull};
        if (alias->section != section)
        {
            continue;
        }
        cli_versions_accessor(aliases->text + alias->alias_at, alias->alias_size, &known);
        if (cli_line_fits(&known))
        {
            cli_build_near_offered(&near, accessors + used, known.at);
            used += known.at;
        }
    }
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        CliLine title = cli_build_said(found);
        cli_line_text(&title, "Change to ");
        cli_line_text(&title, name);
        cli_line_character(&title, '.');
        if (path_from == 1u)
        {
            cli_line_counted(&title, text + from[0], to[0] - from[0]);
            cli_line_character(&title, '.');
        }
        cli_line_counted(&title, near.near[at], near.near_size[at]);
        cli_build_edit_fix(found, finding, &title, lines, from[path_from], to[count - 1u], near.near[at],
                           near.near_size[at]);
    }
    cli_cell_given_by(&g_cli_gradle_cells, accessors);
}

void cli_gradle_references(const CliBuildLines *lines, CliBuildFindings *found, const char *name, const char *shown,
                           const CliVersionsAliases *aliases)
{
    const char *const text = g_cli_gradle.text;
    const unsigned long long size = g_cli_gradle.size;
    const unsigned long long name_size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + name_size + 1u <= size; at += 1u)
    {
        if ((memor.cmp(&(MemoriaCfg){.src = text + at, .other = name, .bytes = (size_t)name_size}) != 0) ||
            (text[at + name_size] != '.') || (g_cli_gradle.class_of[at] != CLI_GRADLE_CODE))
        {
            continue;
        }
        unsigned long long length = 0ull;
        const unsigned int before = (at > 0u) ? cli_text_letter_before(text, at, &length) : ' ';
        if (cli_build_word_letter(before) || (before == '.'))
        {
            continue;
        }
        unsigned long long from[CLI_GRADLE_SEGMENTS];
        unsigned long long to[CLI_GRADLE_SEGMENTS];
        const unsigned int count = references_chain(at + name_size + 1u, from, to);
        if (count > 0u)
        {
            references_one(lines, found, name, shown, aliases, from, to, count);
        }
        at += name_size;
    }
}
