// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// queries.cu: the queries: the trace and the log of the thread that carries them, each question's identity, and a round
// answered from the record or carried
#include "identity_internal.h"

// The trace of every query, in the query's .qry (qry_member.h): the query's number, the code's size,
// the registers, the shape and the launches it is put with and its count of cases, then what came back: alike, apart
// with the first case apart and both answers, the carrier's refusal, or nothing. NULL where nothing is traced
thread_local QueryLog *s_trace_log = NULL;

// The log, in the query's .qry beside the trace, that orior_klq_decoder reads: the cases as `cases <case>...`, written
// again where a query is put over other cases, and each query as `query <number>` and what came back of every case in
// the order of the cases, `=` alike, `x` apart and `-` where the host gives no bracket, or `recorded` or `refused`
// where the part answered it nothing here. A mode writes what it put after the query, and every choice a pair's put
// makes at a link as `choice <flags> <link>`, the flags of the trace's table ORed into one word, `-` for the link where
// the choice is the put's own (src/cu/includes/file_defs/klq/query_trace.tsv). NULL where nothing is traced. The trace
// and the log of the thread that carries the queries are the two below; a pair put in a thread of its own writes its
// trace and its log into logs of its own, each handed whole once the pair is put, so that every put stands in one piece
thread_local QueryLog *s_query_log = NULL;

QueryLog s_trace_whole = {"trace", std::string()};

QueryLog s_log_whole = {"log", std::string()};

// the index of the pair this thread puts, and -1 on the thread that carries the queries, and 1 where the stand-in the
// pair puts now is dangerous
thread_local int t_pair = -1;

thread_local int t_dangerous = 0;

// `format` and what follows it added to the end of `log`'s bytes, as fprintf writes them
void query_log_print(QueryLog *log, const char *format, ...)
{
    va_list values;
    va_start(values, format);
    va_list measured;
    va_copy(measured, values);
    const int length = vsnprintf(NULL, 0u, format, measured);
    va_end(measured);
    if (length > 0)
    {
        const size_t before = log->bytes.size();
        log->bytes.resize(before + (size_t)length + 1u);
        vsnprintf(&log->bytes[before], (size_t)length + 1u, format, values);
        log->bytes.resize(before + (size_t)length);
    }
    va_end(values);
}

// the run the trace and the log are handed to, NULL while nothing is traced
QryShared *s_query_run = NULL;

// `log`'s bytes handed to the run as an ordinary blob of its name, and let go
void query_log_handed(QueryLog *log)
{
    if ((s_query_run != NULL) && !log->bytes.empty())
    {
        qry_member_append(log->name, log->bytes.data(), log->bytes.size(), QRY_ORDINARY);
    }
    log->bytes.clear();
}

// what the thread that carries the queries wrote and has not handed, handed as the run ends
static void query_logs_closed(void)
{
    query_log_handed(&s_trace_whole);
    query_log_handed(&s_log_whole);
}

// the trace and the log of the thread that carries the queries opened into the run, the .qry QRY names
// (qry_member.h), as every path's trace is, and handed the rest as the run ends
void query_logs_opened(void)
{
    if (s_query_run != NULL)
    {
        return;
    }
    s_query_run = qry_member_run();
    if (s_query_run == NULL)
    {
        printf("orior_klq_identity: no query writer runs to take the trace: %s names no .qry a writer has open\n",
               QRY_ENVIRONMENT);
        return;
    }
    s_trace_log = &s_trace_whole;
    s_query_log = &s_log_whole;
    atexit(query_logs_closed);
}

// `log` handed to the run where every query is to reach it as it is written: on the thread that carries the
// queries, and not from a pair's own logs, which are handed whole once the pair is put
void query_log_flushed(QueryLog *log)
{
    if (t_pair < 0)
    {
        query_log_handed(log);
    }
}

std::string case_text(unsigned int at);

// the host's place of the first case the last query answered apart at, from the part or from the record
thread_local unsigned int s_apart_at = 0xffffffffu;

// The round: each pair put in a thread of its own runs while it is its turn, one at a time in the order of the bridge,
// until it queries or is put. Every query a pair hands over waits for the round, and once every pair left has queried,
// the round's queries are carried together: the mundane in one process, each dangerous one in a process of its own. No
// query waits on another query of the same round, since a pair queries its next only once its last is answered.
// `s_turn` is the pair that runs, and -1 the thread that carries the queries
std::mutex s_turn_lock;

std::condition_variable s_turn_changed;

int s_turn = -1;

std::vector<std::vector<CarriedQuery>> s_round_queries;

// what the rounds queried: the queries handed over, those answered from what was stored, and the processes that carried
// the rest
unsigned long long s_round_handed = 0ull;

unsigned long long s_round_recalled = 0ull;

unsigned long long s_round_processes = 0ull;

// a question's identity, seven numbers: a hash of its code, its registers, its shape and launches, its count of cases
// and a hash of its cases
static KqrIdentity question_numbered(const RunQuestion *question)
{
    KqrIdentity identity = {0xcbf29ce484222325ull, question->registers, question->threads,    question->blocks,
                            question->launches,    question->cases,     0xcbf29ce484222325ull};
    for (unsigned long long at = 0ull; at < question->code_size; at += 1ull)
    {
        identity.code = (identity.code ^ question->code[at]) * 0x100000001b3ull;
    }
    const unsigned char *const words = (const unsigned char *)question->word;
    const size_t bytes = (size_t)question->cases * sizeof(question->word[0]);
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        identity.cases_hash = (identity.cases_hash ^ words[at]) * 0x100000001b3ull;
    }
    return identity;
}

// a question's identity written as the query record's text form writes it, for a person to read
std::string question_identity(const RunQuestion *question)
{
    const KqrIdentity identity = question_numbered(question);
    char text[96];
    kqr_identity_text(&identity, text, sizeof(text));
    return text;
}

// `question` answered from `stored`. A question the part answered illegal is censored: the record retains the part's
// answer and its refusal, and the question is blocked from the part from then on, as one the gate censored is
static void answer_given(RunQuestion *question, const KqrQuery &stored)
{
    question->outcome = (stored.answer == KQR_ANSWERS) ? (unsigned int)RUN_ANSWERED
                        : ((stored.answer == KQR_ILLEGAL) || (stored.answer == KQR_CENSORED))
                            ? (unsigned int)RUN_GATED
                            : (unsigned int)RUN_NOTHING;
    snprintf(question->refused, sizeof(question->refused), "%s", stored.refusal);
    for (unsigned int place = 0u; (place < stored.words) && (place < RUN_CASES_MOST); place += 1u)
    {
        question->answered[place] = stored.word[place];
    }
}

// what came back of `question` as the query record records it: KQR_ANSWERS, KQR_ILLEGAL, KQR_NOTHING, KQR_TIMED_OUT
// where the watchdog ended it before it came back, or KQR_CENSORED where the gate blocked it from the part to protect
// the whole; KQR_ANSWER_COUNT where the channel never carried it
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
    return (strstr(question->refused, interface_ending_name(INTERFACE_ENDING_OUT_OF_TIME)) != NULL) ? KQR_TIMED_OUT
                                                                                                    : KQR_NOTHING;
}

// The queries `carried` answered. A timed query is put every time and stored as a sample of its own, with its cost:
// those of no shape of their own together, and alone each dangerous one and each one of a shape of its own. An untimed
// one is answered from the query record where its identity was queried before and did not time out, and the rest are
// carried, one query of an identity the round contains twice: the mundane together, and alone each dangerous one and
// each one of a shape of its own, which a process of many does not take. Each answer the part gave, and each query the
// gate censored, stored in the record after by its identity, an answer in place of the query that timed out
void queries_answered(const std::vector<CarriedQuery> &carried)
{
    std::vector<RunQuestion *> mundane;
    std::vector<RunQuestion *> alone;
    std::vector<RunQuestion *> timed;
    std::vector<RunQuestion *> timed_together;
    std::vector<KqrIdentity> identities(carried.size());
    std::map<KqrIdentity, RunQuestion *, IdentityOrder> first_of;
    std::vector<KqrIdentity> firsts;
    std::map<RunQuestion *, unsigned int> waiting;
    std::vector<std::pair<RunQuestion *, RunQuestion *>> copies;
    // the timed queries the part answered illegal or the gate censored, each with its refusal, censored again unput
    std::map<KqrIdentity, std::string, IdentityOrder> timed_censored;
    static KqrSample s_sample;
    for (unsigned int at = 0u; kqr_sample_read(&s_query_record, at, &s_sample); at += 1u)
    {
        if ((s_sample.answer == KQR_ILLEGAL) || (s_sample.answer == KQR_CENSORED))
        {
            timed_censored[s_sample.identity] = s_sample.refusal;
        }
    }
    for (size_t at = 0u; at < carried.size(); at += 1u)
    {
        RunQuestion *const question = carried[at].question;
        identities[at] = question_numbered(question);
        s_round_handed += 1ull;
        const auto censored = timed_censored.find(identities[at]);
        if ((question->launches != 0u) && (censored != timed_censored.end()))
        {
            question->outcome = RUN_GATED;
            snprintf(question->refused, sizeof(question->refused), "%s", censored->second.c_str());
            s_round_recalled += 1ull;
            continue;
        }
        if (question->launches != 0u)
        {
            const int timed_alone =
                (carried[at].dangerous != 0) || (question->threads != 0u) || (question->blocks != 0u);
            (timed_alone ? timed : timed_together).push_back(question);
            continue;
        }
        static KqrQuery s_found;
        // a query the gate blocked for an unknown key is open as a timed-out one is (run_channel.h, RUN_GATE_KEY_RULE)
        if (kqr_find(&s_query_record, &identities[at], &s_found) && (s_found.answer != KQR_TIMED_OUT) &&
            !((s_found.answer == KQR_CENSORED) && run_channel_key_gated(s_found.refusal)))
        {
            answer_given(question, s_found);
            s_round_recalled += 1ull;
            continue;
        }
        const auto first = first_of.find(identities[at]);
        if (first != first_of.end())
        {
            copies.push_back(std::make_pair(question, first->second));
            waiting[first->second] += 1u;
            continue;
        }
        first_of[identities[at]] = question;
        firsts.push_back(identities[at]);
        const int shaped = (question->threads != 0u) || (question->blocks != 0u);
        ((carried[at].dangerous != 0) || shaped ? alone : mundane).push_back(question);
        waiting[question] = 1u;
    }
    // the query that settles the most entries first: the one the most pairs of the round wait on, in the order handed
    // where two settle as many
    const auto settles_more = [&waiting](RunQuestion *left, RunQuestion *right) {
        return waiting[left] > waiting[right];
    };
    std::stable_sort(mundane.begin(), mundane.end(), settles_more);
    std::stable_sort(alone.begin(), alone.end(), settles_more);
    if (!mundane.empty())
    {
        run_channel_put_many(mundane.data(), (unsigned int)mundane.size());
        s_round_processes += 1ull;
    }
    for (RunQuestion *const question : alone)
    {
        if ((question->threads != 0u) || (question->blocks != 0u))
        {
            run_channel_put(question);
        }
        else
        {
            RunQuestion *const one[1] = {question};
            run_channel_put_many(one, 1u);
        }
        s_round_processes += 1ull;
    }
    // timed queries of no shape of their own carried together, each timed over its own launches, and each other one
    // alone
    if (!timed_together.empty())
    {
        run_channel_put_many(timed_together.data(), (unsigned int)timed_together.size());
        s_round_processes += 1ull;
    }
    for (RunQuestion *const question : timed)
    {
        run_channel_put(question);
        s_round_processes += 1ull;
    }
    timed.insert(timed.end(), timed_together.begin(), timed_together.end());
    for (RunQuestion *const question : timed)
    {
        const unsigned int answer = answer_recorded(question);
        if (answer != KQR_ANSWER_COUNT)
        {
            memset(&s_sample, 0, sizeof(s_sample));
            s_sample.identity = question_numbered(question);
            s_sample.answer = answer;
            s_sample.nanoseconds = question->nanoseconds;
            s_sample.launched = (question->most != 0ull) ? 1 : 0;
            s_sample.least = question->least;
            s_sample.most = question->most;
            snprintf(s_sample.refusal, sizeof(s_sample.refusal), "%s",
                     (answer == KQR_ANSWERS) ? "" : question->refused);
            kqr_sample(&s_query_record, &s_sample);
        }
    }
    static KqrQuery s_stored;
    for (const KqrIdentity &identity : firsts)
    {
        RunQuestion *const question = first_of[identity];
        const unsigned int answer = answer_recorded(question);
        if (answer == KQR_ANSWER_COUNT)
        {
            continue;
        }
        memset(&s_stored, 0, sizeof(s_stored));
        s_stored.identity = identity;
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
            snprintf(s_stored.refusal, sizeof(s_stored.refusal), "%s", question->refused);
        }
        kqr_store(&s_query_record, &s_stored);
    }
    for (const auto &copy : copies)
    {
        copy.first->outcome = copy.second->outcome;
        snprintf(copy.first->refused, sizeof(copy.first->refused), "%s", copy.second->refused);
        memcpy(copy.first->answered, copy.second->answered,
               (size_t)copy.first->cases * sizeof(copy.first->answered[0]));
    }
}

// The queries `questions`, `count` of them, each dangerous where `dangerous` specifies, answered through the query
// record: on a pair's own thread handed to the round and waited on, and on the thread that carries the queries answered
// at once. A timed query, the only one a mode means to query again, is put every time
void queries_carried(RunQuestion *const *questions, const int *dangerous, unsigned int count)
{
    std::vector<CarriedQuery> carried;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        carried.push_back(CarriedQuery{questions[at], (dangerous != NULL) ? dangerous[at] : 0});
    }
    if (t_pair < 0)
    {
        queries_answered(carried);
        return;
    }
    std::unique_lock<std::mutex> lock(s_turn_lock);
    s_round_queries[(size_t)t_pair] = carried;
    s_turn = -1;
    s_turn_changed.notify_all();
    const int pair = t_pair;
    s_turn_changed.wait(lock, [pair]() { return s_turn == pair; });
}

// The host's answer to a case as a bracket [down, up] around the exact value at the result's width, and whether the
// part's answer `answered` falls in it: 1 where it does, 0 where it does not, and -1 where the host gives no bracket.
// An integer case is a bracket of one word, `down`, and is true where the part answers that word. A floating case is a
// bracket of two ends, `down:up`, the two values of the result's type nearest the exact one, and is true where the part
// answers either end, the end it takes being its rounding. A case the host's C leaves undefined is `-` and gates
// nothing
int bracket_contains(const std::string &host, unsigned long long answered)
{
    if (host == "-")
    {
        return -1;
    }
    const size_t colon = host.find(':');
    const unsigned long long down = std::stoull(host.substr(0u, colon), nullptr, 16);
    const unsigned long long up =
        (colon == std::string::npos) ? down : std::stoull(host.substr(colon + 1u), nullptr, 16);
    return ((answered == down) || (answered == up)) ? 1 : 0;
}
