// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_sign.c: a MySQL connection made and let go: the server's greeting read, the answer to its scramble sent as the
// account's plugin requires, a switch to another plugin followed, and the password sealed by the server's RSA key where
// caching_sha2_password requires it whole over a connection that is not encrypted
#include "../command_line/pool.h"
#include "../machine/net.h"
#include "../text/json_tree.h"
#include "mysql_internal.h"

static CliMysql *s_links[CLI_MYSQL_LINKS];

typedef struct
{
    CliMysql *mysql;
    const char *host;
    unsigned int port;
    unsigned char *scramble;
    unsigned long long *scramble_size;
    char *plugin;
    CliDbFailure *failure;
    VerbaLinea *out;
    unsigned long long value;
    const char *user;
    const char *database;
    const unsigned char *answer;
    unsigned int answer_size;
    const char *plugin_read;
    const char *password;
    const unsigned char *scramble_read;
    unsigned long long scramble_size_value;
} MysqlSignCtx;
// the greeting's scramble and plugin read: the scramble into `scramble`, sized for 64 bytes, its length into
// `scramble_size`, and the plugin's name into `plugin`: 1, or 0 with the failure given
EMBED_INLINE int sign_greeting(const MysqlSignCtx *args)
{
    unsigned long long *scramble_size = args->scramble_size;
    CliDbFailure *failure = args->failure;
    const unsigned char *const hello = args->mysql->packet;
    const unsigned long long size = args->mysql->size;
    if ((size > 0u) && (hello[0] == 0xffu))
    {
        EMBED_CALL(cli_mysql_failure_internal.failure_get, CliMysqlFailureCtx, .payload = hello, .size = size,
                   .failure = failure);
        return 0;
    }
    if ((size == 0u) || (hello[0] != 10u))
    {
        char why[CLI_NAME_MAX_LEN + 64u];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        mmgr_line_put(&err_msg, args->host);
        mmgr_line_ch(&err_msg, ':');
        mmgr_line_decimal(&err_msg, args->port, 1u);
        mmgr_line_put(&err_msg, " does not support MySQL's protocol 10");
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = why);
        return 0;
    }
    const unsigned char *const nul =
        memor.chr(&(MemoriaCfg){.src = hello + 1u, .val = 0, .bytes = (embed_word)(size - 1u)});
    unsigned long long at = (nul != NULL) ? (unsigned long long)(nul - hello) : size;
    if (at + 1u + 4u + 9u + 2u + 5u + 1u + 10u > size)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the server's greeting is cut short");
        return 0;
    }
    VerbaLinea version = {args->mysql->version, sizeof(args->mysql->version), 0ULL};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &version, .text_bytes = hello + 1u,
               .length = at - 1u);
    mmgr_line_finish(&version);
    at += 1u;
    args->mysql->cancel.id = (unsigned int)hello[at] | ((unsigned int)hello[at + 1u] << 8u) |
                             ((unsigned int)hello[at + 2u] << 16u) | ((unsigned int)hello[at + 3u] << 24u);
    at += 4u;
    memor.cpy(&(MemoriaCfg){.dst = args->scramble, .src = hello + at, .bytes = 8u});
    *scramble_size = 8ULL;
    at += 9u;
    const unsigned int lower = (unsigned int)hello[at] | ((unsigned int)hello[at + 1u] << 8u);
    at += 5u;
    const unsigned int upper = (unsigned int)hello[at] | ((unsigned int)hello[at + 1u] << 8u);
    const unsigned int capabilities = lower | (upper << 16u);
    at += 2u;
    const unsigned long long data_length = hello[at];
    at += 11u;
    // the scramble's second part: what the length names past its first 8 bytes, 13 at the least
    const unsigned long long rest = ((data_length > 8u) && (data_length - 8u > 13u)) ? data_length - 8u : 13ULL;
    if ((at + rest <= size) && (rest <= 56u))
    {
        const unsigned long long more = ((rest > 0u) && (hello[at + rest - 1u] == 0u)) ? rest - 1u : rest;
        memor.cpy(&(MemoriaCfg){.dst = args->scramble + 8u, .src = hello + at, .bytes = (embed_word)more});
        *scramble_size += more;
        at += rest;
    }
    const unsigned char *const plugin_end =
        (at < size) ? memor.chr(&(MemoriaCfg){.src = hello + at, .val = 0, .bytes = (embed_word)(size - at)}) : NULL;
    unsigned long long plugin_size;
    if (plugin_end != NULL)
    {
        plugin_size = (unsigned long long)(plugin_end - hello) - at;
    }
    else if (at < size)
    {
        plugin_size = size - at;
    }
    else
    {
        plugin_size = 0ULL;
    }
    VerbaLinea plugin_line = {args->plugin, CLI_NAME_MAX_LEN, 0ULL};
    if ((capabilities & CLI_MYSQL_PLUGIN_AUTH) != 0u)
    {
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &plugin_line,
                   .text_bytes = hello + at, .length = plugin_size);
    }
    else
    {
        mmgr_line_put(&plugin_line, "mysql_native_password");
    }
    mmgr_line_finish(&plugin_line);
    return 1;
}

// a length-encoded number written
EMBED_INLINE void sign_lenenc(const MysqlSignCtx *args)
{
    if (args->value < 0xfbu)
    {
        mmgr_line_ch(args->out, (char)args->value);
        return;
    }
    const unsigned int width = (args->value < 0x10000u) ? 2u : 3u;
    mmgr_line_ch(args->out, (char)((width == 2u) ? 0xfcu : 0xfdu));
    for (unsigned int byte = 0u; byte < width; byte += 1u)
    {
        mmgr_line_ch(args->out, (char)((args->value >> (8u * byte)) & 0xffu));
    }
}

// the handshake's answer: the capabilities, the max packet, the charset, the user, the answer to the scramble, the
// database where one is named, and the plugin
EMBED_INLINE int sign_reply(const MysqlSignCtx *args)
{
    CliMysql *mysql = args->mysql;
    CliDbFailure *failure = args->failure;
    char reply[CLI_NAME_MAX_LEN * 4u];
    VerbaLinea out = {reply, sizeof(reply), 0ULL};
    unsigned int flags = CLI_MYSQL_LONG_PASSWORD | CLI_MYSQL_LONG_FLAG | CLI_MYSQL_PROTOCOL_41 |
                         CLI_MYSQL_TRANSACTIONS | CLI_MYSQL_SECURE_CONNECTION | CLI_MYSQL_MULTI_RESULTS |
                         CLI_MYSQL_PLUGIN_AUTH | CLI_MYSQL_PLUGIN_AUTH_LENENC;
    flags |= (args->database[0] != '\0') ? CLI_MYSQL_CONNECT_WITH_DB : 0u;
    const unsigned int max = 16u << 20u;
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        mmgr_line_ch(&out, (char)((flags >> (8u * byte)) & 0xffu));
    }
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        mmgr_line_ch(&out, (char)((max >> (8u * byte)) & 0xffu));
    }
    // utf8mb4's general collation, then 23 bytes reserved for later
    mmgr_line_ch(&out, (char)45);
    for (unsigned int byte = 0u; byte < 23u; byte += 1u)
    {
        mmgr_line_ch(&out, '\0');
    }
    mmgr_line_put(&out, args->user);
    mmgr_line_ch(&out, '\0');
    EMBED_CALL(sign_lenenc, MysqlSignCtx, .out = &out, .value = args->answer_size);
    mmgr_line_put_n(&out, (const char *)args->answer, args->answer_size);
    if (args->database[0] != '\0')
    {
        mmgr_line_put(&out, args->database);
        mmgr_line_ch(&out, '\0');
    }
    mmgr_line_put(&out, args->plugin_read);
    mmgr_line_ch(&out, '\0');
    if (!mmgr_line_ok(&out))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the user's and the database's names are longer than impero sends");
        return 0;
    }
    return EMBED_CALL(cli_mysql_failure_internal.send, CliMysqlFailureCtx, .mysql = mysql,
                      .payload = (const unsigned char *)out.out, .size = out.at, .failure = failure);
}

// the password sealed by the server's RSA key in the PEM the packet contains after its first byte, each byte of it and
// a nul after mixed with the scramble first, and sent
EMBED_INLINE int sign_seal(const MysqlSignCtx *args)
{
    CliMysql *mysql = args->mysql;
    CliDbFailure *failure = args->failure;
    unsigned char secret[CLI_NAME_MAX_LEN + 1u];
    unsigned char sealed[1024];
    unsigned long long sealed_size = 0ULL;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->password, .cap = CLI_POOL_BYTES}) + 1u;
    memor.cpy(&(MemoriaCfg){.dst = secret, .src = args->password, .bytes = length});
    for (embed_word at = 0u; (args->scramble_size_value > 0u) && (at < length); at += 1u)
    {
        secret[at] = (unsigned char)(secret[at] ^ args->scramble_read[at % args->scramble_size_value]);
    }
    if (!EMBED_CALL(cli_mysql_size_internal.rsa_seal, CliMysqlSizeCtx, .pem = (const char *)mysql->packet + 1u,
                    .pem_size = mysql->size - 1u, .message = secret, .size = length, .out = sealed,
                    .out_size = &sealed_size, .err_msg = &err_msg))
    {
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = why);
        return 0;
    }
    mysql->sealed = 1;
    return EMBED_CALL(cli_mysql_failure_internal.send, CliMysqlFailureCtx, .mysql = mysql, .payload = sealed,
                      .size = sealed_size, .failure = failure);
}

// a switch to another plugin followed: its scramble read and the answer that plugin requires sent
EMBED_INLINE int sign_switch(const MysqlSignCtx *args)
{
    CliMysql *mysql = args->mysql;
    const char *password = args->password;
    unsigned char *scramble = args->scramble;
    unsigned long long *scramble_size = args->scramble_size;
    CliDbFailure *failure = args->failure;
    const unsigned char *const packet = mysql->packet;
    const unsigned long long size = mysql->size;
    const unsigned char *const nul =
        memor.chr(&(MemoriaCfg){.src = packet + 1u, .val = 0, .bytes = (embed_word)(size - 1u)});
    const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - packet) : size;
    char plugin[CLI_NAME_MAX_LEN];
    VerbaLinea plugin_line = {plugin, sizeof(plugin), 0ULL};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &plugin_line, .text_bytes = packet + 1u,
               .length = end - 1u);
    mmgr_line_finish(&plugin_line);
    const unsigned long long data = (end + 1u < size) ? size - end - 1u : 0ULL;
    const unsigned long long data_len = ((data > 0u) && (packet[size - 1u] == 0u)) ? data - 1u : data;
    *scramble_size = (data_len < 64u) ? data_len : 64ULL;
    memor.cpy(&(MemoriaCfg){.dst = scramble, .src = packet + end + 1u, .bytes = (embed_word)*scramble_size});
    unsigned char answer[CLI_NAME_MAX_LEN + 1u];
    unsigned int answer_size = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "mysql_native_password", .cap = CLI_POOL_BYTES}))
    {
        answer_size = EMBED_CALL(cli_mysql_size_password_internal.answer, CliMysqlSizePasswordCtx, .password = password,
                                 .scramble = scramble, .size = *scramble_size, .caching = 0, .answer = answer);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "caching_sha2_password", .cap = CLI_POOL_BYTES}))
    {
        answer_size = EMBED_CALL(cli_mysql_size_password_internal.answer, CliMysqlSizePasswordCtx, .password = password,
                                 .scramble = scramble, .size = *scramble_size, .caching = 1, .answer = answer);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "mysql_clear_password", .cap = CLI_POOL_BYTES}))
    {
        answer_size = (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_POOL_BYTES}) + 1u;
        memor.cpy(&(MemoriaCfg){.dst = answer, .src = password, .bytes = answer_size});
    }
    else
    {
        char why[CLI_NAME_MAX_LEN * 2u];
        VerbaLinea line = {why, sizeof(why), 0ULL};
        mmgr_line_put(&line, "the account signs in by ");
        mmgr_line_put(&line, plugin);
        mmgr_line_put(&line, ", which impero does not support: mysql_native_password and caching_sha2_password are "
                             "supported");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = why);
        return 0;
    }
    return EMBED_CALL(cli_mysql_failure_internal.send, CliMysqlFailureCtx, .mysql = mysql, .payload = answer,
                      .size = answer_size, .failure = failure);
}

int cli_mysql_signed_in(const CliMysqlFailurePasswordCtx *args)
{
    CliDbFailure *failure = args->failure;
    unsigned char scramble[64];
    unsigned long long scramble_size = 0ULL;
    char plugin[CLI_NAME_MAX_LEN];
    unsigned char answer[32];
    if (!EMBED_CALL(cli_mysql_failure_internal.packet, CliMysqlFailureCtx, .mysql = args->mysql, .failure = failure) ||
        !EMBED_CALL(sign_greeting, MysqlSignCtx, .mysql = args->mysql, .host = args->host, .port = args->port,
                    .scramble = scramble, .scramble_size = &scramble_size, .plugin = plugin, .failure = failure))
    {
        return 0;
    }
    const int caching =
        (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "caching_sha2_password", .cap = CLI_POOL_BYTES}));
    if (!caching)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = plugin,
            .src = "mysql_native_password",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "mysql_native_password", .cap = CLI_POOL_BYTES}) + 1u});
    }
    const unsigned int answer_size =
        EMBED_CALL(cli_mysql_size_password_internal.answer, CliMysqlSizePasswordCtx, .password = args->password,
                   .scramble = scramble, .size = scramble_size, .caching = caching, .answer = answer);
    if (!EMBED_CALL(sign_reply, MysqlSignCtx, .mysql = args->mysql, .user = args->user, .database = args->database,
                    .answer = answer, .answer_size = answer_size, .plugin_read = plugin, .failure = failure))
    {
        return 0;
    }
    for (;;)
    {
        if (!EMBED_CALL(cli_mysql_failure_internal.packet, CliMysqlFailureCtx, .mysql = args->mysql,
                        .failure = failure))
        {
            return 0;
        }
        const unsigned char first = (args->mysql->size > 0u) ? args->mysql->packet[0] : 0xeeu;
        const unsigned char second = (args->mysql->size > 1u) ? args->mysql->packet[1] : 0u;
        int going = 1;
        if (first == 0x00u)
        {
            return 1;
        }
        if (first == 0xffu)
        {
            EMBED_CALL(cli_mysql_failure_internal.failure_get, CliMysqlFailureCtx, .payload = args->mysql->packet,
                       .size = args->mysql->size, .failure = failure);
            return 0;
        }
        if (first == 0xfeu)
        {
            going = EMBED_CALL(sign_switch, MysqlSignCtx, .mysql = args->mysql, .password = args->password,
                               .scramble = scramble, .scramble_size = &scramble_size, .failure = failure);
        }
        else if ((first == 0x01u) && (second == 0x04u))
        {
            // the server requests the password whole: its public key is requested
            const unsigned char request_packet[1] = {0x02u};
            going = EMBED_CALL(cli_mysql_failure_internal.send, CliMysqlFailureCtx, .mysql = args->mysql,
                               .payload = request_packet, .size = sizeof(request_packet), .failure = failure);
        }
        else if ((first == 0x01u) && (second != 0x03u))
        {
            going = EMBED_CALL(sign_seal, MysqlSignCtx, .mysql = args->mysql, .password = args->password,
                               .scramble_read = scramble, .scramble_size_value = scramble_size, .failure = failure);
        }
        else if (first != 0x01u)
        {
            EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                       .message = "the server answered the sign-in with what impero cannot read");
            return 0;
        }
        if (!going)
        {
            return 0;
        }
    }
}

CliMysql *cli_mysql_connect(const CliMysqlHostCfg *args)
{
    unsigned int port = args->port;
    CliDbFailure *failure = args->failure;
    char address[CLI_NAME_MAX_LEN + 16u];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    VerbaLinea named = {address, sizeof(address), 0ULL};
    mmgr_line_put(&named, args->host);
    mmgr_line_ch(&named, ':');
    mmgr_line_decimal(&named, port, 1u);
    mmgr_line_finish(&named);
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = address, .purpose = "a MySQL connection",
                    .err_msg = &err_msg))
    {
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = why);
        return NULL;
    }
    CliMysql *mysql = NULL;
    unsigned int at = 0u;
    while ((at < CLI_MYSQL_LINKS) && (s_links[at] != NULL))
    {
        at += 1u;
    }
    // a link's record holds its socket, so the records are fixed outside the pool, one for each link open at once,
    // where they punch no hole in the persistent tier
    static CliMysql s_places[CLI_MYSQL_LINKS];
    mysql = (at < CLI_MYSQL_LINKS) ? &s_places[at] : NULL;
    if (mysql != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = mysql, .val = 0, .bytes = sizeof(*mysql)});
        mysql->place = at;
        s_links[at] = mysql;
    }
    if (mysql == NULL)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "impero has as many MySQL connections as it can");
        return NULL;
    }
    mysql->start = 0ULL;
    mysql->end = 0ULL;
    mysql->sequence = 0u;
    mysql->status = 0u;
    mysql->warnings = 0u;
    mysql->read_state = 0;
    mysql->sealed = 0;
    mysql->head.rows = 0;
    mysql->head.column_count = 0u;
    memor.cpy(&(MemoriaCfg){.dst = mysql->cancel.host,
                            .src = args->host,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->host, .cap = CLI_POOL_BYTES}) + 1u});
    mysql->cancel.port = port;
    memor.cpy(&(MemoriaCfg){.dst = mysql->cancel.user,
                            .src = args->user,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->user, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(
        &(MemoriaCfg){.dst = mysql->cancel.password,
                      .src = args->password,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->password, .cap = CLI_POOL_BYTES}) + 1u});
    mysql->cancel.id = 0u;
    // the wait is at max what a socket's wait counts, in milliseconds
    const unsigned int within = (args->wait > 0xffffffffULL) ? 0xffffffffu : (unsigned int)args->wait;
    int connected = EMBED_CALL(cli_err_msg_internal.socket_connected_within, CliErrMsgCtx, .sock_out = &mysql->socket,
                               .host = args->host, .port = port, .milliseconds = within, .err_msg = &err_msg);
    if (!connected)
    {
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = why);
    }
    else
    {
        EMBED_CALL(cli_sock_internal.socket_wait, CliSockCtx, .sock = &mysql->socket, .milliseconds = within);
        connected = EMBED_CALL(cli_mysql_failure_password_internal.signed_in, CliMysqlFailurePasswordCtx,
                               .mysql = mysql, .host = args->host, .port = port, .user = args->user,
                               .password = args->password, .database = args->database, .failure = failure);
        if (connected)
        {
            EMBED_CALL(cli_sock_internal.socket_wait, CliSockCtx, .sock = &mysql->socket, .milliseconds = 0u);
        }
        else
        {
            EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &mysql->socket);
        }
    }
    if (!connected)
    {
        s_links[mysql->place] = NULL;
        // the password lies in the link's record, zeroed as it closes
        mmgr_zero_buf(mysql, sizeof(*mysql));
        return NULL;
    }
    return mysql;
}

void cli_mysql_close(const CliMysqlFailureSqlCfg *args)
{
    CliDbFailure failure;
    const unsigned char quit[1] = {0x01u};
    args->mysql_out->sequence = 0u;
    (void)EMBED_CALL(cli_mysql_failure_internal.send, CliMysqlFailureCtx, .mysql = args->mysql_out, .payload = quit,
                     .size = sizeof(quit), .failure = &failure);
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &args->mysql_out->socket);
    s_links[args->mysql_out->place] = NULL;
    // the password lies in the link's record, zeroed as it closes
    mmgr_zero_buf(args->mysql_out, sizeof(*args->mysql_out));
}
