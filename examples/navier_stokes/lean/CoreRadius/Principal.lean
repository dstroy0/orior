-- SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
import Mathlib

/-!
# The principal tangent's comparison

Along a datum direction e^(mu eta), the paths of the core's tangent that take one derivative of e^(mu eta) at every
step sum, at a point eta and with Y = X / L^2, to

    2 (Y y')' = mu (s y + c - c U' V),   V' = y,   y(0) = V(0) = 0,

y the tangent's axial field and c = L d. With p = Y y' this is y' = p / Y and p' = mu / 2 (s y + c - c U' V). Where
s - c Y max(U', 0) >= s0 > 0, `principal_comparison` puts y above theta z for every theta < 1 and every z of the same
form whose p_z grows no faster than mu / 2 (s0 z + c), and `principal_lower` takes theta to 1. `principal_growth` reads the
polynomials z_K = (c / s0) sum_(k=1..K) (s0 mu Y / 2)^k / (k!)^2, the partial sums of
(c / s0) (I_0(sqrt(2 s0 mu Y)) - 1), as such a z.
-/

namespace CoreRadius

open Set Filter Topology

/-- A right derivative past 0 puts the function above its value just right of the point. -/
lemma eventually_lt_of_right_deriv {q : ℝ → ℝ} {d x : ℝ} (h : HasDerivWithinAt q d (Ici x) x) (hd : 0 < d) :
    ∀ᶠ t in 𝓝[>] x, q x < q t := by
  have h' : HasDerivWithinAt q d (Ioi x) x := h.mono Ioi_subset_Ici_self
  rw [hasDerivWithinAt_iff_tendsto_slope' (show x ∉ Ioi x from lt_irrefl x)] at h'
  filter_upwards [h'.eventually (lt_mem_nhds hd), self_mem_nhdsWithin] with t ht h_x_t
  rw [slope_def_field] at ht
  have h_positive : 0 < t - x := sub_pos.mpr h_x_t
  have := (div_pos_iff_of_pos_right h_positive).mp ht
  linarith

/-- A continuous function at least 0 at 0, which stays at least 0 just right of every point it has been at least 0 up
to, is at least 0 on the whole interval. -/
lemma nonneg_of_step {q : ℝ → ℝ} {T : ℝ} (hq : ContinuousOn q (Icc 0 T)) (h0 : 0 ≤ q 0)
    (step : ∀ x ∈ Ico 0 T, (∀ τ ∈ Icc 0 x, 0 ≤ q τ) → ∀ᶠ t in 𝓝[>] x, 0 ≤ q t) :
    ∀ t ∈ Icc 0 T, 0 ≤ q t := by
  set s : Set ℝ := {t | ∀ τ ∈ Icc 0 t, 0 ≤ q τ} with hs
  have closed : IsClosed (s ∩ Icc 0 T) := by
    apply isClosed_of_closure_subset
    intro t ht
    rcases mem_closure_iff_seq_limit.mp ht with ⟨u, hu, h_limit_of⟩
    have hT : t ∈ Icc 0 T := isClosed_Icc.closure_subset (closure_mono inter_subset_right ht)
    refine ⟨fun τ hτ => ?_, hT⟩
    rcases lt_or_eq_of_le hτ.2 with h_below | h_same
    · obtain ⟨n, hn⟩ := (h_limit_of.eventually (lt_mem_nhds h_below)).exists
      exact (hu n).1 τ ⟨hτ.1, hn.le⟩
    · subst h_same
      have h_limit : Tendsto (fun n => q (u n)) atTop (𝓝 (q τ)) :=
        ((hq τ hT).tendsto).comp (tendsto_nhdsWithin_iff.mpr ⟨h_limit_of, Eventually.of_forall fun n => (hu n).2⟩)
      exact ge_of_tendsto h_limit (Eventually.of_forall fun n => (hu n).1 (u n) ⟨(hu n).2.1, le_rfl⟩)
  have h0s : (0 : ℝ) ∈ s := by
    intro τ hτ
    have : τ = 0 := le_antisymm hτ.2 hτ.1
    rw [this]
    exact h0
  have grow : ∀ x ∈ s ∩ Ico 0 T, s ∈ 𝓝[>] x := by
    intro x hx
    obtain ⟨u, hu, h_ball⟩ := mem_nhdsGT_iff_exists_Ioo_subset.mp (step x hx.2 hx.1)
    rw [mem_nhdsGT_iff_exists_Ioo_subset]
    refine ⟨u, hu, fun t ht τ hτ => ?_⟩
    rcases le_or_gt τ x with hle | h_past
    · exact hx.1 τ ⟨hτ.1, hle⟩
    · exact h_ball ⟨h_past, lt_of_le_of_lt hτ.2 ht.2⟩
  intro t ht
  exact closed.Icc_subset_of_forall_mem_nhdsWithin h0s grow ht t ⟨ht.1, le_rfl⟩

/-- A function continuous on `[0, x]` with a derivative at least 0 on `(0, x)` is at least its value at 0 there. -/
lemma ge_zero_value {f f' : ℝ → ℝ} {x : ℝ} (hx : 0 ≤ x) (hf : ContinuousOn f (Icc 0 x))
    (hd : ∀ t ∈ Ioo 0 x, HasDerivAt f (f' t) t) (hn : ∀ t ∈ Ioo 0 x, 0 ≤ f' t) : f 0 ≤ f x := by
  have mono : MonotoneOn f (Icc 0 x) := by
    apply monotoneOn_of_hasDerivWithinAt_nonneg (f' := f') (convex_Icc 0 x) hf
    · intro t ht
      rw [interior_Icc] at ht
      exact (hd t ht).hasDerivWithinAt
    · intro t ht
      rw [interior_Icc] at ht
      exact hn t ht
  exact mono ⟨le_rfl, hx⟩ ⟨hx, le_rfl⟩ hx

/-- The comparison. With p = Y y', V' = y and p' = mu / 2 (s y + c - c U' V), where s - c Y max(U', 0) >= s0 > 0,
and z, p_z of the same form with p_z' at most mu / 2 (s0 z + c) and p_z at least 0, y stays above theta z on
`[0, T]` for every theta in `[0, 1)`. -/
theorem principal_comparison {T μ c s0 θ : ℝ} {y p V z pz s slope rz : ℝ → ℝ}
    (hμ : 0 < μ) (hc : 0 < c) (hs0 : 0 ≤ s0) (hθ0 : 0 ≤ θ) (hθ : θ < 1)
    (cy : ContinuousOn y (Icc 0 T)) (cV : ContinuousOn V (Icc 0 T)) (cp : ContinuousOn p (Icc 0 T))
    (cz : ContinuousOn z (Icc 0 T)) (c_pz : ContinuousOn pz (Icc 0 T))
    (dy : ∀ t ∈ Ioo 0 T, HasDerivAt y (p t / t) t) (dV : ∀ t ∈ Ioo 0 T, HasDerivAt V (y t) t)
    (dz : ∀ t ∈ Ioo 0 T, HasDerivAt z (pz t / t) t)
    (dp : ∀ t ∈ Ico 0 T, HasDerivWithinAt p (μ / 2 * (s t * y t + c - c * slope t * V t)) (Ici t) t)
    (d_pz : ∀ t ∈ Ico 0 T, HasDerivWithinAt pz (rz t) (Ici t) t)
    (h_rz : ∀ t ∈ Icc 0 T, rz t ≤ μ / 2 * (s0 * z t + c))
    (h_pz0 : ∀ t ∈ Icc 0 T, 0 ≤ pz t)
    (y0 : y 0 = 0) (V0 : V 0 = 0) (p0 : p 0 = 0) (z0 : z 0 = 0) (pz0 : pz 0 = 0)
    (h_least : ∀ t ∈ Icc 0 T, s0 ≤ s t - c * t * max (slope t) 0) :
    ∀ t ∈ Icc 0 T, θ * z t ≤ y t := by
  set q : ℝ → ℝ := fun t => p t - θ * pz t with hq
  -- once q is at least 0 on [0, x]: y and V at least 0, V at most x y, and y at least theta z
  have facts : ∀ x ∈ Icc 0 T, (∀ τ ∈ Icc 0 x, 0 ≤ q τ) →
      0 ≤ y x ∧ 0 ≤ V x ∧ V x ≤ x * y x ∧ θ * z x ≤ y x := by
    intro x hx h_q_x
    have h_inside : Icc 0 x ⊆ Icc 0 T := Icc_subset_Icc_right hx.2
    have inner : ∀ t ∈ Ioo 0 x, t ∈ Ioo 0 T := fun t ht => ⟨ht.1, lt_of_lt_of_le ht.2 hx.2⟩
    have hp : ∀ τ ∈ Icc 0 x, 0 ≤ p τ := by
      intro τ hτ
      have h1 := h_q_x τ hτ
      have h2 := h_pz0 τ (h_inside hτ)
      simp only [hq] at h1
      nlinarith
    have hy : ∀ τ ∈ Icc 0 x, 0 ≤ y τ := by
      intro τ hτ
      have := ge_zero_value (f' := fun t => p t / t) hτ.1 (cy.mono (Icc_subset_Icc_right (le_trans hτ.2 hx.2)))
        (fun t ht => dy t ⟨ht.1, lt_of_lt_of_le ht.2 (le_trans hτ.2 hx.2)⟩)
        (fun t ht => div_nonneg (hp t ⟨ht.1.le, le_trans ht.2.le hτ.2⟩) ht.1.le)
      rwa [y0] at this
    have hV : 0 ≤ V x := by
      have := ge_zero_value (f' := y) hx.1 (cV.mono h_inside) (fun t ht => dV t (inner t ht))
        (fun t ht => hy t (Ioo_subset_Icc_self ht))
      rwa [V0] at this
    have hVy : V x ≤ x * y x := by
      have := ge_zero_value (f := fun t => t * y t - V t) (f' := p) hx.1
        ((continuousOn_id.mul (cy.mono h_inside)).sub (cV.mono h_inside))
        (fun t ht => by
          have h1 : HasDerivAt (fun u => u * y u - V u) (1 * y t + t * (p t / t) - y t) t :=
            ((hasDerivAt_id t).mul (dy t (inner t ht))).sub (dV t (inner t ht))
          have ht0 : t ≠ 0 := ne_of_gt ht.1
          convert h1 using 1
          field_simp
          try ring)
        (fun t ht => hp t (Ioo_subset_Icc_self ht))
      simp only [y0, V0, mul_zero, sub_zero] at this
      linarith
    have hw : θ * z x ≤ y x := by
      have := ge_zero_value (f := fun t => y t - θ * z t) (f' := fun t => (p t - θ * pz t) / t) hx.1
        ((cy.mono h_inside).sub (continuousOn_const.mul (cz.mono h_inside)))
        (fun t ht => by
          have h1 := (dy t (inner t ht)).sub ((dz t (inner t ht)).const_mul θ)
          convert h1 using 1
          ring)
        (fun t ht => div_nonneg (h_q_x t (Ioo_subset_Icc_self ht)) ht.1.le)
      simp only [y0, z0, mul_zero, sub_zero] at this
      linarith
    exact ⟨hy x ⟨hx.1, le_rfl⟩, hV, hVy, hw⟩
  -- the step: at a point q has been at least 0 up to, its right derivative is past 0
  have step : ∀ x ∈ Ico 0 T, (∀ τ ∈ Icc 0 x, 0 ≤ q τ) → ∀ᶠ t in 𝓝[>] x, 0 ≤ q t := by
    intro x hx h_q_x
    have hxT : x ∈ Icc 0 T := Ico_subset_Icc_self hx
    obtain ⟨hy, hV, hVy, hw⟩ := facts x hxT h_q_x
    have hd : HasDerivWithinAt q (μ / 2 * (s x * y x + c - c * slope x * V x) - θ * rz x) (Ici x) x :=
      (dp x hx).sub ((d_pz x hx).const_mul θ)
    have he := h_least x hxT
    have hr := h_rz x hxT
    have hm : slope x * V x ≤ max (slope x) 0 * (x * y x) := by
      have h1 : slope x * V x ≤ max (slope x) 0 * V x := mul_le_mul_of_nonneg_right (le_max_left _ _) hV
      have h2 : max (slope x) 0 * V x ≤ max (slope x) 0 * (x * y x) :=
        mul_le_mul_of_nonneg_left hVy (le_max_right _ _)
      linarith
    have key : s0 * y x ≤ s x * y x - c * (slope x * V x) := by
      have h3 : s0 * y x ≤ (s x - c * x * max (slope x) 0) * y x := mul_le_mul_of_nonneg_right he hy
      have h4 : c * (slope x * V x) ≤ c * (max (slope x) 0 * (x * y x)) := mul_le_mul_of_nonneg_left hm hc.le
      nlinarith
    have hθr : θ * rz x ≤ θ * (μ / 2 * (s0 * z x + c)) := mul_le_mul_of_nonneg_left hr hθ0
    have h_positive : 0 < μ / 2 * (s x * y x + c - c * slope x * V x) - θ * rz x := by
      have h5 : 0 < μ / 2 * ((1 - θ) * c) := by
        have : 0 < (1 - θ) * c := mul_pos (by linarith) hc
        positivity
      have h6 : 0 ≤ μ / 2 * (s0 * (y x - θ * z x)) := by
        have : 0 ≤ s0 * (y x - θ * z x) := by nlinarith [hw]
        positivity
      nlinarith
    have hq0 : 0 ≤ q x := h_q_x x ⟨hx.1, le_rfl⟩
    filter_upwards [eventually_lt_of_right_deriv hd h_positive] with t ht
    linarith
  have h_q_continuous : ContinuousOn q (Icc 0 T) := cp.sub (continuousOn_const.mul c_pz)
  have hq0 : 0 ≤ q 0 := by simp [hq, p0, pz0]
  have all := nonneg_of_step h_q_continuous hq0 step
  intro t ht
  exact (facts t ht (fun τ hτ => all τ ⟨hτ.1, le_trans hτ.2 ht.2⟩)).2.2.2

/-- theta taken to 1: y stays above z on `[0, T]`. -/
theorem principal_lower {T μ c s0 : ℝ} {y p V z pz s slope rz : ℝ → ℝ}
    (hμ : 0 < μ) (hc : 0 < c) (hs0 : 0 ≤ s0)
    (cy : ContinuousOn y (Icc 0 T)) (cV : ContinuousOn V (Icc 0 T)) (cp : ContinuousOn p (Icc 0 T))
    (cz : ContinuousOn z (Icc 0 T)) (c_pz : ContinuousOn pz (Icc 0 T))
    (dy : ∀ t ∈ Ioo 0 T, HasDerivAt y (p t / t) t) (dV : ∀ t ∈ Ioo 0 T, HasDerivAt V (y t) t)
    (dz : ∀ t ∈ Ioo 0 T, HasDerivAt z (pz t / t) t)
    (dp : ∀ t ∈ Ico 0 T, HasDerivWithinAt p (μ / 2 * (s t * y t + c - c * slope t * V t)) (Ici t) t)
    (d_pz : ∀ t ∈ Ico 0 T, HasDerivWithinAt pz (rz t) (Ici t) t)
    (h_rz : ∀ t ∈ Icc 0 T, rz t ≤ μ / 2 * (s0 * z t + c))
    (h_pz0 : ∀ t ∈ Icc 0 T, 0 ≤ pz t)
    (y0 : y 0 = 0) (V0 : V 0 = 0) (p0 : p 0 = 0) (z0 : z 0 = 0) (pz0 : pz 0 = 0)
    (h_least : ∀ t ∈ Icc 0 T, s0 ≤ s t - c * t * max (slope t) 0) :
    ∀ t ∈ Icc 0 T, z t ≤ y t := by
  intro t ht
  by_contra h_below
  push Not at h_below
  have h0 := principal_comparison (θ := 0) hμ hc hs0 le_rfl zero_lt_one cy cV cp cz c_pz dy dV dz dp d_pz h_rz h_pz0
    y0 V0 p0 z0 pz0 h_least t ht
  have hz : 0 < z t := by linarith
  have hθ1 : (y t + z t) / (2 * z t) < 1 := by
    rw [div_lt_one (by positivity)]
    linarith
  have hθ0 : 0 ≤ (y t + z t) / (2 * z t) := by
    apply div_nonneg <;> linarith
  have h1 := principal_comparison (θ := (y t + z t) / (2 * z t)) hμ hc hs0 hθ0 hθ1 cy cV cp cz c_pz dy dV dz dp d_pz
    h_rz h_pz0 y0 V0 p0 z0 pz0 h_least t ht
  have h2 : (y t + z t) / (2 * z t) * z t = (y t + z t) / 2 := by
    field_simp
  linarith

/-- The weight of Y^k in (I_0(sqrt(2 s0 mu Y)) - 1): (s0 mu / 2)^k / (k!)^2. -/
noncomputable def subPower (s0 μ : ℝ) (k : ℕ) : ℝ := (s0 * μ / 2) ^ k / ((k.factorial : ℝ) ^ 2)

lemma subPower_succ (s0 μ : ℝ) (k : ℕ) : ((k : ℝ) + 1) ^ 2 * subPower s0 μ (k + 1) = s0 * μ / 2 * subPower s0 μ k := by
  unfold subPower
  rw [Nat.factorial_succ]
  push_cast
  have hk : ((k.factorial : ℝ)) ≠ 0 := by positivity
  field_simp
  ring

/-- The partial sum z_K = (c / s0) sum_(k=1..K) subPower k Y^k. -/
noncomputable def lowZ (c s0 μ : ℝ) (K : ℕ) (t : ℝ) : ℝ :=
  c / s0 * ∑ k ∈ Finset.range K, subPower s0 μ (k + 1) * t ^ (k + 1)

/-- Y z_K'. -/
noncomputable def lowP (c s0 μ : ℝ) (K : ℕ) (t : ℝ) : ℝ :=
  c / s0 * ∑ k ∈ Finset.range K, ((k : ℝ) + 1) * subPower s0 μ (k + 1) * t ^ (k + 1)

/-- (Y z_K')'. -/
noncomputable def lowR (c s0 μ : ℝ) (K : ℕ) (t : ℝ) : ℝ :=
  c / s0 * ∑ k ∈ Finset.range K, ((k : ℝ) + 1) ^ 2 * subPower s0 μ (k + 1) * t ^ k

lemma lowZ_deriv (c s0 μ : ℝ) (K : ℕ) (t : ℝ) :
    HasDerivAt (lowZ c s0 μ K) (c / s0 * ∑ k ∈ Finset.range K, subPower s0 μ (k + 1) * (((k : ℝ) + 1) * t ^ k)) t := by
  unfold lowZ
  apply HasDerivAt.const_mul
  apply HasDerivAt.fun_sum
  intro k _
  have := (hasDerivAt_pow (k + 1) t).const_mul (subPower s0 μ (k + 1))
  convert this using 1
  push_cast
  ring

lemma lowP_deriv (c s0 μ : ℝ) (K : ℕ) (t : ℝ) : HasDerivAt (lowP c s0 μ K) (lowR c s0 μ K t) t := by
  unfold lowP lowR
  apply HasDerivAt.const_mul
  apply HasDerivAt.fun_sum
  intro k _
  have := (hasDerivAt_pow (k + 1) t).const_mul (((k : ℝ) + 1) * subPower s0 μ (k + 1))
  convert this using 1
  push_cast
  ring

lemma lowZ_slope (c s0 μ : ℝ) (K : ℕ) {t : ℝ} (ht : t ≠ 0) :
    c / s0 * ∑ k ∈ Finset.range K, subPower s0 μ (k + 1) * (((k : ℝ) + 1) * t ^ k) = lowP c s0 μ K t / t := by
  unfold lowP
  rw [mul_div_assoc, Finset.sum_div]
  congr 1
  apply Finset.sum_congr rfl
  intro k _
  rw [pow_succ]
  field_simp

lemma lowR_le (c s0 μ : ℝ) (K : ℕ) (hc : 0 ≤ c) (hs0 : 0 < s0) (hμ : 0 ≤ μ) {t : ℝ} (ht : 0 ≤ t) :
    lowR c s0 μ K t ≤ μ / 2 * (s0 * lowZ c s0 μ K t + c) := by
  have hs : s0 ≠ 0 := ne_of_gt hs0
  have hR : lowR c s0 μ K t = μ / 2 * c * ∑ k ∈ Finset.range K, subPower s0 μ k * t ^ k := by
    unfold lowR
    rw [Finset.mul_sum, Finset.mul_sum]
    apply Finset.sum_congr rfl
    intro k _
    have := subPower_succ s0 μ k
    field_simp
    linear_combination (2 * c * t ^ k) * this
  have hZ : s0 * lowZ c s0 μ K t + c = c * ∑ k ∈ Finset.range (K + 1), subPower s0 μ k * t ^ k := by
    unfold lowZ
    rw [Finset.sum_range_succ']
    have h0 : subPower s0 μ 0 = 1 := by simp [subPower]
    rw [h0]
    field_simp
    try ring
  have h_last : 0 ≤ subPower s0 μ K * t ^ K := by
    unfold subPower
    have : 0 ≤ s0 * μ / 2 := by positivity
    positivity
  rw [hR, hZ, Finset.sum_range_succ]
  have h_scale : 0 ≤ μ / 2 * c := by positivity
  nlinarith [mul_nonneg h_scale h_last]

lemma lowP_nonneg (c s0 μ : ℝ) (K : ℕ) (hc : 0 ≤ c) (hs0 : 0 < s0) (hμ : 0 ≤ μ) {t : ℝ} (ht : 0 ≤ t) :
    0 ≤ lowP c s0 μ K t := by
  unfold lowP
  apply mul_nonneg (div_nonneg hc hs0.le)
  apply Finset.sum_nonneg
  intro k _
  unfold subPower
  have : 0 ≤ s0 * μ / 2 := by positivity
  positivity

/-- The principal tangent's axial field is at least (c / s0) sum_(k=1..K) (s0 mu Y / 2)^k / (k!)^2 on `[0, T]` for
every K: the partial sums of (c / s0) (I_0(sqrt(2 s0 mu Y)) - 1). -/
theorem principal_growth {T μ c s0 : ℝ} {y p V s slope : ℝ → ℝ}
    (hμ : 0 < μ) (hc : 0 < c) (hs0 : 0 < s0)
    (cy : ContinuousOn y (Icc 0 T)) (cV : ContinuousOn V (Icc 0 T)) (cp : ContinuousOn p (Icc 0 T))
    (dy : ∀ t ∈ Ioo 0 T, HasDerivAt y (p t / t) t) (dV : ∀ t ∈ Ioo 0 T, HasDerivAt V (y t) t)
    (dp : ∀ t ∈ Ico 0 T, HasDerivWithinAt p (μ / 2 * (s t * y t + c - c * slope t * V t)) (Ici t) t)
    (y0 : y 0 = 0) (V0 : V 0 = 0) (p0 : p 0 = 0)
    (h_least : ∀ t ∈ Icc 0 T, s0 ≤ s t - c * t * max (slope t) 0) (K : ℕ) :
    ∀ t ∈ Icc 0 T, lowZ c s0 μ K t ≤ y t := by
  refine principal_lower (pz := lowP c s0 μ K) (rz := lowR c s0 μ K) hμ hc hs0.le cy cV cp
    (fun t _ => (lowZ_deriv c s0 μ K t).continuousAt.continuousWithinAt)
    (fun t _ => (lowP_deriv c s0 μ K t).continuousAt.continuousWithinAt)
    dy dV ?_ dp (fun t _ => (lowP_deriv c s0 μ K t).hasDerivWithinAt) ?_ ?_ y0 V0 p0 ?_ ?_ h_least
  · intro t ht
    have := lowZ_deriv c s0 μ K t
    rwa [lowZ_slope c s0 μ K (ne_of_gt ht.1)] at this
  · intro t ht
    exact lowR_le c s0 μ K hc.le hs0 hμ.le ht.1
  · intro t ht
    exact lowP_nonneg c s0 μ K hc.le hs0 hμ.le ht.1
  · simp [lowZ]
  · simp [lowP]

end CoreRadius
