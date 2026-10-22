// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_keychain.c: a secret kept in Windows' Credential Manager, a generic credential named "orior", its service and
// its account, such as "orior database" and a connection's account, on this machine alone
#include "../text/json_tree.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <wincred.h>

#include <stdint.h>

// the credential's name for `account` of `service`, wide, into `name`, which holds CLI_PATH_LONGEST
static void keychain_target(const char *service, const char *account, wchar_t *name)
{
    char text[CLI_PATH_LONGEST];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(&line, "orior ");
    cli_line_text(&line, service);
    cli_line_character(&line, ' ');
    cli_line_text(&line, account);
    cli_line_closed(&line);
    (void)MultiByteToWideChar(CP_UTF8, 0u, text, -1, name, CLI_PATH_LONGEST);
}

int cli_keychain_keep(const char *service, const char *account, const char *secret, CliLine *said)
{
    wchar_t name[CLI_PATH_LONGEST];
    wchar_t user[CLI_PATH_LONGEST];
    keychain_target(service, account, name);
    (void)MultiByteToWideChar(CP_UTF8, 0u, account, -1, user, CLI_PATH_LONGEST);
    CREDENTIALW credential;
    memor.set(&(MemoriaCfg){.dst = &credential, .val = 0, .bytes = sizeof(credential)});
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = name;
    credential.UserName = user;
    // the secret's bytes, as the credential carries a blob of them
    credential.CredentialBlobSize = (DWORD)cellul.len(&(CatenaFinitaCfg){.src = secret, .cap = CLI_CELLS_POOL});
    credential.CredentialBlob = (LPBYTE)(uintptr_t)secret;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&credential, 0u))
    {
        cli_line_text(said, "the keychain did not keep the secret: Windows said ");
        cli_line_decimal(said, GetLastError(), 1u);
        return 0;
    }
    return 1;
}

int cli_keychain_read(const char *service, const char *account, char *out, unsigned long long room)
{
    wchar_t name[CLI_PATH_LONGEST];
    keychain_target(service, account, name);
    PCREDENTIALW found = NULL;
    if (!CredReadW(name, CRED_TYPE_GENERIC, 0u, &found) || (found == NULL))
    {
        return 0;
    }
    CliLine line = {out, room, 0ull};
    cli_lossy(&line, found->CredentialBlob, found->CredentialBlobSize);
    cli_line_closed(&line);
    CredFree(found);
    return 1;
}

int cli_keychain_forget(const char *service, const char *account, CliLine *said)
{
    (void)said;
    wchar_t name[CLI_PATH_LONGEST];
    keychain_target(service, account, name);
    (void)CredDeleteW(name, CRED_TYPE_GENERIC, 0u);
    return 1;
}
#endif
