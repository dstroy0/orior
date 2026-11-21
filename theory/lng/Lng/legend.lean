import Lng.any

/-!
# The canonical chart and the defect legend

A dirty corpus is a list of symbols. Some are symbols of a canonical chart `C`. Some are known defects: typos,
idioms and specific 1:1 mappings, each an entry of a defect legend `L` that names the symbol of `C` it stands for.
Every other symbol is unknown.

A legend holds three laws, and Lean accepts no legend without the proof of each.

* Disjointedness: no defect of `L` is a symbol of `C`. Thus `C ∩ L = ∅`.
* Target closure: each defect of `L` stands for a symbol of `C`.
* One to one: no defect has two entries.

The chart `extend C L` adds the defects to `C`. It is fail-closed, as each chart is.

* `extend`: the chart is sound, and it halts on each symbol outside `C ∪ L`.
* `conservative`: on each symbol of `C`, `extend C L` gives what `C` gives.
* `faithful`: each defect gives what the symbol that its entry names gives, at every width and sign.
* `defect_lifts`: each defect lifts to each symbol of any sound chart with the same anchor.
* `decompose`: each symbol of a corpus is in exactly one of `C`, `L` and the unknown symbols.
* `count`: the three parts of a corpus have as many symbols as the corpus.
* `holes_exact`: `holes` reports each unknown symbol of a corpus with its place. It reports no other symbol.
* `translate_ok`: a corpus translates where it has no hole, and each symbol then resolves to a symbol of `C`.
* `complete`: where a corpus has no hole, each of its symbols gives the relation of its anchor.
-/

namespace Lng.Any

/-- A defect legend over the canonical chart `C`: each entry is a defect and the symbol of `C` it stands for. -/
structure Legend {n : Nat} {s : Bool} (C : Chart n s) where
  entries : List (String × String)
  disjoint : ∀ p ∈ entries, p.1 ∉ C.symbols
  target_closure : ∀ p ∈ entries, p.2 ∈ C.symbols
  one_to_one : (entries.map Prod.fst).Nodup

/-- The defects of a legend. -/
def Legend.defects {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) : List String := L.entries.map Prod.fst

theorem lookup_mem : ∀ (l : List (String × String)) (σ t : String), l.lookup σ = some t → (σ, t) ∈ l
  | [], _, _, h => by simp [List.lookup] at h
  | (k, b) :: rest, σ, t, h => by
    unfold List.lookup at h
    split at h
    · simp_all
    · exact List.mem_cons_of_mem _ (lookup_mem rest σ t h)

theorem lookup_none (l : List (String × String)) (σ : String) (h : σ ∉ l.map Prod.fst) : l.lookup σ = none := by
  cases hl : l.lookup σ with
  | none => rfl
  | some t => exact absurd (List.mem_map.mpr ⟨(σ, t), lookup_mem l σ t hl, rfl⟩) h

theorem lookup_some : ∀ (l : List (String × String)) (σ : String), σ ∈ l.map Prod.fst → ∃ t, l.lookup σ = some t
  | [], _, h => by simp at h
  | (k, b) :: rest, σ, h => by
    unfold List.lookup
    split
    · exact ⟨b, rfl⟩
    · rename_i hk
      have hr : σ ∈ rest.map Prod.fst := by simp_all
      exact lookup_some rest σ hr

theorem lookup_unique : ∀ (l : List (String × String)), (l.map Prod.fst).Nodup → ∀ p ∈ l, l.lookup p.1 = some p.2
  | [], _, p, hp => by simp at hp
  | (k, b) :: rest, hnd, p, hp => by
    simp only [List.map_cons, List.nodup_cons] at hnd
    unfold List.lookup
    rcases List.mem_cons.mp hp with rfl | hp
    · simp
    · have hk : p.1 ≠ k := fun he => hnd.1 (he ▸ List.mem_map.mpr ⟨p, hp, rfl⟩)
      have hb : (p.1 == k) = false := by simpa using hk
      rw [hb]
      exact lookup_unique rest hnd.2 p hp

/-- The symbol of `C` that `σ` stands for: itself where `C` holds it, its entry where `L` holds it, and else itself. -/
def resolve {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) (σ : String) : String :=
  if σ ∈ C.symbols then σ else (L.entries.lookup σ).getD σ

theorem resolve_mem {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) (σ : String)
    (h : σ ∈ C.symbols ∨ σ ∈ L.defects) : resolve L σ ∈ C.symbols := by
  unfold resolve
  split
  · assumption
  · rename_i hn
    obtain ⟨t, ht⟩ := lookup_some L.entries σ (h.resolve_left hn)
    rw [ht]
    exact L.target_closure (σ, t) (lookup_mem L.entries σ t ht)

theorem resolve_unknown {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) (σ : String)
    (h : σ ∉ C.symbols) (hk : σ ∉ L.defects) : resolve L σ = σ := by
  unfold resolve
  rw [ite_eq_right h, lookup_none L.entries σ hk]
  rfl

/-- The canonical chart with the defects of the legend added. -/
def extend {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) : Chart n s where
  symbols := C.symbols ++ L.defects
  anchor := fun σ => C.anchor (resolve L σ)
  domain := fun σ => C.domain (resolve L σ)
  sem := fun σ => C.sem (resolve L σ)
  sound := fun σ hσ x y hd => C.sound _ (resolve_mem L σ (List.mem_append.mp hσ)) x y hd
  closed := by
    intro σ hσ x y
    have h1 : σ ∉ C.symbols := fun h => hσ (List.mem_append_left _ h)
    have h2 : σ ∉ L.defects := fun h => hσ (List.mem_append_right _ h)
    show C.sem (resolve L σ) x y = none
    rw [resolve_unknown L σ h1 h2]
    exact C.closed σ h1 x y

theorem conservative {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) (σ : String) (h : σ ∈ C.symbols) :
    (extend C L).sem σ = C.sem σ ∧ (extend C L).anchor σ = C.anchor σ := by
  simp [extend, resolve, h]

/-- Each defect resolves to the symbol that its entry names. -/
theorem resolve_entry {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) (p : String × String)
    (hp : p ∈ L.entries) : resolve L p.1 = p.2 := by
  unfold resolve
  rw [ite_eq_right (L.disjoint p hp), lookup_unique L.entries L.one_to_one p hp]
  rfl

theorem faithful {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) (p : String × String) (hp : p ∈ L.entries) :
    (extend C L).sem p.1 = C.sem p.2 ∧ (extend C L).anchor p.1 = C.anchor p.2 := by
  simp [extend, resolve_entry L p hp]

theorem defect_lifts {n : Nat} {s : Bool} (C M : Chart n s) (L : Legend C) (p : String × String)
    (hp : p ∈ L.entries) (τ : String) (hτ : τ ∈ M.symbols) (h : C.anchor p.2 = M.anchor τ) (x y : BitVec n)
    (hd : C.domain p.2 y) (hM : M.domain τ y) : (extend C L).sem p.1 x y = M.sem τ x y := by
  have hf := faithful C L p hp
  have hm : p.1 ∈ (extend C L).symbols := List.mem_append_right _ (List.mem_map.mpr ⟨p, hp, rfl⟩)
  have ha : (extend C L).anchor p.1 = M.anchor τ := hf.2.trans h
  have hdom : (extend C L).domain p.1 y := by
    show C.domain (resolve L p.1) y
    rw [resolve_entry L p hp]
    exact hd
  exact lift (extend C L) M p.1 τ hm hτ ha x y hdom hM

/-- The three parts of a symbol: a symbol of the canonical chart, a defect of the legend, and an unknown symbol. -/
inductive Part
  | canonical
  | defect
  | unknown
  deriving DecidableEq

/-- The part of the symbol `σ`. -/
def part {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) (σ : String) : Part :=
  if σ ∈ C.symbols then .canonical else if σ ∈ L.defects then .defect else .unknown

theorem decompose {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) (σ : String) (x y : BitVec n) :
    (part L σ = .canonical ↔ σ ∈ C.symbols) ∧
    (part L σ = .defect ↔ σ ∈ L.defects) ∧
    (part L σ = .unknown ↔ σ ∉ C.symbols ∧ σ ∉ L.defects) ∧
    (part L σ = .unknown → (extend C L).sem σ x y = none) := by
  have hdis : σ ∈ L.defects → σ ∉ C.symbols := by
    intro h
    obtain ⟨p, hp, he⟩ := List.mem_map.mp h
    exact he ▸ L.disjoint p hp
  unfold part
  by_cases h1 : σ ∈ C.symbols
  · have h2 : σ ∉ L.defects := fun h => hdis h h1
    simp [h1, h2]
  · by_cases h2 : σ ∈ L.defects
    · simp [h1, h2]
    · refine ⟨by simp [h1, h2], by simp [h1, h2], by simp [h1, h2], fun _ => ?_⟩
      exact (extend C L).closed σ (fun h => (List.mem_append.mp h).elim h1 h2) x y

theorem count {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) (corpus : List String) :
    (corpus.filter (fun σ => part L σ == .canonical)).length +
    (corpus.filter (fun σ => part L σ == .defect)).length +
    (corpus.filter (fun σ => part L σ == .unknown)).length = corpus.length := by
  induction corpus with
  | nil => rfl
  | cons σ rest ih =>
    simp only [List.filter_cons, List.length_cons]
    cases part L σ <;> simp <;> omega

/-- A hole of a corpus: an unknown symbol and its place, counted from 0. -/
structure Hole where
  place : Nat
  symbol : String
  deriving DecidableEq

/-- The holes of the corpus from the place `i` on, where `known` tells each symbol that the charts cover. -/
def holesFrom (known : String → Bool) : Nat → List String → List Hole
  | _, [] => []
  | i, σ :: rest => (if known σ then [] else [⟨i, σ⟩]) ++ holesFrom known (i + 1) rest

theorem mem_holesFrom (known : String → Bool) :
    ∀ (i : Nat) (l : List String) (p : Nat) (σ : String),
      (⟨p, σ⟩ : Hole) ∈ holesFrom known i l ↔ ∃ k, l[k]? = some σ ∧ p = i + k ∧ known σ = false
  | i, [], p, σ => by simp [holesFrom]
  | i, τ :: rest, p, σ => by
    simp only [holesFrom, List.mem_append, mem_holesFrom known (i + 1) rest p σ]
    constructor
    · rintro (hh | ⟨k, hk, hp, hc⟩)
      · split at hh
        · simp at hh
        · rename_i hkn
          simp only [List.mem_singleton, Hole.mk.injEq] at hh
          obtain ⟨rfl, rfl⟩ := hh
          exact ⟨0, rfl, rfl, by simpa using hkn⟩
      · exact ⟨k + 1, by simpa using hk, by omega, hc⟩
    · rintro ⟨k, hk, hp, hc⟩
      cases k with
      | zero =>
        left
        simp only [List.getElem?_cons_zero, Option.some.injEq] at hk
        subst hk
        simp [hc, hp]
      | succ k =>
        right
        exact ⟨k, by simpa using hk, by omega, hc⟩

/-- Each unknown symbol of `corpus` with its place. -/
def holes {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) (corpus : List String) : List Hole :=
  holesFrom (fun σ => part L σ != .unknown) 0 corpus

theorem holes_exact {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) (corpus : List String) (p : Nat)
    (σ : String) : (⟨p, σ⟩ : Hole) ∈ holes L corpus ↔ corpus[p]? = some σ ∧ σ ∉ C.symbols ∧ σ ∉ L.defects := by
  unfold holes
  rw [mem_holesFrom]
  have hu := (decompose C L σ 0 0).2.2.1
  constructor
  · rintro ⟨k, hk, rfl, hc⟩
    refine ⟨by simpa using hk, hu.mp (by simpa using hc)⟩
  · rintro ⟨hk, h1, h2⟩
    exact ⟨p, hk, by simp, by simpa using hu.mpr ⟨h1, h2⟩⟩

/-- The corpus resolved to symbols of `C`, or each of its holes where it has one. -/
def translate {n : Nat} {s : Bool} {C : Chart n s} (L : Legend C) (corpus : List String) :
    Except (List Hole) (List String) :=
  match holes L corpus with
  | [] => .ok (corpus.map (resolve L))
  | hs => .error hs

theorem translate_ok {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) (corpus : List String) :
    (holes L corpus = [] → translate L corpus = .ok (corpus.map (resolve L))) ∧
    (holes L corpus ≠ [] → translate L corpus = .error (holes L corpus)) := by
  unfold translate
  constructor
  · intro h
    rw [h]
  · intro h
    split
    · contradiction
    · rfl

theorem complete {n : Nat} {s : Bool} (C : Chart n s) (L : Legend C) (corpus : List String)
    (h : holes L corpus = []) : ∀ σ ∈ corpus, resolve L σ ∈ C.symbols ∧
      ∀ x y, (extend C L).domain σ y → (extend C L).sem σ x y = meaning s ((extend C L).anchor σ) x y := by
  intro σ hσ
  obtain ⟨p, hp⟩ := List.getElem?_of_mem hσ
  have hin : σ ∈ C.symbols ∨ σ ∈ L.defects := by
    by_cases h1 : σ ∈ C.symbols
    · exact Or.inl h1
    · by_cases h2 : σ ∈ L.defects
      · exact Or.inr h2
      · have := (holes_exact C L corpus p σ).mpr ⟨hp, h1, h2⟩
        rw [h] at this
        simp at this
  refine ⟨resolve_mem L σ hin, fun x y hd => ?_⟩
  exact (extend C L).sound σ (List.mem_append.mpr hin) x y hd

/-- A legend over C: a typo, an idiom and a specific 1:1 mapping. -/
def cLegend : Legend c where
  entries := [("ad", "add"), ("x = x + y", "add_assign"), ("xor", "bitxor")]
  disjoint := by decide
  target_closure := by decide
  one_to_one := by decide

/-- A dirty corpus: two canonical symbols, three defects and one unknown symbol. -/
def dirty : List String := ["add", "ad", "x = x + y", "shl", "xor", "addd"]

theorem dirty_holes : holes cLegend dirty = [⟨5, "addd"⟩] := by decide

theorem dirty_count :
    (dirty.filter (fun σ => part cLegend σ == .canonical)).length = 2 ∧
    (dirty.filter (fun σ => part cLegend σ == .defect)).length = 3 ∧
    (dirty.filter (fun σ => part cLegend σ == .unknown)).length = 1 := by decide

theorem dirty_translate : translate cLegend dirty = .error [⟨5, "addd"⟩] := by
  unfold translate
  rw [dirty_holes]

theorem idiom (x y : BitVec 32) : (extend c cLegend).sem "x = x + y" x y = python.sem "add_assign" x y :=
  defect_lifts c python cLegend ("x = x + y", "add_assign") (by decide) "add_assign" (by decide) rfl x y
    (fun h => absurd h (by decide)) (fun h => absurd h (by decide))

end Lng.Any
