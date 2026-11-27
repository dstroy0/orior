// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_clipboard.c: the system's clipboard on macOS, the system's pasteboard: its text through pbpaste and pbcopy,
// and its files through osascript's bridge to AppKit
#if defined(__APPLE__)
// the descriptor flags are POSIX, outside strict C11
#define _DARWIN_C_SOURCE
#endif
#include "../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../ui/cli/src/text/text.h"
#include "../posix_systems.h"

#if defined(__APPLE__)
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct
{
    unsigned char character;
    VerbaLinea *out;
    const char *path;
    char digit;
    const char *list;
    unsigned long long size;
    void (*each)(const CliEachPathCfg *args);
    void *context;
    const char *const *words;
    const char *input;
} PosixClipboardCtx;

// the scripts osascript runs in JavaScript, with its bridge to AppKit, which reaches the system's pasteboard. The
// files on it, one path a line. The files read one path a line from the input put on it; and the pasteboard emptied
static const char s_files_held_script[] =
    "ObjC.import('AppKit'); const urls = $.NSPasteboard.generalPasteboard.readObjectsForClassesOptions("
    "$([$.NSURL]), $({NSPasteboardURLReadingFileURLsOnlyKey: true})); const paths = []; "
    "for (let at = 0; urls && at < urls.count; at += 1) paths.push(urls.objectAtIndex(at).path.js); paths.join('\\n');";
static const char s_files_append_script[] =
    "ObjC.import('AppKit'); const input = $.NSFileHandle.fileHandleWithStandardInput.readDataToEndOfFile; "
    "const text = $.NSString.alloc.initWithDataEncoding(input, $.NSUTF8StringEncoding).js; "
    "const urls = text.split('\\n').filter((path) => path.length).map((path) => $.NSURL.fileURLWithPath(path)); "
    "const board = $.NSPasteboard.generalPasteboard; board.clearContents; board.writeObjects($(urls)); '';";
static const char s_emptied_script[] = "ObjC.import('AppKit'); $.NSPasteboard.generalPasteboard.clearContents; '';";

// the encoding pbcopy and pbpaste read and write the text in
static const PlatformSetting s_utf8[1] = {{"LANG", "en_US.UTF-8"}};

// `words` run, with `size` bytes of `input` fed to it, and what it wrote passed to `out` where `out` is not NULL. 1,
// or 0 where it did not run or ended with a code other than 0
EMBED_INLINE int clipboard_run(const PosixClipboardCtx *args)
{
    const char *const *words = args->words;
    const char *input = args->input;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    const int done =
        EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = NULL, .settings = s_utf8,
                   .count = 1u, .input = input, .length = args->size, .run_result = run_result, .err_msg = &err_msg) &&
        (run_result->code == 0LL);
    if (done && (args->out != NULL))
    {
        mmgr_line_put_n(args->out, run_result->out, run_result->out_size);
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return done;
}

int cli_clipboard_text_read(const CliCtx *args)
{
    static const char *const s_words[2] = {"pbpaste", NULL};
    return EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = "", .size = 0ULL,
                      .out = args->text_verba_linea_pointer);
}

int cli_clipboard_text_write(const CliSizeTextCtx *args)
{
    static const char *const s_words[2] = {"pbcopy", NULL};
    return EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = args->text, .size = args->size,
                      .out = NULL);
}

int cli_clipboard_files_append(const CliCountCtx *args)
{
    // the window alone knows a cut on macOS, whose pasteboard says no cut of files
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_files_append_script, NULL};
    const unsigned long long input_capacity = 4096u;
    char *const input_cell = g_platform_guard->temporary_buf_alloc(input_capacity, g_cli_platform_owner);
    VerbaLinea input = mmgr_line_over(input_cell, input_capacity);
    if (input.out == NULL)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&input, args->paths[at]);
        mmgr_line_ch(&input, '\n');
    }
    const int put = mmgr_line_ok(&input) && EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words,
                                                       .input = input.out, .size = input.at, .out = NULL);
    g_platform_guard->persistent_buf_release(input.out, g_cli_platform_owner);
    return put;
}

// a path osascript wrote, one to a line, passed on
typedef struct
{
    void (*each)(const CliEachPathCfg *args);
    void *context;
} CliClipboardEach;

unsigned int cli_clipboard_files_read(const CliEachCtx *args)
{
    int *cut = args->cut_pointer;
    *cut = -1;
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_files_held_script, NULL};
    const unsigned long long paths_capacity = 4096u;
    char *const paths_cell = g_platform_guard->temporary_buf_alloc(paths_capacity, g_cli_platform_owner);
    VerbaLinea paths = mmgr_line_over(paths_cell, paths_capacity);
    if (paths.out == NULL)
    {
        return 0u;
    }
    unsigned int count = 0u;
    if (EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = "", .size = 0ULL, .out = &paths) &&
        mmgr_line_ok(&paths))
    {
        for (unsigned long long at = 0ULL; at < paths.at;)
        {
            const unsigned long long from = at;
            const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                      .text = paths.out, .at = from, .size = paths.at, .set = "\n");
            at = end + 1ULL;
            if (end <= from)
            {
                continue;
            }
            char path[CLI_PATH_MAX_LEN];
            VerbaLinea line = {path, sizeof(path), 0ULL};
            mmgr_line_put_n(&line, paths.out + from, end - from);
            if (mmgr_line_finish(&line) != 0ULL)
            {
                EMBED_CALL(args->each, CliEachPathCfg, .context = args->context, .path = path);
                count += 1u;
            }
        }
    }
    g_platform_guard->persistent_buf_release(paths.out, g_cli_platform_owner);
    return count;
}

void cli_clipboard_clear(void)
{
    static const char *const s_words[6] = {"osascript", "-l", "JavaScript", "-e", s_emptied_script, NULL};
    (void)EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words, .input = "", .size = 0ULL, .out = NULL);
}

void cli_clipboard_units_released(void)
{
    // the pasteboard holds its own copy of a text that pbcopy put, and serves nothing from the pages
}

int cli_clipboard_units_put(const CliClipboardUnitsCtx *args)
{
    (void)args;
    // pbcopy takes the UTF-8 text from the pages through its input
    static const char *const s_words[2] = {"pbcopy", NULL};
    const int put = cli_clipboard_units_text_made() && EMBED_CALL(clipboard_run, PosixClipboardCtx, .words = s_words,
                                                    .input = g_cli_clipboard_units_text, .size = g_cli_clipboard_units_text_size, .out = NULL);
    cli_clipboard_units_pages_free();
    return put;
}
#endif
