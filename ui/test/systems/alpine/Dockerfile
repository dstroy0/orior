# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Alpine, whose C library is musl's and not glibc's, to run orior's C checks on: clang and llvm's
# archiver, and the git, Python and ssh the checks reach for.

FROM alpine:3.20
RUN apk add --no-cache build-base clang llvm curl git python3 openssh-client bash tar
WORKDIR /tree
