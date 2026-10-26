// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_cli.c: orior_cli, the command line alone, for a machine with no window to open. A command that acts in the
// window specifies which program opens it
#include "../machine/host.h"
#include "command_line.h"
#include "command_line_internal.h"

static const char *s_given[CLI_LAUNCH_ARGS];
static CliOutcome s_outcome;

int main(int count, char **given)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (cli_host_prepared(&err_msg) == -1ll)
    {
        static char s_line[4096u + 8u];
        VerbaLinea msg = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&msg, "orior: ");
        mmgr_line_put_n(&msg, why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        cli_err_line(s_line, mmgr_line_ok(&msg) ? msg.at : 0ull);
    }
    // every process a command starts stands under this one, and ends with it
    (void)cli_container_made();
    const unsigned int words = cli_arguments(count, given, s_given, CLI_LAUNCH_ARGS);
    cli_command_line_run(s_given, words, &s_outcome);
    cli_cells_peaks_reported();
    if (!s_outcome.window)
    {
        return s_outcome.code;
    }
    static char s_words[CLI_LAUNCH_TEXT];
    VerbaLinea launch = {s_words, sizeof(s_words), 0ull};
    cli_launch_words(&s_outcome, &launch);
    (void)mmgr_line_finish(&launch);
    static char s_msg_buf[CLI_LAUNCH_TEXT * 2u + 64u];
    VerbaLinea msg = {s_msg_buf, sizeof(s_msg_buf), 0ull};
    mmgr_line_put(&msg, s_words);
    mmgr_line_put(&msg, " acts in the window, which orior_cli opens: orior_cli ");
    mmgr_line_put(&msg, s_words);
    cli_err_line(s_msg_buf, mmgr_line_ok(&msg) ? msg.at : 0ull);
    return CLI_WRONG;
}
