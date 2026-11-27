// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_keychain.c: the keychain on Linux, the Secret Service through `secret-tool`, which reads the secret from its
// input (../posix_systems.h)
#include "../posix_systems.h"

#if defined(__linux__)

const char g_cli_keychain_program[] = "secret-tool, which stores secrets in the Secret Service";

int cli_keychain_store_started(const char *named, const CliErrMsgServiceCtx *args, CliRunResult *run_result,
                               VerbaLinea *err_buf)
{
    char label[CLI_PATH_MAX_LEN];
    VerbaLinea label_line = {label, sizeof(label), 0ULL};
    mmgr_line_put(&label_line, named);
    mmgr_line_ch(&label_line, ' ');
    mmgr_line_put(&label_line, args->account);
    mmgr_line_finish(&label_line);
    const char *const words[9] = {"secret-tool", "store",   "--label",     label, "service",
                                  named,         "account", args->account, NULL};
    return EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                      .count = 0u, .input = args->secret,
                      .length = cellul.len(&(CatenaFinitaCfg){.src = args->secret, .cap = CLI_POOL_BYTES}),
                      .run_result = run_result, .err_msg = err_buf);
}

void cli_keychain_lookup_words(const char **words, const char *named, const char *account)
{
    const char *const lookup[7] = {"secret-tool", "lookup", "service", named, "account", account, NULL};
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        words[at] = lookup[at];
    }
}

void cli_keychain_clear_words(const char **words, const char *named, const char *account)
{
    const char *const clear[7] = {"secret-tool", "clear", "service", named, "account", account, NULL};
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        words[at] = clear[at];
    }
}
#endif
