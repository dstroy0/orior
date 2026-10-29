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

static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_done = CLI_WAKE_INIT;

// the dialog shown and what was chosen written into the pick's path as UTF-8
static void picker_shown(CliPick *pick)
{
    const int saving = pick->kind == CLI_PICK_SAVE;
    IFileDialog *dialog = NULL;
    const HRESULT made = CoCreateInstance(saving ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog, NULL,
                                          CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&dialog);
    if (FAILED(made))
    {
        return;
    }
    FILEOPENDIALOGOPTIONS options = 0u;
    if ((pick->kind == CLI_PICK_FOLDER) && SUCCEEDED(IFileDialog_GetOptions(dialog, &options)))
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
            pick->chosen = WideCharToMultiByte(CP_UTF8, 0u, wide, -1, pick->path, (int)pick->room, NULL, NULL) > 0;
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
        picker_shown(pick);
        CoUninitialize();
    }
    cli_lock_take(&s_lock);
    pick->done = 1;
    cli_wake_all(&s_done);
    cli_lock_give(&s_lock);
}

int cli_file_picked(unsigned int kind, char *path, size_t room)
{
    CliPick pick = {kind, path, room, 0, 0, {picker_run, NULL, CLI_PRIORITY_WORK}};
    pick.thread.context = &pick;
    path[0] = '\0';
    if (!cli_thread_started(&pick.thread))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    while (!pick.done)
    {
        cli_wake_wait(&s_done, &s_lock);
    }
    cli_lock_give(&s_lock);
    return pick.chosen;
}
#endif
