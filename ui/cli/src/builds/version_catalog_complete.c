// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_complete.c: what completes the word before the cursor in a version catalog: a section's name after
// `[`; in a string, the versions' aliases after `version.ref =` or `ref =`, and in a bundle the libraries'; and a
// table's keys after its `{` or a `,`, by the section the cursor's line stands under
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "version_catalog_internal.h"

// the sections a catalog contains, and the keys of a library's, a plugin's and a version's tables
static const char *const s_sections[5] = {"versions", "libraries", "bundles", "plugins", "metadata"};
static const char *const s_library_keys[5] = {"module", "group", "name", "version", "version.ref"};
static const char *const s_plugin_keys[3] = {"id", "version", "version.ref"};
static const char *const s_version_keys[5] = {"strictly", "require", "prefer", "reject", "rejectAll"};

// whether a letter is of a section's name, of an alias, and of a table's key
static int complete_alphanumeric(const CliEachLetterCfg *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = letter);
}

static int complete_alias_letter(const CliEachLetterCfg *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = letter) || (letter == '_') ||
           (letter == '.') || (letter == '-');
}

static int complete_key_letter(const CliEachLetterCfg *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = letter) || (letter == '.');
}

typedef struct
{
    const char *text;
    unsigned long long line_start;
    unsigned long long *size;
    unsigned long long size_value;
    const char *chosen;
    const char *token;
    unsigned long long token_size;
    CliCompletions *items;
} VersionCatalogCompleteCtx;
// the section the line starting at `line_start` stands under, the last line above it that starts with `[`, into
// `name`, `*size` bytes
EMBED_INLINE const char *complete_section(const VersionCatalogCompleteCtx *args)
{
    const char *text = args->text;
    unsigned long long *size = args->size;
    unsigned long long end = args->line_start;
    while (end > 0u)
    {
        const unsigned long long line_end = (end > 0u) ? end - 1u : 0u;
        unsigned long long start = line_end;
        start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = start,
                           .set = "\n");
        if ((start < line_end) && (text[start] == '['))
        {
            const char *const close = memor.chr(
                &(MemoriaCfg){.src = text + start + 1u, .val = ']', .bytes = (size_t)(line_end - start - 1u)});
            unsigned long long stop = (close != NULL) ? (unsigned long long)(close - text) : line_end;
            stop = ((stop > start + 1u) && (close == NULL) && (text[stop - 1u] == '\r')) ? stop - 1u : stop;
            unsigned long long from = 0ULL;
            unsigned long long to = 0ULL;
            EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + start + 1u,
                       .size = stop - start - 1u, .from_u64s = &from, .to_u64s = &to);
            *size = to - from;
            return text + start + 1u + from;
        }
        end = start;
    }
    *size = 0ULL;
    return "";
}

// the aliases of the catalog's section `chosen` put as items after `token`, each with what it stands for
EMBED_INLINE void complete_names(const VersionCatalogCompleteCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size_value;
    const char *token = args->token;
    unsigned long long token_size = args->token_size;
    CliCompletions *items = args->items;
    // a description names texts of the catalog's, with a few bytes between them and two a string of a list
    const int read = EMBED_CALL(cli_version_catalog_text_internal.versions_parse, CliVersionCatalogTextCtx,
                                .text = text, .size = size);
    const unsigned long long capacity = g_cli_versions.used + 16ULL + (2ULL * g_cli_versions.items);
    char *const about_text = read ? g_cli_cells->temporary_buf_alloc(capacity, &g_cli_versions_cells) : NULL;
    for (unsigned int at = 0u; (about_text != NULL) && (at < g_cli_versions.sections); at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        if (!cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_version_catalog_internal.versions_text,
                                                            CliVersionCatalogCtx, .at = section->name_at),
                                          .other = args->chosen,
                                          .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        for (unsigned int entry = section->first; entry != CLI_VERSIONS_NONE; entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            VerbaLinea about = {about_text, capacity, 0ULL};
            EMBED_CALL(cli_version_catalog_field_out_internal.versions_describe, CliVersionCatalogFieldOutCtx,
                       .field = field, .out = &about);
            EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                       .token_size = token_size,
                       .value = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                           .at = field->key_at),
                       .value_size = field->key_size, .kind = "constant", .detail = about_text,
                       .detail_size = about.at);
        }
    }
    g_cli_cells->persistent_buf_release(about_text, &g_cli_versions_cells);
    cli_versions_release();
}

void cli_versions_complete(const CliVersionCatalogTextSizeAtCtx *args)
{
    CliCompletions *items = args->items;
    unsigned long long line_start = args->at_u64;
    line_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->text,
                            .at = line_start, .set = "\n");
    const char *const before = args->text + line_start;
    const unsigned long long before_size = args->at_u64 - line_start;
    unsigned long long section_size = 0ULL;
    const char *const section = EMBED_CALL(complete_section, VersionCatalogCompleteCtx, .text = args->text,
                                           .line_start = line_start, .size = &section_size);
    const unsigned long long lead =
        EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = before, .size = before_size);
    if ((lead < before_size) && (before[lead] == '[') &&
        (memor.chr(&(MemoriaCfg){.src = before, .val = ']', .bytes = (size_t)before_size}) == NULL))
    {
        const unsigned long long start =
            EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = args->text, .at = args->at_u64,
                       .of = complete_alphanumeric);
        for (unsigned int one = 0u; one < 5u; one += 1u)
        {
            EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = args->text + start,
                       .token_size = args->at_u64 - start, .value = s_sections[one],
                       .value_size = cellul.len(&(CatenaFinitaCfg){.src = s_sections[one], .cap = CLI_CELLS_POOL}),
                       .kind = "module", .detail = "section", .detail_size = 7u);
        }
        return;
    }
    unsigned int quotes = 0u;
    for (unsigned long long one = 0ULL; one < before_size; one += 1u)
    {
        quotes += ((before[one] == '"') || (before[one] == '\'')) ? 1u : 0u;
    }
    const unsigned long long token = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg,
                                                .text = args->text, .at = args->at_u64, .of = complete_alias_letter);
    if ((quotes % 2u) == 1u)
    {
        unsigned long long opened = token - line_start;
        opened = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = before, .at = opened,
                            .set = "\"'");
        opened = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = before, .size = opened);
        if ((opened > 0u) && (before[opened - 1u] == '='))
        {
            unsigned long long named = opened;
            named = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = before, .at = named,
                               .set = "=");
            named = EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = before, .size = named);
            if (EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = before, .size = named,
                           .suffix = "ref"))
            {
                EMBED_CALL(complete_names, VersionCatalogCompleteCtx, .text = args->text, .size_value = args->size,
                           .chosen = "versions", .token = args->text + token, .token_size = args->at_u64 - token,
                           .items = items);
                return;
            }
        }
        if ((section_size == 7u) && (memor.cmp(&(MemoriaCfg){.src = section, .other = "bundles", .bytes = 7u}) == 0))
        {
            EMBED_CALL(complete_names, VersionCatalogCompleteCtx, .text = args->text, .size_value = args->size,
                       .chosen = "libraries", .token = args->text + token, .token_size = args->at_u64 - token,
                       .items = items);
        }
        return;
    }
    const char *open = NULL;
    for (unsigned long long one = before_size; one > 0u; one -= 1u)
    {
        if (before[one - 1u] == '{')
        {
            open = before + one - 1u;
            break;
        }
    }
    if ((open == NULL) ||
        (memor.chr(&(MemoriaCfg){.src = open, .val = '}', .bytes = (size_t)(before + before_size - open)}) != NULL))
    {
        return;
    }
    char last = '\0';
    {
        unsigned long long one = before_size;
        while ((last == '\0') && (one > 0u))
        {
            last = ((before[one - 1u] == '{') || (before[one - 1u] == ',') || (before[one - 1u] == '='))
                       ? before[one - 1u]
                       : '\0';
            one -= 1u;
        }
    }
    if ((last != '{') && (last != ','))
    {
        return;
    }
    const char *const *keys = NULL;
    unsigned int count = 0u;
    if ((section_size == 9u) && (memor.cmp(&(MemoriaCfg){.src = section, .other = "libraries", .bytes = 9u}) == 0))
    {
        keys = s_library_keys;
        count = 5u;
    }
    else if ((section_size == 7u) && (memor.cmp(&(MemoriaCfg){.src = section, .other = "plugins", .bytes = 7u}) == 0))
    {
        keys = s_plugin_keys;
        count = 3u;
    }
    else if ((section_size == 8u) && (memor.cmp(&(MemoriaCfg){.src = section, .other = "versions", .bytes = 8u}) == 0))
    {
        keys = s_version_keys;
        count = 5u;
    }
    static char s_detail[1u << 12u];
    VerbaLinea detail = {s_detail, sizeof(s_detail), 0ULL};
    mmgr_line_put(&detail, "key of [");
    mmgr_line_put_n(&detail, section, section_size);
    mmgr_line_ch(&detail, ']');
    const unsigned long long start = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg,
                                                .text = args->text, .at = args->at_u64, .of = complete_key_letter);
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = args->text + start,
                   .token_size = args->at_u64 - start, .value = keys[one],
                   .value_size = cellul.len(&(CatenaFinitaCfg){.src = keys[one], .cap = CLI_CELLS_POOL}),
                   .kind = "field", .detail = s_detail, .detail_size = detail.at);
    }
}
