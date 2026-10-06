# Setup

You don't need a GPU, a service or a network connection for any of this. The one exception is the fetch scripts, which are listed below.

## Python

You need Python 3.9 or newer. It was developed on 3.14.

```sh
python -m pip install numpy
```

That is all you need for the measure, the reference, and every example that reads a text collection you already have. Other tools need one more package each, and each one tells you when it is missing:

| package      | what needs it                                                     |
| ------------ | ----------------------------------------------------------------- |
| `numpy`      | the engine, and 75 call sites across the tree                     |
| `pypdf`      | reading papers, `utils/maint/data/salishan/get_papers.py --convert`     |
| `requests`   | the corpus fetchers under `utils/maint/data/fetch/` and `get_papers.py` |
| `matplotlib` | the corpus derivation figure                                      |
| `soundfile`  | the sound representation, which reads recordings                  |
| `Pillow`     | reading an image as a byte sequence                               |

Install a package when a tool asks for it. A tool that is missing a package names the package and gives you the install line, and the rest of the tree keeps working.

## The C engine

You need a C11 compiler and CMake, with Ninja as the generator. On Windows a bare `cmake` picks NMake and fails.

From a fresh clone, one command configures and builds the engine and runs the graders, the tests that check its answers:

```sh
utils/maint/engine/build_engine.sh               # configure, build, run the graders
utils/maint/engine/build_engine.sh --build-only  # configure and build, run nothing
```

On Windows, use `utils/maint/engine/build_engine.ps1`, which takes the same two forms. It imports the MSVC environment and compiles the device rasterizer. If you run the shell script from Git Bash instead, there is no MSVC environment, and the build uses gcc or clang.

The output goes to `build/engine_c/`. Nothing reads it back, and you can delete it whenever you like.

If you want more control, run CMake yourself:

```sh
cmake -S src/cu -B build/engine_c -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/engine_c
```

That builds the benches and the tests. Every target builds with GCC, Clang and MSVC except one: `bench_lattice` needs C99 `_Complex`, and MSVC provides those types without their operators. Build that one with GCC or Clang.

If all you want is the search kernel, you don't need a build system at all. It is four portable source groups and two include paths:

```sh
gcc -std=c11 -Isrc/cu/engine/nbody/orior -Isrc/cu/types/integers your_program.c \
    src/cu/engine/nbody/orior/orior_*.c src/cu/engine/nbody/orior/scan.c \
    src/cu/types/integers/exact_integer_*.c src/cu/types/integers/arm.c
```

## The research papers

You need XeLaTeX, from TeX Live or MiKTeX.

- The build runs XeLaTeX twice for each research paper. The first pass writes the table of contents and the second reads it.
- It uses XeLaTeX and not LuaLaTeX because arXiv runs XeLaTeX and does not run LuaLaTeX.
- The fonts are Charis SIL, DejaVu and TeX Gyre Termes Math. All are named by file, and all ship with TeX Live 2025.

To build every research paper:

```sh
sh utils/maint/texbuild/build_theory.sh
```

The PDFs go to `build/theory/<research_paper>/main.pdf`. To build just one:

```sh
sh utils/maint/texbuild/build_theory.sh workbook
```

The build fails if a research paper is missing a character. These papers print Salishan orthography, and a missing character would leave a page wrong without any warning.

To package a research paper for arXiv:

```sh
python utils/maint/texbuild/submission_package.py --arxiv <research_paper>
```

The package goes to `build/arxiv/<research_paper>.tar`. It includes a `00README.json` that tells arXiv to use XeLaTeX and TeX Live 2025.

## Text collections

None of them are in git. The scripts in `examples/` take the path to a text collection as an argument. Anything you already have will work.

To build the language collections this work measured, the scripts under `utils/maint/data/fetch/` fetch them from public archives:

```sh
python utils/maint/data/fetch/fetch_parallel_corpus.py
python utils/maint/data/fetch/fetch_treebanks.py
```

Each script names its source and writes under `build/corpora/`.

The Salishan papers come from the ICSNL archive:

```sh
python utils/maint/data/salishan/get_papers.py
```

You can't fetch the hand extractions. They are copied by hand out of published papers and are not included here, as [What is not here](research.md#what-is-not-here) explains. Everything that doesn't read a paper or one of those tables runs without them.

## Checks

```sh
python utils/maint/prose/docs_check      the register check over every document and comment
python utils/maint/catalog/catalog.py --check       every example carries its catalog number
python utils/maint/catalog/catalog_verify.py        where an example's description and its code disagree
python utils/maint/tree/write_survey.py          every file write in the tree, and where it lands
```

`.githooks/pre-commit` runs the first of these. Turn it on once for each clone:

```sh
git config core.hooksPath .githooks
```

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
