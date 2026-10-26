// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// store.c: the record stored: tabled and written by a who, freed, a query found and stored, a sample stored, a mark
// made, an query, a sample and a path read, the paths cleared and added, and the text form rendered and sliced
#include "../../../parsers/who.h"
#include "kqr_internal.h"

int kqr_tabled_by(KqrRecord *record, QryShared *buffer, const char *path, Who who)
{
    return record->open && krep_tabled_by(&record->file, buffer, KQR_TABLE, path, who);
}

int kqr_write_by(const KqrRecord *record, const char *path, Who who)
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

int kqr_find(const KqrRecord *record, const KqrIdentity *identity, KqrQuery *out)
{
    if (!record->open)
    {
        return 0;
    }
    const unsigned int slot = kqr_slot_of(record, identity);
    return (record->slot[slot] != 0u) && kqr_query_of(record, &record->file.record[record->slot[slot] - 1u], out);
}

void kqr_store_by(KqrRecord *record, const KqrQuery *query, Who who)
{
    if (!record->open)
    {
        return;
    }
    static unsigned long long s_words[KQR_QUERY_WORDS + KQR_WORDS + 1u];
    static KqrQuery s_stored;
    const unsigned int count = kqr_query_words(record, query, s_words);
    const unsigned int slot = kqr_slot_of(record, &query->identity);
    if (record->slot[slot] == 0u)
    {
        kqr_marks_written(record, who);
        const unsigned int at = kqr_put(record, KQR_QUERY_RECORD, s_words, count, who);
        if (at < record->file.records)
        {
            // the marks and the who put before it moved no query: they went where the pairs start, past every query
            record->slot[kqr_slot_of(record, &query->identity)] = at + 1u;
            record->query_count += 1u;
            record->changed = 1;
        }
        return;
    }
    unsigned int at = record->slot[slot] - 1u;
    if (!kqr_query_of(record, &record->file.record[at], &s_stored) ||
        ((s_stored.answer != KQR_TIMED_OUT) && (s_stored.answer != KQR_CENSORED)))
    {
        return;
    }
    // the answer that resolves it, or the part's own word where the gate had gated it, takes its place, after a who
    // record of `who` and before the who in force there again, where that one is another's
    Who there;
    const int there_has_who = krep_who_at(&record->file, at, &there);
    if (!there_has_who || !krep_who_same(there, who))
    {
        if (!kqr_who_put(record, at, 1, who))
        {
            return;
        }
        kqr_shifted(record, at, 1u);
        at += 1u;
        if (((at + 1u) < record->file.records) && kqr_who_put(record, at + 1u, there_has_who, there))
        {
            kqr_shifted(record, at + 1u, 1u);
        }
    }
    if (krep_record_set(&record->file, at, s_words, count))
    {
        record->changed = 1;
    }
}

void kqr_sample_by(KqrRecord *record, const KqrSample *sample, Who who)
{
    if (!record->open)
    {
        return;
    }
    unsigned long long words[KQR_QUERY_WORDS + 3u];
    const unsigned int count = kqr_sample_words(record, sample, words);
    kqr_marks_written(record, who);
    const unsigned int at = kqr_put(record, KQR_SAMPLE_RECORD, words, count, who);
    if ((at < record->file.records) && kqr_sample_take(record, at))
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

int kqr_query_read(const KqrRecord *record, unsigned int at, KqrQuery *out)
{
    unsigned int seen = 0u;
    for (unsigned int place = 0u; record->open && (place < record->first_pair); place += 1u)
    {
        const KrepRecord *const query = &record->file.record[place];
        if (query->code != KQR_QUERY_RECORD)
        {
            continue;
        }
        if (seen == at)
        {
            return kqr_query_of(record, query, out);
        }
        seen += 1u;
    }
    return 0;
}

int kqr_sample_read(const KqrRecord *record, unsigned int at, KqrSample *out)
{
    return (at < record->samples) && kqr_sample_of(record, &record->file.record[record->sample[at]], out);
}

int kqr_path_read(const KqrRecord *record, unsigned int at, KqrPath *out)
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
            memset(out, 0, sizeof(*out));
            kqr_text_copied(record, pair->word[0], out->first, sizeof(out->first));
            kqr_text_copied(record, pair->word[1], out->second, sizeof(out->second));
            out->read = (pair->word[2] != 0ull);
            out->reads = pair->word[3];
            kqr_text_copied(record, pair->word[4], out->verdict, sizeof(out->verdict));
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

void kqr_path_add_by(KqrRecord *record, const KqrPath *path, Who who)
{
    Who last;
    const int last_has_who = krep_who_at(&record->file, record->file.records, &last);
    if ((!last_has_who || !krep_who_same(last, who)) && !kqr_who_put(record, record->file.records, 1, who))
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

// the text kqr_text_sliced writes: its bytes, the room they have, and the offset written to, the room once a write did
// not fit
static char *s_text_out;
static size_t s_text_cap;
static size_t s_text_at;

// `text` written after what the text contains
static void text_put(const char *text)
{
    s_text_at =
        verba_textus.put(&(VerbaTextusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .text = text});
}

// the `length` bytes at `text` written after what the text contains
static void text_counted(const char *text, size_t length)
{
    s_text_at = verba_textus.put_n(
        &(VerbaTextusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .text = text, .text_len = length});
}

// `character` written after what the text contains
static void text_character(char character)
{
    s_text_at =
        verba_littera.ch(&(VerbaLitteraCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .ch = character});
}

// `value` written in base ten after what the text contains
static void text_decimal(unsigned long long value)
{
    s_text_at =
        verba_numerus.u64(&(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .val = value});
}

// `value` written in base sixteen, in `digits_least` digits at the least, after what the text contains
static void text_hex(unsigned long long value, unsigned int digits_least)
{
    s_text_at = verba_numerus.hex(
        &(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .val = value, .min = digits_least});
}

size_t kqr_text_rendered(const KqrRecord *record, char *out, size_t cap, size_t at, int named)
{
    return kqr_text_sliced(record, out, cap, at, named, 0u, 0u);
}

size_t kqr_text_sliced(const KqrRecord *record, char *out, size_t cap, size_t at, int named, unsigned int from,
                       unsigned int count)
{
    static KqrQuery s_query;
    static KqrSample s_sample;
    static KqrPath s_path;
    char identity[128];
    char who_name[2400];
    s_text_out = out;
    s_text_cap = cap;
    s_text_at = at;
    if (from >= record->file.records)
    {
        return s_text_at;
    }
    const unsigned int past =
        ((count == 0u) || (count > (record->file.records - from))) ? record->file.records : (from + count);
    for (unsigned int at = from; at < past; at += 1u)
    {
        const KrepRecord *const entry = &record->file.record[at];
        unsigned long long length = 0ull;
        const char *words = NULL;
        switch (entry->code)
        {
        case KQR_MEMBER_RECORD:
            words = (entry->count != 0u) ? krep_text(&record->file, entry->word[0], &length) : NULL;
            text_put("kqr ");
            text_counted((words != NULL) ? words : "", (size_t)length);
            text_character('\n');
            break;
        case KQR_COMMENT_RECORD:
            words = (entry->count != 0u) ? krep_text(&record->file, entry->word[0], &length) : NULL;
            text_counted((words != NULL) ? words : "", (size_t)length);
            text_character('\n');
            break;
        case KQR_CYCLE_RECORD:
            words = (entry->count > 1u) ? krep_text(&record->file, entry->word[1], &length) : NULL;
            text_put("cycle ");
            text_decimal(entry->word[0]);
            text_character(' ');
            text_counted((words != NULL) ? words : "", (size_t)length);
            text_character('\n');
            break;
        case KQR_SEED_RECORD:
            text_put("seed ");
            text_decimal(entry->word[0]);
            text_character('\n');
            break;
        case KQR_ROUND_RECORD:
            text_put("round ");
            text_decimal(entry->word[0]);
            text_character('\n');
            break;
        case KQR_QUERY_RECORD:
            kqr_query_of(record, entry, &s_query);
            kqr_identity_text(&s_query.identity, identity, sizeof(identity));
            text_put("query ");
            text_put(identity);
            text_character(' ');
            text_put(kqr_answer_name(s_query.answer));
            for (unsigned int place = 0u; (s_query.answer == KQR_ANSWERS) && (place < s_query.words); place += 1u)
            {
                text_character(' ');
                text_hex(s_query.word[place], 0u);
            }
            text_put((s_query.refusal[0] != '\0') ? " " : "");
            text_put(s_query.refusal);
            text_character('\n');
            break;
        case KQR_SAMPLE_RECORD:
            kqr_sample_of(record, entry, &s_sample);
            kqr_identity_text(&s_sample.identity, identity, sizeof(identity));
            text_put("sample ");
            text_put(identity);
            text_character(' ');
            text_put(kqr_answer_name(s_sample.answer));
            if (s_sample.answer == KQR_ANSWERS)
            {
                text_character(' ');
                text_decimal(s_sample.nanoseconds);
            }
            if ((s_sample.answer == KQR_ANSWERS) && s_sample.launched)
            {
                text_character(' ');
                text_decimal(s_sample.least);
                text_character(' ');
                text_decimal(s_sample.most);
            }
            if ((s_sample.answer != KQR_ANSWERS) && (s_sample.refusal[0] != '\0'))
            {
                text_character(' ');
                text_put(s_sample.refusal);
            }
            text_character('\n');
            break;
        case KQR_PAIR_RECORD:
            memset(&s_path, 0, sizeof(s_path));
            kqr_text_copied(record, entry->word[0], s_path.first, sizeof(s_path.first));
            kqr_text_copied(record, entry->word[1], s_path.second, sizeof(s_path.second));
            kqr_text_copied(record, entry->word[4], s_path.verdict, sizeof(s_path.verdict));
            if (entry->word[2] != 0ull)
            {
                text_put("pair ");
                text_put(s_path.first);
                text_character(' ');
                text_put(s_path.second);
                text_character(' ');
                text_hex(entry->word[3], 16u);
                text_character(' ');
                text_put(s_path.verdict);
                text_character('\n');
            }
            else
            {
                text_put("pair ");
                text_put(s_path.first);
                text_character(' ');
                text_put(s_path.second);
                text_character(' ');
                text_put(s_path.verdict);
                text_character('\n');
            }
            break;
        case KREP_RECORD_WHO:
            if (entry->count != 2u)
            {
                text_put("who -\n");
                break;
            }
            {
                char code[WHO_CODE_LETTERS + 1u];
                who_code(who_of_words(entry->word), code, sizeof(code));
                if (named && who_named(who_of_words(entry->word), who_name, sizeof(who_name)))
                {
                    text_put("who ");
                    text_put(code);
                    text_character(' ');
                    text_put(who_name);
                    text_character('\n');
                    break;
                }
                text_put("who ");
                text_put(code);
                text_character('\n');
                break;
            }
        default:
            text_put("record ");
            text_hex(entry->code, 0u);
            text_put(" of ");
            text_decimal(entry->count);
            text_put(" words\n");
            break;
        }
    }
    return s_text_at;
}
