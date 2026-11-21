import LngLift
import Lng.charts
import Lng.any

/-!
# The proof of the lift that lng_lift runs

`LngLift.lift` is the code of `lng_lift`. Lean compiles it to C, and the C wrapper in src/lng/lng_lift.c passes it
the columns of the tables and returns its answer. This file proves what that code computes.

* `side_ok`: a side that answers gives the place of a witnessed row of its table that bridges to the bridge word.
* `lift_ok`: a lift that answers has a transpiler word for the bridge word, an anchor or a thing, and a witnessed row
  on each side. The place it gives is the place of the target's row.
* `lift_unknown`: a bridge word that the transpiler lacks gets exactly one refusal, and it names the word.
* `tables_lift`: on the tables of the tree, the lift from C to Python answers for each of the 30 shared bridge
  words, and the row it gives bridges to the same word.
* `tables_meaning`: each such lift keeps the meaning, on every pair of words of the domain.
* `tables_typo`: on the tables of the tree, the typo `ad` gets exactly one refusal.
-/

namespace Lng.LiftCode

open LngLift

theorem firstIndex_some {α : Type} [Inhabited α] (p : α → Bool) :
    ∀ (l : List α) (i : Nat), firstIndex p l = some i → i < l.length ∧ p (l.getD i default) = true
  | [], i, h => by simp [firstIndex] at h
  | a :: rest, i, h => by
    unfold firstIndex at h
    split at h
    · rename_i ha
      simp only [Option.some.injEq] at h
      subst h
      simp [ha]
    · cases hr : firstIndex p rest with
      | none => simp [hr] at h
      | some k =>
        simp only [hr, Option.map_some, Option.some.injEq] at h
        subst h
        obtain ⟨h1, h2⟩ := firstIndex_some p rest k hr
        exact ⟨by simp; omega, by simpa using h2⟩

theorem firstIndex_none {α : Type} (p : α → Bool) :
    ∀ (l : List α), (∀ x ∈ l, p x = false) → firstIndex p l = none
  | [], _ => rfl
  | a :: rest, h => by
    unfold firstIndex
    rw [h a (List.mem_cons_self ..)]
    simp [firstIndex_none p rest (fun x hx => h x (List.mem_cons_of_mem _ hx))]

theorem side_ok (table : List Side) (name bridge : String) (i : Nat) (h : side table name bridge = .ok i) :
    i < table.length ∧ (table.getD i default).transpiler = bridge ∧ (table.getD i default).witness ≠ "" := by
  unfold side at h
  split at h
  · simp at h
  · rename_i j hj
    split at h
    · simp at h
    · rename_i hw
      injection h with h
      subst h
      obtain ⟨h1, h2⟩ := firstIndex_some _ table j hj
      exact ⟨h1, by simpa using h2, by simpa using hw⟩

theorem lift_ok (transpiler : List Crossing) (source target : List Side) (sourceName targetName bridge : String)
    (j : Nat) (h : lift transpiler source target sourceName targetName bridge = .ok j) :
    (∃ i, firstIndex (fun c => c.target == "" && c.word == bridge) transpiler = some i ∧
      ((transpiler.getD i default).type = "thing" ∨ (transpiler.getD i default).anchor ≠ "")) ∧
    (∃ k, side source sourceName bridge = .ok k) ∧ side target targetName bridge = .ok j := by
  unfold lift at h
  split at h
  · simp at h
  · rename_i i hi
    split at h
    · simp at h
    · rename_i hcond
      have hc : (transpiler.getD i default).type = "thing" ∨ (transpiler.getD i default).anchor ≠ "" := by
        by_cases ht : (transpiler.getD i default).type = "thing"
        · exact Or.inl ht
        · exact Or.inr (fun ha => hcond (by rw [Bool.and_eq_true]; exact ⟨by simpa using ht, by simpa using ha⟩))
      split at h
      · rename_i k j' hs hu
        injection h with h
        subst h
        exact ⟨⟨i, hi, hc⟩, ⟨k, hs⟩, hu⟩
      all_goals simp at h

theorem lift_unknown (transpiler : List Crossing) (source target : List Side) (sourceName targetName bridge : String)
    (h : ∀ c ∈ transpiler, (c.target == "" && c.word == bridge) = false) :
    lift transpiler source target sourceName targetName bridge =
      .error [s!"the transpiler has no word {bridge}, and no lift goes through it"] := by
  unfold lift
  rw [firstIndex_none _ transpiler h]

/-- The rows of the target table `name`, as the lift reads them. -/
def sidesOf (name : String) : List Side :=
  (Lng.Charts.charts.filter (fun r => r.target == name)).map (fun r => ⟨r.bridge, r.symbol, r.witness⟩)

/-- The place that the lift of `bridge` from C to Python gives, or none where it refuses. -/
def placeOf (bridge : String) : Option Nat :=
  match lift Lng.Charts.crossings (sidesOf "c") (sidesOf "python") "c" "python" bridge with
  | .ok j => some j
  | .error _ => none

/-- The refusals of the lift of `bridge` from C to Python. -/
def refusalsOf (bridge : String) : List String :=
  match lift Lng.Charts.crossings (sidesOf "c") (sidesOf "python") "c" "python" bridge with
  | .ok _ => []
  | .error r => r

set_option maxRecDepth 100000 in
theorem tables_lift : Lng.Any.shared.all (fun b => match placeOf b with
    | some j => ((sidesOf "python").getD j default).transpiler == b
    | none => false) = true := by decide

theorem tables_meaning (b : String) (hb : b ∈ Lng.Any.shared) (j : Nat)
    (h : lift Lng.Charts.crossings (sidesOf "c") (sidesOf "python") "c" "python" b = .ok j)
    (x y : Lng.Ladder.Word) (hd : Lng.Any.domain b y) :
    ((sidesOf "python").getD j default).transpiler = b ∧
    Lng.Any.c.sem b x y = Lng.Any.python.sem ((sidesOf "python").getD j default).transpiler x y := by
  have hr := (side_ok _ _ _ j (lift_ok _ _ _ _ _ _ j h).2.2).2.1
  refine ⟨hr, ?_⟩
  rw [hr]
  exact Lng.Any.lift Lng.Any.c Lng.Any.python b b hb hb rfl x y hd hd

set_option maxRecDepth 100000 in
theorem tables_typo : refusalsOf "ad" = ["the transpiler has no word ad, and no lift goes through it"] := by decide

end Lng.LiftCode
