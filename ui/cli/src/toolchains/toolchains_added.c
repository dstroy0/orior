// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_added.c: the toolchains and groups the reader added, stored in user_toolchains.json in orior's own folder,
// and the files of orior's own folder these are stored in read and written
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "toolchains_internal.h"

// The reader's file read with orior's own tools beside it, under the lock: 1, or 0 with why given where orior_cli has
// no folder of its own
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    CliJson bundled;
    CliJson json;
    unsigned int tools;
    unsigned int added;
    unsigned int groups;
    unsigned int added_tools;
} CliAdded;

// the file `name` in orior's own folder into `path`: 1, or 0 where orior_cli has no folder of its own
int cli_toolchains_file(const CliToolchainsCtx *args)
{
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = args->path))
    {
        args->path[0] = '\0';
        return 0;
    }
    VerbaLinea line = {args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}),
                       CLI_PATH_MAX_LEN - cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}),
                       0ULL};
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    return mmgr_line_finish(&line) != 0ULL;
}

unsigned int cli_toolchains_file_read(const CliToolchainsJsonCtx *args)
{
    // the empty object made where the file is none takes a node of the capacity over
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int read =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = args->json,
                   .path = args->path_read, .nodes_more = args->nodes_more + 1u, .text_more = args->text_more,
                   .err_msg = &err_msg, .owner = g_cli_toolchains_owner);
    if (args->json->node == NULL)
    {
        (void)EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = args->json,
                         .nodes_max = args->nodes_more + 1u, .text_max = args->text_more,
                         .what = "the reader's toolchains", .owner = g_cli_toolchains_owner);
    }
    return EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json, .node = read,
                      .type = CFG_JSON_OBJECT)
               ? read
               : EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = args->json,
                            .type = CFG_JSON_OBJECT);
}

int cli_toolchains_file_save(const CliToolchainsPathCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    }
    // the text handed to the file as it is written, a piece at a time
    CliFileWriting file;
    char piece[4096];
    const int begun =
        EMBED_CALL(cli_path_file_internal.file_begin, CliPathFileCtx, .file = &file, .path = args->path_read);
    VerbaLinea out = {piece, sizeof(piece), 0ULL, cli_file_write_chunk, &file, 0ULL};
    if (begun)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json_read,
                   .node = args->node, .out = &out, .indent = 2u, .sorted = args->sorted);
        mmgr_line_ch(&out, '\n');
    }
    const int written = begun && mmgr_line_flush(&out);
    const int ended = begun && EMBED_CALL(cli_file_internal.file_end, CliFileCtx, .file = &file, .whole = written);
    if (!written || !ended)
    {
        mmgr_line_put(args->err_msg, args->path_read);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

typedef struct
{
    CliJson *json;
    unsigned int added;
    unsigned int *groups;
    unsigned int *tools;
    const CliJson *json_read;
    unsigned int list;
    const char *key;
    const char *text;
    const CliJson *bundled;
    unsigned int tools_value;
    unsigned int added_tools;
    unsigned int added_groups;
    void (*each)(const CliEachGroupCfg *args);
    void *context;
    const char *name;
    char *id;
    unsigned int value;
    CliAdded *read;
    VerbaLinea *err_msg;
    unsigned int tool;
    unsigned long long capacity;
    const char *group;
    const char *uses;
    unsigned int made;
    unsigned int programs;
} ToolchainsAddedCtx;
// The reader's added groups and tools: the `groups` and `tools` arrays of their file, each made where it is not there
EMBED_INLINE void cli_added_lists(const ToolchainsAddedCtx *args)
{
    CliJson *json = args->json;
    unsigned int *groups = args->groups;
    unsigned int *tools = args->tools;
    *groups = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                         .object = args->added, .name = "groups");
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = *groups,
                    .type = CFG_JSON_ARRAY))
    {
        *groups = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_ARRAY);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->added,
                   .name = "groups", .value = *groups);
    }
    *tools = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                        .object = args->added, .name = "tools");
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = *tools,
                    .type = CFG_JSON_ARRAY))
    {
        *tools = EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = json, .type = CFG_JSON_ARRAY);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = args->added,
                   .name = "tools", .value = *tools);
    }
}

// whether a list contains a node whose `key`, or itself where `key` is NULL, is the text `text`
EMBED_INLINE int cli_list_contains(const ToolchainsAddedCtx *args)
{
    const CliJson *json = args->json_read;
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                        .node = args->list, .type = CFG_JSON_ARRAY)
                                 ? json->node[args->list].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const item_text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = (args->key != NULL) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                                .json_read = json, .object = item, .name = args->key)
                                                   : item,
                       .length_pointer = NULL);
        if ((item_text != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = item_text, .other = args->text, .cap = CLI_POOL_BYTES})))
        {
            return 1;
        }
    }
    return 0;
}

// orior's own tools, read whole into a tree sized to them, which persistent_buf_release releases: the array of them
EMBED_INLINE unsigned int cli_on_bundle(const ToolchainsAddedCtx *args)
{
    CliJson *json = args->json;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    // the program's own bytes are toolchains.json's text
    const unsigned int read =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = json,
                   .text = (const char *)g_cli_toolchains_json, .size = g_cli_toolchains_json_size, .nodes_more = 0u,
                   .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_toolchains_owner);
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = read,
                      .name = "tools");
}

// each group handed on once, in order: those of orior's tools, of the reader's tools, then the reader's own
EMBED_INLINE void cli_groups_get(const ToolchainsAddedCtx *args)
{
    const CliJson *json = args->json_read;
    static char s_seen[512][CLI_NAME_MAX_LEN];
    unsigned int seen = 0u;
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        const CliJson *const in = (list == 0u) ? args->bundled : json;
        unsigned int array;
        if (list == 0u)
        {
            array = args->tools_value;
        }
        else if (list == 1u)
        {
            array = args->added_tools;
        }
        else
        {
            array = args->added_groups;
        }
        for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = in,
                                            .node = array, .type = CFG_JSON_ARRAY)
                                     ? in->node[array].first
                                     : CLI_JSON_NONE;
             item != CLI_JSON_NONE; item = in->node[item].next)
        {
            const char *const group =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = in,
                           .node = (list == 2u) ? item
                                                : EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                             .json_read = in, .object = item, .name = "group"),
                           .length_pointer = NULL);
            int known = group == NULL;
            unsigned int at = 0u;
            while (!known && (at < seen))
            {
                known = cellul.eq(&(CatenaFinitaCfg){.src = s_seen[at], .other = group, .cap = CLI_POOL_BYTES});
                at += 1u;
            }
            if (!known && (seen < 512u))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = s_seen[seen],
                                  .src = group,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_POOL_BYTES}) + 1u});
                seen += 1u;
                EMBED_CALL(args->each, CliEachGroupCfg, .context = args->context, .group = group);
            }
        }
    }
}

void cli_toolchains_groups(const CliToolchainsEachCfg *args)
{
    void (*each)(const CliEachGroupCfg *) = args->each;
    void *context = args->context;
    CliJson bundled;
    CliJson json;
    char path[CLI_PATH_MAX_LEN];
    const unsigned int tools = EMBED_CALL(cli_on_bundle, ToolchainsAddedCtx, .json = &bundled);
    // the two lists made where the file has none take two nodes and their keys
    const unsigned int added = EMBED_CALL(cli_toolchains_json_internal.file_read, CliToolchainsJsonCtx, .json = &json,
                                          .path_read = EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx,
                                                                  .name = "user_toolchains.json", .path = path)
                                                           ? path
                                                           : "",
                                          .nodes_more = 2u, .text_more = 16ULL);
    unsigned int groups = CLI_JSON_NONE;
    unsigned int added_tools = CLI_JSON_NONE;
    EMBED_CALL(cli_added_lists, ToolchainsAddedCtx, .json = &json, .added = added, .groups = &groups,
               .tools = &added_tools);
    EMBED_CALL(cli_groups_get, ToolchainsAddedCtx, .bundled = &bundled, .tools_value = tools, .json_read = &json,
               .added_tools = added_tools, .added_groups = groups, .each = each, .context = context);
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(bundled.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), bundled.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&bundled),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

// whether a group is among those listed: retained while each is handed
typedef struct
{
    const char *group;
    int found;
} CliGroupFound;

static void cli_group_find(const CliEachGroupCfg *args)
{
    CliGroupFound *const found = (CliGroupFound *)args->context;
    found->found = found->found ||
                   (cellul.eq(&(CatenaFinitaCfg){.src = found->group, .other = args->group, .cap = CLI_POOL_BYTES}));
}

// a text trimmed of ASCII's spaces at each end into `out`, sized for `capacity`
void cli_toolchains_trim(const CliToolchainsTextCtx *args)
{
    const char *from = (args->text != NULL) ? args->text : "";
    const char *to = from + cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES});
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
    mmgr_line_finish(&line);
}

// An id made from a name: its letters and digits in lower case, each run of anything else a dash
EMBED_INLINE void cli_id_get(const ToolchainsAddedCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    for (unsigned long long at = 0ULL; (at < whole) && ((length + 1ULL) < CLI_NAME_MAX_LEN);)
    {
        const unsigned long long stop =
            EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->name, .at = at,
                       .size = whole, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, .set = "");
        const unsigned long long fits = CLI_NAME_MAX_LEN - 1ULL - length;
        const unsigned long long take = ((stop - at) < fits) ? (stop - at) : fits;
        memor.cpy(&(MemoriaCfg){.dst = args->id + length, .src = args->name + at, .bytes = (size_t)take});
        EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->id + length, .size = take);
        length += take;
        if ((stop < whole) && (length != 0ULL) && (args->id[length - 1u] != '-') &&
            ((length + 1ULL) < CLI_NAME_MAX_LEN))
        {
            args->id[length] = '-';
            length += 1ULL;
        }
        at = stop + 1ULL;
    }
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->id, .at = length,
                        .set = "-");
    args->id[length] = '\0';
}

// The keys of a tool in the order its struct writes them, each written where it is given and not empty
static const char *const s_tool_keys[21] = {
    "id",       "name",   "group",  "for",     "programs", "version", "env",   "notIn",  "formats",  "runs",  "server",
    "debugger", "builds", "format", "checker", "only",     "install", "setup", "places", "contains", "needed"};

// whether a member's value is written: one that is no empty list, empty object, null or false
EMBED_INLINE int cli_write(const ToolchainsAddedCtx *args)
{
    if (args->value == CLI_JSON_NONE)
    {
        return 0;
    }
    const CfgJsonKind type = args->json_read->node[args->value].type;
    if ((type == CFG_JSON_ARRAY) || (type == CFG_JSON_OBJECT))
    {
        return args->json_read->node[args->value].count != 0u;
    }
    return (type != CFG_JSON_NULL) && (type != CFG_JSON_FALSE);
}

// the text member `key` of the tool `tool` in `json_read`, trimmed, into `id`, sized for `capacity`: empty where the
// tool has none
EMBED_INLINE void cli_member_trimmed(const ToolchainsAddedCtx *args)
{
    EMBED_CALL(cli_toolchains_text_internal.trim, CliToolchainsTextCtx,
               .text = EMBED_CALL(
                   cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json_read,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = args->json_read, .object = args->tool, .name = args->key),
                   .length_pointer = NULL),
               .out = args->id, .capacity = args->capacity);
}

// each program the tool `tool` in `json_read` names, trimmed and not empty, put on the list `programs` in `json`
EMBED_INLINE void cli_programs_read(const ToolchainsAddedCtx *args)
{
    const CliJson *const supplied = args->json_read;
    const unsigned int list_entries = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = supplied, .object = args->tool, .name = "programs");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = supplied,
                                        .node = list_entries, .type = CFG_JSON_ARRAY)
                                 ? supplied->node[list_entries].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = supplied->node[item].next)
    {
        char program[CLI_NAME_MAX_LEN];
        EMBED_CALL(cli_toolchains_text_internal.trim, CliToolchainsTextCtx,
                   .text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                      .json_read = supplied, .node = item, .length_pointer = NULL),
                   .out = program, .capacity = sizeof(program));
        if (program[0] != '\0')
        {
            EMBED_CALL(
                cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json, .array = args->programs,
                .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                    .json = args->json, .text = program,
                                    .length = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_POOL_BYTES})));
        }
    }
}

// the value of the tool's member `key` put in `json`: the `id`, `name`, `group` or `for` made here as a text, the
// list of `programs`, or else the member of the tool `tool` in `json_read` where it is written: its node, or
// CLI_JSON_NONE
EMBED_INLINE unsigned int cli_tool_value(const ToolchainsAddedCtx *args)
{
    const char *const named = args->key;
    const char *text = NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "id", .cap = CLI_POOL_BYTES}))
    {
        text = args->id;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "name", .cap = CLI_POOL_BYTES}))
    {
        text = args->name;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "group", .cap = CLI_POOL_BYTES}))
    {
        text = args->group;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "for", .cap = CLI_POOL_BYTES}))
    {
        text = args->uses;
    }
    if (text != NULL)
    {
        return EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                          .json = args->json, .text = text,
                          .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}));
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "programs", .cap = CLI_POOL_BYTES}))
    {
        return args->programs;
    }
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json_read, .object = args->tool, .name = named);
    return EMBED_CALL(cli_write, ToolchainsAddedCtx, .json_read = args->json_read, .value = member)
               ? EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .to = args->json,
                            .from = args->json_read, .node = member)
               : CLI_JSON_NONE;
}

// the tool `made` in `json` written as its struct is, each key of s_tool_keys in turn with its value where it has
// one, the first five as an empty text where they have none
EMBED_INLINE void cli_tool_written(const ToolchainsAddedCtx *args)
{
    for (unsigned int key = 0u; key < 21u; key += 1u)
    {
        const unsigned int value =
            EMBED_CALL(cli_tool_value, ToolchainsAddedCtx, .key = s_tool_keys[key], .json = args->json, .id = args->id,
                       .name = args->name, .group = args->group, .uses = args->uses, .programs = args->programs,
                       .json_read = args->json_read, .tool = args->tool);
        if ((value != CLI_JSON_NONE) || (key < 5u))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                       .object = args->made, .name = s_tool_keys[key],
                       .value = (value != CLI_JSON_NONE) ? value
                                                         : EMBED_CALL(cli_json_tree_text_length_type.json_string_init,
                                                                      CliJsonTreeTextLengthTypeCfg, .json = args->json,
                                                                      .text = "", .length = 0u));
        }
    }
}

int cli_toolchains_add(const CliToolchainsErrMsgCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = "user_toolchains.json", .path = path))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store toolchains in");
        return 0;
    }
    char name[CLI_NAME_MAX_LEN];
    char group[CLI_NAME_MAX_LEN];
    char uses[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_member_trimmed, ToolchainsAddedCtx, .json_read = args->supplied, .tool = args->tool, .key = "name",
               .id = name, .capacity = sizeof(name));
    EMBED_CALL(cli_member_trimmed, ToolchainsAddedCtx, .json_read = args->supplied, .tool = args->tool, .key = "group",
               .id = group, .capacity = sizeof(group));
    EMBED_CALL(cli_member_trimmed, ToolchainsAddedCtx, .json_read = args->supplied, .tool = args->tool, .key = "for",
               .id = uses, .capacity = sizeof(uses));
    EMBED_CALL(cli_member_trimmed, ToolchainsAddedCtx, .json_read = args->supplied, .tool = args->tool, .key = "id",
               .id = args->id_out, .capacity = CLI_NAME_MAX_LEN);
    if (args->id_out[0] == '\0')
    {
        EMBED_CALL(cli_id_get, ToolchainsAddedCtx, .name = name, .id = args->id_out);
    }
    if ((name[0] == '\0') || (args->id_out[0] == '\0'))
    {
        mmgr_line_put(args->err_msg, "a toolchain needs a name");
        return 0;
    }
    if (group[0] == '\0')
    {
        mmgr_line_put(args->err_msg, name);
        mmgr_line_put(args->err_msg, " needs a group");
        return 0;
    }
    CliJson bundled;
    CliJson json;
    const unsigned int tools = EMBED_CALL(cli_on_bundle, ToolchainsAddedCtx, .json = &bundled);
    // the tool made takes what the given one contains, a node and its key for each of its members, its programs, and
    // the two lists and the group made where the file has none
    unsigned int nodes_more = 0u;
    unsigned long long text_more = 0ULL;
    EMBED_CALL(cli_json_tree_node_nodes.json_extent, CliJsonTreeNodeNodesCfg, .json_read = args->supplied,
               .node = args->tool, .nodes_pointer = &nodes_more, .text_u64s = &text_more);
    const unsigned int added =
        EMBED_CALL(cli_toolchains_json_internal.file_read, CliToolchainsJsonCtx, .json = &json, .path_read = path,
                   .nodes_more = nodes_more + 32u, .text_more = text_more + (4ULL * CLI_PATH_MAX_LEN) + 512ULL);
    unsigned int groups = CLI_JSON_NONE;
    unsigned int added_tools = CLI_JSON_NONE;
    EMBED_CALL(cli_added_lists, ToolchainsAddedCtx, .json = &json, .added = added, .groups = &groups,
               .tools = &added_tools);
    const unsigned int made =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = &json, .type = CFG_JSON_OBJECT);
    const unsigned int programs =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = &json, .type = CFG_JSON_ARRAY);
    EMBED_CALL(cli_programs_read, ToolchainsAddedCtx, .json = &json, .json_read = args->supplied, .tool = args->tool,
               .programs = programs);
    const char *refused = NULL;
    if (json.node[programs].count == 0u)
    {
        refused = " needs the name of at least one program to look for";
    }
    else if (EMBED_CALL(cli_list_contains, ToolchainsAddedCtx, .json_read = &bundled, .list = tools, .key = "id",
                        .text = args->id_out) ||
             EMBED_CALL(cli_list_contains, ToolchainsAddedCtx, .json_read = &json, .list = added_tools, .key = "id",
                        .text = args->id_out))
    {
        refused = "";
    }
    if (refused != NULL)
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_guard->persistent_buf_release(bundled.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), bundled.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&bundled),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        mmgr_line_put(args->err_msg, (refused[0] == '\0') ? "there is a toolchain " : name);
        mmgr_line_put(args->err_msg, (refused[0] == '\0') ? args->id_out : refused);
        mmgr_line_put(args->err_msg, (refused[0] == '\0') ? " already" : "");
        return 0;
    }
    // the tool written as its struct is, the given members past the five made here copied across
    EMBED_CALL(cli_tool_written, ToolchainsAddedCtx, .json = &json, .made = made, .id = args->id_out, .name = name,
               .group = group, .uses = uses, .programs = programs, .json_read = args->supplied, .tool = args->tool);
    CliGroupFound found = {group, 0};
    EMBED_CALL(cli_groups_get, ToolchainsAddedCtx, .bundled = &bundled, .tools_value = tools, .json_read = &json,
               .added_tools = added_tools, .added_groups = groups, .each = cli_group_find, .context = &found);
    if (!found.found)
    {
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &json, .array = groups,
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = &json, .text = group,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_POOL_BYTES})));
    }
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &json, .array = added_tools,
               .value = made);
    const int saved = EMBED_CALL(cli_toolchains_path_internal.file_save, CliToolchainsPathCtx, .json_read = &json,
                                 .node = added, .path_read = path, .sorted = 0, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(bundled.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), bundled.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&bundled),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return saved;
}

EMBED_INLINE int cli_added_read(const ToolchainsAddedCtx *args)
{
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->read->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->read->bundled,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if (!EMBED_CALL(cli_toolchains_internal.file, CliToolchainsCtx, .name = "user_toolchains.json",
                    .path = args->read->path))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store toolchains in");
        return 0;
    }
    // the two lists made where the file has none, and a group put, take three nodes and their texts
    args->read->tools = EMBED_CALL(cli_on_bundle, ToolchainsAddedCtx, .json = &args->read->bundled);
    args->read->added =
        EMBED_CALL(cli_toolchains_json_internal.file_read, CliToolchainsJsonCtx, .json = &args->read->json,
                   .path_read = args->read->path, .nodes_more = 3u, .text_more = CLI_NAME_MAX_LEN + 16ULL);
    EMBED_CALL(cli_added_lists, ToolchainsAddedCtx, .json = &args->read->json, .added = args->read->added,
               .groups = &args->read->groups, .tools = &args->read->added_tools);
    return 1;
}

// each item of a list that is `text`, or whose `key` is, taken out: how many were
EMBED_INLINE unsigned int cli_alloc_out(const ToolchainsAddedCtx *args)
{
    CliJson *json = args->json;
    unsigned int reserved = 0u;
    unsigned int before = CLI_JSON_NONE;
    for (unsigned int item = json->node[args->list].first; item != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[item].next;
        const char *const item_text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = (args->key != NULL) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                                .json_read = json, .object = item, .name = args->key)
                                                   : item,
                       .length_pointer = NULL);
        if ((item_text != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = item_text, .other = args->text, .cap = CLI_POOL_BYTES})))
        {
            if (before == CLI_JSON_NONE)
            {
                json->node[args->list].first = next;
            }
            else
            {
                json->node[before].next = next;
            }
            json->node[args->list].last = (json->node[args->list].last == item) ? before : json->node[args->list].last;
            json->node[args->list].count -= 1u;
            reserved += 1u;
        }
        else
        {
            before = item;
        }
        item = next;
    }
    return reserved;
}

int cli_toolchains_add_group(const CliToolchainsErrMsgNameCfg *args)
{
    static CliAdded s_read;
    char group[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_toolchains_text_internal.trim, CliToolchainsTextCtx, .text = args->name, .out = group,
               .capacity = sizeof(group));
    if (group[0] == '\0')
    {
        mmgr_line_put(args->err_msg, "a group needs a name");
        return 0;
    }
    int done = EMBED_CALL(cli_added_read, ToolchainsAddedCtx, .read = &s_read, .err_msg = args->err_msg);
    CliGroupFound found = {group, 0};
    if (done)
    {
        EMBED_CALL(cli_groups_get, ToolchainsAddedCtx, .bundled = &s_read.bundled, .tools_value = s_read.tools,
                   .json_read = &s_read.json, .added_tools = s_read.added_tools, .added_groups = s_read.groups,
                   .each = cli_group_find, .context = &found);
    }
    if (done && found.found)
    {
        mmgr_line_put(args->err_msg, "there is a group ");
        mmgr_line_put(args->err_msg, group);
        mmgr_line_put(args->err_msg, " already");
        done = 0;
    }
    if (done)
    {
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &s_read.json, .array = s_read.groups,
                   .value = EMBED_CALL(cli_json_tree_text_length_type.json_string_init, CliJsonTreeTextLengthTypeCfg,
                                       .json = &s_read.json, .text = group,
                                       .length = cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_POOL_BYTES})));
        done = EMBED_CALL(cli_toolchains_path_internal.file_save, CliToolchainsPathCtx, .json_read = &s_read.json,
                          .node = s_read.added, .path_read = s_read.path, .sorted = 0, .err_msg = args->err_msg);
    }
    g_cli_guard->persistent_buf_release(s_read.json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_read.bundled.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.bundled.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.bundled),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return done;
}

int cli_toolchains_remove(const CliToolchainsIdErrMsgCfg *args)
{
    static CliAdded s_read;
    int done = EMBED_CALL(cli_added_read, ToolchainsAddedCtx, .read = &s_read, .err_msg = args->err_msg);
    if (done && (EMBED_CALL(cli_alloc_out, ToolchainsAddedCtx, .json = &s_read.json, .list = s_read.added_tools,
                            .key = "id", .text = args->id) == 0u))
    {
        const int own = EMBED_CALL(cli_list_contains, ToolchainsAddedCtx, .json_read = &s_read.bundled,
                                   .list = s_read.tools, .key = "id", .text = args->id);
        mmgr_line_put(args->err_msg, own ? args->id : "there is no toolchain ");
        mmgr_line_put(args->err_msg, own ? " comes with orior: only a toolchain you added can be taken out" : args->id);
        done = 0;
    }
    if (done)
    {
        done = EMBED_CALL(cli_toolchains_path_internal.file_save, CliToolchainsPathCtx, .json_read = &s_read.json,
                          .node = s_read.added, .path_read = s_read.path, .sorted = 0, .err_msg = args->err_msg);
    }
    g_cli_guard->persistent_buf_release(s_read.json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_read.bundled.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.bundled.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.bundled),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return done && cli_toolchains_id_err_msg.forget(args);
}

int cli_toolchains_remove_group(const CliToolchainsErrMsgNameCfg *args)
{
    static CliAdded s_read;
    int done = EMBED_CALL(cli_added_read, ToolchainsAddedCtx, .read = &s_read, .err_msg = args->err_msg);
    if (done && (EMBED_CALL(cli_list_contains, ToolchainsAddedCtx, .json_read = &s_read.bundled, .list = s_read.tools,
                            .key = "group", .text = args->name) ||
                 EMBED_CALL(cli_list_contains, ToolchainsAddedCtx, .json_read = &s_read.json,
                            .list = s_read.added_tools, .key = "group", .text = args->name)))
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " contains toolchains: take them out first, or it is one of orior's own");
        done = 0;
    }
    if (done && (EMBED_CALL(cli_alloc_out, ToolchainsAddedCtx, .json = &s_read.json, .list = s_read.groups, .key = NULL,
                            .text = args->name) == 0u))
    {
        mmgr_line_put(args->err_msg, "there is no group ");
        mmgr_line_put(args->err_msg, args->name);
        done = 0;
    }
    if (done)
    {
        done = EMBED_CALL(cli_toolchains_path_internal.file_save, CliToolchainsPathCtx, .json_read = &s_read.json,
                          .node = s_read.added, .path_read = s_read.path, .sorted = 0, .err_msg = args->err_msg);
    }
    g_cli_guard->persistent_buf_release(s_read.json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_read.bundled.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.bundled.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.bundled),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return done;
}

int cli_toolchains_on_add(const CliToolchainsIdFolderCfg *args)
{
    static CliAdded s_read;
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    const int added = EMBED_CALL(cli_added_read, ToolchainsAddedCtx, .read = &s_read, .err_msg = &err_msg) &&
                      EMBED_CALL(cli_list_contains, ToolchainsAddedCtx, .json_read = &s_read.json,
                                 .list = s_read.added_tools, .key = "id", .text = args->id);
    g_cli_guard->persistent_buf_release(s_read.json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_read.bundled.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read.bundled.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read.bundled),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return added;
}
