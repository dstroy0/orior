// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// retention_internal.h: what retention.c and retention_part.c share: a writer's account of its run, and the calls one
// reads from the other
#ifndef RETENTION_INTERNAL_H
#define RETENTION_INTERNAL_H
#include "../query_loops.h"
#include "embed_dispatch_layout.h"

typedef struct QueryRetentionContextFolderCtx QueryRetentionContextFolderCtx;
typedef struct QueryRetentionContextOwnCtx QueryRetentionContextOwnCtx;
typedef struct QueryRetentionContextFreePartCtx QueryRetentionContextFreePartCtx;
typedef struct QueryRetentionBlobCtx QueryRetentionBlobCtx;
typedef struct QueryRetentionRootFoundCtx QueryRetentionRootFoundCtx;
typedef struct QueryRetentionOwnCtx QueryRetentionOwnCtx;
typedef struct QueryRetentionContextCtx QueryRetentionContextCtx;
typedef struct QueryRetentionRootCtx QueryRetentionRootCtx;
typedef struct QueryRetentionContextErrMsgCtx QueryRetentionContextErrMsgCtx;
typedef struct QueryRetentionCtx QueryRetentionCtx;
typedef struct QueryRetentionInputCtx QueryRetentionInputCtx;

// A writer's account of its run, its last record (qry_writer.c): the records it put, their bytes, the records that did
// not match their CRC, those the run had no capacity for, the panics, and the milliseconds it ran
typedef struct
{
    unsigned long long records;
    unsigned long long bytes;
    unsigned long long corrupt;
    unsigned long long unplaced;
    unsigned long long panics;
    double ms;
} QueryAccount;

// the account the writer's record `blob` contains, six words of eight bytes, the low byte first: 1, or 0 where it is
// not one; and MiB written a second, 0 where no time was spent writing
int query_account_read(const QueryRetentionBlobCtx *args);
double query_account_rate(const QueryRetentionCtx *args);
// whether the part is free: no process of the tree on it, and its use at most QRY_PART_USED percent, 10 where it is
// not set, over five readings a second apart; why not written into `err_msg`
int query_part_free(const QueryRetentionContextErrMsgCtx *args);
// the crystal's program, built against the engine in `build/qry_crystal/engine` where nvcc is found, into `binary`
int query_crystal_build(const QueryRetentionContextCtx *args);
// each .qry of `folder` but the newest, and any whose writer runs, stored as its crystal through `crystal`; the .qry
// let go only where the crystal succeeded and is smaller. A line each
void query_crystals(const QueryRetentionContextOwnCtx *args);

// what the stores' checks reach: the folder's median rate over the runs' rates reported, and every run under half
// of it; every folder under `root` containing a run's .qry, .git and every `restored` pruned, sorted into `found`,
// which is sized for `capacity`, or counted where `found` is NULL: their count; one folder of runs retained; and why no
// run is stored as its crystal reported, where none is (retention.c); whether a line of nvidia-smi's processes names a
// program under `root`; and the part's allowed use in percent `supplied` reads, 10 where it is NULL or no number
// (retention_part.c)
void query_median_print(const QueryRetentionContextFolderCtx *args);
unsigned int query_folders_with_runs(const QueryRetentionRootFoundCtx *args);
void query_retention_folder(const QueryRetentionContextOwnCtx *args);
void query_crystals_reject(const QueryRetentionContextFreePartCtx *args);
int query_on_the_part(const QueryRetentionRootCtx *args);
long long query_part_used_max(const QueryRetentionInputCtx *args);
// a writer for the run at `qry`, in the image of `own`, this process no member of it (retention.c); and the host swept
// once more where no sweep of it is newer than QRY_SWEEP_HOURS, 12 where it is not set (sweep.c)
void query_writer_get(const QueryRetentionOwnCtx *args);
void query_swept_again(const QueryRetentionContextCtx *args);

struct QueryRetentionContextFolderCtx
{
    QueryContext *context;
    const char *folder;
    char (*names)[QUERY_PATH_MAX_LEN];
    const double *rates;
    unsigned int count;
};

struct QueryRetentionContextOwnCtx
{
    QueryContext *context;
    const QueryWriter *own;
    const char *folder;
    const char *crystal;
    unsigned int most;
};

struct QueryRetentionContextFreePartCtx
{
    QueryContext *context;
    int free_part;
    const char *why;
    int built;
};

struct QueryRetentionBlobCtx
{
    const unsigned char *blob;
    unsigned long long size;
    QueryAccount *account;
};

struct QueryRetentionRootFoundCtx
{
    const char *root;
    char (*found)[QUERY_PATH_MAX_LEN];
    unsigned int capacity;
};

struct QueryRetentionOwnCtx
{
    const QueryWriter *own;
    const char *qry;
    QueryWriter *writer;
};

struct QueryRetentionContextCtx
{
    QueryContext *context;
    char *binary;
};

struct QueryRetentionRootCtx
{
    const char *root;
    const char *apps;
};

struct QueryRetentionContextErrMsgCtx
{
    QueryContext *context;
    VerbaLinea *err_msg;
};

struct QueryRetentionCtx
{
    const QueryAccount *account_read;
};

struct QueryRetentionInputCtx
{
    const char *supplied;
};

typedef struct
{
    int (*account_read)(const QueryRetentionBlobCtx *args);
} QueryRetentionBlobNs;
EMBED_TABLE_LAYOUT(QueryRetentionBlobNs, account_read);
EMBED_TABLE_STORAGE QueryRetentionBlobNs query_retention_blob_internal EMBED_UNUSED = {
    .account_read = query_account_read,
};

typedef struct
{
    double (*account_rate)(const QueryRetentionCtx *args);
} QueryRetentionNs;
EMBED_TABLE_LAYOUT(QueryRetentionNs, account_rate);
EMBED_TABLE_STORAGE QueryRetentionNs query_retention_internal EMBED_UNUSED = {
    .account_rate = query_account_rate,
};

typedef struct
{
    int (*part_free)(const QueryRetentionContextErrMsgCtx *args);
} QueryRetentionContextErrMsgNs;
EMBED_TABLE_LAYOUT(QueryRetentionContextErrMsgNs, part_free);
EMBED_TABLE_STORAGE QueryRetentionContextErrMsgNs query_retention_context_err_msg_internal EMBED_UNUSED = {
    .part_free = query_part_free,
};

typedef struct
{
    int (*crystal_build)(const QueryRetentionContextCtx *args);
    void (*swept_again)(const QueryRetentionContextCtx *args);
} QueryRetentionContextNs;
EMBED_TABLE_LAYOUT(QueryRetentionContextNs, crystal_build, swept_again);
EMBED_TABLE_STORAGE QueryRetentionContextNs query_retention_context_internal EMBED_UNUSED = {
    .crystal_build = query_crystal_build,
    .swept_again = query_swept_again,
};

typedef struct
{
    void (*crystals)(const QueryRetentionContextOwnCtx *args);
    void (*folder)(const QueryRetentionContextOwnCtx *args);
} QueryRetentionContextOwnNs;
EMBED_TABLE_LAYOUT(QueryRetentionContextOwnNs, crystals, folder);
EMBED_TABLE_STORAGE QueryRetentionContextOwnNs query_retention_context_own_internal EMBED_UNUSED = {
    .crystals = query_crystals,
    .folder = query_retention_folder,
};

typedef struct
{
    void (*median_print)(const QueryRetentionContextFolderCtx *args);
} QueryRetentionContextFolderNs;
EMBED_TABLE_LAYOUT(QueryRetentionContextFolderNs, median_print);
EMBED_TABLE_STORAGE QueryRetentionContextFolderNs query_retention_context_folder_internal EMBED_UNUSED = {
    .median_print = query_median_print,
};

typedef struct
{
    unsigned int (*folders_with_runs)(const QueryRetentionRootFoundCtx *args);
} QueryRetentionRootFoundNs;
EMBED_TABLE_LAYOUT(QueryRetentionRootFoundNs, folders_with_runs);
EMBED_TABLE_STORAGE QueryRetentionRootFoundNs query_retention_root_found_internal EMBED_UNUSED = {
    .folders_with_runs = query_folders_with_runs,
};

typedef struct
{
    void (*crystals_reject)(const QueryRetentionContextFreePartCtx *args);
} QueryRetentionContextFreePartNs;
EMBED_TABLE_LAYOUT(QueryRetentionContextFreePartNs, crystals_reject);
EMBED_TABLE_STORAGE QueryRetentionContextFreePartNs query_retention_context_free_part_internal EMBED_UNUSED = {
    .crystals_reject = query_crystals_reject,
};

typedef struct
{
    int (*on_the_part)(const QueryRetentionRootCtx *args);
} QueryRetentionRootNs;
EMBED_TABLE_LAYOUT(QueryRetentionRootNs, on_the_part);
EMBED_TABLE_STORAGE QueryRetentionRootNs query_retention_root_internal EMBED_UNUSED = {
    .on_the_part = query_on_the_part,
};

typedef struct
{
    long long (*part_used_max)(const QueryRetentionInputCtx *args);
} QueryRetentionInputNs;
EMBED_TABLE_LAYOUT(QueryRetentionInputNs, part_used_max);
EMBED_TABLE_STORAGE QueryRetentionInputNs query_retention_input_internal EMBED_UNUSED = {
    .part_used_max = query_part_used_max,
};

typedef struct
{
    void (*writer_get)(const QueryRetentionOwnCtx *args);
} QueryRetentionOwnNs;
EMBED_TABLE_LAYOUT(QueryRetentionOwnNs, writer_get);
EMBED_TABLE_STORAGE QueryRetentionOwnNs query_retention_own_internal EMBED_UNUSED = {
    .writer_get = query_writer_get,
};

#endif
