// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr_check.c: the .kqr's reader and writer held to what the record's oracle says of it (kqr.oracle.tsv), on a record
// this check writes in the text form and as records, and on a part's own record
//
//   numbers    a record of the text form read is written as records, read back, and written as the text form again
//              byte for byte
//   found      an ask the record holds is found by its identity's numbers, with the word of every case or its refusal
//   timed out  an ask that timed out is found as timed out, and the answer that resolves it takes its place, between
//              a who record of the code that put it and the who that was in force there before
//   added      what a run puts is added after every record held and before the pairs, after a who record of the code
//              that put it, under one cycle numbered after the record's own, a seed and a round where they were marked,
//              and nothing where nothing new was put
//   samples    every sample is read back as it was kept, its launches' least and most where it has them
//   pairs      every pair is read back, and the pairs a run reads off its asks take the place of those before
//   refused    a line the oracle gives no kind of is refused with its line
//   rows       a record made a view of the run's table of asks is put together from its rows as it is held, every
//              text and every record in its order, before and after what a run puts, and the bytes written are linked
//              to the rows they were written from; with no run it is no table's
//   a part's   every ask of the record named on the command line, where one is, is found by its identity, and the
//              record, kept as records and a view of the run's table, written as records again and read back renders
//              its text form byte for byte
//
//     kqr_check <folder> [<a part's .kqr>]...
//
// It runs in a query run (qry_run.sh): every record it writes is handed to the run's writer.
#include "../../../../../../../src/cu/includes/file_defs/kqr/kqr_record.h"
#include "../../../../../../../src/cu/includes/file_defs/qry/writer/qry_member.h"

#include "../../../../../../../src/cu/includes/parsers/who.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/table_text.h"
#include "verba_scribo/verba_scribo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// the rooms a file read or a record rendered is kept in, so that four stand at once, and the bytes each holds. A file
// is at most what a record holds, but its text form is not bounded by it: a word of one byte in the record can be
// sixteen hex digits in the text, so a room grows to what it is asked to hold
static char *s_room[4];
static size_t s_room_bytes[4];

// room `room` given at least `bytes`: 1, or 0 where there was no memory for it, the room as it was
static int room_held(unsigned int room, size_t bytes)
{
    if (s_room_bytes[room] >= bytes)
    {
        return 1;
    }
    char *const larger = (char *)realloc(s_room[room], bytes);
    if (larger == NULL)
    {
        return 0;
    }
    s_room[room] = larger;
    s_room_bytes[room] = bytes;
    return 1;
}

// the whole file at `path` as the run holds it, or the disk where the run holds none (qry_member_file_blob), into room
// `room` and ended, its length in `size`: the room, or NULL where it did not read or there was no memory for it
static char *file_read(const char *path, size_t *size, unsigned int room)
{
    const unsigned char *blob = NULL;
    unsigned long long length = 0ull;
    if (!qry_member_file_blob(path, &blob, &length) || !room_held(room, (size_t)length + 1u))
    {
        return NULL;
    }
    memcpy(s_room[room], blob, (size_t)length);
    s_room[room][length] = '\0';
    *size = (size_t)length;
    return s_room[room];
}

// `record` written as its text form into room `room`, its length in `size`, each who its code, the room grown until
// the text fits: the room, or NULL where there was no memory for it
static char *rendered(const KqrRecord *record, size_t *size, unsigned int room)
{
    size_t bytes = QRY_WRITER_RING_BYTES / 2ull;
    while (room_held(room, bytes))
    {
        const size_t at = kqr_text_rendered(record, s_room[room], s_room_bytes[room], 0u, 0);
        if (verba_finis.ok(&(VerbaFinisCfg){.cap = s_room_bytes[room], .at = at}))
        {
            *size = verba_finis.finish(&(VerbaFinisCfg){.out = s_room[room], .cap = s_room_bytes[room], .at = at});
            return s_room[room];
        }
        bytes = s_room_bytes[room] * 2u;
    }
    return NULL;
}

static const char *const s_written =
    "kqr part\n"
    "cycle 1 queue\n"
    "ask 00000000000000aa 12 0 0 0 3 00000000000000bb answers 1 2 ffffffff\n"
    "ask 00000000000000ac 12 0 0 0 2 00000000000000bb illegal ILLEGAL_INSTRUCTION\n"
    "ask 00000000000000ad 12 0 0 0 2 00000000000000bb timed_out the carrier out of time\n"
    "cycle 2 pair\n"
    "seed 1\n"
    "round 1\n"
    "sample 00000000000000ae 12 256 4096 100 648 00000000000000bb answers 1800000\n"
    "pair word_add word_sub 0123456789abcdef closed 1006:0040 1,1,0->2\n";

static KqrRecord s_record;

// 1 where the two files hold one kind, the same texts in the same order and the same records, the bytes each is
// written as
static int files_same(const KrepFile *one, const KrepFile *other)
{
    int same =
        (memcmp(one->kind, other->kind, 4u) == 0) && (one->texts == other->texts) && (one->records == other->records);
    for (unsigned int number = 0u; same && (number < one->texts); number += 1u)
    {
        // a text is at most the bytes of a file read whole, which a size_t counts
        same = (one->text_length[number] == other->text_length[number]) &&
               (memcmp(one->text[number], other->text[number], (size_t)one->text_length[number]) == 0);
    }
    for (unsigned int at = 0u; same && (at < one->records); at += 1u)
    {
        const KrepRecord *const left = &one->record[at];
        const KrepRecord *const right = &other->record[at];
        same = (left->code == right->code) && (left->count == right->count) &&
               ((left->count == 0u) ||
                (memcmp(left->word, right->word, (size_t)left->count * sizeof(unsigned long long)) == 0));
    }
    return same;
}

// the run's table of links, gathered
static unsigned char s_links[QRY_WRITER_RING_BYTES / 2ull];

// 1 where the run's table of links holds a link from the latest record of the file at `path`, by its time, to the
// head row `key`, the file's bytes written from the rows of its table
static int written_from(const char *path, unsigned long long key)
{
    const char *const name = qry_member_file_name(path);
    const QryRecord *held = NULL;
    const unsigned long long size = qry_member_gathered(TABLE_LINKS, s_links, sizeof(s_links));
    const unsigned char *const bytes = s_links;
    if ((name == NULL) || !qry_member_latest(name, &held, NULL) || (size == QRY_MEMBER_UNREAD))
    {
        return 0;
    }
    const unsigned long long sequence = held->time;
    // a link's row: the who's two words, the key it is from, the key it is to and its code
    unsigned long long row[5];
    int found = 0;
    for (unsigned long long at = 0ull; !found && ((at + sizeof(row)) <= size); at += sizeof(row))
    {
        memcpy(row, bytes + at, sizeof(row));
        found = (row[2] == sequence) && (row[3] == key) && (row[4] == TABLE_LINK_WRITTEN_FROM);
    }
    return found;
}

// 1 where `record`'s rows in the run's table put together the record it holds
static int view_same(const KqrRecord *record)
{
    KrepFile view;
    memset(&view, 0, sizeof(view));
    const int same = krep_viewed(&record->file, &view) && files_same(&view, &record->file);
    krep_free(&view);
    return same;
}

// the identity `text` writes, read
static KqrIdentity identity_of(const char *text)
{
    KqrIdentity identity;
    memset(&identity, 0, sizeof(identity));
    kqr_identity_read(text, &identity);
    return identity;
}

// the record of a part at `part`, kept as records: every ask of its text form found by its identity, and the record
// written as records under `folder` and read back renders the same text form byte for byte
static void part_checked(const char *part, const char *folder, unsigned int number)
{
    char error[1280];
    size_t records_size = 0u;
    char *const records = file_read(part, &records_size, 0u);
    check_that(records != NULL, "a part's record reads");
    check_that(kqr_read(&s_record, part, "part", error, sizeof(error)) == 1, "a part's record opens");
    size_t size = 0u;
    char *const text = rendered(&s_record, &size, 1u);
    check_that(text != NULL, "a part's record renders its text form");
    char again[1024];
    snprintf(again, sizeof(again), "%s/kqr_check_part_%u.kqr", folder, number);
    check_that(kqr_tabled(&s_record, qry_member_run(), again) == 1,
               "a part's record is made a view of the run's table");
    check_that(view_same(&s_record), "and its rows put together the record as it was read");
    unsigned int asks = 0u;
    unsigned int found = 0u;
    static KqrAsk s_held;
    for (const char *line = text; (line != NULL) && (*line != '\0');)
    {
        const char *const end = strchr(line, '\n');
        if (strncmp(line, "ask ", 4u) == 0)
        {
            char identity[160];
            const char *at = line + 4;
            const char *cut = at;
            for (unsigned int word = 0u; (word < 7u) && (cut != NULL); word += 1u)
            {
                cut = strchr(cut, ' ');
                cut += ((cut != NULL) && (word < 6u)) ? 1 : 0;
            }
            if ((cut != NULL) && ((size_t)(cut - at) < sizeof(identity)))
            {
                memcpy(identity, at, (size_t)(cut - at));
                identity[cut - at] = '\0';
                const KqrIdentity numbers = identity_of(identity);
                asks += 1u;
                found +=
                    (kqr_find(&s_record, &numbers, &s_held) &&
                     (strncmp(cut + 1, kqr_answer_name(s_held.answer), strlen(kqr_answer_name(s_held.answer))) == 0))
                        ? 1u
                        : 0u;
            }
        }
        line = (end != NULL) ? (end + 1) : NULL;
    }
    check_that(kqr_write(&s_record, again) == 1, "a part's record is written as records");
    size_t numbered_size = 0u;
    file_read(again, &numbered_size, 2u);
    check_that(kqr_read(&s_record, again, "part", error, sizeof(error)) == 1, "and read back");
    size_t again_size = 0u;
    char *const again_text = rendered(&s_record, &again_size, 3u);
    const int same =
        (again_text != NULL) && (text != NULL) && (again_size == size) && (memcmp(again_text, text, size) == 0);
    check_that(same, "a part's record read back from its records is its text form byte for byte");
    printf("  %s: %u asks, %u found by their identity, %u samples, %u cycles, %zu bytes of text, %zu as records, "
           "written back %s\n",
           part, asks, found, s_record.samples, s_record.cycles, size, numbered_size, same ? "the same" : "DIFFERING");
    check_that((asks != 0u) && (found == asks), "every ask of a part's record is found by its identity");
    kqr_free(&s_record);
}

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("kqr_check <folder> [<a part's .kqr>]...\n");
        return 2;
    }
    char path[1024];
    char error[1280];
    snprintf(path, sizeof(path), "%s/kqr_check.kqr", words[1]);
    if (qry_member_filed(path, s_written, strlen(s_written)) == 0ull)
    {
        printf("  %s could not be written: no run's writer runs, or it stands outside the tree\n", path);
        return 2;
    }

    static KqrAsk s_held;
    check_that(kqr_read(&s_record, path, "part", error, sizeof(error)) == 1, "the record of the text form opens");
    check_that((s_record.cycles == 2u) && (s_record.asks == 3u) && (s_record.samples == 1u),
               "its cycles, its asks and its samples counted");
    size_t size = 0u;
    char *text = rendered(&s_record, &size, 1u);
    check_that((text != NULL) && (strcmp(text, s_written) == 0), "it is written as its text form byte for byte");
    check_that(kqr_write(&s_record, path) == 1, "it is written as records");
    char *bytes = file_read(path, &size, 0u);
    check_that((bytes != NULL) && (size > 16u) && (memcmp(bytes, "KREP\0\0\0\0KQR\0", 12u) == 0),
               "which open with the k-file head and the .kqr's kind");
    check_that(kqr_read(&s_record, path, "part", error, sizeof(error)) == 1, "the records are read back");
    text = rendered(&s_record, &size, 1u);
    check_that((text != NULL) && (strcmp(text, s_written) == 0), "as the text form byte for byte");
    check_that(kqr_tabled(&s_record, NULL, path) == 0, "with no run, a record is no table's");
    check_that(kqr_tabled(&s_record, qry_member_run(), path) == 1,
               "a record is made a view of the run's table of asks");
    check_that(view_same(&s_record), "and its rows put together every text and every record it holds");

    KqrIdentity identity = identity_of("00000000000000aa 12 0 0 0 3 00000000000000bb");
    check_that(kqr_find(&s_record, &identity, &s_held) == 1, "an ask held is found by its identity's numbers");
    check_that((s_held.answer == KQR_ANSWERS) && (s_held.words == 3u) && (s_held.word[2] == 0xffffffffull),
               "with the word of every case");
    identity = identity_of("00000000000000ac 12 0 0 0 2 00000000000000bb");
    check_that(kqr_find(&s_record, &identity, &s_held) && (s_held.answer == KQR_ILLEGAL) &&
                   (strcmp(s_held.refusal, "ILLEGAL_INSTRUCTION") == 0),
               "an illegal ask is found with its refusal");
    identity = identity_of("00000000000000ad 12 0 0 0 2 00000000000000bb");
    check_that(kqr_find(&s_record, &identity, &s_held) && (s_held.answer == KQR_TIMED_OUT),
               "an ask that timed out is found as timed out");
    identity = identity_of("00000000000000ff 12 0 0 0 2 00000000000000bb");
    check_that(kqr_find(&s_record, &identity, &s_held) == 0, "an ask not held is none");
    static KqrSample s_sample;
    check_that(kqr_sample_read(&s_record, 0u, &s_sample) && (s_sample.nanoseconds == 1800000ull) && !s_sample.launched,
               "a sample is read back with the time its launches took");
    static KqrPath s_path;
    check_that(kqr_path_read(&s_record, 0u, &s_path) && (strcmp(s_path.first, "word_add") == 0) &&
                   (strcmp(s_path.second, "word_sub") == 0) && s_path.read && (s_path.reads == 0x0123456789abcdefull) &&
                   (strcmp(s_path.verdict, "closed 1006:0040 1,1,0->2") == 0),
               "a pair is read back with its forms, the hash of its reads and its verdict");
    check_that(!kqr_path_read(&s_record, 1u, &s_path), "and nothing past the last pair");
    kqr_mark(&s_record, KQR_CYCLE_RECORD, "check");
    check_that(!s_record.changed, "a record nothing was put to is unchanged");
    text = rendered(&s_record, &size, 1u);
    check_that((text != NULL) && (strcmp(text, s_written) == 0),
               "and a cycle marked with nothing put after it adds nothing");

    kqr_mark(&s_record, KQR_SEED_RECORD, "7");
    kqr_mark(&s_record, KQR_ROUND_RECORD, "1");
    kqr_mark(&s_record, KQR_ROUND_RECORD, "2");
    static KqrAsk s_resolved;
    memset(&s_resolved, 0, sizeof(s_resolved));
    s_resolved.answer = KQR_ANSWERS;
    s_resolved.word[0] = 7ull;
    s_resolved.word[1] = 9ull;
    s_resolved.words = 2u;
    s_resolved.identity = identity_of("00000000000000ad 12 0 0 0 2 00000000000000bb");
    kqr_keep(&s_record, &s_resolved);
    s_resolved.identity = identity_of("00000000000000aa 12 0 0 0 3 00000000000000bb");
    kqr_keep(&s_record, &s_resolved);
    s_resolved.identity = identity_of("00000000000000f0 12 0 0 0 2 00000000000000bb");
    const Who put = WHO_HERE;
    kqr_keep_by(&s_record, &s_resolved, put);
    memset(&s_sample, 0, sizeof(s_sample));
    s_sample.identity = identity_of("00000000000000f1 12 256 4096 100 648 00000000000000bb");
    s_sample.answer = KQR_ANSWERS;
    s_sample.nanoseconds = 42ull;
    kqr_sample_by(&s_record, &s_sample, put);
    s_sample.identity = identity_of("00000000000000f2 12 256 4096 100 648 00000000000000bb");
    s_sample.nanoseconds = 300ull;
    s_sample.launched = 1;
    s_sample.least = 2ull;
    s_sample.most = 5ull;
    kqr_sample_by(&s_record, &s_sample, put);
    memset(&s_sample, 0, sizeof(s_sample));
    s_sample.identity = identity_of("00000000000000f3 12 256 4096 100 648 00000000000000bb");
    s_sample.answer = KQR_CENSORED;
    snprintf(s_sample.refusal, sizeof(s_sample.refusal), "the gate held it");
    kqr_sample_by(&s_record, &s_sample, put);
    identity = identity_of("00000000000000ad 12 0 0 0 2 00000000000000bb");
    check_that(kqr_find(&s_record, &identity, &s_held) && (s_held.answer == KQR_ANSWERS),
               "the answer that resolves a timed out ask takes its place");
    identity = identity_of("00000000000000aa 12 0 0 0 3 00000000000000bb");
    check_that(kqr_find(&s_record, &identity, &s_held) && (s_held.words == 3u),
               "an ask held that did not time out keeps its answer");
    identity = identity_of("00000000000000f0 12 0 0 0 2 00000000000000bb");
    check_that(kqr_find(&s_record, &identity, &s_held) && (s_held.word[1] == 9ull), "a new ask is held");
    check_that(kqr_sample_read(&s_record, 2u, &s_sample) && s_sample.launched && (s_sample.least == 2ull) &&
                   (s_sample.most == 5ull),
               "a sample is read back with its launches' least and most");
    check_that(kqr_sample_read(&s_record, 3u, &s_sample) && (strcmp(s_sample.refusal, "the gate held it") == 0),
               "and a sample refused with its refusal");
    kqr_paths_clear(&s_record);
    memset(&s_path, 0, sizeof(s_path));
    snprintf(s_path.first, sizeof(s_path.first), "word_add");
    snprintf(s_path.second, sizeof(s_path.second), "word_xor");
    snprintf(s_path.verdict, sizeof(s_path.verdict), "open 3");
    kqr_path_add_by(&s_record, &s_path, put);
    check_that(s_record.changed, "a record put to is changed");
    check_that(view_same(&s_record),
               "the rows of what a run put, an answer in place of one timed out and the pairs let "
               "go and put again, put together the record as it is held");
    const unsigned long long key = s_record.file.key;
    check_that(kqr_write(&s_record, path) == 1, "the record is written as records");
    check_that(written_from(path, key), "its bytes are linked to the rows they were written from");
    check_that(kqr_read(&s_record, path, "part", error, sizeof(error)) == 1, "and read back");
    text = rendered(&s_record, &size, 1u);
    char put_code[WHO_CODE_LETTERS + 1u];
    who_code(put, put_code, sizeof(put_code));
    // a who line is `who`, a space, the code's letters and the line's end
    const size_t who_line = 4u + WHO_CODE_LETTERS + 1u;
    const char *const resolved_at =
        (text != NULL)
            ? strstr(text, "ask 00000000000000ad 12 0 0 0 2 00000000000000bb answers 7 9\nwho -\ncycle 2 pair\n")
            : NULL;
    check_that((resolved_at != NULL) && ((size_t)(resolved_at - text) > who_line) &&
                   (strncmp(resolved_at - who_line, "who ", 4u) == 0),
               "the resolved ask stands where the timed out one stood, between its who and the who before, none");
    const char *const cycle_at = (text != NULL) ? strstr(text, "answers 1800000\n") : NULL;
    check_that(
        (cycle_at != NULL) && (strncmp(cycle_at + 16, "who ", 4u) == 0) &&
            (strncmp(cycle_at + 16 + who_line, "cycle 3 check\nseed 7\nround 2\nask 00000000000000f0", 49u) == 0),
        "what this run put comes after a who of the code that put it, one cycle numbered after the record's "
        "own, its seed and its last round");
    char put_line[64];
    snprintf(put_line, sizeof(put_line), "\nwho %s\ncycle 3 check\n", put_code);
    check_that((text != NULL) && (strstr(text, put_line) != NULL) &&
                   (strstr(text, "answers 7 9\nsample 00000000000000f1") != NULL),
               "that who's code the who the run's records were put by, one who over them all");
    check_that((text != NULL) &&
                   (strstr(text, "sample 00000000000000f2 12 256 4096 100 648 00000000000000bb answers 300 2 5\n"
                                 "sample 00000000000000f3 12 256 4096 100 648 00000000000000bb censored the gate held "
                                 "it\npair word_add word_xor open 3\n") != NULL),
               "the samples kept as put, and the pairs read off the asks after them, a pair with no hash of its reads");
    check_that((text != NULL) && (strstr(text, "word_sub") == NULL), "the pairs before them gone");
    static char s_named_bytes[QRY_WRITER_RING_BYTES / 2ull];
    const size_t named = kqr_text_rendered(&s_record, s_named_bytes, sizeof(s_named_bytes), 0u, 1);
    check_that(verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_named_bytes), .at = named}) &&
                   (verba_finis.finish(
                        &(VerbaFinisCfg){.out = s_named_bytes, .cap = sizeof(s_named_bytes), .at = named}) != 0u) &&
                   (strstr(s_named_bytes, "utils/test/src/cu/types/file_defs/kqr/kqr_check.c:") != NULL),
               "a who record named through the who table gives this file and the line of the put");
    printf("%s", (text != NULL) ? text : "");
    kqr_free(&s_record);

    const char *const unknown = "kqr part\nask 00000000000000aa 12 0 0 0 3 00000000000000bb answers\nwhat\n";
    check_that(qry_member_filed(path, unknown, strlen(unknown)) != 0ull, "a record with a line of no kind is written");
    check_that(kqr_read(&s_record, path, "part", error, sizeof(error)) == 0, "a line of no kind is refused");
    check_that(strstr(error, ":3: what is no line of a query record") != NULL, "with its line");
    check_that(s_record.open && (s_record.asks == 0u), "and the record is open and empty");
    kqr_free(&s_record);

    for (int given = 2; given < count; given += 1)
    {
        part_checked(words[given], words[1], (unsigned int)given);
    }

    printf("kqr_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
