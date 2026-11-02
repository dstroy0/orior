// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review.c: a change read file by file, its texts before and now, its pieces grouped by the calls between them, and
// the review written as the window reads it
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "git.h"
#include "review_internal.h"

CliReview g_cli_review;

// The lists git gives a review, and the work state the pieces are grouped in
typedef struct
{
    CliRepositories repositories;
    CliTouchedList touched;
    CliChangedList changed;
    unsigned int caller_first[CLI_REVIEW_PIECES];
    unsigned int caller_count[CLI_REVIEW_PIECES];
    unsigned int caller[CLI_REVIEW_CALLS];
    unsigned char reached[CLI_REVIEW_PIECES];
    unsigned char visited[CLI_REVIEW_PIECES];
    unsigned char positioned[CLI_REVIEW_PIECES];
    unsigned int queue[CLI_REVIEW_PIECES];
    unsigned int members[CLI_REVIEW_PIECES];
    unsigned int stack[CLI_REVIEW_CALLS];
} CliReviewWorkState;

static CliReviewWorkState *s_work_state;

typedef struct
{
    const char *at;
    const char *path;
    const char *was_path;
    const char *root;
    const char *id;
    VerbaLinea *by;
    VerbaLinea *subject;
    int *made;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    unsigned int at_u32;
    const char *by_text;
    unsigned long long by_size;
    const char *subject_text;
    unsigned long long subject_size;
} ReviewCtx;
