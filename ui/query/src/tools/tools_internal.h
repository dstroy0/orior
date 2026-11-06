// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tools_internal.h: one program of the tree as the query builds it, the table of every one (tools_table.c), and the
// bounds of a build
#ifndef TOOLS_INTERNAL_H
#define TOOLS_INTERNAL_H
#include "../query_internal.h"
#include "embed_dispatch_layout.h"

typedef struct QueryToolsRootToolCCtx QueryToolsRootToolCCtx;
typedef struct QueryToolsSourceKnownCtx QueryToolsSourceKnownCtx;
typedef struct QueryToolsRootToolSourcesCtx QueryToolsRootToolSourcesCtx;
typedef struct QueryToolsRootObjectsCtx QueryToolsRootObjectsCtx;
typedef struct QueryToolsRootCtx QueryToolsRootCtx;
typedef struct QueryToolsRootToolCtx QueryToolsRootToolCtx;
typedef struct QueryToolsCtx QueryToolsCtx;

// the most groups of patterns a tool's C or C++ sources are given in, and the most sources one tool compiles
#define QUERY_TOOL_GROUPS 4u

#define QUERY_SOURCES_MAX 2048u

// the launch queue's two numbers as the interface stores them (interface.h): the compilers run at most this many at
// once, and this much passes between one start and the next
#define QUERY_LAUNCHES_AT_ONCE 4u

#define QUERY_LAUNCH_SPACING_MILLISECONDS 50u

// One program of the tree: its name; its C sources, compiled with the C compiler and `c_flags`, in groups of
// patterns; the sources compiled as C++ with `-x c++` and `cxx_flags`, the code generator's .cu files; the sources
// compiled as C++ as their extension indicates, .cpp; 1 where the file definitions are linked as C sources; the
// base-name prefixes of sources skipped among the patterns; the include folders, from the top of the tree; the words
// given the linker after the objects, the linker the C++ compiler where any C++ source is there; and the folders whose
// .h files make the binary stale when newer, beyond the sources' own folders. Every list ends at NULL
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
// the words every program's link ends with, after the tool's own
extern const char *const g_query_mmgr_link[];
// every tool, the last one's name NULL
extern const QueryTool g_query_tools[];

// a tool's sources and objects as the last walk of them listed them, released through persistent_buf_release once
// the work on them is done (tools.c)
extern QueryPaths g_query_c_sources;
extern QueryPaths g_query_cxx_sources;
extern QueryPaths g_query_tool_sources;
extern QueryPaths g_query_objects;

const QueryTool *query_tool_by_name(const QueryToolsCtx *args);
// the files `patterns` name under `root` put after those `found` contains, in the order of the patterns and sorted
// within one: the count after
unsigned int query_sources(const QueryToolsRootCtx *args);
// every source of `tool` listed afresh, each count written; and every list of sources and the objects given back
void query_every_source(const QueryToolsRootToolCCtx *args);
void query_every_source_free(void);
unsigned long long query_newest_header_get(const QueryToolsRootToolSourcesCtx *args);
int query_fresh(const QueryToolsSourceKnownCtx *args);
int query_stale(const QueryToolsRootToolCtx *args);
void query_object_get(const QueryToolsRootObjectsCtx *args);

struct QueryToolsRootToolCCtx
{
    const char *root;
    const QueryTool *tool;
    unsigned int *c;
    unsigned int *cxx;
    unsigned int *tools;
};

struct QueryToolsSourceKnownCtx
{
    int source_known;
    unsigned long long source;
    int object_known;
    unsigned long long object;
    unsigned long long newest_header;
};

struct QueryToolsRootToolSourcesCtx
{
    const char *root;
    const QueryTool *tool;
    char (*sources)[QUERY_PATH_MAX_LEN];
    unsigned int count;
};

struct QueryToolsRootObjectsCtx
{
    const char *root;
    const char *objects;
    const char *source_text;
    char *object_capacity;
};

struct QueryToolsRootCtx
{
    const char *root;
    const char *const *patterns;
    QueryPaths *found;
};

struct QueryToolsRootToolCtx
{
    const char *root;
    const QueryTool *tool;
    const char *binary;
};

struct QueryToolsCtx
{
    const char *name;
};

typedef struct
{
    const QueryTool *(*tool_by_name)(const QueryToolsCtx *args);
} QueryToolsNs;
EMBED_TABLE_LAYOUT(QueryToolsNs, tool_by_name);
EMBED_TABLE_STORAGE QueryToolsNs query_tools_internal EMBED_UNUSED = {
    .tool_by_name = query_tool_by_name,
};

typedef struct
{
    unsigned int (*sources)(const QueryToolsRootCtx *args);
} QueryToolsRootNs;
EMBED_TABLE_LAYOUT(QueryToolsRootNs, sources);
EMBED_TABLE_STORAGE QueryToolsRootNs query_tools_root_internal EMBED_UNUSED = {
    .sources = query_sources,
};

typedef struct
{
    void (*every_source)(const QueryToolsRootToolCCtx *args);
} QueryToolsRootToolCNs;
EMBED_TABLE_LAYOUT(QueryToolsRootToolCNs, every_source);
EMBED_TABLE_STORAGE QueryToolsRootToolCNs query_tools_root_tool_c_internal EMBED_UNUSED = {
    .every_source = query_every_source,
};

typedef struct
{
    unsigned long long (*newest_header_get)(const QueryToolsRootToolSourcesCtx *args);
} QueryToolsRootToolSourcesNs;
EMBED_TABLE_LAYOUT(QueryToolsRootToolSourcesNs, newest_header_get);
EMBED_TABLE_STORAGE QueryToolsRootToolSourcesNs query_tools_root_tool_sources_internal EMBED_UNUSED = {
    .newest_header_get = query_newest_header_get,
};

typedef struct
{
    int (*fresh)(const QueryToolsSourceKnownCtx *args);
} QueryToolsSourceKnownNs;
EMBED_TABLE_LAYOUT(QueryToolsSourceKnownNs, fresh);
EMBED_TABLE_STORAGE QueryToolsSourceKnownNs query_tools_source_known_internal EMBED_UNUSED = {
    .fresh = query_fresh,
};

typedef struct
{
    int (*stale)(const QueryToolsRootToolCtx *args);
} QueryToolsRootToolNs;
EMBED_TABLE_LAYOUT(QueryToolsRootToolNs, stale);
EMBED_TABLE_STORAGE QueryToolsRootToolNs query_tools_root_tool_internal EMBED_UNUSED = {
    .stale = query_stale,
};

typedef struct
{
    void (*object_get)(const QueryToolsRootObjectsCtx *args);
} QueryToolsRootObjectsNs;
EMBED_TABLE_LAYOUT(QueryToolsRootObjectsNs, object_get);
EMBED_TABLE_STORAGE QueryToolsRootObjectsNs query_tools_root_objects_internal EMBED_UNUSED = {
    .object_get = query_object_get,
};

#endif
