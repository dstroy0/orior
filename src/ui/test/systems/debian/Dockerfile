# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Debian, on x86-64 and on aarch64, to run orior's crates' tests on: Rust as rustup installs it, and
# the git, Python and ssh the tests reach for.

FROM debian:bookworm-slim
# apt gives up on a mirror that stops answering and asks again, where its own defaults wait on.
RUN printf 'Acquire::Retries "3";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' > /etc/apt/apt.conf.d/80orior
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl build-essential git python3 openssh-client \
 && rm -rf /var/lib/apt/lists/*
RUN curl -sSf --retry 3 --connect-timeout 30 https://sh.rustup.rs | sh -s -- -y --profile minimal
ENV PATH=/root/.cargo/bin:$PATH
WORKDIR /ui
