# Engine plan

**objective**: compile a program written in gnascor to any language, including one nobody has met, and prove it is
the same program everywhere. Where the language is unknown, derive it by asking.

**information is coherence**: the principle the k-files are named for. A description at its Kolmogorov
complexity holds no redundancy, every bit of it carries, and no part predicts another. A system at coherence
has that property from the other side: its parts agree and the friction between them is at its floor. Noise
costs and carries nothing. Compression and coherence are one measurement from two directions, and friction is
the direction a foreign host will hold still to be measured on.

**what is known before meeting anything**: relations. `1,1 -> 2` is a relation and is not an addition, because
addition is a definition. Every system that computes agrees about the relation and each defines it its own way.
Arithmetic is the shared ground and the definition is what differs.

**gnascor** is the internal language, `.g` high order and `.gsm` its assembly. It is designed and it is what a
program is written in. It is not derived and its vocabulary does not move.

**`L*`** is the map from gnascor to a target's definitions, and it is the derived part. The compiler emits whole
files of relations in a shuffled order, which leaves a part nothing to tell measurement from work by, runs them,
and keeps whichever definition produced the relation.

    .ksc   the relations put and what came back                       derived
    .kdm   the part keyed to operators: every chain, each costed      derived
    .krs   gnascor to this target                                     written or derived

All three may be partly known. A known entry is information and is never thrown away; derivation fills the rest,
and the three agreeing is the coherence picture.

**the query protocol** is the form every ask takes, and it is the transpiler's: it, every step of it and its open work are in the query protocol table, [theory/workbooks/Lstar_protocol/query_protocol_table.md](../theory/workbooks/Lstar_protocol/query_protocol_table.md).

Finished work lives in the engine table, `theory/workbooks/engine/engine_table.md`. What is written here is open.

## File types

    .kcr   Kolmogorov information crystal
    .krs   Kolmogorov information ruleset: one language's forms
    .kcs   Kolmogorov information construction set: what reconstructs information
    .knf   Kolmogorov noise floor
    .kdm   Kolmogorov device map
    .ksc   Kolmogorov system classification
    .klq   Kolmogorov language query
    .klm   Kolmogorov language map
    .kqr   Kolmogorov query record
    .khw   Kolmogorov hardware

Doug names these. Do not add one.

**A file is named `<concept or thing>.<filetype>`.** The stem is the concept or thing and the suffix is its
type. `sass.krs` is how to write SASS and `sm_86.krs` is the ops ruleset for sm_86, grouped with `sm_86.kdm`; `sass.ksc`, `ptx.ksc`
and `avx.ksc` follow the same pattern. Files sharing a stem are one member's set, whatever the stem happens to be. `pair.kdm` and `pair.knf` are a pair's map and that map's floor. `set.kcr`, `set.kcs` and
`set.knf` are one set's crystal, the set that reconstructs it, and its floor. Nothing outside the filename
binds them, and no member is required to carry every face: a member holds as many as it has answers for.

**`Lstar.klq` is the bridge between languages.** Its stem is `L*`, the map from gnascor to every target, and it
holds the questions put across languages and what came back: each pair of forms whose sameness the text leaves
open, the cases asked, and the verdict, closed as two operations with the case that witnessed it or open with its
count of cases alike. It is keyed by the schema's form names, which are gnascor's and no target's. A language
attaches to it by its `.klm`, `cu.klm` and `sass.klm`: the language's forms keyed to the operations it answered
for, grouped by stem with the language's other files. A map from one language to another is read through the
bridge, `cu.klm` to `Lstar.klq` to `sass.klm`, and a language nobody has met attaches the same way.

A floor is conceptual and not a fixed quantity, which leaves its definition open to `L*` and lets each member
carry the floor its own set needs. Two members' floors are therefore not comparable by default. That is
correct for fingerprinting one member and is the thing to check before a number is quoted across two.

What follows from that is a rule about membership. Asked from one member's own floor, agreement is not even
symmetric: a fine-floored member reads a neighbor as different while the neighbor reads it as the same. Taken
at the coarser of the two floors it is symmetric and still not transitive, and `utils/maint/engine/order_check.py`
shows three members where the first agrees with the second, the second with the third, and the first with
neither. Pairwise agreement therefore names no set, and which members share a stem has no answer that does not
depend on which was asked first. A group needs one of two things written: a representative every member is
compared against, or a rule that builds the group and says which member it is anchored on. The rule is
written, with its anchor as the representative (Open 5 of the query protocol table).

**`.kdm` grows to whatever specificity a part needs.** It holds as many answers as it has: a general answer
block, and under it a map specific enough to be optimal on one device and nowhere else. A driver written by
hand is general worst case because a person writes it once and cannot write one per device. Nobody writes
these. A specific map therefore costs nothing to keep, and the general block stays as the fallback for a part
with no map yet.

## The method

**Everything is learned through the query protocol, and through nothing else.** An ask is
`[ADDRESS] -> (QUALIFIER) -> [COST] -> BIT`, put with `host_put` and read with `host_read`
(`src/cu/transpiler/lstar/protocol/query/host_entry.h`), with nothing between them and the part. No outside tool is in the loop: no
compiler, assembler, disassembler, object reader, vendor runtime or driver library. A word that went through one is
that tool's answer and not the part's. The SASS probe under `utils/test/src/cu/transpiler/lstar/interface/` and everything it calls
(`nvcc`, `nvdisasm`, `cuobjdump`, `interface_ptx_probe`, the vendor runtime) is scaffolding. It is an answer key in the
sense `precepts.h` is one: it may be read to form a question, and to check a derivation after it has run. It is
never a channel a derivation runs through, never where the work resumes, and never a place to find again what the
protocol answers. When how to reach a part is unclear, the answer is the protocol put at the part's addresses, and
never a tool that already knows.

Ask, and remember the answer. It does not get more complicated than that at any layer. The baseline is
remembered asks. A chain profile is remembered asks at every cut. The winning path is a comparison of two sets
of remembered asks. Nothing is modeled, nothing is predicted, and a known entry is never thrown away.

**Gate, then rank. Never one score.** A relation holds or it does not, and that answer carries no noise. A
cost is measured and every cost carries noise. The two do different jobs and are never added together.
Precepts filter the candidates down to the admissible ones, and cost orders whatever survives. Put both in one
number and a cheap wrong arrangement outranks a correct slow one, with nothing in the result saying which kind
of agreement won. Kept apart, measurement noise can cost speed and can never cost correctness, because wrong
was excluded before anything was ranked.

How many arrangements survive the gate is itself a reading and is kept. One survivor means the precepts decide
that operator. Many means they do not, and the answer to that is another relation, never more measurement.

**A precept is a question to put, never an answer to write.** Asking a target whether `1,1 -> 2` holds in its
definition is the loop working. Reading what the answer should be and writing it into the `.krs` is the loop
lying to itself, and both look like using the precepts. `precepts.h` and `word_web.h` are answer keys. They
may be read to form a question, and to check a derivation after it has run. Nothing that derives may read them
to fill a form in.

A count of precepts held is not a score either, and the reason is separate from the one above. Put five cases
to the ladder's candidate set and one of them decides it on its own; the other four are surplus, and every one
of the five is then implied by the rest. A count over a set like that weights one fact several times, at
weights nobody set. Check the set down to its deciding subset before any count is taken off it.
`utils/maint/engine/order_check.py` does that mechanically and wants running whenever a case is added.

**An answer holds only under what it was asked at.** A reading is an answer for the part it was taken on and
the size it was taken at, and for nothing else by default. A foundation that carried three stories is no
foundation for a tower, and it is not a floor of some other building either. Both transplants are priced in
`order_check.py`: the arrangement winning at one size costs 79 times the best at a larger one, a winner spliced
onto another part costs 3 times that part's own best, and across both at once the penalties multiply. Some
parts agree and some do not, and no reading taken on one part says which. So every answer carries the part and
the size beside it, and a reader outside either has nothing and has to ask. The general block in `.kdm` is the
fallback for a member with nothing measured, and never a result borrowed from a member that has.

## Functions on every part

**The engine asks the part and builds the code that answers.** No `.g`, no `.gsm` and no gnascor stands between a
function and the part that runs it.

**Tessera is the boundary between host and device.** Every process that crosses it is identified there by its
Merkle DAG ID, the seal over its contents: tessera knows every process. Device code never calls the operating
system. A file, a socket, a process or a clock is asked for through tessera and answered on the host.

**The transpiler emits the device code.** A program it emits is built from any form `L*` has learned for a part,
and it can do anything that part can do, branches and loops included wherever the part has the forms. The record
machine's straight-line program is one kind of emitted program and not the limit on them.

**Every function is in every container.** `c/`, `cu/` and `python/` are one container per host entry point, and
every function is in all three under one name (`TREE_LAYOUT_PLAN.md`). A function a container lacks is not ported
by hand: the engine emits it for that container's part and holds it 1:1 against the original, the same inputs and
the same answers. A function that calls the operating system reaches every container through tessera. The part of
it that computes is emitted, and the call crosses at tessera.

The parts of this that are built, the loop asked of the part, the read-back with no disassembler and the
branch distance, are filed in the engine table (M24, item 14).

**When an ask fails, the ask is the problem.** The ruleset defines itself, and an ask that cannot find the answer
bounded the question somewhere. The same small problem is asked again with that bound found and taken out.

## How this is worked

Build the compiler and run it live. A test that takes forty minutes is not a development cycle and is not to be
run. The device is the first target because it is the hard one; every other language falls out of a compiler that
works there.

**The transpiler is never made a compiler.** It reads a program through one ruleset and writes it through another,
form for form, by the place each form holds in the schema. Nothing is added to it that a compiler's back end does:
no pattern read off source text, no type promotion, no folding by heuristic, no instruction chosen by hand, no pass
that rewrites what a form wrote. What the transpiler does not know is asked of the part through the query protocol
and written into the files, and never written into the transpiler as a pass.

**Files separate concerns instead of data.** Each file holds one concern, and its stem names the concept or thing:
`<target>.krs` is how to write the target; `<part>.krs` is the part's operations, its base operations, its chains
through a register's limbs and every operation the part has of its own; `<part>.kdm` is the device map, the part's
registers, widths, loads, stores and pipes; `<target>.ksc` is what the system said, its classes and its folds. No
operation is defined twice, and no form's name defines an operation again for a width or a sign.

**An entry names its relation on its own line.** A reader reads a file as a dictionary of entries: no entry waits on
another, no file is read before another, and no line's place in its file decides what it means.

**Absurdities are found in layers, and the layers are a web.** Each file is read alone first and its absurdities
found there. Then the relations between the files a target reads together, then the whole set against the schema,
then the map from one target's set to another's, each layer built on the one before. The first look at a set is an
experiment: the files loaded in Python and held against each other until the absurdities show. What the experiments
find is then the reader's own logic (`ruleset_core_relation.h`), which reads any combination of files, knows no
target, and holds no container of the standard library: its memory is the caller's, sized from the files.

**A relation is witnessed or open.** A relation is witnessed where every set holding its entries holds it, and it
is then a verdict on whatever part of a set is read. A relation of absence holds only of a whole set, and no set is
known to be whole: it is open, and never a verdict.

**The query protocol is taught the absurdities.** The text witnesses that two names are one operation, and only the
part witnesses that they are two. What the text leaves open is put to the part as a slot's forms are, in the
descent's order: the first case apart closes a pair as two operations, and a pair alike on every case asked stays
open with its count of cases (Q19, P10 in the query protocol table). Its questions and answers are written to
`Lstar.klq`, and each language's operations read off them to that language's `.klm`. A finding goes into the query protocol or into
the files. It never goes into the engine's tooling under `utils/maint` or `utils/test`, which reads the engine and is
not where the engine learns.

**A check runs once, when a piece of work is done.** Not while it is being written, and not again until something it
reads has changed.

## Open

1. **The device engine and this plan disagree where the list below says.** Every file of `src/cu/engine`,
   `src/cu/engine`, `src/cu/transpiler`, `src/cu/types`, `src/cu/includes` and the bootstrap, codegen, cubin, emit and
   interface folders of `src/cu/transpiler` is read against this plan, `gnascor.md`, the engine table and the query
   protocol table. `archive/src/python/engine`, `src/cu/types`, `src/cu/includes` and the host qasm are read with the other
   engines. Each entry is fixed in the code or in the document it contradicts.
   - Named here and not built, or built and not called.
     - The run channel. `run_channel.h` declares `run_channel_open`, `run_channel_ask`, `run_channel_close` and
       `run_channel_carrier`, and nothing defines one of them. `tessera_run` defines a `run_channel_open` and a
       `run_channel_close` that take other arguments. The channel written under these names cannot link beside it.
     - No ask reaches a target. `query_ask`, `query_order_put` and the sweeps run on the host alone.
     - `period_read` is not the stride's source: `bus_enum` reads the stride off the kinds alone.
     - `engine_lattice_bits` has no caller, and the noise root box is priced only where a caller passes a cost.
     - `max_tree_probe_levels`, `max_tree_nodes`, `max_tree_pairs` and `max_tree_overlap_sums` are called by tests
       alone. `max_tree_slide` and `max_tree_overlap` are called only from `slide_score.cu`, which nothing builds.
     - The device tower runs 5/3 alone. A named lifting ruleset reaches the record floors and not `tower_lift`.
     - No engine route calls `sass_assemble`, `cubin_write`, `cubin_safe` or `sass_target`. The cubin path is reached
       from `utils/test` and `utils/maint` alone.
     - `crc.h`'s `CRC_TABLE_DEVICE`, `CRC_ADVANCE_DEVICE`, `crc_pixel`, `crc_apply`, `crc_finish` and its segment
       constants have no caller.
     - `src/build_engine.sh` names 51 module folders that are not in the tree (`engine/compiler/cycle`,
       `engine/nbody/max_tree`, `engine/formats/zarr` and the rest). Its `.cu` glob passes over a missing folder
       without a word; its portable `.c` loop hands a pattern no file matches to the compiler.
       `examples/cell_tracking/build_engine.sh` carries the `c/...` folders that are, and stops at
       `engine_internal.h`'s `codegen_device.h`, whose folder its include path does not carry.
       `examples/cell_tracking/build_driver.sh` carries it.
   - Against the method or the engine's boundary.
     - The engine and the transpiler include each other. `engine_internal.h` and `cycle_shared.h` include
       `codegen_device.h`, `asm_printer.h`, `c_target.h` and `ptx_target.h`; `asm_printer_internal.h` includes the
       engine's cycle, keymath and key_schedule; `EngineModule` names QASM and INTERFACE. No module reaches another.
     - The engine names cells. `schedule.cu` writes `cell_tracking.program`, `spiral_table.h` holds the cell table's
       1182-step spiral, and `track_driver` sets the globals `g_survey` and `g_schedule_path`.
     - Numbers written into the machine: the noise detector's bins and windows from one data set's transfer curve in
       `compression_table.md`; the Rice block, `k` width and escape; the tessera and qasm job times.
     - `tessera_core_wants` reserves the more of standing plus declared and the kept peak, the rule the scheduler
       document says is not approved. `tessera_core_remember` keeps the last run's peak and not the most: one light
       run lowers the next reservation.
     - Modules outside the engine's shape answer a bare -1 or an int with no `EngineError`: `body_overlap`,
       `heaviest_matching`, `shift_agreement`, `golden_bands`, `residual_survey`, `climb_machine`, `schedule` and
       `radix_keys`. orior and render keep their own house style. `container_write`, `container_layout` and
       `sass_assemble` give their reasons by `printf`.
     - Six files pass 500 lines: `sass_assemble.c` 1002, `sass_machine.c` 550, `cycle_compile.cu` 524,
       `code_generator.cu` 523, `cycle_record_launch.cu` 511 and `cycle.c` 508.
   - Documents the tree contradicts.
     - CRC-64 guards `EngineProgramBlock`, the `.oapx` and the `.bapx`; the seal guards the `.kcr`.
       `compression_tower.md`'s byte table for 44b6_0113de3b lays a CRC-64 word in the `.kcr`, and the table is
       mended by measuring that file again.
     - `file_types_table.md` has no `.kdm` or `.ksc` and holds `.ksh` and `.ans`, where this plan's six are `.kcr`,
       `.krs`, `.kcs`, `.knf`, `.kdm` and `.ksc`.

2. **`.krs` has no derived half.** Five are written. None can be completed by asking. A partly written one is the
   normal case and not a failure.

3. **The answer keys still hold the weight.** `precepts.h` holds 19 precepts and `word_web.h` 19 words, both typed.
   `sm_86.khw` is one run's output read back as an input. These are for checking a derivation against. Nothing
   that derives may read them.

4. **The relations are not asked for everything.** An atomic add has no relation put for it. `global_add_atomic_word` waits on
   that, and not on a name a disassembler will not print.

5. **VHDL is a target on the Pi**, built on the `cell_tracking` branch at `bbc464b`, off main. State forms cut the
   program into clock states and `vhdl.krs` writes a clocked entity. In progress, uncommitted, and the device
   writes where the host refuses.

6. **Not proved.** Of the matrix's 57 suites, 43 hold every check on the current tree: daemon, web_check, interface,
   interface_sass, interface_ptx, ruleset_read, cubin_safe, record_host, record_c, codegen_device, engine_c,
   exact_divide, exact_transform, max_tree, device_pool, period, python_period, python_periodic_energy, double_fields,
   obsignatio, qasm, record_sum, vhdl_construction_set, record_bitwise, record_coherence, record_divide,
   record_gaussian, record_guide, record_lane, record_speed, record_order, record_table (16 checks), record_tower (210
   checks), the four parts of record_boundary, and the C builds of bitwise, coherence, divide, gaussian, guide and lane.
   record_boundary is cut into parts a suite each, named by `RECORD_BOUNDARY_PART`, each part drawing from its own
   seed and its lines written as it ends: written, read_off, top and counted_forward together (50 checks), floors,
   counted_inverse (8) and redundant (16). floors holds the 1992-step program whose PTX frame passes the stack
   limit, and NVRTC compiles its C source in 934 s once; the cache holds the cubin after. The C builds of
   record_boundary's four parts have not run. 10 have no result: record_vhdl, the C pairs of speed, table, tower
   and order, residual_odd, shift_agreement_hold, tessera_device, tower_edge and unit_sweep_planes. The C build of
   order writes a 12293-step program as 17 MB of C source in one function, and the host's compiler does not finish
   it inside the harness's 1800 s. interface_sass puts its asks against the machine file the tree
   holds, 63 checks, 0 failed: 36 questions in the part's own code answer as each says, and 26 of the 27 kernels
   written again answer as the toolchain's did. The 27th, wide_div, calls the toolchain's division and is held off
   the part. The three codings weighed run in a loop and are held off the part, and the clock reads nothing. With
   `SASS_LEARN` set it learns the machine again through the disassembler, bit by bit, and prints nothing the harness
   sees for more than 1800 s: the harness ends it. It alone
   puts cubins our own assembler wrote on the part, and every one is read on the host first: `cubin_safe`
   (`src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe.{h,c}`) holds each instruction a kernel reaches to the safe scheduler word, to
   no branch and no wait, to one instruction at most that no form holds, and to an EXIT every thread takes, and
   both `interface_sass_run` and `interface_sass_probe` refuse a cubin that breaks a rule before the driver sees it.
   `src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/cubin_safe_check.sh` holds the gate to one case a rule, 15 checks, 0 failed,
   and finds 105 of the 106 cubins a fields run left safe, the one refused holding no code section.
   `interface_sass_fields.sh` puts 7360 turned-bit cubins over the 75 forms to the part through the gate in 26
   minutes: the gate refuses none, no pass hangs, and the 590 bits whose key holds a branch or a wait are skipped
   before a cubin is written. A loop ask
   branches by its nature and is held off the part until a rule says when a loop ends.

7. **One function of 132 runs on the device.** `src/cu/types/integerfloats/double_fields/double_fields.cu` holds
    `double_fields.c`'s four functions as one record program, encoded, laid out and loaded by the calls
    `engine_record_encode` makes, swept on the device and run on the host. `double_fields_test.cu` holds it 1:1
    against the C on 4110 lanes, the edges of a double and 4096 drawn words, with merges past every mask: the
    device equals the host word for word, both equal the C on every lane, and a mask one short fails 2049 lanes
    of the exponent and 2056 of the merge. The program is written from the C by hand, and deriving a function's
    program from the function is not built. The record program reaches the part through NVRTC and nvJitLink,
    scaffolding until the channel in Open 1 of the query protocol table carries it. The other 131 functions that compute and the 47 that call
    the operating system are rows in `TREE_LAYOUT_PLAN.tsv`, listed by
    `utils/maint/engine/tree_layout_check.py --write`.

## Pending Doug

## Roles
- Theorist writes the engine table and posits. Send it every hash and measured number.
