// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_read.c: the deployments deploy.json names, read as serde reads them; the places a deployment sends to; and a
// shell line run at one
#include "../command_line/pool.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

typedef struct
{
    CliDeployPool *pool;
    unsigned long long capacity;
    const CliJson *json;
    unsigned int node;
    const char *required;
    VerbaLinea *err_msg;
    const char *field;
    unsigned int entry;
    const char ***texts;
    unsigned int *count;
    CliDeployment *deployment;
    unsigned int letter;
} DeployReadCtx;

// a page of `capacity` bytes taken for `pool`, NULL with the pool marked refused where it could not be
EMBED_INLINE CliDeployPage *read_page(const DeployReadCtx *args)
{
    CliDeployPage *const page =
        g_cli_guard->persistent_buf_alloc(sizeof(CliDeployPage) + args->capacity, g_cli_deploy_owner);
    if (page == NULL)
    {
        args->pool->refused = 1;
        return NULL;
    }
    page->size = args->capacity;
    page->used = 0ULL;
    return page;
}

void *cli_deploy_on_position(const CliDeployPoolCtx *args)
{
    CliDeployPage *const newest = args->pool->page;
    if (args->size > CLI_DEPLOY_PAGE)
    {
        // a page of its own goes behind the newest, which retains the bytes it has left
        CliDeployPage *const own = EMBED_CALL(read_page, DeployReadCtx, .pool = args->pool, .capacity = args->size);
        if (own == NULL)
        {
            return NULL;
        }
        own->used = args->size;
        own->before = (newest != NULL) ? newest->before : NULL;
        if (newest != NULL)
        {
            newest->before = own;
        }
        else
        {
            args->pool->page = own;
        }
        return own->bytes;
    }
    if (newest != NULL)
    {
        const unsigned long long at = (newest->used + 7u) & ~7ULL;
        if ((at <= newest->size) && (args->size <= newest->size - at))
        {
            newest->used = at + args->size;
            return newest->bytes + at;
        }
    }
    CliDeployPage *const page = EMBED_CALL(read_page, DeployReadCtx, .pool = args->pool, .capacity = CLI_DEPLOY_PAGE);
    if (page == NULL)
    {
        return NULL;
    }
    page->before = newest;
    page->used = args->size;
    args->pool->page = page;
    return page->bytes;
}

const char *cli_deploy_text_store(const CliDeployPoolCtx *args)
{
    char *const stored =
        EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = args->pool, .size = args->size + 1u);
    if (stored == NULL)
    {
        return "";
    }
    memor.cpy(&(MemoriaCfg){.dst = stored, .src = args->text, .bytes = (size_t)args->size});
    stored[args->size] = '\0';
    return stored;
}

int cli_deploy_order(const CliDeployLeftCtx *args)
{
    const size_t left_length = cellul.len(&(CatenaFinitaCfg){.src = args->left, .cap = CLI_POOL_BYTES});
    const size_t right_length = cellul.len(&(CatenaFinitaCfg){.src = args->right, .cap = CLI_POOL_BYTES});
    // the shorter text's terminator is compared too: a text sorts before a longer one that begins with it
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const embed_iword order = memor.cmp(&(MemoriaCfg){.src = args->left, .other = args->right, .bytes = shorter + 1u});
    return (order > 0) - (order < 0);
}

// the name serde gives a value's type
EMBED_INLINE const char *read_type(const DeployReadCtx *args)
{
    static const char *const types[7] = {"map", "sequence", "string", "integer", "boolean", "boolean", "null"};
    return types[args->json->node[args->node].type];
}

// that a value is not the type wanted reported, as serde reports it
EMBED_INLINE int read_invalid(const DeployReadCtx *args)
{
    mmgr_line_put(args->err_msg, "deploy.json: invalid type: ");
    mmgr_line_put(args->err_msg, read_type(args));
    mmgr_line_put(args->err_msg, ", expected ");
    mmgr_line_put(args->err_msg, args->required);
    return 0;
}

// that a field is missing reported, as serde reports it
EMBED_INLINE int read_missing(const DeployReadCtx *args)
{
    mmgr_line_put(args->err_msg, "deploy.json: missing field `");
    mmgr_line_put(args->err_msg, args->field);
    mmgr_line_ch(args->err_msg, '`');
    return 0;
}

// the texts of the list `field` of `entry` into `*texts`, placed in `pool` at their count, that count into `count`:
// 1, or 0 with why given
static int read_texts(const DeployReadCtx *args)
{
    const CliJson *json = args->json;
    CliDeployPool *pool = args->pool;
    const char ***texts = args->texts;
    unsigned int *count = args->count;
    const unsigned int list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->entry, .name = args->field);
    *texts = NULL;
    *count = 0u;
    if (list == CLI_JSON_NONE)
    {
        return 1;
    }
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = list,
                    .type = CFG_JSON_ARRAY))
    {
        return EMBED_CALL(read_invalid, DeployReadCtx, .json = json, .node = list, .required = "a sequence",
                          .err_msg = args->err_msg);
    }
    unsigned int items = 0u;
    for (; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = list,
                      .index = items) != CLI_JSON_NONE;
         items += 1u)
    {
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = list, .index = items);
        if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = item,
                        .type = CFG_JSON_STRING))
        {
            return EMBED_CALL(read_invalid, DeployReadCtx, .json = json, .node = item, .required = "a string",
                              .err_msg = args->err_msg);
        }
    }
    if (items == 0u)
    {
        return 1;
    }
    const char **const stored = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = pool,
                                           .size = (unsigned long long)items * sizeof(stored[0]));
    if (stored == NULL)
    {
        mmgr_line_put(args->err_msg, "deploy.json: ");
        mmgr_line_put(args->err_msg, args->field);
        mmgr_line_put(args->err_msg, " names more than orior_cli supports");
        return 0;
    }
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        stored[at] = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                   .json_read = json, .array = list, .index = at),
                                .length_pointer = NULL);
    }
    *texts = stored;
    *count = items;
    return 1;
}

// one deployment of the list read into `deployment`, its lists placed in `pool`: 1, or 0 with why given
EMBED_INLINE int read_deployment(const DeployReadCtx *args)
{
    const CliJson *json = args->json;
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = args->entry,
                    .type = CFG_JSON_OBJECT))
    {
        return EMBED_CALL(read_invalid, DeployReadCtx, .json = json, .node = args->entry,
                          .required = "struct Deployment", .err_msg = args->err_msg);
    }
    const unsigned int name = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->entry, .name = "name");
    const unsigned int from = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->entry, .name = "from");
    const unsigned int retain_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = args->entry, .name = "retain");
    const unsigned int in_position = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = args->entry, .name = "inPlace");
    if (name == CLI_JSON_NONE)
    {
        return EMBED_CALL(read_missing, DeployReadCtx, .field = "name", .err_msg = args->err_msg);
    }
    if (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = args->entry,
                   .name = "to") == CLI_JSON_NONE)
    {
        return EMBED_CALL(read_missing, DeployReadCtx, .field = "to", .err_msg = args->err_msg);
    }
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = name,
                    .type = CFG_JSON_STRING) ||
        ((from != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                .json_read = json, .node = from, .type = CFG_JSON_STRING)))
    {
        return EMBED_CALL(read_invalid, DeployReadCtx, .json = json,
                          .node = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                             .node = name, .type = CFG_JSON_STRING)
                                      ? from
                                      : name,
                          .required = "a string", .err_msg = args->err_msg);
    }
    args->deployment->name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = name, .length_pointer = NULL);
    args->deployment->from = (from != CLI_JSON_NONE)
                                 ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = json, .node = from, .length_pointer = NULL)
                                 : ".";
    args->deployment->retain = 5ULL;
    if (retain_node != CLI_JSON_NONE)
    {
        unsigned long long length = 0ULL;
        const char *const digits = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                              .json_read = json, .node = retain_node, .type = CFG_JSON_NUMBER)
                                       ? (json->text + json->node[retain_node].text)
                                       : "";
        length = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                            .node = retain_node, .type = CFG_JSON_NUMBER)
                     ? json->node[retain_node].length
                     : 0ULL;
        unsigned long long value = 0ULL;
        int whole = (length > 0u);
        unsigned long long at = 0ULL;
        while (whole && (at < length))
        {
            whole = (digits[at] >= '0') && (digits[at] <= '9') &&
                    (value <= (0xffffffffffffffffULL - (unsigned long long)(digits[at] - '0')) / 10u);
            value = whole ? ((value * 10u) + (unsigned long long)(digits[at] - '0')) : value;
            at += 1u;
        }
        if (!whole)
        {
            return EMBED_CALL(read_invalid, DeployReadCtx, .json = json, .node = retain_node, .required = "usize",
                              .err_msg = args->err_msg);
        }
        args->deployment->retain = value;
    }
    args->deployment->in_position = 0;
    if (in_position != CLI_JSON_NONE)
    {
        if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = in_position,
                        .type = CFG_JSON_TRUE) &&
            !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = in_position,
                        .type = CFG_JSON_FALSE))
        {
            return EMBED_CALL(read_invalid, DeployReadCtx, .json = json, .node = in_position, .required = "a boolean",
                              .err_msg = args->err_msg);
        }
        args->deployment->in_position = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                   .json_read = json, .node = in_position, .type = CFG_JSON_TRUE);
    }
    return EMBED_CALL(read_texts, DeployReadCtx, .json = json, .entry = args->entry, .field = "exclude",
                      .pool = args->pool, .texts = &args->deployment->exclude, .count = &args->deployment->excludes,
                      .err_msg = args->err_msg) &&
           EMBED_CALL(read_texts, DeployReadCtx, .json = json, .entry = args->entry, .field = "to", .pool = args->pool,
                      .texts = &args->deployment->to, .count = &args->deployment->tos, .err_msg = args->err_msg);
}

int cli_deploy_read(const CliDeployRootCtx *args)
{
    const CliDeployment **deployments = args->deployments;
    unsigned int *count = args->count_pointer;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "deploy.json", .joined = path);
    g_cli_guard->persistent_buf_release(args->tree->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), args->tree->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&args->tree->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    while (args->tree->pool.page != NULL)
    {
        CliDeployPage *const before = args->tree->pool.page->before;
        g_cli_guard->persistent_buf_release(args->tree->pool.page, g_cli_deploy_owner);
        args->tree->pool.page = before;
    }
    args->tree->pool.refused = 0;
    *deployments = NULL;
    *count = 0u;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = g_cli_deploy_owner);
    if ((bytes == NULL) ||
        !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = size))
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_deploy_owner));
        return 1;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {why, sizeof(why), 0ULL};
    const unsigned int top =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &args->tree->json,
                   .text = (const char *)bytes, .size = size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &read_err_msg, .owner = g_cli_deploy_owner);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_deploy_owner));
    const CliJson *const json = &args->tree->json;
    if (top == CLI_JSON_NONE)
    {
        mmgr_line_put(args->err_msg, "deploy.json: ");
        mmgr_line_put_n(args->err_msg, why, read_err_msg.at);
        return 0;
    }
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = top,
                    .type = CFG_JSON_OBJECT))
    {
        return EMBED_CALL(read_invalid, DeployReadCtx, .json = json, .node = top, .required = "struct Named",
                          .err_msg = args->err_msg);
    }
    const unsigned int list = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = top, .name = "deployments");
    if (list == CLI_JSON_NONE)
    {
        return EMBED_CALL(read_missing, DeployReadCtx, .field = "deployments", .err_msg = args->err_msg);
    }
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = list,
                    .type = CFG_JSON_ARRAY))
    {
        return EMBED_CALL(read_invalid, DeployReadCtx, .json = json, .node = list, .required = "a sequence",
                          .err_msg = args->err_msg);
    }
    unsigned int items = 0u;
    while (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = list,
                      .index = items) != CLI_JSON_NONE)
    {
        items += 1u;
    }
    if (items == 0u)
    {
        return 1;
    }
    CliDeployment *const read = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &args->tree->pool,
                                           .size = (unsigned long long)items * sizeof(read[0]));
    if (read == NULL)
    {
        mmgr_line_put(args->err_msg, "deploy.json names more deployments than orior_cli supports");
        return 0;
    }
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        if (!EMBED_CALL(read_deployment, DeployReadCtx, .json = json,
                        .entry = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = list, .index = at),
                        .pool = &args->tree->pool, .deployment = &read[at], .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    *deployments = read;
    *count = items;
    return 1;
}

// whether a letter is white as Rust's char::is_whitespace has it
EMBED_INLINE int read_white(const DeployReadCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

int cli_deploy_position(const CliDeployTextCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    unsigned long long start = 0ULL;
    unsigned long long end = 0ULL;
    unsigned int letter = 0u;
    for (unsigned long long at = 0ULL; at < length;)
    {
        const unsigned long long reserved =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)args->text, .length = length, .at = at, .letter = &letter);
        start = ((start == at) && EMBED_CALL(read_white, DeployReadCtx, .letter = letter)) ? (at + reserved) : start;
        at += reserved;
        end = EMBED_CALL(read_white, DeployReadCtx, .letter = letter) ? end : at;
    }
    end = (end < start) ? start : end;
    char trimmed[CLI_PATH_MAX_LEN];
    const unsigned long long size = (end - start < sizeof(trimmed)) ? (end - start) : (sizeof(trimmed) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = trimmed, .src = args->text + start, .bytes = (size_t)size});
    trimmed[size] = '\0';
    const int letter_drive =
        ((trimmed[0] >= 'a') && (trimmed[0] <= 'z')) || ((trimmed[0] >= 'A') && (trimmed[0] <= 'Z'));
    const int drive = (size >= 2u) && letter_drive && (trimmed[1] == ':') &&
                      ((size == 2u) || (trimmed[2] == '/') || (trimmed[2] == '\\'));
    if (!drive && (trimmed[0] != '/') && (trimmed[0] != '.') && (trimmed[0] != '\\'))
    {
        args->position_out->here = 0;
        return EMBED_CALL(cli_link_text.address_parse, CliLinkTextCfg, .text = trimmed,
                          .address_out = &args->position_out->address, .err_msg = args->err_msg);
    }
    args->position_out->here = 1;
    VerbaLinea path = {args->position_out->path, sizeof(args->position_out->path), 0ULL};
#if defined(_WIN32)
    // a path with a drive stands on its own, as Rust's join takes one, as does a share's
    const int whole =
        drive || (((trimmed[0] == '/') || (trimmed[0] == '\\')) && ((trimmed[1] == '/') || (trimmed[1] == '\\')));
    const int rooted = !whole && !drive && ((trimmed[0] == '/') || (trimmed[0] == '\\'));
    if (rooted && (args->root[0] != '\0') && (args->root[1] == ':'))
    {
        mmgr_line_put_n(&path, args->root, 2u);
    }
    else if (!whole && !rooted)
    {
        mmgr_line_put(&path, args->root);
        mmgr_line_ch(&path, '\\');
    }
#else
    if (trimmed[0] != '/')
    {
        mmgr_line_put(&path, args->root);
        mmgr_line_ch(&path, '/');
    }
#endif
    mmgr_line_put(&path, trimmed);
    if (mmgr_line_finish(&path) == 0ULL)
    {
        mmgr_line_put(args->err_msg, trimmed);
        mmgr_line_put(args->err_msg, ": its path is too long");
        return 0;
    }
    return 1;
}

void cli_deploy_position_name(const CliDeploySizeCtx *args)
{
    if (args->place->here)
    {
        mmgr_line_put(args->out, args->place->path);
    }
    else
    {
        EMBED_CALL(cli_link_out.address_text, CliLinkOutCfg, .address = &args->place->address, .out = args->out);
    }
}

const char *cli_deploy_position_folder(const CliDeploySizeCtx *args)
{
    return args->place->here ? args->place->path : args->place->address.folder;
}

int cli_deploy_remote_run(const CliDeploySizeCtx *args)
{
    if (args->place->here)
    {
        mmgr_line_put(args->err_msg, "a folder on this machine takes no shell line");
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea error_line = {why, sizeof(why) - 1u, 0ULL};
    if (EMBED_CALL(cli_link_size.remote_run, CliLinkSizeCfg, .address = &args->place->address, .line = args->line_text,
                   .input = args->input, .size = args->size, .out = args->out, .err_msg = &error_line))
    {
        return 1;
    }
    why[error_line.at] = '\0';
    mmgr_line_put_n(args->err_msg, why, error_line.at);
    static const char s_denied[] = "Permission denied";
    static const char s_password[] = "password";
    if (cellul.has(
            &(CatenaFinitaCfg){.src = why, .cap = error_line.at, .other = s_denied, .other_cap = sizeof(s_denied)}) ||
        cellul.has(
            &(CatenaFinitaCfg){.src = why, .cap = error_line.at, .other = s_password, .other_cap = sizeof(s_password)}))
    {
        mmgr_line_put(args->err_msg, "; ");
        EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = &args->place->address, .out = args->err_msg);
        mmgr_line_put(args->err_msg,
                      " prompts for a password, which orior_cli does not store: a key the system's ssh agent has "
                      "reaches it, and ssh-copy-id puts one there");
    }
    return 0;
}
