# The reorder bound for the reading map

**Purpose:** Derive the bar the arms check grades against, as a function of the summation shape, so that retiling `apply_reading_map` moves the bar by calculation instead of by judgment.
**Scope:** `examples/quantum_machine/demon_core.cu`, the kernel `apply_reading_map` and the harness `check_arms_and_eyes` on `main` at 07d3244 where the file runs 644 lines, and the three routes of that file on branch `worktree-demon-arms-vectorize` at 59828f3 where it runs 1638

## Contents

1. [The two summation shapes the check compares](#the-two-summation-shapes-the-check-compares)
2. [The model, and the quantity the bound depends on](#the-model-and-the-quantity-the-bound-depends-on)
3. [The depth of the device chain](#the-depth-of-the-device-chain)
4. [Tile depth does not enter the bound](#tile-depth-does-not-enter-the-bound)
5. [The host reference costs one float32 rounding](#the-host-reference-costs-one-float32-rounding)
6. [The bar, assembled](#the-bar-assembled)
7. [The three shipped routes do not share a depth](#the-three-shipped-routes-do-not-share-a-depth)
8. [A prediction that did not hold](#a-prediction-that-did-not-hold)
9. [The distribution, and what it settled](#the-distribution-and-what-it-settled)
10. [The scale factor, and where the present one stops being an upper bound](#the-scale-factor-and-where-the-present-one-stops-being-an-upper-bound)
11. [The square root form, and why the depth form beats it here](#the-square-root-form-and-why-the-depth-form-beats-it-here)
12. [What the vector loads and the fused multiply-add do to the bound](#what-the-vector-loads-and-the-fused-multiply-add-do-to-the-bound)
13. [Half the check already has an exact answer](#half-the-check-already-has-an-exact-answer)
14. [The canonical arm set question](#the-canonical-arm-set-question)
15. [The redraw null degenerates in float32](#the-redraw-null-degenerates-in-float32)
16. [What is derived here and what is measured](#what-is-derived-here-and-what-is-measured)
17. [Sources](#sources)

## The two summation shapes the check compares

The check runs one reading map of 256 rows over 256 sources on a batch of 512 states
(`examples/quantum_machine/demon_core.cu:442-444`). Both routes read the same `float` map and the same
`float` states. The construction of the map therefore carries no error into the comparison.

The host forms each product in `double` and accumulates left to right in `double`
(`examples/quantum_machine/demon_core.cu:323`), then stores the result to `float`
(`examples/quantum_machine/demon_core.cu:326`). The device accumulates a strided subset per thread in
`float` (`examples/quantum_machine/demon_core.cu:183`) and then folds the 128 partial sums through a
shared memory tree (`examples/quantum_machine/demon_core.cu:193`), at 128 threads per block
(`examples/quantum_machine/demon_core.cu:501`).

The difference between the two numbers is the difference between two orders of summation, plus the
difference between two accumulator widths. Both are calculable.

## The model, and the quantity the bound depends on

Take the standard model, where each floating point operation returns the exact result perturbed by
one relative error no larger than the unit roundoff: `fl(a op b) = (a op b)(1 + d)` with `|d| <= u`.
For binary32, `u = 2^-24 = 5.9605e-08`. For binary64, `u = 2^-53 = 1.1102e-16`. The constant
`1.2e-7` in the present bar (`examples/quantum_machine/demon_core.cu:526`) is `2^-23`, which is twice the
binary32 unit roundoff.

Write the dot product as `s = sum of x_i` for `x_i = map_i * state_i`. Any summation order gives a
computed result of the form `sum of x_i (1 + t_i)`, where `t_i` collects the roundings that term `i`
passes through on its way to the root. So `|computed - s| <= g_K * sum of |x_i|`, where
`g_K = K u / (1 - K u)` and `K` is the largest number of roundings any single term passes through.

`K` counts the depth of the addition graph. The count of terms in that graph does not enter, and a
retiling is therefore cheap. A left to right sum of `n` terms has `K = n - 1`, and a balanced tree
over the same `n` terms has `K = ceil(log2 n)`. The terms are identical and the bound differs
by a factor of `n / log2 n`.

## The depth of the device chain

Parameterize the device by two numbers. `T` is the count of threads cooperating on one letter, and
`A` is the count of independent accumulators each thread carries in registers. Each accumulator
absorbs `L = n / (T A)` terms, and it absorbs them sequentially.

Three stages contribute:

| stage                                                  | roundings on the longest path |
| ------------------------------------------------------ | ----------------------------- |
| a thread's accumulator, sequential over `L` terms      | `L`                           |
| the `A` accumulators within a thread, folded as a tree | `ceil(log2 A)`                |
| the `T` threads, folded as a tree in shared memory     | `ceil(log2 T)`                |

So `K = n / (T A) + ceil(log2 A) + ceil(log2 T)`.

The kernel the tree holds today runs `T = 128` and `A = 1`
(`examples/quantum_machine/demon_core.cu:183,501`), giving `L = 2` and `K = 2 + 0 + 7 = 9`.

Now the part worth having before the retiling starts. Whenever `T A = n`, the expression collapses
to `K = 1 + log2 n`, whatever split of `T` against `A` produced it. At `n = 256` that is `K = 9` for
every balanced shape:

| `T` | `A` | `L` | `K` |
| --- | --- | --- | --- |
| 128 | 1   | 2   | 9   |
| 64  | 4   | 1   | 9   |
| 32  | 8   | 1   | 9   |
| 16  | 16  | 1   | 9   |
| 8   | 32  | 1   | 9   |

Minimizing `n/m + log2 m` over `m = T A` puts the minimum at `m = n ln 2 = 177`, where the
expression reads 8.92 against 9.00 at `m = 256`. The bound is flat across the entire practical
design space. Memory traffic alone can therefore choose the tiling, and the bar does not move.

Where `T A < n` the sequential run inside a thread dominates. At `T = 32` and `A = 1`, `L = 8` and
`K = 13`. At `T = 8` and `A = 1`, `L = 32` and `K = 35`. Each accumulator removed costs
`n / (T A)` roundings and saves one bit of tree. A smaller cooperating set is therefore the change that
degrades the bound.

## Tile depth does not enter the bound

The reduction axis walked in tiles of depth `k` gives `n / k` tiles, and a block walks them in
sequence while carrying its running sums in the same registers. The additions across tile
boundaries land on the same sequential chain as the additions inside a tile. The chain therefore has
length `n / (T A)` whatever `k` is, and `k` cancels out of the depth.

That answers the tile depth term in the request directly: the bar is a function of `n`, `T` and `A`,
and the tile depth may be chosen for occupancy and for the shared memory budget with no effect on
accuracy at all.

The cancellation depends on the across-tile accumulation being sequential, and it is the only thing
holding tile depth out of the bound. Fold the per-tile partials through a tree instead of chaining
them and the depth becomes `k + ceil(log2(n / k))` for a tile of depth `k`, where tile depth is then
the largest lever available. At `n = 1024` a tile of 64 gives 68 against the 1024 a chain gives, and
a tile of 32 gives 37. The section on the three routes below is where that matters.

## The host reference costs one float32 rounding

Two properties of the host route matter, and they pull in opposite directions.

Each product is exact. A binary32 value carries a 24 bit significand, the product of two of them
needs at most 48 bits, and binary64 holds 53 (`examples/quantum_machine/demon_core.cu:323-324`). No
rounding occurs in any host product.

The accumulation is the worst shape available, left to right over all 256 terms
(`examples/quantum_machine/demon_core.cu:321-325`), giving `K = 255` at the binary64 unit roundoff. That
is `255 * 1.1102e-16 = 2.83e-14`, which sits 7 decades under the device term and can be carried
along without argument.

The store to `float` is one rounding at the binary32 unit roundoff
(`examples/quantum_machine/demon_core.cu:326`), contributing `u = 5.9605e-08` times the magnitude. That
term is a sixteenth of the device term and belongs in the bar, because both routes land in `float`
and the comparison is taken between two `float` values.

## The bar, assembled

Let `S` be the sum of the absolute values of the terms of one row's dot product. The bar for that
row is

```
bar(row) = ( g_K + u + g_255_double ) * S
```

with `g_K = K u / (1 - K u)` at `u = 2^-24`, `K` from the depth table above, and
`g_255_double = 255 * 2^-53`.

At the shape the tree holds today, `K = 9`, and the three terms are `5.3644e-07`, `5.9605e-08` and
`2.83e-14`. They total `5.9605e-07`, which is ten units of binary32 roundoff. Nine roundings in the
device chain and one in the host's store to `float` make ten, and the bar is ten `u` times the scale.
No further term enters it. Ten `u` is the coefficient to write in place of the constant at
`examples/quantum_machine/demon_core.cu:526`.

Against the present bar, which is `2^-23 * sqrt(256) = 2^-19 = 1.9073e-06` times `scale`, the
derived bar is tighter by `2^-19 / (10 * 2^-24)`, exactly 16/5, on the row where `S` equals `scale`.
It is tighter by a further factor of `scale / S` on every other row.

## The three shipped routes do not share a depth

Three routes sit in `demon_core.cu` on branch `worktree-demon-arms-vectorize`. A bare
`demon_core.cu:N` in this section is that branch at commit 59828f3, where the file runs 1638 lines;
a citation carrying a full path is `main`, where it runs 644. The branch moved 302 lines during the
work and an earlier draft of this document cited the intermediate state, and the commit is therefore named here,
and not left to the reader. The scalar kernel is kept as the graded reference, `ROUTE_VECTOR`
takes 128 bit loads with a shuffle reduction, and `ROUTE_BLOCKED` runs a 64 by 64 output tile with
4 by 4 cells per thread. Both launch at 128 threads (`demon_core.cu:616`).

| route   | shape                                                | `K` at `n = 256` | `K` at `n = 1024` | `K` at 256 after b938ff7 |
| ------- | ---------------------------------------------------- | ---------------- | ----------------- | ------------------------ |
| scalar  | chain of `n/128`, then a 128 wide shared memory tree | 9                | 15                | 9                        |
| vector  | chain of 4 per quad, warp shuffle, then across warps | 11               | 15                | 10                       |
| blocked | every source into one register per output            | 256              | 1024              | 32                       |

The first two columns are the shape as first committed at 9ab10e0. The last column is where the two
changes below left it.

The vector route carried one accumulator and not four when first committed. All four components of
the quad were chained into the same register, at lines 300 to 303 of the 1336 line file at commit
9ab10e0. Its sequential run was therefore 4, where the scalar route's is 2. Its 5 level warp shuffle and 2
level cross warp stage sit on top of that and are unchanged since
(`demon_core.cu:342-345,356-361`).

The blocked route's depth is the reduction extent itself. Its outer loop steps the source axis by
`TILE_DEPTH` and its inner loop walks that tile, both accumulating into the same cell registers, and
no thread shares an output with another (`demon_core.cu:480,513`). So `K = n`, and the bound grows
linearly in the source count while a bar carrying `sqrt(n)` grows as its square root. The two diverge
without limit: the deterministic bound stands at 7.95 times such a bar at `n = 256`, 15.90 times at
`n = 1024`, and 31.80 times at `n = 4096`. One bar over three routes holds at 256 by margin and not
by derivation.

The first measurements taken were worst differences against the host: 3.815e-06, 7.629e-06 and
1.526e-05, which are 64, 128 and 256 units of `u` against sequential runs of 2, 4 and 256. Every
route sat far under its own bound, the blocked one at 0.78 percent of it. These are the engine's
measurements on a 3070 and not this document's.

This document read the doubling from the scalar route to the vector route as a real loss of accuracy
and said the vectorized route was less accurate than the kernel it replaced. That reading is
WITHDRAWN. It was drawn from a maximum, the distribution retired it, and the section on the
prediction below carries the measurement that killed it.

Both routes recover their depth by spending registers, and both changes were made and measured.
Commit b938ff7 gives the blocked route a per-pass accumulator, taking `K` from 256 to 32 at the
graded size and from 1024 to 80 at the bench size, and its measured worst residual halved with it,
1.526e-05 to 7.629e-06. Splitting the quad into four accumulators took the vector route from
`K = 11` to `K = 10`, with the four running sums and their combine at `demon_core.cu:326-340`.

The blocked route's depth is now `TILE_DEPTH + n / TILE_DEPTH`, because the per-pass totals form a
chain. Tile depth entered the bound the moment the pass totals stopped being one chain: the case
that the earlier section holds open.

That expression is smallest at `TILE_DEPTH = sqrt(n)`, and this document proposed raising the tile
from 16 to 32 on the strength of it, reading 64 against 80 at `n = 1024`. That proposal is
WITHDRAWN on three counts the engine measured, and the first of them inverts it. The graded check
runs at 256 sources, where `sqrt(256)` is 16. The tile already sits exactly at the optimum, and
`K = 32`; a tile of 32 reads `K = 40` there, which is worse. The gain was real at 1024 and inverted
at 256, and no single constant serves both.

The second count is that it is not a one-constant change. The load path requires
`TILE_STATES * TILE_DEPTH / SOURCES_PER_QUAD` to equal the block's thread count exactly, so that one
128 bit load per thread fills one tile with no remainder (`demon_core.cu:473-474`). At 64, 32 and 4
that product is 512 against 256 threads, the identity breaks, half the tile is never written, and the
kernel goes silently wrong instead of slow. It needs a load loop first.

This document got the third count wrong in the other direction. Occupancy was deferred to
the engine as the likely objection, and it is not an objection at all. Measured with ptxas, the
blocked kernel takes 47 registers and 8320 bytes of shared memory at 256 threads. On sm_86, the
registers bind it to five blocks per streaming multiprocessor where shared memory would allow twelve.
Doubling the tile to 16640 bytes leaves registers binding at five. Shared memory is nowhere near the
constraint.

## A prediction that did not hold

The four-accumulator change was predicted here to lower the vector route's measured residual toward
the scalar route's. It did not. The measurement is 7.629e-06 before and 7.629e-06 after, unchanged
to the bit, and the vector route remains at twice the scalar route's 3.815e-06.

The reason is the size. At 256 sources with 128 threads there are 64 quads, an active thread holds
exactly one quad (`demon_core.cu:311,330`). The four-way run the fix shortened sits inside that single
quad. The change converted a depth 3 chain into a depth 2 tree underneath a cross-thread tree of
seven levels that the fix never touched. The residual is carried by the part that did not move.

The measurement also cannot resolve what was predicted. These residuals are 2^-18 and 2^-17, which
are small integer multiples of the unit in the last place of the letter they land on, and the
statistic is a maximum over 131072 letters. One step of that quantum is the smallest difference the
check can report, a change of one in `K` has no room to appear. The fix lowers the bound and is
kept, and it will reach the measurement at source counts where a thread holds several quads and the
four-way run compounds.

Attributing the surviving factor of two to a mechanism would be over-reading a maximum. Separating a
partition effect from a depth effect needs the distribution of the residual over all 131072 letters,
or a count of letters at each step of the quantum, in place of the worst one.

## The distribution, and what it settled

The engine built that distribution in commit ab3195a. It retired two claims and corroborated the
depth model on a statistic nothing was tuned for.

| route   | `K` | letters exact | mean over bar | RMS over bar | worst over bar |
| ------- | --- | ------------- | ------------- | ------------ | -------------- |
| scalar  | 9   | 86.5%         | 0.0196        | 0.0540       | 0.2616         |
| vector  | 10  | 83.8%         | 0.0213        | 0.0540       | 0.3459         |
| blocked | 32  | 78.2%         | 0.0102        | 0.0227       | 0.1631         |

The scalar and vector routes carry the same RMS over their bar, 0.0540 against 0.0540, with means a
tenth apart. The factor of two lived in the tail of a maximum. So the vector route never regressed
against the kernel it replaces, this document's reading of it as less accurate was an artifact of the
statistic, and the partition mechanism offered for it described a phenomenon that does not exist.
Both are withdrawn.

The share of letters agreeing with the host to the last bit falls monotonically as the depth rises:
86.5 percent at `K = 9`, 83.8 at `K = 10`, 78.2 at `K = 32`. Three different kernels, ordered
correctly by a model that was derived before any of them was written, on a column none of them was
tuned against. That is the strongest evidence here that the depth accounting describes these kernels
and not only the arithmetic in the abstract.

One column in that table inverts if it is read for the wrong question. The blocked route's mean over
its bar is the lowest of the three, and that is not because it is the most accurate. Its bar is 3.3
times wider, and in absolute terms its mean residual is about 1.7 times the scalar route's. The
exact-agreement column shows that, and the ratio columns hide it. A ratio against the bar
answers whether the bound holds. It does not answer which route is closer to the host, and the two
questions need different columns.

## The scale factor, and where the present one stops being an upper bound

The bound is proportional to the sum of the absolute values of the terms. The harness uses the
largest letter in the whole output instead (`examples/quantum_machine/demon_core.cu:517-519`), and there
are two separate problems with the substitution.

The first is looseness, and it is bounded. Every map weight the harness builds is non-negative and
every state is 0 or 1 (`examples/quantum_machine/demon_core.cu:473,478,486`). No term is negative,
`S` equals the row's own letter, and the largest letter over all rows is an upper bound on every
row's `S`. Row 0 takes `span = 1` and therefore every weight 1.0f
(`examples/quantum_machine/demon_core.cu:472-473`), and its letter is therefore the count of lit sources. A row at
`span = 61` keeps about a sixty-first of the same support. One bar built from the largest letter
therefore grades the sparsest rows against a bar two decades above their own grain, and a fault
confined to a sparse row is what that hides.

The second problem is soundness, and it is not bounded. `theory/workbooks/orior/arm-records.md:35` defines a signed
arm with weights of -1, 0 or 1, as the difference of two overlapping arms. Under a signed arm the
terms cancel, `S` exceeds the letter by as much as the cancellation runs to, and the largest letter
stops being an upper bound on `S`. The bar then sits under the true bound and passes a kernel that
is wrong. The present bar is valid on the present test data because that data happens to be
non-negative, and it fails the first time the check is pointed at the signed arms the record
already defines.

Both problems close the same way. Accumulate `S` per row in `double` in the harness, in the pass
that already walks the map, and grade each row against its own `S`. That replaces a judgment with a
measurement of the input.

## The square root form, and why the depth form beats it here

The comment above the present bar reads that the difference is "eps times the magnitude times the
square root of the term count" (`examples/quantum_machine/demon_core.cu:523-525`). The square root form is
the probabilistic bound, and it holds under an extra assumption the deterministic bound does not
need: that the individual roundings are independent and mean zero. Under that assumption, Hoeffding
applied to the sum of `log(1 + d_k)` over `K` roundings gives, with probability at least `1 - p`,

```
|computed - s| <= lambda sqrt(K) u S,    lambda = sqrt(2 ln(2 / p))
```

The constant is therefore derivable and not chosen: `p = 1e-3` gives `lambda = 3.899`,
`p = 1e-6` gives `lambda = 5.387`, and `p = 1e-9` gives `lambda = 6.545`.

Read the present bar back through that form. It carries `2 u sqrt(n)` with `n` standing in for `K`,
which is `lambda = 2`, and `lambda = 2` corresponds to `p = 2 exp(-2) = 0.27`. A bar that permits a
27 percent failure rate under its own model is why the constant reads as picked.

The deterministic bound `g_K` beats the probabilistic bound `lambda sqrt(K) u` whenever
`K < lambda^2`. At `p = 1e-6` that threshold is `K < 29`, and every shape in the depth table above
reads `K = 9`. So the deterministic bound is both the tighter number and the one carrying no
assumption about the distribution of roundings, and the square root form has nothing to offer this
kernel. It would begin to pay at chain depths past 29, which is where `T A` has fallen to about a
sixteenth of `n`.

## What the vector loads and the fused multiply-add do to the bound

A 128 bit load moves four `float` values in one transaction, and performs no arithmetic. The
`float4` rewrite therefore cannot change a single bit of any result. Memory width is not a term in the bound.
The reorder is the entire accuracy question in the retiling, and the depth table above settles it.

The fused multiply-add lowers the bound. A separate multiply and add round twice per term, while
`fma` rounds once, and nvcc contracts by default. The depth table charges one rounding per term in
the chain either way, because the first addition into a zeroed accumulator is exact, and one rounding
is therefore saved from the non-fused count. `K` is the same under both settings, and the fused
form is never worse. This claim is derived and not measured here.

One condition attaches to every number above. The bound describes the addition graph as written, and
it holds while the realized graph matches it. CUDA C++ does not reassociate floating point additions
without permission, and the graph in the source is the graph that runs. A flag that grants
reassociation invalidates the depth, and `K` has to be recomputed from whatever graph results.

## Half the check already has an exact answer

The request asked for a positive control whose residual must be exactly zero. The check already
contains 128 of them and grades them against a floor.

Every even row takes weights of 1.0f or 0.0f (`examples/quantum_machine/demon_core.cu:473`) and every
state is 1.0f or 0.0f (`examples/quantum_machine/demon_core.cu:486`). Every product is therefore 0 or 1,
every partial sum in every order is a non-negative integer no larger than 256, and every integer
that size is exact in binary32 with 24 bits of significand. The sum of two exactly represented
integers whose total stays under `2^24` is itself exact. No addition in any order rounds at all.

The residual on all 128 even rows is therefore exactly zero, for any `T`, any `A`, any tile depth,
and any reassociation the compiler might perform. That is a stronger statement than a floor can
carry, it needs no new data, and it is the gate for a reorder: grade the even rows at
exactly zero and the odd rows against `bar(row)` from the section above. A tiling bug that survives
an exact zero on 128 integer rows is a bug that only touches the graded rows.

The odd rows carry `std::exp` weights (`examples/quantum_machine/demon_core.cu:478`) and are the graded
case, where a floor is the right instrument. The accuracy of `std::exp` does not enter the
comparison, because the host computes it once and both routes then read the same stored `float`.

## The canonical arm set question

There is no canonical arm set in this tree, and `theory/workbooks/orior/arm-records.md:6` declines to name one: the
eight sign octants are recorded there as one arm set among many, chosen for being easy to state,
draw and compute, with nothing about the reading requiring them. An exact control built on the
octants and described as canonical would assert a status the record refuses.

The octants remain the right choice for continuity with the redraw null, which is measured on them
(`theory/workbooks/orior/arm-records.md:101-103`), and they qualify as an exact control on their own terms: an octant
is an indicator arm with weights of 0 or 1 (`theory/workbooks/orior/arm-records.md:34`), and the eight sign octants
partition the space. Their letters therefore sum to the count of lit points exactly. That gives a second
exact invariant beside the per-letter one.

One detail governs whether the exactness survives to the letter a caller reads. The reading divides
the weighted sum by the count of lit points (`theory/workbooks/orior/arm-records.md:24`), and at the lit weight of 153
the redraw null uses (`theory/workbooks/orior/arm-records.md:103`), the reciprocal is not exact in binary32. The
integer argument covers the unnormalized sum. Both routes have to divide once, at the end, by the
same value, and then the identical division on identical inputs leaves the residual at exactly zero.
Folding the reciprocal into the accumulation instead would break the exact zero for a reason that
has nothing to do with the tiling.

## The redraw null degenerates in float32

Lifting the redraw null onto the device does not work as proposed, for two reasons that are worth
having before any of it is built.

The residuals are below float32's grain. The four redraws report worst letter moves of 0, 0,
2.168e-18 and 2.776e-17 against a floor of 2.220e-16 (`theory/workbooks/orior/arm-records.md:113-116,128`), and that floor
is the binary64 grain. A binary32 device would return exactly zero for all four, because every one
of those residuals is at least 9 decades under `2^-24`. The four numbers would agree, and the
agreement would carry no information. Reproducing the null as it stands requires the device to run
the redraws in binary64.

Two of the four redraws lose their meaning as uploaded matrices. The dwell arm's value is that its
curve uses different arithmetic from the placement, and the agreement is therefore not two calls to
one piece of code (`theory/workbooks/orior/arm-records.md:121-122`). Uploading its precomputed weights as a matrix
discards the second arithmetic and tests the multiply-accumulate that the reorder bound already
covers. Algebraic recombination is listed with no arm because it declines to measure one, taking
each octant's letter as a difference of two-octant letters (`theory/workbooks/orior/arm-records.md:124-126`). It is
not a matrix at all and cannot be uploaded as one.

The point cloud round trip is the redraw to lift. It gives weights directly, point by point, which
is a matrix by construction (`theory/workbooks/orior/arm-records.md:61-62`), and its worst letter move is exactly 0
(`theory/workbooks/orior/arm-records.md:113`). An exact target stays exact in binary32, and it transfers to the device
without the degeneracy above. Keep the other three on the host, where the different arithmetic that
makes them nulls still runs.

## What is derived here and what is measured

No card was run for this document. Every bound above is derived from the standard model and from the
line numbers cited, and the sizes, the thread counts, the tile constants and the present bar were
read out of the source. The three residuals of 3.815e-06, 7.629e-06 and 1.526e-05 are the engine's
measurements on a 3070, reported to this document and not taken again here.

Six claims were put up to be checked against a clock and a card. The first three stand and the last
three are withdrawn, each with the measurement that killed it:

1. The 128 even rows return a residual of exactly zero at every tiling. Cheap to run and a hard
   failure if the integer argument has a hole in it.
2. The derived bar of ten `u` times the per-row `S` passes the present kernel. If the present kernel
   fails a bar 3.2 times tighter than the one it passes today, the depth accounting is wrong or the
   realized addition graph is not the one in the source.
3. `K = 9` for every balanced split of `T` against `A`. Measured as the residual failing to move
   when the tiling changes at fixed `T A = 256`.
4. Withdrawn. Splitting the quad into four accumulators was predicted to lower the vector route's
   residual toward the scalar route's, and the measurement left it at 7.629e-06 unchanged. The
   section above carries the refutation and the two reasons for it.
5. Withdrawn. The vector route was called less accurate than the kernel it replaces, on a maximum.
   Its RMS over its bar equals the scalar route's at 0.0540. The regression was an artifact of
   the statistic, and there was none.
6. Withdrawn. A tile depth of 32 was proposed for the blocked route. The graded check runs at 256
   sources, where 16 is already the optimum. The change inverts there, and the load path breaks
   at 32 until it carries a loop.

Three of the six predictions on this page were wrong. Each was wrong in a way its own measurement
caught, and the two that mattered were both caught by replacing a maximum with a distribution.

The two-level sum is admissible under the house rule against chosen tolerances, because the bar it
is graded against is a function of the shape and moves when the shape moves.

## Sources

The summation results are standard, and all of them come from the literature. Two states are recorded
separately for each, because they are different things. The bibliographic fields were verified
against Crossref on 2026-09-16. The content was not: neither paper was read
for this document, and both are cited from knowledge. A verified identifier says the work exists as
described and says nothing about whether its pages were opened.

- N. J. Higham, _Accuracy and Stability of Numerical Algorithms_, 2nd edition, SIAM 2002.
  ISBN 978-0-89871-521-7, doi:10.1137/1.9780898718027. Carries the `g_K` bound for recursive and tree
  summation and the result that the bound follows the depth of the addition graph. The attribution of
  those to chapter 4 is from memory and is not verified against a copy. It therefore carries no chapter
  number in any claim above.
- N. J. Higham and T. Mary, _A New Approach to Probabilistic Rounding Error Analysis_, SIAM Journal
  on Scientific Computing 41(5), 2019, pages A2815 to A2835, doi:10.1137/18M1226312. The square root
  form and the derivation of the constant from a stated failure probability. An author copy is at
  eprints.maths.manchester.ac.uk/2673 for anyone wanting the content check this document did not do.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-16
