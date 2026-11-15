#!/usr/bin/env bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# window_host_probe.sh: a Linux host read for the window's checks, run there by harness.py window check --on through the
# host's own login shell, so the host's own PATH resolves each program. One line a fact, tab-separated:
#   system <tab> uname -s <tab> uname -m
#   manager <tab> apt|dnf|pacman|apk|zypper|none
#   program <tab> name <tab> path or - <tab> the first line of its version, or -
#   header <tab> X11/Xlib.h <tab> yes|no
#   home <tab> the home folder
# The programs read are the words after the script's name.
set -u
echo "system	$(uname -s)	$(uname -m)"
manager=none
for one in apt-get dnf pacman apk zypper; do
    if command -v "$one" > /dev/null 2>&1; then
        manager="$one"
        break
    fi
done
case "$manager" in
    apt-get) manager=apt ;;
esac
echo "manager	$manager"
for asked in "$@"; do
    program="$asked"
    path="$(command -v "$program" 2> /dev/null || true)"
    # a Linux host names its Python python3 alone
    if [ -z "$path" ] && [ "$asked" = python ]; then
        program=python3
        path="$(command -v "$program" 2> /dev/null || true)"
    fi
    version=-
    if [ -n "$path" ]; then
        version="$("$program" --version 2>&1 < /dev/null | head -1 || true)"
        [ -n "$version" ] || version="$("$program" -v 2>&1 < /dev/null | head -1 || true)"
        [ -n "$version" ] || version=-
    fi
    echo "program	$asked	${path:--}	$version"
done
if [ -f /usr/include/X11/Xlib.h ]; then
    echo "header	X11/Xlib.h	yes"
else
    echo "header	X11/Xlib.h	no"
fi
echo "home	$HOME"
