// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// queries.c: a query and a sample as the record stores them: an answer's name, a query's identity read, written, hashed
// and stored, the slot of an identity, a query's and a sample's words, a row put with its who, the marks written, the
// record opened, and the queries indexed
#include "../../../parsers/who.h"
#include "kqr_internal.h"

// what came back of a query, each as the text form writes it, by its code
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
int kqr_number_read(const char *text, size_t length, int base, unsigned long long *value)
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
    // the five counts are each at most what an unsigned int represents, as read above
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
    size_t written =
        verba_numerus.hex(&(VerbaNumerusCfg){.out = text, .cap = room, .at = 0u, .val = identity->code, .min = 16u});
    const unsigned int counts[5] = {identity->registers, identity->threads, identity->blocks, identity->launches,
                                    identity->cases};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        written = verba_littera.ch(&(VerbaLitteraCfg){.out = text, .cap = room, .at = written, .ch = ' '});
        written = verba_numerus.u64(&(VerbaNumerusCfg){.out = text, .cap = room, .at = written, .val = counts[at]});
    }
    written = verba_littera.ch(&(VerbaLitteraCfg){.out = text, .cap = room, .at = written, .ch = ' '});
    written = verba_numerus.hex(
        &(VerbaNumerusCfg){.out = text, .cap = room, .at = written, .val = identity->cases_hash, .min = 16u});
    verba_finis.finish(&(VerbaFinisCfg){.out = text, .cap = room, .at = written});
}

// `identity` and `answer` as the first KQR_QUERY_WORDS words of a query's or a sample's record
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

// the identity and the answer of the first KQR_QUERY_WORDS words of a query's or a sample's record
static unsigned int kqr_identity_of_words(const unsigned long long *words, KqrIdentity *identity)
{
    // each count and the answer were written from an unsigned int, which represents them again
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
    unsigned long long words[KQR_QUERY_WORDS];
    kqr_identity_words(identity, 0u, words);
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (unsigned int word = 0u; word < (KQR_QUERY_WORDS - 1u); word += 1u)
    {
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            hash = (hash ^ ((words[word] >> (8u * byte)) & 0xffull)) * 0x100000001b3ull;
        }
    }
    return hash;
}

// 1 where the query or sample record `entry` is of `identity`, its seven numbers compared as numbers
static int kqr_identity_matches(const KrepRecord *entry, const KqrIdentity *identity)
{
    unsigned long long words[KQR_QUERY_WORDS];
    kqr_identity_words(identity, 0u, words);
    return (entry->count >= KQR_QUERY_WORDS) &&
           (memcmp(entry->word, words, (KQR_QUERY_WORDS - 1u) * sizeof(unsigned long long)) == 0);
}

// the bucket of `identity`: the one containing it, or the empty one it would be put in
unsigned int kqr_slot_of(const KqrRecord *record, const KqrIdentity *identity)
{
    unsigned int at = (unsigned int)(kqr_identity_hash(identity) & (KQR_SLOTS - 1u));
    while (record->slot[at] != 0u)
    {
        if (kqr_identity_matches(&record->file.record[record->slot[at] - 1u], identity))
        {
            return at;
        }
        at = (at + 1u) & (KQR_SLOTS - 1u);
    }
    return at;
}

// the text of the number `number` copied into `text`, which has room for `room`, cut to it; empty where the file gives
// none
void kqr_text_copied(const KqrRecord *record, unsigned long long number, char *text, size_t room)
{
    unsigned long long length = 0ull;
    const char *const found = krep_text(&record->file, number, &length);
    // a text is cut to its room
    const size_t written =
        (found == NULL) ? 0u
                        : verba_textus.put_n(&(VerbaTextusCfg){.out = text,
                                                               .cap = room,
                                                               .at = 0u,
                                                               .text = found,
                                                               .text_len = (length < room) ? length : (room - 1u)});
    verba_finis.finish(&(VerbaFinisCfg){.out = text, .cap = room, .at = written});
}

// the number of `text`, stored once, or 0 where it is empty or there was no memory for it
unsigned long long kqr_text_number(KqrRecord *record, const char *text)
{
    return (text[0] == '\0') ? 0ull : krep_text_number(&record->file, text, strlen(text));
}

// The query record `entry` read into `query`: 1, or 0 where it is not a query's
int kqr_query_of(const KqrRecord *record, const KrepRecord *entry, KqrQuery *query)
{
    if ((entry->code != KQR_QUERY_RECORD) || (entry->count < KQR_QUERY_WORDS))
    {
        return 0;
    }
    query->answer = kqr_identity_of_words(entry->word, &query->identity);
    query->words = 0u;
    query->refusal[0] = '\0';
    if (query->answer == KQR_ANSWERS)
    {
        for (unsigned int place = KQR_QUERY_WORDS; (place < entry->count) && (query->words < KQR_WORDS); place += 1u)
        {
            query->word[query->words] = entry->word[place];
            query->words += 1u;
        }
    }
    else if (entry->count > KQR_QUERY_WORDS)
    {
        kqr_text_copied(record, entry->word[KQR_QUERY_WORDS], query->refusal, sizeof(query->refusal));
    }
    return 1;
}

// the record of `query` into `words`, which has room for KQR_QUERY_WORDS + KQR_WORDS + 1, the refusal's text taken: its
// count
unsigned int kqr_query_words(KqrRecord *record, const KqrQuery *query, unsigned long long *words)
{
    kqr_identity_words(&query->identity, query->answer, words);
    unsigned int count = KQR_QUERY_WORDS;
    if (query->answer == KQR_ANSWERS)
    {
        for (unsigned int place = 0u; (place < query->words) && (place < KQR_WORDS); place += 1u)
        {
            words[count] = query->word[place];
            count += 1u;
        }
    }
    else if (query->refusal[0] != '\0')
    {
        words[count] = kqr_text_number(record, query->refusal);
        count += 1u;
    }
    return count;
}

// The sample record `entry` read into `sample`: 1, or 0 where it is not a sample's
int kqr_sample_of(const KqrRecord *record, const KrepRecord *entry, KqrSample *sample)
{
    if ((entry->code != KQR_SAMPLE_RECORD) || (entry->count < KQR_QUERY_WORDS))
    {
        return 0;
    }
    memset(sample, 0, sizeof(*sample));
    sample->answer = kqr_identity_of_words(entry->word, &sample->identity);
    if (sample->answer == KQR_ANSWERS)
    {
        sample->nanoseconds = (entry->count > KQR_QUERY_WORDS) ? entry->word[KQR_QUERY_WORDS] : 0ull;
        sample->launched = (entry->count >= (KQR_QUERY_WORDS + 3u)) ? 1 : 0;
        sample->least = sample->launched ? entry->word[KQR_QUERY_WORDS + 1u] : 0ull;
        sample->most = sample->launched ? entry->word[KQR_QUERY_WORDS + 2u] : 0ull;
    }
    else if (entry->count > KQR_QUERY_WORDS)
    {
        kqr_text_copied(record, entry->word[KQR_QUERY_WORDS], sample->refusal, sizeof(sample->refusal));
    }
    return 1;
}

// the record of `sample` into `words`, which has room for KQR_QUERY_WORDS + 3, the refusal's text taken: its count
unsigned int kqr_sample_words(KqrRecord *record, const KqrSample *sample, unsigned long long *words)
{
    kqr_identity_words(&sample->identity, sample->answer, words);
    unsigned int count = KQR_QUERY_WORDS;
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

// every bucket, sample and the pairs' start that stands at the record `from` or past it moved `by` records on
void kqr_shifted(KqrRecord *record, unsigned int from, unsigned int by)
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

// a who record of `who` put at `at`, or one of no words, no who, where `has_who` is 0
int kqr_who_put(KqrRecord *record, unsigned int at, int has_who, Who who)
{
    unsigned long long words[2];
    who_words(who, words);
    return krep_record_put(&record->file, at, KREP_RECORD_WHO, words, has_who ? 2u : 0u);
}

// a record of `code` and `count` words from `words` put by `who` before the pairs, after a who record of `who` where
// the who in force there is another's, and the pairs then put after the who that was in force over them: the record's
// place, or the count of records where it could not be put
unsigned int kqr_put(KqrRecord *record, unsigned int code, const unsigned long long *words, unsigned int count, Who who)
{
    Who before;
    const int before_has_who = krep_who_at(&record->file, record->first_pair, &before);
    if (!before_has_who || !krep_who_same(before, who))
    {
        const int pairs = (record->first_pair < record->file.records);
        const int pairs_own = pairs && (record->file.record[record->first_pair].code == KREP_RECORD_WHO);
        if (pairs && !pairs_own && !kqr_who_put(record, record->first_pair, before_has_who, before))
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

// the marks pending written by `who` before the pairs, a cycle numbered after every cycle the record contains
void kqr_marks_written(KqrRecord *record, Who who)
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

// `record` open and empty, the member `member`: 1, or 0 where there is no room for the table of queries
int kqr_opened(KqrRecord *record, const char *member)
{
    kqr_free(record);
    krep_open(&record->file, KQR_KIND);
    record->slot = (unsigned int *)calloc(KQR_SLOTS, sizeof(unsigned int));
    record->open = (record->slot != NULL);
    const unsigned long long named = krep_text_number(&record->file, member, strlen(member));
    return record->open && krep_record_add(&record->file, KQR_MEMBER_RECORD, &named, 1u);
}

// the sample at record `at` taken into the record's samples: 1, or 0 where it could not be stored
int kqr_sample_take(KqrRecord *record, unsigned int at)
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

// the record's table of queries, its samples, its cycles and where its pairs start read off its records
void kqr_indexed(KqrRecord *record)
{
    memset(record->slot, 0, KQR_SLOTS * sizeof(unsigned int));
    record->samples = 0u;
    record->query_count = 0u;
    record->cycles = 0u;
    record->first_pair = record->file.records;
    static KqrQuery s_query;
    for (unsigned int at = 0u; at < record->file.records; at += 1u)
    {
        const KrepRecord *const entry = &record->file.record[at];
        if ((entry->code == KQR_QUERY_RECORD) && kqr_query_of(record, entry, &s_query))
        {
            const unsigned int slot = kqr_slot_of(record, &s_query.identity);
            record->slot[slot] = (record->slot[slot] == 0u) ? (at + 1u) : record->slot[slot];
            record->query_count += 1u;
        }
        record->samples += 0u;
        if (entry->code == KQR_SAMPLE_RECORD)
        {
            kqr_sample_take(record, at);
        }
        record->cycles += (entry->code == KQR_CYCLE_RECORD) ? 1u : 0u;
        if ((entry->code == KQR_PAIR_RECORD) && (record->first_pair == record->file.records))
        {
            // the pairs start at the first pair, or at the who just before it
            record->first_pair =
                ((at != 0u) && (record->file.record[at - 1u].code == KREP_RECORD_WHO)) ? (at - 1u) : at;
        }
    }
}
