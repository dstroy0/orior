// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// impero.c: impero, the command line alone, for a machine with no window to open. A command that acts in the
// window specifies which program opens it
#include "../machine/host.h"
#include "command_line.h"
#include "command_line_internal.h"

static const char *s_input[CLI_LAUNCH_ARGS];
static CliOutcome s_outcome;

int main(int count, char **supplied)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_command_line_catalog_alloc();
    // inside a Flatpak's sandbox the machine's own programs run on the machine
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_host_into.prepare, CliHostIntoCfg, .err_msg = &err_msg) == -1LL)
    {
        static char s_line[4096u + 8u];
        VerbaLinea msg = {s_line, sizeof(s_line), 0ULL};
        mmgr_line_put(&msg, "orior: ");
        mmgr_line_put_n(&msg, why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size = mmgr_line_ok(&msg) ? msg.at : 0ULL);
    }
    // every process a command starts stands under this one, and ends with it
    (void)cli_container_init();
    const unsigned int words =
        EMBED_CALL(cli_capacity_count_internal.arguments, CliCapacityCountCtx, .count_i32 = count, .supplied = supplied,
                   .words_read = s_input, .capacity_u32 = CLI_LAUNCH_ARGS);
    // the one command this process runs is one lane's: its own cells are the lane's and are rewound as it ends
    EMBED_CALL(cli_command_line_input.run, CliCommandLineInputCfg, .supplied = s_input, .count = words,
               .outcome = &s_outcome);
    cli_pool_peaks_report();
    if (!s_outcome.window)
    {
        return s_outcome.code;
    }
    static char s_words[CLI_LAUNCH_TEXT];
    VerbaLinea launch = {s_words, sizeof(s_words), 0ULL};
    EMBED_CALL(cli_command_line_launch_out.launch_words, CliCommandLineLaunchOutCfg, .launch = &s_outcome,
               .out = &launch);
    (void)mmgr_line_finish(&launch);
    static char s_msg_buf[CLI_LAUNCH_TEXT * 2u + 64u];
    VerbaLinea msg = {s_msg_buf, sizeof(s_msg_buf), 0ULL};
    mmgr_line_put(&msg, s_words);
    mmgr_line_put(&msg, " acts in the window, which impero opens: imp ");
    mmgr_line_put(&msg, s_words);
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_msg_buf,
               .size = mmgr_line_ok(&msg) ? msg.at : 0ULL);
    return CLI_WRONG;
}
