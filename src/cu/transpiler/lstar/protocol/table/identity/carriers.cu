// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carriers.cu: the carriers of the pairs: their links, scratch and marks, and each pair's categories, lines beneath it and verdict row
#include "identity_internal.h"
#include "../../table_text.h"
#include "../../../../../includes/parsers/who.h"

// the carriers of the chains in `engine` the host computes, by `host`, into `carriers`
void carriers_read(const char *engine, const std::map<std::string, std::vector<std::string>> &host,
                          Carriers *carriers)
{
    for (const auto &held : host)
    {
        std::ifstream chain(std::string(engine) + "/" + held.first + ".sass", std::ios::binary);
        if (chain)
        {
            carriers->texts[held.first] =
                std::string((std::istreambuf_iterator<char>(chain)), std::istreambuf_iterator<char>());
        }
    }
    for (const auto &chain : carriers->texts)
    {
        long cases_defined = -2;
        const std::vector<CarrierLink> links = carrier_read(chain.second, &cases_defined);
        carriers->flows[chain.first] = std::make_pair(links, cases_defined);
    }
    for (const auto &flow : carriers->flows)
    {
        carriers->magnitudes[flow.first] =
            std::make_pair(chain_registers(engine, flow.first),
                           (unsigned int)carrier_operands_loaded(flow.second.first, flow.second.second).size());
    }
}

// the scratch a form written into a chain takes, each register as `sass` writes its bank: registers past the `held`
// the chain names, an even one first so that a wide's pair is aligned, the next each time the writing asks; predicate
// 5, which no chain of ours names
std::function<std::string(const std::string &)> scratch_past(const Ruleset *sass, unsigned int held,
                                                             unsigned int *taken)
{
    return [sass, held, taken](const std::string &bank) {
        if (bank == "predicate")
        {
            return ruleset_register(sass, bank, 5u);
        }
        const unsigned int first = ((held + 1u) & ~1u) + *taken;
        *taken += (bank == "wide") ? 2u : 1u;
        return ruleset_register(sass, bank, first);
    };
}

// form `form` as `sass` writes it with each parameter a marker, and each scratch register the writing takes a marker
// of its own past them, and its parameters' names; empty where it gives no form of the name
std::string marked(const Ruleset *sass, const std::string &form, std::vector<std::string> *names)
{
    *names = ruleset_parameters(sass, form);
    std::vector<std::string> markers;
    for (size_t at = 0u; at < names->size(); at += 1u)
    {
        markers.push_back(std::string("\x01") + (char)('A' + at) + "\x01");
    }
    size_t scratch_marker = names->size();
    const auto scratch = [&scratch_marker](const std::string &) {
        scratch_marker += 1u;
        return std::string("\x01") + (char)('A' + scratch_marker - 1u) + "\x01";
    };
    std::string written;
    return (ruleset_opcode(sass, form, markers, scratch, written) != 0) ? written : std::string();
}

// the links the form `text`, each parameter a marker, stands at in every carrier of `carriers`, the least vector
// first and those of one vector in an order drawn from `seed` and `drawn_for`, and in `group` each marker's group in
// the pattern its lines are found by
std::vector<FoundLink> links_of(Carriers &carriers, unsigned long seed, const std::string &text,
                                       std::map<char, unsigned int> *group, const std::string &drawn_for)
{
    // the pattern of the form's lines, each marker an operand, the same marker the same operand
    std::string pattern;
    for (size_t at = 0u; at < text.size(); at += 1u)
    {
        if ((text[at] == '\x01') && ((at + 2u) < text.size()) && (text[at + 2u] == '\x01'))
        {
            const char marker = text[at + 1u];
            if (group->count(marker) == 0u)
            {
                const unsigned int next = (unsigned int)group->size() + 1u;
                (*group)[marker] = next;
                pattern += "([^,;\\s]+?)";
            }
            else
            {
                pattern += "\\" + std::to_string((*group)[marker]);
            }
            at += 2u;
            continue;
        }
        const char letter = text[at];
        const int special = (letter != '\0') && (strchr("\\^$.|?*+()[]{}", letter) != NULL);
        pattern += special ? (std::string("\\") + letter) : std::string(1u, letter);
    }
    const std::regex lines(pattern);
    std::vector<FoundLink> found_links;
    for (const auto &chain : carriers.texts)
    {
        const std::sregex_iterator end;
        for (std::sregex_iterator link_found(chain.second.begin(), chain.second.end(), lines); link_found != end;
             ++link_found)
        {
            const auto &flow = carriers.flows[chain.first];
            const long link_at =
                (long)std::count(chain.second.begin(), chain.second.begin() + link_found->position(0), '\n');
            const long delta = carrier_form_delta(flow.first, flow.second, link_found->str(0), link_at);
            found_links.push_back(FoundLink{
                &chain, *link_found, std::make_pair((delta < 0) ? LONG_MAX : delta, carriers.magnitudes[chain.first])});
        }
    }
    // the links in the order of their vectors, the least first: a form the cases reach as they were loaded answers
    // of the form alone. Links of one vector are tried in an order drawn from the seed and the forms `drawn_for`
    // names alone, so that no other pair, put or settled, moves the order of these
    std::stable_sort(found_links.begin(), found_links.end(),
                     [](const FoundLink &left, const FoundLink &right) { return left.vector < right.vector; });
    const unsigned long long drawn_hash = record_hash(drawn_for.c_str(), drawn_for.size(), seed);
    std::seed_seq drawn_seed{(unsigned int)(drawn_hash & 0xffffffffull), (unsigned int)(drawn_hash >> 32u),
                             (unsigned int)seed};
    std::mt19937 drawn(drawn_seed);
    for (size_t first = 0u; first < found_links.size();)
    {
        size_t last = first;
        while ((last < found_links.size()) && (found_links[last].vector == found_links[first].vector))
        {
            last += 1u;
        }
        std::shuffle(found_links.begin() + (long)first, found_links.begin() + (long)last, drawn);
        first = last;
    }
    return found_links;
}

// The cuts `codes` put together over the cases `question` holds, each dangerous where `dangerous` says and each its
// slot from `slots`, RUN_SLOT_NONE where it holds none: each container declaring `mark` registers, and every register
// its file holds where the part refuses that, those put together in the round after. Each cut's question into `asked`,
// in the order of `codes`, its outcome, answers and refusal its own, and a cut with no code left unasked. Each ask
// counted in `asks`. The answers are values the host does not compute, held in the log alone and never in R
void cuts_asked(const std::vector<std::vector<unsigned char>> &codes, const std::vector<int> &dangerous,
                       const std::vector<unsigned int> &slots, const RunQuestion *question, unsigned long long *asks,
                       unsigned int mark, std::vector<RunQuestion> *asked)
{
    const unsigned int most = 255u;
    asked->assign(codes.size(), RunQuestion());
    std::vector<RunQuestion *> carried;
    std::vector<int> carried_dangerous;
    for (size_t at = 0u; at < codes.size(); at += 1u)
    {
        if (codes[at].empty())
        {
            continue;
        }
        RunQuestion *const cut = &(*asked)[at];
        cut->code = codes[at].data();
        cut->code_size = codes[at].size();
        cut->registers = ((mark != 0u) && (mark < most)) ? mark : most;
        cut->slot = slots[at];
        cut->cases = question->cases;
        memcpy(cut->word, question->word, (size_t)question->cases * sizeof(question->word[0]));
        carried.push_back(cut);
        carried_dangerous.push_back(dangerous[at]);
    }
    *asks += carried.size();
    if (!carried.empty())
    {
        asks_carried(carried.data(), carried_dangerous.data(), (unsigned int)carried.size());
    }
    std::vector<RunQuestion *> again;
    std::vector<int> again_dangerous;
    for (size_t at = 0u; at < carried.size(); at += 1u)
    {
        if ((carried[at]->outcome != RUN_ANSWERED) && (carried[at]->registers != most))
        {
            carried[at]->registers = most;
            again.push_back(carried[at]);
            again_dangerous.push_back(carried_dangerous[at]);
        }
    }
    *asks += again.size();
    if (!again.empty())
    {
        asks_carried(again.data(), again_dangerous.data(), (unsigned int)again.size());
    }
}

// What the texts of a pair's two forms, as `sass` writes them, say of it before any ask, each word after a space: one
// structure, one form the other with a parameter bound; both reading what the launch gives; or two operations, one
// meaning written twice. Then the categories the texts read the pair into: both forms comparisons, every condition
// tested an equality or some an order; either form an operation at any width, every instruction of one giving one
// value with its first two sources either way round, or a doing word that computes no value; either form reading where
// its lane sits, working a wide value in pieces, deciding which form runs next, or holding no instruction
std::string pair_categories(const Ruleset *sass, const std::string &first, const std::string &second)
{
    std::vector<std::string> first_names;
    std::vector<std::string> second_names;
    const std::string first_text = marked(sass, first, &first_names);
    const std::string second_text = marked(sass, second, &second_names);
    std::map<char, std::string> bound;
    std::map<char, std::string> fixed;
    const int structural = (form_unified(sass, first_text, second_text, &bound, &fixed) && !fixed.empty()) ||
                           (form_unified(sass, second_text, first_text, &bound, &fixed) && !fixed.empty());
    const int launched =
        !first_text.empty() && !second_text.empty() && form_launched(first_text) && form_launched(second_text);
    const std::vector<std::string> first_tokens = form_tokens(first_text);
    const std::vector<std::string> second_tokens = form_tokens(second_text);
    const int two_operations = !first_tokens.empty() && !second_tokens.empty() &&
                               (first_tokens[0].substr(0u, first_tokens[0].find('.')) !=
                                second_tokens[0].substr(0u, second_tokens[0].find('.')));
    const std::set<std::string> first_conditions = form_conditions(first_text);
    const std::set<std::string> second_conditions = form_conditions(second_text);
    std::set<std::string> conditions = first_conditions;
    conditions.insert(second_conditions.begin(), second_conditions.end());
    const int compared = !first_conditions.empty() && !second_conditions.empty();
    int ordered = 0;
    int equal = compared;
    for (const std::string &condition : conditions)
    {
        ordered |= ((condition == "LT") || (condition == "LE") || (condition == "GT") || (condition == "GE")) ? 1 : 0;
        equal &= ((condition == "EQ") || (condition == "NE")) ? 1 : 0;
    }
    const int placed = form_launched(first_text) || form_launched(second_text);
    const int ranged = form_ranged(first_text) || form_ranged(second_text);
    const int controlled = form_controls(first_text) || form_controls(second_text);
    const int switched = form_switches(first_text) || form_switches(second_text);
    const int operated = form_operates(first_text) || form_operates(second_text);
    const int commuted = form_commutes(first_text) || form_commutes(second_text);
    const int verbed = form_verbs(first_text) || form_verbs(second_text);
    return std::string(structural ? " structural" : "") + (launched ? " pragmatic" : "") +
           (two_operations ? " syntactic" : "") + (compared ? " comparison" : "") + (equal ? " equality" : "") +
           ((compared && ordered) ? " order" : "") + (operated ? " operation" : "") + (commuted ? " commutative" : "") +
           (verbed ? " verb" : "") + (ranged ? " range" : "") + (placed ? " vector" : "") +
           (controlled ? " control" : "") + (switched ? " switch" : "");
}

// the lines the bridge `bridge` holds beneath the pair of `first` and `second`, either way round: its verdict, the
// metas and sets klq_decoder read it into, its concept and its intent; none where the bridge holds no such pair
std::vector<std::string> pair_beneath(const std::vector<std::string> &bridge, const std::string &first,
                                             const std::string &second)
{
    std::vector<std::string> beneath;
    const std::string one_way = "pair " + first + " " + second;
    const std::string other_way = "pair " + second + " " + first;
    for (size_t at = 0u; at < bridge.size(); at += 1u)
    {
        if ((bridge[at] != one_way) && (bridge[at] != other_way))
        {
            continue;
        }
        for (size_t under = at + 1u; under < bridge.size(); under += 1u)
        {
            const std::string &line = bridge[under];
            const int verdict = (line.rfind("open ", 0u) == 0u) || (line.rfind("closed ", 0u) == 0u);
            const int set = (line.size() > 10u) && (line.find(' ') == std::string::npos) &&
                            (line.compare(line.size() - 10u, 10u, "_coherence") == 0);
            const int named =
                (line.rfind("concept_coherence ", 0u) == 0u) || (line.rfind("intent_coherence ", 0u) == 0u);
            if (!verdict && !set && !named)
            {
                break;
            }
            beneath.push_back(line);
        }
        break;
    }
    return beneath;
}

// the verdicts of a pair put both ways round, `one_way` then `other_way`: the first that closes it, and otherwise open
// with the cases alike over both
std::string pair_verdicts_joined(const std::string &one_way, const std::string &other_way)
{
    if (one_way.rfind("closed ", 0u) == 0u)
    {
        return one_way;
    }
    if (other_way.rfind("closed ", 0u) == 0u)
    {
        return other_way;
    }
    const unsigned long one = (one_way.rfind("open ", 0u) == 0u) ? std::stoul(one_way.substr(5u)) : 0ul;
    const unsigned long other = (other_way.rfind("open ", 0u) == 0u) ? std::stoul(other_way.substr(5u)) : 0ul;
    return "open " + std::to_string(one + other);
}

// on a pair's thread: 1 where a conference turned its put the other way round, and 1 while its put is the other way
// round already or a witness, so that a conference rules the relation waits and turns nothing again (overseer.h)
thread_local int t_turned = 0;

thread_local int t_other_way = 0;

// A pair the cross read, a row of the run's table of pairs (pairs.oracle.tsv): its two forms' texts, closed or open,
// its count of cases alike where it is open, the text of the case that closed it where it is closed, the hash of what
// its put reads where `read` is 1, and its verdict's text
void pair_row_handed(const std::string &first, const std::string &second, int read, unsigned long long reads,
                            const std::string &verdict)
{
    QryShared *const run = qry_member_run();
    const int closed = (verdict.rfind("closed ", 0u) == 0u);
    const int open = (verdict.rfind("open ", 0u) == 0u);
    const std::string closing = closed ? verdict.substr(7u) : std::string();
    unsigned long long row[2u + 8] = {0ull, 0ull, table_text(first.c_str(), first.size()),
                                       table_text(second.c_str(), second.size()),
                                       closed ? 1ull : 0ull,
                                       open ? strtoull(verdict.c_str() + 5, NULL, 10) : 0ull,
                                       table_text(closing.c_str(), closing.size()),
                                       read ? 1ull : 0ull,
                                       read ? reads : 0ull,
                                       table_text(verdict.c_str(), verdict.size())};
    who_words(WHO_HERE, row);
    qry_member_append("pairs", row, sizeof(row), QRY_ORDINARY);
}
