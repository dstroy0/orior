// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jet_rule.h: stored functions of X and eta and their derivatives, every expression in one normal form, every relation
// exact
#ifndef JET_RULE_H
#define JET_RULE_H

// An expression is a sum of exact rational coefficients times products of:
//     eta to the power 0 or 1, h to a whole power, L^-1 to a whole power, L = 1 - 2 h eta^2,
//     X^(a + b h), d^(a + b h) and 2^(a + b h), a and b rational, d = 1 - eta^2,
//     constants stored by name, and
//     the derivatives of stored functions: f_(X^a eta^b) for a function f of X and eta, and w^(n) for Kummer's w of
//     z = X / (2 d).
// eta^2 is stored as 1 - d, every whole power of 2 is moved into the coefficient, and each base X, d and 2 is stored
// once in a product with its exponents added. A stored function may carry a relation that gives its derivative in X of
// one order through lower ones; every derivative at or past that order is written through the relation, differentiated
// as often as it takes. w^(n), n >= 2, is written through Kummer's equation z w'' + (2 - z) w' - (1 + h) w = 0. An
// expression is 0 exactly when, multiplied by L to the highest power of L^-1 it contains, every coefficient is 0: L
// L^-1 = 1 and eta^2 = 1 - d are the only relations among the factors, and the powers of X, d and 2 with distinct
// exponents are independent.
//
// An expression is one cell of the temporary tier, its terms sorted by their products. A call that gives one writes it
// into `out`, which may be one of its operands: it gives back the cell that `out` held, and the caller gives back the
// result with jet_rule_released. A call fills its result unsorted at a count that its operands bound, sorts it once and
// adds the terms of one product. A product is a fixed record of at max JET_RULE_FACTORS factors, and a product past it
// is a refusal that jet_rule_short reports.

#include "sim_rational.h"
#include "verba_scribo/verba_linea.h"

// a factor's place and its exponent a + b h, a and b stored as whole numerators over whole denominators
typedef struct
{
    unsigned int place;
    long long a_numerator;
    long long a_denominator;
    long long b_numerator;
    long long b_denominator;
} JetRuleFactor;

// the factors a product holds at max, one to each place
#define JET_RULE_FACTORS 24u

// a product, its factors sorted by their places
typedef struct
{
    JetRuleFactor factor[JET_RULE_FACTORS];
    unsigned int count;
} JetRuleMonomial;

// a term: its product and its coefficient, not 0
typedef struct
{
    JetRuleMonomial monomial;
    SimRational value;
} JetRuleTerm;

// an expression, its terms sorted by their products
typedef struct
{
    JetRuleTerm *term;
    unsigned int count;
    unsigned int room;
} JetRule;

// an empty expression, with no cell
#define JET_RULE_NONE {NULL, 0u, 0u}

// the bases a power may be taken of
typedef enum
{
    JET_RULE_BASE_X = 0,
    JET_RULE_BASE_D = 1,
    JET_RULE_BASE_TWO = 2
} JetRuleBase;

// the cell of an expression given back, and it empty
void jet_rule_released(JetRule *rule);

void jet_rule_number(long long numerator, long long denominator, JetRule *out);
void jet_rule_rational(SimRational value, JetRule *out);
void jet_rule_eta(JetRule *out);
void jet_rule_h(JetRule *out);
void jet_rule_over_l(JetRule *out);

// base^(a + b h)
void jet_rule_power(JetRuleBase base, long long a_numerator, long long a_denominator, long long b_numerator,
                    long long b_denominator, JetRule *out);

// a constant stored by name, its derivatives 0
void jet_rule_constant(const char *name, JetRule *out);

// the stored function `name` of X and eta, a new one where none is stored by that name, and its derivative
// d^a/dX^a d^b/deta^b
unsigned int jet_rule_function(const char *name);
void jet_rule_slope(unsigned int function, unsigned int a, unsigned int b, JetRule *out);

// Kummer's w of z = X / (2 d), its derivative of order n in z
void jet_rule_w(unsigned int n, JetRule *out);

// d^order f / dX^order = `value` for the stored function, `value` containing only derivatives of lower order in X of
// it. The function keeps a copy of `value` on the persistent tier for the run, which reads the relation to its end
void jet_rule_relate(unsigned int function, unsigned int order, const JetRule *value);

void jet_rule_sum(const JetRule *left, const JetRule *right, JetRule *out);
void jet_rule_difference(const JetRule *left, const JetRule *right, JetRule *out);
void jet_rule_product(const JetRule *left, const JetRule *right, JetRule *out);
void jet_rule_scaled(const JetRule *rule, SimRational factor, JetRule *out);

// d/dX and d/deta
void jet_rule_x(const JetRule *rule, JetRule *out);
void jet_rule_slope_eta(const JetRule *rule, JetRule *out);

// 1 where the expression is 0
int jet_rule_zero(const JetRule *rule);

// the expression as text after the bytes of `line`, one term to a line
void jet_rule_text(const JetRule *rule, VerbaLinea *line);

// the products the expression contains
unsigned int jet_rule_terms(const JetRule *rule);

// 1 where a value in this module outgrew the build's width, an exponent passed a word, a product passed its factors, a
// table passed its bound, or the pool did not meet a cell of an expression
int jet_rule_short(void);

#endif
