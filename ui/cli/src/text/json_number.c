// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_number.c: a 64-bit float written as serde_json writes it: the fewest digits that read back as the float, a tie
// between two of as few going to the nearer, and the point placed where its writer places it
#include "json_tree.h"
#include "text.h"
#include <stdlib.h>

// the 32-bit words of the largest whole number a float's exact digits are read from, its mantissa times five to 1074;
// the most digits that number writes; and the most digits a float needs to read back as itself
#define CLI_NUMBER_LIMBS 96u
#define CLI_NUMBER_DIGITS 832u
#define CLI_NUMBER_SHORTEST 17u

typedef struct
{
    unsigned int *limb;
    unsigned int count;
    unsigned int by;
    char *digits;
    double value;
    int *point;
    const char *digits_read;
    unsigned int most;
    char *cut;
    const char *cut_read;
    int point_value;
} JsonNumberCtx;
// the whole number in `limb`, `count` words, the lowest first, times `by`: its words
EMBED_INLINE unsigned int cli_limbs_times(const JsonNumberCtx *args)
{
    unsigned int count = args->count;
    unsigned long long carry = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long product = ((unsigned long long)args->limb[at] * args->by) + carry;
        // the product's low 32 bits stay in the word and the rest carries
        args->limb[at] = (unsigned int)product;
        carry = product >> 32u;
    }
    if (carry != 0ULL)
    {
        // a carry is below 2^32, `by` being
        args->limb[count] = (unsigned int)carry;
        count += 1u;
    }
    return count;
}

// the decimal digits of the whole number, above 0, into `digits`, the first not 0, the number taken apart as they are
// read: their count
EMBED_INLINE unsigned int cli_limbs_digits(const JsonNumberCtx *args)
{
    unsigned int count = args->count;
    unsigned int chunk[(CLI_NUMBER_DIGITS / 9u) + 1u];
    unsigned int chunks = 0u;
    while (count > 0u)
    {
        unsigned long long rest = 0ULL;
        for (unsigned int at = count; at > 0u; at -= 1u)
        {
            const unsigned long long part = (rest << 32u) | args->limb[at - 1u];
            // a quotient is below 2^32, the rest before it being below 10^9
            args->limb[at - 1u] = (unsigned int)(part / 1000000000ULL);
            rest = part % 1000000000ULL;
        }
        while ((count > 0u) && (args->limb[count - 1u] == 0u))
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
            args->digits[written] = nine[from];
            written += 1u;
        }
    }
    return written;
}

// the exact decimal digits of a finite float above 0 into `digits`, sized for CLI_NUMBER_DIGITS, and where the point
// stands into `point`, the float being 0.d1d2... times ten to it: their count
EMBED_INLINE unsigned int cli_exact(const JsonNumberCtx *args)
{
    double value = args->value;
    int *point = args->point;
    unsigned long long bits = 0ULL;
    memor.cpy(&(MemoriaCfg){.dst = &bits, .src = &value, .bytes = sizeof(bits)});
    // the exponent's field is 11 bits
    const unsigned int field = (unsigned int)((bits >> 52u) & 0x7ffULL);
    unsigned long long mantissa = bits & ((1ULL << 52u) - 1ULL);
    int exponent = -1074;
    if (field != 0u)
    {
        mantissa |= 1ULL << 52u;
        exponent = (int)field - 1075;
    }
    while ((mantissa & 1ULL) == 0ULL)
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
        count =
            EMBED_CALL(cli_limbs_times, JsonNumberCtx, .limb = limb, .count = count, .by = 1u << (unsigned int)step);
    }
    for (int left = -exponent; left > 0; left -= 13)
    {
        const int step = (left > 13) ? 13 : left;
        unsigned int by = 1u;
        for (int place = 0; place < step; place += 1)
        {
            by *= 5u;
        }
        count = EMBED_CALL(cli_limbs_times, JsonNumberCtx, .limb = limb, .count = count, .by = by);
    }
    const unsigned int written =
        EMBED_CALL(cli_limbs_digits, JsonNumberCtx, .limb = limb, .count = count, .digits = args->digits);
    // a count of digits is below CLI_NUMBER_DIGITS, which an int represents
    *point = (int)written - ((exponent < 0) ? -exponent : 0);
    return written;
}

// the exact digits cut to `most`, rounded to the nearer, a tie to the even, into `cut`, the trailing 0s left out, the
// point moved up where 9s carry past the first: their count
EMBED_INLINE unsigned int cli_round(const JsonNumberCtx *args)
{
    int *point = args->point;
    const unsigned int reserved = (args->count < args->most) ? args->count : args->most;
    memor.cpy(&(MemoriaCfg){.dst = args->cut, .src = args->digits_read, .bytes = reserved});
    unsigned int cut_len = reserved;
    if (args->count > args->most)
    {
        int up = args->digits_read[args->most] > '5';
        if (args->digits_read[args->most] == '5')
        {
            int beyond = 0;
            for (unsigned int at = args->most + 1u; at < args->count; at += 1u)
            {
                beyond = beyond || (args->digits_read[at] != '0');
            }
            up = beyond || (((args->cut[args->most - 1u] - '0') & 1) != 0);
        }
        if (up)
        {
            cut_len = (unsigned int)EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                               .text = args->cut, .at = cut_len, .set = "9");
            if (cut_len == 0u)
            {
                args->cut[0] = '1';
                cut_len = 1u;
                *point += 1;
            }
            else
            {
                args->cut[cut_len - 1u] = (char)(args->cut[cut_len - 1u] + 1);
            }
        }
    }
    cut_len = (unsigned int)(1u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                             .text = args->cut + 1u, .at = cut_len - 1u, .set = "0"));
    return cut_len;
}

// whether `count` digits with the point where `point` stands read back as `value`
EMBED_INLINE int cli_reads_back(const JsonNumberCtx *args)
{
    char text[CLI_NUMBER_SHORTEST + 16u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_ch(&line, args->cut_read[0]);
    mmgr_line_ch(&line, '.');
    mmgr_line_put_n(&line, args->cut_read + 1, args->count - 1u);
    mmgr_line_ch(&line, 'e');
    mmgr_line_decimal_signed(&line, (long long)args->point_value - 1LL);
    mmgr_line_finish(&line);
    return strtod(text, NULL) == args->value;
}

void cli_json_double(const CliJsonTreeOutValueCfg *args)
{
    double value = args->value_f64;
    unsigned long long bits = 0ULL;
    memor.cpy(&(MemoriaCfg){.dst = &bits, .src = &value, .bytes = sizeof(bits)});
    if (((bits >> 52u) & 0x7ffULL) == 0x7ffULL)
    {
        mmgr_line_put(args->out, "null");
        return;
    }
    if ((bits >> 63u) != 0ULL)
    {
        mmgr_line_ch(args->out, '-');
        value = -value;
    }
    if (value == 0.0)
    {
        mmgr_line_put(args->out, "0.0");
        return;
    }
    char digits[CLI_NUMBER_DIGITS];
    int exact_point = 0;
    const unsigned int count =
        EMBED_CALL(cli_exact, JsonNumberCtx, .value = value, .digits = digits, .point = &exact_point);
    char cut[CLI_NUMBER_SHORTEST + 1u];
    unsigned int length = 0u;
    int point = exact_point;
    for (unsigned int most = 1u; most <= CLI_NUMBER_SHORTEST; most += 1u)
    {
        point = exact_point;
        length = EMBED_CALL(cli_round, JsonNumberCtx, .digits_read = digits, .count = count, .most = most, .cut = cut,
                            .point = &point);
        if (EMBED_CALL(cli_reads_back, JsonNumberCtx, .cut_read = cut, .count = length, .point_value = point,
                       .value = value))
        {
            break;
        }
    }
    // the digits stand for cut times ten to `shift`, the point `point` digits from their start
    const int shift = point - (int)length;
    if ((shift >= 0) && (point <= 16))
    {
        mmgr_line_put_n(args->out, cut, length);
        for (int zero = 0; zero < shift; zero += 1)
        {
            mmgr_line_ch(args->out, '0');
        }
        mmgr_line_put(args->out, ".0");
    }
    else if ((point > 0) && (point <= 16))
    {
        // the point stands inside the digits, so it is below their count
        mmgr_line_put_n(args->out, cut, (unsigned long long)point);
        mmgr_line_ch(args->out, '.');
        mmgr_line_put_n(args->out, cut + point, length - (unsigned int)point);
    }
    else if ((point > -5) && (point <= 0))
    {
        mmgr_line_put(args->out, "0.");
        for (int zero = point; zero < 0; zero += 1)
        {
            mmgr_line_ch(args->out, '0');
        }
        mmgr_line_put_n(args->out, cut, length);
    }
    else
    {
        mmgr_line_ch(args->out, cut[0]);
        if (length > 1u)
        {
            mmgr_line_ch(args->out, '.');
            mmgr_line_put_n(args->out, cut + 1, length - 1u);
        }
        mmgr_line_ch(args->out, 'e');
        mmgr_line_decimal_signed(args->out, (long long)point - 1LL);
    }
}
