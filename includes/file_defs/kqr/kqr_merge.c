// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr_merge.c: a member's record put together with the records the runs given contain, each a view of its run's table
// of queries (kqr.h, krep_viewed_at). Every query a run's record contains whose identity the record does not, or
// contains timed out where the run's resolved, and every sample a run's record contains that the record contains no
// sample of, the same identity and nanoseconds, is stored into the record in the order of the runs, and the record is
// passed to the run's writer, which writes it in its place as the run ends (kqr_write_by). The pairs of the record stay
// as they are. What each run adds is passed to the run as the record `kqr_merge`.
//
//     kqr_merge <member> <record.kqr> <run.qry>...
//
// Exit 0 where every run's record was read and the record passed; 1 where one was not, or the record was not passed,
// no run's writer running; 2 where the words are not these
#include "kqr_record.h"

#include "../../parsers/who.h"
#include "verba_scribo/verba_scribo.h"

static KqrRecord s_record;
static KqrRecord s_run;
static KqrQuery s_query;
static KqrQuery s_stored;
static KqrSample s_sample;
static KqrSample s_other;

// 1 where `left` and `right` are one identity
static int identity_same(const KqrIdentity *left, const KqrIdentity *right)
{
    return (left->code == right->code) && (left->registers == right->registers) && (left->threads == right->threads) &&
           (left->blocks == right->blocks) && (left->launches == right->launches) && (left->cases == right->cases) &&
           (left->cases_hash == right->cases_hash);
}

// 1 where the record contains no query of `query`'s identity, or contains one that timed out where `query` resolved
static int query_new(const KqrQuery *query)
{
    if (!kqr_find(&s_record, &query->identity, &s_stored))
    {
        return 1;
    }
    return (s_stored.answer == KQR_TIMED_OUT) && (query->answer != KQR_TIMED_OUT);
}

// 1 where the record contains no sample of `sample`'s identity and nanoseconds
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

// the program's report, gathered and passed to the run as the record `kqr_merge` once it is written, `s_report_at`
// bytes so far, the room once a write did not fit
static char s_report[2048];
static size_t s_report_at;

// `text` written after what the report contains
static void print_text(const char *text)
{
    s_report_at =
        verba_textus.put(&(VerbaTextusCfg){.out = s_report, .cap = sizeof(s_report), .at = s_report_at, .text = text});
}

// `value` written in base ten after what the report contains
static void report_decimal(unsigned long long value)
{
    s_report_at = verba_numerus.u64(
        &(VerbaNumerusCfg){.out = s_report, .cap = sizeof(s_report), .at = s_report_at, .val = value});
}

// what the report contains passed to the run, and emptied
static void merge_report_flush(void)
{
    if (verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_report), .at = s_report_at}))
    {
        qry_member_append("kqr_merge", s_report, s_report_at, QRY_ORDINARY);
    }
    s_report_at = 0u;
}

int main(int count, char **word)
{
    if (count < 4)
    {
        print_text("kqr_merge <member> <record.kqr> <run.qry>...\n");
        merge_report_flush();
        return 2;
    }
    char error[512] = "";
    if (!kqr_read(&s_record, word[2], word[1], error, sizeof(error)))
    {
        print_text("kqr_merge: ");
        print_text(word[2]);
        print_text(" did not read: ");
        print_text(error);
        print_text("\n");
        merge_report_flush();
        return 1;
    }
    const Who who = WHO_HERE;
    int read = 1;
    for (int given = 3; given < count; given += 1)
    {
        if (!kqr_viewed_at(&s_run, word[given], NULL, word[1], error, sizeof(error)))
        {
            print_text("kqr_merge: the record of ");
            print_text(word[given]);
            print_text(" did not read: ");
            print_text(error);
            print_text("\n");
            merge_report_flush();
            read = 0;
            continue;
        }
        unsigned int queries = 0u;
        unsigned int samples = 0u;
        for (unsigned int at = 0u; kqr_query_read(&s_run, at, &s_query); at += 1u)
        {
            if (query_new(&s_query))
            {
                kqr_store_by(&s_record, &s_query, who);
                queries += 1u;
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
        print_text("kqr_merge: ");
        print_text(word[given]);
        print_text(" adds ");
        report_decimal(queries);
        print_text(" queries and ");
        report_decimal(samples);
        print_text(" samples\n");
        merge_report_flush();
        kqr_free(&s_run);
    }
    const int passed = kqr_write_by(&s_record, word[2], who);
    print_text("kqr_merge: ");
    print_text(word[2]);
    print_text(passed ? " passed to the run's writer, written in its place as the run ends\n"
                      : " was not passed: no run's writer runs, or the path is outside the tree\n");
    merge_report_flush();
    kqr_free(&s_record);
    return (read && passed) ? 0 : 1;
}
