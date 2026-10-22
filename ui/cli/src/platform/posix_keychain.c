// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_keychain.c: a secret kept in macOS's keychain through `security`, and elsewhere in the Secret Service through
// `secret-tool`, under the service "orior" and its service, such as "orior database", and its account
#include "../text/json_tree.h"

#if !defined(_WIN32)

// what the program said to its errors, trimmed, into `said`
static void keychain_errors(const CliRan *ran, CliLine *said)
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
    cli_lossy(said, (const unsigned char *)ran->err + start, end - start);
}

// the keychain's name for `service`, "orior " and it, into `name`, which holds CLI_NAME_LONGEST
static void keychain_service(const char *service, char *name)
{
    CliLine line = {name, CLI_NAME_LONGEST, 0ull};
    cli_line_text(&line, "orior ");
    cli_line_text(&line, service);
    cli_line_closed(&line);
}

int cli_keychain_keep(const char *service, const char *account, const char *secret, CliLine *said)
{
    char why[CLI_SAID_LONGEST];
    CliLine unsaid = {why, sizeof(why), 0ull};
    char named[CLI_NAME_LONGEST];
    keychain_service(service, named);
    CliRan *const ran = cli_ran_taken();
#if defined(__APPLE__)
    const char *const words[10] = {"security", "add-generic-password", "-U", "-s", named, "-a", account, "-w", secret,
                                   NULL};
    const int started = cli_ran(words, NULL, NULL, 0u, ran, &unsaid);
    const char *const program = "security";
#else
    char label[CLI_PATH_LONGEST];
    CliLine label_line = {label, sizeof(label), 0ull};
    cli_line_text(&label_line, named);
    cli_line_character(&label_line, ' ');
    cli_line_text(&label_line, account);
    cli_line_closed(&label_line);
    const char *const words[9] = {"secret-tool", "store", "--label", label, "service", named, "account", account, NULL};
    const int started = cli_ran_fed(words, NULL, NULL, 0u, secret,
                                    cellul.len(&(CatenaFinitaCfg){.src = secret, .cap = CLI_CELLS_POOL}), ran, &unsaid);
    const char *const program = "secret-tool, which keeps secrets in the Secret Service";
#endif
    if (!started)
    {
        cli_ran_given(ran);
        cli_line_text(said, program);
        cli_line_text(said, ": ");
        cli_line_counted(said, unsaid.out, unsaid.at);
        return 0;
    }
    const int kept = (ran->code == 0ll);
    if (!kept)
    {
        keychain_errors(ran, said);
    }
    cli_ran_given(ran);
    return kept;
}

int cli_keychain_read(const char *service, const char *account, char *out, unsigned long long room)
{
    char why[CLI_SAID_LONGEST];
    CliLine unsaid = {why, sizeof(why), 0ull};
    char named[CLI_NAME_LONGEST];
    keychain_service(service, named);
#if defined(__APPLE__)
    const char *const words[9] = {"security", "find-generic-password", "-s", named, "-a", account, "-w", NULL, NULL};
#else
    const char *const words[7] = {"secret-tool", "lookup", "service", named, "account", account, NULL};
#endif
    CliRan *const ran = cli_ran_taken();
    const int found = cli_ran(words, NULL, NULL, 0u, ran, &unsaid) && (ran->code == 0ll);
    if (found)
    {
        unsigned long long end = ran->out_size;
        while ((end > 0u) && (ran->out[end - 1u] == '\n'))
        {
            end -= 1u;
        }
        CliLine line = {out, room, 0ull};
        cli_lossy(&line, (const unsigned char *)ran->out, end);
        cli_line_closed(&line);
    }
    cli_ran_given(ran);
    return found;
}

int cli_keychain_forget(const char *service, const char *account, CliLine *said)
{
    char named[CLI_NAME_LONGEST];
    keychain_service(service, named);
#if defined(__APPLE__)
    const char *const words[8] = {"security", "delete-generic-password", "-s", named, "-a", account, NULL, NULL};
#else
    const char *const words[7] = {"secret-tool", "clear", "service", named, "account", account, NULL};
#endif
    CliRan *const ran = cli_ran_taken();
    const int ran_well = cli_ran(words, NULL, NULL, 0u, ran, said);
    cli_ran_given(ran);
    return ran_well;
}
#endif
