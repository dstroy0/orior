// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds.c: a build file's kind by its name, and its check, its completion, the names SCons gives it and the line that
// updates every snapshot dependency of its build, each handed to the reader of its kind. One check runs at a time, its
// findings stored in their own room
#include "gradle_internal.h"
#include "maven_internal.h"
#include "scons_internal.h"
#include "version_catalog_internal.h"

static CliLock s_builds_lock = CLI_LOCK_INIT;
static CliBuildFindings s_found;

unsigned int cli_build_kind_of(const char *path)
{
    const char *name = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? at + 1 : name;
    }
    char lower[CLI_NAME_MAX_LEN];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (length >= sizeof(lower))
    {
        return 0u;
    }
    for (size_t at = 0u; at <= length; at += 1u)
    {
        lower[at] = ((name[at] >= 'A') && (name[at] <= 'Z')) ? (char)(name[at] - 'A' + 'a') : name[at];
    }
    if (cli_text_ends(lower, length, ".gradle") || cli_text_ends(lower, length, ".gradle.kts"))
    {
        return CLI_BUILD_GRADLE;
    }
    if (cli_text_ends(lower, length, ".versions.toml"))
    {
        return CLI_BUILD_CATALOG;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "pom.xml", .cap = CLI_CELLS_POOL}))
    {
        return CLI_BUILD_MAVEN;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "sconstruct", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "sconscript", .cap = CLI_CELLS_POOL})))
    {
        return CLI_BUILD_SCONS;
    }
    return 0u;
}

void cli_build_check(const char *root, const char *path, const char *text, unsigned long long size, VerbaLinea *out)
{
    cli_lock_take(&s_builds_lock);
    cli_build_findings_begun(&s_found);
    CliBuildLines lines;
    unsigned int *const starts = cli_build_lines_taken(text, size, &lines);
    const unsigned int kind = cli_build_kind_of(path);
    if (kind == CLI_BUILD_GRADLE)
    {
        cli_gradle_check(root, path, text, size, &lines, &s_found);
    }
    else if (kind == CLI_BUILD_CATALOG)
    {
        cli_versions_check(root, path, text, size, &lines, &s_found);
    }
    else if (kind == CLI_BUILD_MAVEN)
    {
        cli_maven_check(root, path, text, size, &lines, &s_found);
    }
    else if (kind == CLI_BUILD_SCONS)
    {
        cli_scons_check(root, path, text, size, &lines, &s_found);
    }
    cli_build_findings_written(&s_found, out);
    cli_build_findings_given(&s_found);
    cli_cell_release(starts, &g_cli_build_cells);
    cli_lock_give(&s_builds_lock);
}

void cli_build_complete(const char *root, const char *path, const char *text, unsigned long long size,
                        unsigned int line, unsigned int col, CliCompletions *items)
{
    const unsigned long long at = cli_build_byte_at(text, size, line, col);
    const unsigned int kind = cli_build_kind_of(path);
    if (kind == CLI_BUILD_GRADLE)
    {
        cli_gradle_complete(root, path, text, size, at, items);
    }
    else if (kind == CLI_BUILD_CATALOG)
    {
        cli_versions_complete(text, size, at, items);
    }
    else if (kind == CLI_BUILD_MAVEN)
    {
        cli_maven_complete(path, text, size, at, items);
    }
    else if (kind == CLI_BUILD_SCONS)
    {
        cli_scons_complete(text, size, at, items);
    }
    cli_completions_unique(items);
}

void cli_build_given_names(const char *path, const char *text, unsigned long long size,
                           void (*each)(void *context, const char *name, unsigned long long size), void *context)
{
    if (cli_build_kind_of(path) == CLI_BUILD_SCONS)
    {
        cli_scons_given(text, size, each, context);
    }
}

// `from` copied into `out`, sized for CLI_PATH_MAX_LEN
static void builds_copied(const char *from, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, from);
    (void)mmgr_line_finish(&line);
}

int cli_build_snapshots_line(const char *root, const char *path, char *folder, char *line, VerbaLinea *err_msg)
{
    static const char *const s_beside[3] = {"pom.xml", "build.gradle", "build.gradle.kts"};
    static const char *const s_top[5] = {"pom.xml", "build.gradle", "build.gradle.kts", "settings.gradle",
                                         "settings.gradle.kts"};
    char places[9][CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    if (path != NULL)
    {
        builds_copied(path, places[count]);
        count += 1u;
        char above[CLI_PATH_MAX_LEN];
        for (unsigned int at = 0u; cli_path_parent(path, above) && (at < 3u); at += 1u)
        {
            cli_full(above, s_beside[at], places[count]);
            count += 1u;
        }
    }
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        cli_full(root, s_top[at], places[count]);
        count += 1u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int kind = cli_build_kind_of(places[at]);
        if (!query_is_file(places[at]))
        {
            continue;
        }
        char build[CLI_PATH_MAX_LEN];
        char program[CLI_PATH_MAX_LEN];
        VerbaLinea command = {line, CLI_PATH_MAX_LEN, 0ull};
        if (kind == CLI_BUILD_MAVEN)
        {
            if (!cli_path_parent(places[at], build))
            {
                builds_copied(root, build);
            }
            cli_maven_program(root, build, program);
            mmgr_line_put(&command, program);
            mmgr_line_put(&command, " -U dependency:resolve");
        }
        else if ((kind == CLI_BUILD_GRADLE) || (kind == CLI_BUILD_CATALOG))
        {
            cli_gradle_build_root(root, places[at], build);
            cli_gradle_program(build, program);
            mmgr_line_put(&command, program);
            mmgr_line_put(&command, " --refresh-dependencies dependencies");
        }
        else
        {
            continue;
        }
        (void)mmgr_line_finish(&command);
        cli_build_inside(root, build, folder);
        return 1;
    }
    mmgr_line_put(err_msg, "No Maven or Gradle build here: a pom.xml, a build.gradle or a build.gradle.kts");
    return 0;
}
