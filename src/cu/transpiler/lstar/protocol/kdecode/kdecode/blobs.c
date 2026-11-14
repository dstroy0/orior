// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// blobs.c: a run's records walked in the order the writer wrote them, the run read whole, and its blobs of a name
// written whole
#include "kdecode_internal.h"

// a blob written whole: a line of its sequence, its name and its size, then its bytes, a NUL as a space, and a newline
static void blob_printed(const char *name, unsigned int name_length, unsigned long long sequence,
                         const unsigned char *bytes, unsigned long long size)
{
    mmgr_line_decimal(&s_displayed, sequence, 0u);
    mmgr_line_ch(&s_displayed, ' ');
    mmgr_line_put_n(&s_displayed, name, name_length);
    kdecode_decimal_displayed(size);
    mmgr_line_ch(&s_displayed, '\n');
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        // the text is passed every 64 KiB at max, far inside the half of its room it may still take
        if ((at & 0xffffull) == 0ull)
        {
            kdecode_displayed_passed(0);
        }
        mmgr_line_ch(&s_displayed, (bytes[at] == 0u) ? ' ' : (char)bytes[at]);
    }
    kdecode_line_ended();
}

// every record of the `size` bytes at `bytes`, a .qry mapped, passed to `each` with `context`, in the order the writer
// wrote them, its time in place of its place: 1, or 0 where the bytes are not records whole
int kdecode_records_each(const unsigned char *bytes, unsigned long long size,
                         void (*each)(void *context, const char *name, unsigned int name_length,
                                      unsigned long long time, const unsigned char *blob, unsigned long long blob_size),
                         void *context)
{
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while ((bytes != NULL) && qry_record_read(bytes, size, &at, &record))
    {
        each(context, QRY_RECORD_NAME(record), record->name_length, record->time, QRY_RECORD_BLOB(record),
             record->blob_length);
    }
    return (bytes != NULL) && (at == size);
}

static void records_passed(void *context, const char *name, unsigned int name_length, unsigned long long time,
                           const unsigned char *blob, unsigned long long blob_size)
{
    (void)context;
    (void)name;
    (void)name_length;
    (void)time;
    (void)blob;
    (void)blob_size;
}

int kdecode_run_whole(const unsigned char *bytes, unsigned long long size)
{
    return (size > 0ull) && kdecode_records_each(bytes, size, records_passed, NULL);
}

// what the blob walk of kdecode_blobs retains: the name wanted, whether the latest alone is written, and that one where
// it is in the mapped run
typedef struct
{
    const char *name;
    int latest;
    unsigned long long sequence;
    const unsigned char *bytes;
    unsigned long long size;
} KdecodeBlobs;

static void blobs_each(void *context, const char *name, unsigned int name_length, unsigned long long sequence,
                       const unsigned char *bytes, unsigned long long size)
{
    KdecodeBlobs *const walk = (KdecodeBlobs *)context;
    if ((strlen(walk->name) != name_length) || (memcmp(name, walk->name, name_length) != 0))
    {
        return;
    }
    if (!walk->latest)
    {
        blob_printed(name, name_length, sequence, bytes, size);
        return;
    }
    walk->bytes = bytes;
    walk->size = size;
    walk->sequence = sequence;
}

int kdecode_blobs(const char *path, const char *name, int latest)
{
    KdecodeBlobs walk = {name, latest, 0ull, NULL, 0ull};
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    const int read = kdecode_records_each(bytes, size, blobs_each, &walk);
    if (!read)
    {
        kdecode_displayed_passed(1);
        mmgr_line_put(&s_displayed, "orior_kdecode: the run could not be read whole\n");
        kdecode_failure_passed();
    }
    if (read && latest && (walk.bytes != NULL))
    {
        blob_printed(name, (unsigned int)strlen(name), walk.sequence, walk.bytes, walk.size);
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    return read;
}
