"""Each paper's tables, one file to a paper at PRIVATE/tables/<stem>.py, and the tools' view of them.

A paper's file sets, under the name the tool reads, what that tool holds for that paper alone:
residue.py's CORRECTIONS, page_text.py's PAPER_CIPHERS, PRIVATE_USE and the rest, italic_runs.py's
FORM_FACES. A name the tool reads as a set of papers, UNMENDED or PAPER_RULED, is True in the file of
each paper it holds. A paper's file can build on another's through of().
"""
import importlib.util
import os

from workdir import PRIVATE

TABLES = os.path.join(PRIVATE, "tables")
_LOADED = {}


def stems():
    """The stems that have a table file, in order."""
    if not os.path.isdir(TABLES):
        return []
    return sorted(name[:-3] for name in os.listdir(TABLES) if name.endswith(".py"))


def of(stem):
    """The paper's table file as a module, or None where it has none."""
    if stem not in _LOADED:
        path = os.path.join(TABLES, stem + ".py")
        module = None
        if os.path.isfile(path):
            spec = importlib.util.spec_from_file_location("tables_" + "".join(
                one if one.isalnum() else "_" for one in stem), path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
        _LOADED[stem] = module
    return _LOADED[stem]


def gather(name):
    """{stem: value} for each paper whose table file sets name."""
    return {stem: getattr(of(stem), name) for stem in stems() if hasattr(of(stem), name)}


def members(name):
    """The stems whose table file sets name true."""
    return {stem for stem, value in gather(name).items() if value}


if __name__ == "__main__":
    # python tables.py NAME [NAME ...]: a line for each paper whose file sets every name, its stem
    # and each value, parted by tabs. rerun.sh reads AUTHORS and LANG this way.
    import sys
    wanted = sys.argv[1:]
    for one in stems():
        paper = of(one)
        if all(hasattr(paper, name) for name in wanted):
            line = "\t".join([one] + [str(getattr(paper, name)) for name in wanted])
            sys.stdout.buffer.write((line + "\n").encode("utf-8"))
