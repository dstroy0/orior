// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// slice.cu: the slice: nvcc's writing of the stick cut into windows, each an identity at the address it occurs at, and the chains written
#include "identity_internal.h"

// the link written with each register by its class and the order the window first names it in, r for R, p for P, ur
// for UR, each literal operand as a part of its own, k, in the order the window first writes its value, and each target
// of a control transfer as a part of its own, t, in the order the window first aims at it: two windows the same but for
// the registers they were given, the values they hold and where they sit write the same text
static std::string link_render(const Link &link, std::map<std::string, std::string> *names, unsigned int counts[5])
{
    std::string text;
    size_t mark = 0u;
    size_t name = 0u;
    size_t literal = 0u;
    size_t target = 0u;
    for (size_t next = link.marked.find_first_of("\x01\x02\x03", mark); next != std::string::npos;
         next = link.marked.find_first_of("\x01\x02\x03", mark))
    {
        text += link.marked.substr(mark, next - mark);
        const char part = link.marked[next];
        std::string key;
        unsigned int kind = 0u;
        if (part == '\x01')
        {
            key = link.named[name];
            kind = (key[0] == 'U') ? 2u : ((key[0] == 'P') ? 1u : 0u);
            name += 1u;
        }
        else if (part == '\x02')
        {
            key = "\x02" + link.literals[literal];
            kind = 3u;
            literal += 1u;
        }
        else
        {
            key = "\x03" + link.targets[target];
            kind = 4u;
            target += 1u;
        }
        auto found = names->find(key);
        if (found == names->end())
        {
            static const char *const s_classes[5] = {"r", "p", "ur", "k", "t"};
            found = names->insert(std::make_pair(key, s_classes[kind] + std::to_string(counts[kind]))).first;
            counts[kind] += 1u;
        }
        text += found->second;
        mark = next + 1u;
    }
    return text + link.marked.substr(mark);
}

// the window of `length` links from `first` of the question's chain, rendered link by link
std::string window_render(const Question &question, size_t first, size_t length)
{
    std::map<std::string, std::string> names;
    unsigned int counts[5] = {0u, 0u, 0u, 0u, 0u};
    std::string text;
    for (size_t at = first; at < first + length; at += 1u)
    {
        text += link_render(question.chain[at], &names, counts) + "\n";
    }
    return text;
}

// the identity of each window from `first` of the question's chain, one for each length, each the hash of the window
// rendered: the window and its every extension are read in one walk
std::vector<unsigned long long> window_identities(const Question &question, size_t first)
{
    std::vector<unsigned long long> identities;
    std::map<std::string, std::string> names;
    unsigned int counts[5] = {0u, 0u, 0u, 0u, 0u};
    unsigned long long hash = 14695981039346656037ull;
    for (size_t at = first; at < question.chain.size(); at += 1u)
    {
        const std::string text = link_render(question.chain[at], &names, counts) + "\n";
        for (const char letter : text)
        {
            hash = (hash ^ (unsigned char)letter) * 1099511628211ull;
        }
        identities.push_back(hash);
    }
    return identities;
}

std::string address_text(const std::vector<Question> &questions, unsigned int question, unsigned int link)
{
    return questions[question].number + ":" + questions[question].chain[link].position;
}

// The chains of every question, each window of each an identity held at its first occurrence, the lowest address it
// occurs at: every later occurrence reaffirms it at a new address, with the same constituents. A link whose identity
// first occurs where it stands is a primitive. Each question's chain is read from its first link as the constituents
// that came before it, at each link the longest window whose identity first occurs at a lower address, or the link
// alone where nothing does, and its signature is those constituents in order, each an identity at its address, with
// the primitive of each link and the literal operands in that order: identity:address with its primitives.
//
// The run's blob chains.txt holds the primitives in the order of their addresses, each with its count and its link,
// then for each question its chain: the identity of the whole at its address and its count, the constituents, each an
// address and a length, the primitives, and the operands every window of every question's chain, each identity at its
// first occurrence with its count
std::unordered_map<unsigned long long, Occurrence> windows_held(const std::vector<Question> &questions)
{
    std::unordered_map<unsigned long long, Occurrence> held;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        for (unsigned int link = 0u; link < questions[question].chain.size(); link += 1u)
        {
            for (const unsigned long long identity : window_identities(questions[question], link))
            {
                const auto found = held.insert(std::make_pair(identity, Occurrence{question, link, 0u})).first;
                found->second.count += 1u;
            }
        }
    }
    return held;
}

static int chains_write(const std::vector<Question> &questions, const std::string &folder)
{
    const std::unordered_map<unsigned long long, Occurrence> held = windows_held(questions);
    const std::string path = folder + "/chains.txt";
    RunWritten file = {path, 0, std::string()};
    std::string primitives;
    std::string chains;
    unsigned int primitive_count = 0u;
    unsigned int constituent_count = 0u;
    unsigned int reaffirmed = 0u;
    std::set<unsigned long long> constituents;
    std::set<unsigned long long> wholes;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        const Question &asked = questions[question];
        if (asked.chain.empty())
        {
            continue;
        }
        const unsigned long long whole_identity = window_identities(asked, 0u).back();
        const Occurrence &whole = held.at(whole_identity);
        wholes.insert(whole_identity);
        reaffirmed += ((whole.question != question) || (whole.link != 0u)) ? 1u : 0u;
        std::string read;
        std::string primitive_row;
        std::string operand_row;
        for (unsigned int link = 0u; link < asked.chain.size();)
        {
            const std::vector<unsigned long long> identities = window_identities(asked, link);
            unsigned int length = 0u;
            for (unsigned int longest = (unsigned int)identities.size(); (longest > 0u) && (length == 0u); longest -= 1u)
            {
                const Occurrence &first = held.at(identities[longest - 1u]);
                const int lower = (first.question < question) || ((first.question == question) && (first.link < link));
                // the constituents of a reaffirmation are the same, link for link, as those of its first occurrence
                if (lower && (window_render(questions[first.question], first.link, longest) ==
                              window_render(asked, link, longest)))
                {
                    read += " " + address_text(questions, first.question, first.link) + "/" + std::to_string(longest);
                    constituents.insert(identities[longest - 1u]);
                    length = longest;
                }
            }
            if (length == 0u)
            {
                primitives += "primitive " + address_text(questions, question, link) + " " +
                              std::to_string(held.at(identities[0]).count) + " " + window_render(asked, link, 1u);
                read += " " + address_text(questions, question, link) + "/1";
                primitive_count += 1u;
                length = 1u;
            }
            constituent_count += 1u;
            for (unsigned int within = link; within < link + length; within += 1u)
            {
                const Occurrence &primitive = held.at(window_identities(asked, within)[0]);
                primitive_row += " " + address_text(questions, primitive.question, primitive.link);
                // the literal operands and the distances of the targets, in the order the link writes them
                const Link &written = asked.chain[within];
                size_t literal = 0u;
                size_t target = 0u;
                for (const char part : written.marked)
                {
                    if (part == '\x02')
                    {
                        operand_row += " " + written.literals[literal];
                        literal += 1u;
                    }
                    else if (part == '\x03')
                    {
                        operand_row += " " + written.distances[target];
                        target += 1u;
                    }
                }
            }
            link += length;
        }
        // the number of the whole is the hash of its contents in their order, the same wherever and in whichever stick it
        // is read
        char number[17];
        snprintf(number, sizeof(number), "%016llx", whole_identity);
        chains += "chain " + address_text(questions, question, 0u) + " " +
                  address_text(questions, whole.question, whole.link) + " " + std::to_string(whole.count) + " " + number +
                  "\n";
        chains += "constituents" + read + "\nprimitives" + primitive_row + "\noperands" + operand_row + "\n";
    }
    run_print(&file, "%s%s", primitives.c_str(), chains.c_str());
    if (!run_closed(&file))
    {
        printf("  %s could not be written\n", path.c_str());
        return 0;
    }
    printf("  %s: %u chains, %u whole identities, %u chains reaffirming a lower address whole, %u primitives, %u "
           "constituents read, %u identities among them\n",
           path.c_str(), (unsigned int)questions.size(), (unsigned int)wholes.size(), reaffirmed, primitive_count,
           constituent_count, (unsigned int)constituents.size());
    return 1;
}

// 1 where questions `one` and `other` are a slice: their listings the same but for one link a side, the two links
// naming the same registers and read as different kinds, given through `left` and `right`
int slice_found(const Question &one, const Question &other, const Link **left, const Link **right)
{
    const std::vector<std::string> first = links_past(one, other);
    const std::vector<std::string> second = links_past(other, one);
    if ((first.size() != 1u) || (second.size() != 1u))
    {
        return 0;
    }
    *left = &one.links.at(first[0]);
    *right = &other.links.at(second[0]);
    return ((*left)->registers == (*right)->registers) && ((*left)->kind != (*right)->kind);
}

int identity_slice(const char *listing, const char *manifest, const char *stick_path, const std::string &folder,
                          const std::string &ours)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    std::ifstream stick_file(stick_path);
    std::stringstream stick;
    stick << stick_file.rdbuf();
    const std::string path = folder + "/slices.txt";
    RunWritten file = {path, 0, std::string()};
    std::set<std::string> asked;
    unsigned int slices = 0u;
    std::set<std::string> identities;
    for (size_t one = 0u; one < questions.size(); one += 1u)
    {
        for (size_t other = one + 1u; other < questions.size(); other += 1u)
        {
            const Link *found_left = nullptr;
            const Link *found_right = nullptr;
            if (!slice_found(questions[one], questions[other], &found_left, &found_right))
            {
                continue;
            }
            const Link &left = *found_left;
            const Link &right = *found_right;
            // each slice a line: its two members, each a question and the position of its link in that question's chain,
            // then the identity, the two links in order, the first member writing the first, then each member's context
            const int swapped = right.kind < left.kind;
            const Question &lead = swapped ? questions[other] : questions[one];
            const Question &rite = swapped ? questions[one] : questions[other];
            const Link &lead_link = swapped ? right : left;
            const Link &rite_link = swapped ? left : right;
            const std::string identity = lead_link.kind + "\t" + rite_link.kind;
            const std::string lead_context = member_context(lead, lead_link.position);
            const std::string rite_context = member_context(rite, rite_link.position);
            run_print(&file, "%s:%s\t%s:%s\t%s\t%s\t%s\n", lead.number.c_str(), lead_link.position.c_str(),
                      rite.number.c_str(), rite_link.position.c_str(), identity.c_str(), lead_context.c_str(),
                      rite_context.c_str());
            identities.insert(identity);
            slices += 1u;
            if (question_integer(lead) && question_integer(rite))
            {
                asked.insert(lead.number);
                asked.insert(rite.number);
            }
        }
    }
    if (!run_closed(&file))
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    // the host computes each integer question the run channel puts to the part, a slice's member or not: a task of
    // register pressure, which slices with nothing, and every question the engine writes a chain for in `ours`
    for (const Question &question : questions)
    {
        const int written = !ours.empty() && std::filesystem::exists(ours + "/" + question.number + ".bin");
        if (question_integer(question) && ((question.category == "pressure") || (written != 0)))
        {
            asked.insert(question.number);
        }
    }
    printf("  %s: %u questions, %u slices, %u identities\n", path.c_str(), (unsigned int)questions.size(), slices,
           (unsigned int)identities.size());
    if (!chains_write(questions, folder))
    {
        return 1;
    }
    return host_questions_write(folder + "/host_questions.cpp", stick.str(), asked) ? 0 : 1;
}
