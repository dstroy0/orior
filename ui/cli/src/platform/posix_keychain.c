// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_keychain.c: a secret stored in macOS's keychain through `security`, and elsewhere in the Secret Service through
// `secret-tool`, under the service "orior" and its service, such as "orior database", and its account
#include "../text/json_tree.h"
#include "../text/text.h"

typedef struct
{
#if (!defined(_WIN32))
    const CliRan *ran;
    VerbaLinea *err_msg;
    const char *service;
    char *name;
#endif
} PosixKeychainCtx;

#if !defined(_WIN32)

// what the program wrote to its errors, trimmed, into `err_msg`
EMBED_INLINE void keychain_errors(const PosixKeychainCtx *args)
{
    unsigned long long start = 0ull;
    unsigned long long end = args->ran->err_size;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->ran->err[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->ran->err[end - 1u]}) != NULL))
    {
        end -= 1ull;
    }
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
               .text_bytes = (const unsigned char *)args->ran->err + start, .length = end - start);
}

// the keychain's name for `service`, "orior " and it, into `name`, sized for CLI_NAME_MAX_LEN
EMBED_INLINE void keychain_service(const PosixKeychainCtx *args)
{
    VerbaLinea line = {args->name, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, "orior ");
    mmgr_line_put(&line, args->service);
    mmgr_line_finish(&line);
}

int cli_keychain_store(const CliErrMsgServiceCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    char named[CLI_NAME_MAX_LEN];
    EMBED_CALL(keychain_service, PosixKeychainCtx, .service = args->service, .name = named);
    CliRan *const ran = cli_ran_take();
#if defined(__APPLE__)
    const char *const words[10] = {"security", "add-generic-password", "-U", "-s",         named,
                                   "-a",       args->account,          "-w", args->secret, NULL};
    const int started = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL,
                                   .settings = NULL, .count = 0u, .ran = ran, .err_msg = &err_buf);
    const char *const program = "security";
#else
    char label[CLI_PATH_MAX_LEN];
    VerbaLinea label_line = {label, sizeof(label), 0ull};
    mmgr_line_put(&label_line, named);
    mmgr_line_ch(&label_line, ' ');
    mmgr_line_put(&label_line, args->account);
    mmgr_line_finish(&label_line);
    const char *const words[9] = {"secret-tool", "store",   "--label",     label, "service",
                                  named,         "account", args->account, NULL};
    const int started = EMBED_CALL(cli_words_internal.ran_feed, CliWordsCtx, .words = words, .folder = NULL,
                                   .settings = NULL, .count = 0u, .input = args->secret,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->secret, .cap = CLI_CELLS_POOL}),
                                   .ran = ran, .err_msg = &err_buf);
    const char *const program = "secret-tool, which stores secrets in the Secret Service";
#endif
    if (!started)
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        mmgr_line_put(args->err_msg, program);
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put_n(args->err_msg, err_buf.out, err_buf.at);
        return 0;
    }
    const int stored = (ran->code == 0ll);
    if (!stored)
    {
        EMBED_CALL(keychain_errors, PosixKeychainCtx, .ran = ran, .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return stored;
}

int cli_keychain_read(const CliServiceCtx *args)
{
    char *out = args->out;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    char named[CLI_NAME_MAX_LEN];
    EMBED_CALL(keychain_service, PosixKeychainCtx, .service = args->service, .name = named);
#if defined(__APPLE__)
    const char *const words[9] = {"security", "find-generic-password", "-s", named, "-a", args->account, "-w", NULL,
                                  NULL};
#else
    const char *const words[7] = {"secret-tool", "lookup", "service", named, "account", args->account, NULL};
#endif
    CliRan *const ran = cli_ran_take();
    const int found = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                                 .count = 0u, .ran = ran, .err_msg = &err_buf) &&
                      (ran->code == 0ll);
    if (found)
    {
        unsigned long long end = ran->out_size;
        end = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = ran->out, .at = end,
                         .set = "\n");
        VerbaLinea line = {out, args->room, 0ull};
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line,
                   .text_bytes = (const unsigned char *)ran->out, .length = end);
        mmgr_line_finish(&line);
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return found;
}

int cli_keychain_forget(const CliErrMsgServiceCtx *args)
{
    char named[CLI_NAME_MAX_LEN];
    EMBED_CALL(keychain_service, PosixKeychainCtx, .service = args->service, .name = named);
#if defined(__APPLE__)
    const char *const words[8] = {"security", "delete-generic-password", "-s", named, "-a", args->account, NULL, NULL};
#else
    const char *const words[7] = {"secret-tool", "clear", "service", named, "account", args->account, NULL};
#endif
    CliRan *const ran = cli_ran_take();
    const int ran_well = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL,
                                    .settings = NULL, .count = 0u, .ran = ran, .err_msg = args->err_msg);
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return ran_well;
}
#endif
