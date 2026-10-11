# The CASMI table

**Purpose:** The CASMI 2026 problem part by part: for each part, the chemistry and the scoring it is held to, what the program does about it, what it wants to do, every hypothesis tried there with its result, its status, and the next move.
**Scope:** the program: `examples/chemistry/casmi/src/` and the rules in `examples/chemistry/casmi/cfg/casmi.cfg`. The machine the program runs on is a separate concern with its own table, [the engine table](../engine/engine_table.md); a row here names the machine it reads. Statuses follow [casmi_workbook.md](casmi_workbook.md). A hypothesis with no run in the workbook says so.

## The problem

CASMI 2026 on Kaggle gives the tandem mass spectrum of each query molecule and asks for up to 25 structures of it, written as SMILES. A molecule is scored on its InChIKey14, the first fourteen characters of its InChIKey, which name the skeleton. The score is the mean reciprocal rank at 25 (MRR@25): a molecule whose true skeleton is the r-th distinct skeleton in its list of 25 earns 1/r, and 0 where none of the 25 matches. The run's score is the mean over the query molecules, reported here in parts per million, floored.

A spectrum is a set of peaks, each a mass-to-charge value (m/z) and an intensity. The train file holds reference spectra whose structure is known; the test file holds the query spectra. A query's candidates are the known structures whose molecular formula fits the query's precursor mass under its adduct and ionization mode, and a candidate is ranked by how near its reference spectra stand to the query's spectrum.

Everything is exact. Every value read from a file is read as the bits the file stores it in, and no number is chosen or rounded. Every verdict is a sign the device returned from an integer, and the ranking is an order of exact integers.

## The rule the program is held to

**Read the file, do not reshape it.** The parquet files are the data. The program reads every value as the stored bits of its double and computes on those bits with exact wide-integer arithmetic. It does not extract, organize or reshape the data into forms of its own before the math. The engine's own decoders read the column chunks, and the engine's record machine does the arithmetic on the stored bits.

**Exact, and nothing chosen.** A double x is its 64 stored bits, read as x = m·2^(E − 1075): m the 53-bit mantissa, the fraction with its leading 1 where the biased exponent E is above 0, and E the biased exponent, 1 for a subnormal. A denominator is a power of two, 2^(1075 − E). A score is a ratio of exact integers, and the ranking compares two scores by cross-multiplying. A verdict takes no chosen threshold or tolerance, and no floating-point step enters one.

**The device does the sweep.** Every stage that runs over many values is one sweep of a record-machine program on the device, submitted as a job on the tessera daemon. The host builds the records and the index; the device runs the program and returns the records.

## The modules

Paths are from `examples/chemistry/casmi/`.

| module | holds |
|---|---|
| `src/casmi_driver/` | the driver: `--ingest FILE.parquet SET`, and `--rank TRAIN.parquet CFG validate` or `test` |
| `src/parquet/` | the footer (Thrift compact) and one column chunk at a time, decoded through the engine's Snappy and Zstd to the stored bytes |
| `src/ingest/` | `--ingest`: every member of the file sealed as a crystal of a set by the engine's ingest |
| `src/rank/` | `--rank`: the stored-double split, the ion and window programs, the device match, the group sums and tournaments, the score and the submission |
| `src/mass/` | the elements' masses and the formula parse |
| `src/check/` | the lane check held against an exact host read |
| `cfg/casmi.cfg` | the envelope, the query library and the adducts |

The rank module's parts:

| file | holds |
|---|---|
| `rank.cu` | `rank_run`: the stored-double split, the bounds pass, the peak layout, the ion, window, match, score and submission stages |
| `rank_file.cu` | the parquet files read into `RankSet`, every row group appended, each row checked for one precursor and as many intensities as m/z |
| `rank_match.cu` | the match kept on the device: the index kernel, the run, the kept-bit sum and sort, the gather |
| `rank_machine.cu` | the record machine as the ranker reaches it: a program loaded, a sweep run a piece of lanes at a time, a field read or written as an exact integer |
| `rank_internal.h` | the shared types: `RankSet`, `RankColumn`, `RankMachine`, `RankField`, the match device and chunk |

## The table

| part | the chemistry and scoring it is held to | does | wants | tried, and what it gave | status | next |
|---|---|---|---|---|---|---|
| **1. The file** | The data is the parquet file, every value the bits it is stored in. Nothing is dropped, and nothing is reshaped before the math. | **--rank** reads the three double columns it ranks on (`precursor_mz`, `ms2_mzs.list.element`, `ms2_normalized_intensities.list.element`) and the seven text columns it keys on, each through `casmi_parquet_column_read`, which decodes a column chunk through the engine's Snappy and Zstd to the stored bytes, eight to a value, with each row's start. Every row group of every file is appended to one `RankSet`. **--ingest** seals the whole file: `engine_source_samples` lists its members (the footer, then every leaf of every row group), and `engine_ingest_set` seals each as a crystal of the set and rebuilds it against the lanes the file gave, lane for lane. | The ranker reads only what it ranks on, directly, with no crystal between the file and the math. Ingest holds the whole file, nothing left out. | **--rank** read, **measured** on `train.parquet`: 2,539,608 spectra in 21 row groups, 401,806,233 peaks, 275,810 structures, decoded on the host in 100 s. On `test.parquet`: 1,213 spectra in 1 row group. Reading the files directly took the full validate from about 40 min to 2 min 42 s. Crystals of the ranker's own forms: **not taken**, the ranker reads the file. **--ingest** on the engine's parquet source: **measured** on the host by the engine's author, every member byte for byte against the casmi reader (`test.parquet` 13 members, `train.parquet` 379 members, 3,441,431,285 lanes); not yet seen through `engine_ingest_set` on the device. | ranker read built and measured; ingest built, device run pending | Run `--ingest` over `test.parquet`, then `train.parquet`, on the device; check the members, the sizes and the engine's lane-for-lane verdicts. |
| **2. The library** | A query molecule is a set of spectra; a candidate structure has one or more reference spectra; a structure belongs to a molecular formula; a molecule is scored on its InChIKey14. | The text columns are read as distinct strings and each row's index among them: `molecule_id`, `inchikey14`, `normalized_smiles`, `molecular_formula`, `adduct`, `ionization_mode`, `ingest_lib`. The ranker groups structures by formula and reference spectra by structure, and reads the query library and adducts from the cfg. | Every query molecule's candidates and every candidate's reference spectra, from the file's own columns, with nothing invented. | **Measured** on `train.parquet` as a validate set: 2,539,608 reference spectra, 275,810 structures, 50,764 formulas. On `test.parquet`: 1,213 query spectra of 400 molecules, 7 adducts read. | built and measured | none |
| **3. The ion window** | A candidate is admissible where its formula's ion, under the query's adduct and ionization mode, matches the query's precursor m/z within the envelope. The ion is the formula's exact monoisotopic mass plus the adduct's. | The **ion** program computes each formula's ion mass as an exact integer over isotopes (156-bit record, 5 limbs), held beside the engine's own C sum for every formula. The **window** program marks each (query, formula) pair admissible where the ion lies inside the envelope (6-bit record). | Admissibility as an exact integer verdict, no mass computed in floating point. | **Measured** on `test.parquet`: ion over 355,348 lanes, device 7–22 ms, 0 formulas differing from the engine's C sum. Window over 61,576,732 lanes, device 51–193 ms, 6,452 (query, formula) pairs inside, 0 queries holding none. Candidate structures 275,810; formulas read 50,764, refused 432. | built and measured | none |
| **4. The peak match** | Two spectra are near where their peaks line up in m/z and their intensities agree. A query peak matches a reference peak where their m/z preimages overlap under the envelope. The intensities are read on each row's least power of two, a scale every cosine score cancels. | The **match** program runs on the device (`rank_match.cu`): an index kernel builds (query peak, reference atom, mode) per lane; `cycle_record_run` runs the match (215 steps, 252-bit record, 8 limbs), whose kept bit is 1 where the pair's weight is above 0; a sum gets the kept count K; a stable sort puts the kept lanes last; a gather brings back only those K lanes' numbers and records. A match weight is `inside × m_q·m_r × two_to(s_q + s_r, shift_bits + 1)`, with s each intensity's exponent above its row's least. | Every matched peak kept as an exact weight, the device holding the lanes and returning only the kept ones. | **Measured** on `test.parquet`: 62,772,230,394 lanes, device 130–157 s, 528,123 pairs holding a matched peak, 8,816,975 (pair, query peak) groups, 8,859,804 matched lanes kept. On `train.parquet` validate: 19,429,790,476 lanes, device 44 s. The bounds pass reads 1075 − E at most 63 and an intensity's exponent above its row's least at most 41. | built and measured | The chunk takes at least one whole reference spectrum; one spectrum's peaks times a query's peaks past 2^24 lanes would pass the chunk limit. It does not happen on this data; split a chunk by query peaks if a file ever does. |
| **5. The spectral score** | The nearness of two spectra is their cosine similarity: M² over Q·R, M the sum of matched-peak weights, Q and R the sums of each spectrum's squared intensities. The row's power of two cancels: the score is scale-free. | The **peak tournament** keeps each (pair, query peak) group's heaviest weight; the **weight sums** add a pair's kept weights to M; the **norm squares** program squares each intensity (`(m·two_to(s, shift_bits))²`) and the **norm sums** add them per spectrum to Q and R; the **score** program outputs (M², Q·R) per pair (1,100-bit record, 35 limbs). | A cosine² per (query, reference) pair as a ratio of exact integers. | **Measured** on `test.parquet`: norm squares 45,947,797 lanes, device 0.33 s; score 528,123 lanes, device 8–12 ms. Device equal to the host on every lane checked. | built and measured | none |
| **6. The molecule's rank** | A candidate is as near as its nearest reference spectrum; a molecule's list orders its candidates by that nearness, best first, at most 25, on distinct skeletons. | The **structure tournament** keeps each structure's heaviest (query, reference) score by a COMPARE tournament on the exact ratio; the **list tournaments** order each molecule's structures and take the top 25. A structure with no matched peak falls to the bottom. | One ordered list of up to 25 structures per molecule, the order an exact comparison of cosine² ratios. | **Measured** on `train.parquet` validate: the places, 1 to 25 then not listed, 205 22 9 1 4 2 0 1 0 2 1 0 0 1 1 0 1 0 … 0, MRR@25 883,984 ppm. On the slice: 173 30 10 5 6 2 … 19 not listed, MRR@25 778,263 ppm. | built and measured | none |
| **7. The submission** | Up to 25 SMILES per molecule, the normalized SMILES of each ranked structure. A molecule with no candidate is given a placeholder. | **test** writes `build/results/submission.csv`: each molecule's ranked structures as their normalized SMILES, deduplicated by skeleton, padded to a known structure where fewer than 25, and `C` where no candidate or no query of a known mode. | A submission the competition scores, written only in test mode. | **Measured** on `test.parquet`: 400 molecules, 0 given C for no candidate, 0 given C for no known mode. The submission is byte-identical across runs and across the memory-fix rebuilds. | built and measured | none |
| **8. The device budget** | The job declares its device bytes to the tessera daemon, which admits it by memory headroom; a declaration that does not fit waits forever. | `rank.cu` declares `RANK_DECLARED` (2 GiB). Every sweep runs its lanes in pieces of at most `RANK_SWEEP_BYTES` (1 GiB): each piece uploads only the records its lanes read, members passed the same records share one copy, and the group sums are laid as three planes read with no index. The match runs chunks of at most `RANK_CHUNK_LANES` (2^24). | The run's peak device memory under what it declares: the daemon admits it, and no other GPU job stalls it. | **Measured** on `test.parquet`: peak device 1.89 GB against the 2.31 GB declared, after the sweep fix. Before the fix the sweeps uploaded whole: peak 5.26 GB, over the declaration. A first fix that padded short group-sum lanes with one shared zero record made each piece one lane and each lane upload over 1 GB: the norm sums took 92 min; **refuted**, replaced by the three-plane layout. On the slice: peak 1.07 GB. | built and measured | none |

## The algebra of the ranking

Every line is exact. A double is its stored bits; a score is a ratio of exact integers; a verdict is the sign of an integer.

### The stored double

A double's 64 bits are the sign at bit 63, the biased exponent E in bits 52 to 62, and the fraction in bits 0 to 51. The mantissa is m = fraction + (2^52 where E > 0), 53 bits. The value is

  x = m · 2^(E − 1075),  E ← 1 where E is stored 0 (a subnormal).

A zero is m = 0 at E = 1075, denominator 1. The ranker reads m and E from the stored bits and never forms x.

### The m/z and the denominator

A peak's m/z is m · 2^(E − 1075). Its denominator is 2^(1075 − E), computed in a program as `exact_record_two_to(difference(1075, E), two_bits)`, with two_bits the bit length of the largest 1075 − E on the data (59 on the slice, 63 on the full train).

### The intensity on its row's least power of two

Each intensity in a row is m · 2^s, s its exponent above the row's least nonzero exponent. The row's common power of two cancels in the cosine: it is dropped and only s is kept. shift_bits holds the largest s (28 on the slice, 41 on the full train).

### The match weight and the norm

For a query peak (m_q, s_q) and a reference peak (m_r, s_r) whose m/z preimages overlap under the envelope (inside = 1, else 0):

  weight = inside · m_q · m_r · 2^(s_q + s_r)

and the intensity's square in the norm is (m · 2^s)². The device match keeps a lane where weight > 0.

### The score

Per (query q, reference r) pair, with the matched set K(q, r) one heaviest weight per query peak:

  M = Σ_{K(q,r)} weight,  Q = Σ_{peaks of q} (m · 2^s)²,  R = Σ_{peaks of r} (m · 2^s)²

  cos²(q, r) = M² / (Q · R),  output as the pair (M², Q·R).

A candidate structure's score is the largest cos² over its reference spectra; a molecule's list orders its candidates by that score. Two scores a/b and c/d compare by the sign of ad − bc, exact.

### The score (MRR@25)

A molecule whose true InChIKey14 is the r-th distinct skeleton in its list earns 1/r, 0 where none of the first 25 matches. The run's MRR@25 is the mean over the query molecules.

## What this program asks of the engine

The interface between this program and the machine, each optimized on its own terms.

| the program asks | the engine offers | status |
|---|---|---|
| a parquet column chunk decoded to its stored bytes | the Snappy and Zstd decoders under `src/cu/includes/codecs/`, reached by `src/parquet/` | built |
| the whole parquet file sealed as a set | `engine_ingest_set` with the parquet source (`src/cu/includes/formats/parquet/`), its members from `engine_source_samples` | built on the engine; device run pending (part 1) |
| an exact program run over records on the device | the record machine: `exact_record` builds, `keymath`/`key_schedule` lay out, `cycle_record_run` runs, `cycle_record_sum`/`cycle_record_sort` reduce | built |
| a device job admitted by memory headroom | the tessera daemon, submitted through `sim_job_submit` | built |
