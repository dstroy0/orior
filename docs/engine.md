# The engine

The engine is the machine every measurement runs on.

- The C code is under [`src/cu/`](https://github.com/dstroy0/orior/tree/main/src/cu), and so is its device code.
- Its simulations are under [`src/sims/`](https://github.com/dstroy0/orior/tree/main/src/sims).
- [engine_table.md](https://github.com/dstroy0/orior/blob/main/theory/workbooks/engine/engine_table.md) describes it part by part: what each part computes, the exact math it follows, what it does today, what it still needs, every idea tried, its status, and the next step.

**The engine is not tuned for any one size.** No size, spacing, order, window, width, cell count or voxel count is built into it. Each one either comes with the request or is read from the data. The engine is exact at every size its words can hold. When a word is too narrow, it says so (a request error, or `needed_bits`) and never rounds.

A sample is stored as 16-bit lanes along the axes (t, z, y, x). The whole object under examination is stored as one whole number. Every value is a whole number (in ℤ), and nothing is rounded.

| step        | from → to               | what happens                                                                                        |
| ----------- | ----------------------- | --------------------------------------------------------------------------------------------------- |
| 1 ingest    | a source → lanes        | read any source format into 16-bit lanes in key order, with its side bytes                          |
| 2 seal      | lanes → signa           | the dimensional Merkle DAG over rows, planes, volumes and lanes, and the witness against the source |
| 3 lift      | lanes ↔ crystal         | the tower lifts the lattice into the stored stream, and lowers it back exactly                      |
| 4 measure   | lattice → fields        | the residual, the moments, the entropy windows                                                      |
| 5 partition | fields → bodies         | the component tree and its cut into bodies                                                          |
| 6 relate    | bodies × frames → links | overlap, matching, motion, division, the links between frames                                       |

## The parts

| part                           | status                                                                                                                  |
| ------------------------------ | ----------------------------------------------------------------------------------------------------------------------- |
| M1. Exact numbers              | built; the engine build takes it from this tree                                                                         |
| M2. The residual operator      | proved (exact); the sweeps 2× faster; any input width proved (the planes)                                               |
| M3. The component tree         | built, error-wired                                                                                                      |
| M4. The moments                | built, error-wired                                                                                                      |
| M5. The correlation operator C | built; the box proved                                                                                                   |
| M6. The exact marginal         | built                                                                                                                   |
| M7. One-to-one matching        | built                                                                                                                   |
| M8. The entropy history        | built, error-wired                                                                                                      |
| M9. The files                  | proved: the field, and the RSNA knee as `.kcr`                                                                          |
| M10. The record machine        | built, error-wired; the table, register reuse and the division proved; 12 to 17 times the interpreter's speed compiled |
| M11. The golden ladder         | built                                                                                                                   |
| M12. Errors and integrity      | wiring in progress                                                                                                      |
| M13. The seal                  | built and proved: the crystal and the set                                                                               |
| M14. The scheduler             | ledger, frame, measure and the daemon end to end proved on Windows                                                      |
| M15. The sift                  | built and run here on four configurations                                                                               |
| M16. Render                    | built and run here                                                                                                      |
| M17. Memory operations         | built; the x86 arms proved, the aarch64 arms built and emitting                                                         |
| M18. The period reading        | built, proved against its reference                                                                                     |
| M19. The sims                  | built; every sim run recorded here passes every check                                                                   |
| M20. The root universal        | built and proved in `tower`; not in the crystal path                                                                    |
| M21. The ask and the state     | built as a sim; proved exact on the host                                                                                |
| M22. Exact qubit states        | built; exact on the host; the device code proved, 44 checks, 0 failed                                                   |
| M23. The refinement loop       | wanted, not built                                                                                                       |
| M24. The ask                   | proved: the ask on the host and in the cell, the gate, the order and its solve; theory: the run channel                 |

Each status in the engine table names the run that backs it up.

[`src/README.md`](https://github.com/dstroy0/orior/blob/main/src/README.md) explains how to use the engine. It covers:

- its calls and errors;
- one body on one lattice, and many bodies across frames;
- the record machine, with its programs and its settings;
- tessera, the single background service on each device that accepts device jobs from every process.

## The files

The k-files are the file types the compiler reads and writes.

| suffix | name                                    | holds                                                |
| ------ | --------------------------------------- | ---------------------------------------------------- |
| `.ksc` | Kolmogorov system classification        | the language map                                     |
| `.krs` | Kolmogorov information ruleset          | the coherence rules: one language's forms            |
| `.kcr` | Kolmogorov information crystal          | a set written close to the floor its own noise sets  |
| `.knf` | Kolmogorov noise floor                  | a measured noise floor                               |
| `.kcs` | Kolmogorov information construction set | what reconstructs information                        |
| `.kdm` | Kolmogorov device map                   | the hardware map                                     |
| `.g`   | gnascor high order language             | semantic plain language, plus the shortcut operators |
| `.gsm` | gnascor assembly language               | the same program with the switch thrown              |

**A file is named `<concept or thing>.<filetype>`.** The first part says what it is about, and the suffix says what type of file it is.

- `sass.krs` is how to write SASS.
- `sm_86.krs` is the operations ruleset for sm_86, and it goes with `sm_86.kdm`.
- `sass.ksc`, `ptx.ksc` and `avx.ksc` follow the same pattern.

Files with the same first part belong together, whatever that first part is. `pair.kdm` and `pair.knf` are a pair's map and that map's floor. `set.kcr`, `set.kcs` and `set.knf` are one set's crystal, the set that reconstructs it, and its floor. Only the filename ties them together, and a group doesn't have to include every file type.

**`.kcr`, the crystal**, holds these sections from front to back:

1. the head (12 words);
2. the seal (6 roots, every lane node, every chunk leaf);
3. the offsets and the stream;
4. the compressed side bytes;
5. the member tables and names;
6. EOF.

Every file the engine writes is checked by reading it back. Pixels are checked voxel by voxel against a second read of the source, and every part is checked against its seal. Integer lifting can always be reversed exactly.

The engine can read zarr v2/v3/N5 with every codec, TIFF, HDF5, npy/npz, NRRD, NIfTI, DICOM, zip and `.stack`. It can write the crystal back out in any of these source formats: the DICOM zip (byte for byte, from the side bytes), npy, NIfTI, TIFF and zarr.

## Compression

The true floor for a set of data is its Kolmogorov complexity, K(x): the length of the shortest program that prints it. K can't be computed, and it can't be measured directly. What can be measured is a ladder of bounds. Each one is exact for a named kind of coder, and each is lower than the one before because the coder is allowed to see more. [compression_table.md](https://github.com/dstroy0/orior/blob/main/theory/workbooks/compression/compression_table.md) gives the ladder for each set.

| rung | the bound                        | exact for                                                                   |
| ---- | -------------------------------- | --------------------------------------------------------------------------- |
| F0   | raw: the source's own bytes      | nothing; this is the reference every percentage is taken of                 |
| F1   | values, order 0                  | any coder that treats the voxels as a bag of values and sees no position    |
| F2   | coefficients, order 0, per floor | a coder that sees the tower's lifted coefficients floor by floor            |
| F3   | coefficients in context          | context coders: a coefficient coded given its neighbors                     |
| F3′  | a two-part code                  | a coder that stores a model and then the residue the model does not predict |
| F4   | the noise floor                  | every coder: what is left of one frame after all structure is taken         |
| F5   | the integrated noise functionals | a coder driven by the functionals' exact counts                             |

Each bound is a whole number of bits: the bit length of an exact whole number. Stating it takes no logarithm and no floating point.

| set                                    | `.kcr` bytes               | of raw | status   |
| -------------------------------------- | -------------------------- | ------ | -------- |
| RSNA knee, 1 × 34 × 960 × 960 unsigned | 25,218,496                 | 40.2%  | proved   |
| RSNA knee, 1 × 24 × 640 × 640 signed   | 11,319,416                 | 57.5%  | proved   |
| RSNA test_series, 15 of 15             | 192,020,272 of 599,191,552 | 32.0%  | proved   |
| the noise floor (F4) on the 25         | 6.229 bits a voxel         | 38.9%  | measured |

Every crystal is rebuilt exactly: voxel for voxel, pixel for pixel and node for node.

## The transforms

The engine has a large set of transforms. A transform turns the object into a different representation and, when it can be reversed, turns it back. All of them use exact whole numbers. Any transform that can be reversed gives back the input exactly, to the bit, after a round trip.

**The number-theoretic transform.** This is one transform modulo the prime 998244353. It is the exact stand-in for the Fourier transform: it never forms a floating-point number and never approximates a root of unity.

- `exact_translation_by_ntt.py` recovers a translation by NTT convolution, and its answer matches the direct correlation digit for digit.
- `ntt_double_transform_inverts.py` shows that the transform applied twice is a reflection. So the transform is its own inverse, apart from a reversal and a scale.
- `ntt_twiddle_certificate.py` certifies the prime, the primitive root and the order the transform rests on.

**Layout and space-filling maps.** Every render layout matches each cell index to exactly one place, computed in whole numbers. `bench_raster` checks that no two cells ever land in the same place. There are four layouts for a sheet (rows, serpentine, columns, diagonal) and four for a volume (slabs, boustrophedon, Morton, helix). The grouping step uses the same idea for reading: a Morton interleave and a Hilbert curve can flatten any number of dimensions into one, without being told the shape.

**Representation embeddings.**

- A file stored row by row goes back into the flat image it came from, given its width.
- Re-seating renumbers the alphabet so its values are spread as little as any numbering allows.
- Exact decimal input keeps every digit the source wrote, as a pair of whole numbers, and never forms a floating-point number.
- A Gray-coded embedding places any body of text as points in a binary volume, with one bit changing between neighbors.

**Exact codes.**

- A redundant residue number system stores a whole number as its residues over moduli that share no factors. The Chinese remainder theorem reconstructs it, and the extra moduli let it detect errors.
- Hamming(7,4) stores four data bits as seven and corrects any single flipped bit.

**Reference backgrounds.** A maximum-entropy background is built by a transform that removes one property.

- A permutation or a block shuffle draws the baseline the whole engine measures against.
- A phase fold groups positions that match modulo a period, and puts them back together.
- A windowed median and a self-similar context map each build a background, one by nearness and the other by shared context.

**The image transform program.** [`theory/theory/image_transforms`](https://github.com/dstroy0/orior/tree/main/theory/theory/image_transforms) is a research paper on exact image transforms: translation, rotation with scale and perspective, observed motion, and waves on a surface. Translation is built, and it is the number-theoretic transform above. The others are described in the research paper but not yet built, and the paper says which is which.

The full set lives one per file under `archive/src/python/` and in the C renderer. The workbook records what each one has been shown to do. A fair count is eleven families of transforms that can be reversed, or seventeen if each render layout is counted on its own, along with several one-way maps.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
