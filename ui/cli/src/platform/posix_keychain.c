// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_keychain.c: a secret stored in macOS's keychain through `security`, and elsewhere in the Secret Service through
// `secret-tool`, under the service "orior" and its service, such as "orior database", and its account
#include "../text/json_tree.h"

#if !defined(_WIN32)

// what the program wrote to its errors, trimmed, into `err_msg`
static void keychain_errors(const CliRan *ran, VerbaLinea *err_msg)
{
    unsigned long long start = 0ull;
    unsigned long long end = ran->err_size;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = ran->err[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = ran->err[end - 1u]}) != NULL))
    {
        end -= 1ull;
    }
    cli_lossy(err_msg, (const unsigned char *)ran->err + start, end - start);
}

// the keychain's name for `service`, "orior " and it, into `name`, sized for CLI_NAME_MAX_LEN
static void keychain_service(const char *service, char *name)
{
    VerbaLinea line = {name, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, "orior ");
    mmgr_line_put(&line, service);
    mmgr_line_finish(&line);
}

int cli_keychain_store(const char *service, const char *account, const char *secret, VerbaLinea *err_msg)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    char named[CLI_NAME_MAX_LEN];
    keychain_service(service, named);
    CliRan *const ran = cli_ran_taken();
#if defined(__APPLE__)
    const char *const words[10] = {"security", "add-generic-password", "-U", "-s", named, "-a", account, "-w", secret,
                                   NULL};
    const int started = cli_ran(words, NULL, NULL, 0u, ran, &err_buf);
    const char *const program = "security";
#else
    char label[CLI_PATH_MAX_LEN];
    VerbaLinea label_line = {label, sizeof(label), 0ull};
    mmgr_line_put(&label_line, named);
    mmgr_line_ch(&label_line, ' ');
    mmgr_line_put(&label_line, account);
    mmgr_line_finish(&label_line);
    const char *const words[9] = {"secret-tool", "store", "--label", label, "service", named, "account", account, NULL};
    const int started =
        cli_ran_fed(words, NULL, NULL, 0u, secret, cellul.len(&(CatenaFinitaCfg){.src = secret, .cap = CLI_CELLS_POOL}),
                    ran, &err_buf);
    const char *const program = "secret-tool, which stores secrets in the Secret Service";
#endif
    if (!started)
    {
        cli_ran_given(ran);
        mmgr_line_put(err_msg, program);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, err_buf.out, err_buf.at);
        return 0;
    }
    const int stored = (ran->code == 0ll);
    if (!stored)
    {
        keychain_errors(ran, err_msg);
    }
    cli_ran_given(ran);
    return stored;
}

int cli_keychain_read(const char *service, const char *account, char *out, unsigned long long room)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    char named[CLI_NAME_MAX_LEN];
    keychain_service(service, named);
#if defined(__APPLE__)
    const char *const words[9] = {"security", "find-generic-password", "-s", named, "-a", account, "-w", NULL, NULL};
#else
    const char *const words[7] = {"secret-tool", "lookup", "service", named, "account", account, NULL};
#endif
    CliRan *const ran = cli_ran_taken();
    const int found = cli_ran(words, NULL, NULL, 0u, ran, &err_buf) && (ran->code == 0ll);
    if (found)
    {
        unsigned long long end = ran->out_size;
        while ((end > 0u) && (ran->out[end - 1u] == '\n'))
        {
            end -= 1u;
        }
        VerbaLinea line = {out, room, 0ull};
        cli_lossy(&line, (const unsigned char *)ran->out, end);
        mmgr_line_finish(&line);
    }
    cli_ran_given(ran);
    return found;
}

int cli_keychain_forget(const char *service, const char *account, VerbaLinea *err_msg)
{
    char named[CLI_NAME_MAX_LEN];
    keychain_service(service, named);
#if defined(__APPLE__)
    const char *const words[8] = {"security", "delete-generic-password", "-s", named, "-a", account, NULL, NULL};
#else
    const char *const words[7] = {"secret-tool", "clear", "service", named, "account", account, NULL};
#endif
    CliRan *const ran = cli_ran_taken();
    const int ran_well = cli_ran(words, NULL, NULL, 0u, ran, err_msg);
    cli_ran_given(ran);
    return ran_well;
}
#endif
