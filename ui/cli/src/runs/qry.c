// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.c: a run read from its shared memory while its writer runs and from its .qry where none does, its records of a
// name joined, records appended to it, and the run the window is joined to
#include "qry.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"

// what a record contains is read for at most so many of its bytes, and in whole where it is at most so many; the most
// records one read hands, a read of a run of any size taking a bounded room and going on from where it stopped at the
// next; and the most runs viewed at once
#define CLI_TEXT_SMALL 4096ull

#define CLI_TEXT_MOST 65536ull

#define CLI_READ_MOST 4096u

#define CLI_VIEWS_MOST 64u

// A run viewed while its writer writes: its path and its shared memory mapped
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    QryShared *run;
} CliView;

// the runs viewed, the run the window is a member of to append to, and the run the window's own work goes to: one lock
// contains them all
static CliView s_views[CLI_VIEWS_MOST];
static char s_member[CLI_PATH_MAX_LEN];
static int s_membered;
static char s_window[CLI_PATH_MAX_LEN];
static int s_windowed;
static int s_window_read;
static CliLock s_lock = CLI_LOCK_INIT;

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
// whether what a record contains is read
EMBED_INLINE int cli_on_text(const QryCtx *args)
{
    const unsigned int prefix =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = CLI_CONFERENCE, .cap = CLI_CELLS_POOL});
    return (args->priority < QRY_ORDINARY) ||
           ((args->length >= prefix) &&
            (memor.cmp(&(MemoriaCfg){.src = args->name, .other = CLI_CONFERENCE, .bytes = prefix}) == 0)) ||
           (args->size <= CLI_TEXT_SMALL);
}

// `record` written as the window shows it, at the place `sequence`
EMBED_INLINE void cli_blob_write(const QryCtx *args)
{
    VerbaLinea *out = args->out;
    const char *const name = QRY_RECORD_NAME(args->record);
    const unsigned char *const blob = QRY_RECORD_BLOB(args->record);
    mmgr_line_put(out, args->first ? "{\"name\":" : ",{\"name\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)name, .length = args->record->name_length);
    mmgr_line_put(out, ",\"sequence\":");
    mmgr_line_decimal(out, args->sequence, 1u);
    mmgr_line_put(out, ",\"nanoseconds\":");
    mmgr_line_decimal(out, args->record->time, 1u);
    mmgr_line_put(out, ",\"priority\":");
    mmgr_line_decimal(out, args->record->priority, 1u);
    mmgr_line_put(out, ",\"size\":");
    mmgr_line_decimal(out, args->record->blob_length, 1u);
    mmgr_line_put(out, ",\"text\":");
    if (EMBED_CALL(cli_on_text, QryCtx, .name = name, .length = args->record->name_length,
                   .priority = args->record->priority, .size = args->record->blob_length))
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out, .text_bytes = blob,
                   .length = (args->record->blob_length < CLI_TEXT_MOST) ? args->record->blob_length : CLI_TEXT_MOST);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"whole\":true}");
}

// The run at `path` viewed while its writer writes: its shared memory, or NULL where it writes no more or never made
// the run. A view is pinned only while its run's writer writes: each call lets go of every view whose writer has
// stopped, whichever run it is of. Called under the lock
static QryShared *cli_view(const QryCtx *args)
{
    for (unsigned int at = 0u; at < CLI_VIEWS_MOST; at += 1u)
    {
        if ((s_views[at].run != NULL) && !qry_member_viewed_writing(s_views[at].run))
        {
            qry_member_unshared(s_views[at].run);
            s_views[at].run = NULL;
        }
    }
    unsigned int found = CLI_VIEWS_MOST;
    unsigned int free_view = CLI_VIEWS_MOST;
    for (unsigned int at = 0u; at < CLI_VIEWS_MOST; at += 1u)
    {
        found = ((s_views[at].run != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_views[at].path, .other = args->path, .cap = CLI_CELLS_POOL})))
                    ? at
                    : found;
        free_view = ((s_views[at].run == NULL) && (free_view == CLI_VIEWS_MOST)) ? at : free_view;
    }
    if (found == CLI_VIEWS_MOST)
    {
        QryShared *const run = qry_member_shared(args->path);
        if ((run == NULL) || (free_view == CLI_VIEWS_MOST) ||
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

int cli_qry_read_file_from(const CliQryPathFromCfg *args)
{
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->path))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": no such run");
        return 0;
    }
    mmgr_line_put(args->out, "{\"live\":false,\"blobs\":[");
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = &g_cli_runs_cells);
    unsigned long long at = (bytes == NULL) ? args->from : ((args->from < size) ? args->from : size);
    unsigned long long panicked = 0ull;
    unsigned int count = 0u;
    const QryRecord *record = NULL;
    while ((bytes != NULL) && qry_record_read(bytes, size, &at, &record))
    {
        if (count < CLI_READ_MOST)
        {
            EMBED_CALL(cli_blob_write, QryCtx, .out = args->out, .sequence = at, .record = record,
                       .first = count == 0u);
            panicked = ((panicked == 0ull) && (record->priority == QRY_PANIC)) ? at : panicked;
            count += 1u;
        }
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_runs_cells);
    mmgr_line_put(args->out, "],\"next\":");
    mmgr_line_decimal(args->out, at, 1u);
    mmgr_line_put(args->out, ",\"stepped\":0,\"table\":true,\"panicked\":");
    mmgr_line_decimal(args->out, panicked, 1u);
    mmgr_line_ch(args->out, '}');
    return 1;
}

int cli_qry_read(const CliQryPathFromCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    QryShared *const run = EMBED_CALL(cli_view, QryCtx, .path = args->path);
    if (run == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return cli_qry_path_from.read_file_from(args);
    }
    mmgr_line_put(args->out, "{\"live\":true,\"blobs\":[");
    unsigned long long at = args->from;
    unsigned int handed = 0u;
    const QryRecord *record = NULL;
    while ((handed < CLI_READ_MOST) && qry_member_viewed_read(run, &at, &record))
    {
        EMBED_CALL(cli_blob_write, QryCtx, .out = args->out, .sequence = at - 1ull, .record = record,
                   .first = handed == 0u);
        handed += 1u;
    }
    mmgr_line_put(args->out, "],\"next\":");
    mmgr_line_decimal(args->out, at, 1u);
    mmgr_line_put(args->out, ",\"stepped\":0,\"table\":false,\"panicked\":");
    mmgr_line_decimal(args->out, qry_member_viewed_panicked(run), 1u);
    mmgr_line_ch(args->out, '}');
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return 1;
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
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
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return 1;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->path,
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
        if (!seen && (count < args->room))
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
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
            EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
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
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
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
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
        const int viewed = EMBED_CALL(cli_view, QryCtx, .path = args->path) != NULL;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (s_membered)
    {
        qry_member_parted();
        s_membered = 0;
    }
    if (qry_member_joined(args->path) == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        mmgr_line_put(args->err_msg, "no writer runs for this run: ");
        mmgr_line_put(args->err_msg, args->path);
        return 0;
    }
    qry_member_stopped();
    qry_member_parted();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return 1;
}

// the run the window's work goes to read from QRY the first time it is requested. Called under the lock
static void cli_window_read(void)
{
    if (!s_window_read)
    {
        s_windowed = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "QRY",
                                .value_out = s_window, .room = sizeof(s_window)) &&
                     (s_window[0] != '\0');
        s_window_read = 1;
    }
}

int cli_qry_join(const CliQryCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    cli_window_read();
    const int joined = s_windowed;
    memor.cpy(&(MemoriaCfg){
        .dst = args->path_out,
        .src = joined ? s_window : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = joined ? s_window : "", .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return joined;
}

void cli_qry_join_window(const CliQryPathPanickedCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_window_read = 1;
    s_windowed = (args->path != NULL) &&
                 (cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) < CLI_PATH_MAX_LEN);
    memor.cpy(&(MemoriaCfg){
        .dst = s_window,
        .src = s_windowed ? args->path : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_windowed ? args->path : "", .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

int cli_qry_standing(const CliQryPathPanickedCfg *args)
{
    unsigned long long *panicked = args->panicked;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    QryShared *const run = EMBED_CALL(cli_view, QryCtx, .path = args->path);
    *panicked = (run != NULL) ? qry_member_viewed_panicked(run) : 0ull;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return run != NULL;
}
