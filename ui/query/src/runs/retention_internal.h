// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// retention_internal.h: what retention.c and retention_part.c share: a writer's account of its run, and the calls one
// reads from the other
#ifndef RETENTION_INTERNAL_H
#define RETENTION_INTERNAL_H

#include "../query_loops.h"

// A writer's account of its run, its last record (qry_writer.c): the records it put, their bytes, the records that did
// not match their CRC, those the run had no room for, the panics, and the milliseconds it ran
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
int query_account_read(const unsigned char *blob, unsigned long long size, QueryAccount *account);
double query_account_rate(const QueryAccount *account);
// whether the part is free: no process of the tree on it, and its use at most QRY_PART_USED percent, 10 where it is
// not set, over five readings a second apart; why not written into `err_msg`
int query_part_free(QueryContext *context, VerbaLinea *err_msg);
// the crystal's program, built against the engine in `build/qry_crystal/engine` where nvcc is found, into `binary`
int query_crystal_built(QueryContext *context, char *binary);
// each .qry of `folder` but the newest, and any whose writer runs, stored as its crystal through `crystal`; the .qry
// let go only where the crystal succeeded and is smaller. A line each
void query_crystals(QueryContext *context, const QueryWriter *own, const char *folder, const char *crystal);

// what the keepers' checks reach: the folder's median rate over the runs' rates reported, and every run under half
// of it; every folder under `root` containing a run's .qry, .git and every `restored` pruned, sorted into `found`,
// which is sized for `room`, or counted where `found` is NULL: their count; one folder of runs retained; and why no run
// is stored as its crystal reported, where none is (retention.c); whether a line of nvidia-smi's processes names a
// program under `root`; and the part's allowed use in percent `given` reads, 10 where it is NULL or no number
// (retention_part.c)
void query_median_print(QueryContext *context, const char *folder, char (*names)[QUERY_PATH_MAX_LEN],
                        const double *rates, unsigned int count);
unsigned int query_folders_with_runs(const char *root, char (*found)[QUERY_PATH_MAX_LEN], unsigned int room);
void query_retention_folder(QueryContext *context, const char *folder, const char *crystal, unsigned int most,
                            const QueryWriter *own);
void query_crystals_refused(QueryContext *context, int free_part, const char *why, int built);
int query_on_the_part(const char *apps, const char *root);
long long query_part_used_most(const char *given);
// a writer for the run at `qry`, in the image of `own`, this process no member of it (retention.c); and the host swept
// once more where no sweep of it is newer than QRY_SWEEP_HOURS, 12 where it is not set (sweep.c)
void query_writer_of(const QueryWriter *own, const char *qry, QueryWriter *writer);
void query_swept_again(QueryContext *context);

#endif
