# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# platform.sh, sourced by a build script: the files of the platform, which hold every call into the system. The
# folder windows holds the calls of Windows. The folder posix holds the calls that the POSIX systems share, and a folder
# under it for each of those systems holds the calls of that system alone. Each file compiles to nothing on a system
# that is not its own, and so a build names the files of every system.
#
#     source includes/platform/platform.sh
PLATFORM_TOP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# the calls of the platform into the system, which every program links, and the calls built on them that are the same
# on every system. They take no cell of the pool
PLATFORM_SYSTEM=("$PLATFORM_TOP/windows/platform_windows.c" "$PLATFORM_TOP/posix/platform_posix.c"
    "$PLATFORM_TOP/posix/linux/platform_linux.c" "$PLATFORM_TOP/posix/macos/platform_macos.c"
    "$PLATFORM_TOP/platform_shared.c")

# the run of the qry writer: its shared memory, its file and its clock
PLATFORM_QRY_RUN=("$PLATFORM_TOP/windows/windows_qry_run.c" "$PLATFORM_TOP/posix/posix_qry_run.c")

# the programs that a program starts and holds: their pipes, their jobs or groups, their ends and their processor time
PLATFORM_PROGRAMS=("$PLATFORM_TOP/windows/platform_windows_programs.c" "$PLATFORM_TOP/posix/platform_posix_programs.c"
    "$PLATFORM_TOP/posix/linux/platform_linux_programs.c" "$PLATFORM_TOP/posix/macos/platform_macos_programs.c")

# the probe of the lstar interface: its fault rule and its launch queue, over the programs of the platform
PLATFORM_PROBE=("$PLATFORM_TOP/windows/windows_probe.c" "$PLATFORM_TOP/posix/posix_probe.c" "${PLATFORM_PROGRAMS[@]}")

# tessera_run: a job's records and its parent's life, over the programs of the platform. A build that links the tessera
# client takes the programs with the client, and the files of tessera_run's own alone
PLATFORM_TESSERA_RUN_OWN=("$PLATFORM_TOP/windows/windows_tessera_run.c" "$PLATFORM_TOP/posix/posix_tessera_run.c")
PLATFORM_TESSERA_RUN=("${PLATFORM_TESSERA_RUN_OWN[@]}" "${PLATFORM_PROGRAMS[@]}")
