// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// series_rule.h: series in X held by the rule of their entries at a general order k, every expression in one normal form
#ifndef SERIES_RULE_H
#define SERIES_RULE_H

// A series in X is the sum over k of its entries f_k X^k, each entry a function of eta, and an entry at a negative order
// is 0. An expression is the entry at a general order k of a series built from the fields' series, held as a sum of
// parts:
//     a factor times one entry at k + s, or
//     a Cauchy product, the sum over every whole i of a factor times one entry at i and one at k + c - i.
// A factor is a polynomial in eta, h, k and i with exact rational coefficients over a power of L = 1 - 2 h eta^2, and
// an entry carries the order of its derivative in eta. In a Cauchy product the pair of entries is held in one order,
// and where both entries are the same the factor is held even under i -> k + c - i. Parts with the same entries are
// added over the higher power of L. An expression is 0 exactly when every factor's polynomial is 0: a polynomial in
// k that is 0 at every order past a bound, or in i and k at every point of a sum, is the polynomial 0.
// Every operand of a product is an expression the operations build from entries at k, 0 at every negative order.

#include "sim_rational.h"

#include <array>
#include <map>
#include <string>
#include <vector>

// the variables of a factor, in the order its powers are held
typedef enum
{
    SERIES_RULE_ETA = 0,
    SERIES_RULE_ANISOTROPY = 1,
    SERIES_RULE_K = 2,
    SERIES_RULE_I = 3,
    SERIES_RULE_VARIABLES = 4
} SeriesRuleVariable;

typedef std::array<unsigned int, SERIES_RULE_VARIABLES> SeriesRulePower;

// a polynomial in eta, h, k and i, every coefficient not 0
typedef std::map<SeriesRulePower, SimRational> SeriesRulePolynomial;

// a polynomial over L^power
typedef struct
{
    SeriesRulePolynomial numerator;
    unsigned int power;
} SeriesRuleFactor;

// a field's entry: at k + shift in a part of one entry, at i in the first of a Cauchy product and at k + shift - i in
// the second, its derivative in eta of order `slope`
typedef struct
{
    unsigned int field;
    int shift;
    unsigned int slope;
} SeriesRuleEntry;

typedef struct
{
    int product;
    SeriesRuleEntry first;
    SeriesRuleEntry second;
} SeriesRuleKey;

struct SeriesRuleKeyOrder
{
    bool operator()(const SeriesRuleKey &left, const SeriesRuleKey &right) const;
};

typedef std::map<SeriesRuleKey, SeriesRuleFactor, SeriesRuleKeyOrder> SeriesRule;

// the polynomial c, the variable alone, and their sum, difference and product
SeriesRulePolynomial series_rule_constant(SimRational value);
SeriesRulePolynomial series_rule_variable(SeriesRuleVariable variable);
SeriesRulePolynomial series_rule_polynomial_sum(const SeriesRulePolynomial &left, const SeriesRulePolynomial &right);
SeriesRulePolynomial series_rule_polynomial_difference(const SeriesRulePolynomial &left, const SeriesRulePolynomial &right);
SeriesRulePolynomial series_rule_polynomial_product(const SeriesRulePolynomial &left, const SeriesRulePolynomial &right);

// the field's entry at k, and the part `factor` L^-power times the entry at k + shift
SeriesRule series_rule_entry(unsigned int field);
SeriesRule series_rule_part(const SeriesRulePolynomial &factor, unsigned int power, unsigned int field, int shift,
                            unsigned int slope);

// the Cauchy product sum_i factor L^-power (first at i) (second at k + shift - i)
SeriesRule series_rule_cauchy(const SeriesRulePolynomial &factor, unsigned int power, unsigned int first,
                              unsigned int first_slope, unsigned int second, unsigned int second_slope, int shift);

SeriesRule series_rule_sum(const SeriesRule &left, const SeriesRule &right);
SeriesRule series_rule_difference(const SeriesRule &left, const SeriesRule &right);

// every factor times `factor` L^-power, the factor a polynomial in eta, h and k
SeriesRule series_rule_scaled(const SeriesRule &rule, const SeriesRulePolynomial &factor, unsigned int power);

// k -> k + shift
SeriesRule series_rule_shift(const SeriesRule &rule, int shift);

// the entries at k of X f, f_X and X f_X
SeriesRule series_rule_times_x(const SeriesRule &rule);
SeriesRule series_rule_slope_x(const SeriesRule &rule);
SeriesRule series_rule_euler(const SeriesRule &rule);

// the entry at k of the product of two series, each an expression of single entries
SeriesRule series_rule_product(const SeriesRule &left, const SeriesRule &right);

// d/deta of every entry and factor
SeriesRule series_rule_slope_eta(const SeriesRule &rule);

// 1 where every factor is 0
int series_rule_zero(const SeriesRule &rule);

// the parity in eta of every part, the fields' parities given, each 1 for even and -1 for odd: 1 where every part is
// even, -1 where every part is odd, 0 where the parts differ or the rule is 0
int series_rule_parity(const SeriesRule &rule, const std::vector<int> &parities);

// the expression as text, each field by its name
std::string series_rule_text(const SeriesRule &rule, const std::vector<std::string> &names);

// the parts the expression holds
size_t series_rule_parts(const SeriesRule &rule);

// 1 where a value in this module outgrew the build's width
int series_rule_short(void);

#endif
