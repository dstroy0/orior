# The orior app: roadmap

What the window does not do yet, by area. Each item says what it gives the reader. Within an area, the items stand in the order they are to be built. Under an item, each note names a complaint people make of the tools that do the same work, and what orior does so the reader never makes it. What is done moves to [the completed roadmap](COMPLETED_ROADMAP.md).

## Responsiveness

1. **The page's slow modules in Rust, vectorized.** Each module of the page a trace measures as slow, such as the tokens, the diff, the fuzzy match, the search and the minimap's reading of lines, moves into a Rust crate of orior's own, its inner loops written in the processor's vector instructions through `std::arch` alone: SSE2, and AVX2 where the machine has it, on x86-64, as MSVC and every other x86-64 target build them; NEON on aarch64, on Windows on Arm, on Linux and on the Macs of Apple's own chips; and a plain loop where neither is there. A Mac of Intel's chips takes the x86-64 path. A module moves only where the call into Rust and back costs less than the work it saves, measured on the files the tests hold.
   - Tokens, diffs and searches that hold the page's one thread on a large file: each one that costs a frame runs in Rust beside the page and hands back only its answer.
   - A speedup on one machine that is a slowdown or a wrong answer on another: each vector path is tested beside the plain loop, on x86-64 and on aarch64, and gives the same answer.

## Tools

1. **Runs watched as they run.** Every run a script starts, from the command line or anywhere else, shows in the window while it runs: each is found as its .qry file appears in a runs folder and is live while its writer runs, its blobs read from the run's shared buffer as they are written, a panic shown the moment it lands, each conference's accounts shown as the experts give them with the overseer's ruling, and the run's table of contents read once it ends.
   - A run started from a terminal that the window never hears of: the window finds each run by its file and its writer, wherever it was started.
   - A fault in the middle of a long run seen only at its end: a panic in the shared buffer shows in the window as it is written.
   - A ruling seen without what led to it: a conference, the blob `conference_` followed by its question's identity with each space written as an underscore, shows each line `<expert>: <account>` as it is written, and the line that starts `overseer:` as the ruling; the lessons in `overseer.txt`, one `lesson <cause> <carrier or *> <stand-in>` a line, show beside the runs.

## Settings

