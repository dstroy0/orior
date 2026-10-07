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
// whose values, as the part holds them at the link, every frame reads as themselves. A pair of forms of a flag that name
// the same operands, apart whatever the link reads, is a negation's: the two members of one node. A pair answered alike
// at every put is a qualifier's. A pair is written a member of every set a test reads it into, and of every meta, each
// a light of its own, and a pair the part answered apart in this log that no test reads into a set is written
// unknown_coherence. A pair the log holds no answer of keeps the set line the bridge held beneath it, and one that
// holds none is written unknown_coherence: no answer has read it into a set, and it is a member of every one. The
// texts the log read of a pair read it into its categories before any ask, a comparison, an equality or an order, an
// operation, a commutation, a verb, a range, a vector, a control or a switch, each written beside its sets, and a text
// reads no pair out of
// unknown_coherence
//
// Each answered pair whose product is whole is written beneath its set with its concept, `concept_coherence
// <identity>`: whether its two forms write one value or two on each case, as a function of the values the form reads
// there, over every put the part answered (concept_product.h). A concept is its product and cares for nothing that made
// it, and two pairs of one product are one concept whatever forms name them
#include "carrier_flow.h"
#include "concept_product.h"
#include "query_trace.h"

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
    // each register the form reads that a case reaches, as the form writes it, and its value on every case as the part
    // holds it at the form, empty where the part refused the read
    std::vector<std::pair<std::string, std::vector<unsigned long long>>> reads;
    // the value the form writes and the value its stand-in writes on every case, each read just after the form
    std::vector<unsigned long long> writes_from;
    std::vector<unsigned long long> writes_to;
    // the values the pair's two forms write of their own
    std::set<unsigned long long> qualifiers;
};

// 1 where the part answered every read of `put` on every case it was put over
static int put_read(const LoggedPut &put)
{
    int read = !put.reads.empty();
    for (const auto &each : put.reads)
    {
        read &= (each.second.size() >= put.came_back.size()) ? 1 : 0;
    }
    return read;
}

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
    // two readings of the same bits under two frames agree wherever every value the form reads fits every frame,
    // below the top of a byte read signed, the narrowest signed width the cases hold: a modifier apart only past that
    // is the frame's. The values are the form's own, read at the form, and a put the part held no read of is read
    // into no set
    if (!put_read(put))
    {
        return std::string();
    }
    int past_every_frame = 1;
    for (size_t place = 0u; place < put.came_back.size(); place += 1u)
    {
        if (put.came_back[place] != 'x')
        {
            continue;
        }
        int fits = 1;
        for (const auto &each : put.reads)
        {
            fits &= (each.second[place] < s_frame_free_below) ? 1 : 0;
        }
        past_every_frame &= fits ? 0 : 1;
    }
    return past_every_frame ? "frame_coherence" : "modifier_coherence";
}

// the metas, each written above the sets a pair is read into, in this order
static const char *const s_metas[] = {"structural_coherence", "syntactic_coherence",    "pragmatic_coherence",
                                      "data_quality_coherence", "noise_coherence", "folding_coherence"};

// the sets, each written beneath a pair's verdict in the order the tree holds them, `unknown_coherence` last. The
// texts read a pair into a category, and only an answer reads it out of `unknown_coherence`
static const char *const s_sets[] = {
    "comparison_coherence",  "equality_coherence", "order_coherence",   "operation_coherence",
    "commutative_coherence", "verb_coherence",     "frame_coherence",   "qualifier_coherence",
    "modifier_coherence",    "negation_coherence", "witness_coherence", "range_coherence",
    "vector_coherence",      "control_coherence",  "switch_coherence",  "unknown_coherence"};

// the categories the texts read a pair into, before any ask
static const char *const s_categories[] = {"comparison_coherence", "equality_coherence", "order_coherence",
                                           "operation_coherence",  "commutative_coherence", "verb_coherence",
                                           "range_coherence",      "vector_coherence",   "control_coherence",
                                           "switch_coherence"};

// 1 where `name` is one of the `count` names of `names`
static int named_in(const char *const *names, size_t count, const std::string &name)
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
    // each pair's metas, the internal nodes of the tree it hangs from, the categories its texts read it into, and the
    // pairs whose texts the log read
    std::map<std::string, std::set<std::string>> metas;
    std::map<std::string, std::set<std::string>> categories;
    std::set<std::string> faceted;
    // the choices each pair's puts made, read off the log's `choice <flags> <link>` lines by the trace's table: how
    // many of its links held each flag, and how many of every pair's
    QueryTrace trace;
    std::string trace_error;
    if (!query_trace_read(query_trace_path(), &trace, &trace_error))
    {
        printf("klq_decoder: %s\n", trace_error.c_str());
        return 1;
    }
    std::string choosing;
    std::map<std::string, std::map<unsigned long long, unsigned int>> choices;
    std::map<unsigned long long, unsigned int> chosen;
    // the pairs the lower witness answered: a choice of a link of a third form the part answered alike or apart
    const unsigned long long lower_flag = query_trace_flag(trace, "lower");
    const unsigned long long answer_flags = query_trace_flag(trace, "alike") | query_trace_flag(trace, "apart");
    std::set<std::string> witnessed;
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
    // each pair's set, read off its puts apart, or a qualifier's where every put the part answered came back alike
    std::map<std::string, std::set<std::string>> decoded;
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
    // each pair's qualifiers, the values its forms write of their own, and the values its product is whole over
    std::map<std::string, std::set<unsigned long long>> pair_qualifiers;
    for (const LoggedPut &put : puts)
    {
        pair_qualifiers[pair_key(put.from, put.to)].insert(put.qualifiers.begin(), put.qualifiers.end());
    }
    std::map<std::string, std::vector<std::set<unsigned long long>>> pair_cased;
    for (const auto &held : pair_qualifiers)
    {
        pair_cased[held.first] = concept_values_qualified(cased, held.second);
    }
    std::map<std::string, ConceptProduct> products;
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
        const std::string set = put_decoded(put, case_sets[put.cases_at], argv[2]);
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
    // a pair no test read into a set whose whole product agrees only where it reads a value its forms write of their
    // own is a qualifier's, that value its qualifier
    std::map<std::string, std::string> identities;
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
    // each pair's concept as it is written, its own product's where this log answered it and otherwise the one the
    // bridge held beneath it. A pair whose concept another pair shares is folded: one ask answers both
    std::map<std::string, std::string> pair_concepts;
    std::string concept_pair;
    for (const std::string &entry : bridge)
    {
        if (entry.rfind("pair ", 0u) == 0u)
        {
            std::stringstream words(entry);
            std::string kind;
            std::string first;
            std::string second;
            words >> kind >> first >> second;
            concept_pair = pair_key(first, second);
        }
        else if (!concept_pair.empty() && (entry.rfind("concept_coherence ", 0u) == 0u) &&
                 (decoded.count(concept_pair) == 0u))
        {
            pair_concepts[concept_pair] = entry.substr(18u);
        }
    }
    for (const auto &held : identities)
    {
        pair_concepts[held.first] = held.second;
    }
    std::map<std::string, unsigned int> pairs_of_concept;
    for (const auto &held : pair_concepts)
    {
        pairs_of_concept[held.second] += 1u;
    }
    for (const auto &held : pair_concepts)
    {
        if (pairs_of_concept[held.second] > 1u)
        {
            metas[held.first].insert("folding_coherence");
        }
    }
    std::vector<std::string> written;
    std::string pair_held;
    std::vector<std::string> sets_held;
    std::vector<std::string> metas_held;
    std::string identity_held;
    std::string intent_held;
    std::map<std::string, unsigned int> counted;
    std::map<std::string, unsigned int> concepts;
    std::map<std::string, unsigned int> named_pairs;
    std::map<std::string, size_t> set_signal;
    std::map<std::string, size_t> set_noise;
    const auto pair_closed = [&]() {
        if (!pair_held.empty())
        {
            // a pair the log read the texts or the answers of hangs from the metas read of it now, and any other
            // from the metas the bridge held. The fold is read off every pair's concept, and always now
            const int texts_read = faceted.count(pair_held) != 0u;
            const int answered = decoded.count(pair_held) != 0u;
            for (const char *const meta : s_metas)
            {
                const int held = std::find(metas_held.begin(), metas_held.end(), meta) != metas_held.end();
                const int now = texts_read || answered || (std::string(meta) == "folding_coherence");
                if (now ? (metas[pair_held].count(meta) != 0u) : held)
                {
                    written.push_back(meta);
                    counted[meta] += 1u;
                }
            }
            // every set the pair is read into, each a light of its own: the categories its texts read it into and the
            // sets its answers read it into, each read now where the log read it and otherwise as the bridge held it
            std::set<std::string> lit;
            for (const std::string &set : sets_held)
            {
                if (named_in(s_categories, std::size(s_categories), set) ? !texts_read : !answered)
                {
                    lit.insert(set);
                }
            }
            if (answered)
            {
                lit.insert(decoded[pair_held].begin(), decoded[pair_held].end());
            }
            if (texts_read)
            {
                lit.insert(categories[pair_held].begin(), categories[pair_held].end());
            }
            // only an answer reads a pair out of `unknown_coherence`, a member of every category beneath
            int read_down = 0;
            for (const std::string &set : lit)
            {
                read_down |= (!named_in(s_categories, std::size(s_categories), set) && (set != "unknown_coherence"))
                                 ? 1
                                 : 0;
            }
            if (read_down)
            {
                lit.erase("unknown_coherence");
            }
            else
            {
                lit.insert("unknown_coherence");
            }
            std::vector<std::string> sets;
            for (const char *const set : s_sets)
            {
                if (lit.count(set) != 0u)
                {
                    sets.push_back(set);
                    lit.erase(set);
                }
            }
            sets.insert(std::find(sets.begin(), sets.end(), "unknown_coherence"), lit.begin(), lit.end());
            // each set's signal and noise: its pairs, those a whole product names, and the rows of their products
            // that came back one way and both ways
            const auto product_held = products.find(pair_held);
            const size_t signal_rows = (product_held != products.end()) ? concept_signal(product_held->second) : 0u;
            const size_t noise_rows =
                (product_held != products.end()) ? (product_held->second.rows.size() - signal_rows) : 0u;
            for (const std::string &set : sets)
            {
                written.push_back(set);
                counted[set] += 1u;
                named_pairs[set] += (identities.count(pair_held) != 0u) ? 1u : 0u;
                set_signal[set] += signal_rows;
                set_noise[set] += noise_rows;
            }
            // a pair answered in this log is the concept its own product is, and none until that product is whole
            const std::string identity = (identities.count(pair_held) != 0u)
                                             ? ("concept_coherence " + identities[pair_held])
                                         : (decoded.count(pair_held) != 0u) ? std::string()
                                                                            : identity_held;
            if (!identity.empty())
            {
                written.push_back(identity);
                concepts[identity] += 1u;
            }
            // the intent the pair's forms carry of their own, the values each writes as the ruleset gives it
            std::string intent;
            for (const unsigned long long value : pair_qualifiers[pair_held])
            {
                char written_value[24];
                snprintf(written_value, sizeof(written_value), " %llx", value);
                intent += written_value;
            }
            intent = !intent.empty()                    ? ("intent_coherence" + intent)
                     : (decoded.count(pair_held) != 0u) ? std::string()
                                                        : intent_held;
            if (!intent.empty())
            {
                written.push_back(intent);
            }
        }
        pair_held.clear();
        sets_held.clear();
        metas_held.clear();
        identity_held.clear();
        intent_held.clear();
    };
    for (const std::string &entry : bridge)
    {
        const int verdict = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        const int meta = std::find_if(std::begin(s_metas), std::end(s_metas),
                                      [&](const char *const each) { return entry == each; }) != std::end(s_metas);
        if (!pair_held.empty() && meta)
        {
            metas_held.push_back(entry);
            continue;
        }
        if (!pair_held.empty() && set_line(entry))
        {
            sets_held.push_back(entry);
            continue;
        }
        if (!pair_held.empty() && (entry.rfind("concept_coherence ", 0u) == 0u))
        {
            identity_held = entry;
            continue;
        }
        if (!pair_held.empty() && (entry.rfind("intent_coherence ", 0u) == 0u))
        {
            intent_held = entry;
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
        std::string lights;
        for (const std::string &meta : metas[held.first])
        {
            lights += meta + " ";
        }
        for (const std::string &set : categories[held.first])
        {
            lights += set + " ";
        }
        for (const std::string &set : held.second)
        {
            lights += set + " ";
        }
        printf("  %s: %s%s\n", held.first.c_str(), lights.c_str(),
               (identities.count(held.first) != 0u) ? identities[held.first].c_str() : "-");
    }
    unsigned int shared = 0u;
    for (const auto &held : concepts)
    {
        shared += (held.second > 1u) ? 1u : 0u;
    }
    // the choices of every pair's puts by the trace's flags, and those of each pair the part answered nothing of, the
    // questions a further pass takes up
    const auto choices_written = [&trace](const std::map<unsigned long long, unsigned int> &held) {
        std::string written;
        for (const auto &flag : trace.named)
        {
            const auto count = held.find(flag.first);
            if (count != held.end())
            {
                written += (written.empty() ? "" : ", ") + std::to_string(count->second) + " " + flag.second;
            }
        }
        return written;
    };
    if (!chosen.empty())
    {
        printf("  choices: %s\n", choices_written(chosen).c_str());
    }
    const unsigned long long answered_flags = query_trace_flag(trace, "alike") | query_trace_flag(trace, "apart");
    for (const auto &held : choices)
    {
        int answered = 0;
        for (const auto &flag : held.second)
        {
            answered |= ((flag.first & answered_flags) != 0ull) ? 1 : 0;
        }
        if (!held.first.empty() && (decoded.count(held.first) == 0u) && !answered)
        {
            printf("  answered nothing, %s: %s\n", held.first.c_str(), choices_written(held.second).c_str());
        }
    }
    // each set's pairs, those a whole product names, and its signal against its noise in rows of the products
    for (const char *const set : s_sets)
    {
        if (counted[set] != 0u)
        {
            printf("  %s: %u pairs, %u named by a concept, %zu rows of signal, %zu of noise\n", set, counted[set],
                   named_pairs[set], set_signal[set], set_noise[set]);
        }
    }
    printf("klq_decoder: %u puts read, %u concepts, %u of more than one pair; metas", (unsigned int)puts.size(),
           (unsigned int)concepts.size(), shared);
    for (const char *const meta : s_metas)
    {
        printf(" %u %s", counted[meta], meta);
    }
    printf("; sets");
    for (const char *const set : s_sets)
    {
        printf(" %u %s", counted[set], set);
    }
    printf("; written to %s\n", argv[3]);
    return 0;
}
