// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// line_read.c: a text's lines read one at a time out of its bytes, and a line's words and numbers read one at a time
#include "line_read.h"

#include <string.h>

int line_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at, char *line, size_t room)
{
    if (*at >= size)
    {
        return 0;
    }
    // the rest of the text, which a size_t counts as the text is in memory
    const unsigned char *const end = (const unsigned char *)memchr(bytes + *at, '\n', (size_t)(size - *at));
    const size_t length = (end != NULL) ? (size_t)(end - (bytes + *at)) : (size_t)(size - *at);
    const size_t kept = (length < room) ? length : (room - 1u);
    memcpy(line, bytes + *at, kept);
    line[kept] = '\0';
    *at += (unsigned long long)length + 1ull;
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

// 1 where `letter` ends a word: a space, a tab, a line's end or the text's end
static int line_word_ended(char letter)
{
    return (letter == ' ') || (letter == '\t') || (letter == '\r') || (letter == '\n') || (letter == '\0');
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
    memcpy(word, *at, length);
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
    memcpy(text, *at, length);
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
    const unsigned long long base = hex ? 16ull : 10ull;
    unsigned long long read = 0ull;
    unsigned int digits = 0u;
    for (;; walk += 1)
    {
        const char letter = *walk;
        unsigned long long digit = base;
        if ((letter >= '0') && (letter <= '9'))
        {
            digit = (unsigned long long)(letter - '0');
        }
        else if (hex && (letter >= 'a') && (letter <= 'f'))
        {
            digit = (unsigned long long)(letter - 'a') + 10ull;
        }
        else if (hex && (letter >= 'A') && (letter <= 'F'))
        {
            digit = (unsigned long long)(letter - 'A') + 10ull;
        }
        if (digit >= base)
        {
            break;
        }
        if (read > ((~0ull - digit) / base))
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

int line_letters(const char **at, const char *text)
{
    line_spaces_passed(at);
    const size_t length = strlen(text);
    if (strncmp(*at, text, length) != 0)
    {
        return 0;
    }
    *at += length;
    return 1;
}

int line_unsigned(const char **at, unsigned int *value)
{
    unsigned long long read = 0ull;
    const char *walk = *at;
    if (!line_number(&walk, 0, &read) || (read > 0xffffffffull))
    {
        return 0;
    }
    // the test above holds the number inside 32 bits
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
    unsigned long long read = 0ull;
    if (!line_number(&walk, 0, &read) || (read > (negative ? 0x80000000ull : 0x7fffffffull)))
    {
        return 0;
    }
    // the test above holds the magnitude inside an int's range, its least taken through a long long
    *value = (int)(negative ? -(long long)read : (long long)read);
    *at = walk;
    return 1;
}
