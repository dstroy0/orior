# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The window's own system for its tests, on any machine Docker runs on: Ubuntu with the libraries
# orior's window is built with, WebKitGTK's WebDriver, a display of X's that draws in memory and a
# session bus, Rust as rustup installs it, rsync, which brings the tree in, ssh for the tunnels'
# tests, the tools the window's tests drive, clangd, clang and lld, which builds the page's lattice to wasm, lldb's and gdb's debuggers, debugpy and
# pytest, Ruby, which Unity's runner generator runs on, TeX and Poppler, which make a LaTeX file's pages and draw them,
# and VNC and noVNC, which show the display to a browser. harness.py window test --docker runs the
# harness here, which builds the window and drives it through WebDriver.
#
# BASE is the system and LLVM the version of its own clang, whose runtime and archiver are named by it: Ubuntu 24.04
# with 18, or Debian bookworm, Raspberry Pi OS's own, with 14. Each builds for x86-64 and for arm64, as
# window_images.tsv names them and harness.py container builds them.

ARG BASE=ubuntu:24.04
FROM ${BASE}
ARG LLVM=18
ARG TARGETARCH
ENV DEBIAN_FRONTEND=noninteractive
# apt gives up on a mirror that stops answering and asks again, where its own defaults wait on.
RUN printf 'Acquire::Retries "3";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' > /etc/apt/apt.conf.d/80orior
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl build-essential pkg-config git python3 rsync openssh-client \
      libwebkit2gtk-4.1-dev webkit2gtk-driver libayatana-appindicator3-dev librsvg2-dev libxdo-dev libssl-dev \
      xvfb xauth dbus dbus-x11 at-spi2-core fonts-dejavu-core fonts-noto-core x11vnc novnc websockify \
      cmake ninja-build clangd clang clang-tidy lld libclang-rt-${LLVM}-dev llvm-${LLVM} cppcheck lldb gdb python-is-python3 python3-debugpy python3-pytest \
      ruby \
 && rm -rf /var/lib/apt/lists/*
# lldb's debugger for editors goes by its version's name, and before LLVM 18 by lldb-vscode; orior looks for lldb-dap.
RUN found="$(ls /usr/bin/lldb-dap* /usr/lib/llvm-*/bin/lldb-dap /usr/bin/lldb-vscode* /usr/lib/llvm-*/bin/lldb-vscode 2>/dev/null | head -1)" && test -n "$found" && ln -sf "$found" /usr/local/bin/lldb-dap
RUN curl -sSf --retry 3 --connect-timeout 30 https://sh.rustup.rs | sh -s -- -y --profile minimal
# The name git gives the commits a test makes in the run's tree.
RUN git config --global user.name "orior harness" && git config --global user.email harness@localhost
# SonarQube for IDE, as the SonarQube plugin's toolchain finds it: the extension unpacked where VS Code keeps its
# extensions, with the Java and the analyzers it carries. On arm64 no extension carries a Java: the one for every
# platform is unpacked, and the system's Java 17 is linked in where the carried one would be.
ARG SONARLINT=6.1.1
RUN if [ "$TARGETARCH" = amd64 ]; then java=""; else java=openjdk-17-jre-headless; fi \
 && apt-get update && apt-get install -y --no-install-recommends unzip $java && rm -rf /var/lib/apt/lists/* \
 && mkdir -p /tmp/sonarlint /root/.vscode/extensions \
 && if [ "$TARGETARCH" = amd64 ]; then \
      curl -sSfL --retry 3 -o /tmp/sonarlint.vsix "https://open-vsx.org/api/SonarSource/sonarlint-vscode/linux-x64/${SONARLINT}/file/SonarSource.sonarlint-vscode-${SONARLINT}@linux-x64.vsix" \
      && unzip -q /tmp/sonarlint.vsix -d /tmp/sonarlint \
      && mv /tmp/sonarlint/extension "/root/.vscode/extensions/sonarsource.sonarlint-vscode-${SONARLINT}-linux-x64" \
      && chmod -R u+rwX "/root/.vscode/extensions/sonarsource.sonarlint-vscode-${SONARLINT}-linux-x64/jre"; \
    else \
      curl -sSfL --retry 3 -o /tmp/sonarlint.vsix "https://open-vsx.org/api/SonarSource/sonarlint-vscode/${SONARLINT}/file/SonarSource.sonarlint-vscode-${SONARLINT}.vsix" \
      && unzip -q /tmp/sonarlint.vsix -d /tmp/sonarlint \
      && mv /tmp/sonarlint/extension "/root/.vscode/extensions/sonarsource.sonarlint-vscode-${SONARLINT}" \
      && mkdir -p "/root/.vscode/extensions/sonarsource.sonarlint-vscode-${SONARLINT}/jre" \
      && ln -s "$(dirname "$(dirname "$(readlink -f "$(command -v java)")")")" "/root/.vscode/extensions/sonarsource.sonarlint-vscode-${SONARLINT}/jre/system"; \
    fi \
 && rm -rf /tmp/sonarlint /tmp/sonarlint.vsix
# A container gives the web view no namespaces of its own to sandbox in, and no GPU: it draws in
# software, through X.
ENV PATH=/root/.cargo/bin:$PATH \
    WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 \
    LIBGL_ALWAYS_SOFTWARE=1 \
    WEBKIT_DISABLE_COMPOSITING_MODE=1 \
    GDK_BACKEND=x11
# TeX's pdflatex, which makes a LaTeX file into pages with its SyncTeX, and Poppler's pdftocairo, which draws them.
RUN apt-get update \
 && apt-get install -y --no-install-recommends texlive-latex-base poppler-utils \
 && rm -rf /var/lib/apt/lists/*
# The display shown to a browser on this machine as the harness runs.
COPY watch.sh /usr/local/bin/orior-watch
RUN chmod +x /usr/local/bin/orior-watch
WORKDIR /ui
