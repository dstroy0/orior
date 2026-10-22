// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tools_internal.h: one program of the tree as the query builds it, the table of every one (tools_table.c), and the
// bounds of a build
#ifndef TOOLS_INTERNAL_H
#define TOOLS_INTERNAL_H

#include "../query_internal.h"

// the most groups of patterns a tool's C or C++ sources are given in, and the most sources one tool compiles
#define QUERY_TOOL_GROUPS 4u

#define QUERY_SOURCES_MOST 2048u

// the launch queue's two numbers as the interface keeps them (interface.h): the compilers run at most this many at
// once, and this much passes between one start and the next
#define QUERY_LAUNCHES_AT_ONCE 4u

#define QUERY_LAUNCH_SPACING_MILLISECONDS 50u

// One program of the tree: its name; its C sources, compiled with the C compiler and `c_flags`, in groups of
// patterns; the sources compiled as C++ with `-x c++` and `cxx_flags`, the code generator's .cu files; the sources
// compiled as C++ as their extension says, .cpp; 1 where the file definitions are linked as C sources; the base-name
// prefixes of sources skipped among the patterns; the include folders, from the top of the tree; the words given the
// linker after the objects, the linker the C++ compiler where any C++ source is there; and the folders whose .h files
// make the binary stale when newer, beyond the sources' own folders. Every list ends at NULL
typedef struct
{
    const char *name;
    const char *const *c[QUERY_TOOL_GROUPS];
    const char *const *cxx[QUERY_TOOL_GROUPS];
    const char *const *cxx_tool;
    int file_defs;
    const char *const *skip;
    const char *const *includes;
    const char *const *c_flags;
    const char *const *cxx_flags;
    const char *const *link;
    const char *const *headers;
} QueryTool;

extern const char *const g_query_file_defs[];
extern const char *const g_query_file_defs_headers[];
// MMgr's sources, which every tool links, and its include folders and settings, which every compile is given
extern const char *const g_query_mmgr[];
extern const char *const g_query_mmgr_includes[];
extern const char *const g_query_mmgr_settings[];
// every tool, the last one's name NULL
extern const QueryTool g_query_tools[];

// a tool's sources and objects as the last walk of them listed them, given back with query_every_source_given once
// the work on them is done (tools.c)
extern QueryPaths g_query_c_sources;
extern QueryPaths g_query_cxx_sources;
extern QueryPaths g_query_tool_sources;
extern QueryPaths g_query_objects;

const QueryTool *query_tool_named(const char *name);
// the files `patterns` name under `root` put after those `found` holds, in the order of the patterns and sorted within
// one: the count after
unsigned int query_sources(const char *root, const char *const *patterns, QueryPaths *found);
// every source of `tool` listed afresh, each count written; and every list of sources and the objects given back
void query_every_source(const char *root, const QueryTool *tool, unsigned int *c, unsigned int *cxx,
                        unsigned int *tools);
void query_every_source_given(void);
unsigned long long query_newest_header_of(const char *root, const QueryTool *tool, char (*sources)[QUERY_PATH_LONGEST],
                                          unsigned int count);
int query_fresh(int source_known, unsigned long long source, int object_known, unsigned long long object,
                unsigned long long newest_header);
int query_stale(const char *root, const QueryTool *tool, const char *binary);
void query_object_of(const char *root, const char *objects, const char *source, char *object);

#endif
