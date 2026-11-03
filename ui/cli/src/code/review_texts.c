// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_texts.c: the files' texts put in the review's capacity, each read whole into a line first
#include "../command_line/cells.h"
#include "review_internal.h"

unsigned int cli_review_text_append(const CliReviewTextLengthCtx *args)
{
    if ((args->length + 1ULL) > (g_cli_review.text_capacity - g_cli_review.used))
    {
        return 0u;
    }
    const unsigned int at = g_cli_review.used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_review.text + at, .src = args->text, .bytes = args->length});
    g_cli_review.text[at + args->length] = '\0';
    // the texts are below CLI_REVIEW_TEXT bytes, which an unsigned int stores
    g_cli_review.used += (unsigned int)args->length + 1u;
    return at;
}

int cli_review_text_add(const CliReviewLineCtx *args)
{
    unsigned int *at = args->at;
    unsigned long long *size = args->size;
    // a text stored moves the texts' end
    const unsigned int was = g_cli_review.used;
    if (args->written && mmgr_line_ok(args->line))
    {
        *at = EMBED_CALL(cli_review_text_length_internal.text_append, CliReviewTextLengthCtx, .text = args->line->out,
                         .length = args->line->at);
    }
    const int added = g_cli_review.used != was;
    *size = added ? args->line->at : 0ULL;
    g_cli_cells->persistent_buf_release(args->line->out, &g_cli_code_cells);
    return added;
}

VerbaLinea cli_review_text_line(void)
{
    char *const line_cell = g_cli_cells->temporary_buf_alloc(CLI_REVIEW_TEXT_CAPACITY, &g_cli_code_cells);
    VerbaLinea line = mmgr_line_over(line_cell, CLI_REVIEW_TEXT_CAPACITY);
    return line;
}
