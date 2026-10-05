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
exactly one solution is the lemma of the workbook's chapter on terms to put back, and waits on the space it names.

## 6. The paper's system against this one

The paper's text (arXiv 2609.17642v1) holds the operators T_b and Z_b of its (3), the system (4), the rule (17) and
(18) with the products of lower orders named and not written, the heat exterior E = c X^(-A) H(2d/X) as the solution
of T_{-(A+1/2)} F - 2 (X F)_XX = 0, the stresses tau_theta = X^-1 int x R_theta and tau_z = (2X)^(-1/2) int R_z, and
the exterior pressure -int F_ext^2. The family of axis data and annulus content and the fits are in its code, not its
text.

Check (`core_rule`): the system of 2 is the paper's (3) and (4) term for term. The paper's (17) and (18) as printed
hold -(A + 1/2) F_k and -A U_k inside L^-1, where its own (3) gives +(A + 1/2) and +A: they differ from (3) and (4) at
order k by exactly 2 (A + 1/2) L^-1 F_k and 2 A L^-1 U_k, and the rule of 2 is what (3) and (4) give. With
H(Z) = Z^(-1-h) U(1 + h, 2, 1/Z), the paper's exterior is F_ext of 3, and `matching_rule` checks that it solves the
equation the paper names.

## Order

0, then 1, then 2, then 3, then 4, then 5 and 6.
