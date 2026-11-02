// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_check.c: a version catalog checked against itself: what prevented it from reading; a section it has
// no place for; an alias that is none or whose accessor another above it has; a library's alias that starts with a
// section of the catalog's accessor; each version, library, plugin and bundle written in its form, its table's keys
// known, the versions it names by `version.ref` and the libraries a bundle lists there; and each snapshot, with the
// line that refreshes the build's dependencies. The accessors of more versions or libraries than CLI_VERSIONS_NAMES are
// not stored
#include "version_catalog_internal.h"

// the keys of a version's table
static const char *const s_version_keys[5] = {"strictly", "require", "prefer", "reject", "rejectAll"};

CliVersionsNames g_cli_versions_known;
CliVersionsNames g_cli_versions_libraries;

// the accessors of the section `name`'s entries into `names`, each as long as its key, so a cell of as many as the
// section's entries and one of their keys' bytes fit them
int cli_versions_names_get(const CliVersionCatalogNameCtx *args)
{
    args->names_out->count = 0u;
    args->names_out->used = 0u;
    unsigned long long count = 0ull;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        if (!cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_version_catalog_internal.versions_text,
                                                            CliVersionCatalogCtx, .at = section->name_at),
                                          .other = args->name,
                                          .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        for (unsigned int entry = section->first; entry != CLI_VERSIONS_NONE; entry = g_cli_versions.field[entry].next)
        {
            count += 1ull;
            bytes += g_cli_versions.field[entry].key_size;
        }
    }
    // each is under its most, so it fits an unsigned int
    args->names_out->most = (unsigned int)((count < CLI_VERSIONS_NAMES) ? count : CLI_VERSIONS_NAMES);
    args->names_out->room = (unsigned int)((bytes < CLI_VERSIONS_NAMES_TEXT) ? bytes : CLI_VERSIONS_NAMES_TEXT) + 1u;
    args->names_out->at = g_cli_cells->persistent_buf_alloc(
        2ull * (args->names_out->most + 1ull) * sizeof(unsigned int), &g_cli_versions_cells);
    args->names_out->text = g_cli_cells->persistent_buf_alloc(args->names_out->room, &g_cli_versions_cells);
    if ((args->names_out->at == NULL) || (args->names_out->text == NULL))
    {
        g_cli_cells->persistent_buf_release(args->names_out->text, &g_cli_versions_cells);
        g_cli_cells->persistent_buf_release(args->names_out->at, &g_cli_versions_cells);
        args->names_out->at = NULL;
        args->names_out->size = NULL;
        args->names_out->text = NULL;
        args->names_out->most = 0u;
        args->names_out->room = 0u;
        return 0;
    }
    args->names_out->size = args->names_out->at + args->names_out->most + 1u;
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        if (!cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_version_catalog_internal.versions_text,
                                                            CliVersionCatalogCtx, .at = section->name_at),
                                          .other = args->name,
                                          .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        for (unsigned int entry = section->first; entry != CLI_VERSIONS_NONE; entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            VerbaLinea line = {args->names_out->text + args->names_out->used,
                               args->names_out->room - args->names_out->used, 0ull};
            EMBED_CALL(cli_version_catalog_size_out_alias_internal.versions_accessor, CliVersionCatalogSizeOutAliasCtx,
                       .alias = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                           .at = field->key_at),
                       .size = field->key_size, .out = &line);
            if ((args->names_out->count < args->names_out->most) && mmgr_line_ok(&line))
            {
                args->names_out->at[args->names_out->count] = args->names_out->used;
                args->names_out->size[args->names_out->count] = (unsigned int)line.at;
                args->names_out->count += 1u;
                args->names_out->used += (unsigned int)line.at;
            }
        }
    }
    return 1;
}

// the accessor of `size` bytes of `name` into `out`, and whether `names` contains it
int cli_versions_contains(const CliVersionCatalogSizeOutCtx *args)
{
    EMBED_CALL(cli_version_catalog_size_out_alias_internal.versions_accessor, CliVersionCatalogSizeOutAliasCtx,
               .alias = args->name, .size = args->size, .out = args->out);
    for (unsigned int at = 0u; at < args->names->count; at += 1u)
    {
        if ((args->names->size[at] == args->out->at) &&
            (memor.cmp(&(MemoriaCfg){.src = args->names->text + args->names->at[at],
                                     .other = args->out->out,
                                     .bytes = (size_t)args->out->at}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    CliVersionsCheck *check;
    unsigned int finding;
    const char *coordinates;
    unsigned long long size;
    const CliVersionsField *field;
    const char *const *known;
    unsigned int count;
    const char *of;
} VersionCatalogCheckCtx;
// the fix that refreshes the build's dependencies, for the snapshot `coordinates`
EMBED_INLINE void check_update(const VersionCatalogCheckCtx *args)
{
    unsigned int finding = args->finding;
    VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_put(&title, "Update the snapshot ");
    mmgr_line_put_n(&title, args->coordinates, args->size);
    char line[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea command = {line, sizeof(line), 0ull};
    mmgr_line_put(&command, args->check->program);
    mmgr_line_put(&command, " --refresh-dependencies dependencies");
    EMBED_CALL(cli_builds_found.build_run_fix, CliBuildsFoundCfg, .found = args->check->found, .finding = finding,
               .title = &title, .folder = args->check->folder,
               .folder_size = cellul.len(&(CatenaFinitaCfg){.src = args->check->folder, .cap = CLI_CELLS_POOL}),
               .line_text = line, .line_size = command.at);
}

// a finding that `text` over `from` to `to` is a snapshot, with the fix that refreshes it
void cli_versions_snapshot(const CliVersionCatalogTextSizeCtx *args)
{
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(&msg, args->text, args->size);
    mmgr_line_put(&msg, "` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies");
    const unsigned int finding =
        EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                   .lines = args->check->lines, .from = from, .to = to, .severity = CLI_BUILD_NOTE, .code = "snapshot");
    EMBED_CALL(check_update, VersionCatalogCheckCtx, .check = args->check, .finding = finding,
               .coordinates = args->text, .size = args->size);
}

// an error over the entry `field`'s key: its key between backticks, then `rest`
void cli_versions_entry_error(const CliVersionCatalogFieldCtx *args)
{
    const char *code = args->code;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(
        &msg, EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = args->field->key_at),
        args->field->key_size);
    mmgr_line_put(&msg, args->rest);
    (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                     .lines = args->check->lines, .from = args->field->from, .to = args->field->to,
                     .severity = CLI_BUILD_ERROR, .code = code);
}

// the fixes that change the span `from` to `to` to each name of `known` near `name`
void cli_versions_near(const CliVersionCatalogSizeCtx *args)
{
    unsigned int finding = args->finding;
    const char *name = args->name;
    unsigned long long size = args->size;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    CliBuildNear near;
    EMBED_CALL(cli_builds_size.build_near_begin, CliBuildsSizeCfg, .near = &near, .name = name, .size = size);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(cli_builds_size_near.build_near_offer, CliBuildsSizeNearCfg, .near = &near, .one = args->known[at],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->known[at], .cap = CLI_CELLS_POOL}));
    }
    for (unsigned int at = 0u; (args->names != NULL) && (at < args->names->count); at += 1u)
    {
        EMBED_CALL(cli_builds_size_near.build_near_offer, CliBuildsSizeNearCfg, .near = &near,
                   .one = args->names->text + args->names->at[at], .size = args->names->size[at]);
    }
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&title, "Change to ");
        mmgr_line_put(&title, args->open);
        mmgr_line_put_n(&title, near.near[at], near.near_size[at]);
        mmgr_line_put(&title, args->close);
        EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = args->check->found, .finding = finding,
                   .title = &title, .lines = args->check->lines, .from = from, .to = to, .text = near.near[at],
                   .size = near.near_size[at]);
    }
}

// a finding of a key `field` a table of `of` does not know, its keys `known`
EMBED_INLINE void check_unknown_key(const VersionCatalogCheckCtx *args)
{
    CliVersionsCheck *check = args->check;
    const char *const *known = args->known;
    unsigned int count = args->count;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
    mmgr_line_put(&msg, args->of);
    mmgr_line_put(&msg, " has no key `");
    const char *const key =
        EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = args->field->key_at);
    for (unsigned int at = 0u; at < args->field->key_size; at += 1u)
    {
        if (key[at] != '`')
        {
            mmgr_line_ch(&msg, key[at]);
        }
    }
    mmgr_line_put(&msg, "`: its keys are ");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&msg, (at > 0u) ? ", `" : "`");
        mmgr_line_put(&msg, known[at]);
        mmgr_line_ch(&msg, '`');
    }
    const unsigned int finding =
        EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                   .lines = check->lines, .from = args->field->from, .to = args->field->to,
                   .severity = CLI_BUILD_WARNING, .code = "catalog-key");
    EMBED_CALL(cli_version_catalog_size_internal.versions_near, CliVersionCatalogSizeCtx, .check = check,
               .finding = finding, .name = key, .size = args->field->key_size, .known = known, .count = count,
               .names = NULL, .from = args->field->from, .to = args->field->to, .open = "", .close = "");
}

// the keys of a table no table of its kind knows, each a finding
void cli_versions_keys(const CliVersionCatalogCheckCtx *args)
{
    for (unsigned int at = args->table->first; at != CLI_VERSIONS_NONE; at = g_cli_versions.field[at].next)
    {
        const CliVersionsField *const field = &g_cli_versions.field[at];
        int found = 0;
        for (unsigned int one = 0u; one < args->count; one += 1u)
        {
            found |=
                (cellul.len(&(CatenaFinitaCfg){.src = args->known[one], .cap = CLI_CELLS_POOL}) == field->key_size) &&
                (memor.cmp(&(MemoriaCfg){.src = args->known[one],
                                         .other = EMBED_CALL(cli_version_catalog_internal.versions_text,
                                                             CliVersionCatalogCtx, .at = field->key_at),
                                         .bytes = field->key_size}) == 0);
        }
        if (!found)
        {
            EMBED_CALL(check_unknown_key, VersionCatalogCheckCtx, .check = args->check, .field = field,
                       .known = args->known, .count = args->count, .of = args->of);
        }
    }
}

// a version: a snapshot noted, a table's keys known, any other value an error
void cli_versions_check_version(const CliVersionCatalogCheckEntryCtx *args)
{
    if (args->entry->shape == CLI_VERSIONS_TEXT)
    {
        const char *const text =
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = args->entry->text_at);
        if (EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg, .version = text,
                       .size = args->entry->text_size))
        {
            EMBED_CALL(cli_version_catalog_text_size_internal.versions_snapshot, CliVersionCatalogTextSizeCtx,
                       .check = args->check, .from = args->entry->value_from, .to = args->entry->value_to, .text = text,
                       .size = args->entry->text_size);
        }
        return;
    }
    if (args->entry->shape == CLI_VERSIONS_TABLE)
    {
        EMBED_CALL(cli_version_catalog_check_internal.versions_keys, CliVersionCatalogCheckCtx, .check = args->check,
                   .table = args->entry, .known = s_version_keys, .count = 5u, .of = "a version's table");
        return;
    }
    EMBED_CALL(cli_version_catalog_field_internal.versions_entry_error, CliVersionCatalogFieldCtx, .check = args->check,
               .field = args->entry, .code = "catalog-version",
               .rest =
                   "` is a version written as a string, or a table of `strictly`, `require`, `prefer` and `reject`");
}

// the version a library's or a plugin's table names, by `version` or `version.ref`, of the thing at `coordinates`
void cli_versions_version(const CliVersionCatalogCheckTableCtx *args)
{
    for (unsigned int at = args->table->first; at != CLI_VERSIONS_NONE; at = g_cli_versions.field[at].next)
    {
        const CliVersionsField *const field = &g_cli_versions.field[at];
        const char *const key =
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = field->key_at);
        const CliVersionsField *named = NULL;
        if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "version.ref", .cap = CLI_CELLS_POOL})) &&
            (field->shape == CLI_VERSIONS_TEXT))
        {
            named = field;
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "version", .cap = CLI_CELLS_POOL})) &&
                 (field->shape == CLI_VERSIONS_TABLE))
        {
            const unsigned int inner = EMBED_CALL(cli_version_catalog_first_internal.versions_get,
                                                  CliVersionCatalogFirstCtx, .first = field->first, .key = "ref");
            named = ((inner != CLI_VERSIONS_NONE) && (g_cli_versions.field[inner].shape == CLI_VERSIONS_TEXT))
                        ? &g_cli_versions.field[inner]
                        : NULL;
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "version", .cap = CLI_CELLS_POOL})) &&
                 (field->shape == CLI_VERSIONS_TEXT) &&
                 EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg,
                            .version = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                                  .at = field->text_at),
                            .size = field->text_size))
        {
            VerbaLinea full = {args->check->full, 2u * args->check->room, 0ull};
            mmgr_line_put_n(&full, args->coordinates, args->coordinates_size);
            mmgr_line_ch(&full, ':');
            mmgr_line_put_n(
                &full,
                EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = field->text_at),
                field->text_size);
            EMBED_CALL(cli_version_catalog_text_size_internal.versions_snapshot, CliVersionCatalogTextSizeCtx,
                       .check = args->check, .from = field->value_from, .to = field->value_to,
                       .text = args->check->full, .size = full.at);
        }
        if (named == NULL)
        {
            continue;
        }
        const char *const name =
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = named->text_at);
        char accessor[1u << 12u];
        VerbaLinea line = {accessor, sizeof(accessor), 0ull};
        if (EMBED_CALL(cli_version_catalog_size_out_internal.versions_contains, CliVersionCatalogSizeOutCtx,
                       .names = &g_cli_versions_known, .name = name, .size = named->text_size, .out = &line))
        {
            continue;
        }
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "The catalog's `[versions]` has no `");
        mmgr_line_put_n(&msg, name, named->text_size);
        mmgr_line_ch(&msg, '`');
        const unsigned int finding =
            EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                       .lines = args->check->lines, .from = named->value_from, .to = named->value_to,
                       .severity = CLI_BUILD_ERROR, .code = "catalog-reference");
        EMBED_CALL(cli_version_catalog_size_internal.versions_near, CliVersionCatalogSizeCtx, .check = args->check,
                   .finding = finding, .name = accessor, .size = line.at, .known = NULL, .count = 0u,
                   .names = &g_cli_versions_known, .from = named->value_from, .to = named->value_to, .open = "",
                   .close = "");
    }
}
