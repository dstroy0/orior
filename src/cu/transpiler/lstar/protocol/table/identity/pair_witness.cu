// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair_witness.cu: the lower witness: a pair whose puts asked nothing put at the links of a third form, both
// forms standing in there and the part's two answers held against each other
#include "pair.h"

// The lower witness: a pair whose puts asked nothing, neither form standing at a link that carries the cases, is
// put at the links of a third form, the first of the bridge's keys whose parameters name every parameter of both
// and that stands at such a link. Both forms stand in at each of its links, the part answers both over the cases,
// and the two answers are held against each other: the first case apart closes the pair as two operations, and
// alike on every case leaves it open with its count of cases. A link of a shape read before is passed over, and so
// is one whose writing reaches nothing the part answers, where both stand-ins answer as the chain did as it stood;
// two forms that read nothing but the register they write are answered at the first link left. 1 with the verdict
// where the part answered both at a link, 0 otherwise
int PairWork::witness_lower(const std::string &first, const std::string &second, std::string *verdict)
{
    std::vector<std::string> first_names;
    std::vector<std::string> second_names;
    const std::string first_text = marked(sass, first, &first_names);
    const std::string second_text = marked(sass, second, &second_names);
    if (first_text.empty() || second_text.empty())
    {
        return 0;
    }
    // each form read against the other token by token, as a put reads a stand-in against its form: what the other
    // writes in place of each of its parameters, the other's operand or a literal
    std::map<char, std::string> first_bound;
    std::map<char, std::string> second_bound;
    std::map<char, std::string> fixed;
    const int first_unified = form_unified(first_text, second_text, &first_bound, &fixed);
    const int second_unified = form_unified(second_text, first_text, &second_bound, &fixed);
    // the arguments of a form of parameters `names`: each its operand of the third form's link where `given`
    // names it, or else what the other form writes in its place, the operand `given` names of the other's
    // parameter there or the literal it writes. 1 where every parameter has an argument
    const auto arguments_of =
        [](const std::vector<std::string> &names, int unified, const std::map<char, std::string> &bound,
           const std::vector<std::string> &other_names, const std::map<std::string, std::string> &given,
           std::vector<std::string> *arguments) {
            arguments->clear();
            for (size_t at = 0u; at < names.size(); at += 1u)
            {
                const auto direct = given.find(names[at]);
                const auto aligned = bound.find((char)('A' + at));
                if (direct != given.end())
                {
                    arguments->push_back(direct->second);
                    continue;
                }
                if (!unified || (aligned == bound.end()))
                {
                    return 0;
                }
                if (aligned->second[0] != '\x01')
                {
                    arguments->push_back(aligned->second);
                    continue;
                }
                const size_t other = (size_t)(aligned->second[1] - 'A');
                const auto through = (other < other_names.size()) ? given.find(other_names[other]) : given.end();
                if (through == given.end())
                {
                    return 0;
                }
                arguments->push_back(through->second);
            }
            return 1;
        };
    unsigned int alike_cases = 0u;
    int answered = 0;
    for (const std::string &third : keys)
    {
        if ((third == first) || (third == second))
        {
            continue;
        }
        std::vector<std::string> third_names;
        const std::string third_text = marked(sass, third, &third_names);
        std::map<std::string, std::string> named;
        for (const std::string &name : third_names)
        {
            named[name] = name;
        }
        std::vector<std::string> unused;
        if (third_text.empty() ||
            !arguments_of(first_names, first_unified, first_bound, second_names, named, &unused) ||
            !arguments_of(second_names, second_unified, second_bound, first_names, named, &unused))
        {
            continue;
        }
        std::map<char, unsigned int> group;
        const std::vector<FoundLink> found_links =
            links_of(carriers, seed, third_text, &group, first + " " + second + " " + third);
        std::set<std::string> shapes;
        int witnessed = 0;
        for (const FoundLink &found_link : found_links)
        {
            const auto &chain = *found_link.chain;
            const std::smatch &found = found_link.found;
            const long link_at =
                (long)std::count(chain.second.begin(), chain.second.begin() + found.position(0), '\n');
            char link_address[32];
            snprintf(link_address, sizeof(link_address), "%s:%04x", chain.first.c_str(),
                     16u * (unsigned int)link_at);
            const auto lower = [&](const char *name) {
                const char *const names[2] = {"lower", name};
                chosen(names, 2u, link_address);
            };
            std::map<std::string, std::string> given;
            for (const auto &each : group)
            {
                const size_t parameter = (size_t)(each.first - 'A');
                if (parameter < third_names.size())
                {
                    given[third_names[parameter]] = found[each.second].str();
                }
            }
            const std::string written = (group.count('A') != 0u) ? found[group['A']].str() : std::string();
            const std::string written_register = written.substr(0u, written.find('.'));
            const std::string after = chain.second.substr((size_t)(found.position(0) + found.length(0)));
            if (written_register.empty() ||
                !std::regex_search(after, std::regex("(^|[^A-Za-z0-9_])" + written_register + "([^0-9]|$)")))
            {
                lower("unread");
                continue;
            }
            const auto &flow = carriers.flows[chain.first];
            if (!carrier_form_carries(flow.first, flow.second, found.str(0), link_at))
            {
                lower("carrier_own");
                continue;
            }
            if (!shapes
                     .insert(read_shape(flow.first, carrier_pieces(chain.second, '\n'), flow.second, found.str(0),
                                        link_at))
                     .second)
            {
                lower("shape_read");
                continue;
            }
            if (!witnessed && (s_ask_log != NULL))
            {
                ask_written(s_ask_log, "witness %s %s %s\n", first.c_str(), second.c_str(), third.c_str());
            }
            witnessed = 1;
            // each form written in the link's place with the arguments the third form's link gives by name, and
            // through the pair's binding where it gives none
            std::vector<std::string> first_arguments;
            std::vector<std::string> second_arguments;
            arguments_of(first_names, first_unified, first_bound, second_names, given, &first_arguments);
            arguments_of(second_names, second_unified, second_bound, first_names, given, &second_arguments);
            std::string first_standing;
            std::string second_standing;
            unsigned int first_scratch = 0u;
            unsigned int second_scratch = 0u;
            if ((ruleset_opcode(sass, first, first_arguments,
                                scratch_past(carriers.magnitudes[chain.first].first, &first_scratch),
                                first_standing) == 0) ||
                (ruleset_opcode(sass, second, second_arguments,
                                scratch_past(carriers.magnitudes[chain.first].first, &second_scratch),
                                second_standing) == 0))
            {
                lower("unwritable");
                continue;
            }
            // two stand-ins of one text, the pair's binding writing each as the other, ask the part nothing
            if (first_standing == second_standing)
            {
                lower("unchanged");
                continue;
            }
            const std::string before = chain.second.substr(0u, (size_t)found.position(0));
            const std::string rest = chain.second.substr((size_t)(found.position(0) + found.length(0)));
            const std::string standings[2] = {before + first_standing + rest, before + second_standing + rest};
            std::vector<std::vector<unsigned char>> codes(2u, std::vector<unsigned char>(16u * 4096u));
            int assembled = 1;
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                const unsigned int count = sass_assemble_lines(
                    machine, standings[side].c_str(), SASS_CONTROL_SAFE, codes[side].data(), codes[side].size());
                codes[side].resize(16u * count);
                assembled &= (count != 0u) ? 1 : 0;
            }
            if (!assembled)
            {
                lower("unassembled");
                continue;
            }
            // a stand-in a lesson of the overseer holds is answered as held, and neither is put
            char overseen[160];
            if (overseer_holds(first_standing.c_str(), chain.first.c_str(), overseen, sizeof(overseen)) ||
                overseer_holds(second_standing.c_str(), chain.first.c_str(), overseen, sizeof(overseen)))
            {
                lower("overseen");
                continue;
            }
            // both put over the pair's cases at the carrier's registers, and again declaring every register where
            // the part refuses one
            std::vector<RunQuestion> sides(2u, RunQuestion());
            const int dangerous[2] = {form_dangerous(first_standing), form_dangerous(second_standing)};
            RunQuestion *asked[2] = {&sides[0], &sides[1]};
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                sides[side].code = codes[side].data();
                sides[side].code_size = codes[side].size();
                // each stand-in is its side's slot, at the third form's link
                sides[side].slot = (unsigned int)link_at;
                sides[side].registers = carriers.magnitudes[chain.first].first;
                sides[side].cases = t_question->cases;
                memcpy(sides[side].word, t_question->word, (size_t)t_question->cases * sizeof(t_question->word[0]));
            }
            asks += 2ull;
            asks_carried(asked, dangerous, 2u);
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                if ((sides[side].outcome != RUN_ANSWERED) && (sides[side].registers != 255u))
                {
                    sides[side].registers = 255u;
                    asks += 1ull;
                    asks_carried(&asked[side], &dangerous[side], 1u);
                }
            }
            if ((sides[0].outcome != RUN_ANSWERED) || (sides[1].outcome != RUN_ANSWERED))
            {
                // each side the part refused, or a carrier ended on, calls a conference of its own; a lesson it
                // teaches holds the stand-in at the links after this one
                const std::string *const standings_of[2] = {&first_standing, &second_standing};
                const unsigned int scratches[2] = {first_scratch, second_scratch};
                for (unsigned int side = 0u; side < 2u; side += 1u)
                {
                    if ((sides[side].outcome != RUN_ANSWERED) && (sides[side].outcome != RUN_HELD))
                    {
                        conferred(first, second,
                                  "the pair " + first + " " + second + " witnessed at the link " + link_address +
                                      " of " + third + ", " + ((side == 0u) ? first : second) + " standing there",
                                  *standings_of[side], chain.first, scratches[side], sides[side], std::string(),
                                  link_address);
                    }
                }
                lower("refused");
                continue;
            }
            answered = 1;
            unsigned int place = 0u;
            while ((place < sides[0].cases) && (sides[0].answered[place] == sides[1].answered[place]))
            {
                place += 1u;
            }
            if (place < sides[0].cases)
            {
                lower("apart");
                char answers[64];
                snprintf(answers, sizeof(answers), "%llx/%llx", sides[0].answered[place], sides[1].answered[place]);
                *verdict =
                    "closed " + std::string(link_address) + " " + case_text((*t_places)[place]) + "->" + answers;
                return 1;
            }
            // the link reaches what the part answers where a stand-in's answer leaves the host's for the chain as
            // it stood at a case: a link whose writing reaches nothing answers alike whatever stands there
            const std::vector<std::string> &expected = host[chain.first];
            int reaches = 0;
            for (unsigned int at = 0u; at < sides[0].cases; at += 1u)
            {
                const std::string &bracket = expected[(*t_places)[at]];
                reaches |= ((bracket_holds(bracket, sides[0].answered[at]) == 0) ||
                            (bracket_holds(bracket, sides[1].answered[at]) == 0))
                               ? 1
                               : 0;
            }
            if (!reaches)
            {
                lower("unread");
                continue;
            }
            lower("alike");
            alike_cases += sides[0].cases;
            // two forms that read nothing but the register they write answer the same at every link: the first
            // link that reaches what the part answers and both are answered at is the question answered
            if ((first_names.size() == 1u) && (second_names.size() == 1u))
            {
                break;
            }
        }
        if (witnessed)
        {
            break;
        }
    }
    if (answered)
    {
        *verdict = "open " + std::to_string(alike_cases);
    }
    return answered;
}
