// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_internal.h: what tables.c and tables_pull.c share: the tables' folders, the harness's image, the key kernel,
// and the calls one reads from the other
#ifndef TABLES_INTERNAL_H
#define TABLES_INTERNAL_H
#include "../query_loops.h"
#include "embed_dispatch_layout.h"

typedef struct QueryTablesErrMsgCtx QueryTablesErrMsgCtx;
typedef struct QueryTablesOutArchCtx QueryTablesOutArchCtx;
typedef struct QueryTablesOutErrMsgCtx QueryTablesOutErrMsgCtx;
typedef struct QueryTablesPathCtx QueryTablesPathCtx;
typedef struct QueryTablesOutCtx QueryTablesOutCtx;
typedef struct QueryTablesOutputCtx QueryTablesOutputCtx;
typedef struct QueryTablesCtx QueryTablesCtx;
typedef struct QueryTablesCapacityCtx QueryTablesCapacityCtx;
typedef struct QueryTablesRootCtx QueryTablesRootCtx;
typedef struct QueryTablesFolderCtx QueryTablesFolderCtx;

// the tables' folder: the key cubins, the vendor's disassembler for Linux, the captures and the tables
#define QUERY_TABLES_OUT "build/vendor/nvidia"

#define QUERY_TABLES_SOURCE "src/cu/transpiler/vendor_bin_layouts/nvidia/tables"

// the harness's Ubuntu image, which has the C compiler the hook builds with
#define QUERY_TABLES_IMAGE "orior-test-system-ubuntu:latest"

// the key kernel: three lines whose content the disassembler reads the architecture off, and nothing else
#define QUERY_TABLES_KEY "__global__ void kernel(float *out) { out[0] = 0; }\n"

// the max architectures nvcc lists
#define QUERY_ARCHITECTURES_MAX 64u

void query_tables_in(const QueryTablesFolderCtx *args);
void query_tables_get(const QueryTablesOutCtx *args);
void query_in_container(const QueryTablesRootCtx *args);
int query_program(const QueryTablesPathCtx *args);
unsigned int query_architectures(const QueryTablesErrMsgCtx *args);
int query_disassembler(const QueryTablesOutErrMsgCtx *args);
int query_key_cubin(const QueryTablesOutArchCtx *args);

// the compiler's version, `V13.3.73`, read off `nvcc --version` into `version`: 1, or 0 where it names none; the
// architectures `nvcc --list-gpu-arch` lists, `compute_86` read as `sm_86`, into `arches`: their count; and the
// vendor's archive of nvdisasm at `version` named into `archive`, sized for `capacity` (tables.c)
int query_version_get(const QueryTablesOutputCtx *args);
unsigned int query_architectures_get(const QueryTablesCtx *args);
void query_archive_by_name(const QueryTablesCapacityCtx *args);

struct QueryTablesErrMsgCtx
{
    VerbaLinea *err_msg;
    QueryContext *context;
    const QueryRun *run;
    const char *const *words;
    unsigned int count;
    char (*arches)[16];
};

struct QueryTablesOutArchCtx
{
    const char *out;
    const char *arch;
    VerbaLinea *err_msg;
    QueryContext *context;
    const QueryRun *run;
    char *cubin;
};

struct QueryTablesOutErrMsgCtx
{
    const char *out;
    VerbaLinea *err_msg;
    QueryContext *context;
    const QueryRun *run;
    char *program;
};

struct QueryTablesPathCtx
{
    char *path;
    const QueryContext *context_read;
    QueryRole role;
    VerbaLinea *err_msg;
};

struct QueryTablesOutCtx
{
    const char *out;
    const char *arch;
    char *instructions;
    char *latencies;
};

struct QueryTablesOutputCtx
{
    const unsigned char *output;
    unsigned long long size;
    char *version;
    size_t capacity;
};

struct QueryTablesCtx
{
    char (*arches)[16];
    const unsigned char *output;
    unsigned long long size;
};

struct QueryTablesCapacityCtx
{
    size_t capacity;
    const char *version_read;
    char *archive;
};

struct QueryTablesRootCtx
{
    const char *root;
    const char *path_read;
    char *inside;
};

struct QueryTablesFolderCtx
{
    const char *folder;
    const char *name;
    char *path;
};

typedef struct
{
    void (*in)(const QueryTablesFolderCtx *args);
} QueryTablesFolderNs;
EMBED_TABLE_LAYOUT(QueryTablesFolderNs, in);
EMBED_TABLE_STORAGE QueryTablesFolderNs query_tables_folder_internal EMBED_UNUSED = {
    .in = query_tables_in,
};

typedef struct
{
    void (*get)(const QueryTablesOutCtx *args);
} QueryTablesOutNs;
EMBED_TABLE_LAYOUT(QueryTablesOutNs, get);
EMBED_TABLE_STORAGE QueryTablesOutNs query_tables_out_internal EMBED_UNUSED = {
    .get = query_tables_get,
};

typedef struct
{
    void (*in_container)(const QueryTablesRootCtx *args);
} QueryTablesRootNs;
EMBED_TABLE_LAYOUT(QueryTablesRootNs, in_container);
EMBED_TABLE_STORAGE QueryTablesRootNs query_tables_root_internal EMBED_UNUSED = {
    .in_container = query_in_container,
};

typedef struct
{
    int (*program)(const QueryTablesPathCtx *args);
} QueryTablesPathNs;
EMBED_TABLE_LAYOUT(QueryTablesPathNs, program);
EMBED_TABLE_STORAGE QueryTablesPathNs query_tables_path_internal EMBED_UNUSED = {
    .program = query_program,
};

typedef struct
{
    unsigned int (*architectures)(const QueryTablesErrMsgCtx *args);
} QueryTablesErrMsgNs;
EMBED_TABLE_LAYOUT(QueryTablesErrMsgNs, architectures);
EMBED_TABLE_STORAGE QueryTablesErrMsgNs query_tables_err_msg_internal EMBED_UNUSED = {
    .architectures = query_architectures,
};

typedef struct
{
    int (*disassembler)(const QueryTablesOutErrMsgCtx *args);
} QueryTablesOutErrMsgNs;
EMBED_TABLE_LAYOUT(QueryTablesOutErrMsgNs, disassembler);
EMBED_TABLE_STORAGE QueryTablesOutErrMsgNs query_tables_out_err_msg_internal EMBED_UNUSED = {
    .disassembler = query_disassembler,
};

typedef struct
{
    int (*key_cubin)(const QueryTablesOutArchCtx *args);
} QueryTablesOutArchNs;
EMBED_TABLE_LAYOUT(QueryTablesOutArchNs, key_cubin);
EMBED_TABLE_STORAGE QueryTablesOutArchNs query_tables_out_arch_internal EMBED_UNUSED = {
    .key_cubin = query_key_cubin,
};

typedef struct
{
    int (*version_get)(const QueryTablesOutputCtx *args);
} QueryTablesOutputNs;
EMBED_TABLE_LAYOUT(QueryTablesOutputNs, version_get);
EMBED_TABLE_STORAGE QueryTablesOutputNs query_tables_output_internal EMBED_UNUSED = {
    .version_get = query_version_get,
};

typedef struct
{
    unsigned int (*architectures_get)(const QueryTablesCtx *args);
} QueryTablesNs;
EMBED_TABLE_LAYOUT(QueryTablesNs, architectures_get);
EMBED_TABLE_STORAGE QueryTablesNs query_tables_internal EMBED_UNUSED = {
    .architectures_get = query_architectures_get,
};

typedef struct
{
    void (*archive_by_name)(const QueryTablesCapacityCtx *args);
} QueryTablesCapacityNs;
EMBED_TABLE_LAYOUT(QueryTablesCapacityNs, archive_by_name);
EMBED_TABLE_STORAGE QueryTablesCapacityNs query_tables_capacity_internal EMBED_UNUSED = {
    .archive_by_name = query_archive_by_name,
};

#endif
