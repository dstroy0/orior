// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_aliases.c: what a catalog read says: a field of a table by its key, what a value says in a few words,
// the accessor a script names an alias by, and the aliases of each section with what each stands for. An alias past
// what the aliases hold is left out
#include "version_catalog_internal.h"

const char *cli_versions_text(unsigned int at)
{
    return (at < g_cli_versions.used) ? g_cli_versions.pool + at : "";
}

unsigned int cli_versions_get(unsigned int first, const char *key)
{
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    for (unsigned int at = first; at != CLI_VERSIONS_NONE; at = g_cli_versions.field[at].next)
    {
        const CliVersionsField *const field = &g_cli_versions.field[at];
        if ((field->key_size == size) &&
            (memor.cmp(&(MemoriaCfg){.src = cli_versions_text(field->key_at), .other = key, .bytes = size}) == 0))
        {
            return at;
        }
    }
    return CLI_VERSIONS_NONE;
}

// the string a table's field keyed `key` holds, NULL where it holds none
static const CliVersionsField *aliases_text_of(const CliVersionsField *table, const char *key)
{
    const unsigned int at = cli_versions_get(table->first, key);
    if ((at == CLI_VERSIONS_NONE) || (g_cli_versions.field[at].shape != CLI_VERSIONS_TEXT))
    {
        return NULL;
    }
    return &g_cli_versions.field[at];
}

// a field's string written
static void aliases_put(CliLine *out, const CliVersionsField *field)
{
    cli_line_counted(out, cli_versions_text(field->text_at), field->text_size);
}

void cli_versions_describe(const CliVersionsField *field, CliLine *out)
{
    if ((field->shape == CLI_VERSIONS_TEXT) || (field->shape == CLI_VERSIONS_BARE))
    {
        aliases_put(out, field);
        return;
    }
    if (field->shape == CLI_VERSIONS_LIST)
    {
        for (unsigned int at = 0u; at < field->count; at += 1u)
        {
            const CliVersionsItem *const item = &g_cli_versions.item[field->first + at];
            cli_line_text(out, (at > 0u) ? ", " : "");
            cli_line_counted(out, cli_versions_text(item->text_at), item->text_size);
        }
        return;
    }
    const CliVersionsField *const module = aliases_text_of(field, "module");
    const CliVersionsField *const group = aliases_text_of(field, "group");
    const CliVersionsField *const name = aliases_text_of(field, "name");
    const CliVersionsField *const id = aliases_text_of(field, "id");
    if (module != NULL)
    {
        aliases_put(out, module);
    }
    else if ((group != NULL) && (name != NULL))
    {
        aliases_put(out, group);
        cli_line_character(out, ':');
        aliases_put(out, name);
    }
    else if (id != NULL)
    {
        aliases_put(out, id);
    }
    const CliVersionsField *const version = aliases_text_of(field, "version");
    const CliVersionsField *const reference = aliases_text_of(field, "version.ref");
    if (version != NULL)
    {
        cli_line_character(out, ':');
        aliases_put(out, version);
    }
    else if (reference != NULL)
    {
        cli_line_text(out, ", version ");
        aliases_put(out, reference);
    }
}

void cli_versions_accessor(const char *alias, unsigned long long size, CliLine *out)
{
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        cli_line_character(out, ((alias[at] == '-') || (alias[at] == '_')) ? '.' : alias[at]);
    }
}

// `size` bytes of `bytes` kept in the aliases' texts: where they start, CLI_VERSIONS_ALIASES_TEXT where they do not fit
static unsigned int aliases_kept(CliVersionsAliases *aliases, const char *bytes, unsigned long long size)
{
    if (size + 1u > aliases->room - aliases->used)
    {
        return CLI_VERSIONS_ALIASES_TEXT;
    }
    const unsigned int at = aliases->used;
    memor.cpy(&(MemoriaCfg){.dst = aliases->text + at, .src = bytes, .bytes = (size_t)size});
    aliases->text[at + size] = '\0';
    aliases->used += (unsigned int)size + 1u;
    return at;
}

// the section of the aliases a section of the catalog read is, 4 where it is none of them
static unsigned int aliases_kind(const CliVersionsSection *section)
{
    static const char *const s_sections[4] = {"libraries", "versions", "plugins", "bundles"};
    unsigned int kind = 4u;
    for (unsigned int one = 0u; one < 4u; one += 1u)
    {
        kind = (cellul.eq(&(CatenaFinitaCfg){
                   .src = cli_versions_text(section->name_at), .other = s_sections[one], .cap = CLI_CELLS_POOL}))
                   ? one
                   : kind;
    }
    return kind;
}

// the aliases' cells taken: as many aliases as the four sections' entries; and texts of the catalog's texts, which
// hold each key and all that its description names, with each alias's two terminators and the few bytes its
// description puts between what it names, and two a string of a list
static int aliases_taken(CliVersionsAliases *aliases)
{
    unsigned long long count = 0ull;
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        count += (aliases_kind(section) < 4u) ? section->count : 0u;
    }
    count = (count < CLI_VERSIONS_ALIASES) ? count : CLI_VERSIONS_ALIASES;
    const unsigned long long room = g_cli_versions.used + (16ull * count) + (2ull * g_cli_versions.items);
    // the room is under CLI_VERSIONS_ALIASES_TEXT, so it fits an unsigned int
    aliases->room = (unsigned int)((room < CLI_VERSIONS_ALIASES_TEXT) ? room : CLI_VERSIONS_ALIASES_TEXT);
    aliases->alias =
        cli_cell_taken_by(&g_cli_versions_cells, (count + 1ull) * sizeof(CliVersionsAlias), "a catalog's aliases");
    aliases->text = cli_cell_taken_by(&g_cli_versions_cells, aliases->room + 1ull, "a catalog's aliases' texts");
    if ((aliases->alias == NULL) || (aliases->text == NULL))
    {
        cli_versions_aliases_given(aliases);
        return 0;
    }
    return 1;
}

void cli_versions_aliases_given(CliVersionsAliases *aliases)
{
    cli_cell_given_by(&g_cli_versions_cells, aliases->text);
    cli_cell_given_by(&g_cli_versions_cells, aliases->alias);
    aliases->alias = NULL;
    aliases->text = NULL;
    aliases->count = 0u;
    aliases->used = 0u;
    aliases->room = 0u;
}

int cli_versions_aliases(const char *text, unsigned long long size, CliVersionsAliases *aliases)
{
    aliases->alias = NULL;
    aliases->text = NULL;
    aliases->count = 0u;
    aliases->used = 0u;
    aliases->room = 0u;
    cli_lock_take(&g_cli_versions_lock);
    if (!cli_versions_parse(text, size) || !aliases_taken(aliases))
    {
        cli_versions_released();
        cli_lock_give(&g_cli_versions_lock);
        return 0;
    }
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        const unsigned int kind = aliases_kind(section);
        for (unsigned int entry = section->first; (kind < 4u) && (entry != CLI_VERSIONS_NONE);
             entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            const unsigned int alias = aliases_kept(aliases, cli_versions_text(field->key_at), field->key_size);
            // the description written where the texts end, and kept with its terminator where it fit
            CliLine about = {aliases->text + aliases->used, aliases->room - aliases->used, 0ull};
            cli_versions_describe(field, &about);
            if ((aliases->count < CLI_VERSIONS_ALIASES) && (alias != CLI_VERSIONS_ALIASES_TEXT) &&
                (about.capacity > 0u) && cli_line_fits(&about))
            {
                const unsigned int about_at = aliases->used;
                aliases->text[about_at + about.at] = '\0';
                aliases->used += (unsigned int)about.at + 1u;
                aliases->alias[aliases->count] =
                    (CliVersionsAlias){kind, alias, field->key_size, about_at, (unsigned int)about.at};
                aliases->count += 1u;
            }
        }
    }
    cli_versions_released();
    cli_lock_give(&g_cli_versions_lock);
    return 1;
}
