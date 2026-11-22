// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// alike.cu: a question put and read alike or apart with the host, the host's questions written and its answers read,
// and a chain's code read from the engine
#include "identity_internal.h"
#include "../../../../../../platform/platform.h"
#include "../../../../../../../includes/parsers/who.h"

// 1 where the part answers `code`, put as `question` specifies of its registers, shape and launches, alike with the
// host on every case it computes, 0 where it answers apart, refuses it or the gate blocks it, each counted in `queries`
int question_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                   const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *queries)
{
    static int s_opened = 0;
    if (s_opened == 0)
    {
        query_logs_opened();
    }
    s_opened = 1;
    question->code = code.data();
    question->code_size = code.size();
    *queries += 1ULL;
    // the query's key in the record, and what the record contains of it
    unsigned long long hosted = 0xcbf29ce484222325ULL;
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        hosted = record_hash(host[places[place]].c_str(), host[places[place]].size() + 1u, hosted);
    }
    char keyed[160];
    {
        VerbaLinea buffer_line = mmgr_line_over(keyed, sizeof(keyed));
        mmgr_line_hex(&buffer_line, record_hash(code.data(), code.size(), 0xcbf29ce484222325ULL), 16u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_decimal(&buffer_line, question->registers, 1u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_decimal(&buffer_line, question->threads, 1u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_decimal(&buffer_line, question->blocks, 1u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_hex(&buffer_line,
                      record_hash(question->word, sizeof(question->word[0]) * question->cases, 0xcbf29ce484222325ULL),
                      16u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_hex(&buffer_line, hosted, 16u);
        (void)mmgr_line_finish(&buffer_line);
    }
    const std::string key = keyed;
    // R stores a query's verdict and its first case apart, and the query record and the log what came back of every
    // case: with the log retained, a query is read from the query record or put to the part, and R written as ever
    const int recordable = !s_record_ksc.empty() && (question->launches == 0u);
    const auto recorded = (recordable && (s_query_log == NULL)) ? s_record.find(key) : s_record.end();
    if (recorded != s_record.end())
    {
        const QueryRecorded &stored = recorded->second;
        // a query the part answered illegal is censored, its answer stored in R as the part gave it
        question->outcome = (stored.answer == "answers")   ? RUN_ANSWERED
                            : (stored.answer == "illegal") ? RUN_GATED
                                                           : RUN_NOTHING;
        {
            VerbaLinea buffer_line = mmgr_line_over(question->refused, sizeof(question->refused));
            mmgr_line_put(&buffer_line, stored.refusal.c_str());
            (void)mmgr_line_finish(&buffer_line);
        }
        const int alike = (stored.answer == "answers") && (stored.word == 0xffffffffu);
        s_apart_at = (stored.answer == "answers") ? stored.word : 0xffffffffu;
        if (s_trace_log != NULL)
        {
            {
                TextNote text_note;
                text_note_begun(&text_note, &s_trace_log->bytes);
                mmgr_line_put(&text_note.line, "query ");
                mmgr_line_decimal(&text_note.line, *queries, 1u);
                mmgr_line_put(&text_note.line, ": recorded ");
                mmgr_line_put(&text_note.line, stored.answer.c_str());
                mmgr_line_put(&text_note.line, " ");
                mmgr_line_hex(&text_note.line, stored.word, 8u);
                mmgr_line_put(&text_note.line, " ");
                mmgr_line_put(&text_note.line, key.c_str());
                mmgr_line_ch(&text_note.line, '\n');
                text_note_written(&text_note);
            }
            query_log_flushed(s_trace_log);
        }
        if (s_query_log != NULL)
        {
            TextNote text_note;
            text_note_begun(&text_note, &s_query_log->bytes);
            mmgr_line_put(&text_note.line, "query ");
            mmgr_line_decimal(&text_note.line, *queries, 1u);
            mmgr_line_put(&text_note.line, " recorded");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        return alike;
    }
    if (s_trace_log != NULL)
    {
        TextNote text_note;
        text_note_begun(&text_note, &s_trace_log->bytes);
        mmgr_line_put(&text_note.line, "query ");
        mmgr_line_decimal(&text_note.line, *queries, 1u);
        mmgr_line_put(&text_note.line, ": ");
        mmgr_line_decimal(&text_note.line, question->code_size, 1u);
        mmgr_line_put(&text_note.line, " bytes, ");
        mmgr_line_decimal(&text_note.line, question->registers, 1u);
        mmgr_line_put(&text_note.line, " registers, ");
        mmgr_line_decimal(&text_note.line, question->threads, 1u);
        mmgr_line_put(&text_note.line, " threads in ");
        mmgr_line_decimal(&text_note.line, question->blocks, 1u);
        mmgr_line_put(&text_note.line, " blocks, ");
        mmgr_line_decimal(&text_note.line, question->launches, 1u);
        mmgr_line_put(&text_note.line, " launches, ");
        mmgr_line_decimal(&text_note.line, question->cases, 1u);
        mmgr_line_put(&text_note.line, " cases: ");
        text_note_written(&text_note);
    }
    RunQuestion *const single[1] = {question};
    queries_carried(single, &t_dangerous, 1u);
    if (question->outcome != RUN_ANSWERED)
    {
        if (s_trace_log != NULL)
        {
            {
                TextNote text_note;
                text_note_begun(&text_note, &s_trace_log->bytes);
                mmgr_line_put(&text_note.line, "outcome ");
                mmgr_line_decimal(&text_note.line, question->outcome, 1u);
                mmgr_line_put(&text_note.line, ", ");
                mmgr_line_put(&text_note.line, question->refused);
                mmgr_line_ch(&text_note.line, '\n');
                text_note_written(&text_note);
            }
            query_log_flushed(s_trace_log);
        }
        if (s_query_log != NULL)
        {
            TextNote text_note;
            text_note_begun(&text_note, &s_query_log->bytes);
            mmgr_line_put(&text_note.line, "query ");
            mmgr_line_decimal(&text_note.line, *queries, 1u);
            mmgr_line_put(&text_note.line, " refused");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        if (recordable && ((question->outcome == RUN_ILLEGAL) || (question->outcome == RUN_NOTHING)))
        {
            s_record[key] =
                QueryRecorded{(question->outcome == RUN_ILLEGAL) ? "illegal" : "nothing", 0u, question->refused};
        }
        return 0;
    }
    if (s_query_log != NULL)
    {
        // the places this thread's log last wrote as its cases, by a hash of them and their count, a count past every
        // question's where it wrote none
        static thread_local unsigned long long t_logged_hash = 0ULL;
        static thread_local unsigned int t_logged_count = 0xffffffffu;
        const unsigned long long query_hash =
            record_hash(places.data(), sizeof(places[0]) * question->cases, 0xcbf29ce484222325ULL);
        if ((query_hash != t_logged_hash) || (question->cases != t_logged_count))
        {
            {
                TextNote text_note;
                text_note_begun(&text_note, &s_query_log->bytes);
                mmgr_line_put(&text_note.line, "cases");
                text_note_written(&text_note);
            }
            for (unsigned int place = 0u; place < question->cases; place += 1u)
            {
                TextNote text_note;
                text_note_begun(&text_note, &s_query_log->bytes);
                mmgr_line_put(&text_note.line, " ");
                mmgr_line_put(&text_note.line, case_text(places[place]).c_str());
                text_note_written(&text_note);
            }
            {
                TextNote text_note;
                text_note_begun(&text_note, &s_query_log->bytes);
                mmgr_line_ch(&text_note.line, '\n');
                text_note_written(&text_note);
            }
            t_logged_hash = query_hash;
            t_logged_count = question->cases;
        }
        std::string came_back;
        for (unsigned int place = 0u; place < question->cases; place += 1u)
        {
            const int in_bracket = bracket_contains(host[places[place]], question->answered[place]);
            came_back += (in_bracket < 0) ? '-' : (in_bracket != 0) ? '=' : 'x';
        }
        {
            TextNote text_note;
            text_note_begun(&text_note, &s_query_log->bytes);
            mmgr_line_put(&text_note.line, "query ");
            mmgr_line_decimal(&text_note.line, *queries, 1u);
            mmgr_line_put(&text_note.line, " ");
            mmgr_line_put(&text_note.line, came_back.c_str());
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
    }
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const std::string &expected = host[places[place]];
        if (bracket_contains(expected, question->answered[place]) == 0)
        {
            if (s_trace_log != NULL)
            {
                {
                    TextNote text_note;
                    text_note_begun(&text_note, &s_trace_log->bytes);
                    mmgr_line_put(&text_note.line, "apart at case ");
                    mmgr_line_decimal(&text_note.line, places[place], 1u);
                    mmgr_line_put(&text_note.line, ", the host ");
                    mmgr_line_put(&text_note.line, expected.c_str());
                    mmgr_line_put(&text_note.line, " and the part ");
                    mmgr_line_hex(&text_note.line, question->answered[place], 1u);
                    mmgr_line_ch(&text_note.line, '\n');
                    text_note_written(&text_note);
                }
                query_log_flushed(s_trace_log);
            }
            if (recordable)
            {
                s_record[key] = QueryRecorded{"answers", places[place], ""};
            }
            s_apart_at = places[place];
            return 0;
        }
    }
    if (recordable)
    {
        s_record[key] = QueryRecorded{"answers", 0xffffffffu, ""};
    }
    if (s_trace_log != NULL)
    {
        {
            TextNote text_note;
            text_note_begun(&text_note, &s_trace_log->bytes);
            mmgr_line_put(&text_note.line, "alike");
            mmgr_line_put(&text_note.line, (question->launches != 0u)
                                               ? (", " + std::to_string(question->nanoseconds) + " ns").c_str()
                                               : "");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        query_log_flushed(s_trace_log);
    }
    return 1;
}

// `code` put to the part as `question` specifies, to read what it answers, not to check it against the host. R stores a
// verdict alone and cannot answer a reading: the record is neither read nor written. 1 where the part answers it, each
// case's value in `question->answered`, 0 where it refuses it or the gate blocks it, each counted in `queries`
int question_read(const std::vector<unsigned char> &code, RunQuestion *question, unsigned long long *queries)
{
    query_logs_opened();
    question->code = code.data();
    question->code_size = code.size();
    *queries += 1ULL;
    if (s_trace_log != NULL)
    {
        TextNote text_note;
        text_note_begun(&text_note, &s_trace_log->bytes);
        mmgr_line_put(&text_note.line, "query ");
        mmgr_line_decimal(&text_note.line, *queries, 1u);
        mmgr_line_put(&text_note.line, ": ");
        mmgr_line_decimal(&text_note.line, question->code_size, 1u);
        mmgr_line_put(&text_note.line, " bytes, ");
        mmgr_line_decimal(&text_note.line, question->registers, 1u);
        mmgr_line_put(&text_note.line, " registers, ");
        mmgr_line_decimal(&text_note.line, question->threads, 1u);
        mmgr_line_put(&text_note.line, " threads in ");
        mmgr_line_decimal(&text_note.line, question->blocks, 1u);
        mmgr_line_put(&text_note.line, " blocks, ");
        mmgr_line_decimal(&text_note.line, question->launches, 1u);
        mmgr_line_put(&text_note.line, " launches, ");
        mmgr_line_decimal(&text_note.line, question->cases, 1u);
        mmgr_line_put(&text_note.line, " cases, read: ");
        text_note_written(&text_note);
    }
    RunQuestion *const single[1] = {question};
    queries_carried(single, &t_dangerous, 1u);
    const int answered = question->outcome == RUN_ANSWERED;
    if (s_trace_log != NULL)
    {
        {
            TextNote text_note;
            text_note_begun(&text_note, &s_trace_log->bytes);
            mmgr_line_put(&text_note.line, answered ? "answered" : question->refused);
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        query_log_flushed(s_trace_log);
    }
    if (s_query_log != NULL)
    {
        TextNote text_note;
        text_note_begun(&text_note, &s_query_log->bytes);
        mmgr_line_put(&text_note.line, "query ");
        mmgr_line_decimal(&text_note.line, *queries, 1u);
        mmgr_line_put(&text_note.line, " ");
        mmgr_line_put(&text_note.line, answered ? "read" : "refused");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    return answered;
}

// 1 where the part answers `code` alike with the host on every case it computes, in a container declaring every
// register its file uses (sm_86.kdm), 0 where it answers apart, refuses it or the gate blocks it, each counted in
// `queries`
int stall_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *queries)
{
    question->registers = 255u;
    return question_alike(code, host, places, question, queries);
}

// each chain's answers the host computes, one a case over every case, by the chain's number, read from the latest blob
// of `answers` in the run
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
// 0 where either is missing or the listing gives more links than the code contains instructions
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

// The .ksc rewritten by `who` with `rows` in place of every row of the run channel it recorded for a question beginning
// with the words `queried`, whatever came back, the rows put after its last row of the run channel and its counts of
// the run channel taken again (ksc_rows_replaced_traced_by); every row let go that no row put gives again is recorded
// in the part's .krf beside it (krf.h). 1, or 0 where the .ksc could not be written
int ksc_answers_written(const char *ksc, const std::string &queried, const std::vector<std::string> &rows, Who who)
{
    KscFile file = {};
    KfileLines removed = {};
    std::vector<const char *> texts;
    for (const std::string &row : rows)
    {
        texts.push_back(row.c_str());
    }
    const int written = ksc_read(&file, ksc) &&
                        ksc_rows_replaced_traced_by(&file, "run", queried.c_str(), texts.data(),
                                                    (unsigned int)texts.size(), who, &removed) &&
                        ksc_write_by(&file, ksc, who);
    ksc_free(&file);
    if (written && (removed.lines != 0u))
    {
        char krf[1200];
        krf_path_beside(ksc, krf, sizeof(krf));
        if (!krf_removed_by(krf, "ksc", "replaced", removed.line, removed.lines, who))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_decimal(output_line, removed.lines, 1u);
            mmgr_line_put(output_line, " rows let go from ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " could not be recorded in ");
            mmgr_line_put(output_line, krf);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    kfile_lines_free(&removed);
    return written;
}

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
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line,
                      "// the stick's integer questions as the host computes them, written by orior_klq_identity");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "#include <limits>\n#include <stdio.h>\n");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "struct HostIndex\n{\n    unsigned int x, y, z;\n};");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(
            &text_note.line,
            "static const HostIndex threadIdx = {0u, 0u, 0u};\nstatic const HostIndex blockIdx = {0u, 0u, 0u};");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "static const HostIndex blockDim = {1u, 1u, 1u};\n#define __global__\n");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    std::vector<std::string> written;
    for (const std::string &number : numbers)
    {
        const std::string kernel = kernel_text(stick, number);
        if (kernel.empty())
        {
            continue;
        }
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, kernel.c_str());
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        const std::string type = first_type(kernel);
        const int divides = (kernel.find(" / b") != std::string::npos) || (kernel.find(" % b") != std::string::npos) ||
                            (kernel.find("/=") != std::string::npos) || (kernel.find("%=") != std::string::npos) ||
                            (kernel.find(" / ") != std::string::npos);
        const int shifts = (kernel.find("<<") != std::string::npos) || (kernel.find(">>") != std::string::npos);
        // 1 where the host's C would trap on the case or leave it undefined: a divisor of zero, the least value over
        // -1, or a shift by the width or past it
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "static int refused_");
            mmgr_line_put(&text_note.line, number.c_str());
            mmgr_line_put(&text_note.line, "(const unsigned long long *in)\n{");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "    const ");
            mmgr_line_put(&text_note.line, type.c_str());
            mmgr_line_put(&text_note.line, " a = (");
            mmgr_line_put(&text_note.line, type.c_str());
            mmgr_line_put(&text_note.line, ")in[0];\n    const ");
            mmgr_line_put(&text_note.line, type.c_str());
            mmgr_line_put(&text_note.line, " b = (");
            mmgr_line_put(&text_note.line, type.c_str());
            mmgr_line_put(&text_note.line, ")in[1];\n    (void)a;\n    (void)b;");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        if (divides)
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "    if ((b == 0) || (std::numeric_limits<");
            mmgr_line_put(&text_note.line, type.c_str());
            mmgr_line_put(&text_note.line, ">::is_signed && (b == (");
            mmgr_line_put(&text_note.line, type.c_str());
            mmgr_line_put(&text_note.line, ")-1) && (a == std::numeric_limits<");
            mmgr_line_put(&text_note.line, type.c_str());
            mmgr_line_put(&text_note.line, ">::min())))\n    {\n        return 1;\n    }");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        if (shifts)
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "    if (((unsigned long long)b >= (unsigned long long)(sizeof(a + 0) * "
                                           "8u)) || ((long long)b < 0))\n    {\n        return 1;\n    }");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "    return 0;\n}\n");
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        written.push_back(number);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line,
                      "typedef void (*HostKernel)(const unsigned long long *, unsigned long long *, unsigned int);");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "typedef int (*HostRefused)(const unsigned long long *);");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "static const struct\n{\n    const char *number;\n    HostKernel kernel;\n    "
                                       "HostRefused refused;\n} s_questions[] = {");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    for (const std::string &number : written)
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "    {\"");
        mmgr_line_put(&text_note.line, number.c_str());
        mmgr_line_put(&text_note.line, "\", measuring_stick_");
        mmgr_line_put(&text_note.line, number.c_str());
        mmgr_line_put(&text_note.line, ", refused_");
        mmgr_line_put(&text_note.line, number.c_str());
        mmgr_line_put(&text_note.line, "},");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "};\n\nstatic const unsigned long long s_values[] = {");
        text_note_written(&text_note);
    }
    for (unsigned int at = 0u; at < IDENTITY_VALUES; at += 1u)
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, (at == 0u) ? "" : ", ");
        mmgr_line_put(&text_note.line, "0x");
        mmgr_line_hex(&text_note.line, s_values[at], 1u);
        mmgr_line_put(&text_note.line, "ull");
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "};\n");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(
            &text_note.line,
            "// each question a line: its number, then its answer to every case in order, - where the case is refused");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "int main(void)\n{");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(
            &text_note.line,
            "    for (unsigned int at = 0u; at < sizeof(s_questions) / sizeof(s_questions[0]); at += 1u)\n    {");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "        printf(\"");
        mmgr_line_ch(&text_note.line, '%');
        mmgr_line_put(&text_note.line, "s\", s_questions[at].number);");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "        for (unsigned int third = 0u; third < ");
        mmgr_line_decimal(&text_note.line, IDENTITY_THIRDS, 1u);
        mmgr_line_put(&text_note.line, "u; third += 1u)\n        {");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "            for (unsigned int left = 0u; left < ");
        mmgr_line_decimal(&text_note.line, (unsigned int)IDENTITY_VALUES, 1u);
        mmgr_line_put(&text_note.line, "u; left += 1u)\n            {");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "                for (unsigned int right = 0u; right < ");
        mmgr_line_decimal(&text_note.line, (unsigned int)IDENTITY_VALUES, 1u);
        mmgr_line_put(&text_note.line, "u; right += 1u)\n                {");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(
            &text_note.line,
            "                    const unsigned long long in[4] = {s_values[left], s_values[right], third, 0ULL};");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "                    unsigned long long out[2] = {0ULL, 0ULL};");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "                    if (s_questions[at].refused(in))\n                    {");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(
            &text_note.line,
            "                        printf(\" -\");\n                        continue;\n                    }");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "                    s_questions[at].kernel(in, out, 1u);");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "                    printf(\" ");
        mmgr_line_ch(&text_note.line, '%');
        mmgr_line_put(&text_note.line, "llx\", out[0]);\n                }\n            }\n        }");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    {
        TextNote text_note;
        text_note_begun(&text_note, &(&file)->text);
        mmgr_line_put(&text_note.line, "        printf(\"\\n\");\n    }\n    return 0;\n}");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    if (!run_closed(&file))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, path.c_str());
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, path.c_str());
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned int)written.size(), 1u);
        mmgr_line_put(output_line, " questions the host computes");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}
