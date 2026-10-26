// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_hints.c: the names of the parameters a call's arguments are given to, read off the tree by the roles its
// grammar names. A call is a leaf named a call, its arguments in the bracket that opens past it; its definition in
// effect is found as Go to Definition finds it (symbols_usages.c) and its head read there (symbols_heads.c); each
// argument, the leaves between separators at the bracket's depth, is given the name of the parameter at its place,
// where that parameter has a name the argument is not alone
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
static char *hints_held(void *context, const char *path, unsigned long long *size)
{
    const CliHints *const hints = (const CliHints *)context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = path, .other = hints->path, .cap = CLI_CELLS_POOL}))
    {
        return NULL;
    }
    char *const held = g_cli_cells->persistent_buf_alloc(hints->size + 1ull, NULL);
    if (held != NULL)
    {
        // a text the editor holds fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = held, .src = hints->text, .bytes = (size_t)hints->size});
        held[hints->size] = '\0';
        *size = hints->size;
    }
    return held;
}

// the definition of the call found, its file and line kept
static void hints_defined(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                          const char *text, unsigned long long length)
{
    (void)from;
    (void)to;
    (void)text;
    (void)length;
    CliHints *const hints = (CliHints *)context;
    VerbaLinea defined = {hints->defined, sizeof(hints->defined), 0ull};
    mmgr_line_put(&defined, path);
    hints->found = mmgr_line_finish(&defined) != 0ull;
    hints->line = line;
}

// the column of the leaf `leaf` of `read` on its line, in UTF-16 units
static unsigned int hints_col(const CliLeafRead *read, unsigned int leaf)
{
    unsigned long long start = read->leaf[leaf].from;
    while ((start > 0ull) && (read->text[start - 1ull] != '\n'))
    {
        start -= 1ull;
    }
    // a line's units are below 2^32
    return (unsigned int)cli_text_units(read->text + start, read->leaf[leaf].from - start);
}

// the leaf past the blanks from `leaf` of `read` on, `read->count` where none is
static unsigned int hints_next(const CliLeafRead *read, unsigned int leaf)
{
    while ((leaf < read->count) && ((read->leaf[leaf].is & (CLI_LEAF_BLANK | CLI_LEAF_COMMENT)) != 0u))
    {
        leaf += 1u;
    }
    return leaf;
}

// each argument of the call whose opening bracket is the leaf `open` of `read` given the name of its parameter in
// `head`: its first leaf past the blanks handed to `each` with the name, where the parameter has one and the argument
// is not that name alone
static void hints_arguments(const CliLeafRead *read, unsigned int open, const CliSymbolHead *head, CliHinted each,
                            void *context)
{
    const unsigned long long label_size = cellul.len(&(CatenaFinitaCfg){.src = head->label, .cap = CLI_CELLS_POOL});
    unsigned int depth = 0u;
    unsigned int argument = 0u;
    unsigned int first = hints_next(read, open + 1u);
    for (unsigned int leaf = open + 1u; leaf < read->count; leaf += 1u)
    {
        const int opens = cli_leaf_has(read, leaf, CLI_LEAF_OPEN);
        const int closes = cli_leaf_has(read, leaf, CLI_LEAF_CLOSE);
        if (opens || (closes && (depth > 0u)))
        {
            depth = opens ? (depth + 1u) : (depth - 1u);
            continue;
        }
        const int ends = closes || ((depth == 0u) && cli_leaf_has(read, leaf, CLI_LEAF_SEPARATOR));
        if (!ends)
        {
            continue;
        }
        const unsigned long long name_from = cli_text_byte_of(head->label, label_size, head->named[argument][0]);
        const unsigned long long name_to = cli_text_byte_of(head->label, label_size, head->named[argument][1]);
        const unsigned int last = hints_next(read, first + 1u);
        unsigned long long length = 0ull;
        const char *const text = (first < leaf) ? cli_leaf_text(read, first, &length) : "";
        const int alone = (last >= leaf) && (length == (name_to - name_from)) &&
                          cli_text_same(text, head->label + name_from, length);
        if ((argument < head->params) && (first < leaf) && (name_to > name_from) && !alone)
        {
            each(context, read->leaf[first].line, hints_col(read, first), head->label + name_from, name_to - name_from);
        }
        if (closes)
        {
            return;
        }
        argument += 1u;
        first = hints_next(read, leaf + 1u);
    }
}

int cli_symbols_hints(const char *path, const char *text, unsigned long long size, unsigned int from, unsigned int to,
                      CliHinted each, void *context, VerbaLinea *err_msg)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    CliLeafRead read;
    const int leaved = (languages != NULL) && cli_symbols_leaves_kept(languages, path, "root", text, size, &read);
    char root[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_symbols_lock);
    memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_symbols_root, .bytes = sizeof(root)});
    cli_lock_give(&g_cli_symbols_lock);
    CliHints hints = {path, text, size, "", 0u, 0};
    int done = 1;
    for (unsigned int leaf = 0u; done && leaved && (leaf < read.count) && (read.leaf[leaf].line <= to); leaf += 1u)
    {
        const unsigned int open = hints_next(&read, leaf + 1u);
        if ((read.leaf[leaf].line < from) || !cli_leaf_has(&read, leaf, CLI_LEAF_NAME | CLI_LEAF_CALL) ||
            (open == read.count) || !cli_leaf_has(&read, open, CLI_LEAF_OPEN))
        {
            continue;
        }
        hints.found = 0;
        done = cli_symbols_definition(path, read.leaf[leaf].line, hints_col(&read, leaf), hints_held, &hints,
                                      hints_defined, &hints, err_msg) >= 0;
        unsigned long long length = 0ull;
        const char *const name = cli_leaf_text(&read, leaf, &length);
        CliSymbolHead head;
        VerbaLinea head_path = {head.path, sizeof(head.path), 0ull};
        mmgr_line_put(&head_path, hints.defined);
        head.line = hints.line;
        if (done && hints.found && (mmgr_line_finish(&head_path) != 0ull) &&
            cli_symbols_head_read(languages, root, name, length, &head))
        {
            hints_arguments(&read, open, &head, each, context);
        }
    }
    if (leaved)
    {
        cli_symbols_leaves_given(&read);
    }
    cli_symbols_languages_given(languages);
    return done;
}
