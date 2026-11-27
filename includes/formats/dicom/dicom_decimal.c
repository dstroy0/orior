// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dicom_decimal.c: decimal strings and the order of slices
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "dicom_internal.h"
#include "memoria_operor/memoria_operor.h"

static int dicom_decimal_parse(const unsigned char *text, unsigned long long length, DicomDecimal *out,
                               EngineError *error)
{
    // Explicit casts read the bytes of the element as its text, and its length as a count in memory
    unsigned long long at =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = (const char *)text, .cap = (size_t)length, .other = " ");
    unsigned long long end = length;
    while ((end > at) && ((text[end - 1ULL] == ' ') || (text[end - 1ULL] == '\0')))
    {
        end -= 1ULL;
    }
    int negative = 0;
    if ((at < end) && ((text[at] == '+') || (text[at] == '-')))
    {
        negative = (text[at] == '-') ? 1 : 0;
        at += 1ULL;
    }
    orior_exact_zero(&out->mantissa);
    unsigned long long count = 0ULL;
    long long fraction = 0LL;
    int seen_point = 0;
    while ((at < end) && (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = text[at]) ||
                          ((text[at] == '.') && (seen_point == 0))))
    {
        if (text[at] == '.')
        {
            seen_point = 1;
        }
        else
        {
            // a decimal digit character fits in a char
            const char digit_text = (char)text[at];
            OriorExactInteger digit;
            OriorExactInteger next;
            if (!DICOM_CHECK((fraction < LLONG_MAX) &&
                                 (orior_exact_scale_by_ten(&out->mantissa, 1u) == ORIOR_EXACT_OK) &&
                                 (orior_exact_from_decimal(&digit_text, 1u, 0u, &digit) == ORIOR_EXACT_OK) &&
                                 (orior_exact_add(&out->mantissa, &digit, &next) == ORIOR_EXACT_OK),
                             text, error, ENGINE_ERROR_REQUEST))
            {
                return 0;
            }
            out->mantissa = next;
            count += 1ULL;
            fraction += (seen_point != 0) ? 1LL : 0LL;
        }
        at += 1ULL;
    }
    long long exponent = 0LL;
    if ((at < end) && ((text[at] == 'e') || (text[at] == 'E')))
    {
        at += 1ULL;
        int exponent_negative = 0;
        if ((at < end) && ((text[at] == '+') || (text[at] == '-')))
        {
            exponent_negative = (text[at] == '-') ? 1 : 0;
            at += 1ULL;
        }
        unsigned long long exponent_digits = 0ULL;
        while ((at < end) && EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = text[at]))
        {
            if (!DICOM_CHECK(exponent <= (((LLONG_MAX / 4LL) - 9LL) / 10LL), text, error, ENGINE_ERROR_REQUEST))
            {
                return 0;
            }
            // one decimal digit's value, 0 to 9, fits a long long exactly
            exponent = (exponent * 10LL) + (long long)(text[at] - '0');
            exponent_digits += 1ULL;
            at += 1ULL;
        }
        if (!DICOM_CHECK(exponent_digits != 0ULL, text, error, ENGINE_ERROR_REQUEST))
        {
            return 0;
        }
        exponent = (exponent_negative != 0) ? -exponent : exponent;
    }
    if (!DICOM_CHECK((count != 0ULL) && (at == end), text, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    out->mantissa.sign = ((negative != 0) && (out->mantissa.sign != 0)) ? -out->mantissa.sign : out->mantissa.sign;
    out->exponent = exponent - fraction;
    return 1;
}

int dicom_decimal_list(const unsigned char *text, unsigned long long length, unsigned int want, DicomDecimal *out,
                       EngineError *error)
{
    unsigned long long start = 0ULL;
    unsigned int found = 0u;
    for (unsigned long long at = 0ULL; at <= length; at += 1ULL)
    {
        if ((at == length) || (text[at] == '\\'))
        {
            if (!DICOM_CHECK(found < want, text, error, ENGINE_ERROR_REQUEST) ||
                !dicom_decimal_parse(text + start, at - start, &out[found], error))
            {
                return 0;
            }
            found += 1u;
            start = at + 1ULL;
        }
    }
    return DICOM_CHECK(found == want, text, error, ENGINE_ERROR_REQUEST);
}

int dicom_decimal_align(DicomDecimal *value, long long exponent, EngineError *error)
{
    if (!DICOM_CHECK((exponent <= value->exponent) && ((value->exponent - exponent) <= (long long)ORIOR_EXACT_DIGITS),
                     value, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    // the gap is at max ORIOR_EXACT_DIGITS. It fits a uint32_t exactly
    const uint32_t power = (uint32_t)(value->exponent - exponent);
    if (!DICOM_CHECK(orior_exact_scale_by_ten(&value->mantissa, power) == ORIOR_EXACT_OK, value, error,
                     ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    value->exponent = exponent;
    return 1;
}

static int dicom_decimal_multiply(const DicomDecimal *left, const DicomDecimal *right, DicomDecimal *out,
                                  EngineError *error)
{
    out->exponent = left->exponent + right->exponent;
    return DICOM_CHECK(orior_exact_multiply(&left->mantissa, &right->mantissa, &out->mantissa) == ORIOR_EXACT_OK, left,
                       error, ENGINE_ERROR_REQUEST);
}

static int dicom_decimal_combine(const DicomDecimal *left, const DicomDecimal *right, int subtract, DicomDecimal *out,
                                 EngineError *error)
{
    DicomDecimal one = *left;
    DicomDecimal other = *right;
    const long long exponent = (one.exponent < other.exponent) ? one.exponent : other.exponent;
    if (!dicom_decimal_align(&one, exponent, error) || !dicom_decimal_align(&other, exponent, error))
    {
        return 0;
    }
    out->exponent = exponent;
    const OriorExactStatus status = (subtract != 0)
                                        ? orior_exact_subtract(&one.mantissa, &other.mantissa, &out->mantissa)
                                        : orior_exact_add(&one.mantissa, &other.mantissa, &out->mantissa);
    return DICOM_CHECK(status == ORIOR_EXACT_OK, left, error, ENGINE_ERROR_REQUEST);
}

static int dicom_cross_term(const DicomDecimal *orientation, unsigned int first, unsigned int second, DicomDecimal *out,
                            EngineError *error)
{
    DicomDecimal ahead;
    DicomDecimal behind;
    return dicom_decimal_multiply(&orientation[first], &orientation[3u + second], &ahead, error) &&
           dicom_decimal_multiply(&orientation[second], &orientation[3u + first], &behind, error) &&
           dicom_decimal_combine(&ahead, &behind, 1, out, error);
}

int dicom_slice_key(const DicomDecimal *orientation, DicomSlice *slice, EngineError *error)
{
    DicomDecimal normal[3];
    if (!dicom_cross_term(orientation, 1u, 2u, &normal[0], error) ||
        !dicom_cross_term(orientation, 2u, 0u, &normal[1], error) ||
        !dicom_cross_term(orientation, 0u, 1u, &normal[2], error))
    {
        return 0;
    }
    DicomDecimal sum;
    if (!dicom_decimal_multiply(&normal[0], &slice->position[0], &sum, error))
    {
        return 0;
    }
    for (unsigned int axis = 1u; axis < 3u; axis += 1u)
    {
        DicomDecimal term;
        DicomDecimal next;
        if (!dicom_decimal_multiply(&normal[axis], &slice->position[axis], &term, error) ||
            !dicom_decimal_combine(&sum, &term, 0, &next, error))
        {
            return 0;
        }
        sum = next;
    }
    slice->key = sum;
    return 1;
}

// the end of the part of the UID `uid` that begins at `from`: the next dot, or `length` where none follows. Explicit
// casts take the bytes left as a count in memory, and read the found byte as a byte of the UID
static unsigned long long dicom_uid_part_end(const unsigned char *uid, unsigned long long from,
                                             unsigned long long length)
{
    const void *const dot =
        EMBED_CALL(memor.chr, MemoriaCfg, .src = &uid[from], .bytes = (size_t)(length - from), .val = '.');
    return (dot != NULL) ? (unsigned long long)((const unsigned char *)dot - uid) : length;
}

// the leading zeros of the part from `from` to `end`, with its last byte kept where every byte is a zero. Explicit
// casts read the bytes as text, and take the bytes of the part as a count in memory
static unsigned long long dicom_uid_zeros(const unsigned char *uid, unsigned long long from, unsigned long long end)
{
    const unsigned long long room = ((end - from) > 1ULL) ? ((end - from) - 1ULL) : 0ULL;
    return EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = (const char *)&uid[from], .cap = (size_t)room, .other = "0");
}

static int dicom_uid_compare(const unsigned char *left, unsigned long long left_length, const unsigned char *right,
                             unsigned long long right_length)
{
    unsigned long long one = 0ULL;
    unsigned long long other = 0ULL;
    while ((one < left_length) || (other < right_length))
    {
        const unsigned long long one_end = dicom_uid_part_end(left, one, left_length);
        const unsigned long long other_end = dicom_uid_part_end(right, other, right_length);
        one += dicom_uid_zeros(left, one, one_end);
        other += dicom_uid_zeros(right, other, other_end);
        const unsigned long long one_span = one_end - one;
        const unsigned long long other_span = other_end - other;
        if (one_span != other_span)
        {
            return (one_span < other_span) ? -1 : 1;
        }
        const int differ = (one_span != 0ULL) ? EMBED_CALL(memor.cmp, MemoriaCfg, .src = left + one,
                                                           .other = right + other, .bytes = (size_t)one_span)
                                              : 0;
        if (differ != 0)
        {
            return (differ < 0) ? -1 : 1;
        }
        one = (one_end < left_length) ? (one_end + 1ULL) : one_end;
        other = (other_end < right_length) ? (other_end + 1ULL) : other_end;
    }
    return 0;
}

int dicom_order_slices(const void *left, const void *right)
{
    const DicomSlice *const one = &g_dicom_sorting[*(const unsigned long long *)left];
    const DicomSlice *const other = &g_dicom_sorting[*(const unsigned long long *)right];
    if (g_dicom_keyed != 0u)
    {
        const int keyed = orior_exact_compare(&one->key.mantissa, &other->key.mantissa);
        if (keyed != 0)
        {
            return keyed;
        }
    }
    if (one->has_instance != other->has_instance)
    {
        return (one->has_instance != 0u) ? -1 : 1;
    }
    if (one->has_instance != 0u)
    {
        const int instance = orior_exact_compare(&one->instance.mantissa, &other->instance.mantissa);
        if (instance != 0)
        {
            return instance;
        }
    }
    const int uid = dicom_uid_compare(one->sop, one->sop_length, other->sop, other->sop_length);
    if (uid != 0)
    {
        return uid;
    }
    const unsigned long long shorter = (one->name_length < other->name_length) ? one->name_length : other->name_length;
    const int named =
        EMBED_CALL(memor.cmp, MemoriaCfg, .src = one->name, .other = other->name, .bytes = (size_t)shorter);
    if (named != 0)
    {
        return named;
    }
    return (one->name_length < other->name_length) ? -1 : ((one->name_length > other->name_length) ? 1 : 0);
}
