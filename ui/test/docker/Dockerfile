# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The window's own system for its tests, on any machine Docker runs on: Ubuntu with the libraries
# orior's window is built with, WebKitGTK's WebDriver, a display of X's that draws in memory and a
# session bus, Rust as rustup installs it, rsync, which brings the tree in, ssh for the tunnels'
# tests, the tools the window's tests drive, clangd and clang, lldb's and gdb's debuggers, debugpy and
# pytest, and VNC and noVNC, which show the display to a browser. harness.py window test --docker runs the
# harness here, which builds the window and drives it through WebDriver.

FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
# apt gives up on a mirror that stops answering and asks again, where its own defaults wait on.
RUN printf 'Acquire::Retries "3";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' > /etc/apt/apt.conf.d/80orior
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl build-essential pkg-config git python3 rsync openssh-client \
      libwebkit2gtk-4.1-dev webkit2gtk-driver libayatana-appindicator3-dev librsvg2-dev libxdo-dev libssl-dev \
      xvfb xauth dbus dbus-x11 at-spi2-core fonts-dejavu-core fonts-noto-core x11vnc novnc websockify \
      cmake ninja-build clangd clang clang-tidy libclang-rt-18-dev llvm-18 cppcheck lldb gdb python-is-python3 python3-debugpy python3-pytest \
 && rm -rf /var/lib/apt/lists/*
# lldb's debugger for editors goes by its version's name; orior looks for lldb-dap.
RUN found="$(ls /usr/bin/lldb-dap* /usr/lib/llvm-*/bin/lldb-dap 2>/dev/null | head -1)" && test -n "$found" && ln -sf "$found" /usr/local/bin/lldb-dap
RUN curl -sSf --retry 3 --connect-timeout 30 https://sh.rustup.rs | sh -s -- -y --profile minimal
# The name git gives the commits a test makes in the run's tree.
RUN git config --global user.name "orior harness" && git config --global user.email harness@localhost
# SonarQube for IDE, as the SonarQube plugin's toolchain finds it: the extension unpacked where VS Code keeps its
# extensions, with the Java and the analyzers it carries.
ARG SONARLINT=6.1.1
RUN apt-get update && apt-get install -y --no-install-recommends unzip && rm -rf /var/lib/apt/lists/* \
 && curl -sSfL --retry 3 -o /tmp/sonarlint.vsix "https://open-vsx.org/api/SonarSource/sonarlint-vscode/linux-x64/${SONARLINT}/file/SonarSource.sonarlint-vscode-${SONARLINT}@linux-x64.vsix" \
 && mkdir -p /tmp/sonarlint /root/.vscode/extensions \
 && unzip -q /tmp/sonarlint.vsix -d /tmp/sonarlint \
 && mv /tmp/sonarlint/extension "/root/.vscode/extensions/sonarsource.sonarlint-vscode-${SONARLINT}-linux-x64" \
 && chmod -R u+rwX "/root/.vscode/extensions/sonarsource.sonarlint-vscode-${SONARLINT}-linux-x64/jre" \
 && rm -rf /tmp/sonarlint /tmp/sonarlint.vsix
# A container gives the web view no namespaces of its own to sandbox in, and no GPU: it draws in
# software, through X.
ENV PATH=/root/.cargo/bin:$PATH \
    WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 \
    LIBGL_ALWAYS_SOFTWARE=1 \
    WEBKIT_DISABLE_COMPOSITING_MODE=1 \
    GDK_BACKEND=x11
# The display shown to a browser on this machine as the harness runs.
COPY watch.sh /usr/local/bin/orior-watch
RUN chmod +x /usr/local/bin/orior-watch
WORKDIR /ui
