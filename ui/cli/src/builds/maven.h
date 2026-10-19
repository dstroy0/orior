// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven.h: the compiler's settings a POM gives and Maven's own, from `settings.xml`, read as settings and written again
// as they are set (maven_settings.c, maven_set.c)
#ifndef MAVEN_H
#define MAVEN_H

#include "../command_line/cli_internal.h"

// the compiler's settings the POM `pom`, `size` bytes of its text, gives, and Maven's own, written as an array of
// `{"key", "group", "label", "value", "file", "settable"}`: where it is kept, its name and its value as written, the
// file it is read from, and whether it can be set here
void cli_maven_settings(const char *root, const char *pom, const char *text, unsigned long long size, CliLine *out);

// the setting `key` set to `value`: in the POM `pom`, `size` bytes of its text, by the edits written into `out` as an
// array of `{"path", "edits"}`; in the user's `settings.xml`, written to it, `[]` written into `out`: 1, or 0 with why
// said
int cli_maven_set(const char *pom, const char *text, unsigned long long size, const char *key, const char *value,
                  CliLine *out, CliLine *said);

#endif
