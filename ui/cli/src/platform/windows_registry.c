// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_registry.c: the PATH as the registry stores it now, the system's and the reader's, a variable of the reader's
// own read and set as text whose %NAME% parts expand, and every open program notified the environment changed
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the most UTF-16 units of a value read or written
#define CLI_REGISTRY_UNITS 32768u

typedef struct
{
#if (defined(_WIN32))
    HKEY root;
    const wchar_t *key;
    const char *name;
    char *out;
    unsigned long long capacity;
#endif
} WindowsRegistryCtx;
// a value of `key` under `root` read into `out` in UTF-8: 1, or 0 where it is none or no text
EMBED_INLINE int cli_read(const WindowsRegistryCtx *args)
{
    static wchar_t s_data[CLI_REGISTRY_UNITS];
    wchar_t wide_name[CLI_NAME_MAX_LEN];
    if (MultiByteToWideChar(CP_UTF8, 0, args->name, -1, wide_name, CLI_NAME_MAX_LEN) == 0)
    {
        return 0;
    }
    HKEY opened = NULL;
    if (RegOpenKeyExW(args->root, args->key, 0u, KEY_READ, &opened) != ERROR_SUCCESS)
    {
        return 0;
    }
    DWORD kind = 0u;
    DWORD size = sizeof(s_data) - sizeof(wchar_t);
    const LONG status = RegQueryValueExW(opened, wide_name, NULL, &kind, (LPBYTE)s_data, &size);
    RegCloseKey(opened);
    if ((status != ERROR_SUCCESS) || ((kind != REG_SZ) && (kind != REG_EXPAND_SZ)))
    {
        return 0;
    }
    s_data[size / sizeof(wchar_t)] = L'\0';
    // the capacity is far below what an int counts
    const int written = WideCharToMultiByte(CP_UTF8, 0u, s_data, -1, args->out, (int)args->capacity, NULL, NULL);
    return written > 0;
}

int cli_registry_machine_path(const CliOutCtx *args)
{
    return EMBED_CALL(cli_read, WindowsRegistryCtx, .root = HKEY_LOCAL_MACHINE,
                      .key = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", .name = "Path",
                      .out = args->out, .capacity = args->capacity);
}

int cli_registry_user_value(const CliOutCapacityCtx *args)
{
    return EMBED_CALL(cli_read, WindowsRegistryCtx, .root = HKEY_CURRENT_USER, .key = L"Environment",
                      .name = args->name, .out = args->out, .capacity = args->capacity);
}

int cli_registry_set_user_value(const CliErrMsgNameCtx *args)
{
    static wchar_t s_data[CLI_REGISTRY_UNITS];
    wchar_t wide_name[CLI_NAME_MAX_LEN];
    const int units = MultiByteToWideChar(CP_UTF8, 0, args->value, -1, s_data, CLI_REGISTRY_UNITS);
    if ((units == 0) || (MultiByteToWideChar(CP_UTF8, 0, args->name, -1, wide_name, CLI_NAME_MAX_LEN) == 0))
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " was not written: it is too long");
        return 0;
    }
    HKEY opened = NULL;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0u, KEY_READ | KEY_WRITE, &opened);
    if (result != ERROR_SUCCESS)
    {
        mmgr_line_put(args->err_msg, "the reader's environment did not open: error ");
        mmgr_line_decimal_signed(args->err_msg, result);
        return 0;
    }
    // the units written count the closing zero, and each is two bytes
    result = RegSetValueExW(opened, wide_name, 0u, REG_EXPAND_SZ, (const BYTE *)s_data, (DWORD)units * 2u);
    RegCloseKey(opened);
    if (result != ERROR_SUCCESS)
    {
        mmgr_line_put(args->err_msg, args->name);
        mmgr_line_put(args->err_msg, " was not written: error ");
        mmgr_line_decimal_signed(args->err_msg, result);
        return 0;
    }
    return 1;
}

void cli_registry_announce(void)
{
    DWORD_PTR result = 0u;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0u, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 3000u, &result);
}

void cli_registry_expand(const CliOutCtx *args)
{
    static wchar_t s_source[CLI_REGISTRY_UNITS];
    static wchar_t s_expanded[CLI_REGISTRY_UNITS];
    const int read = MultiByteToWideChar(CP_UTF8, 0, args->text, -1, s_source, CLI_REGISTRY_UNITS);
    const DWORD written = (read != 0) ? ExpandEnvironmentStringsW(s_source, s_expanded, CLI_REGISTRY_UNITS) : 0u;
    // the capacity is far below what an int counts
    const int converted =
        ((written != 0u) && (written <= CLI_REGISTRY_UNITS))
            ? WideCharToMultiByte(CP_UTF8, 0u, s_expanded, -1, args->out, (int)args->capacity, NULL, NULL)
            : 0;
    if (converted == 0)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->out,
                          .src = args->text,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) + 1u});
    }
}
#endif
