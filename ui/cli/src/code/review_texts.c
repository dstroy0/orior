// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_texts.c: the files' texts put in the review's room, each read whole into a line first
#include "../command_line/cells.h"
#include "review_internal.h"

unsigned int cli_review_text_put(const char *text, unsigned long long length)
{
    if ((length + 1ull) > (g_cli_review.text_room - g_cli_review.used))
    {
        return 0u;
    }
    const unsigned int at = g_cli_review.used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_review.text + at, .src = text, .bytes = length});
    g_cli_review.text[at + length] = '\0';
    // the texts are below CLI_REVIEW_TEXT bytes, which an unsigned int stores
    g_cli_review.used += (unsigned int)length + 1u;
    return at;
}

int cli_review_text_added(VerbaLinea *line, int written, unsigned int *at, unsigned long long *size)
{
    // a text stored moves the texts' end
    const unsigned int was = g_cli_review.used;
    if (written && mmgr_line_ok(line))
    {
        *at = cli_review_text_put(line->out, line->at);
    }
    const int added = g_cli_review.used != was;
    *size = added ? line->at : 0ull;
    g_cli_cells->persistent_buf_release(line->out, &g_cli_code_cells);
    return added;
}

VerbaLinea cli_review_text_line(void)
{
    char *const line_cell = g_cli_cells->temporary_buf_alloc(CLI_REVIEW_TEXT_ROOM, &g_cli_code_cells);
    VerbaLinea line = mmgr_line_over(line_cell, CLI_REVIEW_TEXT_ROOM);
    return line;
}
