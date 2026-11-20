// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr_record.h: a member's query record stored, read, retained and written (kqr.h)
//
// A record that a run stores is a view of the table of queries of the run, KQR_TABLE (kqr_tabled_by,
// krep_records.h). Each record of it is a row of the table. Each record that the code keeps later gets a row as the
// code keeps it. The code writes the record as its rows put it together.
#ifndef KQR_RECORD_H
#define KQR_RECORD_H

#include "../krep/krep_records.h"
#include "kqr.h"

#include "../../parsers/who.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // The record of a member. A record starts as zeros, and kqr_read opens it. It holds:
    //   - its records, the record of the member first.
    //   - the record of the query of each identity, plus one, 0 for an entry with none.
    //   - the record of each sample.
    //   - the record where the pairs start, and the who before them where it belongs to them.
    //   - the pending marks, each its code and its two words. The first of a cycle is the text of its mode, numbered
    //     as the code writes it.
    typedef struct
    {
        KrepFile file;
        unsigned int *slot;
        unsigned int *sample;
        unsigned int samples;
        unsigned int sample_room;
        unsigned int first_pair;
        unsigned int query_count;
        unsigned int cycles;
        unsigned int pending_code[KQR_PENDING];
        unsigned long long pending_word[KQR_PENDING][2];
        unsigned int pendings;
        int changed;
        int open;
    } KqrRecord;

    // the name of the answer `answer`, as the .kqr's text form writes it; "-" where it is none of the five
    const char *kqr_answer_name(unsigned int answer);

    // the answer the text form writes as `name`, KQR_ANSWER_COUNT where it is none of the five
    unsigned int kqr_answer_of(const char *name);

    // the identity that `text` writes, seven words apart by one space, read into `identity`. The result is 1, or 0 where
    // it is not seven numbers, the two hashes in hex and the rest in decimal
    int kqr_identity_read(const char *text, KqrIdentity *identity);

    // `identity` written as the text form writes it into `text`, which has room for `room`
    void kqr_identity_text(const KqrIdentity *identity, char *text, size_t room);

    // the record at `path`, read into `record` as the run stores it (qry_member_file), its records or its text form.
    // The call lets go of the old contents of `record`.
    //
    // The result is 1, with an empty record of `member` where there is neither. It is 0 with the reason in `error`,
    // which has room for `room`, where a record or a line is not one that the oracle gives.
    //
    // The record is open in both cases. Where the read stopped, it is an empty record of `member`
    int kqr_read(KqrRecord *record, const char *path, const char *member, char *error, size_t room);

    // the record of `member`, put together into `record` from the rows of the table KQR_TABLE of the finished run at
    // `path` (krep_viewed_at). It is the record named `name` in the run, or the first one where `name` is NULL.
    //
    // The call lets go of the old contents of `record`. The result is 1, or 0 with the reason in `error`, which has
    // room for `room`. The record is open in both cases. Where the read stopped, it is an empty record of `member`
    int kqr_viewed_at(KqrRecord *record, const char *path, const char *name, const char *member, char *error,
                      size_t room);

    // `record`, made a view of the table KQR_TABLE of the run `buffer`, as the file at `path`, by `who`
    // (krep_tabled_by). Each record of it is a row of the table. Each record that the code keeps later gets a row as the
    // code keeps it. The code writes the record as its rows put it together.
    //
    // The result is 1, or 0 where the record is not open, the caller gives no run, or the call cannot pass a row. The
    // code then writes the record as it stores it
    int kqr_tabled_by(KqrRecord *record, QryShared *buffer, const char *path, Who who);

    // `record`, passed to the writer of the run as the file at `path` (qry_member_filed), written by `who`. Where it is
    // a view of the table of the run (kqr_tabled_by), the call passes the record that its rows put together.
    //
    // The result is 1, or 0 where no writer of a run runs, or where the path is outside the tree. It is also 0 where the
    // call cannot write it
    int kqr_write_by(const KqrRecord *record, const char *path, Who who);

    // everything `record` contains let go, and the record zeros again
    void kqr_free(KqrRecord *record);

    // 1 with the query of `identity` the record contains in `out`, whatever came back of it; 0 where it contains none
    int kqr_find(const KqrRecord *record, const KqrIdentity *identity, KqrQuery *out);

    // `query`, stored by `who` after each record before the pairs, where the record holds no query of its identity. The
    // call writes the pending marks before it. Where the record holds one that timed out or that a censor removed, this
    // query takes its place, because the part never saw it
    void kqr_store_by(KqrRecord *record, const KqrQuery *query, Who who);

    // `sample` stored by `who` after every record before the pairs, the marks pending written before it
    void kqr_sample_by(KqrRecord *record, const KqrSample *sample, Who who);

    // a cycle, a seed or a round, KQR_CYCLE_RECORD, KQR_SEED_RECORD or KQR_ROUND_RECORD. It is pending until the next
    // new query or sample. A round takes the place of a round that is still pending.
    //
    // The `value` of a cycle is its mode, numbered after each cycle of the record. The `value` of a seed or a round is
    // its number in decimal
    void kqr_mark(KqrRecord *record, unsigned int code, const char *value);

    // 1 with the query `at` of the queries of the record, in their order of storage, in `out`. 0 past the last
    int kqr_query_read(const KqrRecord *record, unsigned int at, KqrQuery *out);

    // 1 with the sample `at` of the samples of the record, in their order of storage, in `out`. 0 past the last
    int kqr_sample_read(const KqrRecord *record, unsigned int at, KqrSample *out);

    // 1 with the path `at` of the record's pairs in `out`; 0 past the last
    int kqr_path_read(const KqrRecord *record, unsigned int at, KqrPath *out);

    // every pair the record contains let go
    void kqr_paths_clear(KqrRecord *record);

    // `path` stored by `who` after every pair before it
    void kqr_path_add_by(KqrRecord *record, const KqrPath *path, Who who);

    // `record`, written as the text form of the .kqr at `at` into `out`, which has room for `cap`, a line a record. Each
    // who is a line `who`. Where `named`, the who table names its code, and else the line holds its code alone. The
    // result is the offset after it, or `cap` where it did not fit (verba_scribo.h)
    size_t kqr_text_rendered(const KqrRecord *record, char *out, size_t cap, size_t at, int named);

    // as kqr_text_rendered, the records from `from` on, `count` of them, or each one past it where `count` is 0. A reader
    // decodes this slice while it is still, and grows out from it. While it moves, the numbers of its records stay raw
    size_t kqr_text_sliced(const KqrRecord *record, char *out, size_t cap, size_t at, int named, unsigned int from,
                           unsigned int count);

// each, the who the code that calls it
#define kqr_tabled(record_, buffer_, path_) kqr_tabled_by((record_), (buffer_), (path_), WHO_HERE)
#define kqr_write(record_, path_) kqr_write_by((record_), (path_), WHO_HERE)
#define kqr_store(record_, query_) kqr_store_by((record_), (query_), WHO_HERE)
#define kqr_sample(record_, sample_) kqr_sample_by((record_), (sample_), WHO_HERE)
#define kqr_path_add(record_, path_) kqr_path_add_by((record_), (path_), WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
