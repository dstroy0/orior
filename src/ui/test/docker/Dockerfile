# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The window's own system for its tests, on any machine Docker runs on: Ubuntu with the libraries
# orior's window is built with, WebKitGTK's WebDriver, a display of X's that draws in memory and a
# session bus, Rust as rustup installs it, and rsync, which brings the tree in. docker.rs runs the
# harness here, and driven.rs drives the window it builds.

FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
# apt gives up on a mirror that stops answering and asks again, where its own defaults wait on.
RUN printf 'Acquire::Retries "3";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' > /etc/apt/apt.conf.d/80orior
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl build-essential pkg-config git python3 rsync \
      libwebkit2gtk-4.1-dev webkit2gtk-driver libayatana-appindicator3-dev librsvg2-dev libxdo-dev libssl-dev \
      xvfb xauth dbus dbus-x11 at-spi2-core fonts-dejavu-core fonts-noto-core \
 && rm -rf /var/lib/apt/lists/*
RUN curl -sSf --retry 3 --connect-timeout 30 https://sh.rustup.rs | sh -s -- -y --profile minimal
# A container gives the web view no namespaces of its own to sandbox in, and no GPU: it draws in
# software, through X.
ENV PATH=/root/.cargo/bin:$PATH \
    WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 \
    LIBGL_ALWAYS_SOFTWARE=1 \
    GDK_BACKEND=x11
WORKDIR /src/ui
