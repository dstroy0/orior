# Examples

**Purpose:** Find the script that produced a figure in the ledger, or run one corpus from raw bytes through to a checked answer.
**Scope:** `examples/`

Every script lives at:

```
examples/<subject>/<stage>/<file>.py
```

The subject says what type of corpus it reads. The stage says which step of the reading it does. So `language/4_measure/cross_corpus.py` measures text.

Every script sits at that depth, and not one counts parent directories to locate the repository. They start at their own directory and walk up until they find `archive/src/python`. A count of parent directories breaks when a file moves. Its distance from the root changes, and its imports fail. The breakage does not show until somebody runs the script.

## Stages

A corpus goes through these in order.

| stage         | what it does                                                   |
| ------------- | -------------------------------------------------------------- |
| `1_represent` | turns the corpus into points that carry values                 |
| `2_partition` | picks the unit and the scale to read those points at           |
| `3_reference` | builds the max entropy background out of the corpus itself |
| `4_measure`   | reads how far the corpus sits from that background             |
| `5_sift`      | filters candidates using the necessary condition               |
| `6_oracle`    | compares the result against an answer published elsewhere      |

Where a subject has no script for a stage, the directory is absent. That means that no script exists yet, and not that the stage does not apply.

## Subjects

| subject            | corpus                                                                 | stages present   |
| ------------------ | ---------------------------------------------------------------------- | ---------------- |
| `any_corpus`       | anything. These read a corpus without knowing what it is               | 1, 2, 3, 4, 5    |
| `language`         | written text: books, encyclopedia articles, two parallel translations  | 1, 2, 3, 4, 6    |
| `art`              | paintings, stored as bytes that are really a plane                     | 1, 2, 4          |
| `proteins`         | structures from the Protein Data Bank                                  | 1, 2, 3, 4, 5, 6 |
| `crystallography`  | published cells from the Crystallography Open Database                 | 1, 2, 3, 4, 5, 6 |
| `chemistry`        | molecules as atoms and bonds, valence as a necessary condition         | 1, 3, 4, 5       |
| `molecules`        | atoms and formulae, read for the valence structure they can carry      | 1, 6             |
| `particle_physics` | atoms as electron shells, Standard Model particles as quantum numbers  | 1, 2, 3, 4, 5, 6 |
| `sound`            | animal and human vocalizations                                         | 1, 3, 4          |
| `source`           | programming languages, assembly, board layouts                         | 1, 4             |
| `game_theory`      | games with their own answer key, played boards and impartial games     | 1, 2, 3, 4, 5, 6 |
| `Salishan`         | the gold standard corpus of Salishan languages, read from hand extractions | 4             |

Start with `any_corpus`. Those scripts do not know what they are reading, and the rest of the work rests on that claim. Each other subject runs the same steps with domain knowledge added at stage one, and some of them can check the answer at stage six.

Six directories sit outside the subject and stage layout. `0_experimental` holds work that does not yet fit a subject or a stage. `00_blob_viz_tools` is the viewers, a Python generator plus an HTML template each, documented in its own README. `proofing` is the precision work the ledger rests on, the pi-digit reading and the device and host arithmetic engines. `cell_tracking` is a full implementation with its own build scripts, configs, source and tests, not a walk through a corpus. `qasm` is the exact qubit states read from OpenQASM, with its own build scripts, source and tests.

`miner` is the SHA-256 stratum miner on the CPU and the device, documented in its own README.

The proofs of the posits the ledger cites are not under `examples/`. They are in `evidence/proofs/posits/`.

## Failures are retained

A reading that does not work stays in its subject, next to whatever comes after it. A read of Dravidian languages at the codepoint puts the family further apart than unrelated languages. That script stays in `language/4_measure` beside the two that repair it. Anyone who finds only the repair cannot tell what it repairs.

## Fetchers are not examples

Scripts that download or generate corpora do not sit here. They are in `utils/maint/data/fetch/`. To get a corpus is a separate job from a read of one.

## Nothing in the engine imports from here

`src/engine/` does not import anything under `examples/`.

Two scripts import a sibling from the same directory. `cluster_branch.py` uses `report` from `cluster_profiles.py`, which uses a corpus reading from `positional_ambiguity.py`. All three sit in `language/4_measure` for that reason.

## Running one

```
python examples/any_corpus/4_measure/collision_entropy.py
python examples/crystallography/6_oracle/proof_positive_control.py
```

Max need corpora under `build/`, which comes to about 1.9 GB and is not in git. `utils/maint/data/fetch/` fetches them.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
