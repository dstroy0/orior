// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr.c: a member's .kqr read, asked of by an ask's identity, kept, and written back whole as records (kqr.h)
#include "kqr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the slots of the table an ask is found by its identity in, a power of two past every ask a record holds
#define KQR_SLOTS 1048576u
// the words of an ask's identity and answer in its record, before its answer's words or its refusal: the identity's
// seven numbers, then the answer
#define KQR_ASKED_WORDS 8u
// the most letters of a line of the text form read
#define KQR_LINE (1u << 20u)

// what came back of an ask, each as the text form writes it, by its code
static const char *const s_answers[KQR_ANSWER_COUNT] = {"answers", "illegal", "nothing", "timed_out", "censored"};

const char *kqr_answer_name(unsigned int answer)
{
    return (answer < KQR_ANSWER_COUNT) ? s_answers[answer] : "-";
}

unsigned int kqr_answer_of(const char *name)
{
    unsigned int answer = 0u;
    while ((answer < KQR_ANSWER_COUNT) && (strcmp(s_answers[answer], name) != 0))
    {
        answer += 1u;
    }
    return answer;
}

// the number `text` writes in `base`, all of it, into `value`: 1, or 0 where it is not that number written plain
static int kqr_number_read(const char *text, size_t length, int base, unsigned long long *value)
{
    char plain[32];
    if ((length == 0u) || (length >= sizeof(plain)))
    {
        return 0;
    }
    memcpy(plain, text, length);
    plain[length] = '\0';
    char *after = NULL;
    *value = strtoull(plain, &after, base);
    return (*after == '\0') && (plain[0] != '-') && (plain[0] != '+');
}

int kqr_identity_read(const char *text, KqrIdentity *identity)
{
    unsigned long long value[7];
    const char *at = text;
    for (unsigned int word = 0u; word < 7u; word += 1u)
    {
        const size_t length = strcspn(at, " ");
        const int hexed = (word == 0u) || (word == 6u);
        if (!kqr_number_read(at, length, hexed ? 16 : 10, &value[word]) || (!hexed && (value[word] > 0xffffffffull)))
        {
            return 0;
        }
        at += length;
        if ((word < 6u) && (*at != ' '))
        {
            return 0;
        }
        at += (word < 6u) ? 1 : 0;
    }
    if (*at != '\0')
    {
        return 0;
    }
    // the five counts are each at most what an unsigned int holds, as read above
    identity->code = value[0];
    identity->registers = (unsigned int)value[1];
    identity->threads = (unsigned int)value[2];
    identity->blocks = (unsigned int)value[3];
    identity->launches = (unsigned int)value[4];
    identity->cases = (unsigned int)value[5];
    identity->cases_hash = value[6];
    return 1;
}

void kqr_identity_text(const KqrIdentity *identity, char *text, size_t room)
{
    snprintf(text, room, "%016llx %u %u %u %u %u %016llx", identity->code, identity->registers, identity->threads,
             identity->blocks, identity->launches, identity->cases, identity->cases_hash);
}

// `identity` and `answer` as the first KQR_ASKED_WORDS words of an ask's or a sample's record
static void kqr_identity_words(const KqrIdentity *identity, unsigned int answer, unsigned long long *words)
{
    words[0] = identity->code;
    words[1] = identity->registers;
    words[2] = identity->threads;
    words[3] = identity->blocks;
    words[4] = identity->launches;
    words[5] = identity->cases;
    words[6] = identity->cases_hash;
    words[7] = answer;
}

// the identity and the answer of the first KQR_ASKED_WORDS words of an ask's or a sample's record
static unsigned int kqr_identity_of_words(const unsigned long long *words, KqrIdentity *identity)
{
    // each count and the answer were written from an unsigned int, which holds them again
    identity->code = words[0];
    identity->registers = (unsigned int)words[1];
    identity->threads = (unsigned int)words[2];
    identity->blocks = (unsigned int)words[3];
    identity->launches = (unsigned int)words[4];
    identity->cases = (unsigned int)words[5];
    identity->cases_hash = words[6];
    return (unsigned int)words[7];
}

// a 64-bit FNV-1a hash of the identity's seven numbers
static unsigned long long kqr_identity_hash(const KqrIdentity *identity)
{
    unsigned long long words[KQR_ASKED_WORDS];
    kqr_identity_words(identity, 0u, words);
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (unsigned int word = 0u; word < (KQR_ASKED_WORDS - 1u); word += 1u)
    {
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            hash = (hash ^ ((words[word] >> (8u * byte)) & 0xffull)) * 0x100000001b3ull;
        }
    }
    return hash;
}

// 1 where the ask or sample record `held` is of `identity`, its seven numbers compared as numbers
static int kqr_identity_held(const KrepRecord *held, const KqrIdentity *identity)
{
    unsigned long long words[KQR_ASKED_WORDS];
    kqr_identity_words(identity, 0u, words);
    return (held->count >= KQR_ASKED_WORDS) &&
           (memcmp(held->word, words, (KQR_ASKED_WORDS - 1u) * sizeof(unsigned long long)) == 0);
}

// the slot of `identity`: the one holding it, or the empty one it would be put in
static unsigned int kqr_slot_of(const KqrRecord *record, const KqrIdentity *identity)
{
    unsigned int at = (unsigned int)(kqr_identity_hash(identity) & (KQR_SLOTS - 1u));
    while (record->slot[at] != 0u)
    {
        if (kqr_identity_held(&record->file.record[record->slot[at] - 1u], identity))
        {
            return at;
        }
        at = (at + 1u) & (KQR_SLOTS - 1u);
    }
    return at;
}

// the text of the number `number` copied into `text`, which holds `room`, cut to it; empty where the file gives none
static void kqr_text_copied(const KqrRecord *record, unsigned long long number, char *text, size_t room)
{
    unsigned long long length = 0ull;
    const char *const held = krep_text(&record->file, number, &length);
    // a text is cut to its room, which an int counts
    snprintf(text, room, "%.*s", (held != NULL) ? (int)((length < room) ? length : (room - 1u)) : 0,
             (held != NULL) ? held : "");
}

// the number of `text`, held once, or 0 where it is empty or there was no memory for it
static unsigned long long kqr_text_number(KqrRecord *record, const char *text)
{
    return (text[0] == '\0') ? 0ull : krep_text_number(&record->file, text, strlen(text));
}

// The ask record `held` read into `ask`: 1, or 0 where it is not an ask's
static int kqr_ask_of(const KqrRecord *record, const KrepRecord *held, KqrAsk *ask)
{
    if ((held->code != KQR_ASK_RECORD) || (held->count < KQR_ASKED_WORDS))
    {
        return 0;
    }
    ask->answer = kqr_identity_of_words(held->word, &ask->identity);
    ask->words = 0u;
    ask->refusal[0] = '\0';
    if (ask->answer == KQR_ANSWERS)
    {
        for (unsigned int place = KQR_ASKED_WORDS; (place < held->count) && (ask->words < KQR_WORDS); place += 1u)
        {
            ask->word[ask->words] = held->word[place];
            ask->words += 1u;
        }
    }
    else if (held->count > KQR_ASKED_WORDS)
    {
        kqr_text_copied(record, held->word[KQR_ASKED_WORDS], ask->refusal, sizeof(ask->refusal));
    }
    return 1;
}

// the record of `ask` into `words`, which holds KQR_ASKED_WORDS + KQR_WORDS + 1, the refusal's text taken: its count
static unsigned int kqr_ask_words(KqrRecord *record, const KqrAsk *ask, unsigned long long *words)
{
    kqr_identity_words(&ask->identity, ask->answer, words);
    unsigned int count = KQR_ASKED_WORDS;
    if (ask->answer == KQR_ANSWERS)
    {
        for (unsigned int place = 0u; (place < ask->words) && (place < KQR_WORDS); place += 1u)
        {
            words[count] = ask->word[place];
            count += 1u;
        }
    }
    else if (ask->refusal[0] != '\0')
    {
        words[count] = kqr_text_number(record, ask->refusal);
        count += 1u;
    }
    return count;
}

// The sample record `held` read into `sample`: 1, or 0 where it is not a sample's
static int kqr_sample_of(const KqrRecord *record, const KrepRecord *held, KqrSample *sample)
{
    if ((held->code != KQR_SAMPLE_RECORD) || (held->count < KQR_ASKED_WORDS))
    {
        return 0;
    }
    memset(sample, 0, sizeof(*sample));
    sample->answer = kqr_identity_of_words(held->word, &sample->identity);
    if (sample->answer == KQR_ANSWERS)
    {
        sample->nanoseconds = (held->count > KQR_ASKED_WORDS) ? held->word[KQR_ASKED_WORDS] : 0ull;
        sample->launched = (held->count >= (KQR_ASKED_WORDS + 3u)) ? 1 : 0;
        sample->least = sample->launched ? held->word[KQR_ASKED_WORDS + 1u] : 0ull;
        sample->most = sample->launched ? held->word[KQR_ASKED_WORDS + 2u] : 0ull;
    }
    else if (held->count > KQR_ASKED_WORDS)
    {
        kqr_text_copied(record, held->word[KQR_ASKED_WORDS], sample->refusal, sizeof(sample->refusal));
    }
    return 1;
}

// the record of `sample` into `words`, which holds KQR_ASKED_WORDS + 3, the refusal's text taken: its count
static unsigned int kqr_sample_words(KqrRecord *record, const KqrSample *sample, unsigned long long *words)
{
    kqr_identity_words(&sample->identity, sample->answer, words);
    unsigned int count = KQR_ASKED_WORDS;
    if (sample->answer == KQR_ANSWERS)
    {
        words[count] = sample->nanoseconds;
        count += 1u;
        if (sample->launched)
        {
            words[count] = sample->least;
            words[count + 1u] = sample->most;
            count += 2u;
        }
    }
    else if (sample->refusal[0] != '\0')
    {
        words[count] = kqr_text_number(record, sample->refusal);
        count += 1u;
    }
    return count;
}

// every slot, sample and the pairs' start that stands at the record `from` or past it moved `by` records on
static void kqr_shifted(KqrRecord *record, unsigned int from, unsigned int by)
{
    for (unsigned int at = 0u; at < KQR_SLOTS; at += 1u)
    {
        record->slot[at] += ((record->slot[at] != 0u) && ((record->slot[at] - 1u) >= from)) ? by : 0u;
    }
    for (unsigned int at = 0u; at < record->samples; at += 1u)
    {
        record->sample[at] += (record->sample[at] >= from) ? by : 0u;
    }
    record->first_pair += (record->first_pair >= from) ? by : 0u;
}

// a who record of `who` put at `at`, or one of no words, no who, where `held` is 0
static int kqr_who_put(KqrRecord *record, unsigned int at, int held, QryWho who)
{
    unsigned long long words[2];
    qry_who_words(who, words);
    return krep_record_put(&record->file, at, KREP_RECORD_WHO, words, held ? 2u : 0u);
}

// a record of `code` and `count` words from `words` put by `who` before the pairs, after a who record of `who` where
// the who in force there is another's, and the pairs then put after the who that was in force over them: the record's
// place, or the count of records where it could not be put
static unsigned int kqr_put(KqrRecord *record, unsigned int code, const unsigned long long *words, unsigned int count,
                            QryWho who)
{
    QryWho before;
    const int before_held = krep_who_at(&record->file, record->first_pair, &before);
    if (!before_held || !krep_who_same(before, who))
    {
        const int pairs = (record->first_pair < record->file.records);
        const int pairs_own = pairs && (record->file.record[record->first_pair].code == KREP_RECORD_WHO);
        if (pairs && !pairs_own && !kqr_who_put(record, record->first_pair, before_held, before))
        {
            return record->file.records;
        }
        if (!kqr_who_put(record, record->first_pair, 1, who))
        {
            return record->file.records;
        }
        record->first_pair += 1u;
    }
    if (!krep_record_put(&record->file, record->first_pair, code, words, count))
    {
        return record->file.records;
    }
    record->first_pair += 1u;
    return record->first_pair - 1u;
}

// the marks pending written by `who` before the pairs, a cycle numbered after every cycle the record holds
static void kqr_marks_written(KqrRecord *record, QryWho who)
{
    for (unsigned int at = 0u; at < record->pendings; at += 1u)
    {
        unsigned long long words[2] = {record->pending_word[at][0], record->pending_word[at][1]};
        unsigned int count = 1u;
        if (record->pending_code[at] == KQR_CYCLE_RECORD)
        {
            words[0] = record->cycles + 1u;
            words[1] = record->pending_word[at][0];
            count = 2u;
            record->cycles += 1u;
        }
        kqr_put(record, record->pending_code[at], words, count, who);
    }
    record->pendings = 0u;
}

// `record` open and empty, the member `member`: 1, or 0 where there is no room for the table of asks
static int kqr_opened(KqrRecord *record, const char *member)
{
    kqr_free(record);
    krep_open(&record->file, KQR_KIND);
    record->slot = (unsigned int *)calloc(KQR_SLOTS, sizeof(unsigned int));
    record->open = (record->slot != NULL);
    const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
    return record->open && krep_record_add(&record->file, KQR_MEMBER_RECORD, &named, 1u);
}

// the sample at record `at` taken into the record's samples: 1, or 0 where it could not be held
static int kqr_sample_held(KqrRecord *record, unsigned int at)
{
    if (record->samples == record->sample_room)
    {
        const unsigned int grown = (record->sample_room == 0u) ? 256u : (2u * record->sample_room);
        unsigned int *const larger = (unsigned int *)realloc(record->sample, (size_t)grown * sizeof(unsigned int));
        if (larger == NULL)
        {
            return 0;
        }
        record->sample = larger;
        record->sample_room = grown;
    }
    record->sample[record->samples] = at;
    record->samples += 1u;
    return 1;
}

// the record's table of asks, its samples, its cycles and where its pairs start read off its records
static void kqr_indexed(KqrRecord *record)
{
    memset(record->slot, 0, KQR_SLOTS * sizeof(unsigned int));
    record->samples = 0u;
    record->asks = 0u;
    record->cycles = 0u;
    record->first_pair = record->file.records;
    static KqrAsk s_ask;
    for (unsigned int at = 0u; at < record->file.records; at += 1u)
    {
        const KrepRecord *const held = &record->file.record[at];
        if ((held->code == KQR_ASK_RECORD) && kqr_ask_of(record, held, &s_ask))
        {
            const unsigned int slot = kqr_slot_of(record, &s_ask.identity);
            record->slot[slot] = (record->slot[slot] == 0u) ? (at + 1u) : record->slot[slot];
            record->asks += 1u;
        }
        record->samples += 0u;
        if (held->code == KQR_SAMPLE_RECORD)
        {
            kqr_sample_held(record, at);
        }
        record->cycles += (held->code == KQR_CYCLE_RECORD) ? 1u : 0u;
        if ((held->code == KQR_PAIR_RECORD) && (record->first_pair == record->file.records))
        {
            // the pairs start at the first pair, or at the who just before it
            record->first_pair =
                ((at != 0u) && (record->file.record[at - 1u].code == KREP_RECORD_WHO)) ? (at - 1u) : at;
        }
    }
}

// 1 where `line` is of the kind `kind`: its first word, and a space or its end after it
static int kqr_line_kind(const char *line, const char *kind)
{
    const size_t length = strlen(kind);
    return (strncmp(line, kind, length) == 0) && ((line[length] == ' ') || (line[length] == '\0'));
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
    snprintf(text, sizeof(text), "%.*s", (int)(end - at), at);
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
    KrepFile pairs = {{0}, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u};
    unsigned long long at = 0ull;
    unsigned int number = 0u;
    const char *stopped = NULL;
    while ((stopped == NULL) && qry_line_next(bytes, size, &at, s_line, sizeof(s_line)))
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
            snprintf(text, sizeof(text), "%.*s", (int)first, field);
            words[0] = kqr_text_number(record, text);
            field += first + strspn(field + first, " ");
            const size_t second = strcspn(field, " ");
            snprintf(text, sizeof(text), "%.*s", (int)second, field);
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
                snprintf(s_sample.refusal, sizeof(s_sample.refusal), "%s", rest);
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
                snprintf(s_ask.refusal, sizeof(s_ask.refusal), "%s", rest);
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
        if (strcmp(stopped, "is no line of a query record") == 0)
        {
            snprintf(error, room, "%s:%u: %.*s %s", path, number, (int)strcspn(s_line, " "), s_line, stopped);
        }
        else
        {
            snprintf(error, room, "%s:%u: %s", path, number, stopped);
        }
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
        KrepRecord *const before = &record->file.record[record->slot[slot] - 1u];
        if (kqr_ask_of(record, before, &s_before) && (s_before.answer == KQR_TIMED_OUT))
        {
            const unsigned int count = kqr_ask_words(record, &s_ask, s_words);
            unsigned long long *const copy = (unsigned long long *)malloc((size_t)count * sizeof(unsigned long long));
            if (copy != NULL)
            {
                memcpy(copy, s_words, (size_t)count * sizeof(unsigned long long));
                free(before->word);
                before->word = copy;
                before->count = count;
            }
        }
        krep_record_drop(&record->file, at);
    }
}

int kqr_read(KqrRecord *record, const char *path, const char *member, char *error, size_t room)
{
    if (!kqr_opened(record, member))
    {
        snprintf(error, room, "%s: no room for the table of asks", path);
        return 0;
    }
    const int read = krep_read(&record->file, path, KQR_KIND, error, room);
    int held = (read != 0);
    if (read == 2)
    {
        // the text form: its bytes read again, as the run holds them, and taken as records
        unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        krep_open(&record->file, KQR_KIND);
        const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
        krep_record_add(&record->file, KQR_MEMBER_RECORD, &named, 1u);
        held = qry_file_read(qry_run(), path, &bytes, &size) && kqr_text_read(record, bytes, size, path, error, room);
        free(bytes);
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

int kqr_write_by(const KqrRecord *record, const char *path, QryWho who)
{
    return krep_write_by(&record->file, path, who);
}

void kqr_free(KqrRecord *record)
{
    krep_free(&record->file);
    free(record->slot);
    free(record->sample);
    memset(record, 0, sizeof(*record));
}

int kqr_find(const KqrRecord *record, const KqrIdentity *identity, KqrAsk *held)
{
    if (!record->open)
    {
        return 0;
    }
    const unsigned int slot = kqr_slot_of(record, identity);
    return (record->slot[slot] != 0u) && kqr_ask_of(record, &record->file.record[record->slot[slot] - 1u], held);
}

void kqr_keep_by(KqrRecord *record, const KqrAsk *ask, QryWho who)
{
    if (!record->open)
    {
        return;
    }
    static unsigned long long s_words[KQR_ASKED_WORDS + KQR_WORDS + 1u];
    static KqrAsk s_held;
    const unsigned int count = kqr_ask_words(record, ask, s_words);
    const unsigned int slot = kqr_slot_of(record, &ask->identity);
    if (record->slot[slot] == 0u)
    {
        kqr_marks_written(record, who);
        const unsigned int at = kqr_put(record, KQR_ASK_RECORD, s_words, count, who);
        if (at < record->file.records)
        {
            // the marks and the who put before it moved no ask: they went where the pairs start, past every ask
            record->slot[kqr_slot_of(record, &ask->identity)] = at + 1u;
            record->asks += 1u;
            record->changed = 1;
        }
        return;
    }
    unsigned int at = record->slot[slot] - 1u;
    if (!kqr_ask_of(record, &record->file.record[at], &s_held) || (s_held.answer != KQR_TIMED_OUT))
    {
        return;
    }
    // the answer that resolves it takes its place, after a who record of `who` and before the who in force there
    // again, where that one is another's
    QryWho there;
    const int there_held = krep_who_at(&record->file, at, &there);
    if (!there_held || !krep_who_same(there, who))
    {
        if (!kqr_who_put(record, at, 1, who))
        {
            return;
        }
        kqr_shifted(record, at, 1u);
        at += 1u;
        if (((at + 1u) < record->file.records) && kqr_who_put(record, at + 1u, there_held, there))
        {
            kqr_shifted(record, at + 1u, 1u);
        }
    }
    unsigned long long *const copy = (unsigned long long *)malloc((size_t)count * sizeof(unsigned long long));
    if (copy == NULL)
    {
        return;
    }
    memcpy(copy, s_words, (size_t)count * sizeof(unsigned long long));
    free(record->file.record[at].word);
    record->file.record[at].word = copy;
    record->file.record[at].count = count;
    record->changed = 1;
}

void kqr_sample_by(KqrRecord *record, const KqrSample *sample, QryWho who)
{
    if (!record->open)
    {
        return;
    }
    unsigned long long words[KQR_ASKED_WORDS + 3u];
    const unsigned int count = kqr_sample_words(record, sample, words);
    kqr_marks_written(record, who);
    const unsigned int at = kqr_put(record, KQR_SAMPLE_RECORD, words, count, who);
    if ((at < record->file.records) && kqr_sample_held(record, at))
    {
        record->changed = 1;
    }
}

void kqr_mark(KqrRecord *record, unsigned int code, const char *value)
{
    unsigned long long word = 0ull;
    if (code == KQR_CYCLE_RECORD)
    {
        word = kqr_text_number(record, value);
    }
    else if (!kqr_number_read(value, strlen(value), 10, &word))
    {
        return;
    }
    if ((code == KQR_ROUND_RECORD) && (record->pendings != 0u) &&
        (record->pending_code[record->pendings - 1u] == KQR_ROUND_RECORD))
    {
        record->pending_word[record->pendings - 1u][0] = word;
        return;
    }
    if (record->pendings == KQR_PENDING)
    {
        return;
    }
    record->pending_code[record->pendings] = code;
    record->pending_word[record->pendings][0] = word;
    record->pending_word[record->pendings][1] = 0ull;
    record->pendings += 1u;
}

int kqr_sample_read(const KqrRecord *record, unsigned int at, KqrSample *held)
{
    return (at < record->samples) && kqr_sample_of(record, &record->file.record[record->sample[at]], held);
}

int kqr_path_read(const KqrRecord *record, unsigned int at, KqrPath *held)
{
    unsigned int seen = 0u;
    for (unsigned int place = record->first_pair; place < record->file.records; place += 1u)
    {
        const KrepRecord *const pair = &record->file.record[place];
        if ((pair->code != KQR_PAIR_RECORD) || (pair->count < 5u))
        {
            continue;
        }
        if (seen == at)
        {
            memset(held, 0, sizeof(*held));
            kqr_text_copied(record, pair->word[0], held->first, sizeof(held->first));
            kqr_text_copied(record, pair->word[1], held->second, sizeof(held->second));
            held->read = (pair->word[2] != 0ull);
            held->reads = pair->word[3];
            kqr_text_copied(record, pair->word[4], held->verdict, sizeof(held->verdict));
            return 1;
        }
        seen += 1u;
    }
    return 0;
}

void kqr_paths_clear(KqrRecord *record)
{
    record->changed = record->changed || (record->first_pair < record->file.records);
    while (record->file.records > record->first_pair)
    {
        krep_record_drop(&record->file, record->file.records - 1u);
    }
}

void kqr_path_add_by(KqrRecord *record, const KqrPath *path, QryWho who)
{
    QryWho last;
    const int last_held = krep_who_at(&record->file, record->file.records, &last);
    if ((!last_held || !krep_who_same(last, who)) && !kqr_who_put(record, record->file.records, 1, who))
    {
        return;
    }
    const unsigned long long words[5] = {kqr_text_number(record, path->first), kqr_text_number(record, path->second),
                                         path->read ? 1ull : 0ull, path->read ? path->reads : 0ull,
                                         kqr_text_number(record, path->verdict)};
    if (krep_record_add(&record->file, KQR_PAIR_RECORD, words, 5u))
    {
        record->changed = 1;
    }
}

void kqr_text_rendered(const KqrRecord *record, QryText *text, int named)
{
    kqr_text_sliced(record, text, named, 0u, 0u);
}

void kqr_text_sliced(const KqrRecord *record, QryText *text, int named, unsigned int from, unsigned int count)
{
    static KqrAsk s_ask;
    static KqrSample s_sample;
    static KqrPath s_path;
    char line[KQR_VERDICT + (2u * KQR_FORM) + 256u];
    char identity[128];
    char said[2400];
    if (from >= record->file.records)
    {
        return;
    }
    const unsigned int past =
        ((count == 0u) || (count > (record->file.records - from))) ? record->file.records : (from + count);
    for (unsigned int at = from; at < past; at += 1u)
    {
        const KrepRecord *const held = &record->file.record[at];
        unsigned long long length = 0ull;
        const char *words = NULL;
        switch (held->code)
        {
        case KQR_MEMBER_RECORD:
            words = (held->count != 0u) ? krep_text(&record->file, held->word[0], &length) : NULL;
            qry_text_printed(text, "kqr %.*s\n", (int)length, (words != NULL) ? words : "");
            break;
        case KQR_COMMENT_RECORD:
            words = (held->count != 0u) ? krep_text(&record->file, held->word[0], &length) : NULL;
            qry_text_printed(text, "%.*s\n", (int)length, (words != NULL) ? words : "");
            break;
        case KQR_CYCLE_RECORD:
            words = (held->count > 1u) ? krep_text(&record->file, held->word[1], &length) : NULL;
            qry_text_printed(text, "cycle %llu %.*s\n", held->word[0], (int)length, (words != NULL) ? words : "");
            break;
        case KQR_SEED_RECORD:
            qry_text_printed(text, "seed %llu\n", held->word[0]);
            break;
        case KQR_ROUND_RECORD:
            qry_text_printed(text, "round %llu\n", held->word[0]);
            break;
        case KQR_ASK_RECORD:
            kqr_ask_of(record, held, &s_ask);
            kqr_identity_text(&s_ask.identity, identity, sizeof(identity));
            qry_text_printed(text, "ask %s %s", identity, kqr_answer_name(s_ask.answer));
            for (unsigned int place = 0u; (s_ask.answer == KQR_ANSWERS) && (place < s_ask.words); place += 1u)
            {
                qry_text_printed(text, " %llx", s_ask.word[place]);
            }
            qry_text_printed(text, "%s%s\n", (s_ask.refusal[0] != '\0') ? " " : "", s_ask.refusal);
            break;
        case KQR_SAMPLE_RECORD:
            kqr_sample_of(record, held, &s_sample);
            kqr_identity_text(&s_sample.identity, identity, sizeof(identity));
            if ((s_sample.answer == KQR_ANSWERS) && s_sample.launched)
            {
                snprintf(line, sizeof(line), " %llu %llu %llu", s_sample.nanoseconds, s_sample.least, s_sample.most);
            }
            else if (s_sample.answer == KQR_ANSWERS)
            {
                snprintf(line, sizeof(line), " %llu", s_sample.nanoseconds);
            }
            else
            {
                snprintf(line, sizeof(line), "%s%s", (s_sample.refusal[0] != '\0') ? " " : "", s_sample.refusal);
            }
            qry_text_printed(text, "sample %s %s%s\n", identity, kqr_answer_name(s_sample.answer), line);
            break;
        case KQR_PAIR_RECORD:
            memset(&s_path, 0, sizeof(s_path));
            kqr_text_copied(record, held->word[0], s_path.first, sizeof(s_path.first));
            kqr_text_copied(record, held->word[1], s_path.second, sizeof(s_path.second));
            kqr_text_copied(record, held->word[4], s_path.verdict, sizeof(s_path.verdict));
            if (held->word[2] != 0ull)
            {
                qry_text_printed(text, "pair %s %s %016llx %s\n", s_path.first, s_path.second, held->word[3],
                                 s_path.verdict);
            }
            else
            {
                qry_text_printed(text, "pair %s %s %s\n", s_path.first, s_path.second, s_path.verdict);
            }
            break;
        case KREP_RECORD_WHO:
            if (held->count != 2u)
            {
                qry_text_printed(text, "who -\n");
                break;
            }
            {
                char code[QRY_WHO_CODE_LETTERS + 1u];
                qry_who_code(qry_who_of_words(held->word), code, sizeof(code));
                if (named && qry_who_named(qry_who_of_words(held->word), said, sizeof(said)))
                {
                    qry_text_printed(text, "who %s %s\n", code, said);
                    break;
                }
                qry_text_printed(text, "who %s\n", code);
                break;
            }
        default:
            qry_text_printed(text, "record %x of %u words\n", held->code, held->count);
            break;
        }
    }
}
