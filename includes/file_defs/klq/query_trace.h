// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_trace.h: the flags of the query's trace, each choice a put makes at a link written as the flags that represent
// there, ORed into one word, and read back by name (includes/file_defs/klq/query_trace.tsv)
#ifndef QUERY_TRACE_H
#define QUERY_TRACE_H

#include "verba_scribo/verba_linea.h"

#include <stddef.h>

// the most flags of a trace, one a bit of its word, and the bytes of a flag's name and gloss, the terminator among them
#define QUERY_TRACE_FLAGS_MAX 64u
#define QUERY_TRACE_NAME_BYTES 64u
#define QUERY_TRACE_GLOSS_BYTES 512u

// one flag as its table defines it: its word, its name and its gloss
struct QueryTraceFlag
{
    unsigned long long word;
    char name[QUERY_TRACE_NAME_BYTES];
    char gloss[QUERY_TRACE_GLOSS_BYTES];
};

// the trace's flags as its table defines them, `count` of them, in the table's order
struct QueryTrace
{
    QueryTraceFlag flags[QUERY_TRACE_FLAGS_MAX];
    unsigned int count;
};

// the table at includes/file_defs/klq/query_trace.tsv in the tree this file was built from, into `path`, `bytes` of
// it: 1 where the whole path fit
int query_trace_path(char *path, size_t bytes);

// the table at `path` read into `trace`: 1, or 0 and why on `error` where it could not be read, or a line of it gives
// no flag of one bit, one two names share, or a name or gloss past its bytes
int query_trace_read(const char *path, QueryTrace *trace, VerbaLinea *error);

// the word of the flag `name`, 0 where the table gives no such flag
unsigned long long query_trace_flag(const QueryTrace *trace, const char *name);

// the names of the flags `word` contains onto `names`, in the table's order, a space between each
void query_trace_names(const QueryTrace *trace, unsigned long long word, VerbaLinea *names);

#endif
