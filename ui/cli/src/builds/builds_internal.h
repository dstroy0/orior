// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_internal.h: what the build files' readers share across their types: the program that runs a Gradle build and
// the folder its build is in, and the program that runs a Maven build
#ifndef BUILDS_INTERNAL_H
#define BUILDS_INTERNAL_H
#include "builds.h"
#include "embed_dispatch_layout.h"

typedef struct CliBuildsCtx CliBuildsCtx;

// the program that runs the Gradle build in `folder`, its wrapper where one is there and `gradle` where none does,
// into `out`, sized for CLI_PATH_MAX_LEN (gradle.c)
void cli_gradle_program(const CliBuildsCtx *args);

struct CliBuildsCtx
{
    const char *folder;
    char *out;
};

typedef struct
{
    void (*gradle_program)(const CliBuildsCtx *args);
} CliBuildsNs;
EMBED_TABLE_LAYOUT(CliBuildsNs, gradle_program);
EMBED_TABLE_STORAGE CliBuildsNs cli_builds_internal EMBED_UNUSED = {
    .gradle_program = cli_gradle_program,
};

#endif
