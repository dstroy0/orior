// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_commit_output.c: what git reported of a commit read for its line, and the lines of the commits made before one
// that failed joined with why it failed
#include "git_internal.h"

void cli_git_commit_line(const char *output, unsigned long long length, VerbaLinea *out)
{
    const char *first = NULL;
    unsigned long long first_length = 0ull;
    const char *line = output;
    const char *const end = output + length;
    while (line < end)
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const unsigned long long size =
            (unsigned long long)(stop - line) - (((stop > line) && (stop[-1] == '\r')) ? 1ull : 0ull);
        if (first == NULL)
        {
            first = line;
            first_length = size;
        }
        if ((size != 0ull) && (line[0] == '['))
        {
            mmgr_line_put_n(out, line, size);
            return;
        }
        line = stop + 1;
    }
    if (first != NULL)
    {
        mmgr_line_put_n(out, first, first_length);
    }
}

void cli_git_failed_after(const VerbaLinea *out, unsigned long long from, const char *error, unsigned long long length,
                          VerbaLinea *err_msg)
{
    VerbaLinea joined;
    if (!cli_growing_line_made(&joined, (out->at - from) + length + 2ull, &g_cli_code_cells))
    {
        mmgr_line_put_n(err_msg, error, length);
        return;
    }
    mmgr_line_put_n(&joined, out->out + from, out->at - from);
    mmgr_line_ch(&joined, '\n');
    mmgr_line_put_n(&joined, error, length);
    const char *start = joined.out;
    const char *stop = joined.out + joined.at;
    cli_git_trimmed(&start, &stop);
    // the trimmed text's length, a difference of two places in it, `stop` never before `start`
    mmgr_line_put_n(err_msg, start, (unsigned long long)(stop - start));
    g_cli_cells->persistent_buf_release(joined.out, &g_cli_code_cells);
}
