// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr.h: a member's query record, its .kqr (kqr.oracle.tsv), a k-file of records (krep_records.h): every ask put to
// the member and what came back, every timed ask a sample of its own, the cycles, seeds and rounds that put them, and
// then the paths read off those asks, each a record of numbers. It is R, the record (Q9, Q10, P13), and every program
// that asks the member holds one record of it through this: an ask R holds is not put again.
//
// An ask is held by its identity, seven numbers: the hash of its code, the registers, threads, blocks and launches it
// is put with, its count of cases and the hash of its cases. What came back is one of five codes, KQR_ANSWERS with the
// word of every case, and the others with the refusal, the one text an ask keeps. An ask that timed out is asked again
// until an answer resolves it, and the answer takes its place. A timed ask is put every time and kept as a sample of
// its own.
//
// Every record the file held stands as it held it, a pair after every other record. What a run adds comes after the
// records it held and before the pairs, each run of them after a who record, the who of the code that put them
// (QryWho); and a cycle, a seed and a round are each written before the first new ask or sample that follows them and
// nowhere where none follows, so that a cycle the record answers whole adds no record to it. A record of the .kqr's
// text form, read where a file holds it, is written back as records, its lines in their order.
//
// A record a run keeps is a view of the run's table of asks, KQR_TABLE (kqr_tabled_by, krep_records.h): every record
// it holds a row of the table, every record kept after a row as it is kept, and the record written as its rows put it
// together.
#ifndef KQR_H
#define KQR_H

#include "../krep_records.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the .kqr's kind, the four bytes after the k-file head
#define KQR_KIND "KQR\0"

// the name of the run's table whose rows a record a run keeps is a view of
#define KQR_TABLE "asks"

// the codes of the .kqr's records, as its oracle gives them
#define KQR_MEMBER_RECORD 0x10u
#define KQR_CYCLE_RECORD 0x11u
#define KQR_SEED_RECORD 0x12u
#define KQR_ROUND_RECORD 0x13u
#define KQR_ASK_RECORD 0x14u
#define KQR_SAMPLE_RECORD 0x15u
#define KQR_PAIR_RECORD 0x16u
#define KQR_COMMENT_RECORD 0x17u

// what came back of an ask
#define KQR_ANSWERS 0u
#define KQR_ILLEGAL 1u
#define KQR_NOTHING 2u
#define KQR_TIMED_OUT 3u
#define KQR_CENSORED 4u
#define KQR_ANSWER_COUNT 5u

// the most letters of a refusal, of a member's name, of a mode, of a pair's form and of its verdict, and the most
// words of an answer
#define KQR_REFUSAL 128u
#define KQR_MEMBER 64u
#define KQR_MODE 64u
#define KQR_FORM 128u
#define KQR_VERDICT 512u
#define KQR_WORDS 4096u

// the most marks a cycle holds pending before its first new ask or sample
#define KQR_PENDING 8u

    // an ask's identity: the hash of its code, the registers, threads, blocks and launches it is put with, its count of
    // cases and the hash of its cases
    typedef struct
    {
        unsigned long long code;
        unsigned int registers;
        unsigned int threads;
        unsigned int blocks;
        unsigned int launches;
        unsigned int cases;
        unsigned long long cases_hash;
    } KqrIdentity;

    // one untimed ask and what came back: its identity; its answer, one of the five codes; and the word of every case
    // where it answered, or the refusal where it did not
    typedef struct
    {
        KqrIdentity identity;
        unsigned int answer;
        unsigned long long word[KQR_WORDS];
        unsigned int words;
        char refusal[KQR_REFUSAL];
    } KqrAsk;

    // one timed ask, a sample of its own: its identity as an ask's, what came back, and the nanoseconds its launches
    // took together where it answered, or the refusal where it did not; and where the sample gives them, the least and
    // the most one launch took, its block latency (P6), `launched` 1
    typedef struct
    {
        KqrIdentity identity;
        unsigned int answer;
        unsigned long long nanoseconds;
        int launched;
        unsigned long long least;
        unsigned long long most;
        char refusal[KQR_REFUSAL];
    } KqrSample;

    // a path read off the asks: a pair of the bridge, the hash of what its put reads where `read` is 1, and its
    // verdict as the bridge writes it beneath the pair
    typedef struct
    {
        char first[KQR_FORM];
        char second[KQR_FORM];
        int read;
        unsigned long long reads;
        char verdict[KQR_VERDICT];
    } KqrPath;

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

    // the record at `path` read into `record` as the run holds it (qry_file_read), its records or its text form,
    // whatever it held before let go: 1, an empty record of `member` where there is neither, or 0 and why in `error`,
    // which holds `room`, where a record or a line is none the oracle gives. The record is open either way, an empty
    // one of `member` where it stopped
    int kqr_read(KqrRecord *record, const char *path, const char *member, char *error, size_t room);

    // `record` made a view of the table KQR_TABLE of the run `buffer`, as the file at `path`, by `who`
    // (krep_tabled_by): every record it holds a row of the table, every record kept after a row as it is kept, and the
    // record written as its rows put it together. 1, or 0 where it is not open, no run is given or a row could not be
    // handed, and the record is written as it is held
    int kqr_tabled_by(KqrRecord *record, QryBuffer *buffer, const char *path, QryWho who);

    // `record` handed to the run's writer as the file at `path` (qry_filed_by), written by `who`, the record its rows
    // put together where it is a view of the run's table (kqr_tabled_by): 1, or 0 where no run's writer runs, the path
    // stands outside the tree, or it could not be written
    int kqr_write_by(const KqrRecord *record, const char *path, QryWho who);

    // everything `record` holds let go, and the record zeros again
    void kqr_free(KqrRecord *record);

    // 1 with the ask of `identity` the record holds in `held`, whatever came back of it; 0 where it holds none
    int kqr_find(const KqrRecord *record, const KqrIdentity *identity, KqrAsk *held);

    // `ask` kept by `who` after every record before the pairs, where the record holds no ask of its identity, the
    // marks pending written before it; and in place of the one it holds where that one timed out
    void kqr_keep_by(KqrRecord *record, const KqrAsk *ask, QryWho who);

    // `sample` kept by `who` after every record before the pairs, the marks pending written before it
    void kqr_sample_by(KqrRecord *record, const KqrSample *sample, QryWho who);

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
    void kqr_path_add_by(KqrRecord *record, const KqrPath *path, QryWho who);

    // `record` written as the .kqr's text form into `text`, a line a record, each who a line `who` its code named
    // through the who table where `named`, and its code alone where not
    void kqr_text_rendered(const KqrRecord *record, QryText *text, int named);

    // as kqr_text_rendered, the records from `from` on, `count` of them, or every one past it where `count` is 0: a
    // slice a reader decodes while it is still and grows outward from, its records' numbers given raw while it moves
    void kqr_text_sliced(const KqrRecord *record, QryText *text, int named, unsigned int from, unsigned int count);

// each, the who the code that calls it
#define kqr_tabled(record_, buffer_, path_) kqr_tabled_by((record_), (buffer_), (path_), QRY_WHO_HERE)
#define kqr_write(record_, path_) kqr_write_by((record_), (path_), QRY_WHO_HERE)
#define kqr_keep(record_, ask_) kqr_keep_by((record_), (ask_), QRY_WHO_HERE)
#define kqr_sample(record_, sample_) kqr_sample_by((record_), (sample_), QRY_WHO_HERE)
#define kqr_path_add(record_, path_) kqr_path_add_by((record_), (path_), QRY_WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
