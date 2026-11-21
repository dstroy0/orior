import Lng.ladder_law
import Lng.g_rows

/-!
# .g, the high order face, compiled to gsm

A .g operator is a column of gnascor_hol_lng.tsv, and the core op that marks it is its word. That word crosses to
the transpiler word that its `transpiler` cell names, or else to the transpiler word of its own name. The anchor of
that transpiler word is a relation of the ladder, and the cases of that anchor in ladder.h are the operator's gsm.
This file proves that transform over the tables, and for every word of 32 places.

* `bound`: each operator that a core op marks compiles to an anchor.
* `answered`: the ladder's relation gives each gsm case of each operator its answer.
* `files`: each case of each .gsm file of the protocol's tables is a gsm case of a .g operator.
* `sm_86`: sm_86.gsm holds the two-word gsm cases of `+`, in the order of the ladder.
* The theorems named for a core op prove its transform for every pair of words.
-/

namespace Lng.g

open Lng.Ladder Lng.GRows Lng.LadderCases Lng.LadderLaw

/-- The core op of the high order table that marks the operator `symbol`. -/
def coreOp (symbol : String) : Option HolRow :=
  holRows.find? (fun r => r.type == "core op" && r.symbols.contains symbol)

/-- The transpiler word that a core op crosses to. -/
def bridge (r : HolRow) : String := if r.transpiler == "" then r.word else r.transpiler

/-- The ladder anchor that the operator `symbol` compiles to. -/
def anchorOf (symbol : String) : Option String :=
  (coreOp symbol).bind (fun r => (anchors.find? (fun p => p.1 == bridge r)).map (·.2))

/-- Every operator that a core op marks. -/
def symbols : List String := (holRows.filter (fun r => r.type == "core op")).flatMap (·.symbols)

/-- The gsm of the operator `symbol`: the words and the answer of each case of its anchor. -/
def gsm (symbol : String) : List (List Nat × Nat) :=
  match anchorOf symbol with
  | some a => (cases.filter (fun c => c.1 == a)).map (·.2)
  | none => []

/-- The answer that the operator `symbol` compiles to on the words `a` and `b`. -/
def apply (symbol : String) (a b : Word) : Option Nat :=
  (anchorOf symbol).bind (fun anchor => answer anchor [a.toNat, b.toNat])

theorem bound : symbols.all (fun s => (anchorOf s).isSome) = true := by decide

theorem answered : symbols.all (fun s => match anchorOf s with
    | some a => (gsm s).all (fun c => answer a c.1 == some c.2)
    | none => false) = true := by decide

theorem files : gsmFiles.all (fun f => f.2.all (fun c => symbols.any (fun s => (gsm s).contains c))) = true := by
  decide

theorem sm_86 : gsmFiles.lookup "sm_86.gsm" = some ((gsm "+").filter (fun c => c.1.length == 2)) := by decide

theorem add (a b : Word) : apply "+" a b = some (Ladder.add a b).toNat := by
  have h : anchorOf "+" = some "add" := by decide
  simp [apply, h, answer]
theorem sub (a b : Word) : apply "-" a b = some (take a b).toNat := by
  have h : anchorOf "-" = some "take" := by decide
  simp [apply, h, answer]
theorem mul (a b : Word) : apply "*" a b = some (product a b).toNat := by
  have h : anchorOf "*" = some "product" := by decide
  simp [apply, h, answer]
theorem div (a b : Word) : apply "/" a b = some (quotient a b).toNat := by
  have h : anchorOf "/" = some "quotient" := by decide
  simp [apply, h, answer]
theorem rem (a b : Word) : apply "%" a b = some (remainder a b).toNat := by
  have h : anchorOf "%" = some "remainder" := by decide
  simp [apply, h, answer]
theorem neg (a b : Word) : apply "-x" a b = some (negate a).toNat := by
  have h : anchorOf "-x" = some "negate" := by decide
  simp [apply, h, answer]
theorem «not» (a b : Word) : apply "~x" a b = some (invert a).toNat := by
  have h : anchorOf "~x" = some "invert" := by decide
  simp [apply, h, answer]
theorem bitand (a b : Word) : apply "&" a b = some (both a b).toNat := by
  have h : anchorOf "&" = some "both" := by decide
  simp [apply, h, answer]
theorem bitor (a b : Word) : apply "|" a b = some (either a b).toNat := by
  have h : anchorOf "|" = some "either" := by decide
  simp [apply, h, answer]
theorem bitxor (a b : Word) : apply "^" a b = some (differ a b).toNat := by
  have h : anchorOf "^" = some "differ" := by decide
  simp [apply, h, answer]
theorem shl (a b : Word) : apply "<<" a b = some (up a b).toNat := by
  have h : anchorOf "<<" = some "up" := by decide
  simp [apply, h, answer]
theorem shr (a b : Word) : apply ">>" a b = some (down a b).toNat := by
  have h : anchorOf ">>" = some "down" := by decide
  simp [apply, h, answer]
theorem eq (a b : Word) : apply "==" a b = some (same a b).toNat := by
  have h : anchorOf "==" = some "same" := by decide
  simp [apply, h, answer]
theorem ne (a b : Word) : apply "!=" a b = some (apart a b).toNat := by
  have h : anchorOf "!=" = some "apart" := by decide
  simp [apply, h, answer]
theorem lt (a b : Word) : apply "<" a b = some (less a b).toNat := by
  have h : anchorOf "<" = some "less" := by decide
  simp [apply, h, answer]
theorem le (a b : Word) : apply "<=" a b = some (atMax a b).toNat := by
  have h : anchorOf "<=" = some "at_max" := by decide
  simp [apply, h, answer]
theorem gt (a b : Word) : apply ">" a b = some (more a b).toNat := by
  have h : anchorOf ">" = some "more" := by decide
  simp [apply, h, answer]
theorem ge (a b : Word) : apply ">=" a b = some (atLeast a b).toNat := by
  have h : anchorOf ">=" = some "at_least" := by decide
  simp [apply, h, answer]

/-- A compound assignment compiles to the anchor of its operator: `a op= b` gives `a` the answer of `a op b`. -/
theorem assign_law : [("+=", "+"), ("-=", "-"), ("*=", "*"), ("/=", "/"), ("%=", "%"), ("&=", "&"), ("|=", "|"),
    ("^=", "^"), ("<<=", "<<"), (">>=", ">>")].all (fun p => anchorOf p.1 == anchorOf p.2) = true := by decide

end Lng.g
