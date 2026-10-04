# Experiments: reproducing Duraiswami's matched core

The goal is to show that our system and Duraiswami's matched core (arXiv 2609.17642, Section 7) are the same
solution. Our forms converge onto his printed numbers, or to within the places he printed them with, as the series
order and the blend's terms grow:

All of his runs here take h = 0.01, c = 0.2, X_a = 1 and X_b = 2.

The exterior tail of S, c^2 X_b^(-2h) / (4h), is 0.99. It needs no fit.

The 8-parameter optimum: F_0 and U_0 in span(T_0, T_1, T_2) of eta, and the annulus return flow
(b_0 + b_1 eta) psi(X):

| quantity               | his value                     |
|------------------------|-------------------------------|
| F_0                    | 0.244 + 0.034 eta - 0.058 T_2 |
| U_0                    | 0.450 + 0.004 eta - 0.034 T_2 |
| b_0                    | -2.01                         |
| b_1                    | not printed                   |
| root-mean-square       | 0.077                         |
| with 13 parameters     | 0.039                         |

The 32-parameter matched core: F_0 and U_0 to T_4 (10), annulus axial content 2 x 6 (12) and annulus swirl content
2 x 5 (10):

| quantity                 | his value | where            |
|--------------------------|-----------|------------------|
| F_0 at eta = 0           | 0.336     | X = 0            |
| U_0 at eta = 0           | 0.452     | X = 0            |
| F_0 at eta = 1           | 0.214     | X = 0            |
| return flow              | -2.06     | the annulus      |
| largest abs U            | 0.60 / 1.9 | core / annulus  |
| root-mean-square         | 1.2e-3    | the six functions |
| worst                    | 3.7e-3    | the six functions |
| pressure datum           | 5e-13     | consistency      |

His family, as his code writes it:

- F_0 and U_0 are Chebyshev sums in eta.
- The annulus axial content is psi_b(X; X_a, X_v) times sum over j, k of U_jk T_j(s_v) T_k(eta), with
  s_v = 2 (X - X_a) / (X_v - X_a) - 1. The swirl content is psi_b(X; X_a, X_b) times sum over j, k of F_jk T_j(s_b)
  T_k(eta). psi_b = e^(4 - 1/(s (1 - s))) on 0 < s < 1, which is ode_series_bump.
- The six matching functions of eta are the net torque X tau_theta(X_b), the net force sqrt(X) tau_z(X_b), M(inf),
  J(inf), S(inf) with the exterior tail taken off, and int_0^inf (H - H_pow) dX.

## Rules for every experiment

- Every value is an exact form: rational coefficients times rational powers of one held e times held terms.
- The terms (E1, w(z_c) and w'(z_c), rho, the tail integral) are never evaluated. Where a form meets one of his
  decimals, his decimals define the terms: the term values his printed numbers imply are solved for exactly, and
  his other printed numbers are the check. They agree, or the disagreement is recorded.
- A subject is measured by 8 witnesses at the corners of a cube around it. The 8 corner forms give the 8 components:
  the value, 3 edge differences, 3 face differences and 1 body difference, each a whole form.
- For each term, the number of the 8 components it needs is recorded as its character and is not interpreted.
- A check passes or fails only where the answer is exact: a residual is 0 or it is not. Everything else is recorded.
- Every whole form, all 8 corners, is written to `records/`, tracked and committed with the work.

## 0. Ground work

- 0a. Dense magnitude arrays. A form is one array of exact integers over a shared index, the power of e and then
  the term powers, with one denominator per form. Check: join_datum, join_series and axis_heat give the same forms
  term for term as the map. Recorded: entries and bits per entry.
- 0b. Module objects built once per width and linked into each driver, each rebuilt only where its source or a
  header it reads is newer.
- 0c. The witness cube module: a subject of 3 rational parameters, a center and a half-edge from the cfg, the 8
  corner forms, the 8 components, and each term's count. The record writer writes every form whole.

## 1. The cube on subjects already known

e^(-1/s), psi, the Kummer w, x^h, and A K and A J from axis_heat. Check: the 8 components equal the matching
combinations of the Taylor coefficients already held, exactly.

Measured (`witness_known`, 5 checks): the trilinear form's 8 terms each have character 1, the product's 2 terms
character 8, and A K and A J 8 terms each of character 8, every corner holding its own E1(s_b) and e^(-s_b).

## 2. The axis core

The subject is F, U and Pi over (X, eta, h), the cubes centered at X = 0, eta = 0 and eta = 1. Recorded: the
difference of the forms at order K and K + 1 for K = 40, 60, 80, and the ratio and root estimates at each order,
beside his radius of 3.9 to 4.0.

Measured (`core_cubes`, 3 checks, 512 limbs) on his 8-parameter F_0 and U_0 with Pi_0 = 0, which stands in until
the datum of experiment 6 replaces it; half-edges 1/8, 1/8 and 1/1000 in (X, eta, h):

| cube center (X, eta, h) | largest component change, 40 to 60 | 60 to 80 |
|-------------------------|------------------------------------|----------|
| (1/8, 0, 1/100)         | 1.2e-56                            | 2.0e-84  |
| (1/8, 7/8, 1/100)       | 2.2e-63                            | 3.2e-94  |
| (1, 0, 1/100)           | 8.2e-30                            | 1.6e-44  |

The change falls by about 1e-15 for each 20 orders at the join, which puts the radius near 6 for these data. Near
the axis c_0 = c_1, c_2 = c_3, c_4 = c_5 and c_6 = c_7 for Pi: every corner at X = 0 is Pi_0 = 0.

## 3. The datum against the join's own choices

The subject is the right side of Pi_0 over (X_a, X_b, eta). The answer depends on eta; the X_a and X_b components
measure how much the join's placement puts into it. A sweep over blend terms, orders and cuts records the
difference forms.

Measured (`datum_cubes`, 3 checks) about (X_a, X_b, eta) = (1, 2, 1/2), half-edges 1/8: 877 terms over 57 terms.
873 terms have character 8. The 4 terms of the exterior tail have character 4, held in c_0, c_2, c_4 and c_6 alone,
the components with no X_a direction: the tail does not depend on X_a. The part with no term has c_0 = -0.168,
c_1 = -0.0074 (X_a), c_2 = -0.0101 (X_b) and c_4 = 0.0218 (eta).

## 4. The annulus family and the six identities

Both sides of each identity as exact forms, the difference reduced in the terms. Every term left in the difference is
recorded: those are the terms the identity drops.

Measured (`matching_functions`, 6 checks) on his 8-parameter F_0, U_0 and b_0 = -2.01, with b_1 = 0 standing in
for the value he does not print and Pi_0 = 0 for the datum: at eta = 0 and 1/2 the six functions are exact forms of
34 to 291 terms over 31 terms. With the blend switched off the core alone fills the annulus, and its torque and force
come out at 1.2e-10 and 3.7e-11, the core series' own truncation at order 24: the boundary terms and the parts taken
out by parts are right.

## 5. The fits

His parameters enter the series polynomially and the Jacobian is exact. Exact Gauss-Newton on the six functions,
the terms defined by his decimals.

- 5a. The exterior tail, c^2 X_b^(-2h) / (4h) at his values, against 0.99. Measured: it is exactly 2^(-1/50), and
  two exact comparisons, 0.985^50 2 < 1 < 0.995^50 2, put it within his two places of 0.99.
- Term values. Every term is written in the few independent numbers it is made of, and each of those is the limit
  of its own exact sequence of rationals, taken to more and more terms; a form's value at each length is exact, and
  the values are recorded as they converge:
  - e^q = e^m e^f by the exponential series, m the whole part of q, and rho(c) = 1 / (1 + e^ratio) from it.
  - 2^(-1/2), 2^(-h) and (3/2)^(-h) by the binomial series, x^p = sum (p)_k / k! (1 - 1/x)^k.
  - E1(x) = e^(-x) eps_1(x), eps_1 by its continued fraction; eps_n = e^x E_n by eps_(n+1) = (1 - x eps_n) / n.
  - w(1) and w'(1) from Gamma(1 + h) w(z) = int_0^inf e^(-zt) t^h (1 + t)^(-h) dt split at t = 1: below,
    (1 + t)^(-h) by its binomial series in (1 - t) / 2 and e^(-zt) by its series, each term a beta integral; above,
    t^h (1 + t)^(-h) = (1 - 1/(1 + t))^h by its binomial series, each term e^(-z) 2^(1-k) eps_k(2z). Gamma(1 + h) is
    int_0^1 by its series and Gamma(1 + h, 1) by its continued fraction. Every other w(z_c) and w'(z_c) is w's own
    series about 1 summed at z_c.
  - The tail of H is no new number: (z^2 w' - z^2 w)' = (h - 1) z w gives
    int_(z_b)^inf (z w - z^(-h)) dz = (z_b^2 (w'(z_b) - w(z_b)) + z_b^(1-h)) / (1 - h).
  - The tail of S: the two integrals of w joined in sigma = t + s and expanded in u = sigma / (1 + sigma) give
    int_(z_b)^inf (z w^2 - z^(-1-2h)) dz = sum_n D_n (z_b G_n^+ + G_n) / Gamma(2 + 2h) - z_b^(-2h) / (2h), every D_n
    rational and every G_n an integral split at sigma = 1 as w's is (term_value.h).

  Measured (`matching_values`, 1024 limbs) on the forms of experiment 4, his 8-parameter F_0, U_0 and b_0 = -2.01,
  b_1 = 0 and Pi_0 = 0, at lengths 16, 24 and 32. Gamma(1 + h) comes out 0.994325851, w(1) = 0.994015508 and
  w'(1) = -0.999925858. At z = 1/2 and z = 5/4, w and w' by the series about 1 and by the integral agree to 8 places.
  The six functions at length 32:

  | eta | torque      | force       | M(inf)      | J(inf)      | S(inf)      | H          |
  |-----|-------------|-------------|-------------|-------------|-------------|------------|
  | 0   | -0.0289574  | 0.00876772  | 0.109920    | -0.102630   | -0.0404458  | -0.0699486 |
  | 1/2 | 0.0129802   | -0.359788   | -0.0682000  | -0.187081   | 0.0284819   | -0.0684166 |

  Each value moves by no more than 8e-7 from length 16 to 24 and 4e-9 from 24 to 32. At length 32 each is an exact
  rational of up to 5000 digits, and the record holds them whole. These are not yet his 0.077: Pi_0 waits on the
  fixed point of experiment 6, b_1 is not solved for, and his eta grid is the Chebyshev points with eta = +-1, where
  the exterior's form is singular.
- 5b. The 8-parameter optimum. His printed F_0, U_0 and b_0 are put in exactly and b_1 is solved for. Check: the
  root-mean-square converges onto 0.077, and a fit over all 8 lands on his printed coefficients within his places.
- 5c. The 32-parameter matched core. Check: the forms converge onto every value in the 32-parameter table.

## 6. The Pi_0 fixed point

Y = G(p) - p and DG(p) at the polynomial p as exact forms on the Chebyshev basis. Z waits on the ellipse space in
the workbook.

Pi_0 = sum_(k<n) p_k T_k(eta), and G(p)(eta) is the right side of the datum with the core built on that Pi_0, every
term valued as in 5. Pi_0 is a fixed point where Y_i = G(p)(eta_i) - p(eta_i) is 0 at n rational nodes inside the
reach of w's series about 1. Pi_0 enters the core at U_1 and comes back through every product after it. G's values
are rationals of thousands of digits, and a whole one carried into the next core passes any width: each iterate is
written to a set number of decimal places, as his data are, and Y at it is exact. The step to the next iterate solves
J d = -Y on decimals of Y and J, J by divided differences at the first iterate and Broyden's update after.

Measured (`datum_fixed_point`, 4 checks, 1024 limbs) on his 8-parameter F_0 and U_0, order 24, blend terms 12, terms at
length 24, five modes on the nodes eta = 0, +-0.3, +-0.6, from Pi_0 = 0:

| iterate | places | largest abs Y at the nodes |
|---------|--------|----------------------------|
| 0       | -      | 0.176                      |
| 1       | 3      | 6.9e-4                     |
| 2       | 4      | 8.3e-5                     |
| 3       | 6      | 1.2e-6                     |
| 4       | 8      | 8.0e-9                     |
| 5       | 10     | 9.0e-11                    |

Pi_0 = -0.1145935217 - 0.0135767167 T_1 + 0.0565850762 T_2 - 0.0070435720 T_3 - 0.0021593181 T_4. Between the nodes,
at eta = +-0.15 and +-0.45, Y is 2e-5 to 8.5e-5: the part of G five modes do not hold.

## 7. The axis heat with every term

The dissipation with the axial shear and the radial strain, integrated on the full core. The difference from
Proposition 20's swirl-only result is recorded term by term. Cubes over (q, h, Pr) centered on his run's values
and on water at 20 C.

## Order

0, then 1, then 2 and 3, then 4, then 5 and 6, then 7.
