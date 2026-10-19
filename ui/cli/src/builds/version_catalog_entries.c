// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_entries.c: a version catalog's sections and entries checked: what kept it from reading, a section it
// has no place for with the sections near its name, each alias's form and accessor, and each library, plugin and
// bundle in its form, against the versions and libraries the catalog holds. The build a catalog belongs to is the one
// above its `gradle` folder
#include "builds_internal.h"
#include "version_catalog_internal.h"

// the sections a catalog holds, the keys of a library's and a plugin's tables, and the first words of a library's
// alias that name the catalog's other sections
static const char *const s_sections[5] = {"versions", "libraries", "bundles", "plugins", "metadata"};
static const char *const s_library_keys[5] = {"module", "group", "name", "version", "version.ref"};
static const char *const s_plugin_keys[3] = {"id", "version", "version.ref"};
static const char *const s_reserved[6] = {"bundles", "versions", "plugins", "extensions", "class", "convention"};

// whether `size` bytes of `text` split at `:` are `count` parts, none of them blank
static int entries_parts(const char *text, unsigned long long size, unsigned int count)
{
    unsigned int parts = 0u;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; at <= size; at += 1u)
    {
        if ((at < size) && (text[at] != ':'))
        {
            continue;
        }
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(text + start, at - start, &from, &to);
        if (to == from)
        {
            return 0;
        }
        parts += 1u;
        start = at + 1u;
    }
    return parts == count;
}

// an error over the span inside a string's quotes: the string between backticks, then `rest`
static void entries_text_error(CliVersionsCheck *check, const CliVersionsField *field, const char *code,
                               const char *rest)
{
    CliLine said = cli_build_said(check->found);
    cli_line_character(&said, '`');
    cli_line_counted(&said, cli_versions_text(field->text_at), field->text_size);
    cli_line_text(&said, rest);
    (void)cli_build_finding(check->found, &said, check->lines, field->value_from, field->value_to, CLI_BUILD_ERROR,
                            code);
}

// the string a table's field keyed `key` holds, NULL where it holds none or the field is none
static const CliVersionsField *entries_text_of(const CliVersionsField *table, const char *key, int *present)
{
    const unsigned int at = cli_versions_get(table->first, key);
    *present = (at != CLI_VERSIONS_NONE);
    return (*present && (g_cli_versions.field[at].shape == CLI_VERSIONS_TEXT)) ? &g_cli_versions.field[at] : NULL;
}

// a library: a string of `group:name:version`, or a table naming its module and its version
static void entries_library(CliVersionsCheck *check, const CliVersionsField *entry)
{
    if (entry->shape == CLI_VERSIONS_TEXT)
    {
        const char *const text = cli_versions_text(entry->text_at);
        unsigned long long last = entry->text_size;
        while ((last > 0u) && (text[last - 1u] != ':'))
        {
            last -= 1u;
        }
        if (!entries_parts(text, entry->text_size, 3u))
        {
            entries_text_error(check, entry, "catalog-library",
                               "` is no library: a library's string is `group:name:version`");
        }
        else if (cli_build_snapshot(text + last, entry->text_size - last))
        {
            cli_versions_snapshot(check, entry->value_from, entry->value_to, text, entry->text_size);
        }
        return;
    }
    if (entry->shape != CLI_VERSIONS_TABLE)
    {
        cli_versions_entry_error(check, entry, "catalog-library",
                                 "` is a library written as a string, `group:name:version`, or a table");
        return;
    }
    cli_versions_keys(check, entry, s_library_keys, 5u, "A library's table");
    int module_present = 0;
    int group_present = 0;
    int name_present = 0;
    const CliVersionsField *const module = entries_text_of(entry, "module", &module_present);
    const CliVersionsField *const group = entries_text_of(entry, "group", &group_present);
    const CliVersionsField *const name = entries_text_of(entry, "name", &name_present);
    CliLine coordinates = {check->coordinates, check->room, 0ull};
    if (module != NULL)
    {
        if (!entries_parts(cli_versions_text(module->text_at), module->text_size, 2u))
        {
            entries_text_error(check, module, "catalog-library", "` is no module: a module is `group:name`");
        }
        cli_line_counted(&coordinates, cli_versions_text(module->text_at), module->text_size);
    }
    else if (!module_present && (group != NULL) && (name != NULL))
    {
        cli_line_counted(&coordinates, cli_versions_text(group->text_at), group->text_size);
        cli_line_character(&coordinates, ':');
        cli_line_counted(&coordinates, cli_versions_text(name->text_at), name->text_size);
    }
    else
    {
        cli_versions_entry_error(check, entry, "catalog-library",
                                 "` names its library by `module`, or by `group` and `name`");
    }
    cli_versions_versioned(check, entry, check->coordinates, coordinates.at);
}

// a plugin: a string of `id:version`, or a table naming its id and its version
static void entries_plugin(CliVersionsCheck *check, const CliVersionsField *entry)
{
    if (entry->shape == CLI_VERSIONS_TEXT)
    {
        if (!entries_parts(cli_versions_text(entry->text_at), entry->text_size, 2u))
        {
            entries_text_error(check, entry, "catalog-plugin", "` is no plugin: a plugin's string is `id:version`");
        }
        return;
    }
    if (entry->shape != CLI_VERSIONS_TABLE)
    {
        cli_versions_entry_error(check, entry, "catalog-plugin",
                                 "` is a plugin written as a string, `id:version`, or a table");
        return;
    }
    cli_versions_keys(check, entry, s_plugin_keys, 3u, "A plugin's table");
    int present = 0;
    const CliVersionsField *const id = entries_text_of(entry, "id", &present);
    if (id == NULL)
    {
        cli_versions_entry_error(check, entry, "catalog-plugin", "` names its plugin by `id`");
        return;
    }
    cli_versions_versioned(check, entry, cli_versions_text(id->text_at), id->text_size);
}

// a bundle: a list of the libraries' aliases, each one the catalog's
static void entries_bundle(CliVersionsCheck *check, const CliVersionsField *entry)
{
    if (entry->shape != CLI_VERSIONS_LIST)
    {
        cli_versions_entry_error(check, entry, "catalog-bundle",
                                 "` is a bundle written as a list of libraries' aliases");
        return;
    }
    for (unsigned int at = 0u; at < entry->count; at += 1u)
    {
        const CliVersionsItem *const member = &g_cli_versions.item[entry->first + at];
        const char *const text = cli_versions_text(member->text_at);
        char accessor[1u << 12u];
        CliLine line = {accessor, sizeof(accessor), 0ull};
        if (cli_versions_holds(&g_cli_versions_libraries, text, member->text_size, &line))
        {
            continue;
        }
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "The catalog's `[libraries]` has no `");
        cli_line_counted(&said, text, member->text_size);
        cli_line_character(&said, '`');
        const unsigned int finding = cli_build_finding(check->found, &said, check->lines, member->from, member->to,
                                                       CLI_BUILD_ERROR, "catalog-reference");
        cli_versions_near(check, finding, accessor, line.at, NULL, 0u, &g_cli_versions_libraries, member->from,
                          member->to, "", "");
    }
}

// an entry's alias checked against its form and the aliases above it in its section, `seen` of them from `first`
static void entries_alias(CliVersionsCheck *check, const CliVersionsField *entry, const char *section,
                          unsigned int first)
{
    const char *const alias = cli_versions_text(entry->key_at);
    char named[1u << 12u];
    CliLine line = {named, sizeof(named), 0ull};
    cli_versions_accessor(alias, entry->key_size, &line);
    for (unsigned int above = first; above != CLI_VERSIONS_NONE; above = g_cli_versions.field[above].next)
    {
        const CliVersionsField *const other = &g_cli_versions.field[above];
        if (other == entry)
        {
            break;
        }
        char other_named[1u << 12u];
        CliLine other_line = {other_named, sizeof(other_named), 0ull};
        cli_versions_accessor(cli_versions_text(other->key_at), other->key_size, &other_line);
        if ((other_line.at == line.at) &&
            (memor.cmp(&(MemoriaCfg){.src = other_named, .other = named, .bytes = (size_t)line.at}) == 0))
        {
            CliLine said = cli_build_said(check->found);
            cli_line_character(&said, '`');
            cli_line_counted(&said, alias, entry->key_size);
            cli_line_text(&said, "` has the accessor of an alias above it, `");
            cli_line_counted(&said, named, line.at);
            cli_line_character(&said, '`');
            (void)cli_build_finding(check->found, &said, check->lines, entry->from, entry->to, CLI_BUILD_ERROR,
                                    "catalog-alias");
            break;
        }
    }
    int shaped = (entry->key_size > 1u) && (alias[0] >= 'a') && (alias[0] <= 'z');
    for (unsigned int at = 0u; shaped && (at < entry->key_size); at += 1u)
    {
        const char one = alias[at];
        shaped = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) || ((one >= '0') && (one <= '9')) ||
                 (one == '_') || (one == '.') || (one == '-');
    }
    if (!shaped)
    {
        cli_versions_entry_error(check, entry, "catalog-alias",
                                 "` is no alias: an alias starts with a lowercase letter and goes on in letters, "
                                 "digits, `-`, `_` and `.`");
    }
    const char *const dot = memor.chr(&(MemoriaCfg){.src = named, .val = '.', .bytes = (size_t)line.at});
    const unsigned long long head = (dot != NULL) ? (unsigned long long)(dot - named) : line.at;
    for (unsigned int at = 0u;
         (cellul.eq(&(CatenaFinitaCfg){.src = section, .other = "libraries", .cap = CLI_CELLS_POOL})) && (at < 6u);
         at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_reserved[at], .cap = CLI_CELLS_POOL}) == head) &&
            (memor.cmp(&(MemoriaCfg){.src = s_reserved[at], .other = named, .bytes = (size_t)head}) == 0))
        {
            CliLine said = cli_build_said(check->found);
            cli_line_text(&said, "A library's alias cannot start with `");
            cli_line_counted(&said, named, head);
            cli_line_text(&said, "`, the name of a section of the catalog's accessor");
            (void)cli_build_finding(check->found, &said, check->lines, entry->from, entry->to, CLI_BUILD_ERROR,
                                    "catalog-alias");
        }
    }
}

// a section no catalog holds, with the sections near its name
static void entries_unknown_section(CliVersionsCheck *check, const CliVersionsSection *section)
{
    const char *const name = cli_versions_text(section->name_at);
    CliLine said = cli_build_said(check->found);
    cli_line_text(&said, "A catalog has no section `[");
    cli_line_counted(&said, name, section->name_size);
    cli_line_text(&said, "]`: its sections are `[versions]`, `[libraries]`, `[bundles]` and `[plugins]`");
    const unsigned int finding = cli_build_finding(check->found, &said, check->lines, section->from, section->to,
                                                   CLI_BUILD_WARNING, "catalog-section");
    cli_versions_near(check, finding, name, section->name_size, s_sections, 5u, NULL, section->from, section->to, "[",
                      "]");
}

void cli_versions_check(const char *root, const char *path, const char *text, unsigned long long size,
                        const CliBuildLines *lines, CliBuildFindings *found)
{
    static CliVersionsCheck s_check;
    cli_lock_take(&g_cli_versions_lock);
    if (!cli_versions_parse(text, size))
    {
        cli_versions_released();
        cli_lock_give(&g_cli_versions_lock);
        return;
    }
    s_check.lines = lines;
    s_check.found = found;
    char folder[CLI_PATH_LONGEST];
    char above[CLI_PATH_LONGEST];
    if (!cli_path_parent(path, above) || !cli_path_parent(above, folder))
    {
        CliLine copy = {folder, sizeof(folder), 0ull};
        cli_line_text(&copy, root);
        (void)cli_line_closed(&copy);
    }
    cli_build_inside(root, folder, s_check.folder);
    char build[CLI_PATH_LONGEST];
    cli_full(root, s_check.folder, build);
    cli_gradle_program(build, s_check.program);
    for (unsigned int at = 0u; at < g_cli_versions.errors; at += 1u)
    {
        const CliVersionsError *const error = &g_cli_versions.error[at];
        const CliLine said = {(char *)cli_versions_text(error->message_at), error->message_size + 1u,
                              error->message_size};
        (void)cli_build_finding(found, &said, lines, error->from, error->to, CLI_BUILD_ERROR, "catalog-syntax");
    }
    // a dependency's coordinates are a module, or a group and a name, of the catalog's texts, and with its version
    // twice that; the entries are checked only where these and the accessors are met
    s_check.room = g_cli_versions.used + 2ull;
    s_check.coordinates = cli_cell_taken_by(&g_cli_versions_cells, 3ull * s_check.room, "a catalog's coordinates");
    s_check.full = (s_check.coordinates != NULL) ? s_check.coordinates + s_check.room : NULL;
    const int met = (s_check.coordinates != NULL) && cli_versions_names_of("versions", &g_cli_versions_known) &&
                    cli_versions_names_of("libraries", &g_cli_versions_libraries);
    for (unsigned int at = 0u; met && (at < g_cli_versions.sections); at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        const char *const name = cli_versions_text(section->name_at);
        unsigned int kind = 5u;
        for (unsigned int one = 0u; one < 5u; one += 1u)
        {
            kind = (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_sections[one], .cap = CLI_CELLS_POOL})) ? one
                                                                                                                 : kind;
        }
        if (kind == 5u)
        {
            entries_unknown_section(&s_check, section);
            continue;
        }
        for (unsigned int entry = section->first; (kind < 4u) && (entry != CLI_VERSIONS_NONE);
             entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            entries_alias(&s_check, field, name, section->first);
            if (kind == 0u)
            {
                cli_versions_check_version(&s_check, field);
            }
            else if (kind == 1u)
            {
                entries_library(&s_check, field);
            }
            else if (kind == 2u)
            {
                entries_bundle(&s_check, field);
            }
            else
            {
                entries_plugin(&s_check, field);
            }
        }
    }
    cli_cell_given_by(&g_cli_versions_cells, s_check.coordinates);
    s_check.coordinates = NULL;
    s_check.full = NULL;
    cli_versions_released();
    cli_lock_give(&g_cli_versions_lock);
}
