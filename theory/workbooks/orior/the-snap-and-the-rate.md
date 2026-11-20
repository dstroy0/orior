# The snap and the rate

Douglas, 2026-09-11: _"it needs to be 4, the integral allows for the qubit to snap its vector and
switch position to release entanglement."_

This is the fourth term built as the operation it exists for, and the relaxation rate that came out
of it.

## The wrong question first

Static capacity is the wrong measure: how many reals it takes to write an arbitrary pure state, which is
2^(n+1) − 2 against a fixed budget of c·n.

| qubits | reals needed | 4 reals gives |       |
| ------ | ------------ | ------------- | ----- |
| 1      | 2            | 4             | exact |
| 2      | 6            | 8             | exact |
| 3      | 14           | 12            | short |
| 16     | 131,070      | 64            | short |

All true, and none of it about the question in the quote. Four reals a qubit is exact through two qubits and
short from three on, and raising 64 bits to 128 does not move that line by one qubit. It is a
counting result.

His fourth term is not storage for an amplitude. It is a magnitude of 0 or 1 saying whether the
qubit is bound to anything, and its purpose is to make the snap available. A binary term cannot hold
an arbitrary amplitude and it holds that perfectly.

**A three term qubit cannot ask the question the snap answers.** With a direction and a
polarization, a qubit carries where it points. Nothing in it says whether it is bound. a three
term machine cannot tell a free qubit from an entangled one and cannot know whether a snap is
available. The fourth term is the predicate that conditions the operation, and not extra capacity.

## The two snaps

Both are standard graph state operations and both are exact on the bits, with no arithmetic.

**Release**, a Z measurement: delete every edge on the qubit. It leaves the cluster and its
entanglement is gone. Snapping without switching position.

**Switch**, a Y measurement: complement the edges _among_ the qubit's neighbors, then delete the
qubit's own edges. The qubit leaves, and its neighbors inherit the connectivity. The entanglement
therefore moves, and does not vanish. The local complementation is the switch.

Order is not interchangeable. The complementation has to happen while the qubit still has its edges,
because the neighborhood is defined by them. Deleting first would complement an empty set and
switch would silently degrade into release.

## What it costs

At 64 bits a term, which `product_machine.py` measures as the floor, and not a preference:

| terms                    | bytes a qubit | 4096 qubits | 1e6 qubits |
| ------------------------ | ------------- | ----------- | ---------- |
| 2, direction             | 16            | 64 KB       | 15.3 MB    |
| 3, plus polarization     | 24            | 96 KB       | 22.9 MB    |
| **4, plus entanglement** | **32**        | 128 KB      | 30.5 MB    |

At 68 qubits a machine is 2461 bytes: 2176 local plus 285 for the 2278 pairs. Measured against real
free memory, that is **2.03e+07** machines in 46.46 GB of RAM, or 2.70e+06 in 6.19 GB of VRAM. A
full state vector at 68 qubits is 4.72e+21 bytes and fits zero times.

The four local terms are a fixed 32 bytes. The adjacency grows as n/16 bytes a qubit, because
entanglement is a property of pairs and there are n(n−1)/2 of them. The fourth term therefore does not
make this expensive at scale. It is 8 bytes flat, and it makes the snap expressible.

## The exact null: the real result about the twist

A twist is local complementation, and local complementation is a local Clifford operation. Those
preserve entanglement across every bipartition. So:

> The entanglement response to a twist is **exactly 0.0** at every vertex, on every shape, at any
> size.

Computed, and not remembered. That is an exact null in the same family as the others here, and it
says the thing this tree keeps finding from new directions: **relabeling is free.** A twist moves
the description and not the content.

The edge count is _not_ a local Clifford invariant. The tension therefore moves while the correlation
does not. That is the torsion against tension relationship, with the conservation measured, and not
asserted. The tension moment separates all four test shapes: `[0.75, 1.0, 55.125, 441.0]`.

## Three entropy-adjacent measures, and they are not one measure

At 8 qubits, every shape against a random graph on the **same edge count**. A comparison without
a match only measures how many edges somebody drew.

| shape         | edges | rank | family | tension | rnd rank | rnd family | rnd tension |
| ------------- | ----- | ---- | ------ | ------- | -------- | ---------- | ----------- |
| star          | 7     | 1.00 | 3.17   | 55.12   | 2.62     | 7.19       | 1.75        |
| line          | 7     | 2.94 | 9.26   | 0.75    | 2.62     | 7.19       | 1.75        |
| ring          | 8     | 3.29 | 11.52  | 1.00    | 2.79     | 8.15       | 3.77        |
| complete      | 28    | 1.00 | 3.17   | 441.00  | 1.00     | 3.17       | 441.00      |
| grid 2x4      | 10    | 3.29 | 11.52  | 5.00    | 3.01     | 9.33       | 5.17        |
| grid3d 2x2    | 12    | 3.60 | 9.06   | 9.00    | 3.18     | 10.47      | 4.33        |
| fibonacci     | 21    | 3.43 | 11.58  | 26.50   | 3.14     | 10.22      | 31.38       |
| golden sphere | 16    | 3.29 | 11.52  | 0.00    | 3.34     | 11.14      | 4.25        |

The three orderings differ, and each carries something the others do not. Kept correlation,
description freedom, and resistance to a twist are separate properties of one graph.

Two rows are controls, and not findings. **Complete** matches its random control exactly, because
a random graph with 28 edges on 8 qubits _is_ the complete graph. **Star and complete both read
family 3.17**, orbit size 9, and star and complete graphs genuinely are local-complementation
equivalent. That agreement is a correctness signal, and not a coincidence.

## A retraction

The golden sphere's `0.00` tension does not mean that the spherical arrangement is uniquely twist-rigid.
That holds at one width, and is false in general.

The mechanism check says why. At 8 qubits, every vertex has degree 4, with 6 pairs among its
neighbors, and exactly **3** present: precisely half. A complement returns 3, and Δ is 0
everywhere. Change the width and it goes:

| qubits | edges among 6 pairs    | delta    | tension moment |
| ------ | ---------------------- | -------- | -------------- |
| 8      | 3                      | 0        | 0.0000         |
| 12     | 2                      | +2       | 4.0000         |
| 16     | mixed, degrees 4 and 5 | +2       | 3.5000         |
| 24     | mixed                  | +2 to +4 | 10.0000        |

It is neighborhood parity at one width, and not a property of spheres. Retracted.

## Dissipation, and it came back null

Douglas: _"they usually dissipate or are vibrated by neighbors back to their lowest state."_

The model has no dissipation in it at all. It is closed: nothing leaks and nothing relaxes.
Dissipation therefore cannot explain the table above, and the table is neighborhood parity.

Added as a process: release qubits one at a time until nothing is bound. **Driven** takes the qubit
under the most neighbor pressure, recomputed each step, where pressure is the sum of the
_neighbors'_ degrees, and not the qubit's own. **Random** takes a fixed order over 20 seeds and
is the matched control.

| shape         | steps | driven rate | random mean | random range, 20 seeds | verdict |
| ------------- | ----- | ----------- | ----------- | ---------------------- | ------- |
| star          | 1     | n/a         | n/a         | too few steps to fit   | n/a     |
| line          | 6     | 0.3612      | 0.3248      | 0.2785 .. 0.4494       | inside  |
| ring          | 7     | 0.3006      | 0.3251      | 0.2473 .. 0.4056       | inside  |
| complete      | 9     | 0.0501      | 0.0501      | 0.0501 .. 0.0501       | inside  |
| grid 2x5      | 6     | 0.3583      | 0.3006      | 0.2397 .. 0.4094       | inside  |
| fibonacci     | 8     | 0.2617      | 0.2573      | 0.2272 .. 0.2924       | inside  |
| golden sphere | 8     | 0.2747      | 0.2692      | 0.2310 .. 0.3140       | inside  |

**0 of 6 shapes put the driven rate outside the random orders' own range.** Neighbor pressure
changes nothing measurable: the relaxation rate is a property of the shape and not of the order of
release. "Vibrated by neighbors" and "released at random" are the same curve. That is against the
reading that prompted the measurement and it is the result.

The complete graph row is a free exact control. Every qubit in it has identical neighbor pressure.
Driven and random are then the same process by construction, and the range has width exactly zero. If
that row ever reads OUTSIDE, the pressure function is not computing what it claims.

**What survived is the rate itself.** It is intrinsic to the topology and robust to order:

| shape         | rate   | half-life, steps |
| ------------- | ------ | ---------------- |
| complete      | 0.0501 | 13.85            |
| fibonacci     | 0.2573 | 2.65             |
| golden sphere | 0.2692 | 2.52             |
| grid 2x5      | 0.3006 | 2.39             |
| ring          | 0.3251 | 2.21             |
| line          | 0.3248 | 1.92             |

A complete graph resists relaxation about **6x** harder than a line. The exact figures need a second
width before they are believed, since these are slopes through under ten points each, but the
ordering is not subtle.

## A gap needs the range of its control

Gaps of `+0.0457`, `−0.0200` and `+0.0641` against rates near 0.3, counted as "differed on 5 of 6"
by exact float inequality, would call pressure a real driver.

They have no null. Without the spread from seed to seed of the random control, no gap can show that it
exceeds noise. Gaps of both signs alone forbid the claim. Against the control's own **range over 20
seeds**, every gap sits inside it.

A difference without a matched null produces a finding that is not there. [The eye test](the-eye-test.md)
shows two more cases.

## What the model keeps doing instead of overturning anything

Every check against established quantum information theory has reproduced it exactly:

- entanglement invariant under local complementation: **0.0** at every vertex, every shape
- star and complete graph in one LC orbit: both **3.17**
- GF(2) rank of the adjacency block equals the entanglement entropy: agrees with the actual 2^n
  state vector on every shape and width, **before and after both snaps**
- Clifford gates exact: `0.000e+00` at every width and depth, in `exact_qubits.py`

There is no result here showing the standard theory is wrong. What there is, is an instrument that
recovers the known answers to the floor, and that is the only reason the relaxation table is worth
arguing with. A new number from a tool that cannot reproduce the old ones is noise.

## Controls

All nine pass.

1. GF(2) rank agrees with the state vector on every shape and width
2. and still agrees after both snaps: each leaves a valid graph state
3. a snap of a free qubit equals the identity on both operations
4. local complementation is an involution on every shape
5. releasing a star's center takes entanglement 1.00 to 0.00 and bound qubits to 0
6. the entanglement measure is not degenerate: 3 distinct values over 4 shapes
7. the twist leaves entanglement exactly unchanged: `[0.0]`
8. relaxation terminates, is monotone, and ends empty on every shape
9. driven relaxation empties a 10 qubit star in exactly 1 step: pressure ranks what it claims

Control 6 exists because a sum of rank over single-qubit splits is degenerate. A nonzero one-row
block has GF(2) rank 1, and that sum only counts
non-isolated vertices. It reads exactly 8 for all eight shapes, the twist moment built on it reads
exactly 0 everywhere, and the ordering printed from them is list order. Two dead columns that
look alive.

## Apparatus

- `examples/00_blob_viz_tools/snap_release.py`, the snaps, the three measures, the relaxation, nine controls
- `examples/00_blob_viz_tools/graph_machine.py`, the representation it builds on

Related: [the eye test](the-eye-test.md), [the quantum machine](the-quantum-machine.md).
