// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_clone.c: `orior_cli file clone`, a repository cloned into a new folder, git's progress written over one
// line as git writes it
#include "../code/git.h"
#include "../text/text.h"
#include "callback_cfg.h"
#include "command_line_internal.h"

// the line written to the terminal
static char s_line[1u << 16u];

// What git's progress has written on the terminal's line: the letters of the line shown last, and whether git wrote
typedef struct
{
    unsigned long long displayed;
    int wrote;
} CliCloneDisplayed;

// a line of git's progress written over the one before it, ended where git ended it
static void files_clone_line(const CliEachLineEndedCfg *args)
{
    CliCloneDisplayed *const displayed = args->context;
    displayed->wrote = 1;
    const unsigned long long width =
        EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = args->line,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->line, .cap = CLI_CELLS_POOL}));
    VerbaLinea over = {s_line, sizeof(s_line), 0ull};
    mmgr_line_ch(&over, '\r');
    mmgr_line_put(&over, args->line);
    for (unsigned long long at = width; at < displayed->displayed; at += 1ull)
    {
        mmgr_line_ch(&over, ' ');
    }
    if (args->ended)
    {
        mmgr_line_ch(&over, '\n');
    }
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = s_line,
               .length = mmgr_line_ok(&over) ? over.at : 0ull);
    displayed->displayed = args->ended ? 0ull : width;
}

int cli_clone_words(const CliCommandLineWordsCountCtx *args)
{
    const char *const url = (args->count > 0u) ? args->words[0] : CLI_GIT_ORIOR;
    char parent[CLI_PATH_MAX_LEN] = "";
    if (args->count > 1u)
    {
        VerbaLinea supplied = {parent, sizeof(parent), 0ull};
        mmgr_line_put(&supplied, args->words[1]);
        (void)mmgr_line_finish(&supplied);
    }
    else
    {
        (void)EMBED_CALL(cli_path_capacity_internal.folder_current, CliPathCapacityCtx, .path_out = parent,
                         .capacity_bytes = sizeof(parent));
    }
    char target[CLI_PATH_MAX_LEN];
    static char s_err_buf[4096];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if (!EMBED_CALL(cli_git_err_msg_folder.clone_folder, CliGitErrMsgFolderCfg, .url = url, .parent = parent,
                    .folder = target, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_WRONG;
    }
    CliCloneDisplayed displayed = {0ull, 0};
    if (!EMBED_CALL(cli_git_err_msg.clone, CliGitErrMsgCfg, .url = url, .target = target,
                    .progress_line = files_clone_line, .context = &displayed, .err_msg = &err_msg))
    {
        // where git wrote why, it is on the screen already
        if (!displayed.wrote)
        {
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                       .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        }
        return CLI_NO_CODE;
    }
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "Cloned into ");
    mmgr_line_put(&line, target);
    mmgr_line_ch(&line, '.');
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = mmgr_line_ok(&line) ? line.at : 0ull);
    line.at = 0ull;
    mmgr_line_put(&line, "orior_cli --root \"");
    mmgr_line_put(&line, target);
    mmgr_line_put(&line, "\" opens it in the window.");
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = mmgr_line_ok(&line) ? line.at : 0ull);
    return 0;
}
