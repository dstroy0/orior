// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// series_rule.h: series in X stored by the rule of their entries at a general order k, every expression in one normal
// form
#ifndef SERIES_RULE_H
#define SERIES_RULE_H

// A series in X is the sum over k of its entries f_k X^k, each entry a function of eta, and an entry at a negative
// order is 0. An expression is the entry at a general order k of a series built from the fields' series, stored as a
// sum of parts:
//     a factor times one entry at k + s, or
//     a Cauchy product, the sum over every whole i of a factor times one entry at i and one at k + c - i.
// A factor is a polynomial in eta, h, k and i with exact rational coefficients over a power of L = 1 - 2 h eta^2, and
// an entry carries the order of its derivative in eta. In a Cauchy product the pair of entries is stored in one order,
// and where both entries are the same the factor is stored even under i -> k + c - i. Parts with the same entries are
// added over the higher power of L. An expression is 0 exactly when every factor's polynomial is 0: a polynomial in
// k that is 0 at every order past a bound, or in i and k at every point of a sum, is the polynomial 0.
// Every operand of a product is an expression the operations build from entries at k, 0 at every negative order.
//
// A polynomial and an expression are each one cell of the temporary tier: a polynomial its terms sorted by their
// powers, an expression its parts sorted by their keys, each part's polynomial a cell of its own. A call that gives one
// writes it into `out`, which may be one of its operands: it gives back the cells that `out` held, and the caller gives
// back the result with series_rule_polynomial_released or series_rule_released. A call fills its result unsorted at a
// count that its operands bound, sorts it once and adds the terms or the parts of one key in the order they came.

#include "sim_rational.h"
#include "verba_scribo/verba_linea.h"

// the variables of a factor, in the order its powers are stored
typedef enum
{
    SERIES_RULE_ETA = 0,
    SERIES_RULE_ANISOTROPY = 1,
    SERIES_RULE_K = 2,
    SERIES_RULE_I = 3,
    SERIES_RULE_VARIABLES = 4
} SeriesRuleVariable;

// the powers of a term, one for each variable
typedef struct
{
    unsigned int at[SERIES_RULE_VARIABLES];
} SeriesRulePower;

// a term of a polynomial: its powers and its coefficient, not 0
typedef struct
{
    SeriesRulePower power;
    SimRational value;
} SeriesRuleTerm;

// a polynomial in eta, h, k and i, its terms sorted by their powers, every coefficient not 0
typedef struct
{
    SeriesRuleTerm *term;
    unsigned int count;
    unsigned int room;
} SeriesRulePolynomial;

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

// a part of an expression: its key and its factor
typedef struct
{
    SeriesRuleKey key;
    SeriesRuleFactor factor;
} SeriesRulePart;

// an expression, its parts sorted by their keys
typedef struct
{
    SeriesRulePart *part;
    unsigned int count;
    unsigned int room;
} SeriesRule;

// an empty polynomial and an empty expression, with no cell
#define SERIES_RULE_NONE {NULL, 0u, 0u}

// the cells of a polynomial or an expression given back, and it empty
void series_rule_polynomial_released(SeriesRulePolynomial *polynomial);
void series_rule_released(SeriesRule *rule);

// the polynomial c, the variable alone, and their sum, difference and product
void series_rule_constant(SimRational value, SeriesRulePolynomial *out);
void series_rule_variable(SeriesRuleVariable variable, SeriesRulePolynomial *out);
void series_rule_polynomial_sum(const SeriesRulePolynomial *left, const SeriesRulePolynomial *right,
                                SeriesRulePolynomial *out);
void series_rule_polynomial_difference(const SeriesRulePolynomial *left, const SeriesRulePolynomial *right,
                                       SeriesRulePolynomial *out);
void series_rule_polynomial_product(const SeriesRulePolynomial *left, const SeriesRulePolynomial *right,
                                    SeriesRulePolynomial *out);

// the field's entry at k, and the part `factor` L^-power times the entry at k + shift
void series_rule_entry(unsigned int field, SeriesRule *out);
void series_rule_part(const SeriesRulePolynomial *factor, unsigned int power, unsigned int field, int shift,
                      unsigned int slope, SeriesRule *out);

// the Cauchy product sum_i factor L^-power (first at i) (second at k + shift - i)
void series_rule_cauchy(const SeriesRulePolynomial *factor, unsigned int power, unsigned int first,
                        unsigned int first_slope, unsigned int second, unsigned int second_slope, int shift,
                        SeriesRule *out);

void series_rule_sum(const SeriesRule *left, const SeriesRule *right, SeriesRule *out);
void series_rule_difference(const SeriesRule *left, const SeriesRule *right, SeriesRule *out);

// every factor times `factor` L^-power, the factor a polynomial in eta, h and k
void series_rule_scaled(const SeriesRule *rule, const SeriesRulePolynomial *factor, unsigned int power,
                        SeriesRule *out);

// k -> k + shift
void series_rule_shift(const SeriesRule *rule, int shift, SeriesRule *out);

// the entries at k of X f, f_X and X f_X
void series_rule_times_x(const SeriesRule *rule, SeriesRule *out);
void series_rule_slope_x(const SeriesRule *rule, SeriesRule *out);
void series_rule_euler(const SeriesRule *rule, SeriesRule *out);

// the entry at k of the product of two series, each an expression of single entries
void series_rule_product(const SeriesRule *left, const SeriesRule *right, SeriesRule *out);

// d/deta of every entry and factor
void series_rule_slope_eta(const SeriesRule *rule, SeriesRule *out);

// 1 where every factor is 0
int series_rule_zero(const SeriesRule *rule);

// the parity in eta of every part, the fields' parities given, each 1 for even and -1 for odd: 1 where every part is
// even, -1 where every part is odd, 0 where the parts differ or the rule is 0
int series_rule_parity(const SeriesRule *rule, const int *parities);

// the expression as text after the bytes of `line`, each field by its name in `names`, one part to a line
void series_rule_text(const SeriesRule *rule, const char *const *names, VerbaLinea *line);

// the parts the expression contains
unsigned int series_rule_parts(const SeriesRule *rule);

// 1 where a value in this module outgrew the build's width, or the pool did not meet a cell of an expression
int series_rule_short(void);

#endif
