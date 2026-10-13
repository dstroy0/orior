// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// log.cu: the log read off the run into the puts, case sets, metas, categories and choices, and each pair's set,
// product and identity decoded off its puts
#include "decoder_internal.h"

int Decoding::log_read(const char *qry)
{
    // the log, every blob of it joined in the order handed: from the run's buffer while its writer runs, from the .qry
    // once it has stopped (qry_buffer.h)
    unsigned char *joined = NULL;
    unsigned long long joined_size = 0ull;
    char joined_error[512] = "";
    QryBuffer *const run = qry_open(qry);
    const int logged = (run != NULL)
                           ? qry_joined(run, "log", &joined, &joined_size)
                           : qry_file_joined(qry, "log", &joined, &joined_size, joined_error, sizeof(joined_error));
    qry_close(run);
    if (!logged)
    {
        printf("klq_decoder: %s\n", (joined_error[0] != '\0') ? joined_error : "the log could not be read");
        return 1;
    }
    if (joined_error[0] != '\0')
    {
        printf("klq_decoder: %s\n", joined_error);
    }
    std::istringstream log(std::string((const char *)joined, (size_t)joined_size));
    free(joined);
    std::string came_back;
    int block_answered = 0;
    std::string line;
    static const std::regex s_put("^pair (\\S+) in place of (\\S+) at (\\S+)$");
    std::string trace_error;
    if (!query_trace_read(query_trace_path(), &trace, &trace_error))
    {
        printf("klq_decoder: %s\n", trace_error.c_str());
        return 1;
    }
    std::string choosing;
    // the pairs the lower witness answered: a choice of a link of a third form the part answered alike or apart
    const unsigned long long lower_flag = query_trace_flag(trace, "lower");
    const unsigned long long answer_flags = query_trace_flag(trace, "alike") | query_trace_flag(trace, "apart");
    while (std::getline(log, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        std::smatch found;
        if (line.rfind("choice ", 0u) == 0u)
        {
            const unsigned long long word = std::stoull(line.substr(7u), nullptr, 16);
            if (((word & lower_flag) != 0ull) && ((word & answer_flags) != 0ull))
            {
                witnessed.insert(choosing);
            }
            for (const auto &flag : trace.named)
            {
                if ((word & flag.first) != 0ull)
                {
                    choices[choosing][flag.first] += 1u;
                    chosen[flag.first] += 1u;
                }
            }
        }
        else if (line.rfind("facts ", 0u) == 0u)
        {
            std::stringstream words(line.substr(6u));
            std::string first;
            std::string second;
            std::string fact;
            words >> first >> second;
            const std::string key = pair_key(first, second);
            choosing = key;
            faceted.insert(key);
            while (words >> fact)
            {
                const std::string set = fact + "_coherence";
                (named_in(s_categories, std::size(s_categories), set) ? categories : metas)[key].insert(set);
            }
        }
        else if (!puts.empty() && (line.rfind("binds", 0u) == 0u))
        {
            metas[pair_key(puts.back().from, puts.back().to)].insert("structural_coherence");
        }
        else if (line.rfind("cases ", 0u) == 0u)
        {
            std::vector<std::vector<std::string>> cases;
            for (const std::string &written : carrier_pieces(line.substr(6u), ' '))
            {
                cases.push_back(carrier_pieces(written, ','));
            }
            case_sets.push_back(cases);
        }
        else if (line.rfind("ask ", 0u) == 0u)
        {
            // a put walks its registers over several asks, and what the part answered of every case is the same at
            // every count it answered at: the put keeps an answered ask's cases over one read from R or refused
            std::stringstream words(line);
            std::string kind;
            std::string number;
            std::string ask_came_back;
            words >> kind >> number >> ask_came_back;
            const int answered = (ask_came_back != "recorded") && (ask_came_back != "refused");
            came_back = (answered || !block_answered) ? ask_came_back : came_back;
            block_answered |= answered ? 1 : 0;
        }
        else if (std::regex_match(line, found, s_put))
        {
            puts.push_back(LoggedPut{found[2].str(), found[1].str(), found[3].str(), std::string(), std::string(), {},
                                     came_back, case_sets.empty() ? 0u : case_sets.size() - 1u, {}, {}, {}, {}});
            came_back.clear();
            block_answered = 0;
        }
        else if (!puts.empty() && (line.rfind("from ", 0u) == 0u))
        {
            puts.back().from_text = line.substr(5u);
        }
        else if (!puts.empty() && (line.rfind("to ", 0u) == 0u))
        {
            puts.back().to_text = line.substr(3u);
        }
        else if (!puts.empty() && (line.rfind("reads ", 0u) == 0u))
        {
            std::stringstream words(line.substr(6u));
            std::string read;
            std::string value;
            words >> read;
            std::vector<unsigned long long> values;
            while ((words >> value) && (value != "refused"))
            {
                values.push_back(std::stoull(value, nullptr, 16));
            }
            puts.back().reads.push_back(std::make_pair(read, values));
        }
        else if (!puts.empty() && ((line.rfind("qualifies", 0u) == 0u) || (line.rfind("qualified", 0u) == 0u)))
        {
            std::stringstream words(line.substr(9u));
            std::string value;
            while (words >> value)
            {
                puts.back().qualifiers.insert(std::stoull(value, nullptr, 16));
            }
        }
        else if (!puts.empty() && (line.rfind("writes ", 0u) == 0u))
        {
            std::stringstream words(line.substr(7u));
            std::string side;
            std::string value;
            words >> side;
            std::vector<unsigned long long> &values = (side == "from") ? puts.back().writes_from : puts.back().writes_to;
            while ((words >> value) && (value != "refused"))
            {
                values.push_back(std::stoull(value, nullptr, 16));
            }
        }
        else if (!puts.empty() && (line.rfind("only", 0u) == 0u))
        {
            std::stringstream words(line.substr(4u));
            std::string operand;
            while (words >> operand)
            {
                puts.back().only.push_back(operand);
            }
        }
    }
    return 0;
}

void Decoding::products_decoded(const char *engine)
{
    // each answered pair's product, its rows over every put the part answered and read
    std::vector<unsigned long long> operands;
    for (const auto &cases : case_sets)
    {
        for (const auto &each : cases)
        {
            for (const std::string &operand : each)
            {
                operands.push_back(std::stoull(operand, nullptr, 16));
            }
        }
    }
    const std::vector<std::set<unsigned long long>> cased = concept_values(operands);
    for (const LoggedPut &put : puts)
    {
        pair_qualifiers[pair_key(put.from, put.to)].insert(put.qualifiers.begin(), put.qualifiers.end());
    }
    std::map<std::string, std::vector<std::set<unsigned long long>>> pair_cased;
    for (const auto &held : pair_qualifiers)
    {
        pair_cased[held.first] = concept_values_qualified(cased, held.second);
    }
    std::map<std::string, std::set<std::string>> apart_sets;
    std::map<std::string, int> apart_held;
    std::map<std::string, int> alike_held;
    for (const LoggedPut &put : puts)
    {
        const std::string key = pair_key(put.from, put.to);
        // a put past the pair's first case apart asks nothing of its verdict, and holds its product alone
        concept_product_held(put.reads, put.writes_from, put.writes_to, pair_cased[key], &products[key]);
        if ((put.came_back == "recorded") || (put.came_back == "refused") || put.came_back.empty() ||
            case_sets.empty())
        {
            continue;
        }
        const int one_text = (operation_stem(put.from_text) == operation_stem(put.to_text));
        if (put.came_back.find('x') == std::string::npos)
        {
            alike_held[key] |= one_text ? 1 : 2;
            continue;
        }
        apart_held[key] = 1;
        const std::string set = put_decoded(put, case_sets[put.cases_at], engine);
        if (!set.empty())
        {
            apart_sets[key].insert(one_text ? set : std::string("unknown_coherence"));
        }
    }
    // a pair is a member of every set its puts apart read it into, each a light of its own, and of `unknown_coherence`
    // where they read it into none
    for (const auto &held : apart_held)
    {
        std::set<std::string> sets = apart_sets[held.first];
        if (sets.size() > 1u)
        {
            sets.erase("unknown_coherence");
        }
        decoded[held.first] = sets.empty() ? std::set<std::string>{"unknown_coherence"} : sets;
    }
    for (const auto &held : alike_held)
    {
        if (apart_held.count(held.first) == 0u)
        {
            decoded[held.first] = {(held.second == 1) ? "qualifier_coherence" : "unknown_coherence"};
        }
    }
    // a pair no put of its own answered, witnessed at a third form's link: the part answered its two forms there,
    // alike or apart, and the pair is known, read out of unknown_coherence into witness_coherence
    for (const std::string &key : witnessed)
    {
        if (decoded.count(key) == 0u)
        {
            decoded[key] = {"witness_coherence"};
        }
    }
    for (const auto &held : products)
    {
        const std::string identity = concept_identity(held.second, pair_cased[held.first]);
        if (!identity.empty())
        {
            identities[held.first] = identity;
        }
        const auto read_into = decoded.find(held.first);
        if (!identity.empty() && (read_into != decoded.end()) &&
            (read_into->second == std::set<std::string>{"unknown_coherence"}) &&
            concept_qualified(held.second, pair_qualifiers[held.first]))
        {
            read_into->second = {"qualifier_coherence"};
        }
        // a row both alike and apart is decided by something outside the values the form reads: noise. An answered
        // product the cycle left short of whole is one the cases could not make whole: the data's quality
        int both = 0;
        for (const auto &row : held.second.rows)
        {
            both |= (row.second.size() > 1u) ? 1 : 0;
        }
        if (both)
        {
            metas[held.first].insert("noise_coherence");
        }
        if (!held.second.rows.empty() && identity.empty() && (decoded.count(held.first) != 0u))
        {
            metas[held.first].insert("data_quality_coherence");
        }
    }
}
