// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry.h: the .qry container, its records end to end (qry.oracle.tsv): each the record's head, the name, the blob,
// zeros to a whole word, and the CRC-64/XZ of every byte of the record before it
#ifndef QRY_H
#define QRY_H

// the longest name a record takes
#define QRY_NAME_LONGEST 255u

// a record's priority, the lowest number first: a panic, a critical record, and an ordinary one
#define QRY_PANIC 0u
#define QRY_CRITICAL 1u
#define QRY_ORDINARY 2u

// a record's head: the time on the one clock as it was appended, the blob's bytes, the name's bytes and the priority
typedef struct
{
    unsigned long long time;
    unsigned long long blob_length;
    unsigned int name_length;
    unsigned int priority;
} QryRecord;

#ifdef __cplusplus
static_assert(sizeof(QryRecord) == 24u, "QryRecord must be the 24 bytes a record opens with");
#else
_Static_assert(sizeof(QryRecord) == 24u, "QryRecord must be the 24 bytes a record opens with");
#endif

// a record's name and its blob, where the record stands
#define QRY_RECORD_NAME(record_) ((const char *)((record_) + 1))
#define QRY_RECORD_BLOB(record_) ((const unsigned char *)((record_) + 1) + (record_)->name_length)

// the bytes a record of a name of `name_length_` bytes and a blob of `blob_length_` takes: its head, the name and the
// blob to a whole word, and the CRC
#define QRY_RECORD_BYTES(name_length_, blob_length_)                                                                   \
    ((((unsigned long long)sizeof(QryRecord) + (unsigned long long)(name_length_) +                                    \
       (unsigned long long)(blob_length_) + 7ull) &                                                                    \
      ~7ull) +                                                                                                         \
     8ull)

#endif
