// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_cells.c: the cells a catalog read is allocated in, each counted from its text before it is read. A
// section starts at a `[`, a field at its `=` and a string of a list between two quotes, so each part is a cell of as
// many as those bytes make, up to its most. What prevented a catalog from reading is at most four a line and one more a
// `{` and a
// `,`. Each text the reader stores is bytes of the catalog's own, none stored twice, or a message of its own naming a
// key, so its texts take the catalog's bytes, a message's for each error and a terminator each. A key or a string is
// closed on its line, so the longest line contains it
#include "version_catalog_internal.h"

typedef struct
{
    unsigned long long count;
    unsigned long long most;
    CliVersionsNames *names;
} VersionCatalogCellsCtx;

// `count` up to `most`
EMBED_INLINE unsigned long long cells_most(const VersionCatalogCellsCtx *args)
{
    return (args->count < args->most) ? args->count : args->most;
}

int cli_versions_take(const CliVersionCatalogTextCtx *args)
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
    for (unsigned long long at = 0ull; at < args->size; at += 1u)
    {
        const char here = args->text[at];
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
    parsed->longest = (args->size - line_from > parsed->longest) ? args->size - line_from : parsed->longest;
    // each room is under its most, so it fits an unsigned int
    parsed->sections_room =
        (unsigned int)EMBED_CALL(cells_most, VersionCatalogCellsCtx, .count = opens, .most = CLI_VERSIONS_SECTIONS);
    parsed->fields_room =
        (unsigned int)EMBED_CALL(cells_most, VersionCatalogCellsCtx, .count = equals, .most = CLI_VERSIONS_FIELDS);
    parsed->items_room =
        (unsigned int)EMBED_CALL(cells_most, VersionCatalogCellsCtx, .count = quotes / 2u, .most = CLI_VERSIONS_ITEMS);
    parsed->errors_room =
        (unsigned int)EMBED_CALL(cells_most, VersionCatalogCellsCtx, .count = (4ull * lines) + braces + commas + 1ull,
                                 .most = CLI_VERSIONS_ERRORS);
    const unsigned long long rooms_total =
        parsed->sections_room + (2ull * parsed->fields_room) + parsed->items_room + parsed->errors_room;
    parsed->pool_room = (unsigned int)EMBED_CALL(
        cells_most, VersionCatalogCellsCtx,
        .count = args->size + rooms_total + ((unsigned long long)parsed->errors_room * CLI_VERSIONS_MSG_MAX_LEN),
        .most = CLI_VERSIONS_POOL);
    // a part of none is a cell of one, so every part is a cell
    parsed->section = g_cli_cells->persistent_buf_alloc((parsed->sections_room + 1ull) * sizeof(CliVersionsSection),
                                                        &g_cli_versions_cells);
    parsed->field = g_cli_cells->persistent_buf_alloc((parsed->fields_room + 1ull) * sizeof(CliVersionsField),
                                                      &g_cli_versions_cells);
    parsed->item =
        g_cli_cells->persistent_buf_alloc((parsed->items_room + 1ull) * sizeof(CliVersionsItem), &g_cli_versions_cells);
    parsed->error = g_cli_cells->persistent_buf_alloc((parsed->errors_room + 1ull) * sizeof(CliVersionsError),
                                                      &g_cli_versions_cells);
    parsed->pool = g_cli_cells->persistent_buf_alloc(parsed->pool_room + 1ull, &g_cli_versions_cells);
    // a key or a string the line contains, and after it a message naming a key
    parsed->line = g_cli_cells->persistent_buf_alloc((2ull * (parsed->longest + 1ull)) + CLI_VERSIONS_MSG_MAX_LEN,
                                                     &g_cli_versions_cells);
    if ((parsed->section == NULL) || (parsed->field == NULL) || (parsed->item == NULL) || (parsed->error == NULL) ||
        (parsed->pool == NULL) || (parsed->line == NULL))
    {
        cli_versions_release();
        return 0;
    }
    return 1;
}

// a section's accessors given back
EMBED_INLINE void cells_names_free(const VersionCatalogCellsCtx *args)
{
    g_cli_cells->persistent_buf_release(args->names->text, &g_cli_versions_cells);
    g_cli_cells->persistent_buf_release(args->names->at, &g_cli_versions_cells);
    args->names->at = NULL;
    args->names->size = NULL;
    args->names->text = NULL;
    args->names->count = 0u;
    args->names->most = 0u;
    args->names->used = 0u;
    args->names->room = 0u;
}

void cli_versions_release(void)
{
    EMBED_CALL(cells_names_free, VersionCatalogCellsCtx, .names = &g_cli_versions_libraries);
    EMBED_CALL(cells_names_free, VersionCatalogCellsCtx, .names = &g_cli_versions_known);
    CliVersionsParsed *const parsed = &g_cli_versions;
    g_cli_cells->persistent_buf_release(parsed->line, &g_cli_versions_cells);
    g_cli_cells->persistent_buf_release(parsed->pool, &g_cli_versions_cells);
    g_cli_cells->persistent_buf_release(parsed->error, &g_cli_versions_cells);
    g_cli_cells->persistent_buf_release(parsed->item, &g_cli_versions_cells);
    g_cli_cells->persistent_buf_release(parsed->field, &g_cli_versions_cells);
    g_cli_cells->persistent_buf_release(parsed->section, &g_cli_versions_cells);
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
