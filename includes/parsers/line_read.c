// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// line_read.c: a text's lines read one at a time out of its bytes, and a line's words and numbers read one at a time
#include "line_read.h"
#include "../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int line_read_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

int line_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at, char *line, size_t room)
{
    if (*at >= size)
    {
        return 0;
    }
    // the rest of the text, which a size_t counts as the text is in memory
    const unsigned char *const end = (const unsigned char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = bytes + *at,
                                                                       .bytes = (size_t)(size - *at), .val = '\n');
    const size_t length = (end != NULL) ? (size_t)(end - (bytes + *at)) : (size_t)(size - *at);
    const size_t copy_len = (length < room) ? length : (room - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = line, .src = bytes + *at, .bytes = copy_len);
    line[copy_len] = '\0';
    *at += (unsigned long long)length + 1ULL;
    return 1;
}

// `*at` moved past the spaces and tabs at it
static void line_spaces_passed(const char **at)
{
    while ((**at == ' ') || (**at == '\t'))
    {
        *at += 1;
    }
}

// 1 where `character` ends a word: a space, a tab, a line's end or the text's end
static int line_word_ended(char character)
{
    return (character == ' ') || (character == '\t') || (character == '\r') || (character == '\n') ||
           (character == '\0');
}

int line_word(const char **at, char *word, size_t room)
{
    line_spaces_passed(at);
    size_t length = 0u;
    while (!line_word_ended((*at)[length]))
    {
        length += 1u;
    }
    if ((length == 0u) || (length >= room))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = word, .src = *at, .bytes = length);
    word[length] = '\0';
    *at += length;
    return 1;
}

int line_until(const char **at, char end, char *text, size_t room)
{
    size_t length = 0u;
    while (((*at)[length] != end) && ((*at)[length] != '\r') && ((*at)[length] != '\n') && ((*at)[length] != '\0'))
    {
        length += 1u;
    }
    if ((length == 0u) || (length >= room))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = text, .src = *at, .bytes = length);
    text[length] = '\0';
    *at += length;
    return 1;
}

int line_number(const char **at, int hex, unsigned long long *value)
{
    line_spaces_passed(at);
    const char *walk = *at;
    if (hex && (walk[0] == '0') && ((walk[1] == 'x') || (walk[1] == 'X')))
    {
        walk += 2;
    }
    const unsigned long long base = hex ? 16ULL : 10ULL;
    unsigned long long read = 0ULL;
    unsigned int digits = 0u;
    for (;; walk += 1)
    {
        const char character = *walk;
        unsigned long long digit = base;
        if ((character >= '0') && (character <= '9'))
        {
            digit = (unsigned long long)(character - '0');
        }
        else if (hex && (character >= 'a') && (character <= 'f'))
        {
            digit = (unsigned long long)(character - 'a') + 10ULL;
        }
        else if (hex && (character >= 'A') && (character <= 'F'))
        {
            digit = (unsigned long long)(character - 'A') + 10ULL;
        }
        if (digit >= base)
        {
            break;
        }
        if (read > ((~0ULL - digit) / base))
        {
            return 0;
        }
        read = (read * base) + digit;
        digits += 1u;
    }
    if (digits == 0u)
    {
        return 0;
    }
    *value = read;
    *at = walk;
    return 1;
}

int line_characters(const char **at, const char *text)
{
    line_spaces_passed(at);
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if (line_read_text_order(*at, text, (size_t)(length)) != 0)
    {
        return 0;
    }
    *at += length;
    return 1;
}

int line_unsigned(const char **at, unsigned int *value)
{
    unsigned long long read = 0ULL;
    const char *walk = *at;
    if (!line_number(&walk, 0, &read) || (read > 0xffffffffULL))
    {
        return 0;
    }
    // the test above bounds the number inside 32 bits
    *value = (unsigned int)read;
    *at = walk;
    return 1;
}

int line_signed(const char **at, int *value)
{
    line_spaces_passed(at);
    const char *walk = *at;
    const int negative = (*walk == '-');
    walk += negative ? 1 : 0;
    unsigned long long read = 0ULL;
    if (!line_number(&walk, 0, &read) || (read > (negative ? 0x80000000ULL : 0x7fffffffULL)))
    {
        return 0;
    }
    // the test above bounds the magnitude inside an int's range, its least taken through a long long
    *value = (int)(negative ? -(long long)read : (long long)read);
    *at = walk;
    return 1;
}
