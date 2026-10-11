// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krep_records.h: a k-file of records (krep_records.oracle.tsv), every record numbers: the k-file head (krep_head.h),
// its four-byte kind, then its records, each a code, a count and that many numbers, every number written in as few
// bytes as it takes, seven bits a byte, the low bits first and the top bit set on every byte but its last. In memory
// each number is a 64-bit word, compared as one. A format's own records take the codes its oracle gives from
// KREP_RECORD_FORMAT up; below them are the stream's own:
//
//   KREP_RECORD_TEXT  a text, numbered from 1 in the order the file gives them and held once, which a record names by
//                     its number: its count of bytes, then the bytes
//   KREP_RECORD_WHO   the who of the records after it to the next who (qry_buffer.h, QryWho), its two words; one of
//                     no words says no who is known of the records after it
//
// Every text the file holds comes before its first record. A file is read whole, as the run holds it (qry_file_read),
// and handed back whole to the run's writer, which writes it in the file's place (qry_filed). Records compare and are
// found as numbers: no text is read to find one, and a text is only what no number gives, read by a person through the
// format's decoder.
#ifndef KREP_RECORDS_H
#define KREP_RECORDS_H

#include "qry/qry_buffer.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define KREP_RECORD_TEXT 0x1u
#define KREP_RECORD_WHO 0x2u
#define KREP_RECORD_FORMAT 0x10u

    // one record: its code, and its words, `count` of them, an allocation of its own
    typedef struct
    {
        unsigned int code;
        unsigned int count;
        unsigned long long *word;
    } KrepRecord;

    // A k-file of records held in memory: its kind, its records in order, and its texts, each by its number less one
    // with its length, and a table finding a text by its bytes, a power of two slots, each a text's number or 0. A file
    // starts as zeros
    typedef struct
    {
        char kind[4];
        KrepRecord *record;
        unsigned int records;
        unsigned int room;
        char **text;
        unsigned long long *text_length;
        unsigned int texts;
        unsigned int text_room;
        unsigned int *text_slot;
        unsigned int text_slots;
    } KrepFile;

    // `file` emptied and given the kind `kind`, four bytes, whatever it held let go
    void krep_open(KrepFile *file, const char *kind);

    // The k-file at `path` read into `file` as the run holds it, its latest blob or the file where the run holds none
    // (qry_file_read), whatever it held before let go: 1, and an empty file of `kind` where there is neither; 2 where
    // the file is there and does not open with the k-file head, its bytes left for the format to read as it reads its
    // text form; 0 and why in `error`, which holds `room`, where its kind is not `kind`, its version is not
    // KREP_VERSION or a record runs past its end
    int krep_read(KrepFile *file, const char *path, const char *kind, char *error, size_t room);

    // `file` handed to the run's writer as the file at `path` (qry_filed_by), written by `who`: 1, or 0 where it could
    // not be written
    int krep_write_by(const KrepFile *file, const char *path, QryWho who);

// krep_write_by, the who the code that calls it
#define krep_write(file_, path_) krep_write_by((file_), (path_), QRY_WHO_HERE)

    // everything `file` holds let go, and the file zeros again
    void krep_free(KrepFile *file);

    // a record of `code` with `count` words from `words` put at `at` in `file`'s records, every record from `at` on
    // one later: 1, or 0 where there was no memory for it
    int krep_record_put(KrepFile *file, unsigned int at, unsigned int code, const unsigned long long *words,
                        unsigned int count);

    // a record of `code` with `count` words from `words` kept after every record: 1, or 0 where there was no memory
    int krep_record_add(KrepFile *file, unsigned int code, const unsigned long long *words, unsigned int count);

    // the record at `at` let go, every record past it one sooner
    void krep_record_drop(KrepFile *file, unsigned int at);

    // the number of the text of `length` bytes at `bytes`, held once: the one `file` gives it, or the next where it
    // gives it none. 0 where there was no memory for it
    unsigned long long krep_text_number(KrepFile *file, const char *bytes, unsigned long long length);

    // the text of the number `number`, its length in `length`: NULL where `file` gives none
    const char *krep_text(const KrepFile *file, unsigned long long number, unsigned long long *length);

    // the who in force at the record `at`: 1 with it in `who`, or 0 where no who record comes before it or the nearest
    // one is of no words
    int krep_who_at(const KrepFile *file, unsigned int at, QryWho *who);

    // 1 where the two whos are one code's, the same source, line and program, whatever their times
    int krep_who_same(QryWho one, QryWho other);

#ifdef __cplusplus
}
#endif

#endif
