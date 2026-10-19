// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_sign.c: a MySQL connection made and let go: the server's greeting read, the answer to its scramble sent as the
// account's plugin asks, a switch to another plugin followed, and the password sealed by the server's RSA key where
// caching_sha2_password asks for it whole over a connection that is not encrypted
#include "../command_line/cells.h"
#include "../machine/net.h"
#include "../text/json_tree.h"
#include "mysql_internal.h"

static CliMysql *s_links[CLI_MYSQL_LINKS];
static CliLock s_links_lock = CLI_LOCK_INIT;

// the greeting's scramble and plugin read: the scramble into `scramble`, which holds 64 bytes, its length into
// `scramble_size`, and the plugin's name into `plugin`: 1, or 0 with the failure said
static int sign_greeting(CliMysql *mysql, const char *host, unsigned int port, unsigned char *scramble,
                         unsigned long long *scramble_size, char *plugin, CliDbFailure *failure)
{
    const unsigned char *const hello = mysql->packet;
    const unsigned long long size = mysql->size;
    if ((size > 0u) && (hello[0] == 0xffu))
    {
        cli_mysql_failure_of(hello, size, failure);
        return 0;
    }
    if ((size == 0u) || (hello[0] != 10u))
    {
        char why[CLI_NAME_LONGEST + 64u];
        CliLine said = {why, sizeof(why), 0ull};
        cli_line_text(&said, host);
        cli_line_character(&said, ':');
        cli_line_decimal(&said, port, 1u);
        cli_line_text(&said, " does not speak MySQL's protocol 10");
        cli_line_closed(&said);
        cli_db_failure_said(failure, why);
        return 0;
    }
    const unsigned char *const nul =
        memor.chr(&(MemoriaCfg){.src = hello + 1u, .val = 0, .bytes = (size_t)(size - 1u)});
    unsigned long long at = (nul != NULL) ? (unsigned long long)(nul - hello) : size;
    if ((nul == NULL) || (at + 1u + 4u + 9u + 2u + 5u + 1u + 10u > size))
    {
        cli_db_failure_said(failure, "the server's greeting is cut short");
        return 0;
    }
    CliLine version = {mysql->version, sizeof(mysql->version), 0ull};
    cli_lossy(&version, hello + 1u, at - 1u);
    cli_line_closed(&version);
    at += 1u;
    mysql->cancel.id = (unsigned int)hello[at] | ((unsigned int)hello[at + 1u] << 8u) |
                       ((unsigned int)hello[at + 2u] << 16u) | ((unsigned int)hello[at + 3u] << 24u);
    at += 4u;
    memor.cpy(&(MemoriaCfg){.dst = scramble, .src = hello + at, .bytes = 8u});
    *scramble_size = 8ull;
    at += 9u;
    const unsigned int lower = (unsigned int)hello[at] | ((unsigned int)hello[at + 1u] << 8u);
    at += 5u;
    const unsigned int upper = (unsigned int)hello[at] | ((unsigned int)hello[at + 1u] << 8u);
    const unsigned int capabilities = lower | (upper << 16u);
    at += 2u;
    const unsigned long long data_length = hello[at];
    at += 11u;
    // the scramble's second part: what the length names past its first 8 bytes, 13 at the least
    const unsigned long long rest = ((data_length > 8u) && (data_length - 8u > 13u)) ? data_length - 8u : 13ull;
    if ((at + rest <= size) && (rest <= 56u))
    {
        const unsigned long long more = ((rest > 0u) && (hello[at + rest - 1u] == 0u)) ? rest - 1u : rest;
        memor.cpy(&(MemoriaCfg){.dst = scramble + 8u, .src = hello + at, .bytes = (size_t)more});
        *scramble_size += more;
        at += rest;
    }
    const unsigned char *const plugin_end =
        (at < size) ? memor.chr(&(MemoriaCfg){.src = hello + at, .val = 0, .bytes = (size_t)(size - at)}) : NULL;
    const unsigned long long plugin_size =
        (plugin_end != NULL) ? (unsigned long long)(plugin_end - hello) - at : ((at < size) ? size - at : 0ull);
    CliLine plugin_line = {plugin, CLI_NAME_LONGEST, 0ull};
    if ((capabilities & CLI_MYSQL_PLUGIN_AUTH) != 0u)
    {
        cli_lossy(&plugin_line, hello + at, plugin_size);
    }
    else
    {
        cli_line_text(&plugin_line, "mysql_native_password");
    }
    cli_line_closed(&plugin_line);
    return 1;
}

// a length-encoded number written
static void sign_lenenc(CliLine *out, unsigned long long value)
{
    if (value < 0xfbu)
    {
        cli_line_character(out, (char)value);
        return;
    }
    const unsigned int width = (value < 0x10000u) ? 2u : 3u;
    cli_line_character(out, (char)((width == 2u) ? 0xfcu : 0xfdu));
    for (unsigned int byte = 0u; byte < width; byte += 1u)
    {
        cli_line_character(out, (char)((value >> (8u * byte)) & 0xffu));
    }
}

// the handshake's answer: the capabilities, the most packet, the charset, the user, the answer to the scramble, the
// database where one is named, and the plugin
static int sign_reply(CliMysql *mysql, const char *user, const char *database, const unsigned char *answer,
                      unsigned int answer_size, const char *plugin, CliDbFailure *failure)
{
    char reply[CLI_NAME_LONGEST * 4u];
    CliLine out = {reply, sizeof(reply), 0ull};
    unsigned int flags = CLI_MYSQL_LONG_PASSWORD | CLI_MYSQL_LONG_FLAG | CLI_MYSQL_PROTOCOL_41 |
                         CLI_MYSQL_TRANSACTIONS | CLI_MYSQL_SECURE_CONNECTION | CLI_MYSQL_MULTI_RESULTS |
                         CLI_MYSQL_PLUGIN_AUTH | CLI_MYSQL_PLUGIN_AUTH_LENENC;
    flags |= (database[0] != '\0') ? CLI_MYSQL_CONNECT_WITH_DB : 0u;
    const unsigned int most = 16u << 20u;
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        cli_line_character(&out, (char)((flags >> (8u * byte)) & 0xffu));
    }
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        cli_line_character(&out, (char)((most >> (8u * byte)) & 0xffu));
    }
    // utf8mb4's general collation, then 23 bytes kept for later
    cli_line_character(&out, (char)45);
    for (unsigned int byte = 0u; byte < 23u; byte += 1u)
    {
        cli_line_character(&out, '\0');
    }
    cli_line_text(&out, user);
    cli_line_character(&out, '\0');
    sign_lenenc(&out, answer_size);
    cli_line_counted(&out, (const char *)answer, answer_size);
    if (database[0] != '\0')
    {
        cli_line_text(&out, database);
        cli_line_character(&out, '\0');
    }
    cli_line_text(&out, plugin);
    cli_line_character(&out, '\0');
    if (!cli_line_fits(&out))
    {
        cli_db_failure_said(failure, "the user's and the database's names are longer than orior sends");
        return 0;
    }
    return cli_mysql_sent(mysql, (const unsigned char *)out.out, out.at, failure);
}

// the password sealed by the server's RSA key in the PEM the packet holds after its first byte, each byte of it and a
// nul after mixed with the scramble first, and sent
static int sign_sealed(CliMysql *mysql, const char *password, const unsigned char *scramble,
                       unsigned long long scramble_size, CliDbFailure *failure)
{
    unsigned char secret[CLI_NAME_LONGEST + 1u];
    unsigned char sealed[1024];
    unsigned long long sealed_size = 0ull;
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}) + 1u;
    memor.cpy(&(MemoriaCfg){.dst = secret, .src = password, .bytes = length});
    for (size_t at = 0u; (scramble_size > 0u) && (at < length); at += 1u)
    {
        secret[at] = (unsigned char)(secret[at] ^ scramble[at % scramble_size]);
    }
    if (!cli_mysql_rsa_sealed((const char *)mysql->packet + 1u, mysql->size - 1u, secret, length, sealed, &sealed_size,
                              &said))
    {
        cli_line_closed(&said);
        cli_db_failure_said(failure, why);
        return 0;
    }
    mysql->sealed = 1;
    return cli_mysql_sent(mysql, sealed, sealed_size, failure);
}

// a switch to another plugin followed: its scramble read and the answer that plugin asks for sent
static int sign_switched(CliMysql *mysql, const char *password, unsigned char *scramble,
                         unsigned long long *scramble_size, CliDbFailure *failure)
{
    const unsigned char *const said = mysql->packet;
    const unsigned long long size = mysql->size;
    const unsigned char *const nul = memor.chr(&(MemoriaCfg){.src = said + 1u, .val = 0, .bytes = (size_t)(size - 1u)});
    const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - said) : size;
    char plugin[CLI_NAME_LONGEST];
    CliLine plugin_line = {plugin, sizeof(plugin), 0ull};
    cli_lossy(&plugin_line, said + 1u, end - 1u);
    cli_line_closed(&plugin_line);
    const unsigned long long data = (end + 1u < size) ? size - end - 1u : 0ull;
    const unsigned long long kept = ((data > 0u) && (said[size - 1u] == 0u)) ? data - 1u : data;
    *scramble_size = (kept < 64u) ? kept : 64ull;
    memor.cpy(&(MemoriaCfg){.dst = scramble, .src = said + end + 1u, .bytes = (size_t)*scramble_size});
    unsigned char answer[CLI_NAME_LONGEST + 1u];
    unsigned int answer_size = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "mysql_native_password", .cap = CLI_CELLS_POOL}))
    {
        answer_size = cli_mysql_answer(password, scramble, *scramble_size, 0, answer);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "caching_sha2_password", .cap = CLI_CELLS_POOL}))
    {
        answer_size = cli_mysql_answer(password, scramble, *scramble_size, 1, answer);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "mysql_clear_password", .cap = CLI_CELLS_POOL}))
    {
        answer_size = (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}) + 1u;
        memor.cpy(&(MemoriaCfg){.dst = answer, .src = password, .bytes = answer_size});
    }
    else
    {
        char why[CLI_NAME_LONGEST * 2u];
        CliLine line = {why, sizeof(why), 0ull};
        cli_line_text(&line, "the account signs in by ");
        cli_line_text(&line, plugin);
        cli_line_text(&line, ", which orior does not speak: mysql_native_password and caching_sha2_password are "
                             "spoken");
        cli_line_closed(&line);
        cli_db_failure_said(failure, why);
        return 0;
    }
    return cli_mysql_sent(mysql, answer, answer_size, failure);
}

int cli_mysql_signed_in(CliMysql *mysql, const char *host, unsigned int port, const char *user, const char *password,
                        const char *database, CliDbFailure *failure)
{
    unsigned char scramble[64];
    unsigned long long scramble_size = 0ull;
    char plugin[CLI_NAME_LONGEST];
    unsigned char answer[32];
    if (!cli_mysql_packet(mysql, failure) ||
        !sign_greeting(mysql, host, port, scramble, &scramble_size, plugin, failure))
    {
        return 0;
    }
    const int caching =
        (cellul.eq(&(CatenaFinitaCfg){.src = plugin, .other = "caching_sha2_password", .cap = CLI_CELLS_POOL}));
    if (!caching)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = plugin,
            .src = "mysql_native_password",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "mysql_native_password", .cap = CLI_CELLS_POOL}) + 1u});
    }
    const unsigned int answer_size = cli_mysql_answer(password, scramble, scramble_size, caching, answer);
    if (!sign_reply(mysql, user, database, answer, answer_size, plugin, failure))
    {
        return 0;
    }
    for (;;)
    {
        if (!cli_mysql_packet(mysql, failure))
        {
            return 0;
        }
        const unsigned char first = (mysql->size > 0u) ? mysql->packet[0] : 0xeeu;
        const unsigned char second = (mysql->size > 1u) ? mysql->packet[1] : 0u;
        int going = 1;
        if (first == 0x00u)
        {
            return 1;
        }
        if (first == 0xffu)
        {
            cli_mysql_failure_of(mysql->packet, mysql->size, failure);
            return 0;
        }
        if (first == 0xfeu)
        {
            going = sign_switched(mysql, password, scramble, &scramble_size, failure);
        }
        else if ((first == 0x01u) && (second == 0x04u))
        {
            // the server asks for the password whole: its public key is asked for
            const unsigned char ask[1] = {0x02u};
            going = cli_mysql_sent(mysql, ask, sizeof(ask), failure);
        }
        else if ((first == 0x01u) && (second != 0x03u))
        {
            going = sign_sealed(mysql, password, scramble, scramble_size, failure);
        }
        else if (first != 0x01u)
        {
            cli_db_failure_said(failure, "the server answered the sign-in with what orior cannot read");
            return 0;
        }
        if (!going)
        {
            return 0;
        }
    }
}

CliMysql *cli_mysql_connected(const char *host, unsigned int port, const char *user, const char *password,
                              const char *database, unsigned long long wait, CliDbFailure *failure)
{
    char address[CLI_NAME_LONGEST + 16u];
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine named = {address, sizeof(address), 0ull};
    cli_line_text(&named, host);
    cli_line_character(&named, ':');
    cli_line_decimal(&named, port, 1u);
    cli_line_closed(&named);
    if (!cli_net_reach(address, "a MySQL connection", &said))
    {
        cli_line_closed(&said);
        cli_db_failure_said(failure, why);
        return NULL;
    }
    CliMysql *mysql = NULL;
    cli_lock_take(&s_links_lock);
    unsigned int at = 0u;
    while ((at < CLI_MYSQL_LINKS) && (s_links[at] != NULL))
    {
        at += 1u;
    }
    mysql = (at < CLI_MYSQL_LINKS) ? cli_cell_taken(sizeof(CliMysql), "a MySQL connection") : NULL;
    if (mysql != NULL)
    {
        mysql->place = at;
        s_links[at] = mysql;
    }
    cli_lock_give(&s_links_lock);
    if (mysql == NULL)
    {
        cli_db_failure_said(failure, "orior holds as many MySQL connections as it can");
        return NULL;
    }
    mysql->start = 0ull;
    mysql->end = 0ull;
    mysql->sequence = 0u;
    mysql->status = 0u;
    mysql->warnings = 0u;
    mysql->reading = 0;
    mysql->sealed = 0;
    mysql->head.rows = 0;
    mysql->head.column_count = 0u;
    memor.cpy(&(MemoriaCfg){.dst = mysql->cancel.host,
                            .src = host,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL}) + 1u});
    mysql->cancel.port = port;
    memor.cpy(&(MemoriaCfg){.dst = mysql->cancel.user,
                            .src = user,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = user, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = mysql->cancel.password,
                            .src = password,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}) + 1u});
    mysql->cancel.id = 0u;
    // the wait is at most what a socket's wait counts, in milliseconds
    const unsigned int within = (wait > 0xffffffffull) ? 0xffffffffu : (unsigned int)wait;
    int connected = cli_socket_connected_within(&mysql->socket, host, port, within, &said);
    if (!connected)
    {
        cli_line_closed(&said);
        cli_db_failure_said(failure, why);
    }
    else
    {
        cli_socket_waited(&mysql->socket, within);
        connected = cli_mysql_signed_in(mysql, host, port, user, password, database, failure);
        if (connected)
        {
            cli_socket_waited(&mysql->socket, 0u);
        }
        else
        {
            cli_socket_closed(&mysql->socket);
        }
    }
    if (!connected)
    {
        cli_lock_take(&s_links_lock);
        s_links[mysql->place] = NULL;
        cli_lock_give(&s_links_lock);
        cli_cell_given(mysql);
        return NULL;
    }
    return mysql;
}

void cli_mysql_closed(CliMysql *mysql)
{
    CliDbFailure failure;
    const unsigned char quit[1] = {0x01u};
    mysql->sequence = 0u;
    (void)cli_mysql_sent(mysql, quit, sizeof(quit), &failure);
    cli_socket_closed(&mysql->socket);
    cli_lock_take(&s_links_lock);
    s_links[mysql->place] = NULL;
    cli_lock_give(&s_links_lock);
    cli_cell_given(mysql);
}
