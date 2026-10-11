#!/bin/sh
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Starts the test printers, each keeping what it is sent in /spool/<its name>, then CUPS, open to
# the machine the container runs on, with a queue named office that prints on the PDF printer. CUPS
# answers a request by any name of the machine, as a container reaches it by Docker's name for it.

mkdir -p /spool/pdf /spool/pwg /spool/urf /run/dbus
rm -f /run/dbus/pid
dbus-daemon --system --fork
avahi-daemon --daemonize --no-drop-root --no-rlimits
sleep 1
ippeveprinter -p 8631 -n localhost -k -d /spool/pdf -f application/pdf,image/pwg-raster "orior pdf" &
ippeveprinter -p 8632 -n localhost -k -d /spool/pwg -f image/pwg-raster "orior pwg" &
ippeveprinter -p 8633 -n localhost -k -d /spool/urf -f image/urf "orior urf" &
grep -q "^ServerAlias \*" /etc/cups/cupsd.conf || echo "ServerAlias *" >> /etc/cups/cupsd.conf
cupsd
sleep 2
cupsctl --remote-any --share-printers
sleep 2
lpadmin -p office -E -v ipp://localhost:8631/ipp/print -m everywhere -D "orior office"
cupsenable office
cupsaccept office
echo "printers ready"
wait
