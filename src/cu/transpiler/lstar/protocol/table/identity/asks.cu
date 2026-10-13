// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// asks.cu: the asks: the trace and the log of the thread that carries them, each question's identity, and a round answered from the record or carried
#include "identity_internal.h"

// The trace of every ask, in the query's .qry unless QRY_TRACE is 0 (qry_buffer.h): the ask's number, the code's size,
// the registers, the shape and the launches it is put with and its count of cases, then what came back: alike, apart
// with the first case apart and both answers, the carrier's refusal, or nothing. NULL where nothing is traced
thread_local AskLog *s_ask_trace = NULL;

// The log, in the query's .qry beside the trace, that klq_decoder reads: the cases as `cases <case>...`, written again
// where an ask is put over other cases, and each ask as `ask <number>` and what came back of every case in the order of
// the cases, `=` alike, `x` apart and `-` where the host gives no bracket, or `recorded` or `refused` where the part
// answered it nothing here. A mode writes what it put after the ask, and every choice a pair's put makes at a link as
// `choice <flags> <link>`, the flags of the trace's table ORed into one word, `-` for the link where the choice is the
// put's own (src/cu/types/file_defs/klq/query_trace.tsv). NULL where nothing is traced. The trace and the log of the
// thread that carries the asks are the two below; a pair put in a thread of its own writes its trace and its log into
// logs of its own, each handed whole once the pair is put, so that every put stands in one piece
thread_local AskLog *s_ask_log = NULL;

AskLog s_trace_whole = {"trace", std::string()};

AskLog s_log_whole = {"log", std::string()};

// the index of the pair this thread puts, and -1 on the thread that carries the asks, and 1 where the stand-in the
// pair puts now is dangerous
thread_local int t_pair = -1;

thread_local int t_dangerous = 0;

// `format` and what follows it added to the end of `log`'s bytes, as fprintf writes them
__attribute__((format(gnu_printf, 2, 3))) void ask_written(AskLog *log, const char *format, ...)
{
    va_list values;
    va_start(values, format);
    va_list measured;
    va_copy(measured, values);
    const int length = vsnprintf(NULL, 0u, format, measured);
    va_end(measured);
    if (length > 0)
    {
        const size_t held = log->bytes.size();
        log->bytes.resize(held + (size_t)length + 1u);
        vsnprintf(&log->bytes[held], (size_t)length + 1u, format, values);
        log->bytes.resize(held + (size_t)length);
    }
    va_end(values);
}

// the run's buffer the trace and the log are handed to, NULL while nothing is traced
QryBuffer *s_ask_run = NULL;

// `log`'s bytes handed to the run's buffer as an ordinary blob of its name, and let go
void ask_handed(AskLog *log)
{
    if ((s_ask_run != NULL) && !log->bytes.empty())
    {
        qry_hand(s_ask_run, log->name, log->bytes.data(), log->bytes.size(), QRY_ORDINARY);
    }
    log->bytes.clear();
}

// what the thread that carries the asks wrote and has not handed, handed as the run ends
static void ask_files_closed(void)
{
    ask_handed(&s_trace_whole);
    ask_handed(&s_log_whole);
}

// the trace and the log of the thread that carries the asks opened into the run's buffer, the .qry QRY names
// (qry_buffer.h), as every path's trace is unless QRY_TRACE is 0, and handed the rest as the run ends
void ask_files_opened(void)
{
    if (!qry_traced("klq_identity", QRY_ORDINARY) || (s_ask_run != NULL))
    {
        return;
    }
    s_ask_run = qry_run();
    if (s_ask_run == NULL)
    {
        printf("klq_identity: no query writer runs to take the trace: %s names no .qry a writer holds open\n",
               QRY_ENVIRONMENT);
        return;
    }
    s_ask_trace = &s_trace_whole;
    s_ask_log = &s_log_whole;
    atexit(ask_files_closed);
}

// `log` handed to the run's buffer where every ask is to reach it as it is written: on the thread that carries the
// asks, and not from a pair's own logs, which are handed whole once the pair is put
void ask_flushed(AskLog *log)
{
    if (t_pair < 0)
    {
        ask_handed(log);
    }
}

std::string case_text(unsigned int at);

// the host's place of the first case the last ask answered apart at, from the part or from the record
thread_local unsigned int s_apart_at = 0xffffffffu;

// The round: each pair put in a thread of its own runs while it is its turn, one at a time in the order of the bridge,
// until it asks or is put. Every ask a pair hands over waits for the round, and once every pair left has asked, the
// round's asks are carried together: the mundane in one process, each dangerous one in a process of its own. No ask
// waits on another ask of the same round, since a pair asks its next only once its last is answered. `s_turn` is the
// pair that runs, and -1 the thread that carries the asks
std::mutex s_turn_lock;

std::condition_variable s_turn_changed;

int s_turn = -1;

std::vector<std::vector<CarriedAsk>> s_round_asks;

// what the rounds asked: the asks handed over, those answered from what was held, and the processes that carried
// the rest
unsigned long long s_round_handed = 0ull;

unsigned long long s_round_held = 0ull;

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

// `asked` answered from `held`. A question the part answered illegal is censored: the record keeps the part's answer
// and its refusal, and the question is held off the part from then on, as one the gate censored is
static void answer_given(RunQuestion *asked, const KqrAsk &held)
{
    asked->outcome = (held.answer == KQR_ANSWERS)                                      ? (unsigned int)RUN_ANSWERED
                     : ((held.answer == KQR_ILLEGAL) || (held.answer == KQR_CENSORED)) ? (unsigned int)RUN_HELD
                                                                                       : (unsigned int)RUN_NOTHING;
    snprintf(asked->refused, sizeof(asked->refused), "%s", held.refusal);
    for (unsigned int place = 0u; (place < held.words) && (place < RUN_CASES_MOST); place += 1u)
    {
        asked->answered[place] = held.word[place];
    }
}

// what came back of `question` as the query record keeps it: KQR_ANSWERS, KQR_ILLEGAL, KQR_NOTHING, KQR_TIMED_OUT where
// the watchdog ended it before it came back, or KQR_CENSORED where the gate held it off the part to protect the whole;
// KQR_ANSWER_COUNT where the channel never carried it
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
    if (question->outcome == RUN_HELD)
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

// The asks `carried` answered. A timed ask is put every time and kept as a sample of its own, with its cost: those of
// no shape of their own together, and alone each dangerous one and each one of a shape of its own. An
// untimed one is answered from the query record where its identity was asked before and did not time out, and the
// rest are carried, one ask of an identity the round holds twice: the mundane together, and alone each dangerous one
// and each one of a shape of its own, which a process of many does not take. Each answer the part gave, and each ask
// the gate censored, kept in the record after by its identity, an answer in place of the ask that timed out
void asks_answered(const std::vector<CarriedAsk> &carried)
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
    // the timed asks the part answered illegal or the gate censored, each with its refusal, censored again unput
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
            question->outcome = RUN_HELD;
            snprintf(question->refused, sizeof(question->refused), "%s", censored->second.c_str());
            s_round_held += 1ull;
            continue;
        }
        if (question->launches != 0u)
        {
            const int timed_alone =
                (carried[at].dangerous != 0) || (question->threads != 0u) || (question->blocks != 0u);
            (timed_alone ? timed : timed_together).push_back(question);
            continue;
        }
        static KqrAsk s_held;
        // an ask the gate held for an unknown key is open as a timed-out one is (run_channel.h, RUN_GATE_KEY_RULE)
        if (kqr_find(&s_query_record, &identities[at], &s_held) && (s_held.answer != KQR_TIMED_OUT) &&
            !((s_held.answer == KQR_CENSORED) && run_channel_key_held(s_held.refusal)))
        {
            answer_given(question, s_held);
            s_round_held += 1ull;
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
    // the ask that settles the most entries first: the one the most pairs of the round wait on, in the order handed
    // where two settle as many
    const auto settles_more = [&waiting](RunQuestion *left, RunQuestion *right) {
        return waiting[left] > waiting[right];
    };
    std::stable_sort(mundane.begin(), mundane.end(), settles_more);
    std::stable_sort(alone.begin(), alone.end(), settles_more);
    if (!mundane.empty())
    {
        run_channel_ask_many(mundane.data(), (unsigned int)mundane.size());
        s_round_processes += 1ull;
    }
    for (RunQuestion *const question : alone)
    {
        if ((question->threads != 0u) || (question->blocks != 0u))
        {
            run_channel_ask(question);
        }
        else
        {
            RunQuestion *const one[1] = {question};
            run_channel_ask_many(one, 1u);
        }
        s_round_processes += 1ull;
    }
    // timed asks of no shape of their own carried together, each timed over its own launches, and each other one alone
    if (!timed_together.empty())
    {
        run_channel_ask_many(timed_together.data(), (unsigned int)timed_together.size());
        s_round_processes += 1ull;
    }
    for (RunQuestion *const question : timed)
    {
        run_channel_ask(question);
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
    static KqrAsk s_kept;
    for (const KqrIdentity &identity : firsts)
    {
        RunQuestion *const question = first_of[identity];
        const unsigned int answer = answer_recorded(question);
        if (answer == KQR_ANSWER_COUNT)
        {
            continue;
        }
        memset(&s_kept, 0, sizeof(s_kept));
        s_kept.identity = identity;
        s_kept.answer = answer;
        if (answer == KQR_ANSWERS)
        {
            for (unsigned int place = 0u; (place < question->cases) && (place < KQR_WORDS); place += 1u)
            {
                s_kept.word[place] = question->answered[place];
                s_kept.words += 1u;
            }
        }
        else
        {
            snprintf(s_kept.refusal, sizeof(s_kept.refusal), "%s", question->refused);
        }
        kqr_keep(&s_query_record, &s_kept);
    }
    for (const auto &copy : copies)
    {
        copy.first->outcome = copy.second->outcome;
        snprintf(copy.first->refused, sizeof(copy.first->refused), "%s", copy.second->refused);
        memcpy(copy.first->answered, copy.second->answered,
               (size_t)copy.first->cases * sizeof(copy.first->answered[0]));
    }
}

// The asks `asked`, `count` of them, each dangerous where `dangerous` says, answered through the query record: on a
// pair's own thread handed to the round and waited on, and on the thread that carries the asks answered at once. A
// timed ask, the only one a mode means to ask again, is put every time
void asks_carried(RunQuestion *const *asked, const int *dangerous, unsigned int count)
{
    std::vector<CarriedAsk> carried;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        carried.push_back(CarriedAsk{asked[at], (dangerous != NULL) ? dangerous[at] : 0});
    }
    if (t_pair < 0)
    {
        asks_answered(carried);
        return;
    }
    std::unique_lock<std::mutex> lock(s_turn_lock);
    s_round_asks[(size_t)t_pair] = carried;
    s_turn = -1;
    s_turn_changed.notify_all();
    const int pair = t_pair;
    s_turn_changed.wait(lock, [pair]() { return s_turn == pair; });
}

// The host's answer to a case as a bracket [down, up] around the exact value at the result's width, and whether the
// part's answer `answered` falls in it: 1 where it does, 0 where it does not, and -1 where the host gives no bracket.
// An integer case is a bracket of one word, `down`, and holds where the part answers that word. A floating case is a
// bracket of two ends, `down:up`, the two values of the result's type nearest the exact one, and holds where the part
// answers either end, the end it takes being its rounding. A case the host's C leaves undefined is `-` and gates
// nothing
int bracket_holds(const std::string &host, unsigned long long answered)
{
    if (host == "-")
    {
        return -1;
    }
    const size_t colon = host.find(':');
    const unsigned long long down = std::stoull(host.substr(0u, colon), nullptr, 16);
    const unsigned long long up = (colon == std::string::npos) ? down : std::stoull(host.substr(colon + 1u), nullptr, 16);
    return ((answered == down) || (answered == up)) ? 1 : 0;
}
