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
EMBED_INLINE unsigned long long cells_max(const VersionCatalogCellsCtx *args)
{
    return (args->count < args->most) ? args->count : args->most;
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
    // each capacity is under its most, so it fits an unsigned int
    parsed->sections_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = opens, .most = CLI_VERSIONS_SECTIONS);
    parsed->fields_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = equals, .most = CLI_VERSIONS_FIELDS);
    parsed->items_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = quotes / 2u, .most = CLI_VERSIONS_ITEMS);
    parsed->errors_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx, .count = (4ULL * lines) + braces + commas + 1ULL,
                                 .most = CLI_VERSIONS_ERRORS);
    const unsigned long long capacities_total =
        parsed->sections_capacity + (2ULL * parsed->fields_capacity) + parsed->items_capacity + parsed->errors_capacity;
    parsed->pool_capacity =
        (unsigned int)EMBED_CALL(cells_max, VersionCatalogCellsCtx,
                                 .count = args->size + capacities_total +
                                          ((unsigned long long)parsed->errors_capacity * CLI_VERSIONS_MSG_MAX_LEN),
                                 .most = CLI_VERSIONS_POOL);
    // a part of none is a cell of one, so every part is a cell
    parsed->section = g_cli_cells->persistent_buf_alloc((parsed->sections_capacity + 1ULL) * sizeof(CliVersionsSection),
                                                        &g_cli_versions_cells);
    parsed->field = g_cli_cells->persistent_buf_alloc((parsed->fields_capacity + 1ULL) * sizeof(CliVersionsField),
                                                      &g_cli_versions_cells);
    parsed->item = g_cli_cells->persistent_buf_alloc((parsed->items_capacity + 1ULL) * sizeof(CliVersionsItem),
                                                     &g_cli_versions_cells);
    parsed->error = g_cli_cells->persistent_buf_alloc((parsed->errors_capacity + 1ULL) * sizeof(CliVersionsError),
                                                      &g_cli_versions_cells);
    parsed->pool = g_cli_cells->persistent_buf_alloc(parsed->pool_capacity + 1ULL, &g_cli_versions_cells);
    // a key or a string the line contains, and after it a message naming a key
    parsed->line = g_cli_cells->persistent_buf_alloc((2ULL * (parsed->longest + 1ULL)) + CLI_VERSIONS_MSG_MAX_LEN,
                                                     &g_cli_versions_cells);
    if ((parsed->section == NULL) || (parsed->field == NULL) || (parsed->item == NULL) || (parsed->error == NULL) ||
        (parsed->pool == NULL) || (parsed->line == NULL))
    {
        g_cli_cells->persistent_buf_release(g_cli_versions_libraries.text, &g_cli_versions_cells);
        g_cli_cells->persistent_buf_release(g_cli_versions_libraries.at, &g_cli_versions_cells);
        g_cli_versions_libraries.at = NULL;
        g_cli_versions_libraries.size = NULL;
        g_cli_versions_libraries.text = NULL;
        g_cli_versions_libraries.count = 0u;
        g_cli_versions_libraries.most = 0u;
        g_cli_versions_libraries.used = 0u;
        g_cli_versions_libraries.capacity = 0u;
        g_cli_cells->persistent_buf_release(g_cli_versions_known.text, &g_cli_versions_cells);
        g_cli_cells->persistent_buf_release(g_cli_versions_known.at, &g_cli_versions_cells);
        g_cli_versions_known.at = NULL;
        g_cli_versions_known.size = NULL;
        g_cli_versions_known.text = NULL;
        g_cli_versions_known.count = 0u;
        g_cli_versions_known.most = 0u;
        g_cli_versions_known.used = 0u;
        g_cli_versions_known.capacity = 0u;
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
        parsed->sections_capacity = 0u;
        parsed->fields = 0u;
        parsed->fields_capacity = 0u;
        parsed->items = 0u;
        parsed->items_capacity = 0u;
        parsed->errors = 0u;
        parsed->errors_capacity = 0u;
        parsed->used = 0u;
        parsed->pool_capacity = 0u;
        parsed->longest = 0ULL;
        return 0;
    }
    return 1;
}
