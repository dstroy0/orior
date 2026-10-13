# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Alpine, whose C library is musl's and not glibc's, to run orior's crates' tests on: Rust as rustup
# installs it, and the git, Python and ssh the tests reach for.

FROM alpine:3.20
RUN apk add --no-cache build-base curl git python3 openssh-client bash
RUN curl -sSf --retry 3 --connect-timeout 30 https://sh.rustup.rs | sh -s -- -y --profile minimal
ENV PATH=/root/.cargo/bin:$PATH
WORKDIR /ui
