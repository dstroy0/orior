# The eye test

Douglas, 2026-09-11: _"we can use an eye … as an extradimensional observer … doesn't disturb the
superset."_

This is that test, run. It asks whether a **line** integral through the SHA-256 compression state
predicts the final digest where a **region** integral could not.

## Why this ran when the question was already answered

`nonce_read.py` asked the same question and returned null, and that null is real. But it was
measured with six scalars: weight, monopole share, dipole share, quadrupole share, fine-structure
share, and the zonal dipole. **Five of those are power shares, and a power share is invariant under
rotation by construction.** An instrument assembled from rotation invariants cannot see orientation
at any strength.

The state carries 256 numbers, one per bit. That count is exact: it is the rank bound that this tree
measures five separate ways. The prior null therefore covered a six-dimensional
rotation-averaged summary of a 256-dimensional object. It is a real result about that summary and it
was never a result about the object.

This is the sense in which the deformation was, in the words of the quote, invisible mathematically: the
apparatus was averaging away the only thing that carried direction.

## The instrument

A harmonic row is a whole-sphere integral. An arm is a region integral. A **beam is a line
integral**: occlusion along a ray, weighted by perpendicular distance to it. It reads passively:
it observes from outside, and does not perturb what it observes.

Measured here, and not quoted from `beam_rows.py`:

| map                           | rank |
| ----------------------------- | ---- |
| harmonic rows, degree 8       | 81   |
| beam rows, 256 line integrals | 256  |
| both stacked                  | 256  |

The beams add **175 dimensions of rank** the region reading does not have. They are a different row
type, and not a restatement of one.

## The result

2048 nonces on one real header shape. Target is the leading zero count of state 64. The bar is the
95th percentile of the largest absolute correlation over the same feature count under 200 shuffled
pairings.

| round  | instrument   | loudest    | null bar | verdict   | limit |                  |
| ------ | ------------ | ---------- | -------- | --------- | ----- | ---------------- |
| 8      | beam         | 0.0000     | 0.0000   | null      | n/a   | by construction  |
| 8      | harmonic     | 0.0000     | 0.0000   | null      | n/a   | by construction  |
| 15     | beam         | 0.0000     | 0.0000   | null      | n/a   | by construction  |
| 15     | harmonic     | 0.0000     | 0.0000   | null      | n/a   | by construction  |
| 16     | beam         | 0.0468     | 0.0736   | null      | 0.070 |                  |
| 16     | harmonic     | 0.0475     | 0.0712   | null      | 0.050 |                  |
| 17     | beam         | 0.0546     | 0.0776   | null      | 0.100 |                  |
| 17     | harmonic     | 0.0489     | 0.0734   | null      | 0.070 |                  |
| 20     | beam         | 0.0666     | 0.0818   | null      | 0.050 |                  |
| 20     | harmonic     | 0.0697     | 0.0762   | null      | 0.070 |                  |
| 24     | beam         | 0.0562     | 0.0797   | null      | 0.070 |                  |
| 24     | harmonic     | 0.0520     | 0.0727   | null      | 0.070 |                  |
| 32     | beam         | 0.0596     | 0.0812   | null      | 0.030 |                  |
| 32     | harmonic     | 0.0689     | 0.0748   | null      | 0.050 |                  |
| 48     | beam         | 0.0594     | 0.0834   | null      | 0.050 |                  |
| 48     | harmonic     | 0.0476     | 0.0739   | null      | 0.070 |                  |
| **64** | **beam**     | **0.5361** | 0.0811   | **FIRES** | 0.030 | positive control |
| **64** | **harmonic** | **0.2570** | 0.0721   | **FIRES** | 0.010 | positive control |

**Null at every live round before 64, on both instruments.** The weakest limit across the live
rounds is 0.100, a correlation below about that would not have been visible to this sample.

## The free exact null, which is ala control

The nonce sits in schedule word 15, and round _r_ consumes schedule word _r_. So rounds 0 through 14
consume words 0 through 14, every one identical across the sweep, and **states 0 through 15 are
bit-identical for every nonce**. State 16 is the first the nonce has reached.

Rounds 8 and 15 are therefore an exact null by construction, and they are in the table for that
reason: they must read exactly `0.0000`, and variance there is a wiring bug, and not a discovery.
Verified at 192, 256 and 500 nonces. The same free control `tail_bit.py` records for word 13.

An unlabeled zero in a results table reads as the strongest measurement on it. These carry labels.

## The positive control

The target is the leading zero count of state 64. A read of state 64 is therefore a read of the answer. That
row must fire, and it does. A null there would have meant the instrument was blind, and a null from
a blind instrument is not evidence of absence, it is no evidence at all.

## Where the eyes do win

On the one round that carries a signal, the two instruments do not read the same amount of it:

> beam **0.5361** against harmonic **0.2570**, a factor of **2.09** on the same state, the same
> target and the same null.

That is the directional information a power share averages away, and it is a measured property of
the row type, and not a claim about it.

**It does not help a miner.** The early rounds have nothing for either instrument to find, and a
better instrument pointed at an empty room still reads empty. This is consistent with every other
result here: the standing constraint that no projection over the digest reduces the number of
evaluations, the twenty null hypotheses of the transform workbook, and the absence of any period
across 75 tests in `nonce_period.py`.

## Two bugs this found in itself

Both produced false positives, and both are recorded in the source at the site.

**An absolute tolerance on an unnormalized quantity.** Constant columns were guarded with
`spread > 1e-12`. Round 15 then read `0.0000` at 2048 nonces and `0.0237` at 192. The answer cannot
depend on the sample count for a column that is constant at every sample count. The difference was
the count's factorization. 2048 is a power of two: a pairwise sum of identical doubles is exact,
and a centered value is exactly zero. 192 is not, and the mean landed a few ulp off, and the residue on
features of magnitude ~100 cleared the threshold and was correlated like data. A tolerance picked by
judgment is what made that possible.

**BLAS tiling.** Replacing the tolerance with an exact `max != min` test made it worse, to 0.207.
A matrix-matrix product is tiled, and rows in different tiles sum in different orders. Bit-identical
input rows then came out different in the last ulp: real variation, correctly detected,
and pure rounding noise. The fix is a multiply over the **distinct** occupancy patterns, and a map
back. Identical states then give identical readings by construction. It is never slower: when every
row is distinct it is the same work.

Neither was caught by reasoning. The first was caught by a control that varied the sample count
across the property that mattered; the second by that control still failing after the obvious fix.

## What this says nothing about

Qubits. The quantum representation question is a separate one, answered in `exact_qubits.py` and
`graph_machine.py`. The eye is a classical line integral, and that is all.

## Apparatus

- `examples/00_blob_viz_tools/beam_read.py`, the measurement, with five controls
- `examples/00_blob_viz_tools/build_beam_view.py`, the drawing, one self-contained file
- `build/view/beam_read.html`, the page

Related: [the eyes](the-eyes.md), [what a reading conserves](what-a-reading-conserves.md).
