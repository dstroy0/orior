# The language: gnascor

The goal is to compile a program written in gnascor into any language, even one nobody has seen before, and to prove that it is the same program everywhere. When the target language is unknown, the engine works it out by asking it questions.

- [gnascor.md](https://github.com/dstroy0/orior/blob/main/src/cu/transpiler/gnascor.md) describes the language part by part.
- [engine_plan.md](https://github.com/dstroy0/orior/blob/main/src/engine_plan.md) lists the work still open.

There are two pieces:

- **gnascor** is the internal language. `.g` is the high order language and `.gsm` is its assembly. gnascor is designed, not discovered, and it is what you write programs in. Its vocabulary is fixed.
- **`L*`** maps gnascor onto each target's own definitions. This is the part the engine works out.

**The language rests on one idea: information is coherence.** A description at its Kolmogorov complexity has no repetition in it. Every bit counts, and no part of it predicts another. A system at full coherence has the same property seen from the other side: its parts agree, and the friction between them is as low as it can go. The idea is that compression and coherence are the same measurement taken from two directions. This is the starting point of the design. It is not a result, and each part built on it is tested on its own.

Before you meet any system, what you know is relations. `1,1 -> 2` is a relation, not an addition, because addition is a definition. Every system that computes agrees on the relation, and each one defines it in its own way.

## The query protocol

Every question the engine asks takes the same form, and working out a language is built from these questions:

    [ address ] -> ( qualifier ) -> [ measured cost ] -> binary result (1 or 0)

- **The address** names the target. It can be a memory address, a URI, an API endpoint, an LLM context key, a register, or a key of `Lstar.klq`. `Lstar.klq` is the bridge between languages: it is keyed by the schema's form names and read through each language's `.klm`.
- **The qualifier** is a yes or no question asked at that address. It always asks the target to confirm a state and never asks it for data.
- **The cost bound** is the most the target may spend to answer. Nobody writes this field by hand. A question asked with no bound gets back the cost instead of a bit. The spread of those costs becomes the baseline, and every later bound is set against it.

**Gate first, then rank. Never one combined score.** A relation either holds or it doesn't, and that answer has no noise in it. A cost is measured, and every measured cost has noise. The gate decides which candidates are allowed, and the rank puts the survivors in order. The two are never added together. [query_protocol_table.md](https://github.com/dstroy0/orior/blob/main/theory/workbooks/engine/query_protocol_table.md) walks through the protocol step by step.

Two branches combine into a pair, and each pair has a four-letter name:

| left branch | right branch | pair state | mnemonic | meaning                                                     |
| ----------- | ------------ | ---------- | -------- | ----------------------------------------------------------- |
| lead (1)    | void (0)     | 1, 0       | core     | The primary intent persists; the secondary path dissolved.  |
| rite (0)    | lead (1)     | 0, 1       | shift    | Focus has migrated from the left domain to the right.       |
| dual (2)    | void (0)     | 2, 0       | echo     | An amplified state is sustained without new external input. |
| dual (2)    | dual (2)     | 2, 2       | nexus    | Maximum systemic coherence; both major systems are aligned. |

## The transpiler

The transpiler has two jobs. It writes the record machine's programs for a particular part, and it asks the questions that teach the engine about that part.

- `keymath` writes the record programs.
- `key_schedule` lays them out.
- `cycle` runs them on the device and checks them against a reference run on the host.

The code generator writes each program's lane from a ruleset, with one `.krs` file per language. Every lane the device writes is checked word for word against the host's.

| directory                                                                                                  | what it holds                                                                                                                                                       |
| ---------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [`src/cu/transpiler/lstar/protocol/`](https://github.com/dstroy0/orior/tree/main/src/cu/transpiler/lstar/protocol)     | the query protocol on the host: one ask (`query_ask`), every arrangement of primitives that produces a relation (`chain_build`), and the order of asks (`ask_order`) |
| [`src/cu/transpiler/codegen/`](https://github.com/dstroy0/orior/tree/main/src/cu/transpiler/codegen)       | the code generator's kernels, and its rulesets: `c.krs`, `ptx.krs`, `sass.krs`, `vhdl.krs` and `yosys.krs`                                                          |
| [`src/cu/scaffolding/`](https://github.com/dstroy0/orior/tree/main/src/cu/scaffolding)             | one line of SASS turned into the sixteen bytes the part runs, and a cubin written from a kernel's machine code; `machines/sm_86.kdm` and `sm_86.ksc`                  |
| [`src/cu/scaffolding/`](https://github.com/dstroy0/orior/tree/main/src/cu/scaffolding)               | one emitter, every container: it reads a layout file and writes what that layout describes                                                                          |
| [`src/cu/transpiler/lstar/interface/`](https://github.com/dstroy0/orior/tree/main/src/cu/transpiler/lstar/interface)     | the cell, a probe runner: a probe asks the target one question in a child process the cell can lose                                                                 |
| [`examples/qasm/`](https://github.com/dstroy0/orior/tree/main/examples/qasm)                                         | exact qubit states, read from OpenQASM                                                                                                                              |

The method works like this: write C source, read the SASS it compiles to, and compare that with what NVIDIA's compiler writes for the same program (Q17). Every slot that a `.krs` file fills in by hand is checked by asking the part, the same way `loop_back_if` is asked of sm_86 (Q16).

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
