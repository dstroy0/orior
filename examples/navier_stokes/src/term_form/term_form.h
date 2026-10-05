// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// term_form.h: an exact polynomial in terms with rational coefficients, e held apart at a rational power
#ifndef TERM_FORM_H
#define TERM_FORM_H

// An term is a number held apart and never evaluated: w(z_c) and w'(z_c) for the Kummer solution, z_c^h, E1(x). e is
// held apart from every term: each term carries its own rational power of e, and e^p e^q is e^(p+q) exactly;
// e^(-1/s_c), e^N and e^(-n/b) are one number at three powers. A form is a sum of rational coefficients times e^q times
// products of terms. The exponent of term i in a term is entry i of its power, trailing zeros dropped; one product has
// one key.

#include "sim_rational.h"

#include <map>
#include <string>
#include <vector>

typedef std::vector<unsigned int> TermPower;

typedef struct
{
    SimRational e;
    TermPower power;
} TermKey;

// e's power first, by value, then the terms' powers
struct TermKeyOrder
{
    bool operator()(const TermKey &left, const TermKey &right) const;
};

// the exact integer 1
AnchorExactInteger term_form_unit(void);

// A form holds exact integers, entry i the numerator of the term at the program's slot i, over one positive
// denominator. A slot is a key, a power of e and the terms' powers, numbered as the program first meets it; every form
// in a run shares the numbering. The denominator and the entries share no factor, and the last entry is not 0.
struct TermForm
{
    std::vector<AnchorExactInteger> magnitude;
    AnchorExactInteger denominator = term_form_unit();
};

TermForm term_form_rational(SimRational value);

// the term `term` itself
TermForm term_form_term(unsigned int term);

// e^power
TermForm term_form_e(SimRational power);

TermForm term_form_sum(const TermForm &left, const TermForm &right);
TermForm term_form_difference(const TermForm &left, const TermForm &right);
TermForm term_form_scaled(const TermForm &form, SimRational factor);
TermForm term_form_product(const TermForm &left, const TermForm &right);

// the form under the relation share (1 + e^ratio) = 1: every term holding share whose power of e is a whole positive
// multiple of `ratio` has e^ratio share replaced by 1 - share until none does. The powers of e in the form are the
// ratio's own.
TermForm term_form_unit_reduced(const TermForm &form, SimRational ratio, unsigned int share);

// the form with every square of the term `root` replaced by the rational `square`, as for 2^(-1/2)
TermForm term_form_square_reduced(const TermForm &form, unsigned int root, SimRational square);

// The terms of one computation, numbered as they are first named
typedef struct
{
    std::vector<std::string> names;
} TermBook;

// the number of the term named `name`, a new one where the book does not hold it
unsigned int term_book_id(TermBook *book, const std::string &name);

// a rational as text for an term's name: p/q, or both parts' limbs in hex where a part passes a word
std::string term_book_rational(SimRational value);

// 1 where every coefficient is 0
int term_form_zero(const TermForm &form);

// the coefficient of the term with no term and e^0
SimRational term_form_constant(const TermForm &form);

// the largest magnitude among the form's coefficients, 0 for the form 0
SimRational term_form_largest(const TermForm &form);

// the coefficient of the term that is the term `term` alone, e^0
SimRational term_form_coefficient_of(const TermForm &form, unsigned int term);

// the number of distinct powers of e the form holds
size_t term_form_e_count(const TermForm &form);

// the form written whole to `file`, one term per line: numerator/denominator in decimal, e^(q) where q is not 0, and
// each term by its name, ^k where its power passes 1
void term_form_write(FILE *file, const TermForm &form, const std::vector<std::string> &names);

// the number of terms the form holds
size_t term_form_terms(const TermForm &form);

// the slots holding a term, in key order
std::vector<unsigned int> term_form_slots(const TermForm &form);

// 1 where the form holds a term at `slot`
int term_form_holds(const TermForm &form, unsigned int slot);

// the key at `slot`, a slot some form holds
const TermKey &term_form_key_at(unsigned int slot);

// the coefficient at `slot` in lowest terms, 0 where the form holds no term there
SimRational term_form_coefficient_at(const TermForm &form, unsigned int slot);

// the key at `slot` as text: "1", or e^(q) and each term by its name, ^k where its power passes 1
std::string term_form_key_text(unsigned int slot, const std::vector<std::string> &names);

// the entries the form's array holds, its zeros counted
size_t term_form_entries(const TermForm &form);

// the most bits any entry or the denominator takes
unsigned long long term_form_bits(const TermForm &form);

// 1 where a value in this module outgrew the build's width
int term_form_short(void);

#endif
