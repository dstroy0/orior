// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_entries.c: a version catalog's sections and entries checked: what prevented it from reading, a
// section it has no place for with the sections near its name, each alias's form and accessor, and each library, plugin
// and bundle in its form, against the versions and libraries the catalog contains. The build a catalog belongs to is
// the one above its `gradle` folder
#include "../text/text.h"
#include "builds_internal.h"
#include "version_catalog_internal.h"

// the sections a catalog contains, the keys of a library's and a plugin's tables, and the first words of a library's
// alias that name the catalog's other sections
static const char *const s_sections[5] = {"versions", "libraries", "bundles", "plugins", "metadata"};
static const char *const s_library_keys[5] = {"module", "group", "name", "version", "version.ref"};
static const char *const s_plugin_keys[3] = {"id", "version", "version.ref"};
static const char *const s_reserved[6] = {"bundles", "versions", "plugins", "extensions", "class", "convention"};

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned int count;
    CliVersionsCheck *check;
    const CliVersionsField *field;
    const char *code;
    const char *rest;
    const CliVersionsField *table;
    const char *key;
    int *present;
    const CliVersionsField *entry;
    const char *section;
    unsigned int first;
    const CliVersionsSection *section_versions_section_pointer;
} VersionCatalogEntriesCtx;
// whether `size` bytes of `text` split at `:` are `count` parts, none of them blank
EMBED_INLINE int entries_parts(const VersionCatalogEntriesCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int parts = 0u;
    unsigned long long start = 0ULL;
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
            return 0;
        }
        parts += 1u;
        start = at + 1u;
    }
    return parts == args->count;
}

// an error over the span inside a string's quotes: the string between backticks, then `rest`
EMBED_INLINE void entries_text_error(const VersionCatalogEntriesCtx *args)
{
    const char *code = args->code;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(
        &msg, EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = args->field->text_at),
        args->field->text_size);
    mmgr_line_put(&msg, args->rest);
    (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                     .lines = args->check->lines, .from = args->field->value_from, .to = args->field->value_to,
                     .severity = CLI_BUILD_ERROR, .code = code);
}

// the string a table's field keyed `key` contains, NULL where it contains none or the field is none
EMBED_INLINE const CliVersionsField *entries_text_get(const VersionCatalogEntriesCtx *args)
{
    const char *key = args->key;
    int *present = args->present;
    const unsigned int at = EMBED_CALL(cli_version_catalog_first_internal.versions_get, CliVersionCatalogFirstCtx,
                                       .first = args->table->first, .key = key);
    *present = (at != CLI_VERSIONS_NONE);
    return (*present && (g_cli_versions.field[at].shape == CLI_VERSIONS_TEXT)) ? &g_cli_versions.field[at] : NULL;
}

// a library: a string of `group:name:version`, or a table naming its module and its version
EMBED_INLINE void entries_library(const VersionCatalogEntriesCtx *args)
{
    CliVersionsCheck *check = args->check;
    if (args->entry->shape == CLI_VERSIONS_TEXT)
    {
        const char *const text =
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = args->entry->text_at);
        unsigned long long last = args->entry->text_size;
        last = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = last,
                          .set = ":");
        if (!EMBED_CALL(entries_parts, VersionCatalogEntriesCtx, .text = text, .size = args->entry->text_size,
                        .count = 3u))
        {
            EMBED_CALL(entries_text_error, VersionCatalogEntriesCtx, .check = check, .field = args->entry,
                       .code = "catalog-library",
                       .rest = "` is no library: a library's string is `group:name:version`");
        }
        else if (EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg, .version = text + last,
                            .size = args->entry->text_size - last))
        {
            EMBED_CALL(cli_version_catalog_text_size_internal.versions_snapshot, CliVersionCatalogTextSizeCtx,
                       .check = check, .from = args->entry->value_from, .to = args->entry->value_to, .text = text,
                       .size = args->entry->text_size);
        }
        return;
    }
    if (args->entry->shape != CLI_VERSIONS_TABLE)
    {
        EMBED_CALL(cli_version_catalog_field_internal.versions_entry_error, CliVersionCatalogFieldCtx, .check = check,
                   .field = args->entry, .code = "catalog-library",
                   .rest = "` is a library written as a string, `group:name:version`, or a table");
        return;
    }
    EMBED_CALL(cli_version_catalog_check_internal.versions_keys, CliVersionCatalogCheckCtx, .check = check,
               .table = args->entry, .known = s_library_keys, .count = 5u, .of = "A library's table");
    int module_present = 0;
    int group_present = 0;
    int name_present = 0;
    const CliVersionsField *const module = EMBED_CALL(entries_text_get, VersionCatalogEntriesCtx, .table = args->entry,
                                                      .key = "module", .present = &module_present);
    const CliVersionsField *const group = EMBED_CALL(entries_text_get, VersionCatalogEntriesCtx, .table = args->entry,
                                                     .key = "group", .present = &group_present);
    const CliVersionsField *const name = EMBED_CALL(entries_text_get, VersionCatalogEntriesCtx, .table = args->entry,
                                                    .key = "name", .present = &name_present);
    VerbaLinea coordinates = {check->coordinates, check->capacity, 0ULL};
    if (module != NULL)
    {
        if (!EMBED_CALL(entries_parts, VersionCatalogEntriesCtx,
                        .text = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                           .at = module->text_at),
                        .size = module->text_size, .count = 2u))
        {
            EMBED_CALL(entries_text_error, VersionCatalogEntriesCtx, .check = check, .field = module,
                       .code = "catalog-library", .rest = "` is no module: a module is `group:name`");
        }
        mmgr_line_put_n(
            &coordinates,
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = module->text_at),
            module->text_size);
    }
    else if (!module_present && (group != NULL) && (name != NULL))
    {
        mmgr_line_put_n(
            &coordinates,
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = group->text_at),
            group->text_size);
        mmgr_line_ch(&coordinates, ':');
        mmgr_line_put_n(
            &coordinates,
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = name->text_at),
            name->text_size);
    }
    else
    {
        EMBED_CALL(cli_version_catalog_field_internal.versions_entry_error, CliVersionCatalogFieldCtx, .check = check,
                   .field = args->entry, .code = "catalog-library",
                   .rest = "` names its library by `module`, or by `group` and `name`");
    }
    EMBED_CALL(cli_version_catalog_check_table_internal.versions_version, CliVersionCatalogCheckTableCtx,
               .check = check, .table = args->entry, .coordinates = check->coordinates,
               .coordinates_size = coordinates.at);
}

// a plugin: a string of `id:version`, or a table naming its id and its version
EMBED_INLINE void entries_plugin(const VersionCatalogEntriesCtx *args)
{
    CliVersionsCheck *check = args->check;
    if (args->entry->shape == CLI_VERSIONS_TEXT)
    {
        if (!EMBED_CALL(entries_parts, VersionCatalogEntriesCtx,
                        .text = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                           .at = args->entry->text_at),
                        .size = args->entry->text_size, .count = 2u))
        {
            EMBED_CALL(entries_text_error, VersionCatalogEntriesCtx, .check = check, .field = args->entry,
                       .code = "catalog-plugin", .rest = "` is no plugin: a plugin's string is `id:version`");
        }
        return;
    }
    if (args->entry->shape != CLI_VERSIONS_TABLE)
    {
        EMBED_CALL(cli_version_catalog_field_internal.versions_entry_error, CliVersionCatalogFieldCtx, .check = check,
                   .field = args->entry, .code = "catalog-plugin",
                   .rest = "` is a plugin written as a string, `id:version`, or a table");
        return;
    }
    EMBED_CALL(cli_version_catalog_check_internal.versions_keys, CliVersionCatalogCheckCtx, .check = check,
               .table = args->entry, .known = s_plugin_keys, .count = 3u, .of = "A plugin's table");
    int present = 0;
    const CliVersionsField *const id =
        EMBED_CALL(entries_text_get, VersionCatalogEntriesCtx, .table = args->entry, .key = "id", .present = &present);
    if (id == NULL)
    {
        EMBED_CALL(cli_version_catalog_field_internal.versions_entry_error, CliVersionCatalogFieldCtx, .check = check,
                   .field = args->entry, .code = "catalog-plugin", .rest = "` names its plugin by `id`");
        return;
    }
    EMBED_CALL(cli_version_catalog_check_table_internal.versions_version, CliVersionCatalogCheckTableCtx,
               .check = check, .table = args->entry,
               .coordinates =
                   EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = id->text_at),
               .coordinates_size = id->text_size);
}

// a bundle: a list of the libraries' aliases, each one the catalog's
EMBED_INLINE void entries_bundle(const VersionCatalogEntriesCtx *args)
{
    CliVersionsCheck *check = args->check;
    if (args->entry->shape != CLI_VERSIONS_LIST)
    {
        EMBED_CALL(cli_version_catalog_field_internal.versions_entry_error, CliVersionCatalogFieldCtx, .check = check,
                   .field = args->entry, .code = "catalog-bundle",
                   .rest = "` is a bundle written as a list of libraries' aliases");
        return;
    }
    for (unsigned int at = 0u; at < args->entry->count; at += 1u)
    {
        const CliVersionsItem *const member = &g_cli_versions.item[args->entry->first + at];
        const char *const text =
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = member->text_at);
        char accessor[1u << 12u];
        VerbaLinea line = {accessor, sizeof(accessor), 0ULL};
        if (EMBED_CALL(cli_version_catalog_size_out_internal.versions_contains, CliVersionCatalogSizeOutCtx,
                       .names = &g_cli_versions_libraries, .name = text, .size = member->text_size, .out = &line))
        {
            continue;
        }
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&msg, "The catalog's `[libraries]` has no `");
        mmgr_line_put_n(&msg, text, member->text_size);
        mmgr_line_ch(&msg, '`');
        const unsigned int finding =
            EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                       .lines = check->lines, .from = member->from, .to = member->to, .severity = CLI_BUILD_ERROR,
                       .code = "catalog-reference");
        EMBED_CALL(cli_version_catalog_size_internal.versions_adjacent, CliVersionCatalogSizeCtx, .check = check,
                   .finding = finding, .name = accessor, .size = line.at, .known = NULL, .count = 0u,
                   .names = &g_cli_versions_libraries, .from = member->from, .to = member->to, .open = "", .close = "");
    }
}

// an entry's alias checked against its form and the aliases above it in its section, `seen` of them from `first`
EMBED_INLINE void entries_alias(const VersionCatalogEntriesCtx *args)
{
    CliVersionsCheck *check = args->check;
    const char *const alias =
        EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = args->entry->key_at);
    char named[1u << 12u];
    VerbaLinea line = {named, sizeof(named), 0ULL};
    EMBED_CALL(cli_version_catalog_size_out_alias_internal.versions_accessor, CliVersionCatalogSizeOutAliasCtx,
               .alias = alias, .size = args->entry->key_size, .out = &line);
    for (unsigned int above = args->first; above != CLI_VERSIONS_NONE; above = g_cli_versions.field[above].next)
    {
        const CliVersionsField *const other = &g_cli_versions.field[above];
        if (other == args->entry)
        {
            break;
        }
        char other_named[1u << 12u];
        VerbaLinea other_line = {other_named, sizeof(other_named), 0ULL};
        EMBED_CALL(
            cli_version_catalog_size_out_alias_internal.versions_accessor, CliVersionCatalogSizeOutAliasCtx,
            .alias = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = other->key_at),
            .size = other->key_size, .out = &other_line);
        if ((other_line.at == line.at) &&
            (memor.cmp(&(MemoriaCfg){.src = other_named, .other = named, .bytes = line.at}) == 0))
        {
            VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
            mmgr_line_ch(&msg, '`');
            mmgr_line_put_n(&msg, alias, args->entry->key_size);
            mmgr_line_put(&msg, "` has the accessor of an alias above it, `");
            mmgr_line_put_n(&msg, named, line.at);
            mmgr_line_ch(&msg, '`');
            (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                             .lines = check->lines, .from = args->entry->from, .to = args->entry->to,
                             .severity = CLI_BUILD_ERROR, .code = "catalog-alias");
            break;
        }
    }
    int shaped = (args->entry->key_size > 1u) && (alias[0] >= 'a') && (alias[0] <= 'z');
    for (unsigned int at = 0u; shaped && (at < args->entry->key_size); at += 1u)
    {
        const char one = alias[at];
        shaped = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) || ((one >= '0') && (one <= '9')) ||
                 (one == '_') || (one == '.') || (one == '-');
    }
    if (!shaped)
    {
        EMBED_CALL(cli_version_catalog_field_internal.versions_entry_error, CliVersionCatalogFieldCtx, .check = check,
                   .field = args->entry, .code = "catalog-alias",
                   .rest = "` is no alias: an alias starts with a lowercase letter and goes on in letters, "
                           "digits, `-`, `_` and `.`");
    }
    const char *const dot = memor.chr(&(MemoriaCfg){.src = named, .val = '.', .bytes = line.at});
    const unsigned long long head = (dot != NULL) ? (unsigned long long)(dot - named) : line.at;
    for (unsigned int at = 0u;
         (cellul.eq(&(CatenaFinitaCfg){.src = args->section, .other = "libraries", .cap = CLI_CELLS_POOL})) &&
         (at < 6u);
         at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_reserved[at], .cap = CLI_CELLS_POOL}) == head) &&
            (memor.cmp(&(MemoriaCfg){.src = s_reserved[at], .other = named, .bytes = (size_t)head}) == 0))
        {
            VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
            mmgr_line_put(&msg, "A library's alias cannot start with `");
            mmgr_line_put_n(&msg, named, head);
            mmgr_line_put(&msg, "`, the name of a section of the catalog's accessor");
            (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                             .lines = check->lines, .from = args->entry->from, .to = args->entry->to,
                             .severity = CLI_BUILD_ERROR, .code = "catalog-alias");
        }
    }
}

// a section no catalog contains, with the sections near its name
EMBED_INLINE void entries_unknown_section(const VersionCatalogEntriesCtx *args)
{
    CliVersionsCheck *check = args->check;
    const char *const name = EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                        .at = args->section_versions_section_pointer->name_at);
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
    mmgr_line_put(&msg, "A catalog has no section `[");
    mmgr_line_put_n(&msg, name, args->section_versions_section_pointer->name_size);
    mmgr_line_put(&msg, "]`: its sections are `[versions]`, `[libraries]`, `[bundles]` and `[plugins]`");
    const unsigned int finding = EMBED_CALL(
        cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
        .lines = check->lines, .from = args->section_versions_section_pointer->from,
        .to = args->section_versions_section_pointer->to, .severity = CLI_BUILD_WARNING, .code = "catalog-section");
    EMBED_CALL(cli_version_catalog_size_internal.versions_adjacent, CliVersionCatalogSizeCtx, .check = check,
               .finding = finding, .name = name, .size = args->section_versions_section_pointer->name_size,
               .known = s_sections, .count = 5u, .names = NULL, .from = args->section_versions_section_pointer->from,
               .to = args->section_versions_section_pointer->to, .open = "[", .close = "]");
}

void cli_versions_check(const CliVersionCatalogTextCtx *args)
{
    const char *root = args->root;
    const CliBuildLines *lines = args->lines;
    CliBuildFindings *found = args->found;
    static CliVersionsCheck s_check;
    if (!cli_version_catalog_text_internal.versions_parse(args))
    {
        cli_versions_release();
        return;
    }
    s_check.lines = lines;
    s_check.found = found;
    char folder[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->path, .out = above) ||
        !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = above, .out = folder))
    {
        VerbaLinea copy = {folder, sizeof(folder), 0ULL};
        mmgr_line_put(&copy, root);
        (void)mmgr_line_finish(&copy);
    }
    EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = root, .folder = folder,
               .out_capacity = s_check.folder);
    char build[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = s_check.folder, .joined = build);
    EMBED_CALL(cli_builds_internal.gradle_program, CliBuildsCtx, .folder = build, .out = s_check.program);
    for (unsigned int at = 0u; at < g_cli_versions.errors; at += 1u)
    {
        const CliVersionsError *const error = &g_cli_versions.error[at];
        const VerbaLinea msg = {(char *)EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx,
                                                   .at = error->message_at),
                                error->message_size + 1u, error->message_size};
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = found, .message = &msg,
                         .lines = lines, .from = error->from, .to = error->to, .severity = CLI_BUILD_ERROR,
                         .code = "catalog-syntax");
    }
    // a dependency's coordinates are a module, or a group and a name, of the catalog's texts, and with its version
    // twice that; the entries are checked only where these and the accessors are met
    s_check.capacity = g_cli_versions.used + 2ULL;
    s_check.coordinates = g_cli_cells->persistent_buf_alloc(3ULL * s_check.capacity, &g_cli_versions_cells);
    s_check.full = (s_check.coordinates != NULL) ? s_check.coordinates + s_check.capacity : NULL;
    const int met = (s_check.coordinates != NULL) &&
                    EMBED_CALL(cli_version_catalog_name_internal.versions_names_get, CliVersionCatalogNameCtx,
                               .name = "versions", .names_out = &g_cli_versions_known) &&
                    EMBED_CALL(cli_version_catalog_name_internal.versions_names_get, CliVersionCatalogNameCtx,
                               .name = "libraries", .names_out = &g_cli_versions_libraries);
    for (unsigned int at = 0u; met && (at < g_cli_versions.sections); at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        const char *const name =
            EMBED_CALL(cli_version_catalog_internal.versions_text, CliVersionCatalogCtx, .at = section->name_at);
        unsigned int kind = 5u;
        for (unsigned int one = 0u; one < 5u; one += 1u)
        {
            kind = (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_sections[one], .cap = CLI_CELLS_POOL})) ? one
                                                                                                                 : kind;
        }
        if (kind == 5u)
        {
            EMBED_CALL(entries_unknown_section, VersionCatalogEntriesCtx, .check = &s_check,
                       .section_versions_section_pointer = section);
            continue;
        }
        for (unsigned int entry = section->first; (kind < 4u) && (entry != CLI_VERSIONS_NONE);
             entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            EMBED_CALL(entries_alias, VersionCatalogEntriesCtx, .check = &s_check, .entry = field, .section = name,
                       .first = section->first);
            if (kind == 0u)
            {
                EMBED_CALL(cli_version_catalog_check_entry_internal.versions_check_version,
                           CliVersionCatalogCheckEntryCtx, .check = &s_check, .entry = field);
            }
            else if (kind == 1u)
            {
                EMBED_CALL(entries_library, VersionCatalogEntriesCtx, .check = &s_check, .entry = field);
            }
            else if (kind == 2u)
            {
                EMBED_CALL(entries_bundle, VersionCatalogEntriesCtx, .check = &s_check, .entry = field);
            }
            else
            {
                EMBED_CALL(entries_plugin, VersionCatalogEntriesCtx, .check = &s_check, .entry = field);
            }
        }
    }
    g_cli_cells->persistent_buf_release(s_check.coordinates, &g_cli_versions_cells);
    s_check.coordinates = NULL;
    s_check.full = NULL;
    cli_versions_release();
}
