// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_calls.c: the call hierarchy read off the tree's leaves, by the roles their grammar names. A function is a
// region a bracket its grammar names a region's opening opens at no depth, whose head, the statement before it, holds
// the leaf that declares a function; a call is a leaf its grammar names a call. A function's callers are found from its
// name's usages (symbols_usages.c), each call grouped under the function of its file it stands in; its callees from the
// calls in its body, each grouped under its definition in effect
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// One function of a file's leaves: its head's first leaf, the bracket that opens its region, the leaf after the one
// that closes it, and the leaf that declares its name
typedef struct
{
    unsigned int head;
    unsigned int open;
    unsigned int end;
    unsigned int name;
} CliCallsFunction;
// A file read for its calls: its text, `open_text`'s or the disk's mapped, its size and which, its leaves, and its
// functions in a line, how many
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
// The calls found: each one, its paths with a zero after each, and the file the usages are read in
typedef struct
{
    VerbaLinea found;
    VerbaLinea paths;
    unsigned int count;
} CliCallsFound;
// the bytes the regions, the calls and the usages are each written in
#define CLI_CALLS_CAPACITY (4ULL << 20u)

// One call found: the file and line of its definition in effect, or of the function it stands in, a place among the
// paths, its name's span and where the call stands
typedef struct
{
    unsigned long long path;
    unsigned int line;
    unsigned int from;
    unsigned int to;
    CliSymbolPosition site;
} CliCallFound;

typedef struct
{
    const CliLeafRead *read;
    unsigned int leaf;
    const CliCallsFile *file;
    unsigned int at;
    CliCallsFile *file_out;
    const char *path;
    CliOpenText open_text;
    void *open_text_context;
    unsigned int line;
    int body;
    CliCallsFunction one;
    const CliSymbolPosition *sites;
    unsigned int count;
    CliCallReached each;
    void *context;
    const CliCallsFound *calls_read;
    CliCallsFound *calls;
    unsigned int from;
    unsigned int to;
    CliSymbolPosition site;
    unsigned int col;
    const CliCallsFound *usages;
    unsigned int first;
    unsigned int end;
} SymbolsCallsCtx;
// the column of the leaf `leaf` of `read` on its line, in UTF-16 units
EMBED_INLINE unsigned int calls_col(const SymbolsCallsCtx *args)
{
    unsigned long long start = args->read->leaf[args->leaf].from;
    start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->read->text,
                       .at = start, .set = "\n");
    // a line's units are below 2^32
    return (unsigned int)EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = args->read->text + start,
                                    .size = args->read->leaf[args->leaf].from - start);
}

// whether the leaf `leaf` of `read` is a call: a name its grammar names a call
EMBED_INLINE int calls_is_call(const SymbolsCallsCtx *args)
{
    const CliLeafRead *read = args->read;
    unsigned int leaf = args->leaf;
    return EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                      .roles = CLI_LEAF_NAME | CLI_LEAF_CALL);
}

// the function at `at` of the file's
EMBED_INLINE CliCallsFunction calls_function_at(const SymbolsCallsCtx *args)
{
    CliCallsFunction one;
    memor.cpy(&(MemoriaCfg){
        .dst = &one, .src = args->file->functions.out + ((size_t)args->at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// the functions of the file's leaves read: each region its grammar's opening bracket opens at no depth whose head holds
// the leaf that declares a function, its head after the last statement's end or region at no depth or the last
// directive's line
EMBED_INLINE void calls_functions_read(const SymbolsCallsCtx *args)
{
    const CliLeafRead *const read = &args->file_out->read;
    unsigned int depth = 0u;
    unsigned int statement = 0u;
    unsigned int open = 0u;
    unsigned int directive_line = CLI_SLICE_NOWHERE;
    for (unsigned int leaf = 0u; leaf < read->count; leaf += 1u)
    {
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_OPEN))
        {
            open = (depth == 0u) ? leaf : open;
            depth += 1u;
            continue;
        }
        if (EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read, .leaf = leaf,
                       .roles = CLI_LEAF_REGION | CLI_LEAF_CLOSE) &&
            (depth > 0u))
        {
            depth -= 1u;
            if (depth > 0u)
            {
                continue;
            }
            unsigned int name = CLI_JSON_NONE;
            unsigned int head = statement;
            while ((name == CLI_JSON_NONE) && (head < open))
            {
                name = EMBED_CALL(cli_symbols_read_internal.leaf_has, CliSymbolsReadCtx, .read_read = read,
                                  .leaf = head, .roles = CLI_LEAF_DECLARES | CLI_LEAF_FUNCTION)
                           ? head
                           : CLI_JSON_NONE;
                head += 1u;
            }
            const CliCallsFunction one = {statement, open, leaf + 1u, name};
            if (name != CLI_JSON_NONE)
            {
                mmgr_line_put_n(&args->file_out->functions, (const char *)&one, sizeof(one));
                args->file_out->count += 1u;
            }
            statement = leaf + 1u;
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
}

// the tree's file at `path` read, its text `open_text`'s else the disk's, its leaves and its functions: 1, 0 where it
// has no text or its language is none read here, or -1 where the cells cannot meet it
static int calls_file_read(const SymbolsCallsCtx *args)
{
    CliCallsFile *file = args->file_out;
    const char *path = args->path;
    *file = (CliCallsFile){
        NULL, 0ULL, 0, NULL, {NULL, NULL, 0u, 0u, 0u, NULL, 0, NULL, NULL}, 0, {NULL, 0ULL, 0ULL, NULL, NULL, 0ULL},
        0u};
    char *const supplied = (args->open_text != NULL)
                               ? EMBED_CALL(args->open_text, CliOpenTextCfg, .context = args->open_text_context,
                                            .path = path, .size = &file->size)
                               : NULL;
    file->mapped = supplied == NULL;
    file->text = supplied;
    if (supplied == NULL)
    {
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = g_cli_tree_root, .relative = path, .joined = full);
        // a text read mapped is read and never written
        file->text = (char *)EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = full,
                                        .size_pointer = &file->size, .owner = g_cli_code_owner);
    }
    if (file->text == NULL)
    {
        return 0;
    }
    file->languages = cli_symbols_languages_read();
    file->leaved = (file->languages != NULL) && EMBED_CALL(cli_symbols_path_internal.leaves_save, CliSymbolsPathCtx,
                                                           .languages = file->languages, .path = path, .state = "root",
                                                           .text = file->text, .size = file->size, .read = &file->read);
    if (!file->leaved)
    {
        return 0;
    }
    char *const functions_cell = g_cli_guard->temporary_buf_alloc(CLI_CALLS_CAPACITY, g_cli_code_owner);
    file->functions = mmgr_line_over(functions_cell, CLI_CALLS_CAPACITY);
    if (functions_cell == NULL)
    {
        return -1;
    }
    EMBED_CALL(calls_functions_read, SymbolsCallsCtx, .file_out = file);
    return mmgr_line_ok(&file->functions) ? 1 : -1;
}

// the place among the file's functions of the one whose name stands on `line`, else where `body` is 1 whose head or
// body does: CLI_JSON_NONE where none does
EMBED_INLINE unsigned int calls_function_on(const SymbolsCallsCtx *args)
{
    for (unsigned int at = 0u; at < args->file->count; at += 1u)
    {
        const CliCallsFunction one = EMBED_CALL(calls_function_at, SymbolsCallsCtx, .file = args->file, .at = at);
        if (args->file->read.leaf[one.name].line == args->line)
        {
            return at;
        }
    }
    for (unsigned int at = 0u; args->body && (at < args->file->count); at += 1u)
    {
        const CliCallsFunction one = EMBED_CALL(calls_function_at, SymbolsCallsCtx, .file = args->file, .at = at);
        if ((args->file->read.leaf[one.head].line <= args->line) &&
            (args->file->read.leaf[one.end - 1u].line >= args->line))
        {
            return at;
        }
    }
    return CLI_JSON_NONE;
}

// the function `one` of the file at `path` handed to `each` with the calls `sites`, `count` of them
EMBED_INLINE void calls_dispatch(const SymbolsCallsCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const name =
        EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx, .read_read = &args->file->read,
                   .leaf = args->one.name, .length_pointer = &length);
    const unsigned int from = EMBED_CALL(calls_col, SymbolsCallsCtx, .read = &args->file->read, .leaf = args->one.name);
    const unsigned int width =
        (unsigned int)EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg, .text = name, .size = length);
    EMBED_CALL(args->each, CliCallReachedCfg, .context = args->context, .path = args->path,
               .line = args->file->read.leaf[args->one.name].line, .from = from, .to = from + width, .name = name,
               .length = length, .sites = args->sites, .count = args->count);
}

int cli_symbols_function_at(const CliSymbolsPathContextLineCfg *args)
{
    CliCallsFile file;
    int found = EMBED_CALL(calls_file_read, SymbolsCallsCtx, .file_out = &file, .path = args->path,
                           .open_text = args->open_text, .open_text_context = args->open_text_context);
    const unsigned int at =
        (found == 1) ? EMBED_CALL(calls_function_on, SymbolsCallsCtx, .file = &file, .line = args->line, .body = 1)
                     : CLI_JSON_NONE;
    found = ((found == 1) && (at == CLI_JSON_NONE)) ? 0 : found;
    if (found == 1)
    {
        EMBED_CALL(calls_dispatch, SymbolsCallsCtx, .file = &file, .path = args->path,
                   .one = EMBED_CALL(calls_function_at, SymbolsCallsCtx, .file = &file, .at = at), .sites = NULL,
                   .count = 0u, .each = args->each, .context = args->context);
    }
    if (found < 0)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the file's functions");
    }
    g_cli_guard->persistent_buf_release(file.functions.out, g_cli_code_owner);
    if (file.leaved)
    {
        g_cli_guard->persistent_buf_release(file.read.leaf, g_cli_leaves_owner);
        file.read.leaf = NULL;
        file.read.count = 0u;
        file.read.capacity = 0u;
    }
    if (file.languages != NULL)
    {
        g_cli_guard->persistent_buf_release(file.languages->languages.pair, g_cli_plugins_owner);
        *(&file.languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(file.languages, g_cli_code_owner);
    if (file.mapped && (file.text != NULL))
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)file.text), (g_cli_code_owner));
    }
    else
    {
        g_cli_guard->persistent_buf_release(file.text, g_cli_code_owner);
    }
    return found;
}

// the call at `at` of those found
EMBED_INLINE CliCallFound calls_found_at(const SymbolsCallsCtx *args)
{
    CliCallFound one;
    memor.cpy(&(MemoriaCfg){
        .dst = &one, .src = args->calls_read->found.out + ((size_t)args->at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// a call put after those found: the file and span its group is keyed by, and where it stands
EMBED_INLINE void calls_found_append(const SymbolsCallsCtx *args)
{
    const CliCallFound one = {args->calls->paths.at, args->line, args->from, args->to, args->site};
    mmgr_line_put(&args->calls->paths, args->path);
    mmgr_line_ch(&args->calls->paths, '\0');
    mmgr_line_put_n(&args->calls->found, (const char *)&one, sizeof(one));
    args->calls->count += 1u;
}

// a usage of the function's name put among those found as its path, its line and its span
static void calls_usage_append(const CliUsageCfg *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int from = args->from;
    unsigned int to = args->to;
    EMBED_CALL(calls_found_append, SymbolsCallsCtx, .calls = (CliCallsFound *)args->context, .path = path, .line = line,
               .from = from, .to = to, .site = (CliSymbolPosition){line, from});
}

// the calls found begun, 1, or 0 where the cells cannot meet them; and given back
EMBED_INLINE int calls_found_begin(const SymbolsCallsCtx *args)
{
    CliCallsFound *calls = args->calls;
    char *const found_cell = g_cli_guard->temporary_buf_alloc(CLI_CALLS_CAPACITY, g_cli_code_owner);
    char *const paths_cell = g_cli_guard->temporary_buf_alloc(CLI_CALLS_CAPACITY, g_cli_code_owner);
    *calls = (CliCallsFound){mmgr_line_over(found_cell, CLI_CALLS_CAPACITY),
                             mmgr_line_over(paths_cell, CLI_CALLS_CAPACITY), 0u};
    return (found_cell != NULL) && (paths_cell != NULL);
}

// the leaf of `file` at `line`, `col` in UTF-16 units, `file->read.count` where none starts there
EMBED_INLINE unsigned int calls_leaf_at(const SymbolsCallsCtx *args)
{
    for (unsigned int leaf = 0u; leaf < args->file->read.count; leaf += 1u)
    {
        if ((args->file->read.leaf[leaf].line == args->line) &&
            (EMBED_CALL(calls_col, SymbolsCallsCtx, .read = &args->file->read, .leaf = leaf) == args->col))
        {
            return leaf;
        }
    }
    return args->file->read.count;
}

// the usages of one file among those found from `first` to before `end`, each a call grouped under the function it
// stands in, each function handed to `each` with its calls' places
EMBED_INLINE int calls_callers_get(const SymbolsCallsCtx *args)
{
    const char *const path =
        args->usages->paths.out +
        EMBED_CALL(calls_found_at, SymbolsCallsCtx, .calls_read = args->usages, .at = args->first).path;
    CliCallsFile file;
    const int read = EMBED_CALL(calls_file_read, SymbolsCallsCtx, .file_out = &file, .path = path,
                                .open_text = args->open_text, .open_text_context = args->open_text_context);
    CliSymbolPosition sites[CLI_SYMBOLS_PARAMS];
    unsigned int count = 0u;
    unsigned int current = CLI_JSON_NONE;
    for (unsigned int at = args->first; (read == 1) && (at <= args->end); at += 1u)
    {
        unsigned int function = CLI_JSON_NONE;
        CliSymbolPosition site = {0u, 0u};
        if (at < args->end)
        {
            site = EMBED_CALL(calls_found_at, SymbolsCallsCtx, .calls_read = args->usages, .at = at).site;
            const unsigned int leaf =
                EMBED_CALL(calls_leaf_at, SymbolsCallsCtx, .file = &file, .line = site.line, .col = site.col);
            function = ((leaf < file.read.count) &&
                        EMBED_CALL(calls_is_call, SymbolsCallsCtx, .read = &file.read, .leaf = leaf))
                           ? EMBED_CALL(calls_function_on, SymbolsCallsCtx, .file = &file, .line = site.line, .body = 1)
                           : CLI_JSON_NONE;
            if (function == CLI_JSON_NONE)
            {
                continue;
            }
        }
        if ((current != CLI_JSON_NONE) && ((function != current) || (count == CLI_SYMBOLS_PARAMS)))
        {
            EMBED_CALL(calls_dispatch, SymbolsCallsCtx, .file = &file, .path = path,
                       .one = EMBED_CALL(calls_function_at, SymbolsCallsCtx, .file = &file, .at = current),
                       .sites = sites, .count = count, .each = args->each, .context = args->context);
            count = 0u;
        }
        current = function;
        if (at < args->end)
        {
            sites[count] = site;
            count += 1u;
        }
    }
    g_cli_guard->persistent_buf_release(file.functions.out, g_cli_code_owner);
    if (file.leaved)
    {
        g_cli_guard->persistent_buf_release(file.read.leaf, g_cli_leaves_owner);
        file.read.leaf = NULL;
        file.read.count = 0u;
        file.read.capacity = 0u;
    }
    if (file.languages != NULL)
    {
        g_cli_guard->persistent_buf_release(file.languages->languages.pair, g_cli_plugins_owner);
        *(&file.languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(file.languages, g_cli_code_owner);
    if (file.mapped && (file.text != NULL))
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)file.text), (g_cli_code_owner));
    }
    else
    {
        g_cli_guard->persistent_buf_release(file.text, g_cli_code_owner);
    }
    return read >= 0;
}

int cli_symbols_callers(const CliSymbolsPathContextLineCfg *args)
{
    CliCallsFile file;
    int found = EMBED_CALL(calls_file_read, SymbolsCallsCtx, .file_out = &file, .path = args->path,
                           .open_text = args->open_text, .open_text_context = args->open_text_context);
    const unsigned int at =
        (found == 1) ? EMBED_CALL(calls_function_on, SymbolsCallsCtx, .file = &file, .line = args->line, .body = 0)
                     : CLI_JSON_NONE;
    found = ((found == 1) && (at == CLI_JSON_NONE)) ? 0 : found;
    CliCallsFound usages;
    if ((found == 1) && !EMBED_CALL(calls_found_begin, SymbolsCallsCtx, .calls = &usages))
    {
        found = -1;
    }
    if (found == 1)
    {
        const CliCallsFunction one = EMBED_CALL(calls_function_at, SymbolsCallsCtx, .file = &file, .at = at);
        const unsigned int name_line = file.read.leaf[one.name].line;
        const unsigned int name_col = EMBED_CALL(calls_col, SymbolsCallsCtx, .read = &file.read, .leaf = one.name);
        found = (EMBED_CALL(cli_symbols_path_context.usages, CliSymbolsPathContextCfg, .path = args->path,
                            .line = name_line, .col = name_col, .across = 1, .open_text = args->open_text,
                            .open_text_context = args->open_text_context, .each_usage = calls_usage_append,
                            .context = &usages, .err_msg = args->err_msg) < 0)
                    ? -1
                    : 1;
        // the usages of each file stand together, in the order of their lines
        unsigned int first = 0u;
        for (unsigned int next = 1u; (found == 1) && mmgr_line_ok(&usages.found) && (next <= usages.count); next += 1u)
        {
            const int same =
                (next < usages.count) &&
                cellul.eq(&(CatenaFinitaCfg){
                    .src = usages.paths.out +
                           EMBED_CALL(calls_found_at, SymbolsCallsCtx, .calls_read = &usages, .at = next).path,
                    .other = usages.paths.out +
                             EMBED_CALL(calls_found_at, SymbolsCallsCtx, .calls_read = &usages, .at = first).path,
                    .cap = CLI_POOL_BYTES});
            if (!same)
            {
                found = EMBED_CALL(calls_callers_get, SymbolsCallsCtx, .usages = &usages, .first = first, .end = next,
                                   .open_text = args->open_text, .open_text_context = args->open_text_context,
                                   .each = args->each, .context = args->context)
                            ? 1
                            : -1;
                first = next;
            }
        }
        g_cli_guard->persistent_buf_release(usages.found.out, g_cli_code_owner);
        g_cli_guard->persistent_buf_release(usages.paths.out, g_cli_code_owner);
    }
    if (found < 0)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the function's callers");
    }
    g_cli_guard->persistent_buf_release(file.functions.out, g_cli_code_owner);
    if (file.leaved)
    {
        g_cli_guard->persistent_buf_release(file.read.leaf, g_cli_leaves_owner);
        file.read.leaf = NULL;
        file.read.count = 0u;
        file.read.capacity = 0u;
    }
    if (file.languages != NULL)
    {
        g_cli_guard->persistent_buf_release(file.languages->languages.pair, g_cli_plugins_owner);
        *(&file.languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(file.languages, g_cli_code_owner);
    if (file.mapped && (file.text != NULL))
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)file.text), (g_cli_code_owner));
    }
    else
    {
        g_cli_guard->persistent_buf_release(file.text, g_cli_code_owner);
    }
    return found;
}

// The definition of a call found as Go to Definition finds it: the calls found, and where the call stands
typedef struct
{
    CliCallsFound *calls;
    CliSymbolPosition site;
} CliCallsDefined;

// a call's definition in effect put among the calls found, keyed by its file and line
static void calls_defined_append(const CliUsageCfg *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int from = args->from;
    unsigned int to = args->to;
    const CliCallsDefined *const defined = (const CliCallsDefined *)args->context;
    EMBED_CALL(calls_found_append, SymbolsCallsCtx, .calls = defined->calls, .path = path, .line = line, .from = from,
               .to = to, .site = defined->site);
}

// the calls found handed to `each` grouped by their definitions, each definition once with its calls' places, in the
// order each was first called
EMBED_INLINE void calls_callees_dispatch(const SymbolsCallsCtx *args)
{
    for (unsigned int at = 0u; at < args->calls_read->count; at += 1u)
    {
        const CliCallFound one = EMBED_CALL(calls_found_at, SymbolsCallsCtx, .calls_read = args->calls_read, .at = at);
        const char *const path = args->calls_read->paths.out + one.path;
        int earlier = 0;
        CliSymbolPosition sites[CLI_SYMBOLS_PARAMS];
        unsigned int count = 0u;
        for (unsigned int other = 0u; other < args->calls_read->count; other += 1u)
        {
            const CliCallFound same =
                EMBED_CALL(calls_found_at, SymbolsCallsCtx, .calls_read = args->calls_read, .at = other);
            const int matches =
                (same.line == one.line) &&
                cellul.eq(&(CatenaFinitaCfg){
                    .src = args->calls_read->paths.out + same.path, .other = path, .cap = CLI_POOL_BYTES});
            earlier |= matches && (other < at);
            if (matches && (count < CLI_SYMBOLS_PARAMS))
            {
                sites[count] = same.site;
                count += 1u;
            }
        }
        if (!earlier)
        {
            unsigned long long length = 0ULL;
            const unsigned int leaf = EMBED_CALL(calls_leaf_at, SymbolsCallsCtx, .file = args->file,
                                                 .line = one.site.line, .col = one.site.col);
            const char *const name =
                (leaf < args->file->read.count)
                    ? EMBED_CALL(cli_symbols_read_leaf_internal.leaf_text, CliSymbolsReadLeafCtx,
                                 .read_read = &args->file->read, .leaf = leaf, .length_pointer = &length)
                    : "";
            EMBED_CALL(args->each, CliCallReachedCfg, .context = args->context, .path = path, .line = one.line,
                       .from = one.from, .to = one.to, .name = name, .length = length, .sites = sites, .count = count);
        }
    }
}

int cli_symbols_callees(const CliSymbolsPathContextLineCfg *args)
{
    CliCallsFile file;
    int found = EMBED_CALL(calls_file_read, SymbolsCallsCtx, .file_out = &file, .path = args->path,
                           .open_text = args->open_text, .open_text_context = args->open_text_context);
    const unsigned int at =
        (found == 1) ? EMBED_CALL(calls_function_on, SymbolsCallsCtx, .file = &file, .line = args->line, .body = 0)
                     : CLI_JSON_NONE;
    found = ((found == 1) && (at == CLI_JSON_NONE)) ? 0 : found;
    CliCallsFound calls;
    if ((found == 1) && !EMBED_CALL(calls_found_begin, SymbolsCallsCtx, .calls = &calls))
    {
        found = -1;
    }
    if (found == 1)
    {
        const CliCallsFunction one = EMBED_CALL(calls_function_at, SymbolsCallsCtx, .file = &file, .at = at);
        for (unsigned int leaf = one.open; (found == 1) && (leaf < one.end); leaf += 1u)
        {
            if (!EMBED_CALL(calls_is_call, SymbolsCallsCtx, .read = &file.read, .leaf = leaf))
            {
                continue;
            }
            CliCallsDefined defined = {
                &calls,
                {file.read.leaf[leaf].line, EMBED_CALL(calls_col, SymbolsCallsCtx, .read = &file.read, .leaf = leaf)}};
            found = (EMBED_CALL(cli_symbols_path_context.definition, CliSymbolsPathContextCfg, .path = args->path,
                                .line = defined.site.line, .col = defined.site.col, .open_text = args->open_text,
                                .open_text_context = args->open_text_context, .each_usage = calls_defined_append,
                                .context = &defined, .err_msg = args->err_msg) < 0)
                        ? -1
                        : 1;
        }
        if ((found == 1) && mmgr_line_ok(&calls.found) && mmgr_line_ok(&calls.paths))
        {
            EMBED_CALL(calls_callees_dispatch, SymbolsCallsCtx, .calls_read = &calls, .file = &file, .each = args->each,
                       .context = args->context);
        }
        g_cli_guard->persistent_buf_release(calls.found.out, g_cli_code_owner);
        g_cli_guard->persistent_buf_release(calls.paths.out, g_cli_code_owner);
    }
    if (found < 0)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the function's callees");
    }
    g_cli_guard->persistent_buf_release(file.functions.out, g_cli_code_owner);
    if (file.leaved)
    {
        g_cli_guard->persistent_buf_release(file.read.leaf, g_cli_leaves_owner);
        file.read.leaf = NULL;
        file.read.count = 0u;
        file.read.capacity = 0u;
    }
    if (file.languages != NULL)
    {
        g_cli_guard->persistent_buf_release(file.languages->languages.pair, g_cli_plugins_owner);
        *(&file.languages->languages) = (CliLanguages){NULL, 0u, 0u};
    }
    g_cli_guard->persistent_buf_release(file.languages, g_cli_code_owner);
    if (file.mapped && (file.text != NULL))
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)file.text), (g_cli_code_owner));
    }
    else
    {
        g_cli_guard->persistent_buf_release(file.text, g_cli_code_owner);
    }
    return found;
}
