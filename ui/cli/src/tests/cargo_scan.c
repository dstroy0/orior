// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_scan.c: a file of Rust read as the search for tests reads it, off the leaves its grammar plugin's walk saves:
// its tokens are the attributes that mark a test, the names functions and modules are declared by, the braces of
// regions and the ends of statements; and the tests and the modules it contains
#include "../code/symbols_internal.h"
#include "../command_line/pool.h"
#include "cargo_tests_internal.h"

CliCargoScan g_cli_cargo_scan;

// the max modules written in one another at once
#define CLI_CARGO_INLINE 256u

// no token
#define CLI_RUST_NONE 0xffffffffu

typedef struct
{
    const char *text;
    const CliLeafRead *read;
    const CliLeaf *leaf;
    unsigned int at;
    const unsigned int *inline_names;
    unsigned int count;
    unsigned int name;
    int test;
    unsigned long long length;
    unsigned int *opened_names;
    unsigned long long *opened_depths;
    unsigned int *open_pointer;
    unsigned long long *depth_pointer;
} CargoScanCtx;
// what a leaf is to the search by its bits, CLI_RUST_NONE where it is no token: a mark of no bytes, a string, a comment
// and every other leaf the search does not read
EMBED_INLINE unsigned int scan_type(const CargoScanCtx *args)
{
    const unsigned long long is = args->leaf->is;
    if ((args->leaf->to == args->leaf->from) || ((is & (CLI_LEAF_STRING | CLI_LEAF_COMMENT)) != 0u))
    {
        return CLI_RUST_NONE;
    }
    if ((is & CLI_LEAF_TEST) != 0u)
    {
        return CLI_RUST_TEST;
    }
    if (((is & CLI_LEAF_DECLARES) != 0u) && ((is & CLI_LEAF_FUNCTION) != 0u))
    {
        return CLI_RUST_FUNCTION;
    }
    if (((is & CLI_LEAF_DECLARES) != 0u) && ((is & CLI_LEAF_MODULE) != 0u))
    {
        return CLI_RUST_MODULE;
    }
    if (((is & CLI_LEAF_REGION) != 0u) && ((is & CLI_LEAF_OPEN) != 0u))
    {
        return CLI_RUST_OPEN;
    }
    if (((is & CLI_LEAF_REGION) != 0u) && ((is & CLI_LEAF_CLOSE) != 0u))
    {
        return CLI_RUST_CLOSE;
    }
    return ((is & CLI_LEAF_STATEMENT) != 0u) ? CLI_RUST_END : CLI_RUST_NONE;
}

// the tokens among the leaves counted, each put where the tokens' cell fits one more
EMBED_INLINE void scan_tokens_append(const CargoScanCtx *args)
{
    const CliLeafRead *const read = args->read;
    g_cli_cargo_scan.token_count = 0u;
    for (unsigned int at = 0u; at < read->count; at += 1u)
    {
        const CliLeaf *const leaf = &read->leaf[at];
        const unsigned int type = EMBED_CALL(scan_type, CargoScanCtx, .leaf = leaf);
        if (type == CLI_RUST_NONE)
        {
            continue;
        }
        if (g_cli_cargo_scan.token_count < g_cli_cargo_scan.token_capacity)
        {
            // a leaf's bytes fit an unsigned int, as a file's tokens are read whole
            g_cli_cargo_scan.tokens[g_cli_cargo_scan.token_count] =
                (CliRustToken){type, leaf->from, (unsigned int)(leaf->to - leaf->from), leaf->line};
        }
        g_cli_cargo_scan.token_count += 1u;
    }
}

void cli_cargo_tokens(const CliCargoTestsCtx *args)
{
    g_cli_guard->persistent_buf_release(g_cli_cargo_scan.tokens, g_cli_cargo_owner);
    g_cli_cargo_scan.tokens = NULL;
    g_cli_cargo_scan.token_capacity = 0ULL;
    g_cli_cargo_scan.token_count = 0u;
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    if (languages == NULL)
    {
        return;
    }
    CliLeafRead read;
    const int leaved =
        EMBED_CALL(cli_symbols_text_internal.language_leaves_save, CliSymbolsTextCtx, .languages = languages,
                   .id = "rust", .text = args->text, .size = args->size, .read = &read);
    if (leaved)
    {
        EMBED_CALL(scan_tokens_append, CargoScanCtx, .read = &read);
        const unsigned long long counted = g_cli_cargo_scan.token_count;
        g_cli_cargo_scan.tokens =
            (counted != 0ULL) ? g_cli_guard->temporary_buf_alloc(counted * sizeof(CliRustToken), g_cli_cargo_owner)
                              : NULL;
        g_cli_cargo_scan.token_capacity = (g_cli_cargo_scan.tokens != NULL) ? counted : 0ULL;
        EMBED_CALL(scan_tokens_append, CargoScanCtx, .read = &read);
        g_cli_cargo_scan.token_count = (unsigned int)g_cli_cargo_scan.token_capacity;
    }
    g_cli_guard->persistent_buf_release(read.leaf, g_cli_leaves_owner);
    g_cli_guard->persistent_buf_release(languages, g_cli_code_owner);
}

// the type of the token at `at`, CLI_RUST_NONE past the tokens
EMBED_INLINE unsigned int scan_type_at(const CargoScanCtx *args)
{
    return (args->at < g_cli_cargo_scan.token_count) ? g_cli_cargo_scan.tokens[args->at].type : CLI_RUST_NONE;
}

// `length` bytes put in the names, ended by a 0, the names' cell containing them: where they start
EMBED_INLINE unsigned long long scan_name_append(const CargoScanCtx *args)
{
    const unsigned long long at = g_cli_cargo_scan.name_used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_cargo_scan.names + at, .src = args->text, .bytes = args->length});
    g_cli_cargo_scan.names[at + args->length] = '\0';
    g_cli_cargo_scan.name_used += args->length + 1ULL;
    return at;
}

// a test or a module stored, with the modules written in the file it is in, `count` of them by their tokens
EMBED_INLINE void scan_item_append(const CargoScanCtx *args)
{
    char joined[4096];
    VerbaLinea line = {joined, sizeof(joined), 0ULL};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const CliRustToken *const token = &g_cli_cargo_scan.tokens[args->inline_names[at]];
        mmgr_line_put(&line, (at > 0u) ? "::" : "");
        mmgr_line_put_n(&line, args->text + token->start, token->size);
    }
    const CliRustToken *const named = &g_cli_cargo_scan.tokens[args->name];
    const unsigned long long names_max = g_cli_cargo_scan.name_used + line.at + 1ULL + named->size + 1ULL;
    if ((g_cli_cargo_scan.item_count >= g_cli_cargo_scan.item_capacity) || (names_max > g_cli_cargo_scan.name_capacity))
    {
        return;
    }
    const unsigned long long inline_at = EMBED_CALL(scan_name_append, CargoScanCtx, .text = joined, .length = line.at);
    const unsigned long long name_at =
        EMBED_CALL(scan_name_append, CargoScanCtx, .text = args->text + named->start, .length = named->size);
    g_cli_cargo_scan.items[g_cli_cargo_scan.item_count] = (CliRustItem){args->test, inline_at, name_at, named->line};
    g_cli_cargo_scan.item_count += 1u;
}

// the module the name at `at` declares: one in a file of its own, ended by its statement's end, stored; one written
// here, opened by its brace, opened, its name put among those open with the depth it opens at; where the tokens go on
EMBED_INLINE unsigned int scan_module(const CargoScanCtx *args)
{
    const unsigned int at = args->at;
    unsigned int *const open = args->open_pointer;
    const unsigned int next = EMBED_CALL(scan_type_at, CargoScanCtx, .at = at + 1u);
    if (next == CLI_RUST_END)
    {
        EMBED_CALL(scan_item_append, CargoScanCtx, .text = args->text, .inline_names = args->opened_names,
                   .count = *open, .name = at, .test = 0);
        return at + 2u;
    }
    if ((next == CLI_RUST_OPEN) && (*open < CLI_CARGO_INLINE))
    {
        args->opened_names[*open] = at;
        args->opened_depths[*open] = *args->depth_pointer;
        *open += 1u;
        *args->depth_pointer += 1ULL;
        return at + 2u;
    }
    return at + 1u;
}

unsigned int cli_cargo_scan(const CliCargoTestsCtx *args)
{
    if (g_cli_cargo_scan.items == NULL)
    {
        const unsigned long long item_bytes = (unsigned long long)CLI_CARGO_FOUND * sizeof(CliRustItem);
        char *const cell = g_cli_guard->temporary_buf_alloc(item_bytes + CLI_CARGO_TEXT, g_cli_cargo_owner);
        if (cell != NULL)
        {
            g_cli_cargo_scan.items = (CliRustItem *)(void *)cell;
            g_cli_cargo_scan.item_capacity = CLI_CARGO_FOUND;
            g_cli_cargo_scan.names = cell + item_bytes;
            g_cli_cargo_scan.name_capacity = CLI_CARGO_TEXT;
        }
    }
    const unsigned int first = g_cli_cargo_scan.item_count;
    unsigned int inline_names[CLI_CARGO_INLINE];
    unsigned long long inline_depths[CLI_CARGO_INLINE];
    unsigned int open = 0u;
    unsigned long long depth = 0ULL;
    int marked = 0;
    unsigned int at = 0u;
    while (at < g_cli_cargo_scan.token_count)
    {
        const unsigned int type = g_cli_cargo_scan.tokens[at].type;
        if (type == CLI_RUST_TEST)
        {
            marked = 1;
        }
        else if (type == CLI_RUST_MODULE)
        {
            at = EMBED_CALL(scan_module, CargoScanCtx, .text = args->text, .at = at, .opened_names = inline_names,
                            .opened_depths = inline_depths, .open_pointer = &open, .depth_pointer = &depth);
            marked = 0;
            continue;
        }
        else if (type == CLI_RUST_FUNCTION)
        {
            if (marked)
            {
                EMBED_CALL(scan_item_append, CargoScanCtx, .text = args->text, .inline_names = inline_names,
                           .count = open, .name = at, .test = 1);
            }
            marked = 0;
        }
        else if (type == CLI_RUST_OPEN)
        {
            depth += 1ULL;
            marked = 0;
        }
        else if (type == CLI_RUST_CLOSE)
        {
            depth -= (depth > 0ULL) ? 1ULL : 0ULL;
            open -= ((open > 0u) && (inline_depths[open - 1u] == depth)) ? 1u : 0u;
        }
        else
        {
            marked = 0;
        }
        at += 1u;
    }
    return g_cli_cargo_scan.item_count - first;
}
