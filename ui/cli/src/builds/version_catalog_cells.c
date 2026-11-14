// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// version_catalog_cells.c: the cells a catalog read is allocated in, each counted from its text before it is read. A
// section starts at a `[`, a field at its `=` and a string of a list between two quotes, so each part is a cell of as
// many as those bytes make, up to its max. What prevented a catalog from reading is at max four a line and one more a
// `{` and a
// `,`. Each text the reader stores is bytes of the catalog's own, none stored twice, or a message of its own naming a
// key, so its texts take the catalog's bytes, a message's for each error and a terminator each. A key or a string is
// closed on its line, so the longest line contains it
#include "version_catalog_internal.h"

typedef struct
{
    unsigned long long count;
    unsigned long long max;
    CliVersionsNames *names;
} VersionCatalogCellsCtx;

// `count` up to `max`
EMBED_INLINE unsigned long long cells_max(const VersionCatalogCellsCtx *args)
{
    return (args->count < args->max) ? args->count : args->max;
}

int cli_versions_alloc(const CliVersionCatalogTextCtx *args)
{
    CliVersionsParsed *const parsed = &g_cli_versions;
    unsigned long long opens = 0ULL;
    unsigned long long equals = 0ULL;
    unsigned long long quotes = 0ULL;
    unsigned long long braces = 0ULL;
    unsigned long long commas = 0ULL;
    unsigned long long lines = 1ULL;
    unsigned long long line_from = 0ULL;
    parsed->longest = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1u)
    {
        const char here = args->text[at];
        opens += (here == '[') ? 1ULL : 0ULL;
        equals += (here == '=') ? 1ULL : 0ULL;
        quotes += ((here == '"') || (here == '\'')) ? 1ULL : 0ULL;
        braces += (here == '{') ? 1ULL : 0ULL;
        commas += (here == ',') ? 1ULL : 0ULL;
        if (here == '\n')
        {
            parsed->longest = (at - line_from > parsed->longest) ? at - line_from : parsed->longest;
            line_from = at + 1u;
            lines += 1ULL;
        }
    }
    parsed->longest = (args->size - line_from > parsed->longest) ? args->size - line_from : parsed->longest;
    // each capacity is under its max, so it fits an unsigned int
    parsed->sections_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = opens, .max = CLI_VERSIONS_SECTIONS);
    parsed->fields_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = equals, .max = CLI_VERSIONS_FIELDS);
    parsed->items_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = quotes / 2u, .max = CLI_VERSIONS_ITEMS);
    parsed->errors_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = (4ULL * lines) + braces + commas + 1ULL,
                                 .max = CLI_VERSIONS_ERRORS);
    const unsigned long long capacities_total =
        parsed->sections_capacity + (2ULL * parsed->fields_capacity) + parsed->items_capacity + parsed->errors_capacity;
    parsed->pool_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx,
                                 .count = args->size + capacities_total +
                                          ((unsigned long long)parsed->errors_capacity * CLI_VERSIONS_MSG_MAX_LEN),
                                 .max = CLI_VERSIONS_POOL);
    // a part of none is a cell of one, so every part is a cell
    parsed->section = g_cli_guard->temporary_buf_alloc((parsed->sections_capacity + 1ULL) * sizeof(CliVersionsSection),
                                                       g_cli_versions_owner);
    parsed->field = g_cli_guard->temporary_buf_alloc((parsed->fields_capacity + 1ULL) * sizeof(CliVersionsField),
                                                     g_cli_versions_owner);
    parsed->item = g_cli_guard->temporary_buf_alloc((parsed->items_capacity + 1ULL) * sizeof(CliVersionsItem),
                                                    g_cli_versions_owner);
    parsed->error = g_cli_guard->temporary_buf_alloc((parsed->errors_capacity + 1ULL) * sizeof(CliVersionsError),
                                                     g_cli_versions_owner);
    parsed->pool = g_cli_guard->temporary_buf_alloc(parsed->pool_capacity + 1ULL, g_cli_versions_owner);
    // a key or a string the line contains, and after it a message naming a key
    parsed->line = g_cli_guard->temporary_buf_alloc((2ULL * (parsed->longest + 1ULL)) + CLI_VERSIONS_MSG_MAX_LEN,
                                                    g_cli_versions_owner);
    if ((parsed->section == NULL) || (parsed->field == NULL) || (parsed->item == NULL) || (parsed->error == NULL) ||
        (parsed->pool == NULL) || (parsed->line == NULL))
    {
        cli_versions_free();
        return 0;
    }
    return 1;
}
