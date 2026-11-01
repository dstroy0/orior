// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_clipboard.c: the window's calls on the system's clipboard: its text, read here so that the page never
// asks the reader for leave to read it, and put here so that the page never asks for leave to write it; and files and
// folders of the tree put on it cut or copied, for another window of orior or the system's file manager to paste, and
// those it holds pasted into a folder of the tree, moved where they were cut. A cut the clipboard does not say is known
// to the window it was made in alone, by the paths it put; files are cut, copied and pasted on this machine only
#include "../comms/link.h"
#include "../editor/files.h"
#include "window_internal.h"

// the most files one copy or paste takes, and the bytes of their paths read off the clipboard
#define CLI_CLIPBOARD_FILES 4096u
#define CLI_CLIPBOARD_PATHS (1u << 20u)

// the bytes of a text read off the clipboard written at once into the answer
#define CLI_CLIPBOARD_PIECE 4096u

// The files read off the clipboard: the texts of their paths, a pointer to each, their count, and whether every one fit
typedef struct
{
    char text[CLI_CLIPBOARD_PATHS];
    unsigned long long used;
    const char *path[CLI_CLIPBOARD_FILES + 1u];
    unsigned int count;
    int whole;
} CliClipboardFiles;

static CliClipboardFiles s_files;
static CliLock s_lock = CLI_LOCK_INIT;

// the files this window last cut, by the digest of their paths and their count, 0 for none: where the clipboard cannot
// say a cut, a paste of the same files is one
static unsigned long long s_cut_digest;
static unsigned int s_cut_count;

// the paths `count` of them, digested: FNV-1a over their bytes, each path ended by a 0
static unsigned long long clipboard_digest(const char *const *paths, unsigned int count)
{
    unsigned long long digest = 0xcbf29ce484222325ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        for (const char *letter = paths[at];; letter += 1)
        {
            digest = (digest ^ (unsigned char)*letter) * 0x100000001b3ull;
            if (*letter == '\0')
            {
                break;
            }
        }
    }
    return digest;
}

// a path the clipboard holds kept among the files read
static void clipboard_file_kept(void *context, const char *path)
{
    CliClipboardFiles *const files = context;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((files->count == CLI_CLIPBOARD_FILES) || (files->used + size > sizeof(files->text)))
    {
        files->whole = 0;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = files->text + files->used, .src = path, .bytes = size});
    files->path[files->count] = files->text + files->used;
    files->count += 1u;
    files->used += size;
}

// the files the clipboard holds read into s_files, under the lock: 1 where they were cut, by what the clipboard says
// or, where it does not say, by whether this window cut the same files
static int clipboard_files_read(void)
{
    s_files.used = 0ull;
    s_files.count = 0u;
    s_files.whole = 1;
    int cut = -1;
    (void)cli_clipboard_files_read(clipboard_file_kept, &s_files, &cut);
    s_files.path[s_files.count] = NULL;
    if (cut < 0)
    {
        cut = (s_files.count > 0u) && (s_cut_count == s_files.count) &&
              (s_cut_digest == clipboard_digest(s_files.path, s_files.count));
    }
    return cut;
}

// files refused where the tree is on another machine, the system's clipboard of files being this machine's: 1, or 0
// with why given
static int clipboard_here(VerbaLinea *err_msg)
{
    CliLinkAddress address;
    if (!cli_link_address(&address))
    {
        return 1;
    }
    mmgr_line_put(err_msg, "files are cut, copied and pasted on this machine, and the tree is on ");
    cli_link_machine(&address, err_msg);
    return 0;
}

// the text on the clipboard, empty where it holds none or cannot be read
static int clipboard_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char piece[CLI_CLIPBOARD_PIECE];
    CliJsonQuoting quoting;
    VerbaLinea text;
    cli_json_quoting_begun(&quoting, out, &text, piece, sizeof(piece));
    (void)cli_clipboard_text_read(&text);
    cli_json_quoting_ended(&quoting, &text);
    return 1;
}

// the text `text` put on the clipboard
static int clipboard_write(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    if (text == NULL)
    {
        mmgr_line_put(err_msg, "text: the clipboard is given text");
        return 0;
    }
    if (!cli_clipboard_text_written(text, length))
    {
        mmgr_line_put(err_msg, "the clipboard did not take the text");
        return 0;
    }
    return 1;
}

// how many files and folders the clipboard holds
static int clipboard_files(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    cli_lock_take(&s_lock);
    (void)clipboard_files_read();
    mmgr_line_decimal(out, s_files.count, 1u);
    cli_lock_give(&s_lock);
    return 1;
}

// the files and folders `paths` of the tree put on the clipboard, to be moved where they are pasted where `cut` and
// copied otherwise
static int clipboard_files_copy(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    char root[CLI_PATH_MAX_LEN];
    int cut = 0;
    unsigned int count = 0u;
    const char **paths = NULL;
    if (!clipboard_here(err_msg) || !cli_serve_bool(args, "cut", &cut, err_msg) || !cli_serve_root(root, err_msg) ||
        !cli_serve_strings_taken(args, "paths", CLI_CLIPBOARD_FILES, &paths, &count, err_msg))
    {
        cli_serve_strings_given(paths);
        return 0;
    }
    cli_lock_take(&s_lock);
    // the paths made whole in the room the clipboard's files are read into, which this call holds
    s_files.used = 0ull;
    s_files.count = 0u;
    int whole = 1;
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        char full[CLI_PATH_MAX_LEN];
        whole = cli_inside(root, paths[at], full, err_msg);
        s_files.whole = 1;
        if (whole)
        {
            clipboard_file_kept(&s_files, full);
            whole = s_files.whole;
        }
    }
    s_files.path[s_files.count] = NULL;
    const int put = whole && cli_clipboard_files_put(s_files.path, s_files.count, cut);
    if (put)
    {
        s_cut_digest = cut ? clipboard_digest(s_files.path, s_files.count) : 0ull;
        s_cut_count = cut ? s_files.count : 0u;
    }
    else if (whole)
    {
        mmgr_line_put(err_msg, "the clipboard did not take the files");
    }
    cli_lock_give(&s_lock);
    cli_serve_strings_given(paths);
    return put;
}

// the files and folders the clipboard holds pasted into the folder `into` of the tree, moved where they were cut, the
// clipboard emptied after a move as the file manager empties it: where each went
static int clipboard_files_paste(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const into = cli_serve_string(args, "into", err_msg);
    if ((into == NULL) || !clipboard_here(err_msg) || !cli_serve_root(root, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    const int cut = clipboard_files_read();
    int pasted = 1;
    if (s_files.count == 0u)
    {
        mmgr_line_put(out, "[]");
    }
    else
    {
        pasted = cli_files_paste(root, into, s_files.path, s_files.count, cut, out, err_msg);
    }
    if (pasted && cut && (s_files.count > 0u))
    {
        cli_clipboard_emptied();
        s_cut_digest = 0ull;
        s_cut_count = 0u;
    }
    cli_lock_give(&s_lock);
    return pasted;
}

const CliServeCall g_cli_window_clipboard_calls[] = {
    {"clip_read", clipboard_read},        {"clip_write", clipboard_write},        {"clip_files", clipboard_files},
    {"files_copy", clipboard_files_copy}, {"files_paste", clipboard_files_paste},
};
const unsigned int g_cli_window_clipboard_count =
    sizeof(g_cli_window_clipboard_calls) / sizeof(g_cli_window_clipboard_calls[0]);
