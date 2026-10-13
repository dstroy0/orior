// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr_merge.c: a member's record put together with the records the runs given hold, each a view of its run's table of
// asks (kqr.h, krep_viewed_at). Every ask a run's record holds whose identity the record does not, or holds timed out
// where the run's resolved, and every sample a run's record holds that the record holds no sample of, the same
// identity and nanoseconds, is kept into the record in the order of the runs, and the record is handed to the run's
// writer, which writes it in its place as the run ends (kqr_write_by). The pairs of the record stay as they are. What
// each run adds is said.
//
//     kqr_merge <member> <record.kqr> <run.qry>...
//
// Exit 0 where every run's record was read and the record handed; 1 where one was not, or the record was not handed,
// no run's writer running; 2 where the words are not these
#include "kqr.h"

#include <stdio.h>

static KqrRecord s_record;
static KqrRecord s_run;
static KqrAsk s_ask;
static KqrAsk s_held;
static KqrSample s_sample;
static KqrSample s_other;

// 1 where `left` and `right` are one identity
static int identity_same(const KqrIdentity *left, const KqrIdentity *right)
{
    return (left->code == right->code) && (left->registers == right->registers) && (left->threads == right->threads) &&
           (left->blocks == right->blocks) && (left->launches == right->launches) && (left->cases == right->cases) &&
           (left->cases_hash == right->cases_hash);
}

// 1 where the record holds no ask of `ask`'s identity, or holds one that timed out where `ask` resolved
static int ask_new(const KqrAsk *ask)
{
    if (!kqr_find(&s_record, &ask->identity, &s_held))
    {
        return 1;
    }
    return (s_held.answer == KQR_TIMED_OUT) && (ask->answer != KQR_TIMED_OUT);
}

// 1 where the record holds no sample of `sample`'s identity and nanoseconds
static int sample_new(const KqrSample *sample)
{
    for (unsigned int at = 0u; kqr_sample_read(&s_record, at, &s_other); at += 1u)
    {
        if (identity_same(&s_other.identity, &sample->identity) && (s_other.nanoseconds == sample->nanoseconds))
        {
            return 0;
        }
    }
    return 1;
}

int main(int count, char **word)
{
    if (count < 4)
    {
        printf("kqr_merge <member> <record.kqr> <run.qry>...\n");
        return 2;
    }
    char error[512] = "";
    if (!kqr_read(&s_record, word[2], word[1], error, sizeof(error)))
    {
        printf("kqr_merge: %s did not read: %s\n", word[2], error);
        return 1;
    }
    const QryWho who = QRY_WHO_HERE;
    int read = 1;
    for (int given = 3; given < count; given += 1)
    {
        if (!kqr_viewed_at(&s_run, word[given], NULL, word[1], error, sizeof(error)))
        {
            printf("kqr_merge: the record of %s did not read: %s\n", word[given], error);
            read = 0;
            continue;
        }
        unsigned int asks = 0u;
        unsigned int samples = 0u;
        for (unsigned int at = 0u; kqr_ask_read(&s_run, at, &s_ask); at += 1u)
        {
            if (ask_new(&s_ask))
            {
                kqr_keep_by(&s_record, &s_ask, who);
                asks += 1u;
            }
        }
        for (unsigned int at = 0u; kqr_sample_read(&s_run, at, &s_sample); at += 1u)
        {
            if (sample_new(&s_sample))
            {
                kqr_sample_by(&s_record, &s_sample, who);
                samples += 1u;
            }
        }
        printf("kqr_merge: %s adds %u asks and %u samples\n", word[given], asks, samples);
        kqr_free(&s_run);
    }
    const int handed = kqr_write_by(&s_record, word[2], who);
    printf(handed ? "kqr_merge: %s handed to the run's writer, written in its place as the run ends\n"
                  : "kqr_merge: %s was not handed: no run's writer runs, or the path stands outside the tree\n",
           word[2]);
    kqr_free(&s_record);
    return (read && handed) ? 0 : 1;
}
