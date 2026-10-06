// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file mass.c
 * @brief Exact masses over NIST's isotope table, and a measured m/z set against them exactly.
 */
#include "mass.h"

#include "isotope_table.h"


/** Symbol slots: an upper-case letter, then nothing or one lower-case letter. */
#define MASS_SYMBOL_SLOTS (26u * 27u)

/** Not yet looked up, in the monoisotope memo. */
#define MASS_MEMO_UNSET (-2ll)

/** A sum past this is refused, keeping every mass and product in range. */
#define MASS_LIMIT (1ll << 62)

static long long s_monoisotope_memo[MASS_SYMBOL_SLOTS];
static int s_memo_ready = 0;

const CasmiIsotope *casmi_isotope_table(unsigned int *count)
{
    *count = CASMI_ISOTOPE_COUNT;
    return casmi_isotope;
}

static long long mass_symbol_slot(const char *symbol, unsigned long long length)
{
    if (length == 0ull || length > 2ull || symbol[0] < 'A' || symbol[0] > 'Z')
    {
        return ENGINE_BYTES_ERROR;
    }
    const long long upper = (long long)(symbol[0] - 'A');
    if (length == 1ull)
    {
        return upper * 27ll;
    }
    if (symbol[1] < 'a' || symbol[1] > 'z')
    {
        return ENGINE_BYTES_ERROR;
    }
    return (upper * 27ll) + 1ll + (long long)(symbol[1] - 'a');
}

static long long mass_monoisotope_search(const char *symbol, unsigned long long length)
{
    long long chosen = ENGINE_BYTES_ERROR;
    long long chosen_composition = -1ll;
    unsigned int listed = 0u;
    for (unsigned int isotope = 0u; isotope < CASMI_ISOTOPE_COUNT; isotope += 1u)
    {
        const CasmiIsotope *const entry = &casmi_isotope[isotope];
        if ((strlen(entry->symbol) != length) || (memcmp(entry->symbol, symbol, (size_t)length) != 0))
        {
            continue;
        }
        listed += 1u;
        if (entry->composition > chosen_composition)
        {
            chosen = (long long)isotope;
            chosen_composition = entry->composition;
        }
        else if ((chosen_composition < 0ll) && (chosen < 0ll))
        {
            chosen = (long long)isotope;
        }
    }
    // With no abundance anywhere, only a lone isotope names itself; several would be a guess.
    if ((chosen_composition < 0ll) && (listed != 1u))
    {
        return ENGINE_BYTES_ERROR;
    }
    return chosen;
}

long long casmi_isotope_monoisotope(const char *symbol, unsigned long long length)
{
    const long long slot = mass_symbol_slot(symbol, length);
    if (slot < 0ll)
    {
        return ENGINE_BYTES_ERROR;
    }
    if (!s_memo_ready)
    {
        for (unsigned int each = 0u; each < MASS_SYMBOL_SLOTS; each += 1u)
        {
            s_monoisotope_memo[each] = MASS_MEMO_UNSET;
        }
        s_memo_ready = 1;
    }
    if (s_monoisotope_memo[slot] == MASS_MEMO_UNSET)
    {
        s_monoisotope_memo[slot] = mass_monoisotope_search(symbol, length);
    }
    return s_monoisotope_memo[slot];
}

/** Reads one run of decimal digits; an absent run is 1. Refuses past 2^31. */
static int mass_count_read(const char *text, unsigned long long length, unsigned long long *at, long long *count)
{
    long long value = 0ll;
    int seen = 0;
    while ((*at < length) && (text[*at] >= '0') && (text[*at] <= '9'))
    {
        value = (value * 10ll) + (long long)(text[*at] - '0');
        if (value > (1ll << 31))
        {
            return 0;
        }
        seen = 1;
        *at += 1ull;
    }
    *count = seen ? value : 1ll;
    return 1;
}

/** Adds sign x the formula text[start, end) into formula, merging a repeated element. */
static int mass_formula_add(const char *text, unsigned long long start, unsigned long long end, long long sign,
                            CasmiFormula *formula)
{
    unsigned long long at = start;
    if (at == end)
    {
        return 0;
    }
    while (at < end)
    {
        if (text[at] < 'A' || text[at] > 'Z')
        {
            return 0;
        }
        const unsigned long long symbol_start = at;
        at += 1ull;
        if ((at < end) && (text[at] >= 'a') && (text[at] <= 'z'))
        {
            at += 1ull;
        }
        const long long isotope = casmi_isotope_monoisotope(text + symbol_start, at - symbol_start);
        long long count = 0ll;
        if ((isotope < 0ll) || !mass_count_read(text, end, &at, &count))
        {
            return 0;
        }
        unsigned int slot = 0u;
        while ((slot < formula->element_count) && (formula->isotope[slot] != (unsigned int)isotope))
        {
            slot += 1u;
        }
        if (slot == formula->element_count)
        {
            if (slot >= CASMI_FORMULA_ELEMENTS_MOST)
            {
                return 0;
            }
            // isotope is an index below CASMI_ISOTOPE_COUNT; it fits unsigned int.
            formula->isotope[slot] = (unsigned int)isotope;
            formula->count[slot] = 0ll;
            formula->element_count += 1u;
        }
        formula->count[slot] += sign * count;
    }
    return 1;
}

long long casmi_formula_read(const CasmiFormulaRead *args)
{
    memset(args->formula, 0, sizeof(*args->formula));
    if (!mass_formula_add(args->text, 0ull, args->length, 1ll, args->formula))
    {
        memset(args->formula, 0, sizeof(*args->formula));
        return ENGINE_BYTES_ERROR;
    }
    return (long long)args->formula->element_count;
}

long long casmi_formula_mass(const CasmiFormula *formula)
{
    long long total = 0ll;
    for (unsigned int slot = 0u; slot < formula->element_count; slot += 1u)
    {
        const long long each = casmi_isotope[formula->isotope[slot]].mass;
        const long long count = formula->count[slot];
        const long long magnitude = (count < 0ll) ? -count : count;
        if ((magnitude != 0ll) && (each > MASS_LIMIT / magnitude))
        {
            return ENGINE_BYTES_ERROR;
        }
        total += count * each;
        if ((total > MASS_LIMIT) || (total < -MASS_LIMIT))
        {
            return ENGINE_BYTES_ERROR;
        }
    }
    return total;
}

long long casmi_adduct_read(const CasmiAdductRead *args)
{
    const char *const text = args->text;
    const unsigned long long length = args->length;
    memset(args->adduct, 0, sizeof(*args->adduct));
    unsigned long long at = 0ull;
    if ((length < 4ull) || (text[0] != '['))
    {
        return ENGINE_BYTES_ERROR;
    }
    at = 1ull;
    long long molecules = 0ll;
    if (!mass_count_read(text, length, &at, &molecules) || (at >= length) || (text[at] != 'M'))
    {
        return ENGINE_BYTES_ERROR;
    }
    at += 1ull;
    CasmiFormula added;
    memset(&added, 0, sizeof(added));
    while ((at < length) && (text[at] != ']'))
    {
        long long sign = 0ll;
        if (text[at] == '+')
        {
            sign = 1ll;
        }
        else if (text[at] == '-')
        {
            sign = -1ll;
        }
        else
        {
            return ENGINE_BYTES_ERROR;
        }
        at += 1ull;
        long long times = 0ll;
        if (!mass_count_read(text, length, &at, &times))
        {
            return ENGINE_BYTES_ERROR;
        }
        const unsigned long long term_start = at;
        while ((at < length) && (text[at] != '+') && (text[at] != '-') && (text[at] != ']'))
        {
            at += 1ull;
        }
        if (!mass_formula_add(text, term_start, at, sign * times, &added))
        {
            return ENGINE_BYTES_ERROR;
        }
    }
    if (at >= length)
    {
        return ENGINE_BYTES_ERROR;
    }
    at += 1ull;
    long long charge = 0ll;
    if (!mass_count_read(text, length, &at, &charge) || (at + 1ull != length))
    {
        return ENGINE_BYTES_ERROR;
    }
    if (text[at] == '-')
    {
        charge = -charge;
    }
    else if (text[at] != '+')
    {
        return ENGINE_BYTES_ERROR;
    }
    const long long added_mass = casmi_formula_mass(&added);
    if (added_mass == ENGINE_BYTES_ERROR)
    {
        return ENGINE_BYTES_ERROR;
    }
    args->adduct->molecules = molecules;
    args->adduct->added = added_mass;
    args->adduct->charge = charge;
    args->adduct->terms = added;
    return 0ll;
}

long long casmi_ion_numerator(long long neutral_mass, const CasmiAdduct *adduct)
{
    if ((adduct->molecules <= 0ll) || (adduct->charge == 0ll) || (neutral_mass <= 0ll) ||
        (neutral_mass > MASS_LIMIT / adduct->molecules))
    {
        return ENGINE_BYTES_ERROR;
    }
    const long long numerator = (adduct->molecules * neutral_mass) + adduct->added - (adduct->charge * CASMI_ELECTRON_MASS);
    if ((numerator <= 0ll) || (numerator > MASS_LIMIT))
    {
        return ENGINE_BYTES_ERROR;
    }
    return numerator;
}
