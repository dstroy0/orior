// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.c: a run read from its shared memory while its writer runs and from its .qry where none does, its records of a
// name joined, records appended to it, and the run the window is joined to
#include "qry.h"
#include "../text/json_tree.h"

// what a record says is read for at most so many of its bytes, and in whole where it is at most so many; the most
// records one read hands, a read of a run of any size taking a bounded room and going on from where it stopped at the
// next; and the most runs viewed at once
#define CLI_TEXT_SMALL 4096ull

#define CLI_TEXT_MOST 65536ull

#define CLI_READ_MOST 4096u

#define CLI_VIEWS_MOST 64u

// A run viewed while its writer writes: its path and its shared memory mapped
typedef struct
{
    char path[CLI_PATH_LONGEST];
    QryShared *run;
} CliView;

// the runs viewed, the run the window is a member of to append to, and the run the window's own work goes to: one lock
// holds them all
static CliView s_views[CLI_VIEWS_MOST];
static char s_member[CLI_PATH_LONGEST];
static int s_membered;
static char s_window[CLI_PATH_LONGEST];
static int s_windowed;
static int s_window_read;
static CliLock s_lock = CLI_LOCK_INIT;

// whether what a record says is read
static int cli_texted(const char *name, unsigned int length, unsigned int priority, unsigned long long size)
{
    const unsigned int prefix =
        (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = CLI_CONFERENCE, .cap = CLI_CELLS_POOL});
    return (priority < QRY_ORDINARY) ||
           ((length >= prefix) &&
            (memor.cmp(&(MemoriaCfg){.src = name, .other = CLI_CONFERENCE, .bytes = prefix}) == 0)) ||
           (size <= CLI_TEXT_SMALL);
}

// `record` written as the window shows it, at the place `sequence`
static void cli_blob_written(CliLine *out, unsigned long long sequence, const QryRecord *record, int first)
{
    const char *const name = QRY_RECORD_NAME(record);
    const unsigned char *const blob = QRY_RECORD_BLOB(record);
    cli_line_text(out, first ? "{\"name\":" : ",{\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)name, record->name_length);
    cli_line_text(out, ",\"sequence\":");
    cli_line_decimal(out, sequence, 1u);
    cli_line_text(out, ",\"nanoseconds\":");
    cli_line_decimal(out, record->time, 1u);
    cli_line_text(out, ",\"priority\":");
    cli_line_decimal(out, record->priority, 1u);
    cli_line_text(out, ",\"size\":");
    cli_line_decimal(out, record->blob_length, 1u);
    cli_line_text(out, ",\"text\":");
    if (cli_texted(name, record->name_length, record->priority, record->blob_length))
    {
        cli_json_quoted_lossy(out, blob, (record->blob_length < CLI_TEXT_MOST) ? record->blob_length : CLI_TEXT_MOST);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"whole\":true}");
}

// The run at `path` viewed while its writer writes: its shared memory, or NULL where it writes no more or never made
// the run. Called under the lock
static QryShared *cli_view(const char *path)
{
    unsigned int found = CLI_VIEWS_MOST;
    unsigned int free_slot = CLI_VIEWS_MOST;
    for (unsigned int at = 0u; at < CLI_VIEWS_MOST; at += 1u)
    {
        found = ((s_views[at].run != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_views[at].path, .other = path, .cap = CLI_CELLS_POOL})))
                    ? at
                    : found;
        free_slot = ((s_views[at].run == NULL) && (free_slot == CLI_VIEWS_MOST)) ? at : free_slot;
    }
    if (found == CLI_VIEWS_MOST)
    {
        QryShared *const run = qry_member_shared(path);
        if ((run == NULL) || (free_slot == CLI_VIEWS_MOST) ||
            (cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) >= CLI_PATH_LONGEST))
        {
            if (run != NULL)
            {
                qry_member_unshared(run);
            }
            return NULL;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_views[free_slot].path,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        s_views[free_slot].run = run;
        found = free_slot;
    }
    if (qry_member_viewed_writing(s_views[found].run))
    {
        return s_views[found].run;
    }
    qry_member_unshared(s_views[found].run);
    s_views[found].run = NULL;
    return NULL;
}

int cli_qry_read_file_from(const char *path, unsigned long long from, CliLine *out, CliLine *said)
{
    if (!query_is_file(path))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": no such run");
        return 0;
    }
    cli_line_text(out, "{\"live\":false,\"blobs\":[");
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    unsigned long long at = (bytes == NULL) ? from : ((from < size) ? from : size);
    unsigned long long panicked = 0ull;
    unsigned int count = 0u;
    const QryRecord *record = NULL;
    while ((bytes != NULL) && qry_record_read(bytes, size, &at, &record))
    {
        if (count < CLI_READ_MOST)
        {
            cli_blob_written(out, at, record, count == 0u);
            panicked = ((panicked == 0ull) && (record->priority == QRY_PANIC)) ? at : panicked;
            count += 1u;
        }
    }
    if (bytes != NULL)
    {
        cli_file_let_go(bytes, size);
    }
    cli_line_text(out, "],\"next\":");
    cli_line_decimal(out, at, 1u);
    cli_line_text(out, ",\"stepped\":0,\"table\":true,\"panicked\":");
    cli_line_decimal(out, panicked, 1u);
    cli_line_character(out, '}');
    return 1;
}

int cli_qry_read(const char *path, unsigned long long from, CliLine *out, CliLine *said)
{
    cli_lock_take(&s_lock);
    QryShared *const run = cli_view(path);
    if (run == NULL)
    {
        cli_lock_give(&s_lock);
        return cli_qry_read_file_from(path, from, out, said);
    }
    cli_line_text(out, "{\"live\":true,\"blobs\":[");
    unsigned long long at = from;
    unsigned int handed = 0u;
    const QryRecord *record = NULL;
    while ((handed < CLI_READ_MOST) && qry_member_viewed_read(run, &at, &record))
    {
        cli_blob_written(out, at - 1ull, record, handed == 0u);
        handed += 1u;
    }
    cli_line_text(out, "],\"next\":");
    cli_line_decimal(out, at, 1u);
    cli_line_text(out, ",\"stepped\":0,\"table\":false,\"panicked\":");
    cli_line_decimal(out, qry_member_viewed_panicked(run), 1u);
    cli_line_character(out, '}');
    cli_lock_give(&s_lock);
    return 1;
}

// whether a record's name is `name`
static int cli_named(const QryRecord *record, const char *name)
{
    return (record->name_length == cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = QRY_RECORD_NAME(record), .other = name, .bytes = record->name_length}) == 0);
}

int cli_qry_joined_bytes(const char *path, const char *name, CliLine *out, CliLine *said)
{
    cli_lock_take(&s_lock);
    QryShared *const run = cli_view(path);
    const QryRecord *record = NULL;
    if (run != NULL)
    {
        unsigned long long at = 0ull;
        while (qry_member_viewed_read(run, &at, &record))
        {
            cli_line_counted(out, (const char *)QRY_RECORD_BLOB(record),
                             cli_named(record, name) ? record->blob_length : 0ull);
        }
        cli_lock_give(&s_lock);
        return 1;
    }
    cli_lock_give(&s_lock);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    unsigned long long at = 0ull;
    while ((bytes != NULL) && qry_record_read(bytes, size, &at, &record))
    {
        cli_line_counted(out, (const char *)QRY_RECORD_BLOB(record),
                         cli_named(record, name) ? record->blob_length : 0ull);
    }
    if (bytes != NULL)
    {
        cli_file_let_go(bytes, size);
    }
    if ((bytes == NULL) || (at != size))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": the run could not be read whole");
        return 0;
    }
    return 1;
}

unsigned int cli_qry_member_names(const unsigned char *data, unsigned long long size, char (*names)[CLI_NAME_LONGEST],
                                  unsigned int room)
{
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while (qry_record_read(data, size, &at, &record))
    {
        char name[CLI_NAME_LONGEST];
        CliLine line = {name, sizeof(name), 0ull};
        cli_line_counted(&line, QRY_RECORD_NAME(record), record->name_length);
        cli_line_closed(&line);
        int seen = 0;
        for (unsigned int each = 0u; !seen && (each < count); each += 1u)
        {
            seen = cellul.eq(&(CatenaFinitaCfg){.src = names[each], .other = name, .cap = CLI_CELLS_POOL});
        }
        if (!seen && (count < room))
        {
            memor.cpy(&(MemoriaCfg){.dst = names[count],
                                    .src = name,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    return count;
}

unsigned long long cli_qry_member_size(const unsigned char *data, unsigned long long size, const char *name)
{
    unsigned long long at = 0ull;
    unsigned long long joined = 0ull;
    const QryRecord *record = NULL;
    while (qry_record_read(data, size, &at, &record))
    {
        joined += cli_named(record, name) ? record->blob_length : 0ull;
    }
    return joined;
}

void cli_qry_member_joined(const unsigned char *data, unsigned long long size, const char *name, CliLine *out)
{
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while (qry_record_read(data, size, &at, &record))
    {
        cli_line_counted(out, (const char *)QRY_RECORD_BLOB(record),
                         cli_named(record, name) ? record->blob_length : 0ull);
    }
}

int cli_qry_append(const char *path, const char *name, const void *bytes, unsigned long long size,
                   unsigned int priority, CliLine *said)
{
    if (priority > QRY_ORDINARY)
    {
        cli_line_decimal(said, priority, 1u);
        cli_line_text(said, " is not a priority: 0 a panic, 1 critical, 2 ordinary");
        return 0;
    }
    cli_lock_take(&s_lock);
    if (!s_membered || (!cellul.eq(&(CatenaFinitaCfg){.src = s_member, .other = path, .cap = CLI_CELLS_POOL})))
    {
        if (s_membered)
        {
            qry_member_parted();
            s_membered = 0;
        }
        if ((cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) >= CLI_PATH_LONGEST) ||
            (qry_member_joined(path) == NULL))
        {
            cli_lock_give(&s_lock);
            cli_line_text(said, "no writer runs for this run: ");
            cli_line_text(said, path);
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_member,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        s_membered = 1;
    }
    const int appended = qry_member_append(name, bytes, size, priority) != 0ull;
    cli_lock_give(&s_lock);
    if (!appended)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not a record the run takes, or larger than half its ring");
    }
    return appended;
}

int cli_qry_ready(const char *path)
{
    for (unsigned int tried = 0u; tried < 3000u; tried += 1u)
    {
        cli_lock_take(&s_lock);
        const int viewed = cli_view(path) != NULL;
        cli_lock_give(&s_lock);
        if (viewed)
        {
            return 1;
        }
        query_paused(10u);
    }
    return 0;
}

int cli_qry_stop(const char *path, CliLine *said)
{
    cli_lock_take(&s_lock);
    if (s_membered)
    {
        qry_member_parted();
        s_membered = 0;
    }
    if (qry_member_joined(path) == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "no writer runs for this run: ");
        cli_line_text(said, path);
        return 0;
    }
    qry_member_stopped();
    qry_member_parted();
    cli_lock_give(&s_lock);
    return 1;
}

// the run the window's work goes to read from QRY the first time it is asked for. Called under the lock
static void cli_window_read(void)
{
    if (!s_window_read)
    {
        s_windowed = query_environment_read("QRY", s_window, sizeof(s_window)) && (s_window[0] != '\0');
        s_window_read = 1;
    }
}

int cli_qry_joined(char *path)
{
    cli_lock_take(&s_lock);
    cli_window_read();
    const int joined = s_windowed;
    memor.cpy(&(MemoriaCfg){
        .dst = path,
        .src = joined ? s_window : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = joined ? s_window : "", .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&s_lock);
    return joined;
}

void cli_qry_join_window(const char *path)
{
    cli_lock_take(&s_lock);
    s_window_read = 1;
    s_windowed =
        (path != NULL) && (cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) < CLI_PATH_LONGEST);
    memor.cpy(&(MemoriaCfg){
        .dst = s_window,
        .src = s_windowed ? path : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_windowed ? path : "", .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&s_lock);
}

int cli_qry_standing(const char *path, unsigned long long *panicked)
{
    cli_lock_take(&s_lock);
    QryShared *const run = cli_view(path);
    *panicked = (run != NULL) ? qry_member_viewed_panicked(run) : 0ull;
    cli_lock_give(&s_lock);
    return run != NULL;
}
