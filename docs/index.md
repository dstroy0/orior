---
hide:
  - navigation
  - toc
---

<div class="orior-hero" markdown>

<div class="orior-title" markdown>

# orior

A unified computational foundation.
{ .orior-lede }

</div>

Orior finds the pattern in anything, from a crystal to a language to a file. It compares the thing with a shuffled copy of itself, and the pattern is what the copy lost. Every number is exact, with nothing rounded, guessed or trained.

[Setup](setup.md){ .md-button .md-button--primary }
[The algorithm](method.md){ .md-button }
[The engine](engine.md){ .md-button }
[The transpiler](gnascor.md#the-transpiler){ .md-button }
[Kolmogorov Complexity filetypes](engine.md#the-files){ .md-button }
[Areas of research](research.md){ .md-button }

</div>

## Where to go

<div class="grid cards" markdown>

-   :material-play-circle-outline:{ .lg .middle } __[Using it](usage.md)__

    ---

    Setting up the engine to solve your problem

-   :material-code-braces:{ .lg .middle } __[The engine/transpiler's language:<br>gnascor](gnascor.md){ .orior-stacked }__

    ---

    The query protocol for the ruleset L*, relational gsm, higher order language g formats, constructs, and file definitions

-   :material-filter-variant:{ .lg .middle } __[The sift](sift.md)__

    ---

    No arrangement of anchors can lose a true occurrence

-   :material-check-decagram:{ .lg .middle } __[Why the count is exact](ENGINE_PROOF.md)__

    ---

    The proofs that every set returns their exact count, and self-terminates

-   :material-book-open-variant:{ .lg .middle } __[Where to start reading](research_papers.md)__

    ---

    Theory & Research

-   :material-account-voice:{ .lg .middle } __[The conditions of use](condition_of_use.md)__

    ---

    Be excellent to one another

</div>

## Quick start

From a fresh clone, at the repository root:

```sh
utils/maint/engine/build_engine.sh                                  # the C engine: configure, build, run the graders
python examples/any_corpus/4_measure/collision_entropy.py           # a reading that knows nothing about its corpus
python examples/crystallography/6_oracle/proof_positive_control.py  # the positive control, against published cells
sh utils/maint/texbuild/build_theory.sh                             # the research papers
```

## What is here

Nothing here asks to be believed: every result is traceable, every validated measurement could have failed but did not, and every claim the work took back is kept in its workbook. Most of the parts of this work are old and named as such. Their arrangements being glued together in orior using exact arithmetic with no exceptions even where the original authors allowed them or did not have access to vector calculus is what sets this work apart.

<div class="grid cards" markdown>

-   :material-infinity:{ .lg .middle } __Exact integers, from end to end__

    ---

    No rounding. No exceptions. Arbitrary precision throughout. No value is too large. A value too wide for its word is refused on compilation.

    [:octicons-arrow-right-24: The engine](engine.md)

-   :material-chart-bell-curve:{ .lg .middle } __The pattern is what a shuffle destroys__

    ---

    Keep the same pieces, shuffle their order, and the shuffled copy is the baseline. Whatever the shuffle wipes out is the pattern. A filter built from any part of a pattern never misses a true match, and every match is still checked in full.

    [:octicons-arrow-right-24: The algorithm](method.md)

-   :material-cube-outline:{ .lg .middle } __Any number of dimensions, the same memory__

    ---

    The filter keeps one bit for each place a match could start. Its memory doesn't grow with the alphabet or with the number of dimensions, and one formula gives its cost for anything from a line to an eight dimensional cube.

    [:octicons-arrow-right-24: The sift](sift.md)

-   :material-layers-triple:{ .lg .middle } __Exact steps joined ahead of time__

    ---

    A chain of exact steps is combined into one program before any data arrives, and runs on the device as one. It does the same work. It saves the time between the steps.

    [:octicons-arrow-right-24: The stack](https://github.com/dstroy0/orior/blob/main/theory/workbooks/engine/vertical_time_compression.md)

-   :material-package-down:{ .lg .middle } __Compression held to the noise of the camera__

    ---

    No program can compute Kolmogorov complexity, and nothing here claims to. On 25 volumes of cell tracking images, the noise of the camera means no file can get below 38.9 percent of the raw size. The engine gets to 42.0 percent.

    [:octicons-arrow-right-24: Compression](https://github.com/dstroy0/orior/tree/main/theory/workbooks/compression)

-   :material-chip:{ .lg .middle } __One program at every width__

    ---

    A program built only from sums, products, exclusive or and AND gives the same answer at every word width. The code writer turns it into PTX, C or SASS. When it doesn't know one of the rules of a target part, it asks the part.

    [:octicons-arrow-right-24: Two crystals](https://github.com/dstroy0/orior/blob/main/theory/workbooks/engine/two_crystals.md)

-   :material-eye-outline:{ .lg .middle } __Laplace's demon, and its bill__

    ---

    In practice, a boundary can rule things out but can't predict them. An exclusion is permanent and costs nothing. Each finer level of detail costs exponentially more precision. Nothing forbids prediction in principle. It just has that bill.

    [:octicons-arrow-right-24: Thought experiments](https://github.com/dstroy0/orior/tree/main/theory/thought_experiments/orior)

-   :material-clock-outline:{ .lg .middle } __What an input stops reaching is a clock__

    ---

    In SHA-256, by round seven there are 214 of 256 positions that no input bit can reach. By about round 30 of 64, every position is reached. Nothing here claims a weakness in SHA-256.

    [:octicons-arrow-right-24: Instruments](https://github.com/dstroy0/orior/tree/main/theory/theory/instruments)

-   :material-arrow-expand-all:{ .lg .middle } __Precision spread__

    ---

    Start with a few numbers known to enough digits, and every quantity an exact identity can reach from them comes out to the same number of digits. Two starting numbers, the square roots of 2 and 3, give 2,230,148 exact square roots up to 10^800.

    [:octicons-arrow-right-24: Precision](https://github.com/dstroy0/orior/tree/main/theory/theory/precision)

</div>

## What came back

<div class="orior-stats">
<div><strong>453 of 453</strong><span>crystal edges that match the published value exactly, as whole numbers, with no tolerance</span></div>
<div><strong>0 missed</strong><span>out of 9,396,207 true matches in byte strings, and out of 213,840 across one to eight dimensions</span></div>
<div><strong>1 of 200</strong><span>random borders did as well as the dialect border it found without ever seeing the labels</span></div>
<div><strong>383 of 383</strong><span>subtraction games where it found the right Grundy period</span></div>
<div><strong>13 to 22 times</strong><span>faster when seven hundred steps run as one combined program, with every record identical</span></div>
<div><strong>792</strong><span>exact numbers are enough to hold 100 quantum bits that are all 0 or all 1 together</span></div>
</div>

## What it does not claim

- It doesn't compute Kolmogorov complexity. It puts an upper limit on the complexity of a file by actually writing the file smaller.
- It doesn't claim any weakness in SHA-256.
- It can't hold every quantum state in a few numbers. A general state of 100 quantum bits still needs 2^100.
- It isn't a model, and nothing in it is trained.
- Some results were found by other people first. Where we know that, the published work is named.
- The thought experiments are ideas whose experiment can't be built as written. They are kept apart from the results, and none of them is one.
