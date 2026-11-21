import Lng.relation
import Lng.g_rows

/-!
# .g, the high order face, compiled to gsm

A .g operator is a column of gnascor_hol_lng.tsv, and the core op that marks it is its word. That word crosses to
the transpiler word that its `transpiler` cell names, or else to the transpiler word of its own name. The anchor of
that transpiler word is a relation of the ladder, and the cases of that anchor in ladder.h are the operator's gsm.
This file proves that transform over the tables, and for every word of every width, read unsigned and signed.

* `bound`: each operator that a core op marks compiles to an anchor.
* `answered`: the ladder's relation gives each gsm case of each operator its answer.
* `files`: each case of each .gsm file of the protocol's tables is a gsm case of a .g operator.
* `sm_86`: sm_86.gsm holds the two-word gsm cases of `+`, in the order of the ladder.
* `types`: each exact integer type has a width of 8, 16, 32 or 64 places or of `n`, and a sign of 0 or 1.
* The theorems named for a core op prove what its operator compiles to on every pair of words of `n` places. A type
  that is signed reads its words signed, and every other type reads them unsigned.
-/

namespace Lng.g

open Lng.Ladder Lng.GRows Lng.LadderCases Lng.LadderLaw Lng.Relation

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

/-- What the operator `symbol` compiles to on the words `a` and `b` of `n` places, read signed where `signed`. -/
def compile {n : Nat} (signed : Bool) (symbol : String) (a b : BitVec n) : Option (BitVec n) :=
  (anchorOf symbol).bind (fun anchor => relation signed anchor a b)

theorem bound : symbols.all (fun s => (anchorOf s).isSome) = true := by decide

theorem answered : symbols.all (fun s => match anchorOf s with
    | some a => (gsm s).all (fun c => answer a c.1 == some c.2)
    | none => false) = true := by decide

theorem files : gsmFiles.all (fun f => f.2.all (fun c => symbols.any (fun s => (gsm s).contains c))) = true := by
  decide

theorem sm_86 : gsmFiles.lookup "sm_86.gsm" = some ((gsm "+").filter (fun c => c.1.length == 2)) := by decide

theorem types : (holRows.filter (fun r => r.type == "type" && r.transpiler != "")).all (fun r =>
    ["8", "16", "32", "64"].contains r.bits && ["0", "1"].contains r.signed) = true := by decide

/-- At 32 places, read unsigned, what an operator compiles to is the ladder law's answer for its anchor. -/
theorem ladder (symbol : String) (a b : Word) :
    (compile false symbol a b).map BitVec.toNat = (anchorOf symbol).bind (fun anchor => answer anchor [a.toNat, b.toNat]) := by
  unfold compile
  cases anchorOf symbol with
  | none => rfl
  | some anchor => simpa using agrees anchor a b

section
variable {n : Nat} (s : Bool) (a b : BitVec n)

theorem add : compile s "+" a b = some (a + b) := by
  have h : anchorOf "+" = some "add" := by decide
  simp [compile, h, relation]
theorem sub : compile s "-" a b = some (a - b) := by
  have h : anchorOf "-" = some "take" := by decide
  simp [compile, h, relation]
theorem mul : compile s "*" a b = some (a * b) := by
  have h : anchorOf "*" = some "product" := by decide
  simp [compile, h, relation]
theorem div (hb : b ≠ 0) : compile s "/" a b = some (if s then a.sdiv b else a / b) := by
  have h : anchorOf "/" = some "quotient" := by decide
  simp [compile, h, relation] <;> exact hb
theorem rem (hb : b ≠ 0) : compile s "%" a b = some (if s then a.srem b else a % b) := by
  have h : anchorOf "%" = some "remainder" := by decide
  simp [compile, h, relation] <;> exact hb
theorem neg : compile s "-x" a b = some (-a) := by
  have h : anchorOf "-x" = some "negate" := by decide
  simp [compile, h, relation]
theorem «not» : compile s "~x" a b = some (~~~a) := by
  have h : anchorOf "~x" = some "invert" := by decide
  simp [compile, h, relation]
theorem «unless» : compile s "!x" a b = some (truth n (a == 0)) := by
  have h : anchorOf "!x" = some "zero" := by decide
  simp [compile, h, relation]
theorem bitand : compile s "&" a b = some (a &&& b) := by
  have h : anchorOf "&" = some "both" := by decide
  simp [compile, h, relation]
theorem bitor : compile s "|" a b = some (a ||| b) := by
  have h : anchorOf "|" = some "either" := by decide
  simp [compile, h, relation]
theorem bitxor : compile s "^" a b = some (a ^^^ b) := by
  have h : anchorOf "^" = some "differ" := by decide
  simp [compile, h, relation]
theorem shl : compile s "<<" a b = some (a <<< count b) := by
  have h : anchorOf "<<" = some "up" := by decide
  simp [compile, h, relation]
theorem shr : compile s ">>" a b = some (if s then a.sshiftRight (count b) else a >>> count b) := by
  have h : anchorOf ">>" = some "down" := by decide
  simp [compile, h, relation]
theorem eq : compile s "==" a b = some (truth n (a == b)) := by
  have h : anchorOf "==" = some "same" := by decide
  simp [compile, h, relation]
theorem ne : compile s "!=" a b = some (truth n (a != b)) := by
  have h : anchorOf "!=" = some "apart" := by decide
  simp [compile, h, relation]
theorem lt : compile s "<" a b = some (truth n (if s then a.slt b else a.ult b)) := by
  have h : anchorOf "<" = some "less" := by decide
  simp [compile, h, relation]
theorem le : compile s "<=" a b = some (truth n (if s then a.sle b else a.ule b)) := by
  have h : anchorOf "<=" = some "at_max" := by decide
  simp [compile, h, relation]
theorem gt : compile s ">" a b = some (truth n (if s then b.slt a else b.ult a)) := by
  have h : anchorOf ">" = some "more" := by decide
  simp [compile, h, relation]
theorem ge : compile s ">=" a b = some (truth n (if s then b.sle a else b.ule a)) := by
  have h : anchorOf ">=" = some "at_least" := by decide
  simp [compile, h, relation]

end

/-- A compound assignment compiles to the anchor of its operator: `a op= b` gives `a` the answer of `a op b`. -/
theorem assign_law : [("+=", "+"), ("-=", "-"), ("*=", "*"), ("/=", "/"), ("%=", "%"), ("&=", "&"), ("|=", "|"),
    ("^=", "^"), ("<<=", "<<"), (">>=", ">>")].all (fun p => anchorOf p.1 == anchorOf p.2) = true := by decide

end Lng.g
