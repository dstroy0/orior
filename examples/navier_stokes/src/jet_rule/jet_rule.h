// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jet_rule.h: held functions of X and eta and their derivatives, every expression in one normal form, every relation
// exact
#ifndef JET_RULE_H
#define JET_RULE_H

// An expression is a sum of exact rational coefficients times products of:
//     eta to the power 0 or 1, h to a whole power, L^-1 to a whole power, L = 1 - 2 h eta^2,
//     X^(a + b h), d^(a + b h) and 2^(a + b h), a and b rational, d = 1 - eta^2,
//     constants held by name, and
//     the derivatives of held functions: f_(X^a eta^b) for a function f of X and eta, and w^(n) for Kummer's w of
//     z = X / (2 d).
// eta^2 is held as 1 - d, every whole power of 2 is moved into the coefficient, and each base X, d and 2 is held once
// in a product with its exponents added. A held function may carry a relation that gives its derivative in X of one
// order through lower ones; every derivative at or past that order is written through the relation, differentiated as
// often as it takes. w^(n), n >= 2, is written through Kummer's equation z w'' + (2 - z) w' - (1 + h) w = 0.
// An expression is 0 exactly when, multiplied by L to the highest power of L^-1 it holds, every coefficient is 0:
// L L^-1 = 1 and eta^2 = 1 - d are the only relations among the factors, and the powers of X, d and 2 with distinct
// exponents are independent.

#include "sim_rational.h"

#include <map>
#include <string>
#include <vector>

// a factor's place and its exponent a + b h, a and b held as whole numerators over whole denominators
typedef struct
{
    unsigned int place;
    long long a_numerator;
    long long a_denominator;
    long long b_numerator;
    long long b_denominator;
} JetRuleFactor;

typedef std::vector<JetRuleFactor> JetRuleMonomial;

struct JetRuleMonomialOrder
{
    bool operator()(const JetRuleMonomial &left, const JetRuleMonomial &right) const;
};

typedef std::map<JetRuleMonomial, SimRational, JetRuleMonomialOrder> JetRule;

// the bases a power may be taken of
typedef enum
{
    JET_RULE_BASE_X = 0,
    JET_RULE_BASE_D = 1,
    JET_RULE_BASE_TWO = 2
} JetRuleBase;

JetRule jet_rule_number(long long numerator, long long denominator);
JetRule jet_rule_rational(SimRational value);
JetRule jet_rule_eta(void);
JetRule jet_rule_h(void);
JetRule jet_rule_over_l(void);

// base^(a + b h)
JetRule jet_rule_power(JetRuleBase base, long long a_numerator, long long a_denominator, long long b_numerator,
                       long long b_denominator);

// a constant held by name, its derivatives 0
JetRule jet_rule_constant(const char *name);

// the held function `name` of X and eta, a new one where none is held by that name, and its derivative
// d^a/dX^a d^b/deta^b
unsigned int jet_rule_function(const char *name);
JetRule jet_rule_slope(unsigned int function, unsigned int a, unsigned int b);

// Kummer's w of z = X / (2 d), its derivative of order n in z
JetRule jet_rule_w(unsigned int n);

// d^order f / dX^order = `value` for the held function, `value` holding only derivatives of lower order in X of it
void jet_rule_relate(unsigned int function, unsigned int order, const JetRule &value);

JetRule jet_rule_sum(const JetRule &left, const JetRule &right);
JetRule jet_rule_difference(const JetRule &left, const JetRule &right);
JetRule jet_rule_product(const JetRule &left, const JetRule &right);
JetRule jet_rule_scaled(const JetRule &rule, SimRational factor);

// d/dX and d/deta
JetRule jet_rule_x(const JetRule &rule);
JetRule jet_rule_slope_eta(const JetRule &rule);

// 1 where the expression is 0
int jet_rule_zero(const JetRule &rule);

// the expression as text
std::string jet_rule_text(const JetRule &rule);

// the products the expression holds
size_t jet_rule_terms(const JetRule &rule);

// 1 where a value in this module outgrew the build's width or an exponent passed a word
int jet_rule_short(void);

#endif
