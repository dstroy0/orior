# The CASMI workbook

**Purpose:** One place where every idea the CASMI program rests on is written beside what stands behind it. A reader then knows at a glance which claims are proved, which are measured, which are built but unmeasured, and which are still only theory.
**Scope:** `workbooks/casmi/`. The program is `examples/chemistry/casmi/`, and the machine it runs on is the engine workbook, `workbooks/engine/`.

## How an entry is kept

Every claim carries one status, and the status says what backs it:

| status | what it means |
|---|---|
| **proved** | an exact check ran over the whole set and found no exception; the count is given |
| **measured** | a number was taken on named files; the number is given, and whether it held up |
| **built** | the code exists and runs; no measurement yet says whether it helps |
| **theory** | stated, not built |
| **refuted** | measured, and the measurement went against it; kept, with the number, and not tried again blind |
| **not so** | fails on its own arithmetic or in any machine, before anything is measured; kept, with the reason |
| **not taken** | a technique weighed and set aside, with the reason it is not used here |

A claim changes status only when a run changes it, and the run is named. Nothing is deleted. An idea that failed stays, with the number that failed it.

## Entries

| file | what it holds |
|---|---|
| [casmi_problem_table.md](casmi_problem_table.md) | the CASMI problem part by part: the chemistry and scoring, what the program does for each part, what it wants, every hypothesis tried with its result, the ranking's algebra, and what the program asks of the engine |

## The files it runs on

| file | what it is |
|---|---|
| `train.parquet` | 2,539,608 reference spectra in 21 row groups, ZSTD with dictionary encoding; the known structures, used whole as the validate set |
| `test.parquet` | 1,213 query spectra of 400 molecules in 1 row group, Snappy; the molecules to rank |
| `train_slice.parquet` | a slice of the train file, a fast validate for a change before the full run |

## The ledger

Every measurement of the ranker and the ingest, in the order it was taken, with its files and its result. A validate run ranks the train file's molecules against the train file; a test run ranks the test file's molecules against the train file and writes the submission.

| run | on | result |
|---|---|---|
| slice validate, parquet-direct ranker | `train_slice.parquet` | MRR@25 778,263 ppm; places 173 30 10 5 6 2 0 1 0 1 1 1 … with 19 not listed; 14.4 s wall, device peak 1.07 GB |
| full validate, parquet-direct ranker | `train.parquet` | MRR@25 883,984 ppm; places 205 22 9 1 4 2 0 1 0 2 1 0 0 1 1 0 1 … with 0 not listed; 2 min 42 s wall (was about 40 min); read 100 s, match 19,429,790,476 lanes in 44 s; device peak 2.0 GB |
| test, parquet-direct ranker | `test.parquet` against `train.parquet` | submission byte-identical to the earlier one; 400 molecules, 0 given C; match 62,772,230,394 lanes in 131 s; device peak 5.26 GB, over the 2.31 GB declared |
| test, sweeps in pieces (whole-upload cause found) | `test.parquet` | submission identical; device peak 2.315 GB, still 8 MiB over the declaration; the norm sums' first pass uploaded each member's records whole |
| test, group sums padded with one shared zero | `test.parquet` | submission identical; device peak 1.90 GB, under the declaration, but the shared zero made each piece one lane and each lane upload over 1 GB; the norm sums took 92 min; **refuted**, replaced |
| test, group sums as three planes with no index | `test.parquet` | submission identical; device peak 1.89 GB; 4 min 29 s wall |
| slice validate, after the sweep fix | `train_slice.parquet` | MRR@25 778,263 ppm, places unchanged; device peak 1.07 GB |

## The hypotheses

Every design decision weighed, with what decided it.

| # | the idea | what decided it | status |
|---|---|---|---|
| **H1. Read the file directly** | The ranker reads the three double columns and seven text columns straight from the parquet file through the engine's decoders, and computes on the stored bits. | The full validate fell from about 40 min to 2 min 42 s, with the MRR@25 and every place unchanged. | **measured** |
| **H2. Compute on the stored bits** | Every double is its 64 stored bits, read as m · 2^(E − 1075), and every program works on m and E with exact wide-integer arithmetic. | The ion sums match the engine's own C sum on every formula (0 differ); the device matches the host on every lane of every stage checked. | **measured** |
| **H3. No crystals for ranking** | The ranker needs no crystal of its own: it reads the file and processes the stored bits without unzipping into forms. | The parquet-direct ranker ranks the whole train file in 2 min 42 s with no crystal built. | **not taken** (crystals are not read for ranking) |
| **H4. Intensities on the row's least power of two** | Each intensity is kept as m · 2^s, s its exponent above the row's least nonzero exponent; the row's common power of two cancels in the cosine. | The cosine score is a ratio M²/(QR) in which the row's power of two divides out of both sides. The score run matches the host on every lane. | **measured** |
| **H5. The match kept on the device** | The match builds its index, runs its program, counts and sorts its kept lanes and gathers them on the device, and copies back only the kept lanes. | On `test.parquet` 62.8 billion lanes reduce to 8,859,804 kept lanes on the device; only those cross back to the host. | **measured** |
| **H6. Sweeps in pieces within a byte budget** | Each sweep runs its lanes in pieces of at most 1 GiB, uploads only the records its lanes read, shares one copy among members passed the same records, and lays the group sums as three planes read with no index. | Device peak fell from 5.26 GB to 1.89 GB on the test run, under the declaration, with the submission byte-identical. | **measured** |
| **H7. Ingest the whole file** | `--ingest` seals every member of the parquet file (the footer and each leaf of each row group) as a crystal of a set through the engine's ingest, nothing dropped and nothing reshaped. | The engine's parquet source seals each member as the stored bytes and rebuilds it lane for lane; verified on the host against the casmi reader (test 13 members, train 379 members). The device run is pending. | **built** (device run pending) |
| **H8. Pick columns into forms before the math** | Read a chosen subset of columns into layouts of the ranker's own and seal those. | This reshapes the data the program is held not to reshape: it drops columns and organizes the file into forms before the math. | **not taken** |

## The rules the program follows

| rule | what it means |
|---|---|
| **The file is the data** | The program reads every value as the stored bits of its double and does not reshape the file before the math. |
| **Exact, nothing chosen** | Every value is an exact integer and every verdict a sign the device returned; no threshold, tolerance or floating-point step enters. |
| **The engine's ingest is the engine's** | The whole-file ingest and its parquet source live in the engine; the program calls them and does not change them. |
| **The device does the sweep** | Every stage over many values is one record-machine sweep, a job on the tessera daemon, built on the host and run on the device. |
