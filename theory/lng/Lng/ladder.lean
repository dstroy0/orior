import Std.Tactic.BVDecide

/-!
# The ladder, the relations of gsm

The anchors of ladder.h (src/cu/transpiler/lstar/protocol/counterexample), each as a relation on the word of 32
places. A target's symbol goes to gsm through its bridge word, and the bridge word names one of these anchors. A
language file proves its symbols against them, for every word and not only on the ladder's cases.

Each relation here is the relation that `ladder_answer` computes. A predicate answers a word of ones where it is
true and 0 where it is false. UP and DOWN read their count from the low five places of the second word. QUOTIENT
and REMAINDER have no answer for a divisor of 0, and their theorems take a divisor that is not 0.
-/

namespace Lng.Ladder

/-- The word of 32 places that every relation of the ladder reads and answers. -/
abbrev Word := BitVec 32

/-- True as the ladder writes it: a word of ones. -/
def ones : Word := BitVec.allOnes 32

/-- The answer of a predicate: ones where `p` is true, and 0 where it is false. -/
def truth (p : Bool) : Word := if p then ones else 0

/-- The count that UP and DOWN read: the low five places of the second word. -/
def count (b : Word) : Nat := b.toNat % 32

def same (a b : Word) : Word := truth (a == b)
def add (a b : Word) : Word := a + b
def take (a b : Word) : Word := a - b
def up (a b : Word) : Word := a <<< count b
def down (a b : Word) : Word := a >>> count b
def product (a b : Word) : Word := a * b
def less (a b : Word) : Word := truth (a.ult b)
def atMax (a b : Word) : Word := truth (a.ule b)
def more (a b : Word) : Word := truth (b.ult a)
def atLeast (a b : Word) : Word := truth (b.ule a)
def apart (a b : Word) : Word := truth (a != b)
def quotient (a b : Word) : Word := a / b
def remainder (a b : Word) : Word := a % b
def both (a b : Word) : Word := a &&& b
def either (a b : Word) : Word := a ||| b
def differ (a b : Word) : Word := a ^^^ b
def negate (a : Word) : Word := -a
def invert (a : Word) : Word := ~~~a
def zero (a : Word) : Word := truth (a == 0)
def copy (a : Word) : Word := a

/-- The reading of a predicate symbol that answers `t` for true and 0 for false. The ladder reads it as it reads
SAME: `t` is ones, and every other answer is 0. -/
def reads (t s : Nat) : Word := if s = t then ones else 0

end Lng.Ladder
