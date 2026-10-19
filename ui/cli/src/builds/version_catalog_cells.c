// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_cells.c: the cells a catalog read is held in, each counted from its text before it is read. A section
// starts at a `[`, a field at its `=` and a string of a list between two quotes, so each part is a cell of as many as
// those bytes make, up to its most. What kept a catalog from reading is at most four a line and one more a `{` and a
// `,`. Each text the reader keeps is bytes of the catalog's own, none kept twice, or a message of its own naming a key,
// so its texts take the catalog's bytes, a message's for each error and a terminator each. A key or a string is closed
// on its line, so the longest line holds it
#include "version_catalog_internal.h"

CliCellsTally g_cli_versions_cells = CLI_CELLS_TALLY("cli_version_catalogs");

// `count` up to `most`
static unsigned long long cells_most(unsigned long long count, unsigned long long most)
{
    return (count < most) ? count : most;
}

int cli_versions_taken(const char *text, unsigned long long size)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    unsigned long long opens = 0ull;
    unsigned long long equals = 0ull;
    unsigned long long quotes = 0ull;
    unsigned long long braces = 0ull;
    unsigned long long commas = 0ull;
    unsigned long long lines = 1ull;
    unsigned long long line_from = 0ull;
    parsed->longest = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        const char here = text[at];
        opens += (here == '[') ? 1ull : 0ull;
        equals += (here == '=') ? 1ull : 0ull;
        quotes += ((here == '"') || (here == '\'')) ? 1ull : 0ull;
        braces += (here == '{') ? 1ull : 0ull;
        commas += (here == ',') ? 1ull : 0ull;
        if (here == '\n')
        {
            parsed->longest = (at - line_from > parsed->longest) ? at - line_from : parsed->longest;
            line_from = at + 1u;
            lines += 1ull;
        }
    }
    parsed->longest = (size - line_from > parsed->longest) ? size - line_from : parsed->longest;
    // each room is under its most, so it fits an unsigned int
    parsed->sections_room = (unsigned int)cells_most(opens, CLI_VERSIONS_SECTIONS);
    parsed->fields_room = (unsigned int)cells_most(equals, CLI_VERSIONS_FIELDS);
    parsed->items_room = (unsigned int)cells_most(quotes / 2u, CLI_VERSIONS_ITEMS);
    parsed->errors_room = (unsigned int)cells_most((4ull * lines) + braces + commas + 1ull, CLI_VERSIONS_ERRORS);
    const unsigned long long kept =
        parsed->sections_room + (2ull * parsed->fields_room) + parsed->items_room + parsed->errors_room;
    parsed->pool_room = (unsigned int)cells_most(
        size + kept + ((unsigned long long)parsed->errors_room * CLI_VERSIONS_SAID), CLI_VERSIONS_POOL);
    // a part of none is a cell of one, so every part is a cell
    parsed->section = cli_cell_taken_by(
        &g_cli_versions_cells, (parsed->sections_room + 1ull) * sizeof(CliVersionsSection), "a catalog's sections");
    parsed->field = cli_cell_taken_by(&g_cli_versions_cells, (parsed->fields_room + 1ull) * sizeof(CliVersionsField),
                                      "a catalog's fields");
    parsed->item = cli_cell_taken_by(&g_cli_versions_cells, (parsed->items_room + 1ull) * sizeof(CliVersionsItem),
                                     "a catalog's lists");
    parsed->error = cli_cell_taken_by(&g_cli_versions_cells, (parsed->errors_room + 1ull) * sizeof(CliVersionsError),
                                      "what kept a catalog from reading");
    parsed->pool = cli_cell_taken_by(&g_cli_versions_cells, parsed->pool_room + 1ull, "a catalog's texts");
    // a key or a string the line holds, and after it a message naming a key
    parsed->line = cli_cell_taken_by(&g_cli_versions_cells, (2ull * (parsed->longest + 1ull)) + CLI_VERSIONS_SAID,
                                     "a catalog's line");
    if ((parsed->section == NULL) || (parsed->field == NULL) || (parsed->item == NULL) || (parsed->error == NULL) ||
        (parsed->pool == NULL) || (parsed->line == NULL))
    {
        cli_versions_released();
        return 0;
    }
    return 1;
}

// a section's accessors given back
static void cells_names_given(CliVersionsNames *names)
{
    cli_cell_given_by(&g_cli_versions_cells, names->text);
    cli_cell_given_by(&g_cli_versions_cells, names->at);
    names->at = NULL;
    names->size = NULL;
    names->text = NULL;
    names->count = 0u;
    names->most = 0u;
    names->used = 0u;
    names->room = 0u;
}

void cli_versions_released(void)
{
    cells_names_given(&g_cli_versions_libraries);
    cells_names_given(&g_cli_versions_known);
    CliVersionsParsed *const parsed = &g_cli_versions;
    cli_cell_given_by(&g_cli_versions_cells, parsed->line);
    cli_cell_given_by(&g_cli_versions_cells, parsed->pool);
    cli_cell_given_by(&g_cli_versions_cells, parsed->error);
    cli_cell_given_by(&g_cli_versions_cells, parsed->item);
    cli_cell_given_by(&g_cli_versions_cells, parsed->field);
    cli_cell_given_by(&g_cli_versions_cells, parsed->section);
    parsed->line = NULL;
    parsed->pool = NULL;
    parsed->error = NULL;
    parsed->item = NULL;
    parsed->field = NULL;
    parsed->section = NULL;
    parsed->sections = 0u;
    parsed->sections_room = 0u;
    parsed->fields = 0u;
    parsed->fields_room = 0u;
    parsed->items = 0u;
    parsed->items_room = 0u;
    parsed->errors = 0u;
    parsed->errors_room = 0u;
    parsed->used = 0u;
    parsed->pool_room = 0u;
    parsed->longest = 0ull;
}
