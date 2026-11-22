# Grover against the chain

Douglas, 2026-09-11: _"can you implement Grover's, I think that's sqrt n."_

Right algorithm, right complexity, and it was already implemented. This is the accounting that grades it,
against the live difficulty, and not in the abstract.

## It was already here

`qubit_trace.py` carries Grover and amplitude amplification with the optimal-time detector. It
works:

> P(marked) climbs from **0.003906** to **0.999947** at step 12, optimal at 13, then **falls back**.

(π/4)√256 = 12.57, and the optimal count is therefore right. The fall-back is the clearest evidence that the
iteration rotates, and does not accumulate: an accumulating process does not come back. Every
qubit's read probability moves toward its bit in the marked pattern `10110101` as the search
converges. The answer therefore arrives in one amplitude at a time, and not one qubit at a time.

## What is measured, derived, and quoted

Keeping these apart is the whole value of the accounting, because the conclusion leans on a
literature figure this tree has not verified.

|                   |                                                                                                                                                                                      |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **measured here** | the difficulty and the nonce count, from the verified block corpus; this machine's rate from its own counters                                                                        |
| **derived**       | the Grover iteration count, (π/4)√N, standard and exact                                                                                                                              |
| **quoted**        | the cost of one coherent reversible SHA-256 circuit, from the published quantum preimage literature. Not verified here. The order of magnitude carries the argument, and the digits do not. |

## The reduction, and WHICH REGIME IT ASSUMES, which this document originally failed to say

Against block 966463, difficulty 1.2745e+14:

|                                                  | oracle calls  | reduction       |
| ------------------------------------------------ | ------------- | --------------- |
| classical hashes for one block                   | 5.474e+23     |                 |
| Grover, oracle over the **whole 640-bit header** | 5.811e+11     | 9.420e+11 x     |
| Grover, oracle over the **32-bit nonce only**    | **6.560e+18** | **8.344e+04 x** |

**THE FIRST ROW WAS THE ONLY ONE HERE AND THAT WAS WRONG BY SEVEN ORDERS OF MAGNITUDE.** Douglas
caught it on 2026-09-12 reading Manson and Sanders, _Strategies for quantum-enabled Bitcoin miners_,
arXiv:2607.23952v1. Their Remark 12 states the condition this document omitted: the iteration count
(π/4)√D "only applies for D ≤ 2^32 **or in the setting where the quantum-phase oracle encodes the
entire block header**", and they call that setting unrealistic "given that even just the 32-bit
quantum-phase oracle is known to be too expensive to construct".

A 32-qubit oracle searches only the nonce. At this difficulty the expected number of valid nonces in
one 32-bit space is |M| = 2^32 / D = **7.846e-15**, almost every header has none at all: a
quantum miner burns the full k_opt^32 = ⌊(π/4)√(2^32)⌋ = **51471** iterations, measures nothing, and
must change the header and start over. That outer loop over **1.275e14 headers is classical**.

**THE LOSS FACTOR EQUALS THE SQUARE ROOT OF WHAT LEAVES THE COHERENT SEARCH.** The
realistic count exceeds the ideal one by 1.129e+07, and √(1.275e14) = 1.129e+07 to four figures.
That general statement is worth more than either number: Grover pays the square root
only on the part of the search that stays inside one coherent amplitude. Anything classicalized
around it is paid in full, and the two do not compound.

The same shape governs the anchor in this tree's own miner. It is an early-out that skips 67 of
about 128 rounds per nonce, roughly 2.1x, and that ceiling cannot be raised by rejecting more
nonces because it already rejects all but one in 2^32. Both are real advantages that multiply
throughput and neither touches the probability per guess.

Grover is still the right algorithm to ask about. The question is what one call costs.

## What a Grover machine is worth in classical hash rate

This is the only apples-to-apples comparison. A machine at G oracle calls a second finds a block in
iterations/G seconds; a classical miner at H hashes a second finds one in N/H seconds. So the
machine equals **G × 9.420e+11 H/s**. The conversion is exact.

**EVERY FIGURE IN THE TABLE BELOW IS THE IDEAL REGIME. Read it as an upper bound, and not as a
forecast.** It uses the whole-header oracle factor of 9.420e+11. On the realistic 32-qubit oracle
the factor is 8.344e+04. Every equivalent hash rate below therefore falls by 1.129e+07, and every time-to-a-
block rises by the same. At one oracle call a second the realistic machine is worth 8.3e+04 H/s,
about a thirty-thousandth of this one GPU, and not four hundred times it.

| oracle calls a second | equivalent H/s | vs this card | vs network | time to a block     |
| --------------------- | -------------- | ------------ | ---------- | ------------------- |
| 1                     | 9.420e+11      | **399 x**    | 1.03e−09 x | 1.84e+04 years      |
| 1e4                   | 9.420e+15      | 3.99e+06 x   | 1.03e−05 x | 1.84 years          |
| 1e7                   | 9.420e+18      | 3.99e+09 x   | 1.03e−02 x | 0.7 days            |
| 9.68e+08              | 9.123e+20      | 3.87e+11 x   | 1.00 x     | matches the network |

**Grover is not useless.** At even one oracle call a second the
machine is worth 9.42e+11 H/s, about 400 times this card. The square root does real work.

What it loses to is the **network**, not the card. At 9.123e+20 H/s the network outruns a
one-call-a-second machine by 9.68e+08 times, and closing that has to happen in oracle speed.

## Why the oracle is the obstruction

A Grover call is not a hash. It is a coherent reversible circuit for the whole function, run in
superposition, with no measurement until the end. Published estimates put a quantum SHA-256 circuit
at a few thousand logical qubits and 1e5 to 1e6 T gates per evaluation, and mining needs SHA-256
twice.

| logical T gate time | T per oracle | oracle calls a second |
| ------------------- | ------------ | --------------------- |
| 1 us                | 1e5          | 1.000e+01             |
| 1 us                | 1e6          | 1.000e+00             |
| 1 ns                | 1e5          | 1.000e+04             |
| 1 ns                | 1e6          | 1.000e+03             |
| 1 ps                | 1e5          | 1.000e+07             |
| 1 ps                | 1e6          | 1.000e+06             |

Even at a picosecond logical T gate, far beyond anything projected, 1e5 T gates an oracle gives 1e7
calls a second against the **9.685e+08** needed.

## The decisive point needs no hardware estimate

Mining is a race between many machines, and the two types scale differently when you add more:

| machines  | classical speedup | Grover speedup |
| --------- | ----------------- | -------------- |
| 10        | 10 x              | 3.2 x          |
| 100       | 100 x             | 10.0 x         |
| 10,000    | 10,000 x          | 100.0 x        |
| 1,000,000 | 1,000,000 x       | 1,000.0 x      |

Parallel Grover splits the search space, and each machine searches its share. m machines therefore buy √m,
and not m. Classical search buys m. That is a standard result and it is the opposite of what mining
rewards.

**So the square root that helps a single searcher hurts a fleet.** The network's classical advantage
grows linearly in hardware while a quantum fleet's grows as a square root. Grover is the right
algorithm for one machine against one unstructured space, and mining is neither of those things.

## Where the metaphor breaks

Grover does not pull the arms faster. It pulls **fewer** arms: 5.811e+11 instead of 5.474e+23. But
each pull runs the whole machine backwards, coherently, in superposition, with no peeking until the
end. The classical pull costs 0.42 nanoseconds. The Grover pull costs about a second.

For scale, from this machine's own counters: 2.359e+09 hashes a second exhausts a 32 bit nonce space
in **1.82 seconds** with none of the above.

## The dimensional split, which is a price tag on partial information

Douglas: _"if it's sqrt do the dimensional 2s comp, you have n dimensions."_

Sound reasoning and the right question to ask of a square root. If the space factors into d
independently searchable dimensions, Grover costs d · N^(1/2d) instead of √N:

| dimensions | oracle calls  | vs plain Grover |
| ---------- | ------------- | --------------- |
| 1          | 7.399e+11     |                 |
| 2          | 1.720e+06     | 3.38e+05 x      |
| 4          | 3.710e+03     | 1.57e+08 x      |
| 8          | 2.436e+02     | 2.39e+09 x      |
| 16         | 8.830e+01     | 6.58e+09 x      |
| **32**     | **7.517e+01** | **7.73e+09 x**  |
| 64         | 9.809e+01     | 5.92e+09 x      |

The optimum is near d = 32 at about **75 oracle calls**. Neither 5.5e23 hashes, nor 5.8e11 Grover
calls. Seventy five. **The arithmetic is correct.**

**AND IT IS THE SAME MISTAKE AS THE REGIME ABOVE, IN ITS MOST EXTREME FORM. Correct arithmetic
is therefore beside the point.** The table prices a search that has been split into 32 pieces searched
_independently_: the assumption that the structure outside each piece comes for free. The
regime correction above shows what that assumption costs when it is false: every factor moved out
of one coherent amplitude is paid in full, and not in square root. Splitting into d pieces and
paying √ on each is only cheaper if the pieces can be solved separately, and a hash is designed so
they cannot.

This tree measures that directly, and does not argue it. `nonce_period.py` found no period in the
nonce-to-hash map, null across 75 tests with an exact Poisson tail. `nonce_read.py` found that a
boundary reading of an early compression state does not predict the digest, null with a matched null
over 200 shuffles. On 2026-09-12 an ordering test over 2000 real headers and 512,000 double
SHA-256 found no cheap feature of the nonce that predicts a lower digest, p = 0.284 against an exact
permutation null, with a positive control confirming it would have caught a 5% bias.

So d = 32 is not a route, it is a **price tag on partial information that nothing in this tree has
been able to buy**. Seventy five oracle calls is what the answer would cost if SHA-256 leaked, and
every measurement says it does not.

It needs **partial credit**: an oracle that reports _this dimension is right_ while the
others are still wrong. A hash function is built to destroy exactly that, because every input bit
reaches every output bit. No projection of the input is therefore separately checkable.

Four measurements here, with four different instruments, found none:

| instrument             | result                                                                                       |
| ---------------------- | -------------------------------------------------------------------------------------------- |
| `nonce_read.py`        | null, drawn null over 200 shuffles                                                           |
| `beam_read.py`         | null at every live round, 256 line integrals adding 175 rank dimensions, weakest limit 0.100 |
| `qubit_trace --invert` | a partial inversion returns zero at best, a loss at worst                                    |
| `qubit_trace --nested` | no partial credit on the way, and reach grows as b^d                                         |

And the BBBV bound proves √N is optimal for a black box oracle, and any scheme that beats it
exploits structure. This one beats it by 7.73e+09 times, the measure of how much
structure it assumes. Quoted, and not verified here.

So this is not a dead end in the idea. It is the same wall from a fifth side, and it is the
cleanest statement yet of what partial information would be worth if it existed.

## Nest and then invert

Douglas: _"nest and then invert them to increase op speed."_

This describes Bennett's compute, copy, uncompute, and it is not a speedup: it is mandatory. A
Grover oracle must leave its ancillas clean, or the interference does not happen. The circuit therefore
computes SHA-256d forward, marks the result, then runs backwards to uncompute. An oracle therefore
costs roughly twice the forward circuit.

Measured here, and not argued: `--invert` returns local purity to **1.000000000000** from
0.890625 when the inversion reaches the whole structure, and a partial inversion returns zero at
best and a loss at worst. `--nested` shows reach growing as b^d with no partial credit on the way.
Full reach works and partial reach loses: the same sentence as the section above.

So nesting and inverting is the price of staying coherent long enough to interfere, not a way to go
faster.

## On the resistance being reassuring

Douglas: _"any lock is a deterrent and a time sink designed to last long enough to be noticed."_

That is the right frame and it is worth being exact about what is locked. Two different things get
called quantum resistance here and only one of them holds.

**The proof of work is structurally resistant.** Grover gives √N, the oracle is huge, and
parallel Grover gives √m where classical gives m. Adding hardware helps the defenders faster than
the attacker. Nothing here depends on a hardware estimate.

**The signatures are not.** Bitcoin spends are secured by ECDSA on secp256k1, and Shor breaks
discrete logarithms outright given a large enough fault-tolerant machine. That is the same Shor as
`shor_trace.py`, on a problem that _does_ have the periodic structure the nonce map lacks: this
tree measures no period across 75 tests on a real header, and mining is safe for that reason, and the
elliptic curve problem has exactly the structure Shor eats.

So the honest version of the reassurance: the mining is resistant, the coins are not, and the
deterrent on the signature side is that a machine big enough does not exist yet. That is a time
sink, and not a lock, as the quote above says, and it is the half worth watching.

## Apparatus

- `examples/00_blob_viz_tools/qubit_trace.py`, Grover and amplitude amplification, traced per step
- `examples/00_blob_viz_tools/nonce_period.py --grover`, this accounting against the live corpus
- `examples/00_blob_viz_tools/nonce_period.py`, the period probe: null across 75 tests, limit 1.83e-02
- `examples/00_blob_viz_tools/shor_trace.py`, Shor factoring 15: the look of a period, when there is one

Related: [the eye test](the-eye-test.md), [the quantum machine](the-quantum-machine.md).
