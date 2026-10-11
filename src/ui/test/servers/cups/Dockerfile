# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The print server orior's printing is tested against: CUPS, with three IPP Everywhere printers of
# CUPS's own ippeveprinter, one that takes PDF, one that takes only PWG raster and one that takes
# only Apple raster, each keeping what it is sent, and cups-filters to read a raster back. Avahi
# announces the printers, as ippeveprinter asks.

FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends cups cups-ipp-utils cups-filters avahi-daemon dbus \
 && rm -rf /var/lib/apt/lists/*
COPY start.sh /start.sh
EXPOSE 631 8631 8632 8633
CMD ["sh", "/start.sh"]
