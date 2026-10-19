// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_internal.h: what the build files' readers share across their kinds: the program that runs a Gradle build and
// the folder its build stands in, and the program that runs a Maven build
#ifndef BUILDS_INTERNAL_H
#define BUILDS_INTERNAL_H

#include "builds.h"

// the program that runs the Gradle build in `folder`, its wrapper where one stands there and `gradle` where none does,
// into `out`, which holds CLI_PATH_LONGEST (gradle.c)
void cli_gradle_program(const char *folder, char *out);

#endif
