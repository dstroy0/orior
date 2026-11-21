import Lng.relation
import Lng.atlas
import Lng.g_lng
import Lng.c_lng
import Lng.python_lng

/-!
# Any language to any language

A chart is a language of the atlas. It gives each of its symbols an anchor and a semantics on two words of `n`
places, read signed or unsigned. The chart is sound where the semantics of each symbol is the relation of its
anchor, inside the domain where the language gives the symbol a value. The chart is fail-closed: it gives no value to
a symbol outside it. Lean accepts no chart without the proof of both laws.

* `halt`: a symbol outside a chart has no value on any pair of words.
* `no_lift`: a symbol outside a chart lifts to no symbol that has a value.
* `clean_or_halt`: each symbol of a corpus is a symbol of the chart, or it halts.
* `typo`: a typo of add halts in .g, in C and in Python.

* `lift`: two symbols of two sound charts with the same anchor agree on each pair of words of both domains. The
  theorem holds at every width and sign, for any two charts, and for two symbols with different bridge words.
* `lift_all`: each pair of symbols in a list of sound charts that reach one anchor agree. A list can hold any
  number of charts and any number of symbols of each chart.
* `g`: .g is a sound chart at every width and sign.
* `c` and `python`: C and Python are sound charts on `uint32_t`, the type of their tables.
* `charted`: each bridge word of the C and Python charts here is a relation of both target tables.
* `many_to_many`: the .g operator `+` and the two symbols of add in C and in Python agree on every pair of words.
-/

namespace Lng.Any

open Lng.Ladder Lng.Relation

/-- The anchor of the bridge word `b`, from the anchor column of the transpiler table. -/
def anchorOf (b : String) : Option String := (Lng.GRows.anchors.find? (fun p => p.1 == b)).map (·.2)

/-- The relation of the anchor `a` on the words `x` and `y` of `n` places, read signed where `signed`. -/
def meaning {n : Nat} (signed : Bool) (a : Option String) (x y : BitVec n) : Option (BitVec n) :=
  a.bind (fun an => relation signed an x y)

/-- A language of the atlas at `n` places, read signed where `signed`. -/
structure Chart (n : Nat) (signed : Bool) where
  symbols : List String
  anchor : String → Option String
  domain : String → BitVec n → Prop
  sem : String → BitVec n → BitVec n → Option (BitVec n)
  sound : ∀ σ ∈ symbols, ∀ x y, domain σ y → sem σ x y = meaning signed (anchor σ) x y
  closed : ∀ σ, σ ∉ symbols → ∀ x y, sem σ x y = none

/-- A symbol outside a chart halts: the chart gives it no value on any pair of words. -/
theorem halt {n : Nat} {s : Bool} (L : Chart n s) (σ : String) (h : σ ∉ L.symbols) (x y : BitVec n) :
    L.sem σ x y = none :=
  L.closed σ h x y

/-- A symbol outside a chart lifts to no symbol that has a value. -/
theorem no_lift {n : Nat} {s : Bool} (L M : Chart n s) (σ τ : String) (h : σ ∉ L.symbols) (x y v : BitVec n)
    (hv : M.sem τ x y = some v) : L.sem σ x y ≠ M.sem τ x y := by
  rw [L.closed σ h x y, hv]
  simp

/-- Under the clean-source assumption each symbol of `corpus` is a symbol of `L`. Each other symbol halts. -/
theorem clean_or_halt {n : Nat} {s : Bool} (L : Chart n s) (corpus : List String) (x y : BitVec n) :
    ∀ σ ∈ corpus, σ ∈ L.symbols ∨ L.sem σ x y = none := by
  intro σ _
  by_cases h : σ ∈ L.symbols
  · exact Or.inl h
  · exact Or.inr (L.closed σ h x y)

/-- Two symbols of two sound charts with the same anchor agree on each pair of words of both domains. -/
theorem lift {n : Nat} {s : Bool} (L M : Chart n s) (σ τ : String) (hσ : σ ∈ L.symbols) (hτ : τ ∈ M.symbols)
    (h : L.anchor σ = M.anchor τ) (x y : BitVec n) (hL : L.domain σ y) (hM : M.domain τ y) :
    L.sem σ x y = M.sem τ x y := by
  rw [L.sound σ hσ x y hL, M.sound τ hτ x y hM, h]

/-- A symbol of a chart, with the proof that the chart holds it. -/
structure Point (n : Nat) (s : Bool) where
  chart : Chart n s
  symbol : String
  mem : symbol ∈ chart.symbols

/-- Each pair of points that reach the anchor `a` agree on each pair of words of both domains. -/
theorem lift_all {n : Nat} {s : Bool} (points : List (Point n s)) (a : Option String)
    (h : ∀ p ∈ points, p.chart.anchor p.symbol = a) (x y : BitVec n) :
    ∀ p ∈ points, ∀ q ∈ points, p.chart.domain p.symbol y → q.chart.domain q.symbol y →
      p.chart.sem p.symbol x y = q.chart.sem q.symbol x y := by
  intro p hp q hq hdp hdq
  exact lift p.chart q.chart p.symbol q.symbol p.mem q.mem ((h p hp).trans (h q hq).symm) x y hdp hdq

/-- .g at `n` places, read signed where `s`: each operator compiles to the relation of its anchor. -/
def g (n : Nat) (s : Bool) : Chart n s where
  symbols := Lng.g.symbols
  anchor := Lng.g.anchorOf
  domain := fun _ _ => True
  sem := fun σ x y => Lng.g.compile s σ x y
  sound := fun _ _ _ _ _ => rfl
  closed := by
    intro σ hσ x y
    have hc : Lng.g.coreOp σ = none := by
      unfold Lng.g.coreOp
      apply List.find?_eq_none.mpr
      intro r hr hp
      apply hσ
      simp only [Bool.and_eq_true] at hp
      obtain ⟨h1, h2⟩ := hp
      simp only [Lng.g.symbols, List.mem_flatMap, List.mem_filter]
      exact ⟨r, ⟨hr, h1⟩, by simpa using h2⟩
    simp [Lng.g.compile, Lng.g.anchorOf, hc]

/-- The 30 bridge words that C and Python both chart. -/
def shared : List String :=
  ["add", "sub", "mul", "div", "rem", "bitand", "bitor", "bitxor", "neg", "not", "set", "shl", "shr", "lt", "le",
   "gt", "ge", "eq", "ne", "unless", "add_assign", "sub_assign", "mul_assign", "div_assign", "rem_assign",
   "bitand_assign", "bitor_assign", "bitxor_assign", "shl_assign", "shr_assign"]

theorem charted : shared.all (fun b => ["c", "python"].all (fun t =>
    (Lng.Atlas.relations t).any (fun r => r.bridge == b))) = true := by decide

/-- The domain of the bridge word `b` on `uint32_t`: a shift takes a count below 32. A quotient and a remainder by
0 have no value in the language or in the relation, and so they need no bound. -/
def domain (b : String) (y : Word) : Prop :=
  ["shl", "shr", "shl_assign", "shr_assign"].contains b = true → y.toNat < 32

/-- What C gives each bridge word on two words of `uint32_t`, as C11 states it. -/
def cSem (b : String) (x y : Word) : Option Word :=
  match b with
  | "add" | "add_assign" => some (x + y)
  | "sub" | "sub_assign" => some (x - y)
  | "mul" | "mul_assign" => some (x * y)
  | "div" | "div_assign" => if y = 0 then none else some (x / y)
  | "rem" | "rem_assign" => if y = 0 then none else some (x % y)
  | "bitand" | "bitand_assign" => some (x &&& y)
  | "bitor" | "bitor_assign" => some (x ||| y)
  | "bitxor" | "bitxor_assign" => some (x ^^^ y)
  | "neg" => some (-x)
  | "not" => some (~~~x)
  | "set" => some x
  | "shl" | "shl_assign" => some (x <<< y.toNat)
  | "shr" | "shr_assign" => some (x >>> y.toNat)
  | "lt" => some (reads 1 (Lng.c.flag (x.ult y)))
  | "le" => some (reads 1 (Lng.c.flag (x.ule y)))
  | "gt" => some (reads 1 (Lng.c.flag (y.ult x)))
  | "ge" => some (reads 1 (Lng.c.flag (y.ule x)))
  | "eq" => some (reads 1 (Lng.c.flag (x == y)))
  | "ne" => some (reads 1 (Lng.c.flag (x != y)))
  | "unless" => some (reads 1 (Lng.c.flag (x == 0)))
  | _ => none

/-- What Python gives each bridge word on two words, through the mask of its table. -/
def pySem (b : String) (x y : Word) : Option Word :=
  match b with
  | "add" | "add_assign" => some (Lng.python.mask (Lng.python.int x + Lng.python.int y))
  | "sub" | "sub_assign" => some (Lng.python.mask (Lng.python.int x - Lng.python.int y))
  | "mul" | "mul_assign" => some (Lng.python.mask (Lng.python.int x * Lng.python.int y))
  | "div" | "div_assign" =>
    if y = 0 then none else some (Lng.python.mask (Int.fdiv (Lng.python.int x) (Lng.python.int y)))
  | "rem" | "rem_assign" =>
    if y = 0 then none else some (Lng.python.mask (Int.fmod (Lng.python.int x) (Lng.python.int y)))
  | "bitand" | "bitand_assign" => some (Lng.python.mask ((x.toNat &&& y.toNat : Nat) : Int))
  | "bitor" | "bitor_assign" => some (Lng.python.mask ((x.toNat ||| y.toNat : Nat) : Int))
  | "bitxor" | "bitxor_assign" => some (Lng.python.mask ((x.toNat ^^^ y.toNat : Nat) : Int))
  | "neg" => some (Lng.python.mask (-(Lng.python.int x)))
  | "not" => some (Lng.python.mask (-(Lng.python.int x) - 1))
  | "set" => some (Lng.python.mask (Lng.python.int x))
  | "shl" | "shl_assign" => some (Lng.python.mask (Lng.python.int x * 2 ^ y.toNat))
  | "shr" | "shr_assign" => some (Lng.python.mask (Int.fdiv (Lng.python.int x) (2 ^ y.toNat)))
  | "lt" => some (reads 1 (Lng.python.flag (decide (Lng.python.int x < Lng.python.int y))))
  | "le" => some (reads 1 (Lng.python.flag (decide (Lng.python.int x ≤ Lng.python.int y))))
  | "gt" => some (reads 1 (Lng.python.flag (decide (Lng.python.int x > Lng.python.int y))))
  | "ge" => some (reads 1 (Lng.python.flag (decide (Lng.python.int x ≥ Lng.python.int y))))
  | "eq" => some (reads 1 (Lng.python.flag (decide (Lng.python.int x = Lng.python.int y))))
  | "ne" => some (reads 1 (Lng.python.flag (decide (Lng.python.int x ≠ Lng.python.int y))))
  | "unless" => some (reads 1 (Lng.python.flag (decide (Lng.python.int x = 0))))
  | _ => none

/-- Two answers that are none at a divisor of 0 agree where their values agree at every other divisor. -/
theorem ite_some {y p q : Word} (h : y ≠ 0 → p = q) :
    (if y = 0 then (none : Option Word) else some p) = (if y = 0 then none else some q) := by
  by_cases hy : y = 0
  · simp [hy]
  · simp [h hy]

/-- C is a sound chart on `uint32_t`. -/
def c : Chart 32 false where
  symbols := shared
  anchor := anchorOf
  domain := domain
  sem := fun b x y => if b ∈ shared then cSem b x y else none
  sound := by
    intro b hb x y hd
    simp only [ite_eq_left hb]
    simp only [shared, List.mem_cons, List.mem_nil_iff, or_false] at hb
    rcases hb with rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl |
      rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · exact congrArg some (Lng.c.shl x y (hd (by decide)))
    · exact congrArg some (Lng.c.shr x y (hd (by decide)))
    · exact congrArg some (Lng.c.lt x y)
    · exact congrArg some (Lng.c.le x y)
    · exact congrArg some (Lng.c.gt x y)
    · exact congrArg some (Lng.c.ge x y)
    · exact congrArg some (Lng.c.eq x y)
    · exact congrArg some (Lng.c.ne x y)
    · exact congrArg some (Lng.c.unless x)
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · rfl
    · exact congrArg some (Lng.c.shl x y (hd (by decide)))
    · exact congrArg some (Lng.c.shr x y (hd (by decide)))
  closed := fun _ hb _ _ => ite_eq_right hb

/-- Python is a sound chart on the words of its mask. -/
def python : Chart 32 false where
  symbols := shared
  anchor := anchorOf
  domain := domain
  sem := fun b x y => if b ∈ shared then pySem b x y else none
  sound := by
    intro b hb x y hd
    simp only [ite_eq_left hb]
    simp only [shared, List.mem_cons, List.mem_nil_iff, or_false] at hb
    rcases hb with rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl |
      rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl
    · exact congrArg some (Lng.python.add x y)
    · exact congrArg some (Lng.python.sub x y)
    · exact congrArg some (Lng.python.mul x y)
    · exact ite_some (Lng.python.div x y)
    · exact ite_some (Lng.python.rem x y)
    · exact congrArg some (Lng.python.bitand x y)
    · exact congrArg some (Lng.python.bitor x y)
    · exact congrArg some (Lng.python.bitxor x y)
    · exact congrArg some (Lng.python.neg x)
    · exact congrArg some (Lng.python.not x)
    · exact congrArg some (Lng.python.set x)
    · exact congrArg some (Lng.python.shl x y (hd (by decide)))
    · exact congrArg some (Lng.python.shr x y (hd (by decide)))
    · exact congrArg some (Lng.python.lt x y)
    · exact congrArg some (Lng.python.le x y)
    · exact congrArg some (Lng.python.gt x y)
    · exact congrArg some (Lng.python.ge x y)
    · exact congrArg some (Lng.python.eq x y)
    · exact congrArg some (Lng.python.ne x y)
    · exact congrArg some (Lng.python.unless x)
    · exact congrArg some (Lng.python.add x y)
    · exact congrArg some (Lng.python.sub x y)
    · exact congrArg some (Lng.python.mul x y)
    · exact ite_some (Lng.python.div x y)
    · exact ite_some (Lng.python.rem x y)
    · exact congrArg some (Lng.python.bitand x y)
    · exact congrArg some (Lng.python.bitor x y)
    · exact congrArg some (Lng.python.bitxor x y)
    · exact congrArg some (Lng.python.shl x y (hd (by decide)))
    · exact congrArg some (Lng.python.shr x y (hd (by decide)))
  closed := fun _ hb _ _ => ite_eq_right hb

/-- A typo of add halts in .g, in C and in Python, and so it lifts to no symbol that has a value. -/
theorem typo (x y : Word) : (g 32 false).sem "ad" x y = none ∧ c.sem "ad" x y = none ∧ python.sem "ad" x y = none :=
  ⟨halt (g 32 false) "ad" (by decide) x y, halt c "ad" (by decide) x y, halt python "ad" (by decide) x y⟩

/-- The .g operator `+`, the C symbols of add and add_assign, and the Python symbols of both reach one anchor. -/
theorem many_to_many (x y : Word) :
    (g 32 false).sem "+" x y = c.sem "add" x y ∧ c.sem "add" x y = c.sem "add_assign" x y ∧
    c.sem "add_assign" x y = python.sem "add_assign" x y ∧ python.sem "add_assign" x y = python.sem "add" x y := by
  have hg : "+" ∈ (g 32 false).symbols := by decide
  have ha : "add" ∈ shared := by decide
  have hs : "add_assign" ∈ shared := by decide
  have da : domain "add" y := fun h => absurd h (by decide)
  have ds : domain "add_assign" y := fun h => absurd h (by decide)
  exact ⟨lift (g 32 false) c "+" "add" hg ha (by decide) x y trivial da,
    lift c c "add" "add_assign" ha hs (by decide) x y da ds,
    lift c python "add_assign" "add_assign" hs hs rfl x y ds ds,
    lift python python "add_assign" "add" hs ha (by decide) x y ds da⟩

end Lng.Any
