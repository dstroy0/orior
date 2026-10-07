-- SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
import CoreRadius.Tail

/-!
# The numbers of one ellipse

The numbers `core_radius.cu` writes for one ellipse: the rule's constants, the split and the order, each field's norm
below the split, and the bounds past it, every one an exact rational. `Checks` is the arithmetic `tail` asks, decided
over ℚ, and `Witness.tail` reads it in any ordered field the norm lives in.
-/

namespace CoreRadius

open Finset

/-- One ellipse's numbers, every one an exact rational. -/
structure Witness where
  rule : Rule ℚ
  split : ℕ
  order : ℕ
  f : List ℚ
  u : List ℚ
  w : List ℚ
  bf : ℚ
  bu : ℚ
  bp : ℚ
  bw : ℚ

variable {K : Type*} [Field K] [LinearOrder K] [IsStrictOrderedRing K]

/-- The rule's constants read in `K`. -/
def Rule.cast (c : Rule ℚ) : Rule K where
  rate := c.rate
  lambda := c.lambda
  angular_a0 := c.angular_a0
  along_a0 := c.along_a0
  a1 := c.a1
  alpha_angular := c.alpha_angular
  alpha_along := c.alpha_along
  alpha_pressure := c.alpha_pressure
  beta := c.beta

/-- The angular part at N grows with each norm below the split. -/
lemma angular_le {c : Rule K} (hc : c.Legal) {s N : ℕ} {F U W F' U' W' : ℕ → K}
    (hF : ∀ i < s, F i ≤ F' i) (hU : ∀ i < s, U i ≤ U' i) (hW : ∀ i < s, W i ≤ W' i)
    {Bf Bu Bw : K} (hBf : 0 < Bf) (hBu : 0 ≤ Bu) (hBw : 0 ≤ Bw) :
    angular c s N F U W Bf Bu Bw ≤ angular c s N F' U' W' Bf Bu Bw := by
  have h_alpha := hc.alpha_angular
  have h_beta := hc.beta
  unfold angular
  gcongr with i hi i hi i hi j hj
  · exact hW i (mem_range.mp hi)
  · exact hF i (mem_range.mp hi)
  · exact hU i (mem_range.mp hi)
  · exact hF j (mem_range.mp hj)

/-- The axial part at N grows with each norm below the split. -/
lemma along_le {c : Rule K} (hc : c.Legal) {s N : ℕ} {U W U' W' : ℕ → K}
    (hU : ∀ i < s, U i ≤ U' i) (hW : ∀ i < s, W i ≤ W' i)
    {Bu Bp Bw : K} (hBu : 0 < Bu) (hBw : 0 ≤ Bw) :
    along c s N U W Bu Bp Bw ≤ along c s N U' W' Bu Bp Bw := by
  have h_alpha := hc.alpha_along
  have h_beta := hc.beta
  unfold along
  gcongr with i hi i hi i hi j hj
  · exact hW i (mem_range.mp hi)
  · exact hU i (mem_range.mp hi)
  · exact hU i (mem_range.mp hi)
  · exact hU j (mem_range.mp hj)

lemma angular_cast (c : Rule ℚ) (s N : ℕ) (f u w : ℕ → ℚ) (bf bu bw : ℚ) :
    angular (Rule.cast c : Rule K) s N (fun i => (f i : K)) (fun i => (u i : K)) (fun i => (w i : K)) bf bu bw
      = ((angular c s N f u w bf bu bw : ℚ) : K) := by
  unfold angular Rule.cast
  push_cast
  rfl

lemma along_cast (c : Rule ℚ) (s N : ℕ) (u w : ℕ → ℚ) (bu bp bw : ℚ) :
    along (Rule.cast c : Rule K) s N (fun i => (u i : K)) (fun i => (w i : K)) bu bp bw
      = ((along c s N u w bu bp bw : ℚ) : K) := by
  unfold along Rule.cast
  push_cast
  rfl

namespace Witness

/-- Every inequality `tail` asks of the numbers, each decided over ℚ. -/
def Checks (x : Witness) : Prop :=
  0 < x.rule.rate ∧ 0 ≤ x.rule.lambda ∧ 0 ≤ x.rule.angular_a0 ∧ 0 ≤ x.rule.along_a0 ∧ 0 ≤ x.rule.a1
  ∧ 0 ≤ x.rule.alpha_angular ∧ 0 ≤ x.rule.alpha_along ∧ 0 ≤ x.rule.alpha_pressure ∧ 0 ≤ x.rule.beta
  ∧ 2 * x.split ≤ x.order ∧ 2 ≤ x.order
  ∧ 0 < x.bf ∧ 0 < x.bu ∧ 0 ≤ x.bp ∧ 0 ≤ x.bw
  ∧ x.rule.alpha_along * x.bu ≤ x.bw ∧ x.rule.beta * x.bu ≤ x.bw
  ∧ x.rule.rate * x.rule.lambda ^ 2
      * (2 * (∑ i ∈ range x.split, x.f.getD i 0) * x.bf / ((x.order : ℚ) + 1) + x.bf ^ 2) ≤ x.bp
  ∧ x.rule.rate * angular x.rule x.split x.order (fun i => x.f.getD i 0) (fun i => x.u.getD i 0)
      (fun i => x.w.getD i 0) x.bf x.bu x.bw ≤ 1
  ∧ x.rule.rate * along x.rule x.split x.order (fun i => x.u.getD i 0) (fun i => x.w.getD i 0)
      x.bu x.bp x.bw ≤ 1

/-- Numbers that pass `Checks` carry the bounds of any norm held by the rule, below the numbers under the split and
below the bounds on `[split, order]`, to every order past the split. -/
theorem tail {x : Witness} (h : x.Checks) {F U P W : ℕ → K}
    (hF : ∀ k, 0 ≤ F k) (hU : ∀ k, 0 ≤ U k) (hW : ∀ k, 0 ≤ W k)
    (h_rule : Holds (Rule.cast x.rule) x.order F U P W)
    (low : ∀ i < x.split, F i ≤ (x.f.getD i 0 : K) ∧ U i ≤ (x.u.getD i 0 : K) ∧ W i ≤ (x.w.getD i 0 : K))
    (base : Below x.split x.order F U P W (x.bf : K) x.bu x.bp x.bw) :
    ∀ k, x.split ≤ k → F k ≤ (x.bf : K) ∧ U k ≤ (x.bu : K) ∧ P k ≤ (x.bp : K) ∧ W k ≤ (x.bw : K) := by
  obtain ⟨h_rate, h_lambda, h_angular_a0, h_along_a0, h_a1, h_alpha_angular, h_alpha_along, h_alpha_pressure,
    h_beta, h_split, h_order, h_bf, h_bu, h_bp, h_bw, h_inflow, h_inflow_beta, h_pressure, h_angular,
    h_along⟩ := h
  have hc : (Rule.cast x.rule : Rule K).Legal :=
    { rate := by simp only [Rule.cast]; exact_mod_cast h_rate
      lambda := by simp only [Rule.cast]; exact_mod_cast h_lambda
      angular_a0 := by simp only [Rule.cast]; exact_mod_cast h_angular_a0
      along_a0 := by simp only [Rule.cast]; exact_mod_cast h_along_a0
      a1 := by simp only [Rule.cast]; exact_mod_cast h_a1
      alpha_angular := by simp only [Rule.cast]; exact_mod_cast h_alpha_angular
      alpha_along := by simp only [Rule.cast]; exact_mod_cast h_alpha_along
      alpha_pressure := by simp only [Rule.cast]; exact_mod_cast h_alpha_pressure
      beta := by simp only [Rule.cast]; exact_mod_cast h_beta }
  have hBf : (0 : K) < x.bf := by exact_mod_cast h_bf
  have hBu : (0 : K) < x.bu := by exact_mod_cast h_bu
  have hBp : (0 : K) ≤ x.bp := by exact_mod_cast h_bp
  have hBw : (0 : K) ≤ x.bw := by exact_mod_cast h_bw
  have h_rate_pos := hc.rate
  have h_low_sum : ∑ i ∈ range x.split, F i ≤ ∑ i ∈ range x.split, ((x.f.getD i 0 : ℚ) : K) :=
    sum_le_sum (fun i hi => (low i (mem_range.mp hi)).1)
  refine CoreRadius.tail hc h_split h_order hF hU hW h_rule hBf hBu hBp hBw base ?_ ?_ ?_ ?_ ?_
  · simp only [Rule.cast]
    exact_mod_cast h_inflow
  · simp only [Rule.cast]
    exact_mod_cast h_inflow_beta
  · have h_lambda_square : (0 : K) ≤ (Rule.cast x.rule : Rule K).rate * (Rule.cast x.rule : Rule K).lambda ^ 2 :=
      mul_nonneg h_rate_pos.le (sq_nonneg _)
    calc (Rule.cast x.rule : Rule K).rate * (Rule.cast x.rule : Rule K).lambda ^ 2
          * (2 * (∑ i ∈ range x.split, F i) * (x.bf : K) / ((x.order : K) + 1) + (x.bf : K) ^ 2)
        ≤ (Rule.cast x.rule : Rule K).rate * (Rule.cast x.rule : Rule K).lambda ^ 2
          * (2 * (∑ i ∈ range x.split, ((x.f.getD i 0 : ℚ) : K)) * (x.bf : K) / ((x.order : K) + 1)
            + (x.bf : K) ^ 2) := by
          gcongr
      _ = ((x.rule.rate * x.rule.lambda ^ 2
          * (2 * (∑ i ∈ range x.split, x.f.getD i 0) * x.bf / ((x.order : ℚ) + 1) + x.bf ^ 2) : ℚ) : K) := by
          simp only [Rule.cast]
          push_cast
          rfl
      _ ≤ (x.bp : K) := by exact_mod_cast h_pressure
  · calc (Rule.cast x.rule : Rule K).rate * angular (Rule.cast x.rule) x.split x.order F U W x.bf x.bu x.bw
        ≤ (Rule.cast x.rule : Rule K).rate * angular (Rule.cast x.rule) x.split x.order
          (fun i => ((x.f.getD i 0 : ℚ) : K)) (fun i => ((x.u.getD i 0 : ℚ) : K))
          (fun i => ((x.w.getD i 0 : ℚ) : K)) x.bf x.bu x.bw :=
          mul_le_mul_of_nonneg_left (angular_le hc (fun i hi => (low i hi).1) (fun i hi => (low i hi).2.1)
            (fun i hi => (low i hi).2.2) hBf hBu.le hBw) h_rate_pos.le
      _ = ((x.rule.rate * angular x.rule x.split x.order (fun i => x.f.getD i 0) (fun i => x.u.getD i 0)
          (fun i => x.w.getD i 0) x.bf x.bu x.bw : ℚ) : K) := by
          rw [angular_cast, Rat.cast_mul]
          rfl
      _ ≤ 1 := by exact_mod_cast h_angular
  · calc (Rule.cast x.rule : Rule K).rate * along (Rule.cast x.rule) x.split x.order U W x.bu x.bp x.bw
        ≤ (Rule.cast x.rule : Rule K).rate * along (Rule.cast x.rule) x.split x.order
          (fun i => ((x.u.getD i 0 : ℚ) : K)) (fun i => ((x.w.getD i 0 : ℚ) : K)) x.bu x.bp x.bw :=
          mul_le_mul_of_nonneg_left (along_le hc (fun i hi => (low i hi).2.1) (fun i hi => (low i hi).2.2)
            hBu hBw) h_rate_pos.le
      _ = ((x.rule.rate * along x.rule x.split x.order (fun i => x.u.getD i 0) (fun i => x.w.getD i 0)
          x.bu x.bp x.bw : ℚ) : K) := by
          rw [along_cast, Rat.cast_mul]
          rfl
      _ ≤ 1 := by exact_mod_cast h_along

end Witness

end CoreRadius
