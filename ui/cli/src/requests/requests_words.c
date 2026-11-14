// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_words.c: the words of a shell line as bash reads them: quoted in single quotes, double quotes or `$'...'`,
// a backslash taking the letter after it, and a backslash at the end of a line joining it to the next; and a line
// copied for cmd read as bash reads it, each caret taking the letter after it as it is and a caret at the end of a
// line joining it to the next
#include "../text/json_tree.h"
#include "requests_internal.h"

typedef struct
{
    unsigned int letter;
    const char *line;
    unsigned long long size;
    unsigned long long at_value;
    unsigned int *letter_pointer;
    CliRequestsWords *words;
    const char *bytes;
    unsigned long long *start;
    unsigned long long *at;
} RequestsWordsCtx;

// whether a letter is white as Rust's char::is_whitespace has it
EMBED_INLINE int words_white(const RequestsWordsCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

// the bytes of the letter at `at` of `size` bytes, one at the fewest
EMBED_INLINE unsigned long long words_letter(const RequestsWordsCtx *args)
{
    unsigned long long at = args->at_value;
    unsigned int *letter = args->letter_pointer;
    return EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                      .text_bytes = (const unsigned char *)args->line, .length = args->size, .at = at,
                      .letter = letter);
}

// `size` bytes put on the word being read: 1, or 0 where the pool is full
EMBED_INLINE int words_append(const RequestsWordsCtx *args)
{
    const char *bytes = args->bytes;
    if (args->words->used + args->size + 1u > sizeof(args->words->text))
    {
        return 0;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = args->words->text + args->words->used, .src = bytes, .bytes = (embed_word)args->size});
    args->words->used += args->size;
    return 1;
}

// the word being read ended and stored: 1, or 0 where the pool or the list is full
EMBED_INLINE int words_end(const RequestsWordsCtx *args)
{
    unsigned long long *start = args->start;
    if ((args->words->count == CLI_REQUESTS_CURL_WORDS) || (args->words->used + 1u > sizeof(args->words->text)))
    {
        return 0;
    }
    args->words->word_text[args->words->count].at = *start;
    args->words->word_text[args->words->count].size = args->words->used - *start;
    args->words->text[args->words->used] = '\0';
    args->words->used += 1u;
    args->words->count += 1u;
    *start = args->words->used;
    return 1;
}

// a `$'...'` quote read from its letters at `*at`, past its closing quote, onto the word: 1, or 0 where the pool is
// full
EMBED_INLINE int words_dollar(const RequestsWordsCtx *args)
{
    unsigned long long *at = args->at;
    int fits = 1;
    while (fits && (*at < args->size) && (args->line[*at] != '\''))
    {
        if ((args->line[*at] == '\\') && (*at + 1u < args->size))
        {
            *at += 1u;
            const char letter = args->line[*at];
            char plain;
            switch (letter)
            {
            case 'n': {
                plain = '\n';
                break;
            }
            case 't': {
                plain = '\t';
                break;
            }
            case 'r': {
                plain = '\r';
                break;
            }
            case '0': {
                plain = '\0';
                break;
            }
            default: {
                plain = letter;
                break;
            }
            }
            unsigned int whole = 0u;
            const unsigned long long reserved =
                EMBED_CALL(words_letter, RequestsWordsCtx, .line = args->line, .size = args->size, .at_value = *at,
                           .letter_pointer = &whole);
            fits = (plain != letter)
                       ? EMBED_CALL(words_append, RequestsWordsCtx, .words = args->words, .bytes = &plain, .size = 1u)
                       : EMBED_CALL(words_append, RequestsWordsCtx, .words = args->words, .bytes = args->line + *at,
                                    .size = reserved);
            *at += reserved;
        }
        else
        {
            unsigned int whole = 0u;
            const unsigned long long reserved =
                EMBED_CALL(words_letter, RequestsWordsCtx, .line = args->line, .size = args->size, .at_value = *at,
                           .letter_pointer = &whole);
            fits = EMBED_CALL(words_append, RequestsWordsCtx, .words = args->words, .bytes = args->line + *at,
                              .size = reserved);
            *at += reserved;
        }
    }
    *at += 1u;
    return fits;
}

// a double quote read from its letters at `*at`, past its closing quote, onto the word: 1, 0 where the pool is full,
// or -1 where it does not close
EMBED_INLINE int words_double(const RequestsWordsCtx *args)
{
    unsigned long long *at = args->at;
    int fits = 1;
    while (fits && (*at < args->size) && (args->line[*at] != '"'))
    {
        if ((args->line[*at] == '\\') && (*at + 1u < args->size) &&
            (cellul.chr(&(CatenaFinitaCfg){.src = "\"\\$`", .cap = CLI_POOL_BYTES, .byte = args->line[*at + 1u]}) !=
             NULL) &&
            (args->line[*at + 1u] != '\0'))
        {
            *at += 1u;
        }
        unsigned int whole = 0u;
        const unsigned long long reserved = EMBED_CALL(words_letter, RequestsWordsCtx, .line = args->line,
                                                       .size = args->size, .at_value = *at, .letter_pointer = &whole);
        fits = EMBED_CALL(words_append, RequestsWordsCtx, .words = args->words, .bytes = args->line + *at,
                          .size = reserved);
        *at += reserved;
    }
    if (*at >= args->size)
    {
        return -1;
    }
    *at += 1u;
    return fits;
}

int cli_requests_words(const CliRequestsSizeCtx *args)
{
    args->words->count = 0u;
    args->words->used = 0ULL;
    unsigned long long start = 0ULL;
    int started = 0;
    int read = 1;
    unsigned long long at = 0ULL;
    while ((read == 1) && (at < args->size))
    {
        const char letter = args->line[at];
        const int joined = (letter == '\\') && (at + 1u < args->size) &&
                           ((args->line[at + 1u] == '\n') ||
                            ((args->line[at + 1u] == '\r') && (at + 2u < args->size) && (args->line[at + 2u] == '\n')));
        if (joined)
        {
            at += (args->line[at + 1u] == '\r') ? 3u : 2u;
        }
        else if (letter == '\\')
        {
            unsigned int whole = 0u;
            const unsigned long long reserved =
                (at + 1u < args->size) ? EMBED_CALL(words_letter, RequestsWordsCtx, .line = args->line,
                                                    .size = args->size, .at_value = at + 1u, .letter_pointer = &whole)
                                       : 0u;
            read = (reserved == 0u) || EMBED_CALL(words_append, RequestsWordsCtx, .words = args->words,
                                                  .bytes = args->line + at + 1u, .size = reserved);
            started = started || (reserved > 0u);
            at += 1u + ((reserved > 0u) ? reserved : 1u);
        }
        else if (letter == '\'')
        {
            const char *const close = memor.chr(
                &(MemoriaCfg){.src = args->line + at + 1u, .val = '\'', .bytes = (embed_word)(args->size - at - 1u)});
            if (close == NULL)
            {
                read = -1;
                break;
            }
            const unsigned long long end = (unsigned long long)(close - args->line);
            read = EMBED_CALL(words_append, RequestsWordsCtx, .words = args->words, .bytes = args->line + at + 1u,
                              .size = end - at - 1u);
            started = 1;
            at = end + 1u;
        }
        else if ((letter == '$') && (at + 1u < args->size) && (args->line[at + 1u] == '\''))
        {
            at += 2u;
            read = EMBED_CALL(words_dollar, RequestsWordsCtx, .line = args->line, .size = args->size, .at = &at,
                              .words = args->words);
            started = 1;
        }
        else if (letter == '"')
        {
            at += 1u;
            read = EMBED_CALL(words_double, RequestsWordsCtx, .line = args->line, .size = args->size, .at = &at,
                              .words = args->words);
            started = 1;
        }
        else
        {
            unsigned int whole = 0u;
            const unsigned long long reserved =
                EMBED_CALL(words_letter, RequestsWordsCtx, .line = args->line, .size = args->size, .at_value = at,
                           .letter_pointer = &whole);
            if (EMBED_CALL(words_white, RequestsWordsCtx, .letter = whole))
            {
                read = !started || EMBED_CALL(words_end, RequestsWordsCtx, .words = args->words, .start = &start);
                started = 0;
            }
            else
            {
                read = EMBED_CALL(words_append, RequestsWordsCtx, .words = args->words, .bytes = args->line + at,
                                  .size = reserved);
                started = 1;
            }
            at += reserved;
        }
    }
    read = ((read == 1) && started) ? EMBED_CALL(words_end, RequestsWordsCtx, .words = args->words, .start = &start)
                                    : read;
    if (read == -1)
    {
        mmgr_line_put(args->err_msg, "a quote does not close");
    }
    else if (read == 0)
    {
        mmgr_line_put(args->err_msg, "the curl line is larger than impero supports");
    }
    return read == 1;
}

void cli_requests_uncareted(const CliRequestsSizeOutCtx *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        if ((args->line[at] == '^') && (at + 1u < args->size))
        {
            const char next = args->line[at + 1u];
            if ((next == '\r') || (next == '\n'))
            {
                at += 2u;
                at += ((next == '\r') && (at < args->size) && (args->line[at] == '\n')) ? 1u : 0u;
                continue;
            }
            at += 1u;
        }
        unsigned int whole = 0u;
        const unsigned long long reserved = EMBED_CALL(words_letter, RequestsWordsCtx, .line = args->line,
                                                       .size = args->size, .at_value = at, .letter_pointer = &whole);
        mmgr_line_put_n(args->out, args->line + at, reserved);
        at += reserved;
    }
}
