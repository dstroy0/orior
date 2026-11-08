// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_config.c: the connections a tree names in databases.json at its top folder, read, stored in place of one of the
// same name or renamed, and taken out with the password the keychain stored for them; each written as the window reads
// it, with whether it is open
#include "../command_line/pool.h"
#include "../text/text.h"
#include "db_internal.h"

// The writing of one connection: where it goes, whether it is laid out a member to a line, the indent of its braces,
// and whether a member is written yet
typedef struct
{
    VerbaLinea *out;
    int pretty;
    const char *indent;
    int written;
} CliDbConfigWriting;
// the most connections a tree names that are read, and the bytes databases.json is written in
#define CLI_DB_CONNECTIONS 256u
#define CLI_DB_CONFIG_CAPACITY (1ULL << 20u)

// the connections read, a cell of their count and one more for one saved, guarded under the lock from the read to
// config_release
static CliDbConnection *s_connections;

unsigned int cli_db_port(const CliDbConnectionOutTypeCfg *args)
{
    if (args->connection->port != 0u)
    {
        return args->connection->port;
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = args->connection->type, .other = "mysql", .cap = CLI_POOL_BYTES}))
               ? 3306u
               : 5432u;
}

void cli_db_account(const CliDbConnectionOutTypeCfg *args)
{
    mmgr_line_put(args->out, args->connection->type);
    mmgr_line_put(args->out, "://");
    mmgr_line_put(args->out, args->connection->user);
    mmgr_line_ch(args->out, '@');
    mmgr_line_put(args->out, args->connection->host);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, cli_db_connection_out_type.port(args), 1u);
    mmgr_line_ch(args->out, '/');
    mmgr_line_put(args->out, args->connection->database);
    if (args->connection->ssh[0] != '\0')
    {
        mmgr_line_put(args->out, " via ");
        mmgr_line_put(args->out, args->connection->ssh);
    }
}

typedef struct
{
    const CliJson *json;
    unsigned int node;
    const char *name;
    char *out;
    unsigned long long capacity;
    const char *root;
    VerbaLinea *err_msg;
    CliDbConfigWriting *writing;
    const char *text;
    int always;
    const CliDbConnection *connection;
    int pretty;
    const char *indent;
    VerbaLinea *out_verba_linea_pointer;
    unsigned int count;
} DbConfigCtx;
// a string member of the connection into `out`, sized for `capacity`, empty where it is none: 0 where it is there and
// is no string
EMBED_INLINE int config_text(const DbConfigCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const char *name = args->name;
    char *out = args->out;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = node, .name = name);
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = member, .length_pointer = NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = out, .text = text, .otherwise = "");
    return (member == CLI_JSON_NONE) || (text != NULL);
}

int cli_db_connection_get(const CliDbConnectionErrMsgCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->connection_out, .val = 0, .bytes = sizeof(*args->connection_out)});
    if ((EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                       .object = args->node, .name = "name"),
                    .length_pointer = NULL) == NULL) ||
        (EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                       .object = args->node, .name = "kind"),
                    .length_pointer = NULL) == NULL))
    {
        mmgr_line_put(args->err_msg, "a connection names no name or no kind");
        return 0;
    }
    const unsigned int port = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->node, .name = "port");
    const char *const digits = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                          .json_read = args->json, .node = port, .type = CFG_JSON_NUMBER)
                                   ? args->json->text + args->json->node[port].text
                                   : NULL;
    unsigned long long value = 0ULL;
    int whole = (port == CLI_JSON_NONE) || ((digits != NULL) && (args->json->node[port].length <= 5ULL));
    for (unsigned long long at = 0ULL; (digits != NULL) && whole && (at < args->json->node[port].length); at += 1ULL)
    {
        whole = (digits[at] >= '0') && (digits[at] <= '9');
        value = (value * 10ULL) + (unsigned long long)(digits[at] - '0');
    }
    const unsigned int matters = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = args->json, .object = args->node, .name = "matters");
    const int texts =
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "name",
                   .out = args->connection_out->name, .capacity = sizeof(args->connection_out->name)) &&
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "kind",
                   .out = args->connection_out->type, .capacity = sizeof(args->connection_out->type)) &&
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "host",
                   .out = args->connection_out->host, .capacity = sizeof(args->connection_out->host)) &&
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "user",
                   .out = args->connection_out->user, .capacity = sizeof(args->connection_out->user)) &&
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "database",
                   .out = args->connection_out->database, .capacity = sizeof(args->connection_out->database)) &&
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "file",
                   .out = args->connection_out->file, .capacity = sizeof(args->connection_out->file)) &&
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "ssh",
                   .out = args->connection_out->ssh, .capacity = sizeof(args->connection_out->ssh)) &&
        EMBED_CALL(config_text, DbConfigCtx, .json = args->json, .node = args->node, .name = "guard",
                   .out = args->connection_out->guard, .capacity = sizeof(args->connection_out->guard));
    if (!texts || !whole || (value > 65535ULL) ||
        ((matters != CLI_JSON_NONE) &&
         !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json, .node = matters,
                     .type = CFG_JSON_TRUE) &&
         !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json, .node = matters,
                     .type = CFG_JSON_FALSE)))
    {
        mmgr_line_put(args->err_msg, "a connection names a setting of the wrong kind");
        return 0;
    }
    args->connection_out->port = (unsigned int)value;
    args->connection_out->matters = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                               .json_read = args->json, .node = matters, .type = CFG_JSON_TRUE);
    return 1;
}

// the connections databases.json at the tree's top folder names, under the lock, in a cell of their count and one
// more, given back with config_release either way: their count, 0 where it is not there; -1 with why given where it
// does not read or the cellblock could not meet them
static int config_read(const DbConfigCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "databases.json", .joined = path);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = g_cli_db_owner);
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea read_err_msg = {why, sizeof(why), 0ULL};
    // the file's bytes read as the text they are
    const unsigned int root_node =
        (bytes != NULL) ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                     .json = &json, .text = (const char *)bytes, .size = size, .nodes_more = 0u,
                                     .text_more = 0ULL, .err_msg = &read_err_msg, .owner = g_cli_db_owner)
                        : CLI_JSON_NONE;
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_db_owner));
    const unsigned int connections = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &json, .object = root_node, .name = "connections");
    unsigned int named = 0u;
    while ((root_node != CLI_JSON_NONE) && (named < CLI_DB_CONNECTIONS) &&
           (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                       .array = connections, .index = named) != CLI_JSON_NONE))
    {
        named += 1u;
    }
    s_connections = g_cli_guard->persistent_buf_alloc((named + 1ULL) * sizeof(*s_connections), g_cli_db_owner);
    if (s_connections == NULL)
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        mmgr_line_put(err_msg, "databases.json: the cells cannot meet its connections");
        return -1;
    }
    if (bytes == NULL)
    {
        return 0;
    }
    int count = 0;
    for (unsigned int at = 0u; (root_node != CLI_JSON_NONE) &&
                               (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = &json, .array = connections, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        if ((count < (int)CLI_DB_CONNECTIONS) &&
            !EMBED_CALL(cli_db_connection_err_msg.connection_get, CliDbConnectionErrMsgCfg, .json = &json,
                        .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = &json, .array = connections, .index = at),
                        .connection_out = &s_connections[count], .err_msg = &read_err_msg))
        {
            break;
        }
        count += (count < (int)CLI_DB_CONNECTIONS) ? 1 : 0;
    }
    const int object =
        (root_node != CLI_JSON_NONE) &&
        EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json, .node = root_node,
                   .type = CFG_JSON_OBJECT) &&
        ((connections == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                                      .json_read = &json, .node = connections, .type = CFG_JSON_ARRAY));
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if (!object || (read_err_msg.at > 0ULL))
    {
        mmgr_line_put(err_msg, "databases.json: ");
        mmgr_line_put_n(err_msg, read_err_msg.out, read_err_msg.at);
        if (read_err_msg.at == 0ULL)
        {
            mmgr_line_put(err_msg, "it is not an object of connections");
        }
        return -1;
    }
    return count;
}

// a member's name written, after the one before it
EMBED_INLINE void config_member(const DbConfigCtx *args)
{
    const char *const after_value = args->writing->pretty ? ",\n" : ",";
    const char *const after_open = args->writing->pretty ? "\n" : "";
    mmgr_line_put(args->writing->out, args->writing->written ? after_value : after_open);
    mmgr_line_put(args->writing->out, args->writing->pretty ? args->writing->indent : "");
    mmgr_line_put(args->writing->out, args->writing->pretty ? "  \"" : "\"");
    mmgr_line_put(args->writing->out, args->name);
    mmgr_line_put(args->writing->out, args->writing->pretty ? "\": " : "\":");
    args->writing->written = 1;
}

// a text member written where it is set, or always where `always`
EMBED_INLINE void config_text_write(const DbConfigCtx *args)
{
    const char *text = args->text;
    if (args->always || (text[0] != '\0'))
    {
        config_member(args);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->writing->out,
                   .text = text, .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}));
    }
}

// a connection written as databases.json stores it, in the order of its settings, those it leaves unset left out, a
// member to a line at `indent` spaces in where `pretty`, else on one line
EMBED_INLINE void config_on_write(const DbConfigCtx *args)
{
    CliDbConfigWriting writing = {args->out_verba_linea_pointer, args->pretty, args->indent, 0};
    mmgr_line_ch(args->out_verba_linea_pointer, '{');
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "name", .text = args->connection->name,
               .always = 1);
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "kind", .text = args->connection->type,
               .always = 1);
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "host", .text = args->connection->host,
               .always = 0);
    if (args->connection->port != 0u)
    {
        EMBED_CALL(config_member, DbConfigCtx, .writing = &writing, .name = "port");
        mmgr_line_decimal(args->out_verba_linea_pointer, args->connection->port, 1u);
    }
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "user", .text = args->connection->user,
               .always = 0);
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "database",
               .text = args->connection->database, .always = 0);
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "file", .text = args->connection->file,
               .always = 0);
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "ssh", .text = args->connection->ssh,
               .always = 0);
    if (args->connection->matters)
    {
        EMBED_CALL(config_member, DbConfigCtx, .writing = &writing, .name = "matters");
        mmgr_line_put(args->out_verba_linea_pointer, "true");
    }
    EMBED_CALL(config_text_write, DbConfigCtx, .writing = &writing, .name = "guard", .text = args->connection->guard,
               .always = 0);
    mmgr_line_put(args->out_verba_linea_pointer, args->pretty ? "\n" : "");
    mmgr_line_put(args->out_verba_linea_pointer, args->pretty ? args->indent : "");
    mmgr_line_ch(args->out_verba_linea_pointer, '}');
}

// the connections, `count` of them, written to databases.json as serde_json's pretty writer lays them out, under the
// lock: 1, or 0 with why given
EMBED_INLINE int config_write(const DbConfigCtx *args)
{
    const char *root = args->root;
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "databases.json", .joined = path);
    char *const out_cell = g_cli_guard->temporary_buf_alloc(CLI_DB_CONFIG_CAPACITY, g_cli_db_owner);
    VerbaLinea out = mmgr_line_over(out_cell, CLI_DB_CONFIG_CAPACITY);
    mmgr_line_put(&out, (args->count == 0u) ? "{\n  \"connections\": []\n}\n" : "{\n  \"connections\": [\n");
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&out, "    ");
        EMBED_CALL(config_on_write, DbConfigCtx, .connection = &s_connections[at], .pretty = 1, .indent = "    ",
                   .out_verba_linea_pointer = &out);
        mmgr_line_put(&out, (at + 1u < args->count) ? ",\n" : "\n  ]\n}\n");
    }
    const int written =
        mmgr_line_ok(&out) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                                         .bytes = out.out, .size = out.at);
    g_cli_guard->persistent_buf_release(out.out, g_cli_db_owner);
    if (!written)
    {
        mmgr_line_put(args->err_msg, "databases.json: it could not be written");
        return 0;
    }
    return 1;
}

int cli_db_by_name(const CliDbRootNameCfg *args)
{
    CliDbConnection *connection = args->connection_out;
    const int count = EMBED_CALL(config_read, DbConfigCtx, .root = args->root, .err_msg = args->err_msg);
    for (int at = 0; at < count; at += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_connections[at].name, .other = args->name, .cap = CLI_POOL_BYTES}))
        {
            *connection = s_connections[at];
            g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
            s_connections = NULL;
            return 1;
        }
    }
    g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
    s_connections = NULL;
    if (count >= 0)
    {
        mmgr_line_put(args->err_msg, "databases.json names no connection ");
        mmgr_line_put(args->err_msg, args->name);
    }
    return 0;
}

int cli_db_save(const CliDbConnectionCfg *args)
{
    unsigned long long start = 0ULL;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = args->connection->name, .cap = CLI_POOL_BYTES});
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = args->connection->name[start]}) != NULL))
    {
        start += 1ULL;
    }
    if (start == end)
    {
        mmgr_line_put(args->err_msg, "a connection needs a name");
        return 0;
    }
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->connection->type, .other = "postgres", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->connection->type, .other = "mysql", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->connection->type, .other = "sqlite", .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(args->err_msg, args->connection->type);
        mmgr_line_put(args->err_msg, " is no kind of database orior_cli supports: postgres, mysql or sqlite");
        return 0;
    }
    const int count = EMBED_CALL(config_read, DbConfigCtx, .root = args->root, .err_msg = args->err_msg);
    if (count < 0)
    {
        g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
        s_connections = NULL;
        return 0;
    }
    const char *const old = (args->was != NULL) ? args->was : args->connection->name;
    int found = -1;
    for (int at = 0; at < count; at += 1)
    {
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = old, .other = args->connection->name, .cap = CLI_POOL_BYTES})) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = s_connections[at].name, .other = args->connection->name, .cap = CLI_POOL_BYTES})))
        {
            g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
            s_connections = NULL;
            mmgr_line_put(args->err_msg, "the tree names a connection ");
            mmgr_line_put(args->err_msg, args->connection->name);
            mmgr_line_put(args->err_msg, " already");
            return 0;
        }
        found = ((found < 0) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_connections[at].name, .other = old, .cap = CLI_POOL_BYTES})))
                    ? at
                    : found;
    }
    if ((found < 0) && (count == (int)CLI_DB_CONNECTIONS))
    {
        g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
        s_connections = NULL;
        mmgr_line_put(args->err_msg, "databases.json names as many connections as orior_cli reads");
        return 0;
    }
    s_connections[(found >= 0) ? found : count] = *args->connection;
    const int written =
        EMBED_CALL(config_write, DbConfigCtx, .root = args->root,
                   .count = (found >= 0) ? (unsigned int)count : (unsigned int)count + 1u, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
    s_connections = NULL;
    return written;
}

int cli_db_remove(const CliDbRootNameCfg *args)
{
    const int count = EMBED_CALL(config_read, DbConfigCtx, .root = args->root, .err_msg = args->err_msg);
    if (count < 0)
    {
        g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
        s_connections = NULL;
        return 0;
    }
    unsigned int remaining = 0u;
    for (int at = 0; at < count; at += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_connections[at].name, .other = args->name, .cap = CLI_POOL_BYTES}))
        {
            char account[CLI_PATH_MAX_LEN];
            char why[CLI_ERR_MSG_MAX_LEN];
            VerbaLinea account_line = {account, sizeof(account), 0ULL};
            VerbaLinea forgot = {why, sizeof(why), 0ULL};
            EMBED_CALL(cli_db_connection_out_type.account, CliDbConnectionOutTypeCfg, .connection = &s_connections[at],
                       .out = &account_line);
            mmgr_line_finish(&account_line);
            (void)EMBED_CALL(cli_err_msg_service_internal.keychain_forget, CliErrMsgServiceCtx, .service = "database",
                             .account = account, .err_msg = &forgot);
            continue;
        }
        s_connections[remaining] = s_connections[at];
        remaining += 1u;
    }
    const int written =
        EMBED_CALL(config_write, DbConfigCtx, .root = args->root, .count = remaining, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
    s_connections = NULL;
    return written;
}

int cli_db_list(const CliDbOutRootCfg *args)
{
    const int read = EMBED_CALL(config_read, DbConfigCtx, .root = args->root, .err_msg = args->err_msg);
    if (read < 0)
    {
        g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
        s_connections = NULL;
        return 0;
    }
    mmgr_line_ch(args->out, '[');
    for (int at = 0; at < read; at += 1)
    {
        const CliDbConnection *const connection = &s_connections[at];
        CliDbOpen *opened = NULL;
        for (unsigned int one = 0u; (one < args->count) && (opened == NULL); one += 1u)
        {
            opened =
                (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_db.name, CliDbCfg, .open_read = args->opens[one]),
                                              .other = connection->name,
                                              .cap = CLI_POOL_BYTES}))
                    ? args->opens[one]
                    : NULL;
        }
        char account[CLI_PATH_MAX_LEN];
        char password[CLI_NAME_MAX_LEN];
        VerbaLinea account_line = {account, sizeof(account), 0ULL};
        EMBED_CALL(cli_db_connection_out_type.account, CliDbConnectionOutTypeCfg, .connection = connection,
                   .out = &account_line);
        mmgr_line_finish(&account_line);
        const int list_entries =
            (!cellul.eq(&(CatenaFinitaCfg){.src = connection->type, .other = "sqlite", .cap = CLI_POOL_BYTES})) &&
            EMBED_CALL(cli_service_internal.keychain_read, CliServiceCtx, .service = "database", .account = account,
                       .out = password, .capacity = sizeof(password));
        mmgr_line_put(args->out, (at > 0) ? ",{\"connection\":" : "{\"connection\":");
        EMBED_CALL(config_on_write, DbConfigCtx, .connection = connection, .pretty = 0, .indent = "",
                   .out_verba_linea_pointer = args->out);
        mmgr_line_put(args->out, (opened != NULL) ? ",\"open\":true,\"version\":" : ",\"open\":false,\"version\":null");
        if (opened != NULL)
        {
            const char *const version = EMBED_CALL(cli_db.version, CliDbCfg, .open_read = opened);
            const char *const transaction = EMBED_CALL(cli_db_err_msg.transaction, CliDbErrMsgCfg, .open = opened);
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = version,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = version, .cap = CLI_POOL_BYTES}));
            mmgr_line_put(args->out, ",\"transaction\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = transaction,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = transaction, .cap = CLI_POOL_BYTES}));
            mmgr_line_put(args->out, ",\"zone\":");
            char zone[CLI_NAME_MAX_LEN];
            VerbaLinea zone_line = {zone, sizeof(zone), 0ULL};
            EMBED_CALL(cli_db_out_count.zone, CliDbOutCountCfg, .open = opened, .out = &zone_line);
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = zone_line.out, .length = zone_line.at);
        }
        else
        {
            mmgr_line_put(args->out, ",\"transaction\":null,\"zone\":null");
        }
        mmgr_line_put(args->out, list_entries ? ",\"passwordKept\":true}" : ",\"passwordKept\":false}");
    }
    mmgr_line_ch(args->out, ']');
    g_cli_guard->persistent_buf_release(s_connections, g_cli_db_owner);
    s_connections = NULL;
    return 1;
}
