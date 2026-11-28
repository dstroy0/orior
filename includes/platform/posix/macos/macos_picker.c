// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_picker.c: the system's picker on macOS, the system's own chooser through osascript
#include "../../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../../ui/cli/src/text/text.h"

#if defined(__APPLE__)
int cli_file_pick(const CliPathCapacityCtx *args)
{
    static const char *const s_scripts[3] = {"POSIX path of (choose file)", "POSIX path of (choose folder)",
                                             "POSIX path of (choose file name)"};
    const char *const words[4] = {"osascript", "-e", s_scripts[(args->type < 3u) ? args->type : 0u], NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    args->path_out[0] = '\0';
    CliRunResult *const run_result = cli_run_result_alloc();
    // a reader who cancels ends osascript with an error
    int chosen = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                            .count = 0u, .run_result = run_result, .err_msg = &err_msg) &&
                 (run_result->code == 0LL) && (run_result->out_size > 0ULL);
    unsigned long long end = chosen ? run_result->out_size : 0ULL;
    end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = run_result->out, .at = end,
                     .set = "\n\r");
    // the system gives a folder's path with a `/` after it, which a path of the window's does not have
    end -= ((end > 1ULL) && (run_result->out[end - 1ULL] == '/')) ? 1ULL : 0ULL;
    VerbaLinea chose = {args->path_out, args->capacity_bytes, 0ULL};
    mmgr_line_put_n(&chose, run_result->out, end);
    chosen = chosen && (end > 0ULL) && (mmgr_line_finish(&chose) != 0ULL);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return chosen;
}
#endif
