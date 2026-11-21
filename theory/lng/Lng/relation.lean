import Lng.ladder_law

/-!
# The ladder's relations on a word of any width

`relation` gives each anchor of ladder.h on two words of `n` places. Where `signed` is true, it reads them as
ladder_signed_answer reads them. The orderings compare signed, a quotient cuts toward 0, and a remainder keeps the
sign of the dividend. DOWN fills the places it empties with the sign. Every other relation answers alike either way.
The lowest word over a word of ones wraps to the lowest word, with a remainder of 0.

A quotient and a remainder by 0 have no answer. UP and DOWN read their count modulo the width, as the ladder reads
it modulo 32. `agrees` proves that at 32 places, read unsigned, each relation is the relation of the ladder law.
-/

namespace Lng.Relation

/-- A predicate's answer on a word of `n` places: ones where `p` is true, and 0 where it is false. -/
def truth (n : Nat) (p : Bool) : BitVec n := if p then BitVec.allOnes n else 0

/-- The count that UP and DOWN read from the second word: its value modulo the width. -/
def count {n : Nat} (b : BitVec n) : Nat := b.toNat % n

/-- The answer of the anchor with the text `anchor` on the words `a` and `b` of `n` places, read signed where
`signed` is true. It is none where no anchor has that text, and where a quotient or a remainder is by 0. -/
def relation {n : Nat} (signed : Bool) (anchor : String) (a b : BitVec n) : Option (BitVec n) :=
  match anchor with
  | "same" => some (truth n (a == b))
  | "places" => some (BitVec.ofNat n n)
  | "add" => some (a + b)
  | "take" => some (a - b)
  | "up" => some (a <<< count b)
  | "down" => some (if signed then a.sshiftRight (count b) else a >>> count b)
  | "product" => some (a * b)
  | "less" => some (truth n (if signed then a.slt b else a.ult b))
  | "at_max" => some (truth n (if signed then a.sle b else a.ule b))
  | "more" => some (truth n (if signed then b.slt a else b.ult a))
  | "at_least" => some (truth n (if signed then b.sle a else b.ule a))
  | "apart" => some (truth n (a != b))
  | "quotient" => if b = 0 then none else some (if signed then a.sdiv b else a / b)
  | "remainder" => if b = 0 then none else some (if signed then a.srem b else a % b)
  | "both" => some (a &&& b)
  | "either" => some (a ||| b)
  | "differ" => some (a ^^^ b)
  | "negate" => some (-a)
  | "invert" => some (~~~a)
  | "zero" => some (truth n (a == 0))
  | "copy" => some a
  | _ => none

/-- At 32 places, read unsigned, each relation is the relation of the ladder law. -/
theorem agrees (anchor : String) (a b : Lng.Ladder.Word) :
    (relation false anchor a b).map BitVec.toNat = Lng.LadderLaw.answer anchor [a.toNat, b.toNat] := by
  unfold relation Lng.LadderLaw.answer
  split <;> simp_all [truth, count, Lng.Ladder.truth, Lng.Ladder.ones, Lng.Ladder.count, Lng.Ladder.same,
    Lng.Ladder.add, Lng.Ladder.take, Lng.Ladder.up, Lng.Ladder.down, Lng.Ladder.product, Lng.Ladder.less,
    Lng.Ladder.atMax, Lng.Ladder.more, Lng.Ladder.atLeast, Lng.Ladder.apart, Lng.Ladder.quotient,
    Lng.Ladder.remainder, Lng.Ladder.both, Lng.Ladder.either, Lng.Ladder.differ, Lng.Ladder.negate,
    Lng.Ladder.invert, Lng.Ladder.zero, Lng.Ladder.copy]
  all_goals (split <;> simp_all [BitVec.toNat_udiv, BitVec.toNat_umod])

/-- The lowest word of 32 places over a word of ones: read signed, the quotient wraps to the lowest word, and the
remainder is 0. -/
theorem lowest_over_ones :
    relation true "quotient" (0x80000000#32) (0xffffffff#32) = some 0x80000000#32 ∧
    relation true "remainder" (0x80000000#32) (0xffffffff#32) = some 0#32 := by decide

end Lng.Relation
