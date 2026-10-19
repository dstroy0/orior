// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_keychain.c: a database's password kept in Windows' Credential Manager, a generic credential named "orior
// database" and the connection's account, on this machine alone
#include "../databases/db.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <wincred.h>

#include <stdint.h>

// the credential's name for `account`, wide, into `name`, which holds CLI_PATH_LONGEST
static void keychain_target(const char *account, wchar_t *name)
{
    char text[CLI_PATH_LONGEST];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(&line, "orior database ");
    cli_line_text(&line, account);
    cli_line_closed(&line);
    (void)MultiByteToWideChar(CP_UTF8, 0u, text, -1, name, CLI_PATH_LONGEST);
}

int cli_keychain_keep(const char *account, const char *password, CliLine *said)
{
    wchar_t name[CLI_PATH_LONGEST];
    wchar_t user[CLI_PATH_LONGEST];
    keychain_target(account, name);
    (void)MultiByteToWideChar(CP_UTF8, 0u, account, -1, user, CLI_PATH_LONGEST);
    CREDENTIALW credential;
    memor.set(&(MemoriaCfg){.dst = &credential, .val = 0, .bytes = sizeof(credential)});
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = name;
    credential.UserName = user;
    // the password's bytes, as the credential carries a blob of them
    credential.CredentialBlobSize = (DWORD)cellul.len(&(CatenaFinitaCfg){.src = password, .cap = CLI_CELLS_POOL});
    credential.CredentialBlob = (LPBYTE)(uintptr_t)password;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&credential, 0u))
    {
        cli_line_text(said, "the keychain did not keep the password: Windows said ");
        cli_line_decimal(said, GetLastError(), 1u);
        return 0;
    }
    return 1;
}

int cli_keychain_read(const char *account, char *out, unsigned long long room)
{
    wchar_t name[CLI_PATH_LONGEST];
    keychain_target(account, name);
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

int cli_keychain_forget(const char *account, CliLine *said)
{
    (void)said;
    wchar_t name[CLI_PATH_LONGEST];
    keychain_target(account, name);
    (void)CredDeleteW(name, CRED_TYPE_GENERIC, 0u);
    return 1;
}
#endif
