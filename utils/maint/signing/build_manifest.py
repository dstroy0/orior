#!/usr/bin/env python3
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Compute the digests that a priority anchor is taken over. Keyless, and safe to run unattended.
#
#   Usage:  python utils/maint/signing/build_manifest.py [--root DIR] [--also DIR] [--out FILE]
#
# WHY THIS EXISTS
#
# docs/provenance.md section 3 argues for a priority anchor. A commit timestamp is no evidence of
# date: section 3 rules it out by name, because a commit date is written by the committer's own clock
# exactly as a signature's date field is.
#
# THE PROPERTY THAT MAKES THIS SAFE TO DO WHILE THE WORK IS STILL WITHHELD
#
# An anchor is taken over a DIGEST. A digest discloses nothing: it is the same size for a sentence or
# a corpus, it cannot be inverted, and it names no file. So the withheld material can be anchored today
# and revealed whenever, and the anchor still proves the bytes existed on the day it was made.
#
# That resolves the tension that would otherwise stall this. Holding the research back and
# establishing priority over it are not in conflict, and waiting for publication to anchor is the
# expensive mistake, because every day unanchored is a day a competing claim can predate.
#
# Keep manifest.tsv itself private while the work is withheld. Publishing it later is what converts the
# anchor from "these bytes existed" into "these bytes, and here they are".
#
# WHAT IS KEYLESS AND WHAT IS NOT
#
# Everything here. Computing digests, assembling the queue, and checking that every listed file is
# present and unchanged need no secret. They run on any machine at any time. Signing is one
# command over one small file, run by the person holding the key, and it is in sign.ps1 for him.

import hashlib
import os
import sys

# Directories holding nothing that needs a date. Build output is derived.
#
# This is the failure mode the skip list has to be audited FOR: a file wrongly skipped is undated
# forever and nothing downstream ever reports it, because the manifest cannot miss what it was told
# not to look at. An over-broad skip is silent in exactly the direction that costs the claim.
SKIP = {"build", "__pycache__", "node_modules", "logs", "obj", "bin"}

# A directory whose name opens with a dot holds a tool's own state: version control, an editor's
# settings, a linter's cache. Each is skipped except these, which hold authored files, and every
# one skipped is printed so that a wrong skip is never silent.
KEPT_DOT = {".github", ".githooks", ".cargo"}

# Artifacts that are regenerated, never authored. A digest over one of these dates a build and
# not the work, and it changes on every run, which would make the manifest churn for no gain.
SKIP_SUFFIX = (
    ".exe", ".o", ".obj", ".pdb", ".ilk", ".exp", ".lib", ".pyc",
    ".log", ".err", ".wav", ".ptx", ".pid",
)


def digest_of(path):
    """SHA-256 of one file, read in chunks to keep a large artifact from having to fit in memory."""
    accumulator = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            block = handle.read(1 << 20)
            if not block:
                break
            accumulator.update(block)
    return accumulator.hexdigest()


def walk(root, label, exclude=(), passed_over=None):
    """Every authored file under one root, as (label, relative path, size, digest).

    `exclude` holds absolute paths left out. The manifest and its signature belong there: a manifest
    cannot contain its own digest. Listing itself makes it report CHANGED on every run forever, the
    kind of defect that trains a reader to ignore the one line the check exists to print.

    `passed_over`, where given, takes the path of each dot-directory skipped.
    """
    rows = []
    excluded = {os.path.normcase(os.path.abspath(p)) for p in exclude}
    for dirpath, dirnames, filenames in os.walk(root):
        kept = []
        for one in dirnames:
            if one in SKIP:
                continue
            if one.startswith(".") and one not in KEPT_DOT:
                if passed_over is not None:
                    passed_over.append(os.path.relpath(os.path.join(dirpath, one), root).replace("\\", "/"))
                continue
            kept.append(one)
        dirnames[:] = kept
        for name in sorted(filenames):
            if name.endswith(SKIP_SUFFIX):
                continue
            full = os.path.join(dirpath, name)
            if os.path.normcase(os.path.abspath(full)) in excluded:
                continue
            # A dangling symlink is listed in a directory and cannot be read. Record it as such
            # instead of dying.
            if not os.path.isfile(full):
                rows.append((label, os.path.relpath(full, root).replace("\\", "/"), -1, "UNREADABLE"))
                continue
            try:
                rows.append((label, os.path.relpath(full, root).replace("\\", "/"),
                             os.path.getsize(full), digest_of(full)))
            except OSError as error:
                rows.append((label, os.path.relpath(full, root).replace("\\", "/"), -1,
                             "UNREADABLE: %s" % error))
    return rows


def label_for(root):
    """What to call a tree in the manifest, TAKEN FROM ITS CONFIG AND NOT FROM ITS DIRECTORY NAME.

    A name taken from the filesystem is not the project's name, it is where somebody put it. Reading
    it from repotools.toml means the manifest carries what the project calls itself, and a directory
    renamed tomorrow changes nothing in the record.

    Falls back to the basename when there is no config, because a tree with no repotools.toml is
    still worth dating and a missing name is not a reason to error instead of building.
    """
    config = os.path.join(root, "repotools.toml")
    if os.path.isfile(config):
        with open(config, "r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                stripped = line.strip()
                if stripped.startswith("name") and "=" in stripped:
                    value = stripped.split("=", 1)[1].strip().strip('"').strip("'")
                    if value:
                        return value
    return os.path.basename(root)


def main(argv):
    root = os.path.abspath(argv[argv.index("--root") + 1]) if "--root" in argv else os.path.abspath(".")
    also = os.path.abspath(argv[argv.index("--also") + 1]) if "--also" in argv else None
    out = argv[argv.index("--out") + 1] if "--out" in argv else os.path.join("utils", "maint", "signing", "manifest.tsv")

    # The manifest, its signature and BOTH proofs carry the claim and are not part of what it covers.
    # Two anchors are taken, one over the manifest and one over the signature. A proof inside the
    # manifest's own coverage makes verify report CHANGED on it on every run.
    itself = (out, out + ".ots", out + ".asc", out + ".asc.ots")

    passed_over = []
    rows = walk(root, label_for(root), exclude=itself, passed_over=passed_over)
    if also:
        rows += walk(also, label_for(also), exclude=itself, passed_over=passed_over)

    unreadable = [r for r in rows if r[3].startswith("UNREADABLE")]
    total = sum(r[2] for r in rows if r[2] >= 0)

    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("# MANIFEST. One row per authored file: which tree, path, bytes, SHA-256.\n")
        handle.write("#\n")
        handle.write("# SIGN THIS FILE INSTEAD OF THE CORPUS. One signature over a list of digests covers\n")
        handle.write("# every byte the list names, and it stays one deliberate act by a person.\n")
        handle.write("# Signing every file separately multiplies attestations without adding\n")
        handle.write("# information, and turns a human attestation into a build step.\n")
        handle.write("#\n")
        handle.write("# Then anchor the signature: `ots stamp manifest.tsv.asc`. The signature says\n")
        handle.write("# who and the anchor says when, and neither answers the other's question.\n")
        handle.write("#\n")
        handle.write("# KEEP THIS FILE PRIVATE WHILE THE WORK IS WITHHELD. The anchor is over a digest and\n")
        handle.write("# discloses nothing; this file names paths and therefore does.\n")
        handle.write("#\n")
        handle.write("# tree\tpath\tbytes\tsha256\n")
        for label, path, size, sha in sorted(rows):
            handle.write("%s\t%s\t%d\t%s\n" % (label, path, size, sha))

    print("  %d files, %.1f MB" % (len(rows), total / 1048576.0))
    print("  written to %s" % out)
    print("  %d dot-directories passed over as a tool's own state:" % len(passed_over))
    for one in sorted(passed_over):
        print("      %s" % one)
    if unreadable:
        print("  %d UNREADABLE, listed in the manifest so they cannot pass silently:" % len(unreadable))
        for label, path, _, why in unreadable:
            print("      %s/%s  %s" % (label, path, why))
    print("")
    print("  Next, and only the first needs the key:")
    print("      gpg --detach-sign --armor --local-user E603975E472D00FE %s" % out)
    print("      ots stamp %s.asc" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
