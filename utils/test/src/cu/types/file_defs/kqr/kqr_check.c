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
//   a part's   every ask of the record named on the command line, where one is, is found by its identity, and the
//              record written as records and read back is its text form byte for byte
//
//     kqr_check <folder> [<a part's .kqr>]...
//
// It runs in a query run (qry_run.sh): every record it writes is handed to the run's writer.
#include "../../../../../../../src/cu/types/file_defs/kqr/kqr.h"
#include "../../../../../../../src/cu/types/file_defs/qry/qry_buffer.h"

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

// the whole file at `path` into memory of the caller's to free, its length in `size`: NULL where it did not read
static char *file_read(const char *path, size_t *size)
{
    unsigned char *bytes = NULL;
    unsigned long long length = 0ull;
    *size = 0u;
    if (!qry_file_read(NULL, path, &bytes, &length))
    {
        return NULL;
    }
    // a file read whole is at most what a size_t counts
    *size = (size_t)length;
    char *const text = (char *)realloc(bytes, (size_t)length + 1u);
    if (text == NULL)
    {
        free(bytes);
        return NULL;
    }
    text[length] = '\0';
    return text;
}

// `record` written as its text form into memory of the caller's to free, its length in `size`, each who its code
static char *rendered(const KqrRecord *record, size_t *size)
{
    QryText text = {0};
    kqr_text_rendered(record, &text, 0);
    qry_text_added(&text, "", 1u);
    *size = (size_t)(text.size - 1ull);
    return text.bytes;
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

// the identity `text` writes, read
static KqrIdentity identity_of(const char *text)
{
    KqrIdentity identity;
    memset(&identity, 0, sizeof(identity));
    kqr_identity_read(text, &identity);
    return identity;
}

// the record of a part at `part`, every ask found by its identity, and written as records under `folder` and read
// back, its text form byte for byte
static void part_checked(const char *part, const char *folder, unsigned int number)
{
    char error[1280];
    size_t size = 0u;
    char *const text = file_read(part, &size);
    check_that(text != NULL, "a part's record reads");
    check_that(kqr_read(&s_record, part, "part", error, sizeof(error)) == 1, "a part's record opens");
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
    char again[1024];
    snprintf(again, sizeof(again), "%s/kqr_check_part_%u.kqr", folder, number);
    check_that(kqr_write(&s_record, again) == 1, "a part's record is written as records");
    size_t numbered_size = 0u;
    char *const numbered = file_read(again, &numbered_size);
    check_that(kqr_read(&s_record, again, "part", error, sizeof(error)) == 1, "and read back");
    size_t again_size = 0u;
    char *const again_text = rendered(&s_record, &again_size);
    const int same =
        (again_text != NULL) && (text != NULL) && (again_size == size) && (memcmp(again_text, text, size) == 0);
    check_that(same, "a part's record read back from its records is its text form byte for byte");
    printf("  %s: %u asks, %u found by their identity, %u samples, %u cycles, %zu bytes of text, %zu as records, "
           "written back %s\n",
           part, asks, found, s_record.samples, s_record.cycles, size, numbered_size, same ? "the same" : "DIFFERING");
    check_that((asks != 0u) && (found == asks), "every ask of a part's record is found by its identity");
    free(text);
    free(numbered);
    free(again_text);
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
    if (qry_filed(qry_run(), path, s_written, strlen(s_written)) == 0ull)
    {
        printf("  %s could not be written: no run's writer runs, or it stands outside the tree\n", path);
        return 2;
    }

    static KqrAsk s_held;
    check_that(kqr_read(&s_record, path, "part", error, sizeof(error)) == 1, "the record of the text form opens");
    check_that((s_record.cycles == 2u) && (s_record.asks == 3u) && (s_record.samples == 1u),
               "its cycles, its asks and its samples counted");
    size_t size = 0u;
    char *text = rendered(&s_record, &size);
    check_that((text != NULL) && (strcmp(text, s_written) == 0), "it is written as its text form byte for byte");
    free(text);
    check_that(kqr_write(&s_record, path) == 1, "it is written as records");
    char *bytes = file_read(path, &size);
    check_that((bytes != NULL) && (size > 16u) && (memcmp(bytes, "KREP\0\0\0\0KQR\0", 12u) == 0),
               "which open with the k-file head and the .kqr's kind");
    free(bytes);
    check_that(kqr_read(&s_record, path, "part", error, sizeof(error)) == 1, "the records are read back");
    text = rendered(&s_record, &size);
    check_that((text != NULL) && (strcmp(text, s_written) == 0), "as the text form byte for byte");
    free(text);

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
    text = rendered(&s_record, &size);
    check_that((text != NULL) && (strcmp(text, s_written) == 0),
               "and a cycle marked with nothing put after it adds nothing");
    free(text);

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
    const QryWho put = QRY_WHO_HERE;
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
    check_that(kqr_write(&s_record, path) == 1, "the record is written as records");
    check_that(kqr_read(&s_record, path, "part", error, sizeof(error)) == 1, "and read back");
    text = rendered(&s_record, &size);
    char put_code[QRY_WHO_CODE_LETTERS + 1u];
    qry_who_code(put, put_code, sizeof(put_code));
    // a who line is `who`, a space, the code's letters and the line's end
    const size_t who_line = 4u + QRY_WHO_CODE_LETTERS + 1u;
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
    QryText named = {0};
    kqr_text_rendered(&s_record, &named, 1);
    qry_text_added(&named, "", 1u);
    check_that((named.bytes != NULL) &&
                   (strstr(named.bytes, "utils/test/src/cu/types/file_defs/kqr/kqr_check.c:") != NULL),
               "a who record named through the who table gives this file and the line of the put");
    printf("%s", (text != NULL) ? text : "");
    qry_text_freed(&named);
    free(text);
    kqr_free(&s_record);

    const char *const unknown = "kqr part\nask 00000000000000aa 12 0 0 0 3 00000000000000bb answers\nwhat\n";
    check_that(qry_filed(qry_run(), path, unknown, strlen(unknown)) != 0ull,
               "a record with a line of no kind is written");
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
