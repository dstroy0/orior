# The width of an eye

Douglas, 2026-09-12, arriving in pieces:

> "if a qubit knows its position and a null permutation gives identity then the demon arm sweeping
> infinite space"
> "If it's the two of them there s a fundamental integral we don't know"
> "Other path is a macro fractal spike"
> "The shape if were 3d looks like a jellyfish with 4 balloons"
> "It's like four fractal spikes between those at oblique angles from the balloons and a larger
> spike in the center"

Two branches, and one of them closes without a measurement.

## The branch that closes by counting

At bandlimit L, every function on the sphere lives in an (L+1)²-dimensional space. Every reading of
every kind - a region integral, a line integral, a point evaluation, an integral against _any_
measure whatever - is a linear functional on that space. So the span of all conceivable integrals is
exactly (L+1)², and **no integral nobody has thought of yet exceeds it.**

Beams therefore add 175 dimensions at degree 8: not because a line integral escapes the space,
but because the region rows do not span it. 81 + 175 = 256.

So "a fundamental integral we don't know" has no room to exist at fixed degree. The `81` in the
measured closure law - arms-to-closure = sources − 81, at 64, 128, 256, 512 and 1024 sources - _is_
(L+1)² at L = 8. The formula already had the dimension count in it.

## The branch that was open: conditioning

Rank was never the scarce thing. This tree has been caught by the difference twice: degree 15
complete at a least singular value of 3.5e-3 against degree 16 at 1.5e-1, and the orthogonal spiral
offset where the concurrent set came out best conditioned at 8.18e+02 against 3.74e+09 - seven
orders of magnitude at **identical rank**.

### The four balloons are derived

Widths double from the source spacing to the shell diameter. Both ends are forced by the sources:
below the spacing a beam reads one source and is a point probe, above the diameter it reads all of
them at equal weight and is a monopole.

| quantity                            | value   |
| ----------------------------------- | ------- |
| source spacing, `DEPTH·√(4π/COUNT)` | 0.13737 |
| shell diameter, `2·DEPTH`           | 1.23600 |
| range                               | 9.027×  |
| rungs, `⌊log₂9.027⌋ + 1`            | **4**   |

Four. The quote "a jellyfish with 4 balloons" came before this run.

### The ladder loses

The count exponent comes from a **sweep, and not a pick**. Three defensible choices disagree, and a
choice of one by hand would be the judgment that this tree does not allow. `n ∝ width⁻ᵉˣᵖ`:

| exponent | meaning                  | spike condition | shuffled null         |
| -------- | ------------------------ | --------------- | --------------------- |
| 0        | equal beams per rung     | 2.029e+08       | 1.902e+12 - 2.668e+12 |
| 1        | equal thickness per rung | 5.926e+04       | 4.922e+04 - 5.899e+04 |
| 2        | equal area coverage      | 2.447e+03       | 2.547e+03 - 3.760e+03 |

The shipped single scale conditions at **5.992e+01**. Every ladder variant is worse, the best by a
factor of 41. **The macro half of the idea is refuted.**

The self-similar layout is real but second-order: against its own null - the same multiset of widths
with the assignment shuffled, verified identical and 171 of 256 beams moved - the ladder wins by
four orders at exponent 0 and _loses_ at exponent 1. Layout is worth a factor of a few. Width is
worth ten million.

### Width is the whole effect, and the derived floor is the wrong bound

A derivation of the source spacing as a delta limit would call the shipped `BEAM_WIDTH = 0.10` a defect,
because it sits below that limit. The measurement says that it works because it sits below the limit. A near-delta
row is nearly a row of the identity, a stack of them is nearly orthogonal; a wide row is nearly
the monopole, a stack of those is nearly rank one. Narrow wins by construction.

## Then the null picked up the turf

The narrow sweep reported condition 4.86 at width 0.003 with the rank never breaking - and that
number measures the **aiming**, and not the instrument. `beam_set` aims beam `at` at source
`(at·7 + 1) mod 256`, and 7 is coprime to 256. The aiming is therefore a **permutation**: one beam pointed
at each source. Shrink the width and the matrix becomes a permuted identity for free.

The instrument had been built out of the answer. So the control aims at directions drawn
independently of the sources, three seeds per width, worst case reported:

| width / spacing | aimed cond | blind cond    | blind rank |
| --------------- | ---------- | ------------- | ---------- |
| 1.000           | 2.765e+02  | 3.441e+02     | 256        |
| 0.707 ← shipped | 5.397e+01  | 8.324e+01     | 256        |
| **0.500**       | 2.453e+01  | **6.229e+01** | 256        |
| 0.354           | 1.566e+01  | 7.756e+01     | 256        |
| 0.250           | 1.007e+01  | 2.742e+02     | 256        |
| 0.177           | 7.968e+00  | 2.221e+03     | 256        |
| 0.125           | 6.925e+00  | 1.307e+05     | 256        |
| 0.088           | 6.352e+00  | 6.102e+08     | 256        |
| 0.062           | 5.898e+00  | 1.078e+13     | 244        |
| 0.044           | 5.567e+00  | 1.275e+13     | 199        |
| 0.031           | 5.181e+00  | 1.274e+13     | 161        |
| 0.022           | 4.857e+00  | 1.275e+13     | 130        |

The aimed column slides monotonically to 4.86 and the rank never breaks, because it cannot. The
blind column has a genuine **minimum at half the source spacing**, then collapses eleven orders of
magnitude and takes 126 sources with it into rows that touch nothing at all.

**That gap is the null permutation with a price on it.** Knowing a source's position is worth 13× in
conditioning and the whole difference between a complete reading and a third of the object
invisible - and it has to be paid in advance, because the aiming _is_ the position. An instrument
that uses it has been handed what it was built to find. Douglas put the clause in the right place:
_"if a qubit knows its position."_

## What shipped changed

`BEAM_WIDTH = 0.10` is 0.707 spacings at blind condition 8.324e+01. **0.0687 - half the spacing -
conditions at 6.229e+01**, both complete, neither using a source position. The shipped width is
about a quarter too wide, and that was found blind.

## The interleaved oblique jellyfish

> "It's like four fractal spikes between those at oblique angles from the balloons and a larger
> spike in the center"

Built as described: wide rungs on golden directions, narrow rungs placed in their **gaps** by
farthest-point selection, aimed off-axis by a swept impact parameter, with one center spike down the
longest chord. Gap filling is verified, and not assumed: 16 directions placed in the gaps of 64
sit 14.44° from that set, where a fresh golden set of the same size sits 4.39°.

**This costs no knowledge of the object.** It uses only the directions the instrument
already chose, and never a source position. Unlike the aimed permutation, a gain here would be free,
and not circular. It stays legal under the blind control.

Every comparison is against the **null's own seed spread**, which spans 1.09×. A first version of
this verdict used a bare inequality and duly reported a 1.03× difference as a finding:

| effect                                         | ratio      | verdict                            |
| ---------------------------------------------- | ---------- | ---------------------------------- |
| **obliquity**                                  | **12.69×** | **REAL**                           |
| interleaving, against the co-aligned ladder    | 1.03×      | not shown, below the null's spread |
| self-similar layout, against its width-shuffle | 1.08×      | not shown                          |
| against the shipped single scale               | 0.03×      | **40× worse**                      |

**Obliquity is real and large.** Aiming the same directions at the center instead of off-axis costs
12.69× in conditioning - 2.369e+03 against 3.006e+04. That is the only arrangement effect in this
whole document that survives its null, and it comes from the quotes above.

**Interleaving is not shown.** Redundancy, the likely cause named for the second-order
layout effect, is therefore not demonstrated to be the cause. That is a negative result against the stated
diagnosis: the co-aligned ladder does not lose for that reason.

And the family verdict does not move: **one narrow single scale still beats every arrangement by
40×.** Every layout measured here is a way of losing more slowly than the ladder.

Two code paths must agree for the obliquity number to mean anything - `offset = 0` aims at `0 ×
side`, the origin, and the `straight` flag aims at the origin directly. They are two separate
code paths, and their rows differ by exactly **0.000e+00**. The 12.69× is therefore two aimings of one
instrument, and not two instruments.

## Apparatus

- `examples/00_blob_viz_tools/fractal_spike.py`, the ladder, both nulls, the width sweep, and 9 checks including a
  positive control (blind must be complete at the widest width) and a negative one (blind must
  break far below the floor)
- `examples/00_blob_viz_tools/beam_rows.py`, the beam row as a Gaussian in perpendicular distance, and the shipped
  `BEAM_WIDTH`
- `examples/00_blob_viz_tools/aimed_arms.py`, the closure law and the source-count ceiling
- `examples/00_blob_viz_tools/arm_rank.py`, rank, nullity and reach into a kernel

Related: [the eye test](the-eye-test.md), [what a reading conserves](what-a-reading-conserves.md).
