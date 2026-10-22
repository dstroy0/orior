// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_sign.c: a PostgreSQL connection's startup and sign-in: the startup message, then a password as the server asks,
// by SCRAM-SHA-256 as RFC 5802 and 7677 set it out, by md5, or in the clear, until the server says it is ready
#include "../text/base64.h"
#include "../text/digest.h"
#include "../text/json_tree.h"
#include "pg_internal.h"

#include "../text/text.h"

// a value of the server's first SCRAM message, `name` with its '=', into `out`: 1, or 0 where it gave none
static int sign_field(const char *server_first, const char *name, char *out, unsigned long long room)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (const char *part = server_first; part != NULL;)
    {
        const char *const comma = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = ','});
        const size_t size = (comma != NULL) ? (size_t)(comma - part)
                                            : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        if ((size >= length) && cli_text_same(part, name, length))
        {
            CliLine line = {out, room, 0ull};
            cli_line_counted(&line, part + length, size - length);
            cli_line_closed(&line);
            return cli_line_fits(&line);
        }
        part = (comma != NULL) ? comma + 1 : NULL;
    }
    return 0;
}

int cli_pg_scram_final(const char *password, const char *first_bare, const char *nonce, const char *server_first,
                       char *client_final, char *signature, CliDbFailure *failure)
{
    char server_nonce[CLI_PG_SIGN_TEXT];
    char salt_text[CLI_PG_SIGN_TEXT];
    char rounds_text[32];
    unsigned char salt[CLI_PG_SIGN_TEXT];
    unsigned long long salt_size = 0ull;
    if (!sign_field(server_first, "r=", server_nonce, sizeof(server_nonce)))
    {
        cli_db_failure_said(failure, "the server gave no nonce");
        return 0;
    }
    if (!cli_text_same(server_nonce, nonce, cellul.len(&(CatenaFinitaCfg){.src = nonce, .cap = CLI_CELLS_POOL})))
    {
        cli_db_failure_said(failure, "the server's nonce is not the one asked for");
        return 0;
    }
    if (!sign_field(server_first, "s=", salt_text, sizeof(salt_text)) ||
        !cli_unbase64(salt_text, cellul.len(&(CatenaFinitaCfg){.src = salt_text, .cap = CLI_CELLS_POOL}), salt,
                      sizeof(salt), &salt_size))
    {
        cli_db_failure_said(failure, "the server gave no salt");
        return 0;
    }
    unsigned long long rounds = 0ull;
    int counted = sign_field(server_first, "i=", rounds_text, sizeof(rounds_text)) && (rounds_text[0] != '\0') &&
                  (cellul.len(&(CatenaFinitaCfg){.src = rounds_text, .cap = CLI_CELLS_POOL}) <= 10u);
    for (const char *digit = rounds_text; counted && (*digit != '\0'); digit += 1)
    {
        counted = (*digit >= '0') && (*digit <= '9');
        rounds = (rounds * 10ull) + (unsigned long long)(*digit - '0');
    }
    if (!counted || (rounds > 0xffffffffull))
    {
        cli_db_failure_said(failure, "the server gave no count of rounds");
        return 0;
    }
    unsigned char salted[32];
    unsigned char client_key[32];
    unsigned char stored_key[32];
    unsigned char client_signature[32];
    unsigned char proof[32];
    unsigned char server_key[32];
    unsigned char server_signature[32];
    char without_proof[CLI_PG_SIGN_TEXT];
    char auth[CLI_PG_SIGN_TEXT * 3u];
    cli_pbkdf2_sha256(password, cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}), salt, salt_size,
                      (unsigned int)rounds, salted);
    cli_hmac_sha256_bytes(salted, sizeof(salted), "Client Key", 10ull, client_key);
    cli_sha256_bytes(client_key, sizeof(client_key), stored_key);
    CliLine without = {without_proof, sizeof(without_proof), 0ull};
    cli_line_text(&without, "c=biws,r=");
    cli_line_text(&without, server_nonce);
    cli_line_closed(&without);
    CliLine auth_line = {auth, sizeof(auth), 0ull};
    cli_line_text(&auth_line, first_bare);
    cli_line_character(&auth_line, ',');
    cli_line_text(&auth_line, server_first);
    cli_line_character(&auth_line, ',');
    cli_line_text(&auth_line, without_proof);
    cli_hmac_sha256_bytes(stored_key, sizeof(stored_key), auth_line.out, auth_line.at, client_signature);
    for (unsigned int at = 0u; at < sizeof(proof); at += 1u)
    {
        proof[at] = (unsigned char)(client_key[at] ^ client_signature[at]);
    }
    cli_hmac_sha256_bytes(salted, sizeof(salted), "Server Key", 10ull, server_key);
    cli_hmac_sha256_bytes(server_key, sizeof(server_key), auth_line.out, auth_line.at, server_signature);
    CliLine final_line = {client_final, CLI_PG_SIGN_TEXT, 0ull};
    cli_line_text(&final_line, without_proof);
    cli_line_text(&final_line, ",p=");
    cli_base64(&final_line, proof, sizeof(proof));
    cli_line_closed(&final_line);
    CliLine signature_line = {signature, CLI_PG_SIGN_TEXT, 0ull};
    cli_base64(&signature_line, server_signature, sizeof(server_signature));
    cli_line_closed(&signature_line);
    return 1;
}

// a text of `size` bytes, then a nul, sent as a PasswordMessage
static int sign_password(CliPg *pg, const char *text, unsigned long long size, int nul, CliDbFailure *failure)
{
    const void *const parts[2] = {text, ""};
    const unsigned long long sizes[2] = {size, 1ull};
    return cli_pg_sent(pg, 'p', parts, sizes, nul ? 2u : 1u, failure);
}

// the startup message: the protocol's version, then each setting and its value, each ended by a nul, and a nul after
static int sign_startup(CliPg *pg, const char *user, const char *database, CliDbFailure *failure)
{
    unsigned char startup[CLI_PG_SIGN_TEXT * 2u];
    CliLine body = {(char *)startup, sizeof(startup), 4ull};
    const char *const pairs[10] = {
        "user", user, "database", database, "application_name", "orior", "client_encoding", "UTF8", "DateStyle", "ISO"};
    // the version, 3.0, as 196608 big-endian
    cli_line_counted(&body, "\x00\x03\x00\x00", 4ull);
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        cli_line_text(&body, pairs[at]);
        cli_line_character(&body, '\0');
    }
    cli_line_character(&body, '\0');
    if (!cli_line_fits(&body))
    {
        cli_db_failure_said(failure, "the user's and the database's names are longer than orior_cli sends");
        return 0;
    }
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        startup[byte] = (unsigned char)((body.at >> (24u - (8u * byte))) & 0xffu);
    }
    if (!cli_socket_written(&pg->socket, startup, body.at))
    {
        cli_db_failure_said(failure, "the connection broke: the server closed it");
        return 0;
    }
    return 1;
}

// what the server answers a sign-in with, one Authentication message by its code: 1, or 0 with the failure said
static int sign_answered(CliPg *pg, const char *user, const char *password, char *first_bare, char *nonce,
                         char *signature, int *begun, CliDbFailure *failure)
{
    const unsigned char *const body = pg->message;
    const unsigned long long size = pg->size;
    const unsigned long long code = (size >= 4u)
                                        ? (((unsigned long long)body[0] << 24u) | ((unsigned long long)body[1] << 16u) |
                                           ((unsigned long long)body[2] << 8u) | (unsigned long long)body[3])
                                        : 0ull;
    char text[CLI_PG_SIGN_TEXT * 2u];
    CliLine line = {text, sizeof(text), 0ull};
    if (code == 0ull)
    {
        return 1;
    }
    if (code == 3ull)
    {
        return sign_password(pg, password, cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL}), 1,
                             failure);
    }
    if (code == 5ull)
    {
        // md5 of the md5 of the password and the user, with the salt the server gave after it
        char inner[64];
        CliLine inner_line = {inner, sizeof(inner), 0ull};
        cli_line_text(&line, password);
        cli_line_text(&line, user);
        cli_md5_text(line.out, line.at, &inner_line);
        cli_line_counted(&inner_line, (const char *)body + 4u, (size >= 8u) ? 4ull : 0ull);
        line.at = 0ull;
        cli_line_text(&line, "md5");
        cli_md5_text(inner_line.out, inner_line.at, &line);
        return sign_password(pg, line.out, line.at, 1, failure);
    }
    if (code == 10ull)
    {
        int scram = 0;
        CliLine offered = {text, sizeof(text), 0ull};
        for (unsigned long long at = 4ull; at < size;)
        {
            const unsigned char *const nul =
                memor.chr(&(MemoriaCfg){.src = body + at, .val = 0, .bytes = (size_t)(size - at)});
            const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - body) : size;
            if (end > at)
            {
                scram = scram ||
                        ((end - at == 13u) &&
                         (memor.cmp(&(MemoriaCfg){.src = body + at, .other = "SCRAM-SHA-256", .bytes = 13u}) == 0));
                cli_line_text(&offered, (offered.at > 0u) ? ", " : "");
                cli_lossy(&offered, body + at, end - at);
            }
            at = end + 1ull;
        }
        if (!scram)
        {
            char why[CLI_PG_SIGN_TEXT * 3u];
            CliLine said = {why, sizeof(why), 0ull};
            cli_line_text(&said, "the server signs in only by ");
            cli_line_counted(&said, offered.out, offered.at);
            cli_line_text(&said, ", which orior_cli does not speak");
            cli_line_closed(&said);
            cli_db_failure_said(failure, why);
            return 0;
        }
        unsigned char random[18];
        if (!cli_random_bytes(random, sizeof(random)))
        {
            cli_db_failure_said(failure, "the system gave no secret");
            return 0;
        }
        CliLine nonce_line = {nonce, CLI_PG_SIGN_TEXT, 0ull};
        cli_base64(&nonce_line, random, sizeof(random));
        cli_line_closed(&nonce_line);
        CliLine bare = {first_bare, CLI_PG_SIGN_TEXT, 0ull};
        cli_line_text(&bare, "n=,r=");
        cli_line_text(&bare, nonce);
        cli_line_closed(&bare);
        // the mechanism's name and a nul, then the first message's length, big-endian, then the message
        const unsigned long long first =
            3ull + cellul.len(&(CatenaFinitaCfg){.src = first_bare, .cap = CLI_CELLS_POOL});
        cli_line_text(&line, "SCRAM-SHA-256");
        cli_line_character(&line, '\0');
        for (unsigned int byte = 0u; byte < 4u; byte += 1u)
        {
            cli_line_character(&line, (char)((first >> (24u - (8u * byte))) & 0xffu));
        }
        cli_line_text(&line, "n,,");
        cli_line_text(&line, first_bare);
        *begun = 1;
        return sign_password(pg, line.out, line.at, 0, failure);
    }
    if (code == 11ull)
    {
        if (!*begun)
        {
            cli_db_failure_said(failure, "the server went on with a sign-in that was not begun");
            return 0;
        }
        char server_first[CLI_PG_SIGN_TEXT];
        char client_final[CLI_PG_SIGN_TEXT];
        CliLine server_line = {server_first, sizeof(server_first), 0ull};
        cli_lossy(&server_line, body + 4u, (size > 4u) ? size - 4u : 0ull);
        cli_line_closed(&server_line);
        if (!cli_pg_scram_final(password, first_bare, nonce, server_first, client_final, signature, failure))
        {
            return 0;
        }
        return sign_password(pg, client_final,
                             cellul.len(&(CatenaFinitaCfg){.src = client_final, .cap = CLI_CELLS_POOL}), 0, failure);
    }
    if (code == 12ull)
    {
        cli_lossy(&line, body + 4u, (size > 4u) ? size - 4u : 0ull);
        unsigned long long start = 0ull;
        unsigned long long end = line.at;
        while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = line.out[start]}) != NULL))
        {
            start += 1ull;
        }
        while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = line.out[end - 1u]}) != NULL))
        {
            end -= 1ull;
        }
        const size_t expected = *begun ? cellul.len(&(CatenaFinitaCfg){.src = signature, .cap = CLI_CELLS_POOL}) : 0u;
        const int proved =
            (end - start == expected + 2u) &&
            (memor.cmp(&(MemoriaCfg){.src = line.out + start, .other = "v=", .bytes = 2u}) == 0) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = line.out + start + 2u, .other = *begun ? signature : "", .bytes = expected}) == 0);
        if (!proved)
        {
            cli_db_failure_said(failure, "the server did not prove it holds the password: the sign-in is not trusted");
        }
        return proved;
    }
    cli_line_text(&line, "the server asks for a sign-in orior_cli does not speak (");
    cli_line_decimal(&line, code, 1u);
    cli_line_text(&line, "): SCRAM-SHA-256, md5 and a password in the clear are spoken");
    cli_line_closed(&line);
    cli_db_failure_said(failure, text);
    return 0;
}

int cli_pg_signed_in(CliPg *pg, const char *host, unsigned int port, const char *user, const char *password,
                     const char *database, CliDbFailure *failure)
{
    char first_bare[CLI_PG_SIGN_TEXT];
    char nonce[CLI_PG_SIGN_TEXT];
    char signature[CLI_PG_SIGN_TEXT];
    int begun = 0;
    if (!sign_startup(pg, user, database, failure))
    {
        return 0;
    }
    for (;;)
    {
        unsigned char kind = 0u;
        if (!cli_pg_message(pg, &kind, failure))
        {
            return 0;
        }
        if ((kind == 'R') && !sign_answered(pg, user, password, first_bare, nonce, signature, &begun, failure))
        {
            return 0;
        }
        if (kind == 'K')
        {
            pg->has_cancel = (pg->size >= 4u);
            memor.cpy(&(MemoriaCfg){.dst = pg->cancel.host,
                                    .src = host,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL}) + 1u});
            pg->cancel.port = port;
            pg->cancel.pid = (pg->size >= 4u)
                                 ? (((unsigned int)pg->message[0] << 24u) | ((unsigned int)pg->message[1] << 16u) |
                                    ((unsigned int)pg->message[2] << 8u) | (unsigned int)pg->message[3])
                                 : 0u;
            const unsigned long long secret = (pg->size > 4u) ? pg->size - 4u : 0ull;
            pg->cancel.secret_size = (secret < sizeof(pg->cancel.secret)) ? (unsigned int)secret : 0u;
            memor.cpy(
                &(MemoriaCfg){.dst = pg->cancel.secret, .src = pg->message + 4u, .bytes = pg->cancel.secret_size});
        }
        if (kind == 'S')
        {
            cli_pg_parameter_kept(pg, pg->message, pg->size);
        }
        if (kind == 'N')
        {
            cli_pg_notice_kept(pg);
        }
        if (kind == 'E')
        {
            cli_pg_failure_of(pg->message, pg->size, failure);
            if ((cellul.find(&(CatenaFinitaCfg){.src = failure->message,
                                                .cap = CLI_CELLS_POOL,
                                                .other = "no encryption",
                                                .other_cap = CLI_CELLS_POOL}) != NULL) ||
                (cellul.find(&(CatenaFinitaCfg){
                     .src = failure->message, .cap = CLI_CELLS_POOL, .other = "SSL", .other_cap = CLI_CELLS_POOL}) !=
                 NULL))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = failure->hint,
                    .src = "The server takes only encrypted connections; orior_cli reaches such a server "
                           "through an ssh tunnel to a machine beside it.",
                    .bytes = cellul.len(&(CatenaFinitaCfg){
                                 .src = "The server takes only encrypted connections; orior_cli reaches such a server "
                                        "through an ssh tunnel to a machine beside it.",
                                 .cap = CLI_CELLS_POOL}) +
                             1u});
            }
            return 0;
        }
        if (kind == 'Z')
        {
            pg->status = (pg->size > 0u) ? (char)pg->message[0] : 'I';
            return 1;
        }
    }
}
