// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_write.cu: writes Lstar.klq, the bridge between languages, and each given language's .klm, its names keyed to
// the bridge
//
//   klq_write <folder> <ruleset>...
//
// Each ruleset is read as its code generator reads it, its files held against each other and its schema
// (ruleset_core_relation.h). The bridge is keyed by the schema's form names, which are gnascor's and no target's, and
// holds at each key the pairs whose sameness the text leaves open: names one word apart with texts the same but for
// their modifiers, and a form that opens another's literal operands. A pair put to no case is open with its count of
// cases alike, 0. A language's .klm keys each name it gives that the schema names, and names each relation of the map
// from the language to the bridge on its own line: a name one text with another in the language, which the bridge
// holds as two keys (breaks), and a name given as another kind than a form (kind). No name of a language collapses
// onto the bridge, which witnesses no two names one text. A name the schema does not name has no key, and is open
//
// The bridge's identities between texts and between slices with their verdicts, its primitives and its chains, are no
// ruleset's: each line is kept as the bridge held it and written after the keys. A pair the bridge held a verdict for,
// put to the part, is written with that verdict, and with the set, the concept and the intent klq_decoder read it into
#include "code_generator.h"
#include "target_internal.h"

#include <stdio.h>

#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

// a language's files read and held against each other and its schema
struct HeldSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// each kind of entry that gives a name, as its line's first word writes it
static const char *const s_kind_written[] = {"form", "nop", "err", "construct"};

// the name of entry `at` of `set`
static std::string entry_name(const HeldSet *set, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    return std::string((const char *)&set->relations.text[entry->name.first], entry->name.length);
}

// 1 where entry `at` of `set` is the first to give its name, and the schema `schema` names it as a form
static int entry_keyed(const HeldSet *set, const RulesetSchema *schema, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    return ruleset_core_relation_gives(entry->kind) &&
           (ruleset_core_relation_giver(&set->relations, entry->name) == at) &&
           (ruleset_find(schema->forms, schema->form_count, entry_name(set, at)) < schema->form_count);
}

// the ruleset `file` read into `set` the way its code generator reads it. 1 where its files were read
static int language_read(const char *file, HeldSet *set)
{
    const RulesetSchema *const schema = code_generator(file).schema();
    std::string paths[RULESET_PATHS_MAX];
    const unsigned int count = ruleset_paths(schema, ruleset_folder() + "/" + schema->file, paths);
    RulesetFlat flat;
    ruleset_flat_schema(schema, &flat);
    std::string error;
    if (ruleset_relations_read(paths, count, &flat.schema, &set->memory, &set->relations, &error) == 0)
    {
        printf("  %s could not be opened\n", error.c_str());
        return 0;
    }
    return 1;
}

// the language `stem` read into `set` against `schema`, its map to the bridge written to `path`. 1 where it was
// written
static int map_write(const std::string &path, const std::string &stem, const HeldSet *set, const RulesetSchema *schema)
{
    FILE *const map = fopen(path.c_str(), "wb");
    if (map == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 0;
    }
    fprintf(map, "klm %s\n", stem.c_str());
    unsigned int keys = 0u;
    unsigned int relations = 0u;
    for (unsigned int at = 0u; at < set->relations.entry_count; at += 1u)
    {
        if (!entry_keyed(set, schema, at))
        {
            continue;
        }
        const std::string name = entry_name(set, at);
        fprintf(map, "key %s %s\n", name.c_str(), name.c_str());
        keys += 1u;
        const unsigned int kind = set->relations.entries[at].kind;
        if (kind != RULESET_CORE_ENTRY_FORM)
        {
            fprintf(map, "kind %s %s\n", name.c_str(), s_kind_written[kind]);
            relations += 1u;
        }
        for (unsigned int earlier = 0u; earlier < at; earlier += 1u)
        {
            if (entry_keyed(set, schema, earlier) && ruleset_core_map_same(&set->relations, at, earlier))
            {
                fprintf(map, "breaks %s %s\n", name.c_str(), entry_name(set, earlier).c_str());
                relations += 1u;
            }
        }
    }
    fclose(map);
    printf("  %s: %u keys, %u relations of the map witnessed\n", path.c_str(), keys, relations);
    return 1;
}

// the pair `key`, `other` held at its key in `pairs`, where it is not held already
static void pair_hold(std::vector<std::pair<std::string, std::string>> *pairs, const std::string &key,
                      const std::string &other)
{
    for (const std::pair<std::string, std::string> &held : *pairs)
    {
        if ((held.first == key) && (held.second == other))
        {
            return;
        }
    }
    pairs->push_back(std::make_pair(key, other));
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("klq_write <folder> <ruleset>...\n");
        return 1;
    }
    const std::string folder = argv[1];
    std::vector<std::string> keys;
    std::vector<std::pair<std::string, std::string>> pairs;
    unsigned int unkeyed = 0u;
    int failed = 0;
    for (int given = 2; given < argc; given += 1)
    {
        const std::string file = argv[given];
        const RulesetSchema *const schema = code_generator(file.c_str()).schema();
        for (unsigned int at = 0u; at < schema->form_count; at += 1u)
        {
            const std::string key = schema->forms[at].text;
            if (ruleset_find(schema->forms, at, key) == at)
            {
                keys.push_back(key);
            }
        }
        HeldSet set;
        if (language_read(file.c_str(), &set) == 0)
        {
            failed = 1;
            continue;
        }
        const std::string stem = file.substr(0u, file.rfind('.'));
        failed |= (map_write(folder + "/" + stem + ".klm", stem, &set, schema) == 0) ? 1 : 0;
        const RulesetCoreRelations *const relations = &set.relations;
        const unsigned int kept = (relations->relation_count < relations->relation_capacity)
                                      ? relations->relation_count
                                      : relations->relation_capacity;
        for (unsigned int at = 0u; at < kept; at += 1u)
        {
            const RulesetCoreRelation *const relation = &relations->relations[at];
            if ((relation->kind != RULESET_CORE_RELATION_MODIFIERS) && (relation->kind != RULESET_CORE_RELATION_OPENED))
            {
                continue;
            }
            if (!entry_keyed(&set, schema, relation->entry) || !entry_keyed(&set, schema, relation->other))
            {
                unkeyed += 1u;
                continue;
            }
            pair_hold(&pairs, entry_name(&set, relation->entry), entry_name(&set, relation->other));
        }
    }
    const std::string path = folder + "/Lstar.klq";
    // every line the bridge holds that no ruleset writes, its identities between texts and between slices with their
    // verdicts, its primitives and its chains, kept whole: each line that is not the bridge's first, a key, a pair or
    // a line beneath a pair, written again after the keys as it was read
    std::vector<std::string> identities;
    // each pair's verdict as the bridge held it, the set of our coherence klq_decoder read it into, its concept and its
    // intent, each written again beneath the pair
    std::map<std::string, std::string> verdicts;
    std::map<std::string, std::string> sets;
    std::map<std::string, std::string> relations;
    std::map<std::string, std::string> intents;
    std::string pair_held;
    std::ifstream held_bridge(path, std::ios::binary);
    std::string line;
    int first = 1;
    while (std::getline(held_bridge, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        const int verdict = (line.rfind("open ", 0u) == 0u) || (line.rfind("closed ", 0u) == 0u);
        const std::string suffix = "_coherence";
        const int set = (line.find(' ') == std::string::npos) && (line.size() > suffix.size()) &&
                        (line.compare(line.size() - suffix.size(), suffix.size(), suffix) == 0);
        if (verdict && !pair_held.empty() && (verdicts.count(pair_held) == 0u))
        {
            verdicts[pair_held] = line;
        }
        if (set && !pair_held.empty())
        {
            sets[pair_held] = line;
        }
        const int relation = (line.rfind("concept_coherence ", 0u) == 0u);
        if (relation && !pair_held.empty())
        {
            relations[pair_held] = line;
        }
        const int intent = (line.rfind("intent_coherence ", 0u) == 0u);
        if (intent && !pair_held.empty())
        {
            intents[pair_held] = line;
        }
        const int beneath_pair = !pair_held.empty() && (verdict || set || relation || intent);
        const int ruleset_line = first || (line.rfind("key ", 0u) == 0u) || (line.rfind("pair ", 0u) == 0u);
        pair_held = (line.rfind("pair ", 0u) == 0u) ? line : (beneath_pair ? pair_held : std::string());
        if (!beneath_pair && !ruleset_line && !line.empty())
        {
            identities.push_back(line);
        }
        first = 0;
    }
    held_bridge.close();
    FILE *const bridge = fopen(path.c_str(), "wb");
    if (bridge == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    fprintf(bridge, "klq L*\n");
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < (unsigned int)keys.size(); at += 1u)
    {
        int earlier = 0;
        for (unsigned int before = 0u; before < at; before += 1u)
        {
            earlier |= (keys[before] == keys[at]) ? 1 : 0;
        }
        if (earlier != 0)
        {
            continue;
        }
        fprintf(bridge, "key %s\n", keys[at].c_str());
        written += 1u;
        for (const std::pair<std::string, std::string> &held : pairs)
        {
            if (held.first == keys[at])
            {
                const std::string pair = "pair " + held.first + " " + held.second;
                fprintf(bridge, "%s\n%s\n", pair.c_str(),
                        (verdicts.count(pair) != 0u) ? verdicts[pair].c_str() : "open 0");
                if (sets.count(pair) != 0u)
                {
                    fprintf(bridge, "%s\n", sets[pair].c_str());
                }
                if (relations.count(pair) != 0u)
                {
                    fprintf(bridge, "%s\n", relations[pair].c_str());
                }
                if (intents.count(pair) != 0u)
                {
                    fprintf(bridge, "%s\n", intents[pair].c_str());
                }
            }
        }
    }
    for (const std::string &kept_line : identities)
    {
        fprintf(bridge, "%s\n", kept_line.c_str());
    }
    fclose(bridge);
    printf("  %s: %u keys, %u pairs open, %u pairs of names the schema does not name, %u lines no ruleset writes "
           "kept\n",
           path.c_str(), written, (unsigned int)pairs.size(), unkeyed, (unsigned int)identities.size());
    return failed;
}
