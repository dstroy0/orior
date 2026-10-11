// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.h: the query container, a .qry. Every log file a query makes goes into one of these, the one place a query's logs
// are kept together: a flat run of blobs, each the bytes of one log, framed by a begin tag and an end tag. The begin
// tag is a line `qry <name> <bytes> <sequence> <nanoseconds> <priority> begin`, the end tag a line `qry <name> end`,
// and the <bytes> bytes of the blob sit whole between them. A reader takes the count from the begin tag and reads that
// many bytes, never the bytes themselves, so a blob holds anything: text, binary, a zip or hex. A name holds no space
// and no line's end.
//
// <sequence> is the blob's place in the order the blobs were handed to the query's writer, from 1, <nanoseconds> the
// wall clock's time it was handed, since 1970 in UTC, and <priority> the number it was handed at, the lowest winning:
// 0 a panic, 1 critical, 2 ordinary (qry_buffer.h). A sequence no blob carries is one the writer dumped to keep room
// for the lower numbers, and the blob named QRY_DUMPED handed after it says how many blobs and bytes were dumped and
// which sequences.
//
// A finished run's .qry opens with its table, a flat table of fixed rows, so that a reader reads the table and jumps
// to the bytes it wants and reads no others: QryTableHead, then a QryRow a blob, then the blobs, framed as above, in
// the order handed. The rows stand in the order of their names' hashes and, within a hash, of their sequences: the
// rows of one name are found by a search for its hash and stand together, its first blob first and its latest last.
// Every place a row gives is a byte's place in the file, and every word is little endian. A row's hash is the 64-bit
// FNV-1a of the name's bytes, 1 where that is 0.
//
// The file is a tree of checks two deep: each row holds the CRC-64/XZ of its blob's bytes, and the head the CRC-64/XZ
// of every row's bytes; the head is the root of every byte past it. A reader holds a blob to its row's CRC as it
// reads it, and the rows to the head's as it reads them all; a blob or a row the file lost is told from one it holds.
//
// A .qry that does not open with QRY_TABLE_MAGIC is one the writer wrote as the run ran and did not dump, a run whose
// writer ended before the run did. Its panics and critical blobs were written ahead of the blobs waiting, and its order
// is not always the order handed: a reader walks its tags and puts the blobs back in order by their sequence
#ifndef QRY_H
#define QRY_H

#define QRY_TAG "qry"

#define QRY_BEGIN "begin"

#define QRY_END "end"

#define QRY_DUMPED "dumped"

// the first eight bytes of a .qry that opens with its table
#define QRY_TABLE_MAGIC "qry toc\n"

// the table's head: the magic, the count of rows, where the rows start and the bytes of each, where the blobs start and
// how many bytes they take, the CRC-64/XZ of the rows, and a word no reader reads yet
typedef struct
{
    char magic[8];
    unsigned long long rows;
    unsigned long long rows_at;
    unsigned long long row_bytes;
    unsigned long long blobs_at;
    unsigned long long blobs_bytes;
    unsigned long long rows_crc;
    unsigned long long spare;
} QryTableHead;

// a row: the name's hash, the blob's sequence and the time it was handed, the CRC-64/XZ of its bytes, where its name
// and its bytes sit and how many bytes it holds, the name's length and the priority it was handed at. Its begin tag
// opens just before its name, `qry ` before it
typedef struct
{
    unsigned long long hash;
    unsigned long long sequence;
    unsigned long long nanoseconds;
    unsigned long long crc;
    unsigned long long name_at;
    unsigned long long bytes_at;
    unsigned long long size;
    unsigned int name_length;
    unsigned int priority;
} QryRow;

#endif
