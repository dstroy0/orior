# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Debian, on x86-64 and on aarch64, to run orior's C checks on: clang and llvm's archiver, and
# the git, Python and ssh the checks reach for.

FROM debian:bookworm-slim
# apt gives up on a mirror that stops answering and asks again, where its own defaults wait on.
RUN printf 'Acquire::Retries "3";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' > /etc/apt/apt.conf.d/80orior
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl build-essential clang llvm git python3 openssh-client \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /tree
