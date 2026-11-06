// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds.c: a build file's kind by its name, and its check, its completion, the names SCons gives it and the line that
// updates every snapshot dependency of its build, each handed to the reader of its kind. One check runs at a time, its
// findings stored in their own capacity
#include "../text/text.h"
#include "gradle_internal.h"
#include "maven_internal.h"
#include "scons_internal.h"
#include "version_catalog_internal.h"

static CliBuildFindings s_found;

unsigned int cli_build_kind_get(const CliBuildsPathCfg *args)
{
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    char lower[CLI_NAME_MAX_LEN];
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
    if (length >= sizeof(lower))
    {
        return 0u;
    }
    for (size_t at = 0u; at <= length; at += 1u)
    {
        lower[at] = ((name[at] >= 'A') && (name[at] <= 'Z')) ? (char)(name[at] - 'A' + 'a') : name[at];
    }
    if (EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = lower, .size = length,
                   .suffix = ".gradle") ||
        EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = lower, .size = length,
                   .suffix = ".gradle.kts"))
    {
        return CLI_BUILD_GRADLE;
    }
    if (EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = lower, .size = length,
                   .suffix = ".versions.toml"))
    {
        return CLI_BUILD_CATALOG;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "pom.xml", .cap = CLI_POOL_BYTES}))
    {
        return CLI_BUILD_MAVEN;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "sconstruct", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "sconscript", .cap = CLI_POOL_BYTES})))
    {
        return CLI_BUILD_SCONS;
    }
    return 0u;
}

void cli_build_check(const CliBuildsPathCfg *args)
{
    EMBED_CALL(cli_builds_lines.build_findings_begin, CliBuildsLinesCfg, .found = &s_found);
    CliBuildLines lines;
    unsigned int *const starts = EMBED_CALL(cli_builds_text_size.build_lines_alloc, CliBuildsTextSizeCfg,
                                            .text = args->text, .size = args->size, .lines_out = &lines);
    const unsigned int kind = cli_builds_path.build_kind_get(args);
    switch (kind)
    {
    case CLI_BUILD_GRADLE: {
        EMBED_CALL(cli_gradle_text_size_internal.check, CliGradleTextSizeCtx, .root = args->root, .path = args->path,
                   .text = args->text, .size = args->size, .lines = &lines, .found = &s_found);
        break;
    }
    case CLI_BUILD_CATALOG: {
        EMBED_CALL(cli_version_catalog_text_internal.versions_check, CliVersionCatalogTextCtx, .root = args->root,
                   .path = args->path, .text = args->text, .size = args->size, .lines = &lines, .found = &s_found);
        break;
    }
    case CLI_BUILD_MAVEN: {
        EMBED_CALL(cli_maven_text_internal.check, CliMavenTextCtx, .root = args->root, .path = args->path,
                   .text = args->text, .size = args->size, .lines = &lines, .found = &s_found);
        break;
    }
    case CLI_BUILD_SCONS: {
        EMBED_CALL(cli_scons_text_internal.check, CliSconsTextCtx, .root = args->root, .path = args->path,
                   .text = args->text, .size = args->size, .lines = &lines, .found = &s_found);
        break;
    }
    default: {
        break;
    }
    }
    EMBED_CALL(cli_builds_table.build_findings_write, CliBuildsCfg, .found_read = &s_found, .out = args->out);
    g_cli_guard->persistent_buf_release(s_found.finding, g_cli_build_owner);
    *(&s_found) = (CliBuildFindings){NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, 0};
    g_cli_guard->persistent_buf_release(starts, g_cli_build_owner);
}

void cli_build_complete(const CliBuildsPathCfg *args)
{
    const unsigned long long at = cli_builds_path.build_byte_at(args);
    const unsigned int kind = cli_builds_path.build_kind_get(args);
    switch (kind)
    {
    case CLI_BUILD_GRADLE: {
        EMBED_CALL(cli_gradle_text_internal.complete, CliGradleTextCtx, .root = args->root, .path = args->path,
                   .text = args->text, .size = args->size, .at = at, .items = args->items);
        break;
    }
    case CLI_BUILD_CATALOG: {
        EMBED_CALL(cli_version_catalog_text_size_at_internal.versions_complete, CliVersionCatalogTextSizeAtCtx,
                   .text = args->text, .size = args->size, .at_u64 = at, .items = args->items);
        break;
    }
    case CLI_BUILD_MAVEN: {
        EMBED_CALL(cli_maven_text_size_internal.complete, CliMavenTextSizeCtx, .path = args->path, .text = args->text,
                   .size = args->size, .at_u64 = at, .items = args->items);
        break;
    }
    case CLI_BUILD_SCONS: {
        EMBED_CALL(cli_scons_text_size_internal.complete, CliSconsTextSizeCtx, .text = args->text, .size = args->size,
                   .at = at, .items = args->items);
        break;
    }
    default: {
        break;
    }
    }
    EMBED_CALL(cli_edits_items.completions_unique, CliEditsItemsCfg, .items = args->items);
}

void cli_build_input_names(const CliBuildsPathTextCfg *args)
{
    const char *path = args->path;
    if (EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = path) == CLI_BUILD_SCONS)
    {
        EMBED_CALL(cli_scons_text_size_context_internal.free, CliSconsTextSizeContextCtx, .text = args->text,
                   .size = args->size, .each_callback = args->each, .context = args->context);
    }
}

typedef struct
{
    const char *from;
    char *out;
} BuildsCtx;
// `from` copied into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void builds_copy(const BuildsCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->from);
    (void)mmgr_line_finish(&line);
}

int cli_build_snapshots_line(const CliBuildsPathRootCfg *args)
{
    static const char *const s_adjacent[3] = {"pom.xml", "build.gradle", "build.gradle.kts"};
    static const char *const s_top[5] = {"pom.xml", "build.gradle", "build.gradle.kts", "settings.gradle",
                                         "settings.gradle.kts"};
    char positions[9][CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    if (args->path != NULL)
    {
        EMBED_CALL(builds_copy, BuildsCtx, .from = args->path, .out = positions[count]);
        count += 1u;
        char above[CLI_PATH_MAX_LEN];
        for (unsigned int at = 0u;
             EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->path, .out = above) &&
             (at < 3u);
             at += 1u)
        {
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = above, .relative = s_adjacent[at],
                       .joined = positions[count]);
            count += 1u;
        }
    }
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = s_top[at],
                   .joined = positions[count]);
        count += 1u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int kind = EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = positions[at]);
        if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = positions[at]))
        {
            continue;
        }
        char build[CLI_PATH_MAX_LEN];
        char program[CLI_PATH_MAX_LEN];
        VerbaLinea command = {args->line_capacity, CLI_PATH_MAX_LEN, 0ULL};
        if (kind == CLI_BUILD_MAVEN)
        {
            if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = positions[at],
                            .out = build))
            {
                EMBED_CALL(builds_copy, BuildsCtx, .from = args->root, .out = build);
            }
            EMBED_CALL(cli_maven_out_internal.program, CliMavenOutCtx, .root = args->root, .folder = build,
                       .out_capacity = program);
            mmgr_line_put(&command, program);
            mmgr_line_put(&command, " -U dependency:resolve");
        }
        else if ((kind == CLI_BUILD_GRADLE) || (kind == CLI_BUILD_CATALOG))
        {
            EMBED_CALL(cli_gradle_root_internal.build_root, CliGradleRootCtx, .root = args->root, .path = positions[at],
                       .out = build);
            EMBED_CALL(cli_builds_internal.gradle_program, CliBuildsCtx, .folder = build, .out = program);
            mmgr_line_put(&command, program);
            mmgr_line_put(&command, " --refresh-dependencies dependencies");
        }
        else
        {
            continue;
        }
        (void)mmgr_line_finish(&command);
        EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = args->root, .folder = build,
                   .out_capacity = args->folder_out);
        return 1;
    }
    mmgr_line_put(args->err_msg, "No Maven or Gradle build here: a pom.xml, a build.gradle or a build.gradle.kts");
    return 0;
}
