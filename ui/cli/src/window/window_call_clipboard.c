// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_clipboard.c: the window's calls on the system's clipboard: its text, read here so that the page never
// asks the reader for leave to read it, and put here so that the page never asks for leave to write it; and files and
// folders of the tree put on it cut or copied, for another window of orior or the system's file manager to paste, and
// those it holds pasted into a folder of the tree, moved where they were cut. A cut the clipboard does not say is known
// to the window it was made in alone, by the paths it put; files are cut, copied and pasted on this machine only
#include "../command_line/callback_cfg.h"
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

typedef struct
{
    const char *const *paths;
    unsigned int count;
    VerbaLinea *err_msg;
} WindowCallClipboardCtx;
// the paths `count` of them, digested: FNV-1a over their bytes, each path ended by a 0
EMBED_INLINE unsigned long long clipboard_digest(const WindowCallClipboardCtx *args)
{
    unsigned long long digest = 0xcbf29ce484222325ull;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        for (const char *letter = args->paths[at];; letter += 1)
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

// a path the clipboard holds saved among the files read
static void clipboard_file_save(const CliEachPathCfg *args)
{
    CliClipboardFiles *const files = args->context;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((files->count == CLI_CLIPBOARD_FILES) || (files->used + size > sizeof(files->text)))
    {
        files->whole = 0;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = files->text + files->used, .src = args->path, .bytes = size});
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
    (void)EMBED_CALL(cli_each_internal.clipboard_files_read, CliEachCtx, .each = clipboard_file_save,
                     .context = &s_files, .cut_pointer = &cut);
    s_files.path[s_files.count] = NULL;
    if (cut < 0)
    {
        cut = (s_files.count > 0u) && (s_cut_count == s_files.count) &&
              (s_cut_digest ==
               EMBED_CALL(clipboard_digest, WindowCallClipboardCtx, .paths = s_files.path, .count = s_files.count));
    }
    return cut;
}

// files refused where the tree is on another machine, the system's clipboard of files being this machine's: 1, or 0
// with why given
EMBED_INLINE int clipboard_here(const WindowCallClipboardCtx *args)
{
    CliLinkAddress address;
    if (!EMBED_CALL(cli_link_text.address, CliLinkTextCfg, .address_out = &address))
    {
        return 1;
    }
    mmgr_line_put(args->err_msg, "files are cut, copied and pasted on this machine, and the tree is on ");
    EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = &address, .out = args->err_msg);
    return 0;
}

// the text on the clipboard, empty where it holds none or cannot be read
static int clipboard_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char piece[CLI_CLIPBOARD_PIECE];
    CliJsonQuoting quoting;
    VerbaLinea text;
    EMBED_CALL(cli_json_tree_out.json_quoting_begin, CliJsonTreeOutCfg, .quoting = &quoting, .out = out, .line = &text,
               .piece = piece, .piece_size = sizeof(piece));
    (void)EMBED_CALL(cli_internal_table.clipboard_text_read, CliCtx, .text_verba_linea_pointer = &text);
    EMBED_CALL(cli_json_tree_out.json_quoting_end, CliJsonTreeOutCfg, .quoting = &quoting, .line = &text);
    return 1;
}

// the text `text` put on the clipboard
static int clipboard_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    if (text == NULL)
    {
        mmgr_line_put(args->err_msg, "text: the clipboard is given text");
        return 0;
    }
    if (!EMBED_CALL(cli_size_text_internal.clipboard_text_write, CliSizeTextCtx, .text = text, .size = length))
    {
        mmgr_line_put(args->err_msg, "the clipboard did not take the text");
        return 0;
    }
    return 1;
}

// how many files and folders the clipboard holds
static int clipboard_files(const CliServeAnswerCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    (void)clipboard_files_read();
    mmgr_line_decimal(args->out, s_files.count, 1u);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return 1;
}

// the files and folders `paths` of the tree put on the clipboard, to be moved where they are pasted where `cut` and
// copied otherwise
static int clipboard_files_copy(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    int cut = 0;
    unsigned int count = 0u;
    const char **paths = NULL;
    if (!EMBED_CALL(clipboard_here, WindowCallClipboardCtx, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "cut",
                    .value_i32s = &cut, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = arguments, .key = "paths",
                    .most = CLI_CLIPBOARD_FILES, .items_pointer = &paths, .count = &count, .err_msg = err_msg))
    {
        EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = paths);
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    // the paths made whole in the room the clipboard's files are read into, which this call holds
    s_files.used = 0ull;
    s_files.count = 0u;
    int whole = 1;
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        char full[CLI_PATH_MAX_LEN];
        whole = EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = paths[at], .joined = full,
                           .err_msg = err_msg);
        s_files.whole = 1;
        if (whole)
        {
            EMBED_CALL(clipboard_file_save, CliEachPathCfg, .context = &s_files, .path = full);
            whole = s_files.whole;
        }
    }
    s_files.path[s_files.count] = NULL;
    const int put = whole && EMBED_CALL(cli_count_internal.clipboard_files_put, CliCountCtx, .paths = s_files.path,
                                        .count = s_files.count, .cut = cut);
    if (put)
    {
        s_cut_digest =
            cut ? EMBED_CALL(clipboard_digest, WindowCallClipboardCtx, .paths = s_files.path, .count = s_files.count)
                : 0ull;
        s_cut_count = cut ? s_files.count : 0u;
    }
    else if (whole)
    {
        mmgr_line_put(err_msg, "the clipboard did not take the files");
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = paths);
    return put;
}

// the files and folders the clipboard holds pasted into the folder `into` of the tree, moved where they were cut, the
// clipboard emptied after a move as the file manager empties it: where each went
static int clipboard_files_paste(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const into = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "into", .err_msg = err_msg);
    if ((into == NULL) || !EMBED_CALL(clipboard_here, WindowCallClipboardCtx, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const int cut = clipboard_files_read();
    int pasted = 1;
    if (s_files.count == 0u)
    {
        mmgr_line_put(out, "[]");
    }
    else
    {
        pasted =
            EMBED_CALL(cli_files_root_count.paste, CliFilesRootCountCfg, .root = root, .into = into,
                       .from = s_files.path, .count = s_files.count, .moving = cut, .out = out, .err_msg = err_msg);
    }
    if (pasted && cut && (s_files.count > 0u))
    {
        cli_clipboard_clear();
        s_cut_digest = 0ull;
        s_cut_count = 0u;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return pasted;
}

const CliServeCall g_cli_window_clipboard_calls[] = {
    {"clip_read", clipboard_read},        {"clip_write", clipboard_write},        {"clip_files", clipboard_files},
    {"files_copy", clipboard_files_copy}, {"files_paste", clipboard_files_paste},
};
const unsigned int g_cli_window_clipboard_count =
    sizeof(g_cli_window_clipboard_calls) / sizeof(g_cli_window_clipboard_calls[0]);
