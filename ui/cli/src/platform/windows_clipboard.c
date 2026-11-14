// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_clipboard.c: the system's clipboard on Windows: its text, read and put as the system's own Unicode text; and
// files on it as the file manager puts them, a list of their whole paths, a cut said in its Preferred DropEffect as the
// file manager's own cut says it, so that a paste of files the file manager cut moves them. What goes on the clipboard
// is put in memory the system's clipboard takes and owns from then on, as the clipboard takes nothing else. The system
// saves another program out while the clipboard is open, but not another thread of this one, so one thread at a time
// opens it here, from its open to its close
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>

// how many times the clipboard is asked for while another program holds it, and the milliseconds apart
#define CLI_CLIPBOARD_TRIES 10u
#define CLI_CLIPBOARD_APART 5u
// the largest size the system's text calls count, an int
#define CLI_CLIPBOARD_INT_MAX 0x7FFFFFFFULL

// the drop effects a file manager reads: a copy, a move, and a link to the file
#define CLI_CLIPBOARD_COPY 1u
#define CLI_CLIPBOARD_MOVE 2u
#define CLI_CLIPBOARD_LINK 4u

// the units of the clipboard's text made UTF-8 at once
#define CLI_CLIPBOARD_PIECE 4096u

// the lock a thread takes as it opens the clipboard and gives back as it closes it

// the clipboard opened for this thread, the lock taken first, asked for again while another program holds it: 1, or
// 0 with the lock given back where it stays locked
static int clipboard_open(void)
{
    for (unsigned int requested = 0u; requested < CLI_CLIPBOARD_TRIES; requested += 1u)
    {
        if (OpenClipboard(NULL))
        {
            return 1;
        }
        Sleep(CLI_CLIPBOARD_APART);
    }
    return 0;
}

// the clipboard this thread opened closed, and the lock given back
static void clipboard_close(void)
{
    CloseClipboard();
}

// the format a file manager says a cut or a copy in
static UINT clipboard_effect_format(void)
{
    return RegisterClipboardFormatW(L"Preferred DropEffect");
}

int cli_clipboard_text_read(const CliCtx *args)
{
    if (!clipboard_open())
    {
        return 0;
    }
    const HANDLE clipboard_data = GetClipboardData(CF_UNICODETEXT);
    const wchar_t *const pixel_width = (clipboard_data != NULL) ? (const wchar_t *)GlobalLock(clipboard_data) : NULL;
    if (pixel_width != NULL)
    {
        const SIZE_T units_max = GlobalSize(clipboard_data) / sizeof(wchar_t);
        SIZE_T units = 0u;
        while ((units < units_max) && (pixel_width[units] != L'\0'))
        {
            units += 1u;
        }
        char piece[CLI_CLIPBOARD_PIECE * 3u];
        for (SIZE_T at = 0u; at < units;)
        {
            SIZE_T count = ((units - at) < CLI_CLIPBOARD_PIECE) ? (units - at) : CLI_CLIPBOARD_PIECE;
            // a character of two units is not cut between pieces
            const wchar_t last = pixel_width[at + count - 1u];
            count -= ((at + count < units) && (last >= 0xd800u) && (last <= 0xdbffu)) ? 1u : 0u;
            // a piece is at max CLI_CLIPBOARD_PIECE units, which an int counts
            const int bytes =
                WideCharToMultiByte(CP_UTF8, 0u, pixel_width + at, (int)count, piece, (int)sizeof(piece), NULL, NULL);
            mmgr_line_put_n(args->text_verba_linea_pointer, piece, (bytes > 0) ? (unsigned long long)bytes : 0ull);
            at += count;
        }
        GlobalUnlock(clipboard_data);
    }
    clipboard_close();
    return 1;
}

typedef struct
{
#if (defined(_WIN32))
    UINT format;
    HGLOBAL memory;
    UINT more_format;
    HGLOBAL more;
#endif
} WindowsClipboardCtx;
// `memory` put on the clipboard as `format`, and `more` as `more_format` where it is not NULL, the clipboard emptied
// first: 1, or 0 with each the clipboard did not take given back
EMBED_INLINE int clipboard_append(const WindowsClipboardCtx *args)
{
    if (!clipboard_open())
    {
        GlobalFree(args->memory);
        GlobalFree(args->more);
        return 0;
    }
    EmptyClipboard();
    const int reserved = SetClipboardData(args->format, args->memory) != NULL;
    const int more_allocated =
        reserved && ((args->more == NULL) || (SetClipboardData(args->more_format, args->more) != NULL));
    clipboard_close();
    if (!reserved)
    {
        GlobalFree(args->memory);
    }
    if (!more_allocated)
    {
        GlobalFree(args->more);
    }
    return reserved && more_allocated;
}

int cli_clipboard_text_write(const CliSizeTextCtx *args)
{
    if (args->size > CLI_CLIPBOARD_INT_MAX)
    {
        return 0;
    }
    // the size is at max what an int counts, as checked above
    const int units = (args->size > 0ull) ? MultiByteToWideChar(CP_UTF8, 0u, args->text, (int)args->size, NULL, 0) : 0;
    const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, ((SIZE_T)units + 1u) * sizeof(wchar_t));
    wchar_t *const pixel_width = (memory != NULL) ? (wchar_t *)GlobalLock(memory) : NULL;
    if (pixel_width == NULL)
    {
        GlobalFree(memory);
        return 0;
    }
    if (units > 0)
    {
        // the size is at max what an int counts, as checked above
        (void)MultiByteToWideChar(CP_UTF8, 0u, args->text, (int)args->size, pixel_width, units);
    }
    pixel_width[units] = L'\0';
    GlobalUnlock(memory);
    return EMBED_CALL(clipboard_append, WindowsClipboardCtx, .format = CF_UNICODETEXT, .memory = memory,
                      .more_format = 0u, .more = NULL);
}

int cli_clipboard_files_append(const CliCountCtx *args)
{
    // the list's head, then each path in UTF-16 with a 0 after it, and one 0 more at the end
    SIZE_T units = 1u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const int path_units = MultiByteToWideChar(CP_UTF8, 0u, args->paths[at], -1, NULL, 0);
        if (path_units <= 0)
        {
            return 0;
        }
        // a count the system gave is positive here
        units += (SIZE_T)path_units;
    }
    const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + (units * sizeof(wchar_t)));
    DROPFILES *const files = (memory != NULL) ? (DROPFILES *)GlobalLock(memory) : NULL;
    const HGLOBAL effect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
    DWORD *const effect_value = (effect != NULL) ? (DWORD *)GlobalLock(effect) : NULL;
    if ((files == NULL) || (effect_value == NULL))
    {
        GlobalFree(memory);
        GlobalFree(effect);
        return 0;
    }
    files->pFiles = sizeof(DROPFILES);
    files->fWide = TRUE;
    wchar_t *at = (wchar_t *)(void *)((unsigned char *)files + sizeof(DROPFILES));
    SIZE_T left = units;
    for (unsigned int path = 0u; path < args->count; path += 1u)
    {
        // what is left is at max the units counted above, which an int counts
        const int written = MultiByteToWideChar(CP_UTF8, 0u, args->paths[path], -1, at, (int)left);
        // a count the system gave is positive here
        at += (written > 0) ? (SIZE_T)written : 0u;
        left -= (written > 0) ? (SIZE_T)written : 0u;
    }
    *at = L'\0';
    GlobalUnlock(memory);
    *effect_value = args->cut ? CLI_CLIPBOARD_MOVE : (CLI_CLIPBOARD_COPY | CLI_CLIPBOARD_LINK);
    GlobalUnlock(effect);
    return EMBED_CALL(clipboard_append, WindowsClipboardCtx, .format = CF_HDROP, .memory = memory,
                      .more_format = clipboard_effect_format(), .more = effect);
}

unsigned int cli_clipboard_files_read(const CliEachCtx *args)
{
    int *cut = args->cut_pointer;
    *cut = -1;
    if (!clipboard_open())
    {
        return 0u;
    }
    const HDROP clipboard_data = (HDROP)GetClipboardData(CF_HDROP);
    unsigned int count = 0u;
    const UINT files = (clipboard_data != NULL) ? DragQueryFileW(clipboard_data, 0xffffffffu, NULL, 0u) : 0u;
    for (UINT at = 0u; at < files; at += 1u)
    {
        wchar_t pixel_width[CLI_PATH_MAX_LEN];
        char path[CLI_PATH_MAX_LEN];
        const int converted =
            (DragQueryFileW(clipboard_data, at, pixel_width, CLI_PATH_MAX_LEN) != 0u) &&
            (WideCharToMultiByte(CP_UTF8, 0u, pixel_width, -1, path, (int)sizeof(path), NULL, NULL) > 0);
        if (converted)
        {
            EMBED_CALL(args->each, CliEachPathCfg, .context = args->context, .path = path);
            count += 1u;
        }
    }
    const HANDLE effect = (count > 0u) ? GetClipboardData(clipboard_effect_format()) : NULL;
    const DWORD *const effect_value = (effect != NULL) ? (const DWORD *)GlobalLock(effect) : NULL;
    if (effect_value != NULL)
    {
        *cut = ((*effect_value & CLI_CLIPBOARD_MOVE) != 0u) && ((*effect_value & CLI_CLIPBOARD_COPY) == 0u);
        GlobalUnlock(effect);
    }
    clipboard_close();
    return count;
}

void cli_clipboard_clear(void)
{
    if (clipboard_open())
    {
        EmptyClipboard();
        clipboard_close();
    }
}
#endif
