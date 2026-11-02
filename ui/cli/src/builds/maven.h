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

#endif
