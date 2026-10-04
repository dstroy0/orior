// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// term_value.h: every term written in a few independent numbers, each the sum of its own exact series to one length,
// and a form's value there, as the sums and chains the record machine runs over its lanes
#ifndef TERM_VALUE_H
#define TERM_VALUE_H

// At length N every series below is summed to N terms and every continued fraction taken to N levels; each value is
// an exact rational, and the values converge as N grows. w = U(1 + h, 2, z), E_n the exponential integral.
// - e^q = e^m e^f, m the whole part of q: e = sum 1/k!, e^f = sum f^k / k!.
// - x^p = (1 - y)^(-p) = sum (p)_k / k! y^k, y = 1 - 1/x, for x above 1/2: 2^(-1/2), 2^(-h), (3/2)^(-h).
// - rho(c) = 1 / (1 + e^r), r = 1/c - 1/(1 - c).
// - eps_n(x) = e^x E_n(x): eps_1 = 1 / (x + 1 - 1 / (x + 3 - 4 / (x + 5 - 9 / ...))), eps_0 = 1/x,
//   eps_-1 = 1/x + 1/x^2, eps_(n+1) = (1 - x eps_n) / n; E1(x) = e^(-x) eps_1(x).
// - Gamma(s), 1 < s < 2: int_0^1 e^(-t) t^(s-1) dt = sum_m (-1)^m / (m! (s + m)), and
//   int_1^inf = e^-1 / (2 - s - 1 (1 - s) / (4 - s - 2 (2 - s) / (6 - s - ...))).
// - Gamma(1 + h) w(z) = int_0^inf e^(-zt) t^h (1 + t)^(-h) dt, and -Gamma(1 + h) w'(z) the same with t^(1+h), split
//   at t = 1. Below, (1 + t)^(-h) = 2^(-h) sum (h)_k / k! ((1 - t) / 2)^k and e^(-zt) by its series; each term is the
//   beta integral int_0^1 t^(h+m) (1 - t)^k dt = k! / ((h + m + 1) ... (h + m + k + 1)). Above,
//   t^h (1 + t)^(-h) = sum (-h)_k / k! (1 + t)^(-k), and int_1^inf e^(-zt) (1 + t)^(-k) dt = e^(-z) 2^(1-k) eps_k(2z).
//   This gives w(1) and w'(1); at any other z, w's own series about 1 summed at z, |z - 1| < 1, its coefficients
//   from z w'' + (2 - z) w' - (1 + h) w = 0.
// - int_(z_b)^inf (z w - z^(-h)) dz = (z_b^2 (w'(z_b) - w(z_b)) + z_b^(1-h)) / (1 - h), from
//   (z^2 w' - z^2 w)' = (h - 1) z w: the tail of H holds no integral of its own.
// - int_(z_b)^inf (z w^2 - z^(-1-2h)) dz = sum_n D_n (z_b G_n^+ + G_n) / Gamma(2 + 2h) - z_b^(-2h) / (2h). The two
//   integrals of w and the two of z^(-1-h) are joined in sigma = t + s, t = sigma x, and expanded in
//   u = sigma / (1 + sigma), with (1 + sigma x)(1 + sigma (1 - x)) = (1 + sigma)^2 (1 - u x)(1 - u (1 - x)):
//   D_n = sum_(j+l=n) (h)_j (h)_l (h+1)_j (h+1)_l / (j! l! (2h+2)_n), and G_n and G_n^+ are
//   int_0^inf e^(-z sigma) sigma^(a-1) (1 + sigma)^(-a) d sigma and the same with sigma^a, a = 2h + n. Each is split
//   at sigma = 1 as w is; above it, with v = 1 / (1 + sigma), the integrand is v (1 - v)^n or (1 - v)^(n+1) times
//   (1 - v)^(2h-1) = sum (1 - 2h)_k / k! v^k, and int_1^inf e^(-z sigma) v^j d sigma = e^(-z) 2^(1-j) eps_j(2z).
//   The sum over j is taken as sum_i c_i G_i, c_i the coefficients of v (1 - v)^n or (1 - v)^(n+1) and
//   G_q = sum_u (1 - 2h)_u / u! 2^(1-u-q) eps_(u+q)(2z), the same sum in another order.
// - int_(z_b)^inf w^2 dz = sum_n D_n G_n^+ / Gamma(2 + 2h), the same join with 1/sigma for z_b/sigma + 1/sigma^2.
//
// None of it is computed here. A value is written as rows over a divisor, and the record machine computes them: a
// row is a sign, whole factors, and residues raised to powers, and a residue is an exact integer the lanes hold modulo
// their primes, one lane a prime. The divisor is whole factors and residues, carried beside the rows and never divided
// out. A sum of rows becomes one residue where the lanes write it, each lane a row summed over its run; a series
// summed by Horner's rule or a continued fraction becomes a chain, the lanes stepping X' = aX + bY, Y' = cX + dY with
// whole a, b, c, d. Each residue carries a bound on the bits its integer takes, and from the widest the program knows
// how many primes hold it.

#include "term_form.h"

#include <map>
#include <string>
#include <vector>

// whole factors as prime to exponent
typedef std::map<unsigned long long, unsigned int> TermValuePrimes;

// residues as residue to exponent
typedef std::map<unsigned int, unsigned int> TermValuePowers;

// a residue: an integer below 2^bits in magnitude, written at `level`, the sweep after every residue it reads
typedef struct
{
    unsigned long long bits;
    unsigned int level;
} TermValueResidue;

// a row: its sign, whole factors as primes, a whole factor past a word as its limbs where there is one, and residues
typedef struct
{
    int negative;
    TermValuePrimes primes;
    std::vector<unsigned int> wide;
    TermValuePowers residues;
} TermValueRow;

// the sum of the rows over the divisor, the divisor's whole factors as primes and the rest residues
typedef struct
{
    std::vector<TermValueRow> rows;
    TermValuePrimes divisor_primes;
    TermValuePowers divisor_residues;
} TermValue;

// a residue the lanes write as the sum of its rows, each row one lane over its run
typedef struct
{
    unsigned int target;
    std::vector<TermValueRow> rows;
} TermValueSum;

// a chain's step: X' = a X + b Y, Y' = c X + d Y
typedef struct
{
    long long a;
    long long b;
    long long c;
    long long d;
} TermValueStep;

// a chain: X from x_word times residue x_residue, Y from y_word times y_residue, then its steps; X times the tail is
// written, the tail a sign and whole factors, and Y beside it. Chains of one run are summed: the run's X times tails
// is one residue. history[s] is X after step s where it is asked for.
typedef struct
{
    long long x_word;
    unsigned int x_residue;
    long long y_word;
    unsigned int y_residue;
    std::vector<TermValueStep> steps;
    TermValueRow tail;
    std::vector<unsigned int> history;
} TermValueChain;

typedef struct
{
    unsigned int x_target;
    unsigned int y_target;
    std::vector<TermValueChain> chains;
} TermValueRun;

typedef struct
{
    SimRational key;
    TermValue value;
} TermValuePair;

typedef struct
{
    SimRational x;
    SimRational p;
    TermValue value;
} TermValuePower;

typedef struct
{
    SimRational x;
    // eps_n(x) at n = -1, 0, 1, ...
    std::vector<TermValue> eps;
    // the continued fraction's tail P/Q, eps_1 = Q/P
    unsigned int continued_x;
    unsigned int continued_y;
} TermValueRatios;

typedef struct
{
    SimRational z;
    TermValue value;
    TermValue slope;
} TermValueKummer;

typedef struct
{
    SimRational h;
    unsigned int length;
    // every residue; residue 0 is the integer 1
    std::vector<TermValueResidue> residues;
    std::vector<TermValueSum> sums;
    std::vector<TermValueRun> runs;
    TermValue e;
    std::vector<TermValuePair> exponentials;
    std::vector<TermValuePower> powers;
    std::vector<TermValueRatios> ratios;
    std::vector<TermValueKummer> kummers;
    TermValue gamma_once;
    TermValue gamma_twice;
    TermValue value_at_one;
    TermValue slope_at_one;
    // w's series about 1 at this length: its coefficients a_k = first[k] / (k! hd^k), and the second's
    std::vector<TermValue> first;
    std::vector<TermValue> second;
} TermValues;

// the program begun: residue 0 the integer 1, nothing written yet
void term_value_begin(TermValues *values);

// the independent numbers at length `length`, written into the program after whatever it already holds
void term_value_open(TermValues *values, SimRational h, unsigned int length);

// a rational whose parts fit a word, as a value
TermValue term_value_from(SimRational rational);

// numerator / denominator, each a word, as a value
TermValue term_value_whole(long long numerator, long long denominator);

// whole factors multiplied into words, each below 2^62: at least one word, 1 where there are no factors
std::vector<unsigned long long> term_value_words(const TermValuePrimes &primes);

// the bits a row's integer takes in magnitude, at most
unsigned long long term_value_row_bits(const TermValues *values, const TermValueRow &row);

// left + right, left - right, left times right
TermValue term_value_add(TermValues *values, const TermValue &left, const TermValue &right);
TermValue term_value_less(TermValues *values, const TermValue &left, const TermValue &right);
TermValue term_value_times(TermValues *values, const TermValue &left, const TermValue &right);

// 1 / value, its rows first written as one residue
TermValue term_value_reciprocal(TermValues *values, const TermValue &value);

// the value with its rows written as one residue: a single row, that residue
TermValue term_value_written(TermValues *values, const TermValue &value);

// e^q
TermValue term_value_e(TermValues *values, SimRational q);

// x^p, x above 1/2
TermValue term_value_power(TermValues *values, SimRational x, SimRational p);

// eps_n(x), n -1 or more
TermValue term_value_ratio(TermValues *values, SimRational x, int n);

// Gamma(s), 1 < s < 2
TermValue term_value_gamma(TermValues *values, SimRational s);

// w(z) and w'(z), 0 < z < 2, by w's series about 1
void term_value_kummer(TermValues *values, SimRational z, TermValue *value, TermValue *slope);

// w(z) and w'(z), z above 0, by the integral split at t = 1
void term_value_integral(TermValues *values, SimRational z, TermValue *value, TermValue *slope);

// int_(z_b)^inf (z w^2 - z^(-1-2h)) dz
TermValue term_value_square_tail(TermValues *values, SimRational z_b);

// int_(z_b)^inf w^2 dz
TermValue term_value_square_integral(TermValues *values, SimRational z_b);

// int_(z_b)^inf (z w - z^(-h)) dz
TermValue term_value_linear_tail(TermValues *values, SimRational z_b);

// the value of the term named `name` as the modules name their terms: 1, or 0 where the name is not one of them
int term_value_named(TermValues *values, const std::string &name, TermValue *value);

// the form's value, term i numerators[i] / divisors[i], or 0 where zero[i]: every slot taken over D_i^k_i, k_i the
// highest power of term i the form asks for, and the form's denominator
TermValue term_value_form(TermValues *values, const TermForm &form, const std::vector<unsigned int> &numerators,
                          const std::vector<unsigned int> &divisors, const std::vector<int> &zero);

// the divisor written as one residue
unsigned int term_value_divisor(TermValues *values, const TermValue &value);

// the value written as one residue over a divisor written as one: 1, or 0 where its rows sum to 0 by their writing
int term_value_parts(TermValues *values, const TermValue &value, unsigned int *numerator, unsigned int *divisor);

// 1 where a value in this module could not be written
int term_value_short(void);

#endif
