// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_find.c: the index searched: each declaration held scored against a query, the best kept in rank
#include "../text/json_tree.h"
#include "symbols_index.h"

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
static void find_line_written(const char *path, unsigned int line, CliLine *out)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    unsigned long long start = 0ull;
    unsigned int counted = 0u;
    while ((bytes != NULL) && (size <= CLI_SYMBOL_LINE_FILE_MOST) && (counted < line) && (start < size))
    {
        const unsigned char *const newline =
            memor.chr(&(MemoriaCfg){.src = bytes + start, .val = '\n', .bytes = size - start});
        // a newline found lies in the bytes, at or past `start`
        start = (newline != NULL) ? ((unsigned long long)(newline - bytes) + 1ull) : size;
        counted += 1u;
    }
    unsigned long long end = start;
    while ((bytes != NULL) && (counted == line) && (end < size) && (bytes[end] != '\n'))
    {
        end += 1ull;
    }
    while ((end > start) && ((bytes[end - 1ull] == '\r') || (bytes[end - 1ull] == ' ') || (bytes[end - 1ull] == '\t')))
    {
        end -= 1ull;
    }
    while ((start < end) && ((bytes[start] == ' ') || (bytes[start] == '\t')))
    {
        start += 1ull;
    }
    const unsigned long long length = ((end - start) < CLI_SYMBOL_LINE_MOST) ? (end - start) : CLI_SYMBOL_LINE_MOST;
    cli_json_quoted_lossy(out, (bytes != NULL) ? bytes + start : (const unsigned char *)"",
                          (bytes != NULL) ? length : 0ull);
    cli_file_let_go(bytes, size);
}

// whether one found ranks before another: more points, then by name, path, kind and line
static int cli_ranks_before(const CliScored *one, const CliScored *other)
{
    if (one->points != other->points)
    {
        return one->points > other->points;
    }
    const CliHeldFile *const one_file = &g_cli_symbol_files[one->file];
    const CliHeldFile *const other_file = &g_cli_symbol_files[other->file];
    const CliSymbol *const left = &cli_held_symbols(one_file)[one->symbol];
    const CliSymbol *const right = &cli_held_symbols(other_file)[other->symbol];
    const unsigned int shorter = (left->length < right->length) ? left->length : right->length;
    int order = memor.cmp(
        &(MemoriaCfg){.src = one_file->cell + left->name, .other = other_file->cell + right->name, .bytes = shorter});
    order = (order != 0) ? order : ((left->length < right->length) ? -1 : (left->length > right->length) ? 1 : 0);
    order = (order != 0) ? order : cli_text_order(cli_held_path(one_file), cli_held_path(other_file));
    order = (order != 0) ? order : cli_text_order(left->kind, right->kind);
    return (order != 0) ? (order < 0) : (left->line < right->line);
}

void cli_symbols_find(const char *query, unsigned int most, CliLine *out)
{
    static CliScored s_found[CLI_SYMBOLS_FOUND];
    const unsigned int room = (most < CLI_SYMBOLS_FOUND) ? most : CLI_SYMBOLS_FOUND;
    unsigned int count = 0u;
    cli_lock_take(&g_cli_symbols_lock);
    for (unsigned int file = 0u; file < g_cli_symbol_file_count; file += 1u)
    {
        for (unsigned int each = 0u; g_cli_symbol_files[file].used && (each < g_cli_symbol_files[file].count);
             each += 1u)
        {
            const CliSymbol *const symbol = &cli_held_symbols(&g_cli_symbol_files[file])[each];
            CliScored scored = {0ll, file, each};
            if (!cli_symbols_score(query, g_cli_symbol_files[file].cell + symbol->name, symbol->length, &scored.points))
            {
                continue;
            }
            // kept in rank among the best `room` so far
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
    cli_line_text(out, "{\"symbols\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliHeldFile *const held = &g_cli_symbol_files[s_found[at].file];
        const CliSymbol *const symbol = &cli_held_symbols(held)[s_found[at].symbol];
        const char *const path = cli_held_path(held);
        cli_line_text(out, (at != 0u) ? ",{\"name\":" : "{\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)(held->cell + symbol->name), symbol->length);
        cli_line_text(out, ",\"kind\":");
        cli_json_quoted(out, symbol->kind, cellul.len(&(CatenaFinitaCfg){.src = symbol->kind, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"path\":");
        cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"line\":");
        cli_line_decimal(out, symbol->line, 1u);
        cli_line_text(out, ",\"text\":");
        char full[CLI_PATH_LONGEST];
        cli_full(g_cli_symbols_root, path, full);
        find_line_written(full, symbol->line, out);
        cli_line_character(out, '}');
    }
    cli_line_text(out, g_cli_symbols_reading ? "],\"reading\":true}" : "],\"reading\":false}");
    cli_lock_give(&g_cli_symbols_lock);
}
