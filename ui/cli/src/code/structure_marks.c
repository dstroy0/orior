// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// structure_marks.c: a compare's tokens marked moved, renamed or changed, its lines given their kinds, the names
// renamed found, and the compare written as the window reads it
#include "../text/json_tree.h"
#include "structure_internal.h"

// a name a rename of which was seen two ways, and so renamed nowhere
#define CLI_BECAME_TWO_WAYS 0xfffffffeu

// a token's mark: its line, where it starts and ends, and its kind
static void cli_mark(const CliSide *side, unsigned int token, unsigned int kind, unsigned int (*marks)[4],
                     unsigned int *count)
{
    marks[*count][0] = side->token[token].line;
    marks[*count][1] = side->token[token].from;
    marks[*count][2] = side->token[token].to;
    marks[*count][3] = kind;
    *count += 1u;
}

// whether a token's text and another's are in order, by their bytes, the shorter first where one starts the other
static int cli_text_order(const CliSide *one, unsigned int at, const CliSide *other, unsigned int other_at)
{
    const CliToken *const left = &one->token[at];
    const CliToken *const right = &other->token[other_at];
    const unsigned int left_length = left->end - left->start;
    const unsigned int right_length = right->end - right->start;
    const int compared = memor.cmp(&(MemoriaCfg){.src = one->text + left->start,
                                                 .other = other->text + right->start,
                                                 .bytes = (left_length < right_length) ? left_length : right_length});
    return (compared != 0) ? compared : ((left_length < right_length) ? -1 : (left_length > right_length) ? 1 : 0);
}

// A name is renamed where every place it changed in a pair it became the same other name; the renames, by what they
// were, what they became and how many places, in order
unsigned int cli_structure_renames_found(const CliCompare *compare)
{
    memor.set(&(MemoriaCfg){.dst = g_cli_structure->became, .val = 0, .bytes = sizeof(g_cli_structure->became)});
    for (unsigned int pair = 0u; pair < compare->pairs; pair += 1u)
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
    for (unsigned int pair = 0u; pair < compare->pairs; pair += 1u)
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
            int order = cli_text_order(&g_cli_structure->old, g_cli_structure->became_from[before],
                                       &g_cli_structure->old, g_cli_structure->became_from[from]);
            order = (order != 0) ? order
                                 : cli_text_order(&g_cli_structure->new, g_cli_structure->became_to[before],
                                                  &g_cli_structure->new, g_cli_structure->became_to[from]);
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
static int cli_renamed(unsigned int x, unsigned int y)
{
    const unsigned int from = g_cli_structure->old.token_name[x];
    return (g_cli_structure->became[from] != CLI_BECAME_TWO_WAYS) &&
           (g_cli_structure->became[from] == g_cli_structure->new.token_name[y]);
}

// each line's worst kind among its tokens' marks: changed over any other, else the first kind marked
static void cli_kinds(const unsigned int (*marks)[4], unsigned int count, unsigned char *worst, unsigned int lines)
{
    memor.set(&(MemoriaCfg){.dst = worst, .val = CLI_SAME, .bytes = lines});
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((marks[at][3] == CLI_CHANGED) || (worst[marks[at][0]] == CLI_SAME))
        {
            // a kind is one of five
            worst[marks[at][0]] = (unsigned char)marks[at][3];
        }
    }
}

int cli_structure_mark_order(const void *one, const void *other)
{
    const unsigned int *const left = (const unsigned int *)one;
    const unsigned int *const right = (const unsigned int *)other;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (left[at] != right[at])
        {
            return (left[at] < right[at]) ? -1 : 1;
        }
    }
    return 0;
}

// a list of numbers, or of a side's line kinds, written as JSON
static void cli_numbers_written(CliLine *out, const unsigned int *numbers, const unsigned char *kinds,
                                unsigned int count)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_line_decimal(out, (numbers != NULL) ? numbers[at] : kinds[at], 1u);
    }
    cli_line_character(out, ']');
}

static void cli_marks_written(CliLine *out, const unsigned int (*marks)[4], unsigned int count)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_numbers_written(out, marks[at], NULL, 4u);
    }
    cli_line_character(out, ']');
}

// the compare written as the window reads it
void cli_structure_written(const CliCompare *compare, unsigned int renames, CliLine *out)
{
    cli_line_text(out, "{\"rows\":[");
    for (unsigned int row = 0u; row < compare->rows; row += 1u)
    {
        cli_line_text(out, (row != 0u) ? ",[" : "[");
        const unsigned int sides[2] = {g_cli_structure->row_left[row], g_cli_structure->row_right[row]};
        for (unsigned int side = 0u; side < 2u; side += 1u)
        {
            cli_line_text(out, (side != 0u) ? "," : "");
            if (sides[side] == CLI_STRUCTURE_NONE)
            {
                cli_line_text(out, "null");
            }
            else
            {
                cli_line_decimal(out, sides[side], 1u);
            }
        }
        cli_line_character(out, ']');
    }
    cli_line_text(out, "],\"starts\":");
    cli_numbers_written(out, g_cli_structure->starts, NULL, compare->starts);
    cli_line_text(out, ",\"left\":");
    cli_numbers_written(out, NULL, g_cli_structure->left, g_cli_structure->old.lines);
    cli_line_text(out, ",\"right\":");
    cli_numbers_written(out, NULL, g_cli_structure->right, g_cli_structure->new.lines);
    cli_line_text(out, ",\"left_marks\":");
    cli_marks_written(out, (const unsigned int (*)[4])g_cli_structure->left_marks, compare->left_marks);
    cli_line_text(out, ",\"right_marks\":");
    cli_marks_written(out, (const unsigned int (*)[4])g_cli_structure->right_marks, compare->right_marks);
    cli_line_text(out, ",\"renames\":[");
    for (unsigned int at = 0u; at < renames; at += 1u)
    {
        const unsigned int from = g_cli_structure->renames[at];
        const CliToken *const was = &g_cli_structure->old.token[g_cli_structure->became_from[from]];
        const CliToken *const became = &g_cli_structure->new.token[g_cli_structure->became_to[from]];
        cli_line_text(out, (at != 0u) ? ",[" : "[");
        cli_json_quoted(out, g_cli_structure->old.text + was->start, was->end - was->start);
        cli_line_character(out, ',');
        cli_json_quoted(out, g_cli_structure->new.text + became->start, became->end - became->start);
        cli_line_character(out, ',');
        cli_line_decimal(out, g_cli_structure->became_count[from] & 0x7fffffffu, 1u);
        cli_line_character(out, ']');
    }
    cli_line_text(out, "],\"moved\":");
    cli_line_decimal(out, compare->moved, 1u);
    cli_line_text(out, ",\"reshaped\":");
    cli_line_decimal(out, compare->reshaped, 1u);
    cli_line_character(out, '}');
}

// the tokens of each moved line marked moved
void cli_structure_moved_marked(CliCompare *compare)
{
    for (unsigned int line = 0u; line < g_cli_structure->old.lines; line += 1u)
    {
        for (unsigned int token = g_cli_structure->old.line_first[line];
             (g_cli_structure->left[line] == CLI_MOVED) && (token < g_cli_structure->old.line_first[line + 1u]);
             token += 1u)
        {
            cli_mark(&g_cli_structure->old, token, CLI_MOVED, g_cli_structure->left_marks, &compare->left_marks);
        }
    }
    for (unsigned int line = 0u; line < g_cli_structure->new.lines; line += 1u)
    {
        for (unsigned int token = g_cli_structure->new.line_first[line];
             (g_cli_structure->right[line] == CLI_MOVED) && (token < g_cli_structure->new.line_first[line + 1u]);
             token += 1u)
        {
            cli_mark(&g_cli_structure->new, token, CLI_MOVED, g_cli_structure->right_marks, &compare->right_marks);
        }
    }
}

// each token of a change that is not moved marked renamed, where it and the token across from it were a name renamed,
// or changed
void cli_structure_touched_marked(CliCompare *compare)
{
    for (unsigned int each = 0u; each < compare->touched; each += 1u)
    {
        const CliHunk *const touched = &g_cli_structure->touched[each];
        const int paired = touched->out_count == touched->in_count;
        for (unsigned int place = 0u; place < touched->out_count; place += 1u)
        {
            const unsigned int x = g_cli_structure->run_out[touched->out_first + place];
            const int renamed = paired && cli_renamed(x, g_cli_structure->run_in[touched->in_first + place]);
            cli_mark(&g_cli_structure->old, x, renamed ? CLI_RENAMED : CLI_CHANGED, g_cli_structure->left_marks,
                     &compare->left_marks);
        }
        for (unsigned int place = 0u; place < touched->in_count; place += 1u)
        {
            const unsigned int y = g_cli_structure->run_in[touched->in_first + place];
            const int renamed = paired && cli_renamed(g_cli_structure->run_out[touched->out_first + place], y);
            cli_mark(&g_cli_structure->new, y, renamed ? CLI_RENAMED : CLI_CHANGED, g_cli_structure->right_marks,
                     &compare->right_marks);
        }
    }
}

// Each line of a change that is not moved: changed where a token of it changed, renamed where only names were renamed,
// and reshaped where none of its tokens changed
void cli_structure_lines_kinded(CliCompare *compare)
{
    cli_kinds((const unsigned int (*)[4])g_cli_structure->left_marks, compare->left_marks, g_cli_structure->left_kinds,
              g_cli_structure->old.lines);
    cli_kinds((const unsigned int (*)[4])g_cli_structure->right_marks, compare->right_marks,
              g_cli_structure->right_kinds, g_cli_structure->new.lines);
    for (unsigned int each = 0u; each < compare->hunks; each += 1u)
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
            compare->reshaped += (g_cli_structure->right[line] == CLI_RESHAPED) ? 1u : 0u;
        }
    }
}
