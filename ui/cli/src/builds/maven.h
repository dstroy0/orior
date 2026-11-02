// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven.h: the compiler's settings a POM gives and Maven's own, from `settings.xml`, read as settings and written again
// as they are set (maven_settings.c, maven_set.c)
#ifndef MAVEN_H
#define MAVEN_H
typedef struct CliMavenKeyCfg CliMavenKeyCfg;
typedef struct CliMavenCfg CliMavenCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the compiler's settings the POM `pom`, `size` bytes of its text, gives, and Maven's own, written as an array of
// `{"key", "group", "label", "value", "file", "settable"}`: where it is stored, its name and its value as written, the
// file it is read from, and whether it can be set here
void cli_maven_settings(const CliMavenCfg *args);

// the setting `key` set to `value`: in the POM `pom`, `size` bytes of its text, by the edits written into `out` as an
// array of `{"path", "edits"}`; in the user's `settings.xml`, written to it, `[]` written into `out`: 1, or 0 with why
// given
int cli_maven_set(const CliMavenKeyCfg *args);

struct CliMavenKeyCfg
{
    const char *pom;
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
    const char *key;
    const char *value;
    VerbaLinea *err_msg;
};

struct CliMavenCfg
{
    const char *root;
    const char *pom;
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
};

typedef struct
{
    void (*settings)(const CliMavenCfg *args);
} CliMavenNs;
EMBED_TABLE_LAYOUT(CliMavenNs, settings);
EMBED_TABLE_STORAGE CliMavenNs cli_maven EMBED_UNUSED = {
    .settings = cli_maven_settings,
};

typedef struct
{
    int (*set)(const CliMavenKeyCfg *args);
} CliMavenKeyNs;
EMBED_TABLE_LAYOUT(CliMavenKeyNs, set);
EMBED_TABLE_STORAGE CliMavenKeyNs cli_maven_key EMBED_UNUSED = {
    .set = cli_maven_set,
};

#endif
