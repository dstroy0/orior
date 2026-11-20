# What a reading conserves, and a claim that is wrong

A boundary reading at or above degree 15 over 256 sources recovers **every one of the 256 numbers**
at the arithmetic floor. Below that degree, it loses exactly its kernel component, and nothing
besides. Both halves are measured, and the first half refutes a tempting claim.

Run it with `python examples/00_blob_viz_tools/conserved.py`.

## The claim this page refutes

The tempting claim: a reading conserves one quantity, the monopole, and not information, and the
kernel is where the rest goes. This page refutes it.

The claim is not in the tree. What `chapter_boundary.tex` says, at line 142, is that degree zero is
the monopole and holds the conserved quantity. To the extent that `a_00` holds, the reading
redistributes, and does not add. That is a statement about **deformation**, a change of the
coefficient vector at fixed support, and it is correct and stands. It does not claim that the
reading conserves nothing else.

A refutation of a claim that the tree never made would be a second error dressed as a correction,
and harder to find than the first. The source of a claim therefore needs a check first.

Douglas's objection: the kernel is background that the reading does not read, and not information
that the reading destroys. **The proof supports the objection.**

"Conserves information" has an exact operational meaning and it is testable: take an object, read
it, and try to recover the object from the reading alone. If the recovery lands at the arithmetic
floor then the reading carried everything.

| degree | coefficients | rank    | kernel | recovery error |             |
| ------ | ------------ | ------- | ------ | -------------- | ----------- |
| 4      | 25           | 25      | 231    | 9.6395e-01     | not carried |
| 8      | 81           | 81      | 175    | 8.7836e-01     | not carried |
| 12     | 169          | 169     | 87     | 6.2037e-01     | not carried |
| 14     | 225          | 225     | 31     | 3.0970e-01     | not carried |
| **15** | **256**      | **256** | **0**  | **1.6452e-13** | **carried** |
| 16     | 289          | 256     | 0      | 1.5480e-14     | carried     |
| 18     | 361          | 256     | 0      | 8.4814e-15     | carried     |
| 20     | 441          | 256     | 0      | 4.7736e-14     | carried     |

At degree 15, where `(L+1)^2` first reaches 256, the kernel empties and the map conserves the object
exactly. **The claim describes a reading truncated below the source count, and states it as a
property of the map.** That is the error, and it is the kind that survives review, because the
sentence is true of the case that its writer had in mind.

The recovery is by least squares on the exact map with a noiseless reading, the most
favorable recovery available. Anything it cannot recover under those conditions is not recoverable.

## Where the missing part goes, at a truncated degree

At degree 8 the kernel is 175 dimensional and a random object splits 0.4780 of its length outside
the kernel and 0.8784 inside. Then:

| quantity                                           | measured   |
| -------------------------------------------------- | ---------- |
| the recovery reproduces the outside-kernel part to | 1.1586e-14 |
| the recovery holds, of any kernel component        | 2.8535e-16 |
| total recovery error                               | 8.7836e-01 |

**a truncated reading loses exactly its kernel and loses nothing besides.** The part it can see
comes back at the floor. That is the precise sense in which it fails to carry information, and it is
a statement about the truncation, and not about the boundary.

## The decisive measurement

A vector drawn from the degree-8 kernel reads:

| reading      | result         |
| ------------ | -------------- |
| at degree 8  | **1.758e-17**  |
| at degree 16 | **2.7376e-02** |

**It was never destroyed. It was unread, and the degrees above read it.** That is Douglas's
objection, measured, and it carries the refutation.

## Nothing in a finite point-source model is nonradiating

A related statement says that the null space of the reading _is_ the class of nonradiating sources:
sources whose exterior field vanishes identically, and which no arithmetic reaches. That folded two different
things together. A source whose exterior field is identically zero has every coefficient zero and is
a genuine kernel; a source whose far field falls below the reader's floor while its near field does
not is not a kernel at all, because the reading is taken at a finite radius where a near field is
present by definition.

The measurement below narrows it further, and in the same direction.

If some combination of these sources radiated nothing at all, it would sit in the kernel at every
degree and the kernel would never empty. Swept past the source count:

| degree | coefficients | rank | kernel | least live value |
| ------ | ------------ | ---- | ------ | ---------------- |
| 15     | 256          | 256  | 0      | 4.5631e-06       |
| 16     | 289          | 256  | 0      | 1.1156e-04       |
| 18     | 361          | 256  | 0      | 1.4096e-03       |
| 20     | 441          | 256  | 0      | 1.4710e-03       |
| 24     | 625          | 256  | 0      | 1.4789e-03       |

**The kernel empties and stays empty: no combination of these sources is nonradiating.** Distinct
point sources are linearly independent as distributions, and this is that fact measured, and not
quoted.

**The nonradiating class therefore does not bind on any model in this tree.** It needs a continuum,
where a volume distribution has infinitely many degrees of freedom and particular ones cancel in the
exterior. A finite set of point sources cannot express one. The obstruction is real and it is not
ours.

## What anisotropy in the object costs

The complement question: does a structureless object read worse than a structured one? Four objects
spanning the range from maximally concentrated to maximally spread, recovered through the same map.

Degree 8, kernel 175:

| object           | flatness | in kernel  | recovery error | monopole error |
| ---------------- | -------- | ---------- | -------------- | -------------- |
| point source     | 0.42     | 0.8268     | 8.2681e-01     | 1.9e-15        |
| **smooth field** | 0.13     | **0.0000** | **1.36e-15**   | 1.6e-14        |
| random           | 0.27     | 0.8426     | 8.4260e-01     | 5.7e-15        |
| alternating      | 0.43     | 0.9976     | 9.9765e-01     | 1.7e-16        |

Degree 16, full rank: every object recovers at 2e-14 to 8e-14, kernel share zero.

Three things fall out.

**At full rank, anisotropy costs exactly nothing.** The point source and the structureless one come
back identically, because least squares does not care what the object looks like once the map is
injective.

**Under truncation the recovery error IS the kernel share** -- 0.8268 against 0.8268, 0.8426 against
0.8426, 0.9976 against 0.9977, identical to four figures. anisotropy never degrades the
instrument; it decides how much of the object sits where the instrument cannot look.

**A smooth object has kernel share exactly 0.0000 at degree 8** and recovers at 1.36e-15 through 175
dimensions of truncation, because a smooth field _is_ low degree. That is the strongest form of the
point: how well the field is described decides what is recoverable, and a well-described field is
fully recoverable by a badly truncated reader.

And the monopole comes back at the floor for every object at every degree. a truncated reading
gives **exact knowledge of a subspace and exactly nothing about its complement** -- not fuzzy
knowledge of the whole thing. The total weight is free.

## A correction to the correction, for a binary object

The paragraph above is true over the reals and misleading for a bit pattern, and the distinction
matters enough to state separately.

The real-valued kernel at degree 8 is 175 dimensional. But the **binary** points in that kernel
coset are generically a single point, because 81 exact real functionals on a finite set of 2^256
vertices generically separate all of them. Counting the distinguishable readings:

| degree | rank | readings at 1 digit each | states |
| ------ | ---- | ------------------------ | ------ |
| 8      | 81   | **1e81**                 | 1e77.1 |
| 16     | 256  | 1e256                    | 1e77.1 |

**At one decimal digit per coefficient a degree-8 reading already over-determines all 2^256 states.**
Information is therefore not the obstruction, and never was. The kernel is huge over the reals and a
point over the binaries, and both statements are true of the same map.

What remains is a closest-vector problem: the real solve hands over the exact projection in closed
form, and picking the lattice vertex is the hard part. `examples/proofing/relation_search.py` does
that with de Weger LLL, and `bbp_scaling.py` measures the cost at **precision^2.64, with the card
unable to help it.**

## Conditioning is a separate axis and is deliberately left out

The least live singular value says how much noise a recovery survives, not whether the information
is present. Degree 15 is complete and badly conditioned at 4.5631e-06; degree 16 is complete and far
better at 1.1156e-04, a factor of twenty four in usability between two maps of equal rank.
Completeness and usability are different questions and `conserved.py` answers only the first.

## Running the check

```
python examples/00_blob_viz_tools/conserved.py --check
python examples/00_blob_viz_tools/conserved.py
python examples/00_blob_viz_tools/conserved.py --radiate
```

Both controls run and the negative one carries the weight. A degree-8 reading must **fail** to
recover, or the test passes everything and measures nothing: it fails at 8.16e-01. A degree-16
reading must succeed at the floor, or the claim that information is carried there is false. The lost
part must be exactly the kernel component. The check therefore asserts that the recovery reproduces the visible part
at the floor, and holds none of the kernel. An object drawn from the kernel must read as nothing at
degree 8, and, as the refutation requires, **must be visible at degree 16**, because if it
stayed invisible it would be nonradiating, and not truncated.

## What this does not establish

It is the linear algebra of a specific map, measured. It does not show that a reading of a physical
field conserves anything.

And injectivity is not causality. The map being invertible says nothing about propagation, because
depth is elliptic and conduction is the heat semigroup -- both have instantaneous global dependence.
The map is therefore acausal, injective or not. Those are independent properties and this page
settles only the first.
