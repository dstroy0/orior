# A quantum examination of computing SHA-256

Everything below is measured in this tree, derived in one line from something measured here, or
labeled **quoted**. The quoted figures are two, both from the published literature on quantum
preimage attacks, and the text names each where it uses it.

The question is narrow and it is the only one worth asking: **is there a quantum route to a
SHA-256 preimage that beats running the function?**

## What the function is, computationally

SHA-256 is arithmetic in the ring mod 2^32. That is the definition and not a tuning choice:
widening the word computes a different function and fails silently.

Its per-round work is three operations. Two are three-input boolean functions, Ch and Maj. The
third is rotation: the Sigma and sigma mixers are each three rotates combined by XOR. Rotation is
the torsion on the state, and it is the dominant cost. Counted from the compiled machine code of
the miner in this tree:

| opcode    | count | share      | what it is                                                  |
| --------- | ----- | ---------- | ----------------------------------------------------------- |
| SHF       | 5394  | **29.90%** | funnel shift, the rotation                                  |
| LOP3      | 3625  | 20.09%     | three-input boolean: Ch and Maj are one instruction each     |
| IMAD      | 2126  | 11.78%     |                                                             |
| IADD3     | 1996  | 11.06%     | three-way add, fused                                        |
| LDL / STL | **0** | 0%         | no register spills anywhere                                 |

Occupancy of the mining kernel is **100%** at 38 registers, 48 of 48 warps resident. So the
classical implementation is at the integer-pipe roofline, and its rotations are 30% of everything.
There is no instruction-level work left to do, which is worth stating before asking whether a
different kind of machine helps.

Rotation is also where the classical optimizations stop: SHA-256's rotate distances are 2/13/22,
6/11/25, 7/18 and 17/19, and **none is byte-aligned**. No rotate can therefore turn into a
single byte-permute.

## Shor: the wrong tool, and the reason is measurable

Shor's algorithm finds the **period** of a function. It prepares a superposition over the domain,
applies the function, and lets the Fourier transform peak on the period. `shor_trace.py`
shows exactly that on N = 15 with base 7: peaks at 0, 4, 8, 12 because 7^x mod 15 has period 4, and
two controlled multiplies that move nothing at all because 7^4 mod 15 closes the orbit. The factors
fall out by gcd: gcd(48,15) = 3 and gcd(50,15) = 5.

That run **factors 15**. It has never touched a nonce, and no run in this tree has.

So the question that decides Shor's relevance is whether the nonce to hash map has a period.
`nonce_period.py` measures it on a chain-verified header with an exact Poisson tail and a
family-wise threshold: **null across 75 tests, detection limit 1.83e-02.**

Two walls follow, and the second does not care about the first.

**Size.** Shor needs about 2n+3 qubits for an n-bit domain:

| domain bits | qubits | amplitudes | bytes    |
| ----------- | ------ | ---------- | -------- |
| 16          | 35     | 1e11       | 1e12     |
| **32**      | **67** | **1e20**   | **1e21** |

A 32-bit nonce space wants 67 qubits as a full state vector, which is 1e21 bytes. The largest full
state-vector simulations performed anywhere are near 46 qubits.

**Structure.** Hand a 67-qubit machine an aperiodic function and it returns a flat distribution,
correctly and uselessly. The resource count is not the obstruction; the absent period is.

## Grover: the right tool, and it is not useless

Grover is the correct algorithm for unstructured search and it gives a genuine square root.
`qubit_trace.py` runs it: P(marked) climbs **0.003906 to 0.999947** at step 12, optimal at 13, then
**falls back**: the over-rotation that proves the iteration rotates, and does not
accumulate. (pi/4)sqrt(256) = 12.57.

Against block 966463 at difficulty 1.2745e+14:

|                                    |                 |
| ---------------------------------- | --------------- |
| classical hashes for one block     | 5.474e+23       |
| Grover oracle calls, (pi/4)sqrt(N) | 5.811e+11       |
| **reduction in calls**             | **9.420e+11 x** |

The conversion to a classical-equivalent rate is exact. A machine at G oracle calls a second finds
a block in iterations/G seconds, and it is therefore worth **G x 9.420e+11 H/s**:

| oracle calls a second | equivalent H/s | vs this card | vs network | time to a block |
| --------------------- | -------------- | ------------ | ---------- | --------------- |
| 1                     | 9.420e+11      | **399 x**    | 1.03e-09 x | 1.84e+04 years  |
| 1e4                   | 9.420e+15      | 3.99e+06 x   | 1.03e-05 x | 1.84 years      |
| 1e7                   | 9.420e+18      | 3.99e+09 x   | 1.03e-02 x | 0.7 days        |

**At even one oracle call a second a Grover machine is worth about 400 times this GPU.** The square
root does real work. What it loses to is the network at 9.123e+20 H/s, which outruns a
one-call-a-second machine by 9.68e+08 times.

Closing that has to happen in oracle speed, and the oracle is the problem. A Grover call is not a
hash: it is a coherent reversible circuit for the whole function, run in superposition, with no
measurement until the end. **Quoted:** published estimates put a quantum SHA-256 circuit at a few
thousand logical qubits and 1e5 to 1e6 T gates per evaluation, and mining needs SHA-256 twice.

| logical T gate | T per oracle | oracle calls a second |
| -------------- | ------------ | --------------------- |
| 1 us           | 1e5          | 1.000e+01             |
| 1 ns           | 1e5          | 1.000e+04             |
| 1 ps           | 1e5          | 1.000e+07             |

Against the **9.685e+08** needed for a ten-minute block. A picosecond logical T gate is far beyond
anything projected and still falls short by two orders of magnitude.

## The decisive point needs no hardware estimate

Mining is a race between many machines, and the two kinds scale oppositely when you add more:

| machines  | classical   | Grover  |
| --------- | ----------- | ------- |
| 10        | 10 x        | 3.2 x   |
| 10,000    | 10,000 x    | 100 x   |
| 1,000,000 | 1,000,000 x | 1,000 x |

Parallel Grover splits the search space, and m machines therefore buy **sqrt(m)**. Classical search buys **m**.
The square root that helps a single searcher **hurts a fleet**, and this argument survives any
hardware estimate because it contains none.

## What partial information would be worth

If the space factored into d independently searchable dimensions, Grover would cost d N^(1/2d):

| dimensions | oracle calls | vs plain Grover |
| ---------- | ------------ | --------------- |
| 1          | 7.399e+11    |                 |
| 8          | 2.436e+02    | 2.39e+09 x      |
| **32**     | **75**       | **7.73e+09 x**  |

**Seventy five oracle calls.** The arithmetic is correct. It needs partial credit: an oracle
that reports _this dimension is right_ while the others are wrong. A hash function is built to
destroy exactly that, since every input bit reaches every output bit.

Four instruments in this tree looked for partial credit independently:

| instrument             | result                                                            |
| ---------------------- | ----------------------------------------------------------------- |
| `nonce_read.py`        | null, drawn null over 200 shuffles                                |
| `beam_read.py`         | null at every live round, 256 line integrals, weakest limit 0.100 |
| `qubit_trace --invert` | a partial inversion returns zero at best, a loss at worst         |
| `qubit_trace --nested` | no partial credit on the way, reach grows as b^d                  |

**Quoted:** the BBBV bound proves sqrt(N) is optimal for a black-box oracle, and any scheme that beats
it exploits structure. The dimensional split beats it by 7.73e+09, the measure of how
much structure it silently assumes.

## Nest and invert is the price of coherence, not a speed-up

Computing forward, marking, then running the circuit backwards to uncompute is Bennett's
construction and it is **mandatory**: a Grover oracle has to leave its ancillas clean or the
interference does not happen. An oracle therefore costs roughly twice the forward circuit.

Measured here: `--invert` returns local purity to **1.000000000000** from 0.890625 when the
inversion reaches the whole structure, and a partial inversion returns zero at best and a loss at
worst. `--nested` shows reach growing as b^d with no partial credit on the way. Full reach works,
partial reach loses.

## What a qubit costs, and where the exponential actually lives

Three representations, measured:

| representation                   | bytes a qubit | 68 qubits  | how many fit in 46 GB |
| -------------------------------- | ------------- | ---------- | --------------------- |
| product state, 4 x float64       | 32            | 2176 B     | 2.29e+07              |
| graph state, 4 terms + adjacency | ~36 at n=68   | 2461 B     | 2.03e+07              |
| full state vector                | not linear    | 4.72e+21 B | **0**                 |

68 qubits is 2.46 kB in the graph form and 4.72e+21 bytes as a state vector. Same qubit count,
eighteen orders of magnitude apart. The exponential lives in the **entanglement**, never in the
qubit count.

And the cheap representations are cheap for a reason with weight here: they hold stabilizer
states, and stabilizer states are classically simulable. `exact_qubits.py` measures the same wall
from the other side: the Clifford gate set is exact at **0.000e+00** at every width and every depth,
because those gates are swaps and sign flips with no arithmetic in them. Nothing that exact is doing
work. 64 bits a term is the floor, measured: at 2 x float32 the state error is 1.2157e-07, at
2 x float16 it is 1.0162e-03, which sits above the detection limits this tree's instruments use.

## The one genuine asymptotic quantum win, and it is not on SHA-256

HHL solves Ax = b in O(log(N) s^2 kappa^2 / eps). It needs sparsity, size, and conditioning. A 1D
qubit chain is tridiagonal, and s = 2, and the graph representation reaches a million qubits in 8 MB.
Conditioning is the only open question, and the diagonal decides it.

A connected graph's Laplacian is **singular**: the all-ones vector is in its kernel. Add a diagonal
field term m and the spectrum moves from [0, 4d] to [m, 4d+m]. Then kappa = 1 + 4d/m, **constant in
N**. Measured against the exact finite-width prediction to **7.28e-16**, and climbing monotonically
to its bound: 4.8478, 4.9616, 4.9904, 4.9976, 4.9994, 4.9998.

Against the honest classical baseline, which is conjugate gradient at O(N s sqrt(kappa)) and not a
dense solve:

| dims  | crossover N   | in this representation |
| ----- | ------------- | ---------------------- |
| **1** | **6.554e+04** | **2 MB of qubits**     |
| 2     | 2.621e+05     | 8 MB                   |
| 3     | 1.049e+06     | 32 MB                  |
| 8     | 8.389e+06     | 256 MB                 |

The crossover **rises** with dimension, because s and kappa both grow with d and enter the quantum
cost squared while the classical cost takes them linearly and as a square root. Nesting therefore does not
lower the bar. It preserves the logarithmic N term at every dimension.

The field term is the strong lever: kappa = 1 + 4d/m, so

| field m | kappa | qubits needed |
| ------- | ----- | ------------- |
| 0.25    | 17.00 | 16.00 MB      |
| 1.00    | 5.00  | 2.00 MB       |
| 8.00    | 1.50  | **0.25 MB**   |

A 32 times stronger field cuts the qubit requirement **64 times**. More carried per qubit, fewer
qubits, and that is the conditioning bound, and not a metaphor.

Uneven field distance is punished by its weakest qubit, since kappa <= (lambda_max + m_max)/m_min:

| shape     | m min | m max | kappa     | crossover |
| --------- | ----- | ----- | --------- | --------- |
| uniform   | 1.000 | 1.000 | 5.00      | 6.554e+04 |
| spikeball | 0.250 | 8.000 | **40.78** | 2.097e+06 |
| fuzzball  | 0.754 | 1.271 | **5.28**  | 6.554e+04 |

A fuzzball costs almost nothing against uniform. A spikeball is **32 times worse** despite
carrying qubits at eight times the field.

**This is a real exponential-in-N advantage, and it is on the qubit map, and not on the hash.** It
solves a linear system. SHA-256 is not one.

## What the instruments could and could not have seen

A null is worth what its instrument's reach is worth, and that reach is derivable before any
measurement. The kernel of a reading map is a property of the map alone:

| instrument         | rank | kernel | a null from it excludes    |
| ------------------ | ---- | ------ | -------------------------- |
| harmonic degree 2  | 9    | 247    | 3.5% of directions         |
| harmonic degree 8  | 81   | 175    | 31.6%                      |
| harmonic degree 15 | 256  | **0**  | **the whole source space** |
| 256 beams          | 256  | **0**  | **the whole source space** |

Verified exactly: twelve kernel vectors derived from the map's SVD, added to a random object, moved
the reading by **7.1054e-15** against a reading scale of 3.4133e+01, a relative change of 2e-16,
while the object itself moved by 0.678. Those are null permutations by construction, and not below a
threshold.

This grades the two nulls in this document against each other. `nonce_read.py` uses six scalars,
five of them rotation-invariant power shares. Its null therefore excludes **at most 2.3%** of the
directions the state can move in. It is an honest run, and silent about the rest.
`beam_read.py` uses 256 beams, rank 256 and kernel 0, and its null excludes signal in
**every** direction the object has. It is worth a run for that reason.

The ceiling is not a wall, either. It is the source count:

| sources | rank | ceiling | kernel | arms to close |
| ------- | ---- | ------- | ------ | ------------- |
| 64      | 64   | 64      | **0**  | 0             |
| 256     | 81   | 256     | 175    | 175           |
| 1024    | 81   | 1024    | 943    | 943           |

Arms to close equals the kernel exactly at every width, and aimed placement beats random by a
factor of 1.00 to 1.02. Position is free and the count is the bill. At 64 sources a degree-8
reading has no kernel at all. Blindness appears only when the object carries more sources than
the reading has coefficients.

## Verdict

**The proof of work is structurally resistant, and the argument does not rest on a hardware
estimate.** Grover gives sqrt(N) at best, the BBBV bound says nothing does better on a
structureless oracle, four instruments here found no structure to exploit, the oracle is a coherent
double SHA-256, and parallel Grover buys sqrt(m) where classical buys m. Adding hardware helps the
defenders faster than the attacker.

**The signatures are a different matter.** Bitcoin spends are secured by ECDSA on secp256k1, and
Shor breaks discrete logarithms outright given a large enough fault-tolerant machine. That is the
same Shor as `shor_trace.py`, applied to a problem that **does** have the periodic structure the
nonce map lacks. The deterrent there is that the machine does not exist yet: a time sink,
and not a lock.

So: the mining is safe and the coins are not, and those two facts get called quantum resistance
interchangeably when only one of them holds.

**And the lever on the mining side is unchanged by any of this.** The card exhausts a 32-bit nonce
space in **1.82 seconds** at 2.359e+09 H/s. More trials is the only thing that has ever moved the
number.

## Apparatus

| tool                            | what it settles                                                        |
| ------------------------------- | ---------------------------------------------------------------------- |
| `examples/00_blob_viz_tools/shor_trace.py`      | Shor factoring 15, traced per step; what a period looks like           |
| `examples/00_blob_viz_tools/nonce_period.py`    | no period across 75 tests, limit 1.83e-02; `--grover`, `--dimensional` |
| `examples/00_blob_viz_tools/qubit_trace.py`     | Grover, amplitude amplification, `--invert`, `--nested`                |
| `examples/00_blob_viz_tools/exact_qubits.py`    | Clifford exactness at 0.000e+00, any width and depth                   |
| `examples/00_blob_viz_tools/product_machine.py` | fixed cost per qubit, and its negative control                         |
| `examples/00_blob_viz_tools/graph_machine.py`   | entanglement as one bit per pair, GF(2) rank entropy                   |
| `examples/00_blob_viz_tools/snap_release.py`    | the fourth term as an operation, and the dissipation null              |
| `examples/00_blob_viz_tools/hhl_nested.py`      | the conditioning bound, the field lever, the fuzzball answer           |
| `examples/00_blob_viz_tools/beam_read.py`       | the beam reading of the compression state, null with full reach        |
| `examples/00_blob_viz_tools/null_first.py`      | every instrument's kernel, derived before measuring                    |
| `examples/00_blob_viz_tools/aimed_arms.py`      | the ceiling is the source count, and the count is the bill             |
| `examples/miner/cuda_miner.cu`      | the classical implementation the whole comparison is against           |

Related: [the eye test](the-eye-test.md), [Grover against the chain](grover-against-the-chain.md),
[the snap and the rate](the-snap-and-the-rate.md), [the quantum machine](the-quantum-machine.md).
