// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_write.cu: writes Lstar.klq, the bridge between languages, and each given language's .klm, its names keyed to
// the bridge
//
//   orior_klq_write <folder> <ruleset>...
//
// Each ruleset is read as its code generator reads it, its files checked against each other and its schema
// (ruleset_core_relation.h). The bridge is keyed by the schema's form names, which are gnascor's and no target's, and
// stores at each key the pairs whose sameness the text leaves open: names one word apart with texts the same but for
// their modifiers, and a form that opens another's literal operands. A pair put to no case is open with its count of
// cases alike, 0. A language's .klm keys each name it gives that the schema names, and names each relation of the map
// from the language to the bridge on its own line: a name one text with another in the language, which the bridge
// exists as two keys (breaks), and a name given as another type than a form (type). No name of a language collapses
// onto the bridge, which witnesses no two names one text. A name the schema does not name has no key, and is open
//
// The bridge's identities between texts and between slices with their verdicts, its primitives and its chains, are no
// ruleset's: each line is stored as the bridge recorded it and written after the keys. A pair the bridge recorded a
// verdict for, put to the part, is written with that verdict, and with the set, the concept and the intent
// orior_klq_decoder read it into
#include "../../qry/writer/qry_member.h"
#include "code_generator.h"
#include "target_internal.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// a language's files read and checked against each other and its schema
struct LanguageSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// each type of entry that gives a name, as its line's first word writes it
static const char *const s_type_written[] = {"form", "nop", "err", "construct"};

// the language's map and the bridge, each put together here and then passed to the run's writer as its file, and the
// bytes each contains so far, the room once a write did not fit
static char s_map_bytes[QRY_WRITER_RING_BYTES / 2ull];
static size_t s_map_at;
static char s_bridge_bytes[QRY_WRITER_RING_BYTES / 2ull];
static size_t s_bridge_at;

// `text` written after what the map contains
static void map_text(const char *text)
{
    const VerbaTextusCfg written = {s_map_bytes, sizeof(s_map_bytes), s_map_at, text, 0u};
    s_map_at = verba_textus.put(&written);
}

// `text` written after what the bridge contains
static void bridge_text(const char *text)
{
    const VerbaTextusCfg written = {s_bridge_bytes, sizeof(s_bridge_bytes), s_bridge_at, text, 0u};
    s_bridge_at = verba_textus.put(&written);
}

// 1 where every write after `at` bytes of a text of `room` fit
static int text_fits(size_t room, size_t at)
{
    const VerbaFinisCfg written = {NULL, room, at};
    return verba_finis.ok(&written) != 0;
}

// the name of entry `at` of `set`
static std::string entry_name(const LanguageSet *set, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    return std::string((const char *)&set->relations.text[entry->name.first], entry->name.length);
}

// 1 where entry `at` of `set` is the first to give its name, and the schema `schema` names it as a form
static int entry_keyed(const LanguageSet *set, const RulesetSchema *schema, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    return ruleset_core_relation_gives(entry->type) &&
           (ruleset_core_relation_giver(&set->relations, entry->name) == at) &&
           (ruleset_find(schema->forms, schema->form_count, entry_name(set, at)) < schema->form_count);
}

// the ruleset `file` read into `set` the way its code generator reads it. 1 where its files were read
static int language_read(const char *file, LanguageSet *set)
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
static int map_write(const std::string &path, const std::string &stem, const LanguageSet *set,
                     const RulesetSchema *schema)
{
    map_text("klm ");
    map_text(stem.c_str());
    map_text("\n");
    unsigned int keys = 0u;
    unsigned int relations = 0u;
    for (unsigned int at = 0u; at < set->relations.entry_count; at += 1u)
    {
        if (!entry_keyed(set, schema, at))
        {
            continue;
        }
        const std::string name = entry_name(set, at);
        map_text("key ");
        map_text(name.c_str());
        map_text(" ");
        map_text(name.c_str());
        map_text("\n");
        keys += 1u;
        const unsigned int type = set->relations.entries[at].type;
        if (type != RULESET_CORE_ENTRY_FORM)
        {
            map_text("type ");
            map_text(name.c_str());
            map_text(" ");
            map_text(s_type_written[type]);
            map_text("\n");
            relations += 1u;
        }
        for (unsigned int earlier = 0u; earlier < at; earlier += 1u)
        {
            if (entry_keyed(set, schema, earlier) && ruleset_core_map_same(&set->relations, at, earlier))
            {
                map_text("breaks ");
                map_text(name.c_str());
                map_text(" ");
                map_text(entry_name(set, earlier).c_str());
                map_text("\n");
                relations += 1u;
            }
        }
    }
    if (!text_fits(sizeof(s_map_bytes), s_map_at) || !qry_member_filed(path.c_str(), s_map_bytes, s_map_at))
    {
        printf("  %s could not be written\n", path.c_str());
        return 0;
    }
    printf("  %s: %u keys, %u relations of the map witnessed\n", path.c_str(), keys, relations);
    return 1;
}

// the pair `key`, `other` stored at its key in `pairs`, where it is not stored already
static void pair_add(std::vector<std::pair<std::string, std::string>> *pairs, const std::string &key,
                     const std::string &other)
{
    for (const std::pair<std::string, std::string> &entry : *pairs)
    {
        if ((entry.first == key) && (entry.second == other))
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
        printf("orior_klq_write <folder> <ruleset>...\n");
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
        LanguageSet set;
        if (language_read(file.c_str(), &set) == 0)
        {
            failed = 1;
            continue;
        }
        const std::string stem = file.substr(0u, file.rfind('.'));
        failed |= (map_write(folder + "/" + stem + ".klm", stem, &set, schema) == 0) ? 1 : 0;
        const RulesetCoreRelations *const relations = &set.relations;
        const unsigned int relation_count = (relations->relation_count < relations->relation_capacity)
                                                ? relations->relation_count
                                                : relations->relation_capacity;
        for (unsigned int at = 0u; at < relation_count; at += 1u)
        {
            const RulesetCoreRelation *const relation = &relations->relations[at];
            if ((relation->type != RULESET_CORE_RELATION_MODIFIERS) && (relation->type != RULESET_CORE_RELATION_OPENED))
            {
                continue;
            }
            if (!entry_keyed(&set, schema, relation->entry) || !entry_keyed(&set, schema, relation->other))
            {
                unkeyed += 1u;
                continue;
            }
            pair_add(&pairs, entry_name(&set, relation->entry), entry_name(&set, relation->other));
        }
    }
    const std::string path = folder + "/Lstar.klq";
    // every line the bridge contains that no ruleset writes, its identities between texts and between slices with their
    // verdicts, its primitives and its chains, stored whole: each line that is not the bridge's first, a key, a pair or
    // a line beneath a pair, written again after the keys as it was read
    std::vector<std::string> identities;
    // each pair's verdict as the bridge recorded it, the sets of our coherence orior_klq_decoder read it into, its
    // metas and then its set, its concept and its intent, each written again beneath the pair
    std::map<std::string, std::string> verdicts;
    std::map<std::string, std::vector<std::string>> sets;
    std::map<std::string, std::string> relations;
    std::map<std::string, std::string> intents;
    std::string pair_key;
    // the bridge as the run stores it (qry_member_file), empty on a first run
    const unsigned char *bridge_bytes = NULL;
    unsigned long long bridge_size = 0ull;
    std::string bridge_read;
    if (qry_member_file_blob(path.c_str(), &bridge_bytes, &bridge_size))
    {
        // a file read whole is at max what a size_t counts
        bridge_read.assign(reinterpret_cast<const char *>(bridge_bytes), static_cast<size_t>(bridge_size));
    }
    std::istringstream bridge_stream(bridge_read);
    std::string line;
    int first = 1;
    while (std::getline(bridge_stream, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        const int verdict = (line.rfind("open ", 0u) == 0u) || (line.rfind("closed ", 0u) == 0u);
        const std::string suffix = "_coherence";
        const int set = (line.find(' ') == std::string::npos) && (line.size() > suffix.size()) &&
                        (line.compare(line.size() - suffix.size(), suffix.size(), suffix) == 0);
        if (verdict && !pair_key.empty() && (verdicts.count(pair_key) == 0u))
        {
            verdicts[pair_key] = line;
        }
        if (set && !pair_key.empty())
        {
            sets[pair_key].push_back(line);
        }
        const int relation = (line.rfind("concept_coherence ", 0u) == 0u);
        if (relation && !pair_key.empty())
        {
            relations[pair_key] = line;
        }
        const int intent = (line.rfind("intent_coherence ", 0u) == 0u);
        if (intent && !pair_key.empty())
        {
            intents[pair_key] = line;
        }
        const int beneath_pair = !pair_key.empty() && (verdict || set || relation || intent);
        const int ruleset_line = first || (line.rfind("key ", 0u) == 0u) || (line.rfind("pair ", 0u) == 0u);
        pair_key = (line.rfind("pair ", 0u) == 0u) ? line : (beneath_pair ? pair_key : std::string());
        if (!beneath_pair && !ruleset_line && !line.empty())
        {
            identities.push_back(line);
        }
        first = 0;
    }
    bridge_text("klq L*\n");
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
        bridge_text("key ");
        bridge_text(keys[at].c_str());
        bridge_text("\n");
        written += 1u;
        for (const std::pair<std::string, std::string> &entry : pairs)
        {
            if (entry.first == keys[at])
            {
                const std::string pair = "pair " + entry.first + " " + entry.second;
                bridge_text(pair.c_str());
                bridge_text("\n");
                bridge_text((verdicts.count(pair) != 0u) ? verdicts[pair].c_str() : "open 0");
                bridge_text("\n");
                for (const std::string &set : sets[pair])
                {
                    bridge_text(set.c_str());
                    bridge_text("\n");
                }
                if (relations.count(pair) != 0u)
                {
                    bridge_text(relations[pair].c_str());
                    bridge_text("\n");
                }
                if (intents.count(pair) != 0u)
                {
                    bridge_text(intents[pair].c_str());
                    bridge_text("\n");
                }
            }
        }
    }
    for (const std::string &identity_line : identities)
    {
        bridge_text(identity_line.c_str());
        bridge_text("\n");
    }
    if (!text_fits(sizeof(s_bridge_bytes), s_bridge_at) || !qry_member_filed(path.c_str(), s_bridge_bytes, s_bridge_at))
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    printf("  %s: %u keys, %u pairs open, %u pairs of names the schema does not name, %u lines no ruleset writes "
           "retained\n",
           path.c_str(), written, (unsigned int)pairs.size(), unkeyed, (unsigned int)identities.size());
    return failed;
}
