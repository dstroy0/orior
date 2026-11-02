// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_text.c: LaTeX text and what a make leaves read as Rust reads them: lines, whitespace and numbers, and the
// engine a text is made with
#include "../text/text.h"
#include "latex_internal.h"

#include <stdlib.h>

int cli_tex_white(const CliLatexCtx *args)
{
    return (args->letter == ' ') || (args->letter == '\t') || (args->letter == '\n') || (args->letter == '\r') ||
           (args->letter == '\f') || (args->letter == '\v');
}

int cli_tex_line(const CliLatexTextCtx *args)
{
    unsigned long long *at = args->at;
    unsigned long long *start = args->start;
    unsigned long long *size = args->size_pointer;
    if (*at >= args->length)
    {
        return 0;
    }
    *start = *at;
    const char *const found =
        memor.chr(&(MemoriaCfg){.src = args->text + *at, .val = '\n', .bytes = args->length - *at});
    if (found == NULL)
    {
        *size = args->length - *at;
        *at = args->length;
        return 1;
    }
    unsigned long long end = (unsigned long long)(found - args->text);
    *at = end + 1ull;
    if ((end > *start) && (args->text[end - 1ull] == '\r'))
    {
        end -= 1ull;
    }
    *size = end - *start;
    return 1;
}

typedef struct
{
    const char *text;
    unsigned long long length;
    const char *with;
    const char *line;
    unsigned long long size;
} LatexTextCtx;
// `length` bytes and `with`, the case of ASCII letters aside
EMBED_INLINE int latex_by_name(const LatexTextCtx *args)
{
    if (cellul.len(&(CatenaFinitaCfg){.src = args->with, .cap = CLI_CELLS_POOL}) != args->length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        const char letter =
            ((args->text[at] >= 'A') && (args->text[at] <= 'Z')) ? (char)(args->text[at] - 'A' + 'a') : args->text[at];
        if (letter != args->with[at])
        {
            return 0;
        }
    }
    return 1;
}

unsigned long long cli_tex_digits(const CliLatexTextLengthAtCtx *args)
{
    unsigned long long count = 0ull;
    count = EMBED_CALL(cli_text_at_size_classes.past_class, CliTextAtSizeClassesCfg, .text = args->text,
                       .at = args->at_value, .size = args->length, .classes = CLI_TEXT_DIGITS, .set = "") -
            args->at_value;
    return count;
}

int cli_tex_double(const CliLatexTextLengthValueCtx *args)
{
    double *value = args->value;
    unsigned long long at = ((args->length > 0ull) && ((args->text[0] == '+') || (args->text[0] == '-'))) ? 1ull : 0ull;
    const int word =
        EMBED_CALL(latex_by_name, LatexTextCtx, .text = args->text + at, .length = args->length - at, .with = "inf") ||
        EMBED_CALL(latex_by_name, LatexTextCtx, .text = args->text + at, .length = args->length - at,
                   .with = "infinity") ||
        EMBED_CALL(latex_by_name, LatexTextCtx, .text = args->text + at, .length = args->length - at, .with = "nan");
    if (!word)
    {
        const unsigned long long whole =
            EMBED_CALL(cli_latex_text_length_at_internal.tex_digits, CliLatexTextLengthAtCtx, .text = args->text,
                       .length = args->length, .at_value = at);
        at += whole;
        unsigned long long part = 0ull;
        if ((at < args->length) && (args->text[at] == '.'))
        {
            part = EMBED_CALL(cli_latex_text_length_at_internal.tex_digits, CliLatexTextLengthAtCtx, .text = args->text,
                              .length = args->length, .at_value = at + 1ull);
            at += 1ull + part;
        }
        if ((whole + part) == 0ull)
        {
            return 0;
        }
        if ((at < args->length) && ((args->text[at] == 'e') || (args->text[at] == 'E')))
        {
            at += 1ull;
            at += ((at < args->length) && ((args->text[at] == '+') || (args->text[at] == '-'))) ? 1ull : 0ull;
            const unsigned long long power =
                EMBED_CALL(cli_latex_text_length_at_internal.tex_digits, CliLatexTextLengthAtCtx, .text = args->text,
                           .length = args->length, .at_value = at);
            if (power == 0ull)
            {
                return 0;
            }
            at += power;
        }
        if (at != args->length)
        {
            return 0;
        }
    }
    char copy[512];
    if (args->length >= sizeof(copy))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = copy, .src = args->text, .bytes = args->length});
    copy[args->length] = '\0';
    *value = strtod(copy, NULL);
    return 1;
}

int cli_tex_count(const CliLatexTextLengthCtx *args)
{
    unsigned long long *value = args->value_u64s;
    const unsigned long long at = ((args->length > 0ull) && (args->text[0] == '+')) ? 1ull : 0ull;
    if ((at == args->length) ||
        (EMBED_CALL(cli_latex_text_length_at_internal.tex_digits, CliLatexTextLengthAtCtx, .text = args->text,
                    .length = args->length, .at_value = at) != args->length - at))
    {
        return 0;
    }
    unsigned long long sum = 0ull;
    for (unsigned long long letter = at; letter < args->length; letter += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(args->text[letter] - '0');
        if (sum > (args->most - digit) / 10ull)
        {
            return 0;
        }
        sum = (sum * 10ull) + digit;
    }
    *value = sum;
    return 1;
}

int cli_tex_sign(const CliLatexTextLengthValueI64sCtx *args)
{
    long long *value = args->value_i64s;
    const int negative = (args->length > 0ull) && (args->text[0] == '-');
    const unsigned long long at = (negative || ((args->length > 0ull) && (args->text[0] == '+'))) ? 1ull : 0ull;
    unsigned long long size = 0ull;
    // the most a negative number may be is one past the most a positive one may
    const unsigned long long most = negative ? 9223372036854775808ull : 9223372036854775807ull;
    if ((at == args->length) || (args->text[at] == '+') ||
        !EMBED_CALL(cli_latex_text_length_internal.tex_count, CliLatexTextLengthCtx, .text = args->text + at,
                    .length = args->length - at, .most = most, .value_u64s = &size))
    {
        return 0;
    }
    // the size is in range of the sign, so negating it in 64 bits as unsigned and reading it as signed is exact
    *value = negative ? (long long)(0ull - size) : (long long)size;
    return 1;
}

// whether a line, before its comment, takes fontspec or unicode-math with a \usepackage
EMBED_INLINE int latex_takes_unicode(const LatexTextCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long cut = 0ull;
    cut = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->line, .at = 0ull,
                     .size = size, .set = "%");
    while ((cut < size) && (cut != 0ull) && (args->line[cut - 1ull] == '\\'))
    {
        cut = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->line, .at = cut + 1ull,
                         .size = size, .set = "%");
    }
    static const char word[] = "\\usepackage";
    const unsigned long long word_size = sizeof(word) - 1ull;
    for (unsigned long long at = 0ull; at + word_size <= cut; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->line + at, .other = word, .bytes = word_size}) != 0)
        {
            continue;
        }
        unsigned long long rest = at + word_size;
        unsigned long long from = rest;
        while ((from < cut) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->line[from]))
        {
            from += 1ull;
        }
        if ((from < cut) && (args->line[from] == '['))
        {
            const char *const closed =
                memor.chr(&(MemoriaCfg){.src = args->line + from + 1ull, .val = ']', .bytes = cut - from - 1ull});
            // the options never closed leave nothing after them
            rest = (closed != NULL) ? (unsigned long long)(closed - args->line) + 1ull : cut;
        }
        while ((rest < cut) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->line[rest]))
        {
            rest += 1ull;
        }
        const char *const closed =
            ((rest < cut) && (args->line[rest] == '{'))
                ? memor.chr(&(MemoriaCfg){.src = args->line + rest, .val = '}', .bytes = cut - rest})
                : NULL;
        if (closed == NULL)
        {
            at += word_size - 1ull;
            continue;
        }
        const unsigned long long end = (unsigned long long)(closed - args->line);
        unsigned long long name = rest + 1ull;
        while (name <= end)
        {
            unsigned long long stop = name;
            stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->line, .at = stop,
                              .size = end, .set = ",");
            unsigned long long first = name;
            unsigned long long last = stop;
            while ((first < last) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->line[first]))
            {
                first += 1ull;
            }
            while ((last > first) &&
                   EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->line[last - 1ull]))
            {
                last -= 1ull;
            }
            if (((last - first == 8ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = args->line + first, .other = "fontspec", .bytes = 8u}) == 0)) ||
                ((last - first == 12ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = args->line + first, .other = "unicode-math", .bytes = 12u}) == 0)))
            {
                return 1;
            }
            name = stop + 1ull;
        }
        at += word_size - 1ull;
    }
    return 0;
}

const char *cli_latex_engine_get(const CliLatexCfg *args)
{
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long size = 0ull;
    while (EMBED_CALL(cli_latex_text_internal.tex_line, CliLatexTextCtx, .text = args->text, .length = args->length,
                      .at = &at, .start = &start, .size_pointer = &size))
    {
        while ((size > 0ull) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->text[start]))
        {
            start += 1ull;
            size -= 1ull;
        }
        while ((size > 0ull) &&
               EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->text[start + size - 1ull]))
        {
            size -= 1ull;
        }
        if (size == 0ull)
        {
            continue;
        }
        if (args->text[start] != '%')
        {
            break;
        }
        char engine_name[32];
        unsigned long long len = 0ull;
        int fits = 1;
        for (unsigned long long letter = start + 1ull; letter < start + size; letter += 1ull)
        {
            if (EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = args->text[letter]))
            {
                continue;
            }
            if (len + 1ull < sizeof(engine_name))
            {
                engine_name[len] = args->text[letter];
                len += 1ull;
            }
            else
            {
                fits = 0;
            }
        }
        const unsigned long long head =
            ((len >= 12ull) &&
             EMBED_CALL(latex_by_name, LatexTextCtx, .text = engine_name, .length = 12ull, .with = "!texprogram="))
                ? 12ull
            : ((len >= 15ull) &&
               EMBED_CALL(latex_by_name, LatexTextCtx, .text = engine_name, .length = 15ull, .with = "!texts-program="))
                ? 15ull
                : 0ull;
        if (!fits || (head == 0ull))
        {
            continue;
        }
        if (EMBED_CALL(latex_by_name, LatexTextCtx, .text = engine_name + head, .length = len - head,
                       .with = "pdflatex") ||
            EMBED_CALL(latex_by_name, LatexTextCtx, .text = engine_name + head, .length = len - head, .with = "latex"))
        {
            return "pdflatex";
        }
        if (EMBED_CALL(latex_by_name, LatexTextCtx, .text = engine_name + head, .length = len - head,
                       .with = "xelatex"))
        {
            return "xelatex";
        }
        if (EMBED_CALL(latex_by_name, LatexTextCtx, .text = engine_name + head, .length = len - head,
                       .with = "lualatex"))
        {
            return "lualatex";
        }
    }
    at = 0ull;
    while (EMBED_CALL(cli_latex_text_internal.tex_line, CliLatexTextCtx, .text = args->text, .length = args->length,
                      .at = &at, .start = &start, .size_pointer = &size))
    {
        if (EMBED_CALL(latex_takes_unicode, LatexTextCtx, .line = args->text + start, .size = size))
        {
            return "lualatex";
        }
    }
    return "pdflatex";
}
