/-! The high order words, the anchors of the transpiler's words and the .gsm files, written by harness.py
lng prove. -/

namespace Lng.GRows

structure HolRow where
  word : String
  type : String
  bits : String
  signed : String
  transpiler : String
  symbols : List String

def holRows : List HolRow := [
  ⟨"i<n>_t", "type", "n", "1", "", []⟩,
  ⟨"u<n>_t", "type", "n", "0", "", []⟩,
  ⟨"efloat<n>", "type", "n", "", "", []⟩,
  ⟨"edouble<n>", "type", "n", "", "", []⟩,
  ⟨"i8_t", "type", "8", "1", "signed_byte", []⟩,
  ⟨"u8_t", "type", "8", "0", "byte", []⟩,
  ⟨"i16_t", "type", "16", "1", "signed_halfword", []⟩,
  ⟨"u16_t", "type", "16", "0", "halfword", []⟩,
  ⟨"i32_t", "type", "32", "1", "signed_word", []⟩,
  ⟨"u32_t", "type", "32", "0", "word", []⟩,
  ⟨"i64_t", "type", "64", "1", "signed_wide", []⟩,
  ⟨"u64_t", "type", "64", "0", "wide", []⟩,
  ⟨"equals", "plain", "", "", "", ["="]⟩,
  ⟨"evaluate", "plain", "", "", "", []⟩,
  ⟨"is identical to", "plain", "", "", "eq", ["=="]⟩,
  ⟨"add", "core op", "", "", "", ["+"]⟩,
  ⟨"sub", "core op", "", "", "", ["-"]⟩,
  ⟨"mul", "core op", "", "", "", ["*"]⟩,
  ⟨"div", "core op", "", "", "", ["/"]⟩,
  ⟨"rem", "core op", "", "", "", ["%"]⟩,
  ⟨"neg", "core op", "", "", "", ["-x"]⟩,
  ⟨"not", "core op", "", "", "", ["~x"]⟩,
  ⟨"unless", "core op", "", "", "", ["!x"]⟩,
  ⟨"bitand", "core op", "", "", "", ["&"]⟩,
  ⟨"bitor", "core op", "", "", "", ["|"]⟩,
  ⟨"bitxor", "core op", "", "", "", ["^"]⟩,
  ⟨"shl", "core op", "", "", "", ["<<"]⟩,
  ⟨"shr", "core op", "", "", "", [">>"]⟩,
  ⟨"add_assign", "core op", "", "", "", ["+="]⟩,
  ⟨"sub_assign", "core op", "", "", "", ["-="]⟩,
  ⟨"mul_assign", "core op", "", "", "", ["*="]⟩,
  ⟨"div_assign", "core op", "", "", "", ["/="]⟩,
  ⟨"rem_assign", "core op", "", "", "", ["%="]⟩,
  ⟨"bitand_assign", "core op", "", "", "", ["&="]⟩,
  ⟨"bitor_assign", "core op", "", "", "", ["|="]⟩,
  ⟨"bitxor_assign", "core op", "", "", "", ["^="]⟩,
  ⟨"shl_assign", "core op", "", "", "", ["<<="]⟩,
  ⟨"shr_assign", "core op", "", "", "", [">>="]⟩,
  ⟨"eq", "core op", "", "", "", ["=="]⟩,
  ⟨"ne", "core op", "", "", "", ["!="]⟩,
  ⟨"lt", "core op", "", "", "", ["<"]⟩,
  ⟨"le", "core op", "", "", "", ["<="]⟩,
  ⟨"gt", "core op", "", "", "", [">"]⟩,
  ⟨"ge", "core op", "", "", "", [">="]⟩,
  ⟨"use", "plain", "", "", "", []⟩
]

def anchors : List (String × String) := [
  ("set", "copy"),
  ("add", "add"),
  ("sub", "take"),
  ("mul", "product"),
  ("div", "quotient"),
  ("bitand", "both"),
  ("bitor", "either"),
  ("bitxor", "differ"),
  ("shl", "up"),
  ("shr", "down"),
  ("neg", "negate"),
  ("lt", "less"),
  ("le", "at_max"),
  ("gt", "more"),
  ("ge", "at_least"),
  ("eq", "same"),
  ("ne", "apart"),
  ("unless", "zero"),
  ("not", "invert"),
  ("rem", "remainder"),
  ("add_assign", "add"),
  ("sub_assign", "take"),
  ("mul_assign", "product"),
  ("div_assign", "quotient"),
  ("rem_assign", "remainder"),
  ("bitand_assign", "both"),
  ("bitor_assign", "either"),
  ("bitxor_assign", "differ"),
  ("shl_assign", "up"),
  ("shr_assign", "down")
]

def gsmFiles : List (String × List (List Nat × Nat)) := [
  ("sm_86.gsm", [([1, 1], 2), ([1, 0], 1), ([0, 1], 1), ([0, 0], 0), ([3, 5], 8), ([7, 9], 16), ([4294967295, 1], 0), ([2147483647, 1], 2147483648)])
]

end Lng.GRows
