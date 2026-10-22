// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_clone.c: `orior_cli file clone`, a repository cloned into a new folder, git's progress written over one line
// as git writes it
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
    CliLine over = {s_line, sizeof(s_line), 0ull};
    cli_line_character(&over, '\r');
    cli_line_text(&over, line);
    for (unsigned long long at = width; at < shown->shown; at += 1ull)
    {
        cli_line_character(&over, ' ');
    }
    if (ended)
    {
        cli_line_character(&over, '\n');
    }
    query_terminal_written(1, s_line, cli_line_fits(&over) ? over.at : 0ull);
    shown->shown = ended ? 0ull : width;
}

int cli_clone_words(const char *const *words, unsigned int count)
{
    const char *const url = (count > 0u) ? words[0] : CLI_GIT_ORIOR;
    char parent[CLI_PATH_LONGEST] = "";
    if (count > 1u)
    {
        CliLine given = {parent, sizeof(parent), 0ull};
        cli_line_text(&given, words[1]);
        (void)cli_line_closed(&given);
    }
    else
    {
        (void)cli_folder_current(parent, sizeof(parent));
    }
    char target[CLI_PATH_LONGEST];
    static char s_said[4096];
    CliLine said = {s_said, sizeof(s_said), 0ull};
    if (!cli_git_clone_folder(url, parent, target, &said))
    {
        cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_WRONG;
    }
    CliCloneShown shown = {0ull, 0};
    if (!cli_git_clone(url, target, files_clone_line, &shown, &said))
    {
        // where git wrote why, it is on the screen already
        if (!shown.wrote)
        {
            cli_err_line(s_said, cli_line_fits(&said) ? said.at : 0ull);
        }
        return CLI_NO_CODE;
    }
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, "Cloned into ");
    cli_line_text(&line, target);
    cli_line_character(&line, '.');
    cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    line.at = 0ull;
    cli_line_text(&line, "orior_cli --root \"");
    cli_line_text(&line, target);
    cli_line_text(&line, "\" opens it in the window.");
    cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    return 0;
}
