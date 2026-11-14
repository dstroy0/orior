// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pg_sign.c: a PostgreSQL connection's startup and sign-in: the startup message, then a password as the server
// requires, by SCRAM-SHA-256 as RFC 5802 and 7677 set it out, by md5, or in the clear, until the server reports it is
// ready
#include "../text/base64.h"
#include "../text/digest.h"
#include "../text/json_tree.h"
#include "pg_internal.h"

#include "../text/text.h"

typedef struct
{
    const char *server_first;
    const char *name;
    char *out;
    unsigned long long capacity;
    CliPg *pg;
    const char *text;
    unsigned long long size;
    int nul;
    CliDbFailure *failure;
    const char *user;
    const char *database;
    const char *password;
    char *first_bare;
    char *nonce;
    char *signature;
    int *begun;
    VerbaLinea *offered;
} PgSignCtx;

// a value of the server's first SCRAM message, `name` with its '=', into `out`: 1, or 0 where it gave none
EMBED_INLINE int sign_field(const PgSignCtx *args)
{
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    for (const char *part = args->server_first; part != NULL;)
    {
        const char *const comma = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES, .byte = ','});
        const embed_word size = (comma != NULL) ? (embed_word)(comma - part)
                                                : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES});
        if ((size >= length) && EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = part,
                                           .other_pointer = args->name, .size = length))
        {
            VerbaLinea line = {args->out, args->capacity, 0ULL};
            mmgr_line_put_n(&line, part + length, size - length);
            mmgr_line_finish(&line);
            return mmgr_line_ok(&line);
        }
        part = (comma != NULL) ? comma + 1 : NULL;
    }
    return 0;
}

int cli_pg_scram_final(const CliPgFailureCtx *args)
{
    CliDbFailure *failure = args->failure;
    char server_nonce[CLI_PG_SIGN_TEXT];
    char salt_text[CLI_PG_SIGN_TEXT];
    char rounds_text[32];
    unsigned char salt[CLI_PG_SIGN_TEXT];
    unsigned long long salt_size = 0ULL;
    if (!EMBED_CALL(sign_field, PgSignCtx, .server_first = args->server_first, .name = "r=", .out = server_nonce,
                    .capacity = sizeof(server_nonce)))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = "the server gave no nonce");
        return 0;
    }
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = server_nonce, .other_pointer = args->nonce,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = args->nonce, .cap = CLI_POOL_BYTES})))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the server's nonce is not the one requested");
        return 0;
    }
    if (!EMBED_CALL(sign_field, PgSignCtx, .server_first = args->server_first, .name = "s=", .out = salt_text,
                    .capacity = sizeof(salt_text)) ||
        !EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = salt_text,
                    .length = cellul.len(&(CatenaFinitaCfg){.src = salt_text, .cap = CLI_POOL_BYTES}),
                    .out_bytes = salt, .capacity = sizeof(salt), .size_pointer = &salt_size))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure, .message = "the server gave no salt");
        return 0;
    }
    unsigned long long rounds = 0ULL;
    int counted = EMBED_CALL(sign_field, PgSignCtx, .server_first = args->server_first,
                             .name = "i=", .out = rounds_text, .capacity = sizeof(rounds_text)) &&
                  (rounds_text[0] != '\0') &&
                  (cellul.len(&(CatenaFinitaCfg){.src = rounds_text, .cap = CLI_POOL_BYTES}) <= 10u);
    counted = counted &&
              (EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = rounds_text, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = rounds_text, .cap = CLI_POOL_BYTES}),
                          .classes = CLI_TEXT_DIGITS,
                          .set = "") == cellul.len(&(CatenaFinitaCfg){.src = rounds_text, .cap = CLI_POOL_BYTES}));
    rounds = counted ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = rounds_text, .at = 0ULL,
                                  .size = cellul.len(&(CatenaFinitaCfg){.src = rounds_text, .cap = CLI_POOL_BYTES}),
                                  .end = NULL)
                     : 0ULL;
    if (!counted || (rounds > 0xffffffffULL))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the server gave no count of rounds");
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
    EMBED_CALL(cli_digest_password.pbkdf2_sha256, CliDigestPasswordCfg, .password = args->password,
               .password_size = cellul.len(&(CatenaFinitaCfg){.src = args->password, .cap = CLI_POOL_BYTES}),
               .salt = salt, .salt_size = salt_size, .rounds = (unsigned int)rounds, .derived = salted);
    EMBED_CALL(cli_digest_key.hmac_sha256_bytes, CliDigestKeyCfg, .key = salted, .key_size = sizeof(salted),
               .message = "Client Key", .message_size = 10ULL, .signed_digest = client_key);
    EMBED_CALL(cli_digest_bytes_size.sha256_bytes, CliDigestBytesSizeCfg, .bytes = client_key,
               .size = sizeof(client_key), .digest = stored_key);
    VerbaLinea without = {without_proof, sizeof(without_proof), 0ULL};
    mmgr_line_put(&without, "c=biws,r=");
    mmgr_line_put(&without, server_nonce);
    mmgr_line_finish(&without);
    VerbaLinea auth_line = {auth, sizeof(auth), 0ULL};
    mmgr_line_put(&auth_line, args->first_bare);
    mmgr_line_ch(&auth_line, ',');
    mmgr_line_put(&auth_line, args->server_first);
    mmgr_line_ch(&auth_line, ',');
    mmgr_line_put(&auth_line, without_proof);
    EMBED_CALL(cli_digest_key.hmac_sha256_bytes, CliDigestKeyCfg, .key = stored_key, .key_size = sizeof(stored_key),
               .message = auth_line.out, .message_size = auth_line.at, .signed_digest = client_signature);
    for (unsigned int at = 0u; at < sizeof(proof); at += 1u)
    {
        proof[at] = (unsigned char)(client_key[at] ^ client_signature[at]);
    }
    EMBED_CALL(cli_digest_key.hmac_sha256_bytes, CliDigestKeyCfg, .key = salted, .key_size = sizeof(salted),
               .message = "Server Key", .message_size = 10ULL, .signed_digest = server_key);
    EMBED_CALL(cli_digest_key.hmac_sha256_bytes, CliDigestKeyCfg, .key = server_key, .key_size = sizeof(server_key),
               .message = auth_line.out, .message_size = auth_line.at, .signed_digest = server_signature);
    VerbaLinea final_line = {args->client_final, CLI_PG_SIGN_TEXT, 0ULL};
    mmgr_line_put(&final_line, without_proof);
    mmgr_line_put(&final_line, ",p=");
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &final_line, .bytes = proof, .size = sizeof(proof));
    mmgr_line_finish(&final_line);
    VerbaLinea signature_line = {args->signature, CLI_PG_SIGN_TEXT, 0ULL};
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &signature_line, .bytes = server_signature,
               .size = sizeof(server_signature));
    mmgr_line_finish(&signature_line);
    return 1;
}

// a text of `size` bytes, then a nul, sent as a PasswordMessage
EMBED_INLINE int sign_password(const PgSignCtx *args)
{
    CliPg *pg = args->pg;
    CliDbFailure *failure = args->failure;
    const void *const parts[2] = {args->text, ""};
    const unsigned long long sizes[2] = {args->size, 1ULL};
    return EMBED_CALL(cli_pg_failure_type_internal.send, CliPgFailureTypeCtx, .pg = pg, .type_value = 'p',
                      .parts = parts, .sizes = sizes, .count = args->nul ? 2u : 1u, .failure = failure);
}

// the startup message: the protocol's version, then each setting and its value, each ended by a nul, and a nul after
EMBED_INLINE int sign_startup(const PgSignCtx *args)
{
    CliDbFailure *failure = args->failure;
    unsigned char startup[CLI_PG_SIGN_TEXT * 2u];
    VerbaLinea body = {(char *)startup, sizeof(startup), 4ULL};
    const char *const pairs[10] = {"user",  args->user,        "database", args->database, "application_name",
                                   "orior", "client_encoding", "UTF8",     "DateStyle",    "ISO"};
    // the version, 3.0, as 196608 big-endian
    mmgr_line_put_n(&body, "\x00\x03\x00\x00", 4ULL);
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        mmgr_line_put(&body, pairs[at]);
        mmgr_line_ch(&body, '\0');
    }
    mmgr_line_ch(&body, '\0');
    if (!mmgr_line_ok(&body))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the user's and the database's names are longer than impero sends");
        return 0;
    }
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        startup[byte] = (unsigned char)((body.at >> (24u - (8u * byte))) & 0xffu);
    }
    if (!EMBED_CALL(cli_bytes_size_internal.socket_write, CliBytesSizeCtx, .sock = &args->pg->socket, .bytes = startup,
                    .size = body.at))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = failure,
                   .message = "the connection broke: the server closed it");
        return 0;
    }
    return 1;
}

// the md5 sign-in an AuthenticationMD5Password message asks for: the md5 of the md5 of the password and the user, with
// the salt the server sent after it, sent as the password; 1, or 0 with the failure set
EMBED_INLINE int sign_md5(const PgSignCtx *args)
{
    const unsigned char *const body = args->pg->message;
    const unsigned long long size = args->pg->size;
    char text[CLI_PG_SIGN_TEXT * 2u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    char inner[64];
    VerbaLinea inner_line = {inner, sizeof(inner), 0ULL};
    mmgr_line_put(&line, args->password);
    mmgr_line_put(&line, args->user);
    EMBED_CALL(cli_digest_bytes.md5_text, CliDigestBytesCfg, .bytes = line.out, .size = line.at, .text = &inner_line);
    mmgr_line_put_n(&inner_line, (const char *)body + 4u, (size >= 8u) ? 4ULL : 0ULL);
    line.at = 0ULL;
    mmgr_line_put(&line, "md5");
    EMBED_CALL(cli_digest_bytes.md5_text, CliDigestBytesCfg, .bytes = inner_line.out, .size = inner_line.at,
               .text = &line);
    return EMBED_CALL(sign_password, PgSignCtx, .pg = args->pg, .text = line.out, .size = line.at, .nul = 1,
                      .failure = args->failure);
}

// the mechanisms an AuthenticationSASL message offers, each ended by a nul, listed into `offered` with ", " between
// them: whether SCRAM-SHA-256 is among them
EMBED_INLINE int sign_scram_offered(const PgSignCtx *args)
{
    const unsigned char *const body = args->pg->message;
    const unsigned long long size = args->pg->size;
    int scram = 0;
    unsigned long long at = 4ULL;
    while (at < size)
    {
        const unsigned char *const nul =
            memor.chr(&(MemoriaCfg){.src = body + at, .val = 0, .bytes = (embed_word)(size - at)});
        const unsigned long long end = (nul != NULL) ? (unsigned long long)(nul - body) : size;
        if (end > at)
        {
            scram =
                scram || ((end - at == 13u) &&
                          (memor.cmp(&(MemoriaCfg){.src = body + at, .other = "SCRAM-SHA-256", .bytes = 13u}) == 0));
            mmgr_line_put(args->offered, (args->offered->at > 0u) ? ", " : "");
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->offered,
                       .text_bytes = body + at, .length = end - at);
        }
        at = end + 1ULL;
    }
    return scram;
}

// a SCRAM-SHA-256 sign-in begun where an AuthenticationSASL message offers it: a nonce of 18 random bytes into
// `nonce`, the client's first message into `first_bare` and sent, and `begun` set; 1, or 0 with the failure set where
// the server offers no SCRAM-SHA-256 or the system no secret
EMBED_INLINE int sign_scram_first(const PgSignCtx *args)
{
    char text[CLI_PG_SIGN_TEXT * 2u];
    VerbaLinea offered = {text, sizeof(text), 0ULL};
    if (!EMBED_CALL(sign_scram_offered, PgSignCtx, .pg = args->pg, .offered = &offered))
    {
        char why[CLI_PG_SIGN_TEXT * 3u];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        mmgr_line_put(&err_msg, "the server signs in only by ");
        mmgr_line_put_n(&err_msg, offered.out, offered.at);
        mmgr_line_put(&err_msg, ", which impero does not support");
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure, .message = why);
        return 0;
    }
    unsigned char random[18];
    if (!EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = random, .size = sizeof(random)))
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = "the system gave no secret");
        return 0;
    }
    VerbaLinea nonce_line = {args->nonce, CLI_PG_SIGN_TEXT, 0ULL};
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &nonce_line, .bytes = random, .size = sizeof(random));
    mmgr_line_finish(&nonce_line);
    VerbaLinea bare = {args->first_bare, CLI_PG_SIGN_TEXT, 0ULL};
    mmgr_line_put(&bare, "n=,r=");
    mmgr_line_put(&bare, args->nonce);
    mmgr_line_finish(&bare);
    // the mechanism's name and a nul, then the first message's length, big-endian, then the message
    const unsigned long long first =
        3ULL + cellul.len(&(CatenaFinitaCfg){.src = args->first_bare, .cap = CLI_POOL_BYTES});
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, "SCRAM-SHA-256");
    mmgr_line_ch(&line, '\0');
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        mmgr_line_ch(&line, (char)((first >> (24u - (8u * byte))) & 0xffu));
    }
    mmgr_line_put(&line, "n,,");
    mmgr_line_put(&line, args->first_bare);
    *args->begun = 1;
    return EMBED_CALL(sign_password, PgSignCtx, .pg = args->pg, .text = line.out, .size = line.at, .nul = 0,
                      .failure = args->failure);
}

// the client's final SCRAM message, computed from the server's first that an AuthenticationSASLContinue message
// carries, sent; 1, or 0 with the failure set where no sign-in was begun or the server's message falls short
EMBED_INLINE int sign_scram_final_sent(const PgSignCtx *args)
{
    const unsigned char *const body = args->pg->message;
    const unsigned long long size = args->pg->size;
    if (!*args->begun)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = "the server went on with a sign-in that was not begun");
        return 0;
    }
    char server_first[CLI_PG_SIGN_TEXT];
    char client_final[CLI_PG_SIGN_TEXT];
    VerbaLinea server_line = {server_first, sizeof(server_first), 0ULL};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &server_line, .text_bytes = body + 4u,
               .length = (size > 4u) ? size - 4u : 0ULL);
    mmgr_line_finish(&server_line);
    if (!EMBED_CALL(cli_pg_failure_internal.scram_final, CliPgFailureCtx, .password = args->password,
                    .first_bare = args->first_bare, .nonce = args->nonce, .server_first = server_first,
                    .client_final = client_final, .signature = args->signature, .failure = args->failure))
    {
        return 0;
    }
    return EMBED_CALL(sign_password, PgSignCtx, .pg = args->pg, .text = client_final,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = client_final, .cap = CLI_POOL_BYTES}), .nul = 0,
                      .failure = args->failure);
}

// the server's final SCRAM message, which an AuthenticationSASLFinal message carries, `v=` and the server's
// signature, compared with the signature the client computed, empty where no sign-in was begun: 1 where they agree, or
// 0 with the failure set
EMBED_INLINE int sign_scram_proved(const PgSignCtx *args)
{
    const unsigned char *const body = args->pg->message;
    const unsigned long long size = args->pg->size;
    char text[CLI_PG_SIGN_TEXT * 2u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line, .text_bytes = body + 4u,
               .length = (size > 4u) ? size - 4u : 0ULL);
    unsigned long long start = 0ULL;
    unsigned long long end = line.at;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = line.out[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = line.out[end - 1u]}) != NULL))
    {
        end -= 1ULL;
    }
    const char *const signature = *args->begun ? args->signature : "";
    const embed_word expected = cellul.len(&(CatenaFinitaCfg){.src = signature, .cap = CLI_POOL_BYTES});
    const int proved =
        (end - start == expected + 2u) &&
        (memor.cmp(&(MemoriaCfg){.src = line.out + start, .other = "v=", .bytes = 2u}) == 0) &&
        (memor.cmp(&(MemoriaCfg){.src = line.out + start + 2u, .other = signature, .bytes = expected}) == 0);
    if (!proved)
    {
        EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure,
                   .message = "the server did not prove it has the password: the sign-in is not trusted");
    }
    return proved;
}

// what the server answers a sign-in with, one Authentication message by its code: 1, or 0 with the failure set
EMBED_INLINE int sign_answer(const PgSignCtx *args)
{
    const unsigned char *const body = args->pg->message;
    const unsigned long long size = args->pg->size;
    const unsigned long long code = (size >= 4u)
                                        ? (((unsigned long long)body[0] << 24u) | ((unsigned long long)body[1] << 16u) |
                                           ((unsigned long long)body[2] << 8u) | (unsigned long long)body[3])
                                        : 0ULL;
    if (code == 0ULL)
    {
        return 1;
    }
    if (code == 3ULL)
    {
        return EMBED_CALL(sign_password, PgSignCtx, .pg = args->pg, .text = args->password,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = args->password, .cap = CLI_POOL_BYTES}),
                          .nul = 1, .failure = args->failure);
    }
    if (code == 5ULL)
    {
        return sign_md5(args);
    }
    if (code == 10ULL)
    {
        return sign_scram_first(args);
    }
    if (code == 11ULL)
    {
        return sign_scram_final_sent(args);
    }
    if (code == 12ULL)
    {
        return sign_scram_proved(args);
    }
    char text[CLI_PG_SIGN_TEXT * 2u];
    VerbaLinea line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&line, "the server requests a sign-in impero does not support (");
    mmgr_line_decimal(&line, code, 1u);
    mmgr_line_put(&line, "): SCRAM-SHA-256, md5 and a password in the clear are supported");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_db_wire.db_failure_set, CliDbWireCfg, .failure = args->failure, .message = text);
    return 0;
}

int cli_pg_signed_in(const CliPgFailurePasswordCtx *args)
{
    unsigned int port = args->port;
    char first_bare[CLI_PG_SIGN_TEXT];
    char nonce[CLI_PG_SIGN_TEXT];
    char signature[CLI_PG_SIGN_TEXT];
    int begun = 0;
    if (!EMBED_CALL(sign_startup, PgSignCtx, .pg = args->pg, .user = args->user, .database = args->database,
                    .failure = args->failure))
    {
        return 0;
    }
    for (;;)
    {
        unsigned char type = 0u;
        if (!EMBED_CALL(cli_pg_type_internal.message, CliPgTypeCtx, .pg = args->pg, .type = &type,
                        .failure = args->failure))
        {
            return 0;
        }
        if ((type == 'R') && !EMBED_CALL(sign_answer, PgSignCtx, .pg = args->pg, .user = args->user,
                                         .password = args->password, .first_bare = first_bare, .nonce = nonce,
                                         .signature = signature, .begun = &begun, .failure = args->failure))
        {
            return 0;
        }
        if (type == 'K')
        {
            args->pg->has_cancel = (args->pg->size >= 4u);
            memor.cpy(
                &(MemoriaCfg){.dst = args->pg->cancel.host,
                              .src = args->host,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->host, .cap = CLI_POOL_BYTES}) + 1u});
            args->pg->cancel.port = port;
            args->pg->cancel.pid =
                (args->pg->size >= 4u)
                    ? (((unsigned int)args->pg->message[0] << 24u) | ((unsigned int)args->pg->message[1] << 16u) |
                       ((unsigned int)args->pg->message[2] << 8u) | (unsigned int)args->pg->message[3])
                    : 0u;
            const unsigned long long secret = (args->pg->size > 4u) ? args->pg->size - 4u : 0ULL;
            args->pg->cancel.secret_size = (secret < sizeof(args->pg->cancel.secret)) ? (unsigned int)secret : 0u;
            memor.cpy(&(MemoriaCfg){
                .dst = args->pg->cancel.secret, .src = args->pg->message + 4u, .bytes = args->pg->cancel.secret_size});
        }
        if (type == 'S')
        {
            EMBED_CALL(cli_pg_body_internal.parameter_store, CliPgBodyCtx, .pg = args->pg, .body = args->pg->message,
                       .size = args->pg->size);
        }
        if (type == 'N')
        {
            cli_pg_failure_password_internal.notice_store(args);
        }
        if (type == 'E')
        {
            EMBED_CALL(cli_pg_internal.failure_get, CliPgCtx, .body = args->pg->message, .size = args->pg->size,
                       .failure = args->failure);
            if ((cellul.find(&(CatenaFinitaCfg){.src = args->failure->message,
                                                .cap = CLI_POOL_BYTES,
                                                .other = "no encryption",
                                                .other_cap = CLI_POOL_BYTES}) != NULL) ||
                (cellul.find(&(CatenaFinitaCfg){.src = args->failure->message,
                                                .cap = CLI_POOL_BYTES,
                                                .other = "SSL",
                                                .other_cap = CLI_POOL_BYTES}) != NULL))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = args->failure->hint,
                    .src = "The server takes only encrypted connections; impero reaches such a server "
                           "through an ssh tunnel to a machine beside it.",
                    .bytes = cellul.len(&(CatenaFinitaCfg){
                                 .src = "The server takes only encrypted connections; impero reaches such a server "
                                        "through an ssh tunnel to a machine beside it.",
                                 .cap = CLI_POOL_BYTES}) +
                             1u});
            }
            return 0;
        }
        if (type == 'Z')
        {
            args->pg->status = (args->pg->size > 0u) ? (char)args->pg->message[0] : 'I';
            return 1;
        }
    }
}
