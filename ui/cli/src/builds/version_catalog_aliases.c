// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_aliases.c: what a catalog read describes: a field of a table by its key, what a value describes in a
// few words, the accessor a script names an alias by, and the aliases of each section with what each stands for. An
// alias past what the aliases contain is left out
#include "version_catalog_internal.h"

const char *cli_versions_text(const CliVersionCatalogCtx *args)
{
    return (args->at < g_cli_versions.used) ? g_cli_versions.pool + args->at : "";
}

unsigned int cli_versions_get(const CliVersionCatalogFirstCtx *args)
{
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    for (unsigned int at = args->first; at != CLI_VERSIONS_NONE; at = g_cli_versions.field[at].next)
    {
        const CliVersionsField *const field = &g_cli_versions.field[at];
        if ((field->key_size == size) &&
            (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                                       .at = field->key_at),
                                     .other = args->key,
                                     .bytes = size}) == 0))
        {
            return at;
        }
    }
    return CLI_VERSIONS_NONE;
}

typedef struct
{
    const CliVersionsField *table;
    const char *key;
    VerbaLinea *out;
    const CliVersionsField *field;
    CliVersionsAliases *aliases;
    const char *bytes;
    unsigned long long size;
    const CliVersionsSection *section;
} VersionCatalogAliasesCtx;
// the string a table's field keyed `key` contains, NULL where it contains none
EMBED_INLINE const CliVersionsField *aliases_text_get(const VersionCatalogAliasesCtx *args)
{
    const char *key = args->key;
    const unsigned int at = EMBED_CALL(cli_version_catalog_first_internal.versions_get, CliVersionCatalogFirstCtx,
                                       .first = args->table->first, .key = key);
    if ((at == CLI_VERSIONS_NONE) || (g_cli_versions.field[at].shape != CLI_VERSIONS_TEXT))
    {
        return NULL;
    }
    return &g_cli_versions.field[at];
}

// a field's string written
EMBED_INLINE void aliases_put(const VersionCatalogAliasesCtx *args)
{
    mmgr_line_put_n(
        args->out,
        EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = args->field->text_at),
        args->field->text_size);
}

void cli_versions_describe(const CliVersionCatalogFieldOutCtx *args)
{
    if ((args->field->shape == CLI_VERSIONS_TEXT) || (args->field->shape == CLI_VERSIONS_BARE))
    {
        EMBED_CALL(aliases_put, VersionCatalogAliasesCtx, .out = args->out, .field = args->field);
        return;
    }
    if (args->field->shape == CLI_VERSIONS_LIST)
    {
        for (unsigned int at = 0u; at < args->field->count; at += 1u)
        {
            const CliVersionsItem *const item = &g_cli_versions.item[args->field->first + at];
            mmgr_line_put(args->out, (at > 0u) ? ", " : "");
            mmgr_line_put_n(
                args->out,
                EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = item->text_at),
                item->text_size);
        }
        return;
    }
    const CliVersionsField *const module =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "module");
    const CliVersionsField *const group =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "group");
    const CliVersionsField *const name =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "name");
    const CliVersionsField *const id =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "id");
    if (module != NULL)
    {
        EMBED_CALL(aliases_put, VersionCatalogAliasesCtx, .out = args->out, .field = module);
    }
    else if ((group != NULL) && (name != NULL))
    {
        EMBED_CALL(aliases_put, VersionCatalogAliasesCtx, .out = args->out, .field = group);
        mmgr_line_ch(args->out, ':');
        EMBED_CALL(aliases_put, VersionCatalogAliasesCtx, .out = args->out, .field = name);
    }
    else if (id != NULL)
    {
        EMBED_CALL(aliases_put, VersionCatalogAliasesCtx, .out = args->out, .field = id);
    }
    const CliVersionsField *const version =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "version");
    const CliVersionsField *const reference =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "version.ref");
    if (version != NULL)
    {
        mmgr_line_ch(args->out, ':');
        EMBED_CALL(aliases_put, VersionCatalogAliasesCtx, .out = args->out, .field = version);
    }
    else if (reference != NULL)
    {
        mmgr_line_put(args->out, ", version ");
        EMBED_CALL(aliases_put, VersionCatalogAliasesCtx, .out = args->out, .field = reference);
    }
}

void cli_versions_accessor(const CliVersionCatalogSizeOutAliasCtx *args)
{
    for (unsigned long long at = 0ull; at < args->size; at += 1u)
    {
        mmgr_line_ch(args->out, ((args->alias[at] == '-') || (args->alias[at] == '_')) ? '.' : args->alias[at]);
    }
}

// `size` bytes of `bytes` stored in the aliases' texts: where they start, CLI_VERSIONS_ALIASES_TEXT where they do not
// fit
EMBED_INLINE unsigned int aliases_text_store(const VersionCatalogAliasesCtx *args)
{
    const char *bytes = args->bytes;
    if (args->size + 1u > args->aliases->room - args->aliases->used)
    {
        return CLI_VERSIONS_ALIASES_TEXT;
    }
    const unsigned int at = args->aliases->used;
    memor.cpy(&(MemoriaCfg){.dst = args->aliases->text + at, .src = bytes, .bytes = (size_t)args->size});
    args->aliases->text[at + args->size] = '\0';
    args->aliases->used += (unsigned int)args->size + 1u;
    return at;
}

// the section of the aliases a section of the catalog read is, 4 where it is none of them
EMBED_INLINE unsigned int aliases_kind(const VersionCatalogAliasesCtx *args)
{
    static const char *const s_sections[4] = {"libraries", "versions", "plugins", "bundles"};
    unsigned int kind = 4u;
    for (unsigned int one = 0u; one < 4u; one += 1u)
    {
        kind = (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_version_catalog_internal.versions_text,
                                                               CliVersionCatalogCtx, .at = args->section->name_at),
                                             .other = s_sections[one],
                                             .cap = CLI_CELLS_POOL}))
                   ? one
                   : kind;
    }
    return kind;
}

// the aliases' cells taken: as many aliases as the four sections' entries; and texts of the catalog's texts, which
// store each key and all that its description names, with each alias's two terminators and the few bytes its
// description puts between what it names, and two a string of a list
EMBED_INLINE int aliases_take(const VersionCatalogAliasesCtx *args)
{
    CliVersionsAliases *aliases = args->aliases;
    unsigned long long count = 0ull;
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        count += (EMBED_CALL(aliases_kind, VersionCatalogAliasesCtx, .section = section) < 4u) ? section->count : 0u;
    }
    count = (count < CLI_VERSIONS_ALIASES) ? count : CLI_VERSIONS_ALIASES;
    const unsigned long long room = g_cli_versions.used + (16ull * count) + (2ull * g_cli_versions.items);
    // the room is under CLI_VERSIONS_ALIASES_TEXT, so it fits an unsigned int
    aliases->room = (unsigned int)((room < CLI_VERSIONS_ALIASES_TEXT) ? room : CLI_VERSIONS_ALIASES_TEXT);
    aliases->alias =
        g_cli_cells->persistent_buf_alloc((count + 1ull) * sizeof(CliVersionsAlias), &g_cli_versions_cells);
    aliases->text = g_cli_cells->persistent_buf_alloc(aliases->room + 1ull, &g_cli_versions_cells);
    if ((aliases->alias == NULL) || (aliases->text == NULL))
    {
        EMBED_CALL(cli_version_catalog_text_size_aliases_internal.versions_aliases_free,
                   CliVersionCatalogTextSizeAliasesCtx, .aliases = aliases);
        return 0;
    }
    return 1;
}

void cli_versions_aliases_free(const CliVersionCatalogTextSizeAliasesCtx *args)
{
    g_cli_cells->persistent_buf_release(args->aliases->text, &g_cli_versions_cells);
    g_cli_cells->persistent_buf_release(args->aliases->alias, &g_cli_versions_cells);
    args->aliases->alias = NULL;
    args->aliases->text = NULL;
    args->aliases->count = 0u;
    args->aliases->used = 0u;
    args->aliases->room = 0u;
}

int cli_versions_aliases(const CliVersionCatalogTextSizeAliasesCtx *args)
{
    const char *text = args->text;
    args->aliases->alias = NULL;
    args->aliases->text = NULL;
    args->aliases->count = 0u;
    args->aliases->used = 0u;
    args->aliases->room = 0u;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_versions_lock);
    if (!EMBED_CALL(cli_version_catalog_text_internal.versions_parse, CliVersionCatalogTextCtx, .text = text,
                    .size = args->size) ||
        !EMBED_CALL(aliases_take, VersionCatalogAliasesCtx, .aliases = args->aliases))
    {
        cli_versions_release();
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_versions_lock);
        return 0;
    }
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        const unsigned int kind = EMBED_CALL(aliases_kind, VersionCatalogAliasesCtx, .section = section);
        for (unsigned int entry = section->first; (kind < 4u) && (entry != CLI_VERSIONS_NONE);
             entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            const unsigned int alias =
                EMBED_CALL(aliases_text_store, VersionCatalogAliasesCtx, .aliases = args->aliases,
                           .bytes = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                               .at = field->key_at),
                           .size = field->key_size);
            // the description written where the texts end, and stored with its terminator where it fit
            VerbaLinea about = {args->aliases->text + args->aliases->used, args->aliases->room - args->aliases->used,
                                0ull};
            EMBED_CALL(cli_version_catalog_field_out_internal.versions_describe, CliVersionCatalogFieldOutCtx,
                       .field = field, .out = &about);
            if ((args->aliases->count < CLI_VERSIONS_ALIASES) && (alias != CLI_VERSIONS_ALIASES_TEXT) &&
                (about.capacity > 0u) && mmgr_line_ok(&about))
            {
                const unsigned int about_at = args->aliases->used;
                args->aliases->text[about_at + about.at] = '\0';
                args->aliases->used += (unsigned int)about.at + 1u;
                args->aliases->alias[args->aliases->count] =
                    (CliVersionsAlias){kind, alias, field->key_size, about_at, (unsigned int)about.at};
                args->aliases->count += 1u;
            }
        }
    }
    cli_versions_release();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_versions_lock);
    return 1;
}
