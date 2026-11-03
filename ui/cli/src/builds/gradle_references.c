// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_references.c: each name a catalog's accessor leads in a Gradle script read, checked against the catalog's
// aliases: the chain of words after the accessor, less the call it ends in, names an alias of the section its first
// word names, and a chain that names none is a finding with the aliases near it as its fixes
#include "../command_line/callback_cfg.h"
#include "gradle_internal.h"
#include "version_catalog_internal.h"

// the most words of one chain a catalog's accessor leads
#define CLI_GRADLE_SEGMENTS 64u

// the calls a chain of a catalog's accessor ends in that name no alias
static const char *const s_getters[9] = {"get",       "asProvider", "orNull", "getOrNull", "orElse",
                                         "isPresent", "present",    "map",    "flatMap"};

typedef struct
{
    unsigned long long at;
    unsigned long long start;
    unsigned long long *from;
    unsigned long long *to;
    const char *word_text;
    unsigned long long size;
    unsigned int *section;
    const char **kind;
    unsigned int *path_from;
    const CliBuildLines *lines;
    CliBuildFindings *found;
    const char *name;
    const char *displayed;
    const CliVersionsAliases *aliases;
    const unsigned long long *from_read;
    const unsigned long long *to_read;
    unsigned int count;
} GradleReferencesCtx;
// the run of word letters from `at`: its size
EMBED_INLINE unsigned long long references_word(const GradleReferencesCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long end = at;
    while (end < g_cli_gradle.size)
    {
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(cli_each_letter.build_word_letter, CliEachLetterCfg,
                        .letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = g_cli_gradle.text,
                                             .size = g_cli_gradle.size, .at = end, .length = &length)))
        {
            break;
        }
        end += length;
    }
    return end - at;
}

// the words of the chain a catalog's accessor leads from `start`, each one's span: their count
EMBED_INLINE unsigned int references_chain(const GradleReferencesCtx *args)
{
    unsigned long long start = args->start;
    const char *const text = g_cli_gradle.text;
    unsigned int count = 0u;
    for (;;)
    {
        const unsigned long long length = EMBED_CALL(references_word, GradleReferencesCtx, .at = start);
        if ((length == 0u) || (count == CLI_GRADLE_SEGMENTS))
        {
            break;
        }
        args->from[count] = start;
        args->to[count] = start + length;
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
        const unsigned long long end = args->to[count - 1u];
        const unsigned long long lead = end + EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                                         .text = text + end, .size = g_cli_gradle.size - end);
        count -= ((lead < g_cli_gradle.size) && (text[lead] == '(')) ? 1u : 0u;
    }
    for (int popped = 1; popped && (count > 0u);)
    {
        popped = 0;
        for (unsigned int at = 0u; at < 9u; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = s_getters[at], .cap = CLI_CELLS_POOL}) ==
                 args->to[count - 1u] - args->from[count - 1u]) &&
                (memor.cmp(&(MemoriaCfg){.src = s_getters[at],
                                         .other = text + args->from[count - 1u],
                                         .bytes = (size_t)(args->to[count - 1u] - args->from[count - 1u])}) == 0))
            {
                popped = 1;
            }
        }
        count -= popped ? 1u : 0u;
    }
    return count;
}

// the alias sections of a chain's first word: the section, the kind it names, and where the alias's path starts
EMBED_INLINE void references_section_get(const GradleReferencesCtx *args)
{
    unsigned int *section = args->section;
    const char **kind = args->kind;
    unsigned int *path_from = args->path_from;
    static const char *const s_names[3] = {"versions", "plugins", "bundles"};
    static const char *const s_kinds[3] = {"version", "plugin", "bundle"};
    static const unsigned int s_sections[3] = {CLI_VERSIONS_VERSIONS, CLI_VERSIONS_PLUGINS, CLI_VERSIONS_BUNDLES};
    *section = CLI_VERSIONS_LIBRARIES;
    *kind = "library";
    *path_from = 0u;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_names[at], .cap = CLI_CELLS_POOL}) == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_names[at], .other = args->word_text, .bytes = (size_t)args->size}) == 0))
        {
            *section = s_sections[at];
            *kind = s_kinds[at];
            *path_from = 1u;
        }
    }
}

// a chain of words of the catalog `name`, at `displayed` in the tree, checked against the aliases of the section its
// first word names
EMBED_INLINE void references_one(const GradleReferencesCtx *args)
{
    const CliBuildLines *lines = args->lines;
    CliBuildFindings *found = args->found;
    const char *name = args->name;
    const unsigned long long *from = args->from_read;
    const unsigned long long *to = args->to_read;
    const char *const text = g_cli_gradle.text;
    unsigned int section = 0u;
    const char *kind = NULL;
    unsigned int path_from = 0u;
    EMBED_CALL(references_section_get, GradleReferencesCtx, .word_text = text + from[0], .size = to[0] - from[0],
               .section = &section, .kind = &kind, .path_from = &path_from);
    if (path_from >= args->count)
    {
        return;
    }
    const char *const path = text + from[path_from];
    const unsigned long long path_size = to[args->count - 1u] - from[path_from];
    CliBuildAdjacent adjacent;
    EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent, .name = path,
               .size = path_size);
    static char s_known[1u << 12u];
    for (unsigned int at = 0u; at < args->aliases->count; at += 1u)
    {
        const CliVersionsAlias *const alias = &args->aliases->alias[at];
        if (alias->section != section)
        {
            continue;
        }
        VerbaLinea known = {s_known, sizeof(s_known), 0ULL};
        EMBED_CALL(cli_version_catalog_size_out_alias_internal.versions_accessor, CliVersionCatalogSizeOutAliasCtx,
                   .alias = args->aliases->text + alias->alias_at, .size = alias->alias_size, .out = &known);
        if ((known.at == path_size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_known, .other = path, .bytes = (size_t)path_size}) == 0))
        {
            return;
        }
    }
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put(&msg, name);
    mmgr_line_ch(&msg, '.');
    mmgr_line_put_n(&msg, text + from[0], to[args->count - 1u] - from[0]);
    mmgr_line_put(&msg, "` names no ");
    mmgr_line_put(&msg, kind);
    mmgr_line_put(&msg, " of `");
    mmgr_line_put(&msg, args->displayed);
    mmgr_line_ch(&msg, '`');
    const unsigned int finding = EMBED_CALL(
        cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = found, .message = &msg, .lines = lines,
        .from = from[path_from], .to = to[args->count - 1u], .severity = CLI_BUILD_ERROR, .code = "gradle-catalog");
    // each accessor is as long as its alias, so a cell of the aliases' texts contains those the nearest are chosen from
    const unsigned long long capacity = args->aliases->used + 1ULL;
    char *const accessors = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_gradle_cells);
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; (accessors != NULL) && (at < args->aliases->count); at += 1u)
    {
        const CliVersionsAlias *const alias = &args->aliases->alias[at];
        VerbaLinea known = {accessors + used, capacity - used, 0ULL};
        if (alias->section != section)
        {
            continue;
        }
        EMBED_CALL(cli_version_catalog_size_out_alias_internal.versions_accessor, CliVersionCatalogSizeOutAliasCtx,
                   .alias = args->aliases->text + alias->alias_at, .size = alias->alias_size, .out = &known);
        if (mmgr_line_ok(&known))
        {
            EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                       .one = accessors + used, .size = known.at);
            used += known.at;
        }
    }
    for (unsigned int at = 0u; at < adjacent.count; at += 1u)
    {
        VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
        mmgr_line_put(&title, "Change to ");
        mmgr_line_put(&title, name);
        mmgr_line_ch(&title, '.');
        if (path_from == 1u)
        {
            mmgr_line_put_n(&title, text + from[0], to[0] - from[0]);
            mmgr_line_ch(&title, '.');
        }
        mmgr_line_put_n(&title, adjacent.adjacent[at], adjacent.adjacent_size[at]);
        EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = found, .finding = finding,
                   .title = &title, .lines = lines, .from = from[path_from], .to = to[args->count - 1u],
                   .text = adjacent.adjacent[at], .size = adjacent.adjacent_size[at]);
    }
    g_cli_cells->persistent_buf_release(accessors, &g_cli_gradle_cells);
}

void cli_gradle_references(const CliVersionCatalogLinesCtx *args)
{
    const char *const text = g_cli_gradle.text;
    const unsigned long long size = g_cli_gradle.size;
    const unsigned long long name_size = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    unsigned long long at = 0ULL;
    while (at + name_size + 1u <= size)
    {
        if ((memor.cmp(&(MemoriaCfg){.src = text + at, .other = args->name, .bytes = (size_t)name_size}) != 0) ||
            (text[at + name_size] != '.') || (g_cli_gradle.class_of[at] != CLI_GRADLE_CODE))
        {
            at += 1u;
            continue;
        }
        unsigned long long length = 0ULL;
        const unsigned int before = (at > 0u) ? EMBED_CALL(cli_text_length.letter_before, CliTextLengthCfg,
                                                           .text = text, .end_value = at, .length = &length)
                                              : ' ';
        if (EMBED_CALL(cli_each_letter.build_word_letter, CliEachLetterCfg, .letter = before) || (before == '.'))
        {
            at += 1u;
            continue;
        }
        unsigned long long from[CLI_GRADLE_SEGMENTS];
        unsigned long long to[CLI_GRADLE_SEGMENTS];
        const unsigned int count =
            EMBED_CALL(references_chain, GradleReferencesCtx, .start = at + name_size + 1u, .from = from, .to = to);
        if (count > 0u)
        {
            EMBED_CALL(references_one, GradleReferencesCtx, .lines = args->lines, .found = args->found,
                       .name = args->name, .displayed = args->displayed, .aliases = args->aliases_read,
                       .from_read = from, .to_read = to, .count = count);
        }
        at += name_size;
        at += 1u;
    }
}
