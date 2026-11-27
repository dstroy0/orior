// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_keychain.c: the keychain on macOS, the login keychain through `security`, which takes the secret as a word
// (../posix_systems.h)
#include "../posix_systems.h"

#if defined(__APPLE__)

const char g_cli_keychain_program[] = "security";

int cli_keychain_store_started(const char *named, const CliErrMsgServiceCtx *args, CliRunResult *run_result,
                               VerbaLinea *err_buf)
{
    const char *const words[10] = {"security", "add-generic-password", "-U", "-s",         named,
                                   "-a",       args->account,          "-w", args->secret, NULL};
    return EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                      .count = 0u, .run_result = run_result, .err_msg = err_buf);
}

void cli_keychain_lookup_words(const char **words, const char *named, const char *account)
{
    const char *const lookup[8] = {"security", "find-generic-password", "-s", named, "-a", account, "-w", NULL};
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        words[at] = lookup[at];
    }
}

void cli_keychain_clear_words(const char **words, const char *named, const char *account)
{
    const char *const clear[7] = {"security", "delete-generic-password", "-s", named, "-a", account, NULL};
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        words[at] = clear[at];
    }
}
#endif
