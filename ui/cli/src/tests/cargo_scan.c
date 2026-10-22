// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_scan.c: a file of Rust read as the search for tests reads it: its tokens, comments, strings and characters
// passed over, and the tests and the modules it holds
#include "cargo_tests_internal.h"

CliCargoScan g_cli_cargo_scan;
CliCellsTally g_cli_cargo_cells = CLI_CELLS_TALLY("cli_cargo_tests");

void *cli_cargo_grown(void *cell, unsigned long long *room, unsigned long long most, unsigned long long width,
                      const char *what)
{
    const unsigned long long doubled = 2ull * *room;
    const unsigned long long taken = (most > doubled) ? most : doubled;
    void *const grown = cli_cell_taken_by(&g_cli_cargo_cells, taken * width, what);
    if (grown == NULL)
    {
        return NULL;
    }
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = cell, .bytes = (size_t)(*room * width)});
        cli_cell_given_by(&g_cli_cargo_cells, cell);
    }
    *room = taken;
    return grown;
}

// the most modules written in one another at once
#define CLI_CARGO_INLINE 256u

// no letter, past a text's end
#define CLI_RUST_NONE 0xffffffffu

// the letter at `at` of `size` bytes and the bytes it takes, CLI_RUST_NONE past the end
static unsigned int scan_letter(const char *text, unsigned long long size, unsigned long long at,
                                unsigned long long *taken)
{
    if (at >= size)
    {
        *taken = 0ull;
        return CLI_RUST_NONE;
    }
    unsigned int letter = 0u;
    *taken = cli_lossy_letter((const unsigned char *)text, size, at, &letter);
    return letter;
}

// whether a letter is whitespace, as Unicode has it
static int scan_white(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

// whether a letter is one a word holds: an ASCII letter, digit or `_`, or a letter past ASCII that is no whitespace,
// punctuation, arrow, mathematical sign or drawing
static int scan_word(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
               ((letter >= '0') && (letter <= '9')) || (letter == '_');
    }
    return (letter != CLI_RUST_NONE) && !scan_white(letter) && !((letter >= 0x2000u) && (letter <= 0x2bffu)) &&
           !((letter >= 0x3000u) && (letter <= 0x303fu)) && !((letter >= 0xa0u) && (letter <= 0xbfu));
}

// a token counted, and put where the tokens' cell holds one more
static void scan_put(unsigned int kind, unsigned int letter, unsigned long long start, unsigned long long end,
                     unsigned int line)
{
    if (g_cli_cargo_scan.token_count < g_cli_cargo_scan.token_room)
    {
        g_cli_cargo_scan.tokens[g_cli_cargo_scan.token_count] =
            (CliRustToken){kind, letter, start, (unsigned int)(end - start), line};
    }
    g_cli_cargo_scan.token_count += 1u;
}

// a raw string or a byte string, `r".."`, `r#".."#`, `b".."` or `br#".."#`, passed over from `at`, at the hashes or
// the quote after its prefix, or the raw word `r#name` put: where the text goes on
static unsigned long long scan_raw(const char *text, unsigned long long size, unsigned long long at, unsigned int *line)
{
    unsigned long long hashes = 0ull;
    while ((at < size) && (text[at] == '#'))
    {
        hashes += 1ull;
        at += 1ull;
    }
    if ((at >= size) || (text[at] != '"'))
    {
        const unsigned long long start = at;
        unsigned long long taken = 0ull;
        while (scan_word(scan_letter(text, size, at, &taken)))
        {
            at += taken;
        }
        scan_put(CLI_RUST_WORD, 0u, start, at, *line);
        return at;
    }
    at += 1ull;
    while (at < size)
    {
        unsigned long long closed = 0ull;
        while ((text[at] == '"') && (closed < hashes) && (at + 1ull + closed < size) &&
               (text[at + 1ull + closed] == '#'))
        {
            closed += 1ull;
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

// the tokens of the text counted, each put where the tokens' cell holds it
static void scan_tokens_put(const char *text, unsigned long long size)
{
    g_cli_cargo_scan.token_count = 0u;
    unsigned int line = 0u;
    unsigned long long at = 0ull;
    // a sign the passing over looks for is ASCII, which no byte of a longer letter is, so they are passed byte by byte
    while (at < size)
    {
        unsigned long long taken = 0ull;
        unsigned long long next_taken = 0ull;
        const unsigned int c = scan_letter(text, size, at, &taken);
        const unsigned int next = scan_letter(text, size, at + taken, &next_taken);
        if (c == '\n')
        {
            line += 1u;
            at += 1ull;
        }
        else if (scan_white(c))
        {
            at += taken;
        }
        else if ((c == '/') && (next == '/'))
        {
            while ((at < size) && (text[at] != '\n'))
            {
                at += 1ull;
            }
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
            while ((at < size) && (text[at] != '"'))
            {
                at += (text[at] == '\\') ? 1ull : 0ull;
                line += ((at < size) && (text[at] == '\n')) ? 1u : 0u;
                at += 1ull;
            }
            at += 1ull;
        }
        else if (c == '\'')
        {
            // a character, 'a' or '\n', or a lifetime, 'a, which is passed over as no token
            unsigned long long third_taken = 0ull;
            const unsigned int third = scan_letter(text, size, at + taken + next_taken, &third_taken);
            if (next == '\\')
            {
                at += 2ull;
                while ((at < size) && (text[at] != '\''))
                {
                    at += 1ull;
                }
                at += 1ull;
            }
            else if (third == '\'')
            {
                at += taken + next_taken + third_taken;
            }
            else
            {
                at += 1ull;
                unsigned long long word_taken = 0ull;
                while (scan_word(scan_letter(text, size, at, &word_taken)))
                {
                    at += word_taken;
                }
            }
        }
        else if (scan_word(c))
        {
            const unsigned long long start = at;
            unsigned long long word_taken = taken;
            while (scan_word(scan_letter(text, size, at, &word_taken)))
            {
                at += word_taken;
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
                at = byte ? at : scan_raw(text, size, at, &line);
                continue;
            }
            scan_put(CLI_RUST_WORD, 0u, start, at, line);
        }
        else if ((c == ':') && (next == ':'))
        {
            scan_put(CLI_RUST_PATH, 0u, at, at, line);
            at += 2ull;
        }
        else
        {
            scan_put(CLI_RUST_MARK, c, at, at, line);
            at += taken;
        }
    }
}

void cli_cargo_tokens(const char *text, unsigned long long size)
{
    cli_cell_given_by(&g_cli_cargo_cells, g_cli_cargo_scan.tokens);
    g_cli_cargo_scan.tokens = NULL;
    g_cli_cargo_scan.token_room = 0ull;
    scan_tokens_put(text, size);
    const unsigned long long counted = g_cli_cargo_scan.token_count;
    g_cli_cargo_scan.tokens = (counted != 0ull) ? cli_cell_taken_by(&g_cli_cargo_cells, counted * sizeof(CliRustToken),
                                                                    "a Rust file's tokens")
                                                : NULL;
    g_cli_cargo_scan.token_room = (g_cli_cargo_scan.tokens != NULL) ? counted : 0ull;
    scan_tokens_put(text, size);
    g_cli_cargo_scan.token_count = (unsigned int)g_cli_cargo_scan.token_room;
}

void cli_cargo_scan_given(void)
{
    cli_cell_given_by(&g_cli_cargo_cells, g_cli_cargo_scan.tokens);
    cli_cell_given_by(&g_cli_cargo_cells, g_cli_cargo_scan.held);
    cli_cell_given_by(&g_cli_cargo_cells, g_cli_cargo_scan.names);
    memor.set(&(MemoriaCfg){.dst = &g_cli_cargo_scan, .val = 0, .bytes = sizeof(g_cli_cargo_scan)});
}

// whether the token at `at` is the sign `letter`, or the word `word`
static int scan_is_mark(unsigned int at, unsigned int letter)
{
    return (at < g_cli_cargo_scan.token_count) && (g_cli_cargo_scan.tokens[at].kind == CLI_RUST_MARK) &&
           (g_cli_cargo_scan.tokens[at].letter == letter);
}

static int scan_is_word(const char *text, unsigned int at, const char *word)
{
    const CliRustToken *const token = &g_cli_cargo_scan.tokens[at];
    return (at < g_cli_cargo_scan.token_count) && (token->kind == CLI_RUST_WORD) &&
           (token->size == cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = text + token->start, .other = word, .bytes = token->size}) == 0);
}

// `length` bytes put in the names, ended by a 0, the names' cell holding them: where they start
static unsigned long long scan_name_put(const char *text, unsigned long long length)
{
    const unsigned long long at = g_cli_cargo_scan.name_used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_cargo_scan.names + at, .src = text, .bytes = length});
    g_cli_cargo_scan.names[at + length] = '\0';
    g_cli_cargo_scan.name_used += length + 1ull;
    return at;
}

// a test or a module held, with the modules written in the file it stands in, `count` of them by their tokens
static void scan_held(const char *text, const unsigned int *inline_names, unsigned int count, unsigned int name,
                      int test)
{
    char joined[4096];
    CliLine line = {joined, sizeof(joined), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliRustToken *const token = &g_cli_cargo_scan.tokens[inline_names[at]];
        cli_line_text(&line, (at > 0u) ? "::" : "");
        cli_line_counted(&line, text + token->start, token->size);
    }
    const CliRustToken *const named = &g_cli_cargo_scan.tokens[name];
    const unsigned long long held_most = g_cli_cargo_scan.held_count + 1ull;
    if (held_most > g_cli_cargo_scan.held_room)
    {
        CliRustHeld *const grown = cli_cargo_grown(g_cli_cargo_scan.held, &g_cli_cargo_scan.held_room, held_most,
                                                   sizeof(CliRustHeld), "the tests and modules followed");
        if (grown == NULL)
        {
            return;
        }
        g_cli_cargo_scan.held = grown;
    }
    const unsigned long long names_most = g_cli_cargo_scan.name_used + line.at + 1ull + named->size + 1ull;
    if (names_most > g_cli_cargo_scan.name_room)
    {
        char *const grown = cli_cargo_grown(g_cli_cargo_scan.names, &g_cli_cargo_scan.name_room, names_most, 1ull,
                                            "the names of the tests and modules followed");
        if (grown == NULL)
        {
            return;
        }
        g_cli_cargo_scan.names = grown;
    }
    const unsigned long long inline_at = scan_name_put(joined, line.at);
    const unsigned long long name_at = scan_name_put(text + named->start, named->size);
    g_cli_cargo_scan.held[g_cli_cargo_scan.held_count] = (CliRustHeld){test, inline_at, name_at, named->line};
    g_cli_cargo_scan.held_count += 1u;
}

// an attribute passed over from its `#`: 1 into `marked` where its path, up to its first sign that is not `::`, ends in
// `test` and it is an outer one; where the text goes on
static unsigned int scan_attribute(const char *text, unsigned int at, int *marked)
{
    const int inner = scan_is_mark(at + 1u, '!');
    const unsigned int open = inner ? at + 2u : at + 1u;
    if (!scan_is_mark(open, '['))
    {
        return at + 1u;
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
    if (!inner && (last != CLI_RUST_NONE) && scan_is_word(text, last, "test"))
    {
        *marked = 1;
    }
    long long brackets = 0ll;
    unsigned int step = open;
    while (step < g_cli_cargo_scan.token_count)
    {
        brackets += scan_is_mark(step, '[') ? 1ll : 0ll;
        if (scan_is_mark(step, ']'))
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

unsigned int cli_cargo_scan(const char *text)
{
    const unsigned int first = g_cli_cargo_scan.held_count;
    unsigned int inline_names[CLI_CARGO_INLINE];
    unsigned long long inline_depths[CLI_CARGO_INLINE];
    unsigned int open = 0u;
    unsigned long long depth = 0ull;
    int marked = 0;
    unsigned int at = 0u;
    while (at < g_cli_cargo_scan.token_count)
    {
        if (scan_is_mark(at, '#'))
        {
            at = scan_attribute(text, at, &marked);
            continue;
        }
        const int named =
            (at + 1u < g_cli_cargo_scan.token_count) && (g_cli_cargo_scan.tokens[at + 1u].kind == CLI_RUST_WORD);
        if (scan_is_word(text, at, "mod") && named)
        {
            if (scan_is_mark(at + 2u, ';'))
            {
                scan_held(text, inline_names, open, at + 1u, 0);
                at += 3u;
            }
            else if (scan_is_mark(at + 2u, '{') && (open < CLI_CARGO_INLINE))
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
        if (scan_is_word(text, at, "fn") && named)
        {
            if (marked)
            {
                scan_held(text, inline_names, open, at + 1u, 1);
            }
            marked = 0;
            at += 2u;
            continue;
        }
        if (scan_is_mark(at, '{'))
        {
            depth += 1ull;
            marked = 0;
        }
        else if (scan_is_mark(at, '}'))
        {
            depth -= (depth > 0ull) ? 1ull : 0ull;
            open -= ((open > 0u) && (inline_depths[open - 1u] == depth)) ? 1u : 0u;
        }
        else if (scan_is_mark(at, ';'))
        {
            marked = 0;
        }
        at += 1u;
    }
    return g_cli_cargo_scan.held_count - first;
}
