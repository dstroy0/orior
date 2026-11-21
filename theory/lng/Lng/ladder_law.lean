import Lng.ladder
import Lng.ladder_cases

/-!
# The ladder's cases, and the relations of Lng.ladder that answer them

`cases` holds every case of ladder.h, as harness.py lng prove writes it. `cases_answered` proves that each relation
of Lng.ladder gives the answer that its case in ladder.h states. Thus the language files prove against the
relations of the ladder that asks the part. No second copy of a relation can drift from it.
-/

namespace Lng.LadderLaw

open Lng.Ladder

/-- The answer of the anchor with the text `anchor` on the words `words`, or none where no anchor has that text. A
quotient and a remainder by 0 have no answer, as ladder_answer gives them none. -/
def answer (anchor : String) (words : List Nat) : Option Nat :=
  let a : Word := BitVec.ofNat 32 (words.getD 0 0)
  let b : Word := BitVec.ofNat 32 (words.getD 1 0)
  match anchor with
  | "same" => some (same a b).toNat
  | "places" => some 32
  | "add" => some (Ladder.add a b).toNat
  | "take" => some (take a b).toNat
  | "up" => some (up a b).toNat
  | "down" => some (down a b).toNat
  | "product" => some (product a b).toNat
  | "less" => some (less a b).toNat
  | "at_max" => some (atMax a b).toNat
  | "more" => some (more a b).toNat
  | "at_least" => some (atLeast a b).toNat
  | "apart" => some (apart a b).toNat
  | "quotient" => if b = 0 then none else some (quotient a b).toNat
  | "remainder" => if b = 0 then none else some (remainder a b).toNat
  | "both" => some (both a b).toNat
  | "either" => some (either a b).toNat
  | "differ" => some (differ a b).toNat
  | "negate" => some (negate a).toNat
  | "invert" => some (invert a).toNat
  | "zero" => some (zero a).toNat
  | "copy" => some (copy a).toNat
  | _ => none

theorem cases_answered :
    Lng.LadderCases.cases.all (fun c => answer c.1 c.2.1 == some c.2.2) = true := by decide

end Lng.LadderLaw
