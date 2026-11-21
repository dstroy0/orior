import Lng.ladder

/-!
# C, the symbols of c_lng.tsv

C11 6.2.5 paragraph 9: an operation on an unsigned integer of N places answers modulo 2^N, and no such operation
overflows. So each operator on `uint32_t` is the operation of the word of 32 places, and each model below is that
operation. A comparison and `!` answer 1 for true and 0 for false (C11 6.5.8 to 6.5.10, and 6.5.3.3).

C11 6.5.7: a shift by a count of 32 or more has no defined answer. So the theorems of `<<` and `>>` take a count
below 32. The ladder's UP and DOWN read the count modulo 32, and a count of 32 or more is outside what C gives.
C11 6.5.5: a quotient or a remainder by 0 has no defined answer, and their theorems take a divisor that is not 0.

Each theorem has the name of the bridge word of its row in c_lng.tsv. The language check reads those names.
-/

namespace Lng.c

open Lng.Ladder

/-- A comparison's answer, 1 for true and 0 for false. -/
def flag (p : Bool) : Nat := if p then 1 else 0

theorem add (a b : Word) : a + b = Ladder.add a b := rfl
theorem sub (a b : Word) : a - b = Ladder.take a b := rfl
theorem mul (a b : Word) : a * b = Ladder.product a b := rfl
theorem div (a b : Word) (_ : b ≠ 0) : a / b = Ladder.quotient a b := rfl
theorem rem (a b : Word) (_ : b ≠ 0) : a % b = Ladder.remainder a b := rfl
theorem bitand (a b : Word) : a &&& b = Ladder.both a b := rfl
theorem bitor (a b : Word) : a ||| b = Ladder.either a b := rfl
theorem bitxor (a b : Word) : a ^^^ b = Ladder.differ a b := rfl
theorem neg (a : Word) : -a = Ladder.negate a := rfl
theorem «not» (a : Word) : ~~~a = Ladder.invert a := rfl
theorem set (a : Word) : a = Ladder.copy a := rfl

theorem shl (a b : Word) (h : b.toNat < 32) : a <<< b.toNat = Ladder.up a b := by
  unfold Ladder.up Ladder.count
  rw [Nat.mod_eq_of_lt h]

theorem shr (a b : Word) (h : b.toNat < 32) : a >>> b.toNat = Ladder.down a b := by
  unfold Ladder.down Ladder.count
  rw [Nat.mod_eq_of_lt h]

theorem lt (a b : Word) : reads 1 (flag (a.ult b)) = Ladder.less a b := by
  unfold reads flag Ladder.less Ladder.truth
  cases a.ult b <;> rfl

theorem le (a b : Word) : reads 1 (flag (a.ule b)) = Ladder.atMax a b := by
  unfold reads flag Ladder.atMax Ladder.truth
  cases a.ule b <;> rfl

theorem gt (a b : Word) : reads 1 (flag (b.ult a)) = Ladder.more a b := by
  unfold reads flag Ladder.more Ladder.truth
  cases b.ult a <;> rfl

theorem ge (a b : Word) : reads 1 (flag (b.ule a)) = Ladder.atLeast a b := by
  unfold reads flag Ladder.atLeast Ladder.truth
  cases b.ule a <;> rfl

theorem eq (a b : Word) : reads 1 (flag (a == b)) = Ladder.same a b := by
  unfold reads flag Ladder.same Ladder.truth
  cases a == b <;> rfl

theorem ne (a b : Word) : reads 1 (flag (a != b)) = Ladder.apart a b := by
  unfold reads flag Ladder.apart Ladder.truth
  cases a != b <;> rfl

theorem «unless» (a : Word) : reads 1 (flag (a == 0)) = Ladder.zero a := by
  unfold reads flag Ladder.zero Ladder.truth
  cases a == 0 <;> rfl

/-! The compound assignments: `a op= b` gives `a` the value of `a op b` (C11 6.5.16.2). -/

theorem add_assign (a b : Word) : a + b = Ladder.add a b := rfl
theorem sub_assign (a b : Word) : a - b = Ladder.take a b := rfl
theorem mul_assign (a b : Word) : a * b = Ladder.product a b := rfl
theorem div_assign (a b : Word) (_ : b ≠ 0) : a / b = Ladder.quotient a b := rfl
theorem rem_assign (a b : Word) (_ : b ≠ 0) : a % b = Ladder.remainder a b := rfl
theorem bitand_assign (a b : Word) : a &&& b = Ladder.both a b := rfl
theorem bitor_assign (a b : Word) : a ||| b = Ladder.either a b := rfl
theorem bitxor_assign (a b : Word) : a ^^^ b = Ladder.differ a b := rfl
theorem shl_assign (a b : Word) (h : b.toNat < 32) : a <<< b.toNat = Ladder.up a b := shl a b h
theorem shr_assign (a b : Word) (h : b.toNat < 32) : a >>> b.toNat = Ladder.down a b := shr a b h

end Lng.c
