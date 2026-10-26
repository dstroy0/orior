// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_clone.c: `orior_cli file clone`, a repository cloned into a new folder, git's progress written over one
// line as git writes it
#include "../code/git.h"
#include "../text/text.h"
#include "command_line_internal.h"

// the line written to the terminal
static char s_line[1u << 16u];

// What git's progress has written on the terminal's line: the letters of the line shown last, and whether git wrote
typedef struct
{
    unsigned long long shown;
    int wrote;
} CliCloneShown;

// a line of git's progress written over the one before it, ended where git ended it
static void files_clone_line(void *context, const char *line, int ended)
{
    CliCloneShown *const shown = context;
    shown->wrote = 1;
    const unsigned long long width =
        cli_text_letters(line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
    VerbaLinea over = {s_line, sizeof(s_line), 0ull};
    mmgr_line_ch(&over, '\r');
    mmgr_line_put(&over, line);
    for (unsigned long long at = width; at < shown->shown; at += 1ull)
    {
        mmgr_line_ch(&over, ' ');
    }
    if (ended)
    {
        mmgr_line_ch(&over, '\n');
    }
    query_terminal_written(1, s_line, mmgr_line_ok(&over) ? over.at : 0ull);
    shown->shown = ended ? 0ull : width;
}

int cli_clone_words(const char *const *words, unsigned int count)
{
    const char *const url = (count > 0u) ? words[0] : CLI_GIT_ORIOR;
    char parent[CLI_PATH_MAX_LEN] = "";
    if (count > 1u)
    {
        VerbaLinea given = {parent, sizeof(parent), 0ull};
        mmgr_line_put(&given, words[1]);
        (void)mmgr_line_finish(&given);
    }
    else
    {
        (void)cli_folder_current(parent, sizeof(parent));
    }
    char target[CLI_PATH_MAX_LEN];
    static char s_err_buf[4096];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ull};
    if (!cli_git_clone_folder(url, parent, target, &err_msg))
    {
        cli_err_line(s_err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_WRONG;
    }
    CliCloneShown shown = {0ull, 0};
    if (!cli_git_clone(url, target, files_clone_line, &shown, &err_msg))
    {
        // where git wrote why, it is on the screen already
        if (!shown.wrote)
        {
            cli_err_line(s_err_buf, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        }
        return CLI_NO_CODE;
    }
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, "Cloned into ");
    mmgr_line_put(&line, target);
    mmgr_line_ch(&line, '.');
    cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
    line.at = 0ull;
    mmgr_line_put(&line, "orior_cli --root \"");
    mmgr_line_put(&line, target);
    mmgr_line_put(&line, "\" opens it in the window.");
    cli_out_line(s_line, mmgr_line_ok(&line) ? line.at : 0ull);
    return 0;
}
