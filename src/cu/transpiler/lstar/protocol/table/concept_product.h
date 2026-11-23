// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// concept_product.h: a pair's product, what its two forms make of what they read, recorded put by put and named once it
// is whole (P13, the concepts)
//
// A put reads its product at the form. The values the form reads, each as the part stores it just before the form.
// Then the value each of the two forms writes, each as the part stores it just after. A case is alike where the two
// forms write one value and apart where they write two. The product is what came back at each combination of the
// values read. The links of a carrier before the form and after it are deltas of their own, and enter no product.
//
// A product is a concept once it is whole, a row at every combination of the values of the cases. Two pairs of one
// product are one concept, whatever forms name them and whatever carriers carried them. `orior_klq_identity` puts a
// pair until its product is whole, and `orior_klq_decoder` writes the concept beneath the pair
#ifndef CONCEPT_PRODUCT_H
#define CONCEPT_PRODUCT_H

#include "../../../../../platform/platform.h"
#include "carrier_flow.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// the types of value a form reads: a word, a wide, a register and its `.hi` read as one, and a flag
enum ConceptValueType
{
    CONCEPT_VALUE_WORD,
    CONCEPT_VALUE_WIDE,
    CONCEPT_VALUE_FLAG
};

// the values a form reads at max, as the form's reads are at max
#define CONCEPT_READS_MAX CARRIER_FORM_READS_MAX

// One value a form reads, as the form writes it, and its value on every case, in the cases' order. `count` of them, in
// a cell of the temporary tier
struct ConceptRead
{
    CarrierName name;
    unsigned long long *values;
    unsigned long long count;
};

struct ConceptReads
{
    ConceptRead read[CONCEPT_READS_MAX];
    unsigned int count;
};

// the reads and their values given back
static inline void concept_reads_ended(ConceptReads *reads)
{
    for (unsigned int at = 0u; at < reads->count; at += 1u)
    {
        platform_cell_released(reads->read[at].values, platform_place_g_program_owner);
        reads->read[at].values = NULL;
        reads->read[at].count = 0ULL;
    }
    reads->count = 0u;
}

// the marks of a row of a product: what came back at it, alike, apart, or both
#define CONCEPT_ALIKE 1u
#define CONCEPT_APART 2u

// the bytes of one row's key, its terminator among them: each value read in hex with a comma after it
#define CONCEPT_ROW_BYTES 256u

// one row of a product: the values read, written in hex with a comma after each, and what came back at them
struct ConceptRow
{
    char key[CONCEPT_ROW_BYTES];
    unsigned int value;
};

// A pair's product. The type of each value its form reads, and what came back at each combination of them. Its rows
// are a PlatformArray of ConceptRow ordered by key, each key once
struct ConceptProduct
{
    int types[CONCEPT_READS_MAX];
    unsigned int type_count;
    PlatformArray rows;
    // 1 where two puts read the form as different values, or a row passes its bytes, and the pair records no product
    int mixed;
};

// the product begun empty, its rows on the temporary tier where `ephemeral` is 1
static inline void concept_product_begun(ConceptProduct *product, int ephemeral)
{
    product->type_count = 0u;
    platform_array_begun(&product->rows, sizeof(ConceptRow), platform_place_g_program_owner, ephemeral);
    product->mixed = 0;
}

// the row at `at` [BORROWS]
static inline const ConceptRow *concept_row_at(const ConceptProduct *product, unsigned long long at)
{
    return (const ConceptRow *)platform_array_at(&product->rows, at);
}

static inline void concept_product_ended(ConceptProduct *product)
{
    platform_array_ended(&product->rows);
    product->type_count = 0u;
    product->mixed = 0;
}

// the values of one type of the cases at max, and the values of one type in their order, each once
#define CONCEPT_CASED_MAX 256u

struct ConceptType
{
    unsigned long long value[CONCEPT_CASED_MAX];
    unsigned int count;
};

// 1 where `type` holds `value`
static inline int concept_type_held(const ConceptType *type, unsigned long long value)
{
    unsigned int low = 0u;
    unsigned int high = type->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (type->value[middle] < value)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return (low < type->count) && (type->value[low] == value);
}

// `value` put into `type` in its order, where it does not hold it yet and holds fewer than CONCEPT_CASED_MAX
static inline void concept_type_added(ConceptType *type, unsigned long long value)
{
    if (concept_type_held(type, value) || (type->count >= CONCEPT_CASED_MAX))
    {
        return;
    }
    unsigned int at = type->count;
    while ((at > 0u) && (type->value[at - 1u] > value))
    {
        type->value[at] = type->value[at - 1u];
        at -= 1u;
    }
    type->value[at] = value;
    type->count += 1u;
}

// the values of the cases, by type
struct ConceptCased
{
    ConceptType type[3];
};

static inline void concept_cased_ended(ConceptCased *cased)
{
    for (unsigned int type = 0u; type < 3u; type += 1u)
    {
        cased->type[type].count = 0u;
    }
}

// The values of the cases, by type, from the `count` operands at `operands` into `cased`. Each operand is a wide, its
// low and its high word a word, and a flag is 1 or 0
static inline void concept_values(const unsigned long long *operands, unsigned long long count, ConceptCased *cased)
{
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        const unsigned long long value = operands[at];
        concept_type_added(&cased->type[CONCEPT_VALUE_WIDE], value);
        concept_type_added(&cased->type[CONCEPT_VALUE_WORD], value & 0xffffffffULL);
        concept_type_added(&cased->type[CONCEPT_VALUE_WORD], value >> 32u);
    }
    concept_type_added(&cased->type[CONCEPT_VALUE_FLAG], 0ULL);
    concept_type_added(&cased->type[CONCEPT_VALUE_FLAG], 1ULL);
}

// The values of the cases, `cased`, with the values `qualifiers` the pair's forms write of their own, each a
// word and a wide, into `qualified`. A value a form brings decides where the two forms agree, and the pair's product
// contains a row at it
static inline void concept_values_qualified(const ConceptCased *cased, const CarrierNumbers *qualifiers,
                                            ConceptCased *qualified)
{
    for (unsigned int type = 0u; type < 3u; type += 1u)
    {
        for (unsigned int at = 0u; at < cased->type[type].count; at += 1u)
        {
            concept_type_added(&qualified->type[type], cased->type[type].value[at]);
        }
    }
    for (unsigned int at = 0u; at < qualifiers->count; at += 1u)
    {
        const unsigned long long value = qualifiers->number[at];
        concept_type_added(&qualified->type[CONCEPT_VALUE_WORD], value & 0xffffffffULL);
        concept_type_added(&qualified->type[CONCEPT_VALUE_WIDE], value);
    }
}

// 1 where `product` is a qualifier's: apart on some row and alike on some. It is alike only on rows where a value the
// form reads is one of `qualifiers`, the values its forms write of their own
static inline int concept_qualified(const ConceptProduct *product, const CarrierNumbers *qualifiers)
{
    int alike = 0;
    int apart = 0;
    int qualified = 1;
    for (unsigned long long at = 0ULL; at < product->rows.count; at += 1ULL)
    {
        const ConceptRow *const row = concept_row_at(product, at);
        const int row_alike = ((row->value & CONCEPT_ALIKE) != 0u) ? 1 : 0;
        alike |= row_alike;
        apart |= ((row->value & CONCEPT_APART) != 0u) ? 1 : 0;
        int reads_qualifier = 0;
        // each value of the row ends at its comma, and a value read as hex stops at it
        for (const char *value = row->key; *value != '\0';)
        {
            const char *const comma =
                EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ',');
            if (comma == NULL)
            {
                break;
            }
            reads_qualifier |= carrier_number_held(qualifiers, EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                                          .src = value, .end = NULL, .base = 16))
                                   ? 1
                                   : 0;
            value = comma + 1;
        }
        qualified &= (!row_alike || reads_qualifier) ? 1 : 0;
    }
    return (alike && apart && qualified) ? 1 : 0;
}

// the order of two row keys as their bytes order
static inline int concept_key_order(const char *left, const char *right)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t right_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const int bytes = EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right,
                                 .bytes = (left_length < right_length) ? left_length : right_length);
    return (bytes != 0) ? bytes : ((left_length < right_length) ? -1 : ((left_length > right_length) ? 1 : 0));
}

// The rows of `product` ordered by key, each key once, its marks joined. The rows from `sorted` on are this put's,
// after the rows ordered already. 1, or 0 where the pool cannot meet the merge
static inline int concept_rows_settled(ConceptProduct *product, unsigned long long sorted)
{
    ConceptRow *const rows = (ConceptRow *)product->rows.cell;
    const unsigned long long count = product->rows.count;
    if (count == sorted)
    {
        return 1;
    }
    ConceptRow *const merged = (ConceptRow *)platform_cell_taken(
        count * sizeof(ConceptRow), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (merged == NULL)
    {
        return 0;
    }
    // this put's rows ordered by key, a merge of runs that doubles each pass, with the merged cell as its spare
    ConceptRow *from = &rows[sorted];
    ConceptRow *to = merged;
    const unsigned long long added = count - sorted;
    for (unsigned long long width = 1ULL; width < added; width *= 2ULL)
    {
        for (unsigned long long left = 0ULL; left < added; left += 2ULL * width)
        {
            const unsigned long long middle = ((left + width) < added) ? (left + width) : added;
            const unsigned long long right = ((left + (2ULL * width)) < added) ? (left + (2ULL * width)) : added;
            unsigned long long first = left;
            unsigned long long second = middle;
            for (unsigned long long put = left; put < right; put += 1ULL)
            {
                const int take_second =
                    (second < right) &&
                    ((first >= middle) || (concept_key_order(from[second].key, from[first].key) < 0));
                to[put] = take_second ? from[second] : from[first];
                second += take_second ? 1ULL : 0ULL;
                first += take_second ? 0ULL : 1ULL;
            }
        }
        ConceptRow *const swapped = to;
        to = from;
        from = swapped;
    }
    if (from != &rows[sorted])
    {
        // the rows are at max the pool's bytes, which a size_t counts
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &rows[sorted], .src = from,
                   .bytes = (size_t)(added * sizeof(ConceptRow)));
    }
    unsigned long long kept = 0ULL;
    unsigned long long one = 0ULL;
    unsigned long long other = sorted;
    while ((one < sorted) || (other < count))
    {
        const int take_other =
            (other < count) && ((one >= sorted) || (concept_key_order(rows[other].key, rows[one].key) < 0));
        const ConceptRow *const next = take_other ? &rows[other] : &rows[one];
        if ((kept != 0ULL) && (concept_key_order(merged[kept - 1ULL].key, next->key) == 0))
        {
            merged[kept - 1ULL].value |= next->value;
        }
        else
        {
            merged[kept] = *next;
            kept += 1ULL;
        }
        other += take_other ? 1ULL : 0ULL;
        one += take_other ? 0ULL : 1ULL;
    }
    // the elements are at max the pool's bytes, which a size_t counts
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = rows, .src = merged, .bytes = (size_t)(kept * sizeof(ConceptRow)));
    platform_cell_released(merged, platform_place_g_program_owner);
    return platform_array_counted(&product->rows, kept);
}

// A put's product, recorded into `product`. What came back of each case, the `count` values of `writes_from` and of
// `writes_to` one value or two, as a function of the values `reads` the form reads there. A register and its `.hi` are
// one wide value, in the order the form names them. Each combination is alike, apart, or both where the cases that
// contain it came back both. The product reads a parameter the chain binds to a literal by its name after `=`, and a
// flag's after `P=`, a flag as a register `P` is. The product records a combination where every value is one of the
// cases', `cased`, so that every carrier records the same rows
static inline void concept_product_made(const ConceptReads *reads, const unsigned long long *writes_from,
                                        const unsigned long long *writes_to, unsigned long long count,
                                        const ConceptCased *cased, ConceptProduct *product)
{
    // Each value the form reads, by the register its pair's low word goes by, and the reads of its low and high word.
    // A base is the bytes of its read's name
    CarrierPiece bases[CONCEPT_READS_MAX];
    long low_at[CONCEPT_READS_MAX];
    long high_at[CONCEPT_READS_MAX];
    unsigned int base_count = 0u;
    for (unsigned int at = 0u; at < reads->count; at += 1u)
    {
        const char *const read = reads->read[at].name.text;
        const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = read, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        const int high = (length > 3u) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &read[length - 3u], .other = ".hi",
                                                      .bytes = 3u) == 0);
        const CarrierPiece base = {read, high ? (length - 3u) : length};
        unsigned int found = base_count;
        for (unsigned int each = 0u; (found == base_count) && (each < base_count); each += 1u)
        {
            found = ((bases[each].length == base.length) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = bases[each].text,
                                                                        .other = base.text, .bytes = base.length) == 0))
                        ? each
                        : found;
        }
        if (found == base_count)
        {
            bases[base_count] = base;
            low_at[base_count] = -1L;
            high_at[base_count] = -1L;
            base_count += 1u;
        }
        // a read's place is far below 2^63
        (high ? high_at : low_at)[found] = (long)at;
    }
    int types[CONCEPT_READS_MAX];
    for (unsigned int value = 0u; value < base_count; value += 1u)
    {
        const char first = bases[value].text[0];
        const int both = (low_at[value] >= 0) && (high_at[value] >= 0);
        types[value] = (first == 'P')   ? (int)CONCEPT_VALUE_FLAG
                       : (first == '[') ? (int)CONCEPT_VALUE_WIDE
                       : both           ? (int)CONCEPT_VALUE_WIDE
                                        : (int)CONCEPT_VALUE_WORD;
    }
    int same_types = base_count != 0u;
    if ((base_count != 0u) && (product->rows.count == 0ULL) && (product->type_count == 0u))
    {
        for (unsigned int at = 0u; at < base_count; at += 1u)
        {
            product->types[at] = types[at];
        }
        product->type_count = base_count;
    }
    same_types = same_types && (product->type_count == base_count);
    for (unsigned int at = 0u; same_types && (at < base_count); at += 1u)
    {
        same_types = product->types[at] == types[at];
    }
    if ((base_count != 0u) && !same_types)
    {
        product->mixed = 1;
    }
    const unsigned long long sorted = product->rows.count;
    for (unsigned long long place = 0ULL; same_types && (place < count); place += 1ULL)
    {
        int every_read = 1;
        for (unsigned int each = 0u; each < reads->count; each += 1u)
        {
            every_read &= (place < reads->read[each].count) ? 1 : 0;
        }
        if (!every_read)
        {
            continue;
        }
        ConceptRow row;
        row.value = (writes_from[place] == writes_to[place]) ? CONCEPT_ALIKE : CONCEPT_APART;
        VerbaLinea values_line = mmgr_line_over(row.key, sizeof(row.key));
        int cased_all = 1;
        for (unsigned int value = 0u; value < base_count; value += 1u)
        {
            const long low_place = low_at[value];
            const long high_place = high_at[value];
            const unsigned long long low = (low_place >= 0) ? reads->read[low_place].values[place] : 0ULL;
            const unsigned long long high = (high_place >= 0) ? reads->read[high_place].values[place] : 0ULL;
            const unsigned long long read = ((low_place >= 0) && (high_place >= 0))
                                                ? ((high << 32u) | (low & 0xffffffffULL))
                                            : (low_place >= 0) ? low
                                                               : high;
            cased_all &= concept_type_held(&cased->type[types[value]], read) ? 1 : 0;
            mmgr_line_hex(&values_line, read, 1u);
            mmgr_line_ch(&values_line, ',');
        }
        const int fits = mmgr_line_ok(&values_line) != 0;
        (void)mmgr_line_finish(&values_line);
        product->mixed |= fits ? 0 : 1;
        if (cased_all && fits)
        {
            (void)platform_array_appended(&product->rows, &row);
        }
    }
    if (!concept_rows_settled(product, sorted))
    {
        product->mixed = 1;
    }
}

// the rows of `product` that came back one way, alike or apart: its signal, the rest its noise
static inline unsigned long long concept_signal(const ConceptProduct *product)
{
    unsigned long long signal = 0ULL;
    for (unsigned long long at = 0ULL; at < product->rows.count; at += 1ULL)
    {
        signal += (concept_row_at(product, at)->value != (CONCEPT_ALIKE | CONCEPT_APART)) ? 1ULL : 0ULL;
    }
    return signal;
}

// 1 where `product` is whole: a row at every combination of the values of the cases, `cased`
static inline int concept_whole(const ConceptProduct *product, const ConceptCased *cased)
{
    unsigned long long whole = (product->type_count == 0u) ? 0ULL : 1ULL;
    for (unsigned int at = 0u; at < product->type_count; at += 1u)
    {
        whole *= cased->type[product->types[at]].count;
    }
    return (!product->mixed && (whole != 0ULL) && (product->rows.count == whole)) ? 1 : 0;
}

// The bytes of a concept's identity, sixteen hexadecimal digits and the terminator
#define CONCEPT_IDENTITY_BYTES 17u

// A concept's identity into `identity`: a whole product's rows written in the order of their values and hashed to
// sixteen hexadecimal digits. 1, or 0 and an empty identity where the product is not whole: a part of a product is not
// yet any one concept
static inline int concept_identity(const ConceptProduct *product, const ConceptCased *cased, char *identity)
{
    identity[0] = '\0';
    if (!concept_whole(product, cased))
    {
        return 0;
    }
    unsigned long long hashed = 0xcbf29ce484222325ULL;
    for (unsigned long long at = 0ULL; at < product->rows.count; at += 1ULL)
    {
        const ConceptRow *const row = concept_row_at(product, at);
        for (const char *character = row->key; *character != '\0'; character += 1)
        {
            hashed = (hashed ^ (unsigned char)*character) * 0x100000001b3ULL;
        }
        const char mark = (row->value == (CONCEPT_ALIKE | CONCEPT_APART)) ? '?'
                          : (row->value == CONCEPT_ALIKE)                 ? '='
                                                                          : 'x';
        hashed = (hashed ^ (unsigned char)mark) * 0x100000001b3ULL;
        hashed = (hashed ^ (unsigned char)';') * 0x100000001b3ULL;
    }
    VerbaLinea line = mmgr_line_over(identity, CONCEPT_IDENTITY_BYTES);
    mmgr_line_hex(&line, hashed, 16u);
    (void)mmgr_line_finish(&line);
    return 1;
}

#endif
