// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// known.cu: the known and the permute: the engine's writing of the stick sifted through what nvcc writes, and nvcc's
// chains put together by their categories
#include "identity_internal.h"
#include "../../../../../../platform/platform.h"

// Each candidate chain sifted through the identities nvcc's chains contain, before anything of it is put to the part:
// its whole known at an address, or read as the longest known windows from each link, with every link whose primitive
// nvcc never writes and every two links side by side nvcc never writes so, each known apart. A part unknown that rule 2
// blocks from the part is marked with the rule. The run's blob known.txt contains a candidate a group of lines
int identity_known(const char *listing, const char *manifest, const std::string &folder,
                   const std::vector<std::string> &paths)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    const std::unordered_map<unsigned long long, Occurrence> windows = windows_counted(questions);
    std::vector<Question> candidates;
    for (const std::string &path : paths)
    {
        candidates_read(path, &candidates);
    }
    const std::string path = folder + "/known.txt";
    RunWritten file = {path, 0, std::string()};
    // the first occurrence of a window where nvcc's chains contain it, its constituents the same link for link
    const auto known = [&](const Question &candidate, unsigned int link, unsigned int length,
                           unsigned long long identity) -> const Occurrence * {
        const auto found = windows.find(identity);
        if ((found == windows.end()) || (window_render(questions[found->second.question], found->second.link, length) !=
                                         window_render(candidate, link, length)))
        {
            return nullptr;
        }
        return &found->second;
    };
    unsigned int wholes = 0u;
    unsigned int permutations = 0u;
    unsigned int strangers = 0u;
    unsigned int unknown_primitives = 0u;
    unsigned int unknown_pairs = 0u;
    unsigned int flagged = 0u;
    for (const Question &candidate : candidates)
    {
        if (candidate.chain.empty())
        {
            continue;
        }
        const unsigned int links = (unsigned int)candidate.chain.size();
        const std::vector<unsigned long long> from_first = window_identities(candidate, 0u);
        const Occurrence *const whole = known(candidate, 0u, links, from_first.back());
        char number[17];
        {
            VerbaLinea buffer_line = mmgr_line_over(number, sizeof(number));
            mmgr_line_hex(&buffer_line, from_first.back(), 16u);
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "candidate ");
            mmgr_line_put(&text_note.line, candidate.number.c_str());
            mmgr_line_put(&text_note.line, " ");
            mmgr_line_put(&text_note.line, number);
            mmgr_line_put(&text_note.line, " ");
            mmgr_line_put(&text_note.line,
                          (whole == nullptr)
                              ? "unknown"
                              : ("known " + address_text(questions, whole->question, whole->link)).c_str());
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        if (whole != nullptr)
        {
            wholes += 1u;
            continue;
        }
        std::string read;
        for (unsigned int link = 0u; link < links;)
        {
            const std::vector<unsigned long long> identities = window_identities(candidate, link);
            unsigned int length = 0u;
            for (unsigned int longest = (unsigned int)identities.size(); (longest > 0u) && (length == 0u);
                 longest -= 1u)
            {
                const Occurrence *const first = known(candidate, link, longest, identities[longest - 1u]);
                if (first != nullptr)
                {
                    read += " " + address_text(questions, first->question, first->link) + "/" + std::to_string(longest);
                    length = longest;
                }
            }
            if (length == 0u)
            {
                read += " ?" + candidate.chain[link].position;
                length = 1u;
            }
            link += length;
        }
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "constituents");
            mmgr_line_put(&text_note.line, read.c_str());
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        unsigned int unknown_here = 0u;
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            const std::vector<unsigned long long> identities = window_identities(candidate, link);
            const int alone = known(candidate, link, 1u, identities[0]) != nullptr;
            const int paired = ((link + 1u) < links) ? (known(candidate, link, 2u, identities[1]) != nullptr) : 1;
            const int next_alone =
                ((link + 1u) < links)
                    ? (known(candidate, link + 1u, 1u, window_identities(candidate, link + 1u)[0]) != nullptr)
                    : 0;
            const Link &written = candidate.chain[link];
            const std::string rule = rule_broken(written.exact) ? " rule 2: its form transfers control or waits" : "";
            if (!alone)
            {
                {
                    TextNote text_note;
                    text_note_begun(&text_note, &(&file)->text);
                    mmgr_line_put(&text_note.line, "unknown ");
                    mmgr_line_put(&text_note.line, written.position.c_str());
                    mmgr_line_put(&text_note.line, " ");
                    mmgr_line_put(&text_note.line, window_render(candidate, link, 1u).c_str());
                    mmgr_line_put(&text_note.line, rule.c_str());
                    mmgr_line_ch(&text_note.line, '\n');
                    text_note_written(&text_note);
                }
                unknown_primitives += 1u;
                unknown_here += 1u;
                flagged += rule.empty() ? 0u : 1u;
            }
            // two links each known alone and never written side by side
            if (alone && next_alone && !paired)
            {
                std::string pair = window_render(candidate, link, 2u);
                std::replace(pair.begin(), pair.end(), '\n', ';');
                {
                    TextNote text_note;
                    text_note_begun(&text_note, &(&file)->text);
                    mmgr_line_put(&text_note.line, "unpaired ");
                    mmgr_line_put(&text_note.line, written.position.c_str());
                    mmgr_line_put(&text_note.line, " ");
                    mmgr_line_put(&text_note.line, pair.c_str());
                    mmgr_line_ch(&text_note.line, '\n');
                    text_note_written(&text_note);
                }
                unknown_pairs += 1u;
                unknown_here += 1u;
            }
        }
        if (unknown_here == 0u)
        {
            permutations += 1u;
        }
        else
        {
            strangers += 1u;
        }
    }
    if (!run_closed(&file))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, path.c_str());
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, path.c_str());
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned int)candidates.size(), 1u);
        mmgr_line_put(output_line, " candidates, ");
        mmgr_line_decimal(output_line, wholes, 1u);
        mmgr_line_put(output_line, " known whole, ");
        mmgr_line_decimal(output_line, permutations, 1u);
        mmgr_line_put(output_line, " not known whole but every link and every two side by side known, ");
        mmgr_line_decimal(output_line, strangers, 1u);
        mmgr_line_put(output_line, " with a part nvcc never writes");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  parts unknown: ");
        mmgr_line_decimal(output_line, unknown_primitives, 1u);
        mmgr_line_put(output_line, " links, ");
        mmgr_line_decimal(output_line, flagged, 1u);
        mmgr_line_put(output_line, " of them blocked from the part by rule 2, ");
        mmgr_line_decimal(output_line, unknown_pairs, 1u);
        mmgr_line_put(output_line, " pairs side by side");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}

// the root of `type` among the categories, each type joined to the types it was found in a slice with
static std::string category_root(std::map<std::string, std::string> *parent, const std::string &type)
{
    std::string root = type;
    while ((*parent)[root] != root)
    {
        root = (*parent)[root];
    }
    for (std::string walk = type; walk != root;)
    {
        const std::string next = (*parent)[walk];
        (*parent)[walk] = root;
        walk = next;
    }
    return root;
}

// The parts of nvcc's chains put together by their categories, no part's meaning read: two links a slice sets apart
// are for each other, and each link joined to another so, at any remove, is of its category. Each link of each chain
// is was for by every other of its category naming as many registers, the registers of the link it is for given
// to it, and queried in the window of the link before it and the link after: a window nvcc writes somewhere is known,
// and one it never writes is a question, recorded at the first address it arises at with its count. A link put in that
// rule 2 blocks from the part is marked with the rule. The run's blob permutations.txt contains the categories, each
// with its members at the first address each occurs at, then the questions in the order of their addresses
int identity_permute(const char *listing, const char *manifest, const std::string &folder)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    const std::unordered_map<unsigned long long, Occurrence> windows = windows_counted(questions);
    // the first occurrence of every type of link, and each type its own category before any slice joins it
    std::map<std::string, std::pair<unsigned int, unsigned int>> first_type;
    std::map<std::string, std::string> parent;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        for (unsigned int link = 0u; link < questions[question].chain.size(); link += 1u)
        {
            const std::string &type = questions[question].chain[link].type;
            first_type.insert(std::make_pair(type, std::make_pair(question, link)));
            parent.insert(std::make_pair(type, type));
        }
    }
    for (size_t one = 0u; one < questions.size(); one += 1u)
    {
        for (size_t other = one + 1u; other < questions.size(); other += 1u)
        {
            const Link *left = nullptr;
            const Link *right = nullptr;
            if (slice_found(questions[one], questions[other], &left, &right))
            {
                const std::string left_root = category_root(&parent, left->type);
                const std::string right_root = category_root(&parent, right->type);
                parent[left_root] = right_root;
            }
        }
    }
    // each category's members in the order of their first occurrence
    std::vector<std::pair<std::pair<unsigned int, unsigned int>, std::string>> ordered;
    for (const auto &type : first_type)
    {
        ordered.push_back(std::make_pair(type.second, type.first));
    }
    std::sort(ordered.begin(), ordered.end());
    std::map<std::string, std::vector<std::string>> categories;
    std::vector<std::string> roots;
    for (const auto &type : ordered)
    {
        const std::string root = category_root(&parent, type.second);
        if (categories[root].empty())
        {
            roots.push_back(root);
        }
        categories[root].push_back(type.second);
    }
    std::unordered_map<unsigned long long, Permuted> permuted;
    std::vector<unsigned long long> permuted_order;
    unsigned long long known = 0ULL;
    unsigned long long put = 0ULL;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        const std::vector<Link> &chain = questions[question].chain;
        for (unsigned int link = 0u; link < chain.size(); link += 1u)
        {
            const std::vector<std::string> &members = categories[category_root(&parent, chain[link].type)];
            for (const std::string &member : members)
            {
                const std::pair<unsigned int, unsigned int> &at = first_type.at(member);
                const Link &source = questions[at.first].chain[at.second];
                if ((member == chain[link].type) || (source.named.size() != chain[link].named.size()))
                {
                    continue;
                }
                Link substitute = source;
                substitute.named = chain[link].named;
                substitute.position = chain[link].position;
                Question window;
                window.number = questions[question].number;
                for (unsigned int within = (link == 0u) ? 0u : link - 1u;
                     within < std::min((unsigned int)chain.size(), link + 2u); within += 1u)
                {
                    window.chain.push_back((within == link) ? substitute : chain[within]);
                }
                const unsigned int length = (unsigned int)window.chain.size();
                const unsigned long long identity = window_identities(window, 0u).back();
                const std::string rendered = window_render(window, 0u, length);
                put += 1ULL;
                const auto found = windows.find(identity);
                if ((found != windows.end()) &&
                    (window_render(questions[found->second.question], found->second.link, length) == rendered))
                {
                    known += 1ULL;
                    continue;
                }
                const auto inserted = permuted.insert(
                    std::make_pair(identity, Permuted{question, link, 0u, rendered, chain[link].type, member}));
                if (inserted.second)
                {
                    permuted_order.push_back(identity);
                }
                inserted.first->second.count += 1u;
            }
        }
    }
    const std::string path = folder + "/permutations.txt";
    RunWritten file = {path, 0, std::string()};
    unsigned int joined = 0u;
    for (const std::string &root : roots)
    {
        const std::vector<std::string> &members = categories[root];
        if (members.size() < 2u)
        {
            continue;
        }
        joined += 1u;
        const std::pair<unsigned int, unsigned int> &first = first_type.at(members[0]);
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "category ");
            mmgr_line_put(&text_note.line, address_text(questions, first.first, first.second).c_str());
            mmgr_line_put(&text_note.line, " ");
            mmgr_line_decimal(&text_note.line, (unsigned int)members.size(), 1u);
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        for (const std::string &member : members)
        {
            const std::pair<unsigned int, unsigned int> &at = first_type.at(member);
            {
                TextNote text_note;
                text_note_begun(&text_note, &(&file)->text);
                mmgr_line_put(&text_note.line, "member ");
                mmgr_line_put(&text_note.line, address_text(questions, at.first, at.second).c_str());
                mmgr_line_put(&text_note.line, " ");
                mmgr_line_put(&text_note.line, member.c_str());
                mmgr_line_ch(&text_note.line, '\n');
                text_note_written(&text_note);
            }
        }
    }
    unsigned int flagged = 0u;
    for (const unsigned long long identity : permuted_order)
    {
        const Permuted &question = permuted.at(identity);
        std::string window = question.window;
        window.pop_back();
        std::replace(window.begin(), window.end(), '\n', ';');
        {
            TextNote text_note;
            text_note_begun(&text_note, &(&file)->text);
            mmgr_line_put(&text_note.line, "question ");
            mmgr_line_put(&text_note.line, address_text(questions, question.question, question.link).c_str());
            mmgr_line_put(&text_note.line, " ");
            mmgr_line_decimal(&text_note.line, question.count, 1u);
            mmgr_line_put(&text_note.line, "\nfrom ");
            mmgr_line_put(&text_note.line, question.from.c_str());
            mmgr_line_put(&text_note.line, "\nto ");
            mmgr_line_put(&text_note.line, question.to.c_str());
            mmgr_line_put(&text_note.line, "\nwindow ");
            mmgr_line_put(&text_note.line, window.c_str());
            mmgr_line_ch(&text_note.line, '\n');
            text_note_written(&text_note);
        }
        if (rule_broken(question.to))
        {
            {
                TextNote text_note;
                text_note_begun(&text_note, &(&file)->text);
                mmgr_line_put(&text_note.line, "rule 2 ");
                mmgr_line_put(&text_note.line, question.to.c_str());
                mmgr_line_put(&text_note.line, ": its form transfers control or waits");
                mmgr_line_ch(&text_note.line, '\n');
                text_note_written(&text_note);
            }
            flagged += 1u;
        }
    }
    if (!run_closed(&file))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, path.c_str());
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, path.c_str());
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, (unsigned int)first_type.size(), 1u);
        mmgr_line_put(output_line, " types of link in ");
        mmgr_line_decimal(output_line, joined, 1u);
        mmgr_line_put(output_line, " categories of more than one, ");
        mmgr_line_decimal(output_line, put, 1u);
        mmgr_line_put(output_line, " windows put, ");
        mmgr_line_decimal(output_line, known, 1u);
        mmgr_line_put(output_line, " of them nvcc writes, ");
        mmgr_line_decimal(output_line, (unsigned int)permuted_order.size(), 1u);
        mmgr_line_put(output_line, " questions, ");
        mmgr_line_decimal(output_line, flagged, 1u);
        mmgr_line_put(output_line, " of them blocked from the part by rule 2");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
