# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Fedora to run orior's C checks on: clang and llvm's archiver, and the git, Python and ssh the
# checks reach for.

FROM fedora:41
RUN dnf install -y gcc clang llvm git python3 openssh-clients curl procps-ng && dnf clean all
WORKDIR /tree
