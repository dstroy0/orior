# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Where a generated page goes. The answer never puts a page beside the tool that wrote it.

Run python examples/00_blob_viz_tools/out_path.py --check to test it. It is a library, and not a tool.

WHY A PAGE NEVER GOES BESIDE ITS BUILDER

A builder that writes its output into its own directory is harmless in the tree that holds it, and
wrong everywhere else. In another repository that fetched it, the page lands among files that the
repository locks. That directory is one that its formatters leave alone. The next fetch then
overwrites the page, or stops because the tree is dirty. A generated file belongs where generated
files go, and the directory of the tool is never that place.

THE ORDER

    1  What the caller asked for, used exactly as given.
    2  The VIEW_OUT directory, for a caller who wants one place for everything.
    3  The scratch root of the host toolkit, when a toolkit holds the tool.
    4  A build directory at the top of the tree that holds the tool.
    5  The temporary directory of the machine.

Step 4 finds the top of the tree by a walk up to a marker, and does not count directories. A fetched
tool sits at whatever depth the host chose. Step 5 is the last resort, and it is still correct: a
disposable place is better than a locked one. No step resolves to the directory of the caller, and
the check below holds that.
"""

import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

# What marks the top of a tree. A version control directory names it in a checkout. The toolkit
# settings name it in a repository that joined one. A build directory names it in an unpacked tree
# with neither.
MARKERS = (".git", "repotools.toml", "build")


def tree_top(start):
    """The top of the tree that holds this path, or None where the walk up meets no marker."""
    here = os.path.abspath(start)
    while True:
        for mark in MARKERS:
            if os.path.exists(os.path.join(here, mark)):
                return here
        parent = os.path.dirname(here)
        if parent == here:
            return None
        here = parent


def host_scratch():
    """The scratch root of a toolkit that holds this tool, where there is one.

    The call sits in a try. In the tree that holds the source of these tools, no toolkit is present.
    Thus an absent host is the normal case, and never an error. A host that offers a root and then
    raises counts as no host at all.
    """
    try:
        import config
        loaded = config.load()
        root = loaded.scratch_root()
        if root and os.path.isdir(os.path.dirname(os.path.abspath(root))):
            return os.path.abspath(root)
    except Exception:
        return None
    return None


def out_root():
    """The directory for generated pages. This makes it where it is absent."""
    named = os.environ.get("VIEW_OUT")
    if named:
        root = os.path.abspath(named)
    else:
        root = host_scratch()
        if root is None:
            top = tree_top(HERE)
            root = os.path.join(top, "build", "view") if top else os.path.join(
                tempfile.gettempdir(), "view")
    if not os.path.isdir(root):
        os.makedirs(root, exist_ok=True)
    return root


def resolve(name, explicit=None):
    """Where to write a page called `name`. An explicit choice comes above everything else.

    This uses an explicit path as given, and never moves it. A caller that names a file has already
    decided. This makes the parent where it is absent. Thus a caller need not make a directory
    before a write into it.
    """
    if explicit:
        out = os.path.abspath(explicit)
        parent = os.path.dirname(out)
        if parent and not os.path.isdir(parent):
            os.makedirs(parent, exist_ok=True)
        return out
    return os.path.join(out_root(), name)


def _check():
    lines = []
    failed = 0

    def say(text):
        lines.append(text)

    # The property the whole module exists for: no default lands in the caller's own directory.
    was = os.environ.pop("VIEW_OUT", None)
    try:
        got = resolve("page.html")
        say("  a default page goes to %s" % os.path.dirname(got))
        if os.path.dirname(os.path.abspath(got)) == HERE:
            say("  FAIL the default resolved to the directory holding the tool")
            failed += 1
        if not os.path.isdir(os.path.dirname(got)):
            say("  FAIL the directory it named was not made")
            failed += 1

        # The resolver obeys an explicit choice exactly.
        want = os.path.join(tempfile.gettempdir(), "out_path_check", "named.html")
        got = resolve("ignored.html", want)
        if got != os.path.abspath(want):
            say("  FAIL an explicit path was not used as given")
            failed += 1
        if not os.path.isdir(os.path.dirname(got)):
            say("  FAIL the parent of an explicit path was not made")
            failed += 1
        say("  an explicit path is obeyed and its parent made")

        # The environment wins over the tree. Thus a caller can put every page in one place.
        os.environ["VIEW_OUT"] = os.path.join(tempfile.gettempdir(), "out_path_env")
        got = resolve("page.html")
        if os.path.dirname(got) != os.path.abspath(os.environ["VIEW_OUT"]):
            say("  FAIL the environment did not win over the tree")
            failed += 1
        say("  the environment wins over the tree")
        del os.environ["VIEW_OUT"]

        # A walk up finds a top from inside a tree, and gives nothing where there is no marker.
        if tree_top(HERE) is None:
            say("  FAIL no tree top was found from a directory inside a checkout")
            failed += 1
        root = os.path.abspath(os.sep)
        if tree_top(os.path.join(root, "nowhere", "at", "all")) is not None:
            say("  FAIL a top was claimed where no marker exists")
            failed += 1
        say("  the tree top is found by marker and not by counting directories")
    finally:
        if was is not None:
            os.environ["VIEW_OUT"] = was

    sys.stdout.write("\n".join(lines) + "\n\n")
    sys.stdout.write("%d check(s) failed\n" % failed)
    return failed


if __name__ == "__main__":
    if "--check" in sys.argv[1:]:
        sys.exit(1 if _check() else 0)
    sys.stdout.write(__doc__)
    sys.exit(2)
