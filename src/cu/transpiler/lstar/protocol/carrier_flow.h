// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carrier_flow.h: a carrier's links read as what each writes, reads and loads, and each register followed back to the
// operands of a case it reads (P13, the pairs)
//
// A link writes the register its first operand names and reads every other register it names; a memory operand after
// the first is a load through its register at its offset. A register is followed to the nearest link before that writes
// it, and a load through the register the carrier's first load reads is the operand of a case its offset names. A link
// carries the cases where it loads one of their operands or reads a register one of them reaches: every other link is
// the carrier's own, the thread's index, the case's index, the bound and the addresses, and a form put there moves the
// case each thread reads. The links between a case's load and the link are the carrier's delta, apart from the form's.
// klq_identity puts a form only where the link carries the cases, the least delta first, and klq_decoder reads each
// put's operands back to the case words through the same links
#ifndef CARRIER_FLOW_H
#define CARRIER_FLOW_H

#include "run_channel.h"

#include <algorithm>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// the bytes of one operand of a case: two words of the run channel
static const unsigned long long s_carrier_operand_bytes = 2ull * sizeof(unsigned int);

// one link of a carrier: the register it writes, the registers it reads, each also as the link writes it, a pair's
// high word with its `.hi`, and the register and offset it loads through
struct CarrierLink
{
    std::string written;
    std::vector<std::string> read;
    std::vector<std::string> read_written;
    int loads;
    std::string address_register;
    unsigned long long offset;
};

// `text` with the space at either end taken off
static inline std::string carrier_trimmed(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    const size_t last = text.find_last_not_of(" \t\r\n");
    return (first == std::string::npos) ? std::string() : text.substr(first, last - first + 1u);
}

// `text` cut at each `separator`
static inline std::vector<std::string> carrier_pieces(const std::string &text, char separator)
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

// the registers `operand` names, each by the name its pair's low register goes by, or with `whole` as the operand
// writes it, a pair's high word with its `.hi`
static inline std::vector<std::string> carrier_registers(const std::string &operand, int whole = 0)
{
    static const std::regex s_register("\\b(U?R[0-9]+|P[0-9]+)(\\.hi)?");
    std::vector<std::string> named;
    for (std::sregex_iterator found(operand.begin(), operand.end(), s_register), end; found != end; ++found)
    {
        named.push_back(whole ? (*found)[0].str() : (*found)[1].str());
    }
    return named;
}

// a link's text read as the register its first operand writes, the registers the rest read, and a memory operand
// after the first read as a load through its register at its offset
static inline CarrierLink carrier_link_read(const std::string &text)
{
    static const std::regex s_memory("\\[(U?R[0-9]+)(\\.64)?(\\+([0-9a-fA-Fx]+))?\\]");
    CarrierLink link{std::string(), {}, {}, 0, std::string(), 0ull};
    std::string rest = carrier_trimmed(text);
    if (!rest.empty() && (rest[0] == '@'))
    {
        const size_t space = rest.find_first_of(" \t");
        for (const std::string &guard : carrier_registers(rest.substr(0u, space)))
        {
            link.read.push_back(guard);
            link.read_written.push_back(guard);
        }
        rest = carrier_trimmed(rest.substr(space));
    }
    const size_t space = rest.find_first_of(" \t");
    rest = (space == std::string::npos) ? std::string() : rest.substr(space);
    rest = rest.substr(0u, rest.find(';'));
    const std::vector<std::string> operands = carrier_pieces(rest, ',');
    for (size_t at = 0u; at < operands.size(); at += 1u)
    {
        std::smatch memory;
        if (std::regex_search(operands[at], memory, s_memory) && (at > 0u))
        {
            link.loads = 1;
            link.address_register = memory[1].str();
            link.offset = memory[4].matched ? std::stoull(memory[4].str(), nullptr, 0) : 0ull;
        }
        const std::vector<std::string> named = carrier_registers(operands[at]);
        const std::vector<std::string> named_whole = carrier_registers(operands[at], 1);
        for (size_t each = 0u; each < named.size(); each += 1u)
        {
            if ((at == 0u) && (each == 0u) && memory.empty())
            {
                link.written = named[each];
                continue;
            }
            link.read.push_back(named[each]);
            link.read_written.push_back(named_whole[each]);
        }
    }
    return link;
}

// the nearest link of `links` before `before` that writes `register_name`, and -1 where none does
static inline long carrier_definition(const std::vector<CarrierLink> &links, const std::string &register_name,
                                      long before)
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

// the operands of a case `register_name` reads at link `before` of `links`, the cases loaded through the register
// written at `cases_defined`
static inline std::set<unsigned long long> carrier_operands_read(const std::vector<CarrierLink> &links,
                                                                 const std::string &register_name, long before,
                                                                 long cases_defined)
{
    std::set<unsigned long long> read;
    const long defined = carrier_definition(links, register_name, before);
    if (defined < 0)
    {
        return read;
    }
    const CarrierLink &link = links[(size_t)defined];
    if (link.loads && (cases_defined >= 0) && (carrier_definition(links, link.address_register, defined) == cases_defined))
    {
        read.insert(link.offset / s_carrier_operand_bytes);
        return read;
    }
    for (const std::string &source : link.read)
    {
        const std::set<unsigned long long> further = carrier_operands_read(links, source, defined, cases_defined);
        read.insert(further.begin(), further.end());
    }
    return read;
}

// a carrier's text read link by link, a link to a line, with the link that writes the register its first load reads
// through in `cases_defined`, -1 where that register is written by none and -2 where the carrier loads nothing
static inline std::vector<CarrierLink> carrier_read(const std::string &text, long *cases_defined)
{
    std::vector<CarrierLink> links;
    *cases_defined = -2;
    for (const std::string &line : carrier_pieces(text, '\n'))
    {
        links.push_back(carrier_link_read(line));
        const CarrierLink &read = links.back();
        if (read.loads && (*cases_defined == -2))
        {
            *cases_defined = carrier_definition(links, read.address_register, (long)links.size() - 1);
        }
    }
    return links;
}

// the operands of a case `links` loads, by their place in the case
static inline std::set<unsigned long long> carrier_operands_loaded(const std::vector<CarrierLink> &links,
                                                                   long cases_defined)
{
    std::set<unsigned long long> loaded;
    for (size_t at = 0u; at < links.size(); at += 1u)
    {
        const CarrierLink &link = links[at];
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register, (long)at) == cases_defined))
        {
            loaded.insert(link.offset / s_carrier_operand_bytes);
        }
    }
    return loaded;
}

// the links of `links` between a case's load and link `before`'s read of `register_name`, the most over every case
// operand that reaches it: 0 where a load of a case writes it, and -1 where no operand of a case reaches it
static inline long carrier_delta(const std::vector<CarrierLink> &links, const std::string &register_name, long before,
                                 long cases_defined)
{
    const long defined = carrier_definition(links, register_name, before);
    if (defined < 0)
    {
        return -1;
    }
    const CarrierLink &link = links[(size_t)defined];
    if (link.loads && (cases_defined >= 0) && (carrier_definition(links, link.address_register, defined) == cases_defined))
    {
        return 0;
    }
    long most = -1;
    for (const std::string &source : link.read)
    {
        const long further = carrier_delta(links, source, defined, cases_defined);
        most = ((further >= 0) && ((further + 1) > most)) ? (further + 1) : most;
    }
    return most;
}

// the carrier's own delta before the form `form_text`, its instructions cut at `;` and the first at link `at` of
// `links`: the links between a case's load and the form, the most over every operand of a case the form reads, 0
// where the form reads the cases as they were loaded or loads them itself, and -1 where it reads none. A put at 0
// answers of the form alone; past 0 the answer is the form's delta composed with the carrier's, a delta of its own
static inline long carrier_form_delta(const std::vector<CarrierLink> &links, long cases_defined,
                                      const std::string &form_text, long at)
{
    std::set<std::string> form_written;
    long most = -1;
    long line_at = at;
    for (const std::string &form_line : carrier_pieces(form_text, ';'))
    {
        if (carrier_trimmed(form_line).empty())
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register, line_at) == cases_defined))
        {
            most = (most < 0) ? 0 : most;
        }
        for (const std::string &source : link.read)
        {
            const long further =
                (form_written.count(source) == 0u) ? carrier_delta(links, source, line_at, cases_defined) : -1;
            most = (further > most) ? further : most;
        }
        form_written.insert(link.written);
        line_at += 1;
    }
    return most;
}

// the registers the form `form_text`, its instructions cut at `;` and the first at link `at` of `links`, reads that an
// operand of a case reaches and no earlier instruction of the form writes, each once, as the form writes it and in the
// order it names them, and `[case <n>]` for each operand of a case it loads itself: what the form's product is made of
static inline std::vector<std::string> carrier_form_reads(const std::vector<CarrierLink> &links, long cases_defined,
                                                          const std::string &form_text, long at)
{
    std::set<std::string> form_written;
    std::vector<std::string> reads;
    long line_at = at;
    for (const std::string &form_line : carrier_pieces(form_text, ';'))
    {
        if (carrier_trimmed(form_line).empty())
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register, line_at) == cases_defined))
        {
            const std::string loaded = "[case " + std::to_string(link.offset / s_carrier_operand_bytes) + "]";
            if (std::find(reads.begin(), reads.end(), loaded) == reads.end())
            {
                reads.push_back(loaded);
            }
            form_written.insert(link.written);
            line_at += 1;
            continue;
        }
        for (size_t each = 0u; each < link.read.size(); each += 1u)
        {
            const std::string &named = link.read_written[each];
            if ((form_written.count(link.read[each]) == 0u) &&
                (std::find(reads.begin(), reads.end(), named) == reads.end()) &&
                (carrier_delta(links, link.read[each], line_at, cases_defined) >= 0))
            {
                reads.push_back(named);
            }
        }
        form_written.insert(link.written);
        line_at += 1;
    }
    return reads;
}

// the register the form `form_text` makes its product in, as its last instruction's first operand writes it, a
// pair's high word with its `.hi`, and in `wide` 1 where that instruction writes the pair whole; empty where it writes
// none
static inline std::string carrier_form_product(const std::string &form_text, int *wide)
{
    std::string last;
    for (const std::string &form_line : carrier_pieces(form_text, ';'))
    {
        last = carrier_trimmed(form_line).empty() ? last : carrier_trimmed(form_line);
    }
    if (!last.empty() && (last[0] == '@'))
    {
        const size_t space = last.find_first_of(" \t");
        last = (space == std::string::npos) ? std::string() : carrier_trimmed(last.substr(space));
    }
    const size_t space = last.find_first_of(" \t");
    const std::string operation = last.substr(0u, space);
    const std::vector<std::string> operands =
        carrier_pieces((space == std::string::npos) ? std::string() : last.substr(space), ',');
    const std::string first_operand = operands.empty() ? std::string() : operands[0];
    const std::vector<std::string> first = carrier_registers(first_operand, 1);
    *wide = ((operation.find(".64") != std::string::npos) || (operation.find(".WIDE") != std::string::npos)) ? 1 : 0;
    return (first.empty() || (first_operand.find('[') != std::string::npos)) ? std::string() : first[0];
}

// 1 where the form `form_text`, its instructions cut at `;` and the first at link `at` of `links`, carries the cases:
// one of its instructions loads an operand of a case, or reads a register an operand of a case reaches that no earlier
// instruction of the form writes
static inline int carrier_form_carries(const std::vector<CarrierLink> &links, long cases_defined,
                                       const std::string &form_text, long at)
{
    std::set<std::string> form_written;
    long line_at = at;
    for (const std::string &form_line : carrier_pieces(form_text, ';'))
    {
        if (carrier_trimmed(form_line).empty())
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register, line_at) == cases_defined))
        {
            return 1;
        }
        for (const std::string &source : link.read)
        {
            if ((form_written.count(source) == 0u) &&
                !carrier_operands_read(links, source, line_at, cases_defined).empty())
            {
                return 1;
            }
        }
        form_written.insert(link.written);
        line_at += 1;
    }
    return 0;
}

#endif
