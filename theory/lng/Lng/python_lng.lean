import Lng.ladder

/-!
# Python, the symbols of python_lng.tsv

A Python int has no width, and its operators answer in the integers. The two words of a case are naturals below
2^32. The Python reference, section 6.9, defines `&` on the two's complement of each int, with an infinite count of
places. So `x & 0xffffffff` is the low 32 places of the two's complement of `x`. That is `BitVec.ofInt 32 x`, and
the model `mask` below is exactly that. Each symbol of a relation that leaves the word comes back through the mask.

Section 6.7: `x << n` is `x * 2 ** n`, and `x >> n` is `x // 2 ** n`. The ladder's UP and DOWN read the count
modulo 32, and the theorems of `<<` and `>>` take a count below 32. Section 6.7: `//` and `%` by 0 raise
ZeroDivisionError, and their theorems take a divisor that is not 0. A comparison and `not` answer True or False,
and `int()` of those is 1 or 0.

`add_leaves` proves the hole that the ladder found: Python's `+` without the mask is not the word's add.

Each theorem has the name of the bridge word of its row in python_lng.tsv. The language check reads those names.
-/

namespace Lng.python

open Lng.Ladder

/-- A word read as a Python int. -/
def int (w : Word) : Int := (w.toNat : Int)

/-- `x & 0xffffffff`: the low 32 places of the two's complement of `x`. -/
def mask (x : Int) : Word := BitVec.ofInt 32 x

/-- `int(p)` of a comparison: 1 for True and 0 for False. -/
def flag (p : Bool) : Nat := if p then 1 else 0

/-- The mask of a natural is the word of it modulo 2^32. -/
theorem mask_nat (n : Nat) : mask (n : Int) = BitVec.ofNat 32 n := by
  unfold mask
  exact BitVec.ofInt_natCast 32 n

theorem add (a b : Word) : mask (int a + int b) = Ladder.add a b := by
  unfold int Ladder.add
  rw [← Int.natCast_add, mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_add]

theorem add_leaves : ∃ a b : Word, int a + int b ≠ int (Ladder.add a b) :=
  ⟨0xffffffff#32, 1#32, by decide⟩

theorem sub (a b : Word) : mask (int a - int b) = Ladder.take a b := by
  unfold mask int Ladder.take
  apply BitVec.eq_of_toNat_eq
  rw [BitVec.toNat_ofInt, BitVec.toNat_sub]
  have ha := a.isLt
  have hb := b.isLt
  omega

theorem mul (a b : Word) : mask (int a * int b) = Ladder.product a b := by
  unfold int Ladder.product
  rw [← Int.natCast_mul, mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_mul]

/-- `//`: the quotient rounded down (section 6.7). -/
theorem div (a b : Word) (_ : b ≠ 0) : mask (Int.fdiv (int a) (int b)) = Ladder.quotient a b := by
  unfold int Ladder.quotient
  rw [Int.fdiv_eq_ediv_of_nonneg _ (Int.natCast_nonneg _), ← Int.natCast_ediv, mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_udiv]
  exact Nat.lt_of_le_of_lt (Nat.div_le_self _ _) a.isLt

/-- `%`: the remainder of the quotient rounded down, with the sign of the divisor (section 6.7). -/
theorem rem (a b : Word) (_ : b ≠ 0) : mask (Int.fmod (int a) (int b)) = Ladder.remainder a b := by
  unfold int Ladder.remainder
  rw [Int.fmod_eq_emod_of_nonneg _ (Int.natCast_nonneg _), ← Int.natCast_emod, mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_umod]
  exact Nat.lt_of_le_of_lt (Nat.mod_le _ _) a.isLt

/-! `&`, `|` and `^` of two ints that are not negative act on their binary places (section 6.9). -/

theorem bitand (a b : Word) : mask ((a.toNat &&& b.toNat : Nat) : Int) = Ladder.both a b := by
  unfold Ladder.both
  rw [mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_and]

theorem bitor (a b : Word) : mask ((a.toNat ||| b.toNat : Nat) : Int) = Ladder.either a b := by
  unfold Ladder.either
  rw [mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_or]

theorem bitxor (a b : Word) : mask ((a.toNat ^^^ b.toNat : Nat) : Int) = Ladder.differ a b := by
  unfold Ladder.differ
  rw [mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_xor]

theorem neg (a : Word) : mask (-(int a)) = Ladder.negate a := by
  unfold mask int Ladder.negate
  apply BitVec.eq_of_toNat_eq
  rw [BitVec.toNat_ofInt, BitVec.toNat_neg]
  have ha := a.isLt
  omega

/-- `~x` is `-x - 1` (section 6.6). -/
theorem «not» (a : Word) : mask (-(int a) - 1) = Ladder.invert a := by
  unfold mask int Ladder.invert
  apply BitVec.eq_of_toNat_eq
  rw [BitVec.toNat_ofInt, BitVec.toNat_not]
  have ha := a.isLt
  omega

/-- `x << n` is `x * 2 ** n` (section 6.7). -/
theorem shl (a b : Word) (h : b.toNat < 32) : mask (int a * 2 ^ b.toNat) = Ladder.up a b := by
  unfold int Ladder.up Ladder.count
  have hp : (2 : Int) ^ b.toNat = ((2 ^ b.toNat : Nat) : Int) := by simp
  rw [Nat.mod_eq_of_lt h, hp, ← Int.natCast_mul, mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_shiftLeft, Nat.shiftLeft_eq]

/-- `x >> n` is `x // 2 ** n` (section 6.7). -/
theorem shr (a b : Word) (h : b.toNat < 32) :
    mask (Int.fdiv (int a) (2 ^ b.toNat)) = Ladder.down a b := by
  unfold int Ladder.down Ladder.count
  have hp : (2 : Int) ^ b.toNat = ((2 ^ b.toNat : Nat) : Int) := by simp
  rw [Nat.mod_eq_of_lt h, hp, Int.fdiv_eq_ediv_of_nonneg _ (Int.natCast_nonneg _), ← Int.natCast_ediv, mask_nat]
  apply BitVec.eq_of_toNat_eq
  simp [BitVec.toNat_ushiftRight, Nat.shiftRight_eq_div_pow]
  exact Nat.lt_of_le_of_lt (Nat.div_le_self _ _) a.isLt

/-- A count of 32 or more: Python moves the places out of the word, and the ladder reads the count modulo 32. -/
theorem shl_count_leaves : ∃ a b : Word, 32 ≤ b.toNat ∧ mask (int a * 2 ^ b.toNat) ≠ Ladder.up a b :=
  ⟨1#32, 33#32, by decide, by decide⟩

theorem set (a : Word) : mask (int a) = Ladder.copy a := by
  unfold int Ladder.copy
  rw [mask_nat]
  simp

/-! The comparisons and `not`: `int()` of each is 1 for True and 0 for False. -/

theorem lt (a b : Word) : reads 1 (flag (decide (int a < int b))) = Ladder.less a b := by
  unfold reads flag int Ladder.less Ladder.truth
  by_cases h : a.toNat < b.toNat <;> simp [h, BitVec.ult]

theorem le (a b : Word) : reads 1 (flag (decide (int a ≤ int b))) = Ladder.atMax a b := by
  unfold reads flag int Ladder.atMax Ladder.truth
  by_cases h : a.toNat ≤ b.toNat <;> simp [h, BitVec.ule]

theorem gt (a b : Word) : reads 1 (flag (decide (int a > int b))) = Ladder.more a b := by
  unfold reads flag int Ladder.more Ladder.truth
  by_cases h : b.toNat < a.toNat <;> simp [h, BitVec.ult]

theorem ge (a b : Word) : reads 1 (flag (decide (int a ≥ int b))) = Ladder.atLeast a b := by
  unfold reads flag int Ladder.atLeast Ladder.truth
  by_cases h : b.toNat ≤ a.toNat <;> simp [h, BitVec.ule]

theorem eq (a b : Word) : reads 1 (flag (decide (int a = int b))) = Ladder.same a b := by
  unfold reads flag int Ladder.same Ladder.truth
  by_cases h : a = b <;> simp [h, Int.natCast_inj, BitVec.toNat_inj]

theorem ne (a b : Word) : reads 1 (flag (decide (int a ≠ int b))) = Ladder.apart a b := by
  unfold reads flag int Ladder.apart Ladder.truth
  by_cases h : a = b <;> simp [h, Int.natCast_inj, BitVec.toNat_inj]

/-- `not x` is True where `x` is 0 (section 6.11). -/
theorem «unless» (a : Word) : reads 1 (flag (decide (int a = 0))) = Ladder.zero a := by
  unfold reads flag int Ladder.zero Ladder.truth
  by_cases h : a = 0 <;> simp [h, BitVec.toNat_eq]

/-! The compound assignments: `a op= b` gives `a` the value of `a op b` (section 7.2.1), and `a &= 0xffffffff`
then gives it the mask. -/

theorem add_assign (a b : Word) : mask (int a + int b) = Ladder.add a b := add a b
theorem sub_assign (a b : Word) : mask (int a - int b) = Ladder.take a b := sub a b
theorem mul_assign (a b : Word) : mask (int a * int b) = Ladder.product a b := mul a b
theorem shl_assign (a b : Word) (h : b.toNat < 32) : mask (int a * 2 ^ b.toNat) = Ladder.up a b := shl a b h
theorem shr_assign (a b : Word) (h : b.toNat < 32) :
    mask (Int.fdiv (int a) (2 ^ b.toNat)) = Ladder.down a b := shr a b h
theorem div_assign (a b : Word) (h : b ≠ 0) : mask (Int.fdiv (int a) (int b)) = Ladder.quotient a b := div a b h
theorem rem_assign (a b : Word) (h : b ≠ 0) : mask (Int.fmod (int a) (int b)) = Ladder.remainder a b := rem a b h
theorem bitand_assign (a b : Word) : mask ((a.toNat &&& b.toNat : Nat) : Int) = Ladder.both a b := bitand a b
theorem bitor_assign (a b : Word) : mask ((a.toNat ||| b.toNat : Nat) : Int) = Ladder.either a b := bitor a b
theorem bitxor_assign (a b : Word) : mask ((a.toNat ^^^ b.toNat : Nat) : Int) = Ladder.differ a b := bitxor a b

end Lng.python
