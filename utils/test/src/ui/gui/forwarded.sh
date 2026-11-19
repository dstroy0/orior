#!/bin/bash
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# forwarded.sh: the window over a forwarded display, run on a Linux host that has an X server, ssh and sshd. The host
# is its own client and its own server: an X server of the script's own on DISPLAY :98, an ssh server on port 2222
# that forwards X11, and the window started through ssh -Y so its display is the forwarded one. The window is then
# looked for on :98 by its title. Exit 0 where it is there, 1 where it is not, 2 where a program is missing.
#
#   bash utils/test/src/ui/gui/forwarded.sh [<window program>]
#
# The window program is build/cli/<target>/tab under the tree's top where none is given.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
TOP="$(cd "$HERE/../../../../.." && pwd)"
PROGRAM="${1:-$TOP/build/cli/x86_64-unknown-linux-gnu/tab}"
NUMBER=98
PORT=2222
SECONDS_SHOWN=8
for program in Xvfb sshd ssh ssh-keygen xwininfo; do
    if ! command -v "$program" >/dev/null 2>&1 && [ ! -x "/usr/sbin/$program" ]; then
        echo "forwarded: no $program"
        exit 2
    fi
done
if [ ! -x "$PROGRAM" ]; then
    echo "forwarded: no window program at $PROGRAM"
    exit 2
fi
export DISPLAY=:$NUMBER
# the X server of the script's own, with no authority file: a local connection needs no cookie, and the forwarded
# connection's cookie is the server's own
pgrep -f "Xvfb :$NUMBER" >/dev/null || (Xvfb :$NUMBER -screen 0 1920x1080x24 -dpi 96 -nolisten tcp >/dev/null 2>&1 &)
sleep 1
# the ssh server with the host's keys and the caller's own key as the one authorized
mkdir -p /run/sshd "$HOME/.ssh"
ssh-keygen -A >/dev/null 2>&1
[ -f "$HOME/.ssh/id_ed25519" ] || ssh-keygen -q -t ed25519 -N "" -f "$HOME/.ssh/id_ed25519"
cat "$HOME/.ssh/id_ed25519.pub" > "$HOME/.ssh/authorized_keys"
chmod 700 "$HOME/.ssh"
chmod 600 "$HOME/.ssh/authorized_keys"
pgrep -x sshd >/dev/null || /usr/sbin/sshd -p $PORT -o X11Forwarding=yes -o X11UseLocalhost=yes -o PermitRootLogin=yes -o PasswordAuthentication=no
sleep 1
ENV_FILE="$(mktemp)"
ERR_FILE="$(mktemp)"
USER_NAME="$(id -un)"
# the window through ssh -Y: the session's DISPLAY is the forwarded one, and the window lives while the session does
ssh -Y -p $PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR "$USER_NAME@localhost" \
    "echo \"\$DISPLAY\" > '$ENV_FILE'; cd '$TOP' && timeout $((SECONDS_SHOWN + 4)) '$PROGRAM' >/dev/null 2>'$ERR_FILE'" &
SESSION=$!
sleep $SECONDS_SHOWN
FORWARDED="$(cat "$ENV_FILE" 2>/dev/null)"
FOUND="$(xwininfo -root -tree -display :$NUMBER 2>/dev/null | grep '"orior"' | head -1 | sed 's/^ *//')"
echo "forwarded: the session's display is ${FORWARDED:-none}"
echo "forwarded: on :$NUMBER ${FOUND:-no window named orior}"
pkill -x "$(basename "$PROGRAM")" 2>/dev/null
wait $SESSION 2>/dev/null
CODE=1
case "$FORWARDED" in
    localhost:*) [ -n "$FOUND" ] && CODE=0 ;;
esac
if [ $CODE -ne 0 ]; then
    echo "forwarded: the window's error stream: $(head -c 300 "$ERR_FILE" | tr '\n' ' ')"
fi
rm -f "$ENV_FILE" "$ERR_FILE"
exit $CODE
