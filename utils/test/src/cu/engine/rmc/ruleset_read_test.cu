// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// Every ruleset read against its code generator's schema, and the files each is read from held against each other
// and the schema for the relations no set of them should hold (ruleset_core_relation.h); then the map from cu's files
// to sass's, each name as one writes it against the other. Files named on the command line are held against each other
// instead, in whatever combination they are given and against no schema. Each set and the map is one check. A set
// fails where it holds a witnessed relation that is a verdict (RULESET_CORE_RELATION_VERDICT); one between two forms'
// writings is written as kept, and an open one, a relation of absence no part of a set can decide
// (RULESET_CORE_RELATION_OPEN, RULESET_CORE_MAP_OPEN), as open, and neither fails anything. The map's relations are the
// lines a lane cannot be read through as it stands, written and failing nothing
#include "code_generator.h"
#include "target_internal.h"

#include <stdio.h>

static unsigned int s_checks;
static unsigned int s_failed;

// a set of files read and held against each other
struct HeldSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// entry `at` of `set` written as its file, its line and its name
static void entry_print(const HeldSet *set, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    printf("%s:%u %.*s", set->memory.labels[entry->file].c_str(), entry->line, (int)entry->name.length,
           (const char *)&set->relations.text[entry->name.first]);
}

// relation `relation` of `set` written on a line of its own: its entry, what it says, and what it names
static void relation_print(const HeldSet *set, const RulesetSchema *names, const RulesetCoreRelation *relation)
{
    const RulesetCoreRelations *const relations = &set->relations;
    const unsigned int kind = relation->kind;
    printf(RULESET_CORE_RELATION_OPEN(kind)      ? "    open: "
           : RULESET_CORE_RELATION_VERDICT(kind) ? "    "
                                                 : "    kept: ");
    if (kind == RULESET_CORE_RELATION_SCHEMA_NOT_GIVEN)
    {
        const RulesetName *const list = (relation->other == 0u)   ? names->forms
                                        : (relation->other == 1u) ? names->banks
                                                                  : names->fixed;
        printf("%s %s\n", list[relation->entry].text, ruleset_relation_said(kind));
        return;
    }
    entry_print(set, relation->entry);
    printf(" %s", ruleset_relation_said(kind));
    if (kind == RULESET_CORE_RELATION_PARAMETER_UNWRITTEN)
    {
        const RulesetCoreSpan parameter = ruleset_core_relation_part(
            relations->text, relations->entries[relation->entry].head, (unsigned char)' ', relation->other);
        printf(" %.*s", (int)parameter.length, (const char *)&relations->text[parameter.first]);
    }
    else if ((kind == RULESET_CORE_RELATION_PLACE_UNKNOWN) || (kind == RULESET_CORE_RELATION_COUNT_DISAGREES))
    {
        printf(" %u", relation->other);
    }
    else if (kind == RULESET_CORE_RELATION_SCHEMA_PARAMETERS)
    {
        printf(" %s, %u", names->forms[relation->other].text, names->forms[relation->other].parameters);
    }
    else if ((kind != RULESET_CORE_RELATION_NO_TEXT) && (kind != RULESET_CORE_RELATION_LINE_NOT_GIVEN) &&
             (kind != RULESET_CORE_RELATION_CLASS_NOT_NAMED) && (kind != RULESET_CORE_RELATION_SCHEMA_NOT_NAMED))
    {
        printf(" ");
        entry_print(set, relation->other);
    }
    printf("\n");
}

// the files at `paths` read into `set` and held against each other, and against `schema` where it is not NULL, `names`
// its names; every relation written, and the set failed where it holds a verdict. 1 where it was read
static int relations_check(const char *label, const std::string *paths, unsigned int count,
                           const RulesetCoreSchema *schema, const RulesetSchema *names, HeldSet *set)
{
    s_checks += 1u;
    std::string error;
    if (ruleset_relations_read(paths, count, schema, &set->memory, &set->relations, &error) == 0)
    {
        printf("  %s could not be opened\n", error.c_str());
        s_failed += 1u;
        return 0;
    }
    const RulesetCoreRelations *const relations = &set->relations;
    const unsigned int kept = (relations->relation_count < relations->relation_capacity) ? relations->relation_count
                                                                                          : relations->relation_capacity;
    unsigned int open = 0u;
    unsigned int verdicts = 0u;
    for (unsigned int at = 0u; at < kept; at += 1u)
    {
        relation_print(set, names, &relations->relations[at]);
        open += RULESET_CORE_RELATION_OPEN(relations->relations[at].kind) ? 1u : 0u;
        verdicts += RULESET_CORE_RELATION_VERDICT(relations->relations[at].kind) ? 1u : 0u;
    }
    // a relation past the capacity is counted and not kept, and is read as a verdict
    verdicts += relations->relation_count - kept;
    printf("  %s: %u files, %u entries, %u relations witnessed, %u of them verdicts, %u open\n", label, count,
           relations->entry_count, relations->relation_count - open, verdicts, open);
    s_failed += (verdicts != 0u) ? 1u : 0u;
    return 1;
}

// a language's ruleset read against its schema, then the files it is read from held against each other and the
// schema, into `set`. 1 where the files were read
static int target_check(Target &target, HeldSet *set)
{
    const RulesetSchema *const schema = target.schema();
    s_checks += 1u;
    const Ruleset *const rules = target.ruleset(1);
    s_failed += (rules == NULL) ? 1u : 0u;
    printf("  %s: %s\n", schema->file, (rules != NULL) ? "read" : "errored");
    std::string paths[RULESET_PATHS_MAX];
    const unsigned int count = ruleset_paths(schema, ruleset_folder() + "/" + schema->file, paths);
    RulesetFlat flat;
    ruleset_flat_schema(schema, &flat);
    return relations_check(schema->file, paths, count, &flat.schema, schema, set);
}

// the map from `from` to `to`, each name `from` gives against the same name in `to`: every relation written
static void map_check(const char *label, const HeldSet *from, const HeldSet *to)
{
    s_checks += 1u;
    std::vector<RulesetCoreRelation> maps((3u * from->relations.entry_count) + 1u);
    const unsigned int count =
        ruleset_core_map(&from->relations, &to->relations, maps.data(), (unsigned int)maps.size());
    unsigned int open = 0u;
    for (unsigned int at = 0u; (at < count) && (at < (unsigned int)maps.size()); at += 1u)
    {
        const RulesetCoreRelation *const map = &maps[at];
        open += RULESET_CORE_MAP_OPEN(map->kind) ? 1u : 0u;
        printf(RULESET_CORE_MAP_OPEN(map->kind) ? "    open: " : "    ");
        entry_print(from, map->entry);
        printf(" %s", ruleset_map_said(map->kind));
        if ((map->kind == RULESET_CORE_MAP_BREAKS) || (map->kind == RULESET_CORE_MAP_COLLAPSES))
        {
            printf(" ");
            entry_print(from, map->other);
        }
        else if (map->kind == RULESET_CORE_MAP_KIND)
        {
            printf(" ");
            entry_print(to, map->other);
        }
        printf("\n");
    }
    printf("  %s: %u relations of the map witnessed, %u open\n", label, count - open, open);
}

int main(int argc, char **argv)
{
    printf("ruleset read test\n");
    if (argc > 1)
    {
        std::vector<std::string> paths(argv + 1, argv + argc);
        HeldSet given;
        relations_check("the files given", paths.data(), (unsigned int)paths.size(), NULL, NULL, &given);
    }
    else
    {
        HeldSet sets[5];
        target_check(code_generator("c.krs"), &sets[0]);
        const int cu = target_check(code_generator("cu.krs"), &sets[1]);
        target_check(code_generator("ptx.krs"), &sets[2]);
        target_check(code_generator("vhdl.krs"), &sets[3]);
        const int sass = target_check(code_generator("sass.krs"), &sets[4]);
        if ((cu != 0) && (sass != 0))
        {
            map_check("cu.krs to sass.krs", &sets[1], &sets[4]);
            map_check("sass.krs to cu.krs", &sets[4], &sets[1]);
        }
    }
    printf("ruleset read test: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
