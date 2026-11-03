// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_find.c: the tree searched: each declaration stored scored against a query, the best retained in rank
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "symbols_tree.h"

#include "../text/text.h"

// One declaration found: its points, its file and its place among its file's declarations
typedef struct
{
    long long points;
    unsigned int file;
    unsigned int symbol;
} CliScored;
// the most declarations a search hands
#define CLI_SYMBOLS_FOUND 1024u

// the most bytes of a declaration's line written with it, and the most bytes of a file read for that line
#define CLI_SYMBOL_LINE_MAX 240ULL
#define CLI_SYMBOL_LINE_FILE_MAX (1ULL << 20u)

typedef struct
{
    const char *path;
    unsigned int line;
    VerbaLinea *out;
    const CliTreeBranch *branch;
    unsigned int symbol;
    const CliTreeBranch *one;
    unsigned int one_at;
    const CliTreeBranch *other;
    unsigned int other_at;
    unsigned int bytes;
    const CliScored *one_scored_pointer;
    const CliScored *other_scored_pointer;
} SymbolsFindCtx;
// the declaration's own line, `line` counted from 0 in the file at `path`, written with the whitespace at either end
// left off, cut to CLI_SYMBOL_LINE_MAX bytes: nothing where the file is not read or has no such line
EMBED_INLINE void find_line_write(const SymbolsFindCtx *args)
{
    const char *path = args->path;
    VerbaLinea *out = args->out;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_code_cells);
    if (bytes == NULL)
    {
        return;
    }
    unsigned long long start = 0ULL;
    unsigned int counted = 0u;
    while ((size <= CLI_SYMBOL_LINE_FILE_MAX) && (counted < args->line) && (start < size))
    {
        start =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)bytes, .at = start, .size = size).next;
        counted += 1u;
    }
    unsigned long long end = start;
    if (counted == args->line)
    {
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                         .at = end, .size = size, .set = "\n");
    }
    end = (start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                              .text = (const char *)(bytes + start), .at = end - start, .set = "\r \t"));
    start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = (const char *)bytes,
                       .at = start, .size = end, .set = " \t");
    const unsigned long long length = ((end - start) < CLI_SYMBOL_LINE_MAX) ? (end - start) : CLI_SYMBOL_LINE_MAX;
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = out, .text_bytes = bytes + start,
               .length = length);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_code_cells);
}

// one declaration of a stored file, read out of its cell
EMBED_INLINE CliDeclaration find_symbol_read(const SymbolsFindCtx *args)
{
    CliDeclaration read = {0u, 0u, 0u, 0u};
    (void)g_cli_cells->movable_buf_read(&(CarcerMovableCfg){.cell = args->branch->cell,
                                                            .offset = (size_t)args->symbol * sizeof(CliDeclaration),
                                                            .dst = &read,
                                                            .bytes = sizeof(read)});
    return read;
}

// two runs of bytes in two stored files' cells ordered by their first difference over the shorter
EMBED_INLINE int find_bytes_order(const SymbolsFindCtx *args)
{
    unsigned int bytes = args->bytes;
    const embed_iword order = g_cli_cells->movable_buf_compare(&(CarcerMovableCfg){.cell = args->one->cell,
                                                                                   .other_cell = args->other->cell,
                                                                                   .offset = args->one_at,
                                                                                   .other_offset = args->other_at,
                                                                                   .bytes = bytes});
    return (order > 0) - (order < 0);
}

// two texts in two stored files' cells ordered: a text with its terminator differs from another there at the latest
EMBED_INLINE int find_text_order(const SymbolsFindCtx *args)
{
    const size_t measured =
        g_cli_cells->movable_buf_measure(&(CarcerMovableCfg){.cell = args->one->cell, .offset = args->one_at});
    const size_t theirs =
        g_cli_cells->movable_buf_measure(&(CarcerMovableCfg){.cell = args->other->cell, .offset = args->other_at});
    const size_t shorter = (measured < theirs) ? measured : theirs;
    // a stored text is below what an unsigned int counts
    const int order = EMBED_CALL(find_bytes_order, SymbolsFindCtx, .one = args->one, .one_at = args->one_at,
                                 .other = args->other, .other_at = args->other_at, .bytes = (unsigned int)shorter);
    if (order != 0)
    {
        return order;
    }
    if (measured < theirs)
    {
        return -1;
    }
    if (measured > theirs)
    {
        return 1;
    }
    return 0;
}

// whether one found ranks before another: more points, then by name, path, kind and line
EMBED_INLINE int cli_ranks_before(const SymbolsFindCtx *args)
{
    if (args->one_scored_pointer->points != args->other_scored_pointer->points)
    {
        return args->one_scored_pointer->points > args->other_scored_pointer->points;
    }
    const CliTreeBranch *const one_file = &g_cli_branches[args->one_scored_pointer->file];
    const CliTreeBranch *const other_file = &g_cli_branches[args->other_scored_pointer->file];
    const CliDeclaration left =
        EMBED_CALL(find_symbol_read, SymbolsFindCtx, .branch = one_file, .symbol = args->one_scored_pointer->symbol);
    const CliDeclaration right = EMBED_CALL(find_symbol_read, SymbolsFindCtx, .branch = other_file,
                                            .symbol = args->other_scored_pointer->symbol);
    const unsigned int shorter = (left.length < right.length) ? left.length : right.length;
    int order = EMBED_CALL(find_bytes_order, SymbolsFindCtx, .one = one_file, .one_at = left.name, .other = other_file,
                           .other_at = right.name, .bytes = shorter);
    if (order == 0)
    {
        if (left.length < right.length)
        {
            order = -1;
        }
        else if (left.length > right.length)
        {
            order = 1;
        }
        else
        {
            order = 0;
        }
    }
    order = (order != 0) ? order
                         : EMBED_CALL(find_text_order, SymbolsFindCtx, .one = one_file, .one_at = one_file->path,
                                      .other = other_file, .other_at = other_file->path);
    order = (order != 0) ? order
                         : EMBED_CALL(find_text_order, SymbolsFindCtx, .one = one_file, .one_at = left.kind,
                                      .other = other_file, .other_at = right.kind);
    return (order != 0) ? (order < 0) : (left.line < right.line);
}

CliFoundDeclarations *cli_tree_declarations_alloc(const CliSymbolsQueryMaxCfg *args)
{
    static CliScored s_found[CLI_SYMBOLS_FOUND];
    const unsigned int capacity = (args->most < CLI_SYMBOLS_FOUND) ? args->most : CLI_SYMBOLS_FOUND;
    unsigned int count = 0u;
    char *const scratch = cli_branch_copy_alloc();
    for (unsigned int file = 0u; (scratch != NULL) && (file < g_cli_branch_count); file += 1u)
    {
        if (g_cli_branches[file].used)
        {
            EMBED_CALL(cli_symbols_tree_branch.branch_copy, CliSymbolsTreeBranchCfg, .branch = &g_cli_branches[file],
                       .scratch = scratch);
        }
        for (unsigned int each = 0u; g_cli_branches[file].used && (each < g_cli_branches[file].count); each += 1u)
        {
            const CliDeclaration *const symbol =
                &EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations, CliSymbolsTreeBranchScratchCfg,
                            .scratch_read = scratch)[each];
            CliScored scored = {0LL, file, each};
            if (!EMBED_CALL(cli_symbols_query.score, CliSymbolsQueryCfg, .query = args->query,
                            .name = scratch + symbol->name, .name_length = symbol->length, .points = &scored.points))
            {
                continue;
            }
            // retained in rank among the best `capacity` so far
            unsigned int place = (count < capacity) ? count : capacity;
            while ((place > 0u) && EMBED_CALL(cli_ranks_before, SymbolsFindCtx, .one_scored_pointer = &scored,
                                              .other_scored_pointer = &s_found[place - 1u]))
            {
                if (place < capacity)
                {
                    s_found[place] = s_found[place - 1u];
                }
                place -= 1u;
            }
            if (place < capacity)
            {
                s_found[place] = scored;
                count += (count < capacity) ? 1u : 0u;
            }
        }
    }
    // each found's name, kind, path and line of text in one line laid after the list, a line of text no longer than its
    // most
    const unsigned long long text_capacity =
        (count * ((3ULL * CLI_PATH_MAX_LEN) + (4ULL * CLI_SYMBOL_LINE_MAX))) + 1ULL;
    CliFoundDeclarations *const declarations = g_cli_cells->persistent_buf_alloc(
        sizeof(*declarations) + (count * sizeof(CliFoundDeclaration)) + text_capacity, args->owner);
    if (declarations != NULL)
    {
        declarations->count = count;
        declarations->reading = g_cli_tree_walking;
        declarations->found = (CliFoundDeclaration *)(void *)(declarations + 1);
        VerbaLinea text = {(char *)(void *)(declarations->found + count), text_capacity, 0ULL};
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const CliTreeBranch *const branch = &g_cli_branches[s_found[at].file];
            EMBED_CALL(cli_symbols_tree_branch.branch_copy, CliSymbolsTreeBranchCfg, .branch = branch,
                       .scratch = scratch);
            const CliDeclaration *const symbol =
                &EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations, CliSymbolsTreeBranchScratchCfg,
                            .branch = branch, .scratch_read = scratch)[s_found[at].symbol];
            const char *const path =
                EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_path, CliSymbolsTreeBranchScratchCfg,
                           .branch = branch, .scratch_read = scratch);
            CliFoundDeclaration *const found = &declarations->found[at];
            found->line = symbol->line;
            found->name = text.out + text.at;
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &text,
                       .text_bytes = (const unsigned char *)(scratch + symbol->name), .length = symbol->length);
            mmgr_line_ch(&text, '\0');
            found->kind = text.out + text.at;
            mmgr_line_put(&text, scratch + symbol->kind);
            mmgr_line_ch(&text, '\0');
            found->path = text.out + text.at;
            mmgr_line_put(&text, path);
            mmgr_line_ch(&text, '\0');
            found->text = text.out + text.at;
            char full[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = g_cli_tree_root, .relative = path, .joined = full);
            EMBED_CALL(find_line_write, SymbolsFindCtx, .path = full, .line = symbol->line, .out = &text);
            mmgr_line_ch(&text, '\0');
        }
    }
    EMBED_CALL(cli_symbols_tree_branch.branch_copy_free, CliSymbolsTreeBranchCfg, .scratch = scratch);
    return declarations;
}
