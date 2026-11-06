// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers_lines.c: findings written a line each, `path:line:col: message`, with a severity word and a code where the
// checker writes them, as flake8, mypy and pylint write them
#include "../text/text.h"
#include "checkers_internal.h"

typedef struct
{
    char letter;
    const char **at;
    unsigned long long *length;
    const char *line;
    unsigned long long length_value;
    unsigned long long *path_length;
    CliFinding *finding;
} CheckersLinesCtx;

// whether a byte is ASCII's whitespace, and a span with that whitespace at either end left off
EMBED_INLINE int cli_blank(const CheckersLinesCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

EMBED_INLINE void cli_trim(const CheckersLinesCtx *args)
{
    const char **at = args->at;
    unsigned long long *length = args->length;
    while ((*length > 0ULL) && EMBED_CALL(cli_blank, CheckersLinesCtx, .letter = (*at)[0]))
    {
        *at += 1;
        *length -= 1ULL;
    }
    while ((*length > 0ULL) && EMBED_CALL(cli_blank, CheckersLinesCtx, .letter = (*at)[*length - 1ULL]))
    {
        *length -= 1ULL;
    }
}

// a line of findings, `path:line:col: rest`, the column where it is written, read into a finding: 1 with the path's
// length, or 0 where the line is none
EMBED_INLINE int cli_line_parts(const CheckersLinesCtx *args)
{
    const char *line = args->line;
    unsigned long long *path_length = args->path_length;
    for (unsigned long long at = 1ULL; at < args->length_value; at += 1ULL)
    {
        if (line[at] != ':')
        {
            continue;
        }
        unsigned long long digits = 0ULL;
        digits = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = line, .at = at + 1ULL,
                            .size = args->length_value, .classes = CLI_TEXT_DIGITS, .set = "") -
                 (at + 1ULL);
        if ((digits == 0ULL) || ((at + 1ULL + digits) >= args->length_value) || (line[at + 1ULL + digits] != ':'))
        {
            continue;
        }
        unsigned long long number = 0ULL;
        for (unsigned long long place = 0ULL; place < digits; place += 1ULL)
        {
            number = (number * 10ULL) + (unsigned long long)(line[at + 1ULL + place] - '0');
        }
        const unsigned long long after = at + 2ULL + digits;
        unsigned long long col_digits = 0ULL;
        col_digits = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = line, .at = after,
                                .size = args->length_value, .classes = CLI_TEXT_DIGITS, .set = "") -
                     after;
        unsigned long long col = 1ULL;
        unsigned long long rest = after;
        if ((col_digits > 0ULL) && ((after + col_digits) < args->length_value) && (line[after + col_digits] == ':'))
        {
            col = 0ULL;
            for (unsigned long long place = 0ULL; place < col_digits; place += 1ULL)
            {
                col = (col * 10ULL) + (unsigned long long)(line[after + place] - '0');
            }
            rest = after + col_digits + 1ULL;
        }
        *path_length = at;
        args->finding->line = number;
        args->finding->col = col;
        args->finding->message = line + rest;
        args->finding->message_length = args->length_value - rest;
        EMBED_CALL(cli_trim, CheckersLinesCtx, .at = &args->finding->message, .length = &args->finding->message_length);
        return 1;
    }
    return 0;
}

// a severity word at the start of a message, as mypy writes one, taken off it: an error, a warning, or a note; a
// warning where none is written
EMBED_INLINE unsigned int cli_severity_get(const CheckersLinesCtx *args)
{
    static const char *const s_words[4] = {"error:", "warning:", "note:", "info:"};
    static const unsigned int s_severity[4] = {1u, 2u, 3u, 3u};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_words[at], .cap = CLI_POOL_BYTES});
        if ((args->finding->message_length >= size) &&
            (memor.cmp(&(MemoriaCfg){.src = args->finding->message, .other = s_words[at], .bytes = size}) == 0))
        {
            args->finding->message += size;
            args->finding->message_length -= size;
            EMBED_CALL(cli_trim, CheckersLinesCtx, .at = &args->finding->message,
                       .length = &args->finding->message_length);
            return s_severity[at];
        }
    }
    return 2u;
}

// a finding's code taken off its message: as flake8 writes one before it, `E501 ...`, as mypy writes one in brackets
// after it, `...  [arg-type]`, or as pylint writes one in brackets before it, `[C0114(missing-module-docstring), ]`
EMBED_INLINE void cli_code_get(const CheckersLinesCtx *args)
{
    const char *const rest = args->finding->message;
    const unsigned long long length = args->finding->message_length;
    args->finding->code = "";
    args->finding->code_length = 0ULL;
    const char *const close = ((length > 0ULL) && (rest[0] == '['))
                                  ? memor.chr(&(MemoriaCfg){.src = rest + 1, .val = ']', .bytes = length - 1ULL})
                                  : NULL;
    if (close != NULL)
    {
        const char *code = rest + 1;
        unsigned long long size = 0ULL;
        while (((code + size) < close) && (code[size] != '(') && (code[size] != ','))
        {
            size += 1ULL;
        }
        EMBED_CALL(cli_trim, CheckersLinesCtx, .at = &code, .length = &size);
        args->finding->code = code;
        args->finding->code_length = size;
        args->finding->message = close + 1;
        args->finding->message_length = (unsigned long long)((rest + length) - (close + 1));
        EMBED_CALL(cli_trim, CheckersLinesCtx, .at = &args->finding->message, .length = &args->finding->message_length);
        return;
    }
    for (unsigned long long open = length; (length > 0ULL) && (rest[length - 1ULL] == ']') && (open >= 3ULL);
         open -= 1ULL)
    {
        if (memor.cmp(&(MemoriaCfg){.src = rest + open - 3ULL, .other = "  [", .bytes = 3u}) == 0)
        {
            args->finding->code = rest + open;
            args->finding->code_length = length - open - 1ULL;
            args->finding->message_length = open - 3ULL;
            EMBED_CALL(cli_trim, CheckersLinesCtx, .at = &args->finding->message,
                       .length = &args->finding->message_length);
            return;
        }
    }
    unsigned long long first = 0ULL;
    while ((first < length) && !EMBED_CALL(cli_blank, CheckersLinesCtx, .letter = rest[first]))
    {
        first += 1ULL;
    }
    int code = (first >= 2ULL) && (rest[0] >= 'A') && (rest[0] <= 'Z');
    {
        unsigned long long at = 1ULL;
        while (code && (at < first))
        {
            code = (rest[at] >= '0') && (rest[at] <= '9');
            at += 1ULL;
        }
    }
    if (code)
    {
        args->finding->code = rest;
        args->finding->code_length = first;
        args->finding->message = rest + first;
        args->finding->message_length = length - first;
        EMBED_CALL(cli_trim, CheckersLinesCtx, .at = &args->finding->message, .length = &args->finding->message_length);
    }
}

void cli_checkers_lines_read(const CliCheckersTextCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->text, .at = at, .size = args->size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        const char *const line = args->text + at;
        at = end + 1ULL;
        CliFinding finding = {0ULL, 1ULL, 0, 0ULL, 0ULL, 2u, NULL, 0ULL, "", 0ULL, "[]"};
        unsigned long long path_length = 0ULL;
        if (!EMBED_CALL(cli_line_parts, CheckersLinesCtx, .line = line, .length_value = length,
                        .path_length = &path_length, .finding = &finding))
        {
            continue;
        }
        finding.severity = EMBED_CALL(cli_severity_get, CheckersLinesCtx, .finding = &finding);
        EMBED_CALL(cli_code_get, CheckersLinesCtx, .finding = &finding);
        EMBED_CALL(cli_checkers_file_internal.finding_append, CliCheckersFileCtx,
                   .file =
                       EMBED_CALL(cli_checkers_internal.file_get, CliCheckersCtx, .name = line, .length = path_length),
                   .finding = &finding);
    }
}
