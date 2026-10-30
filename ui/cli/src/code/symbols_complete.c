// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_complete.c: the names that complete a word in a file of the tree, read off the tree as a walk of the file to
// the word's line meets them: the defines in effect there, the declarations the walk met in an active region, and the
// parameters and locals of the function the word stands in that its grammar names, above the word. A name is known by
// the roles its grammar's tokens name, a region by the brackets it names a region's, a function's by a head that
// declares a function
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// the bytes the names handed on are begun in, doubling as they fill
#define CLI_COMPLETE_FIRST 4096u

// The names being completed: the word typed and how long, the names handed on, each with a zero after, in a growing
// line, and where each goes
typedef struct
{
    const char *typed;
    unsigned long long length;
    VerbaLinea handed;
    CliDeclared each;
    void *context;
} CliComplete;

// `length` bytes of `name` handed on as `kind` declared on `line`, where they begin with the word typed and were not
// handed on before
static void complete_offered(CliComplete *complete, const char *name, unsigned long long length, const char *kind,
                             unsigned int line)
{
    if ((length < complete->length) || !cli_text_same(name, complete->typed, complete->length))
    {
        return;
    }
    const int kept = mmgr_line_ok(&complete->handed);
    for (unsigned long long at = 0ull; kept && (at < complete->handed.at);
         at += cellul.len(&(CatenaFinitaCfg){.src = complete->handed.out + at, .cap = CLI_CELLS_POOL}) + 1ull)
    {
        const char *const one = complete->handed.out + at;
        if ((cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL}) == length) &&
            cli_text_same(one, name, length))
        {
            return;
        }
    }
    // a name is below 2^32 bytes
    mmgr_line_put_n(&complete->handed, name, (size_t)length);
    mmgr_line_ch(&complete->handed, '\0');
    complete->each(complete->context, name, length, kind, line);
}

// the kind the tree's file at `path` declares `length` bytes of `name` as on its line `line`, as its grammar names it,
// into `kind`, sized for CLI_NAME_MAX_LEN, empty where it declares none there, as for a define the database gives
static void complete_kind_declared(const char *path, const char *name, unsigned long long length, unsigned int line,
                                   char *kind)
{
    kind[0] = '\0';
    cli_lock_take(&g_cli_symbols_lock);
    const unsigned int file = cli_symbols_file_of(path);
    const CliSymbolFile *const branch = (file < CLI_SYMBOL_FILES) ? &g_cli_symbol_files[file] : NULL;
    for (unsigned int at = 0u; (branch != NULL) && branch->used && (kind[0] == '\0') && (at < branch->count); at += 1u)
    {
        const CliSymbol *const one = &cli_symbol_file_symbols(branch)[at];
        if ((one->line == line) && (one->length == length) && cli_text_same(branch->cell + one->name, name, length))
        {
            VerbaLinea written = {kind, CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put(&written, branch->cell + one->kind);
            kind[(mmgr_line_finish(&written) != 0ull) ? written.at : 0ull] = '\0';
        }
    }
    cli_lock_give(&g_cli_symbols_lock);
}

// the defines in effect where the walk ended, the database's first, each of the kind its declaration's grammar names,
// then the declarations it met in an active region, each offered in the order met
static void complete_walked(CliComplete *complete, const CliEffect *effect)
{
    for (unsigned int at = 0u; at < effect->macros; at += 1u)
    {
        const char *const name = cli_effect_text(effect, effect->macro[at].name);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        if (cli_effect_macro(effect, name, length) == at)
        {
            char kind[CLI_NAME_MAX_LEN];
            complete_kind_declared(cli_effect_text(effect, effect->macro[at].path), name, length,
                                   effect->macro[at].line, kind);
            complete_offered(complete, name, length, kind, effect->macro[at].line);
        }
    }
    for (unsigned int at = 0u; at < effect->seens; at += 1u)
    {
        const CliEffectSeen *const one = &effect->seen[at];
        if (one->is == CLI_SEEN_DECLARED)
        {
            complete_offered(complete, cli_effect_text(effect, one->name), one->length,
                             cli_effect_text(effect, one->kind), one->line);
        }
    }
}

// the parameters and locals of the function region the leaf `place` of `read` stands in, from its head to the place,
// each offered: a bracket its grammar names a region's opening, at no depth, opens a region, its head the statement
// before it, after the last statement's end or region at no depth or the last directive's line, and a region whose
// head declares a function is a function's
static void complete_locals(CliComplete *complete, const CliLeafRead *read, unsigned int place)
{
    unsigned int depth = 0u;
    unsigned int statement = 0u;
    unsigned int head = 0u;
    unsigned int open = 0u;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int leaf = 0u; leaf < place; leaf += 1u)
    {
        if (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            head = (depth == 0u) ? statement : head;
            open = (depth == 0u) ? leaf : open;
            depth += 1u;
            continue;
        }
        if (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_CLOSE) && (depth > 0u))
        {
            depth -= 1u;
            statement = (depth == 0u) ? (leaf + 1u) : statement;
            continue;
        }
        directive_line = ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u) ? read->leaf[leaf].line : directive_line;
        if ((depth == 0u) &&
            ((read->leaf[leaf].line == directive_line) || cli_leaf_has(read, leaf, CLI_LEAF_STATEMENT)))
        {
            statement = leaf + 1u;
        }
    }
    int function = 0;
    for (unsigned int leaf = head; (depth > 0u) && !function && (leaf < open); leaf += 1u)
    {
        function = cli_leaf_has(read, leaf, CLI_LEAF_DECLARES | CLI_LEAF_FUNCTION);
    }
    for (unsigned int leaf = head; function && (leaf < place); leaf += 1u)
    {
        const unsigned long long is = read->leaf[leaf].is;
        if (((is & (CLI_LEAF_LOCAL | CLI_LEAF_PARAMETER)) != 0u) && ((is & CLI_LEAF_NAME) != 0u))
        {
            unsigned long long length = 0ull;
            const char *const name = cli_leaf_text(read, leaf, &length);
            complete_offered(complete, name, length, ((is & CLI_LEAF_PARAMETER) != 0u) ? "parameter" : "local",
                             read->leaf[leaf].line);
        }
    }
}

int cli_symbols_complete(const char *path, const char *text, unsigned long long size, unsigned int line,
                         unsigned int col, CliDeclared each, void *context, VerbaLinea *err_msg)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    CliLeafRead read;
    const int leaved = (languages != NULL) && cli_symbols_leaves_kept(languages, path, "root", text, size, &read);
    // the byte the place stands at: its line's first, then its column's
    unsigned long long start = 0ull;
    for (unsigned int lines = 0u; (lines < line) && (start < size); start += 1ull)
    {
        lines += (text[start] == '\n') ? 1u : 0u;
    }
    unsigned long long end = start;
    while ((end < size) && (text[end] != '\n'))
    {
        end += 1ull;
    }
    unsigned long long byte = 0ull;
    unsigned int units = 0u;
    const unsigned long long at = start + cli_symbols_byte_at(text + start, end - start, col, &byte, &units);
    // the word typed: the name the place ends or stands in, from its first byte to the place
    unsigned int place = 0u;
    while (leaved && (place < read.count) && (read.leaf[place].to < at))
    {
        place += 1u;
    }
    const unsigned long long words = CLI_LEAF_COMMENT | CLI_LEAF_STRING;
    const int named = leaved && (place < read.count) && (read.leaf[place].from < at) &&
                      ((read.leaf[place].is & CLI_LEAF_NAME) != 0u) && ((read.leaf[place].is & words) == 0u);
    CliComplete complete = {named ? text + read.leaf[place].from : "",
                            named ? at - read.leaf[place].from : 0ull,
                            {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                            each,
                            context};
    int done = cli_growing_line_made(&complete.handed, CLI_COMPLETE_FIRST);
    CliEffect *const effect = done ? cli_effect_made(path, "", 1, err_msg) : NULL;
    if (effect != NULL)
    {
        effect->start_text = text;
        effect->start_size = size;
        cli_effect_taken(effect, path, line);
        done = !effect->refused;
        if (done)
        {
            complete_walked(&complete, effect);
        }
        cli_effect_given_back(effect);
    }
    done = done && (effect != NULL);
    if (done && leaved)
    {
        complete_locals(&complete, &read, place);
    }
    if (!done)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the walk to the word");
    }
    if (leaved)
    {
        cli_symbols_leaves_given(&read);
    }
    cli_symbols_languages_given(languages);
    cli_cell_release(complete.handed.out, NULL);
    return done;
}
