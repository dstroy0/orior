// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_tool.c: the tool plugins, those that come with orior and the reader's, each read from its plugin.json as
// serde reads a Tool: a plugin of kind "tool" that is not one is passed over, and of two of one id the later stands
#include "plugins.h"
#include "validate_internal.h"

#include "../text/text.h"

// the most tool plugins looked over
#define CLI_VALIDATE_TOOLS (CLI_USER_PLUGINS + 256u)

int cli_validate_in_array(const CliValidateJsonCtx *args)
{
    for (unsigned int index = 0u;
         EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                    .array = args->array, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        unsigned long long size = 0ULL;
        const char *const one =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = args->json, .array = args->array, .index = index),
                       .length_pointer = &size);
        if ((one != NULL) && (size == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = one, .other = args->text, .bytes = args->length}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

int cli_validate_contains(const CliValidateTextLengthCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->part, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ULL; at + size <= args->length; at += 1ULL)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = args->part, .bytes = size}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    const CliJson *json;
    unsigned int object;
    const char *name;
    int needed;
    const char **value;
    unsigned int *array_pointer;
    const unsigned long long *fallback;
    unsigned long long most;
    unsigned long long *value_u64s;
    unsigned int array;
    CliTool *tool;
    unsigned int audit;
    unsigned int root;
    const char *language;
} ValidateToolCtx;
// the string member `name` of `object` into `value`: 1, or 0 where it is no string, or absent and `needed`; "" where
// it is absent and not needed
EMBED_INLINE int tool_string(const ValidateToolCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    const char **value = args->value;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = object, .name = name);
    *value = (member == CLI_JSON_NONE) ? ""
                                       : EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                    .json_read = json, .node = member, .length_pointer = NULL);
    return (member == CLI_JSON_NONE) ? !args->needed : (*value != NULL);
}

// the array of strings `name` of `object` into `array`: 1, or 0 where it is no array of strings, or absent and
// `needed`; CLI_JSON_NONE, an array with none, where it is absent and not needed
EMBED_INLINE int tool_strings(const ValidateToolCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    unsigned int *array = args->array_pointer;
    *array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = object,
                        .name = name);
    if (*array == CLI_JSON_NONE)
    {
        return !args->needed;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = *array,
                    .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = *array, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                        .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = *array, .index = index),
                        .kind = CFG_JSON_STRING))
        {
            return 0;
        }
    }
    return 1;
}

// the whole number `name` of `object`, no greater than `most`, into `value`: 1, or 0 where it is none, as serde reads
// an unsigned integer, or absent and no `fallback` is given; `*fallback` where it is absent
EMBED_INLINE int tool_count(const ValidateToolCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    const char *name = args->name;
    unsigned long long *value = args->value_u64s;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = object, .name = name);
    if (member == CLI_JSON_NONE)
    {
        *value = (args->fallback != NULL) ? *args->fallback : 0ULL;
        return args->fallback != NULL;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = member,
                    .kind = CFG_JSON_NUMBER))
    {
        return 0;
    }
    const char *const text = json->text + json->node[member].text;
    const unsigned long long length = json->node[member].length;
    unsigned long long sum = 0ULL;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return 0;
        }
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        if (sum > (args->most - digit) / 10ULL)
        {
            return 0;
        }
        sum = (sum * 10ULL) + digit;
    }
    *value = sum;
    return length > 0ULL;
}

// the barriers of a tool read: 1, or 0 where one is not a Barrier
EMBED_INLINE int tool_barriers(const ValidateToolCtx *args)
{
    const CliJson *json = args->json;
    unsigned int array = args->array;
    args->tool->barrier_count = 0u;
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = array, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = array, .index = index);
        if ((args->tool->barrier_count == CLI_VALIDATE_BARRIERS) ||
            !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = one,
                        .kind = CFG_JSON_OBJECT))
        {
            return 0;
        }
        CliBarrier *const barrier = &args->tool->barriers[args->tool->barrier_count];
        if (!EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = one, .name = "name", .needed = 1,
                        .value = &barrier->name) ||
            !EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = one, .name = "about", .needed = 0,
                        .value = &barrier->about) ||
            !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = one, .name = "kinds", .needed = 0,
                        .array_pointer = &barrier->kinds) ||
            !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = one, .name = "phrases", .needed = 0,
                        .array_pointer = &barrier->phrases) ||
            !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = one, .name = "not", .needed = 0,
                        .array_pointer = &barrier->unless) ||
            !EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = one, .name = "lift", .needed = 1,
                        .value = &barrier->lift) ||
            !EMBED_CALL(tool_count, ValidateToolCtx, .json = json, .object = one, .name = "default", .fallback = NULL,
                        .most = ~0ULL, .value_u64s = &barrier->start) ||
            !EMBED_CALL(tool_count, ValidateToolCtx, .json = json, .object = one, .name = "grow", .fallback = NULL,
                        .most = ~0ULL, .value_u64s = &barrier->grow) ||
            !EMBED_CALL(tool_count, ValidateToolCtx, .json = json, .object = one, .name = "ceiling", .fallback = NULL,
                        .most = ~0ULL, .value_u64s = &barrier->ceiling))
        {
            return 0;
        }
        args->tool->barrier_count += 1u;
    }
    return 1;
}

// the flags of a tool read: 1, or 0 where one is not a Flag
EMBED_INLINE int tool_flags(const ValidateToolCtx *args)
{
    const CliJson *json = args->json;
    unsigned int array = args->array;
    args->tool->flag_count = 0u;
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = array, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = array, .index = index);
        CliFlag *const flag = &args->tool->flags[args->tool->flag_count];
        unsigned long long severity = 0ULL;
        if ((args->tool->flag_count == CLI_VALIDATE_FLAGS) ||
            !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = one,
                        .kind = CFG_JSON_OBJECT) ||
            !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = one, .name = "words", .needed = 1,
                        .array_pointer = &flag->words) ||
            !EMBED_CALL(tool_count, ValidateToolCtx, .json = json, .object = one, .name = "severity", .fallback = NULL,
                        .most = 255ULL, .value_u64s = &severity) ||
            !EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = one, .name = "message", .needed = 1,
                        .value = &flag->phrases))
        {
            return 0;
        }
        flag->severity = (unsigned int)severity;
        args->tool->flag_count += 1u;
    }
    return 1;
}

// a tool's audit read, none where it is absent or null: 1, or 0 where it is not an Audit
EMBED_INLINE int tool_audit(const ValidateToolCtx *args)
{
    const CliJson *json = args->json;
    args->tool->audited =
        (args->audit != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                      .json_read = json, .node = args->audit, .kind = CFG_JSON_NULL);
    args->tool->declarations = CLI_JSON_NONE;
    args->tool->trusted = CLI_JSON_NONE;
    args->tool->gaps = CLI_JSON_NONE;
    args->tool->explains = CLI_JSON_NONE;
    args->tool->repeats = CLI_JSON_NONE;
    args->tool->print = "";
    if (!args->tool->audited)
    {
        return 1;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->audit,
                    .kind = CFG_JSON_OBJECT) ||
        !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = args->audit, .name = "declarations",
                    .needed = 1, .array_pointer = &args->tool->declarations) ||
        !EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = args->audit, .name = "print", .needed = 1,
                    .value = &args->tool->print) ||
        !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = args->audit, .name = "trusted", .needed = 1,
                    .array_pointer = &args->tool->trusted) ||
        !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = args->audit, .name = "gaps", .needed = 0,
                    .array_pointer = &args->tool->gaps) ||
        !EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = args->audit, .name = "repeats", .needed = 0,
                    .array_pointer = &args->tool->repeats))
    {
        return 0;
    }
    args->tool->explains = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->audit, .name = "explains");
    if (args->tool->explains == CLI_JSON_NONE)
    {
        return 1;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                    .node = args->tool->explains, .kind = CFG_JSON_OBJECT))
    {
        return 0;
    }
    for (unsigned int member = json->node[args->tool->explains].first; member != CLI_JSON_NONE;
         member = json->node[member].next)
    {
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = member,
                        .kind = CFG_JSON_STRING))
        {
            return 0;
        }
    }
    // an explanation is found by its key in the order a BTreeMap orders them
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = (CliJson *)json,
               .object = args->tool->explains);
    return 1;
}

// the tool `root` declares read into `tool`: 1, or 0 where it is no tool plugin, or not a Tool as serde reads one
EMBED_INLINE int tool_read(const ValidateToolCtx *args)
{
    const CliJson *json = args->json;
    const char *kind = NULL;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->root,
                    .kind = CFG_JSON_OBJECT) ||
        !EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = args->root, .name = "kind", .needed = 1,
                    .value = &kind) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "tool", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    const unsigned int run = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->root, .name = "run");
    const unsigned int barriers = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = args->root, .name = "barriers");
    const unsigned int flags = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "flags");
    static const unsigned long long s_patience = 600ULL;
    return EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = args->root, .name = "id", .needed = 1,
                      .value = &args->tool->id) &&
           EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = args->root, .name = "name", .needed = 1,
                      .value = &args->tool->name) &&
           EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = args->root, .name = "languages",
                      .needed = 0, .array_pointer = &args->tool->languages) &&
           EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = args->root, .name = "toolchain",
                      .needed = 1, .value = &args->tool->toolchain) &&
           EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = run,
                      .kind = CFG_JSON_OBJECT) &&
           EMBED_CALL(tool_string, ValidateToolCtx, .json = json, .object = run, .name = "program", .needed = 1,
                      .value = &args->tool->program) &&
           EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = run, .name = "args", .needed = 1,
                      .array_pointer = &args->tool->args) &&
           EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = run, .name = "project", .needed = 0,
                      .array_pointer = &args->tool->project) &&
           EMBED_CALL(tool_strings, ValidateToolCtx, .json = json, .object = run, .name = "inProject", .needed = 0,
                      .array_pointer = &args->tool->in_project) &&
           EMBED_CALL(tool_count, ValidateToolCtx, .json = json, .object = run, .name = "patience",
                      .fallback = &s_patience, .most = ~0ULL, .value_u64s = &args->tool->patience) &&
           ((barriers == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                      .json_read = json, .node = barriers, .kind = CFG_JSON_ARRAY)) &&
           EMBED_CALL(tool_barriers, ValidateToolCtx, .json = json, .array = barriers, .tool = args->tool) &&
           ((flags == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                   .json_read = json, .node = flags, .kind = CFG_JSON_ARRAY)) &&
           EMBED_CALL(tool_flags, ValidateToolCtx, .json = json, .array = flags, .tool = args->tool) &&
           EMBED_CALL(tool_audit, ValidateToolCtx, .json = json,
                      .audit = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->root, .name = "audit"),
                      .tool = args->tool);
}

// What a look over the tool plugins finds: each tool by its id, the last plugin of an id standing for it, with its
// place among the plugins and whether it checks the language sought; and, as one is read again, the place sought and
// whether it was read
typedef struct
{
    const char *language;
    char ids[CLI_VALIDATE_TOOLS][CLI_NAME_MAX_LEN];
    unsigned int positions[CLI_VALIDATE_TOOLS];
    int checks[CLI_VALIDATE_TOOLS];
    unsigned int count;
    unsigned int seen;
    int loading;
    unsigned int needle;
    int loaded;
} CliToolLook;

static CliToolLook s_look;

// a plugin looked at: a tool recorded by its id, or, as one is read again, the tool at the place sought read into the
// capacity
static void tool_look(const CliEachPluginCfg *args)
{
    CliToolLook *const look = (CliToolLook *)args->context;
    const unsigned int place = look->seen;
    look->seen += 1u;
    if (look->loading && (place != look->needle))
    {
        return;
    }
    // the plugin's tree in a cell of its own sized to its text, the one before given back, retained until the capacity
    // is
    CliJson *const json = &g_cli_validate->json;
    g_cli_cells->persistent_buf_release(json->node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = args->plugin->text,
                   .size = args->plugin->size)
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = json,
                         .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                         .err_msg = &unread, .owner = &g_cli_plugins_cells)
            : CLI_JSON_NONE;
    CliTool *const tool = &g_cli_validate->tool;
    if ((root == CLI_JSON_NONE) || !EMBED_CALL(tool_read, ValidateToolCtx, .json = json, .root = root, .tool = tool))
    {
        return;
    }
    if (look->loading)
    {
        look->loaded = 1;
        return;
    }
    unsigned int at = 0u;
    while ((at < look->count) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = look->ids[at], .other = tool->id, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if ((at == look->count) &&
        ((look->count == CLI_VALIDATE_TOOLS) ||
         (cellul.len(&(CatenaFinitaCfg){.src = tool->id, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN)))
    {
        return;
    }
    if (at == look->count)
    {
        memor.cpy(&(MemoriaCfg){.dst = look->ids[at],
                                .src = tool->id,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = tool->id, .cap = CLI_CELLS_POOL}) + 1u});
        look->count += 1u;
    }
    look->positions[at] = place;
    look->checks[at] =
        (look->language != NULL) &&
        EMBED_CALL(cli_validate_json_internal.in_array, CliValidateJsonCtx, .json = json, .array = tool->languages,
                   .text = look->language,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = look->language, .cap = CLI_CELLS_POOL}));
}

// every tool plugin looked over, each id's last recorded, under the look's lock, which the caller gives back
EMBED_INLINE void tool_looked_over(const ValidateToolCtx *args)
{
    const char *language = args->language;
    s_look.language = language;
    s_look.count = 0u;
    s_look.seen = 0u;
    s_look.loading = 0;
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = tool_look, .context = &s_look);
}

int cli_validate_tool_for(const CliValidateCfg *args)
{
    // each plugin is read into the validation's capacity, retained for the look as a validation retains it
    if (!cli_validate_lock())
    {
        return 0;
    }
    EMBED_CALL(tool_looked_over, ValidateToolCtx, .language = args->language);
    const char *min = NULL;
    for (unsigned int at = 0u; at < s_look.count; at += 1u)
    {
        if (s_look.checks[at] && ((min == NULL) || (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                               .one = s_look.ids[at], .other_pointer = min) < 0)))
        {
            min = s_look.ids[at];
        }
    }
    if (min != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->id,
                                .src = min,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = min, .cap = CLI_CELLS_POOL}) + 1u});
    }
    g_cli_cells->persistent_buf_release(g_cli_validate->json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg,
                                                                              .cell = g_cli_validate->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_validate->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release(
        g_cli_validate->manifest.node,
        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = g_cli_validate->manifest.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_validate->manifest),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release(g_cli_validate->output, &g_cli_plugins_cells);
    g_cli_cells->persistent_buf_release(g_cli_validate, &g_cli_plugins_cells);
    g_cli_validate = NULL;
    return min != NULL;
}

int cli_validate_tool_read(const CliValidateIdCtx *args)
{
    EMBED_CALL(tool_looked_over, ValidateToolCtx, .language = NULL);
    unsigned int at = 0u;
    while ((at < s_look.count) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_look.ids[at], .other = args->id, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    s_look.loaded = 0;
    if (at < s_look.count)
    {
        s_look.seen = 0u;
        s_look.loading = 1;
        s_look.needle = s_look.positions[at];
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = tool_look, .context = &s_look);
    }
    const int loaded = s_look.loaded;
    return loaded;
}
