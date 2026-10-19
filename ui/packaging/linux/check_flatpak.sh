#!/bin/sh
# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Checks orior's Flatpak in the small image Dockerfile.flatpak makes, the packaging folder mounted
# read-only: the manifest read by flatpak-builder, the desktop file and the AppStream metadata
# checked, and cargo-sources.json read, each crate in it with its hash. No runtime is fetched and
# nothing is built: Flathub builds the Flatpak from the manifest. Run from the ui folder.
#
# Usage: sh packaging/linux/check_flatpak.sh
set -eu
here=packaging/linux
docker build -t orior-flatpak-check -f "$here/Dockerfile.flatpak" "$here"
docker run --rm -v "$(pwd)/$here:/tree:ro" orior-flatpak-check sh -c '
  set -eu
  cp -r /tree /tmp/check && cd /tmp/check
  flatpak-builder --show-manifest com.dstroy0.orior.yml > /dev/null
  desktop-file-validate com.dstroy0.orior.desktop
  appstreamcli validate --no-net com.dstroy0.orior.metainfo.xml
  python3 -c "import json; made = json.load(open(\"cargo-sources.json\")); crates = [one for one in made if one[\"type\"] == \"archive\"]; assert all(len(one[\"sha256\"]) == 64 for one in crates); print(len(crates), \"crates, each with its hash\")"
'
