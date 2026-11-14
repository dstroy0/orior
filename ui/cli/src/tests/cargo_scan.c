// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_scan.c: a file of Rust read as the search for tests reads it: its tokens, comments, strings and characters
// passed over, and the tests and the modules it contains
#include "../text/text.h"
#include "cargo_tests_internal.h"

CliCargoScan g_cli_cargo_scan;

// the max modules written in one another at once
#define CLI_CARGO_INLINE 256u

// no character, past a text's end
#define CLI_RUST_NONE 0xffffffffu

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at_u64;
    unsigned long long *reserved;
    unsigned int character;
    unsigned int type;
    unsigned long long start;
    unsigned long long end;
    unsigned int line;
    unsigned int *line_pointer;
    unsigned int at;
    const char *word_text;
    unsigned long long length;
    const unsigned int *inline_names;
    unsigned int count;
    unsigned int name;
    int test;
    int *marked;
    unsigned int *opened_names;
    unsigned long long *opened_depths;
    unsigned int *open_pointer;
    unsigned long long *depth_pointer;
} CargoScanCtx;
// the character at `at` of `size` bytes and the bytes it takes, CLI_RUST_NONE past the end
EMBED_INLINE unsigned int scan_character(const CargoScanCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at_u64;
    unsigned long long *reserved = args->reserved;
    if (at >= args->size)
    {
        *reserved = 0ULL;
        return CLI_RUST_NONE;
    }
    unsigned int character = 0u;
    *reserved =
        EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = (const unsigned char *)text,
                   .length = args->size, .at = at, .character = &character);
    return character;
}

// whether a character is whitespace, as Unicode has it
EMBED_INLINE int scan_white(const CargoScanCtx *args)
{
    return ((args->character >= 0x09u) && (args->character <= 0x0du)) || (args->character == 0x20u) || (args->character == 0x85u) ||
           (args->character == 0xa0u) || (args->character == 0x1680u) ||
           ((args->character >= 0x2000u) && (args->character <= 0x200au)) || (args->character == 0x2028u) ||
           (args->character == 0x2029u) || (args->character == 0x202fu) || (args->character == 0x205fu) ||
           (args->character == 0x3000u);
}

// whether a character is one a word contains: an ASCII character, digit or `_`, or a character past ASCII that is no whitespace,
// punctuation, arrow, mathematical sign or drawing
EMBED_INLINE int scan_word(const CargoScanCtx *args)
{
    if (args->character < 0x80u)
    {
        return ((args->character >= 'a') && (args->character <= 'z')) || ((args->character >= 'A') && (args->character <= 'Z')) ||
               ((args->character >= '0') && (args->character <= '9')) || (args->character == '_');
    }
    return (args->character != CLI_RUST_NONE) && !scan_white(args) &&
           !((args->character >= 0x2000u) && (args->character <= 0x2bffu)) &&
           !((args->character >= 0x3000u) && (args->character <= 0x303fu)) &&
           !((args->character >= 0xa0u) && (args->character <= 0xbfu));
}

// a token counted, and put where the tokens' cell fits one more
EMBED_INLINE void scan_append(const CargoScanCtx *args)
{
    if (g_cli_cargo_scan.token_count < g_cli_cargo_scan.token_capacity)
    {
        g_cli_cargo_scan.tokens[g_cli_cargo_scan.token_count] =
            (CliRustToken){args->type, args->character, args->start, (unsigned int)(args->end - args->start), args->line};
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
    unsigned long long hashes = 0ULL;
    const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text,
                                              .at = at, .size = size, .set = "#");
    hashes = end - at;
    at = end;
    if ((at >= size) || (text[at] != '"'))
    {
        const unsigned long long start = at;
        unsigned long long reserved = 0ULL;
        while (EMBED_CALL(scan_word, CargoScanCtx,
                          .character = EMBED_CALL(scan_character, CargoScanCtx, .text = text, .size = size, .at_u64 = at,
                                               .reserved = &reserved)))
        {
            at += reserved;
        }
        EMBED_CALL(scan_append, CargoScanCtx, .type = CLI_RUST_WORD, .character = 0u, .start = start, .end = at,
                   .line = *line);
        return at;
    }
    at += 1ULL;
    while (at < size)
    {
        unsigned long long closed = 0ULL;
        if (text[at] == '"')
        {
            const unsigned long long hashed = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                         .text = text, .at = at + 1ULL, .size = size, .set = "#") -
                                              (at + 1ULL);
            closed = (hashed < hashes) ? hashed : hashes;
        }
        if ((text[at] == '"') && (closed == hashes))
        {
            return at + 1ULL + hashes;
        }
        *line += (text[at] == '\n') ? 1u : 0u;
        at += 1ULL;
    }
    return at;
}

// the words' characters from `at` passed over: where the text goes on
EMBED_INLINE unsigned long long scan_word_end(const CargoScanCtx *args)
{
    unsigned long long at = args->at_u64;
    unsigned long long word_allocated = 0ULL;
    while (EMBED_CALL(scan_word, CargoScanCtx,
                      .character = EMBED_CALL(scan_character, CargoScanCtx, .text = args->text, .size = args->size,
                                           .at_u64 = at, .reserved = &word_allocated)))
    {
        at += word_allocated;
    }
    return at;
}

// a block comment passed over from its opening, those written in it with it, each line it ends counted into
// `line_pointer`: where the text goes on
EMBED_INLINE unsigned long long scan_block_comment(const CargoScanCtx *args)
{
    const char *text = args->text;
    const unsigned long long size = args->size;
    unsigned long long at = args->at_u64;
    long long depth = 0LL;
    while (at < size)
    {
        const int opens = (text[at] == '/') && (at + 1ULL < size) && (text[at + 1ULL] == '*');
        const int closes = !opens && (text[at] == '*') && (at + 1ULL < size) && (text[at + 1ULL] == '/');
        if (!opens && !closes)
        {
            *args->line_pointer += (text[at] == '\n') ? 1u : 0u;
            at += 1ULL;
            continue;
        }
        depth += opens ? 1LL : -1LL;
        at += 2ULL;
        if (depth == 0LL)
        {
            break;
        }
    }
    return at;
}

// a string passed over from its quote, its escapes with it, each line it ends counted into `line_pointer`: where the
// text goes on
EMBED_INLINE unsigned long long scan_string(const CargoScanCtx *args)
{
    const char *text = args->text;
    const unsigned long long size = args->size;
    unsigned long long at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text,
                                       .at = args->at_u64 + 1ULL, .size = size, .set = "\"\\\n");
    while ((at < size) && (text[at] != '"'))
    {
        at += (text[at] == '\\') ? 1ULL : 0ULL;
        *args->line_pointer += ((at < size) && (text[at] == '\n')) ? 1u : 0u;
        at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at + 1ULL,
                        .size = size, .set = "\"\\\n");
    }
    return at + 1ULL;
}

// a character, 'a' or '\n', or a lifetime, 'a, passed over from its quote as no token: where the text goes on
EMBED_INLINE unsigned long long scan_quote(const CargoScanCtx *args)
{
    const char *text = args->text;
    const unsigned long long size = args->size;
    const unsigned long long at = args->at_u64;
    unsigned long long next_bytes = 0ULL;
    const unsigned int next =
        EMBED_CALL(scan_character, CargoScanCtx, .text = text, .size = size, .at_u64 = at + 1ULL, .reserved = &next_bytes);
    unsigned long long third_bytes = 0ULL;
    const unsigned int third = EMBED_CALL(scan_character, CargoScanCtx, .text = text, .size = size,
                                          .at_u64 = at + 1ULL + next_bytes, .reserved = &third_bytes);
    if (next == '\\')
    {
        return EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = at + 2ULL,
                          .size = size, .set = "'") +
               1ULL;
    }
    if (third == '\'')
    {
        return at + 1ULL + next_bytes + third_bytes;
    }
    return EMBED_CALL(scan_word_end, CargoScanCtx, .text = text, .size = size, .at_u64 = at + 1ULL);
}

// a word from `at` put as a token on the line `line_pointer` gives, or the raw or byte string it begins passed over:
// where the text goes on
EMBED_INLINE unsigned long long scan_word_token(const CargoScanCtx *args)
{
    const char *text = args->text;
    const unsigned long long size = args->size;
    const unsigned long long start = args->at_u64;
    const unsigned long long at = EMBED_CALL(scan_word_end, CargoScanCtx, .text = text, .size = size, .at_u64 = start);
    const unsigned long long length = at - start;
    const int raw =
        ((length == 1ULL) && (text[start] == 'r')) ||
        ((length == 2ULL) && (memor.cmp(&(MemoriaCfg){.src = text + start, .other = "br", .bytes = 2u}) == 0));
    const int byte = (length == 1ULL) && (text[start] == 'b');
    const int quoted = (at < size) && (text[at] == '"');
    if (byte && quoted)
    {
        // a byte string is passed over as a string is, from its quote
        return at;
    }
    if (raw && (quoted || ((at < size) && (text[at] == '#'))))
    {
        return EMBED_CALL(scan_raw, CargoScanCtx, .text = text, .size = size, .at_u64 = at,
                          .line_pointer = args->line_pointer);
    }
    EMBED_CALL(scan_append, CargoScanCtx, .type = CLI_RUST_WORD, .character = 0u, .start = start, .end = at,
               .line = *args->line_pointer);
    return at;
}

// the tokens of the text counted, each put where the tokens' cell fits it
static void scan_tokens_append(const CargoScanCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    g_cli_cargo_scan.token_count = 0u;
    unsigned int line = 0u;
    unsigned long long at = 0ULL;
    // a sign the passing over looks for is ASCII, which no byte of a longer character is, and they are passed byte by byte
    while (at < size)
    {
        unsigned long long reserved = 0ULL;
        unsigned long long next_allocated = 0ULL;
        const unsigned int c =
            EMBED_CALL(scan_character, CargoScanCtx, .text = text, .size = size, .at_u64 = at, .reserved = &reserved);
        const unsigned int next = EMBED_CALL(scan_character, CargoScanCtx, .text = text, .size = size,
                                             .at_u64 = at + reserved, .reserved = &next_allocated);
        if (c == '\n')
        {
            line += 1u;
            at += 1ULL;
        }
        else if (EMBED_CALL(scan_white, CargoScanCtx, .character = c))
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
            at = EMBED_CALL(scan_block_comment, CargoScanCtx, .text = text, .size = size, .at_u64 = at,
                            .line_pointer = &line);
        }
        else if (c == '"')
        {
            at = EMBED_CALL(scan_string, CargoScanCtx, .text = text, .size = size, .at_u64 = at, .line_pointer = &line);
        }
        else if (c == '\'')
        {
            at = EMBED_CALL(scan_quote, CargoScanCtx, .text = text, .size = size, .at_u64 = at);
        }
        else if (EMBED_CALL(scan_word, CargoScanCtx, .character = c))
        {
            at = EMBED_CALL(scan_word_token, CargoScanCtx, .text = text, .size = size, .at_u64 = at,
                            .line_pointer = &line);
        }
        else if ((c == ':') && (next == ':'))
        {
            EMBED_CALL(scan_append, CargoScanCtx, .type = CLI_RUST_PATH, .character = 0u, .start = at, .end = at,
                       .line = line);
            at += 2ULL;
        }
        else
        {
            EMBED_CALL(scan_append, CargoScanCtx, .type = CLI_RUST_MARK, .character = c, .start = at, .end = at,
                       .line = line);
            at += reserved;
        }
    }
}

void cli_cargo_tokens(const CliCargoTestsCtx *args)
{
    g_cli_guard->persistent_buf_release(g_cli_cargo_scan.tokens, g_cli_cargo_owner);
    g_cli_cargo_scan.tokens = NULL;
    g_cli_cargo_scan.token_capacity = 0ULL;
    EMBED_CALL(scan_tokens_append, CargoScanCtx, .text = args->text, .size = args->size);
    const unsigned long long counted = g_cli_cargo_scan.token_count;
    g_cli_cargo_scan.tokens =
        (counted != 0ULL) ? g_cli_guard->temporary_buf_alloc(counted * sizeof(CliRustToken), g_cli_cargo_owner) : NULL;
    g_cli_cargo_scan.token_capacity = (g_cli_cargo_scan.tokens != NULL) ? counted : 0ULL;
    EMBED_CALL(scan_tokens_append, CargoScanCtx, .text = args->text, .size = args->size);
    g_cli_cargo_scan.token_count = (unsigned int)g_cli_cargo_scan.token_capacity;
}

// whether the token at `at` is the sign `character`, or the word `word`
EMBED_INLINE int scan_is_mark(const CargoScanCtx *args)
{
    return (args->at < g_cli_cargo_scan.token_count) && (g_cli_cargo_scan.tokens[args->at].type == CLI_RUST_MARK) &&
           (g_cli_cargo_scan.tokens[args->at].character == args->character);
}

EMBED_INLINE int scan_is_word(const CargoScanCtx *args)
{
    const CliRustToken *const token = &g_cli_cargo_scan.tokens[args->at];
    return (args->at < g_cli_cargo_scan.token_count) && (token->type == CLI_RUST_WORD) &&
           (token->size == cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES})) &&
           (memor.cmp(
                &(MemoriaCfg){.src = args->text + token->start, .other = args->word_text, .bytes = token->size}) == 0);
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

// an attribute passed over from its `#`: 1 into `marked` where its path, up to its first sign that is not `::`, ends in
// `test` and it is an outer one; where the text goes on
EMBED_INLINE unsigned int scan_attribute(const CargoScanCtx *args)
{
    int *marked = args->marked;
    const int inner = EMBED_CALL(scan_is_mark, CargoScanCtx, .at = args->at + 1u, .character = '!');
    const unsigned int open = inner ? args->at + 2u : args->at + 1u;
    if (!EMBED_CALL(scan_is_mark, CargoScanCtx, .at = open, .character = '['))
    {
        return args->at + 1u;
    }
    unsigned int last = CLI_RUST_NONE;
    for (unsigned int step = open + 1u; step < g_cli_cargo_scan.token_count; step += 1u)
    {
        const unsigned int type = g_cli_cargo_scan.tokens[step].type;
        if (type == CLI_RUST_MARK)
        {
            break;
        }
        last = (type == CLI_RUST_WORD) ? step : last;
    }
    if (!inner && (last != CLI_RUST_NONE) &&
        EMBED_CALL(scan_is_word, CargoScanCtx, .text = args->text, .at = last, .word_text = "test"))
    {
        *marked = 1;
    }
    long long brackets = 0LL;
    unsigned int step = open;
    while (step < g_cli_cargo_scan.token_count)
    {
        brackets += EMBED_CALL(scan_is_mark, CargoScanCtx, .at = step, .character = '[') ? 1LL : 0LL;
        if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = step, .character = ']'))
        {
            brackets -= 1LL;
            if (brackets == 0LL)
            {
                break;
            }
        }
        step += 1u;
    }
    return step + 1u;
}

// the module the `mod` at `at` names: one in a file of its own stored, or one written here opened, its name put among
// those open with the depth it opens at; where the tokens go on
EMBED_INLINE unsigned int scan_module(const CargoScanCtx *args)
{
    const unsigned int at = args->at;
    unsigned int *const open = args->open_pointer;
    if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at + 2u, .character = ';'))
    {
        EMBED_CALL(scan_item_append, CargoScanCtx, .text = args->text, .inline_names = args->opened_names,
                   .count = *open, .name = at + 1u, .test = 0);
        return at + 3u;
    }
    if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at + 2u, .character = '{') && (*open < CLI_CARGO_INLINE))
    {
        args->opened_names[*open] = at + 1u;
        args->opened_depths[*open] = *args->depth_pointer;
        *open += 1u;
        *args->depth_pointer += 1ULL;
        return at + 3u;
    }
    return at + 2u;
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
        if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .character = '#'))
        {
            at = EMBED_CALL(scan_attribute, CargoScanCtx, .text = args->text, .at = at, .marked = &marked);
            continue;
        }
        const int named =
            (at + 1u < g_cli_cargo_scan.token_count) && (g_cli_cargo_scan.tokens[at + 1u].type == CLI_RUST_WORD);
        if (EMBED_CALL(scan_is_word, CargoScanCtx, .text = args->text, .at = at, .word_text = "mod") && named)
        {
            at = EMBED_CALL(scan_module, CargoScanCtx, .text = args->text, .at = at, .opened_names = inline_names,
                            .opened_depths = inline_depths, .open_pointer = &open, .depth_pointer = &depth);
            marked = 0;
            continue;
        }
        if (EMBED_CALL(scan_is_word, CargoScanCtx, .text = args->text, .at = at, .word_text = "fn") && named)
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
        if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .character = '{'))
        {
            depth += 1ULL;
            marked = 0;
        }
        else if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .character = '}'))
        {
            depth -= (depth > 0ULL) ? 1ULL : 0ULL;
            open -= ((open > 0u) && (inline_depths[open - 1u] == depth)) ? 1u : 0u;
        }
        else if (EMBED_CALL(scan_is_mark, CargoScanCtx, .at = at, .character = ';'))
        {
            marked = 0;
        }
        at += 1u;
    }
    return g_cli_cargo_scan.item_count - first;
}
