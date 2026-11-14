// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: the record read: each line's type, a line read as a query, the text form read whole, the queries read once
// each, the record read from its file, and the record viewed at a run's queries table
#include "../../../parsers/line_read.h"
#include "kqr_internal.h"

// 1 where `line` is of the type `type`: its first word, and a space or its end after it
static int kqr_line_type(const char *line, const char *type)
{
    const size_t length = strlen(type);
    return (strncmp(line, type, length) == 0) && ((line[length] == ' ') || (line[length] == '\0'));
}

// the `length` bytes at `text`, as many of them as fit, into `out`, which has room for `room`, and ended
static void kqr_text_cut(char *out, size_t room, const char *text, size_t length)
{
    const size_t at = verba_textus.put_n(&(VerbaTextusCfg){
        .out = out, .cap = room, .at = 0u, .text = text, .text_len = (length < room) ? length : (room - 1u)});
    verba_finis.finish(&(VerbaFinisCfg){.out = out, .cap = room, .at = at});
}

// the `length` bytes at `text` written after the `at` bytes of `error`, which has room for `room`, as many of them as
// fit: the offset after them
static size_t kqr_error_put(char *error, size_t room, size_t at, const char *text, size_t length)
{
    const size_t left = room - 1u - at;
    return verba_textus.put_n(&(VerbaTextusCfg){
        .out = error, .cap = room, .at = at, .text = text, .text_len = (length < left) ? length : left});
}

// the query or sample line `line`, past its type, read into its identity, its answer and where what follows starts:
// 1, or 0 where it has no identity or no answer the oracle gives
static int kqr_line_identity(const char *line, KqrIdentity *identity, unsigned int *answer, const char **rest)
{
    const char *at = strchr(line, ' ');
    if (at == NULL)
    {
        return 0;
    }
    at += 1;
    const char *end = at;
    for (unsigned int word = 0u; (word < 7u) && (end != NULL); word += 1u)
    {
        end = strchr(end, ' ');
        end += ((end != NULL) && (word < 6u)) ? 1 : 0;
    }
    char text[256];
    // the identity is before its end, which an int counts
    if ((end == NULL) || ((size_t)(end - at) >= sizeof(text)))
    {
        return 0;
    }
    kqr_text_cut(text, sizeof(text), at, (size_t)(end - at));
    if (!kqr_identity_read(text, identity))
    {
        return 0;
    }
    const char *const name_start = end + 1;
    const size_t length = strcspn(name_start, " ");
    char name[16];
    if (length >= sizeof(name))
    {
        return 0;
    }
    memcpy(name, name_start, length);
    name[length] = '\0';
    *answer = kqr_answer_of(name);
    *rest = name_start + length + ((name_start[length] == ' ') ? 1u : 0u);
    return *answer < KQR_ANSWER_COUNT;
}

// The .kqr's text form, `size` bytes at `bytes`, read into `record` as records: the first line its member, every query
// stored once, an answer in place of one that timed out, and every pair after every other record. 1, or 0 and why in
// `error` where a line is none the oracle gives
static int kqr_text_read(KqrRecord *record, const unsigned char *bytes, unsigned long long size, const char *path,
                         char *error, size_t room)
{
    static char s_line[KQR_LINE];
    static unsigned long long s_words[KQR_QUERY_WORDS + KQR_WORDS + 1u];
    static KqrQuery s_query;
    static KqrSample s_sample;
    // the pairs, stored apart and put after every other record
    KrepFile pairs = {{0}, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, NULL, NULL, 0ull};
    unsigned long long at = 0ull;
    unsigned int number = 0u;
    const char *stopped = NULL;
    while ((stopped == NULL) && line_read(bytes, size, &at, s_line, sizeof(s_line)))
    {
        number += 1u;
        s_line[strcspn(s_line, "\r\n")] = '\0';
        if (s_line[strspn(s_line, " ")] == '\0')
        {
            continue;
        }
        const char *rest = NULL;
        unsigned long long words[5] = {0ull, 0ull, 0ull, 0ull, 0ull};
        if ((number == 1u) && kqr_line_type(s_line, "kqr"))
        {
            const char *const named = s_line + 3 + strspn(s_line + 3, " ");
            record->file.record[0].word[0] = kqr_text_number(record, named);
        }
        else if (s_line[0] == '#')
        {
            words[0] = kqr_text_number(record, s_line);
            krep_record_add(&record->file, KQR_COMMENT_RECORD, words, 1u);
        }
        else if (kqr_line_type(s_line, "cycle"))
        {
            const char *const after = s_line + 6;
            const size_t length = strcspn(after, " ");
            stopped = kqr_number_read(after, length, 10, &words[0]) ? NULL : "a cycle with no number";
            words[1] = kqr_text_number(record, after + length + ((after[length] == ' ') ? 1u : 0u));
            krep_record_add(&record->file, KQR_CYCLE_RECORD, words, 2u);
        }
        else if (kqr_line_type(s_line, "seed") || kqr_line_type(s_line, "round"))
        {
            const int seed = kqr_line_type(s_line, "seed");
            const char *const after = s_line + (seed ? 5 : 6);
            stopped = kqr_number_read(after, strlen(after), 10, &words[0]) ? NULL : "a seed or a round of no number";
            krep_record_add(&record->file, seed ? KQR_SEED_RECORD : KQR_ROUND_RECORD, words, 1u);
        }
        else if (kqr_line_type(s_line, "pair"))
        {
            const char *field = s_line + 4 + strspn(s_line + 4, " ");
            const size_t first = strcspn(field, " ");
            char text[KQR_VERDICT];
            kqr_text_cut(text, sizeof(text), field, first);
            words[0] = kqr_text_number(record, text);
            field += first + strspn(field + first, " ");
            const size_t second = strcspn(field, " ");
            kqr_text_cut(text, sizeof(text), field, second);
            words[1] = kqr_text_number(record, text);
            field += second + strspn(field + second, " ");
            // the hash of the pair's reads, sixteen hex digits ahead of the verdict, where the record contains one
            const size_t gap = strcspn(field, " ");
            words[2] =
                ((gap == 16u) && (field[gap] == ' ') && (strspn(field, "0123456789abcdef") >= 16u)) ? 1ull : 0ull;
            words[3] = words[2] ? strtoull(field, NULL, 16) : 0ull;
            field += words[2] ? 17 : 0;
            words[4] = kqr_text_number(record, field);
            stopped = (field[0] != '\0') ? NULL : "a pair with no verdict";
            krep_record_add(&pairs, KQR_PAIR_RECORD, words, 5u);
        }
        else if (kqr_line_type(s_line, "sample"))
        {
            memset(&s_sample, 0, sizeof(s_sample));
            stopped = kqr_line_identity(s_line, &s_sample.identity, &s_sample.answer, &rest)
                          ? NULL
                          : "a sample with no identity or no answer";
            if ((stopped == NULL) && (s_sample.answer == KQR_ANSWERS))
            {
                char *after = NULL;
                s_sample.nanoseconds = strtoull(rest, &after, 10);
                char *least_after = NULL;
                const unsigned long long least = strtoull(after, &least_after, 10);
                char *max_after = NULL;
                const unsigned long long max = strtoull(least_after, &max_after, 10);
                s_sample.launched = ((least_after != after) && (max_after != least_after)) ? 1 : 0;
                s_sample.least = s_sample.launched ? least : 0ull;
                s_sample.max = s_sample.launched ? max : 0ull;
            }
            else if (stopped == NULL)
            {
                kqr_text_cut(s_sample.refusal, sizeof(s_sample.refusal), rest, strlen(rest));
            }
            if (stopped == NULL)
            {
                krep_record_add(&record->file, KQR_SAMPLE_RECORD, s_words,
                                kqr_sample_words(record, &s_sample, s_words));
            }
        }
        else if (kqr_line_type(s_line, "query"))
        {
            s_query.words = 0u;
            s_query.refusal[0] = '\0';
            stopped = kqr_line_identity(s_line, &s_query.identity, &s_query.answer, &rest)
                          ? NULL
                          : "a query with no identity or no answer";
            if ((stopped == NULL) && (s_query.answer == KQR_ANSWERS))
            {
                char *after = NULL;
                while ((*rest != '\0') && (s_query.words < KQR_WORDS))
                {
                    s_query.word[s_query.words] = strtoull(rest, &after, 16);
                    if (after == rest)
                    {
                        break;
                    }
                    s_query.words += 1u;
                    rest = after + ((*after == ' ') ? 1 : 0);
                }
            }
            else if (stopped == NULL)
            {
                kqr_text_cut(s_query.refusal, sizeof(s_query.refusal), rest, strlen(rest));
            }
            if (stopped == NULL)
            {
                krep_record_add(&record->file, KQR_QUERY_RECORD, s_words, kqr_query_words(record, &s_query, s_words));
            }
        }
        else
        {
            stopped = "is no line of a query record";
        }
    }
    for (unsigned int pair = 0u; pair < pairs.records; pair += 1u)
    {
        krep_record_add(&record->file, KQR_PAIR_RECORD, pairs.record[pair].word, pairs.record[pair].count);
    }
    krep_free(&pairs);
    if (stopped != NULL)
    {
        char digits[24];
        const size_t digits_at =
            verba_numerus.u64(&(VerbaNumerusCfg){.out = digits, .cap = sizeof(digits), .at = 0u, .val = number});
        size_t error_at = kqr_error_put(error, room, 0u, path, strlen(path));
        error_at = kqr_error_put(error, room, error_at, ":", 1u);
        error_at = kqr_error_put(error, room, error_at, digits, digits_at);
        error_at = kqr_error_put(error, room, error_at, ": ", 2u);
        if (strcmp(stopped, "is no line of a query record") == 0)
        {
            error_at = kqr_error_put(error, room, error_at, s_line, strcspn(s_line, " "));
            error_at = kqr_error_put(error, room, error_at, " ", 1u);
        }
        error_at = kqr_error_put(error, room, error_at, stopped, strlen(stopped));
        verba_finis.finish(&(VerbaFinisCfg){.out = error, .cap = room, .at = error_at});
        return 0;
    }
    return 1;
}

// every query of an identity stored before it let go but where the one before timed out and it takes that one's place:
// each identity's query stored once, as the text form's reading contains it
static void kqr_queries_once(KqrRecord *record)
{
    static KqrQuery s_query;
    static KqrQuery s_before;
    static unsigned long long s_words[KQR_QUERY_WORDS + KQR_WORDS + 1u];
    memset(record->slot, 0, KQR_SLOTS * sizeof(unsigned int));
    unsigned int at = 0u;
    while (at < record->file.records)
    {
        const KrepRecord *const entry = &record->file.record[at];
        if ((entry->code != KQR_QUERY_RECORD) || !kqr_query_of(record, entry, &s_query))
        {
            at += 1u;
            continue;
        }
        const unsigned int slot = kqr_slot_of(record, &s_query.identity);
        if (record->slot[slot] == 0u)
        {
            record->slot[slot] = at + 1u;
            at += 1u;
            continue;
        }
        const unsigned int before = record->slot[slot] - 1u;
        if (kqr_query_of(record, &record->file.record[before], &s_before) && (s_before.answer == KQR_TIMED_OUT))
        {
            krep_record_set(&record->file, before, s_words, kqr_query_words(record, &s_query, s_words));
        }
        krep_record_drop(&record->file, at);
    }
}

int kqr_read(KqrRecord *record, const char *path, const char *member, char *error, size_t room)
{
    if (!kqr_opened(record, member))
    {
        static const char s_no_room[] = ": no room for the table of queries";
        const size_t error_at = kqr_error_put(error, room, 0u, path, strlen(path));
        verba_finis.finish(&(VerbaFinisCfg){
            .out = error, .cap = room, .at = kqr_error_put(error, room, error_at, s_no_room, sizeof(s_no_room) - 1u)});
        return 0;
    }
    const int read = krep_read(&record->file, path, KQR_KIND, error, room);
    int ok = (read != 0);
    if (read == 2)
    {
        // the text form: its bytes read again, as the run stores them, and taken as records
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        krep_open(&record->file, KQR_KIND);
        const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
        krep_record_add(&record->file, KQR_MEMBER_RECORD, &named, 1u);
        ok = qry_member_file_blob(path, &bytes, &size) && kqr_text_read(record, bytes, size, path, error, room);
        if (ok)
        {
            kqr_queries_once(record);
        }
    }
    else if (ok && ((record->file.records == 0u) || (record->file.record[0].code != KQR_MEMBER_RECORD)))
    {
        const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
        ok = krep_record_put(&record->file, 0u, KQR_MEMBER_RECORD, &named, 1u);
    }
    if (!ok)
    {
        kqr_opened(record, member);
        return 0;
    }
    kqr_indexed(record);
    return 1;
}

int kqr_viewed_at(KqrRecord *record, const char *path, const char *name, const char *member, char *error, size_t room)
{
    if (!kqr_opened(record, member))
    {
        static const char s_no_room[] = ": no room for the table of queries";
        const size_t error_at = kqr_error_put(error, room, 0u, path, strlen(path));
        verba_finis.finish(&(VerbaFinisCfg){
            .out = error, .cap = room, .at = kqr_error_put(error, room, error_at, s_no_room, sizeof(s_no_room) - 1u)});
        return 0;
    }
    int ok = krep_viewed_at(path, KQR_TABLE, name, &record->file, error, room);
    if (ok && (memcmp(record->file.type, KQR_KIND, 4u) != 0))
    {
        static const char s_no_kqr[] = ": the record in " KQR_TABLE " is no .kqr";
        const size_t error_at = kqr_error_put(error, room, 0u, path, strlen(path));
        verba_finis.finish(&(VerbaFinisCfg){
            .out = error, .cap = room, .at = kqr_error_put(error, room, error_at, s_no_kqr, sizeof(s_no_kqr) - 1u)});
        ok = 0;
    }
    if (ok && ((record->file.records == 0u) || (record->file.record[0].code != KQR_MEMBER_RECORD)))
    {
        const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
        ok = krep_record_put(&record->file, 0u, KQR_MEMBER_RECORD, &named, 1u);
    }
    if (!ok)
    {
        kqr_opened(record, member);
        return 0;
    }
    kqr_indexed(record);
    return 1;
}
