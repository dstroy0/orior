// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_texts.c: the review's rooms of texts, each a cell that doubles as texts are put, and the files' texts put in
// theirs, each read whole into a growing line first
#include "../command_line/cells.h"
#include "review_internal.h"

int cli_review_room(char **room, unsigned long long *size, unsigned long long used, unsigned long long more,
                    unsigned long long most, const char *what)
{
    const unsigned long long needed = used + more;
    if (needed <= *size)
    {
        return 1;
    }
    if (needed > most)
    {
        return 0;
    }
    unsigned long long grown = (*size == 0ull) ? CLI_REVIEW_TEXT_FIRST : *size;
    while (grown < needed)
    {
        grown *= 2ull;
    }
    grown = (grown < most) ? grown : most;
    char *const moved = g_cli_cells->persistent_buf_alloc(grown, &g_cli_code_cells);
    if (moved == NULL)
    {
        return 0;
    }
    if (*room != NULL)
    {
        // the bytes taken are below the room's size, which a size_t stores
        memor.cpy(&(MemoriaCfg){.dst = moved, .src = *room, .bytes = (size_t)used});
    }
    g_cli_cells->persistent_buf_release(*room, &g_cli_code_cells);
    *room = moved;
    *size = grown;
    return 1;
}

unsigned int cli_review_text_put(const char *text, unsigned long long length)
{
    if (!cli_review_room(&g_cli_review.text, &g_cli_review.text_room, g_cli_review.used, length + 1ull, CLI_REVIEW_TEXT,
                         "a review's file texts"))
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
    // a text is stored where the texts grew by it
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
    VerbaLinea line = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_growing_line_made(&line, CLI_REVIEW_TEXT_FIRST, &g_cli_code_cells);
    return line;
}
