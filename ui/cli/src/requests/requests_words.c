// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_words.c: the words of a shell line as bash reads them: quoted in single quotes, double quotes or `$'...'`,
// a backslash taking the letter after it, and a backslash at the end of a line joining it to the next; and a line
// copied for cmd read as bash reads it, each caret taking the letter after it as it is and a caret at the end of a
// line joining it to the next
#include "../text/json_tree.h"
#include "requests_internal.h"

// whether a letter is white as Rust's char::is_whitespace has it
static int words_white(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

// the bytes of the letter at `at` of `size` bytes, one at the fewest
static unsigned long long words_letter(const char *line, unsigned long long size, unsigned long long at,
                                       unsigned int *letter)
{
    return cli_lossy_letter((const unsigned char *)line, size, at, letter);
}

// `size` bytes put on the word being read: 1, or 0 where the pool is full
static int words_put(CliRequestsWords *words, const char *bytes, unsigned long long size)
{
    if (words->used + size + 1u > sizeof(words->text))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = words->text + words->used, .src = bytes, .bytes = (size_t)size});
    words->used += size;
    return 1;
}

// the word being read ended and kept: 1, or 0 where the pool or the list is full
static int words_ended(CliRequestsWords *words, unsigned long long *start)
{
    if ((words->count == CLI_REQUESTS_CURL_WORDS) || (words->used + 1u > sizeof(words->text)))
    {
        return 0;
    }
    words->word[words->count].at = *start;
    words->word[words->count].size = words->used - *start;
    words->text[words->used] = '\0';
    words->used += 1u;
    words->count += 1u;
    *start = words->used;
    return 1;
}

// a `$'...'` quote read from its letters at `*at`, past its closing quote, onto the word: 1, or 0 where the pool is
// full
static int words_dollar(const char *line, unsigned long long size, unsigned long long *at, CliRequestsWords *words)
{
    int fits = 1;
    while (fits && (*at < size) && (line[*at] != '\''))
    {
        if ((line[*at] == '\\') && (*at + 1u < size))
        {
            *at += 1u;
            const char letter = line[*at];
            const char plain = (letter == 'n')   ? '\n'
                               : (letter == 't') ? '\t'
                               : (letter == 'r') ? '\r'
                               : (letter == '0') ? '\0'
                                                 : letter;
            unsigned int whole = 0u;
            const unsigned long long taken = words_letter(line, size, *at, &whole);
            fits = (plain != letter) ? words_put(words, &plain, 1u) : words_put(words, line + *at, taken);
            *at += taken;
        }
        else
        {
            unsigned int whole = 0u;
            const unsigned long long taken = words_letter(line, size, *at, &whole);
            fits = words_put(words, line + *at, taken);
            *at += taken;
        }
    }
    *at += 1u;
    return fits;
}

// a double quote read from its letters at `*at`, past its closing quote, onto the word: 1, 0 where the pool is full,
// or -1 where it does not close
static int words_double(const char *line, unsigned long long size, unsigned long long *at, CliRequestsWords *words)
{
    int fits = 1;
    while (fits && (*at < size) && (line[*at] != '"'))
    {
        if ((line[*at] == '\\') && (*at + 1u < size) &&
            (cellul.chr(&(CatenaFinitaCfg){.src = "\"\\$`", .cap = CLI_CELLS_POOL, .byte = line[*at + 1u]}) != NULL) &&
            (line[*at + 1u] != '\0'))
        {
            *at += 1u;
        }
        unsigned int whole = 0u;
        const unsigned long long taken = words_letter(line, size, *at, &whole);
        fits = words_put(words, line + *at, taken);
        *at += taken;
    }
    if (*at >= size)
    {
        return -1;
    }
    *at += 1u;
    return fits;
}

int cli_requests_words(const char *line, unsigned long long size, CliRequestsWords *words, CliLine *said)
{
    words->count = 0u;
    words->used = 0ull;
    unsigned long long start = 0ull;
    int started = 0;
    int read = 1;
    unsigned long long at = 0ull;
    while ((read == 1) && (at < size))
    {
        const char letter = line[at];
        const int joined =
            (letter == '\\') && (at + 1u < size) &&
            ((line[at + 1u] == '\n') || ((line[at + 1u] == '\r') && (at + 2u < size) && (line[at + 2u] == '\n')));
        if (joined)
        {
            at += (line[at + 1u] == '\r') ? 3u : 2u;
        }
        else if (letter == '\\')
        {
            unsigned int whole = 0u;
            const unsigned long long taken = (at + 1u < size) ? words_letter(line, size, at + 1u, &whole) : 0u;
            read = (taken == 0u) || words_put(words, line + at + 1u, taken);
            started = started || (taken > 0u);
            at += 1u + ((taken > 0u) ? taken : 1u);
        }
        else if (letter == '\'')
        {
            const char *const close =
                memor.chr(&(MemoriaCfg){.src = line + at + 1u, .val = '\'', .bytes = (size_t)(size - at - 1u)});
            if (close == NULL)
            {
                read = -1;
                break;
            }
            const unsigned long long end = (unsigned long long)(close - line);
            read = words_put(words, line + at + 1u, end - at - 1u);
            started = 1;
            at = end + 1u;
        }
        else if ((letter == '$') && (at + 1u < size) && (line[at + 1u] == '\''))
        {
            at += 2u;
            read = words_dollar(line, size, &at, words);
            started = 1;
        }
        else if (letter == '"')
        {
            at += 1u;
            read = words_double(line, size, &at, words);
            started = 1;
        }
        else
        {
            unsigned int whole = 0u;
            const unsigned long long taken = words_letter(line, size, at, &whole);
            if (words_white(whole))
            {
                read = !started || words_ended(words, &start);
                started = 0;
            }
            else
            {
                read = words_put(words, line + at, taken);
                started = 1;
            }
            at += taken;
        }
    }
    read = ((read == 1) && started) ? words_ended(words, &start) : read;
    if (read == -1)
    {
        cli_line_text(said, "a quote does not close");
    }
    else if (read == 0)
    {
        cli_line_text(said, "the curl line is larger than orior_cli holds");
    }
    return read == 1;
}

void cli_requests_uncareted(const char *line, unsigned long long size, CliLine *out)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        if ((line[at] == '^') && (at + 1u < size))
        {
            const char next = line[at + 1u];
            if ((next == '\r') || (next == '\n'))
            {
                at += 2u;
                at += ((next == '\r') && (at < size) && (line[at] == '\n')) ? 1u : 0u;
                continue;
            }
            at += 1u;
        }
        unsigned int whole = 0u;
        const unsigned long long taken = words_letter(line, size, at, &whole);
        cli_line_counted(out, line + at, taken);
        at += taken;
    }
}
