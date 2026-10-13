// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// alike.cu: a question put and read alike or apart with the host, the host's answers read, and a chain's code read from the engine
#include "identity_internal.h"

// 1 where the part answers `code`, put as `question` says of its registers, shape and launches, alike with the host
// on every case it computes, 0 where it answers apart, refuses it or the gate holds it, each counted in `asks`
int question_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                          const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks)
{
    static int s_opened = 0;
    if (s_opened == 0)
    {
        ask_files_opened();
    }
    s_opened = 1;
    question->code = code.data();
    question->code_size = code.size();
    *asks += 1ull;
    // the ask's key in the record, and what the record holds of it
    unsigned long long hosted = 0xcbf29ce484222325ull;
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        hosted = record_hash(host[places[place]].c_str(), host[places[place]].size() + 1u, hosted);
    }
    char keyed[160];
    snprintf(keyed, sizeof(keyed), "%016llx %u %u %u %016llx %016llx",
             record_hash(code.data(), code.size(), 0xcbf29ce484222325ull), question->registers, question->threads,
             question->blocks, record_hash(question->word, sizeof(question->word[0]) * question->cases, 0xcbf29ce484222325ull),
             hosted);
    const std::string key = keyed;
    // R holds an ask's verdict and its first case apart, and the query record and the log what came back of every
    // case: with the log kept, an ask is read from the query record or put to the part, and R written as ever
    const int recordable = !s_record_ksc.empty() && (question->launches == 0u);
    const auto recorded = (recordable && (s_ask_log == NULL)) ? s_record.find(key) : s_record.end();
    if (recorded != s_record.end())
    {
        const AskRecorded &held = recorded->second;
        // an ask the part answered illegal is censored, its answer kept in R as the part gave it
        question->outcome = (held.answer == "answers")   ? RUN_ANSWERED
                            : (held.answer == "illegal") ? RUN_HELD
                                                         : RUN_NOTHING;
        snprintf(question->refused, sizeof(question->refused), "%s", held.refusal.c_str());
        const int alike = (held.answer == "answers") && (held.word == 0xffffffffu);
        s_apart_at = (held.answer == "answers") ? held.word : 0xffffffffu;
        if (s_ask_trace != NULL)
        {
            ask_written(s_ask_trace, "ask %llu: recorded %s %08x %s\n", *asks, held.answer.c_str(), held.word,
                        key.c_str());
            ask_flushed(s_ask_trace);
        }
        if (s_ask_log != NULL)
        {
            ask_written(s_ask_log, "ask %llu recorded\n", *asks);
        }
        return alike;
    }
    if (s_ask_trace != NULL)
    {
        ask_written(s_ask_trace,
                    "ask %llu: %llu bytes, %u registers, %u threads in %u blocks, %u launches, %u cases: ", *asks,
                    question->code_size, question->registers, question->threads, question->blocks, question->launches,
                    question->cases);
    }
    RunQuestion *const asked[1] = {question};
    asks_carried(asked, &t_dangerous, 1u);
    if (question->outcome != RUN_ANSWERED)
    {
        if (s_ask_trace != NULL)
        {
            ask_written(s_ask_trace, "outcome %u, %s\n", question->outcome, question->refused);
            ask_flushed(s_ask_trace);
        }
        if (s_ask_log != NULL)
        {
            ask_written(s_ask_log, "ask %llu refused\n", *asks);
        }
        if (recordable && ((question->outcome == RUN_ILLEGAL) || (question->outcome == RUN_NOTHING)))
        {
            s_record[key] = AskRecorded{(question->outcome == RUN_ILLEGAL) ? "illegal" : "nothing", 0u,
                                        question->refused};
        }
        return 0;
    }
    if (s_ask_log != NULL)
    {
        // the places this thread's log last wrote as its cases, by a hash of them and their count, a count past every
        // question's where it wrote none
        static thread_local unsigned long long t_logged_hash = 0ull;
        static thread_local unsigned int t_logged_count = 0xffffffffu;
        const unsigned long long asked_hash =
            record_hash(places.data(), sizeof(places[0]) * question->cases, 0xcbf29ce484222325ull);
        if ((asked_hash != t_logged_hash) || (question->cases != t_logged_count))
        {
            ask_written(s_ask_log, "cases");
            for (unsigned int place = 0u; place < question->cases; place += 1u)
            {
                ask_written(s_ask_log, " %s", case_text(places[place]).c_str());
            }
            ask_written(s_ask_log, "\n");
            t_logged_hash = asked_hash;
            t_logged_count = question->cases;
        }
        std::string came_back;
        for (unsigned int place = 0u; place < question->cases; place += 1u)
        {
            const int holds = bracket_holds(host[places[place]], question->answered[place]);
            came_back += (holds < 0) ? '-' : (holds != 0) ? '=' : 'x';
        }
        ask_written(s_ask_log, "ask %llu %s\n", *asks, came_back.c_str());
    }
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const std::string &expected = host[places[place]];
        if (bracket_holds(expected, question->answered[place]) == 0)
        {
            if (s_ask_trace != NULL)
            {
                ask_written(s_ask_trace, "apart at case %u, the host %s and the part %llx\n", places[place],
                            expected.c_str(), question->answered[place]);
                ask_flushed(s_ask_trace);
            }
            if (recordable)
            {
                s_record[key] = AskRecorded{"answers", places[place], ""};
            }
            s_apart_at = places[place];
            return 0;
        }
    }
    if (recordable)
    {
        s_record[key] = AskRecorded{"answers", 0xffffffffu, ""};
    }
    if (s_ask_trace != NULL)
    {
        ask_written(s_ask_trace, "alike%s\n",
                    (question->launches != 0u) ? (", " + std::to_string(question->nanoseconds) + " ns").c_str() : "");
        ask_flushed(s_ask_trace);
    }
    return 1;
}

// 1 where the part answers `code` alike with the host on every case it computes, in a container declaring every
// register its file holds (sm_86.kdm), 0 where it answers apart, refuses it or the gate holds it, each counted in
// `asks`
int stall_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                       const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks)
{
    question->registers = 255u;
    return question_alike(code, host, places, question, asks);
}

// each chain's answers the host computes, one a case over every case, by the chain's number, read from the latest blob
// of `answers` in the run's buffer
std::map<std::string, std::vector<std::string>> host_answers_read(const char *answers)
{
    std::map<std::string, std::vector<std::string>> host;
    std::istringstream host_file(run_blob_text(answers));
    std::string line;
    while (std::getline(host_file, line))
    {
        std::stringstream words(line);
        std::string number;
        words >> number;
        std::vector<std::string> case_answers;
        std::string answer;
        while (words >> answer)
        {
            case_answers.push_back(answer);
        }
        if (case_answers.size() == IDENTITY_CASES)
        {
            host[number] = case_answers;
        }
    }
    return host;
}

// chain `number` of ours, read from `engine`: its code, sixteen bytes an instruction, and the links its listing gives.
// 0 where either is missing or the listing gives more links than the code holds instructions
int chain_code_read(const char *engine, const std::string &number, std::vector<unsigned char> *code,
                           std::vector<Link> *links)
{
    const std::string base = std::string(engine) + "/" + number;
    std::ifstream listing(base + ".dis");
    std::ifstream binary(base + ".bin", std::ios::binary);
    if (!listing || !binary)
    {
        return 0;
    }
    code->assign((std::istreambuf_iterator<char>(binary)), std::istreambuf_iterator<char>());
    links->clear();
    std::string line;
    while (std::getline(listing, line))
    {
        Link link;
        const int read = link_read(line, &link);
        if (read < 0)
        {
            break;
        }
        if ((read > 0) && !link.position.empty())
        {
            links->push_back(link);
        }
    }
    return !links->empty() && ((code->size() % 16u) == 0u) && ((links->size() * 16u) <= code->size());
}

// the registers chain `number` of ours names, its high water mark as `engine` gives it, or 0 where it gives none. The
// part refuses a container declaring the mark alone where the code names a register within the part's own past the
// last it gives code, and the count a container declares is walked up from the mark until the part answers
unsigned int chain_registers(const char *engine, const std::string &number)
{
    std::ifstream file(std::string(engine) + "/" + number + ".registers");
    unsigned int registers = 0u;
    return (file >> registers) ? registers : 0u;
}

// The .ksc rewritten by `who` with `rows` in place of every row of the run channel it held to a question beginning
// with the words `asked`, whatever came back, the rows put after its last row of the run channel and its counts of the
// run channel taken again (ksc_rows_replaced_by). 1, or 0 where the .ksc could not be written
int ksc_answers_written(const char *ksc, const std::string &asked, const std::vector<std::string> &rows,
                               QryWho who)
{
    KscFile file = {};
    std::vector<const char *> texts;
    for (const std::string &row : rows)
    {
        texts.push_back(row.c_str());
    }
    const int written =
        ksc_read(&file, ksc) &&
        ksc_rows_replaced_by(&file, "run", asked.c_str(), texts.data(), (unsigned int)texts.size(), who) &&
        ksc_write_by(&file, ksc, who);
    ksc_free(&file);
    return written;
}

void arrangement_probes(const char *engine, const std::map<std::string, std::vector<std::string>> &host,
                               const std::string &kdm, const Ruleset *sass, const SassMachine *machine,
                               const StallField &stall, unsigned long long longest,
                               const std::set<std::pair<std::string, std::string>> &held, size_t most,
                               std::vector<StallAsk> *asks, std::vector<std::vector<unsigned char>> *codes);
