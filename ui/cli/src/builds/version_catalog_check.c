// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_check.c: a version catalog checked against itself: what kept it from reading; a section it has no
// place for; an alias that is none or whose accessor another above it has; a library's alias that starts with a
// section of the catalog's accessor; each version, library, plugin and bundle written in its form, its table's keys
// known, the versions it names by `version.ref` and the libraries a bundle lists there; and each snapshot, with the
// line that refreshes the build's dependencies. The accessors of more versions or libraries than CLI_VERSIONS_NAMES are
// not kept
#include "version_catalog_internal.h"

// the keys of a version's table
static const char *const s_version_keys[5] = {"strictly", "require", "prefer", "reject", "rejectAll"};

CliVersionsNames g_cli_versions_known;
CliVersionsNames g_cli_versions_libraries;

// the accessors of the section `name`'s entries into `names`, each as long as its key, so a cell of as many as the
// section's entries and one of their keys' bytes hold them
int cli_versions_names_of(const char *name, CliVersionsNames *names)
{
    names->count = 0u;
    names->used = 0u;
    unsigned long long count = 0ull;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        if (!cellul.eq(
                &(CatenaFinitaCfg){.src = cli_versions_text(section->name_at), .other = name, .cap = CLI_CELLS_POOL}))
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
    names->most = (unsigned int)((count < CLI_VERSIONS_NAMES) ? count : CLI_VERSIONS_NAMES);
    names->room = (unsigned int)((bytes < CLI_VERSIONS_NAMES_TEXT) ? bytes : CLI_VERSIONS_NAMES_TEXT) + 1u;
    names->at = cli_cell_taken_by(&g_cli_versions_cells, 2ull * (names->most + 1ull) * sizeof(unsigned int),
                                  "a catalog's accessors");
    names->size = (names->at != NULL) ? names->at + names->most + 1u : NULL;
    names->text = cli_cell_taken_by(&g_cli_versions_cells, names->room, "a catalog's accessors' texts");
    if ((names->at == NULL) || (names->text == NULL))
    {
        cli_cell_given_by(&g_cli_versions_cells, names->text);
        cli_cell_given_by(&g_cli_versions_cells, names->at);
        names->at = NULL;
        names->size = NULL;
        names->text = NULL;
        names->most = 0u;
        names->room = 0u;
        return 0;
    }
    for (unsigned int at = 0u; at < g_cli_versions.sections; at += 1u)
    {
        const CliVersionsSection *const section = &g_cli_versions.section[at];
        if (!cellul.eq(
                &(CatenaFinitaCfg){.src = cli_versions_text(section->name_at), .other = name, .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        for (unsigned int entry = section->first; entry != CLI_VERSIONS_NONE; entry = g_cli_versions.field[entry].next)
        {
            const CliVersionsField *const field = &g_cli_versions.field[entry];
            CliLine line = {names->text + names->used, names->room - names->used, 0ull};
            cli_versions_accessor(cli_versions_text(field->key_at), field->key_size, &line);
            if ((names->count < names->most) && cli_line_fits(&line))
            {
                names->at[names->count] = names->used;
                names->size[names->count] = (unsigned int)line.at;
                names->count += 1u;
                names->used += (unsigned int)line.at;
            }
        }
    }
    return 1;
}

// the accessor of `size` bytes of `name` into `out`, and whether `names` holds it
int cli_versions_holds(const CliVersionsNames *names, const char *name, unsigned long long size, CliLine *out)
{
    cli_versions_accessor(name, size, out);
    for (unsigned int at = 0u; at < names->count; at += 1u)
    {
        if ((names->size[at] == out->at) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = names->text + names->at[at], .other = out->out, .bytes = (size_t)out->at}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// the fix that refreshes the build's dependencies, for the snapshot `coordinates`
static void check_update(CliVersionsCheck *check, unsigned int finding, const char *coordinates,
                         unsigned long long size)
{
    CliLine title = cli_build_said(check->found);
    cli_line_text(&title, "Update the snapshot ");
    cli_line_counted(&title, coordinates, size);
    char line[CLI_PATH_LONGEST + 64u];
    CliLine command = {line, sizeof(line), 0ull};
    cli_line_text(&command, check->program);
    cli_line_text(&command, " --refresh-dependencies dependencies");
    cli_build_run_fix(check->found, finding, &title, check->folder,
                      cellul.len(&(CatenaFinitaCfg){.src = check->folder, .cap = CLI_CELLS_POOL}), line, command.at);
}

// a finding that `text` over `from` to `to` is a snapshot, with the fix that refreshes it
void cli_versions_snapshot(CliVersionsCheck *check, unsigned long long from, unsigned long long to, const char *text,
                           unsigned long long size)
{
    CliLine said = cli_build_said(check->found);
    cli_line_character(&said, '`');
    cli_line_counted(&said, text, size);
    cli_line_text(&said, "` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies");
    const unsigned int finding =
        cli_build_finding(check->found, &said, check->lines, from, to, CLI_BUILD_NOTE, "snapshot");
    check_update(check, finding, text, size);
}

// an error over the entry `field`'s key: its key between backticks, then `rest`
void cli_versions_entry_error(CliVersionsCheck *check, const CliVersionsField *field, const char *code,
                              const char *rest)
{
    CliLine said = cli_build_said(check->found);
    cli_line_character(&said, '`');
    cli_line_counted(&said, cli_versions_text(field->key_at), field->key_size);
    cli_line_text(&said, rest);
    (void)cli_build_finding(check->found, &said, check->lines, field->from, field->to, CLI_BUILD_ERROR, code);
}

// the fixes that change the span `from` to `to` to each name of `known` near `name`
void cli_versions_near(CliVersionsCheck *check, unsigned int finding, const char *name, unsigned long long size,
                       const char *const *known, unsigned int count, const CliVersionsNames *names,
                       unsigned long long from, unsigned long long to, const char *open, const char *close)
{
    CliBuildNear near;
    cli_build_near_begun(&near, name, size);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_build_near_offered(&near, known[at],
                               cellul.len(&(CatenaFinitaCfg){.src = known[at], .cap = CLI_CELLS_POOL}));
    }
    for (unsigned int at = 0u; (names != NULL) && (at < names->count); at += 1u)
    {
        cli_build_near_offered(&near, names->text + names->at[at], names->size[at]);
    }
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        CliLine title = cli_build_said(check->found);
        cli_line_text(&title, "Change to ");
        cli_line_text(&title, open);
        cli_line_counted(&title, near.near[at], near.near_size[at]);
        cli_line_text(&title, close);
        cli_build_edit_fix(check->found, finding, &title, check->lines, from, to, near.near[at], near.near_size[at]);
    }
}

// a finding of a key `field` a table of `of` does not know, its keys `known`
static void check_unknown_key(CliVersionsCheck *check, const CliVersionsField *field, const char *const *known,
                              unsigned int count, const char *of)
{
    CliLine said = cli_build_said(check->found);
    cli_line_text(&said, of);
    cli_line_text(&said, " has no key `");
    const char *const key = cli_versions_text(field->key_at);
    for (unsigned int at = 0u; at < field->key_size; at += 1u)
    {
        if (key[at] != '`')
        {
            cli_line_character(&said, key[at]);
        }
    }
    cli_line_text(&said, "`: its keys are ");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&said, (at > 0u) ? ", `" : "`");
        cli_line_text(&said, known[at]);
        cli_line_character(&said, '`');
    }
    const unsigned int finding =
        cli_build_finding(check->found, &said, check->lines, field->from, field->to, CLI_BUILD_WARNING, "catalog-key");
    cli_versions_near(check, finding, key, field->key_size, known, count, NULL, field->from, field->to, "", "");
}

// the keys of a table no table of its kind knows, each a finding
void cli_versions_keys(CliVersionsCheck *check, const CliVersionsField *table, const char *const *known,
                       unsigned int count, const char *of)
{
    for (unsigned int at = table->first; at != CLI_VERSIONS_NONE; at = g_cli_versions.field[at].next)
    {
        const CliVersionsField *const field = &g_cli_versions.field[at];
        int found = 0;
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            found |=
                (cellul.len(&(CatenaFinitaCfg){.src = known[one], .cap = CLI_CELLS_POOL}) == field->key_size) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = known[one], .other = cli_versions_text(field->key_at), .bytes = field->key_size}) == 0);
        }
        if (!found)
        {
            check_unknown_key(check, field, known, count, of);
        }
    }
}

// a version: a snapshot noted, a table's keys known, any other value an error
void cli_versions_check_version(CliVersionsCheck *check, const CliVersionsField *entry)
{
    if (entry->shape == CLI_VERSIONS_TEXT)
    {
        const char *const text = cli_versions_text(entry->text_at);
        if (cli_build_snapshot(text, entry->text_size))
        {
            cli_versions_snapshot(check, entry->value_from, entry->value_to, text, entry->text_size);
        }
        return;
    }
    if (entry->shape == CLI_VERSIONS_TABLE)
    {
        cli_versions_keys(check, entry, s_version_keys, 5u, "a version's table");
        return;
    }
    cli_versions_entry_error(
        check, entry, "catalog-version",
        "` is a version written as a string, or a table of `strictly`, `require`, `prefer` and `reject`");
}

// the version a library's or a plugin's table names, by `version` or `version.ref`, of the thing at `coordinates`
void cli_versions_versioned(CliVersionsCheck *check, const CliVersionsField *table, const char *coordinates,
                            unsigned long long coordinates_size)
{
    for (unsigned int at = table->first; at != CLI_VERSIONS_NONE; at = g_cli_versions.field[at].next)
    {
        const CliVersionsField *const field = &g_cli_versions.field[at];
        const char *const key = cli_versions_text(field->key_at);
        const CliVersionsField *named = NULL;
        if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "version.ref", .cap = CLI_CELLS_POOL})) &&
            (field->shape == CLI_VERSIONS_TEXT))
        {
            named = field;
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "version", .cap = CLI_CELLS_POOL})) &&
                 (field->shape == CLI_VERSIONS_TABLE))
        {
            const unsigned int inner = cli_versions_get(field->first, "ref");
            named = ((inner != CLI_VERSIONS_NONE) && (g_cli_versions.field[inner].shape == CLI_VERSIONS_TEXT))
                        ? &g_cli_versions.field[inner]
                        : NULL;
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "version", .cap = CLI_CELLS_POOL})) &&
                 (field->shape == CLI_VERSIONS_TEXT) &&
                 cli_build_snapshot(cli_versions_text(field->text_at), field->text_size))
        {
            CliLine full = {check->full, 2u * check->room, 0ull};
            cli_line_counted(&full, coordinates, coordinates_size);
            cli_line_character(&full, ':');
            cli_line_counted(&full, cli_versions_text(field->text_at), field->text_size);
            cli_versions_snapshot(check, field->value_from, field->value_to, check->full, full.at);
        }
        if (named == NULL)
        {
            continue;
        }
        const char *const name = cli_versions_text(named->text_at);
        char accessor[1u << 12u];
        CliLine line = {accessor, sizeof(accessor), 0ull};
        if (cli_versions_holds(&g_cli_versions_known, name, named->text_size, &line))
        {
            continue;
        }
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "The catalog's `[versions]` has no `");
        cli_line_counted(&said, name, named->text_size);
        cli_line_character(&said, '`');
        const unsigned int finding = cli_build_finding(check->found, &said, check->lines, named->value_from,
                                                       named->value_to, CLI_BUILD_ERROR, "catalog-reference");
        cli_versions_near(check, finding, accessor, line.at, NULL, 0u, &g_cli_versions_known, named->value_from,
                          named->value_to, "", "");
    }
}
