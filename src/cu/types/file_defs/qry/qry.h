// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.h: the query container, a .qry. Every log file a query makes goes into one of these, the one place a query's logs
// are kept together: a flat run of blobs, each the bytes of one log in a frame of machine words. A frame opens with
// QryFrameHead: the frame magic, the frame's bytes, the blob's sequence, the time it was handed and its bytes, the
// name's length, where in the frame the bytes start and the priority it was handed at. The name follows the head, the
// bytes start where the head says, and the frame's last word is the sequence again; a frame starts on a boundary of the
// alignment its writer was built with, and its bytes on one too. A reader takes the counts from the head and reads the
// bytes whole, never looking into them, so a blob holds anything: text, binary, a zip or hex. A name holds no space and
// no line's end.
//
// The sequence is the blob's place in the order the blobs were handed to the query's writer, from 1, the time the wall
// clock's when it was handed, nanoseconds since 1970 in UTC, and the priority the number it was handed at, the lowest
// winning: 0 a panic, 1 critical, 2 ordinary (qry_buffer.h). A sequence no blob carries is one the writer dumped to
// keep room for the lower numbers, and the blob named QRY_DUMPED handed after it says how many blobs and bytes were
// dumped and which sequences.
//
// A finished run's .qry opens with its table, a flat table of fixed rows, so that a reader reads the table and jumps
// to the bytes it wants and reads no others: QryTableHead, then a QryRow a blob, then the frames in the order handed,
// from the first boundary of the frames' alignment past the rows. The rows stand in the order of their names' hashes
// and, within a hash, of their sequences: the rows of one name are found by a search for its hash and stand together,
// its first blob first and its latest last. Every place a row gives is a byte's place in the file, and every word is
// little endian. A row's hash is the 64-bit FNV-1a of the name's bytes, 1 where that is 0.
//
// The file is a tree of checks two deep: each row holds the CRC-64/XZ of its blob's bytes, and the head the CRC-64/XZ
// of every row's bytes; the head is the root of every byte past it. A reader holds a blob to its row's CRC as it
// reads it, and the rows to the head's as it reads them all; a blob or a row the file lost is told from one it holds.
//
// A .qry that does not open with QRY_TABLE_MAGIC is one the writer wrote as the run ran and did not dump, a run whose
// writer ended before the run did. Its panics and critical blobs were written ahead of the blobs waiting, and its order
// is not always the order handed: a reader walks its frames and puts the blobs back in order by their sequence
#ifndef QRY_H
#define QRY_H

#define QRY_DUMPED "dumped"

// the first eight bytes of a frame, and of a .qry that opens with its table
#define QRY_FRAME_MAGIC "qryblob\n"
#define QRY_TABLE_MAGIC "qry toc\n"

// a frame's head, the frame's first bytes: the magic, the frame's bytes from its head through its last word, the
// blob's sequence, the time it was handed and its bytes, the name's length, where its bytes start counted from the
// frame's start, the priority it was handed at, and its marks, which no reader of the file needs
typedef struct
{
    char magic[8];
    unsigned long long framed;
    unsigned long long sequence;
    unsigned long long nanoseconds;
    unsigned long long size;
    unsigned int name_length;
    unsigned int bytes_at;
    unsigned int priority;
    unsigned int marks;
} QryFrameHead;

// a frame's marks: its writer also wrote its bytes into the named stream of the .qry that bears its name
// (qry_buffer.h, qry_streamed), or into the current file its name gives, its path from the top of the tree
// (qry_filed)
#define QRY_MARK_STREAMED 1u
#define QRY_MARK_FILED 2u

// the table's head: the magic, the count of rows, where the rows start and the bytes of each, where the frames start
// and how many bytes they take, the CRC-64/XZ of the rows, and the alignment the frames start on
typedef struct
{
    char magic[8];
    unsigned long long rows;
    unsigned long long rows_at;
    unsigned long long row_bytes;
    unsigned long long blobs_at;
    unsigned long long blobs_bytes;
    unsigned long long rows_crc;
    unsigned long long frame_align;
} QryTableHead;

// a row: the name's hash, the blob's sequence and the time it was handed, the CRC-64/XZ of its bytes, where its name
// and its bytes sit and how many bytes it holds, the name's length and the priority it was handed at. Its frame
// opens just before its name, a QryFrameHead before it
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
