// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bridge.cu: the bridge read from the run, each pair's concept folded, each pair closed with what it is read
// into written beneath it, the bridge handed back to the run's writer, and the decoding printed
#include "decoder_internal.h"

// 1 where a line of the bridge is a set's name alone, `<set>_coherence`
static int set_line(const std::string &entry)
{
    const std::string suffix = "_coherence";
    return (entry.find(' ') == std::string::npos) && (entry.size() > suffix.size()) &&
           (entry.compare(entry.size() - suffix.size(), suffix.size(), suffix) == 0);
}

int Decoding::bridge_read(const char *klq)
{
    std::string line;
    unsigned char *bridge_bytes = NULL;
    unsigned long long bridge_size = 0ull;
    if (!qry_file_read(qry_run(), klq, &bridge_bytes, &bridge_size))
    {
        printf("klq_decoder: %s could not be read\n", klq);
        return 1;
    }
    // a file read whole is at most what a size_t counts
    std::istringstream held_bridge(std::string((const char *)bridge_bytes, (size_t)bridge_size));
    free(bridge_bytes);
    while (std::getline(held_bridge, line))
    {
        bridge.push_back((!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line);
    }
    // each pair's concept as it is written, its own product's where this log answered it and otherwise the one the
    // bridge held beneath it. A pair whose concept another pair shares is folded: one ask answers both
    std::map<std::string, std::string> pair_concepts;
    std::string concept_pair;
    for (const std::string &entry : bridge)
    {
        if (entry.rfind("pair ", 0u) == 0u)
        {
            std::stringstream words(entry);
            std::string kind;
            std::string first;
            std::string second;
            words >> kind >> first >> second;
            concept_pair = pair_key(first, second);
        }
        else if (!concept_pair.empty() && (entry.rfind("concept_coherence ", 0u) == 0u) &&
                 (decoded.count(concept_pair) == 0u))
        {
            pair_concepts[concept_pair] = entry.substr(18u);
        }
    }
    for (const auto &held : identities)
    {
        pair_concepts[held.first] = held.second;
    }
    std::map<std::string, unsigned int> pairs_of_concept;
    for (const auto &held : pair_concepts)
    {
        pairs_of_concept[held.second] += 1u;
    }
    for (const auto &held : pair_concepts)
    {
        if (pairs_of_concept[held.second] > 1u)
        {
            metas[held.first].insert("folding_coherence");
        }
    }
    return 0;
}

void Decoding::pair_closed()
{
    if (!pair_held.empty())
    {
        // a pair the log read the texts or the answers of hangs from the metas read of it now, and any other
        // from the metas the bridge held. The fold is read off every pair's concept, and always now
        const int texts_read = faceted.count(pair_held) != 0u;
        const int answered = decoded.count(pair_held) != 0u;
        for (const char *const meta : s_metas)
        {
            const int held = std::find(metas_held.begin(), metas_held.end(), meta) != metas_held.end();
            const int now = texts_read || answered || (std::string(meta) == "folding_coherence");
            if (now ? (metas[pair_held].count(meta) != 0u) : held)
            {
                written.push_back(meta);
                counted[meta] += 1u;
            }
        }
        // every set the pair is read into, each a light of its own: the categories its texts read it into and the
        // sets its answers read it into, each read now where the log read it and otherwise as the bridge held it
        std::set<std::string> lit;
        for (const std::string &set : sets_held)
        {
            if (named_in(s_categories, std::size(s_categories), set) ? !texts_read : !answered)
            {
                lit.insert(set);
            }
        }
        if (answered)
        {
            lit.insert(decoded[pair_held].begin(), decoded[pair_held].end());
        }
        if (texts_read)
        {
            lit.insert(categories[pair_held].begin(), categories[pair_held].end());
        }
        // only an answer reads a pair out of `unknown_coherence`, a member of every category beneath
        int read_down = 0;
        for (const std::string &set : lit)
        {
            read_down |= (!named_in(s_categories, std::size(s_categories), set) && (set != "unknown_coherence"))
                             ? 1
                             : 0;
        }
        if (read_down)
        {
            lit.erase("unknown_coherence");
        }
        else
        {
            lit.insert("unknown_coherence");
        }
        std::vector<std::string> sets;
        for (const char *const set : s_sets)
        {
            if (lit.count(set) != 0u)
            {
                sets.push_back(set);
                lit.erase(set);
            }
        }
        sets.insert(std::find(sets.begin(), sets.end(), "unknown_coherence"), lit.begin(), lit.end());
        // each set's signal and noise: its pairs, those a whole product names, and the rows of their products
        // that came back one way and both ways
        const auto product_held = products.find(pair_held);
        const size_t signal_rows = (product_held != products.end()) ? concept_signal(product_held->second) : 0u;
        const size_t noise_rows =
            (product_held != products.end()) ? (product_held->second.rows.size() - signal_rows) : 0u;
        for (const std::string &set : sets)
        {
            written.push_back(set);
            counted[set] += 1u;
            named_pairs[set] += (identities.count(pair_held) != 0u) ? 1u : 0u;
            set_signal[set] += signal_rows;
            set_noise[set] += noise_rows;
        }
        // a pair answered in this log is the concept its own product is, and none until that product is whole
        const std::string identity = (identities.count(pair_held) != 0u)
                                         ? ("concept_coherence " + identities[pair_held])
                                     : (decoded.count(pair_held) != 0u) ? std::string()
                                                                        : identity_held;
        if (!identity.empty())
        {
            written.push_back(identity);
            concepts[identity] += 1u;
        }
        // the intent the pair's forms carry of their own, the values each writes as the ruleset gives it
        std::string intent;
        for (const unsigned long long value : pair_qualifiers[pair_held])
        {
            char written_value[24];
            snprintf(written_value, sizeof(written_value), " %llx", value);
            intent += written_value;
        }
        intent = !intent.empty()                    ? ("intent_coherence" + intent)
                 : (decoded.count(pair_held) != 0u) ? std::string()
                                                    : intent_held;
        if (!intent.empty())
        {
            written.push_back(intent);
        }
    }
    pair_held.clear();
    sets_held.clear();
    metas_held.clear();
    identity_held.clear();
    intent_held.clear();
}

void Decoding::bridge_folded()
{
    for (const std::string &entry : bridge)
    {
        const int verdict = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        const int meta = std::find_if(std::begin(s_metas), std::end(s_metas),
                                      [&](const char *const each) { return entry == each; }) != std::end(s_metas);
        if (!pair_held.empty() && meta)
        {
            metas_held.push_back(entry);
            continue;
        }
        if (!pair_held.empty() && set_line(entry))
        {
            sets_held.push_back(entry);
            continue;
        }
        if (!pair_held.empty() && (entry.rfind("concept_coherence ", 0u) == 0u))
        {
            identity_held = entry;
            continue;
        }
        if (!pair_held.empty() && (entry.rfind("intent_coherence ", 0u) == 0u))
        {
            intent_held = entry;
            continue;
        }
        if (!pair_held.empty() && verdict)
        {
            written.push_back(entry);
            continue;
        }
        pair_closed();
        written.push_back(entry);
        if (entry.rfind("pair ", 0u) == 0u)
        {
            std::stringstream words(entry);
            std::string kind;
            std::string first;
            std::string second;
            words >> kind >> first >> second;
            pair_held = pair_key(first, second);
        }
    }
    pair_closed();
}

int Decoding::bridge_handed(const char *klq)
{
    // the bridge handed to the run's writer, which writes it in the file's place (qry_filed)
    std::string bridge_text;
    for (const std::string &kept : written)
    {
        bridge_text += kept + "\n";
    }
    if (qry_filed(qry_run(), klq, bridge_text.data(), bridge_text.size()) == 0ull)
    {
        printf("klq_decoder: %s could not be written through the run's writer\n", klq);
        return 1;
    }
    return 0;
}

void Decoding::printed(const char *klq)
{
    for (const auto &held : decoded)
    {
        std::string lights;
        for (const std::string &meta : metas[held.first])
        {
            lights += meta + " ";
        }
        for (const std::string &set : categories[held.first])
        {
            lights += set + " ";
        }
        for (const std::string &set : held.second)
        {
            lights += set + " ";
        }
        printf("  %s: %s%s\n", held.first.c_str(), lights.c_str(),
               (identities.count(held.first) != 0u) ? identities[held.first].c_str() : "-");
    }
    unsigned int shared = 0u;
    for (const auto &held : concepts)
    {
        shared += (held.second > 1u) ? 1u : 0u;
    }
    // the choices of every pair's puts by the trace's flags, and those of each pair the part answered nothing of, the
    // questions a further pass takes up
    const auto choices_written = [this](const std::map<unsigned long long, unsigned int> &held) {
        std::string written;
        for (const auto &flag : trace.named)
        {
            const auto count = held.find(flag.first);
            if (count != held.end())
            {
                written += (written.empty() ? "" : ", ") + std::to_string(count->second) + " " + flag.second;
            }
        }
        return written;
    };
    if (!chosen.empty())
    {
        printf("  choices: %s\n", choices_written(chosen).c_str());
    }
    const unsigned long long answered_flags = query_trace_flag(trace, "alike") | query_trace_flag(trace, "apart");
    for (const auto &held : choices)
    {
        int answered = 0;
        for (const auto &flag : held.second)
        {
            answered |= ((flag.first & answered_flags) != 0ull) ? 1 : 0;
        }
        if (!held.first.empty() && (decoded.count(held.first) == 0u) && !answered)
        {
            printf("  answered nothing, %s: %s\n", held.first.c_str(), choices_written(held.second).c_str());
        }
    }
    // each set's pairs, those a whole product names, and its signal against its noise in rows of the products
    for (const char *const set : s_sets)
    {
        if (counted[set] != 0u)
        {
            printf("  %s: %u pairs, %u named by a concept, %zu rows of signal, %zu of noise\n", set, counted[set],
                   named_pairs[set], set_signal[set], set_noise[set]);
        }
    }
    printf("klq_decoder: %u puts read, %u concepts, %u of more than one pair; metas", (unsigned int)puts.size(),
           (unsigned int)concepts.size(), shared);
    for (const char *const meta : s_metas)
    {
        printf(" %u %s", counted[meta], meta);
    }
    printf("; sets");
    for (const char *const set : s_sets)
    {
        printf(" %u %s", counted[set], set);
    }
    printf("; written to %s\n", klq);
}
