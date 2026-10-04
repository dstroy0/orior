# The `.gsm` parser

**Purpose:** What a parser of gnascor's assembly face (`.gsm`) wants. The face is relational: a statement is a pair's state and the transition between two of them, and a mnemonic is the label that transition is read off as. Each want is stated beside the language table its words come from and what stands open.
**Scope:** `.gsm` source, the text of a `__gsm__(...)` block inside `.g`, and a `.gsm` file `.g` brings in by `use`, as [../c/transpiler/gnascor.md](../c/transpiler/gnascor.md) describes them: the branch pair, the shifts, the environmental base states, and both transition matrices. The `.g` parser is [transpiler_g_parser.md](transpiler_g_parser.md). The words are rows of [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv), checked by [lng_check.py](lng_check.py).
**Status:** Every row is a want. No `.gsm` parser is built.

`.gsm` asks a part how it does math, in the most basic questions there are: `1,1->2` asks whether the part answers 2 for 1 and 1. The answers, held or refused inside the bound, are the sides of a pair, and the states and mnemonics below read them. `.gsm` follows no rule of C's syntax. Inside `.g` it is written only in a `__gsm__(...)` block, and nowhere else. Its identifiers are only the states and the mnemonics of [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv). A transition is the state it left and the state it reached, and the parser keeps that pair beside every mnemonic it reads: a label shared by many transitions then loses none of them (gnascor, Open 4).

## Tokens

| want | form | where its words are | what stands open |
| --- | --- | --- | --- |
| the states | `dual`, `lead`, `rite`, `void`, `busy`, `wait`, `blok`, `gray`, each the case it reads: `lead` is `1,0->1` | the `past` and `present` columns of [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv) | none |
| the mnemonics | every word in the `mnemonic` column | [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv) | gnascor calls them four-letter, and `nexus` and `shift` hold five |
| a word that is both a state and a mnemonic | one word | the high-energy matrix names wait to lead `lead` and wait to rite `rite` | one word read two ways needs a rule between them, and there is none |
| a pair's bits | `(left, right)`, each 0 or 1 | the `left` and `right` bit columns | the branch pair table writes dual as `(2)` and rite as `(0)` on one side; both matrices write dual `(1,1)` and rite `(0,1)`; the parser can read one of the two |
| comments | none given | none | gnascor writes `.gsm` with no comment form |

## Statements

| want | form | where its words are | what stands open |
| --- | --- | --- | --- |
| a case asked of the part | `1,1->2`: the operands, then the answer the relation holds | the cases of the gate, gnascor's query protocol | none |
| a pair resolved to its state | `[left, right]` | the branch pair table, and gnascor's direct protocol execution | gnascor's direct protocol execution reads `1, 1` as dual, and its branch pair table reads `2, 2` as nexus |
| a cross: a transition where both sides flip | `x>`: `lead x> rite`, `1,0 x> 0,1` | the rows of [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv) whose `arrow` is `x>` | none |
| a straight line: a transition where the sides do not cross | `->`: `lead -> void`, `1,0 -> 0,0` | the rows of [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv) whose `arrow` is `->` | none |
| the unknown or a superstate: a transition where a side holds no pair bit | `*>`: `busy *> dual` | the rows of [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv) whose `arrow` is `*>` | none |
| a halt | `\|` | gnascor's environmental base states | what a program does after one is gnascor's Open 3 |
| a transition read off as its mnemonic | the row whose `past` and `present` match | [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv) | dual to lead is drop in the state-transition matrix and tilt in the high-energy one, and dual to void is drop and fuse; which matrix a transition is read in is open |
| a mnemonic written, held against its pair | `mnemonic` where a transition is | the `left` and `right` operations of each row | drop, join, tilt and wake each cover transitions that do different things to the pair, and the mnemonic alone does not give the pair back |
| a sequence of states, read as one movement | `[lead, rite]` gives pass | the shift mnemonics | `[ ]` is the same bracket as a pair; a pair is two branches at once and a sequence is one state over two cycles, and one syntax for both is open |
| sync | one word | 22 transitions of the state-transition matrix | a parser that reads sync alone has lost which of the 22 it was; the pair kept beside it is the answer gnascor gives |
| the transitions with no recovery path | `jamm`, `halt`, `fuse` | [gnascor_asm_lng.tsv](gnascor_asm_lng.tsv) | gnascor, Open 3: what a program does when a branch yields one is not defined |
| the operators of gnascor's gray line | `->`, `<->`, `x>`, `\|` | gnascor's environmental base states | `-` is a straight line, `x` a cross, `*` the unknown or a superstate, and `\|` a halt; `->` also reads a case; `<->` has no meaning written |

## Where it meets the `.g` parser and the transpiler

| want | form | where its words are | what stands open |
| --- | --- | --- | --- |
| the text of a `__gsm__(...)` block | everything up to the matching `)` | [transpiler_g_parser.md](transpiler_g_parser.md) | none |
| a `.gsm` file brought into `.g` | `use bigmul.gsm`, wrapped by the preprocessor as `__gsm__(bigmul.gsm)`, and the file read whole | [transpiler_g_parser.md](transpiler_g_parser.md) | none |
| a function a `.gsm` file defines, ready for `.g` to call | held as a lambda in an op block, a primitive `.g` calls as the file defines it; its body is the arrangement the part's answers settled, as a `.kdm` row holds one | [transpiler_g_parser.md](transpiler_g_parser.md) | gnascor does not yet write how a `.gsm` file defines a function's name and parameters |
| a `.g` statement and its `.gsm` text held to one meaning | none | gnascor: the two faces are the same assembly | no row maps a mnemonic to a transpiler form, and the `.gsm` a `.g` statement compiles to is not derivable from the tables yet |
| every word the parser reads held in a table | none | [lng_check.py](lng_check.py) | the parser is not written, and nothing it reads is checked yet |
