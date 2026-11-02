// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_picker.c: the system's picker on Windows, its common item dialog: a file to open, a folder, or a file to
// save, shown on a thread of its own in a single-threaded apartment, as the dialog must be, while the caller waits
#include "../command_line/cli_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#define COBJMACROS
#include <windows.h>

#include <shobjidl.h>

#if (defined(_WIN32))
// A pick on its thread: what is picked, the path chosen and its room, and whether one was chosen and the thread is done
typedef struct
{
    unsigned int kind;
    char *path;
    size_t room;
    int chosen;
    int done;
    CliThread thread;
} CliPick;
#endif

static CliWake s_done = CLI_WAKE_INIT;

typedef struct
{
#if (defined(_WIN32))
    CliPick *pick;
#endif
} WindowsPickerCtx;
// the dialog shown and what was chosen written into the pick's path as UTF-8
EMBED_INLINE void picker_show(const WindowsPickerCtx *args)
{
    const int saving = args->pick->kind == CLI_PICK_SAVE;
    IFileDialog *dialog = NULL;
    const HRESULT made = CoCreateInstance(saving ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog, NULL,
                                          CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&dialog);
    if (FAILED(made))
    {
        return;
    }
    FILEOPENDIALOGOPTIONS options = 0u;
    if ((args->pick->kind == CLI_PICK_FOLDER) && SUCCEEDED(IFileDialog_GetOptions(dialog, &options)))
    {
        (void)IFileDialog_SetOptions(dialog, options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    IShellItem *item = NULL;
    if (SUCCEEDED(IFileDialog_Show(dialog, NULL)) && SUCCEEDED(IFileDialog_GetResult(dialog, &item)))
    {
        wchar_t *wide = NULL;
        if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &wide)))
        {
            // a room is far below what an int counts
            args->pick->chosen =
                WideCharToMultiByte(CP_UTF8, 0u, wide, -1, args->pick->path, (int)args->pick->room, NULL, NULL) > 0;
            CoTaskMemFree(wide);
        }
        IShellItem_Release(item);
    }
    IFileDialog_Release(dialog);
}

// a pick's thread: its apartment entered, the dialog shown, and the caller woken
static void picker_run(void *context)
{
    CliPick *const pick = context;
    const HRESULT entered = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(entered))
    {
        EMBED_CALL(picker_show, WindowsPickerCtx, .pick = pick);
        CoUninitialize();
    }
    pick->done = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_done);
}

int cli_file_pick(const CliPathRoomCtx *args)
{
    CliPick pick = {args->kind, args->path_out, args->room_size, 0, 0, {picker_run, NULL, CLI_PRIORITY_WORK}};
    pick.thread.context = &pick;
    args->path_out[0] = '\0';
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &pick.thread))
    {
        return 0;
    }
    while (!pick.done)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_done);
    }
    return pick.chosen;
}
#endif
