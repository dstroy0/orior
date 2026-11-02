// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.c: a run read from its shared memory while its writer runs and from its .qry where none does, its records of a
// name joined, records appended to it, and the run the window is joined to
#include "qry.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"

// what a record contains is read for at most so many of its bytes, and in whole where it is at most so many; the most
// records one read hands, a read of a run of any size taking a bounded capacity and going on from where it stopped at
// the next; and the most runs viewed at once
#define CLI_TEXT_SMALL 4096ull

#define CLI_TEXT_MAX 65536ull

#define CLI_READ_MAX 4096u

#define CLI_VIEWS_MAX 64u

// A run viewed while its writer writes: its path and its shared memory mapped
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    QryShared *run;
} CliView;

// the runs viewed, the run the window is a member of to append to, and the run the window's own work goes to: one lock
// contains them all
static CliView s_views[CLI_VIEWS_MAX];
static char s_member[CLI_PATH_MAX_LEN];
static int s_membered;
static char s_window[CLI_PATH_MAX_LEN];
static int s_windowed;
static int s_window_read;

typedef struct
{
    const char *name;
    unsigned int length;
    unsigned int priority;
    unsigned long long size;
    VerbaLinea *out;
    unsigned long long sequence;
    const QryRecord *record;
    int first;
    const char *path;
} QryCtx;
// The run at `path` viewed while its writer writes: its shared memory, or NULL where it writes no more or never made
// the run. A view is pinned only while its run's writer writes: each call lets go of every view whose writer has
// stopped, whichever run it is of. Called under the lock
static QryShared *cli_view(const QryCtx *args)
{
    for (unsigned int at = 0u; at < CLI_VIEWS_MAX; at += 1u)
    {
        if ((s_views[at].run != NULL) && !qry_member_viewed_writing(s_views[at].run))
        {
            qry_member_unshared(s_views[at].run);
            s_views[at].run = NULL;
        }
    }
    unsigned int found = CLI_VIEWS_MAX;
    unsigned int free_view = CLI_VIEWS_MAX;
    for (unsigned int at = 0u; at < CLI_VIEWS_MAX; at += 1u)
    {
        found = ((s_views[at].run != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_views[at].path, .other = args->path, .cap = CLI_CELLS_POOL})))
                    ? at
                    : found;
        free_view = ((s_views[at].run == NULL) && (free_view == CLI_VIEWS_MAX)) ? at : free_view;
    }
    if (found == CLI_VIEWS_MAX)
    {
        QryShared *const run = qry_member_shared(args->path);
        if ((run == NULL) || (free_view == CLI_VIEWS_MAX) ||
            (cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) >= CLI_PATH_MAX_LEN))
        {
            if (run != NULL)
            {
                qry_member_unshared(run);
            }
            return NULL;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = s_views[free_view].path,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
        s_views[free_view].run = run;
        found = free_view;
    }
    if (qry_member_viewed_writing(s_views[found].run))
    {
        return s_views[found].run;
    }
    qry_member_unshared(s_views[found].run);
    s_views[found].run = NULL;
    return NULL;
}

// whether a record's name is `name`
EMBED_INLINE int cli_by_name(const QryCtx *args)
{
    return (args->record->name_length == cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){
                .src = QRY_RECORD_NAME(args->record), .other = args->name, .bytes = args->record->name_length}) == 0);
}

int cli_qry_joined_bytes(const CliQryPathOutCfg *args)
{
    QryShared *const run = EMBED_CALL(cli_view, QryCtx, .path = args->path);
    const QryRecord *record = NULL;
    if (run != NULL)
    {
        unsigned long long at = 0ull;
        while (qry_member_viewed_read(run, &at, &record))
        {
            mmgr_line_put_n(args->out, (const char *)QRY_RECORD_BLOB(record),
                            EMBED_CALL(cli_by_name, QryCtx, .record = record, .name = args->name) ? record->blob_length
                                                                                                  : 0ull);
        }
        return 1;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = &g_cli_runs_cells);
    unsigned long long at = 0ull;
    while ((bytes != NULL) && qry_record_read(bytes, size, &at, &record))
    {
        mmgr_line_put_n(args->out, (const char *)QRY_RECORD_BLOB(record),
                        EMBED_CALL(cli_by_name, QryCtx, .record = record, .name = args->name) ? record->blob_length
                                                                                              : 0ull);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_runs_cells);
    if ((bytes == NULL) || (at != size))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": the run could not be read whole");
        return 0;
    }
    return 1;
}

unsigned int cli_qry_member_names(const CliQryDataCfg *args)
{
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while (qry_record_read(args->data, args->size, &at, &record))
    {
        char name[CLI_NAME_MAX_LEN];
        VerbaLinea line = {name, sizeof(name), 0ull};
        mmgr_line_put_n(&line, QRY_RECORD_NAME(record), record->name_length);
        mmgr_line_finish(&line);
        int seen = 0;
        for (unsigned int each = 0u; !seen && (each < count); each += 1u)
        {
            seen = cellul.eq(&(CatenaFinitaCfg){.src = args->names[each], .other = name, .cap = CLI_CELLS_POOL});
        }
        if (!seen && (count < args->capacity))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->names[count],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    return count;
}

unsigned long long cli_qry_member_size(const CliQryOutCfg *args)
{
    unsigned long long at = 0ull;
    unsigned long long joined = 0ull;
    const QryRecord *record = NULL;
    while (qry_record_read(args->data, args->size, &at, &record))
    {
        joined += EMBED_CALL(cli_by_name, QryCtx, .record = record, .name = args->name) ? record->blob_length : 0ull;
    }
    return joined;
}

void cli_qry_member_join(const CliQryOutCfg *args)
{
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while (qry_record_read(args->data, args->size, &at, &record))
    {
        mmgr_line_put_n(args->out, (const char *)QRY_RECORD_BLOB(record),
                        EMBED_CALL(cli_by_name, QryCtx, .record = record, .name = args->name) ? record->blob_length
                                                                                              : 0ull);
    }
}

int cli_qry_append(const CliQryPathCfg *args)
{
    const void *bytes = args->bytes;
    if (args->priority > QRY_ORDINARY)
    {
        mmgr_line_decimal(args->err_msg, args->priority, 1u);
        mmgr_line_put(args->err_msg, " is not a priority: 0 a panic, 1 critical, 2 ordinary");
        return 0;
    }
    if (!s_membered || (!cellul.eq(&(CatenaFinitaCfg){.src = s_member, .other = args->path, .cap = CLI_CELLS_POOL})))
    {
        if (s_membered)
        {
            qry_member_parted();
            s_membered = 0;
        }
        if ((cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) >= CLI_PATH_MAX_LEN) ||
            (qry_member_joined(args->path) == NULL))
        {
            mmgr_line_put(args->err_msg, "no writer runs for this run: ");
            mmgr_line_put(args->err_msg, args->path);
            return 0;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = s_member,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
        s_membered = 1;
    }
    const int appended = qry_member_append(args->name, bytes, args->size, args->priority) != 0ull;
    if (!appended)
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " is not a record the run takes, or larger than half its ring");
    }
    return appended;
}

int cli_qry_ready(const CliQryPathPanickedCfg *args)
{
    for (unsigned int tried = 0u; tried < 3000u; tried += 1u)
    {
        const int viewed = EMBED_CALL(cli_view, QryCtx, .path = args->path) != NULL;
        if (viewed)
        {
            return 1;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 10u);
    }
    return 0;
}

int cli_qry_stop(const CliQryPathOutCfg *args)
{
    if (s_membered)
    {
        qry_member_parted();
        s_membered = 0;
    }
    if (qry_member_joined(args->path) == NULL)
    {
        mmgr_line_put(args->err_msg, "no writer runs for this run: ");
        mmgr_line_put(args->err_msg, args->path);
        return 0;
    }
    qry_member_stopped();
    qry_member_parted();
    return 1;
}

// the run the window's work goes to read from QRY the first time it is requested. Called under the lock
static void cli_window_read(void)
{
    if (!s_window_read)
    {
        s_windowed = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "QRY",
                                .value_out = s_window, .capacity = sizeof(s_window)) &&
                     (s_window[0] != '\0');
        s_window_read = 1;
    }
}

int cli_qry_join(const CliQryCfg *args)
{
    cli_window_read();
    const int joined = s_windowed;
    memor.cpy(&(MemoriaCfg){
        .dst = args->path_out,
        .src = joined ? s_window : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = joined ? s_window : "", .cap = CLI_CELLS_POOL}) + 1u});
    return joined;
}

void cli_qry_join_window(const CliQryPathPanickedCfg *args)
{
    s_window_read = 1;
    s_windowed = (args->path != NULL) &&
                 (cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) < CLI_PATH_MAX_LEN);
    memor.cpy(&(MemoriaCfg){
        .dst = s_window,
        .src = s_windowed ? args->path : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_windowed ? args->path : "", .cap = CLI_CELLS_POOL}) + 1u});
}

int cli_qry_existing(const CliQryPathPanickedCfg *args)
{
    unsigned long long *panicked = args->panicked;
    QryShared *const run = EMBED_CALL(cli_view, QryCtx, .path = args->path);
    *panicked = (run != NULL) ? qry_member_viewed_panicked(run) : 0ull;
    return run != NULL;
}
