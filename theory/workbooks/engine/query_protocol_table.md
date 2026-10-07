# The query protocol table

**Purpose:** The query protocol step by step: what each step asks, the algebra it holds to, what it does, what it wants, what was tried and what it gave, its status, and the next move.
**Scope:** every step between a question put to a target and a branch consuming the answer: the ask, the unbound and bound passes, the gate, the order of asks, the contention read, cross-branch comparison and the record. Its inputs are the entries of `Lstar.klq`, the bridge between languages, and of each language's `.klm`, and every step gives back entries of them. The protocol's form and its mnemonics are in [gnascor.md](../../../src/cu/transpiler/gnascor.md), and the open work around it is in [engine_plan.md](../../../src/engine_plan.md). Statuses follow [README.md](README.md), and every status names the run behind it.

## The rule the protocol is held to

**Gate, then rank. Never one score.** A relation holds or it does not, and that answer carries no noise. A cost is measured and every cost carries noise. The gate decides which candidates are admissible and the rank orders whatever survives, and the two are never added together. Kept apart, a noisy cost can cost speed and can never cost correctness. The engine already holds this rule in its own descent: survival is a conjunction, order cannot change a conjunction, and a planner that steers badly costs speed and never a wrong survivor (`src/cu/engine/nbody/orior/orior_descent.h`).

**Steer on what is known to be true.** Every relation the ladder holds is arithmetic every system that computes agrees about, and the host computes it. The gate is planned against that ground truth on the host, and a target is asked only the cases the plan placed.

## The inputs

**The inputs are entries.** `Lstar.klq` is keyed by the schema's form names, which are gnascor's and no target's. For each key it holds the pairs of forms whose sameness the text leaves open, the cases put to them, and the verdict: closed as two operations with the case that witnessed it, or open with its count of cases alike. A language's `.klm`, `cu.klm` or `sass.klm`, keys that language's names to the bridge, each entry naming its relation on its own line. An address is a key of the bridge read through a language's map, and a map from one language to another is read through the bridge, `cu.klm` to `Lstar.klq` to `sass.klm`. `klq_write` (`src/cu/transpiler/lstar/protocol`, run by `utils/maint/engine/klq_write.sh`) writes `Lstar.klq` and each given language's `.klm` beside the rulesets, the forms of each line in `src/cu/types/file_defs/klq` and `klm`, and no step reads one.

**A reading of a part of a set is 1, 0 or gray.** No set of entries is known to be whole. A witnessed relation that holds of a part holds of every set holding that part, and its 1 is a verdict; where it does not hold, a file added can make it hold, and the reading is gray. A relation of absence reads the other way: a name given is given in every set holding it, and that 0 is a verdict, and a name not given is gray. gray is gnascor's state: no answer, and not void, which is an answer of no (P1, P10).

## The files

Every file the protocol is made of, by what it holds. Paths are from the root of the tree.

**The ask and the bus.** `src/cu/transpiler/lstar/protocol/`

| file | holds |
|---|---|
| `host_entry.h` | the two relations a host is reached by, a word put and a word read at an address, and the kind an address shows and the width a sizing register names |
| `query_ask.h`, `query_ask.c` | one ask: an address, a qualifier, the cost read off a clock found by asking, and the bit with its kind (Q1, P1) |
| `query_interface.h`, `query_interface.c` | asks put from inside the interface in probes it can lose, an ending kept as the answer of the address that caused it (Q1) |
| `query_walk.c` | the program a walk of asks runs as, inside the cell |
| `classify_walk.c` | the program a classify walk runs as: the kind ask at each address of a span |
| `bus_enum.h`, `bus_enum.c` | the enumeration region found in a run of kinds: a fixed identifier beside a live sizing register at one stride |
| `bus_enum_walk.h`, `bus_enum_walk.c` | the classify walk driven over a span in probes, and the region read from it |
| `run_channel.h` | the run channel: a container in, eight words put, four read back (Q1, Q15) |

**The gate, the order and the rank.** `src/cu/transpiler/lstar/protocol/`

| file | holds |
|---|---|
| `ladder.h` | the relations known before anything is met, written as cases |
| `chain_build.h`, `chain_build.c` | every arrangement of primitives that answers a relation's cases (Q4) |
| `ask_order.h`, `ask_order.c` | the known order of asks, its solve, and the read of whether the links contend (Q5, Q7, P3, P4) |
| `query_order.h`, `query_order.c` | the known order put through the protocol, each cost an exact rational (Q5) |
| `stem_group.h`, `stem_group.c` | members grouped on anchors by a rule the members alone fix (Q13) |

**The part.** `src/cu/transpiler/lstar/protocol/`

| file | holds |
|---|---|
| `monolith.cu`, `monolith.h` | one program holding every base precept between tags, its costed chains and their layout (Q2, Q17) |

**The relations of the text.** `src/cu/types/file_defs/readers/`

| file | holds |
|---|---|
| `ruleset_core_relation.h` | the absurd relations among the entries of any files read together, and the map from one set to another (Q19, P10) |
| `ruleset_flat.h`, `ruleset_flat.cu` | the files a ruleset is read from, read into the caller's memory, and what each relation says |

**The bridge.** `klq_write.sh` writes `Lstar.klq` and each language's `.klm` beside the rulesets in `src/cu/transpiler/lstar/coherence`, and each is tracked: `cu.klm`, `sass.klm` and `openqasm2_0.klm`. `klq_identity.sh pair` writes each pair's verdict beneath it, and `klq_decoder` its set and its concept.

**The checks.** `utils/test/src/cu/transpiler/lstar/protocol/`, built and run by `utils/maint/engine/chain_check.sh`

| file | holds |
|---|---|
| `query_ask_check.c` | the ask held to addresses whose answers are known (Q1) |
| `query_interface_check.c` | walks of asks from inside the interface held to known addresses (Q1) |
| `host_entry_check.c` | the kind and the width read from words put and given back |
| `bus_enum_check.c` | the classify and width asks, the region found, and the walk in probes |
| `gate_descent.c` | the gate run as the engine's own descent (Q4, Q15) |
| `chain_check.c` | how much of a relation the ladder's own cases decide (Q4) |
| `ask_order_check.c` | the known order held to its exact claims at every size (Q5, Q7) |
| `query_order_check.c`, `query_descent.h` | the known order put to the host, its run size and passes steered by the part (Q5) |
| `branch_side_check.c` | whether the side a branch is asked from leaves a mark (gnascor Open 4) |
| `stem_group_check.c` | the stem rule held to what it says (Q13) |

**The relations, checked.** `utils/test/src/cu/engine/rmc/ruleset_read_test.cu`, run by `ruleset_read_test.sh`: every target's files and the map between cu's and sass's (Q19).

**The part, asked.** `utils/test/src/cu/transpiler/lstar/interface/`: the SASS probe (`interface_sass_probe_*.c`, `interface_sass_probe.h`), its runs (`interface_sass_*.sh`) and their records (`interface_sass_*.md`) (Q16, Q17). The probe goes through the toolkit and is scaffolding, an answer key and never the run channel.

**The readings.** `utils/maint/engine/`: `measure_check.py` (Q5, Q6, Q7), `order_check.py` (Q10 to Q14), `gnascor_read.py`, `gnascor_trace.c`, `gnascor_scenario.txt` and `gnascor_measure.py` (the state machine read off asks).

## The table

| step | the algebra it holds to | does | wants | tried, and what it gave | status | next |
|---|---|---|---|---|---|---|
| **Q1. The ask** | `[address] -> (qualifier) -> [cost] -> bit`. The qualifier asks for a state validation and never a data payload. Over the bridge the address is a key of `Lstar.klq` read through a language's `.klm`, `[key] -> (relation) -> [cost] -> bit`, and the qualifier asks one relation of the entries the key names. A relation of the text carries no cost, and its bit is 1, 0 or gray (P1). | `src/cu/transpiler/lstar/protocol/run_channel.h` declares the channel: a container in, eight words put, four read back, and an outcome of answered, nothing, illegal or no channel. Nothing implements `run_channel_ask` and nothing calls it. | Every ask put with `host_put` and read with `host_read` (`src/cu/transpiler/lstar/protocol/host_entry.h`) and nothing between them and the part: no compiler, assembler, disassembler, object reader, vendor runtime or driver library. Where a part's addresses lie is itself asked, an address holding, fixed, live or answering nothing. The SASS probe under `test/engine/compiler/interface/` runs through the toolkit and is scaffolding, an answer key and never this channel. An ask at a key carries the key and the language it is read through, and its answer is written to the key's entry in `Lstar.klq`. | **Proved** on the host (`test/engine/compiler/bootstrap/query_ask_check.c`, run by `maint/engine/chain_check.sh`, 16 checks, 0 failed): `query_ask` (`src/engine/compiler/bootstrap/query_ask.{h,c}`) finds memory the test owns holding and leaves it as it was, reads a written word as that word and no other, reads a word nothing writes as not advancing, and reads a bound with no clock as past the bound. **Measured**: the host's interrupt time at a fixed address advances under the ask, in steps of half a millisecond to a millisecond, and an ask clocked on it reads a cost of a whole step or nothing. A bound from it judges a run of asks. **Proved** from inside the interface (`test/engine/compiler/bootstrap/query_interface_check.c`, 8 checks, 0 failed): `query_interface_walk` (`src/engine/compiler/bootstrap/query_interface.{h,c}`) runs `query_walk` in a probe and keeps an ending as the answer of the address that caused it. A read at address 0 ends the asker on an address fault, the page every Windows process shares answers 64 reads in one child and ends the asker on a put at each word, and of its first sixteen words the walk finds the two its layout names as counters, at 0x8 and 0x14. | **proved** (the ask on the host, the walk from the interface), **measured** (the host clock's step) | The run channel made of asks of this form (Open 2 in the plan): the enumeration space found not by a written address but by its answer, a FIXED identifier beside a LIVE sizing register repeating at one stride, the kind from `host_address_ask` and the stride from `period_read`; a region's width read from the mask a LIVE register gives back when written all ones, the part setting it. |
| **Q2. The unbound pass and the baseline** | An ask with no bound returns its cost in place of a bit. The spread of those costs is `.knf`, and every bound after is expressed against it. Over the bridge a cost is kept at the key it was read at, its spread beside it, and a cost read in one language is read at the same key in another. | `monolith_cost` (`src/cu/transpiler/lstar/protocol/monolith.cu`) reads the part's clock (`CS2R SR_CLOCKLO`) before and after a chain of each base precept, 100 steps a turn and `turns` turns, every step one PTX instruction under a predicate read from a parameter the assembler cannot see, and each precept again interleaved with every other. `monolith_run` puts it `cost runs` times and reads each section's least m, its spread s and its mean, and holds every chain's last word to `precept_applied` on the host. | A watchdog stop marked as a censored sample and not a cost. A reference ask put alongside each real one, in the same conditions. | **Measured** on the RTX 3070, sm_86 (`monolith_run`, 10000 turns of 100 steps, 16 runs, 315 checks, 0 failed): every single-instruction precept reads m = 4.05 to 4.06 cycles a step, NAND and NOR 8.05, NOP and MOV 0. The spread s reaches 2.22 cycles a step alone and 2.53 interleaved, and the means, 4.14 to 4.45, sit inside it. A chain written plainly comes out of the assembler as one instruction a turn, an empty fence after each step notwithstanding; predicated, every step is kept. | **measured** | The spread written to `.knf` as the part's floor, and the bound Q3 reads off it. |
| **Q3. The bound pass** | The same ask with a bound returns 1 or 0. A missing address, a false qualifier, a timeout, an error and too many cycles all read 0, and what separates them is kept in the baseline and not in the bit. Over the bridge the bound of an ask at a key is read off the spread kept at that key. | Nothing. | The bound read off Q2 and never written by hand. | Nothing has run. | **theory** | Follows Q2. |
| **Q4. The gate, as the engine's own descent** | Each candidate arrangement is an alignment, the relation's cases are the needle, and the oracle answers whether arrangement (`corpus_at - needle_at`) gives case `needle_at` the relation's word. `anchor_steer_spawn_coarms` places the case that prunes most, stops where the best case prunes nothing, and leaves the survivors as its output. Over the bridge the candidates are the forms a language's `.klm` and the part's machine file give for a key, the cases are the key's in `Lstar.klq`, and the survivors are written back to the key. The reader's relations are a gate of the same form over a set of files: a set holding a witnessed absurd relation is refused, and no order of the files or of their lines changes which sets are refused (P2). | `test/engine/compiler/bootstrap/gate_descent.c`, built and run by `maint/engine/chain_check.sh`, over every relation the ladder holds, against `chain_build`'s candidates and its own 512 sweep words. | The descent run on the host before any target is asked, and only the cases it placed put to the target. | **Proved**, every relation: the descent's survivors equal every candidate asked every case, and both equal what `chain_build` keeps with its sweep on (add 1,202 of 1,227, take 1,047 of 1,083, up 411 of 698, down 408 of 559, same 0 of 8), 0 lost. A descent with the destroy rule off leaves the same count as one with it on. **Measured**, what decides each relation: one full-width sweep word decides add, take and same, and up and down need a second whose count reads zero (`0x43f96f40`, `0x3ef25a20`). No case the ladder holds is among them. Every impostor the ladder lets through, 507 in all, dies at the first or second case placed. **Measured**, the planning: a level reads every case against every survivor, 924,931 to 1,888,145 questions per relation. It runs on the host for that reason. **Measured** on the part (`interface_sass_descent.md`): the cases `gate_descent` places for each relation, one or two, are put to every arrangement it descends over, 3575 of them, on the part alone. What stands on the part stands on the host for every relation, and no verdict differs. | **proved** (survivors), **measured** (deciding cases, planning cost, the placed cases on the part) | Put the descent ahead of every ask in the loop, and read its survivor count as the reading it is (Q11). |
| **Q5. The order of asks for slicing** | A chain of `n` links measured by asks that each cover half the links, any two overlapping on a quarter: ask `r` covers link `c` where `(r + 1) & (c + 1)` has an odd count of ones, the rows of a Hadamard matrix of order `n + 1` with its first row and column dropped. One ask informs every link at once. The per-link solve is `(n + 1)·x = 4·Sᵀb - 2·(Σb)·1`, integer adds and a division by a power of two. Over the bridge a link is a key: a construct's lines, each read through a language's `.klm` to its key, and each link's solved cost is kept at that key. | `src/engine/compiler/bootstrap/ask_order.{h,c}`: `ask_order_covers` gives the order from the link count alone and stores nothing, and `ask_order_solve` gives every link's cost scaled by `n + 1`. A count one short of a power of two, 3 to 63, has a known order, and every other count is refused and writes nothing. | A chain composed to a length a known order holds, and its asks emitted to a target (Q1). | **Proved** (`test/engine/compiler/bootstrap/ask_order_check.c`, run by `maint/engine/chain_check.sh`, 1,231 checks, 0 failed, at 3, 7, 15, 31 and 63 links): `4·Sᵀ - 2·J` times `S` is `(n + 1)` times the identity entry by entry; every ask covers `(n + 1)/2` links and any two share `(n + 1)/4`; integer link costs solve back exactly at 1 to 8 passes; a fixed cost on every ask, and a cost growing with the square of the count an ask covers, each move every link by one amount and leave their order as it was; every count 0 to 127 without a known order is refused. **Measured**, against one ask per link at one budget (`python maint/engine/measure_check.py`, check 4): the gain is √(n + 1)/2, 1.09x at 3 links, 2.07x at 15, 8.16x at 255, against 1.00, 2.00 and 8.00 predicted. **Measured**, against a drawn order at the budget with nothing spare (check 5): the known order wins by 5.5x at the median worst-link error, 33.7x at the 95th and 125.1x at the worst of 400, and 17 of 400 drawn orders do not come apart at all. **Measured** on the host through the protocol (`test/engine/compiler/bootstrap/query_order_check.c`, run by `maint/engine/chain_check.sh`): `query_order_put` (`src/engine/compiler/bootstrap/query_order.{h,c}`) puts seven links, each a QUERY_ADVANCES ask at a word nothing writes and link k reading it 1 + 64·k times, on the clock the protocol finds by asking, read finely by counting reads of it between turns: each run's cost an exact rational, each of 64 passes solved on its own in exact integers, nothing rounded, summed or cut to a least. The run size is read off the part, the size whose weakest neighbor pair leans hardest; the floor against size has a least between the counting's spread and the interference long runs gather, and the size named moves from run to run (2^10 to 2^14 puts). Every neighbor pair reads dearer in 44 to 54 of 64 passes, past twice the spread. One read against five, QUERY_EQUALS against QUERY_HOLDS, does not order: an ask's own overhead is larger than four reads. | **proved** (the order, the solve and what the count's square does to it), **measured** (gain, known against drawn; the order put to the host) | Emit the order to the device once the run channel reaches its addresses. |
| **Q6. Slicing by adjacent cuts** | A chain's per-link cost read as the difference between neighboring prefix cuts. | Nothing uses it. | Nothing: it is kept as the number that failed it. | **Refuted** (`measure_check.py`, checks 1 and 2): a recovered link carries 1.43 floors of noise against a signal of 1.00 and orders 56% of link pairs correctly, where a coin orders 50%. 1,600 repeats of every cut order 95%. | **refuted** | Never tried again blind. Q5 replaces it. |
| **Q7. The contention read** | Where links contend the cost of a set is not the sum of its parts. Contention grows as the square of how many links an ask covers. What is left of a sweep ask's cost once the known order's links are taken out is read against a constant, the count and the square of the count, and contention is read where the square's slope clears twice its own spread: `Puy²·(N + 1) > 4·Pyy·Puu` with the count taken out, every term an exact integer. Over the bridge the links that contend are named by their keys, and a contention read is kept with the keys of the asks it was read over. | `ask_links_read` in `src/cu/transpiler/lstar/protocol/ask_order.c`. The sweep asks cover one link, half plus one and every link, in turn, each a prefix of an order of the links drawn from a seed. | Sweep asks emitted beside the known order's (Q1). | **Measured** (`ask_order_check.c`, 15 links, 4 passes of the known order, 45 sweep asks, 300 trials, costs in integers around an overhead with a spread of 1000 a pass): contention found on 3.0% with none, 83.6% with up to 80 a pair, 100% with up to 200. **Measured**, the two terms beside the square: every ask pays an overhead, and the known order spreads it evenly over every link, which reaches a sweep ask in proportion to the count it covers. Read against the square alone the read found contention on 0% at every amount; with the constant and the count taken out it reads as above. **Measured**, where the sweep asks sit: covering every count from 1 to 15 found 54.0% at up to 80 a pair, and the two ends and the middle found 83.6% from the same 45 asks, the false alarms 2.0% and 3.0%. **Measured** in floating point (`measure_check.py`, check 6): an additive solve's per-link answers are 1.27x as far off at 0.08 a pair, and the square's term carried in the solve brings them back to 1.01x. On the host the sweep asks are put and timed as exact rationals (`query_order_sweep`), and the read itself takes integer costs: it is not yet put over exact rationals, and no host reading of it stands. **Measured** on the part (`monolith_run`, Q2): two chains with no link between them, interleaved a step at a time, cost the dearer of the two alone, in all 91 pairs of precepts that each cost a cycle a step or more. Chains that share no link do not contend. | **measured** | Emit the sweep asks to the device beside Q5's. |
| **Q8. Cross-branch comparison** | Two branches over one relation compared link by link through the signed per-link difference. The totals are even under swapping the branches and the question of which branch holds a link is odd under it. A total cannot answer it. The sign at each link is lead or rite, a difference inside the floor is dual, a link neither branch passes the gate on is void, and pass marks where the sign changes along the chain. Over the bridge two branches written in two languages are compared at the keys they share, each read through its own `.klm`, and a key one branch reaches and the other does not is gray at that link. | Nothing. | A crossing at a pass costed as its own link, because a path that switches branches pays for the switch. | Nothing has run. The rule that a symmetric magnitude cannot see an odd question is shown on the first move in chess (`examples/game_theory/6_oracle/first_move_advantage.py`). | **theory** | Follows Q5. |
| **Q9. The record** | A known order is a carrier and a shuffle is not: the same asks reach the part either way, and only the record of the order makes the answers decodable. The record holds every ask, its order and its seed, every answer with its cost, refusals and censored samples each marked, and then the winning path per problem over those same asks. | `chain_shuffle` takes a seed. Nothing writes the record. | The record's face, under the member's stem beside `.kdm` and `.knf`. The face is settled and Doug names its suffix (Open 9 in the plan). The record stores what the host cannot reproduce and nothing it can. The known order follows from `n`, the sweep from its seed, the candidates from the ladder's cases and the descent's placement from those. The record holds those few numbers and the target's answers, and the file is small for that reason. `Lstar.klq` holds the questions put across languages and their verdicts; the record holds one member's asks and their costs. | Nothing has run. | **theory** | Follows Q1. |
| **Q10. What an answer holds for** | A reading answers for the part it was taken on and the size it was taken at, and for nothing else by default. Over the bridge the part and the size are carried in the key's entry beside each answer, and a verdict of the text, which no part gave, carries neither. | Nothing records either. | The part and the size carried beside every answer. A generic block is the fallback for a member with nothing measured, never a result borrowed from one that has. | **Measured**, on modeled costs (`python maint/engine/order_check.py`, check 11): the arrangement winning at size 3 costs 79.4x the best at size 300, a winner carried to another part costs 3.06x that part's best, and the two penalties multiply. | **measured** | Carry both in Q9's record. |
| **Q11. The survivor count** | How many candidates survive the gate is itself a reading. One survivor decides the operator, and many say the cases do not, which another relation answers and no measurement can. Over the bridge the count is kept at the key, and an open pair's count of cases alike is the same reading (P10). | Q4 reports it per relation. | Read in the loop and kept. | **Measured** (`order_check.py`, check 8): 32 candidates over add fall to 11, 5 and 1 as cases are put. Q4 gives the same reading on the ladder's real candidates. | **measured** | Read it where Q4 runs. |
| **Q12. One combined score** | Precepts held and cost folded into one number at some exchange rate. | Nothing uses it. | Nothing: it is kept with the numbers that failed it. | **Not so** (`order_check.py`, checks 7 and 9): one score ships a wrong program at 2 of 5 exchange rates. A count of precepts held is not a count of independent facts either: 1 of 5 cases decides add on its own, and every case is then implied by the rest. | **not so** | Never one score. The rule at the top of this table holds it. |
| **Q13. Membership by agreement inside a floor** | Two members share a stem where their readings agree inside a floor. | Nothing uses it. | Nothing: it is kept with the reason it fails. | **Not so** (`order_check.py`, check 10): asked from one member's floor, agreement is not symmetric, and taken at the coarser floor it is not transitive. sm_86 agrees with sm_87 and sm_87 with sm_89, and sm_86 does not agree with sm_89. | **not so** | A representative per group, or a rule that builds the group and names its anchor (Open 11 in the plan). |
| **Q14. The readout** | The bits a branch consumes resolve to a four-letter mnemonic per transition. Over the bridge a verdict reads 1 or 0 and an open reading reads gray, no answer, and a branch over keys reads gray where the bridge holds no verdict. | `gnascor.md` holds the two transition tables. | Each name covering more than one transition says where its distinctions are kept, as the bound pass keeps them in the baseline. | **Measured** (`order_check.py`, check 12, read from `gnascor.md`): of 49 transitions in the state table, 12 resolve to a name nothing else uses, and sync covers 22. | **measured** | sync split, or its distinctions kept in the baseline (gnascor Open 4). |
| **Q15. One ask, many answers** | An ask can carry many checks and its answer one bit for each. The channel answers in `RUN_OUT_WORDS` words of 32 bits, 128 bits an ask. A single bit over a set, 1 where every check in it holds and the set halved on a 0, carries less: it pays only where checks seldom fail, and past a failing share of (3 - √5)/2, about 38%, asking one at a time costs fewer asks. Over the bridge a check is a pair of a key and one case, and a pair a case has closed as two operations is closed on every set of cases holding that case and is not asked again (P5). | Nothing emits a pooled ask. | A container that runs many checks in one launch and writes one bit each into its answer words. | **Measured** (`gate_descent.c`, every arrangement against the cases Q4 placed): add, 1,227 checks with 25 failing: 1,227 asks at one a check, 10 at one bit a check, 129 at one bit a set halved on a 0 and 299 with the checks in a drawn order; take, 1,083 checks with 36 failing: 1,083, 9, 145 and 367; up, 1,396 checks with 538 failing: 1,396, 11, 1,593 and 2,141; down, 1,118 checks with 263 failing: 1,118, 9, 927 and 1,389. One bit a check takes the fewest on every relation. One bit a set takes more than one a check on up, where 38.5% fail, and its count turns on how the failing checks sit together. | **measured** | The answer carries a bit a check, and Q1's container is built to write one. |
| **Q16. A form asked of the part** | A slot of a ruleset is asked the way an operator is (P7). Every form the part's machine file holds is put in the slot with its operands filled by their kinds, the cases of the slot are put, and the forms that answer every case are timed and walked. The ruleset's form for the slot is the part's answer. Over the bridge the slot is a key of `Lstar.klq`, its candidates the machine file's forms, and the part's answer is written to the language's `.klm` at that key. | `loop_back_if` is asked so: `interface_sass_probe` given `loop` and a machine file (`src/cu/scaffolding/interface_sass_probe_ask.c`), run alone against the cubins of an earlier run through `SASS_PATTERN` in `interface_sass_test.sh`. **Measured** on the RTX 3070, sm_86, in two runs: 2533 of the 2927 forms assemble, 1715 fall through and 8 come back on every count, every one a `BRA`. No form answers 2 without answering every count. `sass_loop_walk` agrees with all 8. The 8 cost 33.7 to 34.7 ns a turn, and the next costs 0.0525 ns a turn more than the least against a spread of 0.9260: no form costs less (P6), and `sass.krs` keeps its `BRA`. **Measured** on the part (`interface_sass_writings.md`, `interface_sass_writings.sh`): every form of the machine file that writes a register from registers, predicates and numbers alone, 745 of 2928, is put in place of the frame's IADD3, each through the gate, and run over 256 cases at once: 5685 cubins, 5394 run and the part refuses 291. Every precept that carries a word holds under at least one form, and every ladder relation but same. Every arrangement of `machines/sm_86.kdm`, written node by node from the writings found, answers its relation on every case (`interface_sass_chains.md`): add 1202, take 1047, up 411 and down 408. Of `sass.krs`'s forms for the word web's eight words of one precept, six are writings the part gives, and `word_shl` and `word_shr` are not (`interface_sass_krs.md`). | Every other slot a `.krs` writes by hand asked the same way, each with its own relation's cases, on every part with a machine file. | The least cost read alone from one run: `BRA.CONV` in one run and `BRA` with a number in the next. Inside the spread the order is noise, and P6 reads it. | **proved** (the 8 forms, the walk), **measured** (their cost on sm_86, the writings and arrangements on the part) | Q17 says which slots to ask next. |
| **Q17. Held against NVIDIA's compiler** | A program written as C source and as the part's own code answers the same on every lane and costs no more than what NVIDIA's compiler writes for the C (P8). Where it costs more, the forms NVIDIA's compiler wrote over that run are the next slots Q16 asks. Its listing is a reading of what the part can do and never a rule: a form enters a ruleset once the part answers for it. Over the bridge the program's lane is read `cu.klm` to `Lstar.klq` to `sass.klm`, and a relation the map holds is a line the lane cannot be read through as it stands (P8). | The monolith (`src/cu/transpiler/lstar/protocol/monolith.cu`): one program holding every base precept between tags, built once by NVIDIA's compiler, its listing the answer key. `src/cu/scaffolding/monolith_emit.sh` holds every block against our reader, our assembler and the word our compiler writes the block's precept with through `sass.krs`, and writes `monolith_differences.md` whole on every run. `src/cu/scaffolding/sass_lane_needs.sh` decides the 8 record programs the host oracle runs for SASS: 16 lane programs written, 84 of the schema's 99 forms asked, every instruction assembled against sm_86's machine file (936 for arithmetic, 1110 for division, 0 refused), and no lane run on the part. The same programs run as C source on the record tests ([engine_table.md](engine_table.md), item 11). | Each lane assembled into a cubin and run where its C route runs; both read word for word against the host; both timed (P6) and their instructions counted; every program where ours costs more read form by form against NVIDIA's listing. | **Measured** (`monolith_differences.md`, 98 of NVIDIA's instructions over 18 blocks): where the machine file holds a form, our reader gives NVIDIA's text and our assembler its operation bits on every instruction. 1 has operation bits apart and 85 scheduler bits apart. **Measured**, each field the disassembler hides put to the disassembler a bit at a time: a field whose value renames the operation (`IMAD`'s multiplier at bits 32 to 63, `.MOV` at 0 and `.IADD` at 1), one whose 0 drops the operand (`BPT.TRAP`'s code from bit 34), the descriptor register a `term[URn]` address does not name, and an operand a form holds and does not print, whose run a sibling one bit away prints (`IMAD`'s carry-in at 87 to 90, in 685 forms). **Measured** on the part (`interface_sass_unprinted.md`: every value of each field written into a question's instruction and run over one case, no disassembler asked): `IMAD`'s carry-in without `.X` and `ISETP`'s predicate at 68 to 71 without `.EX` answer as printed at all 16 values. The 1 instruction still apart, `IMAD.IADD` with P2 at 87 to 90 where NVIDIA writes !PT, is apart only at bits the part does not read there. The load's bits 64 to 67 are one predicate, its index inverted at 64 to 66 with 000 for PT and bit 67 negating it, and a load whose predicate is false writes 0. Our assembler writes a printed load predicate there inverted, and the part answers P1, P2, !P1, !P2 and !PT as written. The descriptor register of the load (32 to 37) and of the store (64 to 69) is read whether bit 101 is set or clear: an odd register or UR62 is refused as an illegal instruction, and every even one below answers as printed whatever it holds. Bits 38 and 39 of the load and 70 and 71 of the store answer as printed at every value. **Measured** on the part over every form holding an unprinted operand (`interface_sass_unprinted_forms.md`, each form written with its operands filled by their kinds, four copies to a run): of 437 forms, 368 are asked, and 375 of their 379 runs answer alike at every value asked, every `ISETP`, `IMAD`, `LEA`, `HFMA2`, `F2FP`, `UIMAD` and `ULEA` among them. A kernel refuses a register number past the count it declares: `F2FP`'s run at 64 to 72 is refused at bits 68 to 71 under the 10 the pattern declares and answers alike at every bit under 255. None of the 40 atomics holding an unprinted register at 64 to 71 runs at its own bits: 22 are refused as illegal instructions, and 18 take a 32-bit or shared address nothing in the question backs. Every one was reached by turning bits; NVIDIA's compiler writes an atomic only for an atomic on `.global` through a 64-bit pointer, `ATOMG.E.ADD.64.STRONG.GPU` for `atom.global.add.u64`, `ATOMG.E.CAS.64.STRONG.GPU` for `atom.global.cas.b64` and `RED.E.ADD.STRONG.GPU` for `red.global.add.u32`. On its 64-bit add, run at every value of bits 64 to 71: 64 to 69 are the descriptor register, an odd one or UR62 refused and every other answering as printed whatever it holds; bit 70 clear is refused as an illegal address, and bit 71 set beside it as an illegal instruction. The pair R100 and R101 named there answers as printed at 0, at 4 and at 1 in its high word: no register is read from those bits. **Measured** on the part over every form `sass.krs` uses (`interface_sass_fields.md`, each operation bit turned and run, no disassembler asked): of 75 forms, 1619 bits read inside a run the machine file records and 526 outside every run, and a turned bit whose opcode holds a control transfer, a wait or no form is not run (P9). Our compiler writes AND, OR and XOR bit for bit, and JCC's guard and operation; SHL and SHR one bit apart, NVIDIA's `.W` against a clamp that answers 0 for a count of 32 or more where the precept wraps it; ADD and SUB as `IADD3` where NVIDIA writes `IMAD.IADD`; and for NOT, NAND, NOR, ASR, ROL, ROR and BRA it has no word. **Measured**, cost (P6): NVIDIA writes NAND and NOR as two `LOP3`, 8.05 cycles a step, where one `LOP3` is the gate, and ROL and ROR with the source's guard in front of the one `SHF` the part needs. | **measured** (the monolith and its record), **built** (the 16 lanes), **theory** (the lanes beside their C route) | Fix from the root up, each fix read off the record (Open 2 in the plan): the machine file's missing forms, then `sass.krs`, then the word web. |
| **Q18. A register's width** | A register of the part holds w bits. A value of b bits past w is held in as many registers as it takes to cover b, its words in an order the part sets, and a value of fewer bits is held in the low bits of one. The width and the order are the part's own and differ from part to part, and the system classification routine asks both before any value is given a register. Over the bridge the width and the order are the part's `.kdm` entries, and a language's `.klm` keys a value to them and holds no width of its own. | Nothing asks them. The measuring stick's reader holds 32 bits to a register and a value of 64 bits in an even register and the one past it, its low word in the even register (`src/cu/scaffolding/measuring_stick_engine.cpp`). | Each asked on the run channel and held in the part's `.kdm`, by several questions that must agree: a register written 0xFF, 0xFFFF and 0xFFFFFFFF and one past each, read back for where the word is cut; 1 shifted left until it reads 0, the count of shifts the width; a sum carried past the top, read for where it comes back to 0; and a value of one bit, such as 0b1, put where a register is taken, which the part refuses and names the full width. | Nothing has run. | **theory** | Asked in the system classification routine on the run channel; the reader then takes the width and the order from the part's `.kdm`. |
| **Q19. Absurdity from a part of a set** | A relation among entries is witnessed where every set holding its entries holds it, and open where it is a relation of absence (P10). No set is known to be whole: a witnessed relation is a verdict on any part of a set holding its entries, and an open one never is. Sameness read off the part is a relation of absence over cases: one case apart witnesses two operations, and alike on every case asked leaves the two names open, as P2's survivors stand. A map from one language to another is read through `Lstar.klq`, and of its relations a name one text with another on one side and two on the other, and a name given as another kind, are witnessed, and a name the other language does not give is open. | `ruleset_core_relation.h` (`src/cu/types/file_defs/readers`) reads any files of rulesets, device maps and classifications together, each line an entry that names its relation on its own line, and keeps 18 kinds of absurd relation with the entries each names. `RULESET_CORE_RELATION_OPEN` marks the two of absence: a construct's line writing a name no entry gives, and a schema name no entry gives. `ruleset_read_test.cu` fails a set on a witnessed relation and writes an open one. `klq_identity.cu` (`src/cu/transpiler/lstar/protocol`, run by `klq_identity.sh`) reads nvcc's writing of the measuring stick: two questions apart in one link a side are a slice whose two links are its identity, the carriers of an identity are gathered by their context in their own chains, each chain and each window of it is an identity at the first address it occurs at, and a candidate chain is sifted through those identities before any of it is put to the part. | Every word a language's parts make, put together by their categories with no part's meaning known, read through the language's `.klm` into the bridge's keys and held against these relations first: a word that holds a witnessed one is thrown out and asked nothing. Each word left is asked one case at a time in the descent's order (Q4), of nvcc's writing of the measuring stick's question whose C the word is, with the host computing that C on the case, until the part is asked through the run channel (Q1). The first case apart closes a pair as two operations, and a pair alike on every case asked is kept with its count of cases (Q11). A test and the select that reads it are one word, a part's operands are parts of their own category, and a word no question compiles to stays gray. Each verdict is written to `Lstar.klq` at the pair's key, each case in the case form of `.gsm`, and each language's operations read off the verdicts to its `.klm`. A text the same as another's and a construct that is one form renamed are witnessed by the text and put to nothing. | **Measured** on the host (`ruleset_read_test.sh`, every target's files against its schema): sass 262 witnessed, ptx 65, c 25, cu 25, vhdl 9, yosys 0, and 0 open, every name a construct writes and every name a schema holds being given by a file its target reads. Files given on the command line are read the same way against no schema. **Measured**, the map read directly between two sets with no `.klm` written: cu's files to sass's hold 32 witnessed relations and 0 open, and sass's to cu's 32 and 0. **Measured** on the measuring stick (`measuring_stick.sh` and `measuring_stick_engine.sh`, 1006 kernels, no device): of the 44 open pairs in sass's files, 15 are two forms writing the same operations apart in their operands, and 29 write different operations. nvcc writes both forms of each of the 29 in a question of the stick, and the engine both of 15. **Measured** on the stick's 1006 questions (`klq_identity.sh`, no device): 2972 slices carry 1007 identities in 1113 contexts. The host computes the 325 integer questions over 648 cases each, 351 contexts close on a case apart, 762 are asked of nothing, and rule 2 keeps 4 links off the part. The 1006 chains hold 679 whole identities and 2029 primitives and read as 12492 constituents. nvcc's listing sifted through its own identities is known whole 1006 of 1006. Of the engine's 320 chains none is known whole: 63 of their links nvcc never writes, and 1832 of their pairs side by side nvcc never writes so. | **measured** (the relations on the host, the identities, contexts, chains and sift on the stick), **theory** (the questions put to the part) | Put the primitives together by their categories, sift each through the chain identities, and add those nvcc has never written to the stick as questions, each marked with the rule that keeps it off the part where one does. |

## The algebra of the protocol

Every line is exact. A cost is a reading and carries a spread; a relation is arithmetic and carries none.

Every line takes the entries of the bridge as its inputs (The inputs, above). An address is a key σ of `Lstar.klq`, M_L is language L's `.klm`, which gives each name of L its key, and K(σ) is the key's cases. A candidate is a form a language's map or the part's machine file gives for σ, and what a step finds is written back to σ.

### P1. The ask and its bound (Q1, Q2, Q3)

An ask a = (address, qualifier) returns, unbound, its cost t(a). Bound by β it returns one bit:

  bit(a) = [qualifier holds] · [t(a) ≤ β]

A missing address, a false qualifier, a timeout, an error and too many cycles all read 0. What tells them apart is the baseline, the spread of unbound costs, and the bound is read off it (Q2) and never written by hand. A run that never returns is apart from all of these, a reset and not a reading, and P9 holds it.

Over the bridge an ask is a = (σ, L, r): a key, the language whose map it is read through, and a relation r of the entries M_L gives at σ. A relation of the text carries no cost and no bound enters it. On a part E of a set of entries:

  read(a, E) = 1 where r is witnessed and r(E); 0 where r is a relation of absence and not r(E); gray otherwise

A relation the part answers, forms f and g on the cases K(σ), reads as P1 reads each answer, its kind with it, so that a refusal from one where the other answers is a case apart:

  read(a, K) = 1 where f(k) ≠ g(k) for some k ∈ K; gray where f(k) = g(k) for every k ∈ K, and |K| its reading

Neither ever reads 0: no answer of the part confirms two names one operation (P10).

### P2. The gate (Q4)

A relation r is held on cases K = {k_1, …, k_m}. A candidate c survives when

  c(k) = r(k) for every k ∈ K

- **The survivors are a conjunction.** S = {c : c(k) = r(k) for every k ∈ K} does not depend on the order of K, and no order of the cases changes which candidates survive.
- **The order is the price.** Putting K in order π costs Σ_c f_π(c) evaluations, f_π(c) the place of the first case c fails (m for a survivor). The descent puts first the case the most remaining candidates fail.
- **No cost enters S.** A candidate that fails a case is wrong on every part, and no part is asked about it.
- **Proved** (Q4): the descent's survivors equal `chain_build`'s on every relation the ladder holds.
- **Over the bridge.** With C(σ) the candidates of a key and r_σ its relation, S(σ) = {c ∈ C(σ) : c(k) = r_σ(k) for every k ∈ K(σ)}, written back to σ. The reader's gate over a set of files is the same conjunction: a set E is refused where R(E) for a witnessed absurd relation R, and is then refused in every set holding it. No order of the files or of their lines changes which sets are refused: no entry waits on another. A set not refused stands open, as a survivor of the gate does.

### P3. The order of asks (Q5)

For a chain of n links, n + 1 a power of two, ask r covers link c where (r + 1) & (c + 1) has an odd count of ones: the rows of a Hadamard matrix of order n + 1 with its first row and column dropped, read as 0 and 1. With b_r the cost ask r returns and x the per-link costs:

  (n + 1)·x = 4·Sᵀb − 2·(Σb)·1

- Every link is covered by (n + 1)/2 asks and any two links share (n + 1)/4, and that is the identity 4·SᵀS − (n + 1)·J = (n + 1)·I, with Σb = ((n + 1)/2)·Σx (**proved** here for n = 3, 7, 15, 31 and 63). The solve is integer adds and one division by a power of two (**proved**, Q5).
- Against one ask a link, the gain is √(n + 1)/2 (derived; **measured** 1.09, 2.07 and 8.16 at 3, 15 and 255 links against 1.00, 2.00 and 8.00, Q5).
- **Over the bridge.** The n links are keys σ_1, …, σ_n: a chain written in language L, each of its lines taken to its key by M_L. x_c is kept at σ_c, and two chains written in two languages compare at the keys they share (Q8).

### P4. Contention (Q7)

Each of N sweep asks covers a count u of links, and y is what is left of its cost once the known order's links are taken out. The model is y = α + γ·u + δ·u² + noise, and contention is δ > 0. With every sum centered and scaled by N,

  C_pq = N·Σpq − Σp·Σq

and the count taken out of the square s = u² and of the leftover y,

  P_ss = C_uu·C_ss − C_us²,  P_sy = C_uu·C_sy − C_us·C_uy,  P_yy = C_uu·C_yy − C_uy²

the square's slope P_sy/P_ss has squared spread (P_yy − P_sy²/P_ss)/((N − 3)·P_ss), with N − 3 left over past the three unknowns. The slope stands above twice its spread exactly when

  P_sy²·(N + 1) > 4·P_yy·P_ss

which is every term an exact integer and no division (`ask_links_read`, `src/cu/transpiler/lstar/protocol/ask_order.c`). Fewer than three counts covered leaves P_ss = 0 and nothing to read; P_sy ≤ 0 reads as adding.

Over the bridge the u links an ask covers are keys, and a contention read is kept with the keys of the asks it was read over.

### P5. One ask, many answers (Q15)

A set of checks each failing with share p is asked as one bit, 1 where every check holds and the set halved on a 0. Past p = (3 − √5)/2, about 0.382, asking one check at a time costs no more than any scheme that asks a set as one bit (cited from knowledge: P. Ungar, "The cutoff point for group testing", Comm. Pure Appl. Math. 13 (1960) 49–54). The channel's answer of 128 bits carries one bit a check, which pays on every relation (**measured**, Q15).

Over the bridge a check is a pair (f, g) at a key and one case k, and its bit is [f(k) ≠ g(k)]. A pair closed by k is closed on every K′ holding k (P10) and is not asked again: the checks still put at a key are its open pairs, each against the cases not yet asked of it.

### P6. The rank, read against its own spread (Q16, Q17)

Each survivor c is run T times and its runs t_c,1, …, t_c,T taken. A run is lengthened by whatever else the host does and never shortened by it:

  m_c = min_j t_c,j,  s_c = max_j t_c,j − m_c,  s = max_c s_c

With the survivors ordered by m, c_1 the least and c_2 the next, c_1 is the part's choice where

  m_c2 − m_c1 > s

and otherwise the survivors are one cost and the ruleset keeps its form. The gate (P2) decides which candidates hold and this rule orders the survivors, and the two are never added together.

Over the bridge the survivors are S(σ), and the rank is kept at σ with the part and the size it was read on (Q10): the language's `.klm` gives σ the form c_1 where m_c2 − m_c1 > s, and otherwise keeps its form.

- **Measured** (Q16): 8 survivors in `loop_back_if`'s place on sm_86, m_c2 − m_c1 = 0.0525 ns a turn against s = 0.9260. The least of one run, `BRA.CONV`, is not the least of the next, `BRA` with a number, as a difference inside the spread gives.
- **Measured** (Q2, the monolith): 15 precepts alone, a million steps a chain, 16 runs. The eleven single-instruction precepts sit within 0.006 cycles of each other in m against s = 2.22, one cost. The two deltas that stand are 0 to 4.05, no instruction against one, and 4.06 to 8.05, one against two. `monolith_run` reads a delta inside s as dual or gray and one past it as lead (gnascor).

### P7. A form asked of the part (Q16)

The slot is `loop_back_if loop where`. The question is a body the slot closes:

  R7 ← 0, R0 ← N;  turn: R7 ← R7 + 1, R0 ← R0 − 1, P0 ← [R0 ≠ 0];  then the candidate, guarded by P0

- A candidate that takes the way back on P0 runs N turns and answers R7 = N.
- A candidate that falls through runs one turn and answers 1.
- N = 2 separates the two, 2 against 1. The counts after it, 1, 3, 5 and 64, hold the answer to N: a form that came back a fixed number of times answers that number and not N.
- The candidates are every form in the part's machine file, with every register made R8 and every uniform one UR8, which the body does not keep, a predicate made P0 (PT, RZ and URZ kept), a label the body's and a number the distance back to it, 0x40 bytes over four instructions. Nothing decides beforehand which forms jump.
- A survivor is walked: `sass_loop_walk` reads its encoding back through the forms of the machine file and checks it as a loop's way back, the flag, the distance, and every register the body keeps left as it was.
- **Proved** (Q16): 8 of the 2927 forms answer every count, every one a `BRA`, and the walk agrees with all 8. No form answers N = 2 without answering every count.
- **Over the bridge** the slot is the key `loop_back_if`, its cases the counts, and the 8 forms that answer every count are S(σ), kept at it.

### P8. Held against NVIDIA's compiler (Q17)

For a program p, C(p) is its text through `c.krs` and E(p) its lane through `sass.krs`. V(p) is what NVIDIA's compiler writes for C(p) and h(p) what the host oracle answers.

  E(p)(k) = V(p)(k) = h(p)(k) on every lane k

is the gate, and the rank is P6 between E(p) and V(p), with |E(p)| and |V(p)| their instruction counts as a second reading. E(p) holds NVIDIA's line where m_E(p) − m_V(p) ≤ s.

- Where E(p) costs more, V(p)'s instructions over that part name forms, each in a slot. Each slot is a question for Q16: the forms of the machine file in that slot, the cases of the slot, and the survivors ranked by P6. NVIDIA's listing names the question and the part answers it.
- The loop closes when E(p) holds NVIDIA's line on every program p, and every form in every ruleset was answered by the part.
- **Over the bridge** E(p) is read through `Lstar.klq`: each line of p, written in cu's names, is taken to its key by M_cu and from the key to sass's names by M_sass, E(p) = M_sass⁻¹ ∘ M_cu applied to p. A line whose key the map breaks, collapses or gives as another kind cannot be read through as it stands, and those are the map's witnessed relations (P10). A key sass gives no name for is open, and E(p) is gray at that line and not refused. **Measured** (Q19), the map read directly between cu's set and sass's with no `.klm` written: 32 witnessed relations each way, 0 open.
- **Built:** E(p) for the 8 record programs the host oracle runs, 16 lane programs, every instruction assembled against sm_86's machine file (Q17). **Measured:** V(p) for the monolith's 18 blocks, one base precept each, held against our reader, our assembler and E(p) instruction by instruction (`monolith_differences.md`, Q17). **Theory:** the rest.

### P9. The part's own bound (Q1, Q16, Q17)

P1 counts a timeout among the readings of 0, which holds only where the run returns. A run that never returns is a reading apart: the part's display watchdog ends it with a reset, and across a run of asks that reset can hold a kernel's own watchdog past its bound and stop the host with a DPC_WATCHDOG_VIOLATION.

A body runs without end only where control reaches a line it has passed and the way back never closes. Three contexts reach it:

- A turned bit makes the instruction a branch to itself or a line before it. An isolated instruction turned this way is most often an illegal encoding the part refuses as a clean 0; a turned branch is not refused.
- The body holds a read it waits on, a register or a word read again until it carries a mark, and the turned bit closes the wait: the register the read names, the value it is held against, or the step that moves it. loop_back_if (P7) is such a body, its way back taken on P0 = [R0 ≠ 0]; a bit that stalls the step or frees the branch leaves it turning without end.
- A barrier every lane is waited at, where a turned bit sends a lane down a path that never reaches it, and the lanes that waited do not return.
- The scheduler word waits on a scoreboard barrier a late result sets and that result never clears it. Above the operation the encoding holds a stall, a write and a read barrier and a wait over six barriers, none of it printed. A result ready after a fixed count carries that count as its stall; a result that comes back late, a load or a memory read, sets a barrier instead, and whatever reads the register it writes waits on that barrier. A barrier no instruction sets is at rest and a wait on it resolves at once; a wait on a barrier whose setter a turned bit sends a lane past never resolves, and the lane does not return.

The local absurdity, an isolated instruction turned illegal, the part refuses and the ask reads. The waited read is where a turned bit writes a lawful instruction that breaks the wait, and a lawful instruction runs where an illegal one refuses. An ask that holds a waited read carries its own bound in the body, a turn count past which no turned bit runs, and keeps that count in registers the turned bits do not reach: loop_back_if holds its candidate off the counter R0, every register in the candidate made R8. The host's patience and the display watchdog are a reset, never the bound.

An ask that turns an instruction's bits is checked before it is put. The opcode, the low word's bits 0 to 11, names the operation, and a turned instruction is put to the part only where its key holds forms in the machine file and no control transfer or wait among them. Such an instruction falls through whatever its other bits hold. A turn past the key leaves the operation as it was. Every other turn is marked skipped and run on nothing, which closes the first context above for every key the machine file holds (`interface_sass_probe_fields.c`, Q17).

The scheduler word is set once for a run and no turned bit reaches it. The assembler writes every instruction to wait on all six barriers and to stall the soonest its operation's result is read, the longest where the krs measures no count, and to set a write barrier where the operation's result comes back late and a read barrier where it is a store (`sass_control_safe`). `cubin_safe` holds every instruction it reads to that stall. Which those are is the operation's schedule, a property of the operation the krs holds beside the scheduler's fields (`sass_operation_schedule`, `sass_machine`), and not of the one encoding the machine file first saw it with: that encoding sets no write barrier on `LDS` and no read barrier on any store. A turned bit lies at 0 to 104 and leaves 105 and up as they were: the safe word stands on every flip, and no flip opens a path around a barrier's setter. This closes the scheduler context for a flip probe, where the body is straight-line and every barrier set is cleared by the instruction that set it.

- **Measured** over the tree's CUDA sources built by NVIDIA's compiler for sm_86 (`monolith_scheduler.md`, 72 cubins, 96976 instructions, each named by our reader and its bits 105 to 127 read through the krs's fields, no listing read): NVIDIA gives a write barrier at every place to `LDG`, `LDS.64`, `LDS.128`, `S2R`, `S2UR`, `F2I`, `I2F` and `ATOMG`, and at most places to `LDS`, `MUFU.RCP` and `F2I.FTZ`; to 94 other operations at none. A barrier counts its producers, and a wait on it holds until every one of them is back: of 1716 first reads of a late result with its own barrier, a wait on that barrier stands before all 1716, at the reader or at an instruction before it. A late result NVIDIA gives no barrier is held behind another of the same operation out of the same unit: 20 first reads, 16 of `LDS` and 4 of `MUFU.RCP`, stand behind a wait on a later one's barrier, the results coming back in the order they were put; 6 of `MUFU.RCP` stand behind a wait on an earlier one's barrier and at least 16 cycles past it. No first read of a late result stands behind no wait. Every `STG`, `STL`, `ST`, `STS` and `RED` takes a read barrier at most or every place. A fixed result is first read no sooner than 4 cycles on all 21 integer operations timed over 67443 reads, 5 on `IMAD.U32`, `IMAD.WIDE` and `IMAD.WIDE.U32.X`, and 6 on `CS2R`. The safe word waits on all six barriers at every instruction, which holds every one of these. Six barriers on every part from SM75 on, each a count its producers raise and a wait drains, and loads that keep their order sharing one barrier, are as the published reverse engineering of the scheduling word gives them (cited from knowledge: the ptxas execution model at gh.evko.io/crucible-notes, and the control codes of the maxas assembler, github.com/NervanaSystems/maxas/wiki/Control-Codes).
- **Measured** on the part: a barrier set by an operation whose result is back in a measured count is never released, and a wait on it does not return. `sass_control_safe` sets none on such an operation, and `cubin_safe` refuses one that does (its rule 5). A pair `IMAD.WIDE.U32` writes, read as a load's address four cycles on, is read before its high register is back, and the load ends on an illegal address. NVIDIA stalls six there, and the krs holds six for `IMAD.WIDE`, `IMAD.WIDE.U32` and `IMAD.WIDE.U32.X`.

### P10. Absurdity from a part of a set (Q19)

A set of files E holds entries, and a relation R over entries holds of E or does not. R is witnessed where

  R(E) ⇒ R(E′) for every E′ ⊇ E

and it is then a verdict on any part of a set that holds the entries it names. A relation of absence, that no entry of E gives the name x, holds of E and of no E′ holding a file that gives x. Read on a part of a set it is open: no read of a part decides it, and no set is known to be whole.

- **Witnessed:** a name given twice, the same text under two names, a construct that is one form renamed, names one word apart with texts the same but for their modifiers, a form opening another's literal operands, a construct that reaches itself, a parameter never written, a place no parameter takes, a form with no text, a register's numeric name where a fixed register gives it a name, a row given twice, a name the schema does not name, and a form taking another count of parameters than the schema says. Each is witnessed by the one or two entries it names. A classification's counts and classes are held within the file they are written in, which is read whole, and that file witnesses them.
- **Open:** a construct's line writing a name no entry gives, and a name of the schema no entry gives.
- **Asked of the part:** forms f and g put on cases K are two operations where f(k) ≠ g(k) for some k ∈ K, and that k witnesses it on every K′ ⊇ K. Where f(k) = g(k) for every k ∈ K, their sameness is a relation of absence over the cases, open on every K short of every case, and |K| is its reading, as the survivor count is (Q11). The text witnesses sameness and the part witnesses difference: no answer of the part confirms two names one operation, and no difference of texts confirms two operations.
- **The reading of a part:** a witnessed R reads 1 where R(E) and gray where not; a relation of absence reads 0 where the name is given and gray where it is not. A set known whole would leave no gray, and no set is known whole.
- **Maps:** the map from language L to L′ holds, for each name L gives, whether it is one text with another name in L and two in L′ (breaks), two texts in L and one in L′ (collapses), or given in L′ as another kind of form, nop, err or construct (kind). Each is witnessed by the entries it names. A name L′ does not give is open (absent). **Measured** (Q19): 32 witnessed and 0 open each way between cu's set and sass's.
- **The bridge:** each language is mapped once, to `Lstar.klq`, and a map between two languages is read through it, M_L′⁻¹ ∘ M_L: n languages take n maps, where read pairwise they take n·(n − 1). **Measured** (`klq_write.sh`, `cu.klm` and `sass.klm` with 151 keys each): every name of the 41 the map read directly between cu's set and sass's names is found on the two maps through the bridge, each way. Through the bridge 6 names more each way are one text with another on one side and two on the other, where the other side gives one of the two as another kind, which the direct map reads as kind alone.

### P11. The ruleset, derived ask by ask (Q4, Q11, Q18, Q19)

A ruleset gives each slot σ one form, and each fact of the part one number. Both are derived by putting relations to the part whose right side the host computes, one case at a time, each answer read as P1 reads it. No value is written by hand: a number enters as the answer of the host to a case or as the part's answer to an ask, and every walk over a number runs until the part says no.

**The ask.** A form f is put on a case k beside the answer of the host, h(k):

  `f(k) = h(k) ?`

The part gives one of four answers. Alike, where f(k) = h(k). Apart, where it gives another word. Refused, where it ends the run as an illegal instruction, an illegal address or a launch out of resources. Nothing, where no answer comes back. Alike is truthy and the other three are falsy.

A case on which the C of the host traps or is undefined carries no h(k): a divisor of 0, the least value over −1, and a shift by the width or past it. The part's answer to such a case is read and kept, and gates nothing (`refused_<n>` in the host program `klq_identity` writes). Each is judged in the operand's own type: a `signed char` divisor put 0x100 is a divisor of 0.

**What the host computes.** Every integer question a slice holds, every task of register pressure, and every question the engine writes a chain for: each chain the part is asked is held to h on every case.

**The cases.** Each operand is put through 18 values: 0, 1, 2, 3, and the edges of every width, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000 and the three edges of 64 bits. With a third operand of two values, a two-operand form meets 18 · 18 · 2 = 648 cases.

The steps run in order, and each takes what the steps before it found.

**Step 1. The width (Q18).** Before any value is given a register:

  `read(put(0xFF)) = 0xFF ?`  `read(put(0xFFFF)) = 0xFFFF ?`  `read(put(0xFFFFFFFF)) = 0xFFFFFFFF ?`  `read(put(0x1FFFFFFFF)) = 0x1FFFFFFFF ?`

The width w is the widest put that comes back whole, and the next one comes back cut at w. Once add stands (Step 2), two more asks must agree with it: x doubled from 1, `x ← x + x`, reads 0 at the w-th doubling and at no doubling before it, and `0xFFFFFFFF + 1 = 0 ?` is alike where w = 32. Where the three do not agree, nothing past this step is asked. **Theory.**

**Step 2. Add, by the descent (P2).** The candidates are every form the machine file holds in a two-operand slot, operands filled by their kinds. The descent puts first the case the most candidates fail. Five candidates and add, on four cases:

| case | h = add | or | xor | and | take | mul |
|---|---|---|---|---|---|---|
| `1, 1 → 2 ?` | 2 | 1 | 0 | 1 | 0 | 1 |
| `0, 1 → 1 ?` | 1 | 1 | 1 | 0 | 0xFFFFFFFF | 0 |
| `1, 0 → 1 ?` | 1 | 1 | 1 | 0 | 1 | 0 |
| `0xFFFFFFFF, 1 → 0 ?` | 0 | 0xFFFFFFFF | 0xFFFFFFFE | 1 | 0xFFFFFFFE | 0xFFFFFFFF |

- `1, 1 → 2 ?` fails all five, and it is placed first: the carry out of the low bit is what add holds and no bitwise form holds.
- `0, 1 → 1 ?` and `1, 0 → 1 ?` fail a form that drops an operand.
- `0xFFFFFFFF, 1 → 0 ?` fails a form that saturates at the top, or carries into a 33rd bit.
- On the ladder's candidates the descent places one or two cases a relation, and every impostor fails at the first or second (**proved**, Q4). The survivors are S(add), and their count is a reading (Q11).
- At 2w a value is two words, and a form of 2w is a pair of forms of w. `0xFFFFFFFF, 1 → 0x100000000 ?` fails a pair whose two words are added apart (0): the high word takes the carry out of the low.

**Step 3. Take.** `1, 1 → 0 ?`, `0, 1 → 0xFFFFFFFF ?`, `1, 0 → 1 ?`.

- `1, 1 → 0 ?` fails add (2) and or (1); xor answers 0 and stands.
- `0, 1 → 0xFFFFFFFF ?` fails xor and or (1), a form that takes the operands swapped (1), and one that saturates at 0 (0).
- f(0, 1) ≠ f(1, 0) witnesses that the form does not commute, which add and every bitwise form do.
- At 2w, `0x100000000, 1 → 0xFFFFFFFF ?` fails a pair whose two words are taken apart (0x1FFFFFFFF): the high word takes the borrow out of the low.
- −a is 0 − a in a's type, put to take as `0, a`, and no form of its own is asked for it.

**Step 4. Divide.** `0, 1 → 0 ?`, `1, 1 → 1 ?`, `3, 2 → 1 ?`, `0xFFFFFFFF, 2 → ?`, `1, 0 → ?`.

- `3, 2 → 1 ?` fails a form that rounds (2).
- `0xFFFFFFFF, 2` is 0 signed, −1 over 2 cut toward 0, and 0x7FFFFFFF unsigned; a form that rounds down gives 0xFFFFFFFF. One case separates the three.
- `1, 0` and `0x80000000, 0xFFFFFFFF` signed carry no h(k). The part's word on each is kept at the key with the form that gave it. Two forms that give two words there are two operations (P10), and since C leaves the case undefined, either holds as the slot's form.

**Step 5. Shift.** `1, 31 → 0x80000000 ?`, `0x80000000, 31 → 1 ?` right, `1, 32 → ?`.

- `0x80000000, 31` right is 1 unsigned and 0xFFFFFFFF signed, which separates the logical shift from the arithmetic one.
- `1, 32` carries no h(k). A form that clamps answers 0, and a form that wraps answers 1. The case keeps the two apart as operations without gating either (Q17: NVIDIA's `.W` wraps, where the precept wraps too).
- Every case with an h(k) has a count under w, and its low word holds the count. A count held in two words is put as its low word, and every case with an h(k) answers the same.
- At 2w, `0x100000000, 1 → 0x80000000 ?` right fails a pair whose two words are shifted apart (0): the low word takes the bits the high word lets go.

**Step 6. Compare, and the select that reads it.** `0, 1 → 1 ?`, `1, 0 → 0 ?`, `1, 1 → 0 ?`, `0xFFFFFFFF, 0 → ?` for less-than.

- `1, 1 → 0 ?` fails less-or-equal (1).
- `0xFFFFFFFF, 0` is 1 signed and 0 unsigned.
- A test and the select that reads it are one word, asked as one (Q19).
- A bool is a relation and no narrowing: `(bool)v` is [v ≠ 0] over the whole of v. `0x100000000 → 1 ?` fails a flag of the low word alone (0), which a byte or a halfword is right to read.
- A flag read as a word and tested against 0 is the flag: [select(p, 1, 0) ≠ 0] = p on every case. The round trip is a cost to rank (P6) and never a gate, and a reader that does not hold it reads each conditional nested in another again for every reading of the one around it.

**Measured**, Steps 2 to 6 on sm_86: the engine's chain for every question the host computes, 441 of them, answers alike on all 648 cases and none apart. The 64-bit add, take, bitwise forms, not, negation and both shifts right are among them, each in the two words of NVIDIA's listing, every comparison glued to a second test by `&&` or `||`, and every select over a bool.

**Step 7. Two forms, one slot (P10).** Forms f and g in one slot are put on K. The first k with f(k) ≠ g(k) closes the pair as two operations, and k is kept at the key as its witness. A pair alike on every k ∈ K stays gray, with |K| its reading. A case closes a pair once, and a closed pair is not asked again (P5).

**Step 8. The bound in place of a counterexample.** An L* learner asks whether a conjecture is the machine and is given a case apart when it is not. The part gives no such case: K stands in for it, as a test suite does ([learning_a_ruleset.md](../../thought_experiments/engine/learning_a_ruleset.md)). An operation on words holds no state, and so the bound is on cases and not on states: a form is exact on K and gray past it. A two-operand form of 32 bits has 2^64 cases, and a form apart on a case outside K is found only by adding that case to K. **Measured** at 16 bits: 55 forms on all 65,536 cases against the integers of the host ([learning_a_ruleset.md](../../thought_experiments/engine/learning_a_ruleset.md), Status).

**Where the questions are bounded.** Each step asks only what the step before it can hold, and each bound is a place the ruleset is gray. Read off the stick's 1016 questions:

- **The cases.** An operand meets 18 values and no other. Between 0x100000000 and 0x7FFFFFFFFFFFFFFF none is put, and a form apart only there is found by no case (Step 8).
- **The C of the host.** A case it traps on or leaves undefined carries no h(k), and the part's answer there is held to nothing (Steps 4 and 5).
- **The types of the host.** The host computes integers alone: 436 questions, and float and double, 398 of the 1016, are asked of nothing. No form of a floating slot is derived.
- **The reading of the engine.** A question the engine cannot write through cu.krs is asked of nothing: 166 besides the floating ones. 35 have no reading, 33 of them a quotient or a remainder, which sm_86's ruleset gives no form for (`err word_div`). 66 are calls or elements cu.krs gives no form for, 30 atomics and 34 statements.
- **The run channel.** A question carries as many cases as a launch gives threads, up to the 2^20 the host fills, and the run tool gives every case a thread. Every ask of the protocol puts all 648 cases the host computes: the register walk holds each probe to every one of them and reads the last register R252 over 35 asks, 9 alike, 20 refused and 6 held by the gate (`KLQ_TRACE` writes each ask and its answer).
- **The slices.** A context no two questions of the host hold is asked of nothing: 762 of 1113.
- **Two words.** A 64-bit form is put as two forms of 32 bits as NVIDIA's listing writes it, and the width that would ask it is Step 1, theory.
- **The walks.** The register walk starts below the top of the field, 255, and the threads stop at the 2^20 the host fills cases for (Step 9).

**Step 9. The part's numbers, walked until it says no.** These relations are over a number n and not a word. Each holds below a bound and is refused past it:

  holds(n) ⇒ holds(m) for every m < n

and the walk reads one bound, the greatest n that holds.

- **The last register.** `holds(R_n) ?`: a chain register written and read back, renamed R_n, in a kernel declaring 255, and the control renames it to the least number nothing uses. The walk goes down from the top of the field below RZ, the name the field's top gives the register that reads 0. n = 254 and 253 are refused as illegal instructions, and 252 is alike: last = 252. **Measured** on the RTX 3070, sm_86 (`klq_identity register`, kept in `sm_86.ksc` as `run answers 000000fc register last`).
- **The launch.** `run(r, t) ?`: t threads a block, doubled from 1, with r registers declared. The part refuses as out of resources where r · t > 65536 in a block. That bound is the part's answer and is written nowhere. The host bounds only the threads it fills cases for, 2^20 in all (`CUBIN_RUN_THREADS_MOST`, `cubin_run.c`). **Measured.**
- **The fewest registers.** `alike(T, r) ?`: task T run with r registers declared. r is walked up from the count of registers the chain names until alike, and halved down until not, at one thread a block. A container declaring d gives code R0 to R(d − 3), as the last register gives at d = 255. **Measured** on sm_86: chains naming up to R21, R69 and R197 are refused at 22, 70 and 198 declared, and answer at 24, 72 and 200. A chain naming up to R13 answers at 8 and at 1: under some count the part gives a container more than it declares.

**Step 10. The cost of a task, by the bounced sustain.** c(r, t) is task T's cost on the part's clock, read over 100 launches, at r registers and t threads. The ask is

  `c(r, t) ≤ hi ?`

against a band B = [lo, hi] read at the fewest registers:

- An ask inside B sustains it. B holds after 5 asks in a row land inside it without widening it.
- An ask past B is falsy only where it lands past B twice with the baseline asked between them landing inside B. Where the baseline strays, B widens to hold it and the count starts again.
- knee(t) = the least r with c(r, t) past B, found by bisection over r, which reads one bound where c(·, t) past B holds of every r above it.
- Each t is walked, doubled from 1 until the launch is refused (Step 9), and the curve of a task is its knees against t. **Measured** on task 1006 at one thread: 1.80 ms up to r = 128, 2.11 ms at r = 129, 3.04 ms at r = 192.

**Step 11. The ruleset.** For each slot σ the survivors S(σ) of Steps 2 to 7 are ranked by P6, and the slot is given c_1 where m_c2 − m_c1 > s and keeps its form where not. Steps 1, 9 and 10 give the part's numbers: the width, the last register, the bound on a launch, and each task's knees. Each is kept with the part and the size it was read on (Q10), in the part's `.ksc`.

### P12. Categories, and the coherence set C* (P10, P11)

The ruleset L* of one known language is finite: the language holds finitely many forms, and P11 decides each on finitely many cases. The set of all of them is not finite:

  L_all = {C*, L_1, L_2, …}

C* is our coherence. It is made of the categories we understand, each a pool of questions split from the others by what its questions ask. Our coherence has limits, and C* runs past them toward the edge of human coherence. Where we have no category, the system answers for itself.

**A category.** A category c is a pool of questions Q_c with its own cases K_c, its own answerer h_c, and its own bounds. h_c is the host where the host computes c, and nothing where it does not. The pools split the questions, each question in one pool:

  Q = ⋃_c Q_c,  Q_c ∩ Q_d = ∅ for c ≠ d

Each pool is learned as L* learns a language (P11): an ask a case, and K_c in place of a counterexample.

**Open-ended.** No set of categories is known whole (P10). A category added is a pool asked as the others are, and a question no category holds is kept gray, never thrown out. The flow keeps the unknown.

**Relational: never wrong, never whole.** Every question at its root is a relation over forms and cases. A witnessed answer holds on every set holding the entries it names (P10). A category, a case or a language added never undoes it, and a concept read so is never wrong. A relation of absence stays gray on every part, and no reading is whole, ever.

**The language bounds itself to the answerer's rules.** A form enters a ruleset only where the part answers it (P7, P8). What the part refuses, what it never answers and what no category asks stay gray. The ruleset is the answerer's rules as far as the questions reach.

**We fill part of C*.** The filled part is every category with an answerer and a reading. We build to fill more: each form the part answers is a form to build the next question with (P8).

**Measured**, the stick's categories on sm_86. Each category is a pool. A question is written where the engine reads it through cu.krs, held where the host computes it, and alike where the part answers it as the host does on all 648 cases:

| category | questions | written | held | alike |
|---|---|---|---|---|
| operator | 171 | 131 | 131 | 131 |
| conversion | 110 | 72 | 72 | 72 |
| test | 90 | 90 | 90 | 90 |
| compound | 88 | 64 | 64 | 64 |
| single math | 87 | 0 | 0 | 0 |
| double math | 86 | 0 | 0 | 0 |
| unary | 78 | 64 | 64 | 64 |
| casting intrinsic | 71 | 0 | 0 | 0 |
| single intrinsic | 41 | 0 | 0 | 0 |
| integer intrinsic | 37 | 0 | 0 | 0 |
| atomic | 33 | 0 | 0 | 0 |
| double intrinsic | 28 | 0 | 0 | 0 |
| statement | 19 | 1 | 1 | 1 |
| warp | 17 | 0 | 0 | 0 |
| memory | 13 | 0 | 0 | 0 |
| built-in | 13 | 11 | 0 | 0 |
| conditional | 11 | 9 | 9 | 9 |
| pressure | 10 | 10 | 10 | 10 |
| sync | 9 | 0 | 0 | 0 |
| call | 4 | 0 | 0 | 0 |
| all | 1016 | 452 | 441 | 441 |

Every bound of P11 is a row of this table. A category with no answerer, the floating ones and the intrinsics, holds nothing. built-in is written and not held: the host computes no built-in. None is apart. Every question held answers without error, and every other question stays gray.

**A floating category's answerer.** The host gives a floating question no single answer, only what exact arithmetic gives. For two operands of width w, the exact value v of the operation is computed on exact numbers, and its two neighbors of width w bracket it, down(k) ≤ v ≤ up(k). The ask is a relation over the bracket:

  `f(k) ∈ {down(k), up(k)} ?`

- A part's answer outside the bracket is apart, a wrong operation on every part.
- Inside it, which end the part takes is the part's own rule. Read across the cases, the ends it takes give its rounding, to the nearer with ties to even or toward 0, and no rounding is written by hand.
- A v the bracket holds exactly, down(k) = up(k), is a case with one answer and gates as an integer case does.
- The exact double is a mantissa and an exponent, m · 2^e, each lane its own exponent. Its sum, difference and product are exact, and its cut to width w keeps both ends (`edouble_record`, `src/cu/types/integerfloats/edouble`).
- **Theory.** The floating categories hold nothing until the stick's floating questions are put this way.

### P13. The sets, open-ended, and what they collapse (P2, P5, P10, P11, P12)

Every set below is open. No set is known whole (P10), and an entry added is asked as the others are.

- **The categories.** C* = ⋃_c c, and a category is a pair c = (asked, answerer). The asked side is gnascor's thing and doing words, the tables of [gnascor.md](../../../src/cu/transpiler/gnascor.md), and the sets of our coherence below, which those tables file among their thing, doing and gluing words. Each such set is named `<n>_coherence`, its stem what it holds. The answerer side is one of the kinds below. Each category has a clear operational line, and where two categories blur, the blur is another category.
- **The tree.** The sets are nodes of one tree of linked nodes. Each node is one equality test, and each set is the node of everything beneath it, a set of sets where it holds others. A node with more than two members is a binary tree in the first-child, next-sibling form: the first link goes down into the set, and the next goes to the member beside it. A sibling link between two members is their negation. A fact one node holds and several parents read, the frame, is one node linked from each, and its answer is written there once. A fact of more than one part is a set of its own, each part a member: a set that holds two kinds of thing is a clump, and it splits into a set for each. A set needs no name: its relation is its identity. `concept_coherence` holds the concepts, each a product: whether the two forms write one value or two on each case, as a function of the values the form reads there, alike, apart or both at each combination of them, written beneath the pair as `concept_coherence <hash>` (`concept_product.h`). The values are the part's own, read at the form: the carrier is cut just before the form for each value it reads, and just after it for each form's product, and the value stored in place of its answer. A concept cares for nothing that made it. The links of a carrier before the form and after it are deltas of their own, and a product read through any carrier is the same where the values are those the cases are made of. A product is a concept once it is whole, a row at every combination of those values, a register and its `.hi` one wide value: a part of a product is not yet any one concept, and the query puts the pair on past its first case apart until the product is whole. Two pairs of one product are one concept whatever names them and whatever language wrote them, and a word, where one is given, is a key on the identity as a language's `.klm` keys its names to the bridge. `signed_word_shr` is one such: shifting a signed word right is a concept, whatever chain holds it. The sets are our coherence, and a language is read through them by its relational map to them alone. `unknown_coherence` ends every set: an entry no answer has read further down stays there, a member of every set beneath. Read on this tree, the 25 pairs one text but for a modifier are 15 edges of the frame, 6 edges across the line from an equality to an order, 2 edges within order, swap and strict against inclusive, and 2 duals. The decoder reads all 15 into `frame_coherence`: two readings of the same bits under two frames agree wherever every value the link reads fits every frame, below the top of a byte read signed, and a pair alike on every such case and apart past them is the frame's. The values are those the part holds at the link, and a put with no read of them is read into no set.

```
equality_coherence                every ask: f = g over K
├─ comparison_coherence
│  ├─ equality_coherence          eq, ne, zero, nonzero: no frame
│  │  └─ unknown_coherence
│  ├─ order_coherence             lt, le, gt, ge, negative: a frame
│  │  ├─ strict, inclusive        lt and le, gt and ge: apart at a = b alone
│  │  ├─ operand swap             lt(a, b) = gt(b, a)
│  │  └─ unknown_coherence
│  └─ unknown_coherence
├─ frame_coherence                one node, linked from thing words, modifiers and orders
│  ├─ sign                        signed_, .S8 and .U8, SHF.R.S32 and .U32, ISETP .U32
│  ├─ width                       byte, halfword, word, wide, .64, .128
│  └─ unknown_coherence
├─ qualifier_coherence
│  ├─ memory promise              .CONSTANT
│  ├─ flag join                   _and, _or
│  ├─ guard                       if, unless: linked to the control's condition
│  ├─ exclusion                   atomic
│  ├─ scope                       STRONG, GPU, SYS, CTA, SM
│  ├─ cache                       EF, EL, LU, NA, LTC64B, LTC128B
│  └─ unknown_coherence
├─ modifier_coherence
│  ├─ dual                        and, or
│  └─ unknown_coherence
├─ negation_coherence
│  ├─ bitwise                     not
│  ├─ arithmetic                  neg
│  ├─ predicate                   !, unless
│  └─ unknown_coherence
├─ range_coherence
│  ├─ halves                      low, high
│  ├─ chain position              first, middle, last
│  ├─ sides                       left, right
│  └─ unknown_coherence
├─ vector_coherence
│  ├─ level                       thread, block, grid, warp
│  ├─ position, extent            idx, dim
│  ├─ axis                        x, y, z
│  └─ unknown_coherence
├─ literal, run-time value        a node with no set
│  ├─ text literal                word_copy with 8
│  ├─ launch constant             warp_size
│  └─ unknown_coherence
├─ timing_coherence
│  ├─ stall                       the soonest a result is read, a barrier and a wait, .reuse
│  ├─ operation                   each form's time, the curve's knees (Step 10)
│  ├─ vector                      the delta of a register, a lane, an operand of a case
│  └─ unknown_coherence
├─ control_coherence
│  ├─ transfer                    exit, return, a branch
│  ├─ loop                        the way back to a loop, the loop's label
│  ├─ condition                   if, unless: one node, linked from the qualifiers' guard
│  ├─ target                      label
│  ├─ dispatch                    a state's next, state_dispatch, state_next
│  └─ unknown_coherence
├─ switch_coherence
│  ├─ structure                   open, body, close
│  ├─ declaration                 declare_*
│  ├─ note                        note
│  └─ unknown_coherence
└─ unknown_coherence
```

- **When a language is read.** A language is read through our coherence where its program builds: written from the rules the query has read, it answers alike with the host on every case. `unknown_coherence` empty at every node the program's path passes is the same condition. Where the program does not build, either the question was malformed, the part refusing the ask as it was written, or an entry the program needs is still in `unknown_coherence`, and the query runs another cycle. L* stays open: no set is known whole (P10), a language added brings its entries in at `unknown_coherence`, and a program built closes the path it reads and no other.
- **Every cycle ends.** A cycle ends in one of three ways, and each is bounded. An entry moves down a node, and a path of the tree is finite. A question comes back malformed, refused, nothing or held, and its path stops with the part's reason. A built program answers apart at a case, and that case moves an entry: R holds every ask, and no ask is put twice. The cases K_c grow to test our coherence, each added by the descent (Collapse 4), and a run grows only by the cases added. Where a program does not build, the query says which of the three stopped its path: the question to put otherwise, the node still in `unknown_coherence` to ask next, or the case and the link the program answered apart at.
- **Folding.** `folding_coherence` is internal: our coherence held against itself. Where the tests grow past what the cycles settle, the tree is read for tests that answer one another, and each is factored out and asked once. One ask at a node answers both members a sibling link joins, its negation the other. `lt` answers `gt` across the swap. The frame is asked once at its node, and every parent linking it reads that answer. `zero` is `eq` at 0. What folds is the testing across nodes, and no entry and no answer is lost to it.
- **The qualifiers.** `qualifier_coherence`, a category of C*: a statement x that qualifies the result of a statement y, the condition under which y's result stands. Each is asked as y with x against y alone. On every case where x holds the two answer alike, and on a case where x fails y's result does not stand: a pair of them closes only at a case where x fails. `LDG.E.CONSTANT` against `LDG.E`, `ld.global.nc` against `ld.global` in PTX, qualifies a load by nothing writing the memory it reads while the kernel runs, and the C of both is one text. Every carrier holds that, the two answer alike and the pair stays open, and it closes only on a carrier that breaks it, which the C leaves undefined. A flag glued to a test qualifies it the same way, `test_word_nonzero_and` against `test_word_nonzero` closing at a case where the flag fails, and so do `if` and `unless` on a doing word and `atomic`, no other thread coming between. The statement x is a condition, a flag: an operand one form names and the other does not that carries a value, the value `word_copy` copies or the addend of `word_mul_add`, is what the result is made of, and qualifies nothing.
- **The modifiers.** `modifier_coherence`, a category of C*: a word that changes what a form reads, writes or does, asked as the form with it against the form without it. Where the two differ both results stand, and a pair of them closes at its first case apart: `global_load_signed_byte` against `global_load_byte` closes at `0080:00a0`, and `signed_byte` is the thing it reads. A modifier qualifies nothing. Each pair of `Lstar.klq` whose two forms are one text but for their modifiers is an entry of `qualifier_coherence` or of `modifier_coherence`, and the case it closes at sorts it: a case where x fails is a qualifier's, and a case where both results stand is a modifier's. A pair open on every carrier is a qualifier's.
- **The vectors.** `vector_coherence`, a category of C*: a word whose answer changes with where a lane sits and the shape of its launch, and never with the case it is given: `thread`, `block`, `grid`, `idx`, `dim` and `warp`, each taken at `x`, `y` or `z`. It is asked over launch shapes and not over K. The pairs `block_idx_z` and `block_idx_y`, `block_dim_y` and `block_dim_x`, `block_dim_z` and `block_dim_x`, `grid_dim_y` and `grid_dim_x`, and `grid_dim_z` and `grid_dim_x` answer alike on every case of one launch, and no carrier closes them.
- **The ranges.** `range_coherence`, a category of C*: a part or a place, a description of range, which piece of one whole a form gives: `low`, `high`, `first`, `middle`, `last`, `left` and `right`. The parts of one whole put together give the whole, an entry of I (Collapse 3): `word_mul_low` with `word_mul_high` is `wide_mul`, and `word_add_first`, `word_add_middle` and `word_add_last` are a chain of one add.
- **The comparisons.** `comparison_coherence`, a category of C*: a relation between operands that gives a flag, `lt`, `le`, `gt`, `ge`, `eq`, `ne`, and against 0 `zero`, `nonzero` and `negative`. Each is tied to the others by entries of I: lt(a, b) = gt(b, a), ne = ¬eq, and `zero` is `eq` against 0. The pairs `test_word_ne` and `test_word_nonzero`, `test_word_eq` and `test_word_zero`, `test_signed_word_lt` and `test_signed_word_negative`, and `test_wide_ne` and `test_word_nonzero` are each a comparison against an operand beside the same comparison against 0, and are asked nothing. It holds two sets, and the frame is the line between them.
- **The equalities.** `equality_coherence`, a part of `comparison_coherence`: a distance from n of 0 or not 0, n any value, `eq` and `ne` at the other operand and `zero` and `nonzero` at 0. It needs no frame of reference: two words are equal under every reading of their bits, and gnascor's tests have no signed row for any of them. Equality is purely relational, and maps directly to the relational assembly: every verdict of the protocol is an equality over cases, alike or apart, and Collapse 1's integer case is f(k) = h(k).
- **The orders.** `order_coherence`, a part of `comparison_coherence`: `lt`, `le`, `gt` and `ge` at the other operand and `negative` at 0. Each needs a frame of reference for one thing to be less than another, a sign and a width, and no answer has given one. Its entries are `unknown_coherence` until the part reads the frame out. The frame is the delta between the two parts: `test_signed_word_lt` against `test_word_lt` is apart in the frame alone, `ISETP.LT` against `ISETP.LT.U32`, and the decoder reads the pairs apart in a frame, or across the line from an equality to an order, into `modifier_coherence`, where the frame is what moved. The floating bracket of Collapse 1 is order as well, the end the part takes its rounding.
- **The negations.** `negation_coherence`, a category of C*: a word that, taken twice, gives back what it was given, x(x(y)) = y on every case: `neg`, `not`, a predicate's `!`, and `unless`, which is `if` negated. On the tree a negation is the sibling link between the two members of one node, and the decoder reads it off a pair of forms of a flag that name the same operands and answer apart whatever the link reads: every case apart, or apart only where words the link does not read decide it.
- **The timings.** `timing_coherence`, a category of C*: every time the part gives, held in one place. The stall is the soonest a result is read, walked down on the part (Step 9) with the barriers and the waits that hold a read until its result is back (P9). An operation's time is read off the curve of a task, its knees against the registers and the threads (Step 10). A vector's time is the delta of one step along it: the simplest asks put in orders drawn from a seed, one register, one lane or one operand of a case more, and the delta between the orders the cost of the step. No time is written in: each is read as a delta between orders or between steps, and a time no answer has given is `unknown_coherence`.
- **The vectors of an ask.** An ask enters the part through its vectors, a register, an address, a lane: the registers it declares, the operands of a case its code loads, and the lanes it is put over, a case a lane. Its magnitude is how much of each it takes. The query asks the simplest first, at the least magnitude, the registers its chain names, and walks up only where the part refuses the least and answers the most. Carriers of one magnitude are put in an order drawn from a seed, and the delta between the orders is the cost `timing_coherence` holds.
- **The controls.** `control_coherence`, a category of C*: what decides which form runs next, in its parts. The transfer is what is done, `exit`, `return`, a branch; the loop is the way back and the label it goes back to; the condition is what it is done under, `if` and `unless`, one node that the qualifiers' guard links to, since a guard qualifies a result and conditions a transfer; the target is where it goes, a `label`; and the dispatch is a state's next, `state_dispatch` and `state_next` where a target clocks a lane a state at a time.
- **The switches.** `switch_coherence`, a category of C*: a word no case reads that places the forms a case does read, `open`, `body`, `close`, `note`, `declare` and `start`. Taken out, one changes no answer or refuses the whole program, and never moves one case.
- **The unknowns.** `unknown_coherence`, a member of every set of our coherence: an entry no answer has read into a set. It can be anything, and it is therefore a member of all of them until an answer reads it into one. It is the gray of P1. A pair of `Lstar.klq` the decoder reads into no set is written `unknown_coherence` beneath its verdict.
- **The cases.** K_c for each category, grown by the descent (Collapse 4).
- **The identities.** I, relations between texts. Each is witnessed by the text, and both its sides are held to each other on the part over K (P10). Each is an entry of `Lstar.klq`, `text_identity <text> = <text>`, its sides functions as the stick's manifest writes them, and its verdict beneath it: closed at the first case the host or the part answers its sides apart, open with its count of cases where both answer them alike on every case. A name of the bridge carries its stem and then what it is, an identity or an address: the identities a slice of two questions carries are `slice_identity <address>`.
- **The compositions.** Chains of forms, a category whose cases are its links' cases (Q19).
- **The record.** R, every ask with its answer, the part and the size it was read on (Q9, Q10). An ask R holds is not put again. Its rows sit in the part's `.ksc` (`ksc.oracle.tsv`), each keyed by the hashes of its code, its cases and the host's answers to them, and the registers and shape it is put with. Its word is the host's place of the first case apart, `ffffffff` where every case is alike. A timed ask is a sample of its own and put every time, and an ask the gate holds never reaches the part and has no row.

| answerer | relation | read by |
|---|---|---|
| the host, exact | f(k) ∈ [down(k), up(k)] | P1, P2 |
| the part alone | holds(n) ⇒ holds(m) for every m < n | the walk, Step 9 |
| the part's clock | c(r, t) ≤ hi | the bounced sustain, Step 10 |
| a model holding state | f(k_1 … k_n) over sequences, a stated bound on states | none yet |

The last row is the stick's memory, atomic, warp and sync questions, 72 of them. A per-lane relation does not reach them: they ask a sequence of an answerer holding state, and are exact only up to a stated bound on its states ([learning_a_ruleset.md](../../thought_experiments/engine/learning_a_ruleset.md)).

**Collapse 1. One relation for every exact answer.** The host gives an exact value bracketed at the result's width w, and every ask of it is f(k) ∈ [down(k), up(k)]. An integer case is a bracket of one word, down = up, and the ask is f(k) = h(k). A floating case is a bracket of two ends, and the end the part takes is its rounding (P12). A case the C of the host leaves undefined has no bracket and gates nothing. Equality, the bracket and the refused case are one relation at three widths.

**Collapse 2. One walk.** A relation over an ordered set that holds below a bound and not past it is read by one walk: the last register, the launch, the fewest registers and each knee. The walk steps or bisects to the bound, with the bounced sustain where the reading carries noise and a band of 0 where it carries none.

**Collapse 3. Forms read through identities.** A form of 2w is a pair of forms of w joined by an entry of I: add and take by the carry and the borrow, the bitwise forms word by word, a shift right through both words. The wide row of gnascor's table is then read off the word row and I, and asked of the part as a composition, in place of a form written by hand into every ruleset. The reader's own rules are entries of I as well, and the reader consults I in place of carrying them:

- [select(p, 1, 0) ≠ 0] = p
- −a = 0 − a
- a shift's count is its low word below the width
- (bool)v = [v ≠ 0] over the whole of v
- p ∧ q = q ∧ p, and p ∨ q = q ∨ p
- select(p, a, b) = select(¬p, b, a)
- a test glued to a flag is the test and a join, `test_*_and` and `test_*_or`

**Collapse 4. Cases and checks are one pool.** With K_c open, the descent that orders the cases (P2) also chooses the case to add. The open pairs of c are its candidates (P5), and the case added is a case the most of them fail.

**The scheduler.** The gray entries of every set are its queue, and P11's bounds are that queue written out. Each turn:

1. R gives the gray entries.
2. The ask that settles the most of them is chosen, the descent of P2 taken across categories.
3. It is put through the asker of its answerer: the part through the device daemon's admission, one ask at a time.
4. Its answer is written to R and to every entry it settles.

A process runs again only where an entry it reads has changed. A new category, case or identity is one more pool on the queue.

While the stick's questions share no case, an ask settles its own entry and no other, and every order of the queue settles as many entries. The descent across categories chooses among asks once an ask settles more than one entry: a case added to K_c, an entry of I, or a composition.

**Cost, a curve in the answerer's unit.** What a writing costs is what the answerer spends to give it, read off its curve (P6, Step 10), and never its count of symbols. I gives the writings that mean the same, and the curve chooses among them: of two writings I holds alike, the part spends less on the one written. The cost falls on the step between neighboring forms. Two forms that keep their registers and their unit and differ in a modifier alone step more cheaply than two that move them, and the curve is fitted over pairs of adjacent forms. The context entries of `Lstar.klq`, the nearest link before and after that names a register a link names, are the pairs it is fitted over.

- **Measured**, R (`bash utils/maint/engine/klq_identity.sh queue`, every chain the engine writes and the host computes put over all 648 cases): 441 chains, 441 alike, 0 apart, held in 282 rows of `sm_86.ksc`, a row for each distinct ask. Put again, all 441 asks are read from R and none reaches the part. The register walk's 29 asks that reach the part are read from R the same way.
- **Built**, Collapse 1: every host answer is read as a bracket, one word or `down:up`, and every case as f(k) ∈ [down, up]. The host gives every integer question one word, and over all 648 cases with no ask taken from R the 441 chains answer alike as before.
- **Built**, Collapse 2: `walk_step` and `walk_halved` in `klq_identity.cu` read the last register (R252 over 35 asks), the fewest registers and each knee. Task 1006, walked again, gives the fewest at 1 register and its knees inside the spread of its times.
- **Measured**, the queue (`klq_identity.sh queue`, the stick's manifest and R): 1016 entries, 441 reading 1, 0 reading 0, 575 gray, P12's table row for row. Of the gray: 399 hold a floating value, 92 a call or element nothing types and 4 a name nothing types, 34 a statement nothing reads, 33 a quotient or remainder with no reading, 11 a built-in the host computes no answer to, and 2 a form of `sass.krs` assembled with no reading of its operands. `<folder>/queue.txt` holds each entry and what keeps it gray.
- **Measured**, I (`klq_identity.sh text_identity`: each side a kernel of a stick of its own, listed by nvcc, written by the engine, computed on the host and put to the part through R): 164 identities of Collapse 3 over the stick's integer types and tests, 280 sides. All 164 are open: 160 on all 648 cases, and the 4 shift identities on 144, the cases whose count is under the width, the rest left undefined by the host's C. A shift identity is therefore witnessed only where its count's high word is 0. Two false identities put beside them, `int a + b = int a - b` and `unsigned int a ^ b = unsigned int a | b`, close at their first case apart. No side the engine writes answers apart from the host, and no identity has closed on the part.
- **Measured**, the pairs (`klq_identity.sh pair`: each pair of `Lstar.klq` put to the part through R, one form standing in for the other at every link of the engine's chains that holds it and carries the cases (`carrier_flow.h`), where a later link reads what it writes, and its verdict written beneath it): of 54 pairs, 36 close at their first case apart, over 620 asks, each put at its carrier's least vector, the registers its chain names, and walked up where the part refused that and answered declaring every register, its carriers of one magnitude in an order drawn from seed 1. A link of the carrier's own, the thread's index, the case's index, the bound and the addresses, carries no case, and nothing is put there. `global_load_constant_word` in place of `global_load_word`, the two apart in a qualifier alone, answers alike at 416 links over 259776 cases, and is open: the first entry of `qualifier_coherence`. The other 17 are asked nothing: no link that carries the cases holds them where the other form can be written in. `klq_decoder` reads the log beside the trace and writes each pair's set beneath its verdict: 4 in `qualifier_coherence`, `.CONSTANT` and the three tests glued to a flag by `and`, 15 in `frame_coherence`, the width and sign of a load and the sign of a shift and of a test, and 10 in `modifier_coherence`, the comparison a test makes and `and` against `or`. With the product read at the form and each closed pair put on at the links of delta 0 until its product is whole, 1637 asks at seed 1, 34 pairs carry a whole product and 26 concepts. The four signed comparisons of a word against their unsigned forms, `ge`, `gt`, `le` and `lt`, are one concept, and the four of a wide another: signed and unsigned order part exactly where the operands' signs part, whichever comparison reads them. `zero` against `nonzero` of a word is one concept with the warp's size against a copied word and against a word of a wide: each pair writes two values on every word the cases are made of. The warp's size, 0x20, is not one of those words, and the cases grow to hold it before the three part. 4 are `negation_coherence`, the complements `zero` and `nonzero` of a word and of a wide, `eq` and `ne` of a wide, and `ge` and `lt` of a word. The other 21 are `unknown_coherence`: the 17 asked nothing, among them the indices of a thread and a block no question reads as a value, and 4 pairs of two operations: 3 a value the chain computes against one the launch gives, the warp's size or the thread's index, and a multiply-add against its product alone.
- **Theory.** The reader consulting I in place of carrying its rules, the wide row read off the word row and I, K_c, the cost curve over pairs, and the floating brackets.
