// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// term_form.cu: forms in terms (term_form.h)
#include "term_form.h"

#include "scriptura.h"

#include <algorithm>

bool TermKeyOrder::operator()(const TermKey &left, const TermKey &right) const
{
    const int lean = sim_rational_sign(sim_rational_difference(left.e, right.e));
    if (lean != 0)
    {
        return lean < 0;
    }
    return left.power < right.power;
}

// the slots of the program: the key at each, the slot of each key, and the slot of each product of two
static std::vector<TermKey> s_term_slots;
static std::map<TermKey, unsigned int, TermKeyOrder> s_term_slot_of;
static std::map<std::pair<unsigned int, unsigned int>, unsigned int> s_term_slot_product;

AnchorExactInteger term_form_unit(void)
{
    AnchorExactInteger one;
    sim_exact_unsigned(&one, 1ull);
    return one;
}

static AnchorExactInteger term_form_nothing(void)
{
    AnchorExactInteger zero;
    sim_exact_unsigned(&zero, 0ull);
    return zero;
}

static void term_form_check(int ok)
{
    if (!ok)
    {
        g_sim_rational_wide = 1;
    }
}

// the power with trailing zeros dropped
static TermPower term_form_trimmed(TermPower power)
{
    while (!power.empty() && (power.back() == 0u))
    {
        power.pop_back();
    }
    return power;
}

static TermKey term_form_key(SimRational e, const TermPower &power)
{
    TermKey key;
    key.e = e;
    key.power = term_form_trimmed(power);
    return key;
}

static unsigned int term_form_slot(const TermKey &key)
{
    const auto found = s_term_slot_of.find(key);
    if (found != s_term_slot_of.end())
    {
        return found->second;
    }
    const unsigned int slot = (unsigned int)s_term_slots.size();
    s_term_slots.push_back(key);
    s_term_slot_of.insert(std::make_pair(key, slot));
    return slot;
}

static unsigned int term_form_slot_product(unsigned int left, unsigned int right)
{
    const std::pair<unsigned int, unsigned int> pair = (left < right) ? std::make_pair(left, right)
                                                                      : std::make_pair(right, left);
    const auto found = s_term_slot_product.find(pair);
    if (found != s_term_slot_product.end())
    {
        return found->second;
    }
    const TermPower &left_power = s_term_slots[left].power;
    const TermPower &right_power = s_term_slots[right].power;
    const size_t size = (left_power.size() > right_power.size()) ? left_power.size() : right_power.size();
    TermPower power(size, 0u);
    for (size_t index = 0u; index < size; index += 1u)
    {
        power[index] = ((index < left_power.size()) ? left_power[index] : 0u) +
                       ((index < right_power.size()) ? right_power[index] : 0u);
    }
    const unsigned int slot =
        term_form_slot(term_form_key(sim_rational_sum(s_term_slots[left].e, s_term_slots[right].e), power));
    s_term_slot_product.insert(std::make_pair(pair, slot));
    return slot;
}

// the entry at `slot`, the array grown to hold it
static AnchorExactInteger *term_form_entry(TermForm *form, unsigned int slot)
{
    if (form->magnitude.size() <= slot)
    {
        form->magnitude.resize((size_t)slot + 1u, term_form_nothing());
    }
    return &form->magnitude[slot];
}

// trailing zeros dropped, and the entries and the denominator divided by the factor they share
static void term_form_settle(TermForm *form)
{
    while (!form->magnitude.empty() && (form->magnitude.back().sign == 0))
    {
        form->magnitude.pop_back();
    }
    const AnchorExactInteger one = term_form_unit();
    if (form->magnitude.empty())
    {
        form->denominator = one;
        return;
    }
    AnchorExactInteger common = form->denominator;
    for (const AnchorExactInteger &entry : form->magnitude)
    {
        if (entry.sign == 0)
        {
            continue;
        }
        term_form_check(anchor_exact_gcd(&common, &entry, &common) == ANCHOR_EXACT_OK);
        if (anchor_exact_compare(&common, &one) == 0)
        {
            return;
        }
    }
    for (AnchorExactInteger &entry : form->magnitude)
    {
        if (entry.sign != 0)
        {
            AnchorExactInteger quotient;
            term_form_check(anchor_exact_divide_exact(&entry, &common, &quotient) == ANCHOR_EXACT_OK);
            entry = quotient;
        }
    }
    AnchorExactInteger quotient;
    term_form_check(anchor_exact_divide_exact(&form->denominator, &common, &quotient) == ANCHOR_EXACT_OK);
    form->denominator = quotient;
}

// the term coefficient times the key
static TermForm term_form_term(const TermKey &key, SimRational coefficient)
{
    TermForm form;
    if (sim_rational_sign(coefficient) == 0)
    {
        return form;
    }
    *term_form_entry(&form, term_form_slot(key)) = coefficient.numerator;
    form.denominator = coefficient.denominator;
    return form;
}

// the coefficient at `slot` as a rational in lowest terms
static SimRational term_form_coefficient(const TermForm &form, size_t slot)
{
    SimRational value;
    value.numerator = form.magnitude[slot];
    value.denominator = form.denominator;
    sim_rational_settle(&value);
    return value;
}

// the slots holding a term, in key order
static std::vector<unsigned int> term_form_held(const TermForm &form)
{
    std::vector<unsigned int> held;
    for (size_t slot = 0u; slot < form.magnitude.size(); slot += 1u)
    {
        if (form.magnitude[slot].sign != 0)
        {
            held.push_back((unsigned int)slot);
        }
    }
    const TermKeyOrder order;
    std::sort(held.begin(), held.end(),
              [&order](unsigned int left, unsigned int right) { return order(s_term_slots[left], s_term_slots[right]); });
    return held;
}

TermForm term_form_rational(SimRational value)
{
    return term_form_term(term_form_key(sim_rational(0ll, 1ll), TermPower()), value);
}

TermForm term_form_term(unsigned int term)
{
    TermPower power(term + 1u, 0u);
    power[term] = 1u;
    return term_form_term(term_form_key(sim_rational(0ll, 1ll), power), sim_rational(1ll, 1ll));
}

TermForm term_form_e(SimRational power)
{
    return term_form_term(term_form_key(power, TermPower()), sim_rational(1ll, 1ll));
}

TermForm term_form_sum(const TermForm &left, const TermForm &right)
{
    if (left.magnitude.empty())
    {
        return right;
    }
    if (right.magnitude.empty())
    {
        return left;
    }
    TermForm sum;
    AnchorExactInteger left_factor = term_form_unit();
    AnchorExactInteger right_factor = term_form_unit();
    if (anchor_exact_compare(&left.denominator, &right.denominator) == 0)
    {
        sum.denominator = left.denominator;
    }
    else
    {
        // the least common denominator, each side lifted to it
        AnchorExactInteger common;
        term_form_check(anchor_exact_gcd(&left.denominator, &right.denominator, &common) == ANCHOR_EXACT_OK);
        term_form_check(anchor_exact_divide_exact(&right.denominator, &common, &left_factor) == ANCHOR_EXACT_OK);
        term_form_check(anchor_exact_divide_exact(&left.denominator, &common, &right_factor) == ANCHOR_EXACT_OK);
        term_form_check(sim_exact_product(&left.denominator, &left_factor, &sum.denominator));
    }
    const size_t size = (left.magnitude.size() > right.magnitude.size()) ? left.magnitude.size() : right.magnitude.size();
    sum.magnitude.assign(size, term_form_nothing());
    for (size_t slot = 0u; slot < size; slot += 1u)
    {
        AnchorExactInteger first = term_form_nothing();
        AnchorExactInteger second = term_form_nothing();
        if ((slot < left.magnitude.size()) && (left.magnitude[slot].sign != 0))
        {
            term_form_check(sim_exact_product(&left.magnitude[slot], &left_factor, &first));
        }
        if ((slot < right.magnitude.size()) && (right.magnitude[slot].sign != 0))
        {
            term_form_check(sim_exact_product(&right.magnitude[slot], &right_factor, &second));
        }
        term_form_check(sim_exact_sum(&first, &second, &sum.magnitude[slot]));
    }
    term_form_settle(&sum);
    return sum;
}

TermForm term_form_difference(const TermForm &left, const TermForm &right)
{
    TermForm negative = right;
    for (AnchorExactInteger &entry : negative.magnitude)
    {
        entry.sign = -entry.sign;
    }
    return term_form_sum(left, negative);
}

TermForm term_form_scaled(const TermForm &form, SimRational factor)
{
    TermForm scaled;
    if ((sim_rational_sign(factor) == 0) || form.magnitude.empty())
    {
        return scaled;
    }
    scaled.magnitude.assign(form.magnitude.size(), term_form_nothing());
    for (size_t slot = 0u; slot < form.magnitude.size(); slot += 1u)
    {
        if (form.magnitude[slot].sign != 0)
        {
            term_form_check(sim_exact_product(&form.magnitude[slot], &factor.numerator, &scaled.magnitude[slot]));
        }
    }
    term_form_check(sim_exact_product(&form.denominator, &factor.denominator, &scaled.denominator));
    term_form_settle(&scaled);
    return scaled;
}

TermForm term_form_product(const TermForm &left, const TermForm &right)
{
    TermForm product;
    if (left.magnitude.empty() || right.magnitude.empty())
    {
        return product;
    }
    for (size_t first = 0u; first < left.magnitude.size(); first += 1u)
    {
        if (left.magnitude[first].sign == 0)
        {
            continue;
        }
        for (size_t second = 0u; second < right.magnitude.size(); second += 1u)
        {
            if (right.magnitude[second].sign == 0)
            {
                continue;
            }
            const unsigned int slot = term_form_slot_product((unsigned int)first, (unsigned int)second);
            AnchorExactInteger part;
            term_form_check(sim_exact_product(&left.magnitude[first], &right.magnitude[second], &part));
            AnchorExactInteger *const entry = term_form_entry(&product, slot);
            AnchorExactInteger total;
            term_form_check(sim_exact_sum(entry, &part, &total));
            *entry = total;
        }
    }
    term_form_check(sim_exact_product(&left.denominator, &right.denominator, &product.denominator));
    term_form_settle(&product);
    return product;
}

// 1 where e's power is ratio times a whole number 1 or more
static int term_form_multiple(SimRational e, SimRational ratio)
{
    if ((sim_rational_sign(e) == 0) || (sim_rational_sign(ratio) == 0))
    {
        return 0;
    }
    const SimRational times = sim_rational_product(e, sim_rational_reciprocal(ratio));
    long long denominator = 0ll;
    return (sim_rational_sign(times) > 0) && sim_rational_small(&times.denominator, &denominator) &&
           (denominator == 1ll);
}

TermForm term_form_unit_reduced(const TermForm &form, SimRational ratio, unsigned int share)
{
    TermForm reduced = form;
    for (;;)
    {
        size_t mixed = reduced.magnitude.size();
        for (size_t slot = 0u; slot < reduced.magnitude.size(); slot += 1u)
        {
            const TermPower &power = s_term_slots[slot].power;
            if ((reduced.magnitude[slot].sign != 0) && (power.size() > share) && (power[share] > 0u) &&
                term_form_multiple(s_term_slots[slot].e, ratio))
            {
                mixed = slot;
                break;
            }
        }
        if (mixed == reduced.magnitude.size())
        {
            term_form_settle(&reduced);
            return reduced;
        }
        // e^ratio share -> 1 - share, over the same denominator
        const AnchorExactInteger value = reduced.magnitude[mixed];
        const SimRational lowered_e = sim_rational_difference(s_term_slots[mixed].e, ratio);
        TermPower lowered = s_term_slots[mixed].power;
        reduced.magnitude[mixed] = term_form_nothing();
        lowered[share] -= 1u;
        TermPower kept = lowered;
        kept[share] += 1u;
        const unsigned int lowered_slot = term_form_slot(term_form_key(lowered_e, lowered));
        const unsigned int kept_slot = term_form_slot(term_form_key(lowered_e, kept));
        AnchorExactInteger total;
        term_form_check(sim_exact_sum(term_form_entry(&reduced, lowered_slot), &value, &total));
        *term_form_entry(&reduced, lowered_slot) = total;
        term_form_check(sim_exact_less(term_form_entry(&reduced, kept_slot), &value, &total));
        *term_form_entry(&reduced, kept_slot) = total;
    }
}

TermForm term_form_square_reduced(const TermForm &form, unsigned int root, SimRational square)
{
    TermForm reduced;
    for (size_t slot = 0u; slot < form.magnitude.size(); slot += 1u)
    {
        if (form.magnitude[slot].sign == 0)
        {
            continue;
        }
        TermPower power = s_term_slots[slot].power;
        SimRational value = term_form_coefficient(form, slot);
        if (power.size() > root)
        {
            while (power[root] >= 2u)
            {
                power[root] -= 2u;
                value = sim_rational_product(value, square);
            }
        }
        reduced = term_form_sum(reduced, term_form_term(term_form_key(s_term_slots[slot].e, power), value));
    }
    return reduced;
}

unsigned int term_book_id(TermBook *book, const std::string &name)
{
    for (size_t index = 0u; index < book->names.size(); index += 1u)
    {
        if (book->names[index] == name)
        {
            return (unsigned int)index;
        }
    }
    book->names.push_back(name);
    return (unsigned int)(book->names.size() - 1u);
}

// every limb in hex from the highest held; two values share a text only where they are equal
static std::string term_book_limbs(const AnchorExactInteger &value)
{
    static const char digits[] = "0123456789abcdef";
    std::string text = (value.sign < 0) ? "-0x" : "0x";
    const unsigned long long bits = sim_exact_bits(&value);
    const unsigned int used = (unsigned int)((bits + 31ull) / 32ull);
    if (used == 0u)
    {
        return "0";
    }
    for (unsigned int limb = used; limb > 0u; limb -= 1u)
    {
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            text.push_back(digits[(value.limb[limb - 1u] >> shift) & 0xFu]);
        }
    }
    return text;
}

std::string term_book_rational(SimRational value)
{
    long long numerator = 0ll;
    long long denominator = 0ll;
    if (!sim_rational_small(&value.numerator, &numerator) || !sim_rational_small(&value.denominator, &denominator))
    {
        return "(" + term_book_limbs(value.numerator) + "/" + term_book_limbs(value.denominator) + ")";
    }
    return (denominator == 1ll) ? std::to_string(numerator)
                                : (std::to_string(numerator) + "/" + std::to_string(denominator));
}

int term_form_zero(const TermForm &form)
{
    return form.magnitude.empty();
}

SimRational term_form_constant(const TermForm &form)
{
    const auto found = s_term_slot_of.find(term_form_key(sim_rational(0ll, 1ll), TermPower()));
    if ((found == s_term_slot_of.end()) || (found->second >= form.magnitude.size()) ||
        (form.magnitude[found->second].sign == 0))
    {
        return sim_rational(0ll, 1ll);
    }
    return term_form_coefficient(form, found->second);
}

SimRational term_form_largest(const TermForm &form)
{
    SimRational largest = sim_rational(0ll, 1ll);
    for (size_t slot = 0u; slot < form.magnitude.size(); slot += 1u)
    {
        if (form.magnitude[slot].sign == 0)
        {
            continue;
        }
        const SimRational size = sim_rational_absolute(term_form_coefficient(form, slot));
        if (sim_rational_sign(sim_rational_difference(size, largest)) > 0)
        {
            largest = size;
        }
    }
    return largest;
}

SimRational term_form_coefficient_of(const TermForm &form, unsigned int term)
{
    TermPower power(term + 1u, 0u);
    power[term] = 1u;
    const auto found = s_term_slot_of.find(term_form_key(sim_rational(0ll, 1ll), power));
    if ((found == s_term_slot_of.end()) || !term_form_holds(form, found->second))
    {
        return sim_rational(0ll, 1ll);
    }
    return term_form_coefficient(form, found->second);
}

size_t term_form_e_count(const TermForm &form)
{
    size_t count = 0u;
    const TermKey *last = NULL;
    for (unsigned int slot : term_form_held(form))
    {
        if ((last == NULL) || !sim_rational_equal(last->e, s_term_slots[slot].e))
        {
            count += 1u;
        }
        last = &s_term_slots[slot];
    }
    return count;
}

// one exact integer in decimal, its sign first
static void term_form_write_integer(FILE *file, const AnchorExactInteger &value, std::vector<char> *buffer)
{
    ScripturaLine line;
    line.out = buffer->data();
    line.capacity = buffer->size();
    line.at = 0ull;
    if (value.sign < 0)
    {
        scriptura_character(&line, '-');
    }
    sim_exact_decimal(&line, &value);
    if (line.at >= line.capacity)
    {
        g_sim_rational_wide = 1;
    }
    fwrite(line.out, 1u, (size_t)line.at, file);
}

void term_form_write(FILE *file, const TermForm &form, const std::vector<std::string> &names)
{
    // a magnitude of the build's width in decimal, groups of nine, with room for its sign
    std::vector<char> buffer((size_t)(SIM_DECIMAL_GROUPS * SIM_DECIMAL_GROUP_DIGITS + 8ull));
    for (unsigned int slot : term_form_held(form))
    {
        const TermKey &key = s_term_slots[slot];
        const SimRational coefficient = term_form_coefficient(form, slot);
        term_form_write_integer(file, coefficient.numerator, &buffer);
        fputc('/', file);
        term_form_write_integer(file, coefficient.denominator, &buffer);
        if (sim_rational_sign(key.e) != 0)
        {
            fprintf(file, " e^(%s)", term_book_rational(key.e).c_str());
        }
        for (size_t term = 0u; term < key.power.size(); term += 1u)
        {
            if (key.power[term] == 0u)
            {
                continue;
            }
            fprintf(file, " %s", (term < names.size()) ? names[term].c_str() : "term");
            if (key.power[term] > 1u)
            {
                fprintf(file, "^%u", key.power[term]);
            }
        }
        fputc('\n', file);
    }
}

size_t term_form_terms(const TermForm &form)
{
    size_t count = 0u;
    for (const AnchorExactInteger &entry : form.magnitude)
    {
        count += (entry.sign != 0) ? 1u : 0u;
    }
    return count;
}

std::vector<unsigned int> term_form_slots(const TermForm &form)
{
    return term_form_held(form);
}

int term_form_holds(const TermForm &form, unsigned int slot)
{
    return (slot < form.magnitude.size()) && (form.magnitude[slot].sign != 0);
}

const TermKey &term_form_key_at(unsigned int slot)
{
    return s_term_slots[slot];
}

SimRational term_form_coefficient_at(const TermForm &form, unsigned int slot)
{
    if (!term_form_holds(form, slot))
    {
        return sim_rational(0ll, 1ll);
    }
    return term_form_coefficient(form, slot);
}

std::string term_form_key_text(unsigned int slot, const std::vector<std::string> &names)
{
    const TermKey &key = s_term_slots[slot];
    std::string text;
    if (sim_rational_sign(key.e) != 0)
    {
        text = "e^(" + term_book_rational(key.e) + ")";
    }
    for (size_t term = 0u; term < key.power.size(); term += 1u)
    {
        if (key.power[term] == 0u)
        {
            continue;
        }
        text += text.empty() ? "" : " ";
        text += (term < names.size()) ? names[term] : "term";
        if (key.power[term] > 1u)
        {
            text += "^" + std::to_string(key.power[term]);
        }
    }
    return text.empty() ? "1" : text;
}

size_t term_form_entries(const TermForm &form)
{
    return form.magnitude.size();
}

unsigned long long term_form_bits(const TermForm &form)
{
    unsigned long long most = sim_exact_bits(&form.denominator);
    for (const AnchorExactInteger &entry : form.magnitude)
    {
        const unsigned long long bits = sim_exact_bits(&entry);
        most = (bits > most) ? bits : most;
    }
    return most;
}

int term_form_short(void)
{
    return g_sim_rational_wide != 0;
}
