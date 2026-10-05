# The language: gnascor

**Purpose:** Say what gnascor is, what the query protocol asks, and where the transpiler keeps each piece.
**Scope:** `src/cu/transpiler/`, `src/cu/transpiler/`

The objective is to compile a program written in gnascor to any language, including one nobody has met, and prove it is the same program everywhere. Where the language is unknown, the engine derives it by asking. [gnascor.md](https://github.com/dstroy0/orior/blob/main/src/cu/transpiler/gnascor.md) holds the language part by part, and [engine_plan.md](https://github.com/dstroy0/orior/blob/main/src/engine_plan.md) the open work.

**gnascor** is the internal language, `.g` high order and `.gsm` its assembly. It is designed and it is what a program is written in. It is not derived and its vocabulary does not move. **`L*`** is the map from gnascor to a target's definitions, and it is the derived part.

**The language is built on one idea: information is coherence.** A description at its Kolmogorov complexity holds no redundancy, every bit of it carries, and no part predicts another. A system at coherence has that property from the other side: its parts agree and the friction between them is at its floor. The idea is that compression and coherence are one measurement from two directions. It is the ground the design stands on and not a result. Each part built on it is checked on its own.

What is known before meeting anything is relations. `1,1 -> 2` is a relation and is not an addition, because addition is a definition. Every system that computes agrees about the relation and each defines it its own way.

## The query protocol

The query protocol is the form every ask takes, and it is what derivation is made of:

    [ address ] -> ( qualifier ) -> [ measured cost ] -> binary result (1 or 0)

The address names the target: a memory address, a URI, an API endpoint, an LLM context key, a register, or a key of `Lstar.klq`, the bridge between languages keyed by the schema's form names, read through a language's `.klm`. The qualifier is a binary question asked at it, phrased to demand a state validation and never a data payload. The cost bound is the most the target may spend to answer, and no hand writes that field. An ask carrying no bound returns the cost instead of a bit. The spread of those costs is the baseline, and every bound after that is expressed against it.

**Gate, then rank. Never one score.** A relation holds or it does not, and that answer carries no noise. A cost is measured and every cost carries noise. The gate decides which candidates are admissible and the rank orders whatever survives, and the two are never added together. [query_protocol_table.md](https://github.com/dstroy0/orior/blob/main/theory/workbooks/engine/query_protocol_table.md) holds the protocol step by step.

Two branches resolve to a pair, and the pair to a four-letter mnemonic:

| left branch | right branch | pair state | mnemonic | meaning                                                     |
| ----------- | ------------ | ---------- | -------- | ----------------------------------------------------------- |
| lead (1)    | void (0)     | 1, 0       | core     | The primary intent persists; the secondary path dissolved.  |
| rite (0)    | lead (1)     | 0, 1       | shift    | Focus has migrated from the left domain to the right.       |
| dual (2)    | void (0)     | 2, 0       | echo     | An amplified state is sustained without new external input. |
| dual (2)    | dual (2)     | 2, 2       | nexus    | Maximum systemic coherence; both major systems are aligned. |

## The transpiler

The transpiler is the record machine's programs written for a part, and the asks that learn the part. `keymath` imprints record programs, `key_schedule` lays them out, and `cycle` runs them on the device with a host reference. The code generator writes each program's lane from a ruleset, one `.krs` a language, and every lane the device writes is held word for word against the host's.

| directory                                                                                                  | what it holds                                                                                                                                                       |
| ---------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [`src/cu/transpiler/lstar/protocol/`](https://github.com/dstroy0/orior/tree/main/src/cu/transpiler/lstar/protocol)     | the query protocol on the host: one ask (`query_ask`), every arrangement of primitives that produces a relation (`chain_build`), and the order of asks (`ask_order`) |
| [`src/cu/transpiler/codegen/`](https://github.com/dstroy0/orior/tree/main/src/cu/transpiler/codegen)       | the code generator's kernels, and its rulesets: `c.krs`, `ptx.krs`, `sass.krs`, `vhdl.krs` and `yosys.krs`                                                          |
| [`src/cu/scaffolding/`](https://github.com/dstroy0/orior/tree/main/src/cu/scaffolding)             | one line of SASS turned into the sixteen bytes the part runs, and a cubin written from a kernel's machine code; `machines/sm_86.kdm` and `sm_86.ksc`                  |
| [`src/cu/scaffolding/`](https://github.com/dstroy0/orior/tree/main/src/cu/scaffolding)               | one emitter, every container: it reads a layout file and writes what that layout describes                                                                          |
| [`src/cu/transpiler/lstar/interface/`](https://github.com/dstroy0/orior/tree/main/src/cu/transpiler/lstar/interface)     | the cell, a probe runner: a probe asks the target one question in a child process the cell can lose                                                                 |
| [`examples/qasm/`](https://github.com/dstroy0/orior/tree/main/examples/qasm)                                         | exact qubit states, read from OpenQASM                                                                                                                              |

The method is to write C source, read the SASS it compiles to, and hold it against what NVIDIA's compiler writes for the same program (Q17). Every slot a `.krs` writes by hand is asked of the part the way `loop_back_if` is asked of sm_86 (Q16).

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
