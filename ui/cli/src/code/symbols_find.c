// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_find.c: the tree searched: each declaration stored scored against a query, the best retained in rank
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "symbols_tree.h"

#include "../text/text.h"

// the most declarations a search hands
#define CLI_SYMBOLS_FOUND 1024u

// One declaration found: its points, its file and its place among its file's declarations
typedef struct
{
    long long points;
    unsigned int file;
    unsigned int symbol;
} CliScored;

// the most bytes of a declaration's line written with it, and the most bytes of a file read for that line
#define CLI_SYMBOL_LINE_MOST 240ull
#define CLI_SYMBOL_LINE_FILE_MOST (1ull << 20u)

// the declaration's own line, `line` counted from 0 in the file at `path`, written as a JSON string with the whitespace
// at either end left off, cut to CLI_SYMBOL_LINE_MOST bytes: an empty string where the file is not read or has no
// such line
static void find_line_written(const char *path, unsigned int line, VerbaLinea *out)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_taken(path, &size, &g_cli_code_cells);
    if (bytes == NULL)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)"", 0ull);
        return;
    }
    unsigned long long start = 0ull;
    unsigned int counted = 0u;
    while ((size <= CLI_SYMBOL_LINE_FILE_MOST) && (counted < line) && (start < size))
    {
        start = cli_text_line_at((const char *)bytes, start, size).next;
        counted += 1u;
    }
    unsigned long long end = start;
    if (counted == line)
    {
        end = cli_text_to_any((const char *)bytes, end, size, "\n");
    }
    end = (start + cli_text_back_past((const char *)(bytes + start), end - start, "\r \t"));
    start = cli_text_past_any((const char *)bytes, start, end, " \t");
    const unsigned long long length = ((end - start) < CLI_SYMBOL_LINE_MOST) ? (end - start) : CLI_SYMBOL_LINE_MOST;
    cli_json_quoted_lossy(out, bytes + start, length);
    cli_file_given(bytes, &g_cli_code_cells);
}

// one declaration of a stored file, read out of its cell
static CliSymbol find_symbol_read(const CliSymbolFile *branch, unsigned int symbol)
{
    CliSymbol read = {0u, 0u, 0u, 0u};
    (void)g_cli_cells->movable_buf_read(&(CarcerMovableCfg){
        .cell = branch->cell, .offset = (size_t)symbol * sizeof(CliSymbol), .dst = &read, .bytes = sizeof(read)});
    return read;
}

// two runs of bytes in two stored files' cells ordered by their first difference over the shorter
static int find_bytes_order(const CliSymbolFile *one, unsigned int one_at, const CliSymbolFile *other,
                            unsigned int other_at, unsigned int bytes)
{
    const embed_iword order = g_cli_cells->movable_buf_compare(&(CarcerMovableCfg){
        .cell = one->cell, .other_cell = other->cell, .offset = one_at, .other_offset = other_at, .bytes = bytes});
    return (order > 0) - (order < 0);
}

// two texts in two stored files' cells ordered: a text with its terminator differs from another there at the latest
static int find_text_order(const CliSymbolFile *one, unsigned int one_at, const CliSymbolFile *other,
                           unsigned int other_at)
{
    const size_t measured = g_cli_cells->movable_buf_measure(&(CarcerMovableCfg){.cell = one->cell, .offset = one_at});
    const size_t theirs = g_cli_cells->movable_buf_measure(&(CarcerMovableCfg){.cell = other->cell, .offset = other_at});
    const size_t shorter = (measured < theirs) ? measured : theirs;
    // a stored text is below what an unsigned int counts
    const int order = find_bytes_order(one, one_at, other, other_at, (unsigned int)shorter);
    return (order != 0) ? order : ((measured < theirs) ? -1 : ((measured > theirs) ? 1 : 0));
}

// whether one found ranks before another: more points, then by name, path, kind and line
static int cli_ranks_before(const CliScored *one, const CliScored *other)
{
    if (one->points != other->points)
    {
        return one->points > other->points;
    }
    const CliSymbolFile *const one_file = &g_cli_symbol_files[one->file];
    const CliSymbolFile *const other_file = &g_cli_symbol_files[other->file];
    const CliSymbol left = find_symbol_read(one_file, one->symbol);
    const CliSymbol right = find_symbol_read(other_file, other->symbol);
    const unsigned int shorter = (left.length < right.length) ? left.length : right.length;
    int order = find_bytes_order(one_file, left.name, other_file, right.name, shorter);
    order = (order != 0) ? order : ((left.length < right.length) ? -1 : (left.length > right.length) ? 1 : 0);
    order = (order != 0) ? order : find_text_order(one_file, one_file->path, other_file, other_file->path);
    order = (order != 0) ? order : find_text_order(one_file, left.kind, other_file, right.kind);
    return (order != 0) ? (order < 0) : (left.line < right.line);
}

void cli_symbols_find(const char *query, unsigned int most, VerbaLinea *out)
{
    static CliScored s_found[CLI_SYMBOLS_FOUND];
    const unsigned int room = (most < CLI_SYMBOLS_FOUND) ? most : CLI_SYMBOLS_FOUND;
    unsigned int count = 0u;
    cli_lock_take(&g_cli_symbols_lock);
    char *const scratch = cli_symbol_scratch_taken();
    for (unsigned int file = 0u; (scratch != NULL) && (file < g_cli_symbol_file_count); file += 1u)
    {
        if (g_cli_symbol_files[file].used)
        {
            cli_symbol_file_read(&g_cli_symbol_files[file], scratch);
        }
        for (unsigned int each = 0u; g_cli_symbol_files[file].used && (each < g_cli_symbol_files[file].count);
             each += 1u)
        {
            const CliSymbol *const symbol = &cli_symbol_file_symbols(scratch)[each];
            CliScored scored = {0ll, file, each};
            if (!cli_symbols_score(query, scratch + symbol->name, symbol->length, &scored.points))
            {
                continue;
            }
            // retained in rank among the best `room` so far
            unsigned int place = (count < room) ? count : room;
            while ((place > 0u) && cli_ranks_before(&scored, &s_found[place - 1u]))
            {
                if (place < room)
                {
                    s_found[place] = s_found[place - 1u];
                }
                place -= 1u;
            }
            if (place < room)
            {
                s_found[place] = scored;
                count += (count < room) ? 1u : 0u;
            }
        }
    }
    mmgr_line_put(out, "{\"symbols\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliSymbolFile *const branch = &g_cli_symbol_files[s_found[at].file];
        cli_symbol_file_read(branch, scratch);
        const CliSymbol *const symbol = &cli_symbol_file_symbols(scratch)[s_found[at].symbol];
        const char *const path = cli_symbol_file_path(branch, scratch);
        mmgr_line_put(out, (at != 0u) ? ",{\"name\":" : "{\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)(scratch + symbol->name), symbol->length);
        mmgr_line_put(out, ",\"kind\":");
        const char *const kind = scratch + symbol->kind;
        cli_json_quoted(out, kind, cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"path\":");
        cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"line\":");
        mmgr_line_decimal(out, symbol->line, 1u);
        mmgr_line_put(out, ",\"text\":");
        char full[CLI_PATH_MAX_LEN];
        cli_full(g_cli_symbols_root, path, full);
        find_line_written(full, symbol->line, out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, g_cli_symbols_reading ? "],\"reading\":true}" : "],\"reading\":false}");
    cli_symbol_scratch_given(scratch);
    cli_lock_give(&g_cli_symbols_lock);
}
