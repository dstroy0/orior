// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_text.c: LaTeX text and what a make leaves read as Rust reads them: lines, whitespace and numbers, and the
// engine a text is made with
#include "latex_internal.h"

#include <stdlib.h>

int cli_tex_white(char letter)
{
    return (letter == ' ') || (letter == '\t') || (letter == '\n') || (letter == '\r') || (letter == '\f') ||
           (letter == '\v');
}

int cli_tex_line(const char *text, unsigned long long length, unsigned long long *at, unsigned long long *start,
                 unsigned long long *size)
{
    if (*at >= length)
    {
        return 0;
    }
    *start = *at;
    const char *const found = memor.chr(&(MemoriaCfg){.src = text + *at, .val = '\n', .bytes = length - *at});
    if (found == NULL)
    {
        *size = length - *at;
        *at = length;
        return 1;
    }
    unsigned long long end = (unsigned long long)(found - text);
    *at = end + 1ull;
    if ((end > *start) && (text[end - 1ull] == '\r'))
    {
        end -= 1ull;
    }
    *size = end - *start;
    return 1;
}

// `length` bytes and `with`, the case of ASCII letters aside
static int latex_named(const char *text, unsigned long long length, const char *with)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = with, .cap = CLI_CELLS_POOL}) != length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = ((text[at] >= 'A') && (text[at] <= 'Z')) ? (char)(text[at] - 'A' + 'a') : text[at];
        if (letter != with[at])
        {
            return 0;
        }
    }
    return 1;
}

unsigned long long cli_tex_digits(const char *text, unsigned long long length, unsigned long long at)
{
    unsigned long long count = 0ull;
    while ((at + count < length) && (text[at + count] >= '0') && (text[at + count] <= '9'))
    {
        count += 1ull;
    }
    return count;
}

int cli_tex_double(const char *text, unsigned long long length, double *value)
{
    unsigned long long at = ((length > 0ull) && ((text[0] == '+') || (text[0] == '-'))) ? 1ull : 0ull;
    const int word = latex_named(text + at, length - at, "inf") || latex_named(text + at, length - at, "infinity") ||
                     latex_named(text + at, length - at, "nan");
    if (!word)
    {
        const unsigned long long whole = cli_tex_digits(text, length, at);
        at += whole;
        unsigned long long part = 0ull;
        if ((at < length) && (text[at] == '.'))
        {
            part = cli_tex_digits(text, length, at + 1ull);
            at += 1ull + part;
        }
        if ((whole + part) == 0ull)
        {
            return 0;
        }
        if ((at < length) && ((text[at] == 'e') || (text[at] == 'E')))
        {
            at += 1ull;
            at += ((at < length) && ((text[at] == '+') || (text[at] == '-'))) ? 1ull : 0ull;
            const unsigned long long power = cli_tex_digits(text, length, at);
            if (power == 0ull)
            {
                return 0;
            }
            at += power;
        }
        if (at != length)
        {
            return 0;
        }
    }
    char copy[512];
    if (length >= sizeof(copy))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = copy, .src = text, .bytes = length});
    copy[length] = '\0';
    *value = strtod(copy, NULL);
    return 1;
}

int cli_tex_count(const char *text, unsigned long long length, unsigned long long most, unsigned long long *value)
{
    const unsigned long long at = ((length > 0ull) && (text[0] == '+')) ? 1ull : 0ull;
    if ((at == length) || (cli_tex_digits(text, length, at) != length - at))
    {
        return 0;
    }
    unsigned long long sum = 0ull;
    for (unsigned long long letter = at; letter < length; letter += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[letter] - '0');
        if (sum > (most - digit) / 10ull)
        {
            return 0;
        }
        sum = (sum * 10ull) + digit;
    }
    *value = sum;
    return 1;
}

int cli_tex_signed(const char *text, unsigned long long length, long long *value)
{
    const int negative = (length > 0ull) && (text[0] == '-');
    const unsigned long long at = (negative || ((length > 0ull) && (text[0] == '+'))) ? 1ull : 0ull;
    unsigned long long size = 0ull;
    // the most a negative number may be is one past the most a positive one may
    const unsigned long long most = negative ? 9223372036854775808ull : 9223372036854775807ull;
    if ((at == length) || (text[at] == '+') || !cli_tex_count(text + at, length - at, most, &size))
    {
        return 0;
    }
    // the size is in range of the sign, so negating it in 64 bits as unsigned and reading it as signed is exact
    *value = negative ? (long long)(0ull - size) : (long long)size;
    return 1;
}

// whether a line, before its comment, takes fontspec or unicode-math with a \usepackage
static int latex_takes_unicode(const char *line, unsigned long long size)
{
    unsigned long long cut = 0ull;
    while ((cut < size) && !((line[cut] == '%') && ((cut == 0ull) || (line[cut - 1ull] != '\\'))))
    {
        cut += 1ull;
    }
    static const char word[] = "\\usepackage";
    const unsigned long long word_size = sizeof(word) - 1ull;
    for (unsigned long long at = 0ull; at + word_size <= cut; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = line + at, .other = word, .bytes = word_size}) != 0)
        {
            continue;
        }
        unsigned long long rest = at + word_size;
        unsigned long long from = rest;
        while ((from < cut) && cli_tex_white(line[from]))
        {
            from += 1ull;
        }
        if ((from < cut) && (line[from] == '['))
        {
            const char *const closed =
                memor.chr(&(MemoriaCfg){.src = line + from + 1ull, .val = ']', .bytes = cut - from - 1ull});
            // the options never closed leave nothing after them
            rest = (closed != NULL) ? (unsigned long long)(closed - line) + 1ull : cut;
        }
        while ((rest < cut) && cli_tex_white(line[rest]))
        {
            rest += 1ull;
        }
        const char *const closed = ((rest < cut) && (line[rest] == '{'))
                                       ? memor.chr(&(MemoriaCfg){.src = line + rest, .val = '}', .bytes = cut - rest})
                                       : NULL;
        if (closed == NULL)
        {
            at += word_size - 1ull;
            continue;
        }
        const unsigned long long end = (unsigned long long)(closed - line);
        unsigned long long name = rest + 1ull;
        while (name <= end)
        {
            unsigned long long stop = name;
            while ((stop < end) && (line[stop] != ','))
            {
                stop += 1ull;
            }
            unsigned long long first = name;
            unsigned long long last = stop;
            while ((first < last) && cli_tex_white(line[first]))
            {
                first += 1ull;
            }
            while ((last > first) && cli_tex_white(line[last - 1ull]))
            {
                last -= 1ull;
            }
            if (((last - first == 8ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = line + first, .other = "fontspec", .bytes = 8u}) == 0)) ||
                ((last - first == 12ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = line + first, .other = "unicode-math", .bytes = 12u}) == 0)))
            {
                return 1;
            }
            name = stop + 1ull;
        }
        at += word_size - 1ull;
    }
    return 0;
}

const char *cli_latex_engine_of(const char *text, unsigned long long length)
{
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long size = 0ull;
    while (cli_tex_line(text, length, &at, &start, &size))
    {
        while ((size > 0ull) && cli_tex_white(text[start]))
        {
            start += 1ull;
            size -= 1ull;
        }
        while ((size > 0ull) && cli_tex_white(text[start + size - 1ull]))
        {
            size -= 1ull;
        }
        if (size == 0ull)
        {
            continue;
        }
        if (text[start] != '%')
        {
            break;
        }
        char said[32];
        unsigned long long kept = 0ull;
        int fits = 1;
        for (unsigned long long letter = start + 1ull; letter < start + size; letter += 1ull)
        {
            if (cli_tex_white(text[letter]))
            {
                continue;
            }
            if (kept + 1ull < sizeof(said))
            {
                said[kept] = text[letter];
                kept += 1ull;
            }
            else
            {
                fits = 0;
            }
        }
        const unsigned long long head = ((kept >= 12ull) && latex_named(said, 12ull, "!texprogram="))      ? 12ull
                                        : ((kept >= 15ull) && latex_named(said, 15ull, "!texts-program=")) ? 15ull
                                                                                                           : 0ull;
        if (!fits || (head == 0ull))
        {
            continue;
        }
        if (latex_named(said + head, kept - head, "pdflatex") || latex_named(said + head, kept - head, "latex"))
        {
            return "pdflatex";
        }
        if (latex_named(said + head, kept - head, "xelatex"))
        {
            return "xelatex";
        }
        if (latex_named(said + head, kept - head, "lualatex"))
        {
            return "lualatex";
        }
    }
    at = 0ull;
    while (cli_tex_line(text, length, &at, &start, &size))
    {
        if (latex_takes_unicode(text + start, size))
        {
            return "lualatex";
        }
    }
    return "pdflatex";
}
