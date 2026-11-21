import Lng.c_lng
import Lng.python_lng

/-!
# The lift from C to Python

Each theorem here lifts one symbol of C to its symbol of Python, through the bridge word that both charts share.
Each joins the theorem of C for that bridge word to the theorem of Python for it. Both equal the
relation of the anchor, and so they equal each other on every word where both theorems hold.

Each theorem has the name of its bridge word, as the theorems of c_lng.lean and python_lng.lean do.
-/

namespace Lng.lift

open Lng.Ladder

theorem add (a b : Word) : a + b = Lng.python.mask (Lng.python.int a + Lng.python.int b) :=
  (Lng.c.add a b).trans (Lng.python.add a b).symm
theorem sub (a b : Word) : a - b = Lng.python.mask (Lng.python.int a - Lng.python.int b) :=
  (Lng.c.sub a b).trans (Lng.python.sub a b).symm
theorem mul (a b : Word) : a * b = Lng.python.mask (Lng.python.int a * Lng.python.int b) :=
  (Lng.c.mul a b).trans (Lng.python.mul a b).symm
theorem div (a b : Word) (h : b ≠ 0) :
    a / b = Lng.python.mask (Int.fdiv (Lng.python.int a) (Lng.python.int b)) :=
  (Lng.c.div a b h).trans (Lng.python.div a b h).symm
theorem rem (a b : Word) (h : b ≠ 0) :
    a % b = Lng.python.mask (Int.fmod (Lng.python.int a) (Lng.python.int b)) :=
  (Lng.c.rem a b h).trans (Lng.python.rem a b h).symm
theorem bitand (a b : Word) : a &&& b = Lng.python.mask ((a.toNat &&& b.toNat : Nat) : Int) :=
  (Lng.c.bitand a b).trans (Lng.python.bitand a b).symm
theorem bitor (a b : Word) : a ||| b = Lng.python.mask ((a.toNat ||| b.toNat : Nat) : Int) :=
  (Lng.c.bitor a b).trans (Lng.python.bitor a b).symm
theorem bitxor (a b : Word) : a ^^^ b = Lng.python.mask ((a.toNat ^^^ b.toNat : Nat) : Int) :=
  (Lng.c.bitxor a b).trans (Lng.python.bitxor a b).symm
theorem neg (a : Word) : -a = Lng.python.mask (-(Lng.python.int a)) :=
  (Lng.c.neg a).trans (Lng.python.neg a).symm
theorem «not» (a : Word) : ~~~a = Lng.python.mask (-(Lng.python.int a) - 1) :=
  (Lng.c.not a).trans (Lng.python.not a).symm
theorem set (a : Word) : a = Lng.python.mask (Lng.python.int a) :=
  (Lng.c.set a).trans (Lng.python.set a).symm
theorem shl (a b : Word) (h : b.toNat < 32) : a <<< b.toNat = Lng.python.mask (Lng.python.int a * 2 ^ b.toNat) :=
  (Lng.c.shl a b h).trans (Lng.python.shl a b h).symm
theorem shr (a b : Word) (h : b.toNat < 32) :
    a >>> b.toNat = Lng.python.mask (Int.fdiv (Lng.python.int a) (2 ^ b.toNat)) :=
  (Lng.c.shr a b h).trans (Lng.python.shr a b h).symm
theorem lt (a b : Word) :
    reads 1 (Lng.c.flag (a.ult b)) = reads 1 (Lng.python.flag (decide (Lng.python.int a < Lng.python.int b))) :=
  (Lng.c.lt a b).trans (Lng.python.lt a b).symm
theorem le (a b : Word) :
    reads 1 (Lng.c.flag (a.ule b)) = reads 1 (Lng.python.flag (decide (Lng.python.int a ≤ Lng.python.int b))) :=
  (Lng.c.le a b).trans (Lng.python.le a b).symm
theorem gt (a b : Word) :
    reads 1 (Lng.c.flag (b.ult a)) = reads 1 (Lng.python.flag (decide (Lng.python.int a > Lng.python.int b))) :=
  (Lng.c.gt a b).trans (Lng.python.gt a b).symm
theorem ge (a b : Word) :
    reads 1 (Lng.c.flag (b.ule a)) = reads 1 (Lng.python.flag (decide (Lng.python.int a ≥ Lng.python.int b))) :=
  (Lng.c.ge a b).trans (Lng.python.ge a b).symm
theorem eq (a b : Word) :
    reads 1 (Lng.c.flag (a == b)) = reads 1 (Lng.python.flag (decide (Lng.python.int a = Lng.python.int b))) :=
  (Lng.c.eq a b).trans (Lng.python.eq a b).symm
theorem ne (a b : Word) :
    reads 1 (Lng.c.flag (a != b)) = reads 1 (Lng.python.flag (decide (Lng.python.int a ≠ Lng.python.int b))) :=
  (Lng.c.ne a b).trans (Lng.python.ne a b).symm
theorem «unless» (a : Word) :
    reads 1 (Lng.c.flag (a == 0)) = reads 1 (Lng.python.flag (decide (Lng.python.int a = 0))) :=
  (Lng.c.unless a).trans (Lng.python.unless a).symm
theorem add_assign (a b : Word) : a + b = Lng.python.mask (Lng.python.int a + Lng.python.int b) :=
  (Lng.c.add_assign a b).trans (Lng.python.add_assign a b).symm
theorem sub_assign (a b : Word) : a - b = Lng.python.mask (Lng.python.int a - Lng.python.int b) :=
  (Lng.c.sub_assign a b).trans (Lng.python.sub_assign a b).symm
theorem mul_assign (a b : Word) : a * b = Lng.python.mask (Lng.python.int a * Lng.python.int b) :=
  (Lng.c.mul_assign a b).trans (Lng.python.mul_assign a b).symm
theorem div_assign (a b : Word) (h : b ≠ 0) :
    a / b = Lng.python.mask (Int.fdiv (Lng.python.int a) (Lng.python.int b)) :=
  (Lng.c.div_assign a b h).trans (Lng.python.div_assign a b h).symm
theorem rem_assign (a b : Word) (h : b ≠ 0) :
    a % b = Lng.python.mask (Int.fmod (Lng.python.int a) (Lng.python.int b)) :=
  (Lng.c.rem_assign a b h).trans (Lng.python.rem_assign a b h).symm
theorem bitand_assign (a b : Word) : a &&& b = Lng.python.mask ((a.toNat &&& b.toNat : Nat) : Int) :=
  (Lng.c.bitand_assign a b).trans (Lng.python.bitand_assign a b).symm
theorem bitor_assign (a b : Word) : a ||| b = Lng.python.mask ((a.toNat ||| b.toNat : Nat) : Int) :=
  (Lng.c.bitor_assign a b).trans (Lng.python.bitor_assign a b).symm
theorem bitxor_assign (a b : Word) : a ^^^ b = Lng.python.mask ((a.toNat ^^^ b.toNat : Nat) : Int) :=
  (Lng.c.bitxor_assign a b).trans (Lng.python.bitxor_assign a b).symm
theorem shl_assign (a b : Word) (h : b.toNat < 32) :
    a <<< b.toNat = Lng.python.mask (Lng.python.int a * 2 ^ b.toNat) :=
  (Lng.c.shl_assign a b h).trans (Lng.python.shl_assign a b h).symm
theorem shr_assign (a b : Word) (h : b.toNat < 32) :
    a >>> b.toNat = Lng.python.mask (Int.fdiv (Lng.python.int a) (2 ^ b.toNat)) :=
  (Lng.c.shr_assign a b h).trans (Lng.python.shr_assign a b h).symm

end Lng.lift
