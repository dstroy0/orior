// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_parts.c: the parts of a settings file: its own settings, its system's, and on the reader's own
// settings.json its machine's, each with where it stands; and a type of file's settings, each set aside
#include "settings_internal.h"

// the parts, their values and the types' settings, as settings_layers.c stores them under its lock
static CliPart *s_parts_of;
static CliPartValue *s_values_of;
static CliTypeSet *s_types_of;

void cli_settings_parts_store(const CliSettingsPartsCtx *args)
{
    s_parts_of = args->parts;
    s_values_of = args->values;
    s_types_of = args->types;
}

typedef struct
{
    CliLayers *layers;
    const char *file;
    const char *first;
    const char *second;
    const char *third;
    unsigned int parts;
    const CliChain *chain;
    unsigned int file_u32;
    const char *part;
    unsigned int first_u32;
    unsigned int values;
    unsigned int rank;
    const char *key;
    unsigned int value;
} SettingsPartsCtx;
// a problem of a file put after the layers', its file first
EMBED_INLINE void cli_file_problem(const SettingsPartsCtx *args)
{
    CliLayers *layers = args->layers;
    char problem[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {problem, sizeof(problem), 0ULL};
    mmgr_line_put(&line, args->file);
    mmgr_line_put(&line, args->first);
    mmgr_line_put(&line, args->second);
    mmgr_line_put(&line, args->third);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx, .layers = layers,
               .problem = problem);
}

// a part of a file put after the parts, its values from `first`
EMBED_INLINE unsigned int cli_part_append(const SettingsPartsCtx *args)
{
    if (args->parts < CLI_LAYER_PARTS)
    {
        s_parts_of[args->parts] = (CliPart){
            args->chain, args->file_u32, args->part, args->first_u32, args->values - args->first_u32, args->rank};
        return args->parts + 1u;
    }
    return args->parts;
}

// a value a part sets put after the values: the count after
EMBED_INLINE unsigned int cli_value_append(const SettingsPartsCtx *args)
{
    if (args->values < CLI_LAYER_RAW)
    {
        s_values_of[args->values] = (CliPartValue){args->key, args->value};
        return args->values + 1u;
    }
    return args->values;
}

// the parts of one file put after the parts: its own settings, its system's, and on the reader's own settings.json its
// machine's; a type's settings put after the types
unsigned int cli_settings_file_parts(const CliSettingsLayersCtx *args)
{
    unsigned int parts = args->parts_u32;
    unsigned int *values = args->values_u32s;
    unsigned int *types = args->types_u32s;
    const CliJson *const json = &args->chain->json;
    const char *const name = args->chain->file[args->file];
    const unsigned int object = args->chain->values[args->file];
    const unsigned int first = *values;
    for (unsigned int member = json->node[object].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        const char *const key = json->text + json->node[member].key;
        const char *type = NULL;
        unsigned long long length = 0ULL;
        if (EMBED_CALL(cli_settings_key_internal.type_get, CliSettingsKeyCtx, .key = key, .type = &type,
                       .length_pointer = &length))
        {
            if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = member,
                           .type = CFG_JSON_OBJECT) &&
                (*types < CLI_LAYER_PARTS))
            {
                s_types_of[*types] = (CliTypeSet){args->chain, args->file, key, member};
                *types += 1u;
            }
            else if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                 .node = member, .type = CFG_JSON_OBJECT))
            {
                EMBED_CALL(cli_file_problem, SettingsPartsCtx, .layers = args->layers, .file = name, .first = ": ",
                           .second = key, .third = " is an object of settings");
            }
        }
        else if (args->of_tree && EMBED_CALL(cli_settings_key_internal.tree_scope, CliSettingsKeyCtx, .key = key))
        {
            EMBED_CALL(cli_file_problem, SettingsPartsCtx, .layers = args->layers, .file = name, .first = ": ",
                       .second = key, .third = " is the reader's own to set, and a tree's file does not set it");
        }
        else if (!EMBED_CALL(cli_settings_key.is_file_key, CliSettingsKeyCfg, .key = key))
        {
            *values = EMBED_CALL(cli_value_append, SettingsPartsCtx, .values = *values, .key = key, .value = member);
        }
    }
    unsigned int rank;
    if (args->of_tree)
    {
        rank = CLI_RANK_OVER;
    }
    else if (args->own)
    {
        rank = CLI_RANK_OWN;
    }
    else
    {
        rank = CLI_RANK_UNDER;
    }
    parts = EMBED_CALL(cli_part_append, SettingsPartsCtx, .parts = parts, .chain = args->chain, .file_u32 = args->file,
                       .part = NULL, .first_u32 = first, .values = *values, .rank = rank);
    const unsigned int set = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = object, .name = args->layers->system);
    if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = set,
                   .type = CFG_JSON_OBJECT))
    {
        const unsigned int system_first = *values;
        for (unsigned int member = json->node[set].first; member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            if (EMBED_CALL(cli_settings_key.is_file_key, CliSettingsKeyCfg, .key = key))
            {
                char msg_buf[CLI_NAME_MAX_LEN * 2u];
                VerbaLinea line = {msg_buf, sizeof(msg_buf), 0ULL};
                mmgr_line_put(&line, ": ");
                mmgr_line_put(&line, args->layers->system);
                mmgr_line_put(&line, " sets ");
                mmgr_line_finish(&line);
                EMBED_CALL(cli_file_problem, SettingsPartsCtx, .layers = args->layers, .file = name, .first = msg_buf,
                           .second = key, .third = ", which a file sets outside a system's part");
            }
            else if (args->of_tree && EMBED_CALL(cli_settings_key_internal.tree_scope, CliSettingsKeyCtx, .key = key))
            {
                EMBED_CALL(cli_file_problem, SettingsPartsCtx, .layers = args->layers, .file = name, .first = ": ",
                           .second = key, .third = " is the reader's own to set, and a tree's file does not set it");
            }
            else
            {
                *values =
                    EMBED_CALL(cli_value_append, SettingsPartsCtx, .values = *values, .key = key, .value = member);
            }
        }
        parts = EMBED_CALL(cli_part_append, SettingsPartsCtx, .parts = parts, .chain = args->chain,
                           .file_u32 = args->file, .part = args->layers->system, .first_u32 = system_first,
                           .values = *values, .rank = (rank == CLI_RANK_UNDER) ? CLI_RANK_UNDER : CLI_RANK_OVER);
    }
    else if (set != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_file_problem, SettingsPartsCtx, .layers = args->layers, .file = name, .first = ": ",
                   .second = args->layers->system, .third = " is an object of settings");
    }
    const unsigned int machine = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = object, .name = "machine");
    if (args->own && EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                .node = machine, .type = CFG_JSON_OBJECT))
    {
        const unsigned int machine_first = *values;
        for (unsigned int member = json->node[machine].first; member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            if (!EMBED_CALL(cli_settings_key.is_file_key, CliSettingsKeyCfg, .key = key))
            {
                *values =
                    EMBED_CALL(cli_value_append, SettingsPartsCtx, .values = *values, .key = key, .value = member);
            }
        }
        parts =
            EMBED_CALL(cli_part_append, SettingsPartsCtx, .parts = parts, .chain = args->chain, .file_u32 = args->file,
                       .part = "machine", .first_u32 = machine_first, .values = *values, .rank = CLI_RANK_OVER);
    }
    return parts;
}
