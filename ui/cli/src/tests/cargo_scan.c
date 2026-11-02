// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_scan.c: a file of Rust read as the search for tests reads it: its tokens, comments, strings and characters
// passed over, and the tests and the modules it contains
#include "../text/text.h"
#include "cargo_tests_internal.h"

CliCargoScan g_cli_cargo_scan;

// the most modules written in one another at once
#define CLI_CARGO_INLINE 256u

// no letter, past a text's end
#define CLI_RUST_NONE 0xffffffffu

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at_u64;
    unsigned long long *reserved;
    unsigned int letter;
    unsigned int kind;
    unsigned long long start;
    unsigned long long end;
    unsigned int line;
    unsigned int *line_pointer;
    unsigned int at;
    const char *word;
    unsigned long long length;
    const unsigned int *inline_names;
    unsigned int count;
    unsigned int name;
    int test;
    int *marked;
} CargoScanCtx;
// the letter at `at` of `size` bytes and the bytes it takes, CLI_RUST_NONE past the end
EMBED_INLINE unsigned int scan_letter(const CargoScanCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at_u64;
    unsigned long long *reserved = args->reserved;
    if (at >= args->size)
    {
        *reserved = 0ull;
        return CLI_RUST_NONE;
    }
    unsigned int letter = 0u;
    *reserved =
        EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = (const unsigned char *)text,
                   .length = args->size, .at = at, .letter = &letter);
    return letter;
}

// whether a letter is whitespace, as Unicode has it
EMBED_INLINE int scan_white(const CargoScanCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

// whether a letter is one a word contains: an ASCII letter, digit or `_`, or a letter past ASCII that is no whitespace,
// punctuation, arrow, mathematical sign or drawing
EMBED_INLINE int scan_word(const CargoScanCtx *args)
{
    if (args->letter < 0x80u)
    {
        return ((args->letter >= 'a') && (args->letter <= 'z')) || ((args->letter >= 'A') && (args->letter <= 'Z')) ||
               ((args->letter >= '0') && (args->letter <= '9')) || (args->letter == '_');
    }
    return (args->letter != CLI_RUST_NONE) && !scan_white(args) &&
           !((args->letter >= 0x2000u) && (args->letter <= 0x2bffu)) &&
           !((args->letter >= 0x3000u) && (args->letter <= 0x303fu)) &&
           !((args->letter >= 0xa0u) && (args->letter <= 0xbfu));
}

// a token counted, and put where the tokens' cell fits one more
EMBED_INLINE void scan_append(const CargoScanCtx *args)
{
    if (g_cli_cargo_scan.token_count < g_cli_cargo_scan.token_capacity)
    {
        g_cli_cargo_scan.tokens[g_cli_cargo_scan.token_count] =
            (CliRustToken){args->kind, args->letter, args->start, (unsigned int)(args->end - args->start), args->line};
    }
    g_cli_cargo_scan.token_count += 1u;
}

// a raw string or a byte string, `r".."`, `r#".."#`, `b".."` or `br#".."#`, passed over from `at`, at the hashes or
// the quote after its prefix, or the raw word `r#name` put: where the text goes on
EMBED_INLINE unsigned long long scan_raw(const CargoScanCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = args->at_u64;
    unsigned int *line = args->line_pointer;
    unsigned long long hashes = 0ull;
    const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text,
                                              .at = at, .size = size, .set = "#");
    hashes = end - at;
    at = end;
    if ((at >= size) || (text[at] != '"'))
    {
        const unsigned long long start = at;
        unsigned long long reserved = 0ull;
        while (EMBED_CALL(scan_word, CargoScanCtx,
                          .letter = EMBED_CALL(scan_letter, CargoScanCtx, .text = text, .size = size, .at_u64 = at,
                                               .reserved = &reserved)))
        {
            at += reserved;
        }
        EMBED_CALL(scan_append, CargoScanCtx, .kind = CLI_RUST_WORD, .letter = 0u, .start = start, .end = at,
                   .line = *line);
        return at;
    }
    at += 1ull;
    while (at < size)
    {
        unsigned long long closed = 0ull;
        if (text[at] == '"')
        {
            const unsigned long long hashed = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                         .text = text, .at = at + 1ull, .size = size, .set = "#") -
                                              (at + 1ull);
            closed = (hashed < hashes) ? hashed : hashes;
        }
        if ((text[at] == '"') && (closed == hashes))
        {
            return at + 1ull + hashes;
        }
        *line += (text[at] == '\n') ? 1u : 0u;
        at += 1ull;
    }
    return at;
}

// the tokens of the text counted, each put where the tokens' cell fits it
static void scan_tokens_append(const CargoScanCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    g_cli_cargo_scan.token_count = 0u;
    unsigned int line = 0u;
    unsigned long long at = 0ull;
    // a sign the passing over looks for is ASCII, which no byte of a longer letter is, and they are passed byte by byte
    while (at < size)
    {
        unsigned long long reserved = 0ull;
        unsigned long long next_allocated = 0ull;
        const unsigned int c =
            EMBED_CALL(scan_letter, CargoScanCtx, .text = text, .size = size, .at_u64 = at, .reserved = &reserved);
        const unsigned int next = EMBED_CALL(scan_letter, CargoScanCtx, .text = text, .size = size,
                                             .at_u64 = at + reserved, .reserved = &next_allocated);
        if (c == '\n')
        {
            line += 1u;
            at += 1ull;
        }
        else if (EMBED_CALL(scan_white, CargoScanCtx, .letter = c))
        {
            at += reserved;
        }
        else if ((c == '/') && (next == '/'))
        {
            at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at,
                            .size = size, .set = "\n");
        }
        else if ((c == '/') && (next == '*'))
        {
            long long depth = 0ll;
            while (at < size)
            {
                if ((text[at] == '/') && (at + 1ull < size) && (text[at + 1ull] == '*'))
                {
                    depth += 1ll;
                    at += 2ull;
                }
                else if ((text[at] == '*') && (at + 1ull < size) && (text[at + 1ull] == '/'))
                {
                    depth -= 1ll;
                    at += 2ull;
                    if (depth == 0ll)
                    {
                        break;
                    }
                }
                else
                {
                    line += (text[at] == '\n') ? 1u : 0u;
                    at += 1ull;
                }
            }
        }
        else if (c == '"')
        {
            at += 1ull;
            at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at,
                            .size = size, .set = "\"\\\n");
            while ((at < size) && (text[at] != '"'))
            {
                at += (text[at] == '\\') ? 1ull : 0ull;
                line += ((at < size) && (text[at] == '\n')) ? 1u : 0u;
                at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at + 1ull,
                                .size = size, .set = "\"\\\n");
            }
            at += 1ull;
        }
        else if (c == '\'')
        {
            // a character, 'a' or '\n', or a lifetime, 'a, which is passed over as no token
            unsigned long long third_allocated = 0ull;
            const unsigned int third =
                EMBED_CALL(scan_letter, CargoScanCtx, .text = text, .size = size,
                           .at_u64 = at + reserved + next_allocated, .reserved = &third_allocated);
            if (next == '\\')
            {
                at += 2ull;
                at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at,
                                .size = size, .set = "'");
                at += 1ull;
            }
            else if (third == '\'')
            {
                at += reserved + next_allocated + third_allocated;
            }
            else
            {
                at += 1ull;
                unsigned long long word_allocated = 0ull;
                while (EMBED_CALL(scan_word, CargoScanCtx,
                                  .letter = EMBED_CALL(scan_letter, CargoScanCtx, .text = text, .size = size,
                                                       .at_u64 = at, .reserved = &word_allocated)))
                {
                    at += word_allocated;
                }
            }
        }
        else if (EMBED_CALL(scan_word, CargoScanCtx, .letter = c))
        {
            const unsigned long long start = at;
            unsigned long long word_allocated = reserved;
            while (EMBED_CALL(scan_word, CargoScanCtx,
                              .letter = EMBED_CALL(scan_letter, CargoScanCtx, .text = text, .size = size, .at_u64 = at,
                                                   .reserved = &word_allocated)))
            {
                at += word_allocated;
            }
            const unsigned long long length = at - start;
            const int raw =
                ((length == 1ull) && (text[start] == 'r')) ||
                ((length == 2ull) && (memor.cmp(&(MemoriaCfg){.src = text + start, .other = "br", .bytes = 2u}) == 0));
            const int byte = (length == 1ull) && (text[start] == 'b');
            const int quoted = (at < size) && (text[at] == '"');
            if ((raw && (quoted || ((at < size) && (text[at] == '#')))) || (byte && quoted))
            {
                // a byte string is passed over as a string is, from its quote
                at = byte ? at
                          : EMBED_CALL(scan_raw, CargoScanCtx, .text = text, .size = size, .at_u64 = at,
                                       .line_pointer = &line);
                continue;
            }
            EMBED_CALL(scan_append, CargoScanCtx, .kind = CLI_RUST_WORD, .letter = 0u, .start = start, .end = at,
                       .line = line);
        }
        else if ((c == ':') && (next == ':'))
        {
            EMBED_CALL(scan_append, CargoScanCtx, .kind = CLI_RUST_PATH, .letter = 0u, .start = at, .end = at,
                       .line = line);
            at += 2ull;
        }
        else
        {
            EMBED_CALL(scan_append, CargoScanCtx, .kind = CLI_RUST_MARK, .letter = c, .start = at, .end = at,
                       .line = line);
            at += reserved;
        }
    }
}

void cli_cargo_tokens(const CliCargoTestsCtx *args)
{
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.tokens, &g_cli_cargo_cells);
    g_cli_cargo_scan.tokens = NULL;
    g_cli_cargo_scan.token_capacity = 0ull;
    EMBED_CALL(scan_tokens_append, CargoScanCtx, .text = args->text, .size = args->size);
    const unsigned long long counted = g_cli_cargo_scan.token_count;
    g_cli_cargo_scan.tokens =
        (counted != 0ull) ? g_cli_cells->persistent_buf_alloc(counted * sizeof(CliRustToken), &g_cli_cargo_cells)
                          : NULL;
    g_cli_cargo_scan.token_capacity = (g_cli_cargo_scan.tokens != NULL) ? counted : 0ull;
    EMBED_CALL(scan_tokens_append, CargoScanCtx, .text = args->text, .size = args->size);
    g_cli_cargo_scan.token_count = (unsigned int)g_cli_cargo_scan.token_capacity;
}

void cli_cargo_scan_free(void)
{
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.tokens, &g_cli_cargo_cells);
    g_cli_cells->persistent_buf_release(g_cli_cargo_scan.items, &g_cli_cargo_cells);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
}

// whether the token at `at` is the sign `letter`, or the word `word`
EMBED_INLINE int scan_is_mark(const CargoScanCtx *args)
{
    return (args->at < g_cli_cargo_scan.token_count) && (g_cli_cargo_scan.tokens[args->at].kind == CLI_RUST_MARK) &&
           (g_cli_cargo_scan.tokens[args->at].letter == args->letter);
}

EMBED_INLINE int scan_is_word(const CargoScanCtx *args)
{
    const CliRustToken *const token = &g_cli_cargo_scan.tokens[args->at];
    return (args->at < g_cli_cargo_scan.token_count) && (token->kind == CLI_RUST_WORD) &&
           (token->size == cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text + token->start, .other = args->word, .bytes = token->size}) == 0);
}

// `length` bytes put in the names, ended by a 0, the names' cell containing them: where they start
EMBED_INLINE unsigned long long scan_name_append(const CargoScanCtx *args)
{
    const unsigned long long at = g_cli_cargo_scan.name_used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_cargo_scan.names + at, .src = args->text, .bytes = args->length});
    g_cli_cargo_scan.names[at + args->length] = '\0';
    g_cli_cargo_scan.name_used += args->length + 1ull;
    return at;
}

// a test or a module stored, with the modules written in the file it stands in, `count` of them by their tokens
EMBED_INLINE void scan_item_append(const CargoScanCtx *args)
{
    char joined[4096];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const CliRustToken *const token = &g_cli_cargo_scan.tokens[args->inline_names[at]];
        mmgr_line_put(&line, (at > 0u) ? "::" : "");
        mmgr_line_put_n(&line, args->text + token->start, token->size);
    }
    const CliRustToken *const named = &g_cli_cargo_scan.tokens[args->name];
    const unsigned long long names_max = g_cli_cargo_scan.name_used + line.at + 1ull + named->size + 1ull;
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

// an attribute passed over from its `#`: 1 into `marked` where its path, up to its first sign that is not `::`, ends in
// `test` and it is an outer one; where the text goes on
EMBED_INLINE unsigned int scan_attribute(const CargoScanCtx *args)
{
    int *marked = args->marked;
    const int inner = EMBED_CALL(scan_is_mark, CargoScanCtx, .at = args->at + 1u, .letter = '!');
    const unsigned int open = inner ? args->at + 2u : args->at + 1u;
    if (!EMBED_CALL(scan_is_mark, CargoScanCtx, .at = open, .letter = '['))
    {
        return args->at + 1u;
    }
    unsigned int last = CLI_RUST_NONE;
    for (unsigned int step = open + 1u; step < g_cli_cargo_scan.token_count; step += 1u)
    {
        const unsigned int kind = g_cli_cargo_scan.tokens[step].kind;
        if (kind == CLI_RUST_MARK)
        {
            break;
        }
        last = (kind == CLI_RUST_WORD) ? step : last;
    }
    if (!inner && (last != CLI_RUST_NONE) &&
        EMBED_CALL(scan_is_word, CargoScanCtx, .text = args->text, .at = last, .word = "test"))
    {
        *marked = 1;
    }
    long long brackets = 0ll;
    unsigned int step = open;
    while (step < g_cli_cargo_scan.token_count)
    {
        brackets += EMBED_CALL(scan_is_mark, CargoScanCtx, .at = step, .letter = '[') ? 1ll : 0ll;
        if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = step, .letter = ']'))
        {
            brackets -= 1ll;
            if (brackets == 0ll)
            {
                break;
            }
        }
        step += 1u;
    }
    return step + 1u;
}

unsigned int cli_cargo_scan(const CliCargoTestsCtx *args)
{
    if (g_cli_cargo_scan.items == NULL)
    {
        const unsigned long long item_bytes = (unsigned long long)CLI_CARGO_FOUND * sizeof(CliRustItem);
        char *const cell = g_cli_cells->persistent_buf_alloc(item_bytes + CLI_CARGO_TEXT, &g_cli_cargo_cells);
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
    unsigned long long depth = 0ull;
    int marked = 0;
    unsigned int at = 0u;
    while (at < g_cli_cargo_scan.token_count)
    {
        if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .letter = '#'))
        {
            at = EMBED_CALL(scan_attribute, CargoScanCtx, .text = args->text, .at = at, .marked = &marked);
            continue;
        }
        const int named =
            (at + 1u < g_cli_cargo_scan.token_count) && (g_cli_cargo_scan.tokens[at + 1u].kind == CLI_RUST_WORD);
        if (EMBED_CALL(scan_is_word, CargoScanCtx, .text = args->text, .at = at, .word = "mod") && named)
        {
            if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at + 2u, .letter = ';'))
            {
                EMBED_CALL(scan_item_append, CargoScanCtx, .text = args->text, .inline_names = inline_names,
                           .count = open, .name = at + 1u, .test = 0);
                at += 3u;
            }
            else if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at + 2u, .letter = '{') && (open < CLI_CARGO_INLINE))
            {
                inline_names[open] = at + 1u;
                inline_depths[open] = depth;
                open += 1u;
                depth += 1ull;
                at += 3u;
            }
            else
            {
                at += 2u;
            }
            marked = 0;
            continue;
        }
        if (EMBED_CALL(scan_is_word, CargoScanCtx, .text = args->text, .at = at, .word = "fn") && named)
        {
            if (marked)
            {
                EMBED_CALL(scan_item_append, CargoScanCtx, .text = args->text, .inline_names = inline_names,
                           .count = open, .name = at + 1u, .test = 1);
            }
            marked = 0;
            at += 2u;
            continue;
        }
        if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .letter = '{'))
        {
            depth += 1ull;
            marked = 0;
        }
        else if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .letter = '}'))
        {
            depth -= (depth > 0ull) ? 1ull : 0ull;
            open -= ((open > 0u) && (inline_depths[open - 1u] == depth)) ? 1u : 0u;
        }
        else if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .letter = ';'))
        {
            marked = 0;
        }
        at += 1u;
    }
    return g_cli_cargo_scan.item_count - first;
}
