# Areas of research

Twelve subjects have pipelines under [`examples/`](https://github.com/dstroy0/orior/tree/main/examples), each run in stages.

- **Run end to end, and the results agree:** language, art, crystals, proteins, sound, source code and any body of text.
- **Being brought to the same standard:** chemistry, game theory, cell tracking, molecules and particle physics.

The proofs behind the numbers are in [`evidence/proofs/`](https://github.com/dstroy0/orior/tree/main/evidence/proofs). The six parts of the method also check each other from end to end, and they agree.

## What came back

**A crystal.** A crystal comes with an answer that someone else wrote down before this tool existed. Nothing else in this work can be checked that way. The test works like this:

1. Tile a published unit cell.
2. Store every atom site as an exact whole number.
3. Score how well the layout matches itself when it is shifted along one axis.

The shift with the best match is the edge of the cell. Across 453 axes from the Crystallography Open Database, every edge found equals the published edge as a whole number. No tolerance is allowed, and there is no error to average away. [Crystallography](https://github.com/dstroy0/orior/tree/main/theory/theory/crystallography)

**A dialect border.** The tool found a border it was never shown. Mellesmoen and Kye label every word form they cite as northern or southern Lushootseed. The tool was given the forms but never the labels. The feature it found to carry the border is the stressed schwa, which is southern: 45 against 6 over the same concepts. Only 1 of 200 random borders did as well. [Salishan](https://github.com/dstroy0/orior/tree/main/theory/theory/Salishan)

**A game.** On subtraction games, the detector finds the Grundy period in 383 of the 383 rows it can score. Each answer is checked against periods worked out by a separate exact routine, and the closest call still clears by 16 floors. On a sequence with no period at all, the same detector still gives a confident number. What it is reading there is the continued fraction of the sequence's slope. [Game Theory](https://github.com/dstroy0/orior/tree/main/theory/theory/game_theory)

**The periodic table.** The exclusion principle shows up here as a count. The same tool, which knows nothing about the subject, finds two elements on one crystal site and finds two electrons in one state. Across all 118 elements it finds no two electrons in one state. The row lengths, 8, 8, 18, 18, 32, 32, come out as the gaps between the closed shells. [Particle Physics](https://github.com/dstroy0/orior/tree/main/theory/theory/particle_physics)

**A hash.** Fold the dependency matrix onto (input bit − output bit) mod 32, and 4096 cells sit behind each number. Take out what every class shares, and the band left standing is residues 0, 6, 11, 25 and 31. Each one is a named operation: the diagonal, the three rotation amounts of Σ1, and −1 mod 32 for the carry. The shared part is what fades, by one thirty-second each round. After round twenty-three the fold reads 2.09, 2.07 and 2.04, against a baseline peak of 2.63. It finds nothing there. [SHA-256](https://github.com/dstroy0/orior/tree/main/theory/theory/cryptography/sha256)

**Told nothing.** None of these was told anything in advance:

- An image read as a stream of bytes gives back its own width.
- A Vigenère cipher gives back the length of its key.
- A protein backbone gives back bond lengths of 1.45, 1.52 and 1.33, where chemistry says 1.46, 1.52 and 1.33.

The workbook has the rest, including every row that failed and why.

## Where to start reading

The research is written up as twenty research papers in `theory/`. [Where to start reading](research_papers.md) lists each one and what it covers.

## What is not here

The text collections, papers, audio and rendered pages come to about 1.9 GB, and none of it is in git.

- `utils/maint/data/salishan/get_papers.py` fetches the papers from their archive.
- [`utils/maint/data/fetch/`](https://github.com/dstroy0/orior/tree/main/utils/maint/data/fetch) fetches the other text collections.
- The tools rebuild everything else.

The hand extractions are word forms copied by hand out of published papers. Those tables are the papers' own text. This work has no right to share them, and they are not included. Everything that doesn't read a paper or one of those tables runs without them.

## A note on how this is written

1. The workbook always keeps its own corrections.
    - A claim that was withdrawn stays on the page, next to the measurement that disproved it.
    - A record that keeps only what survived is not evidence.
2. Several results turned out to repeat published work. Where that is known, the earlier work is named.
    - Citation is still in progress. Corrections are welcome, and giving credit matters.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
