// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_trace.cu: the trace's flags read from their table, and a word of them written back by name
#include "query_trace.h"

#include <fstream>
#include <sstream>

std::string query_trace_path(void)
{
    const std::string file = __FILE__;
    const size_t slash = file.find_last_of("/\\");
    const std::string here = (slash == std::string::npos) ? std::string() : file.substr(0u, slash + 1u);
    return here + "../klq/query_trace.tsv";
}

int query_trace_read(const std::string &path, QueryTrace *trace, std::string *error)
{
    *trace = QueryTrace();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        *error = path + " could not be opened";
        return 0;
    }
    std::string line;
    unsigned int number = 0u;
    unsigned long long held = 0ull;
    while (std::getline(file, line))
    {
        number += 1u;
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        if ((number == 1u) || line.empty() || (line[0] == '#'))
        {
            continue;
        }
        std::stringstream cells(line);
        std::string flag;
        std::string name;
        std::string gloss;
        std::getline(cells, flag, '\t');
        std::getline(cells, name, '\t');
        std::getline(cells, gloss);
        const unsigned long long word = std::stoull(flag, nullptr, 16);
        if ((word == 0ull) || ((word & (word - 1ull)) != 0ull) || ((held & word) != 0ull) || name.empty() ||
            (trace->flags.count(name) != 0u))
        {
            *error =
                path + ":" + std::to_string(number) + ": a flag of more than one bit, or one bit or one name twice";
            return 0;
        }
        held |= word;
        trace->flags[name] = word;
        trace->named.push_back(std::make_pair(word, name));
        trace->glosses.push_back(gloss);
    }
    return 1;
}

unsigned long long query_trace_flag(const QueryTrace &trace, const std::string &name)
{
    const auto found = trace.flags.find(name);
    return (found == trace.flags.end()) ? 0ull : found->second;
}

std::string query_trace_names(const QueryTrace &trace, unsigned long long word)
{
    std::string names;
    for (const auto &flag : trace.named)
    {
        if ((word & flag.first) != 0ull)
        {
            names += (names.empty() ? "" : " ") + flag.second;
        }
    }
    return names;
}
