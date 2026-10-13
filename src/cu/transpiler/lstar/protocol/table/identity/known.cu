// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// known.cu: the known and the permute: the engine's writing of the stick sifted through what nvcc writes, and nvcc's chains put together by their categories
#include "identity_internal.h"

// Each candidate chain sifted through the identities nvcc's chains hold, before anything of it is put to the part:
// its whole known at an address, or read as the longest known windows from each link, with every link whose primitive
// nvcc never writes and every two links side by side nvcc never writes so, each known apart. A part unknown that rule 2
// keeps off the part is marked with the rule. The run's blob known.txt holds a candidate a group of lines
int identity_known(const char *listing, const char *manifest, const std::string &folder,
                          const std::vector<std::string> &paths)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    const std::unordered_map<unsigned long long, Occurrence> held = windows_held(questions);
    std::vector<Question> candidates;
    for (const std::string &path : paths)
    {
        candidates_read(path, &candidates);
    }
    const std::string path = folder + "/known.txt";
    RunWritten file = {path, 0, std::string()};
    // the first occurrence of a window where nvcc's chains hold it, its constituents the same link for link
    const auto known = [&](const Question &candidate, unsigned int link, unsigned int length,
                           unsigned long long identity) -> const Occurrence *
    {
        const auto found = held.find(identity);
        if ((found == held.end()) || (window_render(questions[found->second.question], found->second.link, length) !=
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
        snprintf(number, sizeof(number), "%016llx", from_first.back());
        run_print(&file, "candidate %s %s %s\n", candidate.number.c_str(), number,
                  (whole == nullptr) ? "unknown"
                                     : ("known " + address_text(questions, whole->question, whole->link)).c_str());
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
            for (unsigned int longest = (unsigned int)identities.size(); (longest > 0u) && (length == 0u); longest -= 1u)
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
        run_print(&file, "constituents%s\n", read.c_str());
        unsigned int unknown_here = 0u;
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            const std::vector<unsigned long long> identities = window_identities(candidate, link);
            const int alone = known(candidate, link, 1u, identities[0]) != nullptr;
            const int paired = ((link + 1u) < links) ? (known(candidate, link, 2u, identities[1]) != nullptr) : 1;
            const int next_alone =
                ((link + 1u) < links) ? (known(candidate, link + 1u, 1u, window_identities(candidate, link + 1u)[0]) != nullptr) : 0;
            const Link &written = candidate.chain[link];
            const std::string rule = rule_broken(written.exact) ? " rule 2: its form transfers control or waits" : "";
            if (!alone)
            {
                run_print(&file, "unknown %s %s%s\n", written.position.c_str(),
                          window_render(candidate, link, 1u).c_str(), rule.c_str());
                unknown_primitives += 1u;
                unknown_here += 1u;
                flagged += rule.empty() ? 0u : 1u;
            }
            // two links each known alone and never written side by side
            if (alone && next_alone && !paired)
            {
                std::string pair = window_render(candidate, link, 2u);
                std::replace(pair.begin(), pair.end(), '\n', ';');
                run_print(&file, "unpaired %s %s\n", written.position.c_str(), pair.c_str());
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
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    printf("  %s: %u candidates, %u known whole, %u not known whole but every link and every two side by side known, "
           "%u with a part nvcc never writes\n",
           path.c_str(), (unsigned int)candidates.size(), wholes, permutations, strangers);
    printf("  parts unknown: %u links, %u of them kept off the part by rule 2, %u pairs side by side\n",
           unknown_primitives, flagged, unknown_pairs);
    return 0;
}

// the root of `kind` among the categories, each kind joined to the kinds it was found in a slice with
static std::string category_root(std::map<std::string, std::string> *parent, const std::string &kind)
{
    std::string root = kind;
    while ((*parent)[root] != root)
    {
        root = (*parent)[root];
    }
    for (std::string walk = kind; walk != root;)
    {
        const std::string next = (*parent)[walk];
        (*parent)[walk] = root;
        walk = next;
    }
    return root;
}

// The parts of nvcc's chains put together by their categories, no part's meaning read: two links a slice sets apart
// stand for each other, and each link joined to another so, at any remove, is of its category. Each link of each chain
// is stood for by every other of its category naming as many registers, the registers of the link it stands for given
// to it, and asked in the window of the link before it and the link after: a window nvcc writes somewhere is known, and
// one it never writes is a question, held at the first address it arises at with its count. A link put in that rule 2
// keeps off the part is marked with the rule. The run's blob permutations.txt holds the categories, each with its
// members at the first address each occurs at, then the questions in the order of their addresses
int identity_permute(const char *listing, const char *manifest, const std::string &folder)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    const std::unordered_map<unsigned long long, Occurrence> held = windows_held(questions);
    // the first occurrence of every kind of link, and each kind its own category before any slice joins it
    std::map<std::string, std::pair<unsigned int, unsigned int>> first_kind;
    std::map<std::string, std::string> parent;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        for (unsigned int link = 0u; link < questions[question].chain.size(); link += 1u)
        {
            const std::string &kind = questions[question].chain[link].kind;
            first_kind.insert(std::make_pair(kind, std::make_pair(question, link)));
            parent.insert(std::make_pair(kind, kind));
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
                const std::string left_root = category_root(&parent, left->kind);
                const std::string right_root = category_root(&parent, right->kind);
                parent[left_root] = right_root;
            }
        }
    }
    // each category's members in the order of their first occurrence
    std::vector<std::pair<std::pair<unsigned int, unsigned int>, std::string>> ordered;
    for (const auto &kind : first_kind)
    {
        ordered.push_back(std::make_pair(kind.second, kind.first));
    }
    std::sort(ordered.begin(), ordered.end());
    std::map<std::string, std::vector<std::string>> categories;
    std::vector<std::string> roots;
    for (const auto &kind : ordered)
    {
        const std::string root = category_root(&parent, kind.second);
        if (categories[root].empty())
        {
            roots.push_back(root);
        }
        categories[root].push_back(kind.second);
    }
    std::unordered_map<unsigned long long, Permuted> asked;
    std::vector<unsigned long long> asked_order;
    unsigned long long known = 0ull;
    unsigned long long put = 0ull;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        const std::vector<Link> &chain = questions[question].chain;
        for (unsigned int link = 0u; link < chain.size(); link += 1u)
        {
            const std::vector<std::string> &members = categories[category_root(&parent, chain[link].kind)];
            for (const std::string &member : members)
            {
                const std::pair<unsigned int, unsigned int> &at = first_kind.at(member);
                const Link &source = questions[at.first].chain[at.second];
                if ((member == chain[link].kind) || (source.named.size() != chain[link].named.size()))
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
                put += 1ull;
                const auto found = held.find(identity);
                if ((found != held.end()) &&
                    (window_render(questions[found->second.question], found->second.link, length) == rendered))
                {
                    known += 1ull;
                    continue;
                }
                const auto inserted = asked.insert(std::make_pair(
                    identity, Permuted{question, link, 0u, rendered, chain[link].kind, member}));
                if (inserted.second)
                {
                    asked_order.push_back(identity);
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
        const std::pair<unsigned int, unsigned int> &first = first_kind.at(members[0]);
        run_print(&file, "category %s %u\n", address_text(questions, first.first, first.second).c_str(),
                  (unsigned int)members.size());
        for (const std::string &member : members)
        {
            const std::pair<unsigned int, unsigned int> &at = first_kind.at(member);
            run_print(&file, "member %s %s\n", address_text(questions, at.first, at.second).c_str(), member.c_str());
        }
    }
    unsigned int flagged = 0u;
    for (const unsigned long long identity : asked_order)
    {
        const Permuted &question = asked.at(identity);
        std::string window = question.window;
        window.pop_back();
        std::replace(window.begin(), window.end(), '\n', ';');
        run_print(&file, "question %s %u\nfrom %s\nto %s\nwindow %s\n",
                  address_text(questions, question.question, question.link).c_str(), question.count,
                  question.from.c_str(), question.to.c_str(), window.c_str());
        if (rule_broken(question.to))
        {
            run_print(&file, "rule 2 %s: its form transfers control or waits\n", question.to.c_str());
            flagged += 1u;
        }
    }
    if (!run_closed(&file))
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    printf("  %s: %u kinds of link in %u categories of more than one, %llu windows put, %llu of them nvcc writes, "
           "%u questions, %u of them kept off the part by rule 2\n",
           path.c_str(), (unsigned int)first_kind.size(), joined, put, known, (unsigned int)asked_order.size(), flagged);
    return 0;
}
