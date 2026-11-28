// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_system.c: what the command line requests of macOS itself: when the machine started, from sysctl, the
// reader's log in, which macOS does not name, the folders of its fonts, and its facts
#if defined(__APPLE__)
// sysctlbyname is the system's own, outside POSIX
#define _DARWIN_C_SOURCE
#endif
#include "../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../ui/cli/src/text/text.h"
#include "../posix_systems.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/time.h>

// the machine's start as sysctl saves it
unsigned long long cli_booted_seconds(void)
{
    struct timeval booted;
    embed_word size = sizeof(booted);
    // a start the system gives is after 1970, never below 0
    return (sysctlbyname("kern.boottime", &booted, &size, NULL, 0u) == 0) ? (unsigned long long)booted.tv_sec : 0ULL;
}

// macOS names no log in of the reader; the machine's start alone marks a session there
void cli_logon_by_name(const CliCapacityLogonCtx *args)
{
    VerbaLinea named = {args->logon, args->capacity_bytes, 0ULL};
    (void)mmgr_line_finish(&named);
}

// the folders the system saves its fonts in, for every reader and for the reader alone
static const CliSystemFolder s_font_folders[3] = {
    {NULL, "/System/Library/Fonts"}, {NULL, "/Library/Fonts"}, {"HOME", "/Library/Fonts"}};

// macOS's facts, its .NET name by the processor that the build targets
#if defined(__aarch64__) || defined(_M_ARM64)
#define SYSTEM_DOTNET_RUNTIME "osx-arm64"
#else
#define SYSTEM_DOTNET_RUNTIME "osx-x64"
#endif
const CliSystemFacts g_cli_system_facts =
    CLI_POSIX_SYSTEM_FACTS("macos", "open", 1, s_font_folders, "darwin", SYSTEM_DOTNET_RUNTIME, 0);
#endif
