// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_commit_said.c: what git said of a commit read for its line, and the lines of the commits made before one that
// failed joined with why it failed
#include "git_internal.h"

void cli_git_commit_line(const char *said, unsigned long long length, CliLine *out)
{
    const char *first = NULL;
    unsigned long long first_length = 0ull;
    const char *line = said;
    const char *const end = said + length;
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
            cli_line_counted(out, line, size);
            return;
        }
        line = stop + 1;
    }
    if (first != NULL)
    {
        cli_line_counted(out, first, first_length);
    }
}

void cli_git_failed_after(const CliLine *out, unsigned long long from, const char *error, unsigned long long length,
                          CliLine *said)
{
    CliLine joined;
    if (!cli_line_growing(&joined, (out->at - from) + length + 2ull, "the lines of a commit that failed"))
    {
        cli_line_counted(said, error, length);
        return;
    }
    cli_line_counted(&joined, out->out + from, out->at - from);
    cli_line_character(&joined, '\n');
    cli_line_counted(&joined, error, length);
    const char *start = joined.out;
    const char *stop = joined.out + joined.at;
    cli_git_trimmed(&start, &stop);
    // the trimmed text's length, a difference of two places in it, `stop` never before `start`
    cli_line_counted(said, start, (unsigned long long)(stop - start));
    cli_cell_given(joined.out);
}
