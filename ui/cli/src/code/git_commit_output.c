// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_commit_output.c: what git reported of a commit read for its line, and the lines of the commits made before one
// that failed joined with why it failed
#include "../text/text.h"
#include "git_internal.h"

void cli_git_commit_line(const CliGitLengthOutCtx *args)
{
    const char *first = NULL;
    unsigned long long first_length = 0ULL;
    const char *line = args->output;
    const char *const end = args->output + args->length;
    while (line < end)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const unsigned long long size =
            (unsigned long long)(stop - line) - (((stop > line) && (stop[-1] == '\r')) ? 1ULL : 0ULL);
        if (first == NULL)
        {
            first = line;
            first_length = size;
        }
        if ((size != 0ULL) && (line[0] == '['))
        {
            mmgr_line_put_n(args->out, line, size);
            return;
        }
        line = stop + 1;
    }
    if (first != NULL)
    {
        mmgr_line_put_n(args->out, first, first_length);
    }
}

void cli_git_failed_after(const CliGitLengthCtx *args)
{
    const unsigned long long joined_capacity = (args->out_read->at - args->from_u64) + args->length + 2ULL;
    char *const joined_cell = g_cli_guard->temporary_buf_alloc(joined_capacity, g_cli_code_owner);
    VerbaLinea joined = mmgr_line_over(joined_cell, joined_capacity);
    if (joined.out == NULL)
    {
        mmgr_line_put_n(args->err_msg, args->error, args->length);
        return;
    }
    mmgr_line_put_n(&joined, args->out_read->out + args->from_u64, args->out_read->at - args->from_u64);
    mmgr_line_ch(&joined, '\n');
    mmgr_line_put_n(&joined, args->error, args->length);
    const char *start = joined.out;
    const char *stop = joined.out + joined.at;
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &start, .to = &stop);
    // the trimmed text's length, a difference of two places in it, `stop` never before `start`
    mmgr_line_put_n(args->err_msg, start, (unsigned long long)(stop - start));
    g_cli_guard->persistent_buf_release(joined.out, g_cli_code_owner);
}
