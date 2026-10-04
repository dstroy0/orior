// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// atom_value.h: every atom written in a few independent numbers, each the sum of its own exact series to one length,
// and a form's value there
#ifndef ATOM_VALUE_H
#define ATOM_VALUE_H

// At length N every series below is summed to N terms and every continued fraction taken to N levels; each value is
// an exact rational, and the values converge as N grows. w = U(1 + h, 2, z), E_n the exponential integral.
// - e^q = e^m e^f, m the whole part of q: e = sum 1/k!, e^f = sum f^k / k!.
// - x^p = (1 - y)^(-p) = sum (p)_k / k! y^k, y = 1 - 1/x, for x above 1/2: 2^(-1/2), 2^(-h), (3/2)^(-h).
// - rho(c) = 1 / (1 + e^r), r = 1/c - 1/(1 - c).
// - eps_n(x) = e^x E_n(x): eps_1 = 1 / (x + 1 - 1 / (x + 3 - 4 / (x + 5 - 9 / ...))), eps_0 = 1/x,
//   eps_-1 = 1/x + 1/x^2, eps_(n+1) = (1 - x eps_n) / n; E1(x) = e^(-x) eps_1(x).
// - Gamma(s), 1 < s < 2: int_0^1 e^(-t) t^(s-1) dt = sum (-1)^m / (m! (m + s)), and
//   int_1^inf = e^-1 / (2 - s - 1 (1 - s) / (4 - s - 2 (2 - s) / (6 - s - ...))).
// - Gamma(1 + h) w(z) = int_0^inf e^(-zt) t^h (1 + t)^(-h) dt, and -Gamma(1 + h) w'(z) the same with t^(1+h), split
//   at t = 1. Below, (1 + t)^(-h) = 2^(-h) sum (h)_k / k! ((1 - t) / 2)^k and e^(-zt) by its series; each term is the
//   beta integral int_0^1 t^(h+m) (1 - t)^k dt = k! / ((h + m + 1) ... (h + m + k + 1)). Above,
//   t^h (1 + t)^(-h) = sum (-h)_k / k! (1 + t)^(-k), and int_1^inf e^(-zt) (1 + t)^(-k) dt = e^(-z) 2^(1-k) eps_k(2z).
//   This gives w(1) and w'(1); at any other z, w's own series about 1 (ode_series_kummer) summed at z, |z - 1| < 1.
// - int_(z_b)^inf (z w - z^(-h)) dz = (z_b^2 (w'(z_b) - w(z_b)) + z_b^(1-h)) / (1 - h), from
//   (z^2 w' - z^2 w)' = (h - 1) z w: the tail of H holds no integral of its own.
// - int_(z_b)^inf (z w^2 - z^(-1-2h)) dz = sum_n D_n (z_b G_n^+ + G_n) / Gamma(2 + 2h) - z_b^(-2h) / (2h). The two
//   integrals of w and the two of z^(-1-h) are joined in sigma = t + s, t = sigma x, and expanded in
//   u = sigma / (1 + sigma), with (1 + sigma x)(1 + sigma (1 - x)) = (1 + sigma)^2 (1 - u x)(1 - u (1 - x)):
//   D_n = sum_(j+l=n) (h)_j (h)_l (h+1)_j (h+1)_l / (j! l! (2h+2)_n), and G_n and G_n^+ are
//   int_0^inf e^(-z sigma) sigma^(a-1) (1 + sigma)^(-a) d sigma and the same with sigma^a, a = 2h + n. Each is split
//   at sigma = 1 as w is; above it, with v = 1 / (1 + sigma), the integrand is v (1 - v)^n or (1 - v)^(n+1) times
//   (1 - v)^(2h-1) = sum (1 - 2h)_k / k! v^k, and int_1^inf e^(-z sigma) v^j d sigma = e^(-z) 2^(1-j) eps_j(2z).
// - int_(z_b)^inf w^2 dz = sum_n D_n G_n^+ / Gamma(2 + 2h), the same join with 1/sigma for z_b/sigma + 1/sigma^2.

#include "atom_form.h"

#include <string>
#include <vector>

typedef struct
{
    SimRational key;
    SimRational value;
} AtomValuePair;

typedef struct
{
    SimRational x;
    // eps_n(x) at n = -1, 0, 1, ...
    std::vector<SimRational> eps;
} AtomValueRatios;

typedef struct
{
    SimRational h;
    unsigned int length;
    SimRational e;
    // e^q at each q asked for
    std::vector<AtomValuePair> exponentials;
    std::vector<AtomValueRatios> ratios;
    SimRational gamma_once;
    SimRational gamma_twice;
    SimRational value_at_one;
    SimRational slope_at_one;
} AtomValues;

// the independent numbers at length `length`
void atom_value_open(AtomValues *values, SimRational h, unsigned int length);

// e^q
SimRational atom_value_e(AtomValues *values, SimRational q);

// x^p, x above 1/2
SimRational atom_value_power(AtomValues *values, SimRational x, SimRational p);

// eps_n(x), n -1 or more
SimRational atom_value_ratio(AtomValues *values, SimRational x, int n);

// Gamma(s), 1 < s < 2
SimRational atom_value_gamma(AtomValues *values, SimRational s);

// w(z) and w'(z), 0 < z < 2, by w's series about 1
void atom_value_kummer(AtomValues *values, SimRational z, SimRational *value, SimRational *slope);

// w(z) and w'(z), z above 0, by the integral split at t = 1
void atom_value_integral(AtomValues *values, SimRational z, SimRational *value, SimRational *slope);

// int_(z_b)^inf (z w^2 - z^(-1-2h)) dz
SimRational atom_value_square_tail(AtomValues *values, SimRational z_b);

// int_(z_b)^inf w^2 dz
SimRational atom_value_square_integral(AtomValues *values, SimRational z_b);

// int_(z_b)^inf (z w - z^(-h)) dz
SimRational atom_value_linear_tail(AtomValues *values, SimRational z_b);

// the value of the atom named `name` as the modules name their atoms: 1, or 0 where the name is not one of them
int atom_value_named(AtomValues *values, const std::string &name, SimRational *value);

// the form's value, atom i given by atoms[i]
SimRational atom_value_form(AtomValues *values, const AtomForm &form, const std::vector<SimRational> &atoms);

// 1 where a value in this module outgrew the build's width
int atom_value_short(void);

#endif
