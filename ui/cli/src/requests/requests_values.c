// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_values.c: the environment files of a file of requests, each the nearest beside it or in a folder above it
// within the tree; the values of an environment they give, the private file's over the shared file's and `$shared`
// under the environment's own; whether an environment checks the peer's certificate; and the reader's private file
// made
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

#include "../text/text.h"

// the bytes a values' pool is taken with, and a value written as JSON is written in
#define CLI_REQUESTS_OBJECT_CAPACITY (1ULL << 20u)

// the environment file read, a tree in a cell sized to it, and the names of the environments, under the lock and
// given back before it is
static CliJson s_json;
static CliRequestsValues s_names;

// the environment files, the shared one first
static const char *const s_files[2] = {CLI_REQUESTS_PUBLIC, CLI_REQUESTS_PRIVATE};

typedef struct
{
    const char *path;
    const char *base;
    const char *file;
    VerbaLinea *err_msg;
    const char *root;
    char (*relative)[CLI_PATH_MAX_LEN];
    int *has;
    CliRequestsValues *values;
    unsigned int member;
    const char *ignore;
    int *joined;
} RequestsValuesCtx;
// 1 where `path` is `base` or stands under it
EMBED_INLINE int values_under(const RequestsValuesCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_POOL_BYTES});
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->path, .other_pointer = args->base,
                      .size = length) &&
           ((args->path[length] == '\0') || (args->path[length] == '/') || (args->path[length] == '\\'));
}

int cli_requests_nearest(const CliRequestsRootCtx *args)
{
    char file[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->path, .joined = file,
                    .err_msg = &err_msg) ||
        !EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = file, .out = folder))
    {
        return 0;
    }
    while (EMBED_CALL(values_under, RequestsValuesCtx, .path = folder, .base = args->root))
    {
        VerbaLinea line = {args->found, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, args->name);
        if ((mmgr_line_finish(&line) != 0ULL) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->found))
        {
            return 1;
        }
        char above[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = folder, .out = above))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = above, .bytes = sizeof(folder)});
    }
    return 0;
}

// the environment file at `file` read into the tree, the lock acquired: its object, or CLI_JSON_NONE with why given
EMBED_INLINE unsigned int values_read(const RequestsValuesCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->file,
                                                  .size_pointer = &size, .owner = g_cli_requests_owner);
    if ((bytes == NULL) ||
        !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = size))
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_requests_owner));
        mmgr_line_put(err_msg, args->file);
        mmgr_line_put(err_msg, ": it could not be read");
        return CLI_JSON_NONE;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {why, sizeof(why), 0ULL};
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int object = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                           .json = &s_json, .text = (const char *)bytes, .size = size, .nodes_more = 0u,
                                           .text_more = 0ULL, .err_msg = &read_err_msg, .owner = g_cli_requests_owner);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_requests_owner));
    if (object == CLI_JSON_NONE)
    {
        mmgr_line_put(err_msg, args->file);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, read_err_msg.at);
        return CLI_JSON_NONE;
    }
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_json, .node = object,
                    .type = CFG_JSON_OBJECT))
    {
        mmgr_line_put(err_msg, args->file);
        mmgr_line_put(err_msg, " contains no object of environments");
        return CLI_JSON_NONE;
    }
    return object;
}

int cli_requests_value_append(const CliRequestsNameCtx *args)
{
    unsigned int at = 0u;
    while ((at < args->values->count) &&
           ((args->values->name[at].size != args->name_size) ||
            (memor.cmp(&(MemoriaCfg){.src = args->values->text + args->values->name[at].at,
                                     .other = args->name,
                                     .bytes = (size_t)args->name_size}) != 0)))
    {
        at += 1u;
    }
    const unsigned long long needed =
        args->value_size + 1u + ((at == args->values->count) ? (args->name_size + 1u) : 0u);
    if (args->values->text == NULL)
    {
        args->values->text = g_cli_guard->persistent_buf_alloc(CLI_REQUESTS_TEXT, g_cli_requests_owner);
        args->values->capacity = (args->values->text != NULL) ? CLI_REQUESTS_TEXT : 0ULL;
    }
    if ((args->values->text == NULL) || (args->values->used + needed > args->values->capacity) ||
        (at == CLI_REQUESTS_VALUES))
    {
        return 0;
    }
    if (at == args->values->count)
    {
        args->values->name[at].at = args->values->used;
        args->values->name[at].size = args->name_size;
        memor.cpy(&(MemoriaCfg){
            .dst = args->values->text + args->values->used, .src = args->name, .bytes = (size_t)args->name_size});
        args->values->text[args->values->used + args->name_size] = '\0';
        args->values->used += args->name_size + 1u;
        args->values->count += 1u;
    }
    args->values->value[at].at = args->values->used;
    args->values->value[at].size = args->value_size;
    memor.cpy(&(MemoriaCfg){
        .dst = args->values->text + args->values->used, .src = args->value, .bytes = (size_t)args->value_size});
    args->values->text[args->values->used + args->value_size] = '\0';
    args->values->used += args->value_size + 1u;
    return 1;
}

const CliRequestsText *cli_requests_value(const CliRequestsSizeNameCtx *args)
{
    for (unsigned int at = 0u; at < args->values_read->count; at += 1u)
    {
        if ((args->values_read->name[at].size == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = args->values_read->text + args->values_read->name[at].at,
                                     .other = args->name,
                                     .bytes = (size_t)args->size}) == 0))
        {
            return &args->values_read->value[at];
        }
    }
    return NULL;
}

// the reason the values of an environment did not fit
EMBED_INLINE void values_full_err(const RequestsValuesCtx *args)
{
    mmgr_line_put(args->err_msg, "the values of the environment are larger than orior_cli supports");
}

// the names of the environments of the file at `path` put in s_names, the lock acquired, and the tree's paths of the
// shared file and the private one into `relative` where `has` shows it is there: 1, or 0 with why given
EMBED_INLINE int values_list(const RequestsValuesCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    char (*relative)[CLI_PATH_MAX_LEN] = args->relative;
    int read = 1;
    int fits = 1;
    g_cli_guard->persistent_buf_release(s_names.text, g_cli_requests_owner);
    s_names.text = NULL;
    s_names.capacity = 0ULL;
    s_names.used = 0ULL;
    s_names.count = 0u;
    unsigned int at = 0u;
    while (read && fits && (at < 2u))
    {
        char file[CLI_PATH_MAX_LEN];
        args->has[at] = EMBED_CALL(cli_requests_root_internal.nearest, CliRequestsRootCtx, .root = root, .path = path,
                                   .name = s_files[at], .found = file);
        if (!args->has[at])
        {
            at += 1u;
            continue;
        }
        const unsigned int object = EMBED_CALL(values_read, RequestsValuesCtx, .file = file, .err_msg = args->err_msg);
        read = (object != CLI_JSON_NONE);
        for (unsigned int member = read ? s_json.node[object].first : CLI_JSON_NONE; fits && (member != CLI_JSON_NONE);
             member = s_json.node[member].next)
        {
            const char *const key = s_json.text + s_json.node[member].key;
            fits =
                (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "$shared", .cap = CLI_POOL_BYTES})) ||
                EMBED_CALL(cli_requests_name_internal.value_append, CliRequestsNameCtx, .values = &s_names, .name = key,
                           .name_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}), .value = "",
                           .value_size = 0ULL);
        }
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = file,
                   .relative_out = relative[at]);
        at += 1u;
    }
    if (!fits)
    {
        values_full_err(args);
    }
    return read && fits;
}

int cli_requests_environments(const CliRequestsTextCfg *args)
{
    char relative[2][CLI_PATH_MAX_LEN];
    int has[2] = {0, 0};
    const int list_entries = EMBED_CALL(values_list, RequestsValuesCtx, .root = args->root, .path = args->path,
                                        .relative = relative, .has = has, .err_msg = args->err_msg);
    if (list_entries)
    {
        mmgr_line_put(args->out, "{\"names\":[");
        for (unsigned int at = 0u; at < s_names.count; at += 1u)
        {
            mmgr_line_put(args->out, (at > 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = s_names.text + s_names.name[at].at, .length = s_names.name[at].size);
        }
        static const char *const keys[2] = {"],\"shared\":", ",\"private\":"};
        for (unsigned int index = 0u; index < 2u; index += 1u)
        {
            mmgr_line_put(args->out, keys[index]);
            if (has[index])
            {
                EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                           .text = relative[index],
                           .length = cellul.len(&(CatenaFinitaCfg){.src = relative[index], .cap = CLI_POOL_BYTES}));
            }
            else
            {
                mmgr_line_put(args->out, "null");
            }
        }
        mmgr_line_ch(args->out, '}');
    }
    g_cli_guard->persistent_buf_release(s_names.text, g_cli_requests_owner);
    s_names.text = NULL;
    s_names.capacity = 0ULL;
    s_names.used = 0ULL;
    s_names.count = 0u;
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return list_entries;
}

// a value of the tree, a string as its letters and any other as JSON on one line, put as `name` in `values`: 1, or 0
// where it does not fit
EMBED_INLINE int values_member_append(const RequestsValuesCtx *args)
{
    CliRequestsValues *values = args->values;
    const char *const key = s_json.text + s_json.node[args->member].key;
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = &s_json, .node = args->member, .length_pointer = &length);
    if (text != NULL)
    {
        return EMBED_CALL(cli_requests_name_internal.value_append, CliRequestsNameCtx, .values = values, .name = key,
                          .name_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}), .value = text,
                          .value_size = length);
    }
    char *const written_cell = g_cli_guard->temporary_buf_alloc(CLI_REQUESTS_OBJECT_CAPACITY, g_cli_requests_owner);
    VerbaLinea written = mmgr_line_over(written_cell, CLI_REQUESTS_OBJECT_CAPACITY);
    if (written.out == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_json, .node = args->member,
               .out = &written, .indent = 0u, .sorted = 0);
    const int put =
        mmgr_line_ok(&written) &&
        EMBED_CALL(cli_requests_name_internal.value_append, CliRequestsNameCtx, .values = values, .name = key,
                   .name_size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}), .value = written.out,
                   .value_size = written.at);
    g_cli_guard->persistent_buf_release(written.out, g_cli_requests_owner);
    return put;
}

int cli_requests_values(const CliRequestsErrMsgRootCtx *args)
{
    g_cli_guard->persistent_buf_release(args->values->text, g_cli_requests_owner);
    args->values->text = NULL;
    args->values->capacity = 0ULL;
    args->values->used = 0ULL;
    args->values->count = 0u;
    int read = 1;
    int fits = 1;
    unsigned int at = 0u;
    while (read && fits && (at < 2u))
    {
        char file[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_requests_root_internal.nearest, CliRequestsRootCtx, .root = args->root, .path = args->path,
                        .name = s_files[at], .found = file))
        {
            at += 1u;
            continue;
        }
        const unsigned int object = EMBED_CALL(values_read, RequestsValuesCtx, .file = file, .err_msg = args->err_msg);
        read = (object != CLI_JSON_NONE);
        for (unsigned int group = 0u; read && fits && (group < ((args->env != NULL) ? 2u : 1u)); group += 1u)
        {
            const unsigned int set =
                EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                           .object = object, .name = (group == 0u) ? "$shared" : args->env);
            for (unsigned int member = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                  .json_read = &s_json, .node = set, .type = CFG_JSON_OBJECT)
                                           ? s_json.node[set].first
                                           : CLI_JSON_NONE;
                 fits && (member != CLI_JSON_NONE); member = s_json.node[member].next)
            {
                fits = EMBED_CALL(values_member_append, RequestsValuesCtx, .values = args->values, .member = member);
            }
        }
        at += 1u;
    }
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if (!fits)
    {
        EMBED_CALL(values_full_err, RequestsValuesCtx, .err_msg = args->err_msg);
    }
    return read && fits;
}

int cli_requests_verifies(const CliRequestsErrMsgRootCtx *args)
{
    if (args->env == NULL)
    {
        return 1;
    }
    int verifies = 1;
    unsigned int at = 0u;
    while (verifies && (at < 2u))
    {
        char file[CLI_PATH_MAX_LEN];
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const unsigned int object =
            EMBED_CALL(cli_requests_root_internal.nearest, CliRequestsRootCtx, .root = args->root, .path = args->path,
                       .name = s_files[at], .found = file)
                ? EMBED_CALL(values_read, RequestsValuesCtx, .file = file, .err_msg = &err_msg)
                : CLI_JSON_NONE;
        const unsigned int set = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = &s_json, .object = object, .name = args->env);
        const unsigned int ssl = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = &s_json, .object = set, .name = "SSLConfiguration");
        verifies = !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_json,
                               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &s_json, .object = ssl, .name = "verifyHostCertificate"),
                               .type = CFG_JSON_FALSE);
        at += 1u;
    }
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return verifies;
}

// whether the .gitignore at `ignore` has a line that, trimmed, names the reader's private file; and whether it ends
// in a newline or is empty, into `joined`
EMBED_INLINE int values_ignore(const RequestsValuesCtx *args)
{
    int *joined = args->joined;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->ignore,
                                                  .size_pointer = &size, .owner = g_cli_requests_owner);
    const char *const list_entries = (const char *)bytes;
    const unsigned long long length = (bytes != NULL) ? size : 0ULL;
    *joined = (length == 0ULL) || (list_entries[length - 1u] == '\n');
    int named = 0;
    for (unsigned long long at = 0ULL; !named && (at < length);)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = list_entries, .at = at, .size = length);
        unsigned long long end = bounds.stop;
        unsigned long long start = at;
        at = bounds.next;
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = list_entries,
                   .start = &start, .end = &end);
        static const char *const names[3] = {CLI_REQUESTS_PRIVATE, "/" CLI_REQUESTS_PRIVATE, "*.private.env.json"};
        for (unsigned int name = 0u; name < 3u; name += 1u)
        {
            named = named ||
                    ((cellul.len(&(CatenaFinitaCfg){.src = names[name], .cap = CLI_POOL_BYTES}) == end - start) &&
                     (memor.cmp(&(MemoriaCfg){
                          .src = list_entries + start, .other = names[name], .bytes = (size_t)(end - start)}) == 0));
        }
    }
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_requests_owner));
    return named;
}

// the reader's private file written at `file`, an empty set for each environment named in s_names, or for `dev`
// where none is, as serde_json's pretty writer lays the object out: 1, or 0 where it could not be written
EMBED_INLINE int values_private_write(const RequestsValuesCtx *args)
{
    char *const object_cell = g_cli_guard->temporary_buf_alloc(CLI_REQUESTS_OBJECT_CAPACITY, g_cli_requests_owner);
    VerbaLinea object = mmgr_line_over(object_cell, CLI_REQUESTS_OBJECT_CAPACITY);
    if (object.out == NULL)
    {
        return 0;
    }
    mmgr_line_ch(&object, '{');
    for (unsigned int at = 0u; at < s_names.count; at += 1u)
    {
        mmgr_line_put(&object, (at > 0u) ? ",\n  " : "\n  ");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &object,
                   .text = s_names.text + s_names.name[at].at, .length = s_names.name[at].size);
        mmgr_line_put(&object, ": {}");
    }
    if (s_names.count == 0u)
    {
        mmgr_line_put(&object, "\n  \"dev\": {}");
    }
    mmgr_line_put(&object, "\n}\n");
    const int written =
        mmgr_line_ok(&object) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                            .path = args->file, .bytes = object.out, .size = object.at);
    g_cli_guard->persistent_buf_release(object.out, g_cli_requests_owner);
    return written;
}

int cli_requests_private_init(const CliRequestsTextCfg *args)
{
    char file[CLI_PATH_MAX_LEN];
    char relative[2][CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_requests_root_internal.nearest, CliRequestsRootCtx, .root = args->root, .path = args->path,
                   .name = CLI_REQUESTS_PRIVATE, .found = file))
    {
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = file,
                   .relative_out = relative[0]);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = relative[0],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = relative[0], .cap = CLI_POOL_BYTES}));
        return 1;
    }
    char inside[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->path, .joined = inside,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = inside, .out = folder))
    {
        mmgr_line_put(args->err_msg, "the file has no folder");
        return 0;
    }
    VerbaLinea made = {file, sizeof(file), 0ULL};
    mmgr_line_put(&made, folder);
    mmgr_line_put(&made, "/" CLI_REQUESTS_PRIVATE);
    char ignore[CLI_PATH_MAX_LEN];
    VerbaLinea ignore_line = {ignore, sizeof(ignore), 0ULL};
    mmgr_line_put(&ignore_line, args->root);
    mmgr_line_put(&ignore_line, "/.gitignore");
    if ((mmgr_line_finish(&made) == 0ULL) || (mmgr_line_finish(&ignore_line) == 0ULL))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": its path is too long");
        return 0;
    }
    int has[2] = {0, 0};
    int done = EMBED_CALL(values_list, RequestsValuesCtx, .root = args->root, .path = args->path, .relative = relative,
                          .has = has, .err_msg = args->err_msg);
    int joined = 1;
    const char *failed = NULL;
    if (done && !EMBED_CALL(values_ignore, RequestsValuesCtx, .ignore = ignore, .joined = &joined))
    {
        // a tree with no .gitignore is given one, which an empty file's `joined` leaves to the second write to make
        const int fresh = !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = ignore);
        done = (joined || EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = ignore,
                                     .bytes = "\n", .size = 1ULL, .fresh = 0)) &&
               EMBED_CALL(cli_path_bytes_internal.file_append, CliPathBytesCtx, .path = ignore,
                          .bytes = CLI_REQUESTS_PRIVATE "\n", .size = sizeof(CLI_REQUESTS_PRIVATE), .fresh = fresh);
        failed = done ? NULL : ignore;
    }
    if (done)
    {
        done = EMBED_CALL(values_private_write, RequestsValuesCtx, .file = file);
        failed = done ? NULL : file;
    }
    g_cli_guard->persistent_buf_release(s_names.text, g_cli_requests_owner);
    s_names.text = NULL;
    s_names.capacity = 0ULL;
    s_names.used = 0ULL;
    s_names.count = 0u;
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if (failed != NULL)
    {
        mmgr_line_put(args->err_msg, failed);
        mmgr_line_put(args->err_msg, ": it could not be written");
    }
    if (done)
    {
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = file,
                   .relative_out = relative[0]);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = relative[0],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = relative[0], .cap = CLI_POOL_BYTES}));
    }
    return done;
}
