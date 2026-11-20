# The quantum machine

Three representations of a quantum state at three costs, each exact for a class and each carrying a
measured wall. A product state of 4096 qubits fits in 32 KB. An entangled graph state of 4096 qubits
fits in 1 MB. A general state of 4096 qubits needs 1e1234 bytes and is not reachable by any means.

All of it rests on one correction from Douglas. The correction is the first section, because
everything after it depends on that count.

```
python examples/00_blob_viz_tools/product_machine.py --check     --width
python examples/00_blob_viz_tools/graph_machine.py    --check    --shapes
python examples/00_blob_viz_tools/bloch_cost.py       --check    --terms
python examples/00_blob_viz_tools/qubit_trace.py      --check    --compare --optime --elsewhere --invert --nested
```

## The correction the whole thing rests on

The usual answer says that n qubits need 2^n amplitudes, and quotes the storage for 4096 of them as
though that settled the matter.

**Douglas: infinite compressibility means a double is a vector, and another double is a magnitude,
and their difference is their angle.**

The correction is right, and the usual answer counts the wrong object. A qubit is a direction and a length:

    |psi> = cos(theta/2)|0> + exp(i phi) sin(theta/2)|1>

which is two real numbers. So n qubits in that form is 2n reals, and the exponential is nowhere in
the description. 4096 qubits is 8192 numbers.

**So where is the exponential, since it is real?** In the entanglement, and nowhere else. The Bloch
form describes a product state, one direction per qubit with no correlation between them, and every
extra parameter beyond 2n buys correlation. Measured by fitting the best product description to
states of known entanglement:

| entanglement | product overlap | error      |
| ------------ | --------------- | ---------- |
| 0.000 bits   | 1.000000000     | -2.220e-16 |
| 0.798 bits   | 0.944404597     | 5.560e-02  |
| 1.815 bits   | 0.822664388     | 1.773e-01  |
| 3.000 bits   | 0.353553391     | 6.464e-01  |

A product state is reproduced from 2n angles at difference **0.000e+00**, exactly. The error rises
with the entanglement, and nothing besides changes down the table.

## The product machine, at the quoted figures

**Douglas: build the quantum computer with n qubit cap here, it should cost about 64b a qubit but we
really only need 32b.**

Two float32 is 64 bits a qubit: exactly a (theta, phi) pair. The specification is therefore a
product-state machine, and the qubit cap is memory over eight bytes.

| memory | qubits            | a state vector of the same | full single-qubit layer |
| ------ | ----------------- | -------------------------- | ----------------------- |
| 32 KB  | **4,096**         | 1e1234 bytes               | 0.0002 s                |
| 8 MB   | **1,048,576**     | 1e315654 bytes             | 0.0393 s                |
| 8 GB   | **1,073,741,824** | 1e323228498 bytes          | not run                 |

Every single-qubit gate is exact and costs two array updates whatever n is. The probability a qubit
reads one comes off its own angle with no state vector, a million qubits can be watched live.

**The 64-bit call is right, and the 32-bit shortcut is not**, measured against the same angles kept
at float64:

| per qubit                | state error    |
| ------------------------ | -------------- |
| 2 x float64, 128 bits    | **0.0000e+00** |
| 2 x float32, **64 bits** | 1.2157e-07     |
| 2 x float16, 32 bits     | 1.0162e-03     |

float16 angles land at 1e-3, which sits **above** the 1e-08 detection limit `unmodelled.py` measured
in plain float64. A 32-bit machine would swamp the instruments it exists to feed. That is the
precision-floor argument applied to a representation instead of an arithmetic: a format is adequate
when its floor sits below the smallest thing the instrument must see.

**The negative control keeps this honest.** A maximally entangled state fits the product form
at overlap 0.353553. The representation genuinely cannot express entanglement, and it is
cheap for that reason.

## Four terms per qubit, and why the exponent does not move

**Douglas: we can represent entanglement, we have a third and fourth term, a polarization and an
entanglement vector, magnitudeless.**

A pure state of n qubits carries 2^(n+1) - 2 real parameters after removal of normalization and global
phase. A fixed budget per qubit carries c\*n.

| qubits | 2n Bloch | **4n proposed** | n^2 pairwise | full state | 4n covers |
| ------ | -------- | --------------- | ------------ | ---------- | --------- |
| 2      | 4        | **8**           | 4            | **6**      | **yes**   |
| 3      | 6        | 12              | 9            | **14**     | no        |
| 4      | 8        | 16              | 16           | 30         | no        |
| 16     | 32       | 64              | 256          | 131,070    | no        |
| 4096   | 8192     | 16,384          | 1.7e7        | 1e1233     | no        |

**Four terms covers two qubits exactly and fails from three.** At two qubits the space is six
parameters, and four terms each gives eight. Every two-qubit state, a Bell pair among them, is
therefore representable with room to spare. Three terms also covers two qubits, six against six exactly.
A count of certain angles as up and down therefore changes the constant, and not the exponent.

And the budget that would suffice grows as 2^n / n:

| terms per qubit | covers every state of up to |
| --------------- | --------------------------- |
| 3               | 2 qubits                    |
| 8               | 4 qubits                    |
| 64              | 8 qubits                    |
| 1024            | 12 qubits                   |
| 1,048,576       | 23 qubits                   |

**Doubling the budget buys one more qubit, every time.** Covering 128 qubits needs about 1e37 terms
each, and at 2^n / n terms per qubit the representation _is_ the amplitude array reached by a longer
road.

## The graph machine: the refinement that works

**Douglas: it might be a vector and magnitude of entanglement, and the magnitude is one or zero.**

That is a graph state, reached independently. A binary entanglement magnitude between each pair is an
adjacency matrix, and a state specified by which pairs are connected is

    |G> = product over edges (i,j) of CZ_ij applied to |+> on every qubit

One bit per pair, n(n-1)/2 bits total, and the states covered are **genuinely entangled**: Bell
pairs, GHZ, cluster states, the whole graph-state family.

| qubits   | pairs     | adjacency  | a state vector of the same |
| -------- | --------- | ---------- | -------------------------- |
| 64       | 2,016     | 0.0 MB     | 1e20 bytes                 |
| **4096** | 8,386,560 | **1.0 MB** | 1e1234 bytes               |
| 65,536   | 2.1e9     | 256.0 MB   | 1e19730 bytes              |

A cluster state on 4096 qubits is 4095 edges, half a kilobyte.

**It beats the four-term version for a structural reason.** Four reals a qubit is linear in n and
covers two qubits; one bit per _pair_ is quadratic and covers an exponentially large class of
maximally entangled states.

**And the entanglement is read off the bits, exactly.** For a graph state the entropy across a split
is the GF(2) rank of the adjacency block between the halves. No amplitude array is necessary to find
it. It agrees with the state vector's own SVD entropy to 1.000000000 across five shapes and sizes,
and the arithmetic is integer modulo two throughout. The answer is therefore an exact integer, with no floor
under it. Ordinary numerical rank is the wrong tool, and the check carries a matrix with GF(2) rank 2
and real rank 3 to show the field matters.

## Shapes: random wins, and the reason is entropy

64 qubits split 32 against 32, and the entropy ceiling is therefore 32 bits for any shape whatsoever.

| shape                | edges  | entropy | per edge   | of ceiling |
| -------------------- | ------ | ------- | ---------- | ---------- |
| star                 | 63     | 1       | 0.0159     | 3.1%       |
| line, 1D cluster     | 63     | 1       | 0.0159     | 3.1%       |
| ring                 | 64     | 2       | 0.0312     | 6.2%       |
| grid 8x8, 2D         | 112    | 8       | 0.0714     | 25.0%      |
| grid 4x4x4, 3D       | 144    | 16      | 0.1111     | 50.0%      |
| golden sphere, k=4   | 135    | 13      | 0.0963     | 40.6%      |
| golden sphere, k=8   | 271    | 21      | 0.0775     | 65.6%      |
| fibonacci offsets    | 434    | 27      | 0.0622     | 84.4%      |
| euler offsets        | 762    | 31      | 0.0407     | 96.9%      |
| complete             | 2,016  | 1       | 0.0005     | 3.1%       |
| **random, 63 edges** | **63** | **19**  | **0.3016** | **59.4%**  |

**Random at 63 edges reaches 19 bits where the line reaches 1.** Three times the best structured
shape per edge, and it beats the 2-D grid using 44% fewer edges.

**Douglas's explanation is the right one: highest information entropy.** Rank tracks the entropy of
the _family_ that each graph comes from, and not any geometric property:

| shape            | graphs in its family     | rank |
| ---------------- | ------------------------ | ---- |
| complete         | 1, fully determined      | 1    |
| star             | n                        | 1    |
| line             | about n!/2               | 1    |
| grid             | few                      | 8    |
| random, 63 edges | C(2016, 63), about 1e110 | 19   |

Zero choice in the structure, zero rank. Maximum choice, maximum rank. The complete graph spends
2016 edges to buy one bit precisely because there is only one complete graph.

The mechanism is exact, and not empirical: the entropy _is_ the GF(2) rank of the adjacency block,
a random binary matrix has near-full rank with high probability, and a regular one is rank deficient
by construction. A complete graph's off-diagonal block is all ones, GF(2) rank one, and it buys a
single bit however large n is.

**And it inverts the placement result: the finding worth keeping.** For conditioning of a
boundary reading the golden placement beat random, 42 against 2174. For entanglement per edge random
beats every structured shape. Same tree, opposite optima. The rule that resolves it is Douglas's: a
representation should match the structure of its target, because any structure in the representation
is a bet on structure in the object. Smooth target, structured placement. Generic target, disorder.

**The embedding does not enter**, which settles the spherical and oblate questions without measuring
them. A graph state depends only on which pairs are connected, and not on where the qubits sit. A
shape in space matters only where the adjacency comes from proximity. The golden
sphere rows do that, and proximity graphs are smooth, hence rank deficient, hence inefficient: 0.0963
against random's 0.3016.

## Tracing an algorithm

8 qubits is 256 amplitudes, the placement carries 256 sources, and a degree-16 boundary reading has
rank 256. So the reading is **complete** for an 8-qubit state and the harmonic rows are the right
instrument: a state vector is complex, and the harmonic coefficients are complex, with the phase
that torsion reads. The beam rows are real and non-negative, and would throw the phase away: the
reverse of the bit-pattern case.

Grover searching for 10110101 over 256 amplitudes, optimal count 13:

| step | P(marked)    |
| ---- | ------------ |
| 0    | 0.003906     |
| 12   | **0.999947** |
| 13   | 0.986186     |
| 17   | 0.664165     |

P(marked) climbs and then **falls back**, and that matters: an accumulating process does not come
back, and a rotation over-rotates. Per-qubit probability at step 13 reads
`0.993 0.007 0.993 0.007 0.993 0.993 0.007 0.993`: the marked pattern, least significant bit
first. The answer arrives one amplitude at a time, and not one qubit at a time, and the trace
shows it.

**A prediction in this work fails, and the failure is worth more than the prediction.** The prediction:
Grover's iteration, being a rotation, to show a large rigid component in the deformation scalar
against SHA-256's tail bit which showed none. **The scalar reads 1.0000 at every Grover step**,
indistinguishable from the avalanche. The reason is structural: the only rigid motion divided out is
a _global phase_: the right invariance for physical indistinguishability, and not the
motion that Grover performs. Grover rotates inside the two-dimensional span of the marked state and
everything else, and that subspace is named by the answer. A quotient that needs the answer is not
an instrument.

**What separates them is the magnitude, not the direction**, which is Douglas's "everything here is
supposed to be a magnitude and a vector" applied to the right column:

|                                  | coefficient of variation |
| -------------------------------- | ------------------------ |
| Grover step norm                 | **0.0222**               |
| SHA-256 tail bit, round to round | 0.3285                   |

A factor of 14.8. A rotation at fixed angular rate takes fixed-size steps; an avalanche has no fixed
step size.

## The optimal-time detector

Optimal time for amplitude amplification is (pi/4) sqrt(N/M), which is known in advance and is never
given to the detector. It sees only the step norms of the trace.

| marked | predicted | found |
| ------ | --------- | ----- |
| 1      | 12.57     | 13    |
| 2      | 8.89      | 9     |
| 4      | 6.28      | 7     |
| 8      | 4.44      | 5     |
| 16     | 3.14      | 4     |
| 32     | 2.22      | 3     |
| 64     | 1.57      | 2     |

**found = ceil(predicted) in all seven rows**: the right answer, and not a close one:
iterations come in whole steps.

The reader must take the **first** turning point. Amplitude amplification is a rotation, and
therefore periodic. The _least_ step norm over the window can belong to a later minimum that came back
around, and a reader that takes it misses a row. That miss is not a resolution limit: 64 marked items
predict 1.57 and land correctly on 2. The failure would not be monotone in the marked count.

**What it is worth, narrowly:** knowing when to stop amplitude amplification normally means knowing
N and M in advance, and the turning point needs neither. It is **not** a speedup. The iterations
still cost what they cost and a detector watching them is strictly more work than counting them.

## Where the coherence goes

<!-- docs-check: quoting -->
**Douglas: we do not need to hold incoherent information stable here, because it is coherent
elsewhere, and that is why qubits resist being expressed.**
<!-- docs-check: end quoting -->

The physics half is right. 8 qubits, four kept and four traced out. The narrow instrument is therefore the
partial trace, and the wide one is the whole state:

| step | local purity    | local entropy |
| ---- | --------------- | ------------- |
| 0    | 1.000000000     | 0.000000      |
| 4    | 0.573223305     | 0.891619      |
| 5    | **0.980969883** | 0.078179      |
| 7    | 0.750000000     | 0.600876      |

The local purity falls and then **returns** at step 5, under nothing but forward evolution. That is
recurrence, and not a one-way drain: the quote "conservation in action, the system balances
itself" with a visible signature.

**Global purity is a vacuous column, and this page does not use it.** _Global_ purity at
1.000000000000 is no evidence that coherence survives. Global purity is |<psi|psi>|^2, which is
**one for any normalized state vector by definition**. It is normalization, and not unitarity, and the
number cannot come out any other way, whatever the dynamics. As a headline, it establishes nothing.

The non-vacuous test is exact return, and the inversion does it properly.

## The wave inversion is exact, all-or-nothing, and a half measure makes it worse

**Douglas: the boundary surface wave needs to invert.**

That is the right operation and it is exactly computable, because inverting a wave is conjugating its
phase. The exact inverse of a phase sequence is the negated sequence run backwards, which is U
dagger alone, and a phase times its negation is one with no arithmetic to round.

| reach | touch | after wave | after inversion    |           |
| ----- | ----- | ---------- | ------------------ | --------- |
| 1     | 1     | 0.750000   | **1.000000000000** | FULL      |
| 2     | 2     | 0.812500   | **1.000000000000** | FULL      |
| 3     | 3     | 0.890625   | **1.000000000000** | FULL      |
| 3     | **1** | 0.890625   | **0.765625**       | **WORSE** |
| 3     | 2     | 0.890625   | 0.890625           | unchanged |

Full reach returns the purity to exactly one, every time. **And a partial inversion is not merely
ineffective, it can make things worse.** That row is a theorem, and not a code limit: local operations
cannot _reduce_ entanglement and nothing stops them increasing it. At best a too-narrow phase
commutes and changes nothing; at worst it adds a correlation of its own. A half measure is a live
risk, and not partial credit.

**Nesting costs recoverability nothing when the inversion reaches the whole structure:**

| branch | depth | modes | local purity | inversion returns                  |
| ------ | ----- | ----- | ------------ | ---------------------------------- |
| 2      | 2     | 4     | 0.500000     | **1.000000000000**                 |
| 2      | 3     | 8     | 0.500000     | **1.000000000000**                 |
| 3      | 2     | 9     | 1.000000     | 1.000000 (degenerate, no evidence) |

The two rows whose purity fell went to exactly 0.5, maximally mixed for that split, and the inversion
returned them to one. Depth never enters the argument, because a product of unitaries is unitary
however it is nested. **Douglas's claim holds in its non-vacuous form.**

The branch-three row builds pairs that do not straddle the split that the readout takes. Its "returns
one" is therefore trivially true. It stays visible, and is not dropped, because a row that proves nothing is
worth a look next to rows that do.

**And the reach is the price**, growing as b^d while recoverability holds. Coherence is recoverable
at any depth and the recovery must touch every mode the coupling reached:

| kernel                         | dimensions       | cost to close         |
| ------------------------------ | ---------------- | --------------------- |
| a harmonic reading at degree 8 | 175              | 256 beams             |
| a four-qubit environment       | 16               | hold all eight qubits |
| a warm laboratory environment  | order 1e23 modes | not available         |

The structure is identical at every scale, and **the scale is the problem**. For that reason,
decoherence is unitary in principle and irreversible in practice, and for the same reason "if we ask nothing" buys no
isolation: there is **no measurement anywhere in these files**, no projection and no collapse, and
the local purity degraded regardless, from coupling alone.

## The wall, a single fact

Every representation here is cheap for exactly the reason it carries no quantum advantage.

| representation            | holds                                           | why it is cheap                                                     |
| ------------------------- | ----------------------------------------------- | ------------------------------------------------------------------- |
| 2n reals                  | product states                                  | no entanglement at all                                              |
| 4n reals                  | every 2-qubit state                             | linear budget, exponential space                                    |
| O(n^2) bits, stabilizer   | GHZ, Bell, cluster states at millions of qubits | Clifford circuits are classically simulable, by Gottesman and Knill |
| 2n\*chi^2, matrix product | entanglement up to log2(chi) per split          | low entanglement by construction                                    |
| 2^(n+1) - 2               | everything                                      | not cheap, and not reachable past about 46 qubits                   |

**Entanglement is not what is expensive. Structureless entanglement is.** A GHZ state over a million
qubits is maximally entangled and costs O(n^2) bits. What no polynomial budget holds is a _generic_
state, and by pigeonhole almost every state is generic, which is Douglas's own compressibility
argument arriving from the other side: the states that compress are the ones with structure, and a
quantum computer is worth having for the ones without.

**Where the advantage actually sits in the graph-state formalism**, since it is not a dead end: a
two-dimensional cluster state plus adaptive non-Clifford _measurement_ is universal. The state is
cheap, and the universality lives in the measurement schedule. The cost therefore returns there, and is
not avoided. That says which half to look at.

## What this is not

It simulates and does not use a quantum computer. There is no quantum hardware in this work and none
is implied by any figure in it.

The full state vector wall is a count, and not an engineering forecast. The largest such simulations
anywhere are in the region of 45 to 50 qubits on machines with petabytes of memory, and
each additional qubit doubles the requirement:

| memory | qubits |
| ------ | ------ |
| 16 KB  | 10     |
| 500 MB | 25     |
| 32 GB  | 31     |
| 1 PB   | 46     |

Going from 50 to 4096 is 4,046 doublings. A 4096-qubit state vector needs about 1e1234 bytes against
1e80 atoms in the observable universe and 1e185 Planck volumes in it. High-precision numbers do not
help: arbitrary precision means unbounded _magnitude_, not unbounded _storage_, and a big integer of
N bits occupies N bits.

And nothing here touches mining. Shor factors and solves discrete log, and it therefore threatens signatures,
and not hashing, and a hash has no period for it to find. For preimages the relevant algorithm is
Grover, a square-root speedup which is real on quantum hardware and **inverts into a 51,472-times
slowdown when simulated classically**, because each iteration must touch every amplitude while only
sqrt(N) iterations are needed. That measurement is in `theory/workbooks/orior/what-a-reading-conserves.md`'s companion
work and is not repeated here.

## Every module carries its own controls

Because a checker that has never been shown failing grades nothing:

- **product_machine**: the representation must be exact for a product state, memory must be linear in
  the qubit count, an expansion of a 64-qubit machine to a state vector must _raise_, and not attempt it,
  and a maximally entangled state must **not** fit, or the cheapness would be free advantage.
- **graph_machine**: the GF(2) entropy must agree with the state vector's own SVD entropy across five
  shapes, an empty graph must read zero, a Bell pair must read exactly one bit, cost must be
  quadratic, and entropy must never exceed the smaller half for any shape.
- **bloch_cost**: a product state must fit exactly, 2n angles must determine the state, a maximally
  entangled state must _not_ fit, and the error must be monotone in the entanglement.
- **qubit_trace**: the simulation must stay normalized, Grover must actually concentrate the
  amplitude, it must **over**-rotate, a state against itself must read exactly 0.0, a pure global
  phase must read as rigid and be recovered exactly, two unrelated random states must read as nearly
  all deformation, and the reading must be complete at rank 256 for 256 amplitudes.
