// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: the record read: each line's kind, a line read as an ask, the text form read whole, the asks read once each,
// the record read from its file, and the record viewed at a run's asks table
#include "../../../parsers/line_read.h"
#include "kqr_internal.h"

// 1 where `line` is of the kind `kind`: its first word, and a space or its end after it
static int kqr_line_kind(const char *line, const char *kind)
{
    const size_t length = strlen(kind);
    return (strncmp(line, kind, length) == 0) && ((line[length] == ' ') || (line[length] == '\0'));
}

// the `length` bytes at `text`, as many of them as fit, into `out`, which holds `room`, and ended
static void kqr_text_cut(char *out, size_t room, const char *text, size_t length)
{
    const size_t at = verba_textus.put_n(&(VerbaTextusCfg){
        .out = out, .cap = room, .at = 0u, .text = text, .text_len = (length < room) ? length : (room - 1u)});
    verba_finis.finish(&(VerbaFinisCfg){.out = out, .cap = room, .at = at});
}

// the `length` bytes at `text` written after the `at` bytes of `error`, which holds `room`, as many of them as fit: the
// offset after them
static size_t kqr_said(char *error, size_t room, size_t at, const char *text, size_t length)
{
    const size_t left = room - 1u - at;
    return verba_textus.put_n(&(VerbaTextusCfg){
        .out = error, .cap = room, .at = at, .text = text, .text_len = (length < left) ? length : left});
}

// the ask or sample line `line`, past its kind, read into its identity, its answer and where what follows starts:
// 1, or 0 where it has no identity or no answer the oracle gives
static int kqr_line_asked(const char *line, KqrIdentity *identity, unsigned int *answer, const char **rest)
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
    // the identity stands before its end, which an int counts
    if ((end == NULL) || ((size_t)(end - at) >= sizeof(text)))
    {
        return 0;
    }
    kqr_text_cut(text, sizeof(text), at, (size_t)(end - at));
    if (!kqr_identity_read(text, identity))
    {
        return 0;
    }
    const char *const said = end + 1;
    const size_t length = strcspn(said, " ");
    char name[16];
    if (length >= sizeof(name))
    {
        return 0;
    }
    memcpy(name, said, length);
    name[length] = '\0';
    *answer = kqr_answer_of(name);
    *rest = said + length + ((said[length] == ' ') ? 1u : 0u);
    return *answer < KQR_ANSWER_COUNT;
}

// The .kqr's text form, `size` bytes at `bytes`, read into `record` as records: the first line its member, every ask
// held once, an answer in place of one that timed out, and every pair after every other record. 1, or 0 and why in
// `error` where a line is none the oracle gives
static int kqr_text_read(KqrRecord *record, const unsigned char *bytes, unsigned long long size, const char *path,
                         char *error, size_t room)
{
    static char s_line[KQR_LINE];
    static unsigned long long s_words[KQR_ASKED_WORDS + KQR_WORDS + 1u];
    static KqrAsk s_ask;
    static KqrSample s_sample;
    // the pairs, kept apart and put after every other record
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
        if ((number == 1u) && kqr_line_kind(s_line, "kqr"))
        {
            const char *const named = s_line + 3 + strspn(s_line + 3, " ");
            record->file.record[0].word[0] = kqr_text_number(record, named);
        }
        else if (s_line[0] == '#')
        {
            words[0] = kqr_text_number(record, s_line);
            krep_record_add(&record->file, KQR_COMMENT_RECORD, words, 1u);
        }
        else if (kqr_line_kind(s_line, "cycle"))
        {
            const char *const after = s_line + 6;
            const size_t length = strcspn(after, " ");
            stopped = kqr_number_read(after, length, 10, &words[0]) ? NULL : "a cycle with no number";
            words[1] = kqr_text_number(record, after + length + ((after[length] == ' ') ? 1u : 0u));
            krep_record_add(&record->file, KQR_CYCLE_RECORD, words, 2u);
        }
        else if (kqr_line_kind(s_line, "seed") || kqr_line_kind(s_line, "round"))
        {
            const int seed = kqr_line_kind(s_line, "seed");
            const char *const after = s_line + (seed ? 5 : 6);
            stopped = kqr_number_read(after, strlen(after), 10, &words[0]) ? NULL : "a seed or a round of no number";
            krep_record_add(&record->file, seed ? KQR_SEED_RECORD : KQR_ROUND_RECORD, words, 1u);
        }
        else if (kqr_line_kind(s_line, "pair"))
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
            // the hash of the pair's reads, sixteen hex digits ahead of the verdict, where the record holds one
            const size_t gap = strcspn(field, " ");
            words[2] =
                ((gap == 16u) && (field[gap] == ' ') && (strspn(field, "0123456789abcdef") >= 16u)) ? 1ull : 0ull;
            words[3] = words[2] ? strtoull(field, NULL, 16) : 0ull;
            field += words[2] ? 17 : 0;
            words[4] = kqr_text_number(record, field);
            stopped = (field[0] != '\0') ? NULL : "a pair with no verdict";
            krep_record_add(&pairs, KQR_PAIR_RECORD, words, 5u);
        }
        else if (kqr_line_kind(s_line, "sample"))
        {
            memset(&s_sample, 0, sizeof(s_sample));
            stopped = kqr_line_asked(s_line, &s_sample.identity, &s_sample.answer, &rest)
                          ? NULL
                          : "a sample with no identity or no answer";
            if ((stopped == NULL) && (s_sample.answer == KQR_ANSWERS))
            {
                char *after = NULL;
                s_sample.nanoseconds = strtoull(rest, &after, 10);
                char *least_after = NULL;
                const unsigned long long least = strtoull(after, &least_after, 10);
                char *most_after = NULL;
                const unsigned long long most = strtoull(least_after, &most_after, 10);
                s_sample.launched = ((least_after != after) && (most_after != least_after)) ? 1 : 0;
                s_sample.least = s_sample.launched ? least : 0ull;
                s_sample.most = s_sample.launched ? most : 0ull;
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
        else if (kqr_line_kind(s_line, "ask"))
        {
            s_ask.words = 0u;
            s_ask.refusal[0] = '\0';
            stopped = kqr_line_asked(s_line, &s_ask.identity, &s_ask.answer, &rest)
                          ? NULL
                          : "an ask with no identity or no answer";
            if ((stopped == NULL) && (s_ask.answer == KQR_ANSWERS))
            {
                char *after = NULL;
                while ((*rest != '\0') && (s_ask.words < KQR_WORDS))
                {
                    s_ask.word[s_ask.words] = strtoull(rest, &after, 16);
                    if (after == rest)
                    {
                        break;
                    }
                    s_ask.words += 1u;
                    rest = after + ((*after == ' ') ? 1 : 0);
                }
            }
            else if (stopped == NULL)
            {
                kqr_text_cut(s_ask.refusal, sizeof(s_ask.refusal), rest, strlen(rest));
            }
            if (stopped == NULL)
            {
                krep_record_add(&record->file, KQR_ASK_RECORD, s_words, kqr_ask_words(record, &s_ask, s_words));
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
        size_t said = kqr_said(error, room, 0u, path, strlen(path));
        said = kqr_said(error, room, said, ":", 1u);
        said = kqr_said(error, room, said, digits, digits_at);
        said = kqr_said(error, room, said, ": ", 2u);
        if (strcmp(stopped, "is no line of a query record") == 0)
        {
            said = kqr_said(error, room, said, s_line, strcspn(s_line, " "));
            said = kqr_said(error, room, said, " ", 1u);
        }
        said = kqr_said(error, room, said, stopped, strlen(stopped));
        verba_finis.finish(&(VerbaFinisCfg){.out = error, .cap = room, .at = said});
        return 0;
    }
    return 1;
}

// every ask of an identity held before it let go but where the one before timed out and it takes that one's place:
// each identity's ask held once, as the text form's reading holds it
static void kqr_asks_once(KqrRecord *record)
{
    static KqrAsk s_ask;
    static KqrAsk s_before;
    static unsigned long long s_words[KQR_ASKED_WORDS + KQR_WORDS + 1u];
    memset(record->slot, 0, KQR_SLOTS * sizeof(unsigned int));
    unsigned int at = 0u;
    while (at < record->file.records)
    {
        const KrepRecord *const held = &record->file.record[at];
        if ((held->code != KQR_ASK_RECORD) || !kqr_ask_of(record, held, &s_ask))
        {
            at += 1u;
            continue;
        }
        const unsigned int slot = kqr_slot_of(record, &s_ask.identity);
        if (record->slot[slot] == 0u)
        {
            record->slot[slot] = at + 1u;
            at += 1u;
            continue;
        }
        const unsigned int before = record->slot[slot] - 1u;
        if (kqr_ask_of(record, &record->file.record[before], &s_before) && (s_before.answer == KQR_TIMED_OUT))
        {
            krep_record_set(&record->file, before, s_words, kqr_ask_words(record, &s_ask, s_words));
        }
        krep_record_drop(&record->file, at);
    }
}

int kqr_read(KqrRecord *record, const char *path, const char *member, char *error, size_t room)
{
    if (!kqr_opened(record, member))
    {
        static const char s_no_room[] = ": no room for the table of asks";
        const size_t said = kqr_said(error, room, 0u, path, strlen(path));
        verba_finis.finish(&(VerbaFinisCfg){
            .out = error, .cap = room, .at = kqr_said(error, room, said, s_no_room, sizeof(s_no_room) - 1u)});
        return 0;
    }
    const int read = krep_read(&record->file, path, KQR_KIND, error, room);
    int held = (read != 0);
    if (read == 2)
    {
        // the text form: its bytes read again, as the run holds them, and taken as records
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        krep_open(&record->file, KQR_KIND);
        const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
        krep_record_add(&record->file, KQR_MEMBER_RECORD, &named, 1u);
        held = qry_member_file_blob(path, &bytes, &size) && kqr_text_read(record, bytes, size, path, error, room);
        if (held)
        {
            kqr_asks_once(record);
        }
    }
    else if (held && ((record->file.records == 0u) || (record->file.record[0].code != KQR_MEMBER_RECORD)))
    {
        const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
        held = krep_record_put(&record->file, 0u, KQR_MEMBER_RECORD, &named, 1u);
    }
    if (!held)
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
        static const char s_no_room[] = ": no room for the table of asks";
        const size_t said = kqr_said(error, room, 0u, path, strlen(path));
        verba_finis.finish(&(VerbaFinisCfg){
            .out = error, .cap = room, .at = kqr_said(error, room, said, s_no_room, sizeof(s_no_room) - 1u)});
        return 0;
    }
    int held = krep_viewed_at(path, KQR_TABLE, name, &record->file, error, room);
    if (held && (memcmp(record->file.kind, KQR_KIND, 4u) != 0))
    {
        static const char s_no_kqr[] = ": the record in " KQR_TABLE " is no .kqr";
        const size_t said = kqr_said(error, room, 0u, path, strlen(path));
        verba_finis.finish(&(VerbaFinisCfg){
            .out = error, .cap = room, .at = kqr_said(error, room, said, s_no_kqr, sizeof(s_no_kqr) - 1u)});
        held = 0;
    }
    if (held && ((record->file.records == 0u) || (record->file.record[0].code != KQR_MEMBER_RECORD)))
    {
        const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
        held = krep_record_put(&record->file, 0u, KQR_MEMBER_RECORD, &named, 1u);
    }
    if (!held)
    {
        kqr_opened(record, member);
        return 0;
    }
    kqr_indexed(record);
    return 1;
}
