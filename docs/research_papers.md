# Where to start reading

The research is written up as twenty research papers in [`theory/`](https://github.com/dstroy0/orior/tree/main/theory). They are typeset with XeLaTeX, and one command builds all of them:

```sh
sh utils/maint/texbuild/build_theory.sh
```

To read the code instead of the papers, go in this order:

1. [`archive/src/python/README.md`](https://github.com/dstroy0/orior/blob/main/archive/src/python/README.md)
2. [`src/README.md`](https://github.com/dstroy0/orior/blob/main/src/README.md)
3. [`examples/README.md`](https://github.com/dstroy0/orior/blob/main/examples/README.md)
4. [`examples/any_corpus/`](https://github.com/dstroy0/orior/tree/main/examples/any_corpus)

## Workbooks

A workbook is the running record of one line of work. It keeps every claim, including the ones that turned out wrong.

### [Workbook](https://github.com/dstroy0/orior/tree/main/theory/workbooks/orior)

_Every claim, what killed it, and what still stands._ This is my workbook for building and testing the general method. It keeps every idea I tested and every result that proved one wrong.

### [Engine](https://github.com/dstroy0/orior/tree/main/theory/workbooks/engine)

_The engine in math and in the machine._ Every claim, what supports it, and what it still needs.

### [Compression](https://github.com/dstroy0/orior/tree/main/theory/workbooks/compression)

_The floor the coder answers to._ Every claim, what supports it, and what it still needs.

### [Cell Tracking](https://github.com/dstroy0/orior/tree/main/theory/workbooks/cell_tracking)

_The cell program._ Every claim, what supports it, and what it still needs.

## Theory

### [Salishan](https://github.com/dstroy0/orior/tree/main/theory/theory/Salishan)

_Whose words these are, and how wrong the corpus could be._ Everything in this work is measured against one body of text: Salishan speech, written down. This paper says whose words those are, how wrong the record could be, and what was done to check.

### [Crystallography](https://github.com/dstroy0/orior/tree/main/theory/theory/crystallography)

_A published cell edge read back off a voxel grid, and whose result that is._ A crystal comes with an answer that someone else wrote down before this tool existed. Nothing else in this work can be checked that way.

### [Game Theory](https://github.com/dstroy0/orior/tree/main/theory/theory/game_theory)

_A domain that supplies its own answers, and the reading it corrected._ Games come with their own answers, and that makes checking free. The Grundy sequence of a subtraction game is proved to repeat in the end. Its period can be found by a plain exact routine that just compares values.

### [Particle Physics](https://github.com/dstroy0/orior/tree/main/theory/theory/particle_physics)

_Exactness, discrimination, and the accumulation of matter._ This paper treats an atom as a set of parts. An element is the electrons of its neutral atom, and each electron is a whole number quantum state. The whole periodic table is one accumulation of those states.

### [Chemistry](https://github.com/dstroy0/orior/tree/main/theory/theory/chemistry)

_The atom as a part, valence as the rule, the bond length as a fact._ Chemistry asks the same question with the atom as the part. Nothing in this paper has been measured here yet. The predictions are written down first, before anyone checks them.

### [SHA-256](https://github.com/dstroy0/orior/tree/main/theory/theory/cryptography/sha256)

_Where the structure is, where it stops, and how each null was measured._ The baseline is measured under the same conditions as the effect.

### [Delta Null](https://github.com/dstroy0/orior/tree/main/theory/theory/delta_null)

_Precision Measurement._ Two results limit what the search part can do. Never losing a true match costs nothing. Reading the result does cost something.

### [Image Transforms](https://github.com/dstroy0/orior/tree/main/theory/theory/image_transforms)

_How a view moves, how what it watches moves, and how to tell them apart._ When you track an object through a series of images, in any number of dimensions, the view you see it through can move too.

### [The Millennium Problems](https://github.com/dstroy0/orior/tree/main/theory/theory/millennium)

_The corpus, the state of the field, and what this toolkit reaches._ This is a survey. It claims no result.

### [The Apparatus](https://github.com/dstroy0/orior/tree/main/theory/theory/apparatus)

_The constants, the unit, and the instrument that draws the result._

### [Reading a Boundary](https://github.com/dstroy0/orior/tree/main/theory/theory/boundary)

_What a surface holds, the alphabet that reads it, and where the reading stops._

### [The Instruments](https://github.com/dstroy0/orior/tree/main/theory/theory/instruments)

_What a measurement can see, what it cannot, and how each null was drawn._

### [Precision](https://github.com/dstroy0/orior/tree/main/theory/theory/precision)

_Certificates instead of tables, and the constants nobody checks._

## Thought experiments

These are ideas whose experiment can't be built as written. Each one comes with the test that would decide it.

### [A Constructed Far End](https://github.com/dstroy0/orior/tree/main/theory/thought_experiments/orior)

_The method applied to ideas._ It covers:

- a reference built from an object's own parts and laid over the object;
- the delta null idea: three claims, of which two hold and one fails;
- what a closed boundary holds;
- the notebook's testable ideas, each with its test.

Ideas that have no test yet are kept as wants in the workbook.

### [The Engine's Ideas, Stated So They Can Be Decided](https://github.com/dstroy0/orior/tree/main/theory/thought_experiments/engine)

Sensor noise, threshold search, state vectors, dwell and entropy, self-reproduction, learned rulesets and identity, each with the test that decides it. Engine claims that have no such test yet are kept as wants in the engine workbook.

### [Cell Lineage from Shape and Whole-Sequence Evidence](https://github.com/dstroy0/orior/tree/main/theory/thought_experiments/cell_tracking)

_A proposal for the Biohub cell tracking task._ Each part comes with the test that decides it.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
