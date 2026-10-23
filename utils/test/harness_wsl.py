# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py wsl: the ext4 test volume under WSL, the tree copied onto it, and commands run there.

    harness.py wsl mount                  the volume's state: mounted, made and not mounted, or not made
    harness.py wsl mount --create         the volume made as WSL's root: a 64 GB ext4 image on the distro's disk
    harness.py wsl mount --attach         the volume made before mounted again, as after a WSL restart
    harness.py wsl sync                   the working tree copied onto the volume with rsync, build/ and .git left out
    harness.py wsl run -- COMMAND...      a command run in the volume's tree under bash -lc, the tree copied first

The repository sits on a Windows drive, which WSL reaches through its 9p mount; a build there spends its time in the
mount. The volume is an image file on the distro's own ext4 disk, /var/lib/orior_test.img, 64 GB, holding an ext4
file system mounted through a loop device as /mnt/orior_test. What runs on it runs at the disk's own speed and
cannot grow past the image. An /etc/fstab line mounts it at boot. Its tree holds the copy of the working tree the
suites build, and its tools hold the programs the suites need and the toolchains sheet looks for there, Ghidra among
them. Making and mounting take root, which WSL gives through `wsl -u root` with no password; a disk attached from
Windows with `wsl --mount` is not used, as an elevated PowerShell attaches it to the elevated session's own WSL
instance, where the suites do not run."""

import os
import shlex
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
GIGABYTES = 64
NAME = "orior_test"
IMAGE = "/var/lib/%s.img" % NAME
MOUNT = "/mnt/" + NAME
TREE = MOUNT + "/tree"
TOOLS = MOUNT + "/tools"
FSTAB = "%s %s ext4 loop,nofail 0 0" % (IMAGE, MOUNT)


def wsl_path(path):
    """A Windows path as WSL mounts it: D:/a/b -> /mnt/d/a/b."""
    p = os.path.abspath(path).replace("\\", "/")
    if len(p) > 1 and p[1] == ":":
        return "/mnt/%s%s" % (p[0].lower(), p[2:])
    return p


def words(command, user=None):
    """The wsl.exe command line that runs a bash command, as `user` where one is named."""
    return ["wsl.exe"] + (["-u", user] if user else []) + ["-e", "bash", "-lc", command]


def said(command, user=None):
    """What a command under WSL prints, stripped; '' where it fails."""
    r = subprocess.run(words(command, user), capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ""


def ran(command, user=None):
    """A command under WSL run with its output through the harness's own: its exit code."""
    return subprocess.run(words(command, user)).returncode


def mounted():
    """Whether the volume is mounted as ext4."""
    return said("findmnt -n -o FSTYPE %s" % MOUNT) == "ext4"


def made():
    """Whether the image is made."""
    return said("test -f %s && echo yes" % IMAGE) == "yes"


def attach_commands(user):
    """The root commands that mount the image and hand its root to `user`, the fstab line put once."""
    return ["mkdir -p %s" % MOUNT,
            "grep -qF %s /etc/fstab || echo %s >> /etc/fstab" % (shlex.quote(IMAGE), shlex.quote(FSTAB)),
            "mount %s" % MOUNT,
            "mkdir -p %s %s" % (TREE, TOOLS),
            "chown %s:%s %s %s %s" % (user, user, MOUNT, TREE, TOOLS)]


def create_commands(user):
    """The root commands that make the image where none is, format it ext4 and mount it."""
    return ["test -f %s || fallocate -l %dG %s" % (IMAGE, GIGABYTES, IMAGE),
            "blkid -o value -s TYPE %s | grep -q ext4 || mkfs.ext4 -q -F -L %s %s" % (IMAGE, NAME, IMAGE),
            ] + attach_commands(user)


def cmd_wsl_mount(a):
    if a.create or a.attach:
        user = said("id -un")
        if not user:
            print("WSL did not answer")
            return 1
        command = " && ".join(create_commands(user) if a.create else attach_commands(user))
        code = ran("export PATH=/usr/sbin:/sbin:$PATH && " + command, user="root")
        if code != 0:
            print("the root commands ended with %d" % code)
            return code
    if mounted():
        print("mounted: %s is ext4, %s" % (MOUNT, said("df -h %s | tail -1" % MOUNT)))
        return 0
    if made():
        print("%s is made and not mounted: run `harness.py wsl mount --attach`" % IMAGE)
    else:
        print("no ext4 test volume: run `harness.py wsl mount --create`: it makes %s (%d GB) as WSL's root, formats "
              "it ext4 and mounts it as %s" % (IMAGE, GIGABYTES, MOUNT))
    return 1


def synced():
    """The working tree copied onto the volume, build/ and .git left out: rsync's exit code."""
    return ran("rsync -a --delete --exclude /build/ --exclude /.git/ %s/ %s/" % (wsl_path(ROOT), TREE))


def cmd_wsl_sync(a):
    if not mounted():
        return cmd_wsl_mount(a)
    return synced()


def cmd_wsl_run(a):
    if not mounted():
        return cmd_wsl_mount(a)
    if not a.no_sync:
        code = synced()
        if code != 0:
            return code
    words_given = a.words[1:] if a.words[:1] == ["--"] else a.words
    return ran("cd %s && %s" % (TREE, " ".join(shlex.quote(word) for word in words_given)))


def register(sub):
    """The wsl group's commands added to harness.py's subparsers."""
    import argparse

    group = sub.add_parser("wsl", help="the ext4 test volume under WSL, the tree copied onto it, commands run there")
    wsl = group.add_subparsers(dest="command", required=True)
    p = wsl.add_parser("mount", help="the volume's state, or the volume made or mounted as WSL's root")
    p.add_argument("--create", action="store_true",
                   help="make the %d GB image, format it ext4 and mount it" % GIGABYTES)
    p.add_argument("--attach", action="store_true", help="mount the image made before")
    p.set_defaults(fn=cmd_wsl_mount, create=False, attach=False)
    p = wsl.add_parser("sync", help="the working tree copied onto the volume")
    p.set_defaults(fn=cmd_wsl_sync, create=False, attach=False)
    p = wsl.add_parser("run", help="a command run in the volume's tree, the tree copied first")
    p.add_argument("--no-sync", action="store_true", help="run on the tree as it stands on the volume")
    p.add_argument("words", nargs=argparse.REMAINDER, help="the command and its words, after --")
    p.set_defaults(fn=cmd_wsl_run, create=False, attach=False)
