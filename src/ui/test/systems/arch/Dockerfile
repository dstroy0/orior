# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Arch Linux, the newest of each library, to run orior's crates' tests on: Rust as rustup installs
# it, and the git, Python and ssh the tests reach for.

FROM archlinux:latest
RUN pacman -Syu --noconfirm base-devel git python openssh curl && pacman -Scc --noconfirm
RUN curl -sSf --retry 3 --connect-timeout 30 https://sh.rustup.rs | sh -s -- -y --profile minimal
ENV PATH=/root/.cargo/bin:$PATH
WORKDIR /ui
