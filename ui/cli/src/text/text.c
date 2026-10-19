// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.c: a text of UTF-8 read letter by letter. A byte that starts no letter, or a letter cut short, is read as one
// letter of its own byte
#include "text.h"
#include "regexp.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

unsigned int cli_text_letter(const char *text, unsigned long long size, unsigned long long at,
                             unsigned long long *length)
{
    const unsigned char lead = (unsigned char)text[at];
    const unsigned long long want = (lead < 0xc0u) ? 1u : (lead < 0xe0u) ? 2u : (lead < 0xf0u) ? 3u : 4u;
    if ((want == 1u) || (at + want > size))
    {
        *length = 1u;
        return lead;
    }
    unsigned int letter = lead & ((want == 2u) ? 0x1fu : (want == 3u) ? 0x0fu : 0x07u);
    for (unsigned long long next = 1ull; next < want; next += 1u)
    {
        const unsigned char more = (unsigned char)text[at + next];
        if ((more & 0xc0u) != 0x80u)
        {
            *length = 1u;
            return lead;
        }
        letter = (letter << 6u) | (more & 0x3fu);
    }
    *length = want;
    return letter;
}

unsigned int cli_text_letter_before(const char *text, unsigned long long end, unsigned long long *length)
{
    unsigned long long start = end - 1u;
    while ((start > 0u) && (end - start < 4u) && (((unsigned char)text[start] & 0xc0u) == 0x80u))
    {
        start -= 1u;
    }
    unsigned long long read = 0ull;
    const unsigned int letter = cli_text_letter(text, end, start, &read);
    if (start + read != end)
    {
        *length = 1u;
        return (unsigned char)text[end - 1u];
    }
    *length = read;
    return letter;
}

int cli_text_white(unsigned int letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r')) || (letter == 0x85u) || (letter == 0xA0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200Au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202Fu) || (letter == 0x205Fu) || (letter == 0x3000u);
}

int cli_text_alphabetic(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z'));
    }
    return cli_regexp_property(CLI_REGEXP_LETTER, letter);
}

int cli_text_alphanumeric(unsigned int letter)
{
    if (letter < 0x80u)
    {
        return cli_text_alphabetic(letter) || ((letter >= '0') && (letter <= '9'));
    }
    return cli_regexp_property(CLI_REGEXP_LETTER, letter) || cli_regexp_property(CLI_REGEXP_NUMBER, letter);
}

unsigned long long cli_text_lead(const char *text, unsigned long long size)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned long long length = 0ull;
        if (!cli_text_white(cli_text_letter(text, size, at, &length)))
        {
            break;
        }
        at += length;
    }
    return at;
}

unsigned long long cli_text_trimmed_end(const char *text, unsigned long long size)
{
    unsigned long long end = size;
    while (end > 0u)
    {
        unsigned long long length = 0ull;
        if (!cli_text_white(cli_text_letter_before(text, end, &length)))
        {
            break;
        }
        end -= length;
    }
    return end;
}

void cli_text_trimmed(const char *text, unsigned long long size, unsigned long long *from, unsigned long long *to)
{
    *from = cli_text_lead(text, size);
    *to = *from + cli_text_trimmed_end(text + *from, size - *from);
}

int cli_text_word(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *word,
                  unsigned long long *length)
{
    *at += cli_text_lead(text + *at, size - *at);
    if (*at >= size)
    {
        return 0;
    }
    *word = *at;
    while (*at < size)
    {
        unsigned long long letter = 0ull;
        if (cli_text_white(cli_text_letter(text, size, *at, &letter)))
        {
            break;
        }
        *at += letter;
    }
    *length = *at - *word;
    return 1;
}

unsigned long long cli_text_letters(const char *text, unsigned long long size)
{
    unsigned long long count = 0ull;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned long long length = 0ull;
        (void)cli_text_letter(text, size, at, &length);
        at += length;
        count += 1u;
    }
    return count;
}

unsigned long long cli_text_units(const char *text, unsigned long long size)
{
    unsigned long long count = 0ull;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(text, size, at, &length);
        at += length;
        count += (letter >= 0x10000u) ? 2u : 1u;
    }
    return count;
}

int cli_text_starts(const char *text, unsigned long long size, const char *prefix)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    return (length <= size) && (cellul.diff(&(CatenaFinitaCfg){.src = text, .other = prefix, .cap = length}) == length);
}

int cli_text_ends(const char *text, unsigned long long size, const char *suffix)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = suffix, .cap = CLI_CELLS_POOL});
    return (length <= size) &&
           (cellul.diff(&(CatenaFinitaCfg){.src = text + size - length, .other = suffix, .cap = length}) == length);
}

int cli_text_find(const char *text, unsigned long long length, const char *wanted, unsigned long long size,
                  unsigned long long *at)
{
    // a wanted text of no bytes is at the start; find would measure one of 0 as terminated
    const char *const found = (size == 0ull) ? text
                                             : cellul.find(&(CatenaFinitaCfg){.src = text,
                                                                              .cap = (size_t)length,
                                                                              .other = wanted,
                                                                              .other_cap = (size_t)size,
                                                                              .other_len = (size_t)size});
    if (found == NULL)
    {
        return 0;
    }
    *at = (unsigned long long)(found - text);
    return 1;
}

int cli_text_find_last(const char *text, unsigned long long length, const char *wanted, unsigned long long size,
                       unsigned long long *at)
{
    if (size > length)
    {
        return 0;
    }
    if (size == 0ull)
    {
        *at = length;
        return 1;
    }
    // the places the wanted text can start at, its first byte found by the word scan from the last of them back, and
    // the whole compared only where that byte stands
    unsigned long long places = length - size + 1ull;
    for (const char *start = cli_text_last(text, places, wanted[0]); start != NULL;
         start = cli_text_last(text, places, wanted[0]))
    {
        if (cellul.diff(&(CatenaFinitaCfg){.src = start, .other = wanted, .cap = (size_t)size}) == (size_t)size)
        {
            *at = (unsigned long long)(start - text);
            return 1;
        }
        places = (unsigned long long)(start - text);
    }
    return 0;
}

int cli_text_order(const char *one, const char *other)
{
    // `one` with its terminator: `other` differs from it there at the latest, so the first difference is the order
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_CELLS_POOL}) + 1u;
    const embed_iword order = memor.cmp(&(MemoriaCfg){.src = one, .other = other, .bytes = length});
    return (order > 0) - (order < 0);
}

int cli_text_same(const char *one, const char *other, unsigned long long size)
{
    return cellul.diff(&(CatenaFinitaCfg){.src = one, .other = other, .cap = (size_t)size}) == (size_t)size;
}

// Walks the aligned words the text stands in from the one holding its first byte, a word at a time: each word tested
// against every byte of the set in every lane at once and the lanes the set holds joined, the lanes before the text
// masked off, and the first lane that stops the run taken: its terminator, or a byte of the set where `in_set` and
// one outside it where not. Every load is an aligned word holding a byte of the text, so nothing outside those words
// is read
static unsigned long long text_run(const char *text, const char *set, int in_set)
{
    // Explicit casts read the addresses as integers, so a word's place and the text's start compare as numbers
    const uintptr_t first = (uintptr_t)text;
    uintptr_t word_at = first & ~(uintptr_t)(MMGR_SWAR_BYTES - 1u);
    embed_word before = mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(first - word_at)});
    for (;;)
    {
        const embed_word loaded = word.load_al(&(ScrutWordCfg){.at = (const void *)word_at});
        embed_word held = 0u;
        for (const char *letter = set; *letter != '\0'; letter += 1)
        {
            // Explicit cast reads the set's byte as unsigned before it is repeated into every lane
            held |= lane.eq(&(ScrutLaneCfg){.word = loaded, .byte = (uint8_t)*letter});
        }
        const embed_word ended = lane.has_zero(&(ScrutLaneCfg){.word = loaded});
        const embed_word stops = (ended | (in_set ? held : (~held & MMGR_VERBUM_SCRUTOR_HIGH))) & ~before;
        if (stops != 0u)
        {
            return (unsigned long long)(word_at + lane.first(&(ScrutLaneCfg){.mask = stops}) - first);
        }
        word_at += MMGR_SWAR_BYTES;
        before = 0u;
    }
}

unsigned long long cli_text_upto(const char *text, const char *set)
{
    return text_run(text, set, 1);
}

unsigned long long cli_text_over(const char *text, const char *set)
{
    return text_run(text, set, 0);
}

// Walks the aligned words the text stands in, from the one holding its last byte back to the one holding its first,
// a word at a time: each word tested against the byte in every lane at once, its lanes outside the text masked off,
// and the last lane that holds the byte taken. Every load is an aligned word holding a byte of the text, so nothing
// outside those words is read
const char *cli_text_last(const char *text, unsigned long long length, char byte)
{
    if (length == 0ull)
    {
        return NULL;
    }
    // Explicit casts read the addresses as integers, so a word's place and the text's bounds compare as numbers
    const uintptr_t first = (uintptr_t)text;
    const uintptr_t end = first + (uintptr_t)length;
    const embed_word every = mask.lanes_below(&(ScrutMaskCfg){.bytes = MMGR_SWAR_BYTES});
    // Explicit cast reads the byte as unsigned before it is repeated into every lane
    const embed_word broadcast = MMGR_SWAR_ONES * (embed_word)(uint8_t)byte;
    uintptr_t word_at = (end - 1u) & ~(uintptr_t)(MMGR_SWAR_BYTES - 1u);
    embed_word keep = mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(end - word_at)});
    for (;;)
    {
        if (word_at < first)
        {
            keep &= ~mask.lanes_below(&(ScrutMaskCfg){.bytes = (size_t)(first - word_at)});
        }
        const embed_word loaded = word.load_al(&(ScrutWordCfg){.at = (const void *)word_at});
        const embed_word hit = lane.has_zero(&(ScrutLaneCfg){.word = loaded ^ broadcast}) & keep;
        if (hit != 0u)
        {
            return (const char *)(word_at + lane.last(&(ScrutLaneCfg){.mask = hit}));
        }
        if (word_at <= first)
        {
            return NULL;
        }
        word_at -= MMGR_SWAR_BYTES;
        keep = every;
    }
}
