import Lng.matrix_rows

/-!
# The matrices of gsm

`rows` holds every row of gnascor_asm_lng.tsv, as harness.py lng prove writes it. A row is a transition: the state
it left, the state it reached, and the bits of the pair in each. It gives what the transition does to each side,
and the arrow.
A branch pair row has two states for its sides, and each state is its two bits. Its rows are no transition, and
only the state law holds for them.

The laws, each over every row:

* A side's operation follows from its two bits. It is `-` where a bit is unknown, `=` where the bit stays, and
  the bit it reaches where it changes.
* The arrow follows from the two operations. It is `*>` where a side is unknown, `x>` where both sides change,
  and `->` where a side stays.
* A state's name fixes its bits: lead is 1 0, rite 0 1, dual 1 1 and void 0 0. A state with no pair bit writes
  `-` for each.
* One table gives one transition one mnemonic.
* Each side's anchor, copy or invert, gives the bit that the side reaches.
-/

namespace Lng.Matrix

open Lng.MatrixRows

/-- 1 where the row is of the branch pair. Its sides are two states, and no transition. The laws of a transition
leave its rows out, and the state law holds for them too. -/
def branch (r : Row) : Bool := r.table == "the branch pair"

/-- The ladder anchor of what a transition does to one side. It is copy where the side keeps its bit, and invert
where it reaches the other bit. -/
def anchorOf (operation : String) : String := if operation == "=" then "copy" else "invert"

/-- The bit that an anchor gives a side's bit. -/
def apply (anchor bit : String) : String :=
  if anchor == "copy" then bit else if bit == "1" then "0" else "1"

/-- What a transition does to one side, from the side's bit in the state it left and in the state it reached. -/
def side (past present : String) : String :=
  if past == "-" || present == "-" then "-" else if past == present then "=" else present

/-- The arrow that the two operations give. -/
def arrowOf (left right : String) : String :=
  if left == "-" || right == "-" then "*>" else if left != "=" && right != "=" then "x>" else "->"

/-- The bits of a state that has a pair, left then right. -/
def bitsOf : String → Option (String × String)
  | "lead" => some ("1", "0")
  | "rite" => some ("0", "1")
  | "dual" => some ("1", "1")
  | "void" => some ("0", "0")
  | _ => none

/-- 1 where `left` and `right` are the bits that the name `state` fixes. -/
def named (state left right : String) : Bool :=
  match bitsOf state with
  | some (l, r) => left == l && right == r
  | none => left == "-" && right == "-"

theorem side_law : rows.all (fun r => branch r ||
    (r.left == side r.pastLeft r.presentLeft && r.right == side r.pastRight r.presentRight)) = true := by decide

theorem arrow_law : rows.all (fun r => branch r || r.arrow == arrowOf r.left r.right) = true := by decide

theorem state_law : rows.all (fun r =>
    named r.past r.pastLeft r.pastRight && named r.present r.presentLeft r.presentRight) = true := by decide

/-- Each side's anchor gives the bit that the side reaches, where the side holds a pair bit. This is the law that the
transpiler form of a mnemonic stands on: the bridge word of each side's anchor. -/
theorem form_law : rows.all (fun r => branch r ||
    ((r.left == "-" || apply (anchorOf r.left) r.pastLeft == r.presentLeft) &&
     (r.right == "-" || apply (anchorOf r.right) r.pastRight == r.presentRight))) = true := by decide

theorem mnemonic_law : rows.all (fun a => rows.all (fun b => branch a ||
    !(a.table == b.table && a.past == b.past && a.present == b.present) || a.mnemonic == b.mnemonic)) = true := by
  decide

end Lng.Matrix
