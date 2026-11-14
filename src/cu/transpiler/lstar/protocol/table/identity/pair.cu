// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair.cu: the pair: each pair of the bridge put to the part through the carriers, its verdict written beneath it
#include "pair.h"

thread_local RunQuestion *t_question = NULL;
thread_local std::vector<unsigned int> *t_places = NULL;

// every choice a put makes at a link, written to the log as `choice <flags> <link>`, the flags of the trace's
// table that are set there ORed into one word (query_trace.tsv): why a link was passed over, how the part answered
// it, or why the put ended at it. The flags an open pair's links carry are the questions a further pass takes up
void PairWork::chosen(const char *const *names, unsigned int count, const std::string &link) const
{
    if (s_query_log == NULL)
    {
        return;
    }
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        word |= query_trace_flag(s_query_trace, names[at]);
    }
    query_log_print(s_query_log, "choice 0x%llx %s\n", word, link.c_str());
}

// one choice written, by its flag's name
void PairWork::chose(const char *name, const std::string &link) const
{
    const char *const names[1] = {name};
    chosen(names, 1u, link);
}

// The conference a refused put calls, convened on the pair's thread once the teacher has called it (overseer.h). The
// learner passes its statement: what it put, the carrier, what the stand-in does and the stand-in as written. The
// coherence sets hand what the pair was being queried as: the sets the bridge reads it into and what its texts show
// of it. The counterexample passes the first case apart the pair contains, this run's or the bridge's. The ruling is
// written to the trace and its choice to the log, and its guidance given back: OVERSEER_HANDS_OFF where no party
// called a conference of `statement`
unsigned int PairWork::conferred(const std::string &from, const std::string &to, const std::string &statement,
                                 const std::string &located, const std::string &carrier_name, unsigned int scratch,
                                 const RunQuestion &question, const std::string &closed_here,
                                 const std::string &link_address)
{
    QryShared *const run = qry_member_run();
    const std::string identity = question_identity(&question);
    char joined[OVERSEER_STANDING_MAX_LEN];
    overseer_located_joined(located.c_str(), joined);
    const std::string statement_line = statement + "; carrier " + carrier_name +
                                       (form_addresses(located) ? "; stand-in addresses" : "") +
                                       (form_controls(located) ? "; stand-in controls" : "") +
                                       ((scratch != 0u) ? "; stand-in scratch" : "") + "; located " + joined;
    overseer_account(run, identity.c_str(), "learner", statement_line.c_str());
    std::string sets;
    std::string counterexample = closed_here;
    for (const std::string &line : pair_beneath(bridge, from, to))
    {
        if ((line.rfind("closed ", 0u) == 0u) && counterexample.empty())
        {
            counterexample = line;
        }
        else if ((line.rfind("open ", 0u) != 0u) && (line.rfind("closed ", 0u) != 0u))
        {
            sets += (sets.empty() ? "" : " ") + line;
        }
    }
    const std::string categories = pair_categories(sass, from, to);
    const std::string learning = (sets.empty() ? std::string("no set an answer reads it into yet") : sets) +
                                 (categories.empty() ? std::string() : (", its texts reading it" + categories));
    overseer_account(run, identity.c_str(), "coherence",
                     ("the pair " + from + " " + to + "; learning " + learning).c_str());
    overseer_account(run, identity.c_str(), "counterexample",
                     counterexample.empty()
                         ? "the pair contains no case apart yet"
                         : ("the pair contains its first case apart; counterexample " + counterexample).c_str());
    OverseerRuling ruling;
    if (!overseer_convened(run, identity.c_str(), t_other_way, &ruling))
    {
        return OVERSEER_HANDS_OFF;
    }
    conferences += 1u;
    lessons_taught += ruling.taught ? 1u : 0u;
    chose("conference", link_address);
    if (s_trace_log != NULL)
    {
        query_log_print(s_trace_log, "conference %s: %s\n", identity.c_str(), ruling.text);
        query_log_flushed(s_trace_log);
    }
    return ruling.guidance;
}

// The bridge's pairs put to the part, `pair <form> <form>` in Lstar.klq: two forms whose sameness the text leaves
// open. A pair is put in a carrier, a chain of ours the host computes that contains one form's text as the ruleset
// writes it: the form's arguments are read off the chain's lines, the other form is written in its place with the
// arguments of its parameters' names, and the chain is assembled as the engine assembles it and put to the part over
// every case the host computes the carrier on. The pair is open with its count of cases where the part answers the
// changed chain as the host answers the carrier on every one, and closed at the carrier's address, the question and the
// position of the link, and the first case it answers apart. Each form is tried in the other's place, the first in the
// second's first; a pair neither form can are in for in any carrier, the second naming a parameter the first does
// not or neither's text located in a chain, and a pair the part refuses in every carrier, is open with 0. Each
// verdict is written beneath its pair in the bridge in place of the one it recorded. Where the log is retained, each
// put is followed by the values the form reads there on every case, the carrier cut at the form, for orior_klq_decoder
// to read its product off
int identity_pair(const char *engine, const char *answers, const char *ksc, const char *klq,
                  const char *machine_path, const char *const *carrier)
{
    PairWork work;
    work.host = host_answers_read(answers);
    work.sass = identity_ruleset();
    work.machine = machine_path;
    if (work.sass == NULL)
    {
        printf("orior_klq_identity pair: the ruleset %s names is not read\n", IDENTITY_RULESET);
        return 1;
    }
    carriers_read(engine, work.host, &work.carriers);
    // the seed the order of the links of one magnitude is drawn from, with each put's two forms, the run's
    // IDENTITY_SEED where it is given
    work.seed = identity_seed();
    std::string line;
    std::istringstream bridge_stream(run_file_text(klq));
    while (std::getline(bridge_stream, line))
    {
        work.bridge.push_back((!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line);
    }
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "pair");
    std::string trace_error;
    if (!query_trace_read(query_trace_path(), &s_query_trace, &trace_error))
    {
        printf("orior_klq_identity pair: %s\n", trace_error.c_str());
        run_channel_close();
        return 1;
    }
    kqr_mark(&s_query_record, KQR_SEED_RECORD, std::to_string(work.seed).c_str());
    // the lessons a run before this one taught, carried into this run (overseer.h)
    overseer_lessons_cleared();
    const unsigned int lessons_carried = overseer_lessons_read(qry_member_run());
    // the queries the query record contained before this cycle, read with R
    const unsigned int recorded_before = s_query_record.query_count;
    const std::unique_ptr<RunQuestion> stalled(new RunQuestion());
    std::vector<unsigned int> stalled_places;
    stall_cases(stalled.get(), &stalled_places);
    // the values the cases are made of: the three operands of every case, as the log's cases write them
    std::vector<unsigned long long> operands;
    for (unsigned int place = 0u; place < stalled->cases; place += 1u)
    {
        for (unsigned int operand = 0u; operand < 3u; operand += 1u)
        {
            operands.push_back(((unsigned long long)stalled->word[place][2u * operand + 1u] << 32u) |
                               stalled->word[place][2u * operand]);
        }
    }
    work.cased = concept_values(operands);
    // the bridge's keys, the schema's forms, in its order
    for (const std::string &entry : work.bridge)
    {
        if (entry.rfind("key ", 0u) == 0u)
        {
            work.keys.push_back(entry.substr(4u));
        }
    }
    std::vector<std::string> written;
    unsigned int open = 0u;
    unsigned int closed = 0u;
    unsigned int unqueried = 0u;
    int beneath = 0;
    query_logs_opened();
    // what the texts of a pair's two forms show of it before any query, written to the log
    const auto pair_facts = [&](const std::string &first, const std::string &second) {
        query_log_print(s_query_log, "facts %s %s%s\n", first.c_str(), second.c_str(),
                        pair_categories(work.sass, first, second).c_str());
    };
    // every pair of the bridge, in its order, the question and places its put queries with, the hash of what its put
    // reads, and the verdict it came to
    struct PairPut
    {
        std::string first;
        std::string second;
        std::string verdict;
        int put_whole;
        std::unique_ptr<RunQuestion> question;
        std::vector<unsigned int> places;
        std::string reads;
    };
    // what every put reads of the engine: each chain's text and the host's answers to its cases, by its question
    unsigned long long engine_read = 0xcbf29ce484222325ull;
    for (const auto &chain : work.carriers.texts)
    {
        engine_read = record_hash(chain.first.c_str(), chain.first.size() + 1u, engine_read);
        engine_read = record_hash(chain.second.c_str(), chain.second.size() + 1u, engine_read);
        for (const std::string &answer : work.host[chain.first])
        {
            engine_read = record_hash(answer.c_str(), answer.size() + 1u, engine_read);
        }
    }
    // The scheduler: the record's gray entries are the queue. A pair closed on the record's path, whose reads are as
    // they were, is settled: its verdict is the record's, and it is put no more, since a put runs again only where what
    // it reads has changed. Every other pair, open, with no path, or reading what changed, is gray, and the rounds put
    // the gray pairs alone
    std::map<std::string, KqrPath> recorded_paths;
    static KqrPath s_recorded;
    for (unsigned int at = 0u; kqr_path_read(&s_query_record, at, &s_recorded); at += 1u)
    {
        recorded_paths[std::string(s_recorded.first) + " " + s_recorded.second] = s_recorded;
    }
    std::vector<PairPut> pairs;
    size_t settled = 0u;
    for (const std::string &entry : work.bridge)
    {
        if (entry.rfind("pair ", 0u) == 0u)
        {
            std::stringstream words(entry);
            std::string type;
            PairPut pair{std::string(),
                         std::string(),
                         "open 0",
                         0,
                         std::unique_ptr<RunQuestion>(new RunQuestion()),
                         std::vector<unsigned int>(),
                         std::string()};
            words >> type >> pair.first >> pair.second;
            std::vector<std::string> names;
            const std::string first_text = marked(work.sass, pair.first, &names);
            const std::string second_text = marked(work.sass, pair.second, &names);
            unsigned long long reads = record_hash(first_text.c_str(), first_text.size() + 1u, engine_read);
            reads = record_hash(second_text.c_str(), second_text.size() + 1u, reads);
            char hashed[17];
            snprintf(hashed, sizeof(hashed), "%016llx", reads);
            pair.reads = hashed;
            const auto path = recorded_paths.find(pair.first + " " + pair.second);
            if ((path != recorded_paths.end()) && path->second.read &&
                (strtoull(pair.reads.c_str(), NULL, 16) == path->second.reads) &&
                (strncmp(path->second.verdict, "closed ", 7u) == 0))
            {
                pair.verdict = path->second.verdict;
                pair.put_whole = 1;
                settled += 1u;
            }
            pairs.push_back(std::move(pair));
        }
    }
    // Each pair put in a thread of its own, run only in its turn: its facts, its put, and the stand-in the other way
    // round where the form's own way answered nothing. Its trace and its log are logs of its own, stored in memory and
    // passed whole to the query's writer once it is put, after what the thread that carries the queries wrote before it
    s_round_queries.assign(pairs.size(), std::vector<CarriedQuery>());
    std::vector<std::thread> threads;
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        if (pairs[at].put_whole)
        {
            continue;
        }
        threads.emplace_back([&, at]() {
            {
                std::unique_lock<std::mutex> lock(s_turn_lock);
                s_turn_changed.wait(lock, [at]() { return s_turn == (int)at; });
            }
            t_pair = (int)at;
            QueryLog pair_trace = {s_trace_whole.name, std::string()};
            QueryLog pair_log = {s_log_whole.name, std::string()};
            if (s_query_run != NULL)
            {
                s_trace_log = &pair_trace;
                s_query_log = &pair_log;
            }
            PairPut &pair = pairs[at];
            t_question = pair.question.get();
            t_places = &pair.places;
            stall_cases(t_question, t_places);
            if (s_query_log != NULL)
            {
                pair_facts(pair.first, pair.second);
            }
            // the form's own way first, then the other way round where it answered nothing or a conference turned it,
            // then the lower witness where neither answered
            t_turned = 0;
            t_other_way = 0;
            const int one_way = work.put(pair.first, pair.second, &pair.verdict);
            int either_way = one_way;
            if (!one_way || t_turned)
            {
                t_other_way = 1;
                std::string other_verdict;
                const int other_way = work.put(pair.second, pair.first, &other_verdict);
                pair.verdict = one_way ? pair_verdicts_joined(pair.verdict, other_verdict) : other_verdict;
                either_way = one_way || other_way;
            }
            if (!either_way)
            {
                work.witness_lower(pair.first, pair.second, &pair.verdict);
            }
            if (s_query_log != NULL)
            {
                s_trace_log = NULL;
                s_query_log = NULL;
                // the thread that carries the queries waits out this pair's turn: what it wrote before the pair goes
                // first
                query_log_passed(&s_trace_whole);
                query_log_passed(&s_log_whole);
                query_log_passed(&pair_trace);
                query_log_passed(&pair_log);
            }
            std::unique_lock<std::mutex> lock(s_turn_lock);
            pair.put_whole = 1;
            s_turn = -1;
            s_turn_changed.notify_all();
        });
    }
    // The rounds: each pair not yet put runs in its turn until it queries or is put, and the queries the round was
    // passed are answered together. The rounds loop while a pair is left to put, and end once every pair is put
    unsigned int round = 0u;
    size_t left = pairs.size() - settled;
    printf("orior_klq_identity pair: %zu pairs settled on the record, %zu gray in the queue\n", settled, left);
    while (left != 0u)
    {
        const std::chrono::steady_clock::time_point round_began = std::chrono::steady_clock::now();
        const unsigned long long passed_before = s_round_passed;
        const unsigned long long recalled_before = s_round_recalled;
        const unsigned long long processes_before = s_round_processes;
        round += 1u;
        kqr_mark(&s_query_record, KQR_ROUND_RECORD, std::to_string(round).c_str());
        std::vector<CarriedQuery> carried;
        unsigned int in_flight = 0u;
        for (size_t at = 0u; at < pairs.size(); at += 1u)
        {
            std::unique_lock<std::mutex> lock(s_turn_lock);
            if (pairs[at].put_whole)
            {
                continue;
            }
            s_turn = (int)at;
            s_turn_changed.notify_all();
            s_turn_changed.wait(lock, []() { return s_turn == -1; });
            in_flight += s_round_queries[at].empty() ? 0u : 1u;
            carried.insert(carried.end(), s_round_queries[at].begin(), s_round_queries[at].end());
            s_round_queries[at].clear();
        }
        queries_answered(carried);
        left = (size_t)std::count_if(pairs.begin(), pairs.end(), [](const PairPut &pair) { return !pair.put_whole; });
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - round_began).count();
        printf("orior_klq_identity round %u: %u pairs queried, %llu queries passed, %llu answered from what was "
               "recorded, %llu "
               "processes, %zu of %zu pairs put, %.1f seconds\n",
               round, in_flight, s_round_passed - passed_before, s_round_recalled - recalled_before,
               s_round_processes - processes_before, pairs.size() - left, pairs.size(), seconds);
        fflush(stdout);
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    // the bridge written with each pair's verdict beneath it, in place of the verdict it recorded
    size_t paired = 0u;
    for (const std::string &entry : work.bridge)
    {
        const int verdict_line = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        beneath = (entry.rfind("pair ", 0u) == 0u) || (beneath && verdict_line);
        if (verdict_line && beneath)
        {
            continue;
        }
        written.push_back(entry);
        if (entry.rfind("pair ", 0u) != 0u)
        {
            continue;
        }
        const PairPut &pair = pairs[paired];
        paired += 1u;
        open += ((pair.verdict.rfind("open ", 0u) == 0u) && (pair.verdict != "open 0")) ? 1u : 0u;
        closed += (pair.verdict.rfind("closed ", 0u) == 0u) ? 1u : 0u;
        unqueried += (pair.verdict == "open 0") ? 1u : 0u;
        printf("  %s %s: %s\n", pair.first.c_str(), pair.second.c_str(), pair.verdict.c_str());
        written.push_back(pair.verdict);
    }
    run_channel_close();
    RunWritten file = {klq, 1, std::string()};
    for (const std::string &written_line : written)
    {
        run_print(&file, "%s\n", written_line.c_str());
    }
    if (!run_closed(&file))
    {
        printf("orior_klq_identity pair: %s could not be written\n", klq);
        return 1;
    }
    printf("orior_klq_identity pair: %llu queries, %u open, %u closed, %u not queried, written to %s\n",
           work.queries, open, closed, unqueried, klq);
    // the lessons stored, those carried in and those taught, passed to the run for the next run to carry
    overseer_lessons_passed(qry_member_run());
    printf("orior_klq_identity pair: %u conferences called by refusals, %u lessons taught, %u carried in\n", work.conferences,
           work.lessons_taught, lessons_carried);
    // the path this cycle read off the queries, each pair and its verdict, an open pair the question a further pass
    // takes up, written to the query record with every query it contained and every query this cycle put once R is
    // written back
    kqr_paths_clear(&s_query_record);
    static KqrPath s_path;
    for (const PairPut &pair : pairs)
    {
        snprintf(s_path.first, sizeof(s_path.first), "%s", pair.first.c_str());
        snprintf(s_path.second, sizeof(s_path.second), "%s", pair.second.c_str());
        s_path.read = pair.reads.empty() ? 0 : 1;
        s_path.reads = strtoull(pair.reads.c_str(), NULL, 16);
        snprintf(s_path.verdict, sizeof(s_path.verdict), "%s", pair.verdict.c_str());
        kqr_path_add(&s_query_record, &s_path);
        pair_row_passed(pair.first, pair.second, s_path.read, s_path.reads, pair.verdict);
    }
    printf("orior_klq_identity pair: %u queries stored in the query record, %u of them put this cycle\n",
           s_query_record.query_count, s_query_record.query_count - recorded_before);
    return 0;
}
