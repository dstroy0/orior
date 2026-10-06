# Using it

## The shortest way to start

Every example takes the path to your data and prints what it found. There is nothing to set up first and no model to train.

```sh
python examples/any_corpus/4_measure/head_and_tail.py yours.sym
```

A `.sym` file has one symbol per line. Any sequence works: characters, words, byte values, note numbers, residue names. The measure never learns what a symbol means, and it can't tell one subject from another.

If you run an example with no argument, it prints how to use it and stops.

## The six parts

| part | what you call it for |
|---|---|
| `representation` | turn your object into points carrying values |
| `partition` | choose the unit and the scale to read at |
| `reference` | build the maximum entropy background the object's own constraints allow |
| `measure` | read the departure from that background |
| `sift` | filter candidates with a necessary condition |
| `oracle` | check against ground truth somebody else published |

Only `representation` knows what kind of thing your object is. Under it are `atom`, `constants`, `game`, `particle`, `picture`, `sound`, `structure` and `text`. Everything after it sees only points and values.

[`archive/src/python/README.md`](https://github.com/dstroy0/orior/blob/main/archive/src/python/README.md) is the map.

## Where everything else is

| | what it operates on |
|---|---|
| [`src/`](https://github.com/dstroy0/orior/tree/main/src) | points and values, no domain. The engine |
| [`evidence/`](https://github.com/dstroy0/orior/tree/main/evidence) | the claims. The proofs, and the R and MATLAB ports |
| [`examples/`](https://github.com/dstroy0/orior/tree/main/examples) | a corpus, through `src/`. Numbered demonstrations |
| [`utils/test/`](https://github.com/dstroy0/orior/tree/main/utils/test) | the engine. The correctness checks in `utils/test/src/`, laid out as `src/` is, and the published test vectors |
| [`utils/bench/`](https://github.com/dstroy0/orior/tree/main/utils/bench) | the engine. The benches: how fast it is |
| [`theory/`](https://github.com/dstroy0/orior/tree/main/theory) | the research papers, and the ledger they cite |
| [`utils/maint/`](https://github.com/dstroy0/orior/tree/main/utils/maint) | the repository itself. Records, gates, prose checks, fetchers, the research paper build |

`utils/maint/` is sorted into categories, with no loose scripts. [`utils/maint/README.md`](https://github.com/dstroy0/orior/blob/main/utils/maint/README.md) says what goes in each one. For example, `utils/maint/data/` holds outside material, and `utils/maint/analysis/` holds the surveys the research papers ask for.

## Reading the result

Every measurement comes with a floor. The floor is what the same measure gives on a shuffled copy of the same symbols, a version with no structure at all.

**A reading below its floor found nothing.** That's why every example prints the floor next to the number.

The floor changes with the size of the sample. A floor worked out on a large body of text tells you nothing about a short file. The examples work out the floor at the size actually measured.

## The twelve subjects

`examples/` runs the same six parts from start to finish on real data. The proofs behind the ledger are kept separately, in `evidence/proofs/posits/`.

| domain | examples | what it reads |
|---|---|---|
| `language` | 60 | corpora, orthographies, dialect borders |
| `any_corpus` | 19 | any symbol sequence, domain unspecified |
| `game_theory` | 16 | games, by how open the result stays after a move |
| `proteins` | 13 | backbone coordinates |
| `particle_physics` | 11 | atoms and particles as exact quantum numbers |
| `crystallography` | 9 | cell edges, against published ones |
| `art` | 9 | images as byte sequences |
| `cell_tracking` | 7 | cell positions across frames |
| `chemistry` | 5 | molecules as atoms and bonds |
| `source` | 4 | source code as a symbol stream |
| `sound` | 3 | recordings as bit fields |
| `molecules` | 3 | molecular formulae, legal from illegal by valence |

Every example has a catalog number at the top of the file, such as `LNG-4-012`. If you cite that number, the citation still works after the file moves. [`utils/maint/catalog/catalog.py`](https://github.com/dstroy0/orior/blob/main/utils/maint/catalog/catalog.py) keeps the list.

## The search kernel

`anchor_steer_count` counts how many times a needle (the pattern you search for) appears in a corpus (the text you search in). Its last argument is 1 to order the probes by rarity, or 0 to keep them in the order they appear. The count is the same either way ([`orior_descent.h:342-368`](https://github.com/dstroy0/orior/blob/main/src/cu/engine/nbody/orior/orior_descent.h#L342-L368)). The function borrows both buffers for the length of the call ([BORROWS]).

```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "orior.h"

int main(void)
{
    const char *corpus = "abracadabra abracadabra";
    const char *needle = "abra";
    const size_t corpus_len = strlen(corpus);
    const size_t needle_len = strlen(needle);

    const size_t count = anchor_steer_count((const uint8_t *)corpus, corpus_len,
                                            (const uint8_t *)needle, needle_len, 1);
    printf("%lu occurrences, scanned by the %s engine\n", (unsigned long)count,
           anchor_steer_best_engine()->name);
    return 0;
}
```

Built with the gcc line in [Setup](setup.md#the-c-engine), under gcc on x86-64 Windows, it prints `4 occurrences, scanned by the portable engine`.

The sift is a safe filter: no arrangement of anchors can lose a true occurrence.

- **It can only be wrong in one direction.** Any mistake is an over-count, never a missed match.
- **Its memory is small.** It keeps `m` bits of state for a pattern of length `m`, with no table over the alphabet. An alphabet of real numbers, or one too large to list, costs it nothing extra.

The Python in [`archive/src/python/engine/nbody/orior/sift/`](https://github.com/dstroy0/orior/tree/main/archive/src/python/engine/nbody/orior/sift) builds the same thing and shares no code with the C version. The two are checked against each other by making sure their counts agree.

## Other languages

| language | file | status |
|---|---|---|
| R | [`evidence/sims/r/departure.R`](https://github.com/dstroy0/orior/blob/main/evidence/sims/r/departure.R) | runs, checked against the reference |
| MATLAB and Octave | [`evidence/sims/matlab/orior_departure.m`](https://github.com/dstroy0/orior/blob/main/evidence/sims/matlab/orior_departure.m) | run on Octave 11.3.0, inside the reference floor; MATLAB proper not run here |

Each language draws its shuffle from its own random number generator, and no two agree to the last digit. A port counts as correct when its result lands inside the reseeding floor of the Python version.

This was checked on 200000 symbols over twelve seeds:

- A clustered sequence reads 0.4228 in Python and 0.4282 in R, against a floor of 0.0092.
- A memoryless sequence reads 0.9953 and 0.9933, against a floor of 0.0044.

Both gaps come to about half a floor.

## If you are working on a language

Read [the conditions of use](condition_of_use.md) first. These tools can generate language. Output near the edge of what the source covers can read smoothly and still not be the language. Nothing here marks which side of that line a result fell on. A person must review the output. That review is a condition of use.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
