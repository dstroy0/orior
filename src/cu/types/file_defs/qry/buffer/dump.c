// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dump.c: the dump: the current files put, the marks and firsts written, a range of blobs written, the rows ordered and the table dumped
#include "qry_buffer_internal.h"

// Every current file the run holds written in its place as the run ends: of each name, its latest blob marked
// QRY_MARK_FILED (qry_file_put), up to the buffer's tail `tail`. 1, or 0 where one was not written whole; `count` the
// files written
int qry_files_put(QryBuffer *buffer, unsigned long long tail, unsigned long long *count)
{
    *count = 0ull;
    unsigned long long blobs = 0ull;
    QryBlob blob;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        blobs += 1ull;
    }
    QryFileBlob *const filed = (QryFileBlob *)malloc((size_t)(blobs + 1ull) * sizeof(QryFileBlob));
    if (filed == NULL)
    {
        return 0;
    }
    unsigned long long kept = 0ull;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        QryFrameHead opened;
        memcpy(&opened, buffer->data + walk, sizeof(opened));
        if ((opened.marks & QRY_MARK_FILED) != 0u)
        {
            filed[kept] = (QryFileBlob){qry_hash((const char *)(buffer->data + blob.name_at), (size_t)blob.name_length),
                                        blob.name_at, blob.name_length, blob.sequence, walk};
            kept += 1ull;
        }
    }
    s_file_blobs_data = buffer->data;
    qsort(filed, (size_t)kept, sizeof(QryFileBlob), qry_file_blob_order);
    int whole = 1;
    for (unsigned long long at = 0ull; at < kept; at += 1ull)
    {
        // the last of each name's run, its latest
        const int last = ((at + 1ull) == kept) || (filed[at + 1ull].hash != filed[at].hash) ||
                         (filed[at + 1ull].name_length != filed[at].name_length) ||
                         (memcmp(buffer->data + filed[at + 1ull].name_at, buffer->data + filed[at].name_at,
                                 (size_t)filed[at].name_length) != 0);
        if (last)
        {
            whole &= qry_file_put(buffer, filed[at].at);
            *count += 1ull;
        }
    }
    free(filed);
    return whole;
}

// The bytes of the frame at `at` in the buffer written where its head's marks say, before the hand is told its blob
// is on disk: into the named stream of the .qry that bears its name, `<path>:<name>`, where it is marked
// QRY_MARK_STREAMED. A blob marked QRY_MARK_FILED stays in the run until it ends (qry_files_put). 1, or 0 where it is
// marked for a stream and was not written whole; the hand reads it back and is told so
static int qry_marked_written(QryBuffer *buffer, unsigned long long at)
{
    QryFrameHead opened;
    memcpy(&opened, buffer->data + at, sizeof(opened));
    const char *const name = (const char *)(buffer->data + at + sizeof(opened));
    const unsigned char *const bytes = buffer->data + at + opened.bytes_at;
    if ((opened.marks & QRY_MARK_STREAMED) == 0u)
    {
        return 1;
    }
#if defined(_WIN32)
    char stream[sizeof(buffer->path) + QRY_NAME_LONGEST + 2u];
    // a name is at most QRY_NAME_LONGEST bytes, which an int counts
    snprintf(stream, sizeof(stream), "%s:%.*s", buffer->path, (int)opened.name_length, name);
    return qry_bytes_put(stream, bytes, opened.size);
#else
    (void)name;
    (void)bytes;
    return 0;
#endif
}

// every marked frame from `from` to `to` in the buffer written where its marks say (qry_marked_written): 1, or 0
// where one was not written whole
int qry_marks_written(QryBuffer *buffer, unsigned long long from, unsigned long long to)
{
    int whole = 1;
    QryBlob blob;
    for (unsigned long long walk = from; (walk < to) && qry_blob_read(buffer->data, to, walk, &blob); walk = blob.next)
    {
        whole &= qry_marked_written(buffer, walk);
    }
    return whole;
}

// Every panic and critical blob handed since the writer last looked written at the file's end through to the disk
// itself, the panics first, then the critical blobs, each in the order handed, each marked one written where its marks
// say, and the latest of them made the latest on disk; the tail, the places and the latest sequence handed read with
// them kept in `state`. 1, or 0 where more were handed than the buffer keeps places for, and the caller writes the
// queue in order and flushes it through instead
int qry_firsts_written(QryBuffer *buffer, QryFile *file, QryWriterState *state)
{
    QryHead *const head = buffer->head;
    static QrySpan s_taken[QRY_CRITICAL_SPANS];
    size_t count = 0u;
    qry_lock(buffer);
    const unsigned long long spans = atomic_load(&head->spans);
    const unsigned long long handed = atomic_load(&head->critical_handed);
    state->tail_seen = atomic_load(&head->tail);
    state->spans_read = spans;
    state->handed_read = handed;
    unsigned long long *const tail_seen = &state->tail_seen;
    const int kept = ((spans - state->spans_seen) <= QRY_CRITICAL_SPANS) &&
                     ((state->early_count + (size_t)(spans - state->spans_seen)) <= (2u * QRY_CRITICAL_SPANS));
    for (unsigned long long place = state->spans_seen; kept && (place < spans); place += 1ull)
    {
        s_taken[count] = head->span[place % QRY_CRITICAL_SPANS];
        count += 1u;
    }
    qry_unlock(buffer);
    if (!kept)
    {
        return 0;
    }
    if (count != 0u)
    {
        const unsigned long long began = qry_nanoseconds();
        int whole = 1;
        unsigned long long bytes = 0ull;
        // the panics first, then the critical blobs, each kind in the order handed
        for (unsigned int priority = QRY_PANIC; priority < QRY_ORDINARY; priority += 1u)
        {
            for (size_t place = 0u; place < count; place += 1u)
            {
                if (s_taken[place].priority != priority)
                {
                    continue;
                }
                whole &= qry_file_added(file, buffer->data + s_taken[place].at, s_taken[place].size, 1);
                whole &= qry_marked_written(buffer, s_taken[place].at);
                bytes += s_taken[place].size;
                // the blob read off its frame: said, a panic or a critical blob as it is written first, and a panic's
                // time from its hand to the disk
                QryBlob blob;
                const int read = qry_blob_read(buffer->data, *tail_seen, s_taken[place].at, &blob);
                if (read)
                {
                    qry_blob_said(buffer, blob.sequence, blob.nanoseconds, priority, QRY_MEMBERS,
                                  (const char *)(buffer->data + blob.name_at), (size_t)blob.name_length, blob.size);
                }
                if (priority == QRY_PANIC)
                {
                    const unsigned long long now = qry_nanoseconds();
                    if (read && (now > blob.nanoseconds))
                    {
                        const double milliseconds = (double)(now - blob.nanoseconds) / 1000000.0;
                        state->writes.panic_slowest =
                            (milliseconds > state->writes.panic_slowest) ? milliseconds : state->writes.panic_slowest;
                    }
                    state->writes.panics += 1ull;
                }
            }
        }
        // kept in the order of their places, the order handed, for the queue's write to step over
        for (size_t place = 0u; place < count; place += 1u)
        {
            state->early[state->early_count] = s_taken[place];
            state->early_count += 1u;
        }
        qry_write_counted(&state->writes, began, bytes, whole);
    }
    state->spans_seen = spans;
    if (handed > atomic_load(&head->critical_written))
    {
        atomic_store(&head->critical_written, handed);
    }
    return 1;
}

// The buffer's bytes `from` to `to` written in order at the file's end a slice at a time, the blobs written ahead of it
// stepped over and let go, and between slices every panic or critical blob handed meanwhile let through first: one
// write, timed. A slice ends on a blob's end, the first past QRY_SLICE_BYTES or the stretch's, never inside a blob: a
// blob let through between two slices stands whole between two whole blobs
void qry_range_written(QryBuffer *buffer, QryFile *file, unsigned long long from, unsigned long long to,
                              QryWriterState *state)
{
    if (to <= from)
    {
        return;
    }
    const unsigned long long began = qry_nanoseconds();
    unsigned long long at = from;
    unsigned long long bytes = 0ull;
    int whole = 1;
    // where more were handed than the buffer keeps places for, the rest of the stretch looks for none: they are written
    // in order and flushed through by the writer's loop
    int overflowed = 0;
    while (at < to)
    {
        // the next blob written ahead within the stretch, the earliest place, where there is one
        size_t next = state->early_count;
        for (size_t place = 0u; place < state->early_count; place += 1u)
        {
            if ((state->early[place].at >= at) && (state->early[place].at < to) &&
                ((next == state->early_count) || (state->early[place].at < state->early[next].at)))
            {
                next = place;
            }
        }
        const unsigned long long stop = (next != state->early_count) ? state->early[next].at : to;
        // the slice's end stepped blob by blob from its start, each blob's end read off its own frame
        unsigned long long slice_end = at;
        QryBlob blob;
        while ((slice_end < stop) && ((slice_end - at) < QRY_SLICE_BYTES) &&
               qry_blob_read(buffer->data, stop, slice_end, &blob))
        {
            slice_end = blob.next;
        }
        // a stretch whose frames do not read is written whole to its stop, as it stands in the buffer
        slice_end = ((slice_end == at) && (at < stop)) ? stop : slice_end;
        whole &= qry_file_added(file, buffer->data + at, slice_end - at, 0);
        bytes += slice_end - at;
        at = slice_end;
        if ((next != state->early_count) && (at == state->early[next].at))
        {
            at += state->early[next].size;
            state->early[next] = state->early[state->early_count - 1u];
            state->early_count -= 1u;
        }
        // a panic or critical blob handed since the stretch was read goes before the next slice; its place lies past
        // the stretch, which was read with every place before it
        if (!overflowed && (at < to) && (atomic_load(&buffer->head->spans) != state->spans_seen))
        {
            overflowed = !qry_firsts_written(buffer, file, state);
        }
    }
    qry_write_counted(&state->writes, began, bytes, whole);
    atomic_store(&buffer->head->written, to);
}

static int qry_row_order(const void *left, const void *right)
{
    const QryRow *const one = (const QryRow *)left;
    const QryRow *const two = (const QryRow *)right;
    if (one->hash != two->hash)
    {
        return (one->hash < two->hash) ? -1 : 1;
    }
    return (one->sequence < two->sequence) ? -1 : ((one->sequence > two->sequence) ? 1 : 0);
}

// a row of every blob before `tail`, in memory of the caller's to free: the table's own where it holds a row of every
// blob, else each read off the blob's frame. NULL where there was no memory; `count` the rows
static QryRow *qry_rows_taken(QryBuffer *buffer, unsigned long long tail, unsigned long long rows,
                              unsigned long long *count)
{
    *count = 0ull;
    if (buffer->head->table_full == 0ull)
    {
        QryRow *const taken = (QryRow *)malloc((size_t)(rows + 1ull) * sizeof(QryRow));
        if (taken != NULL)
        {
            // the rows fit the buffer's table, whose bytes a size_t counts
            memcpy(taken, buffer->row, (size_t)(rows * sizeof(QryRow)));
            *count = rows;
        }
        return taken;
    }
    unsigned long long blobs = 0ull;
    QryBlob blob;
    for (unsigned long long walk = 0ull; (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob);
         walk = blob.next)
    {
        blobs += 1ull;
    }
    QryRow *const taken = (QryRow *)malloc((size_t)(blobs + 1ull) * sizeof(QryRow));
    for (unsigned long long walk = 0ull;
         (taken != NULL) && (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob); walk = blob.next)
    {
        // a name and a priority are each far below what an unsigned int counts
        taken[*count] = (QryRow){qry_hash((const char *)(buffer->data + blob.name_at), (size_t)blob.name_length),
                                 blob.sequence,
                                 blob.nanoseconds,
                                 0ull,
                                 blob.name_at,
                                 blob.bytes_at,
                                 blob.size,
                                 (unsigned int)blob.name_length,
                                 (unsigned int)blob.priority};
        *count += 1ull;
    }
    return taken;
}

// The run dumped from the buffer as the .qry readers take (qry.h): the table's head, every row put in the order of its
// name's hash and then its sequence with its places made the file's, then every blob in the order handed, written as
// it stands in the buffer; written beside the .qry, flushed through to the disk, and put in the .qry's place. The .qry
// the writer wrote as the run ran is left as it is where any of that fails. 1, or 0; `count` the rows dumped
int qry_table_dumped(QryBuffer *buffer, unsigned long long *count)
{
    qry_lock(buffer);
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    const unsigned long long rows = atomic_load(&buffer->head->rows);
    qry_unlock(buffer);
    QryRow *const taken = qry_rows_taken(buffer, tail, rows, count);
    if (taken == NULL)
    {
        return 0;
    }
    qsort(taken, (size_t)*count, sizeof(QryRow), qry_row_order);
    QryTableHead table;
    memset(&table, 0, sizeof(table));
    memcpy(table.magic, QRY_TABLE_MAGIC, sizeof(table.magic));
    table.rows = *count;
    table.rows_at = sizeof(QryTableHead);
    table.row_bytes = sizeof(QryRow);
    // the frames from the first boundary past the rows, where the buffer's own alignment holds in the file
    table.blobs_at = qry_aligned(table.rows_at + (*count * sizeof(QryRow)));
    table.blobs_bytes = tail;
    table.frame_align = QRY_FRAME_ALIGN;
    static const unsigned char s_zeros[QRY_FRAME_ALIGN] = {0};
    const unsigned long long padding = table.blobs_at - table.rows_at - (*count * sizeof(QryRow));
    // each row carries the CRC its member took as it handed the blob; a row read off a frame, where the table was
    // full, is given its blob's here. Each row's places are made the file's, and the rows' CRC is the root of the
    // file's checks
    for (unsigned long long row = 0ull; row < *count; row += 1ull)
    {
        if (taken[row].crc == 0ull)
        {
            taken[row].crc = qry_crc(buffer->data + taken[row].bytes_at, taken[row].size);
        }
        taken[row].name_at += table.blobs_at;
        taken[row].bytes_at += table.blobs_at;
    }
    table.rows_crc = qry_crc((const unsigned char *)taken, *count * sizeof(QryRow));
    char dumped[sizeof(buffer->path) + 8u];
    snprintf(dumped, sizeof(dumped), "%s.tmp", buffer->path);
    QryFile file;
    int whole = qry_file_opened(&file, dumped);
    if (whole)
    {
        whole = qry_file_added(&file, (const unsigned char *)&table, sizeof(table), 0) &&
                qry_file_added(&file, (const unsigned char *)taken, *count * sizeof(QryRow), 0) &&
                qry_file_added(&file, s_zeros, padding, 0) && qry_file_added(&file, buffer->data, tail, 0);
        whole = qry_file_closed(&file) && whole;
    }
    free(taken);
#if defined(_WIN32)
    whole = whole && (MoveFileExA(dumped, buffer->path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0);
#else
    whole = whole && (rename(dumped, buffer->path) == 0);
#endif
    if (!whole)
    {
        remove(dumped);
    }
    return whole;
}
