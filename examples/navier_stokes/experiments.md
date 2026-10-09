# Experiments: Duraiswami's matched core, exactly

The matched core of Duraiswami (arXiv 2609.17642, Section 7) is the leading-order system of OpenAI 2026, eq. (4.13),
in the paper's profile variables, solved from data on the axis and joined to the heat exterior by a blend psi. The
goal is to show that the system, the family and the six matching functions held here are the paper's, as exact
relations, with no number in the comparison.

## Rules for every experiment

- Nothing infinite is summed. A series is held as the rule that gives its coefficient of order k, and a coefficient
  is computed only where one is asked for. No series is cut at an order, no sum is taken to a length, and no value is
  written to a number of places.
- A function that is not a polynomial, the core's fields as functions of X, psi, w, E1 and e^(-1/s) among them, is a
  held term carried with the exact relations that define it, and is never evaluated.
- A check passes or fails only where the answer is exact: an identity reduces to 0 or it does not. An identity over
  the orders of a series is checked at a general order k, never for k up to a bound.
- No printed decimal of the paper enters. The paper's numbers are rounded, and a comparison with them is not exact.
- Every form and every reduction is written whole to `records/`, tracked and committed with the work.

## 0. What the tree holds

`term_form` holds exact forms in held terms, e at a rational power. `eta_function` holds exact functions of eta over
powers of L = 1 - 2 h eta^2. `decay_integral` writes int_0^b s^k e^(-n/s) ds as two terms. `witness_cube`, `record`
and `run_cfg` measure, write and read.

## 1. Series held by their rule

A series in X is the sum over k of its entries a_k X^k. An expression in the entries at a general order k is a sum of
parts. A part is a factor times one entry at k plus a fixed shift, or a Cauchy product: the sum over i of a factor
times one entry at i and one at k + c - i, an entry at a negative order being 0. A factor is a polynomial in eta, h,
k and i over a power of L, and an entry carries a derivative in eta of its field. Each expression is held in one
normal form, and two are equal exactly when their difference is 0.

The operations are X f, d/dX, X d/dX, the product of two series, a shift of k, and the eta operators L^-1, eta,
d = 1 - eta^2 and d/deta.

Check (`core_rule`): d/dX (f g) - (f' g + f g'), X d/dX (f g) - (X f' g + f X g'), d/deta (f g) - (f_eta g + f g_eta)
and d/deta (L^-1 f) - L^-1 f_eta - 4 h eta L^-2 f are 0 at a general k.

## 2. The core at every order

The system of OpenAI 2026, eq. (4.13), in the paper's variables with nu = 1, A = 1/2 + h and D = 1/2 - h:

    T_{-(A+1/2)} F + v0 (X F_X + F) + U Z_{-(A+1/2)} F - 2 (X F)_XX = 0,
    T_{-A} U + X v0 U_X + U Z_{-A} U + Z_{-2A} Pi - 2 (X U_X)_X = 0,
    (X v0)_X = L^-1 (2 A eta U - d U_eta + 2 eta X U_X),   Pi_X = F^2,
    T_b f = L^-1 (-b f + D eta f_eta + X f_X),   Z_b f = L^-1 (2 b eta f + d f_eta - 2 eta X f_X),

and the rule of the paper's eqs. (17) and (18) for F_(k+1), U_(k+1), v_k and P_(k+1).

Check (`core_rule`): the coefficient of X^k of each of the four equations, taken from the system by the operations of
1, equals the rule's at a general k: the rule solves the system at every order at once. With one part of the rule
changed it does not. The rule takes F_0 and P_0 even in eta and U_0 odd to F_k and P_k even and U_k odd at every k.

## 3. Held functions and their relations

F, U, v0 and Pi as functions of (X, eta), psi, w, E1 and e^(-1/s) are held terms, each with the relations that define
it: the system of 2, psi' = psi (1 - 2 s) / (s (1 - s))^2, Kummer's equation for w, and E1' = -e^(-x) / x. An integral
of a product of held functions is a held term of its own, with its derivative and the relation integration by parts
gives.

An expression is held in one normal form (`jet_rule`): eta^2 is written as 1 - d, the powers of X, d and 2 are held
with exponents a + b h, and every derivative a relation reaches is written through it.

Check (`matching_rule`): the exterior F_ext = (c / sqrt 2) (2d)^(-1-h) w(X / (2d)), with Kummer's equation for w,
solves T_{-(A+1/2)} F - 2 (X F)_XX = 0 exactly, and X F_ext does not.

## 4. The six matching functions

The net torque X tau_theta(X_b), the net force sqrt(X) tau_z(X_b), M(inf), J(inf), S(inf) with the exterior tail taken
off, and int_0^inf (H - H_pow) dX, each an exact form in the held terms of 3.

Check (`matching_rule`): with V = X v0 and Pi held by V_X = L^-1 (2 A eta U - d U_eta + 2 eta X U_X) and Pi_X = F^2,
the torque int X R_theta and the force int R_z each equal a boundary bracket and a local integrand for any F and U,
and with one part changed they do not. The tail of H, int (z w - z^(-h)) dz = (z^2 (w' - w) + z^(1-h)) / (h - 1), the
tail of S, int X F_ext^2 dX = -c^2 X^(-2h) / (4h) + (c^2 / 2) (2d)^(-2h) int (z w^2 - z^(-1-2h)) dz, and
int H_pow dX = sqrt 2 c X^(1-h) / (1 - h) hold with Kummer's equation, and the tail of H with h - 2 in place of h - 1
does not. No order of the core and no length of a blend enters.

## 5. The pressure datum

The datum Pi_0 = G(Pi_0) is stated as a relation on a held function Pi_0 of eta, never taken by steps. Whether it has
exactly one solution is the lemma of the workbook's chapter on terms to put back. G reads the core out to X_b, and the
first thing the lemma needs is a radius in X the core's series is proved to reach, every bound of the proof exact.

Check (`core_radius`): on the ellipses E_rho over the segment [-1, 1], one scale per order,
|f|_k = sup ||f||_rho (rho0 - rho)^k, the rule of 2 bounds |F_k|_k, |U_k|_k, |P_k|_k and |v_k|_(k+1) order by order
from the data: a witness, exact to an order N. Two inequalities at N carry x_k <= B r^-k / (k + 1) to every order past
N, and the series reaches X < r (rho0 - rho_min). The derivative d f' is held at the feet eta = -1 and eta = +1, where
d is 0: (1 - eta^2) T_m' = m (T_(m-1) - T_(m+1)) / 2 bounds it with no rho_min, and that bound proves three to nine
times the radius the bound through f' alone does.

A witness of one bound per order grows by one ratio from its first orders, and the proof reaches that ratio: each order
loses Delta in eta, and a radius near 1/10 is what any ellipse of the cfg proves, where X_b near 2 is asked. The
witness by weights keeps each order apart as its Chebyshev weights, f_k with F_k = f_k / L^(2k): a derivative scales
mode m by m, a product is a sum over modes, and no ellipse is carried through the orders. One is read only where the
order is bounded, sum |f_(k,m)| rho^m. The weights' magnitudes prove about three times the radius of the bounds. The
exact weights keep every cancellation, and on E_2 they shrink by a ratio near 5 an order, the series' own X near 2.5
on that ellipse.

The proof on one ellipse: the rule fixes the length of each order's weights, at most g0 + (g0 + 4) k + 1 for the
data's degree g0, and on E_rho a derivative costs that length times 2 rho / (rho^2 - 1), or (rho + 1/rho) / 2 at the
feet. No ellipse is lost, and the proof reaches X < r / l^2 on E_rho itself. The Cauchy sums split at an order N0:
a part with one index below N0 reads the exact weights, and only a part with both indices at N0 or past it reads the
bound. At N = 24 it proves X < 0.95 on E_(3/2) and X < 0.75 on E_2, where the exact weights shrink by a ratio near
3.5 an order on E_(3/2), the series' own X past 3 on that ellipse.

The parts linear in the order past N fall as 1 / N, and the largest of them is the transport U_0 d F_k', its
derivative charged at the whole length of F_k's weights. Read as amplitudes in [0, 1] over the place t = m / G_k in
[0, 1], the exact weights sit near t = 0.11 on E_(3/2), 0.2 on E_2 and 0.35 on E_3 at every order the record holds:
the length charges a derivative about ten times what the weights ask on E_(3/2).

A bound past N of the binomial shape binom(k + m, m) s^-m r^-k, whose mean mode grows as k, does not hold in the
Chebyshev weights: D eta F_k' reaches every lower mode, and below the bound's peak the bound rises with m. At mode 0 the
bound grows as (s rho / (s rho - 1))^k.

The carry: the exact weights to N at their magnitudes, carried past N mode by mode by the magnitudes of the rule. Each
carried order bounds the exact one where its weight sits, with no shape assumed, and each is rounded up to a multiple
of 2^-bits. Carried from N = 24 to 36 it proves X < 1.18 on E_(1.1), X < 1.24 on E_(5/4) and
X < 1.14 on E_(3/2).

The first carried order gives up every cancellation of its step at once, about ten times the exact one, and past it
the carried weights shrink by about 2.3 an order on E_(1.1), where X = 2 asks 2.09. The proof's parts that read the
bound, B_w / 4 and B_u beta / 4, do not fall with N, and B_u is set by the orders just past the split: a split near
N / 2 takes them out of it. The parts linear in the order fall as 1 / N.

The carry on the device: past the exact orders every weight the carry reads is a multiple of 2^-bits, the exact
orders' magnitudes rounded up as they enter, and an order's Cauchy sums are integer sums. The device takes them
modulo primes below 2^31, the host reads them back whole by the Chinese remainder theorem, and the rest of each order
stays on the host. To the host's carry it is the same to the last bit. Carried to N = 100 and split at 50, the proof
gives X < 2.08 on E_(1.1), X < 2.04 on E_(5/4) and X < 1.82 on E_(3/2): the radius X_b near 2 the lemma reads, with
each witness in `lean/CoreRadius/Record.lean`. The questions left:

- The loss in eta comes from U d F_eta, a transport in eta. Along the flow of that transport no derivative is lost.
- The series continued from a point X_0 inside the radius: whether the scale of ellipses starts again there, or
  whether each center loses Delta again.

## 6. The paper's system against this one

The paper's text (arXiv 2609.17642v1) holds the operators T_b and Z_b of its (3), the system (4), the rule (17) and
(18) with the products of lower orders named and not written, the heat exterior E = c X^(-A) H(2d/X) as the solution
of T_{-(A+1/2)} F - 2 (X F)_XX = 0, the stresses tau_theta = X^-1 int x R_theta and tau_z = (2X)^(-1/2) int R_z, and
the exterior pressure -int F_ext^2. Its code (gitlab.umiacs.umd.edu/ramanid/swirl-collapse, `axis_cone.py`) holds
the family and the matching route: F_0 and U_0 Chebyshev sums in eta, the annulus content a bump in X times a tensor
Chebyshev sum, and the six functions taken from the stress T_0 of OpenAI 2026, eqs. (4.11) and (4.15)-(4.16), built
from the five cumulative integrals M, I, J, S and Pi.

Check (`core_rule`): the system of 2 is the paper's (3) and (4) term for term. The paper's (17) and (18) as printed
hold -(A + 1/2) F_k and -A U_k inside L^-1, where its own (3) gives +(A + 1/2) and +A: they differ from (3) and (4) at
order k by exactly 2 (A + 1/2) L^-1 F_k and 2 A L^-1 U_k, and the rule of 2 is what (3) and (4) give. With
H(Z) = Z^(-1-h) U(1 + h, 2, 1/Z), the paper's exterior is F_ext of 3, and `matching_rule` checks that it solves the
equation the paper names.

Check (`matching_rule`): with M_X = U, I_X = 2 X F, J_X = 2 X F U, S_X = U^2 - X F^2 and Pi_X = F^2, continuity
integrated from the axis is V = X v0 = L^-1 (2 eta X U - 2 D eta M - d M_eta), and the code's stress is minus the
integrated residuals, (X T0_theta)_X = -X R_theta and (sqrt(2X) T0_z)_X = -R_z, for any F and U; with one part of Q_s
changed it is not. The matching route of the code and the torque and force of 4 are one identity, and it holds for
every member of the family, whatever its weights.

## 7. The walls of a medium

The core's angular speed is v = r tau^(-1-h) F(X, eta) with tau = T - t and X = r^2 / (2 nu tau), as Proposition 20 of
the millennium chapter on the coupled system takes it, tau in seconds. A medium leaves the conditions (1) is written
for at walls: its speed passes a share of its sound speed, the light speed, or the speed at which one particle carries
the energy that frees an electron; the radius at X falls to the spacing below which it is not a continuum; or the time
left falls to its relaxation time, where it answers as an elastic solid.

Check (`core_radius`): on [-1, 1], F at X is at least the data's c_0 - sum |c_m| less the sum of n_k (l^2 X)^k to the
order the norms reach and B_f (l^2 X / r)^k past it, every term from the proof on one ellipse, the bound rounded down
to a multiple of 2^-bits. With tau <= 1, v^2 >= 2 nu X F_low^2 / tau, and each wall is an exact time left the core
passes it by. The X of a proved radius where X F_low^2 is largest is 43/32 on E_(1.1), with F_low near 0.75.

| medium | first wall | its time left | speed of light | radius at the spacing |
|---|---|---|---|---|
| water | 3/10 of sound | 10^-12 to 10^-11 s | 10^-23 to 10^-22 s | 10^-14 to 10^-13 s |
| air | 3/10 of sound | 10^-9 to 10^-8 s | 10^-22 to 10^-21 s | 10^-10 to 10^-9 s |
| neutron star matter | 3/10 of sound | 10^-16 to 10^-15 s | 10^-17 to 10^-16 s | 10^-30 to 10^-29 s |

In water the walls of sound, of the elastic answer, of the spacing and of the collisions that free an electron fall
within two decades of each other, and the radius at X still halves 48 to 53 times before the Planck time. Neutron
star matter passes the light speed thirteen decades before its radius reaches its spacing. The constants are the
cfg's: water at 20 C by IAPWS R6-95 and R12-08, air at 20 C and 0.1 MPa from standard tables, the relaxation of each
its order of magnitude, and neutron star matter at nuclear density with its viscosity and sound speed of order
estimates. The data are the cfg's, F_0 = 1, and not the matched datum of 5: every wall's time scales as F_low^2.

## 8. The datum's tangent

The datum's right side G(Pi_0) = -int_0^(X_b) F_blend^2 dX - int_(X_b)^inf F_ext^2 dX holds the exterior, and the
exterior is not analytic at eta = -1 and +1: F_ext is the second solution U of Kummer's equation at X / (2d), and
its expansion as d falls to 0 diverges. G therefore leaves every space of functions analytic on an ellipse about [-1, 1], and the datum is held on
a cut |eta| <= a < 1, where the exterior is analytic. The rule is local in eta, its integrals all in X, and the cut
problem is closed.

Check (`core_radius` with `cfg/core_radius_cut.cfg`): the rule in xi = eta / a, with eta = a xi, d/deta = (1 / a)
d/dxi, L = (1 - h a^2) - h a^2 T_2, d = (1 - a^2 / 2) - (a^2 / 2) T_2 and d f_eta = (1 / a) [(1 - xi^2) f_xi +
(1 - a^2) xi^2 f_xi], gives at a = 1 the record of 5 to the last bit. At a = 9/10 the proof carried to 100 on the
device gives X < 1.73 on E_(1.1), X < 1.88 on E_(5/4) and X < 1.77 on E_(3/2), below X_b = 2: the cut loses the
feet's exact form, and the radius with it.

The datum enters the core through Z_{-2A} Pi alone, and each order takes one eta-derivative and pays
1 / (2 (k+1)^2) for it. Along a datum direction e^(mu eta) every order of the tangent is exactly e^(mu eta) times a
polynomial in mu, and only a path that takes the derivative of e^(mu eta) at every order reaches mu^k at order k.

Check (`core_tangent`): to order 20, du_k has degree k in mu and df_k degree k from k = 2, and their top weights are
the products du_1 = L d / 2, du_(k+1) = sigma L du_k / (2 (k+1)^2), dw_k = -L d du_k / (k + 1) and
df_(k+1) = (sigma L df_k + F_0 dw_k) / (2 (k+1)(k+2)), sigma = D eta + U_0 d, every weight exact. At eta = 1/2 on the
cut 9/10, sigma = 173/400 and the top coefficient of dF_k is below 0 at every order: the coefficient of mu^k grows
by a factor near sigma mu / (2 k^2) an order.

The sum of every path that takes one derivative a step is a system in X at a fixed eta. With Y = X / L^2, V' = dU and
the inflow W = -mu L d V / Y,
    2 (Y V'')' - mu s(Y) V' + mu L d U'(Y) V = mu L d,   2 (Y dF)'' = mu s(Y) dF + W (Y F)',
s(Y) = L (D eta + d U(Y, eta)), and a path that skips a derivative is smaller by mu^(-1/2). The growth of the tangent
is e^(sqrt(mu) Phi(Y)) with Phi(Y) = int_0^Y sqrt(s(y) / (2 y)) dy, which at Y near 0 is the Bessel series the top
weights sum to. s holds the core's axial field along X, and the core holds the datum from U_1 on: the exponent of
the growth is the datum's own. The derivative of G on a direction of eta-frequency mu then grows as e^(c sqrt(mu)),
with c set by the datum, and the lemma of the workbook, Kantorovich's, asks a bound on that derivative over a ball
that no fixed space of functions in eta gives.

On [0, Y_1], let s_eff = s - L d Y max(U', 0). Where s_eff >= s_least > 0, y = V' and y' stay at least 0, V <= Y y,
and 2 (Y y')' >= mu s_least y + mu L d; z = (L d / s_least) (I_0(sqrt(2 s_least mu Y)) - 1) solves it with equality,
and y - z, 0 at Y = 0, stays at least 0. Where (Y F)' is past 0 as well, W <= 0 drives dF below 0, and -dF grows
as z does.

The point is the vertex eta_v = a (rho0 + 1 / rho0) / 2 of the ellipse. The Chebyshev weights of e^(mu eta) in xi are
2 I_m(mu a), all past 0, and its norm on E_rho0 is e^(mu eta_v); a field's norm is at least its value at the vertex,
since |T_m(xi_v)| <= rho0^m there. The ratio of the derivative of G on e^(mu eta) to e^(mu eta)'s own norm is then at
least the growth the tangent has at the vertex, and a point inside the cut would set e^(mu eta) against
e^(mu eta_v) and lose.

Check (`core_radius` with the cut's `principal`): at the vertex of each ellipse, on Y in [0, 1.25] cut in 64 pieces, the
core's U, U' and (Y F)' read from the exact orders to 24, the device's magnitudes on E_rho0 to 100 and the reached
proof's tail past it give, every bound exact,

| ellipse | vertex eta_v | s_eff at least | (Y F)' at least |
|---|---|---|---|
| E_(1.1) | 0.9041 | 0.3941 | 0.9951 |
| E_(5/4) | 0.9225 | 0.4085 | 0.9951 |
| E_(3/2) | 0.9750 | 0.4550 | 0.9951 |

Check (`lean/CoreRadius/Principal.lean`): `principal_comparison` puts y above theta z for theta in [0, 1) by a
continuous induction on p - theta p_z, `principal_lower` takes theta to 1, and `principal_growth` puts y above
(c / s0) sum_(k=1..K) (s0 mu Y / 2)^k / (k!)^2 for every K, the partial sums of (c / s0) (I_0(sqrt(2 s0 mu Y)) - 1),
each a polynomial that solves the inequality: the kernel checks it on the axioms propext, Classical.choice and
Quot.sound alone.

Measured (`core_tangent` with `measure`): at the vertex of E_(1.1), to order 32, the base's s(Y) gives
Phi(Y) = sqrt(2 s_0 Y) sum e_k Y^k / (2k + 1) exactly, and the principal and the full tangent are summed exactly for
mu = 1, 4, 16 and 64 on Y = 1/8 to 1. The principal axial field follows the measured Phi with the Bessel factor:
ln|y| - sqrt(mu) Phi(Y) + ln(mu) / 4 at Y = 1 is -3.35, -2.43, -1.99 and -1.90, and from mu = 16 on it is flat in Y to
0.01. The full tangent over the principal, less 1, is

| Y | mu = 4 | mu = 16 | mu = 64 |
|---|---|---|---|
| 1/4, axial | -2.75 | -0.80 | -0.30 |
| 1, axial | -3.51 | -1.28 | -0.58 |
| 1, swirl | -8.45 | -1.68 | -0.66 |

and times sqrt(mu) it falls toward a constant near -5 at Y = 1: the paths that skip a derivative are a part of order
mu^(-1/2) whose constant is large, and below mu = 16 they outweigh the principal part and turn its sign.

What is left to prove it: lower bounds on the system in X with error bounds of Olver's kind, on the base fields'
bounds, for the paths that skip a derivative; the amplitude's eta-derivatives, by Cauchy's estimates on disks of
radius near mu^(-1/2); and the integral of G against the blend near X_b.

## Order

0, then 1, then 2, then 3, then 4, then 5 and 6, then 7, then 8.
