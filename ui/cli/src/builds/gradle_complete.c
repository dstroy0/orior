// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_complete.c: what completes the word before the cursor in a Gradle script: after a catalog's accessor, its
// libraries' aliases and its versions', plugins' and bundles' under their sections; and at the head of a statement,
// what the block it stands in takes, a settings script's own words or a build script's at the top, and in
// `dependencies` the configurations, what the block calls beside them, and those the script declares. Nothing completes
// in a string or a comment
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
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
static int complete_chain_letter(const CliEachLetterCfg *args)
{
    return cli_each_letter.build_word_letter(args) || (args->letter == '.');
}

typedef struct
{
    const char *root;
    const char *path;
    const char *first;
    unsigned long long first_size;
    const char *rest;
    unsigned long long remainder_size;
    CliCompletions *items;
} GradleCompleteCtx;
// the aliases of the catalog `first` of the build of the script at `path`, put after `rest`
EMBED_INLINE void complete_catalog(const GradleCompleteCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    CliCompletions *items = args->items;
    static CliGradleCatalogs s_catalogs;
    static char s_value[1u << 12u];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_gradle_root_internal.build_root, CliGradleRootCtx, .root = root, .path = path, .out = folder);
    EMBED_CALL(cli_gradle_folder_internal.catalogs, CliGradleFolderCtx, .folder = folder, .catalogs = &s_catalogs);
    unsigned int chosen = s_catalogs.count;
    for (unsigned int at = 0u; (chosen == s_catalogs.count) && (at < s_catalogs.count); at += 1u)
    {
        chosen =
            ((cellul.len(&(CatenaFinitaCfg){.src = s_catalogs.name[at], .cap = CLI_CELLS_POOL}) == args->first_size) &&
             (memor.cmp(&(MemoriaCfg){.src = s_catalogs.name[at], .other = args->first, .bytes = args->first_size}) ==
              0))
                ? at
                : chosen;
    }
    if (chosen == s_catalogs.count)
    {
        return;
    }
    // the catalog read where it lies, mapped while its aliases are read and let go after
    unsigned long long read = 0ULL;
    const unsigned char *const catalog =
        EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = s_catalogs.path[chosen], .size_pointer = &read,
                   .owner = &g_cli_gradle_cells);
    CliVersionsAliases aliases;
    const int whole =
        (catalog != NULL) && (read <= CLI_GRADLE_CATALOG_TEXT) &&
        EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)catalog, .size = read) &&
        EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases, CliVersionCatalogTextSizeAliasesCtx,
                   .text = (const char *)catalog, .size = read, .aliases = &aliases);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = catalog, .owner = &g_cli_gradle_cells);
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
            VerbaLinea value = {s_value, sizeof(s_value), 0ULL};
            mmgr_line_put(&value, s_leads[one]);
            EMBED_CALL(cli_version_catalog_size_out_alias_internal.versions_accessor, CliVersionCatalogSizeOutAliasCtx,
                       .alias = aliases.text + alias->alias_at, .size = alias->alias_size, .out = &value);
            if (mmgr_line_ok(&value))
            {
                EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = args->rest,
                           .token_size = args->remainder_size, .value = s_value, .value_size = value.at,
                           .kind = s_kinds[one], .detail = aliases.text + alias->about_at,
                           .detail_size = alias->about_size);
            }
        }
    }
    EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases_free,
               CliVersionCatalogTextSizeAliasesCtx, .aliases = &aliases);
}

// whether the file at `path` is a settings script, by its name in lower case
EMBED_INLINE int complete_settings(const GradleCompleteCtx *args)
{
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
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
EMBED_INLINE void complete_declare(const GradleCompleteCtx *args)
{
    CliCompletions *items = args->items;
    for (unsigned int at = 0u; at < g_cli_gradle.declares; at += 1u)
    {
        (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = items,
                         .label = g_cli_gradle.declared[at], .label_size = g_cli_gradle.declared_size[at],
                         .kind = "function", .detail = "declared configuration", .detail_size = 22u,
                         .insert = g_cli_gradle.declared[at], .insert_size = g_cli_gradle.declared_size[at],
                         .snippet = 0);
    }
}

void cli_gradle_complete(const CliGradleTextCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    CliCompletions *items = args->items;
    if (args->at > CLI_GRADLE_TEXT)
    {
        return;
    }
    // the bytes before the cursor classed in a cell of their own, given back once the last of them is read
    unsigned char *const before = g_cli_cells->temporary_buf_alloc(args->at + 1ULL, &g_cli_gradle_cells);
    if (before == NULL)
    {
        return;
    }
    EMBED_CALL(cli_gradle_text_size_class_of_internal.classes, CliGradleTextSizeClassOfCtx, .text = text,
               .size = args->at, .class_of = before);
    const int open = (args->at > 0u) && (before[args->at - 1u] != CLI_GRADLE_CODE) && (text[args->at - 1u] != '\n');
    g_cli_cells->persistent_buf_release(before, &g_cli_gradle_cells);
    const unsigned long long token = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = text,
                                                .at = args->at, .of = complete_chain_letter);
    const char *const dot =
        memor.chr(&(MemoriaCfg){.src = text + token, .val = '.', .bytes = (size_t)(args->at - token)});
    if (open)
    {
        return;
    }
    if (dot != NULL)
    {
        const unsigned long long first = (unsigned long long)(dot - text);
        EMBED_CALL(complete_catalog, GradleCompleteCtx, .root = args->root, .path = args->path, .first = text + token,
                   .first_size = first - token, .rest = dot + 1, .remainder_size = args->at - first - 1u,
                   .items = items);
        return;
    }
    unsigned long long line_start = token;
    line_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = line_start,
                            .set = "\n");
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + line_start,
               .size = token - line_start, .from_u64s = &from, .to_u64s = &to);
    const char last = (to > from) ? text[line_start + to - 1u] : '\0';
    if ((to > from) && (last != '{') && (last != ';'))
    {
        return;
    }
    if (!EMBED_CALL(cli_gradle_text_size_internal.read, CliGradleTextSizeCtx, .text = text, .size = size))
    {
        return;
    }
    const unsigned int inner = cli_gradle_text_internal.block_at(args);
    if (inner == CLI_GRADLE_BLOCKS)
    {
        if (EMBED_CALL(complete_settings, GradleCompleteCtx, .path = args->path))
        {
            EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items,
                       .words = s_settings_top, .count = 9u, .kind = "keyword", .detail = "settings");
        }
        else
        {
            unsigned int count = 0u;
            const char *const *const words = EMBED_CALL(cli_gradle_size_internal.block_words, CliGradleSizeCtx,
                                                        .name = "allprojects", .size = 11u, .count = &count);
            EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items, .words = words,
                       .count = count, .kind = "keyword", .detail = "Gradle");
        }
    }
    else if (EMBED_CALL(cli_gradle_internal.block_by_name, CliGradleCtx, .block = &g_cli_gradle.block[inner],
                        .name = "dependencies"))
    {
        EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items,
                   .words = g_cli_gradle_configurations, .count = 30u, .kind = "function", .detail = "configuration");
        EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items,
                   .words = g_cli_gradle_helpers, .count = 12u, .kind = "function", .detail = "dependencies");
        // the script's lines a cell of their count, each line's start, given back once the declared are read
        unsigned long long count = 1ULL;
        for (unsigned long long byte = 0ULL; byte < size; byte += 1ULL)
        {
            count += (text[byte] == '\n') ? 1ULL : 0ULL;
        }
        count = (count < CLI_GRADLE_LINES) ? count : CLI_GRADLE_LINES;
        unsigned int *const starts =
            g_cli_cells->temporary_buf_alloc(count * sizeof(unsigned int), &g_cli_gradle_cells);
        if (starts != NULL)
        {
            CliBuildLines lines;
            // a count no more than CLI_GRADLE_LINES, which an unsigned int stores
            EMBED_CALL(cli_builds_text_size.build_lines_get, CliBuildsTextSizeCfg, .text = text, .size = size,
                       .starts = starts, .most = (unsigned int)count, .lines_out = &lines);
            EMBED_CALL(cli_gradle_lines_internal.declare, CliGradleLinesCtx, .lines = &lines);
            EMBED_CALL(complete_declare, GradleCompleteCtx, .items = items);
        }
        g_cli_cells->persistent_buf_release(starts, &g_cli_gradle_cells);
    }
    else
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[inner];
        static char s_name[1u << 12u];
        const unsigned long long name_size = (block->name_size < sizeof(s_name)) ? block->name_size : 0ULL;
        memor.cpy(&(MemoriaCfg){.dst = s_name, .src = text + block->name, .bytes = (size_t)name_size});
        s_name[name_size] = '\0';
        unsigned int count = 0u;
        const char *const *const words = EMBED_CALL(cli_gradle_size_internal.block_words, CliGradleSizeCtx,
                                                    .name = s_name, .size = name_size, .count = &count);
        EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items, .words = words,
                   .count = count, .kind = "keyword", .detail = s_name);
    }
    cli_gradle_release();
}
