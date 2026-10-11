#!/bin/sh
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Makes the server's own keys and starts it, a key alone letting a user in. The harness writes the
# key it signs in with to /home/orior/.ssh/authorized_keys each time it starts its servers.

ssh-keygen -A
echo "ssh ready"
exec /usr/sbin/sshd -D -e -o PasswordAuthentication=no -o KbdInteractiveAuthentication=no -o AllowTcpForwarding=yes
