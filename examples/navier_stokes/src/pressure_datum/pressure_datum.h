// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pressure_datum.h: the right side of Duraiswami's pressure datum at one eta, an exact form in terms
#ifndef PRESSURE_DATUM_H
#define PRESSURE_DATUM_H

// His join fixes the axis value of the pressure by
//     Pi_0(eta) = -int_0^{X_b} F_blend^2 dX + Pi_ext(X_b, eta),
//     F_blend = chi F_core + (1 - chi) F_ext,   chi = 1 - psi(s),   s = (X - X_a) / (X_b - X_a),
// and F_blend = F_core + psi (F_ext - F_core) on the annulus and F_core inside it. On [0, X_a] the integral is of the
// core polynomial's square, exact. On the annulus, in s, it is
//     (X_b - X_a) int_0^1 (G_0 + psi G_1 + psi^2 G_2) ds,
//     G_0 = F_core^2, G_1 = 2 F_core (F_ext - F_core), G_2 = (F_ext - F_core)^2,
// each by blend.h. The exterior swirl is F_ext = (c / sqrt 2) (2d)^(-1-h) w(X / (2d)), w = U(1 + h, 2, z), about each
// center its terms w(z_c) and w'(z_c), with 2^(-1/2) and (2d)^(-h) terms of their own, 2^(-1/2) squared reduced to
// 1/2. Past the join, Pi_ext(X_b, eta) = -(c^2 / 2) (2d)^(-1-2h) int_{z_b}^inf w^2 dz, z_b = X_b / (2d), the integral
// an term.

#include "blend.h"
#include "core_series.h"

typedef struct
{
    EtaShape shape;
    SimRational h;
    // c, the exterior's amplitude
    SimRational amplitude;
    // the core's series, its swirl cut where the caller cut it
    const CoreSeries *series;
    BlendPieces pieces;
} PressureDatumRequest;

typedef struct
{
    // int_0^{X_a} F_core^2 dX, (X_b - X_a) int_0^1 F_blend^2 ds, Pi_ext, and the right side -inside - annulus + beyond
    TermForm inside;
    TermForm annulus;
    TermForm beyond;
    TermForm datum;
} PressureDatum;

// the right side at `eta`, -1 < eta < 1, with the join from X_a = `inner` to X_b = `outer`, 0 < inner < outer
void pressure_datum_at(const PressureDatumRequest *request, SimRational eta, SimRational inner, SimRational outer,
                       TermBook *book, PressureDatum *datum);

// 1 where a value in this module outgrew the build's width
int pressure_datum_short(void);

#endif
