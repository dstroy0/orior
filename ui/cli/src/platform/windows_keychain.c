// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_keychain.c: a secret stored in Windows' Credential Manager, a generic credential named "orior", its service
// and its account, such as "orior database" and a connection's account, on this machine alone
#include "../text/json_tree.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <wincred.h>

#include <stdint.h>

// the credential's name for `account` of `service`, wide, into `name`, sized for CLI_PATH_MAX_LEN
static void keychain_target(const char *service, const char *account, wchar_t *name)
{
    char text[CLI_PATH_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "orior ");
    mmgr_line_put(&line, service);
    mmgr_line_ch(&line, ' ');
    mmgr_line_put(&line, account);
    mmgr_line_finish(&line);
    (void)MultiByteToWideChar(CP_UTF8, 0u, text, -1, name, CLI_PATH_MAX_LEN);
}

int cli_keychain_store(const char *service, const char *account, const char *secret, VerbaLinea *err_msg)
{
    wchar_t name[CLI_PATH_MAX_LEN];
    wchar_t user[CLI_PATH_MAX_LEN];
    keychain_target(service, account, name);
    (void)MultiByteToWideChar(CP_UTF8, 0u, account, -1, user, CLI_PATH_MAX_LEN);
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
        mmgr_line_put(err_msg, "the keychain did not store the secret: Windows reported ");
        mmgr_line_decimal(err_msg, GetLastError(), 1u);
        return 0;
    }
    return 1;
}

int cli_keychain_read(const char *service, const char *account, char *out, unsigned long long room)
{
    wchar_t name[CLI_PATH_MAX_LEN];
    keychain_target(service, account, name);
    PCREDENTIALW found = NULL;
    if (!CredReadW(name, CRED_TYPE_GENERIC, 0u, &found) || (found == NULL))
    {
        return 0;
    }
    VerbaLinea line = {out, room, 0ull};
    cli_lossy(&line, found->CredentialBlob, found->CredentialBlobSize);
    mmgr_line_finish(&line);
    CredFree(found);
    return 1;
}

int cli_keychain_forget(const char *service, const char *account, VerbaLinea *err_msg)
{
    (void)err_msg;
    wchar_t name[CLI_PATH_MAX_LEN];
    keychain_target(service, account, name);
    (void)CredDeleteW(name, CRED_TYPE_GENERIC, 0u);
    return 1;
}
#endif
