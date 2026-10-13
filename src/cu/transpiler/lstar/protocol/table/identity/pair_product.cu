// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair_product.cu: the put beneath its ask in the log, and the product asked over the cases at its link: what
// the form reads there, and what it and its stand-in write
#include "pair.h"

// The put beneath its ask in the log: the link as it stood, the link standing in its place, each on one line of
// the log, and the operands only the form it stood as names. Then the product: what the form reads at the link
// and what it and its stand-in write, asked over the cases and put to the product, and a product the question's
// cases leave short of whole asked again over every combination of the values the cases are made of. 1 where a
// put added a row that came back one way, signal, 0 where it added none or only rows that came back both ways
int PutLink::logged(PairWork &work) const
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
                question.registers, question.cases, work.carriers.magnitudes[chain.first].second, delta,
                work.seed);
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
                            : sass_assemble_lines(work.machine, cut.c_str(), SASS_CONTROL_SAFE,
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
                            : sass_assemble_lines(work.machine, cut.c_str(), SASS_CONTROL_SAFE,
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
        cuts_asked(cut_codes, cut_dangerous, cut_slots, &question, &work.asks, work.carriers.magnitudes[chain.first].first,
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
    return (concept_signal(product) > signal_before) ? 1 : 0;
}
