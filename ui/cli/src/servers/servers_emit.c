// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_emit.c: each file's diagnostics as its server last gave them, the inspections' findings and the checkers',
// what a build tool gives a file and what each Python class's family sets; and a file's diagnostics passed on whole:
// its server's, less its reports of an attribute a class's family sets and of a name a build tool gives it, then the
// inspections', then each checker's, the checkers by their ids in order
#include "../builds/builds.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "servers_internal.h"

#include "../text/text.h"

// the most files whose diagnostics are stored, and the most bytes a report writes for each byte of it stored: an empty
// report and its comma are 3 bytes, and it writes every member of a diagnostic
#define CLI_SERVERS_PUBLISHED_MAX (1u << 16u)
#define CLI_SERVERS_EMIT_WIDER 64ULL

CliKeyed g_cli_servers_published = {"published diagnostics", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};
CliKeyed g_cli_servers_inspected = {"inspections' findings", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};
CliKeyed g_cli_servers_checked = {"checkers' findings", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};
CliKeyed g_cli_servers_input = {"names build tools give", {NULL}, 0u, 4096u};
CliKeyed g_cli_servers_supplied = {"names supplied", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};

// what is read to pass a file's diagnostics on
static CliServersReadState s_read_state = {0};

void cli_servers_uri_get(const CliServersPathUriCtx *args)
{
    const char *path = args->path;
    char *uri = args->uri_out;
    EMBED_CALL(cli_lsp_capacity_path.uri_get, CliLspCapacityPathCfg, .path = path, .uri_out = uri,
               .capacity = CLI_PATH_MAX_LEN);
}

void cli_servers_key_get(const CliServersPathKeyCtx *args)
{
    char *key = args->key_out;
    char uri[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = args->path, .uri_out = uri);
    EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = uri,
               .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .key = key);
}

void cli_servers_tree_path(const CliServersPathErrMsgCtx *args)
{
    char full_path[CLI_PATH_MAX_LEN];
    char full_tree[CLI_PATH_MAX_LEN];
    if ((args->tree == NULL) || (args->tree[0] == '\0'))
    {
        mmgr_line_put(args->out, args->path);
        return;
    }
    const char *const integer = EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx,
                                           .path = args->path, .full = full_path, .capacity = sizeof(full_path))
                                    ? full_path
                                    : args->path;
    const char *const top = EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = args->tree,
                                       .full = full_tree, .capacity = sizeof(full_tree))
                                ? full_tree
                                : args->tree;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL});
    const int under =
        EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = integer, .other_pointer = top, .size = length) &&
        ((integer[length] == '\0') || (integer[length] == '/') || (integer[length] == '\\'));
    // a file of a folder mounted beside the tree goes by its mount's name
    const char *names[CLI_MOUNTS_MAX];
    const char *paths[CLI_MOUNTS_MAX];
    const unsigned int mounts = EMBED_CALL(cli_capacity_names_internal.mounts, CliCapacityNamesCtx, .names = names,
                                           .paths_read = paths, .capacity_u32 = CLI_MOUNTS_MAX);
    int mounted = 0;
    for (unsigned int at = 0u; !under && (at < mounts); at += 1u)
    {
        const size_t base = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL});
        mounted |= EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = integer, .other_pointer = paths[at],
                              .size = base) &&
                   ((integer[base] == '\0') || (integer[base] == '/') || (integer[base] == '\\'));
    }
    if (!under && !mounted)
    {
        mmgr_line_put(args->out, integer);
        return;
    }
    char relative[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = top, .path = integer, .relative_out = relative);
    mmgr_line_put(args->out, relative);
}

typedef struct
{
    const CliJson *json;
    unsigned int item;
    unsigned long long *size_pointer;
    const char *names;
    unsigned long long size;
    const char *required;
    unsigned long long length;
    const char *also;
    unsigned long long also_length;
    const char *supplied;
    const char *key;
    const char *uri;
    const char **names_pointer;
    const CliKeyed *keyed;
    unsigned int hash;
    CliKeyed *keyed_out;
    unsigned int array;
    int filtered;
    int *written;
    VerbaLinea *out;
    CliServerDiagnostic *items;
    unsigned int capacity;
    unsigned int *laid;
} ServersEmitCtx;
// the message of a server's report, NULL where it has none
EMBED_INLINE const char *emit_message_get(const ServersEmitCtx *args)
{
    const CliJson *json = args->json;
    const unsigned int message = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->item, .name = "message");
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = message, .length_pointer = args->size_pointer);
    return (text != NULL)
               ? text
               : EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = message, .name = "value"),
                            .length_pointer = args->size_pointer);
}

// whether `size` bytes of `names`, one to a line, contain the line `size` bytes of `required` and then `also`, where it
// is given
EMBED_INLINE int emit_has_line(const ServersEmitCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long at = 0ULL;
    while (at < size)
    {
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->names, .at = end,
                         .size = size, .set = "\n");
        if ((end - at == args->length + args->also_length) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->names + at, .other = args->required, .bytes = (size_t)args->length}) == 0) &&
            ((args->also_length == 0ULL) || (memor.cmp(&(MemoriaCfg){.src = args->names + at + args->length,
                                                                     .other = args->also,
                                                                     .bytes = (size_t)args->also_length}) == 0)))
        {
            return 1;
        }
        at = end + 1ULL;
    }
    return 0;
}

int cli_servers_attribute_get(const CliServersLengthCtx *args)
{
    const char **name = args->name_pointer;
    unsigned long long *name_size = args->name_size;
    const char **class_name = args->class_name;
    unsigned long long *class_size = args->class_size;
    static const char s_head[] = "Cannot access attribute \"";
    static const char s_middle[] = "\" for class \"";
    if ((args->message == NULL) || !EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = args->message,
                                               .size = args->length, .prefix = s_head))
    {
        return 0;
    }
    *name = args->message + sizeof(s_head) - 1u;
    const unsigned long long rest = args->length - (sizeof(s_head) - 1u);
    *name_size = 0ULL;
    *name_size = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (*name), .at = *name_size,
                            .size = rest, .set = "\"");
    if ((*name_size == rest) || !EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg,
                                            .text = *name + *name_size, .size = rest - *name_size, .prefix = s_middle))
    {
        return 0;
    }
    *class_name = *name + *name_size + sizeof(s_middle) - 1u;
    const unsigned long long class_remainder = rest - *name_size - (sizeof(s_middle) - 1u);
    *class_size = 0ULL;
    *class_size = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (*class_name),
                             .at = *class_size, .size = class_remainder, .set = "\"");
    if (*class_size == class_remainder)
    {
        return 0;
    }
    {
        const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                   .text = *class_name, .at = 0ULL, .size = *class_size, .set = "*");
        *class_name += lead;
        *class_size -= lead;
    }
    *class_size = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = (*class_name),
                             .at = *class_size, .set = "*");
    return 1;
}

// whether a server's report that a class has no such attribute names an attribute the class's family sets; what the
// file's family sets stored one to a line as `class attribute`
EMBED_INLINE int emit_supplied(const ServersEmitCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const message =
        EMBED_CALL(emit_message_get, ServersEmitCtx, .json = args->json, .item = args->item, .size_pointer = &length);
    const char *name = NULL;
    unsigned long long name_size = 0ULL;
    const char *class_name = NULL;
    unsigned long long class_size = 0ULL;
    if (!EMBED_CALL(cli_servers_length_internal.attribute_get, CliServersLengthCtx, .message = message,
                    .length = length, .name_pointer = &name, .name_size = &name_size, .class_name = &class_name,
                    .class_size = &class_size))
    {
        return 0;
    }
    static char s_attribute[1024];
    if (name_size + 1ULL >= sizeof(s_attribute))
    {
        return 0;
    }
    s_attribute[0] = ' ';
    memor.cpy(&(MemoriaCfg){.dst = s_attribute + 1u, .src = name, .bytes = (size_t)name_size});
    return EMBED_CALL(emit_has_line, ServersEmitCtx, .names = args->supplied, .size = args->size,
                      .required = class_name, .length = class_size, .also = s_attribute,
                      .also_length = name_size + 1ULL);
}

// whether a server's report that a name is undefined names one of `names`, as pyright writes it: `"name" is not
// defined`
EMBED_INLINE int emit_free(const ServersEmitCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long length = 0ULL;
    const char *const message =
        EMBED_CALL(emit_message_get, ServersEmitCtx, .json = args->json, .item = args->item, .size_pointer = &length);
    static const char s_tail[] = "\" is not defined";
    if ((message == NULL) || (length < sizeof(s_tail)) || (message[0] != '"') ||
        !EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = message, .size = length, .suffix = s_tail))
    {
        return 0;
    }
    return EMBED_CALL(emit_has_line, ServersEmitCtx, .names = args->names, .size = size, .required = message + 1u,
                      .length = length - sizeof(s_tail), .also = "", .also_length = 0ULL);
}

// a name a build tool gives a file put on its own line
static void emit_name_append(const CliEachNameSizeCfg *args)
{
    VerbaLinea *const names = args->context;
    mmgr_line_put_n(names, args->name, args->size);
    mmgr_line_ch(names, '\n');
}

// the names a build tool gives the file of `key` and `uri`, recorded as its text was checked or read from the disk for
// a file the editor does not have open, one to a line: 1 with them into `names`, or 0 for a file of no build tool that
// gives names; under the checks' lock
EMBED_INLINE int emit_input_names(const ServersEmitCtx *args)
{
    const char *key = args->key;
    const char *uri = args->uri;
    const char **names = args->names_pointer;
    unsigned long long *size = args->size_pointer;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL});
    static const char s_construct[] = "/sconstruct";
    static const char s_script[] = "/sconscript";
    int construct = (length >= sizeof(s_construct) - 1u);
    int script = (length >= sizeof(s_script) - 1u);
    {
        unsigned int at = 0u;
        while (construct && (at < sizeof(s_construct) - 1u))
        {
            const char one = uri[length - (sizeof(s_construct) - 1u) + at];
            const char lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
            construct = (lower == s_construct[at]);
            at += 1u;
        }
    }
    {
        unsigned int at = 0u;
        while (script && (at < sizeof(s_script) - 1u))
        {
            const char one = uri[length - (sizeof(s_script) - 1u) + at];
            const char lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
            script = (lower == s_script[at]);
            at += 1u;
        }
    }
    if (!construct && !script)
    {
        return 0;
    }
    const char *adjacent = NULL;
    *names = EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_input, .key = key,
                        .adjacent = &adjacent, .size = size);
    if (*names != NULL)
    {
        return 1;
    }
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri, .length = length, .path_out = path))
    {
        return 0;
    }
    unsigned long long build_size = 0ULL;
    const unsigned char *const build = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &build_size, .owner = &g_cli_servers_cells);
    if (build == NULL)
    {
        return 0;
    }
    // each name a part of the file's text, and a break after it
    const unsigned long long capacity = (build_size * 2ULL) + 1ULL;
    char *const joined = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_servers_cells);
    VerbaLinea supplied = mmgr_line_over(joined, capacity);
    EMBED_CALL(cli_builds_path_text.build_input_names, CliBuildsPathTextCfg, .path = path, .text = (const char *)build,
               .size = build_size, .each = emit_name_append, .context = &supplied);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = build, .owner = &g_cli_servers_cells);
    const int integer = mmgr_line_ok(&supplied);
    (void)EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_input, .key = key,
                     .adjacent_value = "", .text = integer ? supplied.out : "",
                     .size_value = integer ? supplied.at : 0ULL);
    g_cli_cells->persistent_buf_release(joined, &g_cli_servers_cells);
    *names = EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_input, .key = key,
                        .adjacent = &adjacent, .size = size);
    return *names != NULL;
}

// the stored text of `key`, whose hash is `hash`, in `keyed` read into the reading's tree under its lock: its root,
// CLI_JSON_NONE where none is
EMBED_INLINE unsigned int emit_stored_read(const ServersEmitCtx *args)
{
    const CliKeyed *keyed = args->keyed_out;
    const char *key = args->key;
    unsigned int hash = args->hash;
    const char *adjacent = NULL;
    unsigned long long size = 0ULL;
    const char *const text = EMBED_CALL(cli_keyed_key.get_hash, CliKeyedKeyCfg, .keyed = keyed, .key = key,
                                        .hash = hash, .adjacent = &adjacent, .size = &size);
    const unsigned int root = ((text != NULL) && (size > 0ULL))
                                  ? EMBED_CALL(cli_servers_read_state_internal.read, CliServersReadStateCtx,
                                               .read_state = &s_read_state, .text = text, .size = size)
                                  : CLI_JSON_NONE;
    return root;
}

// each report of an array written as orior's diagnostic, after those written, those the filter refuses left out where
// `filtered`
EMBED_INLINE void emit_items_lay(const ServersEmitCtx *args)
{
    const char *key = args->key;
    const CliJson *const json = &s_read_state.json;
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = args->array, .kind = CFG_JSON_ARRAY)
                                 ? json->node[args->array].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        if (args->filtered)
        {
            const char *adjacent = NULL;
            unsigned long long size = 0ULL;
            const char *const supplied =
                EMBED_CALL(cli_keyed_key_adjacent.get, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_supplied,
                           .key = key, .adjacent = &adjacent, .size = &size);
            int dropped = (supplied != NULL) && EMBED_CALL(emit_supplied, ServersEmitCtx, .json = json, .item = item,
                                                           .supplied = supplied, .size = size);
            const char *names = NULL;
            dropped = dropped ||
                      (EMBED_CALL(emit_input_names, ServersEmitCtx, .key = key, .uri = args->uri,
                                  .names_pointer = &names, .size_pointer = &size) &&
                       EMBED_CALL(emit_free, ServersEmitCtx, .json = json, .item = item, .names = names, .size = size));
            if (dropped)
            {
                continue;
            }
        }
        if (*args->laid < args->capacity)
        {
            EMBED_CALL(cli_servers_json_internal.diagnostic_get, CliServersJsonCtx, .json = json, .one = item,
                       .diagnostic = args->items + *args->laid);
            *args->laid += 1u;
        }
    }
}

void cli_servers_emit_file(const CliServersSinkKeyCtx *args)
{
    const char *uri = args->uri;
    char path[CLI_PATH_MAX_LEN];
    if ((args->sink->diagnostics == NULL) ||
        !EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .path_out = path))
    {
        return;
    }
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_lock, CliServersReadStateAnswerCtx,
               .read_state = &s_read_state);
    const unsigned int hash = EMBED_CALL(cli_keyed_key_adjacent.hash, CliKeyedKeyAdjacentCfg, .key = args->key);
    char displayed[CLI_PATH_MAX_LEN];
    VerbaLinea where = {displayed, sizeof(displayed), 0ULL};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->sink->tree,
               .path = path, .out = &where);
    (void)mmgr_line_finish(&where);
    // the reports read one after another, the file's published, then its inspections', then each checker's
    const unsigned int published = EMBED_CALL(emit_stored_read, ServersEmitCtx, .keyed_out = &g_cli_servers_published,
                                              .key = args->key, .hash = hash);
    const unsigned int inspected = EMBED_CALL(emit_stored_read, ServersEmitCtx, .keyed_out = &g_cli_servers_inspected,
                                              .key = args->key, .hash = hash);
    const unsigned int by_checker = EMBED_CALL(emit_stored_read, ServersEmitCtx, .keyed_out = &g_cli_servers_checked,
                                               .key = args->key, .hash = hash);
    const CliJson *const json = &s_read_state.json;
    unsigned int most = (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                    .node = published, .kind = CFG_JSON_ARRAY)
                             ? json->node[published].count
                             : 0u) +
                        (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                    .node = inspected, .kind = CFG_JSON_ARRAY)
                             ? json->node[inspected].count
                             : 0u);
    const unsigned int first_checker = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                  .json_read = json, .node = by_checker, .kind = CFG_JSON_OBJECT)
                                           ? json->node[by_checker].first
                                           : CLI_JSON_NONE;
    for (unsigned int id = first_checker; id != CLI_JSON_NONE; id = json->node[id].next)
    {
        most += EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = id,
                           .kind = CFG_JSON_ARRAY)
                    ? json->node[id].count
                    : 0u;
    }
    CliServerDiagnostic *const items =
        g_cli_cells->temporary_buf_alloc(((most != 0u) ? most : 1u) * sizeof(*items), &g_cli_servers_cells);
    if (items != NULL)
    {
        unsigned int laid = 0u;
        EMBED_CALL(emit_items_lay, ServersEmitCtx, .array = published, .filtered = 1, .key = args->key, .uri = uri,
                   .items = items, .capacity = most, .laid = &laid);
        EMBED_CALL(emit_items_lay, ServersEmitCtx, .array = inspected, .filtered = 0, .key = args->key, .uri = uri,
                   .items = items, .capacity = most, .laid = &laid);
        for (unsigned int id = first_checker; id != CLI_JSON_NONE; id = json->node[id].next)
        {
            EMBED_CALL(emit_items_lay, ServersEmitCtx, .array = id, .filtered = 0, .key = args->key, .uri = uri,
                       .items = items, .capacity = most, .laid = &laid);
        }
        EMBED_CALL(args->sink->diagnostics, CliServerDiagnosticsCfg, .context = args->sink->context, .path = displayed,
                   .item = items, .count = laid);
    }
    g_cli_cells->persistent_buf_release(items, &g_cli_servers_cells);
    EMBED_CALL(cli_servers_read_state_answer_internal.read_state_release, CliServersReadStateAnswerCtx,
               .read_state = &s_read_state);
}

// a file recorded among those whose server reports an attribute, where it does, put on the line that is `context`: its
// key then its URI, each ended by a 0
static void emit_reporting(const CliEachKeyAdjacentTextSizeCfg *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    VerbaLinea *const reporting = args->context;
    static const char s_report[] = "Cannot access attribute \\\"";
    unsigned long long at = 0ULL;
    if (EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text, .length_value = size, .required = s_report,
                   .size = sizeof(s_report) - 1u, .at_pointer = &at))
    {
        mmgr_line_put_n(reporting, args->key,
                        cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}) + 1u);
        mmgr_line_put_n(reporting, args->adjacent,
                        cellul.len(&(CatenaFinitaCfg){.src = args->adjacent, .cap = CLI_CELLS_POOL}) + 1u);
    }
}

void cli_servers_emit_reporting_attributes(const CliServersSinkKeyCtx *args)
{
    const unsigned long long capacity =
        EMBED_CALL(cli_keyed_table.listed_size, CliKeyedCfg, .keyed = &g_cli_servers_published) + 1ULL;
    char *const reporting = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_servers_cells);
    VerbaLinea line = mmgr_line_over(reporting, capacity);
    EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = &g_cli_servers_published, .each = emit_reporting,
               .context = &line);
    const unsigned long long size = mmgr_line_ok(&line) ? line.at : 0ULL;
    for (unsigned long long at = 0ULL; at < size;)
    {
        const char *const key = reporting + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        EMBED_CALL(cli_servers_sink_key_internal.emit_file, CliServersSinkKeyCtx, .key = key, .uri = uri,
                   .sink = args->sink);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    g_cli_cells->persistent_buf_release(reporting, &g_cli_servers_cells);
}
