# The algorithm

Measure how far something sits from the most disordered arrangement of its own parts. (This is Shannon's information entropy.)

That is the basic idea, and everything else is built from it.

Every subject asks that same question. What changes is what counts as a part:

- atoms in a cell
- symbols in a corpus
- bytes in a file
- coordinates in a board layout

The reference is built from the object's own parts. Nothing is estimated in advance. No training set, model or neural network stands in for the subject.

Building the reference this way, by making entropy as high as the object's own constraints allow, is Jaynes's principle. How far the object sits from that reference is the free energy above equilibrium.

## The six parts

The basic construction, called Identity:Null Permutation, runs through all six parts. In order:

1. Write the object as points that carry values.
2. Choose how to group those points.
3. Build the maximum entropy reference that grouping allows.
4. Measure how far the object sits from that reference.

The sift and the oracle sit on either side. The sift throws out candidates, and the oracle brings in an answer from outside the sample.

| part             | what it does                                                                                       |
| ---------------- | -------------------------------------------------------------------------------------------------- |
| `representation` | any domain written as points carrying values, and the re-seatings that put one symbol in one place |
| `partition`      | the unit(s) and the scale those points are read at                                                 |
| `reference`      | the maximum entropy background under the constraints the object supplies                           |
| `measure`        | the departure from that background                                                                 |
| `sift`           | the sound filter, a necessary condition over any index set                                         |
| `oracle`         | agreement with ground truth that somebody else published                                           |

Each part is a name you can import. They are listed in [`archive/src/python/manifest.tsv`](https://github.com/dstroy0/orior/blob/main/archive/src/python/manifest.tsv), along with `instrument` and `render`. Each row gives the name a program imports and the path under `archive/src/python/` that provides it. A program adds `archive/src/python/` to its path and imports `manifest` first.

Only `representation` knows what the object is. Everything after it sees just points and values, and one instrument reads every subject. Seven subjects have their own directories under `representation`: `atom`, `game`, `particle`, `picture`, `sound`, `structure` and `text`. `representation.constants` works out the natural constants, each one two different ways that have to agree.

[`archive/src/python/README.md`](https://github.com/dstroy0/orior/blob/main/archive/src/python/README.md) is the map. [`examples/`](https://github.com/dstroy0/orior/tree/main/examples) runs the same six parts from start to finish on real data, with one stage directory per part.

## No cutoff, no tuning

The method never picks a tolerance, a threshold or a setting by judgment. Picking a cutoff to make a result come out the way you expected is the mistake this work is built to avoid, and it isn't allowed anywhere in it.

- **Every comparison uses exact whole number arithmetic.** The engine holds no floating-point values, and that leaves no rounding to hide a chosen cutoff in.
- **The baseline comes from a shuffle of the object's own parts.** It is never worked out from a formula that someone could tune.
- **Where a reading seems to need a cutoff, every cutoff is tried.** The result is reported across the whole range, as a curve.
- **Every negative result has a positive control running next to it.** A negative result with no positive control hasn't measured anything.

A number picked to make a result come out right is not a measurement. This work contains none, and a contribution that adds one won't be accepted.

## The detector and the measure are not the same reading

The engine has many readers, one per file under [`archive/src/python/engine/analysis/measure/`](https://github.com/dstroy0/orior/tree/main/archive/src/python/engine/analysis/measure) and [`archive/src/python/engine/analysis/reference/`](https://github.com/dstroy0/orior/tree/main/archive/src/python/engine/analysis/reference). The examples run each one on real data. Two of them get mixed up more than any others, and reporting one as the other is a mistake.

- **The shift agreement detector** finds a period or an offset. It looks at how often the object matches itself when shifted.
- **The permutation null measure** finds how far an object sits from a shuffle of its own parts. Its bit form finds the exact patterns that survive the shuffle.

A number from one is not a number from the other.

The workbook records, row by row, what each reader has and hasn't been shown to do, including the rows that failed. Read it before you quote any reading. Only one reading has been checked against an answer from outside this work: the crystal cell edge, described under [Areas of research](research.md).

## What it knows

Nothing. There is no model, no training, no set of examples to learn from, and nothing assumed in advance. It knows nothing about language, chemistry or images, and it never learns anything. It computes one distance, and every result comes from that.

It runs in human time: seconds on a laptop, against a database someone else published. It can be pointed at anything, because it assumes nothing about the subject. All it needs is an object that isn't already at maximum entropy. Every single reading has a limit, and each one comes with a stated floor. If a reading doesn't clear its floor, the honest answer is that nothing was found. [Reading the result](usage.md#reading-the-result) explains what a floor is.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
