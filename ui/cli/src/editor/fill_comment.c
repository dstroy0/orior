// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_comment.c: a block comment of documentation written again whole. Its lines are read without their stars, the
// opening and the closing lines' text retained where they contain any; split into paragraphs at blank lines and at each
// tag; each paragraph's words filled under ` * `, and each run of tags of one kind lined up: the tags, their types in
// braces, the names of those that name one, and their descriptions each in one column, a description that runs past the
// margin going on under its own first word. A run's tags are joined from their lines into a work state sized to the
// comment's bytes it left out of the run
#include "../command_line/pool.h"
#include "../text/text.h"
#include "fill_internal.h"

// A tag of a run: the tag, its type in braces, its name, and its description
typedef struct
{
    const char *tag;
    unsigned long long tag_size;
    const char *kind;
    unsigned long long kind_size;
    const char *name;
    unsigned long long name_size;
    const char *description;
    unsigned long long description_size;
} CliFillTag;
// the bytes a tag line's head, its hanging lead and the comment's lead each occupy
#define CLI_FILL_LINE_CAPACITY (1u << 16u)

// A fill's work state, parts of one cell taken for the comment filled, sized to its lines and bytes, and given back as
// the fill ends: each part's first line among the comment's lines and its count, 0 for a blank line between parts; each
// part's tag; the tags' texts joined; and a tag line's head, its hanging lead and the comment's lead
typedef struct
{
    unsigned int *part_first;
    unsigned int *part_lines;
    unsigned int parts_max;
    CliFillTag *tag;
    char *joined;
    unsigned long long joined_max;
    char *head;
    char *hang;
    char *lead;
} CliFillWorkState;

static CliFillWorkState s_work_state;
static CliFillWorkState *s_fill;

int cli_fill_comment_lock(const CliFillCtx *args)
{
    (void)args;
    s_fill = &s_work_state;
    return 1;
}

typedef struct
{
    unsigned char **at;
    unsigned long long bytes;
    unsigned int lines;
    VerbaLinea *err_msg;
    const char *text;
    const CliFillLine *line_fill_line_pointer;
    unsigned long long *size_pointer;
    unsigned long long size;
    const CliFillLine *at_fill_line_pointer;
    CliFillLine *first;
    CliFillLine *last;
    unsigned int count;
    const char *line;
    int stored;
    const CliFillLine *first_read;
    const CliFillLine *last_read;
    unsigned long long *tag_size;
    const char **kind;
    unsigned long long *kind_size;
    CliFillTag *tag;
    VerbaLinea *out;
    unsigned long long width;
    unsigned int part;
    unsigned long long *used;
    CliFilled *filled;
    unsigned int *part_pointer;
    unsigned int parts;
    const char *lead;
    unsigned long long comment_prefix_size;
    unsigned int width_u32;
} FillCommentCtx;
