// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_keychain.c: a secret stored in Windows' Credential Manager, a generic credential named "orior", its service
// and its account, such as "orior database" and a connection's account, on this machine alone
#include "../text/json_tree.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <wincred.h>

typedef struct
{
#if (defined(_WIN32))
    const char *service;
    const char *account;
    wchar_t *name;
#endif
} WindowsKeychainCtx;

// the credential's name for `account` of `service`, wide, into `name`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void keychain_target(const WindowsKeychainCtx *args)
{
    char text[CLI_PATH_MAX_LEN];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "orior ");
    mmgr_line_put(&line, args->service);
    mmgr_line_ch(&line, ' ');
    mmgr_line_put(&line, args->account);
    mmgr_line_finish(&line);
    (void)MultiByteToWideChar(CP_UTF8, 0u, text, -1, args->name, CLI_PATH_MAX_LEN);
}

int cli_keychain_store(const CliErrMsgServiceCtx *args)
{
    wchar_t name[CLI_PATH_MAX_LEN];
    wchar_t user[CLI_PATH_MAX_LEN];
    EMBED_CALL(keychain_target, WindowsKeychainCtx, .service = args->service, .account = args->account, .name = name);
    (void)MultiByteToWideChar(CP_UTF8, 0u, args->account, -1, user, CLI_PATH_MAX_LEN);
    CREDENTIALW credential;
    memor.set(&(MemoriaCfg){.dst = &credential, .val = 0, .bytes = sizeof(credential)});
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = name;
    credential.UserName = user;
    // the secret's bytes, as the credential carries a blob of them
    credential.CredentialBlobSize = (DWORD)cellul.len(&(CatenaFinitaCfg){.src = args->secret, .cap = CLI_POOL_BYTES});
    credential.CredentialBlob = (LPBYTE)(embed_word)args->secret;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteW(&credential, 0u))
    {
        mmgr_line_put(args->err_msg, "the keychain did not store the secret: Windows reported ");
        mmgr_line_decimal(args->err_msg, GetLastError(), 1u);
        return 0;
    }
    return 1;
}

int cli_keychain_read(const CliServiceCtx *args)
{
    char *out = args->out;
    wchar_t name[CLI_PATH_MAX_LEN];
    EMBED_CALL(keychain_target, WindowsKeychainCtx, .service = args->service, .account = args->account, .name = name);
    PCREDENTIALW found = NULL;
    if (!CredReadW(name, CRED_TYPE_GENERIC, 0u, &found) || (found == NULL))
    {
        return 0;
    }
    VerbaLinea line = {out, args->capacity, 0ull};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line,
               .text_bytes = found->CredentialBlob, .length = found->CredentialBlobSize);
    mmgr_line_finish(&line);
    CredFree(found);
    return 1;
}

int cli_keychain_forget(const CliErrMsgServiceCtx *args)
{
    wchar_t name[CLI_PATH_MAX_LEN];
    EMBED_CALL(keychain_target, WindowsKeychainCtx, .service = args->service, .account = args->account, .name = name);
    (void)CredDeleteW(name, CRED_TYPE_GENERIC, 0u);
    return 1;
}
#endif
