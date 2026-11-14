// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_sync.c: the boxes of text a SyncTeX file reports each page contains, each with the lines of a file of the tree
// that set it, in the page's own points from its top left
#include "latex_internal.h"

// One record of a SyncTeX file: its file's tag, its line, its point and, for a box, its width, height and depth, all
// in the file's own units
typedef struct
{
    unsigned int tag;
    unsigned long long line;
    long long x;
    long long y;
    long long size[3];
} CliTexRecord;
// The scale a SyncTeX file gives its units by
typedef struct
{
    double unit;
    double magnification;
    double x_offset;
    double y_offset;
} CliTexScale;

typedef struct
{
    const char *text;
    unsigned long long *start;
    unsigned long long *size;
    unsigned long long length;
    unsigned long long at;
    char letter;
    unsigned long long start_value;
    long long *value;
    CliTexRecord *record;
    long long value_value;
    double offset;
    const CliTexScale *scale;
    unsigned int files;
    unsigned int tag;
    const char *rest;
    const CliTexNames *names;
    unsigned int *files_pointer;
    unsigned int *used;
    const CliTexOpen *done;
    unsigned int marks;
    unsigned long long page;
    int *written;
    VerbaLinea *out;
    const char *line;
    unsigned long long size_value;
    CliTexScale *scale_out;
    int *content;
} LatexSyncCtx;
