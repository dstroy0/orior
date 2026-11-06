// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_set.c: a setting set. A compiler setting is written to the property that sets it where the POM's properties
// contain it or the plugin's configuration does not, and to the configuration where only it contains it; a POM with no
// properties is given them after its coordinates, a step deeper than its root. The compiler plugin's version is
// written over its own. Maven's own settings are written to the user's `settings.xml`, made where there is none. An
// element added under another goes on a line of its own, at the depth of the other's first child
#include "../command_line/pool.h"
#include "../text/edits.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "maven.h"
#include "maven_internal.h"

// the bytes the user's settings written are taken with beyond what they contained
#define CLI_MAVEN_SET_MORE 4096ull

typedef struct
{
    const char *text;
    unsigned long long at;
    VerbaLinea *out;
    const CliXml *xml;
    unsigned int holder;
    const char *name;
    const char *value;
    const CliBuildLines *lines;
    const char *pom;
    const CliTextEdit *edit;
    unsigned int setting;
    VerbaLinea *err_msg;
} MavenSetCtx;
