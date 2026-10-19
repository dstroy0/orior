// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stall.cu: the stall walk: the soonest a result is read, asked of the part link by link
#include "identity_internal.h"

// The soonest each operation's result is read, asked of the part. Every chain of ours the host also computes is put
// with every stall the longest, all in one round, and a chain the part answers alike with the host there is a probe
// wherever an instruction that writes a register is read by the very next. Every stall of each pair's writer below the
// longest is then put on each of its probes, at most three, all in one round, since none waits on another's answer. A
// stall is truthy where every probe of its pair answers alike, and the soonest is the least stall from which every
// longer one is truthy: a shorter stall is not answered as no worse than a longer one, and at 0 a pair answers alike
// where it is refused at 1. An instruction that sets a write barrier is read behind the barrier and not behind its
// stall, and is not asked. A load's address is written in every chain of ours by IMAD.WIDE.U32, and each probe of a
// pair whose reader loads through the pair the multiply writes is written again with the address written last by each
// other word the ruleset writes a pair with: wide_add, the base read from the constant bank by wide_pack and the offset
// by wide_mul_word, added; and wide_pack, the multiply's pair moved into the address's. Each is asked at every stall of
// the word's last link, the one the load reads after, every other stall the longest, in the same round. Each pair
// that no chain of ours holds and an arrangement of the .kdm beside the machine file does is probed in that
// arrangement, written at its operator's link as the cost puts it (arrangement_probes). A round is
// carried STALL_CARRIED_MOST questions to a process. Each pair's soonest is written to the .ksc as the part's answer on
// the run channel, `run answers <stall> stall <writer> <reader>`, in place of those it held, and the run's blob
// stall.txt holds every pair with its probes and what came back at every stall: `=` alike on every probe, `x` apart on
// one, `!` refused on one
int identity_stall(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                          const char *machine_path, const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("write_barrier") == 0u))
    {
        printf("klq_identity stall: %s gives no stall field or no write barrier field\n", ksc);
        return 1;
    }
    const StallField stall = fields["stall"];
    const StallField barrier = fields["write_barrier"];
    const unsigned long long longest = (1ull << stall.bits) - 1ull;
    const unsigned long long none = (1ull << barrier.bits) - 1ull;
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "stall");
    // a chain of ours holds no slot, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.slot = RUN_SLOT_NONE;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    // the questions put together, STALL_CARRIED_MOST to a process, none waiting on another's answer: each one's mark,
    // `=` alike with the host's answers to the chain `numbers` names at its place on every case, `x` apart on one, `!`
    // refused
    const auto asked_marks = [&](const std::vector<std::vector<unsigned char>> &codes,
                                 const std::vector<std::string> &numbers) -> std::string {
        std::string marks;
        for (size_t first = 0u; first < codes.size(); first += STALL_CARRIED_MOST)
        {
            const size_t count = std::min((size_t)STALL_CARRIED_MOST, codes.size() - first);
            const std::vector<std::vector<unsigned char>> carried(codes.begin() + (long)first,
                                                                  codes.begin() + (long)(first + count));
            std::vector<RunQuestion> asked;
            cuts_asked(carried, std::vector<int>(count, 0), std::vector<unsigned int>(count, RUN_SLOT_NONE),
                       &s_question, &asks, 255u, &asked);
            for (size_t at = 0u; at < count; at += 1u)
            {
                char mark = (asked[at].outcome != RUN_ANSWERED) ? '!' : '=';
                for (unsigned int place = 0u; (mark == '=') && (place < s_question.cases); place += 1u)
                {
                    mark = (bracket_holds(host[numbers[first + at]][places[place]], asked[at].answered[place]) == 0)
                               ? 'x'
                               : mark;
                }
                marks += mark;
            }
        }
        return marks;
    };
    // every chain of ours with every stall the longest, put in one round
    std::vector<std::string> chain_numbers;
    std::vector<std::vector<unsigned char>> chain_codes;
    std::vector<std::vector<Link>> chain_links;
    for (const auto &held : host)
    {
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if (!chain_code_read(engine, held.first, &code, &links))
        {
            continue;
        }
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            stall_bits_write(&code, index, stall, longest);
        }
        chain_numbers.push_back(held.first);
        chain_codes.push_back(code);
        chain_links.push_back(links);
    }
    const std::string chain_marks = asked_marks(chain_codes, chain_numbers);
    // the probes of each pair, a writer's operation and its reader's, in the chains the part answers alike
    std::map<std::pair<std::string, std::string>, std::vector<StallProbe>> pairs;
    unsigned int unanswered = 0u;
    for (size_t chain = 0u; chain < chain_codes.size(); chain += 1u)
    {
        if (chain_marks[chain] != '=')
        {
            unanswered += 1u;
            printf("  %s: not alike at the longest stalls, and no probe\n", chain_numbers[chain].c_str());
            continue;
        }
        const std::vector<unsigned char> &code = chain_codes[chain];
        const std::vector<Link> &links = chain_links[chain];
        for (size_t index = 0u; (index + 1u) < links.size(); index += 1u)
        {
            const size_t place = (size_t)std::stoul(links[index].position, nullptr, 16) / 16u;
            if (stall_bits(code, place, barrier) != none)
            {
                continue;
            }
            const std::set<std::string> written = stall_written(links[index]);
            const std::set<std::string> read = stall_read(links[index + 1u]);
            const int reads = std::any_of(written.begin(), written.end(),
                                          [&](const std::string &name) { return read.count(name) != 0u; });
            if (!reads)
            {
                continue;
            }
            std::vector<std::string> operands;
            const std::pair<std::string, std::string> pair(stall_operation(links[index], &operands),
                                                           stall_operation(links[index + 1u], &operands));
            pairs[pair].push_back(StallProbe{chain_numbers[chain], code, place});
        }
    }
    printf("klq_identity stall: %zu chains, %u not alike at the longest stalls, %zu pairs, carried by %s\n",
           chain_codes.size(), unanswered, pairs.size(), run_channel_carrier());
    // the probes a pair is walked over at most, each from a chain of its own
    const size_t most = 3u;
    std::vector<std::string> rows;
    RunWritten table = {(folder + "/stall.txt"), 0, std::string()};
    // every stall below the longest of each pair's writer, on each of its probes; the longest each probe answered alike
    std::vector<StallAsk> stall_asks;
    std::vector<std::vector<unsigned char>> stall_codes;
    for (const auto &pair : pairs)
    {
        std::set<std::string> numbers;
        for (const StallProbe &probe : pair.second)
        {
            if ((numbers.size() >= most) || !numbers.insert(probe.number).second)
            {
                continue;
            }
            for (unsigned long long step = 0ull; step < longest; step += 1ull)
            {
                std::vector<unsigned char> turned = probe.code;
                stall_bits_write(&turned, probe.writer, stall, step);
                stall_codes.push_back(turned);
                stall_asks.push_back(StallAsk{pair.first, probe.number, step});
            }
        }
    }
    // the load's address written last by the other words that write a pair, every stall of the last link, in the round
    const Ruleset *const sass = identity_ruleset();
    const int writable = (sass != NULL);
    const auto none_named = [](const std::string &) { return std::string(); };
    const std::regex wide_written("^\\s*IMAD\\.WIDE\\.U32\\s+(R[0-9]+),\\s*(R[0-9]+),\\s*(R[0-9]+),\\s*"
                                  "c\\[0x0\\]\\[(0x[0-9a-fA-F]+)\\+([0-9]+)\\];?\\s*$");
    const std::regex first_word("^\\s*(\\S+)");
    for (const auto &pair : pairs)
    {
        if (!writable || (pair.first.first != "IMAD.WIDE.U32") || (pair.first.second.compare(0u, 3u, "LDG") != 0))
        {
            continue;
        }
        std::set<std::string> numbers;
        for (const StallProbe &probe : pair.second)
        {
            if ((numbers.size() >= most) || !numbers.insert(probe.number).second)
            {
                continue;
            }
            std::ifstream chain(std::string(engine) + "/" + probe.number + ".sass", std::ios::binary);
            const std::string chain_text((std::istreambuf_iterator<char>(chain)), std::istreambuf_iterator<char>());
            const std::vector<std::string> lines = carrier_pieces(chain_text, '\n');
            // the line of the probe's writer, its instructions counted past the labels
            size_t line_at = lines.size();
            size_t instructions = 0u;
            for (size_t at = 0u; at < lines.size(); at += 1u)
            {
                const std::string kept = trimmed(lines[at]);
                if (kept.empty() || (kept.back() == ':'))
                {
                    continue;
                }
                if (instructions == probe.writer)
                {
                    line_at = at;
                    break;
                }
                instructions += 1u;
            }
            std::smatch found;
            if ((line_at == lines.size()) || !std::regex_match(lines[line_at], found, wide_written))
            {
                continue;
            }
            const unsigned int base = (chain_registers(engine, probe.number) + 1u) & ~1u;
            const std::string held = ruleset_register(sass, "file", base);
            const std::string offset = ruleset_register(sass, "file", base + 2u);
            const std::string low = "c[0x0][" + found[4].str() + "+" + found[5].str() + "]";
            const std::string high =
                "c[0x0][" + found[4].str() + "+" + std::to_string(std::stoul(found[5].str()) + 4u) + "]";
            std::string by_add;
            std::string by_pack =
                "\tIMAD.WIDE.U32 \t" + held + ", " + found[2].str() + ", " + found[3].str() + ", " + low + ";\n";
            // each word appended to its own probe's text, as the ruleset writes it
            int ways = ruleset_opcode(sass, "wide_pack", {held, low, high}, none_named, by_add);
            ways = ways &&
                   ruleset_opcode(sass, "wide_mul_word", {offset, found[2].str(), found[3].str()}, none_named, by_add);
            ways = ways && ruleset_opcode(sass, "wide_add", {found[1].str(), held, offset}, none_named, by_add);
            ways = ways && ruleset_opcode(sass, "wide_pack", {found[1].str(), held, held + ".hi"}, none_named, by_pack);
            if (!ways)
            {
                continue;
            }
            std::string before;
            std::string after;
            for (size_t at = 0u; at < lines.size(); at += 1u)
            {
                (at < line_at) ? (before += lines[at] + "\n") : (at > line_at) ? (after += lines[at] + "\n") : before;
            }
            for (const std::string &way : {by_add, by_pack})
            {
                const std::vector<std::vector<unsigned char>> ways_code =
                    vendor_assembled(machine_path, std::vector<std::string>{before + way, before + way + after});
                const unsigned int written_count = (unsigned int)(ways_code[0].size() / 16u);
                std::vector<unsigned char> code = ways_code[1];
                const unsigned int count = (unsigned int)(code.size() / 16u);
                const std::vector<std::string> way_lines = carrier_pieces(way, '\n');
                std::smatch last;
                if ((written_count == 0u) || (count == 0u) || way_lines.empty() ||
                    !std::regex_search(way_lines.back(), last, first_word))
                {
                    continue;
                }
                for (size_t index = 0u; index < count; index += 1u)
                {
                    stall_bits_write(&code, index, stall, longest);
                }
                for (unsigned long long step = 0ull; step <= longest; step += 1ull)
                {
                    std::vector<unsigned char> turned = code;
                    stall_bits_write(&turned, written_count - 1u, stall, step);
                    stall_codes.push_back(turned);
                    stall_asks.push_back(
                        StallAsk{std::make_pair(last[1].str(), pair.first.second), probe.number, step});
                }
            }
        }
    }
    // the pairs the .kdm's arrangements hold and no chain of ours does, the rotates among them, in the same round
    std::set<std::pair<std::string, std::string>> held_pairs;
    for (const auto &pair : pairs)
    {
        held_pairs.insert(pair.first);
    }
    if (writable)
    {
        const std::string machine_file(machine_path);
        const size_t typed = machine_file.rfind(".khw");
        const std::string stem = ((typed != std::string::npos) && (typed + 4u == machine_file.size()))
                                     ? machine_file.substr(0u, typed)
                                     : machine_file;
        arrangement_probes(engine, host, stem + ".kdm", sass, machine_path, stall, longest,
                           held_pairs, most, &stall_asks, &stall_codes);
    }
    // the one round, and what came back at every stall of each pair
    std::vector<std::string> stall_numbers;
    for (const StallAsk &each : stall_asks)
    {
        stall_numbers.push_back(each.number);
    }
    printf("klq_identity stall: %zu questions, every stall of every pair, put in one round\n", stall_codes.size());
    const std::string marks = asked_marks(stall_codes, stall_numbers);
    std::map<std::pair<std::string, std::string>, std::string> came_back;
    std::map<std::pair<std::string, std::string>, std::set<std::string>> asked_over;
    for (size_t at = 0u; at < stall_asks.size(); at += 1u)
    {
        const StallAsk &each = stall_asks[at];
        std::string &pair_marks = came_back[each.pair];
        pair_marks.resize((size_t)longest + 1u, '=');
        asked_over[each.pair].insert(each.number);
        char &held_mark = pair_marks[(size_t)each.step];
        held_mark = (held_mark == '!') ? '!' : (marks[at] != '=') ? marks[at] : held_mark;
    }
    for (const auto &each : came_back)
    {
        // the soonest is the least stall from which every longer one answers alike
        unsigned long long soonest = longest + 1ull;
        while ((soonest > 0ull) && (each.second[(size_t)soonest - 1u] == '='))
        {
            soonest -= 1ull;
        }
        std::string over;
        for (const std::string &number : asked_over[each.first])
        {
            over += " " + number;
        }
        run_print(&table, "%s %s soonest %llu, stalls 0 to %llu: %s, over%s\n", each.first.first.c_str(),
                  each.first.second.c_str(), soonest, longest, each.second.c_str(), over.c_str());
        printf("  %s then %s: soonest %llu, stalls 0 to %llu %s\n", each.first.first.c_str(), each.first.second.c_str(),
               soonest, longest, each.second.c_str());
        if (soonest <= longest)
        {
            char row[256];
            snprintf(row, sizeof(row), "run answers %08llx stall %s %s", soonest, each.first.first.c_str(),
                     each.first.second.c_str());
            rows.push_back(row);
        }
    }
    // every pair the .ksc answers that this round did not put keeps its answer
    {
        KscFile held = {};
        static KscRow s_row;
        ksc_read(&held, ksc);
        for (unsigned int at = 0u; at < held.lines; at += 1u)
        {
            if (!ksc_row_read(held.line[at], "run", &s_row))
            {
                continue;
            }
            std::stringstream words(s_row.question);
            std::string kind;
            std::string writer;
            std::string reader;
            words >> kind >> writer >> reader;
            if ((kind == "stall") && !reader.empty() && (came_back.count(std::make_pair(writer, reader)) == 0u))
            {
                rows.push_back(held.line[at]);
            }
        }
        ksc_free(&held);
    }
    std::sort(rows.begin(), rows.end(), [](const std::string &left, const std::string &right) {
        return left.substr(left.find(" stall ")) < right.substr(right.find(" stall "));
    });
    if (!run_closed(&table))
    {
        printf("klq_identity stall: %s/stall.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    run_channel_close();
    if (!ksc_answers_write(ksc, "stall", rows))
    {
        printf("klq_identity stall: %s could not be written\n", ksc);
        return 1;
    }
    printf("klq_identity stall: %zu pairs over %llu asks, written to %s\n", pairs.size(), asks, ksc);
    return 0;
}
