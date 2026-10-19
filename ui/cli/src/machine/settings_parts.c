// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_parts.c: the parts of a settings file: its own settings, its system's, and on the reader's own
// settings.json its machine's, each with where it stands; and a kind of file's settings, each set aside
#include "settings_internal.h"

// the parts, their values and the kinds' settings, as settings_layers.c keeps them under its lock
static CliPart *s_parts_of;
static CliPartValue *s_values_of;
static CliKindSet *s_kinds_of;

void cli_settings_parts_kept(CliPart *parts, CliPartValue *values, CliKindSet *kinds)
{
    s_parts_of = parts;
    s_values_of = values;
    s_kinds_of = kinds;
}

// a problem of a file put after the layers', its file first
static void cli_file_problem(CliLayers *layers, const char *file, const char *first, const char *second,
                             const char *third)
{
    char problem[CLI_PATH_LONGEST * 2u];
    CliLine line = {problem, sizeof(problem), 0ull};
    cli_line_text(&line, file);
    cli_line_text(&line, first);
    cli_line_text(&line, second);
    cli_line_text(&line, third);
    cli_line_closed(&line);
    cli_settings_problem(layers, problem);
}

// a part of a file put after the parts, its values from `first`
static unsigned int cli_part_put(unsigned int parts, const CliChain *chain, unsigned int file, const char *part,
                                 unsigned int first, unsigned int values, unsigned int rank)
{
    if (parts < CLI_LAYER_PARTS)
    {
        s_parts_of[parts] = (CliPart){chain, file, part, first, values - first, rank};
        return parts + 1u;
    }
    return parts;
}

// a value a part sets put after the values: the count after
static unsigned int cli_value_put(unsigned int values, const char *key, unsigned int value)
{
    if (values < CLI_LAYER_RAW)
    {
        s_values_of[values] = (CliPartValue){key, value};
        return values + 1u;
    }
    return values;
}

// the parts of one file put after the parts: its own settings, its system's, and on the reader's own settings.json its
// machine's; a kind's settings put after the kinds
unsigned int cli_settings_file_parts(CliLayers *layers, const CliChain *chain, unsigned int file, int of_tree, int own,
                                     unsigned int parts, unsigned int *values, unsigned int *kinds)
{
    const CliJson *const json = &chain->json;
    const char *const name = chain->file[file];
    const unsigned int object = chain->values[file];
    const unsigned int first = *values;
    for (unsigned int member = json->node[object].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        const char *const key = json->text + json->node[member].key;
        const char *kind = NULL;
        unsigned long long length = 0ull;
        if (cli_settings_kind_of(key, &kind, &length))
        {
            if (cli_json_is(json, member, CFG_JSON_OBJECT) && (*kinds < CLI_LAYER_PARTS))
            {
                s_kinds_of[*kinds] = (CliKindSet){chain, file, key, member};
                *kinds += 1u;
            }
            else if (!cli_json_is(json, member, CFG_JSON_OBJECT))
            {
                cli_file_problem(layers, name, ": ", key, " is an object of settings");
            }
        }
        else if (of_tree && cli_settings_held(key))
        {
            cli_file_problem(layers, name, ": ", key, " is the reader's own to set, and a tree's file does not set it");
        }
        else if (!cli_settings_is_file_key(key))
        {
            *values = cli_value_put(*values, key, member);
        }
    }
    const unsigned int rank = of_tree ? CLI_RANK_OVER : (own ? CLI_RANK_OWN : CLI_RANK_UNDER);
    parts = cli_part_put(parts, chain, file, NULL, first, *values, rank);
    const unsigned int set = cli_json_member(json, object, layers->system);
    if (cli_json_is(json, set, CFG_JSON_OBJECT))
    {
        const unsigned int system_first = *values;
        for (unsigned int member = json->node[set].first; member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            if (cli_settings_is_file_key(key))
            {
                char said[CLI_NAME_LONGEST * 2u];
                CliLine line = {said, sizeof(said), 0ull};
                cli_line_text(&line, ": ");
                cli_line_text(&line, layers->system);
                cli_line_text(&line, " sets ");
                cli_line_closed(&line);
                cli_file_problem(layers, name, said, key, ", which a file sets outside a system's part");
            }
            else if (of_tree && cli_settings_held(key))
            {
                cli_file_problem(layers, name, ": ", key,
                                 " is the reader's own to set, and a tree's file does not set it");
            }
            else
            {
                *values = cli_value_put(*values, key, member);
            }
        }
        parts = cli_part_put(parts, chain, file, layers->system, system_first, *values,
                             (rank == CLI_RANK_UNDER) ? CLI_RANK_UNDER : CLI_RANK_OVER);
    }
    else if (set != CLI_JSON_NONE)
    {
        cli_file_problem(layers, name, ": ", layers->system, " is an object of settings");
    }
    const unsigned int machine = cli_json_member(json, object, "machine");
    if (own && cli_json_is(json, machine, CFG_JSON_OBJECT))
    {
        const unsigned int machine_first = *values;
        for (unsigned int member = json->node[machine].first; member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            if (!cli_settings_is_file_key(key))
            {
                *values = cli_value_put(*values, key, member);
            }
        }
        parts = cli_part_put(parts, chain, file, "machine", machine_first, *values, CLI_RANK_OVER);
    }
    return parts;
}
