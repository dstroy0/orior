// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krep_records.h: a k-file of records (krep_records.oracle.tsv), every record numbers: the k-file head (krep_head.h),
// its four-byte type, then its records, each a code, a count and that many numbers, every number written in as few
// bytes as it takes, seven bits a byte, the low bits first and the top bit set on every byte but its last. In memory
// each number is a 64-bit word, compared as one. A format's own records take the codes its oracle gives from
// KREP_RECORD_FORMAT up; below them are the stream's own:
//
//   - KREP_RECORD_TEXT  a text, which a record names by its number. The texts have numbers from 1 in the order of the
//                       file, and the file stores each once. A text holds its count of bytes, then the bytes.
//   - KREP_RECORD_WHO   the who of the records after it, up to the next who (who.h, Who), its two words. A who of no
//                       words states that the records after it have no who.
//
// Each text of the file comes before its first record. The code reads a file whole, as the run stores it
// (qry_member_file). It passes it back whole to the writer of the run, which writes it in the place of the file
// (qry_member_filed).
//
// Records compare as numbers, and the code finds them as numbers. No search reads a text. A text holds only what no
// number gives, and a person reads it through the decoder of the format.
//
// A file that a run writes is a view of a table of the run (krep_tabled_by). Its head is a row of the table, with a
// blob of the name of the table. Then each text and each record of the file is a row. Each later change to it gets a
// row of its own.
//
// A row is 64-bit words:
//   - the key of the file: the sequence of the row of its head, and 0 in the row of the head itself.
//   - the who of the record, its two words, of no who in the row of a text.
//   - the change, one of KREP_ROW_HEAD to KREP_ROW_DROP.
//   - the own numbers of the change. The bytes of a text or a name come after them, filled out to a whole word with
//     zeros.
//
// The changes:
//   - KREP_ROW_HEAD  the type of the file as a number, its four bytes low first, and the version. Then the length and
//                    bytes of the name of the file in the run (qry_member_file_name).
//   - KREP_ROW_TEXT  the number of the text, its length and its bytes.
//   - KREP_ROW_PUT   the place of the new record, its code, its count and its words. Each record from that place on
//                    moves one later.
//   - KREP_ROW_SET   the place of a record that gets other words, their count and the words.
//   - KREP_ROW_DROP  the place of a record that goes. Each record past it moves one sooner.
//
// The code writes the file that its rows put together, in the order that the run stores them.
#ifndef KREP_RECORDS_H
#define KREP_RECORDS_H

#include "../qry/writer/qry_member.h"

#include "../../../src/cu/transpiler/lstar/protocol/table_text.h"
#include "../../parsers/who.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define KREP_RECORD_TEXT 0x1u
#define KREP_RECORD_WHO 0x2u
#define KREP_RECORD_FORMAT 0x10u

// the changes a row of a file's table is, its fourth word
#define KREP_ROW_HEAD 0x0u
#define KREP_ROW_TEXT 0x1u
#define KREP_ROW_PUT 0x2u
#define KREP_ROW_SET 0x3u
#define KREP_ROW_DROP 0x4u

    // one record: its code, and its words, `count` of them, an allocation of its own
    typedef struct
    {
        unsigned int code;
        unsigned int count;
        unsigned long long *word;
    } KrepRecord;

    // A k-file of records in memory. It holds its type and its records in order. It holds its texts, each by its number
    // less one, with its length. It holds a table that finds a text by its bytes: a power of two buckets, each the number
    // of a text or 0.
    //
    // Where it is a view of a table of a run, it holds the run and the name of the table. It also holds the key of the
    // file. These are 0 where it is no view. A file starts as zeros
    typedef struct
    {
        char type[4];
        KrepRecord *record;
        unsigned int records;
        unsigned int room;
        char **text;
        unsigned long long *text_length;
        unsigned int texts;
        unsigned int text_room;
        unsigned int *text_slot;
        unsigned int text_slots;
        QryShared *run;
        const char *table;
        unsigned long long key;
    } KrepFile;

    // `file` emptied and given the type `type`, four bytes, whatever it contained let go
    void krep_open(KrepFile *file, const char *type);

    // The k-file at `path`, read into `file` as the run stores it (qry_member_file). That is its latest blob, or the file
    // where the run stores none. The call lets go of the old contents of `file`.
    //
    // The result is 1, with an empty file of `type` where there is neither. It is 2 where the file is there and does
    // not open with the k-file head. The bytes then stay for the format, which reads them as it reads its text form.
    //
    // It is 0, with the reason in `error`, which has room for `room`, in three cases. They are a type that is not
    // `type`, a version that is not KREP_VERSION, and a record that runs past its end
    int krep_read(KrepFile *file, const char *path, const char *type, char *error, size_t room);

    // `file` passed to the writer of the run as the file at `path` (qry_member_filed), written by `who`. Where it is a
    // view of a table of the run, the call passes the file that its rows put together (krep_viewed).
    //
    // The blob of its bytes then gets a link to the key of the file in the table of links of the run
    // (TABLE_LINK_WRITTEN_FROM). The result is 1, or 0 where the call cannot write it
    int krep_write_by(const KrepFile *file, const char *path, Who who);

// krep_write_by, the who the code that calls it
#define krep_write(file_, path_) krep_write_by((file_), (path_), WHO_HERE)

    // `file`, made a view of the table `table` of the run `buffer`, as the file at `path`, by `who`. The call passes its
    // head as a critical row of the table, and its sequence is the key of the file. Then comes a row of each text and
    // each record of the file in their order.
    //
    // From then on, each text that the file takes gets a row. Each record that it puts, gives other words, or lets go
    // gets a row too.
    //
    // The result is 1, or 0 where the caller gives no run or the path is outside the top of the tree. It is also 0 where
    // the call cannot pass a row, and the file is then no view.
    //
    // Where a later row does not pass, the file is no longer a view, and the run writes it as it stores it. `table` must
    // live as long as the file
    int krep_tabled_by(KrepFile *file, QryShared *buffer, const char *table, const char *path, Who who);

// krep_tabled_by, the who the code that calls it
#define krep_tabled(file_, buffer_, table_, path_) krep_tabled_by((file_), (buffer_), (table_), (path_), WHO_HERE)

    // the file that the rows of the key of `file` put together, in the order that the run stores them, into `view`.
    // `view` starts as an empty file of its type. The result is 1, or 0 where `file` is no view, or where the call cannot
    // read its rows. It is also 0 where a row does not fit the file that it puts together
    int krep_viewed(const KrepFile *file, KrepFile *view);

    // the file named `name` in the run, its path from the top of the tree (qry_member_file_name). The call puts it
    // together from the rows of the table `table` of the finished .qry at `path`, in the order of their sequences.
    //
    // It goes into `view`, which starts as an empty file of its type. Where `name` is NULL, it is the first file of the
    // table.
    //
    // The result is 1, or 0 with the reason in `error`, which has room for `room`. It is 0 where the call cannot read
    // the .qry, or the table holds no head of the file. It is also 0 where a row does not fit the file that it puts
    // together
    int krep_viewed_at(const char *path, const char *table, const char *name, KrepFile *view, char *error, size_t room);

    // everything `file` contains let go, and the file zeros again
    void krep_free(KrepFile *file);

    // a record of `code` with `count` words from `words` put at `at` in `file`'s records, every record from `at` on
    // one later: 1, or 0 where there was no memory for it
    int krep_record_put(KrepFile *file, unsigned int at, unsigned int code, const unsigned long long *words,
                        unsigned int count);

    // a record of `code` with `count` words from `words` added after every record: 1, or 0 where there was no memory
    int krep_record_add(KrepFile *file, unsigned int code, const unsigned long long *words, unsigned int count);

    // the record at `at` let go, every record past it one sooner
    void krep_record_drop(KrepFile *file, unsigned int at);

    // the record at `at` given `count` words from `words` in place of its own: 1, or 0 where `at` is past the last or
    // there was no memory for them
    int krep_record_set(KrepFile *file, unsigned int at, const unsigned long long *words, unsigned int count);

    // the number of the text of `length` bytes at `bytes`, stored once: the one `file` gives it, or the next where it
    // gives it none. 0 where there was no memory for it
    unsigned long long krep_text_number(KrepFile *file, const char *bytes, unsigned long long length);

    // the text of the number `number`, its length in `length`: NULL where `file` gives none
    const char *krep_text(const KrepFile *file, unsigned long long number, unsigned long long *length);

    // the who in force at the record `at`. The result is 1 with it in `who`, or 0 where no who record comes before it.
    // It is also 0 where the nearest one is of no words
    int krep_who_at(const KrepFile *file, unsigned int at, Who *who);

    // 1 where the two whos are one code's, the same source, line and program, whatever their times
    int krep_who_same(Who one, Who other);

#ifdef __cplusplus
}
#endif

#endif
