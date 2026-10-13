// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mapped.c: a finished .qry mapped, its table found and its rows read, and a file's blobs joined out of it
#include "qry_buffer_internal.h"

// a blob of a name a file holds: its sequence, and its bytes' place and count in the bytes read off the file
typedef struct
{
    unsigned long long sequence;
    unsigned long long at;
    unsigned long long size;
} QryFiled;

static int qry_filed_order(const void *left, const void *right)
{
    const unsigned long long one = ((const QryFiled *)left)->sequence;
    const unsigned long long two = ((const QryFiled *)right)->sequence;
    return (one < two) ? -1 : ((one > two) ? 1 : 0);
}

int qry_mapped(QryMapped *mapped, const char *path)
{
    memset(mapped, 0, sizeof(*mapped));
#if defined(_WIN32)
    mapped->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    LARGE_INTEGER length;
    if ((mapped->file == INVALID_HANDLE_VALUE) || !GetFileSizeEx(mapped->file, &length))
    {
        if (mapped->file != INVALID_HANDLE_VALUE)
        {
            CloseHandle(mapped->file);
        }
        mapped->file = NULL;
        return 0;
    }
    // a file's length is not negative
    mapped->size = (unsigned long long)length.QuadPart;
    if (mapped->size == 0ull)
    {
        return 1;
    }
    mapped->section = CreateFileMappingA(mapped->file, NULL, PAGE_READONLY, 0u, 0u, NULL);
    mapped->bytes = (mapped->section != NULL)
                        ? (const unsigned char *)MapViewOfFile(mapped->section, FILE_MAP_READ, 0u, 0u, 0u)
                        : NULL;
    return mapped->bytes != NULL;
#else
    mapped->file = open(path, O_RDONLY);
    struct stat held;
    if ((mapped->file < 0) || (fstat(mapped->file, &held) != 0))
    {
        return 0;
    }
    mapped->size = (unsigned long long)held.st_size;
    if (mapped->size == 0ull)
    {
        return 1;
    }
    void *const view = mmap(NULL, (size_t)mapped->size, PROT_READ, MAP_PRIVATE, mapped->file, 0);
    mapped->bytes = (view != MAP_FAILED) ? (const unsigned char *)view : NULL;
    return mapped->bytes != NULL;
#endif
}

void qry_unmapped(QryMapped *mapped)
{
#if defined(_WIN32)
    if (mapped->bytes != NULL)
    {
        UnmapViewOfFile(mapped->bytes);
    }
    if (mapped->section != NULL)
    {
        CloseHandle(mapped->section);
    }
    if (mapped->file != NULL)
    {
        CloseHandle(mapped->file);
    }
#else
    if (mapped->bytes != NULL)
    {
        munmap((void *)mapped->bytes, (size_t)mapped->size);
    }
    if (mapped->file >= 0)
    {
        close(mapped->file);
    }
#endif
    memset(mapped, 0, sizeof(*mapped));
}

// the next place past `at` where a frame that reads whole begins, or `size` where none does: a frame starts on a word,
// and each word past `at` whose bytes are the frame magic is read as one
unsigned long long qry_next_whole(const unsigned char *bytes, unsigned long long size, unsigned long long at)
{
    QryBlob blob;
    for (unsigned long long from = (at + 8ull) & ~7ull; (from < size) && ((size - from) >= sizeof(QryFrameHead));
         from += 8ull)
    {
        if ((memcmp(bytes + from, QRY_FRAME_MAGIC, 8u) == 0) && qry_blob_read(bytes, size, from, &blob))
        {
            return from;
        }
    }
    return size;
}

// The table a mapped .qry opens with (qry.h), its head copied to `table` and its rows found in place: 1, or 0 where the
// .qry opens with none, or with one whose rows or blobs do not lie inside it, and it is read by its frames
int qry_table_found(const QryMapped *mapped, QryTableHead *table, const QryRow **rows)
{
    if ((mapped->size < sizeof(QryTableHead)) || (memcmp(mapped->bytes, QRY_TABLE_MAGIC, sizeof(table->magic)) != 0))
    {
        return 0;
    }
    memcpy(table, mapped->bytes, sizeof(*table));
    const int sized = (table->row_bytes == sizeof(QryRow)) && (table->rows_at >= sizeof(QryTableHead)) &&
                      (table->rows_at <= mapped->size) &&
                      (table->rows <= ((mapped->size - table->rows_at) / sizeof(QryRow)));
    const unsigned long long rows_end = sized ? (table->rows_at + (table->rows * sizeof(QryRow))) : 0ull;
    if (!sized || (table->blobs_at < rows_end) || (table->blobs_at > mapped->size) ||
        (table->blobs_bytes > (mapped->size - table->blobs_at)))
    {
        return 0;
    }
    // the rows start on a whole row past the mapping's start, which the system places on a page; they are held to the
    // head's CRC, the root of every check past it, and rows that do not hold it leave the file read by its frames
    *rows = (const QryRow *)(const void *)(mapped->bytes + table->rows_at);
    return qry_crc(mapped->bytes + table->rows_at, table->rows * sizeof(QryRow)) == table->rows_crc;
}

// whether `row` of a mapped .qry's table has its name and its bytes inside the blobs
int qry_row_inside(const QryTableHead *table, const QryRow *row)
{
    const unsigned long long end = table->blobs_at + table->blobs_bytes;
    return (row->name_at >= table->blobs_at) && (row->name_length <= end) &&
           (row->name_at <= (end - row->name_length)) && (row->bytes_at >= table->blobs_at) && (row->bytes_at <= end) &&
           (row->size <= (end - row->bytes_at));
}

// whether `row` of a mapped .qry's table names `name`, `length` bytes whose hash is `hash`, with its name and its bytes
// inside the blobs
static int qry_row_filed(const QryMapped *mapped, const QryTableHead *table, const QryRow *row, unsigned long long hash,
                         const char *name, size_t length)
{
    return (row->hash == hash) && (row->name_length == length) && qry_row_inside(table, row) &&
           (memcmp(mapped->bytes + row->name_at, name, length) == 0);
}

// whether the blob `row` of a mapped .qry's table jumps to reads whole by its own frame, is the blob the row says, and
// holds the bytes the row's CRC was taken of: what a reader reads it reads checked, and a blob the file lost is told
// from one it holds
int qry_row_whole(const QryMapped *mapped, const QryTableHead *table, const QryRow *row)
{
    QryBlob blob;
    const unsigned long long end = table->blobs_at + table->blobs_bytes;
    const unsigned long long opened = sizeof(QryFrameHead);
    return (row->name_at >= (table->blobs_at + opened)) && (row->name_at < end) &&
           qry_blob_read(mapped->bytes, end, row->name_at - opened, &blob) && (blob.name_at == row->name_at) &&
           (blob.bytes_at == row->bytes_at) && (blob.size == row->size) && (blob.sequence == row->sequence) &&
           (qry_crc(mapped->bytes + row->bytes_at, row->size) == row->crc);
}

// the first of `count` rows whose hash is `hash`, or the first past it where none is: the rows stand in the order of
// their hashes, and a search halves them
static unsigned long long qry_rows_searched(const QryRow *rows, unsigned long long count, unsigned long long hash)
{
    unsigned long long low = 0ull;
    unsigned long long high = count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        if (rows[middle].hash < hash)
        {
            low = middle + 1ull;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

int qry_file_joined(const char *path, const char *name, unsigned char **bytes, unsigned long long *size, char *error,
                    size_t room)
{
    size_t length = 0u;
    *bytes = NULL;
    *size = 0ull;
    if (room != 0u)
    {
        error[0] = '\0';
    }
    if (!qry_name_taken(name, &length))
    {
        snprintf(error, room, "%s is no name a blob takes", (name != NULL) ? name : "(none)");
        return 0;
    }
    QryMapped mapped;
    if (!qry_mapped(&mapped, path))
    {
        qry_unmapped(&mapped);
        snprintf(error, room, "%s could not be opened", path);
        return 0;
    }
    // a .qry that opens with its table: the name's rows found by its hash and their bytes joined, the rows of one name
    // standing in the order of their sequences, and no other blob read
    QryTableHead table;
    const QryRow *rows = NULL;
    if (qry_table_found(&mapped, &table, &rows))
    {
        const unsigned long long hash = qry_hash(name, length);
        unsigned long long joined_room = 0ull;
        unsigned long long lost = 0ull;
        int joined = 1;
        for (unsigned long long row = qry_rows_searched(rows, table.rows, hash);
             joined && (row < table.rows) && (rows[row].hash == hash); row += 1ull)
        {
            if (rows[row].name_length != length)
            {
                continue;
            }
            // a blob of the name's hash that does not read whole may be the name's, its own name lost with it: it is
            // counted as lost, never passed over unsaid
            if (!qry_row_whole(&mapped, &table, &rows[row]))
            {
                lost += rows[row].size;
                continue;
            }
            if (qry_row_filed(&mapped, &table, &rows[row], hash, name, length))
            {
                joined = qry_join_added(bytes, size, &joined_room, mapped.bytes + rows[row].bytes_at, rows[row].size);
            }
        }
        qry_unmapped(&mapped);
        if (!joined)
        {
            free(*bytes);
            *bytes = NULL;
            *size = 0ull;
            snprintf(error, room, "%s: no memory to join the blobs of %s", path, name);
        }
        else if (lost != 0ull)
        {
            snprintf(error, room, "%s: %llu bytes of %s whose frames do not read whole were stepped over", path, lost,
                     name);
        }
        return joined;
    }
    QryFiled *filed = NULL;
    size_t filed_count = 0u;
    size_t filed_room = 0u;
    unsigned long long skipped = 0ull;
    int whole = 1;
    // each blob read by its frame in place and taken where it is of `name`; a stretch no frame opens is stepped over to
    // the next frame that reads whole
    unsigned long long at = 0ull;
    while (whole && (at < mapped.size))
    {
        QryBlob blob;
        if (!qry_blob_read(mapped.bytes, mapped.size, at, &blob))
        {
            const unsigned long long next = qry_next_whole(mapped.bytes, mapped.size, at);
            skipped += next - at;
            at = next;
            continue;
        }
        if ((blob.name_length == length) && (memcmp(mapped.bytes + blob.name_at, name, length) == 0))
        {
            if (filed_count == filed_room)
            {
                filed_room = (filed_room == 0u) ? 64u : (filed_room * 2u);
                QryFiled *const larger = (QryFiled *)realloc(filed, filed_room * sizeof(QryFiled));
                whole = (larger != NULL);
                filed = (larger != NULL) ? larger : filed;
            }
            if (whole)
            {
                filed[filed_count] = (QryFiled){blob.sequence, blob.bytes_at, blob.size};
                filed_count += 1u;
            }
        }
        at = blob.next;
    }
    // the blobs put back in the order handed, by their sequence, and joined straight from the mapped file
    if (whole && (filed_count != 0u))
    {
        qsort(filed, filed_count, sizeof(QryFiled), qry_filed_order);
    }
    unsigned long long joined_room = 0ull;
    for (size_t place = 0u; whole && (place < filed_count); place += 1u)
    {
        whole = qry_join_added(bytes, size, &joined_room, mapped.bytes + filed[place].at, filed[place].size);
    }
    free(filed);
    qry_unmapped(&mapped);
    if (!whole)
    {
        free(*bytes);
        *bytes = NULL;
        *size = 0ull;
        snprintf(error, room, "%s: no memory to join the blobs of %s", path, name);
        return 0;
    }
    if (skipped != 0ull)
    {
        snprintf(error, room, "%s: %llu bytes no frame opens were stepped over", path, skipped);
    }
    return 1;
}
