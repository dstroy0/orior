// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_cli.c: orior-cli, the command line alone, for a machine with no window to open. A command that acts in the
// window says which program opens it
#include "../machine/host.h"
#include "command_line.h"
#include "command_line_internal.h"

static const char *s_given[CLI_LAUNCH_ARGS];
static CliOutcome s_outcome;

int main(int count, char **given)
{
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    char why[4096];
    CliLine said = {why, sizeof(why), 0ull};
    if (cli_host_prepared(&said) == -1ll)
    {
        static char s_line[4096u + 8u];
        CliLine told = {s_line, sizeof(s_line), 0ull};
        cli_line_text(&told, "orior: ");
        cli_line_counted(&told, why, cli_line_fits(&said) ? said.at : 0ull);
        cli_err_line(s_line, cli_line_fits(&told) ? told.at : 0ull);
    }
    // every process a command starts stands under this one, and ends with it
    (void)cli_container_held();
    const unsigned int words = cli_arguments(count, given, s_given, CLI_LAUNCH_ARGS);
    cli_command_line_run(s_given, words, &s_outcome);
    if (!s_outcome.window)
    {
        return s_outcome.code;
    }
    static char s_words[CLI_LAUNCH_TEXT];
    CliLine launch = {s_words, sizeof(s_words), 0ull};
    cli_launch_words(&s_outcome, &launch);
    (void)cli_line_closed(&launch);
    static char s_told[CLI_LAUNCH_TEXT * 2u + 64u];
    CliLine told = {s_told, sizeof(s_told), 0ull};
    cli_line_text(&told, s_words);
    cli_line_text(&told, " acts in the window, which orior opens: orior ");
    cli_line_text(&told, s_words);
    cli_err_line(s_told, cli_line_fits(&told) ? told.at : 0ull);
    return CLI_WRONG;
}
