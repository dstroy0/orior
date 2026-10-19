// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_registry.c: the PATH as the registry holds it now, the system's and the reader's, a variable of the reader's
// own read and set as text whose %NAME% parts expand, and every open program told the environment changed
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the most UTF-16 units of a value read or written
#define CLI_REGISTRY_UNITS 32768u

// a value of `key` under `root` read into `out` in UTF-8: 1, or 0 where it is none or no text
static int cli_read(HKEY root, const wchar_t *key, const char *name, char *out, unsigned long long room)
{
    static wchar_t s_data[CLI_REGISTRY_UNITS];
    static CliLock s_lock = CLI_LOCK_INIT;
    wchar_t wide_name[CLI_NAME_LONGEST];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, CLI_NAME_LONGEST) == 0)
    {
        return 0;
    }
    HKEY opened = NULL;
    if (RegOpenKeyExW(root, key, 0u, KEY_READ, &opened) != ERROR_SUCCESS)
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    DWORD kind = 0u;
    DWORD size = sizeof(s_data) - sizeof(wchar_t);
    const LONG said = RegQueryValueExW(opened, wide_name, NULL, &kind, (LPBYTE)s_data, &size);
    RegCloseKey(opened);
    if ((said != ERROR_SUCCESS) || ((kind != REG_SZ) && (kind != REG_EXPAND_SZ)))
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    s_data[size / sizeof(wchar_t)] = L'\0';
    // the room is far below what an int counts
    const int written = WideCharToMultiByte(CP_UTF8, 0u, s_data, -1, out, (int)room, NULL, NULL);
    cli_lock_give(&s_lock);
    return written > 0;
}

int cli_registry_machine_path(char *out, unsigned long long room)
{
    return cli_read(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", "Path",
                    out, room);
}

int cli_registry_user_value(const char *name, char *out, unsigned long long room)
{
    return cli_read(HKEY_CURRENT_USER, L"Environment", name, out, room);
}

int cli_registry_set_user_value(const char *name, const char *value, CliLine *said)
{
    static wchar_t s_data[CLI_REGISTRY_UNITS];
    wchar_t wide_name[CLI_NAME_LONGEST];
    const int units = MultiByteToWideChar(CP_UTF8, 0, value, -1, s_data, CLI_REGISTRY_UNITS);
    if ((units == 0) || (MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name, CLI_NAME_LONGEST) == 0))
    {
        cli_line_text(said, name);
        cli_line_text(said, " was not written: it is too long");
        return 0;
    }
    HKEY opened = NULL;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0u, KEY_READ | KEY_WRITE, &opened);
    if (result != ERROR_SUCCESS)
    {
        cli_line_text(said, "the reader's environment did not open: error ");
        cli_line_signed(said, result);
        return 0;
    }
    // the units written count the closing zero, and each is two bytes
    result = RegSetValueExW(opened, wide_name, 0u, REG_EXPAND_SZ, (const BYTE *)s_data, (DWORD)units * 2u);
    RegCloseKey(opened);
    if (result != ERROR_SUCCESS)
    {
        cli_line_text(said, name);
        cli_line_text(said, " was not written: error ");
        cli_line_signed(said, result);
        return 0;
    }
    return 1;
}

void cli_registry_announce(void)
{
    DWORD_PTR result = 0u;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0u, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 3000u, &result);
}

void cli_registry_expand(const char *text, char *out, unsigned long long room)
{
    static wchar_t s_source[CLI_REGISTRY_UNITS];
    static wchar_t s_expanded[CLI_REGISTRY_UNITS];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    const int read = MultiByteToWideChar(CP_UTF8, 0, text, -1, s_source, CLI_REGISTRY_UNITS);
    const DWORD written = (read != 0) ? ExpandEnvironmentStringsW(s_source, s_expanded, CLI_REGISTRY_UNITS) : 0u;
    // the room is far below what an int counts
    const int converted = ((written != 0u) && (written <= CLI_REGISTRY_UNITS))
                              ? WideCharToMultiByte(CP_UTF8, 0u, s_expanded, -1, out, (int)room, NULL, NULL)
                              : 0;
    cli_lock_give(&s_lock);
    if (converted == 0)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = out, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
    }
}
#endif
