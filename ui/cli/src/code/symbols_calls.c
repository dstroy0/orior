// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_calls.c: the call hierarchy read off the tree's leaves, by the roles their grammar names. A function is a
// region a bracket its grammar names a region's opening opens at no depth, whose head, the statement before it, holds
// the leaf that declares a function; a call is a leaf its grammar names a call. A function's callers are found from its
// name's usages (symbols_usages.c), each call grouped under the function of its file it stands in; its callees from the
// calls in its body, each grouped under its definition in effect
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// the bytes the regions, the calls and the usages are each begun in, doubling as they fill
#define CLI_CALLS_FIRST 4096u

// One function of a file's leaves: its head's first leaf, the bracket that opens its region, the leaf after the one
// that closes it, and the leaf that declares its name
typedef struct
{
    unsigned int head;
    unsigned int open;
    unsigned int end;
    unsigned int name;
} CliCallsFunction;

// A file read for its calls: its text, `held`'s or the disk's mapped, its size and which, its leaves, and its functions
// in a growing line, how many
typedef struct
{
    char *text;
    unsigned long long size;
    int mapped;
    CliSymbolsLanguages *languages;
    CliLeafRead read;
    int leaved;
    VerbaLinea functions;
    unsigned int count;
} CliCallsFile;

// One call found: the file and line of its definition in effect, or of the function it stands in, a place among the
// paths, its name's span and where the call stands
typedef struct
{
    unsigned long long path;
    unsigned int line;
    unsigned int from;
    unsigned int to;
    CliSymbolPlace site;
} CliCallFound;

// the column of the leaf `leaf` of `read` on its line, in UTF-16 units
static unsigned int calls_col(const CliLeafRead *read, unsigned int leaf)
{
    unsigned long long start = read->leaf[leaf].from;
    while ((start > 0ull) && (read->text[start - 1ull] != '\n'))
    {
        start -= 1ull;
    }
    // a line's units are below 2^32
    return (unsigned int)cli_text_units(read->text + start, read->leaf[leaf].from - start);
}

// whether the leaf `leaf` of `read` is a call: a name its grammar names a call
static int calls_is_call(const CliLeafRead *read, unsigned int leaf)
{
    return cli_leaf_has(read, leaf, CLI_LEAF_NAME | CLI_LEAF_CALL);
}

// the function at `at` of the file's
static CliCallsFunction calls_function_at(const CliCallsFile *file, unsigned int at)
{
    CliCallsFunction one;
    memor.cpy(&(MemoriaCfg){
        .dst = &one, .src = file->functions.out + ((size_t)at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// the functions of the file's leaves read: each region its grammar's opening bracket opens at no depth whose head holds
// the leaf that declares a function, its head after the last statement's end or region at no depth or the last
// directive's line
static void calls_functions_read(CliCallsFile *file)
{
    const CliLeafRead *const read = &file->read;
    unsigned int depth = 0u;
    unsigned int statement = 0u;
    unsigned int open = 0u;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int leaf = 0u; leaf < read->count; leaf += 1u)
    {
        if (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            open = (depth == 0u) ? leaf : open;
            depth += 1u;
            continue;
        }
        if (cli_leaf_has(read, leaf, CLI_LEAF_REGION | CLI_LEAF_CLOSE) && (depth > 0u))
        {
            depth -= 1u;
            if (depth > 0u)
            {
                continue;
            }
            unsigned int name = CLI_JSON_NONE;
            for (unsigned int head = statement; (name == CLI_JSON_NONE) && (head < open); head += 1u)
            {
                name = cli_leaf_has(read, head, CLI_LEAF_DECLARES | CLI_LEAF_FUNCTION) ? head : CLI_JSON_NONE;
            }
            const CliCallsFunction one = {statement, open, leaf + 1u, name};
            if (name != CLI_JSON_NONE)
            {
                mmgr_line_put_n(&file->functions, (const char *)&one, sizeof(one));
                file->count += 1u;
            }
            statement = leaf + 1u;
            continue;
        }
        directive_line = ((read->leaf[leaf].is & CLI_LEAF_DIRECTIVE) != 0u) ? read->leaf[leaf].line : directive_line;
        if ((depth == 0u) &&
            ((read->leaf[leaf].line == directive_line) || cli_leaf_has(read, leaf, CLI_LEAF_STATEMENT)))
        {
            statement = leaf + 1u;
        }
    }
}

// the tree's file at `path` read, its text `held`'s else the disk's, its leaves and its functions: 1, 0 where it has no
// text or its language is none read here, or -1 where the cells cannot meet it
static int calls_file_read(CliCallsFile *file, const char *path, CliHeld held, void *held_context)
{
    *file = (CliCallsFile){NULL, 0ull, 0, NULL, {NULL, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL}, 0,
                           {NULL, 0ull, 0ull, NULL, NULL, 0ull}, 0u};
    char *const given = (held != NULL) ? held(held_context, path, &file->size) : NULL;
    file->mapped = given == NULL;
    file->text = given;
    if (given == NULL)
    {
        char full[CLI_PATH_MAX_LEN];
        cli_lock_take(&g_cli_symbols_lock);
        cli_full(g_cli_symbols_root, path, full);
        cli_lock_give(&g_cli_symbols_lock);
        // a text read mapped is read and never written
        file->text = (char *)cli_file_read(full, &file->size);
    }
    if (file->text == NULL)
    {
        return 0;
    }
    file->languages = cli_symbols_languages_read();
    file->leaved = (file->languages != NULL) &&
                   cli_symbols_leaves_kept(file->languages, path, "root", file->text, file->size, &file->read);
    if (!file->leaved)
    {
        return 0;
    }
    if (!cli_growing_line_made(&file->functions, CLI_CALLS_FIRST))
    {
        return -1;
    }
    calls_functions_read(file);
    return mmgr_line_ok(&file->functions) ? 1 : -1;
}

// a file read given back
static void calls_file_given(CliCallsFile *file)
{
    g_cli_cells->persistent_buf_release(file->functions.out, NULL);
    if (file->leaved)
    {
        cli_symbols_leaves_given(&file->read);
    }
    cli_symbols_languages_given(file->languages);
    if (file->mapped && (file->text != NULL))
    {
        cli_file_let_go((const unsigned char *)file->text, file->size);
    }
    else
    {
        g_cli_cells->persistent_buf_release(file->text, NULL);
    }
}

// the place among the file's functions of the one whose name stands on `line`, else where `body` is 1 whose head or
// body does: CLI_JSON_NONE where none does
static unsigned int calls_function_on(const CliCallsFile *file, unsigned int line, int body)
{
    for (unsigned int at = 0u; at < file->count; at += 1u)
    {
        const CliCallsFunction one = calls_function_at(file, at);
        if (file->read.leaf[one.name].line == line)
        {
            return at;
        }
    }
    for (unsigned int at = 0u; body && (at < file->count); at += 1u)
    {
        const CliCallsFunction one = calls_function_at(file, at);
        if ((file->read.leaf[one.head].line <= line) && (file->read.leaf[one.end - 1u].line >= line))
        {
            return at;
        }
    }
    return CLI_JSON_NONE;
}

// the function `one` of the file at `path` handed to `each` with the calls `sites`, `count` of them
static void calls_handed(const CliCallsFile *file, const char *path, CliCallsFunction one, const CliSymbolPlace *sites,
                         unsigned int count, CliCallReached each, void *context)
{
    unsigned long long length = 0ull;
    const char *const name = cli_leaf_text(&file->read, one.name, &length);
    const unsigned int from = calls_col(&file->read, one.name);
    const unsigned int width = (unsigned int)cli_text_units(name, length);
    each(context, path, file->read.leaf[one.name].line, from, from + width, name, length, sites, count);
}

int cli_symbols_function_at(const char *path, unsigned int line, CliHeld held, void *held_context, CliCallReached each,
                            void *context, VerbaLinea *err_msg)
{
    CliCallsFile file;
    int found = calls_file_read(&file, path, held, held_context);
    const unsigned int at = (found == 1) ? calls_function_on(&file, line, 1) : CLI_JSON_NONE;
    found = ((found == 1) && (at == CLI_JSON_NONE)) ? 0 : found;
    if (found == 1)
    {
        calls_handed(&file, path, calls_function_at(&file, at), NULL, 0u, each, context);
    }
    if (found < 0)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the file's functions");
    }
    calls_file_given(&file);
    return found;
}

// The calls found: each one, its paths with a zero after each, and the file the usages are read in
typedef struct
{
    VerbaLinea found;
    VerbaLinea paths;
    unsigned int count;
} CliCallsFound;

// the call at `at` of those found
static CliCallFound calls_found_at(const CliCallsFound *calls, unsigned int at)
{
    CliCallFound one;
    memor.cpy(&(MemoriaCfg){.dst = &one, .src = calls->found.out + ((size_t)at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// a call put after those found: the file and span its group is keyed by, and where it stands
static void calls_found_put(CliCallsFound *calls, const char *path, unsigned int line, unsigned int from,
                            unsigned int to, CliSymbolPlace site)
{
    const CliCallFound one = {calls->paths.at, line, from, to, site};
    mmgr_line_put(&calls->paths, path);
    mmgr_line_ch(&calls->paths, '\0');
    mmgr_line_put_n(&calls->found, (const char *)&one, sizeof(one));
    calls->count += 1u;
}

// a usage of the function's name put among those found as its path, its line and its span
static void calls_usage_put(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                            const char *text, unsigned long long length)
{
    (void)text;
    (void)length;
    calls_found_put((CliCallsFound *)context, path, line, from, to, (CliSymbolPlace){line, from});
}

// the calls found begun, 1, or 0 where the cells cannot meet them; and given back
static int calls_found_begun(CliCallsFound *calls)
{
    *calls = (CliCallsFound){{NULL, 0ull, 0ull, NULL, NULL, 0ull}, {NULL, 0ull, 0ull, NULL, NULL, 0ull}, 0u};
    return cli_growing_line_made(&calls->found, CLI_CALLS_FIRST) &&
           cli_growing_line_made(&calls->paths, CLI_CALLS_FIRST);
}

static void calls_found_given(CliCallsFound *calls)
{
    g_cli_cells->persistent_buf_release(calls->found.out, NULL);
    g_cli_cells->persistent_buf_release(calls->paths.out, NULL);
}

// the leaf of `file` at `line`, `col` in UTF-16 units, `file->read.count` where none starts there
static unsigned int calls_leaf_at(const CliCallsFile *file, unsigned int line, unsigned int col)
{
    for (unsigned int leaf = 0u; leaf < file->read.count; leaf += 1u)
    {
        if ((file->read.leaf[leaf].line == line) && (calls_col(&file->read, leaf) == col))
        {
            return leaf;
        }
    }
    return file->read.count;
}

// the usages of one file among those found from `first` to before `end`, each a call grouped under the function it
// stands in, each function handed to `each` with its calls' places
static int calls_callers_of(const CliCallsFound *usages, unsigned int first, unsigned int end, CliHeld held,
                            void *held_context, CliCallReached each, void *context)
{
    const char *const path = usages->paths.out + calls_found_at(usages, first).path;
    CliCallsFile file;
    const int read = calls_file_read(&file, path, held, held_context);
    CliSymbolPlace sites[CLI_SYMBOLS_PARAMS];
    unsigned int count = 0u;
    unsigned int current = CLI_JSON_NONE;
    for (unsigned int at = first; (read == 1) && (at <= end); at += 1u)
    {
        unsigned int function = CLI_JSON_NONE;
        CliSymbolPlace site = {0u, 0u};
        if (at < end)
        {
            site = calls_found_at(usages, at).site;
            const unsigned int leaf = calls_leaf_at(&file, site.line, site.col);
            function = ((leaf < file.read.count) && calls_is_call(&file.read, leaf))
                           ? calls_function_on(&file, site.line, 1)
                           : CLI_JSON_NONE;
            if (function == CLI_JSON_NONE)
            {
                continue;
            }
        }
        if ((current != CLI_JSON_NONE) && ((function != current) || (count == CLI_SYMBOLS_PARAMS)))
        {
            calls_handed(&file, path, calls_function_at(&file, current), sites, count, each, context);
            count = 0u;
        }
        current = function;
        if (at < end)
        {
            sites[count] = site;
            count += 1u;
        }
    }
    calls_file_given(&file);
    return read >= 0;
}

int cli_symbols_callers(const char *path, unsigned int line, CliHeld held, void *held_context, CliCallReached each,
                        void *context, VerbaLinea *err_msg)
{
    CliCallsFile file;
    int found = calls_file_read(&file, path, held, held_context);
    const unsigned int at = (found == 1) ? calls_function_on(&file, line, 0) : CLI_JSON_NONE;
    found = ((found == 1) && (at == CLI_JSON_NONE)) ? 0 : found;
    CliCallsFound usages;
    if ((found == 1) && !calls_found_begun(&usages))
    {
        found = -1;
    }
    if (found == 1)
    {
        const CliCallsFunction one = calls_function_at(&file, at);
        const unsigned int name_line = file.read.leaf[one.name].line;
        const unsigned int name_col = calls_col(&file.read, one.name);
        found = (cli_symbols_usages(path, name_line, name_col, 1, held, held_context, calls_usage_put, &usages,
                                    err_msg) < 0)
                    ? -1
                    : 1;
        // the usages of each file stand together, in the order of their lines
        unsigned int first = 0u;
        for (unsigned int next = 1u; (found == 1) && mmgr_line_ok(&usages.found) && (next <= usages.count);
             next += 1u)
        {
            const int same = (next < usages.count) &&
                             cellul.eq(&(CatenaFinitaCfg){.src = usages.paths.out + calls_found_at(&usages, next).path,
                                                          .other = usages.paths.out + calls_found_at(&usages, first).path,
                                                          .cap = CLI_CELLS_POOL});
            if (!same)
            {
                found = calls_callers_of(&usages, first, next, held, held_context, each, context) ? 1 : -1;
                first = next;
            }
        }
        calls_found_given(&usages);
    }
    if (found < 0)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the function's callers");
    }
    calls_file_given(&file);
    return found;
}

// The definition of a call found as Go to Definition finds it: the calls found, and where the call stands
typedef struct
{
    CliCallsFound *calls;
    CliSymbolPlace site;
} CliCallsDefined;

// a call's definition in effect put among the calls found, keyed by its file and line
static void calls_defined_put(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                              const char *text, unsigned long long length)
{
    (void)text;
    (void)length;
    const CliCallsDefined *const defined = (const CliCallsDefined *)context;
    calls_found_put(defined->calls, path, line, from, to, defined->site);
}

// the calls found handed to `each` grouped by their definitions, each definition once with its calls' places, in the
// order each was first called
static void calls_callees_handed(const CliCallsFound *calls, const CliCallsFile *file, CliCallReached each,
                                 void *context)
{
    for (unsigned int at = 0u; at < calls->count; at += 1u)
    {
        const CliCallFound one = calls_found_at(calls, at);
        const char *const path = calls->paths.out + one.path;
        int earlier = 0;
        CliSymbolPlace sites[CLI_SYMBOLS_PARAMS];
        unsigned int count = 0u;
        for (unsigned int other = 0u; other < calls->count; other += 1u)
        {
            const CliCallFound same = calls_found_at(calls, other);
            const int matches = (same.line == one.line) &&
                                cellul.eq(&(CatenaFinitaCfg){
                                    .src = calls->paths.out + same.path, .other = path, .cap = CLI_CELLS_POOL});
            earlier |= matches && (other < at);
            if (matches && (count < CLI_SYMBOLS_PARAMS))
            {
                sites[count] = same.site;
                count += 1u;
            }
        }
        if (!earlier)
        {
            unsigned long long length = 0ull;
            const unsigned int leaf = calls_leaf_at(file, one.site.line, one.site.col);
            const char *const name = (leaf < file->read.count) ? cli_leaf_text(&file->read, leaf, &length) : "";
            each(context, path, one.line, one.from, one.to, name, length, sites, count);
        }
    }
}

int cli_symbols_callees(const char *path, unsigned int line, CliHeld held, void *held_context, CliCallReached each,
                        void *context, VerbaLinea *err_msg)
{
    CliCallsFile file;
    int found = calls_file_read(&file, path, held, held_context);
    const unsigned int at = (found == 1) ? calls_function_on(&file, line, 0) : CLI_JSON_NONE;
    found = ((found == 1) && (at == CLI_JSON_NONE)) ? 0 : found;
    CliCallsFound calls;
    if ((found == 1) && !calls_found_begun(&calls))
    {
        found = -1;
    }
    if (found == 1)
    {
        const CliCallsFunction one = calls_function_at(&file, at);
        for (unsigned int leaf = one.open; (found == 1) && (leaf < one.end); leaf += 1u)
        {
            if (!calls_is_call(&file.read, leaf))
            {
                continue;
            }
            CliCallsDefined defined = {&calls, {file.read.leaf[leaf].line, calls_col(&file.read, leaf)}};
            found = (cli_symbols_definition(path, defined.site.line, defined.site.col, held, held_context,
                                            calls_defined_put, &defined, err_msg) < 0)
                        ? -1
                        : 1;
        }
        if ((found == 1) && mmgr_line_ok(&calls.found) && mmgr_line_ok(&calls.paths))
        {
            calls_callees_handed(&calls, &file, each, context);
        }
        calls_found_given(&calls);
    }
    if (found < 0)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the function's callees");
    }
    calls_file_given(&file);
    return found;
}
