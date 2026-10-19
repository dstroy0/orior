// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_build.c: a Gradle build around a script: whether a word is a configuration Gradle or the script declares, the
// folder of the build a file belongs to, the program that runs it, and the catalogs of its `gradle` folder, at most
// CLI_GRADLE_CATALOGS of them
#include "gradle_internal.h"

#include "../text/text.h"
#include <stdlib.h>

int cli_gradle_known_configuration(const char *word, unsigned long long size)
{
    for (unsigned int at = 0u; at < 30u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_configurations[at], .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_gradle_configurations[at], .other = word, .bytes = (size_t)size}) ==
             0))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < 12u; at += 1u)
    {
        unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_helpers[at], .cap = CLI_CELLS_POOL});
        while ((length >= 2u) && (memor.cmp(&(MemoriaCfg){
                                      .src = g_cli_gradle_helpers[at] + length - 2u, .other = "()", .bytes = 2u}) == 0))
        {
            length -= 2u;
        }
        if ((length == size) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_gradle_helpers[at], .other = word, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < g_cli_gradle.declares; at += 1u)
    {
        if ((g_cli_gradle.declared_size[at] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_gradle.declared[at], .other = word, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_suffixes[at], .cap = CLI_CELLS_POOL});
        if ((size > length) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = word + size - length, .other = g_cli_gradle_suffixes[at], .bytes = length}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// whether `folder` is `root` or lies inside it
static int gradle_inside(const char *root, const char *folder)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    if (!cli_text_same(folder, root, length))
    {
        return 0;
    }
    const char after = folder[length];
    return (after == '\0') || (after == '/') || (after == '\\') ||
           ((length > 0u) && ((root[length - 1u] == '/') || (root[length - 1u] == '\\')));
}

// whether `name` is a file in `folder`
static int gradle_holds(const char *folder, const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    return query_is_file(path);
}

// `from` copied into `out`, which holds CLI_PATH_LONGEST
static void gradle_copied(const char *from, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, from);
    (void)cli_line_closed(&line);
}

void cli_gradle_build_root(const char *root, const char *path, char *out)
{
    char start[CLI_PATH_LONGEST];
    char above[CLI_PATH_LONGEST];
    if (!cli_path_parent(path, start))
    {
        gradle_copied(root, start);
    }
    if (cli_build_kind_of(path) == CLI_BUILD_CATALOG)
    {
        gradle_copied(cli_path_parent(start, above) ? above : root, start);
    }
    char folder[CLI_PATH_LONGEST];
    gradle_copied(start, folder);
    for (;;)
    {
        if (gradle_holds(folder, "settings.gradle") || gradle_holds(folder, "settings.gradle.kts") ||
            gradle_holds(folder, "gradlew"))
        {
            gradle_copied(folder, out);
            return;
        }
        if ((cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = root, .cap = CLI_CELLS_POOL})) ||
            !gradle_inside(root, folder) || !cli_path_parent(folder, above))
        {
            gradle_copied(start, out);
            return;
        }
        gradle_copied(above, folder);
    }
}

void cli_gradle_program(const char *folder, char *out)
{
    gradle_copied(gradle_holds(folder, "gradlew") ? "./gradlew" : "gradle", out);
}

// The catalogs of a `gradle` folder being gathered: the catalogs, and the folder
typedef struct
{
    CliGradleCatalogs *catalogs;
    const char *folder;
} CliGradleGathered;

// a catalog of the `gradle` folder kept, where its name ends `.versions.toml`
static void gradle_catalog_seen(void *context, const char *name, int is_folder)
{
    const CliGradleGathered *const gathered = context;
    CliGradleCatalogs *const catalogs = gathered->catalogs;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length < 14u) ||
        (!cellul.eq(
            &(CatenaFinitaCfg){.src = name + length - 14u, .other = ".versions.toml", .cap = CLI_CELLS_POOL})) ||
        (catalogs->count == CLI_GRADLE_CATALOGS) || (length - 14u >= CLI_NAME_LONGEST))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = catalogs->name[catalogs->count], .src = name, .bytes = length - 14u});
    catalogs->name[catalogs->count][length - 14u] = '\0';
    cli_full(gathered->folder, name, catalogs->path[catalogs->count]);
    catalogs->count += 1u;
}

// two catalogs in the order of their names, then their paths
static int gradle_catalog_order(const void *one, const void *other)
{
    const char *const *const left = one;
    const char *const *const right = other;
    const int names = cli_text_order(left[0], right[0]);
    return (names != 0) ? names : cli_text_order(left[1], right[1]);
}

void cli_gradle_catalogs(const char *folder, CliGradleCatalogs *catalogs)
{
    static const char *s_pairs[CLI_GRADLE_CATALOGS][2];
    static CliGradleCatalogs s_sorted;
    catalogs->count = 0u;
    char gradle[CLI_PATH_LONGEST];
    cli_full(folder, "gradle", gradle);
    CliGradleGathered gathered = {catalogs, gradle};
    (void)query_folder_each(gradle, gradle_catalog_seen, &gathered);
    for (unsigned int at = 0u; at < catalogs->count; at += 1u)
    {
        s_pairs[at][0] = catalogs->name[at];
        s_pairs[at][1] = catalogs->path[at];
    }
    qsort(s_pairs, catalogs->count, sizeof(s_pairs[0]), gradle_catalog_order);
    for (unsigned int at = 0u; at < catalogs->count; at += 1u)
    {
        gradle_copied(s_pairs[at][0], s_sorted.name[at]);
        gradle_copied(s_pairs[at][1], s_sorted.path[at]);
    }
    for (unsigned int at = 0u; at < catalogs->count; at += 1u)
    {
        gradle_copied(s_sorted.name[at], catalogs->name[at]);
        gradle_copied(s_sorted.path[at], catalogs->path[at]);
    }
}
