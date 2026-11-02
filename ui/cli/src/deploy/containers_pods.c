// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers_pods.c: kubectl run for the cluster's pods and a pod's logs, and what it writes read
#include "../text/text.h"
#include "containers_internal.h"

typedef struct
{
    const CliJson *json;
    unsigned int node;
    const char *key;
    const char *otherwise;
    unsigned long long *length;
    unsigned int one;
    unsigned int other;
    int first;
    VerbaLinea *out;
} ContainersPodsCtx;
