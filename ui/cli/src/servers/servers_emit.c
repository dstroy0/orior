// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_emit.c: each file's diagnostics as its server last gave them, the inspections' findings and the checkers',
// what a build tool gives a file and what each Python class's family sets; and a file's diagnostics passed on whole:
// its server's, less its reports of an attribute a class's family sets and of a name a build tool gives it, then the
// inspections', then each checker's, the checkers by their ids in order
#include "../builds/builds.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "servers_internal.h"

#include "../text/text.h"

// the max files whose diagnostics are stored, and the max bytes a report writes for each byte of it stored: an empty
// report and its comma are 3 bytes, and it writes every member of a diagnostic
#define CLI_SERVERS_PUBLISHED_MAX (1u << 16u)
#define CLI_SERVERS_EMIT_WIDER 64ULL

CliKeyed g_cli_servers_published = {"published diagnostics", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};
CliKeyed g_cli_servers_inspected = {"inspections' findings", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};
CliKeyed g_cli_servers_checked = {"checkers' findings", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};
CliKeyed g_cli_servers_input = {"names build tools give", {NULL}, 0u, 4096u};
CliKeyed g_cli_servers_supplied = {"names supplied", {NULL}, 0u, CLI_SERVERS_PUBLISHED_MAX};

// the stores a file's report is read from, in the order its diagnostics are: its server's, its inspections' and its
// checkers'
#define SERVERS_EMIT_STORES 3u

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
               .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES}), .key = key);
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
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_POOL_BYTES});
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
        const embed_word base = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_POOL_BYTES});
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
    CliKeyedEntry entry;
    CliKeyedEntry *entry_out;
    unsigned long long at;
    unsigned long long size;
    const char *required;
    unsigned long long length;
    const char *also;
    unsigned long long also_length;
    const char *key;
    const char *uri;
    const CliKeyed *keyed;
    unsigned int hash;
    CliKeyed *keyed_out;
    unsigned int array;
    int filtered;
    int *written;
    VerbaLinea *out;
    CliServerDiagnostic *items;
    unsigned int capacity;
    unsigned int *positioned;
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

// the bytes of a text stored as an entry read at a time, as its lines are looked through
#define SERVERS_EMIT_WINDOW 256u

// the end of the line from `at` of the text stored as `entry`: where its break is, or the text's end
EMBED_INLINE unsigned long long emit_line_end(const ServersEmitCtx *args)
{
    unsigned long long at = args->at;
    while (at < args->entry.text_size)
    {
        char window[SERVERS_EMIT_WINDOW];
        const unsigned long long left = args->entry.text_size - at;
        const unsigned long long bytes = (left < sizeof(window)) ? left : sizeof(window);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->entry.cell,
                                                                .offset = (embed_word)(args->entry.text_at + at),
                                                                .dst = window,
                                                                .bytes = (embed_word)bytes});
        const char *const found =
            memor.chr(&(MemoriaCfg){.src = window, .bytes = (embed_word)bytes, .val = (unsigned char)'\n'});
        if (found != NULL)
        {
            return at + (unsigned long long)(found - window);
        }
        at += bytes;
    }
    return args->entry.text_size;
}

// whether the text stored as `entry`, names one to a line, contains the line `length` bytes of `required` and then
// `also`, where it is given, read in its cell
EMBED_INLINE int emit_has_line(const ServersEmitCtx *args)
{
    unsigned long long at = 0ULL;
    while (at < args->entry.text_size)
    {
        const unsigned long long end = EMBED_CALL(emit_line_end, ServersEmitCtx, .entry = args->entry, .at = at);
        const unsigned long long start = args->entry.text_at + at;
        if ((end - at == args->length + args->also_length) &&
            (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = args->entry.cell,
                                                                  .offset = (embed_word)start,
                                                                  .src = args->required,
                                                                  .bytes = (embed_word)args->length}) == 0) &&
            ((args->also_length == 0ULL) ||
             (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = args->entry.cell,
                                                                   .offset = (embed_word)(start + args->length),
                                                                   .src = args->also,
                                                                   .bytes = (embed_word)args->also_length}) == 0)))
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
    memor.cpy(&(MemoriaCfg){.dst = s_attribute + 1u, .src = name, .bytes = (embed_word)name_size});
    return EMBED_CALL(emit_has_line, ServersEmitCtx, .entry = args->entry, .required = class_name, .length = class_size,
                      .also = s_attribute, .also_length = name_size + 1ULL);
}

// whether a server's report that a name is undefined names one of `names`, as pyright writes it: `"name" is not
// defined`
EMBED_INLINE int emit_free(const ServersEmitCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const message =
        EMBED_CALL(emit_message_get, ServersEmitCtx, .json = args->json, .item = args->item, .size_pointer = &length);
    static const char s_tail[] = "\" is not defined";
    if ((message == NULL) || (length < sizeof(s_tail)) || (message[0] != '"') ||
        !EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = message, .size = length, .suffix = s_tail))
    {
        return 0;
    }
    return EMBED_CALL(emit_has_line, ServersEmitCtx, .entry = args->entry, .required = message + 1u,
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
// a file the editor does not have open, one to a line: 1 with their entry into `entry_out`, or 0 for a file of no build
// tool that gives names; under the checks' lock
EMBED_INLINE int emit_input_names(const ServersEmitCtx *args)
{
    const char *key = args->key;
    const char *uri = args->uri;
    CliKeyedEntry *const entry = args->entry_out;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES});
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
    const unsigned int stored =
        EMBED_CALL(cli_keyed_key_adjacent.find, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_input, .key = key);
    if (stored != CLI_KEYED_NONE)
    {
        *entry = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_input, .at = stored);
        return 1;
    }
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri, .length = length, .path_out = path))
    {
        return 0;
    }
    unsigned long long build_size = 0ULL;
    const unsigned char *const build = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &build_size, .owner = g_cli_servers_owner);
    if (build == NULL)
    {
        return 0;
    }
    // each name a part of the file's text, and a break after it
    const unsigned long long capacity = (build_size * 2ULL) + 1ULL;
    char *const joined = g_cli_guard->temporary_buf_alloc(capacity, g_cli_servers_owner);
    VerbaLinea supplied = mmgr_line_over(joined, capacity);
    EMBED_CALL(cli_builds_path_text.build_input_names, CliBuildsPathTextCfg, .path = path, .text = (const char *)build,
               .size = build_size, .each = emit_name_append, .context = &supplied);
    g_cli_guard->persistent_buf_release((void *)(embed_word)build, (g_cli_servers_owner));
    const int integer = mmgr_line_ok(&supplied);
    const int put = EMBED_CALL(cli_keyed_key_adjacent.put, CliKeyedKeyAdjacentCfg, .keyed_out = &g_cli_servers_input,
                               .key = key, .adjacent_value = "", .text = integer ? supplied.out : "",
                               .size_value = integer ? supplied.at : 0ULL);
    g_cli_guard->persistent_buf_release(joined, g_cli_servers_owner);
    const unsigned int placed =
        put ? EMBED_CALL(cli_keyed_key_adjacent.find, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_input, .key = key)
            : CLI_KEYED_NONE;
    if (placed != CLI_KEYED_NONE)
    {
        *entry = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_input, .at = placed);
    }
    return placed != CLI_KEYED_NONE;
}

// the diagnostics of an array of `json` at `array`: its count, 0 where it is no array
EMBED_INLINE unsigned int emit_array_count(const ServersEmitCtx *args)
{
    return EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json,
                      .node = args->array, .type = CFG_JSON_ARRAY)
               ? args->json->node[args->array].count
               : 0u;
}

// each report of an array written as orior's diagnostic, after those written, those the filter refuses left out where
// `filtered`
EMBED_INLINE void emit_items_position(const ServersEmitCtx *args)
{
    const char *key = args->key;
    const CliJson *const json = args->json;
    // where the filter asks, the entries of the names supplied for the file and of those its build tool gives, read in
    // their cells
    CliKeyedEntry supplied = {0, 0u, NULL, 0ULL, 0u, 0u, 0ULL, 0ULL};
    CliKeyedEntry names = {0, 0u, NULL, 0ULL, 0u, 0u, 0ULL, 0ULL};
    if (args->filtered)
    {
        const unsigned int place = EMBED_CALL(cli_keyed_key_adjacent.find, CliKeyedKeyAdjacentCfg,
                                              .keyed = &g_cli_servers_supplied, .key = key);
        if (place != CLI_KEYED_NONE)
        {
            supplied = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_supplied, .at = place);
        }
        (void)EMBED_CALL(emit_input_names, ServersEmitCtx, .key = key, .uri = args->uri, .entry_out = &names);
    }
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                        .node = args->array, .type = CFG_JSON_ARRAY)
                                 ? json->node[args->array].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        if (args->filtered)
        {
            int dropped = supplied.used &&
                          EMBED_CALL(emit_supplied, ServersEmitCtx, .json = json, .item = item, .entry = supplied);
            dropped = dropped ||
                      (names.used && EMBED_CALL(emit_free, ServersEmitCtx, .json = json, .item = item, .entry = names));
            if (dropped)
            {
                continue;
            }
        }
        if (*args->positioned < args->capacity)
        {
            EMBED_CALL(cli_servers_json_internal.diagnostic_get, CliServersJsonCtx, .json = json, .one = item,
                       .diagnostic = args->items + *args->positioned);
            *args->positioned += 1u;
        }
    }
}

void cli_servers_emit_file(const CliServersSinkKeyCtx *args)
{
    const char *uri = args->uri;
    char path[CLI_PATH_MAX_LEN];
    if ((args->sink->diagnostics == NULL) ||
        !EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES}), .path_out = path))
    {
        return;
    }
    const unsigned int hash = EMBED_CALL(cli_keyed_key_adjacent.hash, CliKeyedKeyAdjacentCfg, .key = args->key);
    char displayed[CLI_PATH_MAX_LEN];
    VerbaLinea where = {displayed, sizeof(displayed), 0ULL};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->sink->tree,
               .path = path, .out = &where);
    const unsigned long long displayed_length = mmgr_line_finish(&where);
    // the file's stored texts, the server's, the inspections' and the checkers', and the nodes and bytes the tree they
    // are read into takes, a node for each token and the bytes of each text, as a tree read whole takes
    CliKeyed *const stores[SERVERS_EMIT_STORES] = {&g_cli_servers_published, &g_cli_servers_inspected,
                                                   &g_cli_servers_checked};
    char *stored[SERVERS_EMIT_STORES];
    unsigned long long sizes[SERVERS_EMIT_STORES];
    unsigned int tokens[SERVERS_EMIT_STORES];
    unsigned int nodes = 0u;
    unsigned int tokens_max = 0u;
    unsigned long long bytes = 0ULL;
    for (unsigned int store = 0u; store < SERVERS_EMIT_STORES; store += 1u)
    {
        sizes[store] = 0ULL;
        // each text stored read out of its cell into the JSON reader's own cell for this call
        const unsigned int place =
            EMBED_CALL(cli_keyed_key.find_hash, CliKeyedKeyCfg, .keyed = stores[store], .key = args->key, .hash = hash);
        const CliKeyedEntry one = (place != CLI_KEYED_NONE) ? EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg,
                                                                         .keyed = stores[store], .at = place)
                                                            : (CliKeyedEntry){0, 0u, NULL, 0ULL, 0u, 0u, 0ULL, 0ULL};
        sizes[store] = one.used ? one.text_size : 0ULL;
        stored[store] =
            (sizes[store] > 0ULL) ? g_cli_guard->temporary_buf_alloc(sizes[store], g_cli_servers_owner) : NULL;
        if (stored[store] != NULL)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one.cell,
                                                                    .offset = (embed_word)one.text_at,
                                                                    .dst = stored[store],
                                                                    .bytes = (embed_word)sizes[store]});
        }
        tokens[store] = ((stored[store] != NULL) && (sizes[store] > 0ULL))
                            ? (EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                          .text = stored[store], .size = sizes[store]) +
                               1u)
                            : 0u;
        nodes += tokens[store];
        bytes += (tokens[store] != 0u) ? sizes[store] : 0ULL;
        tokens_max = (tokens[store] > tokens_max) ? tokens[store] : tokens_max;
    }
    // the texts decoded once into one tree of the report's own, each read appending to it
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    CfgJsonToken *const scanned =
        (tokens_max != 0u) ? g_cli_guard->temporary_buf_alloc((unsigned long long)tokens_max * sizeof(CfgJsonToken),
                                                               g_cli_servers_owner)
                            : NULL;
    const int reserved =
        (nodes == 0u) || ((scanned != NULL) && EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg,
                                                          .json = &json, .nodes_max = nodes, .text_max = bytes,
                                                          .what = "a file's report", .owner = g_cli_servers_owner));
    unsigned int roots[SERVERS_EMIT_STORES] = {CLI_JSON_NONE, CLI_JSON_NONE, CLI_JSON_NONE};
    for (unsigned int store = 0u; reserved && (store < SERVERS_EMIT_STORES); store += 1u)
    {
        char quiet[256];
        VerbaLinea ignored = {quiet, sizeof(quiet), 0ULL};
        roots[store] = (tokens[store] != 0u)
                           ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                                        .json = &json, .text = stored[store], .size = sizes[store], .tokens = scanned,
                                        .tokens_max = tokens[store], .err_msg = &ignored)
                           : CLI_JSON_NONE;
    }
    g_cli_guard->persistent_buf_release(scanned, g_cli_servers_owner);
    for (unsigned int store = 0u; store < SERVERS_EMIT_STORES; store += 1u)
    {
        g_cli_guard->persistent_buf_release(stored[store], g_cli_servers_owner);
    }
    if (!reserved || (displayed_length == 0ULL))
    {
        if (json.node != NULL)
        {
            g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
            EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                       .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        }
        return;
    }
    // the diagnostics the tree contains at max: the server's, the inspections', and each checker's array
    unsigned int max = EMBED_CALL(emit_array_count, ServersEmitCtx, .json = &json, .array = roots[0]) +
                        EMBED_CALL(emit_array_count, ServersEmitCtx, .json = &json, .array = roots[1]);
    const unsigned int first_checker = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                  .json_read = &json, .node = roots[2], .type = CFG_JSON_OBJECT)
                                           ? json.node[roots[2]].first
                                           : CLI_JSON_NONE;
    for (unsigned int id = first_checker; id != CLI_JSON_NONE; id = json.node[id].next)
    {
        max += EMBED_CALL(emit_array_count, ServersEmitCtx, .json = &json, .array = id);
    }
    // the diagnostics positioned in a cell of this call, their messages in the tree's texts
    CliServerDiagnostic *const items =
        g_cli_guard->temporary_buf_alloc((unsigned long long)max * sizeof(CliServerDiagnostic), g_cli_servers_owner);
    unsigned int positioned = 0u;
    if (items != NULL)
    {
        EMBED_CALL(emit_items_position, ServersEmitCtx, .json = &json, .array = roots[0], .filtered = 1, .key = args->key,
                   .uri = uri, .items = items, .capacity = max, .positioned = &positioned);
        EMBED_CALL(emit_items_position, ServersEmitCtx, .json = &json, .array = roots[1], .filtered = 0, .key = args->key,
                   .uri = uri, .items = items, .capacity = max, .positioned = &positioned);
        for (unsigned int id = first_checker; id != CLI_JSON_NONE; id = json.node[id].next)
        {
            EMBED_CALL(emit_items_position, ServersEmitCtx, .json = &json, .array = id, .filtered = 0, .key = args->key,
                       .uri = uri, .items = items, .capacity = max, .positioned = &positioned);
        }
    }
    // the report's cell: its head, its diagnostics with their messages' places, its path, then the messages
    unsigned long long message_bytes = 0ULL;
    for (unsigned int at = 0u; at < positioned; at += 1u)
    {
        message_bytes += items[at].message_length + 1ULL;
    }
    const unsigned long long path_at = CLI_SERVER_REPORT_ITEM_AT(positioned);
    const unsigned long long messages_at = path_at + displayed_length + 1ULL;
    const CarcerMovableEntry *const report =
        (items != NULL) ? g_cli_guard->movable_buf_alloc(messages_at + message_bytes, g_cli_servers_owner) : NULL;
    if (report != NULL)
    {
        const CliServerReportHead head = {positioned, path_at};
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = report, .offset = 0ULL, .src = &head, .bytes = sizeof(head)});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = report, .offset = path_at, .src = displayed, .bytes = displayed_length + 1ULL});
        unsigned long long message_at = messages_at;
        for (unsigned int at = 0u; at < positioned; at += 1u)
        {
            const CliServerStoredDiagnostic stored = {items[at].from_line,     items[at].from_col, items[at].to_line,
                                                      items[at].to_col,        items[at].severity, message_at,
                                                      items[at].message_length};
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = report, .offset = CLI_SERVER_REPORT_ITEM_AT(at), .src = &stored, .bytes = sizeof(stored)});
            const char zero = '\0';
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = report, .offset = message_at, .src = items[at].message, .bytes = items[at].message_length});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = report, .offset = message_at + items[at].message_length, .src = &zero, .bytes = 1ULL});
            message_at += items[at].message_length + 1ULL;
        }
    }
    if (report != NULL)
    {
        int taken = 0;
        EMBED_CALL(args->sink->diagnostics, CliServerDiagnosticsCfg, .context = args->sink->context, .path = displayed,
                   .item = items, .count = positioned, .report = report, .taken = &taken);
        if (!taken)
        {
            cli_servers_report_release(report);
        }
    }
    g_cli_guard->persistent_buf_release(items, g_cli_servers_owner);
    if (json.node != NULL)
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    }
}

void cli_servers_report_release(const CarcerMovableEntry *report)
{
    if (report != NULL)
    {
        (void)g_cli_guard->movable_buf_release(report, g_cli_servers_owner);
    }
}

// a file recorded among those whose server reports an attribute, where it does, put on the line that is `context`: its
// key then its URI, each ended by a 0
static void emit_reporting(const CliEachKeyedEntryCfg *args)
{
    VerbaLinea *const reporting = args->context;
    static const char s_report[] = "Cannot access attribute \\\"";
    const unsigned long long needle = sizeof(s_report) - 1u;
    // the report looked for at each place of the text, in its cell
    int found = 0;
    for (unsigned long long at = 0ULL; !found && (at + needle <= args->entry.text_size); at += 1ULL)
    {
        found = g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = args->entry.cell,
                                                                     .offset = (embed_word)(args->entry.text_at + at),
                                                                     .src = s_report,
                                                                     .bytes = (embed_word)needle}) == 0;
    }
    if (found)
    {
        // the key and the URI beside it, each a path's length at max, read out of the cell with their 0
        char key[CLI_PATH_MAX_LEN];
        char uri[CLI_PATH_MAX_LEN];
        const unsigned long long key_bytes =
            (args->entry.key_size < sizeof(key)) ? (args->entry.key_size + 1ULL) : sizeof(key);
        const unsigned long long uri_bytes =
            (args->entry.adjacent_size < sizeof(uri)) ? (args->entry.adjacent_size + 1ULL) : sizeof(uri);
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = args->entry.cell, .offset = 0u, .dst = key, .bytes = (embed_word)key_bytes});
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->entry.cell,
                                                                .offset = (embed_word)args->entry.key_size + 1u,
                                                                .dst = uri,
                                                                .bytes = (embed_word)uri_bytes});
        key[key_bytes - 1ULL] = '\0';
        uri[uri_bytes - 1ULL] = '\0';
        mmgr_line_put_n(reporting, key, key_bytes);
        mmgr_line_put_n(reporting, uri, uri_bytes);
    }
}

void cli_servers_emit_reporting_attributes(const CliServersSinkKeyCtx *args)
{
    const unsigned long long capacity =
        EMBED_CALL(cli_keyed_table.listed_size, CliKeyedCfg, .keyed = &g_cli_servers_published) + 1ULL;
    char *const reporting = g_cli_guard->temporary_buf_alloc(capacity, g_cli_servers_owner);
    VerbaLinea line = mmgr_line_over(reporting, capacity);
    EMBED_CALL(cli_keyed_each_table.each, CliKeyedEachCfg, .keyed = &g_cli_servers_published, .each = emit_reporting,
               .context = &line);
    const unsigned long long size = mmgr_line_ok(&line) ? line.at : 0ULL;
    for (unsigned long long at = 0ULL; at < size;)
    {
        const char *const key = reporting + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1u;
        EMBED_CALL(cli_servers_sink_key_internal.emit_file, CliServersSinkKeyCtx, .key = key, .uri = uri,
                   .sink = args->sink);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_POOL_BYTES}) + 1u;
    }
    g_cli_guard->persistent_buf_release(reporting, g_cli_servers_owner);
}
