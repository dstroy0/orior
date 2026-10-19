// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr_record.h: a member's query record held, read, kept and written (kqr.h)
//
// A record a run keeps is a view of the run's table of asks, KQR_TABLE (kqr_tabled_by, krep_records.h): every record
// it holds a row of the table, every record kept after a row as it is kept, and the record written as its rows put it
// together.
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

    // A member's record. A record starts as zeros, and kqr_read opens it. Its records, the member's first; the record
    // each identity's ask stands at, plus one, 0 for a slot that holds none; the record of each sample; the record the
    // pairs start at, the who before them where it is theirs; and the marks pending, each its code and its two words,
    // a cycle's first its mode's text, numbered as it is written
    typedef struct
    {
        KrepFile file;
        unsigned int *slot;
        unsigned int *sample;
        unsigned int samples;
        unsigned int sample_room;
        unsigned int first_pair;
        unsigned int asks;
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

    // the identity `text` writes, seven words apart by one space, read into `identity`: 1, or 0 where it is not seven
    // numbers, the two hashes in hex and the rest in decimal
    int kqr_identity_read(const char *text, KqrIdentity *identity);

    // `identity` written as the text form writes it into `text`, which holds `room`
    void kqr_identity_text(const KqrIdentity *identity, char *text, size_t room);

    // the record at `path` read into `record` as the run holds it (qry_member_file), its records or its text form,
    // whatever it held before let go: 1, an empty record of `member` where there is neither, or 0 and why in `error`,
    // which holds `room`, where a record or a line is none the oracle gives. The record is open either way, an empty
    // one of `member` where it stopped
    int kqr_read(KqrRecord *record, const char *path, const char *member, char *error, size_t room);

    // the record of `member` put together from the rows of the table KQR_TABLE of the finished run at `path`, the
    // record named `name` in the run or the first it holds where `name` is NULL (krep_viewed_at), into `record`,
    // whatever it held before let go: 1, or 0 and why in `error`, which holds `room`. The record is open either way,
    // an empty one of `member` where it stopped
    int kqr_viewed_at(KqrRecord *record, const char *path, const char *name, const char *member, char *error,
                      size_t room);

    // `record` made a view of the table KQR_TABLE of the run `buffer`, as the file at `path`, by `who`
    // (krep_tabled_by): every record it holds a row of the table, every record kept after a row as it is kept, and the
    // record written as its rows put it together. 1, or 0 where it is not open, no run is given or a row could not be
    // handed, and the record is written as it is held
    int kqr_tabled_by(KqrRecord *record, QryShared *buffer, const char *path, Who who);

    // `record` handed to the run's writer as the file at `path` (qry_member_filed), written by `who`, the record its
    // rows put together where it is a view of the run's table (kqr_tabled_by): 1, or 0 where no run's writer runs, the
    // path stands outside the tree, or it could not be written
    int kqr_write_by(const KqrRecord *record, const char *path, Who who);

    // everything `record` holds let go, and the record zeros again
    void kqr_free(KqrRecord *record);

    // 1 with the ask of `identity` the record holds in `held`, whatever came back of it; 0 where it holds none
    int kqr_find(const KqrRecord *record, const KqrIdentity *identity, KqrAsk *held);

    // `ask` kept by `who` after every record before the pairs, where the record holds no ask of its identity, the
    // marks pending written before it; and in place of the one it holds where that one timed out or was censored,
    // the part never having seen it
    void kqr_keep_by(KqrRecord *record, const KqrAsk *ask, Who who);

    // `sample` kept by `who` after every record before the pairs, the marks pending written before it
    void kqr_sample_by(KqrRecord *record, const KqrSample *sample, Who who);

    // a cycle, a seed or a round, KQR_CYCLE_RECORD, KQR_SEED_RECORD or KQR_ROUND_RECORD, pending until the next new ask
    // or sample, a round in place of a round still pending; a cycle's `value` its mode, written numbered after every
    // cycle the record holds, and a seed's and a round's their number in decimal
    void kqr_mark(KqrRecord *record, unsigned int code, const char *value);

    // 1 with the ask `at` of the record's asks, in the order they were kept, in `held`; 0 past the last
    int kqr_ask_read(const KqrRecord *record, unsigned int at, KqrAsk *held);

    // 1 with the sample `at` of the record's samples, in the order they were kept, in `held`; 0 past the last
    int kqr_sample_read(const KqrRecord *record, unsigned int at, KqrSample *held);

    // 1 with the path `at` of the record's pairs in `held`; 0 past the last
    int kqr_path_read(const KqrRecord *record, unsigned int at, KqrPath *held);

    // every pair the record holds let go
    void kqr_paths_clear(KqrRecord *record);

    // `path` kept by `who` after every pair before it
    void kqr_path_add_by(KqrRecord *record, const KqrPath *path, Who who);

    // `record` written as the .kqr's text form at `at` into `out`, which holds `cap`, a line a record, each who a line
    // `who` its code named through the who table where `named`, and its code alone where not: the offset after it, or
    // `cap` where it did not fit (verba_scribo.h)
    size_t kqr_text_rendered(const KqrRecord *record, char *out, size_t cap, size_t at, int named);

    // as kqr_text_rendered, the records from `from` on, `count` of them, or every one past it where `count` is 0: a
    // slice a reader decodes while it is still and grows outward from, its records' numbers given raw while it moves
    size_t kqr_text_sliced(const KqrRecord *record, char *out, size_t cap, size_t at, int named, unsigned int from,
                           unsigned int count);

// each, the who the code that calls it
#define kqr_tabled(record_, buffer_, path_) kqr_tabled_by((record_), (buffer_), (path_), WHO_HERE)
#define kqr_write(record_, path_) kqr_write_by((record_), (path_), WHO_HERE)
#define kqr_keep(record_, ask_) kqr_keep_by((record_), (ask_), WHO_HERE)
#define kqr_sample(record_, sample_) kqr_sample_by((record_), (sample_), WHO_HERE)
#define kqr_path_add(record_, path_) kqr_path_add_by((record_), (path_), WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
