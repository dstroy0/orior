// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// alike.cu: a question put and read alike or apart with the host, the host's questions written and its answers read, and a chain's code read from the engine
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

// `code` put to the part as `question` says, to read what it answers, not to hold it to the host. R holds a verdict
// alone and cannot answer a reading: the record is neither read nor written. 1 where the part answers it, each case's
// value in `question->answered`, 0 where it refuses it or the gate holds it, each counted in `asks`
int question_read(const std::vector<unsigned char> &code, RunQuestion *question, unsigned long long *asks)
{
    ask_files_opened();
    question->code = code.data();
    question->code_size = code.size();
    *asks += 1ull;
    if (s_ask_trace != NULL)
    {
        ask_written(s_ask_trace,
                    "ask %llu: %llu bytes, %u registers, %u threads in %u blocks, %u launches, %u cases, read: ", *asks,
                    question->code_size, question->registers, question->threads, question->blocks, question->launches,
                    question->cases);
    }
    RunQuestion *const asked[1] = {question};
    asks_carried(asked, &t_dangerous, 1u);
    const int answered = question->outcome == RUN_ANSWERED;
    if (s_ask_trace != NULL)
    {
        ask_written(s_ask_trace, "%s\n", answered ? "answered" : question->refused);
        ask_flushed(s_ask_trace);
    }
    if (s_ask_log != NULL)
    {
        ask_written(s_ask_log, "ask %llu %s\n", *asks, answered ? "read" : "refused");
    }
    return answered;
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
// run channel taken again (ksc_rows_replaced_traced_by); every row let go that no row put gives again is kept in the
// part's .krf beside it (krf.h). 1, or 0 where the .ksc could not be written
int ksc_answers_written(const char *ksc, const std::string &asked, const std::vector<std::string> &rows,
                               QryWho who)
{
    KscFile file = {};
    KtextFile removed = {};
    std::vector<const char *> texts;
    for (const std::string &row : rows)
    {
        texts.push_back(row.c_str());
    }
    const int written = ksc_read(&file, ksc) &&
                        ksc_rows_replaced_traced_by(&file, "run", asked.c_str(), texts.data(),
                                                    (unsigned int)texts.size(), who, &removed) &&
                        ksc_write_by(&file, ksc, who);
    ksc_free(&file);
    if (written && (removed.lines != 0u))
    {
        char krf[1200];
        krf_path_beside(ksc, krf, sizeof(krf));
        if (!krf_removed_by(krf, "ksc", "replaced", removed.line, removed.lines, who))
        {
            printf("  %u rows let go from %s could not be kept in %s\n", removed.lines, ksc, krf);
        }
    }
    ktext_free(&removed);
    return written;
}

void arrangement_probes(const char *engine, const std::map<std::string, std::vector<std::string>> &host,
                               const std::string &kdm, const Ruleset *sass, const SassMachine *machine,
                               const StallField &stall, unsigned long long longest,
                               const std::set<std::pair<std::string, std::string>> &held, size_t most,
                               std::vector<StallAsk> *asks, std::vector<std::vector<unsigned char>> *codes);

// the kernel `number` of the stick's text, from its signature to the brace that closes it
static std::string kernel_text(const std::string &stick, const std::string &number)
{
    const std::string signature = "extern \"C\" __global__ void measuring_stick_" + number + "(";
    const size_t first = stick.find(signature);
    if (first == std::string::npos)
    {
        return std::string();
    }
    const size_t last = stick.find("\n}\n", first);
    return stick.substr(first, last + 3u - first);
}

// the type a kernel's first operand is cast to, as its text declares it, const or not: a compound assignment writes it
static std::string first_type(const std::string &kernel)
{
    static const std::regex s_first("\\b(?:const )?([a-z][a-z ]*) a = \\(");
    std::smatch found;
    return std::regex_search(kernel, found, s_first) ? found[1].str() : std::string("int");
}

int host_questions_write(const std::string &path, const std::string &stick, const std::set<std::string> &numbers)
{
    RunWritten file = {path, 0, std::string()};
    run_print(&file, "// the stick's integer questions as the host computes them, written by klq_identity\n");
    run_print(&file, "#include <limits>\n#include <stdio.h>\n\n");
    run_print(&file, "struct HostIndex\n{\n    unsigned int x, y, z;\n};\n");
    run_print(&file,
              "static const HostIndex threadIdx = {0u, 0u, 0u};\nstatic const HostIndex blockIdx = {0u, 0u, 0u};\n");
    run_print(&file, "static const HostIndex blockDim = {1u, 1u, 1u};\n#define __global__\n\n");
    std::vector<std::string> written;
    for (const std::string &number : numbers)
    {
        const std::string kernel = kernel_text(stick, number);
        if (kernel.empty())
        {
            continue;
        }
        run_print(&file, "%s\n", kernel.c_str());
        const std::string type = first_type(kernel);
        const int divides = (kernel.find(" / b") != std::string::npos) || (kernel.find(" % b") != std::string::npos) ||
                            (kernel.find("/=") != std::string::npos) || (kernel.find("%=") != std::string::npos) ||
                            (kernel.find(" / ") != std::string::npos);
        const int shifts = (kernel.find("<<") != std::string::npos) || (kernel.find(">>") != std::string::npos);
        // 1 where the host's C would trap on the case or leave it undefined: a divisor of zero, the least value over -1,
        // or a shift by the width or past it
        run_print(&file, "static int refused_%s(const unsigned long long *in)\n{\n", number.c_str());
        run_print(&file, "    const %s a = (%s)in[0];\n    const %s b = (%s)in[1];\n    (void)a;\n    (void)b;\n",
                  type.c_str(), type.c_str(), type.c_str(), type.c_str());
        if (divides)
        {
            run_print(&file,
                      "    if ((b == 0) || (std::numeric_limits<%s>::is_signed && (b == (%s)-1) && "
                      "(a == std::numeric_limits<%s>::min())))\n    {\n        return 1;\n    }\n",
                      type.c_str(), type.c_str(), type.c_str());
        }
        if (shifts)
        {
            run_print(
                &file,
                "    if (((unsigned long long)b >= (unsigned long long)(sizeof(a + 0) * 8u)) || ((long long)b < 0))\n"
                "    {\n        return 1;\n    }\n");
        }
        run_print(&file, "    return 0;\n}\n\n");
        written.push_back(number);
    }
    run_print(&file, "typedef void (*HostKernel)(const unsigned long long *, unsigned long long *, unsigned int);\n");
    run_print(&file, "typedef int (*HostRefused)(const unsigned long long *);\n");
    run_print(&file,
              "static const struct\n{\n    const char *number;\n    HostKernel kernel;\n    HostRefused refused;\n} "
              "s_questions[] = {\n");
    for (const std::string &number : written)
    {
        run_print(&file, "    {\"%s\", measuring_stick_%s, refused_%s},\n", number.c_str(), number.c_str(),
                  number.c_str());
    }
    run_print(&file, "};\n\nstatic const unsigned long long s_values[] = {");
    for (unsigned int at = 0u; at < IDENTITY_VALUES; at += 1u)
    {
        run_print(&file, "%s0x%llxull", (at == 0u) ? "" : ", ", s_values[at]);
    }
    run_print(&file, "};\n\n");
    run_print(
        &file,
        "// each question a line: its number, then its answer to every case in order, - where the case is refused\n");
    run_print(&file, "int main(void)\n{\n");
    run_print(&file,
              "    for (unsigned int at = 0u; at < sizeof(s_questions) / sizeof(s_questions[0]); at += 1u)\n    {\n");
    run_print(&file, "        printf(\"%%s\", s_questions[at].number);\n");
    run_print(&file, "        for (unsigned int third = 0u; third < %uu; third += 1u)\n        {\n", IDENTITY_THIRDS);
    run_print(&file, "            for (unsigned int left = 0u; left < %uu; left += 1u)\n            {\n",
              (unsigned int)IDENTITY_VALUES);
    run_print(&file, "                for (unsigned int right = 0u; right < %uu; right += 1u)\n                {\n",
              (unsigned int)IDENTITY_VALUES);
    run_print(&file,
              "                    const unsigned long long in[4] = {s_values[left], s_values[right], third, 0ull};\n");
    run_print(&file, "                    unsigned long long out[2] = {0ull, 0ull};\n");
    run_print(&file, "                    if (s_questions[at].refused(in))\n                    {\n");
    run_print(&file,
              "                        printf(\" -\");\n                        continue;\n                    }\n");
    run_print(&file, "                    s_questions[at].kernel(in, out, 1u);\n");
    run_print(&file, "                    printf(\" %%llx\", out[0]);\n                }\n            }\n        }\n");
    run_print(&file, "        printf(\"\\n\");\n    }\n    return 0;\n}\n");
    if (!run_closed(&file))
    {
        printf("  %s could not be written\n", path.c_str());
        return 0;
    }
    printf("  %s: %u questions the host computes\n", path.c_str(), (unsigned int)written.size());
    return 1;
}
