// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_build.c: a Gradle build around a script: whether a word is a configuration Gradle or the script declares, the
// folder of the build a file belongs to, the program that runs it, and the catalogs of its `gradle` folder, at max
// CLI_GRADLE_CATALOGS of them
#include "../command_line/callback_cfg.h"
#include "../text/sort.h"
#include "gradle_internal.h"

#include "../text/text.h"

int cli_gradle_known_configuration(const CliGradleSizeWordCtx *args)
{
    for (unsigned int at = 0u; at < 30u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_configurations[at], .cap = CLI_POOL_BYTES}) ==
             args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_gradle_configurations[at],
                                     .other = args->word_text,
                                     .bytes = (embed_word)args->size}) == 0))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < 12u; at += 1u)
    {
        unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_helpers[at], .cap = CLI_POOL_BYTES});
        while ((length >= 2u) && (memor.cmp(&(MemoriaCfg){
                                      .src = g_cli_gradle_helpers[at] + length - 2u, .other = "()", .bytes = 2u}) == 0))
        {
            length -= 2u;
        }
        if ((length == args->size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = g_cli_gradle_helpers[at], .other = args->word_text, .bytes = (embed_word)args->size}) == 0))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < g_cli_gradle.declares; at += 1u)
    {
        if ((g_cli_gradle.declared_size[at] == args->size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = g_cli_gradle.declared[at], .other = args->word_text, .bytes = (embed_word)args->size}) == 0))
        {
            return 1;
        }
    }
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const embed_word length =
            cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_suffixes[at], .cap = CLI_POOL_BYTES});
        if ((args->size > length) && (memor.cmp(&(MemoriaCfg){.src = args->word_text + args->size - length,
                                                              .other = g_cli_gradle_suffixes[at],
                                                              .bytes = length}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    const char *root;
    const char *folder;
    const char *name;
    const char *from;
    char *out;
} GradleBuildCtx;
// whether `folder` is `root` or lies inside it
EMBED_INLINE int gradle_inside(const GradleBuildCtx *args)
{
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES});
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->folder, .other_pointer = args->root,
                    .size = length))
    {
        return 0;
    }
    const char after = args->folder[length];
    return (after == '\0') || (after == '/') || (after == '\\') ||
           ((length > 0u) && ((args->root[length - 1u] == '/') || (args->root[length - 1u] == '\\')));
}

// whether `name` is a file in `folder`
EMBED_INLINE int gradle_has_file(const GradleBuildCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = args->name, .joined = path);
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
}

// `from` copied into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void gradle_copy(const GradleBuildCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->from);
    (void)mmgr_line_finish(&line);
}

void cli_gradle_build_root(const CliGradleRootCtx *args)
{
    const char *path = args->path;
    char start[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = path, .out = start))
    {
        EMBED_CALL(gradle_copy, GradleBuildCtx, .from = args->root, .out = start);
    }
    if (EMBED_CALL(cli_builds_path.build_type_get, CliBuildsPathCfg, .path = path) == CLI_BUILD_CATALOG)
    {
        EMBED_CALL(
            gradle_copy, GradleBuildCtx,
            .from = EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = start, .out = above)
                        ? above
                        : args->root,
            .out = start);
    }
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(gradle_copy, GradleBuildCtx, .from = start, .out = folder);
    for (;;)
    {
        if (EMBED_CALL(gradle_has_file, GradleBuildCtx, .folder = folder, .name = "settings.gradle") ||
            EMBED_CALL(gradle_has_file, GradleBuildCtx, .folder = folder, .name = "settings.gradle.kts") ||
            EMBED_CALL(gradle_has_file, GradleBuildCtx, .folder = folder, .name = "gradlew"))
        {
            EMBED_CALL(gradle_copy, GradleBuildCtx, .from = folder, .out = args->out);
            return;
        }
        if ((cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = args->root, .cap = CLI_POOL_BYTES})) ||
            !EMBED_CALL(gradle_inside, GradleBuildCtx, .root = args->root, .folder = folder) ||
            !EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = folder, .out = above))
        {
            EMBED_CALL(gradle_copy, GradleBuildCtx, .from = start, .out = args->out);
            return;
        }
        EMBED_CALL(gradle_copy, GradleBuildCtx, .from = above, .out = folder);
    }
}

void cli_gradle_program(const CliBuildsCtx *args)
{
    EMBED_CALL(gradle_copy, GradleBuildCtx,
               .from = EMBED_CALL(gradle_has_file, GradleBuildCtx, .folder = args->folder, .name = "gradlew")
                           ? "./gradlew"
                           : "gradle",
               .out = args->out);
}

// The catalogs of a `gradle` folder being gathered: the catalogs, and the folder
typedef struct
{
    CliGradleCatalogs *catalogs;
    const char *folder;
} CliGradleGathered;

// a catalog of the `gradle` folder stored, where its name ends `.versions.toml`
static void gradle_catalog_visit(const CliEachNameIsFolderCfg *args)
{
    const CliGradleGathered *const gathered = args->context;
    CliGradleCatalogs *const catalogs = gathered->catalogs;
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    if (args->is_folder || (length < 14u) ||
        (!cellul.eq(
            &(CatenaFinitaCfg){.src = args->name + length - 14u, .other = ".versions.toml", .cap = CLI_POOL_BYTES})) ||
        (catalogs->count == CLI_GRADLE_CATALOGS) || (length - 14u >= CLI_NAME_MAX_LEN))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = catalogs->name[catalogs->count], .src = args->name, .bytes = length - 14u});
    catalogs->name[catalogs->count][length - 14u] = '\0';
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = gathered->folder, .relative = args->name,
               .joined = catalogs->path[catalogs->count]);
    catalogs->count += 1u;
}

// two catalogs in the order of their names, then their paths
static int gradle_catalog_order(const void *one, const void *other)
{
    const char *const *const left = one;
    const char *const *const right = other;
    const int names = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = left[0], .other_pointer = right[0]);
    return (names != 0)
               ? names
               : EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = left[1], .other_pointer = right[1]);
}

void cli_gradle_catalogs(const CliGradleFolderCtx *args)
{
    static const char *s_pairs[CLI_GRADLE_CATALOGS][2];
    static CliGradleCatalogs s_sorted;
    args->catalogs->count = 0u;
    char gradle[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = "gradle", .joined = gradle);
    CliGradleGathered gathered = {args->catalogs, gradle};
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = gradle,
                     .each = gradle_catalog_visit, .context_bytess = &gathered);
    for (unsigned int at = 0u; at < args->catalogs->count; at += 1u)
    {
        s_pairs[at][0] = args->catalogs->name[at];
        s_pairs[at][1] = args->catalogs->path[at];
    }
    EMBED_CALL(cli_sort, CliSortCfg, .base = s_pairs, .count = args->catalogs->count, .size = sizeof(s_pairs[0]),
               .order = gradle_catalog_order);
    for (unsigned int at = 0u; at < args->catalogs->count; at += 1u)
    {
        EMBED_CALL(gradle_copy, GradleBuildCtx, .from = s_pairs[at][0], .out = s_sorted.name[at]);
        EMBED_CALL(gradle_copy, GradleBuildCtx, .from = s_pairs[at][1], .out = s_sorted.path[at]);
    }
    for (unsigned int at = 0u; at < args->catalogs->count; at += 1u)
    {
        EMBED_CALL(gradle_copy, GradleBuildCtx, .from = s_sorted.name[at], .out = args->catalogs->name[at]);
        EMBED_CALL(gradle_copy, GradleBuildCtx, .from = s_sorted.path[at], .out = args->catalogs->path[at]);
    }
}
