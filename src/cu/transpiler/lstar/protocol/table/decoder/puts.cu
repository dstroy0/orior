// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// puts.cu: the puts of the log: a put's cases read, a link's operation stem, the operands deciding a put, the set
// a put is read into, a name among names, and the key of a pair
#include "decoder_internal.h"

static int put_read(const LoggedPut &put)
{
    int read = !put.reads.empty();
    for (const auto &each : put.reads)
    {
        read &= (each.second.size() >= put.came_back.size()) ? 1 : 0;
    }
    return read;
}

// the values every statement reads as themselves lie below the top of a byte read signed

static const unsigned long long s_statement_free_below = 0x80ull;

// the first word of a link's text up to its first `.`, its guard passed over

std::string operation_stem(const std::string &text)
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

std::string put_decoded(const LoggedPut &put, const std::vector<std::vector<std::string>> &cases,
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
    // two readings of the same bits under two statements agree wherever every value the form reads fits every statement,
    // below the top of a byte read signed, the narrowest signed width the cases hold: a modifier apart only past that
    // is the statement's. The values are the form's own, read at the form, and a put the part held no read of is read
    // into no set
    if (!put_read(put))
    {
        return std::string();
    }
    int past_every_statement = 1;
    for (size_t place = 0u; place < put.came_back.size(); place += 1u)
    {
        if (put.came_back[place] != 'x')
        {
            continue;
        }
        int fits = 1;
        for (const auto &each : put.reads)
        {
            fits &= (each.second[place] < s_statement_free_below) ? 1 : 0;
        }
        past_every_statement &= fits ? 0 : 1;
    }
    return past_every_statement ? "statement_coherence" : "modifier_coherence";
}

// the metas, each written above the sets a pair is read into, in this order

int named_in(const char *const *names, size_t count, const std::string &name)
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (name == names[at])
        {
            return 1;
        }
    }
    return 0;
}

// the two forms of a pair, the lesser first, as one key

std::string pair_key(const std::string &first, const std::string &second)
{
    return (first < second) ? (first + " " + second) : (second + " " + first);
}

