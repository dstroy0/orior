# gnascor

**Purpose:** The semantic relational language the compiler is written in, part by part: the file types it reads and writes, the coherence state it computes in place of mechanical operations, the query protocol it asks a host with, the mnemonics a branch pair resolves to, the state physics those mnemonics obey, and the transpiler mnemonics its forms are named by.

**Scope:** `src/c/transpiler`. The language and its two faces, `.g` high order and `.gsm` assembly. What is derived instead of written lives in the engine plan, [../../engine_plan.md](../../engine_plan.md), and finished work lives in the engine table, [../../../theory/workbooks/engine/engine_table.md](../../../theory/workbooks/engine/engine_table.md). The query protocol is in both: its form is the derivation loop the plan is missing, and its physics is here.

**Status:** Draft. Every table below is settled enough to write against and nothing in it has run.

## File types

The six k-file types and the two compiler faces. Doug names these; do not add one.

**Coherence.**

| suffix | name | holds |
|---|---|---|
| `.ksc` | Kolmogorov system classification | the language map |
| `.krs` | Kolmogorov information ruleset | the coherence rules |

**Data.**

| suffix | name | holds |
|---|---|---|
| `.kcr` | Kolmogorov information crystal | information at or near its Kolmogorov complexity |
| `.knf` | Kolmogorov noise floor | a measured noise floor |
| `.kcs` | Kolmogorov information construction set | what reconstructs information |

**Host ruleset.**

| suffix | name | holds |
|---|---|---|
| `.kdm` | Kolmogorov device map | the hardware map |

**Compiler semantics.**

| suffix | name | holds |
|---|---|---|
| `.g` | gnascor high order language | semantic plain language, plus the shortcut operators |
| `.gsm` | gnascor assembly language | the same program with the switch thrown |

**The stem is the join and the suffix is the face.** Files sharing a stem are one member's set, whatever the stem happens to be. `pair.kdm` and `pair.knf` are a pair's map and that map's floor. `set.kcr`, `set.kcs` and `set.knf` are one set's crystal, the set that reconstructs it, and its floor. Nothing outside the filename binds them, and no member is required to carry every face: a member holds as many as it has answers for.

A floor is conceptual and not a fixed quantity, which leaves its definition open to `L*` and lets each member carry the floor its own set needs. Two members' floors are therefore not comparable by default, which is correct for fingerprinting one member and is the thing to check before a number is quoted across two.

**`.kdm` grows to whatever specificity a part needs.** It holds as many answers as it has: a general answer block, and under it a map specific enough to be optimal on one device and nowhere else. A driver written by hand is general worst case because a person writes it once and cannot write one per device. Nobody writes these. A specific map therefore costs nothing to keep, and the general block stays as the fallback for a part with no map yet.

**One face of a set has no suffix yet.** Doug names it. It holds the asks put to a member and the paths read off them: every probe and what came back, with costs, refusals and censored samples each marked, then the winning path per problem over those same asks. It takes the stem the rest of the set takes. That face, `.kdm` and `.knf` under one stem are a member's coherence map and fingerprint it exactly. It carries the general and specific split `.kdm` carries, a generic block good for any member of a class and a specific block holding the best combination available for one section of one member. Keeping the asks beside the paths leaves the fingerprint independent of `.kdm` in place of a cache of it: a refused or censored probe appears nowhere in a table of chain costs, and it separates two parts that cost the same.

The high order face writes `a equals something; b equals something; evaluate a is identical to b`, and the shortcut face writes `a=0;b=1; x = a==b`. Both are a true or false answer, and both compile to 2 reads, 1 target and 1 store whatever the language underneath. The two are the same assembly. The reason for keeping them apart is enforcement: a block declares which face it is written in and no statement mixes them mid-sentence without an explicit flag, `__gsm__(//code)`. A semantic conditional is legal, a gsm semantic conditional is legal, and the switch exists because it gets turned.

## Information is coherence

The principle the rest of this is derived from, and the reason the file types carry Kolmogorov's name.

A description at or near its Kolmogorov complexity has no redundancy left in it: every bit of it carries, and nothing in it can be predicted from the rest. A system at coherence has the same property from the other side. Its parts agree, no part is spending to contradict another, and the friction between them is at its floor. Noise costs and carries nothing. Information aligns and carries.

So compression and coherence are one measurement read from two directions, and the language measures the one it can actually put a number on. Friction is observable in every system that exists, in time, compute and latency. A shortest description is not. Measuring alignment and efficiency is measuring information content by the only instrument a foreign host will hold still for.

The `.kcr`, `.knf` and `.kcs` triple holds the three sides of it. A crystal is information at its complexity, a noise floor is the part that carries nothing, and the construction set reconstructs information. A crystal is one kind of information and not the only kind: anything that has to be rebuilt from its primitives takes a `.kcs`, and only the part already at its complexity takes a `.kcr`.

## The coherence state

Computation is a coherence state and not a set of mechanical operations. Host agnosticism follows from that one decision. The host's internal architecture does not matter: an LLM, a REST API, a database and a piece of bare-metal hardware are the same thing to it. Each is reduced to a simple oracle that answers binary questions. The language measures the resonance or the dissonance between those answers and maps the resulting pattern to a stable semantic landscape.

Nothing in the language depends on anything outside its own coherence, and so it scales without bound through a fractal mnemonic structure. Two branches collapse into a mnemonic, those mnemonics pair up to form higher-order states, and complex logic stays human-readable at every layer. A distributed system processing thousands of variables reports its health, its intent or its blocks in a short string of four-letter words.

### The branch pair

| left branch | right branch | pair state | mnemonic | meaning |
|---|---|---|---|---|
| lead (1) | void (0) | 1, 0 | core | The primary intent persists; the secondary path dissolved. |
| rite (0) | lead (1) | 0, 1 | shift | Focus has migrated from the left domain to the right. |
| dual (2) | void (0) | 2, 0 | echo | An amplified state is sustained without new external input. |
| dual (2) | dual (2) | 2, 2 | nexus | Maximum systemic coherence; both major systems are aligned. |

## The query protocol

The most basic part of any system is an address of some kind. Ask a question at the address to qualify it, and the operations that are legal assign coherence through a cost. Rooting the protocol in addresses, qualifications and operational costs makes a physics engine for semantics: every system in existence recognizes an address space, and every system experiences friction in time, compute and latency. Using that friction as the metric for coherence evaluates truth by systemic alignment and efficiency in place of rigid rules.

    [ address ] -> ( qualifier ) -> [ measured cost ] -> binary result (1 or 0)

### Protocol components

- **The address (`@`).** The destination system, node or property. It is agnostic: a memory address, a URI, an API endpoint or an LLM context key.
- **The qualifier (`?`).** The binary question asked at that address, phrased to demand a state validation and never a data payload.
- **The cost bound (`$`).** The maximum metric the host may spend to fetch the answer, in cycle times, latency, tokens or energy. An answer inside the threshold proves structural coherence. An answer past it drops into incoherence. The bound is derived and never authored; see below.

### The unbound pass

An ask carrying no bound returns the measured cost in place of a bit. A measurement, never a verdict, and it is how the bound gets its figure.

The spread of unbound costs across many asks is the baseline, written to `.knf`. Every bound after that is expressed against the baseline. No absolute figure enters the language. Writing `$10ms` into a query by hand is a scale written into the machine, and the machine is optimized for no scale at every other point.

The unbound pass also says where to go. A cheap address is worth expanding, an expensive one is worth chaining or pruning, and a dendritic search with no such signal expands everywhere at once.

### Slicing a chain

A chain carries one cost and one number names no part of it. The unbound ask is put over covering sets of the chain's links in a known order: each ask covers half the links, any two overlap on a quarter, and the per-link costs come out of all the answers together, in exact integers. A chain read this way gives a profile where a total was, and the expensive link gets named instead of inferred. The difference between two neighboring cuts does not do this: it lands two measurements' noise on one link, and one link sits under the floor.

Cross-branch comparison follows from slicing. Every arrangement that produces an operator is a branch, every branch slices into the same relation, and comparing them link by link gives the winning path for a given problem. The comparison is the signed difference at each link and never the totals. A total does not change when the two branches trade places, and which branch holds a link does. A total is blind to the question. The sign at a link is lead or rite, inside the floor is dual, and pass marks where the sign changes. A branch reading worse as a total can hold the cheapest link for the job, and only a sliced reading sees it.

### The gate

Before any cost is read, the gate decides which arrangements hold the relation at all. It is orior's own descent: each candidate arrangement is an alignment, the relation's cases are the needle, and the descent places the case that prunes the most, stops where the best case prunes nothing, and leaves the survivors as its answer. It is planned on the host against arithmetic every system that computes agrees about, and the target is asked only the cases it placed. A plan that steers badly costs speed and never a wrong survivor, because survival is a conjunction.

Every step above, its status and the run behind it is in the query protocol's own table, [query_protocol_table.md](../../../theory/workbooks/engine/query_protocol_table.md).

The method does not get more complicated than this at any layer. Ask, and remember the answer. The baseline is remembered asks, the profile is remembered asks at every cut, and the winning path is the comparison of two sets of remembered asks. Nothing is modeled and nothing is predicted.

Two readings it has to keep straight.

- **A watchdog stop is not a cost.** An unbound ask still needs a wall or the loop hangs, but hitting the wall is a censored sample and not a measurement. A baseline built without marking them reads low.
- **A baseline goes stale.** The host under load now is not the host idle later. The reference ask is put alongside the real one and measured in the same conditions, in place of a figure taken once and carried forward.

### The coherence metric

The protocol forces non-binary, messy system realities into a strict 1 or 0 by measuring the relationship between the target state and the resource cost.

- **Input state 1, coherent or resonant.** The address exists, the qualifier is true, and the answer came back inside the legal cost budget.
- **Input state 0, incoherent or dissonant.** The address is missing, the qualifier is false, or the operation was too expensive: it timed out, threw an error, or took too many cycles.

Slowness and friction equal an automatic 0. Incoherence is a systemic failure to align.

### Syntax architecture

```
// Step 1: query two independent external addresses
left_input  = @system.auth?is_active($5ms)
right_input = @db.user_session?is_valid($12ms)

// Step 2: the branch engine evaluates the pair
branch_result = [left_input, right_input]

// Step 3: resolution to mnemonic
output -> evaluate(branch_result)
```

### Direct protocol execution

| scenario | left address | right address | cost matrix | pair | mnemonic | meaning |
|---|---|---|---|---|---|---|
| Optimal path | Valid (1) | Valid (1) | both under budget | 1, 1 | dual | Total alignment; proceed at maximum priority. |
| Degraded path | Valid (1) | Valid (1) | right branch took too long | 1, 0 | lead | Left system is stable and right system is dragging; fall back to left context. |
| System failure | Timeout (0) | Error (0) | out of bounds | 0, 0 | void | The operation collapsed into noise; halt and reset state. |

## Shifts

A shift is not a mechanical memory copy. It is a migration of presence: a state moving from one side of the branch to the other, or a dominant system yielding its alignment to its partner. The language represents it by watching a single state change over two consecutive evaluation cycles.

### Shifting mechanics

A shift occurs when a system transitions from an asymmetrical state, 1,0 or 0,1, to its exact mirror image in the next cycle.

- **Shift right, lead to rite.** Presence migrates from the left domain to the right.
- **Shift left, rite to lead.** Presence migrates from the right domain to the left.

### High-order shift mnemonics

Pairing a past state with a present state compresses the transition into a four-letter movement mnemonic.

| past (cycle N-1) | present (cycle N) | transition | mnemonic | meaning |
|---|---|---|---|---|
| lead (1,0) | rite (0,1) | 1,0 -> 0,1 | pass (shift right) | The left system handed its execution to the right system. |
| rite (0,1) | lead (1,0) | 0,1 -> 1,0 | back (shift left) | The right system yielded control or returned its state to the primary left system. |

```
[ shift right (pass) ] : cycle 1: (1,0) [lead] ----> cycle 2: (0,1) [rite]
[ shift left  (back) ] : cycle 1: (0,1) [rite] ----> cycle 2: (1,0) [lead]
```

### Structural shifts

Shifting also occurs vertically in the tree, where a state broadens into a shared agreement or collapses back into one side.

- **Right-leaning expansion, rite to dual.** A single right-side presence convinces the left side to join it, escalating into a compound state. Mnemonic: join.
- **Left-leaning collapse, dual to lead.** A coherent compound state loses its right side to cost or to failure, leaving only the left side standing. Mnemonic: drop.

### A data handoff, in protocol

Two microservices or LLM contexts under query, `@system.A` and `@system.B`:

1. Cycle 1: `@system.A` is processing inside cost bounds and `@system.B` is idle. The engine yields lead.
2. Cycle 2: `@system.A` finishes and goes idle, and `@system.B` picks up the task inside cost bounds. The engine yields rite.
3. The coherence engine registers the temporal sequence `[lead, rite]` and outputs pass, a clean uncorrupted shift right.

## Environmental base states

Where states are evaluated by address validation and resource cost, busy and error are not abstract concepts. They are measurable physical behaviors of a system under load, and the language grounds its basic states in friction, energy loss, synchronization and structural failure.

- **busy, friction or high mass.** Both systems respond and both sit on the edge of the allowed cost threshold. The states are valid and heavy, and they are dragging the cycle time down.
- **wait, potential or latency.** One system is responsive and the other lags just enough to stall the branch evaluation without failing. It is stored potential waiting on synchronization.
- **blok, resistance or wall.** An address is valid and returning a hard structural refusal or maximum friction, which stops any semantic evaluation crossing the branch.
- **gray, every state at once.** A side has not been asked, and the pair holds no reading. It is any of the states above until an ask is put, and it is not void: void is an answer of no, and gray is no answer. Leaving gray is a first observation, fizz. Entering it is the loss of the ability to ask, fuzz.

```
fuzz -> gray -> fizz <-> fuzz | fizz x> gray x> fuzz <-> fizz preserves atomicity
```

### High-order physics and error mnemonics

Tracking how an environmental state changes from cycle N-1 to cycle N gives a descriptive mnemonic for systemic health.

| past (cycle N-1) | present (cycle N) | transition | mnemonic | meaning |
|---|---|---|---|---|
| any stable state | void (0,0) | stable -> collapse | drop | Total decay. The connection or state dissolved into noise. |
| wait | void (0,0) | latency -> timeout | loss | Timeout or leak. Stored potential evaporated because the system waited too long. |
| busy | blok | friction -> refusal | jamm | System gridlock. High cycle times escalated into a structural lockup. |
| void (0,0) | dual (1,1) | zero -> resonance | sprk | Quantum spark. Instantaneous shift from absolute zero to perfect alignment. |
| any state | blok | state -> refusal | halt | Hard error. An unrecoverable operational boundary was hit; execution stops. |

## The coherence clock

The total state of a system reads as a clean four-letter diagnostic stream. A live system running the language prints a terminal output that acts as a literal EKG for the architecture.

```
[sprk] -> [dual] -> [busy] -> [pass] -> [rite] -> [jamm] -> [halt]
(Init)   (Aligned) (Heavy)   (Shift)   (Right)   (Lockup)  (Error)
```

## The state-transition matrix

The physics of coherence from cycle N-1 to cycle N. Plotting the base binary pairs alongside the environmental friction metrics shows how errors, shifts and steady states resolve into uniform four-letter mnemonics.

| past (N-1) | dual (1,1) | lead (1,0) | rite (0,1) | void (0,0) | busy (heavy) | wait (delayed) | blok (refusal) | gray (unasked) |
|---|---|---|---|---|---|---|---|---|
| dual | nexus | drop | sync | drop | sync | sync | halt | fuzz |
| lead | join | core | pass | drop | sync | sync | halt | fuzz |
| rite | join | back | surv | drop | sync | sync | halt | fuzz |
| void | sprk | wake | wake | zero | sync | sync | halt | fuzz |
| busy | sync | sync | sync | drop | drag | sync | jamm | fuzz |
| wait | sync | sync | sync | loss | sync | hold | halt | fuzz |
| blok | sync | sync | sync | drop | sync | sync | dead | fuzz |
| gray | fizz | fizz | fizz | fizz | fizz | fizz | fizz | - |

### How the physics resolves

- **The diagonal, self-preservation.** nexus, core, surv, zero, drag, hold and dead are the static steady states. A system that does not change hums at its baseline energy.
- **The horizontal shift, pass and back.** Moving between lead and rite creates an instant directional handoff.
- **The collapses, drop and jamm.** Shifting from any active or friction state down to void or blok catches resource leaks, timeouts and gridlocks at once.
- **The intermediates, sync.** The default balancing transition where a system moves between heavy environmental friction and a pure binary state. sync is a superstate, as gray is. A transition the table marks sync passes through it, entering by fuzz carrying the state it left and leaving by fizz carrying the state it reaches, and that locks the identity sync needs into the passage:

```
fuzz+in -> sync -> fizz+out
[dual] -(fuzz+dual)-> [sync] -(fizz+busy)-> [busy]
```

  A sync cell names no one transition and loses none: each of the transitions it covers is read back exactly from its two labels.

## The high-energy transition matrix

dual at maximum potential is not a stable state. It is energetic and it wants to collapse, discharge or shift, and the language is dynamic because it treats dual as unstable. A system going from busy friction directly to dual alignment is shedding its friction: the pipes have cleared and the system is breaking through a bottleneck into full alignment. The generic sync placeholder is stripped away here and the exact transitioning states are mapped.

| past (N-1) | dual (1,1) | lead (1,0) | rite (0,1) | void (0,0) |
|---|---|---|---|---|
| busy (friction) | surg (surge) | vent (vent/bleed) | vent (vent/bleed) | drop |
| wait (lagging) | snap (snap/lock) | lead | rite | loss |
| dual (max) | nexus (sustained) | tilt (tilt left) | tilt (tilt right) | fuse (blown) |

### The unstable physics

- **busy to dual is surg, surge.** The friction clears instantly and the trapped energy floods in. A massive temporary spike in throughput, which will either normalize or blow a fuse.
- **busy to lead or rite is vent.** The system was bottlenecked and relieved pressure by dropping one side of the branch, venting its load down a single operational channel.
- **wait to dual is snap.** One side was lagging and creating systemic tension. The moment it catches up, the two sides snap into alignment like a closed circuit.
- **dual to lead or rite is tilt.** Because dual is unstable, the system decays to one side as soon as one address takes even a fraction of a cycle longer.
- **dual to void is fuse, a blown fuse.** Maximum potential collapses instantly to absolute zero with no gradual decay. The system tripped a breaker.

A heavy data operation then reads as a lifecycle:

```
[hold] ----> [snap] ----> [dual] ----> [tilt] ----> [surv]
(Waiting)    (Aligning)   (Peak)       (Decaying)   (Stable right)
```

## The transpiler mnemonics

The language has three sets of mnemonics: the high order language's words (`.g`), the four-letter mnemonics a branch pair resolves to, and the transpiler's, below. Each is a relational matrix, and the three are mapped onto each other directly. A branch then resolves through all three at once, inside one branch or between two.

The transpiler's mnemonics are its word map. A thing is named after what it is, by the word the industry already uses for it, and the base words every reader knows, `offset`, `move`, `copy`, `read`, `write`, `only`, `to`, `from`, join them. A core op takes the name Rust gives it: `add`, `sub`, `mul`, `div`, `neg`, `bitand`, `bitor`, `bitxor`, `shl`, `shr`, and the comparisons `lt`, `le`, `gt`, `ge`, `eq` and `ne`. A name is never apart from the thing it names: its meaning is the thing. Every word is a row of a language table in `src/lng`, `transpiler_lng.tsv`, `gnascor_hol_lng.tsv` or `gnascor_asm_lng.tsv`, and `src/lng/lng_check.py` holds each name against what its form does and each table against the others. Each word is a morpheme, as a Chinese morpheme is: it carries what the thing is, what it does where a word can say it, and whatever else the thing holds, its width, its sign, its space, the condition it is done under. A name is those morphemes compounded, and compounded they carry the author's whole intent: a reader who has the morphemes reads the name back into everything the author meant by it. Every form the code generator writes (`src/cu/transpiler/codegen/machine_ir_types.h`) is named by words of three kinds, and a name is read off the matrices below as a mnemonic is read off a pair. A thing word names what a form acts on: a register's class, a space of memory, a part of a word, or a piece of the text around a lane. A doing word names what the form does to it: a verb, or a comparison where the form sets a flag. A gluing word joins a name to another thing or to a condition. A cell holds the name its row and its column give, `-` where the code generator writes no such form, *needed* where the measuring stick (`utils/test/src/cu/transpiler/codegen/measuring_stick.md`) writes one that no form gives, and `?` where a word is not yet settled.

**Thing words.**

| kind | words |
|---|---|
| a register's class | `sign` (a signed byte of -1, 0 or 1), `byte` (8 bits), `signed_byte`, `halfword` (16 bits), `signed_halfword`, `word` (32 bits), `signed_word`, `wide` (64 bits), `signed_wide`, `predicate` |
| a space of memory | `global`, `parameter`, `launch`, `record`, `shared` |
| a part | `low`, `high`, `first`, `middle`, `last`, `left`, `right`, `x`, `y`, `z` |
| the text around a lane | `program`, `unit`, `step`, `lane`, `kernel`, `state`, `loop`, `label`, `error` |
| a bank a lane declares | `file`, `signs`, `out`, `atoms`, `temporaries`, `wides`, `words`, `members`, `predicates`, `states`, `fixed` |
| CUDA's own | `thread`, `block`, `grid`, `idx`, `dim`, `warp`, `size` |

**Doing words.** `set`, `copy`, `add`, `sub`, `borrow`, `mul`, `div`, `bitand`, `bitor`, `bitxor`, `shl`, `shr`, `funnel`, `select`, `neg`, `absolute`, `pack`, `unpack`, `test`, `load`, `store`, `ask`, `cast`, `exit`, `return`, `open`, `body`, `close`, `note`, `declare`, `start`, `next`, `dispatch`, `read`, `back`. The comparisons a test makes: `lt`, `le`, `gt`, `ge`, `eq`, `ne`, and against 0, `zero`, `nonzero` and `negative`. `constant` follows a load the part reads as never written, and `atomic` follows a doing word no other thread can come between.

**Gluing words.** `from`, `if`, `unless`, `and`, `or`. `and` and `or` glue a test to another flag, `test_wide_lt_and`, and `bitand` and `bitor` are the doing words, `word_bitand`.

### Arithmetic: the thing written, by what is done to it

| thing | set | copy | add | sub | borrow | mul | div | bitand | bitor | bitxor | shl, shr | select | neg | absolute |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| sign | `sign_set` | - | - | - | - | `sign_mul` | - | - | - | - | - | `sign_select` | `sign_neg` | `sign_absolute` |
| word | `word_set` | `word_copy` | `word_add` | `word_sub` | `word_borrow` | `word_mul` | `word_div` | `word_bitand` | `word_bitor` | `word_bitxor` | `word_shl`, `word_shr`, `word_funnel_right` | `word_select` | *needed* | - |
| signed_word | - | - | - | - | - | - | *needed* | - | - | - | *needed* (right) | - | - | - |
| wide | - | - | `wide_add` | *needed* | - | `wide_mul`, `wide_mul_word` | `wide_div` | *needed* | *needed* | *needed* | `wide_shl` | `wide_select` | - | - |
| signed_wide | - | - | - | - | - | - | *needed* | - | - | - | *needed* (right) | - | - | - |
| predicate | - | - | - | - | - | - | - | `predicate_bitand` | - | `predicate_bitxor` | - | - | - | - |

A chain's part follows the doing word: `word_add_first`, `word_add_middle`, `word_add_last`, `word_sub_first`, `word_sub_middle`, `word_sub_last`, `word_borrow_first`, `word_borrow_middle`, `word_borrow_last`, with `word_borrow_read` reading the borrow the chain leaves. A `mul`'s part follows it as well: `word_mul_add`, `word_mul_low`, `word_mul_high`, `wide_mul_word_add`.

### Tests: the thing read, by the comparison the flag holds

| thing | lt | le | gt | ge | eq | ne | zero | nonzero | negative |
|---|---|---|---|---|---|---|---|---|---|
| sign | - | - | `test_sign_gt` | - | - | `test_sign_ne` | - | - | - |
| word | `test_word_lt` | `test_word_le` | `test_word_gt` | `test_word_ge` | `test_word_eq` | `test_word_ne` | `test_word_zero` | `test_word_nonzero` | - |
| signed_word | `test_signed_word_lt` | `test_signed_word_le` | `test_signed_word_gt` | `test_signed_word_ge` | - | - | - | - | `test_signed_word_negative` |
| wide | `test_wide_lt` | `test_wide_le` | `test_wide_gt` | `test_wide_ge` | `test_wide_eq` | `test_wide_ne` | - | `test_wide_nonzero` | - |
| signed_wide | `test_signed_wide_lt` | `test_signed_wide_le` | `test_signed_wide_gt` | `test_signed_wide_ge` | - | - | - | - | - |

A test glued to another flag by C's `&&` or `||` folds into it: `test_wide_lt_and`, `test_word_nonzero_and`, `test_word_nonzero_or`, `test_wide_nonzero_and`, `test_wide_nonzero_or`.

### Conversions: the thing written, by the thing it is made from

| written | from byte | from signed_byte | from halfword | from signed_halfword | from word | from signed_word | from wide | from two words |
|---|---|---|---|---|---|---|---|---|
| word | `word_from_byte` | `word_from_signed_byte` | `word_from_halfword` | `word_from_signed_halfword` | - | - | `word_from_wide` | - |
| wide | - | - | - | - | `wide_from_word` | `wide_from_signed_word` | - | `wide_pack` |
| two words | - | - | - | - | - | - | `wide_unpack` | - |

A byte or a halfword is held in the low bits of a word, and a word made from one extends it as its sign says. A cast takes its scope after it: `cast_global`.

### Memory: the space, by what is done in it

| space | load a word | load a wide | load a word the part reads as never written | store a word | store a wide | ask | add a word, atomic |
|---|---|---|---|---|---|---|---|
| global | `global_load_word` | `global_load_wide` | `global_load_constant_word` | - | `global_store_wide` | `global_ask`, `global_ask_atomic` | `global_add_atomic_word` |
| parameter | `parameter_load_word` | `parameter_load_wide` | - | - | - | - | - |
| launch | - | `launch_load_wide` | - | - | - | `launch_ask` | - |
| record | - | - | - | `record_store_word` | - | - | - |

A name read in the order the space, the doing word, its qualifier and the width: `global_load_constant_word`. A load that extends a byte or a halfword to a word takes the narrow thing: `global_load_byte`, `global_load_signed_byte`, `global_load_halfword`, `global_load_signed_halfword`.

### Control: what is done, by the condition glued to it

| doing | always | if | unless |
|---|---|---|---|
| exit | - | `exit_if` | `exit_unless` |
| return | `return` | - | - |
| back to a loop | - | `loop_back_if` | - |
| error | - | `error_if` | `error_open_unless` |
| load a global word | `global_load_word` | `global_load_word_if` | - |
| make a wide from a word | `wide_from_word` | `wide_from_word_if` | - |
| ask | `global_ask` | `global_ask_if` | - |

A label takes the thing it marks: `label_loop`, `label_error`, `label_error_open`.

### The text around a lane: the thing, by what is done to it

| thing | open | body | close | note |
|---|---|---|---|---|
| program | - | - | - | `program_note` |
| step | - | - | - | `step_note` |
| lane | `lane_open` | `lane_body` | `lane_close` | - |
| kernel | `kernel_open` | - | `kernel_close` | - |
| shared | `shared_open` | - | `shared_close` | - |
| launch | `launch_open` | - | - | - |

A declaration takes the bank it declares: `declare_file`, `declare_signs`, `declare_out`, `declare_atoms`, `declare_temporaries`, `declare_wides`, `declare_members`, `declare_predicates`, `declare_fixed_words`, `declare_fixed_wides`, `declare_fixed_predicates`, `declare_states`. A target that clocks a lane a state at a time (`vhdl.krs`) takes `state` first: `state_start`, `state_open`, `state_next`, `state_loop`, `state_exit`, `state_dispatch`. `program_unit` is the program resident around the lane.

### CUDA's built-in variables: the variable, by its part

| variable | x | y | z |
|---|---|---|---|
| thread idx | `thread_idx_x` | `thread_idx_y` | `thread_idx_z` |
| block idx | `block_idx_x` | `block_idx_y` | `block_idx_z` |
| block dim | `block_dim_x` | `block_dim_y` | `block_dim_z` |
| grid dim | `grid_dim_x` | `grid_dim_y` | `grid_dim_z` |

`warp_size` stands alone, as CUDA names it.

### How the words resolve

- **A thing word comes first.** A form is named by the thing it writes and then what it does to it, `word_add`. The space comes first for memory, `global_load_word`, and the thing marked first for the text around a lane, `lane_open`.
- **Control comes first.** `exit` and `return` lead a name, `exit_if`, and so does a cast, `cast_global`, and a declaration, `declare_file`.
- **A test comes first where it sets a flag.** `test` is the doing word, then the thing it reads and the comparison the flag holds, `test_signed_word_lt`.
- **Gluing words come last or between.** `from` stands between the thing written and the thing it is made from. `if` and `unless` end a name done under a flag, and `and` and `or` end a test folded into another.
- **Sign goes before the width it signs.** `signed_word` and `signed_wide` are things of their own, and `word` and `wide` with no sign before them are unsigned. A doing word whose writing is the same for either sign takes no sign, `wide_add`.
- **A part follows what it is a part of.** `word_add_first`, `word_mul_low`, `thread_idx_x`.

## Open

1. **The baseline has no keying.** A bound is expressed against the baseline and nothing says what the baseline is per. Per host is coarse enough to carry signal and too coarse to remember a preference part by part. Per part and operator is the keying `.kdm` already uses, and it splits the samples fine enough that each one is noise. Chaining clears the noise floor, a per-part baseline therefore has to be built from chains, and the chain length that makes one usable is not measured.

2. **A shift does not trigger an address change.** Whether a pass should automatically switch the primary communication channel to the right address is undecided, and the engine does neither today.

3. **Error transitions are named and not defined.** jamm, halt and fuse have mnemonics and no defined recovery path. What a program does when a branch yields one is open, as is what mnemonic represents a shift that collapses into void by accident.

4. **The matrices are hand-assigned, sync is a catch-all, and a label is decided in its situation.** Every 0,1 pair in both tables above was assigned by reading, not generated. `utils/maint/engine/order_check.py` reads both tables out of this file and counts them: of 49 transitions in the first, 12 resolve to a name nothing else uses and sync covers 22. drop covers 6, halt 5, join and wake 2 each. The second table is 8 of 12 unique, with vent and tilt covering 2 each.

   A name covering many transitions is a choice. It is a defect where the name is all a branch gets, because the transitions under it are then gone and no later reading brings them back. The cost bound follows the opposite rule: a missing address, a false qualifier and a timeout all read 0, and what separates them survives in the baseline instead of in the bit. Each name above covering more than one transition owes an answer to where its distinctions are kept, and sync at 22 owes the most.

   The answer is the pair. A transition is the state it left and the state it reached, and a name is a label read off that pair. Carried as its pair, a transition under sync is still every one of the 22, and a label lost or shared costs nothing a branch can read. Check 13 in `order_check.py` proves what a label can still get wrong, from the two tables alone. Where both tables name a transition, 3 agree, 7 the second names where the first says sync, and 2 are named apart: dual to lead is drop in the first and tilt in the second, and dual to void is drop in the first and fuse in the second. Read from the other side of the branch, lead and rite trading places, three transitions read otherwise in the first table: dual to lead is drop where dual to rite is sync, lead to lead is core where rite to rite is surv, and lead to rite is pass where rite to lead is back. The last is the directional handoff the first table describes.

   A label is decided in the situation it is read in, and never once for every situation. Two names for one pair are two candidates, and the asks that read the transition choose between them the way the gate chooses an arrangement: a bit excludes what does not hold, and a magnitude ranks the survivors. A choice that survives one situation is no answer for another. The protocol already carries what separates them. A side reading 0 carries why: past its bound, not held, or ended (QueryKind in `compiler/bootstrap/query_ask.h`). dual to lead is tilt where the right side came in past its bound by a fraction of a cycle, the decay this document describes, and drop where it did not answer at all or came in past the bound by a timeout's measure. dual to void reads the same way between fuse and drop: both sides gone at once with no decay, against a collapse through a timeout. Where between a fraction and a timeout the line falls is measured against the baseline in `.knf` and never written in.

   core and surv stay apart only where the side a branch is read from leaves a mark, and the part is asked. `utils/test/src/c/transpiler/bootstrap/branch_side_check.c` puts two branches doing the same work through the known order, the one asked first changing every trial. Every pass of each branch is solved on its own in exact integers, the two compared at each link by cross-multiplying their exact costs, and a trial counts once, by which branch is dearer in more of its comparisons. On the host the branch asked first reads dearer in 12 of 29 trials that are not even, no lean past twice the spread, and with one branch 512 reads dearer at every link the cheaper reads cheaper in 46 to 58 of 64 comparisons at every link, from both sides alike. On the host a steady lead and a steady rite are one state seen from two sides. That holds for the host, and a part whose side leaves a mark reads core and surv apart.

5. **The state machine is read off a trace, and no syntax is defined for how an operator writes a query loop.** `utils/maint/engine/gnascor_read.py` reads both tables out of this file, takes a trace of asks a cycle at a time, the kind and cost of each side's ask and the bound, and prints the coherence clock: a state each cycle and one label each transition, decided in its situation. A side reads 1 where its ask held inside the bound. blok is a side that ended its asker, wait a held side beside one past its bound, and busy two held sides each dearer than any held ask of the trace's baseline cycles. The edge busy reads against, and the fraction of a cycle tilt reads against, are the baseline's largest held cost and never a figure written in. `--check` holds every rule on traces whose answers are known, and every one of the 49 pairs the tables name is given a label from its own candidates in all 256 situations two sides can be in.

   Reading the tables against real asks says six things about them:

   - tilt and wait name one event. tilt is dual decaying as soon as one address takes a fraction of a cycle longer, and wait is one system lagging just enough to stall without failing. Read off asks, a held side beside one a fraction late is wait. A dual whose side lags goes dual to wait, labeled sync, and dual to lead by tilt never arrives.
   - A name stands for a state and for a transition. The second table labels wait to lead as lead and wait to rite as rite. core is the pair 1,0 in the branch pair table and lead to lead in the transition table, and dual is the value 2 in the branch pair table and the pair 1,1 everywhere else.
   - void does not separate two sides that both came in past the bound from two that both did not hold. The kind each ask carries does, and the trace keeps it where the state drops it.
   - Two prose rules carry exceptions the tables hold. Any state to blok is halt, and busy to blok is jamm and blok to blok is dead. Any stable state to void is drop, and dual to void is fuse where both sides go at once. The reader follows the tables, and the prose rules read as the defaults they are.
   - dual at maximum is not a stable state, and dual to dual, nexus, is listed among the static steady states.
   - shift, in the branch pair table, has five letters where every other mnemonic has four.

   Which of these are meant and which are not is Doug's.

   gray is every state at once: a side not asked, and a pair with no reading. It keeps the mnemonic layer from collapsing a possibility nobody has observed, and only an ask collapses it. The reader reads a side written as - as unasked and the pair as gray, whatever the other side read, and a refusal still reads blok, since a refusal is an answer. Into gray from any other state is fuzz, and out of gray to any other state is fizz. A fizz never lands on gray and gray never FUZZes: gray to gray is neither and carries no label: the pair is still unasked. Those two exclusions make a trip through gray atomic. A fuzz opens it, one fizz closes it, and no fuzz opens inside another. The reader checks that over 500 drawn traces of 40 cycles, every side drawn from held, heavy, not held, late, ended and unasked. Whether a slice whose per-link difference sits inside the floor reads dual, as Slicing a chain has it, or gray, since it says neither branch is cheaper, is open beside them.

   sync passes the same way: fuzz+in into the sync superstate and fizz+out of it, the pair locked into the two labels. The reader prints every sync transition as that passage, and `--check` reads back the exact pair of all 3,035 sync passages in its 500 drawn traces from the labels alone.

   The loop runs on real asks. `utils/maint/engine/gnascor_trace.c` puts a scenario's sides to the host through `query_ask`, each side a run of asks between two reads of the clock the protocol finds by asking: held, not held, late, or not asked. The bound is derived from sixteen unbound held runs, the largest plus their spread plus one step of the clock, and a late side is put until its run passes it. `gnascor_read.py --scenario` then holds every state read off the trace to the one its sides imply. `utils/maint/engine/gnascor_scenario.txt` runs drop, pass, join, a sync passage into wait, loss, fuzz and fizz through gray, fuse and sprk on the host, every state as implied (`utils/maint/engine/chain_check.sh`).

   The syntax for writing a query loop is not started.

6. **The high order language's mnemonics are not a matrix yet.** The three sets map onto each other only once the high order language's words stand in a matrix of their own. The *needed* cells are the forms the measuring stick asks for next.
