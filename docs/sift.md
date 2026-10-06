# The sift

The search, and the steering that decides where it looks, live in [`src/cu/engine/nbody/orior/orior_*.c`](https://github.com/dstroy0/orior/tree/main/src/cu/engine/nbody/orior). Together with the portable scan and the exact whole number math in [`src/cu/types/integers/`](https://github.com/dstroy0/orior/tree/main/src/cu/types/integers), it builds and runs with nothing but a C11 compiler: four source groups and no build system ([Setup](setup.md#the-c-engine)).

The Python in [`archive/src/python/engine/nbody/orior/sift/`](https://github.com/dstroy0/orior/tree/main/archive/src/python/engine/nbody/orior/sift) builds the same thing and shares no code with the C version. The two are checked against each other by making sure their counts agree.

**It is a safe filter.** It checks a few of a pattern's points first. Any true match must pass that check. That means no choice of those points (the anchors) can lose a true match. This is proved, and the proof uses no order, no number of dimensions and no alphabet ([Why the count is exact](ENGINE_PROOF.md)). The measurements agree: across 35 rows of text collections, needle lengths and strides, no search ever reported fewer matches than there really were. It can only be wrong in one direction. Any mistake is an over-count, and you can spot one without knowing the right answer.

**Its memory is small.** It keeps `m` bits of state for a pattern of length `m`, no matter how large the alphabet or how many dimensions there are. It builds no index and no table over the alphabet. An alphabet of real numbers, or one too large to list, costs it nothing extra. That is a claim about what it can do. It says nothing about speed.

**It can search with no pattern at all.** Given only raw bytes, it found a multiple of a record period from 512 reads. It scored 92 shifts on the real bytes and 0 on a shuffle of the same bytes.

## The kernel picks its own engine, and grades its choice

`orior_choose` picks a scan engine from a count of the symbols in the data, which the first histogram pass has already made. The comparison is exact whole number math, and the engine holds no floating-point value anywhere.

- The effective alphabet size, `2^H2`, works out to `total^2 / sum(count^2)`.
- The rule asks whether that reaches 85 percent of the symbols the data uses. With the fractions cleared, that becomes `100*total^2 >= 85*distinct*sum(count^2)`.

`bench_dispatch` times every engine, prints what the dispatcher picked next to what was fastest, and scores six candidate rules against each other. Over 42 rows, the rule the kernel uses picks the faster engine:

- 39 times on x64 MSVC 19.44 at Release, losing 9131790 cycles, or 0.035 of what the worst rule loses;
- 41 times under gcc on the same machine, losing 86511 cycles, or 0.000.

That is a hundredfold difference in cycles, and it is not rounding. Both are real runs, and each number belongs to the toolchain that produced it. That's why the bench exists: its output is advice to act on. It also tries every threshold instead of assuming one. Every value from 0.34 to 0.96 scores the same, and the kernel's 0.85 sits inside that range.

**On this data, the needle length part of the rule does nothing.** Scoring flatness alone ties the kernel exactly, with the same rows and the same cycles. The length part changes no answer on any of the 42 rows. Flatness followed by length scores strictly worse than flatness alone, and length alone is worse than both. A setting that nothing reads yet is a hook for later. It is neither removed nor described as not built. It is named here and kept until a row turns up where it helps.

The real score is cycles lost, and it turns the row count upside down. Always taking the free order engine is right on 17 of the 42 rows, the fewest of any rule. It still loses fewer cycles than always taking the short-circuiting engine, which is right on 25. Counting rows treats a row where the engines differ by one percent the same as a row where they differ threefold. A rule can be wrong more often and still cost less.

The dispatcher still has a blind spot, and it comes from the statistic it uses.

- A counter that repeats every 16 steps uses sixteen symbols evenly. Its collision entropy reads 4.0. A perfectly ordered sequence looks random to it.
- Collision entropy doesn't change when you shuffle the data. It can't see arrangement, and the dispatcher carries the same blind spot. Exact math removes rounding. It does not remove that blindness.
- Seeing arrangement needs a different measure, and `orior_anchors_for` is where one comes in. It takes the period the data repeats at and drops to a single anchor. At a known period, every anchor after the first checks the same thing and rules out nothing new.

## Building it, and what each tool tells you

From a fresh clone, it is one command. You need `cmake` and a C11 compiler on your `PATH`. There is no network step, no submodule to fetch, no generator to run first, and no library beyond the C standard headers.

```sh
utils/maint/engine/build_engine.sh               # configure, build, run the graders
utils/maint/engine/build_engine.sh --build-only  # configure and build, run nothing
```

In Windows PowerShell, use `utils/maint/engine/build_engine.ps1`, with `-BuildOnly` instead of `--build-only`. It imports the MSVC environment and compiles the device rasterizer. If you run the shell script from Git Bash instead, there is no MSVC environment. It then uses gcc or clang and tells you which. The output goes to `build/engine_c/`. Nothing reads it back, and you can delete it whenever you like.

Both scripts use that same directory, and a CMake cache outranks anything a script prints. So each script passes the settings that matter on every configure, and it wipes a cache that names a different toolchain. Before it builds, the PowerShell script checks that the configure found a CUDA compiler, and it fails if not.

On a machine with a card, rendering should use the card without being asked. When it does, `bench_raster` prints `device rasterizer: present` and grades all twenty configurations `host/device identical`.

The tests and the benches answer two different questions, and they live in two directories. [`utils/test/src/`](https://github.com/dstroy0/orior/tree/main/utils/test/src) answers whether the engine is right. [`utils/bench/`](https://github.com/dstroy0/orior/tree/main/utils/bench) answers how fast it is. A failing test is a bug; a slow bench is a cost.

After a build, the scripts run these graders: `test_steer`, `test_adversarial`, `test_arm_agreement`, `bench_steer_arms`, `bench_raster` and `bench_exact_arms`. The PowerShell script also builds and runs `test_o2_spawn`. The rest are built and left for you to run. Neither script builds `bench_lattice` or `bench_sigma`. To build one target on its own, run `cmake --build build/engine_c --target <name>`.

| run this                          | it answers                                                                                                                                                                                                                                                                                                 |
| --------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `test_arm_agreement`              | every engine against the naive one at the lengths that bound the input: none, one, two. A disagreement is a defect whatever it measures.                                                                                                                                                                   |
| `test_adversarial`                | thirteen cases built to break the guarantee from outside the public surface: overlapping occurrences, both boundary alignments, a field where every survivor is false, a permutation null, the probe guard including the widest line that must be admitted, and a joint projection.                        |
| `test_steer`                      | the steering, on five fields. Grades the ordering at seven needle lengths, carries a negative control ordering the commonest symbol first that must read MORE, checks the exact dispatch against four fields worked out by hand, and asserts that the widest scan engine the machine carries actually ran. |
| `test_o2_spawn`                   | the engine turned onto itself: a descent that resumes from another descent's survivors, past the four conditions one descent can place.                                                                                                                                                                   |
| `bench_dispatch`                  | which dispatch rule to carry, scored against the clock over 42 rows, sweeping its threshold instead of assuming it.                                                                                                                                                                                        |
| `bench_steer_arms`                | the scan engines graded against the portable one and then timed, at lengths straddling the thirty-two lane boundary where a vectorized tail fails if it is going to.                                                                                                                                       |
| `bench_scaling_reads`             | reads per alignment as the corpus grows. Reads travel between machines and are what an asymptotic claim is made of.                                                                                                                                                                                        |
| `bench_scaling_cycles`            | the same sweep in cycles, which belong to the machine that produced them.                                                                                                                                                                                                                                  |
| `bench_coherence`                 | at what scale the corpus agrees with itself, and what that costs the histogram bound.                                                                                                                                                                                                                      |
| `bench_sigma`                     | the oracle route against a counter table, timed as the alphabet grows and as it does not.                                                                                                                                                                                                                  |
| `bench_raster`                    | every render configuration. Four sheet layouts by five channels, each written as a PGM and graded host against device byte for byte where a device is present, then four volume layouts by the same five channels into a 32 by 32 by 32 block.                                                             |
| `bench_exact`, `bench_exact_arms` | the fixed width limb arithmetic, and every vectorized limb engine against the portable one.                                                                                                                                                                                                                |
| `bench_lattice`                   | soundness in one to eight dimensions, over a rotated point set and a scatter no rectangle covers. It holds its own core, because the construction is under test, apart from the byte specialization.                                                                                                       |

**The counting build and the timing build are different programs, and they can't be mixed.** `bench_scaling_reads` links the kernel built with `ORIOR_COUNT_READS=1`, and `bench_scaling_cycles` links the kernel built without it. Counting slows the code down, which would throw off the timings reported beside it. A program that calls `orior_counters_reset` therefore fails to link against the timing kernel, and that failure is deliberate.

**Known gap:** `bench_lattice` needs C99 `_Complex` math, and it doesn't build under MSVC, which provides those types without their operators. Build it with GCC or Clang. Every other target in the table was built and run on MSVC 19.44 x64 at Release. The same CMake file covers the GCC and Clang builds.

## Drawing the object, flat and solid

The renderer draws the object straight from the engine's state. What you see is what the search saw. There are two kinds of output, because a flat sheet and a solid block are different maps, not one map at two sizes. [`rendering.md`](https://github.com/dstroy0/orior/blob/main/theory/workbooks/orior/rendering.md) covers both.

- `AnchorRasterConfig` draws a sheet. You give it a `width` and a `height`, one of four layouts, one of five channels, a rule for cells that several positions land on, and a gain.
- `AnchorVolumeConfig` draws a block. You give it a `width`, a `height` and a `depth`, one of four volume layouts, and the same five channels, the same two rules and the same gain. They point to the same definitions, because a channel means one thing everywhere in this tree.

| volume layout   | what it is for                                                                                                                                                                                                        |
| --------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `slabs`         | fills a sheet, then the sheet behind it. The three dimensional reading of the row layout.                                                                                                                             |
| `boustrophedon` | every other row and every other slab reversed. Consecutive alignments stay adjacent across both boundaries.                                                                                                           |
| `morton`        | interleaves the bits of x, y and z, preserving locality on all three axes at once. This reads as a solid instead of as stacked sheets. Needs power of two extents and errors others instead of remapping quietly.    |
| `helix`         | each slab's rows shifted by its depth index. A feature at a fixed corpus offset winds through the block. A shear and not a rotation, because a true helix needs trigonometry and this renderer is integer throughout. |

Every layout sends each cell index to exactly one place, computed in whole numbers. `bench_raster` checks this. It sends every position through every layout on every channel and counts the collisions, which must be zero. A layout that quietly folded two positions together would still draw a convincing picture, and no other check would catch it.

The Netpbm image formats have no way to store a volume. So `anchor_volume_write_raw` writes the block as raw bytes, with x changing fastest. It puts the size, the layout, the channel, the rule and the gain in a `.txt` file next to it, which also names the function that made it. Any volume viewer that reads raw unsigned 8-bit data can open the block, given those three size numbers.

**The device draws both sheets and volumes, and the engine uses the device when there is one.**

- `anchor_raster_render` and the volume renderer both use the device first.
- `anchor_volume_device_available` returns 1 when there is a device and the build includes the device volume kernel, and 0 otherwise. It never reports a stub as a real device.
- `anchor_volume_render_host` only ever runs on the host, as its name tells you.
- `bench_raster` checks the device against the host voxel by voxel, and prints `device rasterizer: present` when there is a device.

Nothing silently falls back to the host, because a stub that reports itself as present is a bug.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
