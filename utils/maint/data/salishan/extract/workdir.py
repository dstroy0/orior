"""Where the extraction tools find what they read and keep what they make.

The tools are in the public tree. The published papers, and every text read off their pages, stay
in the closed corpus, and the tools read them there:
  CORPUS      the closed corpus checkout: ANCHOR_SIFT_PRIVATE when it is set, and otherwise the
              checkout the public tree's build/papers links into;
  PRIVATE     its extract/ directory: each paper's page text and ops, the defects table, and the
              outline and line tables the tools read and write;
  GENERATORS  each paper's generator, at PRIVATE/generators, and FINISH each paper's finish
              context, at PRIVATE/finish. Both import the tools from SALISHAN_TOOLS, which
              importing this sets to the tools' own directory. Each paper's tables are at
              PRIVATE/tables, read through tables.py;
  WORK        what a command remakes: a paper's engine draft, its alphabet, word web and residue
              tables, the oracles a rerun saves to compare against, and the crops read to settle a
              glyph. SALISHAN_WORK when it is set, and otherwise salishan_work beside CORPUS;
  ORACLES     the oracle tables and their notes, at examples/Salishan/oracles in the public tree,
              where the experiments read them;
  SALISHAN    the public tree's utils/maint/data/salishan, for paper_config, repairs, oracle_check
              and word_web;
  INSTRUMENT  the engine's instrument directory, for english_sift.
Without the closed corpus there is nothing to read, and importing this says so and stops.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
# Walks up to the repository instead of counting directories to it.
while (ROOT != os.path.dirname(ROOT)) and not os.path.isdir(os.path.join(ROOT, "src", "python")):
    ROOT = os.path.dirname(ROOT)
SALISHAN = os.path.dirname(HERE)
INSTRUMENT = os.path.join(ROOT, "src", "python", "engine", "nbody", "orior", "instrument")
ORACLES = os.path.join(ROOT, "examples", "Salishan", "oracles")
CORPUS = os.environ.get("ANCHOR_SIFT_PRIVATE") or os.path.dirname(
    os.path.realpath(os.path.join(ROOT, "build", "papers")))
PRIVATE = os.path.join(CORPUS, "extract")
GENERATORS = os.path.join(PRIVATE, "generators")
FINISH = os.path.join(PRIVATE, "finish")
os.environ["SALISHAN_TOOLS"] = HERE
if not (os.path.isdir(os.path.join(CORPUS, "papers")) and os.path.isdir(PRIVATE)):
    raise SystemExit("no closed corpus at %s: set ANCHOR_SIFT_PRIVATE, or link build/papers to its papers/"
                     % CORPUS)
WORK = os.environ.get("SALISHAN_WORK") or os.path.join(os.path.dirname(CORPUS), "salishan_work")
os.makedirs(WORK, exist_ok=True)
