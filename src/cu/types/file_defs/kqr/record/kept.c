// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept.c: the record kept: tabled and written by a who, freed, an ask found and kept, a sample kept, a mark made, an ask, a sample and a path read, the paths cleared and added, and the text form rendered and sliced
#include "kqr_internal.h"

int kqr_tabled_by(KqrRecord *record, QryBuffer *buffer, const char *path, QryWho who)
{
    return record->open && krep_tabled_by(&record->file, buffer, KQR_TABLE, path, who);
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
    if (!kqr_ask_of(record, &record->file.record[at], &s_held) ||
        ((s_held.answer != KQR_TIMED_OUT) && (s_held.answer != KQR_CENSORED)))
    {
        return;
    }
    // the answer that resolves it, or the part's own word where the gate had held it, takes its place, after a who
    // record of `who` and before the who in force there again, where that one is another's
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
    if (krep_record_set(&record->file, at, s_words, count))
    {
        record->changed = 1;
    }
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

int kqr_ask_read(const KqrRecord *record, unsigned int at, KqrAsk *held)
{
    unsigned int seen = 0u;
    for (unsigned int place = 0u; record->open && (place < record->first_pair); place += 1u)
    {
        const KrepRecord *const ask = &record->file.record[place];
        if (ask->code != KQR_ASK_RECORD)
        {
            continue;
        }
        if (seen == at)
        {
            return kqr_ask_of(record, ask, held);
        }
        seen += 1u;
    }
    return 0;
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
