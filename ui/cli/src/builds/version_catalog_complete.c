// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_complete.c: what completes the word before the cursor in a version catalog: a section's name after
// `[`; in a string, the versions' aliases after `version.ref =` or `ref =`, and in a bundle the libraries'; and a
// table's keys after its `{` or a `,`, by the section the cursor's line stands under
#include "version_catalog_internal.h"

// the sections a catalog holds, and the keys of a library's, a plugin's and a version's tables
static const char *const s_sections[5] = {"versions", "libraries", "bundles", "plugins", "metadata"};
static const char *const s_library_keys[5] = {"module", "group", "name", "version", "version.ref"};
static const char *const s_plugin_keys[3] = {"id", "version", "version.ref"};
static const char *const s_version_keys[5] = {"strictly", "require", "prefer", "reject", "rejectAll"};

// whether a letter is of a section's name, of an alias, and of a table's key
static int complete_alphanumeric(unsigned int letter)
{
    return cli_text_alphanumeric(letter);
}

static int complete_alias_letter(unsigned int letter)
{
    return cli_text_alphanumeric(letter) || (letter == '_') || (letter == '.') || (letter == '-');
}

static int complete_key_letter(unsigned int letter)
{
    return cli_text_alphanumeric(letter) || (letter == '.');
}

// the section the line starting at `line_start` stands under, the last line above it that starts with `[`, into
// `name`, `*size` bytes
static const char *complete_section(const char *text, unsigned long long line_start, unsigned long long *size)
{
    unsigned long long end = line_start;
    while (end > 0u)
    {
        const unsigned long long line_end = (end > 0u) ? end - 1u : 0u;
        unsigned long long start = line_end;
        while ((start > 0u) && (text[start - 1u] != '\n'))
        {
            start -= 1u;
        }
        if ((start < line_end) && (text[start] == '['))
        {
            const char *const close = memor.chr(
                &(MemoriaCfg){.src = text + start + 1u, .val = ']', .bytes = (size_t)(line_end - start - 1u)});
            unsigned long long stop = (close != NULL) ? (unsigned long long)(close - text) : line_end;
            stop = ((stop > start + 1u) && (close == NULL) && (text[stop - 1u] == '\r')) ? stop - 1u : stop;
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            cli_text_trimmed(text + start + 1u, stop - start - 1u, &from, &to);
            *size = to - from;
            return text + start + 1u + from;
        }
        end = start;
    }
    *size = 0ull;
    return "";
}

// the aliases of the catalog's section `chosen` put as items after `token`, each with what it stands for
static void complete_names(const char *text, unsigned long long size, const char *chosen, const char *token,
                           unsigned long long token_size, CliCompletions *items)
{
    cli_lock_take(&g_cli_versions_lock);
    // a description names texts of the catalog's, with a few bytes between them and two a string of a list
    const int read = cli_versions_parse(text, size);
    const unsigned long long room = g_cli_versions.used + 16ull + (2ull * g_cli_versions.items);
    char *const about_text = read ? cli_cell_taken_by(&g_cli_versions_cells, room, "a catalog's description") : NULL;
    for (unsigned int at = 0u; (about_text != NULL) && (at < g_cli_versions.sections); at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        if (!cellul.eq(
                &(CatenaFinitaCfg){.src = cli_versions_text(section->name_at), .other = chosen, .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        for (unsigned int entry = section->first; entry != CLI_VERSIONS_NONE; entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            CliLine about = {about_text, room, 0ull};
            cli_versions_describe(field, &about);
            cli_build_relative(items, token, token_size, cli_versions_text(field->key_at), field->key_size, "constant",
                               about_text, about.at);
        }
    }
    cli_cell_given_by(&g_cli_versions_cells, about_text);
    cli_versions_released();
    cli_lock_give(&g_cli_versions_lock);
}

void cli_versions_complete(const char *text, unsigned long long size, unsigned long long at, CliCompletions *items)
{
    unsigned long long line_start = at;
    while ((line_start > 0u) && (text[line_start - 1u] != '\n'))
    {
        line_start -= 1u;
    }
    const char *const before = text + line_start;
    const unsigned long long before_size = at - line_start;
    unsigned long long section_size = 0ull;
    const char *const section = complete_section(text, line_start, &section_size);
    const unsigned long long lead = cli_text_lead(before, before_size);
    if ((lead < before_size) && (before[lead] == '[') &&
        (memor.chr(&(MemoriaCfg){.src = before, .val = ']', .bytes = (size_t)before_size}) == NULL))
    {
        const unsigned long long start = cli_build_run_before(text, at, complete_alphanumeric);
        for (unsigned int one = 0u; one < 5u; one += 1u)
        {
            cli_build_relative(items, text + start, at - start, s_sections[one],
                               cellul.len(&(CatenaFinitaCfg){.src = s_sections[one], .cap = CLI_CELLS_POOL}), "module",
                               "section", 7u);
        }
        return;
    }
    unsigned int quotes = 0u;
    for (unsigned long long one = 0ull; one < before_size; one += 1u)
    {
        quotes += ((before[one] == '"') || (before[one] == '\'')) ? 1u : 0u;
    }
    const unsigned long long token = cli_build_run_before(text, at, complete_alias_letter);
    if ((quotes % 2u) == 1u)
    {
        unsigned long long opened = token - line_start;
        while ((opened > 0u) && ((before[opened - 1u] == '"') || (before[opened - 1u] == '\'')))
        {
            opened -= 1u;
        }
        opened = cli_text_trimmed_end(before, opened);
        if ((opened > 0u) && (before[opened - 1u] == '='))
        {
            unsigned long long named = opened;
            while ((named > 0u) && (before[named - 1u] == '='))
            {
                named -= 1u;
            }
            named = cli_text_trimmed_end(before, named);
            if (cli_text_ends(before, named, "ref"))
            {
                complete_names(text, size, "versions", text + token, at - token, items);
                return;
            }
        }
        if ((section_size == 7u) && (memor.cmp(&(MemoriaCfg){.src = section, .other = "bundles", .bytes = 7u}) == 0))
        {
            complete_names(text, size, "libraries", text + token, at - token, items);
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
    for (unsigned long long one = before_size; (last == '\0') && (one > 0u); one -= 1u)
    {
        last = ((before[one - 1u] == '{') || (before[one - 1u] == ',') || (before[one - 1u] == '=')) ? before[one - 1u]
                                                                                                     : '\0';
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
    CliLine detail = {s_detail, sizeof(s_detail), 0ull};
    cli_line_text(&detail, "key of [");
    cli_line_counted(&detail, section, section_size);
    cli_line_character(&detail, ']');
    const unsigned long long start = cli_build_run_before(text, at, complete_key_letter);
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        cli_build_relative(items, text + start, at - start, keys[one],
                           cellul.len(&(CatenaFinitaCfg){.src = keys[one], .cap = CLI_CELLS_POOL}), "field", s_detail,
                           detail.at);
    }
}
