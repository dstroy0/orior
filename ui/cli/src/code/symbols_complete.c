// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_complete.c: the names that complete a word in a file of the tree. The tree's branch for the file holds the
// names its last walk or save joined, each with what it is. The line under edit gives the rest: its own names, read
// off that line alone. No call reads the text past that line or walks the file again
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// The names to complete: the word typed and its length, then the names passed on. A line keeps those names, each with
// a zero after, and the record says where each goes
typedef struct
{
    const char *typed;
    unsigned long long length;
    CliGrownLine transferred;
    CliDeclared each;
    void *context;
} CliComplete;
// the max bytes the names passed on grow to
#define CLI_COMPLETE_CAPACITY (1ULL << 20u)

typedef struct
{
    CliComplete *complete;
    const char *name;
    unsigned long long length;
    const char *type;
    unsigned int line;
    const char *path;
    const CliLeafRead *read;
    unsigned int place;
} SymbolsCompleteCtx;

// `length` bytes of `name` passed on as `type` declared on `line`, where they begin with the word typed and no earlier
// call passed them on
EMBED_INLINE void complete_offer(const SymbolsCompleteCtx *args)
{
    if ((args->length < args->complete->length) ||
        !EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->name,
                    .other_pointer = args->complete->typed, .size = args->complete->length))
    {
        return;
    }
    const int saved = mmgr_line_ok(&args->complete->transferred.line);
    for (unsigned long long at = 0ULL; saved && (at < cli_grown_size(&args->complete->transferred));
         at +=
         cellul.len(&(CatenaFinitaCfg){.src = args->complete->transferred.cell + at, .cap = CLI_POOL_BYTES}) + 1ULL)
    {
        const char *const one = args->complete->transferred.cell + at;
        if ((cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_POOL_BYTES}) == args->length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = one, .other_pointer = args->name,
                       .size = args->length))
        {
            return;
        }
    }
    // a name is below 2^32 bytes
    mmgr_line_put_n(&args->complete->transferred.line, args->name, (embed_word)args->length);
    mmgr_line_ch(&args->complete->transferred.line, '\0');
    EMBED_CALL(args->complete->each, CliDeclaredCfg, .context = args->complete->context, .name = args->name,
               .length = args->length, .type = args->type, .line = args->line);
}

// Each name the tree's branch for the file at `path` keeps, offered with what it is and its line. The call copies the
// branch's cell once into a cell of its own. A file without a branch in the tree offers no names
EMBED_INLINE void complete_branch(const SymbolsCompleteCtx *args)
{
    const unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = args->path);
    CliTreeBranch branch = {0};
    if (file < CLI_BRANCHES_MAX)
    {
        EMBED_CALL(cli_symbols_tree_at.branch_read, CliSymbolsTreeAtCfg, .file = file, .branch_out = &branch);
    }
    char *const scratch = ((file < CLI_BRANCHES_MAX) && branch.used) ? cli_branch_copy_alloc() : NULL;
    if (scratch == NULL)
    {
        return;
    }
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = branch.cell, .dst = scratch, .bytes = branch.bytes});
    const CliDeclaration *const declared = EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations,
                                                      CliSymbolsTreeBranchScratchCfg, .scratch_read = scratch);
    for (unsigned int at = 0u; at < branch.count; at += 1u)
    {
        EMBED_CALL(complete_offer, SymbolsCompleteCtx, .complete = args->complete, .name = scratch + declared[at].name,
                   .length = declared[at].length, .type = scratch + declared[at].type, .line = declared[at].line);
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_code_owner);
}

// The names on the line under edit, `read` its leaves, offered on that line, the word typed at the leaf `place` left
// out. A name the branch holds already goes on once
EMBED_INLINE void complete_line(const SymbolsCompleteCtx *args)
{
    const CliLeafRead *read = args->read;
    const unsigned long long words = CLI_LEAF_COMMENT | CLI_LEAF_STRING;
    for (unsigned int leaf = 0u; leaf < read->count; leaf += 1u)
    {
        const unsigned long long is = read->leaf[leaf].is;
        if ((leaf == args->place) || ((is & CLI_LEAF_NAME) == 0u) || ((is & words) != 0u))
        {
            continue;
        }
        unsigned long long length = 0ULL;
        const char *const name = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                            .read_read = read, .leaf = leaf, .length_pointer = &length);
        EMBED_CALL(complete_offer, SymbolsCompleteCtx, .complete = args->complete, .name = name, .length = length,
                   .type = ((is & CLI_LEAF_PARAMETER) != 0u) ? "parameter"
                           : ((is & CLI_LEAF_LOCAL) != 0u)   ? "local"
                                                             : "",
                   .line = args->line);
    }
}

int cli_symbols_complete(const CliSymbolsPathTextSizeCfg *args)
{
    // the line under edit: its first byte, then the byte after its last
    unsigned long long start = 0ULL;
    for (unsigned int lines = 0u; (lines < args->line) && (start < args->size); start += 1ULL)
    {
        lines += (args->text[start] == '\n') ? 1u : 0u;
    }
    const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                              .text = args->text, .at = start, .size = args->size, .set = "\n");
    // that line's leaves alone, read from the grammar's root, each at its bytes in the line
    CliLeafRead read;
    const int leaved = EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx, .path = args->path,
                                  .state = "root", .text = args->text + start, .size = end - start, .read = &read);
    unsigned long long byte = 0ULL;
    unsigned int units = 0u;
    const unsigned long long at =
        EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = args->text + start,
                   .size = end - start, .col = args->col, .byte = &byte, .at = &units);
    // the word typed: the name the place ends or is in, from its first byte to the place
    unsigned int place = 0u;
    while (leaved && (place < read.count) && (read.leaf[place].to < at))
    {
        place += 1u;
    }
    const unsigned long long words = CLI_LEAF_COMMENT | CLI_LEAF_STRING;
    const int named = leaved && (place < read.count) && (read.leaf[place].from < at) &&
                      ((read.leaf[place].is & CLI_LEAF_NAME) != 0u) && ((read.leaf[place].is & words) == 0u);
    CliComplete complete = {named ? args->text + start + read.leaf[place].from : "",
                            named ? at - read.leaf[place].from : 0ULL,
                            {NULL, 0ULL, 0ULL, NULL, {NULL, 0ULL, 0ULL, NULL, NULL, 0ULL}},
                            args->each_declared,
                            args->context};
    cli_grown_begin(&complete.transferred, CLI_COMPLETE_CAPACITY, g_cli_code_owner);
    const int done = complete.transferred.cell != NULL;
    if (done)
    {
        EMBED_CALL(complete_branch, SymbolsCompleteCtx, .complete = &complete, .path = args->path);
    }
    if (done && leaved)
    {
        EMBED_CALL(complete_line, SymbolsCompleteCtx, .complete = &complete, .read = &read,
                   .place = named ? place : CLI_JSON_NONE, .line = args->line);
    }
    if (!done)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the names for the word");
    }
    if (leaved)
    {
        g_cli_guard->persistent_buf_release(read.leaf, g_cli_leaves_owner);
    }
    cli_grown_end(&complete.transferred);
    return done;
}
