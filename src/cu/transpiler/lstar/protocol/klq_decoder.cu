// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_decoder.cu: reads the log klq_identity writes beside its trace, and writes the set of our coherence each pair of
// the bridge is read into beneath its verdict in Lstar.klq (P13, the qualifiers and the modifiers)
//
//   klq_decoder <log> <carrier folder> <Lstar.klq>
//
// A pair is read where its two forms are one text but for their modifiers, the first word of each link's text the same
// up to its first `.`. Each put of one form in the other's place that the part answered is read off the cases it
// answered apart on and the carrier it was put in. A case word decides the cases apart where two cases apart in that
// word alone answer one alike and the other apart. Each operand of the link is followed back through the carrier to
// the case words it reads: a register to the nearest link before that writes it, a load through the register the
// carrier's first load reads to the operand its offset names. A pair apart where a case word decides it that an
// operand only the form it stood as names reads, and no other operand of the link, is a qualifier's. A pair whose forms
// name the same operands, apart where a case word the link reads decides it, is a modifier's. A pair answered alike at
// every put is a qualifier's. A pair this log reads into no set keeps the set line the bridge held beneath it, and one
// that holds none is written unknown_coherence: no answer has read it into a set, and it is a member of every one
#include "run_channel.h"

#include <stdio.h>

#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// the bytes of one operand of a case: two words of the run channel
static const unsigned long long s_operand_bytes = 2ull * sizeof(unsigned int);

// one put of a form in another's place, as the log holds it
struct LoggedPut
{
    std::string from;
    std::string to;
    std::string address;
    std::string from_text;
    std::string to_text;
    std::vector<std::string> only;
    // what came back of every case, `=` alike, `x` apart and `-` unbracketed, or `recorded` or `refused`
    std::string came_back;
    // the cases the ask was put over, by their place in the log's case sets
    size_t cases_at;
};

// one link of a carrier: the register it writes, the registers it reads, and the register and offset it loads through
struct CarrierLink
{
    std::string written;
    std::vector<std::string> read;
    int loads;
    std::string address_register;
    unsigned long long offset;
};

// `text` with the space at either end taken off
static std::string trimmed(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    const size_t last = text.find_last_not_of(" \t\r\n");
    return (first == std::string::npos) ? std::string() : text.substr(first, last - first + 1u);
}

// `text` cut at each `separator`
static std::vector<std::string> pieces(const std::string &text, char separator)
{
    std::vector<std::string> cut;
    std::stringstream reading(text);
    std::string piece;
    while (std::getline(reading, piece, separator))
    {
        cut.push_back(piece);
    }
    return cut;
}

// the first word of a link's text up to its first `.`, its guard passed over
static std::string operation_stem(const std::string &text)
{
    std::stringstream words(trimmed(text));
    std::string word;
    words >> word;
    if (!word.empty() && (word[0] == '@'))
    {
        words >> word;
    }
    return word.substr(0u, word.find('.'));
}

// the registers `operand` names, each by the name its pair's low register goes by
static std::vector<std::string> registers_named(const std::string &operand)
{
    static const std::regex s_register("\\b(U?R[0-9]+|P[0-9]+)");
    std::vector<std::string> named;
    for (std::sregex_iterator found(operand.begin(), operand.end(), s_register), end; found != end; ++found)
    {
        named.push_back((*found)[1].str());
    }
    return named;
}

// a link's text read as the register its first operand writes, the registers the rest read, and a memory operand
// after the first read as a load through its register at its offset
static CarrierLink link_read(const std::string &text)
{
    static const std::regex s_memory("\\[(U?R[0-9]+)(\\.64)?(\\+([0-9a-fA-Fx]+))?\\]");
    CarrierLink link{std::string(), {}, 0, std::string(), 0ull};
    std::string rest = trimmed(text);
    if (!rest.empty() && (rest[0] == '@'))
    {
        const size_t space = rest.find_first_of(" \t");
        for (const std::string &guard : registers_named(rest.substr(0u, space)))
        {
            link.read.push_back(guard);
        }
        rest = trimmed(rest.substr(space));
    }
    const size_t space = rest.find_first_of(" \t");
    rest = (space == std::string::npos) ? std::string() : rest.substr(space);
    rest = rest.substr(0u, rest.find(';'));
    const std::vector<std::string> operands = pieces(rest, ',');
    for (size_t at = 0u; at < operands.size(); at += 1u)
    {
        std::smatch memory;
        if (std::regex_search(operands[at], memory, s_memory) && (at > 0u))
        {
            link.loads = 1;
            link.address_register = memory[1].str();
            link.offset = memory[4].matched ? std::stoull(memory[4].str(), nullptr, 0) : 0ull;
        }
        const std::vector<std::string> named = registers_named(operands[at]);
        for (size_t each = 0u; each < named.size(); each += 1u)
        {
            if ((at == 0u) && (each == 0u) && memory.empty())
            {
                link.written = named[each];
                continue;
            }
            link.read.push_back(named[each]);
        }
    }
    return link;
}

// the nearest link of `links` before `before` that writes `register_name`, and -1 where none does
static long definition_of(const std::vector<CarrierLink> &links, const std::string &register_name, long before)
{
    for (long at = before - 1; at >= 0; at -= 1)
    {
        if (links[(size_t)at].written == register_name)
        {
            return at;
        }
    }
    return -1;
}

// the case operands `register_name` reads at link `before` of `links`, the cases loaded through the register written
// at `cases_defined`
static std::set<unsigned long long> operands_read(const std::vector<CarrierLink> &links, const std::string &register_name,
                                                 long before, long cases_defined)
{
    std::set<unsigned long long> read;
    const long defined = definition_of(links, register_name, before);
    if (defined < 0)
    {
        return read;
    }
    const CarrierLink &link = links[(size_t)defined];
    if (link.loads && (definition_of(links, link.address_register, defined) == cases_defined))
    {
        read.insert(link.offset / s_operand_bytes);
        return read;
    }
    for (const std::string &source : link.read)
    {
        const std::set<unsigned long long> further = operands_read(links, source, defined, cases_defined);
        read.insert(further.begin(), further.end());
    }
    return read;
}

// the case operands that decide which cases of `cases` came back apart in `came_back`
static std::set<unsigned long long> operands_deciding(const std::vector<std::vector<std::string>> &cases,
                                                     const std::string &came_back)
{
    std::set<unsigned long long> deciding;
    const size_t operands = cases.empty() ? 0u : cases[0].size();
    for (size_t operand = 0u; operand < operands; operand += 1u)
    {
        std::map<std::string, std::set<char>> by_rest;
        for (size_t place = 0u; (place < cases.size()) && (place < came_back.size()); place += 1u)
        {
            if (came_back[place] == '-')
            {
                continue;
            }
            std::string rest;
            for (size_t other = 0u; other < cases[place].size(); other += 1u)
            {
                rest += (other == operand) ? std::string("*,") : (cases[place][other] + ",");
            }
            by_rest[rest].insert(came_back[place]);
        }
        for (const auto &held : by_rest)
        {
            if (held.second.size() > 1u)
            {
                deciding.insert(operand);
                break;
            }
        }
    }
    return deciding;
}

// the set of our coherence a put apart is read into, and empty where it is read into none
static std::string put_decoded(const LoggedPut &put, const std::vector<std::vector<std::string>> &cases,
                               const std::string &folder)
{
    const size_t colon = put.address.find(':');
    std::ifstream carrier(folder + "/" + put.address.substr(0u, colon) + ".sass", std::ios::binary);
    if (!carrier || (colon == std::string::npos))
    {
        return std::string();
    }
    std::vector<CarrierLink> links;
    std::string line;
    long cases_defined = -2;
    while (std::getline(carrier, line))
    {
        links.push_back(link_read(line));
        const CarrierLink &read = links.back();
        if (read.loads && (cases_defined == -2))
        {
            cases_defined = definition_of(links, read.address_register, (long)links.size() - 1);
        }
    }
    const long at = (long)(std::stoul(put.address.substr(colon + 1u), nullptr, 16) / 16u);
    const std::set<std::string> only(put.only.begin(), put.only.end());
    std::set<unsigned long long> qualifying;
    std::set<unsigned long long> qualified;
    // each line of the form at its own place in the carrier, a register an earlier line of the form writes being the
    // form's own and followed no further
    std::set<std::string> form_written;
    long line_at = at;
    for (const std::string &form_line : pieces(put.from_text, ';'))
    {
        if (trimmed(form_line).empty())
        {
            continue;
        }
        const CarrierLink link = link_read(form_line);
        if (link.loads && (definition_of(links, link.address_register, line_at) == cases_defined))
        {
            qualified.insert(link.offset / s_operand_bytes);
        }
        for (const std::string &source : link.read)
        {
            if (form_written.count(source) != 0u)
            {
                continue;
            }
            const std::set<unsigned long long> read = operands_read(links, source, line_at, cases_defined);
            int named_only = 0;
            for (const std::string &operand : only)
            {
                for (const std::string &named : registers_named(operand))
                {
                    named_only |= (named == source) ? 1 : 0;
                }
            }
            (named_only ? qualifying : qualified).insert(read.begin(), read.end());
        }
        form_written.insert(link.written);
        line_at += 1;
    }
    const std::set<unsigned long long> deciding = operands_deciding(cases, put.came_back);
    if (deciding.empty())
    {
        return std::string();
    }
    // a case word the carrier reads after the link decides the cases apart beside the words the link reads, and is
    // read by neither side of the pair
    int qualifier = 0;
    int link_read = 0;
    for (const unsigned long long operand : deciding)
    {
        qualifier |= ((qualifying.count(operand) != 0u) && (qualified.count(operand) == 0u)) ? 1 : 0;
        link_read |= (qualified.count(operand) != 0u) ? 1 : 0;
    }
    if (qualifier)
    {
        return "qualifier_coherence";
    }
    return (only.empty() && link_read) ? "modifier_coherence" : std::string();
}

// the two forms of a pair, the lesser first, as one key
static std::string pair_key(const std::string &first, const std::string &second)
{
    return (first < second) ? (first + " " + second) : (second + " " + first);
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        printf("klq_decoder <log> <carrier folder> <Lstar.klq>\n");
        return 1;
    }
    std::ifstream log(argv[1], std::ios::binary);
    if (!log)
    {
        printf("klq_decoder: %s could not be read\n", argv[1]);
        return 1;
    }
    std::vector<std::vector<std::vector<std::string>>> case_sets;
    std::vector<LoggedPut> puts;
    std::string came_back;
    std::string line;
    static const std::regex s_put("^pair (\\S+) in place of (\\S+) at (\\S+)$");
    while (std::getline(log, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        std::smatch found;
        if (line.rfind("cases ", 0u) == 0u)
        {
            std::vector<std::vector<std::string>> cases;
            for (const std::string &written : pieces(line.substr(6u), ' '))
            {
                cases.push_back(pieces(written, ','));
            }
            case_sets.push_back(cases);
        }
        else if (line.rfind("ask ", 0u) == 0u)
        {
            std::stringstream words(line);
            std::string kind;
            std::string number;
            words >> kind >> number >> came_back;
        }
        else if (std::regex_match(line, found, s_put))
        {
            puts.push_back(LoggedPut{found[2].str(), found[1].str(), found[3].str(), std::string(), std::string(), {},
                                     came_back, case_sets.empty() ? 0u : case_sets.size() - 1u});
        }
        else if (!puts.empty() && (line.rfind("from ", 0u) == 0u))
        {
            puts.back().from_text = line.substr(5u);
        }
        else if (!puts.empty() && (line.rfind("to ", 0u) == 0u))
        {
            puts.back().to_text = line.substr(3u);
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
    // each pair's set, read off its put apart, or a qualifier's where every put the part answered came back alike
    std::map<std::string, std::string> decoded;
    std::map<std::string, int> apart_held;
    std::map<std::string, int> alike_held;
    for (const LoggedPut &put : puts)
    {
        const std::string key = pair_key(put.from, put.to);
        if ((put.came_back == "recorded") || (put.came_back == "refused") || put.came_back.empty() ||
            (operation_stem(put.from_text) != operation_stem(put.to_text)))
        {
            continue;
        }
        if (put.came_back.find('x') == std::string::npos)
        {
            alike_held[key] = 1;
            continue;
        }
        apart_held[key] = 1;
        const std::string set =
            case_sets.empty() ? std::string() : put_decoded(put, case_sets[put.cases_at], argv[2]);
        if (!set.empty())
        {
            decoded[key] = set;
        }
    }
    for (const auto &held : alike_held)
    {
        if (apart_held.count(held.first) == 0u)
        {
            decoded[held.first] = "qualifier_coherence";
        }
    }
    std::vector<std::string> bridge;
    std::ifstream held_bridge(argv[3], std::ios::binary);
    while (std::getline(held_bridge, line))
    {
        bridge.push_back((!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line);
    }
    held_bridge.close();
    const std::string suffix = "_coherence";
    const auto set_line = [&](const std::string &entry) {
        return (entry.find(' ') == std::string::npos) && (entry.size() > suffix.size()) &&
               (entry.compare(entry.size() - suffix.size(), suffix.size(), suffix) == 0);
    };
    std::vector<std::string> written;
    std::string pair_held;
    std::string set_held;
    std::map<std::string, unsigned int> counted;
    const auto pair_closed = [&]() {
        if (!pair_held.empty())
        {
            const std::string set = (decoded.count(pair_held) != 0u) ? decoded[pair_held]
                                    : set_held.empty()                ? std::string("unknown_coherence")
                                                                      : set_held;
            written.push_back(set);
            counted[set] += 1u;
        }
        pair_held.clear();
        set_held.clear();
    };
    for (const std::string &entry : bridge)
    {
        const int verdict = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        if (!pair_held.empty() && set_line(entry))
        {
            set_held = entry;
            continue;
        }
        if (!pair_held.empty() && verdict)
        {
            written.push_back(entry);
            continue;
        }
        pair_closed();
        written.push_back(entry);
        if (entry.rfind("pair ", 0u) == 0u)
        {
            std::stringstream words(entry);
            std::string kind;
            std::string first;
            std::string second;
            words >> kind >> first >> second;
            pair_held = pair_key(first, second);
        }
    }
    pair_closed();
    FILE *const file = fopen(argv[3], "wb");
    if (file == NULL)
    {
        printf("klq_decoder: %s could not be written\n", argv[3]);
        return 1;
    }
    for (const std::string &kept : written)
    {
        fprintf(file, "%s\n", kept.c_str());
    }
    fclose(file);
    for (const auto &held : decoded)
    {
        printf("  %s: %s\n", held.first.c_str(), held.second.c_str());
    }
    printf("klq_decoder: %u puts read, %u qualifier_coherence, %u modifier_coherence, %u unknown_coherence, written to "
           "%s\n",
           (unsigned int)puts.size(), counted["qualifier_coherence"], counted["modifier_coherence"],
           counted["unknown_coherence"], argv[3]);
    return 0;
}
