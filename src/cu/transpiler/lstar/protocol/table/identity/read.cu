// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.cu: the read: the forms of the rulesets, the members and carriers of the stick, and the candidate paths read against them
#include "identity_internal.h"

// a form's text as the links it writes, each parameter a pattern any operand matches
static std::vector<std::regex> form_links(const std::string &text)
{
    std::vector<std::regex> links;
    std::string written;
    for (size_t at = 0u; at < text.size(); at += 1u)
    {
        if ((text[at] == '\\') && ((at + 1u) < text.size()))
        {
            written += (text[at + 1u] == 'n') ? '\n' : ((text[at + 1u] == 't') ? '\t' : text[at + 1u]);
            at += 1u;
            continue;
        }
        written += text[at];
    }
    std::stringstream lines(written);
    std::string line;
    while (std::getline(lines, line))
    {
        Link link;
        if (link_read(line, &link) != 1)
        {
            continue;
        }
        std::string pattern;
        static const std::regex s_special("[.^$|()\\[\\]*+?\\\\]");
        const std::string escaped = std::regex_replace(link.kind, s_special, "\\$&");
        // an offset added to an address is not written where it is 0: the parameter after a + is there or not
        static const std::regex s_offset("\\\\\\+\\{[a-z_]+\\}");
        static const std::regex s_parameter("\\{[a-z_]+\\}(\\\\\\.hi)?");
        pattern = std::regex_replace(std::regex_replace(escaped, s_offset, "(\\+[^,\\]]+)?"), s_parameter, "[^,]+");
        links.push_back(std::regex("^" + pattern + "$"));
    }
    return links;
}

// the forms of each ruleset given, each name with the links its text writes; 0 where a ruleset could not be read
int forms_read(int count, char **files, std::vector<std::pair<std::string, std::vector<std::regex>>> *forms)
{
    for (int given = 0; given < count; given += 1)
    {
        HeldSet set;
        const RulesetSchema *const schema = code_generator(files[given]).schema();
        std::string paths[RULESET_PATHS_MAX];
        const unsigned int paths_count = ruleset_paths(schema, ruleset_folder() + "/" + schema->file, paths);
        RulesetFlat flat;
        ruleset_flat_schema(schema, &flat);
        std::string error;
        if (ruleset_relations_read(paths, paths_count, &flat.schema, &set.memory, &set.relations, &error) == 0)
        {
            printf("  %s could not be opened\n", error.c_str());
            return 0;
        }
        for (unsigned int at = 0u; at < set.relations.entry_count; at += 1u)
        {
            const RulesetCoreEntry *const entry = &set.relations.entries[at];
            if (entry->kind != RULESET_CORE_ENTRY_FORM)
            {
                continue;
            }
            const std::string name((const char *)&set.relations.text[entry->name.first], entry->name.length);
            const std::string text((const char *)&set.relations.text[entry->text.first], entry->text.length);
            forms->push_back(std::make_pair(name, form_links(text)));
        }
        // a pipe row chooses between an operation and its writing on the other pipe: a link of either was placed by
        // the row's choice, whichever way it went
        for (unsigned int at = 0u; at < paths_count; at += 1u)
        {
            // each file as the run holds it (ruleset_bytes_read)
            std::string held;
            ruleset_bytes_read(paths[at], &held);
            std::stringstream file(held);
            std::string line;
            while (std::getline(file, line))
            {
                std::stringstream words(line);
                std::string head;
                std::string operation;
                std::string writing;
                if ((words >> head >> operation >> writing) && (head == "pipe"))
                {
                    static const std::regex s_special("[.^$|()\\[\\]*+?\\\\]");
                    const std::string either = std::regex_replace(operation, s_special, "\\$&") + "|" +
                                               std::regex_replace(writing, s_special, "\\$&");
                    forms->push_back(std::make_pair("pipe:" + operation + ":" + writing,
                                                    std::vector<std::regex>{std::regex("^(@!?P )?(" + either + ")( .*)?$")}));
                }
            }
        }
    }
    return 1;
}

// the names of the forms whose texts write `kind`, joined by spaces, or - where none does
std::string forms_writing(const std::vector<std::pair<std::string, std::vector<std::regex>>> &forms,
                                 const std::string &kind)
{
    std::string names;
    for (const auto &form : forms)
    {
        if (std::any_of(form.second.begin(), form.second.end(),
                        [&](const std::regex &one) { return std::regex_match(kind, one); }))
        {
            names += (names.empty() ? "" : " ") + form.first;
        }
    }
    return names.empty() ? std::string("-") : names;
}

// the entries of a context, split
static std::set<std::string> context_entries(const std::string &context)
{
    std::set<std::string> entries;
    std::stringstream split(context);
    std::string entry;
    while (std::getline(split, entry, ';'))
    {
        entries.insert(entry);
    }
    return entries;
}

// a verdict over the carriers given, written after them: the first case one carrier's two questions answer apart, or
// the count of cases alike. 1 where it closed, 0 where it stayed open over cases asked, -1 where nothing was asked
static int carriers_verdict(RunWritten *file, const std::vector<const Carrier *> &carriers,
                            const std::map<std::string, std::vector<std::string>> &answers,
                            std::string (*text)(const Carrier &))
{
    unsigned int alike = 0u;
    for (const Carrier *const slice : carriers)
    {
        const auto lead = answers.find(slice->first.question);
        const auto rite = answers.find(slice->second.question);
        if ((lead == answers.end()) || (rite == answers.end()))
        {
            continue;
        }
        for (unsigned int at = 0u; at < IDENTITY_CASES; at += 1u)
        {
            const std::string &one = lead->second[at];
            const std::string &other = rite->second[at];
            if ((one == "-") || (other == "-"))
            {
                continue;
            }
            if (one != other)
            {
                const unsigned int third = at / (IDENTITY_VALUES * IDENTITY_VALUES);
                const unsigned int left = (at / IDENTITY_VALUES) % IDENTITY_VALUES;
                const unsigned int right = at % IDENTITY_VALUES;
                run_print(file, "closed %s %llx,%llx,%x->%s,%s\n", text(*slice).c_str(), s_values[left],
                          s_values[right], third, one.c_str(), other.c_str());
                return 1;
            }
            alike += 1u;
        }
    }
    run_print(file, "open %u\n", alike);
    return (alike != 0u) ? 0 : -1;
}

static Member member_read(const std::string &text)
{
    const size_t colon = text.find(':');
    return Member{text.substr(0u, colon), text.substr(colon + 1u)};
}

// 1 where `one` sits before `other` in the chain: by question, then by position in it
static int member_before(const Member &one, const Member &other)
{
    if (one.question != other.question)
    {
        return std::stoul(one.question) < std::stoul(other.question);
    }
    return std::stoull(one.position, nullptr, 16) < std::stoull(other.position, nullptr, 16);
}

// 1 where `one` sits before `other`: contention defers to the most basal member of each, then to the other member
static bool carrier_before(const Carrier &one, const Carrier &other)
{
    const int one_swapped = member_before(one.second, one.first);
    const int other_swapped = member_before(other.second, other.first);
    const Member &one_basal = one_swapped ? one.second : one.first;
    const Member &other_basal = other_swapped ? other.second : other.first;
    if (member_before(one_basal, other_basal) || member_before(other_basal, one_basal))
    {
        return member_before(one_basal, other_basal);
    }
    return member_before(one_swapped ? one.first : one.second, other_swapped ? other.first : other.second);
}

static std::string carrier_text(const Carrier &carrier)
{
    return carrier.first.question + ":" + carrier.first.position + " " + carrier.second.question + ":" +
           carrier.second.position;
}

int identity_read(const std::string &folder, const std::string &klq, int count, char **files)
{
    std::map<std::string, std::vector<std::string>> answers;
    std::istringstream answered(run_blob_text(IDENTITY_HOST_ANSWERS));
    std::string line;
    while (std::getline(answered, line))
    {
        std::stringstream words(line);
        std::string number;
        words >> number;
        std::string word;
        while (words >> word)
        {
            answers[number].push_back(word);
        }
    }
    // members the same make the identity the same, and the carriers of one identity are told apart by address: the
    // position of each in the chain, the most basal member first
    std::map<std::string, std::vector<Carrier>> carried;
    std::istringstream sliced(run_blob_text("slices.txt"));
    while (std::getline(sliced, line))
    {
        std::vector<std::string> cells;
        std::stringstream split(line);
        std::string cell;
        while (std::getline(split, cell, '\t'))
        {
            cells.push_back(cell);
        }
        cells.resize(6u);
        carried[cells[2] + "\t" + cells[3]].push_back(
            Carrier{member_read(cells[0]), member_read(cells[1]), cells[4], cells[5]});
    }
    if (carried.empty())
    {
        printf("  %s/slices.txt holds no slice\n", folder.c_str());
        return 1;
    }
    std::vector<std::pair<std::string, std::vector<Carrier>>> addressed;
    // the most basal member writes an identity's first link
    for (auto &identity : carried)
    {
        std::sort(identity.second.begin(), identity.second.end(), carrier_before);
        if (member_before(identity.second[0].second, identity.second[0].first))
        {
            const size_t tab = identity.first.find('\t');
            for (Carrier &carrier : identity.second)
            {
                std::swap(carrier.first, carrier.second);
                std::swap(carrier.first_context, carrier.second_context);
            }
            addressed.push_back(std::make_pair(identity.first.substr(tab + 1u) + "\t" + identity.first.substr(0u, tab),
                                               identity.second));
            continue;
        }
        addressed.push_back(identity);
    }
    std::sort(addressed.begin(), addressed.end(),
              [](const std::pair<std::string, std::vector<Carrier>> &one,
                 const std::pair<std::string, std::vector<Carrier>> &other)
              { return carrier_before(one.second[0], other.second[0]); });
    std::vector<std::pair<std::string, std::vector<std::regex>>> forms;
    if (!forms_read(count, files, &forms))
    {
        return 1;
    }
    std::istringstream held(run_file_text(klq));
    std::stringstream kept;
    while (std::getline(held, line) && (line.rfind("slice_identity ", 0u) != 0u))
    {
        kept << line << "\n";
    }
    RunWritten file = {klq, 1, std::string()};
    run_print(&file, "%s", kept.str().c_str());
    unsigned int number = 0u;
    unsigned int contexts = 0u;
    unsigned int closed = 0u;
    unsigned int open = 0u;
    unsigned int unasked = 0u;
    unsigned int many = 0u;
    unsigned int split = 0u;
    unsigned int flagged = 0u;
    unsigned int formed = 0u;
    for (const auto &identity : addressed)
    {
        const size_t tab = identity.first.find('\t');
        const std::string sides[2] = {identity.first.substr(0u, tab), identity.first.substr(tab + 1u)};
        // an identity's address is its most basal carrier's
        run_print(&file, "slice_identity %s\nfirst %s\nsecond %s\n", carrier_text(identity.second[0]).c_str(),
                  sides[0].c_str(), sides[1].c_str());
        number += 1u;
        for (const std::string &side : sides)
        {
            if (rule_broken(side))
            {
                run_print(&file, "rule 2 %s: its form transfers control or waits\n", side.c_str());
                flagged += 1u;
            }
        }
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            for (const auto &form : forms)
            {
                const auto &links = form.second;
                if (std::any_of(links.begin(), links.end(),
                                [&](const std::regex &one) { return std::regex_match(sides[at], one); }))
                {
                    run_print(&file, "form %s %s\n", (at == 0u) ? "first" : "second", form.first.c_str());
                    formed += 1u;
                }
            }
        }
        // the carriers of one identity, gathered by context in the order of their most basal carrier: each context the
        // same members in one place of the grammar, with a verdict of its own
        std::vector<std::pair<std::string, std::vector<const Carrier *>>> placed;
        for (const Carrier &slice : identity.second)
        {
            const std::string key = slice.first_context + "\t" + slice.second_context;
            auto found = std::find_if(placed.begin(), placed.end(),
                                      [&](const std::pair<std::string, std::vector<const Carrier *>> &one)
                                      { return one.first == key; });
            if (found == placed.end())
            {
                placed.push_back(std::make_pair(key, std::vector<const Carrier *>()));
                found = placed.end() - 1;
            }
            found->second.push_back(&slice);
        }
        many += (placed.size() > 1u) ? 1u : 0u;
        int verdicts[2] = {0, 0};
        for (const auto &place : placed)
        {
            const Carrier &basal = *place.second[0];
            run_print(&file, "context %s\n", carrier_text(basal).c_str());
            // an entry both members hold is written once, and one a member alone holds is written under its side
            const std::set<std::string> first_entries = context_entries(basal.first_context);
            const std::set<std::string> second_entries = context_entries(basal.second_context);
            for (const std::string &entry : first_entries)
            {
                run_print(&file, "%s%s\n", second_entries.count(entry) ? "" : "first ", entry.c_str());
            }
            for (const std::string &entry : second_entries)
            {
                if (!first_entries.count(entry))
                {
                    run_print(&file, "second %s\n", entry.c_str());
                }
            }
            for (const Carrier *const slice : place.second)
            {
                run_print(&file, "address %s\n", carrier_text(*slice).c_str());
            }
            const int verdict = carriers_verdict(&file, place.second, answers, carrier_text);
            contexts += 1u;
            closed += (verdict == 1) ? 1u : 0u;
            open += (verdict == 0) ? 1u : 0u;
            unasked += (verdict == -1) ? 1u : 0u;
            verdicts[0] |= (verdict == 1);
            verdicts[1] |= (verdict == 0);
        }
        split += (verdicts[0] && verdicts[1]) ? 1u : 0u;
    }
    // the chains after the identities, as slice wrote them
    std::istringstream chained(run_blob_text("chains.txt"));
    while (std::getline(chained, line))
    {
        run_print(&file, "%s\n", line.c_str());
    }
    if (!run_closed(&file))
    {
        printf("  %s could not be written\n", klq.c_str());
        return 1;
    }
    printf("  %s: %u identities in %u contexts, %u identities in more than one, %u closed in one context and open in "
           "another\n",
           klq.c_str(), number, contexts, many, split);
    printf("  contexts: %u closed, %u open over the cases asked, %u asked of nothing; %u links a rule keeps off the part, "
           "%u forms written beside them\n",
           closed, open, unasked, flagged, formed);
    return 0;
}

// the files `given` names, a folder standing for every .dis file in it by name: a folder of the engine's writing of the
// stick holds more files than one command line can name
std::vector<std::string> candidate_paths(const std::vector<std::string> &given)
{
    std::vector<std::string> paths;
    for (const std::string &path : given)
    {
        if (!std::filesystem::is_directory(path))
        {
            paths.push_back(path);
            continue;
        }
        std::vector<std::string> held;
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path))
        {
            if (entry.path().extension() == ".dis")
            {
                held.push_back(entry.path().generic_string());
            }
        }
        std::sort(held.begin(), held.end());
        paths.insert(paths.end(), held.begin(), held.end());
    }
    return paths;
}

// the chains of a candidate file: one for each function its listing names, or the whole file as one chain named for
// its stem where it names none
void candidates_read(const std::string &path, std::vector<Question> *candidates)
{
    std::ifstream file(path);
    static const std::regex s_function("Function : ([A-Za-z0-9_]+)");
    const size_t slash = path.find_last_of("/\\");
    const std::string stem = path.substr((slash == std::string::npos) ? 0u : slash + 1u);
    const size_t first = candidates->size();
    std::string line;
    int ended = 0;
    while (std::getline(file, line))
    {
        std::smatch function;
        if (std::regex_search(line, function, s_function))
        {
            Question candidate;
            candidate.number = function[1].str();
            candidates->push_back(candidate);
            ended = 0;
            continue;
        }
        Link link;
        const int read = ended ? 0 : link_read(line, &link);
        ended = ended || (read < 0);
        if (read > 0)
        {
            if (candidates->size() == first)
            {
                Question candidate;
                candidate.number = stem.substr(0u, stem.find('.'));
                candidates->push_back(candidate);
            }
            candidates->back().chain.push_back(link);
        }
    }
}
