#!/bin/sh
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Shows the display the window draws on in memory to a browser on the machine that runs the
# container, then runs the command it is given: x11vnc serves the display over VNC, and noVNC, behind
# websockify, serves that at http://localhost:6080/vnc.html. It is shown and never driven: no press
# or key in the browser reaches the window the tests drive. Neither asks a password; docker.rs
# publishes the port on the machine's loopback alone.

x11vnc -display "$DISPLAY" -forever -shared -viewonly -nopw -rfbport 5900 -quiet -bg -o /tmp/x11vnc.log
websockify --web /usr/share/novnc 6080 localhost:5900 > /tmp/novnc.log 2>&1 &
exec "$@"
