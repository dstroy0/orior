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
// the case words it reads (carrier_flow.h). A pair apart where a case word decides it that a flag only the form it
// stood as names reads, and no other operand of the link, is a qualifier's. A pair whose forms name the same operands,
// apart where a case word the link reads decides it, is a modifier's, and a frame's where it is alike on every case
// whose operands every frame reads as themselves. A pair of forms of a flag that name the same operands, apart whatever
// the link reads, is a negation's: the two members of one node. A pair answered alike at every put is a qualifier's.
// A pair the part answered apart in this log that no test reads into a set is written unknown_coherence. A pair the
// log holds no answer of keeps the set line the bridge held beneath it, and one that holds none is written
// unknown_coherence: no answer has read it into a set, and it is a member of every one
#include "carrier_flow.h"

#include <stdio.h>

#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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

// the values every frame reads as themselves lie below the top of a byte read signed
static const unsigned long long s_frame_free_below = 0x80ull;

// the first word of a link's text up to its first `.`, its guard passed over
static std::string operation_stem(const std::string &text)
{
    std::stringstream words(carrier_trimmed(text));
    std::string word;
    words >> word;
    if (!word.empty() && (word[0] == '@'))
    {
        words >> word;
    }
    return word.substr(0u, word.find('.'));
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
    long cases_defined = -2;
    const std::vector<CarrierLink> links = carrier_read(
        std::string((std::istreambuf_iterator<char>(carrier)), std::istreambuf_iterator<char>()), &cases_defined);
    const long at = (long)(std::stoul(put.address.substr(colon + 1u), nullptr, 16) / 16u);
    const std::set<std::string> only(put.only.begin(), put.only.end());
    std::set<unsigned long long> qualifying;
    std::set<unsigned long long> qualified;
    // each line of the form at its own place in the carrier, a register an earlier line of the form writes being the
    // form's own and followed no further
    std::set<std::string> form_written;
    long line_at = at;
    for (const std::string &form_line : carrier_pieces(put.from_text, ';'))
    {
        if (carrier_trimmed(form_line).empty())
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register, line_at) == cases_defined))
        {
            qualified.insert(link.offset / s_carrier_operand_bytes);
        }
        for (const std::string &source : link.read)
        {
            if (form_written.count(source) != 0u)
            {
                continue;
            }
            const std::set<unsigned long long> read = carrier_operands_read(links, source, line_at, cases_defined);
            int named_only = 0;
            for (const std::string &operand : only)
            {
                for (const std::string &named : carrier_registers(operand))
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
    // a case word the carrier reads after the link decides the cases apart beside the words the link reads, and is
    // read by neither side of the pair
    int qualifier = 0;
    int link_read = 0;
    for (const unsigned long long operand : deciding)
    {
        qualifier |= ((qualifying.count(operand) != 0u) && (qualified.count(operand) == 0u)) ? 1 : 0;
        link_read |= ((qualified.count(operand) != 0u) || (qualifying.count(operand) != 0u)) ? 1 : 0;
    }
    // a qualifier is a condition, a flag: an operand only one form names that carries a value, the value copied or
    // the addend added, is the value the result is made of and qualifies nothing
    for (const std::string &operand : only)
    {
        for (const std::string &named : carrier_registers(operand))
        {
            qualifier &= (named[0] == 'P') ? 1 : 0;
        }
        qualifier &= carrier_registers(operand).empty() ? 0 : 1;
    }
    if (qualifier)
    {
        return "qualifier_coherence";
    }
    // two forms of a flag apart whatever the link reads, every case apart or apart where only words the link does not
    // read decide it, are the two members of one node, each the other's negation
    int flags = !form_written.empty();
    for (const std::string &written : form_written)
    {
        flags &= (!written.empty() && (written[0] == 'P')) ? 1 : 0;
    }
    if (only.empty() && !link_read && flags)
    {
        return "negation_coherence";
    }
    if (!only.empty() || !link_read)
    {
        return std::string();
    }
    // two readings of the same bits under two frames agree wherever every operand fits every frame, below the top of
    // a byte read signed, the narrowest signed width the cases hold: a modifier apart only past that is the frame's
    int past_every_frame = 1;
    for (size_t place = 0u; (place < cases.size()) && (place < put.came_back.size()); place += 1u)
    {
        if (put.came_back[place] != 'x')
        {
            continue;
        }
        int fits = 1;
        for (const std::string &operand : cases[place])
        {
            fits &= (std::stoull(operand, nullptr, 16) < s_frame_free_below) ? 1 : 0;
        }
        past_every_frame &= fits ? 0 : 1;
    }
    return past_every_frame ? "frame_coherence" : "modifier_coherence";
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
    int block_answered = 0;
    std::string line;
    static const std::regex s_put("^pair (\\S+) in place of (\\S+) at (\\S+)$");
    while (std::getline(log, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        std::smatch found;
        if (line.rfind("cases ", 0u) == 0u)
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
                                     came_back, case_sets.empty() ? 0u : case_sets.size() - 1u});
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
        decoded[key] = set.empty() ? std::string("unknown_coherence") : set;
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
    printf("klq_decoder: %u puts read, %u qualifier_coherence, %u frame_coherence, %u modifier_coherence, %u "
           "negation_coherence, %u unknown_coherence, written to %s\n",
           (unsigned int)puts.size(), counted["qualifier_coherence"], counted["frame_coherence"],
           counted["modifier_coherence"], counted["negation_coherence"], counted["unknown_coherence"], argv[3]);
    return 0;
}
