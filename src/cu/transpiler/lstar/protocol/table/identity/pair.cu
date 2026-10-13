// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair.cu: the pair: each pair of the bridge put to the part through the carriers, its verdict written beneath it
#include "identity_internal.h"

// The bridge's pairs put to the part, `pair <form> <form>` in Lstar.klq: two forms whose sameness the text leaves
// open. A pair is put in a carrier, a chain of ours the host computes that holds one form's text as sass.krs writes it:
// the form's arguments are read off the chain's lines, the other form is written in its place with the arguments of
// its parameters' names, and the chain is assembled as the engine assembles it and put to the part over every case
// the host computes the carrier on. The pair is open with its count of cases where the part answers the changed chain
// as the host answers the carrier on every one, and closed at the carrier's address, the question and the position of
// the link, and the first case it answers apart. Each form is tried in the other's place, the first in the second's
// first; a pair neither form can stand in for in any carrier, the second naming a parameter the first does not or
// neither's text standing in a chain, and a pair the part refuses in every carrier, is open with 0. Each verdict is
// written beneath its pair in the bridge in place of the one it held. Where the log is kept, each put is followed by
// the values the form reads there on every case, the carrier cut at the form, for klq_decoder to read its product off
int identity_pair(const char *engine, const char *answers, const char *ksc, const char *klq,
                         const char *machine_path, const char *const *carrier)
{
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    const Ruleset *const sass = code_generator("sass.krs").ruleset(1);
    static SassMachine s_machine;
    if ((sass == NULL) || (sass_machine_read(&s_machine, machine_path) == 0))
    {
        printf("klq_identity pair: sass.krs or the machine file %s is not read\n", machine_path);
        return 1;
    }
    Carriers carriers;
    carriers_read(engine, host, &carriers);
    const std::map<std::string, std::string> &texts = carriers.texts;
    std::map<std::string, std::pair<std::vector<CarrierLink>, long>> &flows = carriers.flows;
    std::map<std::string, std::pair<unsigned int, unsigned int>> &magnitudes = carriers.magnitudes;
    // the seed the order of the links of one magnitude is drawn from, with each put's two forms, KLQ_SEED where it is
    // given
    const unsigned long seed = (getenv("KLQ_SEED") != NULL) ? std::stoul(getenv("KLQ_SEED")) : 1ul;
    std::vector<std::string> bridge;
    std::string line;
    std::istringstream held_bridge(run_file_text(klq));
    while (std::getline(held_bridge, line))
    {
        bridge.push_back((!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line);
    }
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "pair");
    std::string trace_error;
    if (!query_trace_read(query_trace_path(), &s_query_trace, &trace_error))
    {
        printf("klq_identity pair: %s\n", trace_error.c_str());
        run_channel_close();
        return 1;
    }
    kqr_mark(&s_query_record, KQR_SEED_RECORD, std::to_string(seed).c_str());
    // the lessons a run before this one taught, carried into this run (overseer.h)
    overseer_lessons_cleared();
    const unsigned int lessons_carried = overseer_lessons_read(qry_run());
    unsigned int conferences = 0u;
    unsigned int lessons_taught = 0u;
    // the asks the query record held before this cycle, read with R
    const unsigned int recorded_before = s_query_record.asks;
    // each pair's question and places, held with the pair and reached from its own thread, since a pair waits on its
    // round with both
    static thread_local RunQuestion *t_question = NULL;
    static thread_local std::vector<unsigned int> *t_places = NULL;
    const std::unique_ptr<RunQuestion> stalled(new RunQuestion());
    std::vector<unsigned int> stalled_places;
    stall_cases(stalled.get(), &stalled_places);
    // the values the cases are made of: the three operands of every case, as the log's cases write them
    std::vector<unsigned long long> operands;
    for (unsigned int place = 0u; place < stalled->cases; place += 1u)
    {
        for (unsigned int operand = 0u; operand < 3u; operand += 1u)
        {
            operands.push_back(((unsigned long long)stalled->word[place][2u * operand + 1u] << 32u) |
                               stalled->word[place][2u * operand]);
        }
    }
    const std::vector<std::set<unsigned long long>> cased = concept_values(operands);
    unsigned long long asks = 0ull;
    // every choice a put makes at a link, written to the log as `choice <flags> <link>`, the flags of the trace's
    // table that hold there ORed into one word (query_trace.tsv): why a link was passed over, how the part answered
    // it, or why the put ended at it. The flags an open pair's links hold are the questions a further pass takes up
    const auto chosen = [](const char *const *names, unsigned int count, const std::string &link) {
        if (s_ask_log == NULL)
        {
            return;
        }
        unsigned long long word = 0ull;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            word |= query_trace_flag(s_query_trace, names[at]);
        }
        ask_written(s_ask_log, "choice 0x%llx %s\n", word, link.c_str());
    };
    const auto chose = [&chosen](const char *name, const std::string &link) {
        const char *const names[1] = {name};
        chosen(names, 1u, link);
    };
    // The conference a refused put calls, held on the pair's thread once the teacher has called it (overseer.h). The
    // learner hands its statement: what it put, the carrier, what the stand-in does and the stand-in as written. The
    // coherence sets hand what the pair was being asked as: the sets the bridge reads it into and what its texts say
    // of it. The counterexample hands the first case apart the pair holds, this run's or the bridge's. The ruling is
    // written to the trace and its choice to the log, and its guidance given back: OVERSEER_HANDS_OFF where no party
    // called a conference of `asked`
    const auto conferred = [&](const std::string &from, const std::string &to, const std::string &statement,
                               const std::string &standing, const std::string &carrier_name, unsigned int scratch,
                               const RunQuestion &asked, const std::string &closed_here,
                               const std::string &link_address) -> unsigned int {
        QryBuffer *const run = qry_run();
        const std::string identity = question_identity(&asked);
        char joined[OVERSEER_STANDING_LONGEST];
        overseer_standing_joined(standing.c_str(), joined);
        const std::string said = statement + "; carrier " + carrier_name +
                                 (form_addresses(standing) ? "; stand-in addresses" : "") +
                                 (form_controls(standing) ? "; stand-in controls" : "") +
                                 ((scratch != 0u) ? "; stand-in scratch" : "") + "; standing " + joined;
        overseer_account(run, identity.c_str(), "learner", said.c_str());
        std::string sets;
        std::string counterexample = closed_here;
        for (const std::string &line : pair_beneath(bridge, from, to))
        {
            if ((line.rfind("closed ", 0u) == 0u) && counterexample.empty())
            {
                counterexample = line;
            }
            else if ((line.rfind("open ", 0u) != 0u) && (line.rfind("closed ", 0u) != 0u))
            {
                sets += (sets.empty() ? "" : " ") + line;
            }
        }
        const std::string categories = pair_categories(sass, from, to);
        const std::string learning = (sets.empty() ? std::string("no set an answer reads it into yet") : sets) +
                                     (categories.empty() ? std::string() : (", its texts reading it" + categories));
        overseer_account(run, identity.c_str(), "coherence",
                         ("the pair " + from + " " + to + "; learning " + learning).c_str());
        overseer_account(run, identity.c_str(), "counterexample",
                         counterexample.empty()
                             ? "the pair holds no case apart yet"
                             : ("the pair holds its first case apart; counterexample " + counterexample).c_str());
        OverseerRuling ruling;
        if (!overseer_convened(run, identity.c_str(), t_other_way, &ruling))
        {
            return OVERSEER_HANDS_OFF;
        }
        conferences += 1u;
        lessons_taught += ruling.taught ? 1u : 0u;
        chose("conference", link_address);
        if (s_ask_trace != NULL)
        {
            ask_written(s_ask_trace, "conference %s: %s\n", identity.c_str(), ruling.said);
            ask_flushed(s_ask_trace);
        }
        return ruling.guidance;
    };
    // `from` stood in for by `to` at each of its links in each carrier, until the part answers one apart: 1 with its
    // verdict, closed at the first link apart or open with the cases alike at every link the part answered, 0 where the
    // part answered none
    const auto put = [&](const std::string &from, const std::string &to, std::string *verdict) -> int {
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
            const auto &flow = flows[chain.first];
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
            if (ruleset_opcode(sass, to, arguments, scratch_past(magnitudes[chain.first].first, &scratch_taken),
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
            const unsigned int count = sass_assemble_lines(&s_machine, changed.c_str(), SASS_CONTROL_SAFE, code.data(), code.size());
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
                past_closed ? 0 : vector_alike(code, expected, places, &question, &asks, magnitudes[chain.first].first);
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
                std::string from_line = found.str(0);
                std::string to_line = standing;
                std::replace(from_line.begin(), from_line.end(), '\n', ' ');
                std::replace(to_line.begin(), to_line.end(), '\n', ' ');
                ask_written(s_ask_log, "pair %s in place of %s at %s\nfrom %s\nto %s\nonly", to.c_str(), from.c_str(),
                            address, from_line.c_str(), to_line.c_str());
                for (const std::string &name : from_names)
                {
                    if ((given.count(name) != 0u) && (read_by_to.count(name) == 0u))
                    {
                        ask_written(s_ask_log, " %s", given[name].c_str());
                    }
                }
                // each of the form's parameters the stand-in writes a literal in place of: one structure, bound
                if (unified && !fixed.empty())
                {
                    ask_written(s_ask_log, "\nbinds");
                    for (const auto &each : fixed)
                    {
                        const size_t parameter = (size_t)(each.first - 'A');
                        ask_written(s_ask_log, " %s=%s",
                                    (parameter < from_names.size()) ? from_names[parameter].c_str() : "?",
                                    each.second.c_str());
                    }
                }
                ask_written(s_ask_log, "\nvector registers %u lanes %u operands %u delta %ld seed %lu\n",
                            question.registers, question.cases, magnitudes[chain.first].second, found_link.vector.first,
                            seed);
                // what the form's product is made of: each register it reads that a case reaches, its value on every
                // case as the part holds it at the form, the carrier cut there and the value stored in its place
                const std::string form_found = found.str(0);
                const long form_lines = (long)std::count(form_found.begin(), form_found.end(), '\n') +
                                        ((!form_found.empty() && (form_found.back() == '\n')) ? 0 : 1);
                // the product asked over the cases the question holds: what the form reads, and what it and its
                // stand-in write
                // what the form reads, in the order it names its parameters: a parameter the chain binds to a literal
                // is read as that literal on every case, so that every carrier of the form reads it as one set of
                // values. A parameter inside `[` and `]` says where the form reads and is no value it reads. With none
                // bound, the registers in the order the form names them
                const std::vector<std::string> registers_read =
                    carrier_form_reads(flow.first, flow.second, form_found, link_at);
                std::vector<std::string> form_reads;
                std::map<std::string, unsigned long long> literal_reads;
                int addressed = 0;
                for (const std::string &token : form_tokens(from_text))
                {
                    addressed += (token == "[") ? 1 : (token == "]") ? -1 : 0;
                    const size_t parameter = (token.size() >= 3u) ? (size_t)(token[1] - 'A') : from_names.size();
                    if ((token[0] != '\x01') || (addressed > 0) || (parameter >= from_names.size()) ||
                        (given.count(from_names[parameter]) == 0u))
                    {
                        continue;
                    }
                    const std::string part = token.substr(3u);
                    const std::string value = given[from_names[parameter]];
                    const std::string named = value + part;
                    std::string read;
                    if (std::find(registers_read.begin(), registers_read.end(), named) != registers_read.end())
                    {
                        read = named;
                    }
                    else if ((value == "RZ") || (value == "PT") || (value == "!PT") ||
                             std::regex_match(value, std::regex("-?(0x[0-9a-fA-F]+|[0-9]+)")))
                    {
                        // a flag bound to a literal is read as a flag, `P` before its name
                        const int flag = (value == "PT") || (value == "!PT");
                        read = (flag ? "P=" : "=") + from_names[parameter] + part;
                        literal_reads[read] = (value == "PT") ? 1ull
                                              : ((value == "RZ") || (value == "!PT"))
                                                  ? 0ull
                                                  : (unsigned long long)std::stoll(value, nullptr, 0);
                    }
                    if (!read.empty() && (std::find(form_reads.begin(), form_reads.end(), read) == form_reads.end()))
                    {
                        form_reads.push_back(read);
                    }
                }
                for (const std::string &read : registers_read)
                {
                    if (std::find(form_reads.begin(), form_reads.end(), read) == form_reads.end())
                    {
                        form_reads.push_back(read);
                    }
                }
                const std::vector<std::string> &reads_named = literal_reads.empty() ? registers_read : form_reads;
                const auto product_asked = [&](ConceptReads &put_reads, std::vector<unsigned long long> *put_writes) {
                    // the cuts the product asks, put together: one before the form for each value it reads that a
                    // register holds, and one after it for each side's product. Each is the carrier, the value stored
                    // in place of its answer and nothing between, and a cut the part refuses costs no other cut its
                    // answer
                    std::vector<std::vector<unsigned char>> cut_codes;
                    std::vector<int> cut_dangerous;
                    // the stand-in's cut holds it at the form's link, its slot; every other cut is the carrier's own
                    // and holds none
                    std::vector<unsigned int> cut_slots;
                    for (const std::string &read : reads_named)
                    {
                        if ((literal_reads.count(read) != 0u) || (read[0] == '['))
                        {
                            continue;
                        }
                        const std::string cut =
                            carrier_cut_at_read(flow.first, flow.second, chain.second, link_at, form_lines, read);
                        std::vector<unsigned char> cut_code(16u * 4096u);
                        const unsigned int cut_count =
                            cut.empty() ? 0u
                                        : sass_assemble_lines(&s_machine, cut.c_str(), SASS_CONTROL_SAFE,
                                                              cut_code.data(), cut_code.size());
                        cut_code.resize(16u * cut_count);
                        if (s_ask_trace != NULL)
                        {
                            ask_written(s_ask_trace, "reads %s, the carrier cut at %s, %u links:\n%s", read.c_str(),
                                        address, cut_count, cut.c_str());
                            ask_flushed(s_ask_trace);
                        }
                        cut_codes.push_back(cut_code);
                        cut_dangerous.push_back(0);
                        cut_slots.push_back(RUN_SLOT_NONE);
                    }
                    // the form's product and its stand-in's, each the value it writes on every case as the part holds
                    // it at the form, the carrier cut just after it: the links after the form are a delta of their own
                    const std::string sides[2] = {form_found, standing};
                    const char *const side_names[2] = {"from", "to"};
                    for (unsigned int side = 0u; side < 2u; side += 1u)
                    {
                        int wide = 0;
                        const std::string product = carrier_form_product(sides[side], &wide);
                        // the stand-in's side is left unwritten where a lesson holds the stand-in off the part
                        const std::string cut =
                            (product.empty() || ((side == 1u) && stand_in_held))
                                ? std::string()
                                : carrier_cut_at_read(flow.first, flow.second, chain.second, link_at, form_lines,
                                                      product, sides[side], wide);
                        std::vector<unsigned char> cut_code(16u * 4096u);
                        const unsigned int cut_count =
                            cut.empty() ? 0u
                                        : sass_assemble_lines(&s_machine, cut.c_str(), SASS_CONTROL_SAFE,
                                                              cut_code.data(), cut_code.size());
                        cut_code.resize(16u * cut_count);
                        if (s_ask_trace != NULL)
                        {
                            ask_written(s_ask_trace, "writes %s %s, the carrier cut after %s, %u links:\n%s",
                                        side_names[side], product.c_str(), address, cut_count, cut.c_str());
                            ask_flushed(s_ask_trace);
                        }
                        cut_codes.push_back(cut_code);
                        cut_dangerous.push_back((side == 1u) ? t_dangerous : 0);
                        cut_slots.push_back((side == 1u) ? (unsigned int)link_at : RUN_SLOT_NONE);
                    }
                    std::vector<RunQuestion> cut_answers;
                    cuts_asked(cut_codes, cut_dangerous, cut_slots, &question, &asks, magnitudes[chain.first].first,
                               &cut_answers);
                    size_t cut_at = 0u;
                    for (const std::string &read : reads_named)
                    {
                        // a parameter the chain binds to a literal is that literal on every case, and asks nothing
                        if (literal_reads.count(read) != 0u)
                        {
                            ask_written(s_ask_log, "reads %s", read.c_str());
                            put_reads.push_back(std::make_pair(read, std::vector<unsigned long long>()));
                            for (unsigned int place = 0u; place < question.cases; place += 1u)
                            {
                                ask_written(s_ask_log, " %llx", literal_reads[read]);
                                put_reads.back().second.push_back(literal_reads[read]);
                            }
                            ask_written(s_ask_log, "\n");
                            continue;
                        }
                        // an operand of a case the form loads itself is read as the question holds it, and asks nothing
                        if (read[0] == '[')
                        {
                            const unsigned int operand = (unsigned int)std::stoul(read.substr(6u));
                            ask_written(s_ask_log, "reads [%u]", operand);
                            put_reads.push_back(
                                std::make_pair("[" + std::to_string(operand) + "]", std::vector<unsigned long long>()));
                            for (unsigned int place = 0u; place < question.cases; place += 1u)
                            {
                                const unsigned int *const words = question.word[place];
                                const unsigned long long value =
                                    ((2u * operand + 1u) < RUN_IN_WORDS)
                                        ? (((unsigned long long)words[2u * operand + 1u] << 32u) | words[2u * operand])
                                        : 0ull;
                                ask_written(s_ask_log, " %llx", value);
                                put_reads.back().second.push_back(value);
                            }
                            ask_written(s_ask_log, "\n");
                            continue;
                        }
                        const RunQuestion &cut = cut_answers[cut_at];
                        const int unwritten = cut_codes[cut_at].empty();
                        cut_at += 1u;
                        ask_written(s_ask_log, "reads %s", read.c_str());
                        put_reads.push_back(std::make_pair(read, std::vector<unsigned long long>()));
                        if (unwritten || (cut.outcome != RUN_ANSWERED))
                        {
                            ask_written(s_ask_log, " refused %s\n", unwritten ? "unwritten" : cut.refused);
                            continue;
                        }
                        for (unsigned int place = 0u; place < question.cases; place += 1u)
                        {
                            ask_written(s_ask_log, " %llx", cut.answered[place]);
                            put_reads.back().second.push_back(cut.answered[place]);
                        }
                        ask_written(s_ask_log, "\n");
                    }
                    for (unsigned int side = 0u; side < 2u; side += 1u)
                    {
                        const RunQuestion &cut = cut_answers[cut_at];
                        const int unwritten = cut_codes[cut_at].empty();
                        cut_at += 1u;
                        ask_written(s_ask_log, "writes %s", side_names[side]);
                        if (unwritten || (cut.outcome != RUN_ANSWERED))
                        {
                            ask_written(s_ask_log, " refused %s\n", unwritten ? "unwritten" : cut.refused);
                            continue;
                        }
                        for (unsigned int place = 0u; place < question.cases; place += 1u)
                        {
                            ask_written(s_ask_log, " %llx", cut.answered[place]);
                            put_writes[side].push_back(cut.answered[place]);
                        }
                        ask_written(s_ask_log, "\n");
                    }
                    ask_flushed(s_ask_log);
                };
                // the values the pair's forms write of their own, each put to the product with every case value
                ask_written(s_ask_log, "qualifies");
                for (const unsigned long long value : qualifiers)
                {
                    ask_written(s_ask_log, " %llx", value);
                }
                ask_written(s_ask_log, "\n");
                const size_t signal_before = concept_signal(product);
                ConceptReads put_reads;
                std::vector<unsigned long long> put_writes[2];
                product_asked(put_reads, put_writes);
                concept_product_held(put_reads, put_writes[0], put_writes[1], pair_cased, &product);
                // a product the question's cases leave short of whole is put over every combination of the values the
                // cases are made of, at each put of a shape no link read before whose every read the part answered, run
                // by run until the product is whole or every run is put
                int read_whole = !put_reads.empty();
                for (const auto &each : put_reads)
                {
                    read_whole &= !each.second.empty() ? 1 : 0;
                }
                if (shape_new && read_whole && !concept_whole(product, pair_cased))
                {
                    const unsigned int runs = whole_cases(&question, qualifiers, 0u);
                    for (unsigned int run = 0u; (run < runs) && !concept_whole(product, pair_cased); run += 1u)
                    {
                        whole_cases(&question, qualifiers, run);
                        ask_written(s_ask_log, "pair %s in place of %s at %s\nqualified", to.c_str(), from.c_str(),
                                    address);
                        for (const unsigned long long value : qualifiers)
                        {
                            ask_written(s_ask_log, " %llx", value);
                        }
                        ask_written(s_ask_log, "\n");
                        ConceptReads whole_reads;
                        std::vector<unsigned long long> whole_writes[2];
                        product_asked(whole_reads, whole_writes);
                        concept_product_held(whole_reads, whole_writes[0], whole_writes[1], pair_cased, &product);
                    }
                    stall_cases(&question, &places);
                }
                band_truthy |= (concept_signal(product) > signal_before) ? 1 : 0;
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
    };
    // the bridge's keys, the schema's forms, in its order
    std::vector<std::string> keys;
    for (const std::string &entry : bridge)
    {
        if (entry.rfind("key ", 0u) == 0u)
        {
            keys.push_back(entry.substr(4u));
        }
    }
    // The lower witness: a pair whose puts asked nothing, neither form standing at a link that carries the cases, is
    // put at the links of a third form, the first of the bridge's keys whose parameters name every parameter of both
    // and that stands at such a link. Both forms stand in at each of its links, the part answers both over the cases,
    // and the two answers are held against each other: the first case apart closes the pair as two operations, and
    // alike on every case leaves it open with its count of cases. A link of a shape read before is passed over, and so
    // is one whose writing reaches nothing the part answers, where both stand-ins answer as the chain did as it stood;
    // two forms that read nothing but the register they write are answered at the first link left. 1 with the verdict
    // where the part answered both at a link, 0 otherwise
    const auto witness_lower = [&](const std::string &first, const std::string &second, std::string *verdict) -> int {
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
                const auto &flow = flows[chain.first];
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
                                    scratch_past(magnitudes[chain.first].first, &first_scratch),
                                    first_standing) == 0) ||
                    (ruleset_opcode(sass, second, second_arguments,
                                    scratch_past(magnitudes[chain.first].first, &second_scratch),
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
                        &s_machine, standings[side].c_str(), SASS_CONTROL_SAFE, codes[side].data(), codes[side].size());
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
                    sides[side].registers = magnitudes[chain.first].first;
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
    };
    std::vector<std::string> written;
    unsigned int open = 0u;
    unsigned int closed = 0u;
    unsigned int unasked = 0u;
    int beneath = 0;
    ask_files_opened();
    // what the texts of a pair's two forms say of it before any ask, written to the log
    const auto pair_facts = [&](const std::string &first, const std::string &second) {
        ask_written(s_ask_log, "facts %s %s%s\n", first.c_str(), second.c_str(),
                    pair_categories(sass, first, second).c_str());
    };
    // every pair of the bridge, in its order, the question and places its put asks with, the hash of what its put
    // reads, and the verdict it came to
    struct PairPut
    {
        std::string first;
        std::string second;
        std::string verdict;
        int put_whole;
        std::unique_ptr<RunQuestion> question;
        std::vector<unsigned int> places;
        std::string reads;
    };
    // what every put reads of the engine: each chain's text and the host's answers to its cases, by its question
    unsigned long long engine_read = 0xcbf29ce484222325ull;
    for (const auto &chain : texts)
    {
        engine_read = record_hash(chain.first.c_str(), chain.first.size() + 1u, engine_read);
        engine_read = record_hash(chain.second.c_str(), chain.second.size() + 1u, engine_read);
        for (const std::string &answer : host[chain.first])
        {
            engine_read = record_hash(answer.c_str(), answer.size() + 1u, engine_read);
        }
    }
    // The scheduler: the record's gray entries are the queue. A pair closed on the record's path, whose reads are as
    // they were, is settled: its verdict is the record's, and it is put no more, since a put runs again only where what
    // it reads has changed. Every other pair, open, with no path, or reading what changed, is gray, and the rounds put
    // the gray pairs alone
    std::map<std::string, KqrPath> recorded_paths;
    static KqrPath s_recorded;
    for (unsigned int at = 0u; kqr_path_read(&s_query_record, at, &s_recorded); at += 1u)
    {
        recorded_paths[std::string(s_recorded.first) + " " + s_recorded.second] = s_recorded;
    }
    std::vector<PairPut> pairs;
    size_t settled = 0u;
    for (const std::string &entry : bridge)
    {
        if (entry.rfind("pair ", 0u) == 0u)
        {
            std::stringstream words(entry);
            std::string kind;
            PairPut pair{std::string(),
                         std::string(),
                         "open 0",
                         0,
                         std::unique_ptr<RunQuestion>(new RunQuestion()),
                         std::vector<unsigned int>(),
                         std::string()};
            words >> kind >> pair.first >> pair.second;
            std::vector<std::string> names;
            const std::string first_text = marked(sass, pair.first, &names);
            const std::string second_text = marked(sass, pair.second, &names);
            unsigned long long reads = record_hash(first_text.c_str(), first_text.size() + 1u, engine_read);
            reads = record_hash(second_text.c_str(), second_text.size() + 1u, reads);
            char hashed[17];
            snprintf(hashed, sizeof(hashed), "%016llx", reads);
            pair.reads = hashed;
            const auto path = recorded_paths.find(pair.first + " " + pair.second);
            if ((path != recorded_paths.end()) && path->second.read &&
                (strtoull(pair.reads.c_str(), NULL, 16) == path->second.reads) &&
                (strncmp(path->second.verdict, "closed ", 7u) == 0))
            {
                pair.verdict = path->second.verdict;
                pair.put_whole = 1;
                settled += 1u;
            }
            pairs.push_back(std::move(pair));
        }
    }
    // Each pair put in a thread of its own, run only in its turn: its facts, its put, and the stand-in the other way
    // round where the form's own way answered nothing. Its trace and its log are logs of its own, held in memory and
    // handed whole to the query's writer once it is put, after what the thread that carries the asks wrote before it
    s_round_asks.assign(pairs.size(), std::vector<CarriedAsk>());
    std::vector<std::thread> threads;
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        if (pairs[at].put_whole)
        {
            continue;
        }
        threads.emplace_back([&, at]() {
            {
                std::unique_lock<std::mutex> lock(s_turn_lock);
                s_turn_changed.wait(lock, [at]() { return s_turn == (int)at; });
            }
            t_pair = (int)at;
            AskLog pair_trace = {s_trace_whole.name, std::string()};
            AskLog pair_log = {s_log_whole.name, std::string()};
            if (s_ask_run != NULL)
            {
                s_ask_trace = &pair_trace;
                s_ask_log = &pair_log;
            }
            PairPut &pair = pairs[at];
            t_question = pair.question.get();
            t_places = &pair.places;
            stall_cases(t_question, t_places);
            if (s_ask_log != NULL)
            {
                pair_facts(pair.first, pair.second);
            }
            // the form's own way first, then the other way round where it answered nothing or a conference turned it,
            // then the lower witness where neither answered
            t_turned = 0;
            t_other_way = 0;
            const int one_way = put(pair.first, pair.second, &pair.verdict);
            int either_way = one_way;
            if (!one_way || t_turned)
            {
                t_other_way = 1;
                std::string other_verdict;
                const int other_way = put(pair.second, pair.first, &other_verdict);
                pair.verdict = one_way ? pair_verdicts_joined(pair.verdict, other_verdict) : other_verdict;
                either_way = one_way || other_way;
            }
            if (!either_way)
            {
                witness_lower(pair.first, pair.second, &pair.verdict);
            }
            if (s_ask_log != NULL)
            {
                s_ask_trace = NULL;
                s_ask_log = NULL;
                // the thread that carries the asks waits out this pair's turn: what it wrote before the pair goes first
                ask_handed(&s_trace_whole);
                ask_handed(&s_log_whole);
                ask_handed(&pair_trace);
                ask_handed(&pair_log);
            }
            std::unique_lock<std::mutex> lock(s_turn_lock);
            pair.put_whole = 1;
            s_turn = -1;
            s_turn_changed.notify_all();
        });
    }
    // The rounds: each pair not yet put runs in its turn until it asks or is put, and the asks the round was handed
    // are answered together. The rounds loop while a pair is left to put, and end once every pair is put
    unsigned int round = 0u;
    size_t left = pairs.size() - settled;
    printf("klq_identity pair: %zu pairs settled on the record, %zu gray in the queue\n", settled, left);
    while (left != 0u)
    {
        const std::chrono::steady_clock::time_point round_began = std::chrono::steady_clock::now();
        const unsigned long long handed_before = s_round_handed;
        const unsigned long long held_before = s_round_held;
        const unsigned long long processes_before = s_round_processes;
        round += 1u;
        kqr_mark(&s_query_record, KQR_ROUND_RECORD, std::to_string(round).c_str());
        std::vector<CarriedAsk> carried;
        unsigned int asking = 0u;
        for (size_t at = 0u; at < pairs.size(); at += 1u)
        {
            std::unique_lock<std::mutex> lock(s_turn_lock);
            if (pairs[at].put_whole)
            {
                continue;
            }
            s_turn = (int)at;
            s_turn_changed.notify_all();
            s_turn_changed.wait(lock, []() { return s_turn == -1; });
            asking += s_round_asks[at].empty() ? 0u : 1u;
            carried.insert(carried.end(), s_round_asks[at].begin(), s_round_asks[at].end());
            s_round_asks[at].clear();
        }
        asks_answered(carried);
        left = (size_t)std::count_if(pairs.begin(), pairs.end(), [](const PairPut &pair) { return !pair.put_whole; });
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - round_began).count();
        printf("klq_identity round %u: %u pairs asked, %llu asks handed, %llu answered from what was held, %llu "
               "processes, %zu of %zu pairs put, %.1f seconds\n",
               round, asking, s_round_handed - handed_before, s_round_held - held_before,
               s_round_processes - processes_before, pairs.size() - left, pairs.size(), seconds);
        fflush(stdout);
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    // the bridge written with each pair's verdict beneath it, in place of the verdict it held
    size_t paired = 0u;
    for (const std::string &entry : bridge)
    {
        const int verdict_line = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        beneath = (entry.rfind("pair ", 0u) == 0u) || (beneath && verdict_line);
        if (verdict_line && beneath)
        {
            continue;
        }
        written.push_back(entry);
        if (entry.rfind("pair ", 0u) != 0u)
        {
            continue;
        }
        const PairPut &pair = pairs[paired];
        paired += 1u;
        open += ((pair.verdict.rfind("open ", 0u) == 0u) && (pair.verdict != "open 0")) ? 1u : 0u;
        closed += (pair.verdict.rfind("closed ", 0u) == 0u) ? 1u : 0u;
        unasked += (pair.verdict == "open 0") ? 1u : 0u;
        printf("  %s %s: %s\n", pair.first.c_str(), pair.second.c_str(), pair.verdict.c_str());
        written.push_back(pair.verdict);
    }
    run_channel_close();
    RunWritten file = {klq, 1, std::string()};
    for (const std::string &kept : written)
    {
        run_print(&file, "%s\n", kept.c_str());
    }
    if (!run_closed(&file))
    {
        printf("klq_identity pair: %s could not be written\n", klq);
        return 1;
    }
    printf("klq_identity pair: %llu asks, %u open, %u closed, %u not asked, written to %s\n", asks, open, closed,
           unasked, klq);
    // the lessons held, those carried in and those taught, handed to the run for the next run to carry
    overseer_lessons_handed(qry_run());
    printf("klq_identity pair: %u conferences called by refusals, %u lessons taught, %u carried in\n", conferences,
           lessons_taught, lessons_carried);
    // the path this cycle read off the asks, each pair and its verdict, an open pair the question a further pass takes
    // up, written to the query record with every ask it held and every ask this cycle put once R is written back
    kqr_paths_clear(&s_query_record);
    static KqrPath s_path;
    for (const PairPut &pair : pairs)
    {
        snprintf(s_path.first, sizeof(s_path.first), "%s", pair.first.c_str());
        snprintf(s_path.second, sizeof(s_path.second), "%s", pair.second.c_str());
        s_path.read = pair.reads.empty() ? 0 : 1;
        s_path.reads = strtoull(pair.reads.c_str(), NULL, 16);
        snprintf(s_path.verdict, sizeof(s_path.verdict), "%s", pair.verdict.c_str());
        kqr_path_add(&s_query_record, &s_path);
        pair_row_handed(pair.first, pair.second, s_path.read, s_path.reads, pair.verdict);
    }
    printf("klq_identity pair: %u asks held in the query record, %u of them put this cycle\n", s_query_record.asks,
           s_query_record.asks - recorded_before);
    return 0;
}
