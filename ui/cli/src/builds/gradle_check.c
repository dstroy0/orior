// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_check.c: a Gradle script checked: each brace that closes no block and each block left open; each statement of
// a `dependencies` block by the configuration it names, one Gradle 7 dropped with the one that stands in its place and
// one neither Gradle nor the script declares with the configurations near its name; the dependency a configuration
// gives as a string, in its notation, a snapshot noted with the line that refreshes the build's dependencies; and each
// name a catalog's accessor leads, against the catalog's aliases (gradle_references.c). A catalog larger than
// CLI_GRADLE_CATALOG_TEXT is not read
#include "../text/json_tree.h"
#include "gradle_internal.h"
#include "version_catalog_internal.h"

// the most bytes of a catalog read
#define CLI_GRADLE_CATALOG_TEXT (1u << 22u)

// the configurations Gradle 7 has no more, and the one that stands in for each
static const char *const s_removed[5][2] = {{"compile", "implementation"},
                                            {"testCompile", "testImplementation"},
                                            {"runtime", "runtimeOnly"},
                                            {"testRuntime", "testRuntimeOnly"},
                                            {"provided", "compileOnly"}};

// What a check works with: the lines, the findings, the build's folder in the tree, and the program that runs it
typedef struct
{
    const CliBuildLines *lines;
    CliBuildFindings *found;
    char folder[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
} CliGradleCheck;

// a finding that the dependency `given`, over `from` to `to`, is a snapshot, with the line that refreshes it
static void check_snapshot(CliGradleCheck *check, unsigned long long from, unsigned long long to, const char *given,
                           unsigned long long size)
{
    CliLine said = cli_build_said(check->found);
    cli_line_character(&said, '`');
    cli_line_counted(&said, given, size);
    cli_line_text(&said, "` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies");
    const unsigned int finding =
        cli_build_finding(check->found, &said, check->lines, from, to, CLI_BUILD_NOTE, "snapshot");
    CliLine title = cli_build_said(check->found);
    cli_line_text(&title, "Update the snapshot ");
    cli_line_counted(&title, given, size);
    char line[CLI_PATH_LONGEST + 64u];
    CliLine command = {line, sizeof(line), 0ull};
    cli_line_text(&command, check->program);
    cli_line_text(&command, " --refresh-dependencies dependencies");
    cli_build_run_fix(check->found, finding, &title, check->folder,
                      cellul.len(&(CatenaFinitaCfg){.src = check->folder, .cap = CLI_CELLS_POOL}), line, command.at);
}

// the parts of `size` bytes of `text` split at `:`, at most `most` counted: their count, 0 where one is blank, and the
// third's bounds into `third`
static unsigned int check_parts(const char *text, unsigned long long size, unsigned long long *third,
                                unsigned long long *third_size)
{
    unsigned int parts = 0u;
    unsigned long long start = 0ull;
    *third = 0ull;
    *third_size = 0ull;
    for (unsigned long long at = 0ull; at <= size; at += 1u)
    {
        if ((at < size) && (text[at] != ':'))
        {
            continue;
        }
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(text + start, at - start, &from, &to);
        if (to == from)
        {
            return 0u;
        }
        if (parts == 2u)
        {
            *third = start;
            *third_size = at - start;
        }
        parts += 1u;
        start = at + 1u;
    }
    return parts;
}

// the dependency a configuration ending at `after` gives as a string, where it gives one, checked in its notation
static void check_notation(CliGradleCheck *check, unsigned long long after)
{
    const char *const text = g_cli_gradle.text;
    const unsigned char *const class_of = g_cli_gradle.class_of;
    const char *const newline =
        (after < g_cli_gradle.size)
            ? memor.chr(&(MemoriaCfg){.src = text + after, .val = '\n', .bytes = (size_t)(g_cli_gradle.size - after)})
            : NULL;
    const unsigned long long line_end = (newline != NULL) ? (unsigned long long)(newline - text) : g_cli_gradle.size;
    unsigned long long at = after;
    static const char *const s_wrappers[3] = {"platform", "enforcedPlatform", "testFixtures"};
    while ((at < line_end) && ((text[at] == ' ') || (text[at] == '\t') || (text[at] == '(')))
    {
        at += 1u;
    }
    for (unsigned int wrapper = 0u; wrapper < 3u; wrapper += 1u)
    {
        if (cli_text_starts(text + at, line_end - at, s_wrappers[wrapper]))
        {
            at += cellul.len(&(CatenaFinitaCfg){.src = s_wrappers[wrapper], .cap = CLI_CELLS_POOL});
            while ((at < line_end) && ((text[at] == ' ') || (text[at] == '\t') || (text[at] == '(')))
            {
                at += 1u;
            }
        }
    }
    if ((at >= line_end) || (class_of[at] != CLI_GRADLE_STRING) || ((text[at] != '"') && (text[at] != '\'')))
    {
        return;
    }
    unsigned long long end = at + 1u;
    while ((end < line_end) && (class_of[end] == CLI_GRADLE_STRING))
    {
        end += 1u;
    }
    const unsigned long long inner_from = at + 1u;
    const unsigned long long inner_to = ((end > 0u) && (end - 1u > at + 1u)) ? end - 1u : at + 1u;
    const char *const given = text + inner_from;
    const unsigned long long given_size = inner_to - inner_from;
    if (memor.chr(&(MemoriaCfg){.src = given, .val = '$', .bytes = (size_t)given_size}) != NULL)
    {
        return;
    }
    const char *const at_sign = memor.chr(&(MemoriaCfg){.src = given, .val = '@', .bytes = (size_t)given_size});
    const unsigned long long bare = (at_sign != NULL) ? (unsigned long long)(at_sign - given) : given_size;
    unsigned long long third = 0ull;
    unsigned long long third_size = 0ull;
    const unsigned int parts = check_parts(given, bare, &third, &third_size);
    if ((parts < 2u) || (parts > 4u))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_character(&said, '`');
        cli_line_counted(&said, given, given_size);
        cli_line_text(&said, "` is no dependency: a dependency is written `group:name:version`");
        (void)cli_build_finding(check->found, &said, check->lines, inner_from, inner_to, CLI_BUILD_WARNING,
                                "gradle-notation");
    }
    else if ((parts >= 3u) && cli_build_snapshot(given + third, third_size))
    {
        check_snapshot(check, inner_from, inner_to, given, given_size);
    }
}

// a statement of a `dependencies` block checked by the configuration it names
static void check_dependency(void *context, unsigned long long word, unsigned long long word_size,
                             unsigned long long rest, unsigned long long rest_end)
{
    CliGradleCheck *const check = context;
    const char *const text = g_cli_gradle.text;
    const char *const name = text + word;
    const unsigned long long lead = rest + cli_text_lead(text + rest, rest_end - rest);
    const char next = (lead < rest_end) ? text[lead] : '\0';
    unsigned int removed = 5u;
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        removed = ((cellul.len(&(CatenaFinitaCfg){.src = s_removed[at][0], .cap = CLI_CELLS_POOL}) == word_size) &&
                   (memor.cmp(&(MemoriaCfg){.src = s_removed[at][0], .other = name, .bytes = (size_t)word_size}) == 0))
                      ? at
                      : removed;
    }
    if (((next == '=') || (next == '.') || (next == '\0')) && (removed == 5u))
    {
        return;
    }
    if (removed < 5u)
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "Gradle 7 and later have no configuration `");
        cli_line_counted(&said, name, word_size);
        cli_line_text(&said, "`: `");
        cli_line_text(&said, s_removed[removed][1]);
        cli_line_text(&said, "` stands in its place");
        const unsigned int finding = cli_build_finding(check->found, &said, check->lines, word, word + word_size,
                                                       CLI_BUILD_WARNING, "gradle-configuration");
        CliLine title = cli_build_said(check->found);
        cli_line_text(&title, "Change to ");
        cli_line_text(&title, s_removed[removed][1]);
        cli_build_edit_fix(check->found, finding, &title, check->lines, word, word + word_size, s_removed[removed][1],
                           cellul.len(&(CatenaFinitaCfg){.src = s_removed[removed][1], .cap = CLI_CELLS_POOL}));
        return;
    }
    if (next == '{')
    {
        return;
    }
    if (!cli_gradle_known_configuration(name, word_size))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "No configuration `");
        cli_line_counted(&said, name, word_size);
        cli_line_text(&said, "` is declared here, by Gradle or the script");
        const unsigned int finding = cli_build_finding(check->found, &said, check->lines, word, word + word_size,
                                                       CLI_BUILD_WARNING, "gradle-configuration");
        CliBuildNear near;
        cli_build_near_begun(&near, name, word_size);
        for (unsigned int at = 0u; at < 30u; at += 1u)
        {
            cli_build_near_offered(
                &near, g_cli_gradle_configurations[at],
                cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_configurations[at], .cap = CLI_CELLS_POOL}));
        }
        for (unsigned int at = 0u; at < g_cli_gradle.declares; at += 1u)
        {
            cli_build_near_offered(&near, g_cli_gradle.declared[at], g_cli_gradle.declared_size[at]);
        }
        for (unsigned int at = 0u; at < near.count; at += 1u)
        {
            CliLine title = cli_build_said(check->found);
            cli_line_text(&title, "Change to ");
            cli_line_counted(&title, near.near[at], near.near_size[at]);
            cli_build_edit_fix(check->found, finding, &title, check->lines, word, word + word_size, near.near[at],
                               near.near_size[at]);
        }
        return;
    }
    check_notation(check, word + word_size);
}

void cli_gradle_check(const char *root, const char *path, const char *text, unsigned long long size,
                      const CliBuildLines *lines, CliBuildFindings *found)
{
    static CliGradleCheck s_check;
    static CliGradleCatalogs s_catalogs;
    cli_lock_take(&g_cli_gradle_lock);
    if (!cli_gradle_read(text, size))
    {
        cli_lock_give(&g_cli_gradle_lock);
        return;
    }
    s_check.lines = lines;
    s_check.found = found;
    for (unsigned int at = 0u; at < g_cli_gradle.strays; at += 1u)
    {
        CliLine said = cli_build_said(found);
        cli_line_text(&said, "This `}` closes no block");
        (void)cli_build_finding(found, &said, lines, g_cli_gradle.stray[at], g_cli_gradle.stray[at] + 1u,
                                CLI_BUILD_ERROR, "gradle-brace");
    }
    for (unsigned int at = 0u; at < g_cli_gradle.blocks; at += 1u)
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[at];
        if (block->close != CLI_GRADLE_NONE)
        {
            continue;
        }
        CliLine said = cli_build_said(found);
        cli_line_text(&said, "The block");
        if (block->name_size > 0u)
        {
            cli_line_text(&said, " `");
            cli_line_counted(&said, text + block->name, block->name_size);
            cli_line_character(&said, '`');
        }
        cli_line_text(&said, " this `{` opens is not closed");
        (void)cli_build_finding(found, &said, lines, block->open, block->open + 1u, CLI_BUILD_ERROR, "gradle-brace");
    }
    char folder[CLI_PATH_LONGEST];
    cli_gradle_build_root(root, path, folder);
    cli_build_inside(root, folder, s_check.folder);
    cli_gradle_program(folder, s_check.program);
    cli_gradle_declared(lines);
    for (unsigned int at = 0u; at < g_cli_gradle.blocks; at += 1u)
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[at];
        if (cli_gradle_block_named(block, "dependencies"))
        {
            cli_gradle_statements(lines, at, (block->close != CLI_GRADLE_NONE) ? block->close : size, check_dependency,
                                  &s_check);
        }
    }
    cli_gradle_catalogs(folder, &s_catalogs);
    for (unsigned int at = 0u; at < s_catalogs.count; at += 1u)
    {
        // each catalog read where it lies, mapped while its aliases are read and let go after; a catalog whose
        // aliases the cellblock could not meet is not checked against
        unsigned long long read = 0ull;
        const unsigned char *const catalog = cli_file_held(s_catalogs.path[at], &read);
        CliVersionsAliases aliases;
        const int whole = (catalog != NULL) && (read <= CLI_GRADLE_CATALOG_TEXT) &&
                          cli_utf8_whole((const char *)catalog, read) &&
                          cli_versions_aliases((const char *)catalog, read, &aliases);
        cli_file_let_go(catalog, read);
        if (!whole)
        {
            continue;
        }
        char shown[CLI_PATH_LONGEST];
        cli_build_inside(root, s_catalogs.path[at], shown);
        cli_gradle_references(lines, found, s_catalogs.name[at], shown, &aliases);
        cli_versions_aliases_given(&aliases);
    }
    cli_gradle_released();
    cli_lock_give(&g_cli_gradle_lock);
}
