# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The window's own system on Windows, in a Windows container: Windows Server with the desktop's libraries, the
# WebView2 runtime the window draws with, Git for Windows, whose bash runs ui/cli/build.sh, SonarQube for IDE where
# the SonarQube plugin's toolchain finds it, and what builds the window: the MSVC build tools and Windows SDK cargo
# links with, LLVM's clang and lld, which build the C side and the page's lattice to wasm, CMake and Ninja, which build
# MMgr, Python, which runs the harness, and Rust as rustup installs it. The window's suite takes the window built on
# the machine that runs the container, passed in at C:\host; harness.py container builds it in the container's own
# copy of the tree, at C:\top.

FROM mcr.microsoft.com/windows/server:ltsc2022
SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

# The WebView2 runtime, installed for every account of the container.
RUN Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile C:\webview2.exe; \
    Start-Process -FilePath C:\webview2.exe -ArgumentList '/silent', '/install' -Wait; \
    Remove-Item C:\webview2.exe

# Git for Windows whole, with the bash and the tools ui/cli/build.sh runs under, on the PATH.
ARG GIT=2.47.1
RUN Invoke-WebRequest -Uri ('https://github.com/git-for-windows/git/releases/download/v{0}.windows.1/PortableGit-{0}-64-bit.7z.exe' -f $env:GIT) -OutFile C:\portablegit.exe; \
    Start-Process -FilePath C:\portablegit.exe -ArgumentList '-oC:\git', '-y' -Wait; \
    Remove-Item C:\portablegit.exe; \
    [Environment]::SetEnvironmentVariable('PATH', $env:PATH + ';C:\git\cmd;C:\git\bin;C:\git\usr\bin', 'Machine'); \
    C:\git\cmd\git.exe config --system user.name 'orior harness'; \
    C:\git\cmd\git.exe config --system user.email harness@localhost; \
    C:\git\cmd\git.exe config --system commit.gpgsign false

# SonarQube for IDE, as the SonarQube plugin's toolchain finds it: the extension unpacked where VS
# Code retains its extensions, with the Java and the analyzers it carries.
ARG SONARLINT=6.1.1
RUN Invoke-WebRequest -Uri ('https://open-vsx.org/api/SonarSource/sonarlint-vscode/win32-x64/{0}/file/SonarSource.sonarlint-vscode-{0}@win32-x64.vsix' -f $env:SONARLINT) -OutFile C:\sonarlint.zip; \
    Expand-Archive -Path C:\sonarlint.zip -DestinationPath C:\sonarlint; \
    New-Item -ItemType Directory -Force -Path $env:USERPROFILE\.vscode\extensions | Out-Null; \
    Move-Item -Path C:\sonarlint\extension -Destination ('{0}\.vscode\extensions\sonarsource.sonarlint-vscode-{1}-win32-x64' -f $env:USERPROFILE, $env:SONARLINT); \
    Remove-Item -Recurse -Force C:\sonarlint, C:\sonarlint.zip

# The MSVC build tools and the Windows SDK, which cargo's msvc target links with. 3010 is the installer's success
# that asks for a restart, which a container never needs.
RUN Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_buildtools.exe' -OutFile C:\vs_buildtools.exe; \
    $installed = Start-Process -FilePath C:\vs_buildtools.exe -ArgumentList '--quiet', '--wait', '--norestart', '--nocache', '--installPath', 'C:\BuildTools', '--add', 'Microsoft.VisualStudio.Workload.VCTools', '--includeRecommended' -Wait -PassThru; \
    if ($installed.ExitCode -notin 0, 3010) { exit $installed.ExitCode }; \
    Remove-Item C:\vs_buildtools.exe

# LLVM's clang, lld and wasm-ld, the version the Linux image's Ubuntu carries.
ARG LLVM=18.1.8
RUN Invoke-WebRequest -Uri ('https://github.com/llvm/llvm-project/releases/download/llvmorg-{0}/LLVM-{0}-win64.exe' -f $env:LLVM) -OutFile C:\llvm.exe; \
    Start-Process -FilePath C:\llvm.exe -ArgumentList '/S', '/D=C:\LLVM' -Wait; \
    Remove-Item C:\llvm.exe

# CMake and Ninja, unpacked.
ARG CMAKE=3.30.5
ARG NINJA=1.12.1
RUN Invoke-WebRequest -Uri ('https://github.com/Kitware/CMake/releases/download/v{0}/cmake-{0}-windows-x86_64.zip' -f $env:CMAKE) -OutFile C:\cmake.zip; \
    Expand-Archive -Path C:\cmake.zip -DestinationPath C:\; \
    Rename-Item -Path ('C:\cmake-{0}-windows-x86_64' -f $env:CMAKE) -NewName cmake; \
    Remove-Item C:\cmake.zip; \
    Invoke-WebRequest -Uri ('https://github.com/ninja-build/ninja/releases/download/v{0}/ninja-win.zip' -f $env:NINJA) -OutFile C:\ninja.zip; \
    Expand-Archive -Path C:\ninja.zip -DestinationPath C:\ninja; \
    Remove-Item C:\ninja.zip

# Python, for every account.
ARG PYTHON=3.12.7
RUN Invoke-WebRequest -Uri ('https://www.python.org/ftp/python/{0}/python-{0}-amd64.exe' -f $env:PYTHON) -OutFile C:\python.exe; \
    Start-Process -FilePath C:\python.exe -ArgumentList '/quiet', 'InstallAllUsers=1', 'Include_test=0', 'TargetDir=C:\Python' -Wait; \
    Remove-Item C:\python.exe

# Rust, as rustup installs it, for the msvc target.
RUN Invoke-WebRequest -Uri 'https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe' -OutFile C:\rustup-init.exe; \
    Start-Process -FilePath C:\rustup-init.exe -ArgumentList '-y', '--profile', 'minimal', '--default-toolchain', 'stable' -Wait; \
    Remove-Item C:\rustup-init.exe

RUN [Environment]::SetEnvironmentVariable('PATH', $env:PATH + (';C:\LLVM\bin;C:\cmake\bin;C:\ninja;C:\Python;C:\Python\Scripts;{0}\.cargo\bin' -f $env:USERPROFILE), 'Machine')

WORKDIR C:\host
