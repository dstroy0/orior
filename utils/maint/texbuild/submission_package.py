#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Assemble one self-contained submission per research paper, in whatever shape the venue asks for.
#
#   `python utils/maint/texbuild/submission_package.py`                  every research paper, gzipped tar
#   `python utils/maint/texbuild/submission_package.py --archive zip`    every research paper, zip
#   `python utils/maint/texbuild/submission_package.py --archive none`   directories only
#   `python utils/maint/texbuild/submission_package.py <research_paper> ...`       named research papers
#   `python utils/maint/texbuild/submission_package.py --arxiv [<research_paper>]` arXiv's shape, under build/arxiv/
#
# The tool writes build/submission/<research_paper>/ and, unless --archive none, an archive beside it. It changes
# nothing under theory/. The rewrite happens on the copy, and the source keeps its own preamble.
#
# THE REQUIREMENT IS NOT ANY ONE VENUE'S
#
# A submission is a directory that compiles in place. Every venue enforces that, and they differ only in what they want
# it wrapped in. The arXiv site extracts a tarball into one root and runs LaTeX there. Most journals ask for a zip. A
# repository deposit takes the directory as it is. A venue that wants only the built PDF still needs a build that worked
# somewhere.
#
# The assembly and the checks below are therefore all of the job, and they are the same every time. The archive format
# is the only part that changes, and it is a flag. One venue's packaging is not a property of the work.
#
# WHY THE TOOL EXISTS
#
# A research paper can reach outside its own directory for a file that it shares with the others. Take a main.tex that
# says \input{../preamble.tex}, or a preamble.tex that says \input{../macros.tex}. Each resolves here, because theory/
# is the directory above them. It resolves nowhere else.
#
# The fix is not a new layout of theory/: one shared file across every research paper would let a layout change reach
# all of them. The fix is to copy the shared files into the research paper when the tool packs it. The tool then
# rewrites the lines that point at them.
#
# THE TOOL FINDS RESEARCH PAPERS, AND HOLDS NO LIST
#
# A list written by hand goes stale. A research paper added after the list does not build, and does not appear where the
# list feeds the documentation. This walks for main.tex at both depths, and the packager cannot go stale that way.
#
# WHAT IT REFUSES TO SHIP
#
# A submission that still holds a path that climbs out is broken, and fails on upload with a LaTeX error that nobody can
# read. The tool reads every .tex in the assembled copy again AFTER the rewrite. A ../ still in an \input or \include
# fails that research paper, and names the file and the line. A rule meant to catch a path is not evidence that it did.
#
# It also errors where a submission would carry what a LaTeX run leaves behind. See LEAVINGS.
#
# WHAT --arxiv ADDS, AND WHY EACH STEP IS THERE
#
# The arXiv site extracts a tarball into one directory and runs LaTeX there. Everything extracted becomes public, even
# where the document does not read it. That second half is the part that costs somebody later. The extra steps are
# mostly deletions:
#   - flattened: the tool empties every subdirectory into the root, and rewrites the \input and \include lines that
#     named it. It checks first for two files with one basename, because a flatten of those would silently destroy one
#     of them.
#   - comments gone: the tool deletes a full-line comment. A comment at the end of a line loses its text and keeps its
#     percent sign. In LaTeX that percent sign eats the newline, and without it a space appears in the typeset page. A
#     strip of the whole comment is the usual way this step goes wrong.
#   - nothing hidden: the tool does not copy a dotfile or dot-directory. The document never reads one, and each one is
#     somebody's local state.
#   - nothing unread: the tool drops a .tex that no \input or \include chain from main.tex reaches. An old draft that
#     nobody compiles still publishes.
#   - no notes: the tool drops a Markdown file. The ones beside a research paper are the notes and logs of the work, and
#     LaTeX reads none of them. The tool names and keeps any other file that is not source or a figure.
#   - four passes: \typeout just before \end{document} makes arXiv run LaTeX until the labels settle. After
#     \end{document}, LaTeX has already stopped reading, and the line never reaches the log.
#   - 00README.json: the engine, the top-level file and the TeX Live release. They go in the JSON form that arXiv reads
#     (https://info.arxiv.org/help/00README.html). Without it, arXiv guesses the engine from the source, and these
#     research papers need xelatex.
#   - flat tarball: no wrapping directory. `tar -cvvf ax.tar *` produces that from inside the assembled copy.
#   - metadata.txt: the title, the authors and the abstract, with the LaTeX taken out and the line breaks collapsed. It
#     is ready to paste into the web form. The arXiv form shows the whitespace that LaTeX ignores, and the collapse is
#     not cosmetic.
#
# It submits nothing and holds no credential. The tarball and the metadata are all of the output, and a person uploads
# them.

import io
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import zipfile

def _repository_root():
    """This repository, asked of git and not inferred from a marker directory.

    A marker that the repository produces, such as build/, is absent from a linked worktree and from a clone with no
    build. A climb to it can pass this root and land in another checkout whose paths look valid. A marker infers the
    root. Git answers it. The climb below serves only an exported tree with no git directory. It looks for
    src/cu/engine, which the repository tracks and every checkout of it holds.

    The function clears the variables of git first. Inside a hook, git exports GIT_DIR. A rev-parse that inherits it
    answers about that repository, and not about the directory that the caller asks from. It then returns the current
    directory instead of the root.
    """
    start = os.path.dirname(os.path.abspath(__file__))
    environment = dict(os.environ)
    for key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_PREFIX", "GIT_COMMON_DIR"):
        environment.pop(key, None)

    try:
        said = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], cwd=start,
                                       stderr=subprocess.PIPE, env=environment)
    except (OSError, subprocess.CalledProcessError):
        said = b""

    top = said.decode("utf-8", "replace").strip()
    if top and os.path.isdir(top):
        return os.path.abspath(top)

    climbed = start
    while (climbed != os.path.dirname(climbed)) \
            and not os.path.isdir(os.path.join(climbed, "src", "cu", "engine")):
        climbed = os.path.dirname(climbed)
    return climbed


ROOT = _repository_root()
THEORY = os.path.join(ROOT, "theory")
# Every research paper sits under theory/, a directory of this repository, on one of its three shelves: theory/theory/,
# theory/workbooks/ and theory/thought_experiments/. Its path below theory/ names a research paper.
TREES = (THEORY,)
OUT = os.path.join(ROOT, "build", "submission")
OUT_ARXIV = os.path.join(ROOT, "build", "arxiv")

# The files that every research paper can share, which sit in theory/ and not in any research paper. The tool copies
# these into the submission root, and rewrites the lines that reach up for them to name them plainly.
#
# dedication.tex is here for the same reason as preamble.tex. One text reaches every research paper, kept in one place
# so that it cannot drift between them.
#
# A research paper can carry its own preamble.tex. The tool copies a shared file only where theory/ has it and the
# research paper does not. A copy with no such test would stop the packager on the first research paper, with an error
# for an absent file.
SHARED = ("preamble.tex", "macros.tex", "dedication.tex")

# What arXiv reads before it compiles anything. spec_version 1 is the only version the format has.
README = {
    "spec_version": 1,
    "process": {"compiler": "xelatex"},
    "sources": [{"filename": "main.tex", "usage": "toplevel"}],
    "texlive_version": 2025,
}

# An \input or \include whose argument climbs out of the directory of its own file. The pattern captures the name, and
# only the shared files get a rewrite. Another path that climbs out is a different problem, and the tool reports it
# instead of a quiet repoint.
CLIMBING = re.compile(r"(\\(?:input|include)\{)((?:\.\./)+)([^}]+)(\})")

# What a source file is. The tool copies everything else in a research paper directory as it is. On those terms, the
# matplotlib figure beside the corpus derivation chapter travels with its chapter.
TEX = ".tex"

# What the archive flag accepts, against what it writes.
ARCHIVES = ("tar.gz", "zip", "none")

# What a LaTeX run leaves beside the source, and what never belongs in a submission.
#
# build_theory.sh compiles with -output-directory, and leaves none of this. TeXworks and TeXShop compile IN PLACE. A
# research paper opened in one of those comes back with main.pdf, main.log, main.aux and main.toc. It also gets a .aux
# per included chapter, and a synctex index larger than all the rest of the research paper. A copy of a research paper
# directory as a whole ships all of it.
LEAVINGS = (".aux", ".log", ".toc", ".lof", ".lot", ".out", ".bbl", ".blg", ".idx", ".ilg",
            ".ind", ".nav", ".snm", ".vrb", ".fls", ".fdb_latexmk", ".synctex", ".synctex.gz")


def is_leaving(name):
    """Whether a filename is something that a LaTeX run dropped beside the source.

    The test excludes main.pdf by name, and not by extension. A PDF is otherwise a figure, and the matplotlib figure of
    the corpus derivation chapter must travel with its chapter.
    """
    lowered = name.lower()
    if lowered == "main.pdf":
        return True
    return any(lowered.endswith(one) for one in LEAVINGS)


# The line arXiv reads as an instruction to run LaTeX again. Placed just before \end{document}: it
# typesets nothing and only reaches the log the rerun logic watches.
TYPEOUT = "\\typeout{get arXiv to do 4 passes: Label(s) may have changed. Rerun}"
END_DOCUMENT = "\\end{document}"

# An \input or \include that names a file inside the research paper. The flatten rewrites it.
NAMED = re.compile(r"(\\(?:input|include)\{)([^}]+)(\})")


def uncomment(text):
    """One file's text with the comment text removed, and how many comments that was.

    A percent sign starts a comment when the run of backslashes before it is even. An odd run ends in an escaped
    percent, which typesets as a character.

    The function deletes a full line comment outright. A comment after content keeps its percent sign, and loses all
    after it. That percent sign does work: LaTeX drops the newline that follows it. Its removal joins two words with a
    space that was not there before. This is the step that quietly changes a typeset page, and the kept sign stops it.
    """
    held = []
    found = 0
    for line in text.splitlines():
        at = -1
        run = 0
        for index, character in enumerate(line):
            if character == "\\":
                run += 1
                continue
            if (character == "%") and (run % 2 == 0):
                at = index
                break
            run = 0
        if at < 0:
            held.append(line)
            continue
        found += 1
        if not line[:at].strip():
            continue
        held.append(line[:at + 1])
    return "\n".join(held) + ("\n" if text.endswith("\n") else ""), found


def reachable(out):
    """Every .tex under out that main.tex reaches, by name without its extension.

    The walk goes from main.tex through \\input and \\include. A file that only another reached file reaches still
    counts. A .tex outside this set is an old draft, and an upload of one publishes it.
    """
    seen = set()
    pending = ["main"]
    while pending:
        stem = pending.pop()
        if stem in seen:
            continue
        seen.add(stem)
        for where, _dirs, names in os.walk(out):
            for name in names:
                if name != (stem + TEX) and name != stem:
                    continue
                with open(os.path.join(where, name), encoding="utf-8") as handle:
                    text = handle.read()
                for found in NAMED.finditer(text):
                    target = os.path.basename(found.group(2))
                    pending.append(target[:-len(TEX)] if target.endswith(TEX) else target)
    return seen


def flatten(out):
    """Every file moved into the root of out, with the naming lines rewritten.

    Returns (moved, collisions). A collision is two files with one basename. The function moves nothing when there is
    one, because a flatten past it would overwrite a file that the document reads.
    """
    holding = {}
    for where, _dirs, names in os.walk(out):
        if os.path.abspath(where) == os.path.abspath(out):
            continue
        for name in names:
            holding.setdefault(name, []).append(os.path.join(where, name))
    collisions = {name: paths for name, paths in holding.items()
                  if (len(paths) > 1) or os.path.isfile(os.path.join(out, name))}
    if collisions:
        return [], collisions

    moved = []
    for name, paths in sorted(holding.items()):
        shutil.move(paths[0], os.path.join(out, name))
        moved.append(os.path.relpath(paths[0], out).replace("\\", "/"))
    for where, dirs, _names in os.walk(out, topdown=False):
        for one in dirs:
            folder = os.path.join(where, one)
            if not os.listdir(folder):
                os.rmdir(folder)

    for name in sorted(os.listdir(out)):
        if not name.endswith(TEX):
            continue
        path = os.path.join(out, name)
        with open(path, encoding="utf-8") as handle:
            text = handle.read()
        changed = NAMED.sub(lambda found: "%s%s%s" % (found.group(1),
                                                      os.path.basename(found.group(2)),
                                                      found.group(3)), text)
        if changed != text:
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(changed)
    return moved, {}


def strip_latex(text):
    """One run of LaTeX source as the plain text that the arXiv web form wants.

    Control sequences and their braces come out. The accented forms that LaTeX defines in ASCII become their character,
    and every run of whitespace becomes one space. The last part matters most: LaTeX ignores the line breaks in an
    abstract, and arXiv prints them.
    """
    text = re.sub(r"\\(?:emph|textbf|textit|texttt|text|mbox|spacedallcaps|spacedlowsmallcaps)"
                  r"\{([^{}]*)\}", r"\1", text)
    text = re.sub(r"\\[A-Za-z@]+\*?", " ", text)
    text = text.replace("\\\\", " ").replace("~", " ")
    text = re.sub(r"[{}$]", "", text)
    text = text.replace("--", "-")
    return " ".join(text.split())


def metadata(out, research_paper):
    """The title, authors and abstract of an assembled research paper, as text to paste into the form.

    The function reads them off the assembled copy, because that copy ships. The titlepage carries the title and the
    author. The abstract sits in its own file. The function finds both by name and not by position, and a research paper
    that orders its frontmatter differently still reports.
    """
    lines = []
    title = ""
    authors = ""
    titlepage = os.path.join(out, "titlepage.tex")
    if os.path.isfile(titlepage):
        with open(titlepage, encoding="utf-8") as handle:
            held = [strip_latex(one) for one in handle.read().splitlines()]
        held = [one for one in held if one]
        if held:
            title = held[0]
        for index, one in enumerate(held):
            if one.lower() == "by" and (index + 1) < len(held):
                authors = held[index + 1]
                break

    abstract = ""
    where = os.path.join(out, "abstract.tex")
    if os.path.isfile(where):
        with open(where, encoding="utf-8") as handle:
            body = handle.read()
        body = re.sub(r"\\(?:pdfbookmark|chapter)\*?(?:\[[^\]]*\])?\{[^}]*\}\{?[^}]*\}?", " ", body)
        abstract = strip_latex(body)

    lines.append("title")
    lines.append("  " + (title.title() if title.isupper() else title))
    lines.append("")
    lines.append("authors")
    lines.append("  " + authors.replace(" and ", ", "))
    lines.append("")
    lines.append("abstract")
    lines.append("  " + abstract)
    lines.append("")
    lines.append("subject area is not filled in here. It is a judgement about where the work")
    lines.append("belongs and nobody but the author can make it.")
    lines.append("")
    path = os.path.join(os.path.dirname(out), "%s_metadata.txt" % os.path.basename(out))
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines))
    return path, title, authors, abstract


def flat_tar(out):
    """The assembled directory as a tarball with no wrapping directory.

    arXiv extracts into one root it chose. A wrapping directory puts every file one level below
    where main.tex says they are. This is `tar -cvvf ax.tar *` run from inside the copy.
    """
    archive = "%s.tar" % out
    if os.path.isfile(archive):
        os.remove(archive)
    with tarfile.open(archive, "w") as handle:
        for name in sorted(os.listdir(out)):
            handle.add(os.path.join(out, name), arcname=name)
    return os.path.basename(archive), os.path.getsize(archive)


def research_paper_tree(research_paper):
    """Which tree holds this research paper, or None where no tree does."""
    for tree in TREES:
        if os.path.isfile(os.path.join(tree, research_paper, "main.tex")):
            return tree
    return None


def research_papers():
    """Every research paper, as its path below theory/.

    Two depths, matching build_theory.sh. <shelf>/<research_paper>/ is where the majority of them sit, and
    theory/<subject>/<research_paper>/ is where the cryptography one does.
    """
    import glob
    found = []
    for tree in TREES:
        for pattern in (("*", "*", "main.tex"), ("*", "*", "*", "main.tex")):
            for one in sorted(glob.glob(os.path.join(tree, *pattern))):
                found.append(os.path.relpath(os.path.dirname(one), tree).replace("\\", "/"))
    return sorted(set(found))


def rewrite(text):
    """One file's text with the shared inputs brought in, and the paths that still climb out.

    Returns (text, rewritten, errored). `rewritten` names the shared files repointed. `errored` holds any other path
    that climbs out, which is not for this function to decide.
    """
    rewritten = []
    error = []

    def one(found):
        opener, climb, name, closer = found.groups()
        target = os.path.basename(name)
        if target in SHARED:
            rewritten.append(target)
            return "%s%s%s" % (opener, target, closer)
        error.append("%s%s" % (climb, name))
        return found.group(0)

    return CLIMBING.sub(one, text), rewritten, error


def assemble(research_paper, out):
    """One research paper copied into out, with the shared files inside it. Returns what was rewritten."""
    source = os.path.join(research_paper_tree(research_paper) or THEORY, research_paper)
    if os.path.isdir(out):
        shutil.rmtree(out)
    shutil.copytree(source, out, ignore=lambda where, names: [one for one in names
                                                             if is_leaving(one)])
    for name in SHARED:
        shared = os.path.join(THEORY, name)
        if os.path.isfile(shared) and not os.path.isfile(os.path.join(out, name)):
            shutil.copy2(shared, os.path.join(out, name))

    rewritten = []
    error = []
    for where, _dirs, names in os.walk(out):
        for name in sorted(names):
            if not name.endswith(TEX):
                continue
            path = os.path.join(where, name)
            with open(path, encoding="utf-8") as handle:
                text = handle.read()
            changed, did, would_not = rewrite(text)
            if changed != text:
                with open(path, "w", encoding="utf-8", newline="\n") as handle:
                    handle.write(changed)
            rewritten.extend((os.path.relpath(path, out), one) for one in did)
            error.extend((os.path.relpath(path, out), one) for one in would_not)
    return rewritten, error


def still_climbing(out):
    """Any \\input or \\include left with a path above the submission root, with its line number.

    The function reads it back off the assembled copy, and does not predict it from the rewrite, because the copy is
    what ships.
    """
    held = []
    for where, _dirs, names in os.walk(out):
        for name in sorted(names):
            if not name.endswith(TEX):
                continue
            path = os.path.join(where, name)
            with open(path, encoding="utf-8") as handle:
                for number, line in enumerate(handle, 1):
                    if CLIMBING.search(line):
                        held.append((os.path.relpath(path, out), number, line.strip()))
    return held


def bundle(out, type):
    """The assembled directory wrapped as the venue asked. Returns (name, bytes), or None.

    Every path inside the archive sits under one top-level directory named for the research paper. A venue that extracts
    into one root needs that, and a person who unpacks it locally wants it anyway.
    """
    base = os.path.basename(out)
    if type == "none":
        return None
    if type == "tar.gz":
        archive = "%s.tar.gz" % out
        if os.path.isfile(archive):
            os.remove(archive)
        with tarfile.open(archive, "w:gz") as handle:
            handle.add(out, arcname=base)
    else:
        archive = "%s.zip" % out
        if os.path.isfile(archive):
            os.remove(archive)
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as handle:
            for where, _dirs, names in os.walk(out):
                for name in sorted(names):
                    full = os.path.join(where, name)
                    handle.write(full, os.path.join(base, os.path.relpath(full, out)))
    return os.path.basename(archive), os.path.getsize(archive)


def for_arxiv(out, research_paper):
    """The --arxiv steps, in the order the header lists them, on an assembled copy.

    Returns (said, stopped). said is one line per thing done. stopped is a reason the copy cannot
    ship, or empty.
    """
    said = []

    for where, dirs, names in os.walk(out):
        for one in [name for name in dirs if name.startswith(".")]:
            shutil.rmtree(os.path.join(where, one))
            dirs.remove(one)
            said.append("hidden       %s/ removed" % one)
        for one in [name for name in names if name.startswith(".")]:
            os.remove(os.path.join(where, one))
            said.append("hidden       %s removed" % one)

    read = reachable(out)
    for where, _dirs, names in os.walk(out):
        for name in names:
            if name.endswith(TEX) and (name[:-len(TEX)] not in read):
                os.remove(os.path.join(where, name))
                said.append("unread       %s dropped" % os.path.relpath(os.path.join(where, name), out))

    moved, collisions = flatten(out)
    if collisions:
        for name, paths in sorted(collisions.items()):
            said.append("COLLISION    %s at %s" % (name, ", ".join(os.path.relpath(one, out) for one in paths)))
        return said, "two files share a basename and flattening would destroy one"
    said.append("flattened    %d file(s) into the root" % len(moved))

    stripped = 0
    for name in sorted(os.listdir(out)):
        if not name.endswith(TEX):
            continue
        path = os.path.join(out, name)
        with open(path, encoding="utf-8") as handle:
            text, found = uncomment(handle.read())
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        stripped += found
    said.append("comments     %d removed" % stripped)

    main_tex = os.path.join(out, "main.tex")
    with open(main_tex, encoding="utf-8") as handle:
        text = handle.read()
    if text.count(END_DOCUMENT) != 1:
        return said, "main.tex holds %d \\end{document}, not one" % text.count(END_DOCUMENT)
    with open(main_tex, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text.replace(END_DOCUMENT, TYPEOUT + "\n" + END_DOCUMENT))
    said.append("four passes  requested before \\end{document}")

    with open(os.path.join(out, "00README.json"), "w", encoding="utf-8", newline="\n") as handle:
        handle.write(json.dumps(README, indent=2) + "\n")
    said.append("00README.json  %s, TeX Live %d" % (README["process"]["compiler"], README["texlive_version"]))

    # Markdown beside a research paper is working notes and logs. LaTeX never reads it, and arXiv would publish
    # it with the paper. The workbook carried seven such files into its first package.
    for name in sorted(os.listdir(out)):
        if name.lower().endswith(".md"):
            os.remove(os.path.join(out, name))
            said.append("notes        %s dropped, LaTeX does not read it" % name)

    # Anything else left that is neither source nor a figure publishes with the paper. The tool names it here and does
    # not delete it, because an unknown file may be one that the research paper needs.
    for name in sorted(os.listdir(out)):
        if not name.lower().endswith((TEX, ".pdf", ".png", ".jpg", ".jpeg", ".json")):
            said.append("SHIPS        %s is not source or a figure and will be public" % name)

    path, _title, _authors, _abstract = metadata(out, research_paper)
    said.append("metadata     %s" % os.path.basename(path))
    return said, ""


def wanted_archive(argv):
    """The archive type named on the command line, with a gzipped tar as the default."""
    if "--archive" not in argv:
        return "tar.gz"
    at = argv.index("--archive")
    if (at + 1) >= len(argv):
        raise SystemExit("submission_package: --archive wants one of %s" % ", ".join(ARCHIVES))
    type = argv[at + 1]
    if type not in ARCHIVES:
        raise SystemExit("submission_package: unknown archive %r, wants one of %s"
                         % (type, ", ".join(ARCHIVES)))
    return type


def main():
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace", newline="")
    argv = sys.argv[1:]
    type = wanted_archive(argv)
    arxiv = "--arxiv" in argv
    skip = {"--archive", type}
    named = [one for one in argv if (one not in skip) and not one.startswith("-")]
    status = 0

    for research_paper in (named or research_papers()):
        if research_paper_tree(research_paper) is None:
            out.write("  no such research paper: %s\n" % research_paper)
            status = 1
            continue

        flat = research_paper.replace("/", "_")
        target = os.path.join(OUT_ARXIV if arxiv else OUT, flat)
        rewritten, error = assemble(research_paper, target)
        left = still_climbing(target)

        out.write("  %s\n" % research_paper)
        for path, name in rewritten:
            out.write("    brought in   %-34s in %s\n" % (name, path))
        for path, name in error:
            out.write("    NOT MINE     %-34s in %s\n" % (name, path))
            status = 1
        if left:
            for path, number, line in left:
                out.write("    STILL CLIMBS %s:%d  %s\n" % (path, number, line))
            out.write("    %s is not shippable and no archive was written\n" % research_paper)
            status = 1
            continue

        if arxiv:
            said, stopped = for_arxiv(target, research_paper)
            for line in said:
                out.write("    %s\n" % line)
            if stopped:
                out.write("    %s is not shippable: %s\n" % (research_paper, stopped))
                status = 1
                continue
            out.write("    %s  %d bytes\n" % flat_tar(target))
            continue

        made = bundle(target, type)
        if made is None:
            out.write("    %s/  directory only\n" % flat)
        else:
            out.write("    %s  %d bytes\n" % made)

    out.write("\n  Assembled under %s\n" % os.path.relpath(OUT_ARXIV if arxiv else OUT, ROOT))
    out.write("  A package is not a submission until it has compiled from inside its own\n")
    out.write("  directory. Nothing here has compiled anything.\n")
    out.flush()
    return status


if __name__ == "__main__":
    raise SystemExit(main())
