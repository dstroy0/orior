// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_event.c: what a run reports written as the page reads it, each field by its name
#include "../text/json_tree.h"
#include "runner.h"

typedef struct
{
    const char *text;
    VerbaLinea *out;
} RunnerEventCtx;
