# The `.g` parser

**Purpose:** What a parser of gnascor's high order face (`.g`) wants. `.g` takes C's syntax style and is not C: no rule of C's binds it past that, reserved identifiers included. Each want is stated in the terms of C11 (ISO/IEC 9899:2011), the section of the standard it follows, the language table its words come from, and what stands open between it and a parser that reads it.
**Scope:** `.g` source as [../cu/transpiler/gnascor.md](../cu/transpiler/gnascor.md) describes it: semantic plain language plus the shortcut operators, the query protocol's syntax, and the switch into `.gsm`. The `.gsm` parser is [transpiler_gsm_parser.md](transpiler_gsm_parser.md). The words are rows of [transpiler_lng.tsv](transpiler_lng.tsv) and [gnascor_hol_lng.tsv](gnascor_hol_lng.tsv), checked by [lng_check.py](lng_check.py).
**Status:** Every row is a want. No `.g` parser is built.

`.g` is not C. It takes C's syntax style: tokens as C11 §6.4 has them, expressions by C11 §6.5's precedence, declarations and statements as §6.7 and §6.8 have them, a statement ended by `;` and a block held in `{}`. Where `.g` has a word for an operator, the word is the row of [gnascor_hol_lng.tsv](gnascor_hol_lng.tsv) that marks the operator's column, and a core op takes the name Rust gives it. Asm is written only inside `__gsm__(...)`: a state, a mnemonic, a pair and a transition appear nowhere else in `.g`. Everything else is plain C, written as C writes it: the translation phases and the preprocessor, comments line and block, a line continued by `\`, calls, casts, `&&` and `||`, loads and stores through a pointer, members by `.` and by `->`, declarations, `const`, atomics, blocks, tests against 0, select, selection, loops, jumps and labels. A want whose words no table holds says so, and the name is Doug's to give.

## Translation phases and the preprocessor (C11 §5.1.1.2, §6.10)

| want | C form | where its words are | what stands open |
| --- | --- | --- | --- |
| a line continued onto the next | phase 2, `\` then a newline, the two deleted | plain C | none |
| comments, each read as one space | phase 3; `//` to the end of the line, `/* */` across lines and not nested (§6.4.9) | `//` is the column `note` marks | none |
| the preprocessor | phase 4: `#include`, `#define` of objects and of functions with `#` and `##`, `#undef`, `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`, `#line`, `#error`, `#pragma` and `_Pragma` (§6.10) | plain C | none |
| a `.gsm` file brought into `.g` | `use bigmul.gsm`, which the preprocessor writes as `__gsm__(bigmul.gsm)` | `use` in [gnascor_hol_lng.tsv](gnascor_hol_lng.tsv) | none |
| a function of a used `.gsm` file | called by the name and the parameters the file defines | the `.gsm` file's definitions | none: the preprocessor holds each function as a lambda in an op block, `.g` knows it from there, and a call to it is a primitive; its body stays asm, inside the op block |
| the predefined macros | `__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__STDC__` and the rest (§6.10.8) | plain C | none |
| escape sequences in character constants and string literals | phase 5 (§6.4.4.4) | plain C | none |
| adjacent string literals joined | phase 6 | plain C | none |

## Tokens (C11 §6.4)

| want | C form | where its words are | what stands open |
| --- | --- | --- | --- |
| the five kinds of token: keyword, identifier, constant, string literal, punctuator | §6.4 | the punctuators are the operator columns of both language tables | none: `@` opens an address and `$` a cost bound, below |
| character constants and string literals | §6.4.4.4, §6.4.5 | plain C | none |
| the digraphs | §6.4.6, `<:`, `:>`, `<%`, `%>`, `%:`, `%:%:` | plain C | none |
| identifiers | §6.4.2, a letter or `_`, then letters, digits and `_` | none: an identifier names a value, not a word of the language | `is identical to` holds spaces and is no single token of C |
| the integer types, any power-of-two width with no ceiling | identifiers of the form `i<n>_t` and `u<n>_t`, the shape of `<stdint.h>`'s `int32_t` (§7.20.1.1) | the `type` rows of [gnascor_hol_lng.tsv](gnascor_hol_lng.tsv); widths as `exact_integer_widths.h` holds them | a width past 64 has no transpiler thing word; it is a chain of 32-bit limbs no form writes yet |
| the exact float types | identifiers `efloat<n>` and `edouble<n>` | the `type` rows of [gnascor_hol_lng.tsv](gnascor_hol_lng.tsv); fields as `double_fields` holds them | no row says which widths `n` takes, nor how many of `n` bits the exponent holds |
| integer constants, exact | §6.4.4.1, decimal, octal and hexadecimal | none | C's suffixes `u`, `l` and `ll` fix a width; an exact constant takes the width that holds it, and no suffix is named for that |
| floating constants, exact | §6.4.4.2 | none | a decimal constant that does not terminate in binary is not exact; `decimal_double` reads whether it does, and the parser's answer when it does not is open |

## Expressions (C11 §6.5, by precedence)

| want | C form | where its words are | what stands open |
| --- | --- | --- | --- |
| a primary expression: an identifier, a constant, a parenthesized expression | §6.5.1 | none | none |
| a generic selection | §6.5.1.1, `_Generic` | plain C | none |
| a subscript | §6.5.2.1, `a[i]` | plain C | none |
| a call | §6.5.2.2, `f()` | written as C writes it; the transpiler means it by `ask` | none |
| a member, and a member through a pointer | §6.5.2.3, `.` and `->` | written as C writes them; the transpiler means `.` by `x`, `y`, `z` | none |
| increment and decrement, postfix and prefix | §6.5.2.4, §6.5.3.1, `++` and `--` | plain C | none |
| a compound literal | §6.5.2.5, `(T){...}` | plain C | none |
| address and indirection | §6.5.3.2, `&x` and `*p` | plain C | none |
| sizes and alignments | §6.5.3.4, `sizeof` and `_Alignof` | plain C | the size of `i<n>_t` past 64 bits is the limbs that hold it, and no row says so |
| negation and complement | §6.5.3.3, `-x`, `~x`, `!x` | `neg`, `not` | Rust's `not` is both `~` and `!`; C keeps them apart, and the parser has to tell them by the operand's type |
| a cast | §6.5.4, `(T)` | written as C writes it | none |
| multiplicative | §6.5.5, `*`, `/`, `%` | `mul`, `div`, `rem` | no form writes `%` yet |
| additive | §6.5.6, `+`, `-` | `add`, `sub` | none |
| shift | §6.5.7, `<<`, `>>` | `shl`, `shr` | C fixes a shift past the width as undefined; an exact width has no edge to pass |
| relational | §6.5.8, `<`, `>`, `<=`, `>=` | `lt`, `gt`, `le`, `ge` | none |
| equality | §6.5.9, `==`, `!=` | `eq`, `ne`; the plain face's `is identical to` | none |
| bitwise | §6.5.10 to §6.5.12, `&`, `^`, `\|` | `bitand`, `bitxor`, `bitor` | none |
| logical | §6.5.13 and §6.5.14, `&&`, `\|\|` | written as C writes them; the transpiler glues them with `and` and `or` | none |
| conditional | §6.5.15, `? :` | written as C writes it; the transpiler means it by `select` | none: a `?` right after an address is the ask's call, below |
| assignment and compound assignment | §6.5.16, `=`, `+=` and the rest | `equals` for `=`; `add_assign` to `shr_assign` for the compound forms | none |
| the comma | §6.5.17 | none | none |

## Declarations and statements (C11 §6.7, §6.8)

| want | C form | where its words are | what stands open |
| --- | --- | --- | --- |
| a declaration | §6.7, `T x = e;` | the `type` rows for `T` | the plain face, `a equals something;`, declares no type; where the type comes from is open |
| storage classes and `typedef` | §6.7.1, `static`, `extern`, `_Thread_local`, `typedef` | plain C | none |
| structures, unions and enumerations | §6.7.2.1, §6.7.2.2 | plain C | none |
| qualifiers | §6.7.3, `const`, `volatile`, `restrict`, `_Atomic` | plain C | none |
| function specifiers and alignment | §6.7.4, §6.7.5, `inline`, `_Noreturn`, `_Alignas` | plain C | none |
| initializers, designated and with a trailing comma | §6.7.9, `{ .x = 1, [2] = 3, }` | plain C | none |
| a static assertion | §6.7.10, `_Static_assert` | plain C | none |
| a function definition | §6.9.1 | plain C | none |
| a compound statement | §6.8.2, `{ }` | written as C writes it; the transpiler means it by `open`, `body`, `close` | none |
| an expression statement | §6.8.3, `e;` | `evaluate` gives the true or false answer of what follows it | `evaluate` matches no operator of C: in the shortcut face the answer is the value of `a==b`, and `evaluate` has no column |
| selection | §6.8.4, `if`, `switch` | written as C writes them; the transpiler means them by `if`, `unless`, `dispatch` | none |
| iteration | §6.8.5, `while`, `do`, `for` | written as C writes them; the transpiler writes a loop as `label_loop` and `loop_back_if` | none |
| jumps | §6.8.6, `goto`, `continue`, `break`, `return` | written as C writes them; the transpiler means them by `back`, `exit`, `return` | none |
| labels | §6.8.1, `label:` | written as C writes them; the transpiler means one by `label` | none |

## The query protocol and the two faces

| want | C form | where its words are | what stands open |
| --- | --- | --- | --- |
| an address | `@` then a dotted name, `@system.auth` | gnascor's protocol components | none |
| a qualifier, called at the address | `?` then an identifier, `@db.user_session?is_valid` | gnascor's protocol components | none: `?` is the call, as `x = func(val)` calls `func` |
| a cost bound | `($...)` after the qualifier | gnascor's protocol components | gnascor holds that a bound is derived and never written, and its own example writes `$5ms`; a parser either rejects a written bound or reads it, and which is open |
| a pair of branches | `[left, right]` | gnascor's syntax architecture | none: a pair is asm, written only inside `__gsm__(...)` and read by [transpiler_gsm_parser.md](transpiler_gsm_parser.md) |
| resolution to a mnemonic | `output -> evaluate(...)` | gnascor's syntax architecture | resolution to a mnemonic is asm, written only inside `__gsm__(...)`, and `->` outside it is member access through a pointer |
| the face a block is written in | none given | gnascor: a block declares which face it is written in | none: `__gsm__(...)` is the only switch, and everything outside it is the high order face |
| the switch into `.gsm` | `__gsm__(...)` | gnascor's two faces | none: the text inside, or the `.gsm` file it names, is handed to [transpiler_gsm_parser.md](transpiler_gsm_parser.md) |
| the plain face and the shortcut face held to one meaning | `a equals something;` against `a=0;` | `equals`, `evaluate`, `is identical to` | both faces compile to 2 reads, 1 target and 1 store; the parser's two trees for them being one tree is the check, and it is not written |

## What the parser hands to the transpiler

| want | C form | where its words are | what stands open |
| --- | --- | --- | --- |
| each node named as a form is named: the thing it writes, then what is done to it | the operand's type and the operator | the `transpiler` column of [gnascor_hol_lng.tsv](gnascor_hol_lng.tsv): `u32_t` to `word`, `i64_t` to `signed_wide`; the operator's core op name | none past 64 bits, above |
| a sign the operator ignores dropped from the name | `+` on `u32_t` and on `i32_t` | `wide_add` takes no sign | none |
| every word the parser emits held in a table | none | [lng_check.py](lng_check.py) | the parser is not written, and nothing it emits is checked yet |
