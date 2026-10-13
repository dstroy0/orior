// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disk.c: the disk: a file opened, bytes added through or in bulk, made durable and closed, a write counted, and a current file put in its place
#include "qry_buffer_internal.h"

// the .qry at `path` made anew and both its handles opened: 1, or 0 where either was not
int qry_file_opened(QryFile *file, const char *path)
{
    file->length = 0ull;
#if defined(_WIN32)
    file->bulk = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    file->through = (file->bulk != INVALID_HANDLE_VALUE)
                        ? CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL)
                        : INVALID_HANDLE_VALUE;
    return (file->bulk != INVALID_HANDLE_VALUE) && (file->through != INVALID_HANDLE_VALUE);
#else
    file->bulk = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    file->through = (file->bulk >= 0) ? open(path, O_WRONLY | O_DSYNC) : -1;
    return (file->bulk >= 0) && (file->through >= 0);
#endif
}

// `size` bytes at `bytes` written at the file's end, through to the disk itself where `through`: 1, or 0 where they
// did not all go
int qry_file_added(QryFile *file, const unsigned char *bytes, unsigned long long size, int through)
{
    unsigned long long done = 0ull;
    while (done < size)
    {
        const unsigned long long piece = ((size - done) < QRY_WRITE_CALL_BYTES) ? (size - done) : QRY_WRITE_CALL_BYTES;
        const unsigned long long offset = file->length + done;
#if defined(_WIN32)
        OVERLAPPED place;
        memset(&place, 0, sizeof(place));
        place.Offset = (DWORD)(offset & 0xffffffffull);
        place.OffsetHigh = (DWORD)(offset >> 32u);
        DWORD wrote = 0u;
        // a piece is at most QRY_WRITE_CALL_BYTES, which a DWORD counts
        if (!WriteFile(through ? file->through : file->bulk, bytes + done, (DWORD)piece, &wrote, &place) ||
            (wrote == 0u))
        {
            return 0;
        }
        done += wrote;
#else
        const ssize_t wrote = pwrite(through ? file->through : file->bulk, bytes + done, (size_t)piece, (off_t)offset);
        if ((wrote < 0) && (errno == EINTR))
        {
            continue;
        }
        if (wrote <= 0)
        {
            return 0;
        }
        done += (unsigned long long)wrote;
#endif
    }
    file->length += size;
    return 1;
}

// the bytes written through the bulk handle flushed through to the disk itself: 1, or 0 where they were not
int qry_file_durable(QryFile *file)
{
#if defined(_WIN32)
    return FlushFileBuffers(file->bulk) != 0;
#else
    return fsync(file->bulk) == 0;
#endif
}

// the file flushed through to the disk itself and both handles closed: 1, or 0 where either failed
int qry_file_closed(QryFile *file)
{
    const int durable = qry_file_durable(file);
#if defined(_WIN32)
    const int closed = (CloseHandle(file->bulk) != 0) & (CloseHandle(file->through) != 0);
#else
    const int closed = (close(file->bulk) == 0) & (close(file->through) == 0);
#endif
    return durable && closed;
}

// a write of `bytes` that took from `began` to now, counted
void qry_write_counted(QryWrites *writes, unsigned long long began, unsigned long long bytes, int whole)
{
    const double milliseconds = (double)(qry_nanoseconds() - began) / 1000000.0;
    writes->bytes += bytes;
    writes->writes += 1ull;
    writes->all += milliseconds;
    writes->failed |= !whole;
    if (milliseconds > writes->slowest)
    {
        writes->slowest = milliseconds;
        writes->slowest_bytes = bytes;
    }
}

// `size` bytes at `bytes` written through to the disk at `path`, anew: 1, or 0 where they were not written whole
int qry_bytes_put(const char *path, const unsigned char *bytes, unsigned long long size)
{
#if defined(_WIN32)
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    unsigned long long done = 0ull;
    int whole = 1;
    while (whole && (done < size))
    {
        const unsigned long long piece = ((size - done) < QRY_WRITE_CALL_BYTES) ? (size - done) : QRY_WRITE_CALL_BYTES;
        DWORD wrote = 0u;
        // a piece is at most QRY_WRITE_CALL_BYTES, which a DWORD counts
        whole = WriteFile(file, bytes + done, (DWORD)piece, &wrote, NULL) && (wrote != 0u);
        done += wrote;
    }
    return (CloseHandle(file) != 0) && whole;
#else
    const int file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file < 0)
    {
        return 0;
    }
    unsigned long long done = 0ull;
    int whole = 1;
    while (whole && (done < size))
    {
        const unsigned long long piece = ((size - done) < QRY_WRITE_CALL_BYTES) ? (size - done) : QRY_WRITE_CALL_BYTES;
        // a piece is at most QRY_WRITE_CALL_BYTES, which a size_t counts
        const ssize_t wrote = write(file, bytes + done, (size_t)piece);
        whole = (wrote > 0) || ((wrote < 0) && (errno == EINTR));
        done += (wrote > 0) ? (unsigned long long)wrote : 0ull;
    }
    whole = whole && (fsync(file) == 0);
    return (close(file) == 0) && whole;
#endif
}

// The bytes of the frame at `at` in the buffer, a blob marked QRY_MARK_FILED, written in the place of the current file
// its name gives, its path from the top of the tree: beside it as `<file>.part` and then put in its place whole. 1, or
// 0 where they were not written whole
int qry_file_put(QryBuffer *buffer, unsigned long long at)
{
    QryFrameHead opened;
    memcpy(&opened, buffer->data + at, sizeof(opened));
    const char *const name = (const char *)(buffer->data + at + sizeof(opened));
    const unsigned char *const bytes = buffer->data + at + opened.bytes_at;
    char placed[1400];
    char beside[1410];
    if (!qry_file_placed(name, (size_t)opened.name_length, placed, sizeof(placed)))
    {
        return 0;
    }
    snprintf(beside, sizeof(beside), "%s.part", placed);
    const int put = qry_bytes_put(beside, bytes, opened.size);
#if defined(_WIN32)
    const int moved = put && (MoveFileExA(beside, placed, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0);
#else
    const int moved = put && (rename(beside, placed) == 0);
#endif
    if (!moved)
    {
        remove(beside);
    }
    return moved;
}

// the order of two filed blobs: by their names, the hash first, and then by their sequences
const unsigned char *s_file_blobs_data;

int qry_file_blob_order(const void *left, const void *right)
{
    const QryFileBlob *const one = (const QryFileBlob *)left;
    const QryFileBlob *const two = (const QryFileBlob *)right;
    if (one->hash != two->hash)
    {
        return (one->hash < two->hash) ? -1 : 1;
    }
    const unsigned long long shorter = (one->name_length < two->name_length) ? one->name_length : two->name_length;
    // a name is at most QRY_NAME_LONGEST bytes, which a size_t counts
    const int named = memcmp(s_file_blobs_data + one->name_at, s_file_blobs_data + two->name_at, (size_t)shorter);
    if ((named != 0) || (one->name_length != two->name_length))
    {
        return (named != 0) ? named : ((one->name_length < two->name_length) ? -1 : 1);
    }
    return (one->sequence < two->sequence) ? -1 : ((one->sequence > two->sequence) ? 1 : 0);
}
