// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_marks.c: a compare's tokens marked moved, renamed or changed, its lines given their kinds, the names
// renamed found, and the compare written as the window reads it
#include "../text/json_tree.h"
#include "structure_internal.h"

// a name a rename of which was seen two ways, and so renamed nowhere
#define CLI_BECAME_TWO_WAYS 0xfffffffeu

typedef struct
{
    const CliSide *side;
    unsigned int token;
    unsigned int kind;
    unsigned int (*marks_callback)[4];
    unsigned int *count_pointer;
    const CliSide *one;
    unsigned int at;
    const CliSide *other;
    unsigned int other_at;
    unsigned int x;
    unsigned int y;
    const unsigned int (*marks)[4];
    unsigned int count;
    unsigned char *worst;
    unsigned int lines;
    VerbaLinea *out;
    const unsigned int *numbers;
    const unsigned char *kinds;
} StructureMarksCtx;
// a token's mark: its line, where it starts and ends, and its kind
EMBED_INLINE void cli_mark(const StructureMarksCtx *args)
{
    unsigned int *count = args->count_pointer;
    args->marks_callback[*count][0] = args->side->token[args->token].line;
    args->marks_callback[*count][1] = args->side->token[args->token].from;
    args->marks_callback[*count][2] = args->side->token[args->token].to;
    args->marks_callback[*count][3] = args->kind;
    *count += 1u;
}

// whether a token's text and another's are in order, by their bytes, the shorter first where one starts the other
EMBED_INLINE int cli_text_order(const StructureMarksCtx *args)
{
    const CliSide *other = args->other;
    const CliToken *const left = &args->one->token[args->at];
    const CliToken *const right = &other->token[args->other_at];
    const unsigned int left_length = left->end - left->start;
    const unsigned int right_length = right->end - right->start;
    const int compared = memor.cmp(&(MemoriaCfg){.src = args->one->text + left->start,
                                                 .other = other->text + right->start,
                                                 .bytes = (left_length < right_length) ? left_length : right_length});
    return (compared != 0) ? compared : ((left_length < right_length) ? -1 : (left_length > right_length) ? 1 : 0);
}

// A name is renamed where every place it changed in a pair it became the same other name; the renames, by what they
// were, what they became and how many places, in order
unsigned int cli_structure_renames_find(const CliStructureCompareRenamesCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = g_cli_structure->became, .val = 0, .bytes = sizeof(g_cli_structure->became)});
    for (unsigned int pair = 0u; pair < args->compare_read->pairs; pair += 1u)
    {
        const unsigned int from = g_cli_structure->old.token_name[g_cli_structure->pair_x[pair]];
        const unsigned int to = g_cli_structure->new.token_name[g_cli_structure->pair_y[pair]];
        if (g_cli_structure->became[from] == 0u)
        {
            g_cli_structure->became[from] = to;
            g_cli_structure->became_count[from] = 0u;
            g_cli_structure->became_from[from] = g_cli_structure->pair_x[pair];
            g_cli_structure->became_to[from] = g_cli_structure->pair_y[pair];
        }
        if (g_cli_structure->became[from] == to)
        {
            g_cli_structure->became_count[from] += 1u;
        }
        else
        {
            g_cli_structure->became[from] = CLI_BECAME_TWO_WAYS;
        }
    }
    unsigned int renames = 0u;
    for (unsigned int pair = 0u; pair < args->compare_read->pairs; pair += 1u)
    {
        const unsigned int from = g_cli_structure->old.token_name[g_cli_structure->pair_x[pair]];
        if ((g_cli_structure->became[from] == CLI_BECAME_TWO_WAYS) || (g_cli_structure->became_count[from] == 0u))
        {
            continue;
        }
        // listed once: its count's top bit marks it listed
        if ((g_cli_structure->became_count[from] & 0x80000000u) != 0u)
        {
            continue;
        }
        g_cli_structure->became_count[from] |= 0x80000000u;
        unsigned int place = renames;
        while (place > 0u)
        {
            const unsigned int before = g_cli_structure->renames[place - 1u];
            int order = EMBED_CALL(cli_text_order, StructureMarksCtx, .one = &g_cli_structure->old,
                                   .at = g_cli_structure->became_from[before], .other = &g_cli_structure->old,
                                   .other_at = g_cli_structure->became_from[from]);
            order = (order != 0) ? order
                                 : EMBED_CALL(cli_text_order, StructureMarksCtx, .one = &g_cli_structure->new,
                                              .at = g_cli_structure->became_to[before], .other = &g_cli_structure->new,
                                              .other_at = g_cli_structure->became_to[from]);
            order = (order != 0) ? order
                                 : (((g_cli_structure->became_count[before] & 0x7fffffffu) >
                                     (g_cli_structure->became_count[from] & 0x7fffffffu))
                                        ? 1
                                        : 0);
            if (order <= 0)
            {
                break;
            }
            g_cli_structure->renames[place] = before;
            place -= 1u;
        }
        g_cli_structure->renames[place] = from;
        renames += 1u;
    }
    return renames;
}

// whether `x` became `y` as a name renamed
EMBED_INLINE int cli_rename(const StructureMarksCtx *args)
{
    const unsigned int from = g_cli_structure->old.token_name[args->x];
    return (g_cli_structure->became[from] != CLI_BECAME_TWO_WAYS) &&
           (g_cli_structure->became[from] == g_cli_structure->new.token_name[args->y]);
}

// each line's worst kind among its tokens' marks: changed over any other, else the first kind marked
EMBED_INLINE void cli_kinds(const StructureMarksCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->worst, .val = CLI_SAME, .bytes = args->lines});
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if ((args->marks[at][3] == CLI_CHANGED) || (args->worst[args->marks[at][0]] == CLI_SAME))
        {
            // a kind is one of five
            args->worst[args->marks[at][0]] = (unsigned char)args->marks[at][3];
        }
    }
}

int cli_structure_mark_order(const CliStructureOneCtx *args)
{
    const unsigned int *const left = (const unsigned int *)args->one;
    const unsigned int *const right = (const unsigned int *)args->other;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (left[at] != right[at])
        {
            return (left[at] < right[at]) ? -1 : 1;
        }
    }
    return 0;
}

// the tokens of each moved line marked moved
void cli_structure_moved_mark(const CliStructureCtx *args)
{
    for (unsigned int line = 0u; line < g_cli_structure->old.lines; line += 1u)
    {
        for (unsigned int token = g_cli_structure->old.line_first[line];
             (g_cli_structure->left[line] == CLI_MOVED) && (token < g_cli_structure->old.line_first[line + 1u]);
             token += 1u)
        {
            EMBED_CALL(cli_mark, StructureMarksCtx, .side = &g_cli_structure->old, .token = token, .kind = CLI_MOVED,
                       .marks_callback = g_cli_structure->left_marks, .count_pointer = &args->compare->left_marks);
        }
    }
    for (unsigned int line = 0u; line < g_cli_structure->new.lines; line += 1u)
    {
        for (unsigned int token = g_cli_structure->new.line_first[line];
             (g_cli_structure->right[line] == CLI_MOVED) && (token < g_cli_structure->new.line_first[line + 1u]);
             token += 1u)
        {
            EMBED_CALL(cli_mark, StructureMarksCtx, .side = &g_cli_structure->new, .token = token, .kind = CLI_MOVED,
                       .marks_callback = g_cli_structure->right_marks, .count_pointer = &args->compare->right_marks);
        }
    }
}

// each token of a change that is not moved marked renamed, where it and the token across from it were a name renamed,
// or changed
void cli_structure_touched_mark(const CliStructureCtx *args)
{
    for (unsigned int each = 0u; each < args->compare->touched; each += 1u)
    {
        const CliHunk *const touched = &g_cli_structure->touched[each];
        const int paired = touched->out_count == touched->in_count;
        for (unsigned int place = 0u; place < touched->out_count; place += 1u)
        {
            const unsigned int x = g_cli_structure->run_out[touched->out_first + place];
            const int renamed = paired && EMBED_CALL(cli_rename, StructureMarksCtx, .x = x,
                                                     .y = g_cli_structure->run_in[touched->in_first + place]);
            EMBED_CALL(cli_mark, StructureMarksCtx, .side = &g_cli_structure->old, .token = x,
                       .kind = renamed ? CLI_RENAMED : CLI_CHANGED, .marks_callback = g_cli_structure->left_marks,
                       .count_pointer = &args->compare->left_marks);
        }
        for (unsigned int place = 0u; place < touched->in_count; place += 1u)
        {
            const unsigned int y = g_cli_structure->run_in[touched->in_first + place];
            const int renamed = paired && EMBED_CALL(cli_rename, StructureMarksCtx,
                                                     .x = g_cli_structure->run_out[touched->out_first + place], .y = y);
            EMBED_CALL(cli_mark, StructureMarksCtx, .side = &g_cli_structure->new, .token = y,
                       .kind = renamed ? CLI_RENAMED : CLI_CHANGED, .marks_callback = g_cli_structure->right_marks,
                       .count_pointer = &args->compare->right_marks);
        }
    }
}

// Each line of a change that is not moved: changed where a token of it changed, renamed where only names were renamed,
// and reshaped where none of its tokens changed
void cli_structure_lines_kind(const CliStructureCtx *args)
{
    EMBED_CALL(cli_kinds, StructureMarksCtx, .marks = (const unsigned int (*)[4])g_cli_structure->left_marks,
               .count = args->compare->left_marks, .worst = g_cli_structure->left_kinds,
               .lines = g_cli_structure->old.lines);
    EMBED_CALL(cli_kinds, StructureMarksCtx, .marks = (const unsigned int (*)[4])g_cli_structure->right_marks,
               .count = args->compare->right_marks, .worst = g_cli_structure->right_kinds,
               .lines = g_cli_structure->new.lines);
    for (unsigned int each = 0u; each < args->compare->hunks; each += 1u)
    {
        const CliHunk *const hunk = &g_cli_structure->hunks[each];
        for (unsigned int at = 0u; at < hunk->out_count; at += 1u)
        {
            const unsigned int line = g_cli_structure->outs[hunk->out_first + at];
            g_cli_structure->left[line] = (g_cli_structure->left_kinds[line] == CLI_SAME)
                                              ? (unsigned char)CLI_RESHAPED
                                              : g_cli_structure->left_kinds[line];
        }
        for (unsigned int at = 0u; at < hunk->in_count; at += 1u)
        {
            const unsigned int line = g_cli_structure->ins[hunk->in_first + at];
            g_cli_structure->right[line] = (g_cli_structure->right_kinds[line] == CLI_SAME)
                                               ? (unsigned char)CLI_RESHAPED
                                               : g_cli_structure->right_kinds[line];
            args->compare->reshaped += (g_cli_structure->right[line] == CLI_RESHAPED) ? 1u : 0u;
        }
    }
}
