// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_signature.c: the signature of the call the cursor is in, from the tree: the call found off the leaves of the
// lines the page hands, its arguments and each one's type, every declaration of its name in the tree with its head
// read off its own file's leaves, each held against the arguments, the best fit first. No language is read here but by
// its plugin's grammar
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "symbols_internal.h"
#include "symbols_tree.h"

// how many lines above the cursor's the call may have opened on, the most arguments read, and the most bytes of an
// argument's text written
#define CLI_SYMBOLS_CALL_LINES 24u
#define CLI_SYMBOLS_ARGUMENTS 32u
#define CLI_SYMBOLS_ARGUMENT_TEXT 80ull

// The call the cursor is in: its name's leaf, its bracket's, the argument the cursor stands in, and each argument's
// first leaf, the leaf after its last, and its type
typedef struct
{
    unsigned int name;
    unsigned int open;
    unsigned int active;
    unsigned int arguments;
    unsigned int first[CLI_SYMBOLS_ARGUMENTS];
    unsigned int end[CLI_SYMBOLS_ARGUMENTS];
    CliSymbolType type[CLI_SYMBOLS_ARGUMENTS];
} CliSymbolCall;

// A signature worked out, in a cell of its own: the call, the heads of its name's declarations, how many, the places
// of those read in order of fit, how many, the tree's top, the path and line of a name's declaration sought, and the
// id of the language the call is read in
typedef struct
{
    CliSymbolCall call;
    CliSymbolHead head[CLI_SYMBOLS_HEADS];
    unsigned int heads;
    unsigned int order[CLI_SYMBOLS_HEADS];
    unsigned int ordered;
    char root[CLI_PATH_MAX_LEN];
    char sought[CLI_PATH_MAX_LEN];
    unsigned int sought_line;
    char language[CLI_NAME_MAX_LEN];
} CliSymbolSignature;

// whether the file at `path` opens in the language the call is read in
static int cli_signature_language_is(const CliSymbolsLanguages *languages, const CliSymbolSignature *signature,
                                     const char *path)
{
    char id[CLI_NAME_MAX_LEN];
    return cli_plugins_language_for(&languages->languages, path, id) &&
           cellul.eq(&(CatenaFinitaCfg){.src = id, .other = signature->language, .cap = CLI_CELLS_POOL});
}

// the byte of `size` bytes of `text` the place `line`, `col` stands at, `size` where the text has no such line
static unsigned long long cli_signature_cursor(const char *text, unsigned long long size, unsigned int line,
                                               unsigned int col)
{
    unsigned long long start = 0ull;
    for (unsigned int at = 0u; at < line; at += 1u)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = size - start});
        if ((newline == NULL) || (start >= size))
        {
            return size;
        }
        // a newline found lies in the text, at or past `start`
        start = (unsigned long long)(newline - text) + 1ull;
    }
    unsigned long long end = start;
    while ((end < size) && (text[end] != '\n') && (text[end] != '\r'))
    {
        end += 1ull;
    }
    unsigned long long byte = 0ull;
    unsigned int units = 0u;
    return start + cli_symbols_byte_at(text + start, end - start, col, &byte, &units);
}

// the call the cursor at byte `cursor` of line `line` stands in, walked back over the leaves before it, each as its
// grammar names it: its bracket the first that opens at depth 0, a name named a call right before it, the separators at
// that depth counted: 1, or 0 where a bracket opens at depth 0 with no call before it, a statement ends at depth 0, or
// the lines' reach comes first
static int cli_signature_call(const CliLeafRead *read, unsigned long long cursor, unsigned int line,
                              CliSymbolCall *call)
{
    unsigned int leaf = 0u;
    while ((leaf < read->count) && (read->leaf[leaf].from < cursor))
    {
        leaf += 1u;
    }
    unsigned int depth = 0u;
    call->active = 0u;
    while (leaf > 0u)
    {
        leaf -= 1u;
        if ((read->leaf[leaf].line + CLI_SYMBOLS_CALL_LINES) <= line)
        {
            return 0;
        }
        if (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE))
        {
            depth += 1u;
        }
        else if (cli_leaf_has(read, leaf, CLI_LEAF_OPEN) && (depth > 0u))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) && cli_leaf_has(read, leaf, CLI_LEAF_STATEMENT))
        {
            return 0;
        }
        else if (cli_leaf_has(read, leaf, CLI_LEAF_OPEN))
        {
            unsigned int name = leaf;
            while ((name > 0u) && ((read->leaf[name - 1u].is & CLI_LEAF_BLANK) != 0u))
            {
                name -= 1u;
            }
            if ((name == 0u) || !cli_leaf_has(read, name - 1u, CLI_LEAF_NAME | CLI_LEAF_CALL))
            {
                return 0;
            }
            call->name = name - 1u;
            call->open = leaf;
            return 1;
        }
        else if ((depth == 0u) && cli_leaf_has(read, leaf, CLI_LEAF_SEPARATOR))
        {
            call->active += 1u;
        }
    }
    return 0;
}

// the argument of the leaves from `first` to before `end` put after the call's others, the blank and comment leaves at
// either end of it left off
static void cli_signature_argument_put(const CliLeafRead *read, CliSymbolCall *call, unsigned int first,
                                       unsigned int end)
{
    const unsigned long long passed = CLI_LEAF_BLANK | CLI_LEAF_COMMENT;
    while ((first < end) && ((read->leaf[first].is & passed) != 0u))
    {
        first += 1u;
    }
    while ((end > first) && ((read->leaf[end - 1u].is & passed) != 0u))
    {
        end -= 1u;
    }
    if (call->arguments < CLI_SYMBOLS_ARGUMENTS)
    {
        call->first[call->arguments] = first;
        call->end[call->arguments] = end;
        call->arguments += 1u;
    }
}

// the call's arguments from its bracket on, each the leaves between separators at its depth, to the bracket that closes
// it or the last leaf of the cursor's line
static void cli_signature_arguments(const CliLeafRead *read, unsigned int line, CliSymbolCall *call)
{
    unsigned int depth = 0u;
    unsigned int first = call->open + 1u;
    unsigned int leaf = call->open + 1u;
    call->arguments = 0u;
    for (; (leaf < read->count) && (read->leaf[leaf].line <= line); leaf += 1u)
    {
        if (cli_leaf_has(read, leaf, CLI_LEAF_OPEN))
        {
            depth += 1u;
        }
        else if (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE) && (depth == 0u))
        {
            break;
        }
        else if (cli_leaf_has(read, leaf, CLI_LEAF_CLOSE))
        {
            depth -= 1u;
        }
        else if ((depth == 0u) && cli_leaf_has(read, leaf, CLI_LEAF_SEPARATOR))
        {
            cli_signature_argument_put(read, call, first, leaf);
            first = leaf + 1u;
        }
    }
    cli_signature_argument_put(read, call, first, leaf);
}

// the type of the tree's first declaration of `name`, `length` bytes, in a file of the call's language, read off the
// leaves of its own line
static void cli_signature_tree_type(CliSymbolsLanguages *languages, CliSymbolSignature *signature, const char *name,
                                    unsigned long long length, CliSymbolType *type)
{
    int seen = 0;
    cli_lock_take(&g_cli_symbols_lock);
    for (unsigned int file = 0u; !seen && (file < g_cli_symbol_file_count); file += 1u)
    {
        const CliSymbolFile *const branch = &g_cli_symbol_files[file];
        const int spoken =
            branch->used && cli_signature_language_is(languages, signature, cli_symbol_file_path(branch));
        for (unsigned int each = 0u; !seen && spoken && (each < branch->count); each += 1u)
        {
            const CliSymbol *const symbol = &cli_symbol_file_symbols(branch)[each];
            seen = (symbol->length == length) && cli_text_same(branch->cell + symbol->name, name, length);
            if (seen)
            {
                const char *const path = cli_symbol_file_path(branch);
                const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
                memor.cpy(&(MemoriaCfg){.dst = signature->sought, .src = path, .bytes = (size_t)size + 1u});
                signature->sought_line = symbol->line;
            }
        }
    }
    cli_lock_give(&g_cli_symbols_lock);
    char full[CLI_PATH_MAX_LEN];
    cli_full(signature->root, signature->sought, full);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = seen ? cli_file_read(full, &size) : NULL;
    if (bytes == NULL)
    {
        return;
    }
    const char *const text = (const char *)bytes;
    unsigned long long start = 0ull;
    for (unsigned int at = 0u; (at < signature->sought_line) && (start < size); at += 1u)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = size - start});
        // a newline found lies in the text, at or past `start`
        start = (newline != NULL) ? ((unsigned long long)(newline - text) + 1ull) : size;
    }
    unsigned long long end = start;
    while ((end < size) && (text[end] != '\n'))
    {
        end += 1ull;
    }
    CliLeafRead read;
    if (cli_symbols_leaves_kept(languages, signature->sought, "root", text + start, end - start, &read))
    {
        for (unsigned int leaf = 0u; leaf < read.count; leaf += 1u)
        {
            unsigned long long size_of = 0ull;
            const char *const named = cli_leaf_text(&read, leaf, &size_of);
            if (((read.leaf[leaf].is & CLI_LEAF_DECLARES) != 0u) && (size_of == length) &&
                cli_text_same(named, name, length))
            {
                cli_symbols_type_of(&read, leaf, type);
                break;
            }
        }
    }
    cli_symbols_leaves_given(&read);
    cli_file_let_go(bytes, size);
}

// the type of the argument at `argument`: a literal's where every leaf of it is a number's or every one a string's,
// and for one name its declaration's, the last parameter or local of that name before the call among the leaves
// handed, else the tree's; none for any other
static void cli_signature_typed(CliSymbolsLanguages *languages, const CliLeafRead *read, CliSymbolSignature *signature,
                                unsigned int argument)
{
    CliSymbolCall *const call = &signature->call;
    CliSymbolType *const type = &call->type[argument];
    type->text[0] = '\0';
    type->is = 0ull;
    const unsigned int first = call->first[argument];
    const unsigned int last = call->end[argument];
    unsigned long long is = CLI_LEAF_NUMBER | CLI_LEAF_STRING;
    for (unsigned int leaf = first; leaf < last; leaf += 1u)
    {
        is &= read->leaf[leaf].is;
    }
    if ((first < last) && ((is & (CLI_LEAF_NUMBER | CLI_LEAF_STRING)) != 0u))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = type->text, .src = ((is & CLI_LEAF_NUMBER) != 0u) ? "number" : "string", .bytes = 7u});
        type->is = is & (CLI_LEAF_NUMBER | CLI_LEAF_STRING);
        return;
    }
    if (((last - first) != 1u) || ((read->leaf[first].is & CLI_LEAF_NAME) == 0u))
    {
        return;
    }
    unsigned long long length = 0ull;
    const char *const name = cli_leaf_text(read, first, &length);
    for (unsigned int leaf = call->name; leaf > 0u; leaf -= 1u)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_leaf_text(read, leaf - 1u, &size);
        if (((read->leaf[leaf - 1u].is & (CLI_LEAF_PARAMETER | CLI_LEAF_LOCAL)) != 0u) && (size == length) &&
            cli_text_same(text, name, length))
        {
            cli_symbols_type_of(read, leaf - 1u, type);
            return;
        }
    }
    cli_signature_tree_type(languages, signature, name, length, type);
}

// the heads of every declaration of the call's name, `length` bytes, in the tree's files of the call's language,
// CLI_SYMBOLS_HEADS at most, each held against the call's arguments, and the places of those read put in order of
// fit: those whose count fits first, then those with fewer arguments that differ
static void cli_signature_heads(CliSymbolsLanguages *languages, CliSymbolSignature *signature, const char *name,
                                unsigned long long length)
{
    signature->heads = 0u;
    cli_lock_take(&g_cli_symbols_lock);
    for (unsigned int file = 0u; file < g_cli_symbol_file_count; file += 1u)
    {
        const CliSymbolFile *const branch = &g_cli_symbol_files[file];
        const int spoken =
            branch->used && cli_signature_language_is(languages, signature, cli_symbol_file_path(branch));
        for (unsigned int each = 0u; spoken && (each < branch->count); each += 1u)
        {
            const CliSymbol *const symbol = &cli_symbol_file_symbols(branch)[each];
            if ((signature->heads < CLI_SYMBOLS_HEADS) && (symbol->length == length) &&
                cli_text_same(branch->cell + symbol->name, name, length))
            {
                CliSymbolHead *const head = &signature->head[signature->heads];
                const char *const path = cli_symbol_file_path(branch);
                const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
                memor.cpy(&(MemoriaCfg){.dst = head->path, .src = path, .bytes = (size_t)size + 1u});
                head->line = symbol->line;
                signature->heads += 1u;
            }
        }
    }
    cli_lock_give(&g_cli_symbols_lock);
    const CliSymbolCall *const call = &signature->call;
    unsigned int given = 0u;
    for (unsigned int argument = 0u; argument < call->arguments; argument += 1u)
    {
        given += (call->end[argument] > call->first[argument]) ? 1u : 0u;
    }
    signature->ordered = 0u;
    for (unsigned int at = 0u; at < signature->heads; at += 1u)
    {
        CliSymbolHead *const head = &signature->head[at];
        if (!cli_symbols_head_read(languages, signature->root, name, length, head))
        {
            continue;
        }
        head->differing = 0u;
        for (unsigned int argument = 0u; (argument < call->arguments) && (argument < head->params); argument += 1u)
        {
            if (cli_symbols_type_fits(&call->type[argument], &head->type[argument]) == 0)
            {
                head->differs[head->differing] = argument;
                head->differing += 1u;
            }
        }
        const int reached = (call->active < head->params) || ((head->params == 0u) && (call->active == 0u));
        head->counts = head->variadic || ((given <= head->params) && reached);
        // its place put among those of the heads read before it, after each that fits as well or better
        unsigned int place = signature->ordered;
        while (place > 0u)
        {
            const CliSymbolHead *const before = &signature->head[signature->order[place - 1u]];
            const int better = (head->counts && !before->counts) ||
                               ((head->counts == before->counts) && (head->differing < before->differing));
            if (!better)
            {
                break;
            }
            signature->order[place] = signature->order[place - 1u];
            place -= 1u;
        }
        signature->order[place] = at;
        signature->ordered += 1u;
    }
}

// the signature's answer written: the call's name, the argument the cursor stands in, each argument's text and type,
// and each declaration's head with where it stands
static void cli_signature_written(const CliLeafRead *read, const CliSymbolSignature *signature, VerbaLinea *out)
{
    const CliSymbolCall *const call = &signature->call;
    unsigned long long length = 0ull;
    const char *const name = cli_leaf_text(read, call->name, &length);
    mmgr_line_put(out, "{\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)name, length);
    mmgr_line_put(out, ",\"active\":");
    mmgr_line_decimal(out, call->active, 1u);
    mmgr_line_put(out, ",\"arguments\":[");
    for (unsigned int argument = 0u; argument < call->arguments; argument += 1u)
    {
        const unsigned int first = call->first[argument];
        const unsigned int end = call->end[argument];
        const unsigned long long from = (end > first) ? read->leaf[first].from : 0ull;
        const unsigned long long to = (end > first) ? read->leaf[end - 1u].to : 0ull;
        const unsigned long long size =
            ((to - from) < CLI_SYMBOLS_ARGUMENT_TEXT) ? (to - from) : CLI_SYMBOLS_ARGUMENT_TEXT;
        mmgr_line_put(out, (argument != 0u) ? ",{\"text\":" : "{\"text\":");
        cli_json_quoted_lossy(out, (const unsigned char *)read->text + from, size);
        mmgr_line_put(out, ",\"type\":");
        const char *const type = call->type[argument].text;
        if (type[0] == '\0')
        {
            mmgr_line_put(out, "null");
        }
        else
        {
            cli_json_quoted_lossy(out, (const unsigned char *)type,
                                  cellul.len(&(CatenaFinitaCfg){.src = type, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, "],\"declarations\":[");
    for (unsigned int at = 0u; at < signature->ordered; at += 1u)
    {
        const CliSymbolHead *const head = &signature->head[signature->order[at]];
        mmgr_line_put(out, (at != 0u) ? ",{\"path\":" : "{\"path\":");
        cli_json_quoted(out, head->path, cellul.len(&(CatenaFinitaCfg){.src = head->path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"line\":");
        mmgr_line_decimal(out, head->line, 1u);
        mmgr_line_put(out, ",\"label\":");
        cli_json_quoted_lossy(out, (const unsigned char *)head->label,
                              cellul.len(&(CatenaFinitaCfg){.src = head->label, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"params\":[");
        for (unsigned int param = 0u; param < head->params; param += 1u)
        {
            mmgr_line_put(out, (param != 0u) ? ",[" : "[");
            mmgr_line_decimal(out, head->span[param][0], 1u);
            mmgr_line_ch(out, ',');
            mmgr_line_decimal(out, head->span[param][1], 1u);
            mmgr_line_ch(out, ']');
        }
        mmgr_line_put(out, "],\"types\":[");
        for (unsigned int param = 0u; param < head->params; param += 1u)
        {
            mmgr_line_put(out, (param != 0u) ? "," : "");
            const char *const type = head->type[param].text;
            cli_json_quoted_lossy(out, (const unsigned char *)type,
                                  cellul.len(&(CatenaFinitaCfg){.src = type, .cap = CLI_CELLS_POOL}));
        }
        mmgr_line_put(out, "],\"count\":");
        mmgr_line_put(out, head->counts ? "true" : "false");
        mmgr_line_put(out, ",\"variadic\":");
        mmgr_line_put(out, head->variadic ? "true" : "false");
        mmgr_line_put(out, ",\"differs\":[");
        for (unsigned int differs = 0u; differs < head->differing; differs += 1u)
        {
            mmgr_line_put(out, (differs != 0u) ? "," : "");
            mmgr_line_decimal(out, head->differs[differs], 1u);
        }
        mmgr_line_put(out, "]}");
    }
    cli_lock_take(&g_cli_symbols_lock);
    mmgr_line_put(out, g_cli_symbols_reading ? "],\"reading\":true}" : "],\"reading\":false}");
    cli_lock_give(&g_cli_symbols_lock);
}

int cli_symbols_signature(const char *path, const char *state, const char *text, unsigned long long size,
                          unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    CliSymbolSignature *const signature =
        (languages != NULL) ? g_cli_cells->persistent_buf_alloc(sizeof(CliSymbolSignature), NULL) : NULL;
    if (signature == NULL)
    {
        cli_symbols_languages_given(languages);
        mmgr_line_put(err_msg, "the cells cannot meet a signature's reading");
        return 0;
    }
    cli_lock_take(&g_cli_symbols_lock);
    memor.cpy(&(MemoriaCfg){.dst = signature->root, .src = g_cli_symbols_root, .bytes = CLI_PATH_MAX_LEN});
    cli_lock_give(&g_cli_symbols_lock);
    signature->language[0] = '\0';
    (void)cli_plugins_language_for(&languages->languages, path, signature->language);
    CliLeafRead read;
    const int kept = cli_symbols_leaves_kept(languages, path, state, text, size, &read);
    CliSymbolCall *const call = &signature->call;
    const int called = kept && cli_signature_call(&read, cli_signature_cursor(text, size, line, col), line, call);
    if (called)
    {
        cli_signature_arguments(&read, line, call);
        for (unsigned int argument = 0u; argument < call->arguments; argument += 1u)
        {
            cli_signature_typed(languages, &read, signature, argument);
        }
        unsigned long long length = 0ull;
        const char *const name = cli_leaf_text(&read, call->name, &length);
        cli_signature_heads(languages, signature, name, length);
        cli_signature_written(&read, signature, out);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    const int refused = read.refused;
    cli_symbols_leaves_given(&read);
    g_cli_cells->persistent_buf_release(signature, NULL);
    cli_symbols_languages_given(languages);
    if (refused)
    {
        mmgr_line_put(err_msg, "the cells cannot meet the leaves of the lines handed");
    }
    return !refused;
}
