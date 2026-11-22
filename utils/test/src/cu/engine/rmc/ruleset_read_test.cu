// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// Every ruleset read against its code generator's schema, and the files each is read from checked against each other
// and the schema for the relations no set of them should contain (ruleset_core_relation.h); then the map from cu's
// files to sass's, each name as one writes it against the other. Files named on the command line are checked against
// each other instead, in whatever combination they are given and against no schema. Each set and the map is one check.
// A set fails where it contains a witnessed relation that is a verdict (RULESET_CORE_RELATION_VERDICT); one between two
// forms' writings is written as retained, and an open one, a relation of absence no part of a set can decide
// (RULESET_CORE_RELATION_OPEN, RULESET_CORE_MAP_OPEN), as open, and neither fails anything. The map's relations are the
// lines a lane cannot be read through as it is, written and failing nothing
#include "../../../../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "code_generator.h"
#include "memoria_operor/memoria_linea.h"
#include "target_internal.h"

#include <stdio.h>

static unsigned int s_checks;
static unsigned int s_failed;

// a set of files read and checked against each other
struct LanguageSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// entry `at` of `set` written as its file, its line and its name
static void entry_print(const LanguageSet *set, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, set->memory.labels[entry->file]);
        mmgr_line_put(output_line, ":");
        mmgr_line_decimal(output_line, entry->line, 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_max(output_line, (const char *)&set->relations.text[entry->name.first],
                          (size_t)((int)entry->name.length));
        platform_line_out_ended();
    }
}

// relation `relation` of `set` written on a line of its own: its entry, what it states, and what it names
static void relation_print(const LanguageSet *set, const RulesetSchema *names, const RulesetCoreRelation *relation)
{
    const RulesetCoreRelations *const relations = &set->relations;
    const unsigned int type = relation->type;
    mmgr_line_put(platform_line_out(), RULESET_CORE_RELATION_OPEN(type)      ? "    open: "
                                       : RULESET_CORE_RELATION_VERDICT(type) ? "    "
                                                                             : "    retained: ");
    platform_line_out_ended();
    if (type == RULESET_CORE_RELATION_SCHEMA_NOT_GIVEN)
    {
        const RulesetName *const list = (relation->other == 0u)   ? names->forms
                                        : (relation->other == 1u) ? names->banks
                                                                  : names->fixed;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, list[relation->entry].text);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, ruleset_relation_text(type));
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return;
    }
    entry_print(set, relation->entry);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, ruleset_relation_text(type));
        platform_line_out_ended();
    }
    if (type == RULESET_CORE_RELATION_PARAMETER_UNWRITTEN)
    {
        const RulesetCoreSpan parameter = ruleset_core_relation_part(
            relations->text, relations->entries[relation->entry].head, (unsigned char)' ', relation->other);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_put_max(output_line, (const char *)&relations->text[parameter.first],
                              (size_t)((int)parameter.length));
            platform_line_out_ended();
        }
    }
    else if ((type == RULESET_CORE_RELATION_PLACE_UNKNOWN) || (type == RULESET_CORE_RELATION_COUNT_DISAGREES))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, relation->other, 1u);
        platform_line_out_ended();
    }
    else if (type == RULESET_CORE_RELATION_SCHEMA_PARAMETERS)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, names->forms[relation->other].text);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, names->forms[relation->other].parameters, 1u);
        platform_line_out_ended();
    }
    else if ((type != RULESET_CORE_RELATION_NO_TEXT) && (type != RULESET_CORE_RELATION_LINE_NOT_GIVEN) &&
             (type != RULESET_CORE_RELATION_CLASS_NOT_NAMED) && (type != RULESET_CORE_RELATION_SCHEMA_NOT_NAMED))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            platform_line_out_ended();
        }
        entry_print(set, relation->other);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

// the files at `paths` read into `set` and checked against each other, and against `schema` where it is not NULL,
// `names` its names. Every relation written, and the set failed where it contains a verdict. 1 where it was read
static int relations_check(const char *label, const char (*paths)[PLATFORM_PATH_BYTES], unsigned int count,
                           const RulesetCoreSchema *schema, const RulesetSchema *names, LanguageSet *set)
{
    s_checks += 1u;
    char why[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    const int read = ruleset_relations_read(paths, count, schema, &set->memory, &set->relations, &error);
    (void)mmgr_line_finish(&error);
    if (read == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, why);
            mmgr_line_put(output_line, " could not be opened");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        s_failed += 1u;
        return 0;
    }
    const RulesetCoreRelations *const relations = &set->relations;
    const unsigned int relation_count = (relations->relation_count < relations->relation_capacity)
                                            ? relations->relation_count
                                            : relations->relation_capacity;
    unsigned int open = 0u;
    unsigned int verdicts = 0u;
    for (unsigned int at = 0u; at < relation_count; at += 1u)
    {
        relation_print(set, names, &relations->relations[at]);
        open += RULESET_CORE_RELATION_OPEN(relations->relations[at].type) ? 1u : 0u;
        verdicts += RULESET_CORE_RELATION_VERDICT(relations->relations[at].type) ? 1u : 0u;
    }
    // a relation past the capacity is counted and not stored, and is read as a verdict
    verdicts += relations->relation_count - relation_count;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, label);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, count, 1u);
        mmgr_line_put(output_line, " files, ");
        mmgr_line_decimal(output_line, relations->entry_count, 1u);
        mmgr_line_put(output_line, " entries, ");
        mmgr_line_decimal(output_line, relations->relation_count - open, 1u);
        mmgr_line_put(output_line, " relations witnessed, ");
        mmgr_line_decimal(output_line, verdicts, 1u);
        mmgr_line_put(output_line, " of them verdicts, ");
        mmgr_line_decimal(output_line, open, 1u);
        mmgr_line_put(output_line, " open");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    s_failed += (verdicts != 0u) ? 1u : 0u;
    return 1;
}

// a language's ruleset read against its schema, then the files it is read from checked against each other and the
// schema, into `set`. 1 where the files were read
static int target_check(Target &target, LanguageSet *set)
{
    const RulesetSchema *const schema = target.schema();
    s_checks += 1u;
    const Ruleset *const rules = target.ruleset(1);
    s_failed += (rules == NULL) ? 1u : 0u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, schema->file);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, (rules != NULL) ? "read" : "errored");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    char folder[PLATFORM_PATH_BYTES];
    char path[PLATFORM_PATH_BYTES];
    VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
    if (ruleset_folder(folder, sizeof(folder)) != 0)
    {
        mmgr_line_put(&path_line, folder);
        mmgr_line_ch(&path_line, '/');
        mmgr_line_put(&path_line, schema->file);
    }
    (void)mmgr_line_finish(&path_line);
    char paths[RULESET_PATHS_MAX][PLATFORM_PATH_BYTES];
    const unsigned int count = ruleset_paths(schema, path, paths);
    RulesetFlat flat;
    ruleset_flat_schema(schema, &flat);
    // the read of the relations reads the flat schema, and nothing reads it after
    const int read = relations_check(schema->file, paths, count, &flat.schema, schema, set);
    ruleset_flat_ended(&flat);
    return read;
}

// the scratch an open check gives each construct: R100 of any bank
static int open_scratch(void *context, const char *bank, VerbaLinea *name)
{
    (void)context;
    (void)bank;
    mmgr_line_put(name, "R100");
    return 1;
}

// a form the part's ruleset does not give is open (P10): the ruleset reads, a lane that decides the open form is
// refused, one that decides a given form is written, and one that decides the lane's structure, given nop, is
// written without it
static void open_check(Target &target)
{
    s_checks += 1u;
    const Ruleset *const rules = target.ruleset(1);
    const RulesetNamedScratch scratch = {open_scratch, NULL};
    PlatformText text;
    platform_text_begun(&text, RULESET_FILE_MAX, platform_place_g_program_owner, 0);
    const char *const registers[3] = {"R1", "R0", "R7"};
    const char *const note[6] = {"0", "0", "0", "0", "0", "0"};
    const int open_written =
        (rules != NULL) ? ruleset_form_written(rules, "word_div", registers, 3u, &scratch, &text.line) : 1;
    const int given_written =
        (rules != NULL) ? ruleset_form_written(rules, "word_add", registers, 3u, &scratch, &text.line) : 0;
    const int structure_written =
        (rules != NULL) ? ruleset_form_written(rules, "program_note", note, 6u, &scratch, &text.line) : 0;
    platform_text_ended(&text);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  sass.krs: a lane through word_div, open, is ");
        mmgr_line_put(output_line, (open_written != 0) ? "written" : "refused");
        mmgr_line_put(output_line, "; through word_add, given, is ");
        mmgr_line_put(output_line, (given_written != 0) ? "written" : "refused");
        mmgr_line_put(output_line, "; through program_note, the lane's structure, is ");
        mmgr_line_put(output_line, (structure_written != 0) ? "written" : "refused");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    s_failed += ((open_written != 0) || (given_written == 0) || (structure_written == 0)) ? 1u : 0u;
}

// the map from `from` to `to`, each name `from` gives against the same name in `to`: every relation written
static void map_check(const char *label, const LanguageSet *from, const LanguageSet *to)
{
    s_checks += 1u;
    const unsigned int room = (3u * from->relations.entry_count) + 1u;
    RulesetCoreRelation *const maps = (RulesetCoreRelation *)platform_cell_cleared(room, sizeof(RulesetCoreRelation),
                                                                                   platform_place_g_program_owner, 0);
    const unsigned int count = (maps != NULL) ? ruleset_core_map(&from->relations, &to->relations, maps, room) : 0u;
    s_failed += (maps == NULL) ? 1u : 0u;
    unsigned int open = 0u;
    for (unsigned int at = 0u; (at < count) && (at < room); at += 1u)
    {
        const RulesetCoreRelation *const map = &maps[at];
        open += RULESET_CORE_MAP_OPEN(map->type) ? 1u : 0u;
        mmgr_line_put(platform_line_out(), RULESET_CORE_MAP_OPEN(map->type) ? "    open: " : "    ");
        platform_line_out_ended();
        entry_print(from, map->entry);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, ruleset_map_text(map->type));
            platform_line_out_ended();
        }
        if ((map->type == RULESET_CORE_MAP_BREAKS) || (map->type == RULESET_CORE_MAP_COLLAPSES))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                platform_line_out_ended();
            }
            entry_print(from, map->other);
        }
        else if (map->type == RULESET_CORE_MAP_TYPE)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                platform_line_out_ended();
            }
            entry_print(to, map->other);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, label);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, count - open, 1u);
        mmgr_line_put(output_line, " relations of the map witnessed, ");
        mmgr_line_decimal(output_line, open, 1u);
        mmgr_line_put(output_line, " open");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(maps, platform_place_g_program_owner);
}

int program_main(int argc, char **argv)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "ruleset read test");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (argc > 1)
    {
        // the paths given are far fewer than 2^32
        const unsigned int given_count = (unsigned int)(argc - 1);
        char (*const paths)[PLATFORM_PATH_BYTES] = (char (*)[PLATFORM_PATH_BYTES])platform_cell_cleared(
            given_count, PLATFORM_PATH_BYTES, platform_place_g_program_owner, 0);
        for (unsigned int at = 0u; (paths != NULL) && (at < given_count); at += 1u)
        {
            (void)mmgr_text_copy(paths[at], argv[at + 1], PLATFORM_PATH_BYTES);
        }
        LanguageSet given;
        s_failed += (paths == NULL) ? 1u : 0u;
        if (paths != NULL)
        {
            relations_check("the files given", paths, given_count, NULL, NULL, &given);
        }
        platform_cell_released(paths, platform_place_g_program_owner);
    }
    else
    {
        LanguageSet sets[5];
        target_check(code_generator("c.krs"), &sets[0]);
        const int cu = target_check(code_generator("cu.krs"), &sets[1]);
        target_check(code_generator("ptx.krs"), &sets[2]);
        target_check(code_generator("vhdl.krs"), &sets[3]);
        const int sass = target_check(code_generator("sass.krs"), &sets[4]);
        open_check(code_generator("sass.krs"));
        if ((cu != 0) && (sass != 0))
        {
            map_check("cu.krs to sass.krs", &sets[1], &sets[4]);
            map_check("sass.krs to cu.krs", &sets[4], &sets[1]);
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "ruleset read test: ");
        mmgr_line_decimal(output_line, s_checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, s_failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (s_failed == 0u) ? 0 : 1;
}
