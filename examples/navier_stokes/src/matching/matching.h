// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// matching.h: Duraiswami's six matching functions of eta (arXiv 2609.17642, Section 7) at one eta, exact forms
#ifndef MATCHING_H
#define MATCHING_H

// The blended profile, as his code builds it, on the annulus X_a <= X <= X_b, X = X_a + w s, w = X_b - X_a:
//     F = F_core + psi (F_ext - F_core) + dF,   U = (1 - psi) U_core + dU,
//     dF = b(s) sum F_jk T_j(2s - 1) T_k(eta),   dU = b(s) sum U_jk T_j(2s - 1) T_k(eta),   b = e^(4 - 1/(s(1-s))),
// F_ext = (c / sqrt 2) (2d)^(-1-h) w(X / (2d)), w = U(1 + h, 2, z), and beyond X_b the exterior with U = 0. With
// E^2 = 2 X F^2 and H = 2 X F his cumulative integrals are M = int U, I = int H, J = int U H, S = int (U^2 - E^2 / 2),
// and his six functions are
//     X tau_theta(X_b) = int_{X_a}^{X_b} X R_theta dX,   sqrt(X) tau_z(X_b) = 2^(-1/2) int_{X_a}^{X_b} R_z dX,
//     M(X_b),   J(X_b),   S(X_b) - int_{X_b}^inf E^2 / 2 dX,   I(X_b) - int_0^{X_b} H_pow + int_{X_b}^inf (H - H_pow),
// with R_theta and R_z the left sides of core_series.h and H_pow = sqrt 2 c X^(-h). The cumulative v0 and Pi enter
// R_theta and R_z, and are taken out by parts, V = X v0 and C = V_X = L^-1 (2 A eta U - d U_eta + 2 eta X U_X):
//     int V (X F)_X = [V X F] - int C X F,   int V U_X = [V U] - int C U,
//     int Pi = Pi(X_a) w + X_b P(X_b) - int X F^2,   P = int_{X_a} F^2,   and the same for Pi_eta,
// and the viscous terms are boundary values, -2 [X^2 F_X] and -2 [X U_X]. Every other integrand is local, a field of
// blend_field.h. The exterior tail of S is c^2 X_b^(-2h) / (4h) + (c^2 / 2) (2d)^(-2h) int_{z_b}^inf (z w^2 - z^(-1-2h))
// dz, the last integral an atom, and the tail of H is sqrt 2 c times the atom
// int_{X_b}^inf ((2d)^(-1-h) x w(x / (2d)) - x^(-h)) dx.

#include "blend_field.h"
#include "core_series.h"
#include "run_cfg.h"

typedef struct
{
    EtaShape shape;
    SimRational h;
    // c, the exterior's amplitude
    SimRational amplitude;
    const CoreSeries *series;
    SimRational inner;
    SimRational outer;
    BlendPieces pieces;
    // the annulus content, row j the mode T_j(2s - 1), column k the mode T_k(eta)
    std::vector<std::vector<SimRational>> swirl_content;
    std::vector<std::vector<SimRational>> axial_content;
    // 1 for his blend; 0 leaves the core alone on the annulus, where the torque and force are the core's residuals
    int blended;
} MatchingRequest;

typedef struct
{
    AtomForm torque;
    AtomForm force;
    AtomForm m_inf;
    AtomForm j_inf;
    AtomForm s_inf;
    AtomForm h_match;
    // c^2 X_b^(-2h) / (4h), the part of the tail of S with no atom of w
    AtomForm s_tail_power;
    // 1 where the fields' values at X_a and X_b were there to take
    int ends;
} MatchingFunctions;

// the request the cfg gives: core anisotropy, axis swirl, axial and pressure and order, join amplitude, inner and outer,
// content swirl and axial each as eta_modes and weights, blend cuts, terms and orders; the core series built into
// `series`, which the request points to. 1, or 0 where a member is missing or out of its range
int matching_read(const RunCfg *cfg, MatchingRequest *request, CoreSeries *series);

// the six functions at `eta`, -1 < eta < 1
void matching_at(const MatchingRequest *request, SimRational eta, AtomBook *book, MatchingFunctions *functions);

// 1 where a value in this module outgrew the build's width
int matching_short(void);

#endif
