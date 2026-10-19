// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_number.c: a 64-bit float written as serde_json writes it: the fewest digits that read back as the float, a tie
// between two of as few going to the nearer, and the point placed where its writer places it
#include "json_tree.h"
#include <stdlib.h>

// the 32-bit words of the largest whole number a float's exact digits are read from, its mantissa times five to 1074;
// the most digits that number writes; and the most digits a float needs to read back as itself
#define CLI_NUMBER_LIMBS 96u
#define CLI_NUMBER_DIGITS 832u
#define CLI_NUMBER_SHORTEST 17u

// the whole number in `limb`, `count` words, the lowest first, times `by`: its words
static unsigned int cli_limbs_times(unsigned int *limb, unsigned int count, unsigned int by)
{
    unsigned long long carry = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long product = ((unsigned long long)limb[at] * by) + carry;
        // the product's low 32 bits stay in the word and the rest carries
        limb[at] = (unsigned int)product;
        carry = product >> 32u;
    }
    if (carry != 0ull)
    {
        // a carry is below 2^32, `by` being
        limb[count] = (unsigned int)carry;
        count += 1u;
    }
    return count;
}

// the decimal digits of the whole number, above 0, into `digits`, the first not 0, the number taken apart as they are
// read: their count
static unsigned int cli_limbs_digits(unsigned int *limb, unsigned int count, char *digits)
{
    unsigned int chunk[(CLI_NUMBER_DIGITS / 9u) + 1u];
    unsigned int chunks = 0u;
    while (count > 0u)
    {
        unsigned long long rest = 0ull;
        for (unsigned int at = count; at > 0u; at -= 1u)
        {
            const unsigned long long part = (rest << 32u) | limb[at - 1u];
            // a quotient is below 2^32, the rest before it being below 10^9
            limb[at - 1u] = (unsigned int)(part / 1000000000ull);
            rest = part % 1000000000ull;
        }
        while ((count > 0u) && (limb[count - 1u] == 0u))
        {
            count -= 1u;
        }
        // a rest is below 10^9
        chunk[chunks] = (unsigned int)rest;
        chunks += 1u;
    }
    unsigned int written = 0u;
    for (unsigned int index = chunks; index > 0u; index -= 1u)
    {
        char nine[9];
        unsigned int value = chunk[index - 1u];
        for (unsigned int place = 9u; place > 0u; place -= 1u)
        {
            nine[place - 1u] = (char)('0' + (value % 10u));
            value /= 10u;
        }
        unsigned int from = 0u;
        while ((index == chunks) && (from < 8u) && (nine[from] == '0'))
        {
            from += 1u;
        }
        for (; from < 9u; from += 1u)
        {
            digits[written] = nine[from];
            written += 1u;
        }
    }
    return written;
}

// the exact decimal digits of a finite float above 0 into `digits`, which holds CLI_NUMBER_DIGITS, and where the point
// stands into `point`, the float being 0.d1d2... times ten to it: their count
static unsigned int cli_exact(double value, char *digits, int *point)
{
    unsigned long long bits = 0ull;
    memor.cpy(&(MemoriaCfg){.dst = &bits, .src = &value, .bytes = sizeof(bits)});
    // the exponent's field is 11 bits
    const unsigned int field = (unsigned int)((bits >> 52u) & 0x7ffull);
    unsigned long long mantissa = bits & ((1ull << 52u) - 1ull);
    int exponent = -1074;
    if (field != 0u)
    {
        mantissa |= 1ull << 52u;
        exponent = (int)field - 1075;
    }
    while ((mantissa & 1ull) == 0ull)
    {
        mantissa >>= 1u;
        exponent += 1;
    }
    unsigned int limb[CLI_NUMBER_LIMBS];
    // the mantissa's low and high 32 bits
    limb[0] = (unsigned int)mantissa;
    limb[1] = (unsigned int)(mantissa >> 32u);
    unsigned int count = (limb[1] != 0u) ? 2u : 1u;
    // the mantissa times two to the exponent where it is 0 or more, and times five to its negation, over ten to it,
    // where it is less
    for (int left = exponent; left > 0; left -= 31)
    {
        const int step = (left > 31) ? 31 : left;
        count = cli_limbs_times(limb, count, 1u << (unsigned int)step);
    }
    for (int left = -exponent; left > 0; left -= 13)
    {
        const int step = (left > 13) ? 13 : left;
        unsigned int by = 1u;
        for (int place = 0; place < step; place += 1)
        {
            by *= 5u;
        }
        count = cli_limbs_times(limb, count, by);
    }
    const unsigned int written = cli_limbs_digits(limb, count, digits);
    // a count of digits is below CLI_NUMBER_DIGITS, which an int holds
    *point = (int)written - ((exponent < 0) ? -exponent : 0);
    return written;
}

// the exact digits cut to `most`, rounded to the nearer, a tie to the even, into `cut`, the trailing 0s left out, the
// point moved up where 9s carry past the first: their count
static unsigned int cli_rounded(const char *digits, unsigned int count, unsigned int most, char *cut, int *point)
{
    const unsigned int taken = (count < most) ? count : most;
    memor.cpy(&(MemoriaCfg){.dst = cut, .src = digits, .bytes = taken});
    unsigned int kept = taken;
    if (count > most)
    {
        int up = digits[most] > '5';
        if (digits[most] == '5')
        {
            int beyond = 0;
            for (unsigned int at = most + 1u; at < count; at += 1u)
            {
                beyond = beyond || (digits[at] != '0');
            }
            up = beyond || (((cut[most - 1u] - '0') & 1) != 0);
        }
        if (up)
        {
            while ((kept > 0u) && (cut[kept - 1u] == '9'))
            {
                kept -= 1u;
            }
            if (kept == 0u)
            {
                cut[0] = '1';
                kept = 1u;
                *point += 1;
            }
            else
            {
                cut[kept - 1u] = (char)(cut[kept - 1u] + 1);
            }
        }
    }
    while ((kept > 1u) && (cut[kept - 1u] == '0'))
    {
        kept -= 1u;
    }
    return kept;
}

// whether `count` digits with the point where `point` stands read back as `value`
static int cli_reads_back(const char *cut, unsigned int count, int point, double value)
{
    char text[CLI_NUMBER_SHORTEST + 16u];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_character(&line, cut[0]);
    cli_line_character(&line, '.');
    cli_line_counted(&line, cut + 1, count - 1u);
    cli_line_character(&line, 'e');
    cli_line_signed(&line, (long long)point - 1ll);
    cli_line_closed(&line);
    return strtod(text, NULL) == value;
}

void cli_json_double(CliLine *out, double value)
{
    unsigned long long bits = 0ull;
    memor.cpy(&(MemoriaCfg){.dst = &bits, .src = &value, .bytes = sizeof(bits)});
    if (((bits >> 52u) & 0x7ffull) == 0x7ffull)
    {
        cli_line_text(out, "null");
        return;
    }
    if ((bits >> 63u) != 0ull)
    {
        cli_line_character(out, '-');
        value = -value;
    }
    if (value == 0.0)
    {
        cli_line_text(out, "0.0");
        return;
    }
    char digits[CLI_NUMBER_DIGITS];
    int exact_point = 0;
    const unsigned int count = cli_exact(value, digits, &exact_point);
    char cut[CLI_NUMBER_SHORTEST + 1u];
    unsigned int length = 0u;
    int point = exact_point;
    for (unsigned int most = 1u; most <= CLI_NUMBER_SHORTEST; most += 1u)
    {
        point = exact_point;
        length = cli_rounded(digits, count, most, cut, &point);
        if (cli_reads_back(cut, length, point, value))
        {
            break;
        }
    }
    // the digits stand for cut times ten to `shift`, the point `point` digits from their start
    const int shift = point - (int)length;
    if ((shift >= 0) && (point <= 16))
    {
        cli_line_counted(out, cut, length);
        for (int zero = 0; zero < shift; zero += 1)
        {
            cli_line_character(out, '0');
        }
        cli_line_text(out, ".0");
    }
    else if ((point > 0) && (point <= 16))
    {
        // the point stands inside the digits, so it is below their count
        cli_line_counted(out, cut, (unsigned long long)point);
        cli_line_character(out, '.');
        cli_line_counted(out, cut + point, length - (unsigned int)point);
    }
    else if ((point > -5) && (point <= 0))
    {
        cli_line_text(out, "0.");
        for (int zero = point; zero < 0; zero += 1)
        {
            cli_line_character(out, '0');
        }
        cli_line_counted(out, cut, length);
    }
    else
    {
        cli_line_character(out, cut[0]);
        if (length > 1u)
        {
            cli_line_character(out, '.');
            cli_line_counted(out, cut + 1, length - 1u);
        }
        cli_line_character(out, 'e');
        cli_line_signed(out, (long long)point - 1ll);
    }
}
