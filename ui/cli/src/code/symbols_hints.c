// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_hints.c: the names of the parameters a call's arguments are given to, read off the tree by the roles its
// grammar names. A call is a leaf named a call, its arguments in the bracket that opens past it; its definition in
// effect is found as Go to Definition finds it (symbols_usages.c) and its head read there (symbols_heads.c); each
// argument, the leaves between separators at the bracket's depth, is given the name of the parameter at its place,
// where that parameter has a name the argument is not alone
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/edits.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// The text the hints are read from: the tree's file it is and its bytes; and the definition of the call met last, its
// file's path and its line, and whether one was found
typedef struct
{
    const char *path;
    const char *text;
    unsigned long long size;
    char defined[CLI_PATH_MAX_LEN];
    unsigned int line;
    int found;
} CliHints;

// the text of the tree's file at `path`, the one the hints are read from where it is that file, in a cell the caller
// gives back: NULL where it is another
static char *hints_open_text(const CliOpenTextCfg *args)
{
    unsigned long long *size = args->size;
    const CliHints *const hints = (const CliHints *)args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->path, .other = hints->path, .cap = CLI_CELLS_POOL}))
    {
        return NULL;
    }
    char *const open_text = g_cli_cells->persistent_buf_alloc(hints->size + 1ULL, &g_cli_code_cells);
    if (open_text != NULL)
    {
        // a text the editor holds fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = open_text, .src = hints->text, .bytes = (size_t)hints->size});
        open_text[hints->size] = '\0';
        *size = hints->size;
    }
    return open_text;
}

// the definition of the call found, its file and line saved
static void hints_define(const CliUsageCfg *args)
{
    unsigned int line = args->line;
    CliHints *const hints = (CliHints *)args->context;
    VerbaLinea defined = {hints->defined, sizeof(hints->defined), 0ULL};
    mmgr_line_put(&defined, args->path);
    hints->found = mmgr_line_finish(&defined) != 0ULL;
    hints->line = line;
}

typedef struct
{
    const CliLeafRead *read;
    unsigned int leaf;
    unsigned int open;
    const CliSymbolHead *head;
    CliHinted each;
    void *context;
} SymbolsHintsCtx;
// the column of the leaf `leaf` of `read` on its line, in UTF-16 units
EMBED_INLINE unsigned int hints_col(const SymbolsHintsCtx *args)
{
    unsigned long long start = args->read->leaf[args->leaf].from;
    start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->read->text,
                       .at = start, .set = "\n");
    // a line's units are below 2^32
    return (unsigned int)EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = args->read->text + start,
                                    .size = args->read->leaf[args->leaf].from - start);
}

// the leaf past the blanks from `leaf` of `read` on, `read->count` where none is
EMBED_INLINE unsigned int hints_next(const SymbolsHintsCtx *args)
{
    unsigned int leaf = args->leaf;
    while ((leaf < args->read->count) && ((args->read->leaf[leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        leaf += 1u;
    }
    return leaf;
}

// each argument of the call whose opening bracket is the leaf `open` of `read` given the name of its parameter in
// `head`: its first leaf past the blanks handed to `each` with the name, where the parameter has one and the argument
// is not that name alone
EMBED_INLINE void hints_arguments(const SymbolsHintsCtx *args)
{
    const CliLeafRead *read = args->read;
    const unsigned long long label_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->head->label, .cap = CLI_CELLS_POOL});
    unsigned int depth = 0u;
    unsigned int argument = 0u;
    unsigned int first = EMBED_CALL(hints_next, SymbolsHintsCtx, .read = read, .leaf = args->open + 1u);
    for (unsigned int leaf = args->open + 1u; leaf < read->count; leaf += 1u)
    {
        const int opens = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                     .leaf = leaf, .roles = CLI_LEAF_OPEN);
        const int closes = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                      .leaf = leaf, .roles = CLI_LEAF_CLOSE);
        if (opens || (closes && (depth > 0u)))
        {
            depth = opens ? (depth + 1u) : (depth - 1u);
            continue;
        }
        const int ends =
            closes || ((depth == 0u) && EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx,
                                                   .read_read = read, .leaf = leaf, .roles = CLI_LEAF_SEPARATOR));
        if (!ends)
        {
            continue;
        }
        const unsigned long long name_from =
            EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = args->head->label, .size = label_size,
                       .col = args->head->named[argument][0]);
        const unsigned long long name_to =
            EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = args->head->label, .size = label_size,
                       .col = args->head->named[argument][1]);
        const unsigned int last = EMBED_CALL(hints_next, SymbolsHintsCtx, .read = read, .leaf = first + 1u);
        unsigned long long length = 0ULL;
        const char *const text = (first < leaf)
                                     ? EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                                  .read_read = read, .leaf = first, .length_pointer = &length)
                                     : "";
        const int alone = (last >= leaf) && (length == (name_to - name_from)) &&
                          EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = text,
                                     .other_pointer = args->head->label + name_from, .size = length);
        if ((argument < args->head->params) && (first < leaf) && (name_to > name_from) && !alone)
        {
            EMBED_CALL(args->each, CliHintedCfg, .context = args->context, .line = read->leaf[first].line,
                       .col = EMBED_CALL(hints_col, SymbolsHintsCtx, .read = read, .leaf = first),
                       .name = args->head->label + name_from, .length = name_to - name_from);
        }
        if (closes)
        {
            return;
        }
        argument += 1u;
        first = EMBED_CALL(hints_next, SymbolsHintsCtx, .read = read, .leaf = leaf + 1u);
    }
}

int cli_symbols_hints(const CliSymbolsPathTextSizeContextCfg *args)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_for_call();
    CliLeafRead read;
    const int leaved = (languages != NULL) && EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx,
                                                         .languages = languages, .path = args->path, .state = "root",
                                                         .text = args->text, .size = args->size, .read = &read);
    char root[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_tree_root, .bytes = sizeof(root)});
    CliHints hints = {args->path, args->text, args->size, "", 0u, 0};
    int done = 1;
    for (unsigned int leaf = 0u; done && leaved && (leaf < read.count) && (read.leaf[leaf].line <= args->to);
         leaf += 1u)
    {
        const unsigned int open = EMBED_CALL(hints_next, SymbolsHintsCtx, .read = &read, .leaf = leaf + 1u);
        if ((read.leaf[leaf].line < args->from) ||
            !EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = &read, .leaf = leaf,
                        .roles = CLI_LEAF_NAME | CLI_LEAF_CALL) ||
            (open == read.count) ||
            !EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = &read, .leaf = open,
                        .roles = CLI_LEAF_OPEN))
        {
            continue;
        }
        hints.found = 0;
        done = EMBED_CALL(cli_symbols_path_context.definition, CliSymbolsPathContextCfg, .path = args->path,
                          .line = read.leaf[leaf].line,
                          .col = EMBED_CALL(hints_col, SymbolsHintsCtx, .read = &read, .leaf = leaf),
                          .open_text = hints_open_text, .open_text_context = &hints, .each_usage = hints_define,
                          .context = &hints, .err_msg = args->err_msg) >= 0;
        unsigned long long length = 0ULL;
        const char *const name = EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                            .read_read = &read, .leaf = leaf, .length_pointer = &length);
        CliSymbolHead head;
        VerbaLinea head_path = {head.path, sizeof(head.path), 0ULL};
        mmgr_line_put(&head_path, hints.defined);
        head.line = hints.line;
        if (done && hints.found && (mmgr_line_finish(&head_path) != 0ULL) &&
            EMBED_CALL(cli_symbols_languages_internal.head_read, CliSymbolsLanguagesCtx, .languages = languages,
                       .root = root, .name = name, .length = length, .head = &head))
        {
            EMBED_CALL(hints_arguments, SymbolsHintsCtx, .read = &read, .open = open, .head = &head,
                       .each = args->each_hinted, .context = args->context);
        }
    }
    if (leaved)
    {
        g_cli_cells->persistent_buf_release(read.leaf, &g_cli_leaves_cells);
        read.leaf = NULL;
        read.count = 0u;
        read.capacity = 0u;
    }
    if (languages != NULL)
    {
        g_cli_cells->persistent_buf_release(languages->languages.pair, &g_cli_plugins_cells);
        *(&languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_cells->persistent_buf_release(languages, &g_cli_code_cells);
    return done;
}
