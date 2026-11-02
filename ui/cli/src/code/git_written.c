// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_written.c: what git reports of the tree written as the page reads it: each list a JSON array of objects, each
// field by its name
#include "../text/json_tree.h"
#include "git.h"

typedef struct
{
    const CliGitText *text;
    unsigned int at;
    const char *name;
    int first;
    VerbaLinea *out;
    int has;
    long long when;
    char state;
    unsigned long long value;
    const char *repo;
} GitWrittenCtx;
