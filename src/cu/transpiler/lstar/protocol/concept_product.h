// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// concept_product.h: a pair's product, what its two forms make of what they read, held put by put and named once it is
// whole (P13, the concepts)
//
// A put's product is read at the form: the values the form reads, each as the part holds it just before the form, and
// the value each of the two forms writes, each as the part holds it just after. A case is alike where the two forms
// write one value and apart where they write two, and the product is what came back at each combination of the values
// read. The links of a carrier before the form and after it are deltas of their own, and enter no product. A product
// is a concept once it is whole, a row at every combination of the values the cases are made of: two pairs of one
// product are one concept, whatever forms name them and whatever carriers held them. klq_identity puts a pair until
// its product is whole, and klq_decoder writes the concept beneath the pair
#ifndef CONCEPT_PRODUCT_H
#define CONCEPT_PRODUCT_H

#include <stdio.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// the kinds of value a form reads: a word, a wide, a register and its `.hi` read as one, and a flag
enum ConceptValueKind
{
    CONCEPT_VALUE_WORD,
    CONCEPT_VALUE_WIDE,
    CONCEPT_VALUE_FLAG
};

// each value a form reads, as the form writes it, and its value on every case
typedef std::vector<std::pair<std::string, std::vector<unsigned long long>>> ConceptReads;

// a pair's product: the kind of each value its form reads, and what came back at each combination of them
struct ConceptProduct
{
    std::vector<int> kinds;
    std::map<std::string, std::set<char>> rows;
    // 1 where two puts read the form as different values, and no product is held
    int mixed = 0;
};

// The values the cases are made of, by kind, from every operand of every case: each operand is a wide, its low and
// its high word a word, and a flag holds or does not
static inline std::vector<std::set<unsigned long long>> concept_values(const std::vector<unsigned long long> &operands)
{
    std::vector<std::set<unsigned long long>> cased(3u);
    for (const unsigned long long value : operands)
    {
        cased[CONCEPT_VALUE_WIDE].insert(value);
        cased[CONCEPT_VALUE_WORD].insert(value & 0xffffffffull);
        cased[CONCEPT_VALUE_WORD].insert(value >> 32u);
    }
    cased[CONCEPT_VALUE_FLAG] = {0ull, 1ull};
    return cased;
}

// The values the cases are made of, `cased`, with the values `qualifiers` the pair's forms write of their own, each a
// word and a wide: a value a form brings decides where the two forms agree, and the pair's product holds a row at it
static inline std::vector<std::set<unsigned long long>> concept_values_qualified(
    const std::vector<std::set<unsigned long long>> &cased, const std::set<unsigned long long> &qualifiers)
{
    std::vector<std::set<unsigned long long>> qualified = cased;
    for (const unsigned long long value : qualifiers)
    {
        qualified[CONCEPT_VALUE_WORD].insert(value & 0xffffffffull);
        qualified[CONCEPT_VALUE_WIDE].insert(value);
    }
    return qualified;
}

// 1 where `product` is a qualifier's: apart on some row and alike on some, and alike only on rows where a value the
// form reads is one of `qualifiers`, the values its forms write of their own
static inline int concept_qualified(const ConceptProduct &product, const std::set<unsigned long long> &qualifiers)
{
    int alike = 0;
    int apart = 0;
    int qualified = 1;
    for (const auto &held : product.rows)
    {
        const int row_alike = (held.second.count('=') != 0u) ? 1 : 0;
        alike |= row_alike;
        apart |= (held.second.count('x') != 0u) ? 1 : 0;
        int holds_qualifier = 0;
        size_t start = 0u;
        for (size_t comma = held.first.find(','); comma != std::string::npos; comma = held.first.find(',', start))
        {
            const unsigned long long value = std::stoull(held.first.substr(start, comma - start), nullptr, 16);
            holds_qualifier |= (qualifiers.count(value) != 0u) ? 1 : 0;
            start = comma + 1u;
        }
        qualified &= (!row_alike || holds_qualifier) ? 1 : 0;
    }
    return (alike && apart && qualified) ? 1 : 0;
}

// A put's product, held into `product`: what came back of each case, `writes_from` and `writes_to` one value or two,
// as a function of the values `reads` the form reads there, a register and its `.hi` one wide value, in the order the
// form names them, each combination alike, apart, or both where the cases holding it came back both. A combination is
// held where every value is one the cases are made of, `cased`, so that every carrier holds the same rows
static inline void concept_product_held(const ConceptReads &reads, const std::vector<unsigned long long> &writes_from,
                                        const std::vector<unsigned long long> &writes_to,
                                        const std::vector<std::set<unsigned long long>> &cased,
                                        ConceptProduct *product)
{
    // each value the form reads, by the register its pair's low word goes by, and the reads of its low and high word
    std::vector<std::string> bases;
    std::vector<int> low_at;
    std::vector<int> high_at;
    for (size_t at = 0u; at < reads.size(); at += 1u)
    {
        const std::string &read = reads[at].first;
        const int high = (read.size() > 3u) && (read.compare(read.size() - 3u, 3u, ".hi") == 0);
        const std::string base = high ? read.substr(0u, read.size() - 3u) : read;
        const size_t found = (size_t)(std::find(bases.begin(), bases.end(), base) - bases.begin());
        if (found == bases.size())
        {
            bases.push_back(base);
            low_at.push_back(-1);
            high_at.push_back(-1);
        }
        (high ? high_at : low_at)[found] = (int)at;
    }
    std::vector<int> kinds;
    for (size_t value = 0u; value < bases.size(); value += 1u)
    {
        kinds.push_back((bases[value][0] == 'P')                               ? CONCEPT_VALUE_FLAG
                        : (bases[value][0] == '[')                             ? CONCEPT_VALUE_WIDE
                        : ((low_at[value] >= 0) && (high_at[value] >= 0)) ? CONCEPT_VALUE_WIDE
                                                                               : CONCEPT_VALUE_WORD);
    }
    if (kinds.empty())
    {
        return;
    }
    if (product->rows.empty() && product->kinds.empty())
    {
        product->kinds = kinds;
    }
    if (product->kinds != kinds)
    {
        product->mixed = 1;
        return;
    }
    const size_t places = std::min(writes_from.size(), writes_to.size());
    for (size_t place = 0u; place < places; place += 1u)
    {
        int every_read = 1;
        for (const auto &each : reads)
        {
            every_read &= (place < each.second.size()) ? 1 : 0;
        }
        if (!every_read)
        {
            continue;
        }
        const char came_back = (writes_from[place] == writes_to[place]) ? '=' : 'x';
        std::string values;
        int held = 1;
        for (size_t value = 0u; value < bases.size(); value += 1u)
        {
            const unsigned long long low = (low_at[value] >= 0) ? reads[(size_t)low_at[value]].second[place] : 0ull;
            const unsigned long long high = (high_at[value] >= 0) ? reads[(size_t)high_at[value]].second[place] : 0ull;
            const unsigned long long read = ((low_at[value] >= 0) && (high_at[value] >= 0))
                                                ? ((high << 32u) | (low & 0xffffffffull))
                                            : (low_at[value] >= 0) ? low
                                                                   : high;
            held &= (cased[(size_t)kinds[value]].count(read) != 0u) ? 1 : 0;
            char written[24];
            snprintf(written, sizeof(written), "%llx,", read);
            values += written;
        }
        if (held)
        {
            product->rows[values].insert(came_back);
        }
    }
}

// 1 where `product` is whole: a row at every combination of the values the cases are made of, `cased`
static inline int concept_whole(const ConceptProduct &product, const std::vector<std::set<unsigned long long>> &cased)
{
    size_t whole = product.kinds.empty() ? 0u : 1u;
    for (const int kind : product.kinds)
    {
        whole *= cased[(size_t)kind].size();
    }
    return (!product.mixed && (whole != 0u) && (product.rows.size() == whole)) ? 1 : 0;
}

// A concept's identity: a whole product's rows written in the order of their values and hashed to sixteen hexadecimal
// digits, and empty where the product is not whole: a part of a product is not yet any one concept
static inline std::string concept_identity(const ConceptProduct &product,
                                           const std::vector<std::set<unsigned long long>> &cased)
{
    if (!concept_whole(product, cased))
    {
        return std::string();
    }
    std::string written;
    for (const auto &held : product.rows)
    {
        written += held.first + ((held.second.size() > 1u) ? "?" : std::string(1u, *held.second.begin())) + ";";
    }
    unsigned long long hashed = 0xcbf29ce484222325ull;
    for (const char letter : written)
    {
        hashed = (hashed ^ (unsigned char)letter) * 0x100000001b3ull;
    }
    char identity[24];
    snprintf(identity, sizeof(identity), "%016llx", hashed);
    return identity;
}

#endif
