// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// blend_field.h: fields on the annulus 0 <= s <= 1 that carry the blend weight psi, their products, their derivative
// in s, and their integrals over the cut pieces, every value a form in atoms
#ifndef BLEND_FIELD_H
#define BLEND_FIELD_H

// psi = e^(-1/s) / (e^(-1/s) + e^(-1/(1-s))) and the bump e^(4 - 1/(s(1-s))) are flat at both ends, where no Taylor
// series reaches them, and a field is held differently in the middle and at the ends:
// - About a middle center c, a field is one Taylor series in s - c, psi among its factors as blend_weight gives it,
//   every form reduced by rho (1 + r) = 1.
// - At an end, in tau = s at s = 0 and tau = 1 - s at s = 1, a field is sum over j of psi_e^j times a sum of pieces
//   e^(-m/tau) tau^(-p) S(tau), S a Taylor series about 0. psi_e is psi at s = 0 and psi(t) = 1 - psi(s) at s = 1.
// d/dtau psi_e = (psi_e - psi_e^2) tau^(-2) Q(tau), Q = 1 + tau^2 / (1 - tau)^2: a derivative stays a polynomial in
// psi_e, two orders of pole higher; d/ds = d/dtau at s = 0 and -d/dtau at s = 1. An end integral expands psi_e^j in
// u = e^(-1/tau) e^(1/(1-tau)): psi_e^j = sum over N >= j of (-1)^(N-j) C(N-1, j-1) u^N, u^N = e^N e^(-N/tau) g_N, and
// each power of tau against e^(-(N+m)/tau) is the two-term decay integral, k = -4 and below included.

#include "blend.h"

typedef struct
{
    unsigned int decay;
    unsigned int pole;
    TaylorSeries series;
} BlendFieldPiece;

enum
{
    BLEND_FIELD_MIDDLE = 0,
    BLEND_FIELD_LOW = 1,
    BLEND_FIELD_HIGH = 2
};

// where fields are held: a middle center, or an end, with the interval in s it covers
typedef struct
{
    unsigned int kind;
    SimRational center;
    SimRational low;
    SimRational high;
    unsigned int terms;
    unsigned int orders;
    // the middle's weight series and its reduction, rho (1 + e^ratio) = 1
    TaylorSeries weight;
    SimRational ratio;
    unsigned int share;
} BlendPlace;

typedef struct
{
    const BlendPlace *place;
    // part[j] multiplies psi_e^j at an end; in the middle part[0] alone is held
    std::vector<std::vector<BlendFieldPiece>> part;
} BlendField;

// the field a smooth function gives, its series in s about the place's center
BlendField blend_field_smooth(const BlendPlace *place, const TaylorSeries &series);

// the weight psi itself
BlendField blend_field_weight(const BlendPlace *place);

// the bump e^(4 - 1/(s(1-s)))
BlendField blend_field_bump(const BlendPlace *place);

BlendField blend_field_sum(const BlendField &left, const BlendField &right);
BlendField blend_field_difference(const BlendField &left, const BlendField &right);
BlendField blend_field_scaled(const BlendField &field, SimRational factor);
BlendField blend_field_form_scaled(const BlendField &field, const AtomForm &form);
BlendField blend_field_product(const BlendField &left, const BlendField &right);

// d/ds, one term fewer in each series
BlendField blend_field_derivative(const BlendField &field);

// the field's value at the place's center: the middle's series at c, or at an end the part with no psi and no decay,
// pole 0, at tau = 0; 1 where it is there to take, 0 where a piece is singular at the end
int blend_field_value(const BlendField &field, AtomForm *value);

// the field integrated over the place's interval in s
AtomForm blend_field_integral(const BlendField &field, AtomBook *book);

// the places the cuts make: the end at s = 0, the middle pieces about their midpoints, the end at s = 1
std::vector<BlendPlace> blend_field_places(const BlendPieces *pieces, AtomBook *book);

// a field at each place, given by the caller
typedef BlendField (*BlendFieldIntegrand)(const void *context, const BlendPlace *place, AtomBook *book);

// int_0^1 of the caller's field over the pieces the cuts make
AtomForm blend_field_total(const BlendPieces *pieces, BlendFieldIntegrand integrand, const void *context,
                           AtomBook *book);

// 1 where a value in this module outgrew the build's width, or an end integral was asked of a piece that diverges
int blend_field_short(void);

#endif
