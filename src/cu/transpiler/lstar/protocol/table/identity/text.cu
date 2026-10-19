// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.cu: the queue of the stick's manifest read against the record, and the text identity each side of a slice is put through
#include "identity_internal.h"

// The queue over the stick's questions (P13, the scheduler). Every question of the manifest is an entry of its
// category, and R gives each what it reads: 1 where the part answers it as the host does on every case the host
// computes it on, 0 where it answers one case otherwise or refuses it, and gray where nothing has settled it. A
// gray entry carries what keeps it gray: the engine's note where the engine writes no chain for it, that the host
// computes no answer to it, or that the gate holds it off the part. A gray entry with a chain and a host answer is
// put through the run channel, every case at once in a container declaring every register its file holds, and its
// answer is written to R. A question R has settled is never put again. Each ask settles one entry and no other:
// the questions of the stick share no case, and the order the entries are put in settles no more of them than another
// order would. The run's blob queue.txt holds every entry with its category, what it reads and what keeps it gray, and
// each category's entries are counted on the standard output as a row of P12's table
int identity_queue(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                          const char *manifest, const char *const *carrier)
{
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    // the engine's answer to each question and its note where it writes no chain
    std::map<std::string, std::string> notes;
    std::ifstream written(std::string(engine) + "/engine.tsv", std::ios::binary);
    std::string line;
    while (std::getline(written, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        std::vector<std::string> cells;
        std::stringstream row(line);
        std::string cell;
        while (std::getline(row, cell, '\t'))
        {
            cells.push_back(cell);
        }
        if ((cells.size() >= 2u) && (cells[1] != "answered") && (cells[0] != "number"))
        {
            notes[cells[0]] = (cells.size() >= 5u) ? cells[4] : cells[1];
        }
    }
    std::vector<std::pair<std::string, std::string>> entries;
    std::ifstream listed(manifest, std::ios::binary);
    while (std::getline(listed, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        const size_t first_tab = line.find('\t');
        const size_t second_tab = line.find('\t', first_tab + 1u);
        if ((first_tab == std::string::npos) || (second_tab == std::string::npos) || (line.compare(0u, 6u, "number") == 0))
        {
            continue;
        }
        entries.emplace_back(line.substr(0u, first_tab), line.substr(first_tab + 1u, second_tab - first_tab - 1u));
    }
    if (entries.empty())
    {
        printf("klq_identity queue: %s lists no question\n", manifest);
        return 1;
    }
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "queue");
    RunWritten table = {(folder + "/queue.txt"), 0, std::string()};
    // a chain of ours holds no slot, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.slot = RUN_SLOT_NONE;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    // each category's entries counted by what came of them, in the order the manifest first names the category
    std::vector<std::string> categories;
    std::map<std::string, std::map<std::string, unsigned int>> counted;
    for (const auto &entry : entries)
    {
        const std::string &number = entry.first;
        const std::string &category = entry.second;
        if (counted.count(category) == 0u)
        {
            categories.push_back(category);
        }
        counted[category]["questions"] += 1u;
        std::string reads = "gray";
        std::string reason;
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if ((notes.count(number) != 0u) || !chain_code_read(engine, number, &code, &links))
        {
            reason = (notes.count(number) != 0u) ? notes[number] : "the engine writes no chain";
        }
        else if (host.count(number) == 0u)
        {
            counted[category]["written"] += 1u;
            reason = "the host computes no answer";
        }
        else
        {
            counted[category]["written"] += 1u;
            counted[category]["held"] += 1u;
            const int alike = stall_alike(code, host[number], places, &s_question, &asks);
            reads = alike ? "1" : (s_question.outcome == RUN_HELD) ? "gray" : "0";
            reason = alike ? "alike"
                     : (s_question.outcome == RUN_ANSWERED) ? "apart"
                     : (s_question.outcome == RUN_HELD)     ? std::string("the gate holds it: ") + s_question.refused
                                                            : std::string("refused: ") + s_question.refused;
            counted[category]["alike"] += alike ? 1u : 0u;
        }
        counted[category][reads] += 1u;
        run_print(&table, "%s\t%s\t%s\t%s\n", number.c_str(), category.c_str(), reads.c_str(), reason.c_str());
    }
    if (!run_closed(&table))
    {
        printf("klq_identity queue: %s/queue.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    run_channel_close();
    printf("| category | questions | written | held | alike | 1 | 0 | gray |\n");
    std::map<std::string, unsigned int> all;
    for (const std::string &category : categories)
    {
        std::map<std::string, unsigned int> &row = counted[category];
        printf("| %s | %u | %u | %u | %u | %u | %u | %u |\n", category.c_str(), row["questions"], row["written"],
               row["held"], row["alike"], row["1"], row["0"], row["gray"]);
        for (const char *const column : {"questions", "written", "held", "alike", "1", "0", "gray"})
        {
            all[column] += row[column];
        }
    }
    printf("| all | %u | %u | %u | %u | %u | %u | %u |\n", all["questions"], all["written"], all["held"], all["alike"],
           all["1"], all["0"], all["gray"]);
    printf("klq_identity queue: %llu asks, %u read 1, %u read 0, %u gray\n", asks, all["1"], all["0"], all["gray"]);
    return (all["0"] == 0u) ? 0 : 1;
}

// a case of the host, its operands and its third, as an identity's verdict writes it
std::string case_text(unsigned int at)
{
    const unsigned int third = at / (IDENTITY_VALUES * IDENTITY_VALUES);
    const unsigned int left = (at / IDENTITY_VALUES) % IDENTITY_VALUES;
    const unsigned int right = at % IDENTITY_VALUES;
    char written[96];
    snprintf(written, sizeof(written), "%llx,%llx,%x", s_values[left], s_values[right], third);
    return written;
}

// The bridge's identities between texts held on the part (P13, the identities). Each side of
// `text_identity <text> = <text>` is a question of the sides' stick, its chain the engine's and its answers the host's,
// found by its text in the sides' manifest. An identity closes at the first case the host answers its two sides apart
// on, the texts then two meanings, and at the first case the part answers a side apart from the host on, the sides
// then not held to each other there. Where the host answers the two sides alike on every case it computes them on and
// the part answers each as the host does, the identity is open with its count of cases, every case both sides are
// computed on. A side with no chain, no host answer, or that the part refuses or the gate holds is not asked, and its
// identity is open with 0. Each identity's verdict is written beneath it in the bridge in place of the one it held
int identity_text(const char *engine, const char *answers, const char *ksc, const char *klq,
                         const char *manifest, const char *const *carrier)
{
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    std::map<std::string, std::string> numbered;
    std::ifstream listed(manifest, std::ios::binary);
    std::string line;
    while (std::getline(listed, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        const size_t first_tab = line.find('\t');
        const size_t second_tab = line.find('\t', first_tab + 1u);
        if ((first_tab != std::string::npos) && (second_tab != std::string::npos))
        {
            numbered[line.substr(second_tab + 1u)] = line.substr(0u, first_tab);
        }
    }
    std::vector<std::string> bridge;
    std::istringstream held(run_file_text(klq));
    while (std::getline(held, line))
    {
        bridge.push_back((!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line);
    }
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "text_identity");
    // a chain of ours holds no slot, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.slot = RUN_SLOT_NONE;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    // a side put to the part: 1 where it answers as the host does on every case, 0 where it answers one apart, its
    // place in s_apart_at, and -1 where it is not asked
    const auto side_alike = [&](const std::string &number) -> int {
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if ((host.count(number) == 0u) || !chain_code_read(engine, number, &code, &links))
        {
            return -1;
        }
        const int alike = stall_alike(code, host[number], places, &s_question, &asks);
        return alike ? 1 : (s_question.outcome == RUN_ANSWERED) ? 0 : -1;
    };
    std::vector<std::string> written;
    unsigned int open = 0u;
    unsigned int closed = 0u;
    unsigned int unasked = 0u;
    // every verdict line the bridge held beneath an identity between texts, however many, gives way to the one written
    int beneath = 0;
    for (size_t at = 0u; at < bridge.size(); at += 1u)
    {
        const std::string &entry = bridge[at];
        const int verdict_line = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        beneath = (entry.rfind("text_identity ", 0u) == 0u) || (beneath && verdict_line);
        if (verdict_line && beneath)
        {
            continue;
        }
        written.push_back(entry);
        if (entry.rfind("text_identity ", 0u) != 0u)
        {
            continue;
        }
        const std::string sides = entry.substr(14u);
        const size_t equals = sides.find(" = ");
        const std::string one = (equals == std::string::npos) ? sides : sides.substr(0u, equals);
        const std::string other = (equals == std::string::npos) ? std::string() : sides.substr(equals + 3u);
        const std::string first = (numbered.count(one) != 0u) ? numbered[one] : std::string();
        const std::string second = (numbered.count(other) != 0u) ? numbered[other] : std::string();
        std::string verdict = "open 0";
        if (!first.empty() && !second.empty() && (host.count(first) != 0u) && (host.count(second) != 0u))
        {
            unsigned int alike = 0u;
            std::string apart;
            for (const unsigned int place : places)
            {
                const std::string &left = host[first][place];
                const std::string &right = host[second][place];
                if ((left == "-") || (right == "-"))
                {
                    continue;
                }
                if (left != right)
                {
                    apart = "closed " + case_text(place) + "->" + left + "," + right;
                    break;
                }
                alike += 1u;
            }
            const int first_alike = apart.empty() ? side_alike(first) : -1;
            const unsigned int first_apart = s_apart_at;
            const int second_alike = (apart.empty() && (first_alike >= 0)) ? side_alike(second) : -1;
            const unsigned int second_apart = s_apart_at;
            if (!apart.empty())
            {
                verdict = apart;
            }
            else if ((first_alike == 0) || (second_alike == 0))
            {
                const unsigned int place = (first_alike == 0) ? first_apart : second_apart;
                verdict = "closed " + case_text(place) + "->" + host[first][place] + ", the part apart on " +
                          ((first_alike == 0) ? one : other);
            }
            else if ((first_alike == 1) && (second_alike == 1))
            {
                verdict = "open " + std::to_string(alike);
            }
        }
        open += (verdict.rfind("open ", 0u) == 0u) && (verdict != "open 0") ? 1u : 0u;
        closed += (verdict.rfind("closed ", 0u) == 0u) ? 1u : 0u;
        unasked += (verdict == "open 0") ? 1u : 0u;
        if (s_ask_trace != NULL)
        {
            ask_written(s_ask_trace, "text_identity %s [%s %s]: %s\n", sides.c_str(), first.c_str(), second.c_str(),
                        verdict.c_str());
            ask_flushed(s_ask_trace);
        }
        written.push_back(verdict);
    }
    run_channel_close();
    RunWritten file = {klq, 1, std::string()};
    for (const std::string &kept : written)
    {
        run_print(&file, "%s\n", kept.c_str());
    }
    if (!run_closed(&file))
    {
        printf("klq_identity text_identity: %s could not be written\n", klq);
        return 1;
    }
    printf("klq_identity text_identity: %llu asks, %u open, %u closed, %u not asked, written to %s\n", asks, open,
           closed, unasked, klq);
    return 0;
}
