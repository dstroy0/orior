# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Arch Linux, the newest of each library, to run orior's C checks on: clang and llvm's archiver,
# and the git, Python and ssh the checks reach for.

FROM archlinux:latest
RUN pacman -Syu --noconfirm base-devel clang llvm git python openssh curl && pacman -Scc --noconfirm
WORKDIR /tree
