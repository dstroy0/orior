// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_check.c: a Gradle script checked: each brace that closes no block and each block left open; each statement of
// a `dependencies` block by the configuration it names, one Gradle 7 dropped with the one that stands in its place and
// one neither Gradle nor the script declares with the configurations near its name; the dependency a configuration
// gives as a string, in its notation, a snapshot noted with the line that refreshes the build's dependencies; and each
// name a catalog's accessor leads, against the catalog's aliases (gradle_references.c). A catalog larger than
// CLI_GRADLE_CATALOG_TEXT is not read
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "gradle_internal.h"
#include "version_catalog_internal.h"

// What a check works with: the lines, the findings, the build's folder in the tree, and the program that runs it
typedef struct
{
    const CliBuildLines *lines;
    CliBuildFindings *found;
    char folder[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
} CliGradleCheck;
// the most bytes of a catalog read
#define CLI_GRADLE_CATALOG_TEXT (1u << 22u)

// the configurations Gradle 7 has no more, and the one that stands in for each
static const char *const s_removed[5][2] = {{"compile", "implementation"},
                                            {"testCompile", "testImplementation"},
                                            {"runtime", "runtimeOnly"},
                                            {"testRuntime", "testRuntimeOnly"},
                                            {"provided", "compileOnly"}};

typedef struct
{
    CliGradleCheck *check;
    unsigned long long from;
    unsigned long long to;
    const char *supplied;
    unsigned long long size;
    const char *text;
    unsigned long long *third;
    unsigned long long *third_size;
    unsigned long long after;
} GradleCheckCtx;
// a finding that the dependency `supplied`, over `from` to `to`, is a snapshot, with the line that refreshes it
EMBED_INLINE void check_snapshot(const GradleCheckCtx *args)
{
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(&msg, args->supplied, args->size);
    mmgr_line_put(&msg, "` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies");
    const unsigned int finding =
        EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                   .lines = args->check->lines, .from = from, .to = to, .severity = CLI_BUILD_NOTE, .code = "snapshot");
    VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_put(&title, "Update the snapshot ");
    mmgr_line_put_n(&title, args->supplied, args->size);
    char line[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea command = {line, sizeof(line), 0ULL};
    mmgr_line_put(&command, args->check->program);
    mmgr_line_put(&command, " --refresh-dependencies dependencies");
    EMBED_CALL(cli_builds_found.build_run_fix, CliBuildsFoundCfg, .found = args->check->found, .finding = finding,
               .title = &title, .folder = args->check->folder,
               .folder_size = cellul.len(&(CatenaFinitaCfg){.src = args->check->folder, .cap = CLI_CELLS_POOL}),
               .line_text = line, .line_size = command.at);
}

// the parts of `size` bytes of `text` split at `:`, at most `most` counted: their count, 0 where one is blank, and the
// third's bounds into `third`
EMBED_INLINE unsigned int check_parts(const GradleCheckCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long *third = args->third;
    unsigned long long *third_size = args->third_size;
    unsigned int parts = 0u;
    unsigned long long start = 0ULL;
    *third = 0ULL;
    *third_size = 0ULL;
    for (unsigned long long at = 0ULL; at <= size; at += 1u)
    {
        if ((at < size) && (text[at] != ':'))
        {
            continue;
        }
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + start, .size = at - start,
                   .from_u64s = &from, .to_u64s = &to);
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
EMBED_INLINE void check_notation(const GradleCheckCtx *args)
{
    const char *const text = g_cli_gradle.text;
    const unsigned char *const class_of = g_cli_gradle.class_of;
    const char *const newline =
        (args->after < g_cli_gradle.size)
            ? memor.chr(&(MemoriaCfg){
                  .src = text + args->after, .val = '\n', .bytes = (size_t)(g_cli_gradle.size - args->after)})
            : NULL;
    const unsigned long long line_end = (newline != NULL) ? (unsigned long long)(newline - text) : g_cli_gradle.size;
    unsigned long long at = args->after;
    static const char *const s_wrappers[3] = {"platform", "enforcedPlatform", "testFixtures"};
    at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = at, .size = line_end,
                    .set = " \t(");
    for (unsigned int wrapper = 0u; wrapper < 3u; wrapper += 1u)
    {
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text + at, .size = line_end - at,
                       .prefix = s_wrappers[wrapper]))
        {
            at += cellul.len(&(CatenaFinitaCfg){.src = s_wrappers[wrapper], .cap = CLI_CELLS_POOL});
            at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = at,
                            .size = line_end, .set = " \t(");
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
    const char *const supplied = text + inner_from;
    const unsigned long long input_size = inner_to - inner_from;
    if (memor.chr(&(MemoriaCfg){.src = supplied, .val = '$', .bytes = (size_t)input_size}) != NULL)
    {
        return;
    }
    const char *const at_sign = memor.chr(&(MemoriaCfg){.src = supplied, .val = '@', .bytes = (size_t)input_size});
    const unsigned long long bare = (at_sign != NULL) ? (unsigned long long)(at_sign - supplied) : input_size;
    unsigned long long third = 0ULL;
    unsigned long long third_size = 0ULL;
    const unsigned int parts = EMBED_CALL(check_parts, GradleCheckCtx, .text = supplied, .size = bare, .third = &third,
                                          .third_size = &third_size);
    if ((parts < 2u) || (parts > 4u))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_ch(&msg, '`');
        mmgr_line_put_n(&msg, supplied, input_size);
        mmgr_line_put(&msg, "` is no dependency: a dependency is written `group:name:version`");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = inner_from, .to = inner_to,
                         .severity = CLI_BUILD_WARNING, .code = "gradle-notation");
    }
    else if ((parts >= 3u) && EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg,
                                         .version = supplied + third, .size = third_size))
    {
        EMBED_CALL(check_snapshot, GradleCheckCtx, .check = args->check, .from = inner_from, .to = inner_to,
                   .supplied = supplied, .size = input_size);
    }
}

// a statement of a `dependencies` block checked by the configuration it names
static void check_dependency(const CliGradleStatementCfg *args)
{
    unsigned long long word = args->word;
    CliGradleCheck *const check = args->context;
    const char *const text = g_cli_gradle.text;
    const char *const name = text + word;
    const unsigned long long lead =
        args->rest + EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text + args->rest,
                                .size = args->remainder_end - args->rest);
    const char next = (lead < args->remainder_end) ? text[lead] : '\0';
    unsigned int removed = 5u;
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        removed =
            ((cellul.len(&(CatenaFinitaCfg){.src = s_removed[at][0], .cap = CLI_CELLS_POOL}) == args->word_size) &&
             (memor.cmp(&(MemoriaCfg){.src = s_removed[at][0], .other = name, .bytes = (size_t)args->word_size}) == 0))
                ? at
                : removed;
    }
    if (((next == '=') || (next == '.') || (next == '\0')) && (removed == 5u))
    {
        return;
    }
    if (removed < 5u)
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&msg, "Gradle 7 and later have no configuration `");
        mmgr_line_put_n(&msg, name, args->word_size);
        mmgr_line_put(&msg, "`: `");
        mmgr_line_put(&msg, s_removed[removed][1]);
        mmgr_line_put(&msg, "` stands in its place");
        const unsigned int finding =
            EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                       .lines = check->lines, .from = word, .to = word + args->word_size, .severity = CLI_BUILD_WARNING,
                       .code = "gradle-configuration");
        VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&title, "Change to ");
        mmgr_line_put(&title, s_removed[removed][1]);
        EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = check->found, .finding = finding,
                   .title = &title, .lines = check->lines, .from = word, .to = word + args->word_size,
                   .text = s_removed[removed][1],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = s_removed[removed][1], .cap = CLI_CELLS_POOL}));
        return;
    }
    if (next == '{')
    {
        return;
    }
    if (!EMBED_CALL(cli_gradle_size_word_internal.known_configuration, CliGradleSizeWordCtx, .word = name,
                    .size = args->word_size))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&msg, "No configuration `");
        mmgr_line_put_n(&msg, name, args->word_size);
        mmgr_line_put(&msg, "` is declared here, by Gradle or the script");
        const unsigned int finding =
            EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                       .lines = check->lines, .from = word, .to = word + args->word_size, .severity = CLI_BUILD_WARNING,
                       .code = "gradle-configuration");
        CliBuildAdjacent adjacent;
        EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent, .name = name,
                   .size = args->word_size);
        for (unsigned int at = 0u; at < 30u; at += 1u)
        {
            EMBED_CALL(
                cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                .one = g_cli_gradle_configurations[at],
                .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_gradle_configurations[at], .cap = CLI_CELLS_POOL}));
        }
        for (unsigned int at = 0u; at < g_cli_gradle.declares; at += 1u)
        {
            EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                       .one = g_cli_gradle.declared[at], .size = g_cli_gradle.declared_size[at]);
        }
        for (unsigned int at = 0u; at < adjacent.count; at += 1u)
        {
            VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
            mmgr_line_put(&title, "Change to ");
            mmgr_line_put_n(&title, adjacent.adjacent[at], adjacent.adjacent_size[at]);
            EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = check->found, .finding = finding,
                       .title = &title, .lines = check->lines, .from = word, .to = word + args->word_size,
                       .text = adjacent.adjacent[at], .size = adjacent.adjacent_size[at]);
        }
        return;
    }
    EMBED_CALL(check_notation, GradleCheckCtx, .check = check, .after = word + args->word_size);
}

void cli_gradle_check(const CliGradleTextSizeCtx *args)
{
    const char *root = args->root;
    const CliBuildLines *lines = args->lines;
    CliBuildFindings *found = args->found;
    static CliGradleCheck s_check;
    static CliGradleCatalogs s_catalogs;
    if (!cli_gradle_text_size_internal.read(args))
    {
        return;
    }
    s_check.lines = lines;
    s_check.found = found;
    for (unsigned int at = 0u; at < g_cli_gradle.strays; at += 1u)
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
        mmgr_line_put(&msg, "This `}` closes no block");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = found, .message = &msg,
                         .lines = lines, .from = g_cli_gradle.stray[at], .to = g_cli_gradle.stray[at] + 1u,
                         .severity = CLI_BUILD_ERROR, .code = "gradle-brace");
    }
    for (unsigned int at = 0u; at < g_cli_gradle.blocks; at += 1u)
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[at];
        if (block->close != CLI_GRADLE_NONE)
        {
            continue;
        }
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
        mmgr_line_put(&msg, "The block");
        if (block->name_size > 0u)
        {
            mmgr_line_put(&msg, " `");
            mmgr_line_put_n(&msg, args->text + block->name, block->name_size);
            mmgr_line_ch(&msg, '`');
        }
        mmgr_line_put(&msg, " this `{` opens is not closed");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = found, .message = &msg,
                         .lines = lines, .from = block->open, .to = block->open + 1u, .severity = CLI_BUILD_ERROR,
                         .code = "gradle-brace");
    }
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_gradle_root_internal.build_root, CliGradleRootCtx, .root = root, .path = args->path, .out = folder);
    EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = root, .folder = folder,
               .out_capacity = s_check.folder);
    EMBED_CALL(cli_builds_internal.gradle_program, CliBuildsCtx, .folder = folder, .out = s_check.program);
    EMBED_CALL(cli_gradle_lines_internal.declare, CliGradleLinesCtx, .lines = lines);
    for (unsigned int at = 0u; at < g_cli_gradle.blocks; at += 1u)
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[at];
        if (EMBED_CALL(cli_gradle_internal.block_by_name, CliGradleCtx, .block = block, .name = "dependencies"))
        {
            EMBED_CALL(cli_gradle_lines_internal.statements, CliGradleLinesCtx, .lines = lines, .block_u32 = at,
                       .end = (block->close != CLI_GRADLE_NONE) ? block->close : args->size, .each = check_dependency,
                       .context = &s_check);
        }
    }
    EMBED_CALL(cli_gradle_folder_internal.catalogs, CliGradleFolderCtx, .folder = folder, .catalogs = &s_catalogs);
    for (unsigned int at = 0u; at < s_catalogs.count; at += 1u)
    {
        // each catalog read where it lies, mapped while its aliases are read and let go after; a catalog whose
        // aliases the cellblock could not meet is not checked against
        unsigned long long read = 0ULL;
        const unsigned char *const catalog =
            EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = s_catalogs.path[at], .size_pointer = &read,
                       .owner = &g_cli_gradle_cells);
        CliVersionsAliases aliases;
        const int whole = (catalog != NULL) && (read <= CLI_GRADLE_CATALOG_TEXT) &&
                          EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)catalog,
                                     .size = read) &&
                          EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases,
                                     CliVersionCatalogTextSizeAliasesCtx, .text = (const char *)catalog, .size = read,
                                     .aliases = &aliases);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = catalog,
                   .owner = &g_cli_gradle_cells);
        if (!whole)
        {
            continue;
        }
        char displayed[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = root, .folder = s_catalogs.path[at],
                   .out_capacity = displayed);
        EMBED_CALL(cli_version_catalog_lines_internal.gradle_references, CliVersionCatalogLinesCtx, .lines = lines,
                   .found = found, .name = s_catalogs.name[at], .displayed = displayed, .aliases_read = &aliases);
        EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases_free,
                   CliVersionCatalogTextSizeAliasesCtx, .aliases = &aliases);
    }
    cli_gradle_release();
}
