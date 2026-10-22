# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The window's own system for its tests on Windows, in a Windows container: Windows Server with the
# desktop's libraries, the WebView2 runtime the window draws with, git, which the tests drive, and
# SonarQube for IDE where the SonarQube plugin's toolchain finds it. Nothing is built here: the
# window, the harness and the run's writer come built from the machine that runs the container, which
# docker.rs hands in at C:\host, and the harness drives the window through its debugging port.

FROM mcr.microsoft.com/windows/server:ltsc2022
SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

# The WebView2 runtime, installed for every account of the container.
RUN Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile C:\webview2.exe; \
    Start-Process -FilePath C:\webview2.exe -ArgumentList '/silent', '/install' -Wait; \
    Remove-Item C:\webview2.exe

# git, the least of it, on the PATH.
ARG GIT=2.47.1
RUN Invoke-WebRequest -Uri ('https://github.com/git-for-windows/git/releases/download/v{0}.windows.1/MinGit-{0}-64-bit.zip' -f $env:GIT) -OutFile C:\mingit.zip; \
    Expand-Archive -Path C:\mingit.zip -DestinationPath C:\git; \
    Remove-Item C:\mingit.zip; \
    setx /M PATH ($env:PATH + ';C:\git\cmd;C:\git\usr\bin'); \
    C:\git\cmd\git.exe config --system user.name 'orior harness'; \
    C:\git\cmd\git.exe config --system user.email harness@localhost; \
    C:\git\cmd\git.exe config --system commit.gpgsign false

# SonarQube for IDE, as the SonarQube plugin's toolchain finds it: the extension unpacked where VS
# Code keeps its extensions, with the Java and the analyzers it carries.
ARG SONARLINT=6.1.1
RUN Invoke-WebRequest -Uri ('https://open-vsx.org/api/SonarSource/sonarlint-vscode/win32-x64/{0}/file/SonarSource.sonarlint-vscode-{0}@win32-x64.vsix' -f $env:SONARLINT) -OutFile C:\sonarlint.zip; \
    Expand-Archive -Path C:\sonarlint.zip -DestinationPath C:\sonarlint; \
    New-Item -ItemType Directory -Force -Path $env:USERPROFILE\.vscode\extensions | Out-Null; \
    Move-Item -Path C:\sonarlint\extension -Destination ('{0}\.vscode\extensions\sonarsource.sonarlint-vscode-{1}-win32-x64' -f $env:USERPROFILE, $env:SONARLINT); \
    Remove-Item -Recurse -Force C:\sonarlint, C:\sonarlint.zip

WORKDIR C:\host
