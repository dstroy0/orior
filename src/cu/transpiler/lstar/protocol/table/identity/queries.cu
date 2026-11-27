// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// queries.cu: the queries: the trace and the log of the thread that carries them, each question's identity, and a round
// answered from the record or carried
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "identity_internal.h"
#include "memoria_operor/memoria_operor.h"

// The trace of every query, in the query's .qry (qry_member.h). The query's number, the code's size, the registers, the
// shape and the launches it goes with, and its count of cases. Then what came back: alike, apart with the first case
// apart and both answers, the carrier's refusal, or nothing. NULL where the run traces nothing
thread_local QueryLog *s_trace_log = NULL;

// The log, in the query's .qry beside the trace, that orior_klq_decoder reads. It writes the cases as `cases
// <case>...`, again where a query goes over other cases. It writes each query as `query <number>` and what came back
// of every case in the order of the cases: `=` alike, `x` apart and `-` where the host gives no bracket. It writes
// `recorded` or `refused` where the part answered it nothing here. A mode writes what it put after the query.
//
// It writes every choice a pair's put makes at a link as `choice <flags> <link>`. The flags of the trace's table ORed
// into one word, and `-` for the link where the choice is the put's own (`includes/file_defs/klq/query_trace.tsv`).
// NULL where the run traces nothing. The two below are the trace and the log of the thread that carries the queries. A
// pair put in a thread of its own writes its trace and its log into logs of its own. Each passes whole once the pair's
// put ends, so that every put is in one piece
thread_local QueryLog *s_query_log = NULL;

QueryLog s_trace_whole = {"trace"};

QueryLog s_log_whole = {"log"};

// the index of the pair this thread puts, and -1 on the thread that carries the queries. Then 1 where the stand-in the
// pair puts now is dangerous
thread_local int t_pair = -1;

thread_local int t_dangerous = 0;

// the run the trace and the log pass to, NULL while the run traces nothing
QryShared *s_query_run = NULL;

// `log`'s bytes passed to the run as an ordinary blob of its name, and let go
void query_log_passed(QueryLog *log)
{
    if (log->begun == 0)
    {
        return;
    }
    const unsigned long long size = platform_text_size(&log->bytes);
    if ((s_query_run != NULL) && (size != 0ULL))
    {
        const char *const bytes = platform_text_bytes(&log->bytes);
        if (bytes != NULL)
        {
            qry_member_append(log->name, bytes, size, QRY_ORDINARY);
        }
    }
    platform_text_reset(&log->bytes);
}

// what the thread that carries the queries wrote and has not passed, passed as the run ends
static void query_logs_closed(void)
{
    query_log_passed(&s_trace_whole);
    query_log_passed(&s_log_whole);
}

// the trace and the log of the thread that carries the queries opened into the run, the .qry QRY names
// (`qry_member.h`), as every path's trace is. Each passes the rest as the run ends
void query_logs_opened(void)
{
    if (s_query_run != NULL)
    {
        return;
    }
    s_query_run = qry_member_run();
    if (s_query_run == NULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity: no query writer runs to take the trace: ");
        mmgr_line_put(output_line, QRY_ENVIRONMENT);
        mmgr_line_put(output_line, " names no .qry a writer has open");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return;
    }
    s_trace_log = &s_trace_whole;
    s_query_log = &s_log_whole;
    (void)platform_at_close(query_logs_closed);
}

// `log` passed to the run where every query is to reach it as the query writes it. That is on the thread that carries
// the queries, and not from a pair's own logs. Those pass whole once the pair's put ends
void query_log_flushed(QueryLog *log)
{
    if (t_pair < 0)
    {
        query_log_passed(log);
    }
}

// the host's place of the first case the last query answered apart at, from the part or from the record
thread_local unsigned int s_apart_at = 0xffffffffu;

// The round. Each pair put in a thread of its own runs while it is its turn, one at a time in the order of the bridge.
// It runs until it queries or its put ends. Every query a pair passes over waits for the round. Once every pair left
// queried, the round's queries go together: the mundane in one process, each dangerous one in a process of its own.
// No query waits on another query of the same round, since a pair queries its next only once the part answers its last
embed_atomic_word s_turn = EMBED_ATOMIC_INIT(0u);

PlatformArray s_round_queries;

// what the rounds queried: the queries passed over, those answered from what the record stored, and the processes that
// carried the rest
unsigned long long s_round_passed = 0ULL;

unsigned long long s_round_recalled = 0ULL;

unsigned long long s_round_processes = 0ULL;

// a question's identity, seven numbers. A hash of its code, its registers, its shape and launches, its count of cases
// and a hash of its cases
static KqrIdentity question_numbered(const RunQuestion *question)
{
    KqrIdentity identity = {0xcbf29ce484222325ULL, question->registers, question->threads,    question->blocks,
                            question->launches,    question->cases,     0xcbf29ce484222325ULL};
    for (unsigned long long at = 0ULL; at < question->code_size; at += 1ULL)
    {
        identity.code = (identity.code ^ question->code[at]) * 0x100000001b3ULL;
    }
    const unsigned char *const words = (const unsigned char *)question->word;
    const size_t bytes = (size_t)question->cases * sizeof(question->word[0]);
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        identity.cases_hash = (identity.cases_hash ^ words[at]) * 0x100000001b3ULL;
    }
    return identity;
}

void question_identity(const RunQuestion *question, char *text, size_t capacity)
{
    const KqrIdentity identity = question_numbered(question);
    kqr_identity_text(&identity, text, capacity);
}

// `question` answered from `stored`. The record censors a question the part answered illegal. It retains the part's
// answer and its refusal, and blocks the question from the part from then on, as it blocks one the gate censored
static void answer_given(RunQuestion *question, const KqrQuery *stored)
{
    question->outcome = (stored->answer == KQR_ANSWERS) ? (unsigned int)RUN_ANSWERED
                        : ((stored->answer == KQR_ILLEGAL) || (stored->answer == KQR_CENSORED))
                            ? (unsigned int)RUN_GATED
                            : (unsigned int)RUN_NOTHING;
    VerbaLinea buffer_line = mmgr_line_over(question->refused, sizeof(question->refused));
    mmgr_line_put(&buffer_line, stored->refusal);
    (void)mmgr_line_finish(&buffer_line);
    for (unsigned int place = 0u; (place < stored->words) && (place < RUN_CASES_MAX); place += 1u)
    {
        question->answered[place] = stored->word[place];
    }
}

// What came back of `question` as the query record records it. KQR_ANSWERS, KQR_ILLEGAL, KQR_NOTHING, or KQR_TIMED_OUT
// where the watchdog ended it before it came back. KQR_CENSORED where the gate blocked it from the part to protect the
// whole. KQR_ANSWER_COUNT where the channel never carried it
static unsigned int answer_recorded(const RunQuestion *question)
{
    if (question->outcome == RUN_ANSWERED)
    {
        return KQR_ANSWERS;
    }
    if (question->outcome == RUN_ILLEGAL)
    {
        return KQR_ILLEGAL;
    }
    if (question->outcome == RUN_GATED)
    {
        return KQR_CENSORED;
    }
    if (question->outcome != RUN_NOTHING)
    {
        return KQR_ANSWER_COUNT;
    }
    return (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = question->refused, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = interface_ending_name(INTERFACE_ENDING_OUT_OF_TIME),
                       .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL)
               ? KQR_TIMED_OUT
               : KQR_NOTHING;
}

// a query that copies the answer of the first query of its identity in the round
struct QueryCopy
{
    RunQuestion *copy;
    RunQuestion *first;
};

// the first query of one identity in the round, and how many queries of the round wait on its answer
struct QueryFirst
{
    KqrIdentity identity;
    RunQuestion *question;
    unsigned int waiting;
};

// a timed query the record censored or the part answered illegal, and its refusal
struct QueryCensored
{
    KqrIdentity identity;
    char refusal[sizeof(((KqrSample *)0)->refusal)];
};

// The cells of one round's answers, each of the temporary tier at the round's count of queries. The queries put
// together, alone, timed and timed together, the first of each identity, and the copies of a first's answer. Then the
// timed queries censored, at the record's count of samples
struct QueryRound
{
    unsigned long long *mundane;
    unsigned long long mundane_count;
    unsigned long long *alone;
    unsigned long long alone_count;
    RunQuestion **timed;
    unsigned long long timed_count;
    RunQuestion **timed_together;
    unsigned long long timed_together_count;
    QueryFirst *firsts;
    unsigned long long first_count;
    QueryCopy *copies;
    unsigned long long copy_count;
    QueryCensored *censored;
    unsigned long long censored_count;
};

static void query_round_ended(QueryRound *round)
{
    void *const cells[7] = {round->mundane, round->alone,  round->timed,   round->timed_together,
                            round->firsts,  round->copies, round->censored};
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        platform_cell_released(cells[at], platform_place_g_program_owner);
    }
}

// a cell of the temporary tier of `count` elements of `size` bytes, at least one
static void *round_cell(unsigned long long count, unsigned long long size)
{
    return platform_cell_taken(((count != 0ULL) ? count : 1ULL) * size, platform_place_g_program_owner,
                               PLATFORM_TIER_TEMPORARY);
}

// The cells of a round of `count` queries, and the timed queries the record censored ordered by identity: 1, or 0
// where the pool cannot meet a cell
static int query_round_taken(unsigned long long count, QueryRound *round)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = round, .bytes = sizeof(*round), .val = 0);
    static KqrSample s_sample;
    unsigned long long samples = 0ULL;
    while (kqr_sample_read(&s_query_record, (unsigned int)samples, &s_sample))
    {
        samples += 1ULL;
    }
    round->mundane = (unsigned long long *)round_cell(count, sizeof(unsigned long long));
    round->alone = (unsigned long long *)round_cell(count, sizeof(unsigned long long));
    round->timed = (RunQuestion **)round_cell(count, sizeof(RunQuestion *));
    round->timed_together = (RunQuestion **)round_cell(count, sizeof(RunQuestion *));
    round->firsts = (QueryFirst *)round_cell(count, sizeof(QueryFirst));
    round->copies = (QueryCopy *)round_cell(count, sizeof(QueryCopy));
    round->censored = (QueryCensored *)round_cell(samples, sizeof(QueryCensored));
    if ((round->mundane == NULL) || (round->alone == NULL) || (round->timed == NULL) ||
        (round->timed_together == NULL) || (round->firsts == NULL) || (round->copies == NULL) ||
        (round->censored == NULL))
    {
        query_round_ended(round);
        return 0;
    }
    for (unsigned long long at = 0ULL; at < samples; at += 1ULL)
    {
        // the record's samples are far below 2^32
        (void)kqr_sample_read(&s_query_record, (unsigned int)at, &s_sample);
        if ((s_sample.answer != KQR_ILLEGAL) && (s_sample.answer != KQR_CENSORED))
        {
            continue;
        }
        QueryCensored *const kept = &round->censored[round->censored_count];
        kept->identity = s_sample.identity;
        (void)identity_name_set(kept->refusal, sizeof(kept->refusal), s_sample.refusal, PLATFORM_TEXT_TO_TERMINATOR);
        round->censored_count += 1ULL;
    }
    // an identity the record gives twice keeps its last refusal
    (void)identity_sorted(round->censored, round->censored_count,
                          [](const QueryCensored &one, const QueryCensored &other) {
                              return identity_order(one.identity, other.identity) < 0;
                          });
    unsigned long long kept_count = 0ULL;
    for (unsigned long long at = 0ULL; at < round->censored_count; at += 1ULL)
    {
        if ((kept_count != 0ULL) &&
            (identity_order(round->censored[kept_count - 1ULL].identity, round->censored[at].identity) == 0))
        {
            round->censored[kept_count - 1ULL] = round->censored[at];
            continue;
        }
        round->censored[kept_count] = round->censored[at];
        kept_count += 1ULL;
    }
    round->censored_count = kept_count;
    return 1;
}

// the refusal the record censored the timed query `identity` with, or NULL where it censored none
static const char *query_censored(const QueryRound *round, const KqrIdentity &identity)
{
    const unsigned long long at =
        identity_lower(round->censored, round->censored_count,
                       [&identity](const QueryCensored &one) { return identity_order(one.identity, identity) < 0; });
    return ((at < round->censored_count) && (identity_order(round->censored[at].identity, identity) == 0))
               ? round->censored[at].refusal
               : NULL;
}

// the first query of `identity` in the round, or NULL where the round holds none yet
static QueryFirst *query_first(QueryRound *round, const KqrIdentity &identity)
{
    for (unsigned long long at = 0ULL; at < round->first_count; at += 1ULL)
    {
        if (identity_order(round->firsts[at].identity, identity) == 0)
        {
            return &round->firsts[at];
        }
    }
    return NULL;
}

// The queries `carried` answered. A timed query goes every time, and the record stores it as a sample of its own, with
// its cost. Those of no shape of their own go together, and alone each dangerous one and each one of a shape of its
// own. The query record answers an untimed one where a query of its identity went before and did not time out. The
// channel carries the rest, one query of an identity the round contains twice. The mundane go together, and alone
// each dangerous one and each of a shape of its own, which a process of many does not take.
//
// Each answer the part gave, and each query the gate censored, goes in the record after by its identity. An answer
// goes in place of the query that timed out
void queries_answered(const CarriedQueries *carried)
{
    QueryRound round;
    if (!query_round_taken(carried->count, &round))
    {
        identity_path_said("orior_klq_identity", ": the pool cannot meet the round's cells");
        return;
    }
    for (unsigned long long at = 0ULL; at < carried->count; at += 1ULL)
    {
        const CarriedQuery query = *(const CarriedQuery *)platform_array_at(carried, at);
        RunQuestion *const question = query.question;
        const KqrIdentity identity = question_numbered(question);
        s_round_passed += 1ULL;
        const char *const censored = query_censored(&round, identity);
        if ((question->launches != 0u) && (censored != NULL))
        {
            question->outcome = RUN_GATED;
            VerbaLinea buffer_line = mmgr_line_over(question->refused, sizeof(question->refused));
            mmgr_line_put(&buffer_line, censored);
            (void)mmgr_line_finish(&buffer_line);
            s_round_recalled += 1ULL;
            continue;
        }
        if (question->launches != 0u)
        {
            const int timed_alone = (query.dangerous != 0) || (question->threads != 0u) || (question->blocks != 0u);
            if (timed_alone)
            {
                round.timed[round.timed_count] = question;
                round.timed_count += 1ULL;
            }
            else
            {
                round.timed_together[round.timed_together_count] = question;
                round.timed_together_count += 1ULL;
            }
            continue;
        }
        static KqrQuery s_found;
        // a query the gate blocked for an unknown key is open as a timed-out one is (run_channel.h, RUN_GATE_KEY_RULE)
        if (kqr_find(&s_query_record, &identity, &s_found) && (s_found.answer != KQR_TIMED_OUT) &&
            !((s_found.answer == KQR_CENSORED) && run_channel_key_gated(s_found.refusal)))
        {
            answer_given(question, &s_found);
            s_round_recalled += 1ULL;
            continue;
        }
        QueryFirst *const first = query_first(&round, identity);
        if (first != NULL)
        {
            const QueryCopy copy = {question, first->question};
            round.copies[round.copy_count] = copy;
            round.copy_count += 1ULL;
            first->waiting += 1u;
            continue;
        }
        const QueryFirst added = {identity, question, 1u};
        round.firsts[round.first_count] = added;
        const int shaped = (question->threads != 0u) || (question->blocks != 0u);
        if ((query.dangerous != 0) || shaped)
        {
            round.alone[round.alone_count] = round.first_count;
            round.alone_count += 1ULL;
        }
        else
        {
            round.mundane[round.mundane_count] = round.first_count;
            round.mundane_count += 1ULL;
        }
        round.first_count += 1ULL;
    }
    // the query that settles the max entries first. The one the max pairs of the round wait on, in the order passed
    // where two settle as many
    const QueryFirst *const firsts = round.firsts;
    const auto settles_more = [firsts](unsigned long long left, unsigned long long right) {
        return firsts[left].waiting > firsts[right].waiting;
    };
    (void)identity_sorted(round.mundane, round.mundane_count, settles_more);
    (void)identity_sorted(round.alone, round.alone_count, settles_more);
    if (round.mundane_count != 0ULL)
    {
        // the mundane queries put together, in the order sorted, their questions in the cell of the timed ones' room
        RunQuestion **const together = (RunQuestion **)round_cell(round.mundane_count, sizeof(RunQuestion *));
        for (unsigned long long at = 0ULL; (together != NULL) && (at < round.mundane_count); at += 1ULL)
        {
            together[at] = round.firsts[round.mundane[at]].question;
        }
        if (together != NULL)
        {
            // the queries of one round are far below 2^32
            run_channel_put_many(together, (unsigned int)round.mundane_count);
            s_round_processes += 1ULL;
        }
        platform_cell_released(together, platform_place_g_program_owner);
    }
    for (unsigned long long at = 0ULL; at < round.alone_count; at += 1ULL)
    {
        RunQuestion *const question = round.firsts[round.alone[at]].question;
        if ((question->threads != 0u) || (question->blocks != 0u))
        {
            run_channel_put(question);
        }
        else
        {
            RunQuestion *one[1] = {question};
            run_channel_put_many(one, 1u);
        }
        s_round_processes += 1ULL;
    }
    // timed queries of no shape of their own carried together, each timed over its own launches, and each other one
    // alone
    if (round.timed_together_count != 0ULL)
    {
        // the queries of one round are far below 2^32
        run_channel_put_many(round.timed_together, (unsigned int)round.timed_together_count);
        s_round_processes += 1ULL;
    }
    for (unsigned long long at = 0ULL; at < round.timed_count; at += 1ULL)
    {
        run_channel_put(round.timed[at]);
        s_round_processes += 1ULL;
    }
    static KqrSample s_sample;
    for (unsigned long long at = 0ULL; at < (round.timed_count + round.timed_together_count); at += 1ULL)
    {
        const RunQuestion *const question =
            (at < round.timed_count) ? round.timed[at] : round.timed_together[at - round.timed_count];
        const unsigned int answer = answer_recorded(question);
        if (answer != KQR_ANSWER_COUNT)
        {
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &s_sample, .bytes = sizeof(s_sample), .val = 0);
            s_sample.identity = question_numbered(question);
            s_sample.answer = answer;
            s_sample.nanoseconds = question->nanoseconds;
            s_sample.launched = (question->max != 0ULL) ? 1 : 0;
            s_sample.least = question->least;
            s_sample.max = question->max;
            VerbaLinea buffer_line = mmgr_line_over(s_sample.refusal, sizeof(s_sample.refusal));
            mmgr_line_put(&buffer_line, (answer == KQR_ANSWERS) ? "" : question->refused);
            (void)mmgr_line_finish(&buffer_line);
            kqr_sample(&s_query_record, &s_sample);
        }
    }
    static KqrQuery s_stored;
    for (unsigned long long at = 0ULL; at < round.first_count; at += 1ULL)
    {
        const RunQuestion *const question = round.firsts[at].question;
        const unsigned int answer = (question != NULL) ? answer_recorded(question) : KQR_ANSWER_COUNT;
        if (answer == KQR_ANSWER_COUNT)
        {
            continue;
        }
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &s_stored, .bytes = sizeof(s_stored), .val = 0);
        s_stored.identity = round.firsts[at].identity;
        s_stored.answer = answer;
        if (answer == KQR_ANSWERS)
        {
            for (unsigned int place = 0u; (place < question->cases) && (place < KQR_WORDS); place += 1u)
            {
                s_stored.word[place] = question->answered[place];
                s_stored.words += 1u;
            }
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(s_stored.refusal, sizeof(s_stored.refusal));
            mmgr_line_put(&buffer_line, question->refused);
            (void)mmgr_line_finish(&buffer_line);
        }
        kqr_store(&s_query_record, &s_stored);
    }
    for (unsigned long long at = 0ULL; at < round.copy_count; at += 1ULL)
    {
        const QueryCopy copy = round.copies[at];
        copy.copy->outcome = copy.first->outcome;
        VerbaLinea buffer_line = mmgr_line_over(copy.copy->refused, sizeof(copy.copy->refused));
        mmgr_line_put(&buffer_line, copy.first->refused);
        (void)mmgr_line_finish(&buffer_line);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy.copy->answered, .src = copy.first->answered,
                   .bytes = (size_t)copy.copy->cases * sizeof(copy.copy->answered[0]));
    }
    query_round_ended(&round);
}

// The queries `questions`, `count` of them, each dangerous where `dangerous` specifies, answered through the query
// record. On a pair's own thread they go to the round and the thread waits on them. On the thread that carries the
// queries the call answers them at once. A timed query, the only one a mode means to query again, goes every time
void queries_carried(RunQuestion *const *questions, const int *dangerous, unsigned int count)
{
    if (t_pair < 0)
    {
        CarriedQueries carried;
        platform_array_begun(&carried, sizeof(CarriedQuery), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const CarriedQuery query = {questions[at], (dangerous != NULL) ? dangerous[at] : 0};
            (void)platform_array_appended(&carried, &query);
        }
        queries_answered(&carried);
        platform_array_ended(&carried);
        return;
    }
    // the pair's own list of the round, which the thread that carries the queries reads while this thread waits
    CarriedQueries *const round = (CarriedQueries *)platform_array_at(&s_round_queries, (unsigned long long)t_pair);
    (void)platform_array_counted(round, 0ULL);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CarriedQuery query = {questions[at], (dangerous != NULL) ? dangerous[at] : 0};
        (void)platform_array_appended(round, &query);
    }
    const int pair = t_pair;
    identity_turn_given(-1);
    identity_turn_awaited(pair);
}

// The host's answer to a case as a bracket `[down, up]` around the exact value at the result's width, and whether the
// part's answer `answered` falls in it. 1 where it does, 0 where it does not, and -1 where the host gives no bracket.
// An integer case is a bracket of one word, `down`, and is true where the part answers that word. A floating case is a
// bracket of two ends, `down:up`, the two values of the result's type nearest the exact one. It is true where the part
// answers either end, and the end it takes is its rounding. A case the host's C leaves undefined is `-` and gates
// nothing
int bracket_contains(const char *host, unsigned long long answered)
{
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = host, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "-",
                   .ci = EMBED_FALSE))
    {
        return -1;
    }
    const char *const colon =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = host, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ':');
    char down_text[64] = {0};
    // the colon lies at or past the host's start
    const size_t down_length =
        (colon != NULL) ? (size_t)(colon - host) : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = host, .cap = 63u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = down_text, .src = host, .bytes = (down_length < 63u) ? down_length : 63u);
    const unsigned long long down =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = down_text, .end = NULL, .base = 16);
    const unsigned long long up =
        (colon == NULL) ? down
                        : EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = colon + 1, .end = NULL, .base = 16);
    return ((answered == down) || (answered == up)) ? 1 : 0;
}
