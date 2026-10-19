// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_config.c: the connections a tree names in databases.json at its top folder, read, kept in place of one of the same
// name or renamed, and taken out with the password the keychain kept for them; each written as the window reads it,
// with whether it is open
#include "db_internal.h"

// the most connections a tree names that are read, and the nodes and bytes databases.json is read in
#define CLI_DB_CONNECTIONS 256u
#define CLI_DB_CONFIG_NODES 16384u
#define CLI_DB_CONFIG_TEXT (1u << 20u)

static CliDbConnection s_connections[CLI_DB_CONNECTIONS];
static CliJsonNode s_nodes[CLI_DB_CONFIG_NODES];
static CfgJsonToken s_tokens[CLI_DB_CONFIG_NODES];
static char s_text[CLI_DB_CONFIG_TEXT];
static char s_written[CLI_DB_CONFIG_TEXT];
static CliLock s_lock = CLI_LOCK_INIT;

unsigned int cli_db_port(const CliDbConnection *connection)
{
    if (connection->port != 0u)
    {
        return connection->port;
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = connection->kind, .other = "mysql", .cap = CLI_CELLS_POOL})) ? 3306u
                                                                                                             : 5432u;
}

void cli_db_account(const CliDbConnection *connection, CliLine *out)
{
    cli_line_text(out, connection->kind);
    cli_line_text(out, "://");
    cli_line_text(out, connection->user);
    cli_line_character(out, '@');
    cli_line_text(out, connection->host);
    cli_line_character(out, ':');
    cli_line_decimal(out, cli_db_port(connection), 1u);
    cli_line_character(out, '/');
    cli_line_text(out, connection->database);
    if (connection->ssh[0] != '\0')
    {
        cli_line_text(out, " via ");
        cli_line_text(out, connection->ssh);
    }
}

// a string member of the connection into `out`, which holds `room`, empty where it is none: 0 where it is there and is
// no string
static int config_text(const CliJson *json, unsigned int node, const char *name, char *out, unsigned long long room)
{
    const unsigned int member = cli_json_member(json, node, name);
    const char *const text = cli_json_string(json, member, NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = out,
        .src = (text != NULL) ? text : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (text != NULL) ? text : "", .cap = CLI_CELLS_POOL}) + 1u});
    return (member == CLI_JSON_NONE) || (text != NULL);
}

int cli_db_connection_of(const CliJson *json, unsigned int node, CliDbConnection *connection, CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = connection, .val = 0, .bytes = sizeof(*connection)});
    if ((cli_json_string(json, cli_json_member(json, node, "name"), NULL) == NULL) ||
        (cli_json_string(json, cli_json_member(json, node, "kind"), NULL) == NULL))
    {
        cli_line_text(said, "a connection names no name or no kind");
        return 0;
    }
    const unsigned int port = cli_json_member(json, node, "port");
    const char *const digits = cli_json_is(json, port, CFG_JSON_NUMBER) ? json->text + json->node[port].text : NULL;
    unsigned long long value = 0ull;
    int whole = (port == CLI_JSON_NONE) || ((digits != NULL) && (json->node[port].length <= 5ull));
    for (unsigned long long at = 0ull; (digits != NULL) && whole && (at < json->node[port].length); at += 1ull)
    {
        whole = (digits[at] >= '0') && (digits[at] <= '9');
        value = (value * 10ull) + (unsigned long long)(digits[at] - '0');
    }
    const unsigned int matters = cli_json_member(json, node, "matters");
    const int texts = config_text(json, node, "name", connection->name, sizeof(connection->name)) &&
                      config_text(json, node, "kind", connection->kind, sizeof(connection->kind)) &&
                      config_text(json, node, "host", connection->host, sizeof(connection->host)) &&
                      config_text(json, node, "user", connection->user, sizeof(connection->user)) &&
                      config_text(json, node, "database", connection->database, sizeof(connection->database)) &&
                      config_text(json, node, "file", connection->file, sizeof(connection->file)) &&
                      config_text(json, node, "ssh", connection->ssh, sizeof(connection->ssh)) &&
                      config_text(json, node, "guard", connection->guard, sizeof(connection->guard));
    if (!texts || !whole || (value > 65535ull) ||
        ((matters != CLI_JSON_NONE) && !cli_json_is(json, matters, CFG_JSON_TRUE) &&
         !cli_json_is(json, matters, CFG_JSON_FALSE)))
    {
        cli_line_text(said, "a connection names a setting of the wrong kind");
        return 0;
    }
    connection->port = (unsigned int)value;
    connection->matters = cli_json_is(json, matters, CFG_JSON_TRUE);
    return 1;
}

// the connections databases.json at the tree's top folder names, under the lock: their count, 0 where it is not
// there; -1 with why said where it does not read
static int config_read(const char *root, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, "databases.json", path);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if (bytes == NULL)
    {
        return 0;
    }
    CliJson json;
    char why[CLI_SAID_LONGEST];
    CliLine read_said = {why, sizeof(why), 0ull};
    cli_json_begun(&json, s_nodes, CLI_DB_CONFIG_NODES, s_text, sizeof(s_text));
    const unsigned int root_node =
        cli_json_read(&json, (const char *)bytes, size, s_tokens, CLI_DB_CONFIG_NODES, &read_said);
    qry_member_file_unmapped(bytes, size);
    const unsigned int connections = cli_json_member(&json, root_node, "connections");
    int count = 0;
    for (unsigned int at = 0u; (root_node != CLI_JSON_NONE) && (cli_json_item(&json, connections, at) != CLI_JSON_NONE);
         at += 1u)
    {
        if ((count < (int)CLI_DB_CONNECTIONS) &&
            !cli_db_connection_of(&json, cli_json_item(&json, connections, at), &s_connections[count], &read_said))
        {
            break;
        }
        count += (count < (int)CLI_DB_CONNECTIONS) ? 1 : 0;
    }
    if ((root_node == CLI_JSON_NONE) || !cli_json_is(&json, root_node, CFG_JSON_OBJECT) ||
        ((connections != CLI_JSON_NONE) && !cli_json_is(&json, connections, CFG_JSON_ARRAY)) || (read_said.at > 0ull))
    {
        cli_line_text(said, "databases.json: ");
        cli_line_counted(said, read_said.out, read_said.at);
        if (read_said.at == 0ull)
        {
            cli_line_text(said, "it is not an object of connections");
        }
        return -1;
    }
    return count;
}

// The writing of one connection: where it goes, whether it is laid out a member to a line, the indent of its braces,
// and whether a member is written yet
typedef struct
{
    CliLine *out;
    int pretty;
    const char *indent;
    int written;
} CliDbConfigWriting;

// a member's name written, after the one before it
static void config_member(CliDbConfigWriting *writing, const char *name)
{
    cli_line_text(writing->out, writing->written ? (writing->pretty ? ",\n" : ",") : (writing->pretty ? "\n" : ""));
    cli_line_text(writing->out, writing->pretty ? writing->indent : "");
    cli_line_text(writing->out, writing->pretty ? "  \"" : "\"");
    cli_line_text(writing->out, name);
    cli_line_text(writing->out, writing->pretty ? "\": " : "\":");
    writing->written = 1;
}

// a text member written where it is set, or always where `always`
static void config_text_written(CliDbConfigWriting *writing, const char *name, const char *text, int always)
{
    if (always || (text[0] != '\0'))
    {
        config_member(writing, name);
        cli_json_quoted(writing->out, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    }
}

// a connection written as databases.json keeps it, in the order of its settings, those it leaves unset left out, a
// member to a line at `indent` spaces in where `pretty`, else on one line
static void config_written(const CliDbConnection *connection, int pretty, const char *indent, CliLine *out)
{
    CliDbConfigWriting writing = {out, pretty, indent, 0};
    cli_line_character(out, '{');
    config_text_written(&writing, "name", connection->name, 1);
    config_text_written(&writing, "kind", connection->kind, 1);
    config_text_written(&writing, "host", connection->host, 0);
    if (connection->port != 0u)
    {
        config_member(&writing, "port");
        cli_line_decimal(out, connection->port, 1u);
    }
    config_text_written(&writing, "user", connection->user, 0);
    config_text_written(&writing, "database", connection->database, 0);
    config_text_written(&writing, "file", connection->file, 0);
    config_text_written(&writing, "ssh", connection->ssh, 0);
    if (connection->matters)
    {
        config_member(&writing, "matters");
        cli_line_text(out, "true");
    }
    config_text_written(&writing, "guard", connection->guard, 0);
    cli_line_text(out, pretty ? "\n" : "");
    cli_line_text(out, pretty ? indent : "");
    cli_line_character(out, '}');
}

// the connections, `count` of them, written to databases.json as serde_json's pretty writer lays them out, under the
// lock: 1, or 0 with why said
static int config_write(const char *root, unsigned int count, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, "databases.json", path);
    CliLine out = {s_written, sizeof(s_written), 0ull};
    cli_line_text(&out, (count == 0u) ? "{\n  \"connections\": []\n}\n" : "{\n  \"connections\": [\n");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&out, "    ");
        config_written(&s_connections[at], 1, "    ", &out);
        cli_line_text(&out, (at + 1u < count) ? ",\n" : "\n  ]\n}\n");
    }
    if (!cli_line_fits(&out) || !query_file_written(path, out.out, out.at))
    {
        cli_line_text(said, "databases.json: it could not be written");
        return 0;
    }
    return 1;
}

int cli_db_named(const char *root, const char *name, CliDbConnection *connection, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int count = config_read(root, said);
    for (int at = 0; at < count; at += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_connections[at].name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            *connection = s_connections[at];
            cli_lock_give(&s_lock);
            return 1;
        }
    }
    cli_lock_give(&s_lock);
    if (count >= 0)
    {
        cli_line_text(said, "databases.json names no connection ");
        cli_line_text(said, name);
    }
    return 0;
}

int cli_db_save(const char *root, const CliDbConnection *connection, const char *was, CliLine *said)
{
    unsigned long long start = 0ull;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = connection->name, .cap = CLI_CELLS_POOL});
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = connection->name[start]}) != NULL))
    {
        start += 1ull;
    }
    if (start == end)
    {
        cli_line_text(said, "a connection needs a name");
        return 0;
    }
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = connection->kind, .other = "postgres", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = connection->kind, .other = "mysql", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = connection->kind, .other = "sqlite", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, connection->kind);
        cli_line_text(said, " is no kind of database orior speaks: postgres, mysql or sqlite");
        return 0;
    }
    cli_lock_take(&s_lock);
    const int count = config_read(root, said);
    if (count < 0)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    const char *const old = (was != NULL) ? was : connection->name;
    int found = -1;
    for (int at = 0; at < count; at += 1)
    {
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = old, .other = connection->name, .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = s_connections[at].name, .other = connection->name, .cap = CLI_CELLS_POOL})))
        {
            cli_lock_give(&s_lock);
            cli_line_text(said, "the tree names a connection ");
            cli_line_text(said, connection->name);
            cli_line_text(said, " already");
            return 0;
        }
        found = ((found < 0) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_connections[at].name, .other = old, .cap = CLI_CELLS_POOL})))
                    ? at
                    : found;
    }
    if ((found < 0) && (count == (int)CLI_DB_CONNECTIONS))
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "databases.json names as many connections as orior reads");
        return 0;
    }
    s_connections[(found >= 0) ? found : count] = *connection;
    const int written = config_write(root, (found >= 0) ? (unsigned int)count : (unsigned int)count + 1u, said);
    cli_lock_give(&s_lock);
    return written;
}

int cli_db_remove(const char *root, const char *name, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int count = config_read(root, said);
    if (count < 0)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    unsigned int kept = 0u;
    for (int at = 0; at < count; at += 1)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_connections[at].name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            char account[CLI_PATH_LONGEST];
            char why[CLI_SAID_LONGEST];
            CliLine account_line = {account, sizeof(account), 0ull};
            CliLine forgot = {why, sizeof(why), 0ull};
            cli_db_account(&s_connections[at], &account_line);
            cli_line_closed(&account_line);
            (void)cli_keychain_forget(account, &forgot);
            continue;
        }
        s_connections[kept] = s_connections[at];
        kept += 1u;
    }
    const int written = config_write(root, kept, said);
    cli_lock_give(&s_lock);
    return written;
}

int cli_db_listed(const char *root, CliDbOpen *const *opens, unsigned int count, CliLine *out, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int read = config_read(root, said);
    if (read < 0)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    cli_line_character(out, '[');
    for (int at = 0; at < read; at += 1)
    {
        const CliDbConnection *const connection = &s_connections[at];
        CliDbOpen *opened = NULL;
        for (unsigned int one = 0u; (one < count) && (opened == NULL); one += 1u)
        {
            opened = (cellul.eq(&(CatenaFinitaCfg){
                         .src = cli_db_name(opens[one]), .other = connection->name, .cap = CLI_CELLS_POOL}))
                         ? opens[one]
                         : NULL;
        }
        char account[CLI_PATH_LONGEST];
        char password[CLI_NAME_LONGEST];
        CliLine account_line = {account, sizeof(account), 0ull};
        cli_db_account(connection, &account_line);
        cli_line_closed(&account_line);
        const int kept =
            (!cellul.eq(&(CatenaFinitaCfg){.src = connection->kind, .other = "sqlite", .cap = CLI_CELLS_POOL})) &&
            cli_keychain_read(account, password, sizeof(password));
        cli_line_text(out, (at > 0) ? ",{\"connection\":" : "{\"connection\":");
        config_written(connection, 0, "", out);
        cli_line_text(out, (opened != NULL) ? ",\"open\":true,\"version\":" : ",\"open\":false,\"version\":null");
        if (opened != NULL)
        {
            const char *const version = cli_db_version(opened);
            const char *const transaction = cli_db_transaction(opened);
            cli_json_quoted(out, version, cellul.len(&(CatenaFinitaCfg){.src = version, .cap = CLI_CELLS_POOL}));
            cli_line_text(out, ",\"transaction\":");
            cli_json_quoted(out, transaction,
                            cellul.len(&(CatenaFinitaCfg){.src = transaction, .cap = CLI_CELLS_POOL}));
            cli_line_text(out, ",\"zone\":");
            char zone[CLI_NAME_LONGEST];
            CliLine zone_line = {zone, sizeof(zone), 0ull};
            cli_db_zone(opened, &zone_line);
            cli_json_quoted(out, zone_line.out, zone_line.at);
        }
        else
        {
            cli_line_text(out, ",\"transaction\":null,\"zone\":null");
        }
        cli_line_text(out, kept ? ",\"passwordKept\":true}" : ",\"passwordKept\":false}");
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
    return 1;
}
