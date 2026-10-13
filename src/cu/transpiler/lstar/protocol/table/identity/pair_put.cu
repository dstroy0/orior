// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair_put.cu: the put: one form stood in for by the other at each of its links in each carrier, until the
// part answers one apart
#include "pair.h"

// `from` stood in for by `to` at each of its links in each carrier, until the part answers one apart: 1 with its
// verdict, closed at the first link apart or open with the cases alike at every link the part answered, 0 where the
// part answered none
int PairWork::put(const std::string &from, const std::string &to, std::string *verdict)
{
    RunQuestion &question = *t_question;
    std::vector<unsigned int> &places = *t_places;
    std::vector<std::string> from_names;
    std::vector<std::string> to_names;
    const std::string from_text = marked(sass, from, &from_names);
    const std::string to_text = marked(sass, to, &to_names);
    // the stand-in read against the form token by token: what the form writes in place of each of the stand-in's
    // parameters, and each of the form's parameters the stand-in writes a literal in place of
    std::map<char, std::string> bound;
    std::map<char, std::string> fixed;
    const int unified = form_unified(to_text, from_text, &bound, &fixed);
    // a const load and a plain one: the first link alike is the part's answer, and the question stays open for
    // the chain to settle, or for a lower identity to witness where the rest of the chain fails
    const int const_loaded = form_const_loaded(from_text, to_text);
    // the form's parameters the stand-in reads: by name, or in place of one of its own
    std::set<std::string> read_by_to(to_names.begin(), to_names.end());
    for (const auto &each : bound)
    {
        if (unified && (each.second[0] == '\x01') && ((size_t)(each.second[1] - 'A') < from_names.size()))
        {
            read_by_to.insert(from_names[(size_t)(each.second[1] - 'A')]);
        }
    }
    // the values the two forms write of their own, as the ruleset gives them, and the values the pair's product is
    // whole over with them
    std::set<unsigned long long> qualifiers = carrier_literals(from_text);
    for (const unsigned long long value : carrier_literals(to_text))
    {
        qualifiers.insert(value);
    }
    const std::vector<std::set<unsigned long long>> pair_cased = concept_values_qualified(cased, qualifiers);
    if (from_text.empty() || to_text.empty())
    {
        return 0;
    }
    std::map<char, unsigned int> group;
    const std::vector<FoundLink> found_links = links_of(carriers, seed, from_text, &group, from + " " + to);
    // the pair's product over every put, the shapes of what the form read at each, and its verdict where a put
    // closed it
    unsigned int alike_cases = 0u;
    int answered = 0;
    ConceptProduct product;
    std::set<std::string> shapes;
    std::string closed;
    long band = LONG_MIN;
    int band_truthy = 0;
    if (found_links.empty())
    {
        chose("no_link", "-");
    }
    for (const FoundLink &found_link : found_links)
    {
        const auto &chain = *found_link.chain;
        const std::smatch &found = found_link.found;
        char link_address[32];
        snprintf(
            link_address, sizeof(link_address), "%s:%04x", chain.first.c_str(),
            16u * (unsigned int)std::count(chain.second.begin(), chain.second.begin() + found.position(0), '\n'));
        std::map<std::string, std::string> given;
        for (const auto &each : group)
        {
            const size_t parameter = (size_t)(each.first - 'A');
            if (parameter < from_names.size())
            {
                given[from_names[parameter]] = found[each.second].str();
            }
        }
        // a carrier is one that reads what the form writes: its first parameter, the register it writes, named
        // again after it, a pair's high word read where the pair is named. A link nothing reads answers alike
        // whatever stands in for it
        const std::string written = (group.count('A') != 0u) ? found[group['A']].str() : std::string();
        const std::string written_register = written.substr(0u, written.find('.'));
        const std::string after = chain.second.substr((size_t)(found.position(0) + found.length(0)));
        if (written_register.empty() ||
            !std::regex_search(after, std::regex("(^|[^A-Za-z0-9_])" + written_register + "([^0-9]|$)")))
        {
            chose("unread", link_address);
            continue;
        }
        // a link of the carrier's own, the thread's index, the case's index, the bound or an address, moves the case
        // every thread reads: a form is put only where its link carries the cases (carrier_flow.h)
        const auto &flow = carriers.flows[chain.first];
        const long link_at = (long)std::count(chain.second.begin(), chain.second.begin() + found.position(0), '\n');
        if (!carrier_form_carries(flow.first, flow.second, found.str(0), link_at))
        {
            chose("carrier_own", link_address);
            continue;
        }
        // each of the stand-in's parameters given by name, or where a name is not given, by what the form writes
        // in its place, token by token: an argument of the form, or a literal the form writes there
        std::vector<std::string> arguments;
        for (size_t parameter = 0u; parameter < to_names.size(); parameter += 1u)
        {
            const std::string &name = to_names[parameter];
            const auto aligned = bound.find((char)('A' + parameter));
            if (given.count(name) != 0u)
            {
                arguments.push_back(given[name]);
            }
            else if (unified && (aligned != bound.end()) && (aligned->second[0] == '\x01') &&
                     ((size_t)(aligned->second[1] - 'A') < from_names.size()) &&
                     (given.count(from_names[(size_t)(aligned->second[1] - 'A')]) != 0u))
            {
                arguments.push_back(given[from_names[(size_t)(aligned->second[1] - 'A')]]);
            }
            else if (unified && (aligned != bound.end()) && (aligned->second[0] != '\x01'))
            {
                arguments.push_back(aligned->second);
            }
            else
            {
                chose("parameter_unbound", link_address);
                return 0;
            }
        }
        // a stand-in the ruleset cannot write is a malformed question: the put exits, settled. Its scratch is
        // registers past the chain's own
        std::string standing;
        unsigned int scratch_taken = 0u;
        if (ruleset_opcode(sass, to, arguments, scratch_past(carriers.magnitudes[chain.first].first, &scratch_taken),
                           standing) == 0)
        {
            chose("unwritable", link_address);
            return 0;
        }
        // a link the stand-in writes as it stood is no ask
        if (standing == found.str(0))
        {
            chose("unchanged", link_address);
            continue;
        }
        t_dangerous = form_dangerous(standing);
        const std::string before = chain.second.substr(0u, (size_t)found.position(0));
        const std::string changed = before + standing + chain.second.substr((size_t)(found.position(0) + found.length(0)));
        std::vector<unsigned char> code(16u * 4096u);
        const unsigned int count = sass_assemble_lines(machine, changed.c_str(), SASS_CONTROL_SAFE, code.data(), code.size());
        if (count == 0u)
        {
            chose("unassembled", link_address);
            continue;
        }
        code.resize(16u * count);
        const unsigned int link = (unsigned int)std::count(before.begin(), before.end(), '\n');
        char address[32];
        snprintf(address, sizeof(address), "%s:%04x", chain.first.c_str(), 16u * link);
        const std::vector<std::string> &expected = host[chain.first];
        // a stand-in a lesson of the overseer holds is answered as held at this link, and the part never sees it
        char overseen[160];
        if (overseer_holds(standing.c_str(), chain.first.c_str(), overseen, sizeof(overseen)))
        {
            chose("overseen", link_address);
            if (s_ask_trace != NULL)
            {
                ask_written(s_ask_trace, "pair %s in place of %s at %s: %s\n", to.c_str(), from.c_str(), address,
                            overseen);
                ask_flushed(s_ask_trace);
            }
            continue;
        }
        // Past the first case apart a link adds to the product alone, and only where the form reads in a shape no
        // link read before it: a link of a shape read before holds the same rows, and is no branch of its own. The
        // cycle loops while the product is not whole and a branch is left, and ends at a whole product or with
        // every branch put or steered off. Its verdict stands, and it asks nothing
        const int past_closed = !closed.empty();
        // The steer: the links of one delta are one band, the nearest first. A band is truthy where a put in it
        // adds a row that came back one way, signal, and falsy where its puts add none or only rows that came back
        // both ways, noise. A truthy band steers on to the next; past the first case apart, a falsy band steers off
        // every farther one, whose links stand farther from the cases and read them through more links
        if (found_link.vector.first != band)
        {
            if (past_closed && (band != LONG_MIN) && !band_truthy)
            {
                if (s_ask_log != NULL)
                {
                    chose("steered_off", link_address);
                }
                break;
            }
            band = found_link.vector.first;
            band_truthy = 0;
        }
        const int shape_new =
            shapes.insert(read_shape(flow.first, carrier_pieces(chain.second, '\n'), flow.second, found.str(0),
                                     link_at))
                .second;
        if (past_closed && !shape_new)
        {
            chose("shape_read", link_address);
            continue;
        }
        // the stand-in is the question's slot: the cross reads it as the probe it is (measuring_stick_query.c)
        question.slot = link;
        const int alike =
            past_closed ? 0 : vector_alike(code, expected, places, &question, &asks, carriers.magnitudes[chain.first].first);
        const unsigned int put_outcome = past_closed ? (unsigned int)RUN_HELD : question.outcome;
        if (!past_closed)
        {
            chose(alike ? "alike" : (put_outcome == RUN_ANSWERED) ? "apart" : "refused", link_address);
        }
        if ((s_ask_trace != NULL) && !past_closed)
        {
            ask_written(s_ask_trace, "pair %s in place of %s at %s: %s\n", to.c_str(), from.c_str(), address,
                        alike                           ? "alike"
                        : (put_outcome == RUN_ANSWERED) ? "apart"
                                                        : question.refused);
            ask_flushed(s_ask_trace);
        }
        // a put the part refused, or a carrier ended on, calls a conference: its guidance is what the learner puts
        // next, and a lesson it teaches holds the stand-in's own product below off the part
        unsigned int guidance = OVERSEER_HANDS_OFF;
        if (!past_closed && !alike && (put_outcome != RUN_ANSWERED) && (put_outcome != RUN_HELD))
        {
            guidance = conferred(
                from, to, "the pair " + from + " " + to + ", " + to + " put in place of " + from + " at " + address,
                standing, chain.first, scratch_taken, question, closed, link_address);
        }
        const int stand_in_held = overseer_holds(standing.c_str(), chain.first.c_str(), overseen, sizeof(overseen));
        // the put beneath its ask in the log: the link as it stood, the link standing in its place, each on one
        // line of the log, and the operands only the form it stood as names
        if (s_ask_log != NULL)
        {
            const PutLink link = {from,      to,     from_names, from_text, read_by_to,   unified,
                                  fixed,     qualifiers, pair_cased, question, places,  product,
                                  chain,     found,  given,      flow,      link_at,      link_address,
                                  standing,  address, found_link.vector.first, stand_in_held, shape_new};
            band_truthy |= link.logged(*this);
        }
        // the conference's guidance: the stand-in brought it on, and the pair is put the other way round, or its
        // relation waits; otherwise the put goes on to its next link as it would
        if (guidance == OVERSEER_OTHER_WAY)
        {
            t_turned = 1;
            break;
        }
        if (guidance == OVERSEER_WAITS)
        {
            break;
        }
        if (past_closed && concept_whole(product, pair_cased))
        {
            *verdict = closed;
            return 1;
        }
        if (alike)
        {
            alike_cases += (unsigned int)std::count_if(places.begin(), places.end(),
                                                       [&](unsigned int place) { return expected[place] != "-"; });
            answered = 1;
            if (const_loaded)
            {
                const char *const stop[2] = {"const_stop", "alike"};
                chosen(stop, 2u, link_address);
                break;
            }
            continue;
        }
        if (put_outcome == RUN_ANSWERED)
        {
            // the verdict is the first case apart; with the log kept, the pair is put on until its product is whole
            closed = closed.empty() ? ("closed " + std::string(address) + " " + case_text(s_apart_at) + "->" +
                                       expected[s_apart_at])
                                    : closed;
            if ((s_ask_log == NULL) || concept_whole(product, pair_cased))
            {
                *verdict = closed;
                return 1;
            }
        }
    }
    if (!closed.empty())
    {
        *verdict = closed;
        return 1;
    }
    *verdict = "open " + std::to_string(alike_cases);
    return answered;
}
