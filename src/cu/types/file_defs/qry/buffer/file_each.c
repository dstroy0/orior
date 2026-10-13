// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_each.c: each blob of a finished .qry given to a function: the latest of each name, or every one in order
#include "qry_buffer_internal.h"

// a name's latest blob in a file: the name's hash, place and length, and the blob's sequence, bytes' place and count
typedef struct
{
    unsigned long long hash;
    unsigned long long name_at;
    unsigned long long name_length;
    unsigned long long sequence;
    unsigned long long bytes_at;
    unsigned long long size;
} QryLatest;

static int qry_latest_order(const void *left, const void *right)
{
    const unsigned long long one = ((const QryLatest *)left)->sequence;
    const unsigned long long two = ((const QryLatest *)right)->sequence;
    return (one < two) ? -1 : ((one > two) ? 1 : 0);
}

int qry_file_latest_each(const char *path,
                         void (*each)(void *context, const char *name, const unsigned char *bytes,
                                      unsigned long long size),
                         void *context, char *error, size_t room)
{
    if (room != 0u)
    {
        error[0] = '\0';
    }
    QryMapped mapped;
    if (!qry_mapped(&mapped, path))
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s could not be opened", path);
        return 0;
    }
    // a .qry that opens with its table: the rows gone down a hash at a time, each name's latest the last of its rows,
    // which stand in the order of their sequences, and only those blobs read
    QryTableHead front;
    const QryRow *front_rows = NULL;
    if (qry_table_found(&mapped, &front, &front_rows))
    {
        QryLatest *const latest = (QryLatest *)malloc((size_t)(front.rows + 1ull) * sizeof(QryLatest));
        if (latest == NULL)
        {
            qry_unmapped(&mapped);
            snprintf(error, room, "%s: no memory to read its names", path);
            return 0;
        }
        size_t kept = 0u;
        unsigned long long lost = 0ull;
        unsigned long long run = 0ull;
        while (run < front.rows)
        {
            unsigned long long end = run;
            while ((end < front.rows) && (front_rows[end].hash == front_rows[run].hash))
            {
                end += 1ull;
            }
            // each name of the hash kept once, its latest row in its place; two names share a hash only where their
            // hashes meet, and then the names of the run are few
            const size_t first = kept;
            for (unsigned long long row = run; row < end; row += 1ull)
            {
                const QryRow *const one = &front_rows[row];
                if (!qry_row_inside(&front, one) || !qry_row_whole(&mapped, &front, one))
                {
                    lost += one->size;
                    continue;
                }
                size_t named = first;
                while ((named < kept) && !((latest[named].name_length == one->name_length) &&
                                           (memcmp(mapped.bytes + latest[named].name_at, mapped.bytes + one->name_at,
                                                   one->name_length) == 0)))
                {
                    named += 1u;
                }
                latest[named] =
                    (QryLatest){one->hash, one->name_at, one->name_length, one->sequence, one->bytes_at, one->size};
                kept += (named == kept) ? 1u : 0u;
            }
            run = end;
        }
        qsort(latest, kept, sizeof(QryLatest), qry_latest_order);
        char name[QRY_NAME_LONGEST + 1u];
        for (size_t place = 0u; place < kept; place += 1u)
        {
            snprintf(name, sizeof(name), "%.*s", (int)latest[place].name_length,
                     (const char *)(mapped.bytes + latest[place].name_at));
            each(context, name, mapped.bytes + latest[place].bytes_at, latest[place].size);
        }
        free(latest);
        qry_unmapped(&mapped);
        if (lost != 0ull)
        {
            snprintf(error, room, "%s: %llu bytes of blobs whose frames do not read whole were stepped over", path, lost);
        }
        return 1;
    }
    // the frames counted, a name at most one a frame, and a table of twice as many slots: each frame read by its head
    // and stepped over whole, a stretch none opens stepped over to the next
    size_t slots = 64u;
    for (unsigned long long walk = 0ull; walk < mapped.size;)
    {
        QryBlob blob;
        if (qry_blob_read(mapped.bytes, mapped.size, walk, &blob))
        {
            slots += 2u;
            walk = blob.next;
        }
        else
        {
            walk = qry_next_whole(mapped.bytes, mapped.size, walk);
        }
    }
    size_t power = 64u;
    while (power < slots)
    {
        power *= 2u;
    }
    QryLatest *const table = (QryLatest *)calloc(power, sizeof(QryLatest));
    if (table == NULL)
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s: no memory to read its names", path);
        return 0;
    }
    unsigned long long skipped = 0ull;
    unsigned long long at = 0ull;
    while (at < mapped.size)
    {
        QryBlob blob;
        if (!qry_blob_read(mapped.bytes, mapped.size, at, &blob))
        {
            const unsigned long long next = qry_next_whole(mapped.bytes, mapped.size, at);
            skipped += next - at;
            at = next;
            continue;
        }
        // the blob kept where its name is new, or where it is later than the one kept; a hash is never 0, the mark of a
        // slot no name holds
        const unsigned long long hash = qry_hash((const char *)(mapped.bytes + blob.name_at), (size_t)blob.name_length);
        for (size_t probe = (size_t)hash & (power - 1u);; probe = (probe + 1u) & (power - 1u))
        {
            QryLatest *const slot = &table[probe];
            if (slot->hash == 0ull)
            {
                *slot = (QryLatest){hash, blob.name_at, blob.name_length, blob.sequence, blob.bytes_at, blob.size};
                break;
            }
            if ((slot->hash == hash) && (slot->name_length == blob.name_length) &&
                (memcmp(mapped.bytes + slot->name_at, mapped.bytes + blob.name_at, (size_t)blob.name_length) == 0))
            {
                if (blob.sequence > slot->sequence)
                {
                    slot->sequence = blob.sequence;
                    slot->bytes_at = blob.bytes_at;
                    slot->size = blob.size;
                }
                break;
            }
        }
        at = blob.next;
    }
    // the kept blobs gathered and put in the order of their sequences, each handed with its name
    size_t kept = 0u;
    for (size_t probe = 0u; probe < power; probe += 1u)
    {
        if (table[probe].hash != 0ull)
        {
            table[kept] = table[probe];
            kept += 1u;
        }
    }
    qsort(table, kept, sizeof(QryLatest), qry_latest_order);
    char name[QRY_NAME_LONGEST + 1u];
    for (size_t place = 0u; place < kept; place += 1u)
    {
        snprintf(name, sizeof(name), "%.*s", (int)table[place].name_length,
                 (const char *)(mapped.bytes + table[place].name_at));
        each(context, name, mapped.bytes + table[place].bytes_at, table[place].size);
    }
    free(table);
    qry_unmapped(&mapped);
    if (skipped != 0ull)
    {
        snprintf(error, room, "%s: %llu bytes no frame opens were stepped over", path, skipped);
    }
    return 1;
}

int qry_file_each(const char *path,
                  void (*each)(void *context, const char *name, unsigned long long sequence, const unsigned char *bytes,
                               unsigned long long size),
                  void *context, char *error, size_t room)
{
    if (room != 0u)
    {
        error[0] = '\0';
    }
    QryMapped mapped;
    if (!qry_mapped(&mapped, path))
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s could not be opened", path);
        return 0;
    }
    // a .qry that opens with its table is read by its rows, and one that does not by its frames, counted first: a blob
    // at most a row or a frame
    QryTableHead front;
    const QryRow *front_rows = NULL;
    const int tabled = qry_table_found(&mapped, &front, &front_rows);
    size_t most = tabled ? (size_t)front.rows : 0u;
    for (unsigned long long walk = 0ull; !tabled && (walk < mapped.size);)
    {
        QryBlob blob;
        if (qry_blob_read(mapped.bytes, mapped.size, walk, &blob))
        {
            most += 1u;
            walk = blob.next;
        }
        else
        {
            walk = qry_next_whole(mapped.bytes, mapped.size, walk);
        }
    }
    QryLatest *const kept = (QryLatest *)malloc((most + 1u) * sizeof(QryLatest));
    if (kept == NULL)
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s: no memory to read its blobs", path);
        return 0;
    }
    // every row that reads whole kept, a row that does not counted as lost; or every frame read by its head, a stretch
    // none opens stepped over to the next and counted
    size_t count = 0u;
    unsigned long long lost = 0ull;
    for (unsigned long long row = 0ull; tabled && (row < front.rows); row += 1ull)
    {
        const QryRow *const one = &front_rows[row];
        if (!qry_row_inside(&front, one) || !qry_row_whole(&mapped, &front, one))
        {
            lost += one->size;
            continue;
        }
        kept[count] = (QryLatest){one->hash, one->name_at, one->name_length, one->sequence, one->bytes_at, one->size};
        count += 1u;
    }
    for (unsigned long long at = 0ull; !tabled && (at < mapped.size);)
    {
        QryBlob blob;
        if (!qry_blob_read(mapped.bytes, mapped.size, at, &blob))
        {
            const unsigned long long next = qry_next_whole(mapped.bytes, mapped.size, at);
            lost += next - at;
            at = next;
            continue;
        }
        kept[count] = (QryLatest){0ull, blob.name_at, blob.name_length, blob.sequence, blob.bytes_at, blob.size};
        count += 1u;
        at = blob.next;
    }
    qsort(kept, count, sizeof(QryLatest), qry_latest_order);
    char name[QRY_NAME_LONGEST + 1u];
    for (size_t place = 0u; place < count; place += 1u)
    {
        snprintf(name, sizeof(name), "%.*s", (int)kept[place].name_length,
                 (const char *)(mapped.bytes + kept[place].name_at));
        each(context, name, kept[place].sequence, mapped.bytes + kept[place].bytes_at, kept[place].size);
    }
    free(kept);
    qry_unmapped(&mapped);
    if ((lost != 0ull) && tabled)
    {
        snprintf(error, room, "%s: %llu bytes of blobs whose frames do not read whole were stepped over", path, lost);
    }
    else if (lost != 0ull)
    {
        snprintf(error, room, "%s: %llu bytes no frame opens were stepped over", path, lost);
    }
    return 1;
}
