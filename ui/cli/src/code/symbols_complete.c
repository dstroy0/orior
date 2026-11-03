// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_complete.c: the names that complete a word in a file of the tree, read off the tree as a walk of the file to
// the word's line meets them: the defines in effect there, the declarations the walk met in an active region, and the
// parameters and locals of the function the word stands in that its grammar names, above the word. A name is known by
// the roles its grammar's tokens name, a region by the brackets it names a region's, a function's by a head that
// declares a function
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// The names being completed: the word typed and how long, the names handed on, each with a zero after, in a line, and
// where each goes
typedef struct
{
    const char *typed;
    unsigned long long length;
    VerbaLinea transferred;
    CliDeclared each;
    void *context;
} CliComplete;
// the bytes the names handed on are written in
#define CLI_COMPLETE_CAPACITY (1ULL << 20u)

typedef struct
{
    CliComplete *complete;
    const char *name;
    unsigned long long length;
    const char *kind;
    unsigned int line;
    const char *path;
    char *kind_out;
    const CliEffect *effect;
    const CliLeafRead *read;
    unsigned int place;
} SymbolsCompleteCtx;
// `length` bytes of `name` handed on as `kind` declared on `line`, where they begin with the word typed and were not
// handed on before
EMBED_INLINE void complete_offer(const SymbolsCompleteCtx *args)
{
    if ((args->length < args->complete->length) ||
        !EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->name,
                    .other_pointer = args->complete->typed, .size = args->complete->length))
    {
        return;
    }
    const int saved = mmgr_line_ok(&args->complete->transferred);
    for (unsigned long long at = 0ULL; saved && (at < args->complete->transferred.at);
         at +=
         cellul.len(&(CatenaFinitaCfg){.src = args->complete->transferred.out + at, .cap = CLI_CELLS_POOL}) + 1ULL)
    {
        const char *const one = args->complete->transferred.out + at;
        if ((cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL}) == args->length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = one, .other_pointer = args->name,
                       .size = args->length))
        {
            return;
        }
    }
    // a name is below 2^32 bytes
    mmgr_line_put_n(&args->complete->transferred, args->name, (size_t)args->length);
    mmgr_line_ch(&args->complete->transferred, '\0');
    EMBED_CALL(args->complete->each, CliDeclaredCfg, .context = args->complete->context, .name = args->name,
               .length = args->length, .kind = args->kind, .line = args->line);
}

// the kind the tree's file at `path` declares `length` bytes of `name` as on its line `line`, as its grammar names it,
// into `kind`, sized for CLI_NAME_MAX_LEN, empty where it declares none there, as for a define the database gives
EMBED_INLINE void complete_kind_declare(const SymbolsCompleteCtx *args)
{
    const char *path = args->path;
    args->kind_out[0] = '\0';
    const unsigned int file = EMBED_CALL(cli_symbols_tree_path.branch_get, CliSymbolsTreePathCfg, .path = path);
    const CliTreeBranch *const branch = (file < CLI_BRANCHES_MAX) ? &g_cli_branches[file] : NULL;
    char *const scratch = ((branch != NULL) && branch->used) ? cli_branch_copy_alloc() : NULL;
    if (scratch != NULL)
    {
        EMBED_CALL(cli_symbols_tree_branch.branch_copy, CliSymbolsTreeBranchCfg, .branch = branch, .scratch = scratch);
    }
    for (unsigned int at = 0u; (scratch != NULL) && (args->kind_out[0] == '\0') && (at < branch->count); at += 1u)
    {
        const CliDeclaration *const one = &EMBED_CALL(cli_symbols_tree_branch_scratch.branch_copy_declarations,
                                                      CliSymbolsTreeBranchScratchCfg, .scratch_read = scratch)[at];
        if ((one->line == args->line) && (one->length == args->length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = scratch + one->name,
                       .other_pointer = args->name, .size = args->length))
        {
            VerbaLinea written = {args->kind_out, CLI_NAME_MAX_LEN, 0ULL};
            mmgr_line_put(&written, scratch + one->kind);
            args->kind_out[(mmgr_line_finish(&written) != 0ULL) ? written.at : 0ULL] = '\0';
        }
    }
    EMBED_CALL(cli_symbols_tree_branch.branch_copy_free, CliSymbolsTreeBranchCfg, .scratch = scratch);
}

// the defines in effect where the walk ended, the database's first, each of the kind its declaration's grammar names,
// then the declarations it met in an active region, each offered in the order met
EMBED_INLINE void complete_walk(const SymbolsCompleteCtx *args)
{
    const CliEffect *effect = args->effect;
    for (unsigned int at = 0u; at < effect->macros; at += 1u)
    {
        const char *const name = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                            .at_value = effect->macro[at].name);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        if (EMBED_CALL(cli_symbols_name_internal.effect_macro, CliSymbolsNameCtx, .effect_read = effect, .name = name,
                       .length = length) == at)
        {
            char kind[CLI_NAME_MAX_LEN];
            EMBED_CALL(complete_kind_declare, SymbolsCompleteCtx,
                       .path = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                          .at_value = effect->macro[at].path),
                       .name = name, .length = length, .line = effect->macro[at].line, .kind_out = kind);
            EMBED_CALL(complete_offer, SymbolsCompleteCtx, .complete = args->complete, .name = name, .length = length,
                       .kind = kind, .line = effect->macro[at].line);
        }
    }
    for (unsigned int at = 0u; at < effect->seens; at += 1u)
    {
        const CliEffectSeen *const one = &effect->seen[at];
        if (one->is == CLI_SEEN_DECLARED)
        {
            EMBED_CALL(complete_offer, SymbolsCompleteCtx, .complete = args->complete,
                       .name = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                          .at_value = one->name),
                       .length = one->length,
                       .kind = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                          .at_value = one->kind),
                       .line = one->line);
        }
    }
}

// the parameters and locals of the function region the leaf `place` of `read` stands in, from its head to the place,
// each offered: a bracket its grammar names a region's opening, at no depth, opens a region, its head the statement
// before it, after the last statement's end or region at no depth or the last directive's line, and a region whose
// head declares a function is a function's
EMBED_INLINE void complete_locals(const SymbolsCompleteCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int depth = 0u;
    unsigned int statement = 0u;
    unsigned int head = 0u;
    unsigned int open = 0u;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int leaf = 0u; leaf < args->place; leaf += 1u)
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            head = (depth == 0u) ? statement : head;
            open = (depth == 0u) ? leaf : open;
            depth += 1u;
            continue;
        }
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_CLOSE) &&
            (depth > 0u))
        {
            depth -= 1u;
            statement = (depth == 0u) ? (leaf + 1u) : statement;
            continue;
        }
        directive_line = ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u) ? read->leaf[leaf].line : directive_line;
        if ((depth == 0u) && ((read->leaf[leaf].line == directive_line) ||
                              EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                         .leaf = leaf, .roles = CLI_LEAF_STATEMENT)))
        {
            statement = leaf + 1u;
        }
    }
    int function = 0;
    {
        unsigned int leaf = head;
        while ((depth > 0u) && !function && (leaf < open))
        {
            function = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                  .leaf = leaf, .roles = CLI_LEAF_DECLARES | CLI_LEAF_FUNCTION);
            leaf += 1u;
        }
    }
    for (unsigned int leaf = head; function && (leaf < args->place); leaf += 1u)
    {
        const unsigned long long is = read->leaf[leaf].is;
        if (((is & (CLI_LEAF_LOCAL | CLI_LEAF_PARAMETER)) != 0u) && ((is & CLI_LEAF_NAME) != 0u))
        {
            unsigned long long length = 0ULL;
            const char *const name = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                .read_read = read, .leaf = leaf, .length_pointer = &length);
            EMBED_CALL(complete_offer, SymbolsCompleteCtx, .complete = args->complete, .name = name, .length = length,
                       .kind = ((is & CLI_LEAF_PARAMETER) != 0u) ? "parameter" : "local",
                       .line = read->leaf[leaf].line);
        }
    }
}

int cli_symbols_complete(const CliSymbolsPathTextSizeCfg *args)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_for_call();
    CliLeafRead read;
    const int leaved = (languages != NULL) && EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx,
                                                         .languages = languages, .path = args->path, .state = "root",
                                                         .text = args->text, .size = args->size, .read = &read);
    // the byte the place stands at: its line's first, then its column's
    unsigned long long start = 0ULL;
    for (unsigned int lines = 0u; (lines < args->line) && (start < args->size); start += 1ULL)
    {
        lines += (args->text[start] == '\n') ? 1u : 0u;
    }
    unsigned long long end = start;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = end,
                     .size = args->size, .set = "\n");
    unsigned long long byte = 0ULL;
    unsigned int units = 0u;
    const unsigned long long at =
        start + EMBED_CALL(cli_symbols_size_internal.byte_at, CliSymbolsSizeCtx, .line = args->text + start,
                           .size = end - start, .col = args->col, .byte = &byte, .at = &units);
    // the word typed: the name the place ends or stands in, from its first byte to the place
    unsigned int place = 0u;
    while (leaved && (place < read.count) && (read.leaf[place].to < at))
    {
        place += 1u;
    }
    const unsigned long long words = CLI_LEAF_COMMENT | CLI_LEAF_STRING;
    const int named = leaved && (place < read.count) && (read.leaf[place].from < at) &&
                      ((read.leaf[place].is & CLI_LEAF_NAME) != 0u) && ((read.leaf[place].is & words) == 0u);
    char *const transferred_cell = g_cli_cells->temporary_buf_alloc(CLI_COMPLETE_CAPACITY, &g_cli_code_cells);
    CliComplete complete = {named ? args->text + read.leaf[place].from : "", named ? at - read.leaf[place].from : 0ULL,
                            mmgr_line_over(transferred_cell, CLI_COMPLETE_CAPACITY), args->each_declared,
                            args->context};
    int done = transferred_cell != NULL;
    CliEffect *const effect = done ? EMBED_CALL(cli_symbols_path_name_internal.effect_init, CliSymbolsPathNameCtx,
                                                .path = args->path, .name = "", .slicing = 1, .err_msg = args->err_msg)
                                   : NULL;
    if (effect != NULL)
    {
        effect->start_text = args->text;
        effect->start_size = args->size;
        EMBED_CALL(cli_symbols_path_effect_internal.effect_alloc, CliSymbolsPathEffectCtx, .effect = effect,
                   .path = args->path, .line_u32 = args->line);
        done = !effect->refused;
        if (done)
        {
            EMBED_CALL(complete_walk, SymbolsCompleteCtx, .complete = &complete, .effect = effect);
        }
        EMBED_CALL(cli_symbols_text_length_internal.effect_input_back, CliSymbolsTextLengthCtx, .effect = effect);
    }
    done = done && (effect != NULL);
    if (done && leaved)
    {
        EMBED_CALL(complete_locals, SymbolsCompleteCtx, .complete = &complete, .read = &read, .place = place);
    }
    if (!done)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the walk to the word");
    }
    if (leaved)
    {
        EMBED_CALL(cli_symbols_text_internal.leaves_free, CliSymbolsTextCtx, .read = &read);
    }
    EMBED_CALL(cli_symbols.languages_free, CliSymbolsCfg, .read = languages);
    g_cli_cells->persistent_buf_release(complete.transferred.out, &g_cli_code_cells);
    return done;
}
