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
    const embed_word size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
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
EMBED_INLINE void aliases_append(const VersionCatalogAliasesCtx *args)
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
        EMBED_CALL(aliases_append, VersionCatalogAliasesCtx, .out = args->out, .field = args->field);
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
        EMBED_CALL(aliases_append, VersionCatalogAliasesCtx, .out = args->out, .field = module);
    }
    else if ((group != NULL) && (name != NULL))
    {
        EMBED_CALL(aliases_append, VersionCatalogAliasesCtx, .out = args->out, .field = group);
        mmgr_line_ch(args->out, ':');
        EMBED_CALL(aliases_append, VersionCatalogAliasesCtx, .out = args->out, .field = name);
    }
    else if (id != NULL)
    {
        EMBED_CALL(aliases_append, VersionCatalogAliasesCtx, .out = args->out, .field = id);
    }
    const CliVersionsField *const version =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "version");
    const CliVersionsField *const reference =
        EMBED_CALL(aliases_text_get, VersionCatalogAliasesCtx, .table = args->field, .key = "version.ref");
    if (version != NULL)
    {
        mmgr_line_ch(args->out, ':');
        EMBED_CALL(aliases_append, VersionCatalogAliasesCtx, .out = args->out, .field = version);
    }
    else if (reference != NULL)
    {
        mmgr_line_put(args->out, ", version ");
        EMBED_CALL(aliases_append, VersionCatalogAliasesCtx, .out = args->out, .field = reference);
    }
}

void cli_versions_accessor(const CliVersionCatalogSizeOutAliasCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->size; at += 1u)
    {
        mmgr_line_ch(args->out, ((args->alias[at] == '-') || (args->alias[at] == '_')) ? '.' : args->alias[at]);
    }
}

// `size` bytes of `bytes` stored in the aliases' texts: where they start, CLI_VERSIONS_ALIASES_TEXT where they do not
// fit
EMBED_INLINE unsigned int aliases_text_store(const VersionCatalogAliasesCtx *args)
{
    const char *bytes = args->bytes;
    if (args->size + 1u > args->aliases->capacity - args->aliases->used)
    {
        return CLI_VERSIONS_ALIASES_TEXT;
    }
    const unsigned int at = args->aliases->used;
    memor.cpy(&(MemoriaCfg){.dst = args->aliases->text + at, .src = bytes, .bytes = (embed_word)args->size});
    args->aliases->text[at + args->size] = '\0';
    args->aliases->used += (unsigned int)args->size + 1u;
    return at;
}

// the section of the aliases a section of the catalog read is, 4 where it is none of them
EMBED_INLINE unsigned int aliases_type(const VersionCatalogAliasesCtx *args)
{
    static const char *const s_sections[4] = {"libraries", "versions", "plugins", "bundles"};
    unsigned int type = 4u;
    for (unsigned int one = 0u; one < 4u; one += 1u)
    {
        type = (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_version_catalog_internal.versions_text,
                                                               CliVersionCatalogCtx, .at = args->section->name_at),
                                             .other = s_sections[one],
                                             .cap = CLI_POOL_BYTES}))
                   ? one
                   : type;
    }
    return type;
}

// the aliases' cells taken: as many aliases as the four sections' entries; and texts of the catalog's texts, which
// store each key and all that its description names, with each alias's two terminators and the few bytes its
// description puts between what it names, and two a string of a list
EMBED_INLINE int aliases_alloc(const VersionCatalogAliasesCtx *args)
{
    CliVersionsAliases *aliases = args->aliases;
    unsigned long long count = 0ULL;
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        count += (EMBED_CALL(aliases_type, VersionCatalogAliasesCtx, .section = section) < 4u) ? section->count : 0u;
    }
    count = (count < CLI_VERSIONS_ALIASES) ? count : CLI_VERSIONS_ALIASES;
    const unsigned long long capacity = g_cli_versions.used + (16ULL * count) + (2ULL * g_cli_versions.items);
    // the capacity is under CLI_VERSIONS_ALIASES_TEXT, so it fits an unsigned int
    aliases->capacity = (unsigned int)((capacity < CLI_VERSIONS_ALIASES_TEXT) ? capacity : CLI_VERSIONS_ALIASES_TEXT);
    aliases->alias = g_cli_guard->temporary_buf_alloc((count + 1ULL) * sizeof(CliVersionsAlias), g_cli_versions_owner);
    aliases->text = g_cli_guard->temporary_buf_alloc(aliases->capacity + 1ULL, g_cli_versions_owner);
    if ((aliases->alias == NULL) || (aliases->text == NULL))
    {
        g_cli_guard->persistent_buf_release(aliases->text, g_cli_versions_owner);
        g_cli_guard->persistent_buf_release(aliases->alias, g_cli_versions_owner);
        aliases->alias = NULL;
        aliases->text = NULL;
        aliases->count = 0u;
        aliases->used = 0u;
        aliases->capacity = 0u;
        return 0;
    }
    return 1;
}

int cli_versions_aliases(const CliVersionCatalogTextSizeAliasesCtx *args)
{
    const char *text = args->text;
    args->aliases->alias = NULL;
    args->aliases->text = NULL;
    args->aliases->count = 0u;
    args->aliases->used = 0u;
    args->aliases->capacity = 0u;
    if (!EMBED_CALL(cli_version_catalog_text_internal.versions_parse, CliVersionCatalogTextCtx, .text = text,
                    .size = args->size) ||
        !EMBED_CALL(aliases_alloc, VersionCatalogAliasesCtx, .aliases = args->aliases))
    {
        cli_versions_free();
        return 0;
    }
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        const unsigned int type = EMBED_CALL(aliases_type, VersionCatalogAliasesCtx, .section = section);
        for (unsigned int entry = section->first; (type < 4u) && (entry != CLI_VERSIONS_NONE);
             entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            const unsigned int alias =
                EMBED_CALL(aliases_text_store, VersionCatalogAliasesCtx, .aliases = args->aliases,
                           .bytes = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                               .at = field->key_at),
                           .size = field->key_size);
            // the description written where the texts end, and stored with its terminator where it fit
            VerbaLinea about = {args->aliases->text + args->aliases->used,
                                args->aliases->capacity - args->aliases->used, 0ULL};
            EMBED_CALL(cli_version_catalog_field_out_internal.versions_describe, CliVersionCatalogFieldOutCtx,
                       .field = field, .out = &about);
            if ((args->aliases->count < CLI_VERSIONS_ALIASES) && (alias != CLI_VERSIONS_ALIASES_TEXT) &&
                (about.capacity > 0u) && mmgr_line_ok(&about))
            {
                const unsigned int about_at = args->aliases->used;
                args->aliases->text[about_at + about.at] = '\0';
                args->aliases->used += (unsigned int)about.at + 1u;
                args->aliases->alias[args->aliases->count] =
                    (CliVersionsAlias){type, alias, field->key_size, about_at, (unsigned int)about.at};
                args->aliases->count += 1u;
            }
        }
    }
    cli_versions_free();
    return 1;
}
