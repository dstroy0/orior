// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_keychain.c: a secret stored in macOS's keychain through `security`, and elsewhere in the Secret Service through
// `secret-tool`, under the service "orior" and its service, such as "orior database", and its account
#include "../text/json_tree.h"
#include "../text/text.h"

typedef struct
{
#if (!defined(_WIN32))
    const CliRunResult *run_result;
    VerbaLinea *err_msg;
    const char *service;
    char *name;
#endif
} PosixKeychainCtx;

#if !defined(_WIN32)

// what the program wrote to its errors, trimmed, into `err_msg`
EMBED_INLINE void keychain_errors(const PosixKeychainCtx *args)
{
    unsigned long long start = 0ULL;
    unsigned long long end = args->run_result->err_size;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = args->run_result->err[start]}) != NULL))
    {
        start += 1ULL;
    }
    while ((end > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_POOL_BYTES, .byte = args->run_result->err[end - 1u]}) != NULL))
    {
        end -= 1ULL;
    }
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
               .text_bytes = (const unsigned char *)args->run_result->err + start, .length = end - start);
}

// the keychain's name for `service`, "orior " and it, into `name`, sized for CLI_NAME_MAX_LEN
EMBED_INLINE void keychain_service(const PosixKeychainCtx *args)
{
    VerbaLinea line = {args->name, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&line, "orior ");
    mmgr_line_put(&line, args->service);
    mmgr_line_finish(&line);
}

int cli_keychain_store(const CliErrMsgServiceCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ULL};
    char named[CLI_NAME_MAX_LEN];
    EMBED_CALL(keychain_service, PosixKeychainCtx, .service = args->service, .name = named);
    CliRunResult *const run_result = cli_run_result_alloc();
#if defined(__APPLE__)
    const char *const words[10] = {"security", "add-generic-password", "-U", "-s",         named,
                                   "-a",       args->account,          "-w", args->secret, NULL};
    const int started = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL,
                                   .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &err_buf);
    const char *const program = "security";
#else
    char label[CLI_PATH_MAX_LEN];
    VerbaLinea label_line = {label, sizeof(label), 0ULL};
    mmgr_line_put(&label_line, named);
    mmgr_line_ch(&label_line, ' ');
    mmgr_line_put(&label_line, args->account);
    mmgr_line_finish(&label_line);
    const char *const words[9] = {"secret-tool", "store",   "--label",     label, "service",
                                  named,         "account", args->account, NULL};
    const int started = EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = NULL,
                                   .settings = NULL, .count = 0u, .input = args->secret,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->secret, .cap = CLI_POOL_BYTES}),
                                   .run_result = run_result, .err_msg = &err_buf);
    const char *const program = "secret-tool, which stores secrets in the Secret Service";
#endif
    if (!started)
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(args->err_msg, program);
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put_n(args->err_msg, err_buf.out, err_buf.at);
        return 0;
    }
    const int stored = (run_result->code == 0LL);
    if (!stored)
    {
        EMBED_CALL(keychain_errors, PosixKeychainCtx, .run_result = run_result, .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return stored;
}

int cli_keychain_read(const CliServiceCtx *args)
{
    char *out = args->out;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ULL};
    char named[CLI_NAME_MAX_LEN];
    EMBED_CALL(keychain_service, PosixKeychainCtx, .service = args->service, .name = named);
#if defined(__APPLE__)
    const char *const words[9] = {"security", "find-generic-password", "-s", named, "-a", args->account, "-w", NULL,
                                  NULL};
#else
    const char *const words[7] = {"secret-tool", "lookup", "service", named, "account", args->account, NULL};
#endif
    CliRunResult *const run_result = cli_run_result_alloc();
    const int found = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                                 .count = 0u, .run_result = run_result, .err_msg = &err_buf) &&
                      (run_result->code == 0LL);
    if (found)
    {
        unsigned long long end = run_result->out_size;
        end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = run_result->out, .at = end,
                         .set = "\n");
        VerbaLinea line = {out, args->capacity, 0ULL};
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line,
                   .text_bytes = (const unsigned char *)run_result->out, .length = end);
        mmgr_line_finish(&line);
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
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
    CliRunResult *const run_result = cli_run_result_alloc();
    const int run_result_well =
        EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u,
                   .run_result = run_result, .err_msg = args->err_msg);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return run_result_well;
}
#endif
