// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_letters.c: a letter's Unicode properties and its case, as the matcher reads them. ASCII matches exactly; past
// it the blocks listed below match: the Latin, Greek, Cyrillic, Armenian, Georgian and fullwidth cases, and the letters
// and numbers of the scripts named in the tables. A letter of a block not listed has no property and no other case
#include "regexp.h"

typedef struct
{
    unsigned int first;
    unsigned int last;
    unsigned int offset;
    unsigned int parity;
} CliCaseBlock;
// a block whose capitals each have their small letter `offset` above: every one from `first` to `last`, or the even or
// the odd of them where the cases alternate
enum
{
    CLI_CASE_ALL,
    CLI_CASE_EVEN,
    CLI_CASE_ODD
};

static const CliCaseBlock s_case_blocks[] = {
    {'A', 'Z', 32u, CLI_CASE_ALL},           {0xc0u, 0xd6u, 32u, CLI_CASE_ALL},
    {0xd8u, 0xdeu, 32u, CLI_CASE_ALL},       {0x100u, 0x12fu, 1u, CLI_CASE_EVEN},
    {0x132u, 0x137u, 1u, CLI_CASE_EVEN},     {0x139u, 0x148u, 1u, CLI_CASE_ODD},
    {0x14au, 0x177u, 1u, CLI_CASE_EVEN},     {0x179u, 0x17eu, 1u, CLI_CASE_ODD},
    {0x386u, 0x386u, 38u, CLI_CASE_ALL},     {0x388u, 0x38au, 37u, CLI_CASE_ALL},
    {0x38cu, 0x38cu, 64u, CLI_CASE_ALL},     {0x38eu, 0x38fu, 63u, CLI_CASE_ALL},
    {0x391u, 0x3a1u, 32u, CLI_CASE_ALL},     {0x3a3u, 0x3abu, 32u, CLI_CASE_ALL},
    {0x400u, 0x40fu, 80u, CLI_CASE_ALL},     {0x410u, 0x42fu, 32u, CLI_CASE_ALL},
    {0x460u, 0x481u, 1u, CLI_CASE_EVEN},     {0x48au, 0x4bfu, 1u, CLI_CASE_EVEN},
    {0x4d0u, 0x52fu, 1u, CLI_CASE_EVEN},     {0x531u, 0x556u, 48u, CLI_CASE_ALL},
    {0x10a0u, 0x10c5u, 7264u, CLI_CASE_ALL}, {0x1e00u, 0x1e95u, 1u, CLI_CASE_EVEN},
    {0x1ea0u, 0x1effu, 1u, CLI_CASE_EVEN},   {0x2160u, 0x216fu, 16u, CLI_CASE_ALL},
    {0x24b6u, 0x24cfu, 26u, CLI_CASE_ALL},   {0xff21u, 0xff3au, 32u, CLI_CASE_ALL}};

// the cases no block above covers: a capital and the small letter it takes, then a small letter and the capital it
// takes, where the two do not go back the same way
static const unsigned int s_lowered[][2] = {{0x178u, 0xffu}, {0x130u, 'i'}};

static const unsigned int s_raised[][2] = {{0xffu, 0x178u}, {0x131u, 'I'}, {0x3c2u, 0x3a3u}, {0xb5u, 0x39cu}};

// the letters past ASCII, then the numbers, each a range of the blocks listed
static const unsigned int s_letters[][2] = {
    {0xaau, 0xaau},     {0xb5u, 0xb5u},     {0xbau, 0xbau},      {0xc0u, 0xd6u},     {0xd8u, 0xf6u},
    {0xf8u, 0x2c1u},    {0x370u, 0x374u},   {0x376u, 0x37du},    {0x37fu, 0x37fu},   {0x386u, 0x386u},
    {0x388u, 0x3ffu},   {0x400u, 0x481u},   {0x48au, 0x52fu},    {0x531u, 0x556u},   {0x561u, 0x587u},
    {0x5d0u, 0x5eau},   {0x620u, 0x64au},   {0x671u, 0x6d3u},    {0x904u, 0x939u},   {0xe01u, 0xe30u},
    {0x10a0u, 0x10ffu}, {0x1100u, 0x11ffu}, {0x1e00u, 0x1fffu},  {0x2160u, 0x2188u}, {0x3041u, 0x3096u},
    {0x30a1u, 0x30fau}, {0x3400u, 0x4dbfu}, {0x4e00u, 0x9fffu},  {0xac00u, 0xd7a3u}, {0xf900u, 0xfaffu},
    {0xff21u, 0xff3au}, {0xff41u, 0xff5au}, {0x20000u, 0x2fa1fu}};

static const unsigned int s_numbers[][2] = {{'0', '9'},         {0xb2u, 0xb3u},    {0xb9u, 0xb9u},   {0xbcu, 0xbeu},
                                            {0x660u, 0x669u},   {0x6f0u, 0x6f9u},  {0x966u, 0x96fu}, {0x2160u, 0x2188u},
                                            {0x2460u, 0x249bu}, {0xff10u, 0xff19u}};

typedef struct
{
    const CliCaseBlock *block;
    unsigned int capital;
    const unsigned int (*ranges)[2];
    unsigned int count;
    unsigned int letter;
} RegexpLettersCtx;
EMBED_INLINE int cli_case_contains(const RegexpLettersCtx *args)
{
    if ((args->capital < args->block->first) || (args->capital > args->block->last))
    {
        return 0;
    }
    return (args->block->parity == CLI_CASE_ALL) ||
           ((args->capital % 2u) == ((args->block->parity == CLI_CASE_EVEN) ? 0u : 1u));
}

unsigned int cli_regexp_lower(const CliRegexpLetterCfg *args)
{
    for (unsigned int at = 0u; at < (sizeof(s_lowered) / sizeof(s_lowered[0])); at += 1u)
    {
        if (args->letter == s_lowered[at][0])
        {
            return s_lowered[at][1];
        }
    }
    for (unsigned int at = 0u; at < (sizeof(s_case_blocks) / sizeof(s_case_blocks[0])); at += 1u)
    {
        if (EMBED_CALL(cli_case_contains, RegexpLettersCtx, .block = &s_case_blocks[at], .capital = args->letter))
        {
            return args->letter + s_case_blocks[at].offset;
        }
    }
    return args->letter;
}

unsigned int cli_regexp_upper(const CliRegexpLetterCfg *args)
{
    for (unsigned int at = 0u; at < (sizeof(s_raised) / sizeof(s_raised[0])); at += 1u)
    {
        if (args->letter == s_raised[at][0])
        {
            return s_raised[at][1];
        }
    }
    for (unsigned int at = 0u; at < (sizeof(s_case_blocks) / sizeof(s_case_blocks[0])); at += 1u)
    {
        const unsigned int offset = s_case_blocks[at].offset;
        if ((args->letter >= offset) && EMBED_CALL(cli_case_contains, RegexpLettersCtx, .block = &s_case_blocks[at],
                                                   .capital = args->letter - offset))
        {
            return args->letter - offset;
        }
    }
    return args->letter;
}

EMBED_INLINE int cli_on_range(const RegexpLettersCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if ((args->letter >= args->ranges[at][0]) && (args->letter <= args->ranges[at][1]))
        {
            return 1;
        }
    }
    return 0;
}

int cli_regexp_property(const CliRegexpLetterCfg *args)
{
    switch (args->property)
    {
    case CLI_REGEXP_LETTER:
        return ((args->letter >= 'a') && (args->letter <= 'z')) || ((args->letter >= 'A') && (args->letter <= 'Z')) ||
               EMBED_CALL(cli_on_range, RegexpLettersCtx, .ranges = s_letters,
                          .count = sizeof(s_letters) / sizeof(s_letters[0]), .letter = args->letter);
    case CLI_REGEXP_NUMBER:
        return EMBED_CALL(cli_on_range, RegexpLettersCtx, .ranges = s_numbers,
                          .count = sizeof(s_numbers) / sizeof(s_numbers[0]), .letter = args->letter);
    case CLI_REGEXP_UPPER:
        return cli_regexp_letter.lower(args) != args->letter;
    default:
        // the small letters with no capital: the sharp s, kra, the n after an apostrophe and the ordinal marks
        return (cli_regexp_letter.upper(args) != args->letter) || (args->letter == 0xdfu) || (args->letter == 0x138u) ||
               (args->letter == 0x149u) || (args->letter == 0xaau) || (args->letter == 0xbau);
    }
}
