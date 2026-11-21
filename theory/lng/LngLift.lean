/-!
# The lift that lng_lift runs

This module is the code of `lng_lift`. Lean compiles it to C, and Lake builds it into `Lng_LngLift.a`. A program that
calls `lng_lift` links that archive, and no lift written in C. `Lng/lift_code.lean` proves what the code computes.
The module imports only the core of Lean. The archive therefore holds this module and the core alone.

A lift takes one bridge word from a source table to a target table. It refuses where the transpiler has no word for
the bridge, where a relation has no ladder anchor, and where a side has no witnessed row for the bridge.
-/

namespace LngLift

/-- A row of the transpiler table: its word, its type, its ladder anchor and its target. -/
structure Crossing where
  word : String
  type : String
  anchor : String
  target : String
  deriving Inhabited, Repr, DecidableEq

/-- A row of a target table: the transpiler word it bridges to, its own word and its witness. -/
structure Side where
  transpiler : String
  word : String
  witness : String
  deriving Inhabited, Repr, DecidableEq

/-- The place of the first element of `l` that satisfies `p`, or none where no element does. -/
def firstIndex {α : Type} (p : α → Bool) : List α → Option Nat
  | [] => none
  | a :: rest => if p a then some 0 else (firstIndex p rest).map (· + 1)

/-- The place of the witnessed row of `table`, named `name`, that bridges to `bridge`, or the refusal. -/
def side (table : List Side) (name bridge : String) : Except String Nat :=
  match firstIndex (fun r => r.transpiler == bridge) table with
  | none => .error s!"{name} has no row for {bridge}"
  | some i =>
    if (table.getD i default).witness == "" then
      .error s!"{name}'s {(table.getD i default).word} for {bridge} has no witness"
    else .ok i

/-- The lift of `bridge` from `source` to `target`: the place of its row in `target`, or each refusal. -/
def lift (transpiler : List Crossing) (source target : List Side) (sourceName targetName bridge : String) :
    Except (List String) Nat :=
  match firstIndex (fun c => c.target == "" && c.word == bridge) transpiler with
  | none => .error [s!"the transpiler has no word {bridge}, and no lift goes through it"]
  | some i =>
    if (transpiler.getD i default).type != "thing" && (transpiler.getD i default).anchor == "" then
      .error [s!"{bridge} has no gsm cases, and no lift goes through it"]
    else
      match side source sourceName bridge, side target targetName bridge with
      | .ok _, .ok j => .ok j
      | .error a, .ok _ => .error [a]
      | .ok _, .error b => .error [b]
      | .error a, .error b => .error [a, b]

/-- The rows of the columns `first`, `second`, `third` and `fourth`, one row for each place of `first`. -/
def crossings (words types anchors targets : Array String) : List Crossing :=
  (List.range words.size).map fun i => ⟨words[i]!, types[i]!, anchors[i]!, targets[i]!⟩

/-- The rows of the columns `bridges`, `words` and `witnesses`, one row for each place of `bridges`. -/
def sides (bridges words witnesses : Array String) : List Side :=
  (List.range bridges.size).map fun i => ⟨bridges[i]!, words[i]!, witnesses[i]!⟩

/-- The lift for C. Its first element is the place of the target's row in decimal, or empty where the lift refuses.
Each refusal follows it. -/
@[export lng_lean_lift]
def liftForC (words types anchors targets sourceBridges sourceWords sourceWitnesses : Array String)
    (sourceName : String) (targetBridges targetWords targetWitnesses : Array String) (targetName bridge : String) :
    Array String :=
  match lift (crossings words types anchors targets) (sides sourceBridges sourceWords sourceWitnesses)
      (sides targetBridges targetWords targetWitnesses) sourceName targetName bridge with
  | .ok j => #[toString j]
  | .error refusals => #[""] ++ refusals.toArray

end LngLift
