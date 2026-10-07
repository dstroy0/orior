-- SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
import Mathlib.Tactic

/-!
# The tail of the proof on one ellipse

`core_radius.cu` bounds the norms of the axis core's fields on one ellipse order by order. With each norm scaled by
`r^k`, `F k`, `U k`, `P k` and `W k` for the fields F, U, Pi and v0, the rule of `core_rule.cu` gives four
relations at every order past N (`Holds`). The Cauchy sums split at an order `s`: a part with one index below `s`
reads the sequences themselves, and only a part with both indices at `s` or past it reads the bound. `tail` proves
that the bounds held on `[s, N]` and the two inequalities at N, `angular` and `along`, carry the bounds to every order
past `s`.
-/

namespace CoreRadius

open Finset

variable {K : Type*} [Field K] [LinearOrder K] [IsStrictOrderedRing K]

/-- The constants of the rule on one ellipse. -/
structure Rule (K : Type*) where
  rate : K
  lambda : K
  angular_a0 : K
  along_a0 : K
  a1 : K
  alpha_angular : K
  alpha_along : K
  alpha_pressure : K
  beta : K

/-- Every constant of the rule is at least 0, the rate past 0, and the angular and pressure alphas at least 2 beta's
share the bound of a Z part reads. -/
structure Rule.Legal (c : Rule K) : Prop where
  rate : 0 < c.rate
  lambda : 0 ≤ c.lambda
  angular_a0 : 0 ≤ c.angular_a0
  along_a0 : 0 ≤ c.along_a0
  a1 : 0 ≤ c.a1
  alpha_angular : 0 ≤ c.alpha_angular
  alpha_along : 0 ≤ c.alpha_along
  alpha_pressure : 0 ≤ c.alpha_pressure
  beta : 0 ≤ c.beta

/-- The four relations the rule gives past N, the norms scaled by `r^k`. -/
structure Holds (c : Rule K) (N : ℕ) (F U P W : ℕ → K) : Prop where
  angular : ∀ k, N ≤ k → 2 * ((k : K) + 1) * ((k : K) + 2) * F (k + 1) ≤
    c.rate * ((c.angular_a0 + c.a1 * k) * F k
      + ∑ i ∈ range (k + 1), (((k - i : ℕ) : K) + 1) * W i * F (k - i)
      + ∑ i ∈ range (k + 1), U i * (c.alpha_angular + c.beta * ((k - i : ℕ) : K)) * F (k - i))
  along : ∀ k, N ≤ k → 2 * ((k : K) + 1) ^ 2 * U (k + 1) ≤
    c.rate * ((c.along_a0 + c.a1 * k) * U k
      + ∑ i ∈ range (k + 1), ((k - i : ℕ) : K) * W i * U (k - i)
      + ∑ i ∈ range (k + 1), U i * (c.alpha_along + c.beta * ((k - i : ℕ) : K)) * U (k - i)
      + (c.alpha_pressure + c.beta * k) * P k)
  pressure : ∀ k, N ≤ k → ((k : K) + 1) * P (k + 1) ≤
    c.rate * c.lambda ^ 2 * ∑ i ∈ range (k + 1), F i * F (k - i)
  inflow : ∀ k, N < k → ((k : K) + 1) * W k ≤ (c.alpha_along + c.beta * k) * U k

/-- The angular inequality's left side at N, over `r`. -/
def angular (c : Rule K) (s N : ℕ) (F U W : ℕ → K) (Bf Bu Bw : K) : K :=
  (c.angular_a0 + c.a1 * N) / (2 * ((N : K) + 1) * ((N : K) + 2))
  + (∑ i ∈ range s, W i) / (2 * ((N : K) + 2))
  + Bw * (∑ j ∈ range s, ((j : K) + 1) * F j) / (2 * ((N : K) + 1) * ((N : K) + 2) * Bf)
  + Bw / 4
  + (∑ i ∈ range s, U i) * (c.alpha_angular + c.beta * N) / (2 * ((N : K) + 1) * ((N : K) + 2))
  + Bu * (∑ j ∈ range s, (c.alpha_angular + c.beta * j) * F j) / (2 * ((N : K) + 1) * ((N : K) + 2) * Bf)
  + Bu * (c.alpha_angular / (2 * ((N : K) + 2)) + c.beta / 4)

/-- The axial inequality's left side at N, over `r`. -/
def along (c : Rule K) (s N : ℕ) (U W : ℕ → K) (Bu Bp Bw : K) : K :=
  (c.along_a0 + c.a1 * N) / (2 * ((N : K) + 1) ^ 2)
  + (∑ i ∈ range s, W i) / (2 * ((N : K) + 1))
  + Bw * (∑ j ∈ range s, (j : K) * U j) / (2 * ((N : K) + 1) ^ 2 * Bu)
  + Bw / 4
  + (∑ i ∈ range s, U i) * (c.alpha_along + c.beta * N) / (2 * ((N : K) + 1) ^ 2)
  + (∑ j ∈ range s, (c.alpha_along + c.beta * j) * U j) / (2 * ((N : K) + 1) ^ 2)
  + Bu * (c.alpha_along / (2 * ((N : K) + 1)) + c.beta / 4)
  + (c.alpha_pressure + c.beta * N) * Bp / (2 * ((N : K) + 1) ^ 2 * Bu)

/-- A Cauchy sum split at `s`: the terms with `i < s`, those with `k - i < s`, and the rest. -/
lemma sum_split {k s : ℕ} (h_order : 2 * s ≤ k + 1) {t a b c : ℕ → K}
    (hc0 : ∀ i, 0 ≤ c i)
    (ha : ∀ i, i ≤ k → i < s → t i ≤ a i)
    (hb : ∀ i, i ≤ k → k - i < s → t i ≤ b (k - i))
    (hc : ∀ i, i ≤ k → s ≤ i → s ≤ k - i → t i ≤ c i) :
    ∑ i ∈ range (k + 1), t i ≤ ∑ i ∈ range s, a i + ∑ j ∈ range s, b j + ∑ i ∈ range (k + 1), c i := by
  have h_term : ∀ i ∈ range (k + 1),
      t i ≤ (if i < s then a i else 0) + (if k - i < s then b (k - i) else 0) + c i := by
    intro i hi
    have h_index : i ≤ k := by
      rw [mem_range] at hi
      omega
    by_cases h1 : i < s
    · have h2 : ¬ (k - i < s) := by omega
      simp only [h1, h2, ↓reduceIte]
      linarith [ha i h_index h1, hc0 i]
    · by_cases h2 : k - i < s
      · simp only [h1, h2, ↓reduceIte]
        linarith [hb i h_index h2, hc0 i]
      · simp only [h1, h2, ↓reduceIte]
        linarith [hc i h_index (by omega) (by omega)]
  have h_first : ∑ i ∈ range (k + 1), (if i < s then a i else 0) = ∑ i ∈ range s, a i := by
    rw [← sum_filter]
    congr 1
    ext i
    simp only [mem_filter, mem_range]
    omega
  have h_second : ∑ i ∈ range (k + 1), (if k - i < s then b (k - i) else 0) = ∑ j ∈ range s, b j := by
    have h_reflect := sum_range_reflect (fun j => if j < s then b j else 0) (k + 1)
    have h_same : ∀ i ∈ range (k + 1), (if k - i < s then b (k - i) else 0) =
        (fun j => if j < s then b j else 0) (k + 1 - 1 - i) := by
      intro i _
      simp only [Nat.add_sub_cancel]
    rw [sum_congr rfl h_same, h_reflect, ← sum_filter]
    congr 1
    ext j
    simp only [mem_filter, mem_range]
    omega
  calc ∑ i ∈ range (k + 1), t i
      ≤ ∑ i ∈ range (k + 1),
          ((if i < s then a i else 0) + (if k - i < s then b (k - i) else 0) + c i) := sum_le_sum h_term
    _ = ∑ i ∈ range s, a i + ∑ j ∈ range s, b j + ∑ i ∈ range (k + 1), c i := by
      rw [sum_add_distrib, sum_add_distrib, h_first, h_second]

/-- `0 + 1 + ... + k`, as the terms `k - i`. -/
lemma sum_rest : ∀ k : ℕ, ∑ i ∈ range (k + 1), ((k - i : ℕ) : K) = (k : K) * ((k : K) + 1) / 2
  | 0 => by simp
  | k + 1 => by
    rw [sum_range_succ', show ((k + 1 - 0 : ℕ) : K) = (k : K) + 1 by push_cast; ring]
    have h_shift : ∀ i ∈ range (k + 1), ((k + 1 - (i + 1) : ℕ) : K) = ((k - i : ℕ) : K) := by
      intro i _
      congr 1
      omega
    rw [sum_congr rfl h_shift, sum_rest k]
    push_cast
    ring

/-- For `k ≥ N ≥ 2`, `(a + b k) / ((k+1)(k+2))` is at most its value at N. -/
lemma falling_two {a b : K} (ha : 0 ≤ a) (hb : 0 ≤ b) {k N : ℕ} (hN : 2 ≤ N) (hk : N ≤ k) :
    (a + b * k) * (((N : K) + 1) * ((N : K) + 2)) ≤ (a + b * N) * (((k : K) + 1) * ((k : K) + 2)) := by
  have hNk : (N : K) ≤ k := by exact_mod_cast hk
  have hN2 : (2 : K) ≤ N := by exact_mod_cast hN
  have h_product : (2 : K) ≤ (N : K) * k := by nlinarith
  nlinarith [mul_nonneg hb (sub_nonneg.mpr hNk), mul_nonneg (mul_nonneg hb (sub_nonneg.mpr hNk)) (sub_nonneg.mpr h_product),
    mul_nonneg ha (sub_nonneg.mpr hNk)]

/-- For `k ≥ N ≥ 1`, `(a + b k) / (k+1)^2` is at most its value at N. -/
lemma falling_square {a b : K} (ha : 0 ≤ a) (hb : 0 ≤ b) {k N : ℕ} (hN : 1 ≤ N) (hk : N ≤ k) :
    (a + b * k) * ((N : K) + 1) ^ 2 ≤ (a + b * N) * ((k : K) + 1) ^ 2 := by
  have hNk : (N : K) ≤ k := by exact_mod_cast hk
  have hN1 : (1 : K) ≤ N := by exact_mod_cast hN
  have h_product : (1 : K) ≤ (N : K) * k := by nlinarith
  nlinarith [mul_nonneg hb (sub_nonneg.mpr hNk), mul_nonneg (mul_nonneg hb (sub_nonneg.mpr hNk)) (sub_nonneg.mpr h_product),
    mul_nonneg ha (sub_nonneg.mpr hNk)]

/-- `(k+1)(k+2)/2` times `x`, as the sum over `i` of `(k - i + 1) x`. -/
lemma sum_rest_succ (k : ℕ) (x : K) :
    ∑ i ∈ range (k + 1), (((k - i : ℕ) : K) + 1) * x = ((k : K) + 1) * ((k : K) + 2) / 2 * x := by
  rw [← sum_mul, sum_add_distrib, sum_rest, sum_const, card_range, nsmul_eq_mul]
  push_cast
  ring

/-- `k(k+1)/2` times `x`, as the sum over `i` of `(k - i) x`. -/
lemma sum_rest_mul (k : ℕ) (x : K) :
    ∑ i ∈ range (k + 1), ((k - i : ℕ) : K) * x = (k : K) * ((k : K) + 1) / 2 * x := by
  rw [← sum_mul, sum_rest]

/-- The sum over `i` of `x (alpha + beta (k - i))`. -/
lemma sum_alpha_rest (k : ℕ) (alpha beta x : K) :
    ∑ i ∈ range (k + 1), x * (alpha + beta * ((k - i : ℕ) : K)) =
      x * (((k : K) + 1) * alpha + beta * ((k : K) * ((k : K) + 1) / 2)) := by
  rw [← mul_sum, sum_add_distrib, sum_const, card_range, nsmul_eq_mul, ← mul_sum, sum_rest]
  push_cast
  ring

/-- A product of three factors at least 0 grows with each. -/
lemma le_mul3 {x y z x' y' z' : K} (hx : x ≤ x') (hy : y ≤ y') (hz : z ≤ z') (hx0 : 0 ≤ x) (hy0 : 0 ≤ y)
    (hz0 : 0 ≤ z) : x * y * z ≤ x' * y' * z' :=
  mul_le_mul (mul_le_mul hx hy hy0 (hx0.trans hx)) hz hz0 (mul_nonneg (hx0.trans hx) (hy0.trans hy))

section Steps

variable {c : Rule K} {s N : ℕ} {F U P W : ℕ → K} {Bf Bu Bp Bw : K}

/-- The bounds at every order of `[s, k]`. -/
def Below (s k : ℕ) (F U P W : ℕ → K) (Bf Bu Bp Bw : K) : Prop :=
  ∀ j, s ≤ j → j ≤ k → F j ≤ Bf ∧ U j ≤ Bu ∧ P j ≤ Bp ∧ W j ≤ Bw

set_option maxHeartbeats 2000000 in
/-- The angular part carries the bound of F to `k + 1`. -/
lemma step_angular (hc : c.Legal) (hsN : 2 * s ≤ N) (hN : 2 ≤ N)
    (hF : ∀ k, 0 ≤ F k) (hU : ∀ k, 0 ≤ U k) (hW : ∀ k, 0 ≤ W k)
    (h_rule : Holds c N F U P W) (hBf : 0 < Bf) (hBu : 0 ≤ Bu) (hBw : 0 ≤ Bw)
    (h_angular : c.rate * angular c s N F U W Bf Bu Bw ≤ 1)
    {k : ℕ} (hk : N ≤ k) (ih : Below s k F U P W Bf Bu Bp Bw) :
    F (k + 1) ≤ Bf := by
  have h_order : 2 * s ≤ k + 1 := by omega
  have hkN : (N : K) ≤ k := by exact_mod_cast hk
  have hk0 : (0 : K) ≤ k := Nat.cast_nonneg k
  have hN0 : (0 : K) ≤ N := Nat.cast_nonneg N
  have h_cast : ∀ i, ((k - i : ℕ) : K) ≤ k := fun i => by exact_mod_cast Nat.sub_le k i
  have h_cast0 : ∀ i, (0 : K) ≤ ((k - i : ℕ) : K) := fun i => Nat.cast_nonneg _
  set alpha := c.alpha_angular
  set beta := c.beta
  have h_alpha : 0 ≤ alpha := hc.alpha_angular
  have h_beta : 0 ≤ beta := hc.beta
  -- the inflow's sum
  have h_sum_w := sum_split (K := K) (k := k) (s := s) h_order
    (t := fun i => (((k - i : ℕ) : K) + 1) * W i * F (k - i))
    (a := fun i => ((k : K) + 1) * Bf * W i)
    (b := fun j => ((j : K) + 1) * Bw * F j)
    (c := fun i => (((k - i : ℕ) : K) + 1) * (Bw * Bf))
    (fun i => mul_nonneg (by linarith [h_cast0 i]) (mul_nonneg hBw hBf.le))
    (fun i hi his => by
      have hFb : F (k - i) ≤ Bf := (ih (k - i) (by omega) (by omega)).1
      calc (((k - i : ℕ) : K) + 1) * W i * F (k - i) ≤ ((k : K) + 1) * W i * Bf :=
            le_mul3 (by linarith [h_cast i]) le_rfl hFb (by linarith [h_cast0 i]) (hW i) (hF _)
        _ = ((k : K) + 1) * Bf * W i := by ring)
    (fun i hi his => by
      have hWb : W i ≤ Bw := (ih i (by omega) hi).2.2.2
      exact le_mul3 le_rfl hWb le_rfl (by linarith [h_cast0 i]) (hW i) (hF _))
    (fun i hi h_low h_high => by
      have hWb : W i ≤ Bw := (ih i h_low hi).2.2.2
      have hFb : F (k - i) ≤ Bf := (ih (k - i) h_high (by omega)).1
      calc (((k - i : ℕ) : K) + 1) * W i * F (k - i) ≤ (((k - i : ℕ) : K) + 1) * Bw * Bf :=
            le_mul3 le_rfl hWb hFb (by linarith [h_cast0 i]) (hW i) (hF _)
        _ = (((k - i : ℕ) : K) + 1) * (Bw * Bf) := by ring)
  -- the transport's sum
  have h_sum_u := sum_split (K := K) (k := k) (s := s) h_order
    (t := fun i => U i * (alpha + beta * ((k - i : ℕ) : K)) * F (k - i))
    (a := fun i => U i * (alpha + beta * k) * Bf)
    (b := fun j => Bu * (alpha + beta * j) * F j)
    (c := fun i => Bu * Bf * (alpha + beta * ((k - i : ℕ) : K)))
    (fun i => mul_nonneg (mul_nonneg hBu hBf.le) (by nlinarith [h_cast0 i]))
    (fun i hi his => by
      have hFb : F (k - i) ≤ Bf := (ih (k - i) (by omega) (by omega)).1
      exact le_mul3 le_rfl (by nlinarith [h_cast i]) hFb (hU i) (by nlinarith [h_cast0 i]) (hF _))
    (fun i hi his => by
      have hUb : U i ≤ Bu := (ih i (by omega) hi).2.1
      exact le_mul3 hUb le_rfl le_rfl (hU i) (by nlinarith [h_cast0 i]) (hF _))
    (fun i hi h_low h_high => by
      have hUb : U i ≤ Bu := (ih i h_low hi).2.1
      have hFb : F (k - i) ≤ Bf := (ih (k - i) h_high (by omega)).1
      calc U i * (alpha + beta * ((k - i : ℕ) : K)) * F (k - i)
          ≤ Bu * (alpha + beta * ((k - i : ℕ) : K)) * Bf :=
            le_mul3 hUb le_rfl hFb (hU i) (by nlinarith [h_cast0 i]) (hF _)
        _ = Bu * Bf * (alpha + beta * ((k - i : ℕ) : K)) := by ring)
  -- the sums written out
  set W0 := ∑ i ∈ range s, W i
  set U0 := ∑ i ∈ range s, U i
  set F1 := ∑ j ∈ range s, ((j : K) + 1) * F j
  set Fa := ∑ j ∈ range s, (alpha + beta * j) * F j
  have hW0 : 0 ≤ W0 := sum_nonneg (fun i _ => hW i)
  have hU0 : 0 ≤ U0 := sum_nonneg (fun i _ => hU i)
  have hF1 : 0 ≤ F1 := sum_nonneg (fun j _ => mul_nonneg (by positivity) (hF j))
  have hFa : 0 ≤ Fa := sum_nonneg (fun j _ => mul_nonneg (by positivity) (hF j))
  have ha_w : ∑ i ∈ range s, ((k : K) + 1) * Bf * W i = ((k : K) + 1) * Bf * W0 := by rw [mul_sum]
  have hb_w : ∑ j ∈ range s, ((j : K) + 1) * Bw * F j = Bw * F1 := by
    rw [mul_sum]
    exact sum_congr rfl (fun j _ => by ring)
  have hc_w : ∑ i ∈ range (k + 1), (((k - i : ℕ) : K) + 1) * (Bw * Bf) =
      ((k : K) + 1) * ((k : K) + 2) / 2 * (Bw * Bf) := sum_rest_succ k _
  have ha_u : ∑ i ∈ range s, U i * (alpha + beta * k) * Bf = (alpha + beta * k) * Bf * U0 := by
    rw [mul_sum]
    exact sum_congr rfl (fun i _ => by ring)
  have hb_u : ∑ j ∈ range s, Bu * (alpha + beta * j) * F j = Bu * Fa := by
    rw [mul_sum]
    exact sum_congr rfl (fun j _ => by ring)
  have hc_u : ∑ i ∈ range (k + 1), Bu * Bf * (alpha + beta * ((k - i : ℕ) : K)) =
      Bu * Bf * (((k : K) + 1) * alpha + beta * ((k : K) * ((k : K) + 1) / 2)) := sum_alpha_rest k _ _ _
  rw [ha_w, hb_w, hc_w] at h_sum_w
  rw [ha_u, hb_u, hc_u] at h_sum_u
  have hFk : F k ≤ Bf := (ih k (by omega) le_rfl).1
  -- each part against its term at N, over D = 2 (k+1)(k+2) Bf
  set D := 2 * ((k : K) + 1) * ((k : K) + 2) * Bf with hD
  have hD_positive : 0 < D := by positivity
  have hNN : 0 < 2 * ((N : K) + 1) * ((N : K) + 2) := by positivity
  have hNk2 : ((N : K) + 1) * ((N : K) + 2) ≤ ((k : K) + 1) * ((k : K) + 2) := by nlinarith
  have p1 : (c.angular_a0 + c.a1 * k) * F k ≤ D * ((c.angular_a0 + c.a1 * N) / (2 * ((N : K) + 1) * ((N : K) + 2))) := by
    have fl := falling_two hc.angular_a0 hc.a1 hN hk
    have h_linear : (c.angular_a0 + c.a1 * k) * F k ≤ (c.angular_a0 + c.a1 * k) * Bf :=
      mul_le_mul_of_nonneg_left hFk (by nlinarith [hc.angular_a0, hc.a1])
    rw [← mul_div_assoc, le_div_iff₀ hNN]
    nlinarith [mul_le_mul_of_nonneg_left fl (by linarith : (0 : K) ≤ 2 * Bf)]
  have p2 : ((k : K) + 1) * Bf * W0 ≤ D * (W0 / (2 * ((N : K) + 2))) := by
    rw [← mul_div_assoc, le_div_iff₀ (by positivity)]
    nlinarith [mul_le_mul_of_nonneg_left (by linarith : (N : K) + 2 ≤ (k : K) + 2)
      (by positivity : (0 : K) ≤ 2 * (((k : K) + 1) * Bf * W0))]
  have p3 : Bw * F1 ≤ D * (Bw * F1 / (2 * ((N : K) + 1) * ((N : K) + 2) * Bf)) := by
    rw [← mul_div_assoc, le_div_iff₀ (by positivity)]
    nlinarith [mul_le_mul_of_nonneg_left hNk2 (by positivity : (0 : K) ≤ 2 * Bf * (Bw * F1))]
  have p4 : ((k : K) + 1) * ((k : K) + 2) / 2 * (Bw * Bf) = D * (Bw / 4) := by
    rw [hD]
    ring
  have p5 : (alpha + beta * k) * Bf * U0 ≤ D * (U0 * (alpha + beta * N) / (2 * ((N : K) + 1) * ((N : K) + 2))) := by
    have fl := falling_two h_alpha h_beta hN hk
    rw [← mul_div_assoc, le_div_iff₀ hNN]
    nlinarith [mul_le_mul_of_nonneg_left fl (by positivity : (0 : K) ≤ 2 * Bf * U0)]
  have p6 : Bu * Fa ≤ D * (Bu * Fa / (2 * ((N : K) + 1) * ((N : K) + 2) * Bf)) := by
    rw [← mul_div_assoc, le_div_iff₀ (by positivity)]
    nlinarith [mul_le_mul_of_nonneg_left hNk2 (by positivity : (0 : K) ≤ 2 * Bf * (Bu * Fa))]
  have p7 : Bu * Bf * (((k : K) + 1) * alpha + beta * ((k : K) * ((k : K) + 1) / 2)) ≤
      D * (Bu * (alpha / (2 * ((N : K) + 2)) + beta / 4)) := by
    have h_split : D * (Bu * (alpha / (2 * ((N : K) + 2)) + beta / 4)) =
        2 * ((k : K) + 1) * ((k : K) + 2) * Bf * Bu * alpha / (2 * ((N : K) + 2)) +
          ((k : K) + 1) * ((k : K) + 2) * Bf * Bu * beta / 2 := by
      rw [hD]
      ring
    have q1 : Bu * Bf * (((k : K) + 1) * alpha) ≤
        2 * ((k : K) + 1) * ((k : K) + 2) * Bf * Bu * alpha / (2 * ((N : K) + 2)) := by
      rw [le_div_iff₀ (by positivity)]
      nlinarith [mul_le_mul_of_nonneg_left (by linarith : (N : K) + 2 ≤ (k : K) + 2)
        (by positivity : (0 : K) ≤ 2 * (Bu * Bf * ((k : K) + 1) * alpha))]
    have q2 : Bu * Bf * (beta * ((k : K) * ((k : K) + 1) / 2)) ≤
        ((k : K) + 1) * ((k : K) + 2) * Bf * Bu * beta / 2 := by
      rw [le_div_iff₀ (by positivity)]
      nlinarith [mul_nonneg (mul_nonneg (mul_nonneg hBu hBf.le) h_beta) (by positivity : (0 : K) ≤ (k : K) + 1)]
    rw [h_split]
    nlinarith [q1, q2]
  have hA : D * angular c s N F U W Bf Bu Bw =
      D * ((c.angular_a0 + c.a1 * N) / (2 * ((N : K) + 1) * ((N : K) + 2))) + D * (W0 / (2 * ((N : K) + 2)))
      + D * (Bw * F1 / (2 * ((N : K) + 1) * ((N : K) + 2) * Bf)) + D * (Bw / 4)
      + D * (U0 * (alpha + beta * N) / (2 * ((N : K) + 1) * ((N : K) + 2)))
      + D * (Bu * Fa / (2 * ((N : K) + 1) * ((N : K) + 2) * Bf))
      + D * (Bu * (alpha / (2 * ((N : K) + 2)) + beta / 4)) := by
    unfold angular
    ring
  have hS := h_rule.angular k hk
  have h_sum : (c.angular_a0 + c.a1 * k) * F k
      + ∑ i ∈ range (k + 1), (((k - i : ℕ) : K) + 1) * W i * F (k - i)
      + ∑ i ∈ range (k + 1), U i * (alpha + beta * ((k - i : ℕ) : K)) * F (k - i)
      ≤ D * angular c s N F U W Bf Bu Bw := by
    rw [hA]
    linarith [p1, p2, p3, p4, p5, p6, p7, h_sum_w, h_sum_u]
  have h_rate := hc.rate
  have h_final : 2 * ((k : K) + 1) * ((k : K) + 2) * F (k + 1) ≤ 2 * ((k : K) + 1) * ((k : K) + 2) * Bf := by
    calc 2 * ((k : K) + 1) * ((k : K) + 2) * F (k + 1)
        ≤ c.rate * (D * angular c s N F U W Bf Bu Bw) := le_trans hS (mul_le_mul_of_nonneg_left h_sum h_rate.le)
      _ = D * (c.rate * angular c s N F U W Bf Bu Bw) := by ring
      _ ≤ D * 1 := mul_le_mul_of_nonneg_left h_angular hD_positive.le
      _ = 2 * ((k : K) + 1) * ((k : K) + 2) * Bf := by rw [hD]; ring
  exact le_of_mul_le_mul_left h_final (by positivity)

set_option maxHeartbeats 2000000 in
/-- The axial part carries the bound of U to `k + 1`. -/
lemma step_along (hc : c.Legal) (hsN : 2 * s ≤ N) (hN : 2 ≤ N)
    (hU : ∀ k, 0 ≤ U k) (hW : ∀ k, 0 ≤ W k)
    (h_rule : Holds c N F U P W) (hBu : 0 < Bu) (hBp : 0 ≤ Bp) (hBw : 0 ≤ Bw)
    (h_along : c.rate * along c s N U W Bu Bp Bw ≤ 1)
    {k : ℕ} (hk : N ≤ k) (ih : Below s k F U P W Bf Bu Bp Bw) :
    U (k + 1) ≤ Bu := by
  have h_order : 2 * s ≤ k + 1 := by omega
  have hkN : (N : K) ≤ k := by exact_mod_cast hk
  have hk0 : (0 : K) ≤ k := Nat.cast_nonneg k
  have hN0 : (0 : K) ≤ N := Nat.cast_nonneg N
  have h_cast : ∀ i, ((k - i : ℕ) : K) ≤ k := fun i => by exact_mod_cast Nat.sub_le k i
  have h_cast0 : ∀ i, (0 : K) ≤ ((k - i : ℕ) : K) := fun i => Nat.cast_nonneg _
  set alpha := c.alpha_along
  set beta := c.beta
  have h_alpha : 0 ≤ alpha := hc.alpha_along
  have h_beta : 0 ≤ beta := hc.beta
  have h_sum_w := sum_split (K := K) (k := k) (s := s) h_order
    (t := fun i => ((k - i : ℕ) : K) * W i * U (k - i))
    (a := fun i => (k : K) * Bu * W i)
    (b := fun j => (j : K) * Bw * U j)
    (c := fun i => ((k - i : ℕ) : K) * (Bw * Bu))
    (fun i => mul_nonneg (h_cast0 i) (mul_nonneg hBw hBu.le))
    (fun i hi his => by
      have hUb : U (k - i) ≤ Bu := (ih (k - i) (by omega) (by omega)).2.1
      calc ((k - i : ℕ) : K) * W i * U (k - i) ≤ (k : K) * W i * Bu :=
            le_mul3 (h_cast i) le_rfl hUb (h_cast0 i) (hW i) (hU _)
        _ = (k : K) * Bu * W i := by ring)
    (fun i hi his => by
      have hWb : W i ≤ Bw := (ih i (by omega) hi).2.2.2
      exact le_mul3 le_rfl hWb le_rfl (h_cast0 i) (hW i) (hU _))
    (fun i hi h_low h_high => by
      have hWb : W i ≤ Bw := (ih i h_low hi).2.2.2
      have hUb : U (k - i) ≤ Bu := (ih (k - i) h_high (by omega)).2.1
      calc ((k - i : ℕ) : K) * W i * U (k - i) ≤ ((k - i : ℕ) : K) * Bw * Bu :=
            le_mul3 le_rfl hWb hUb (h_cast0 i) (hW i) (hU _)
        _ = ((k - i : ℕ) : K) * (Bw * Bu) := by ring)
  have h_sum_u := sum_split (K := K) (k := k) (s := s) h_order
    (t := fun i => U i * (alpha + beta * ((k - i : ℕ) : K)) * U (k - i))
    (a := fun i => U i * (alpha + beta * k) * Bu)
    (b := fun j => Bu * (alpha + beta * j) * U j)
    (c := fun i => Bu * Bu * (alpha + beta * ((k - i : ℕ) : K)))
    (fun i => mul_nonneg (mul_nonneg hBu.le hBu.le) (by nlinarith [h_cast0 i]))
    (fun i hi his => by
      have hUb : U (k - i) ≤ Bu := (ih (k - i) (by omega) (by omega)).2.1
      exact le_mul3 le_rfl (by nlinarith [h_cast i]) hUb (hU i) (by nlinarith [h_cast0 i]) (hU _))
    (fun i hi his => by
      have hUb : U i ≤ Bu := (ih i (by omega) hi).2.1
      exact le_mul3 hUb le_rfl le_rfl (hU i) (by nlinarith [h_cast0 i]) (hU _))
    (fun i hi h_low h_high => by
      have hUi : U i ≤ Bu := (ih i h_low hi).2.1
      have hUb : U (k - i) ≤ Bu := (ih (k - i) h_high (by omega)).2.1
      calc U i * (alpha + beta * ((k - i : ℕ) : K)) * U (k - i)
          ≤ Bu * (alpha + beta * ((k - i : ℕ) : K)) * Bu :=
            le_mul3 hUi le_rfl hUb (hU i) (by nlinarith [h_cast0 i]) (hU _)
        _ = Bu * Bu * (alpha + beta * ((k - i : ℕ) : K)) := by ring)
  set W0 := ∑ i ∈ range s, W i
  set U0 := ∑ i ∈ range s, U i
  set U1 := ∑ j ∈ range s, (j : K) * U j
  set Ua := ∑ j ∈ range s, (alpha + beta * j) * U j
  have hW0 : 0 ≤ W0 := sum_nonneg (fun i _ => hW i)
  have hU0 : 0 ≤ U0 := sum_nonneg (fun i _ => hU i)
  have hU1 : 0 ≤ U1 := sum_nonneg (fun j _ => mul_nonneg (by positivity) (hU j))
  have hUa : 0 ≤ Ua := sum_nonneg (fun j _ => mul_nonneg (by positivity) (hU j))
  have ha_w : ∑ i ∈ range s, (k : K) * Bu * W i = (k : K) * Bu * W0 := by rw [mul_sum]
  have hb_w : ∑ j ∈ range s, (j : K) * Bw * U j = Bw * U1 := by
    rw [mul_sum]
    exact sum_congr rfl (fun j _ => by ring)
  have hc_w : ∑ i ∈ range (k + 1), ((k - i : ℕ) : K) * (Bw * Bu) = (k : K) * ((k : K) + 1) / 2 * (Bw * Bu) :=
    sum_rest_mul k _
  have ha_u : ∑ i ∈ range s, U i * (alpha + beta * k) * Bu = (alpha + beta * k) * Bu * U0 := by
    rw [mul_sum]
    exact sum_congr rfl (fun i _ => by ring)
  have hb_u : ∑ j ∈ range s, Bu * (alpha + beta * j) * U j = Bu * Ua := by
    rw [mul_sum]
    exact sum_congr rfl (fun j _ => by ring)
  have hc_u : ∑ i ∈ range (k + 1), Bu * Bu * (alpha + beta * ((k - i : ℕ) : K)) =
      Bu * Bu * (((k : K) + 1) * alpha + beta * ((k : K) * ((k : K) + 1) / 2)) := sum_alpha_rest k _ _ _
  rw [ha_w, hb_w, hc_w] at h_sum_w
  rw [ha_u, hb_u, hc_u] at h_sum_u
  have hUk : U k ≤ Bu := (ih k (by omega) le_rfl).2.1
  have hPk : P k ≤ Bp := (ih k (by omega) le_rfl).2.2.1
  set D := 2 * ((k : K) + 1) ^ 2 * Bu with hD
  have hD_positive : 0 < D := by positivity
  have hNN : 0 < 2 * ((N : K) + 1) ^ 2 := by positivity
  have hNk2 : ((N : K) + 1) ^ 2 ≤ ((k : K) + 1) ^ 2 := by nlinarith
  have q1 : (c.along_a0 + c.a1 * k) * U k ≤ D * ((c.along_a0 + c.a1 * N) / (2 * ((N : K) + 1) ^ 2)) := by
    have fl := falling_square hc.along_a0 hc.a1 (by omega : 1 ≤ N) hk
    have h_linear : (c.along_a0 + c.a1 * k) * U k ≤ (c.along_a0 + c.a1 * k) * Bu :=
      mul_le_mul_of_nonneg_left hUk (by nlinarith [hc.along_a0, hc.a1])
    rw [← mul_div_assoc, le_div_iff₀ hNN]
    nlinarith [mul_le_mul_of_nonneg_left fl (by linarith : (0 : K) ≤ 2 * Bu)]
  have q2 : (k : K) * Bu * W0 ≤ D * (W0 / (2 * ((N : K) + 1))) := by
    rw [← mul_div_assoc, le_div_iff₀ (by positivity)]
    have h_square : (k : K) * ((N : K) + 1) ≤ ((k : K) + 1) ^ 2 := by nlinarith
    nlinarith [mul_le_mul_of_nonneg_left h_square (by positivity : (0 : K) ≤ 2 * (Bu * W0))]
  have q3 : Bw * U1 ≤ D * (Bw * U1 / (2 * ((N : K) + 1) ^ 2 * Bu)) := by
    rw [← mul_div_assoc, le_div_iff₀ (by positivity)]
    nlinarith [mul_le_mul_of_nonneg_left hNk2 (by positivity : (0 : K) ≤ 2 * Bu * (Bw * U1))]
  have q4 : (k : K) * ((k : K) + 1) / 2 * (Bw * Bu) ≤ D * (Bw / 4) := by
    have h_square : (k : K) * ((k : K) + 1) ≤ ((k : K) + 1) ^ 2 := by nlinarith
    rw [hD]
    nlinarith [mul_le_mul_of_nonneg_left h_square (by positivity : (0 : K) ≤ Bw * Bu)]
  have q5 : (alpha + beta * k) * Bu * U0 ≤ D * (U0 * (alpha + beta * N) / (2 * ((N : K) + 1) ^ 2)) := by
    have fl := falling_square h_alpha h_beta (by omega : 1 ≤ N) hk
    rw [← mul_div_assoc, le_div_iff₀ hNN]
    nlinarith [mul_le_mul_of_nonneg_left fl (by positivity : (0 : K) ≤ 2 * Bu * U0)]
  have q6 : Bu * Ua ≤ D * (Ua / (2 * ((N : K) + 1) ^ 2)) := by
    rw [← mul_div_assoc, le_div_iff₀ hNN]
    nlinarith [mul_le_mul_of_nonneg_left hNk2 (by positivity : (0 : K) ≤ 2 * Bu * Ua)]
  have q7 : Bu * Bu * (((k : K) + 1) * alpha + beta * ((k : K) * ((k : K) + 1) / 2)) ≤
      D * (Bu * (alpha / (2 * ((N : K) + 1)) + beta / 4)) := by
    have h_split : D * (Bu * (alpha / (2 * ((N : K) + 1)) + beta / 4)) =
        2 * ((k : K) + 1) ^ 2 * Bu * Bu * alpha / (2 * ((N : K) + 1)) + ((k : K) + 1) ^ 2 * Bu * Bu * beta / 2 := by
      rw [hD]
      ring
    have r1 : Bu * Bu * (((k : K) + 1) * alpha) ≤ 2 * ((k : K) + 1) ^ 2 * Bu * Bu * alpha / (2 * ((N : K) + 1)) := by
      rw [le_div_iff₀ (by positivity)]
      nlinarith [mul_le_mul_of_nonneg_left (by linarith : (N : K) + 1 ≤ (k : K) + 1)
        (by positivity : (0 : K) ≤ 2 * (Bu * Bu * ((k : K) + 1) * alpha))]
    have r2 : Bu * Bu * (beta * ((k : K) * ((k : K) + 1) / 2)) ≤ ((k : K) + 1) ^ 2 * Bu * Bu * beta / 2 := by
      rw [le_div_iff₀ (by positivity)]
      nlinarith [mul_nonneg (mul_nonneg (mul_nonneg hBu.le hBu.le) h_beta) (by positivity : (0 : K) ≤ (k : K) + 1)]
    rw [h_split]
    nlinarith [r1, r2]
  have q8 : (c.alpha_pressure + beta * k) * P k ≤
      D * ((c.alpha_pressure + beta * N) * Bp / (2 * ((N : K) + 1) ^ 2 * Bu)) := by
    have fl := falling_square hc.alpha_pressure h_beta (by omega : 1 ≤ N) hk
    have h_linear : (c.alpha_pressure + beta * k) * P k ≤ (c.alpha_pressure + beta * k) * Bp :=
      mul_le_mul_of_nonneg_left hPk (by nlinarith [hc.alpha_pressure])
    rw [← mul_div_assoc, le_div_iff₀ (by positivity)]
    calc (c.alpha_pressure + beta * k) * P k * (2 * ((N : K) + 1) ^ 2 * Bu)
        ≤ (c.alpha_pressure + beta * k) * Bp * (2 * ((N : K) + 1) ^ 2 * Bu) :=
          mul_le_mul_of_nonneg_right h_linear (by positivity)
      _ = (c.alpha_pressure + beta * k) * ((N : K) + 1) ^ 2 * (2 * Bu * Bp) := by ring
      _ ≤ (c.alpha_pressure + beta * N) * ((k : K) + 1) ^ 2 * (2 * Bu * Bp) :=
          mul_le_mul_of_nonneg_right fl (by positivity)
      _ = D * ((c.alpha_pressure + beta * N) * Bp) := by rw [hD]; ring
  have hA : D * along c s N U W Bu Bp Bw =
      D * ((c.along_a0 + c.a1 * N) / (2 * ((N : K) + 1) ^ 2)) + D * (W0 / (2 * ((N : K) + 1)))
      + D * (Bw * U1 / (2 * ((N : K) + 1) ^ 2 * Bu)) + D * (Bw / 4)
      + D * (U0 * (alpha + beta * N) / (2 * ((N : K) + 1) ^ 2))
      + D * (Ua / (2 * ((N : K) + 1) ^ 2))
      + D * (Bu * (alpha / (2 * ((N : K) + 1)) + beta / 4))
      + D * ((c.alpha_pressure + beta * N) * Bp / (2 * ((N : K) + 1) ^ 2 * Bu)) := by
    unfold along
    ring
  have hS := h_rule.along k hk
  have h_sum : (c.along_a0 + c.a1 * k) * U k
      + ∑ i ∈ range (k + 1), ((k - i : ℕ) : K) * W i * U (k - i)
      + ∑ i ∈ range (k + 1), U i * (alpha + beta * ((k - i : ℕ) : K)) * U (k - i)
      + (c.alpha_pressure + beta * k) * P k
      ≤ D * along c s N U W Bu Bp Bw := by
    rw [hA]
    linarith [q1, q2, q3, q4, q5, q6, q7, q8, h_sum_w, h_sum_u]
  have h_rate := hc.rate
  have h_final : 2 * ((k : K) + 1) ^ 2 * U (k + 1) ≤ 2 * ((k : K) + 1) ^ 2 * Bu := by
    calc 2 * ((k : K) + 1) ^ 2 * U (k + 1)
        ≤ c.rate * (D * along c s N U W Bu Bp Bw) := le_trans hS (mul_le_mul_of_nonneg_left h_sum h_rate.le)
      _ = D * (c.rate * along c s N U W Bu Bp Bw) := by ring
      _ ≤ D * 1 := mul_le_mul_of_nonneg_left h_along hD_positive.le
      _ = 2 * ((k : K) + 1) ^ 2 * Bu := by rw [hD]; ring
  exact le_of_mul_le_mul_left h_final (by positivity)

/-- The pressure carries the bound of P to `k + 1`. -/
lemma step_pressure (hc : c.Legal) (hsN : 2 * s ≤ N) (hF : ∀ k, 0 ≤ F k)
    (h_rule : Holds c N F U P W) (hBf : 0 < Bf)
    (hp : c.rate * c.lambda ^ 2 * (2 * (∑ i ∈ range s, F i) * Bf / ((N : K) + 1) + Bf ^ 2) ≤ Bp)
    {k : ℕ} (hk : N ≤ k) (ih : Below s k F U P W Bf Bu Bp Bw) :
    P (k + 1) ≤ Bp := by
  have h_order : 2 * s ≤ k + 1 := by omega
  have hkN : (N : K) ≤ k := by exact_mod_cast hk
  have h_sum := sum_split (K := K) (k := k) (s := s) h_order
    (t := fun i => F i * F (k - i))
    (a := fun i => F i * Bf)
    (b := fun j => Bf * F j)
    (c := fun _ => Bf * Bf)
    (fun _ => mul_nonneg hBf.le hBf.le)
    (fun i hi his => mul_le_mul_of_nonneg_left (ih (k - i) (by omega) (by omega)).1 (hF i))
    (fun i hi his => mul_le_mul_of_nonneg_right (ih i (by omega) hi).1 (hF _))
    (fun i hi h_low h_high => mul_le_mul (ih i h_low hi).1 (ih (k - i) h_high (by omega)).1 (hF _) hBf.le)
  set F0 := ∑ i ∈ range s, F i
  have hF0 : 0 ≤ F0 := sum_nonneg (fun i _ => hF i)
  have ha : ∑ i ∈ range s, F i * Bf = F0 * Bf := by rw [sum_mul]
  have hb : ∑ j ∈ range s, Bf * F j = Bf * F0 := by rw [mul_sum]
  have h_constant : ∑ _i ∈ range (k + 1), Bf * Bf = ((k : K) + 1) * (Bf * Bf) := by
    rw [sum_const, card_range, nsmul_eq_mul]
    push_cast
    ring
  rw [ha, hb, h_constant] at h_sum
  have h_scale : 0 ≤ c.rate * c.lambda ^ 2 := mul_nonneg hc.rate.le (sq_nonneg _)
  have h_middle : 2 * F0 * Bf ≤ ((k : K) + 1) * (2 * F0 * Bf / ((N : K) + 1)) := by
    rw [mul_div_assoc', le_div_iff₀ (by positivity)]
    nlinarith [mul_le_mul_of_nonneg_left (by linarith : (N : K) + 1 ≤ (k : K) + 1)
      (by positivity : (0 : K) ≤ 2 * F0 * Bf)]
  have h_step := h_rule.pressure k hk
  have h_bound : ((k : K) + 1) * P (k + 1) ≤ ((k : K) + 1) * Bp := by
    calc ((k : K) + 1) * P (k + 1)
        ≤ c.rate * c.lambda ^ 2 * (F0 * Bf + Bf * F0 + ((k : K) + 1) * (Bf * Bf)) :=
          le_trans h_step (mul_le_mul_of_nonneg_left h_sum h_scale)
      _ ≤ c.rate * c.lambda ^ 2 * (((k : K) + 1) * (2 * F0 * Bf / ((N : K) + 1)) + ((k : K) + 1) * (Bf * Bf)) := by
          apply mul_le_mul_of_nonneg_left _ h_scale
          nlinarith [h_middle]
      _ = ((k : K) + 1) * (c.rate * c.lambda ^ 2 * (2 * F0 * Bf / ((N : K) + 1) + Bf ^ 2)) := by ring
      _ ≤ ((k : K) + 1) * Bp := mul_le_mul_of_nonneg_left hp (by positivity)
  exact le_of_mul_le_mul_left h_bound (by positivity)

/-- The inflow carries the bound of v0 to `k + 1` from that of U. -/
lemma step_inflow (hc : c.Legal) (h_rule : Holds c N F U P W)
    (hw : c.alpha_along * Bu ≤ Bw) (hw' : c.beta * Bu ≤ Bw)
    {k : ℕ} (hk : N ≤ k) (hu : U (k + 1) ≤ Bu) :
    W (k + 1) ≤ Bw := by
  have h_step := h_rule.inflow (k + 1) (by omega)
  push_cast at h_step
  have hk0 : (0 : K) ≤ k := Nat.cast_nonneg k
  have h_coefficient : 0 ≤ c.alpha_along + c.beta * ((k : K) + 1) := by nlinarith [hc.alpha_along, hc.beta]
  have h_bound : ((k : K) + 1 + 1) * W (k + 1) ≤ ((k : K) + 1 + 1) * Bw := by
    calc ((k : K) + 1 + 1) * W (k + 1)
        ≤ (c.alpha_along + c.beta * ((k : K) + 1)) * U (k + 1) := h_step
      _ ≤ (c.alpha_along + c.beta * ((k : K) + 1)) * Bu := mul_le_mul_of_nonneg_left hu h_coefficient
      _ = c.alpha_along * Bu + ((k : K) + 1) * (c.beta * Bu) := by ring
      _ ≤ Bw + ((k : K) + 1) * Bw := by nlinarith [hw, hw']
      _ = ((k : K) + 1 + 1) * Bw := by ring
  exact le_of_mul_le_mul_left h_bound (by positivity)

/-- The bounds held on `[s, N]`, the pressure's and the inflow's bounds raised as the rule asks, and the two
inequalities at N carry the bounds of F, U, Pi and v0 to every order past `s`. -/
theorem tail (hc : c.Legal) (hsN : 2 * s ≤ N) (hN : 2 ≤ N)
    (hF : ∀ k, 0 ≤ F k) (hU : ∀ k, 0 ≤ U k) (hW : ∀ k, 0 ≤ W k)
    (h_rule : Holds c N F U P W) (hBf : 0 < Bf) (hBu : 0 < Bu) (hBp : 0 ≤ Bp) (hBw : 0 ≤ Bw)
    (base : Below s N F U P W Bf Bu Bp Bw)
    (hw : c.alpha_along * Bu ≤ Bw) (hw' : c.beta * Bu ≤ Bw)
    (hp : c.rate * c.lambda ^ 2 * (2 * (∑ i ∈ range s, F i) * Bf / ((N : K) + 1) + Bf ^ 2) ≤ Bp)
    (h_angular : c.rate * angular c s N F U W Bf Bu Bw ≤ 1)
    (h_along : c.rate * along c s N U W Bu Bp Bw ≤ 1) :
    ∀ k, s ≤ k → F k ≤ Bf ∧ U k ≤ Bu ∧ P k ≤ Bp ∧ W k ≤ Bw := by
  have key : ∀ n, Below s (N + n) F U P W Bf Bu Bp Bw := by
    intro n
    induction n with
    | zero => simpa using base
    | succ n ih =>
      intro j hj1 hj2
      rcases Nat.lt_or_ge j (N + n + 1) with h | h
      · exact ih j hj1 (by omega)
      · have hj : j = N + n + 1 := by omega
        subst hj
        have hf := step_angular hc hsN hN hF hU hW h_rule hBf hBu.le hBw h_angular (k := N + n) (by omega) ih
        have hu := step_along hc hsN hN hU hW h_rule hBu hBp hBw h_along (k := N + n) (by omega) ih
        have hpp := step_pressure hc hsN hF h_rule hBf hp (k := N + n) (by omega) ih
        have h_inflow := step_inflow hc h_rule hw hw' (k := N + n) (by omega) hu
        exact ⟨hf, hu, hpp, h_inflow⟩
  intro k hk
  exact key k k hk (by omega)

end Steps

end CoreRadius
