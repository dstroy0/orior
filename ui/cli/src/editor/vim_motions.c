// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim_motions.c: the text as Vim reads it through the view, a letter a unit, and where each motion goes from a place:
// whether it takes the letter it ends on and whether it takes whole lines. A line's units are read through the view one
// line at a time and stand until the next line is read, so no two lines' units are held at once
#include "vim_internal.h"

#include "../text/text.h"

unsigned long long cli_vim_lines(const CliVimCtx *args)
{
    return EMBED_CALL(cli_view_vim.lines, CliViewVimCfg, .key = args->key);
}

const unsigned short *cli_vim_line(const CliVimCtx *args)
{
    unsigned long long count = 0ULL;
    const unsigned short *const units =
        EMBED_CALL(cli_view_vim.line, CliViewVimCfg, .key = args->key, .line = args->line, .count = &count);
    *args->length_out = count;
    return units;
}

void cli_vim_line_end(const CliVimCtx *args)
{
    EMBED_CALL(cli_view_vim.line_end, CliViewVimCfg, .units = args->units);
}

// the units the line `line` takes
EMBED_INLINE unsigned long long motions_length(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->line, .length_out = &length);
    EMBED_CALL(cli_vim_motions_internal.line_end, CliVimCtx, .units = units);
    return length;
}

unsigned int cli_vim_letter_at(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->place.line, .length_out = &length);
    const unsigned int letter =
        ((units != NULL) && (args->place.col < length)) ? units[args->place.col] : CLI_VIM_LINE_END;
    EMBED_CALL(cli_vim_motions_internal.line_end, CliVimCtx, .units = units);
    return letter;
}

int cli_vim_forward(const CliVimCtx *args)
{
    if (args->place.col < EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = args->place.line))
    {
        *args->place_out = (CliPosition){args->place.line, args->place.col + 1ULL};
        return 1;
    }
    if (args->place.line + 1ULL < EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key))
    {
        *args->place_out = (CliPosition){args->place.line + 1ULL, 0ULL};
        return 1;
    }
    return 0;
}

int cli_vim_backward(const CliVimCtx *args)
{
    if (args->place.col > 0ULL)
    {
        *args->place_out = (CliPosition){args->place.line, args->place.col - 1ULL};
        return 1;
    }
    if (args->place.line > 0ULL)
    {
        const unsigned long long line = args->place.line - 1ULL;
        *args->place_out = (CliPosition){line, EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = line)};
        return 1;
    }
    return 0;
}

unsigned int cli_vim_type(const CliVimCtx *args)
{
    if ((args->letter == CLI_VIM_LINE_END) || EMBED_CALL(cli_text_table.white, CliTextCfg, .letter = args->letter))
    {
        return 0u;
    }
    return (args->big || (args->letter == '_') ||
            EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = args->letter))
               ? 1u
               : 2u;
}

unsigned long long cli_vim_first_letter(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->line, .length_out = &length);
    unsigned long long first = (length > 0ULL) ? length - 1ULL : 0ULL;
    int found = 0;
    for (unsigned long long col = 0ULL; !found && (units != NULL) && (col < length); col += 1ULL)
    {
        found = !EMBED_CALL(cli_text_table.white, CliTextCfg, .letter = units[col]);
        first = found ? col : first;
    }
    EMBED_CALL(cli_vim_motions_internal.line_end, CliVimCtx, .units = units);
    return first;
}

unsigned long long cli_vim_last_col(const CliVimCtx *args)
{
    const unsigned long long length = EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = args->line);
    return (length > 0ULL) ? length - 1ULL : 0ULL;
}

CliPosition cli_vim_head(const CliVimCtx *args)
{
    CliVimView view;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_vim_state.view,
                            .offset = (unsigned long long)(args->at) * sizeof(CliVimView),
                            .dst = &view,
                            .bytes = sizeof(CliVimView)});
    if ((view.mode == CLI_VIM_VISUAL) || (view.mode == CLI_VIM_LINE))
    {
        return view.visual_head;
    }
    CliPosition cursor = {0ULL, 0ULL};
    CliPosition anchor = {0ULL, 0ULL};
    (void)EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = args->key, .cursor = &cursor,
                     .anchor = &anchor);
    return cursor;
}

void cli_vim_put(const CliVimCtx *args)
{
    CliVimView view;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_vim_state.view,
                            .offset = (unsigned long long)(args->at) * sizeof(CliVimView),
                            .dst = &view,
                            .bytes = sizeof(CliVimView)});
    const unsigned long long lines = EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
    const unsigned long long last = (lines > 0ULL) ? lines - 1ULL : 0ULL;
    const unsigned long long line = (args->place.line < last) ? args->place.line : last;
    const unsigned long long length = EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = line);
    const unsigned long long most = (view.mode == CLI_VIM_INSERT) ? length : ((length > 0ULL) ? length - 1ULL : 0ULL);
    const CliPosition place = {line, (args->place.col < most) ? args->place.col : most};
    if (!args->goal_stays)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = g_cli_vim_state.view,
            .offset = ((unsigned long long)(args->at) * sizeof(CliVimView)) + offsetof(CliVimView, goal_set),
            .src = &(int){0},
            .bytes = sizeof(int)});
    }
    EMBED_CALL(cli_view_vim.place, CliViewVimCfg, .key = args->key, .cursor = place, .anchor = place);
}

// the place a word starts after `place`, a `big` word with every letter but a blank; an empty line stops it
EMBED_INLINE CliPosition motions_word_start(const CliVimCtx *args)
{
    CliPosition at = args->place;
    const unsigned int start =
        EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                   .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key, .place = at));
    while ((start != 0u) && (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                                        .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx,
                                                             .key = args->key, .place = at)) == start))
    {
        if (!EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = at, .place_out = &at))
        {
            return args->place;
        }
    }
    while (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                      .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key,
                                           .place = at)) == 0u)
    {
        CliPosition next = at;
        if (!EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = at, .place_out = &next))
        {
            return at;
        }
        at = next;
        if ((at.col == 0ULL) && (EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = at.line) == 0ULL))
        {
            return at;
        }
    }
    return at;
}

// the last letter of the word that ends after `place`
EMBED_INLINE CliPosition motions_word_end(const CliVimCtx *args)
{
    CliPosition at = args->place;
    int more = EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = args->place,
                          .place_out = &at);
    while (more && (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                               .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key,
                                                    .place = at)) == 0u))
    {
        more = EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = at, .place_out = &at);
    }
    if (!more)
    {
        return args->place;
    }
    const unsigned int type =
        EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                   .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key, .place = at));
    CliPosition next = at;
    while (EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = at, .place_out = &next) &&
           (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                       .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key,
                                            .place = next)) == type))
    {
        at = next;
    }
    return at;
}

// the first letter of the word that starts before `place`, an empty line stopping it
EMBED_INLINE CliPosition motions_word_back(const CliVimCtx *args)
{
    CliPosition at = args->place;
    int more = EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key, .place = args->place,
                          .place_out = &at);
    while (more &&
           (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                       .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key,
                                            .place = at)) == 0u) &&
           !((at.col == 0ULL) && (EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = at.line) == 0ULL)))
    {
        more =
            EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key, .place = at, .place_out = &at);
    }
    if (!more)
    {
        return (CliPosition){0ULL, 0ULL};
    }
    const unsigned int type =
        EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                   .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key, .place = at));
    CliPosition back = at;
    while (
        (type != 0u) &&
        EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key, .place = at, .place_out = &back) &&
        (back.line == at.line) &&
        (EMBED_CALL(cli_vim_motions_internal.type, CliVimCtx, .big = args->big,
                    .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key,
                                         .place = back)) == type))
    {
        at = back;
    }
    return at;
}

// the letter `letter` on the line of `from`, `count` times over, after it for f and t and before it for F and T, on
// it for f and F and a letter short of it for t and T, into `found`
EMBED_INLINE void motions_to_letter(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->from.line, .length_out = &length);
    const unsigned int how = args->how;
    const int ahead = (how == 'f') || (how == 't');
    const unsigned long long short_of = ((how == 't') || (how == 'T')) ? 1ULL : 0ULL;
    unsigned long long col = args->from.col;
    for (unsigned long long time = 0ULL; time < args->count; time += 1ULL)
    {
        int seen = 0;
        if (ahead)
        {
            for (unsigned long long at = col + 1ULL + short_of; (units != NULL) && (at < length); at += 1ULL)
            {
                if (units[at] == args->letter)
                {
                    col = at - short_of;
                    seen = 1;
                    break;
                }
            }
        }
        else
        {
            for (unsigned long long at = col; (units != NULL) && (at > short_of); at -= 1ULL)
            {
                if (units[at - 1ULL - short_of] == args->letter)
                {
                    col = at - 1ULL;
                    seen = 1;
                    break;
                }
            }
        }
        if (!seen)
        {
            args->found->found = 0;
            EMBED_CALL(cli_vim_motions_internal.line_end, CliVimCtx, .units = units);
            return;
        }
    }
    EMBED_CALL(cli_vim_motions_internal.line_end, CliVimCtx, .units = units);
    args->found->found = 1;
    args->found->to = (CliPosition){args->from.line, col};
    args->found->inclusive = ahead;
}

// the bracket that pairs with the first one at or after `from` on its line, into `found`
EMBED_INLINE void motions_pair_of(const CliVimCtx *args)
{
    static const char s_brackets[] = "()[]{}";
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->from.line, .length_out = &length);
    unsigned long long col = args->from.col;
    unsigned int at_bracket = 6u;
    while ((units != NULL) && (col < length) && (at_bracket == 6u))
    {
        for (unsigned int each = 0u; each < 6u; each += 1u)
        {
            at_bracket = (units[col] == (unsigned int)(unsigned char)s_brackets[each]) ? each : at_bracket;
        }
        col += (at_bracket == 6u) ? 1ULL : 0ULL;
    }
    EMBED_CALL(cli_vim_motions_internal.line_end, CliVimCtx, .units = units);
    if (at_bracket == 6u)
    {
        return;
    }
    // a bracket of the six and the one it pairs with, a byte of the basic Latin each
    const unsigned int letter = (unsigned int)(unsigned char)s_brackets[at_bracket];
    const unsigned int pair = (unsigned int)(unsigned char)s_brackets[at_bracket ^ 1u];
    const int opens = (at_bracket & 1u) == 0u;
    unsigned long long depth = 0ULL;
    CliPosition at = {args->from.line, col};
    for (int more = 1; more;)
    {
        const unsigned int here =
            EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key, .place = at);
        if (here == letter)
        {
            depth += 1ULL;
        }
        else if ((here == pair) && (depth > 0ULL))
        {
            depth -= 1ULL;
            if (depth == 0ULL)
            {
                args->found->found = 1;
                args->found->to = at;
                args->found->inclusive = 1;
                return;
            }
        }
        more = opens ? EMBED_CALL(cli_vim_motions_internal.forward, CliVimCtx, .key = args->key, .place = at,
                                  .place_out = &at)
                     : EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key, .place = at,
                                  .place_out = &at);
    }
}

// the empty line before `from`, or after it where `back` is 0, `count` times over, the text's ends past the last
EMBED_INLINE CliPosition motions_paragraph(const CliVimCtx *args)
{
    const long long lines = (long long)EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
    const long long step = args->back ? -1LL : 1LL;
    // a line's place is far inside what a long long counts
    long long line = (long long)args->from.line;
    for (unsigned long long time = 0ULL; time < args->count; time += 1ULL)
    {
        // the line is among the text's lines, which an unsigned long long counts
        while ((line + step >= 0LL) && (line + step < lines) &&
               (EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = (unsigned long long)line) == 0ULL))
        {
            line += step;
        }
        while ((line + step >= 0LL) && (line + step < lines) &&
               (EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = (unsigned long long)(line + step)) !=
                0ULL))
        {
            line += step;
        }
        line += step;
    }
    if (line < 0LL)
    {
        return (CliPosition){0ULL, 0ULL};
    }
    if (line >= lines)
    {
        // a text has a line at the least, so its last is not negative
        const unsigned long long last = (lines > 0LL) ? (unsigned long long)(lines - 1LL) : 0ULL;
        return (CliPosition){last, EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = last)};
    }
    // the line is among the text's lines here
    return (CliPosition){(unsigned long long)line, 0ULL};
}

// the next place the last search finds from `from`, back where `back`: 1 with it into `place_out`, or 0
EMBED_INLINE int motions_searched(const CliVimCtx *args)
{
    CliVimView view;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_vim_state.view,
                            .offset = (unsigned long long)(args->at) * sizeof(CliVimView),
                            .dst = &view,
                            .bytes = sizeof(CliVimView)});
    // the search's text copied out of the cell for the find
    unsigned long long length = 0ULL;
    char *const text = EMBED_CALL(
        cli_vim_internal.part_alloc, CliVimCtx, .length_out = &length,
        .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_SEARCH));
    const int found =
        (text != NULL) && (length != 0ULL) &&
        EMBED_CALL(cli_view_vim.find, CliViewVimCfg, .key = args->key, .text = text, .length = length,
                   .from = args->from, .back = args->back, .word_text = view.searched_word, .place = args->place_out);
    g_cli_guard->persistent_buf_release(text, g_cli_vim_owner);
    return found;
}

void cli_vim_motion(const CliVimCtx *args)
{
    CliVimView view_copy;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_vim_state.view,
                            .offset = (unsigned long long)(args->at) * sizeof(CliVimView),
                            .dst = &view_copy,
                            .bytes = sizeof(CliVimView)});
    const CliVimView *const view = &view_copy;
    CliVimFound *const found = args->found;
    *found = (CliVimFound){0, args->from, 0, 0, 0, 0, args->from};
    const unsigned long long times = args->counted ? args->count : 1ULL;
    const CliPosition from = args->from;
    const unsigned long long lines = EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
    const unsigned long long last_line = (lines > 0ULL) ? lines - 1ULL : 0ULL;
    const unsigned long long length = EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = from.line);
    const unsigned int name = args->name;
    found->found = 1;
    if (name == 'h')
    {
        found->to = (CliPosition){from.line, (from.col > times) ? from.col - times : 0ULL};
    }
    else if (name == 'l')
    {
        found->to = (CliPosition){from.line, (from.col + times < length) ? from.col + times : length};
    }
    else if ((name == 'j') || (name == 'k') || (name == CLI_VIM_DOWN) || (name == CLI_VIM_UP))
    {
        const unsigned int rows = EMBED_CALL(cli_view_vim.rows, CliViewVimCfg, .key = args->key);
        const unsigned long long half = ((rows / 2u) > 1u) ? (unsigned long long)(rows / 2u) : 1ULL;
        const unsigned long long by = ((name == CLI_VIM_DOWN) || (name == CLI_VIM_UP)) ? half * times : times;
        const int down = (name == 'j') || (name == CLI_VIM_DOWN);
        unsigned long long line = from.line;
        if (down)
        {
            line = ((last_line - from.line) > by) ? from.line + by : last_line;
        }
        else
        {
            line = (from.line > by) ? from.line - by : 0ULL;
        }
        if (!view->goal_set)
        {
            view_copy.goal = from.col;
            view_copy.goal_set = 1;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = g_cli_vim_state.view,
                .offset = ((unsigned long long)args->at * sizeof(CliVimView)) + offsetof(CliVimView, goal),
                .src = &view_copy.goal,
                .bytes = sizeof(view_copy.goal)});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = g_cli_vim_state.view,
                .offset = ((unsigned long long)(args->at) * sizeof(CliVimView)) + offsetof(CliVimView, goal_set),
                .src = &(int){1},
                .bytes = sizeof(int)});
        }
        found->to = (CliPosition){line, view->goal};
        found->linewise = 1;
        found->goal_stays = 1;
    }
    else if ((name == 'w') || (name == 'W'))
    {
        CliPosition at = from;
        for (unsigned long long time = 0ULL; time < times; time += 1ULL)
        {
            at = EMBED_CALL(motions_word_start, CliVimCtx, .key = args->key, .place = at, .big = name == 'W');
        }
        found->to = at;
    }
    else if ((name == 'b') || (name == 'B'))
    {
        CliPosition at = from;
        for (unsigned long long time = 0ULL; time < times; time += 1ULL)
        {
            at = EMBED_CALL(motions_word_back, CliVimCtx, .key = args->key, .place = at, .big = name == 'B');
        }
        found->to = at;
    }
    else if ((name == 'e') || (name == 'E'))
    {
        CliPosition at = from;
        for (unsigned long long time = 0ULL; time < times; time += 1ULL)
        {
            at = EMBED_CALL(motions_word_end, CliVimCtx, .key = args->key, .place = at, .big = name == 'E');
        }
        found->to = at;
        found->inclusive = 1;
    }
    else if (name == '0')
    {
        found->to = (CliPosition){from.line, 0ULL};
    }
    else if (name == '^')
    {
        found->to = (CliPosition){from.line, EMBED_CALL(cli_vim_motions_internal.first_letter, CliVimCtx,
                                                        .key = args->key, .line = from.line)};
    }
    else if (name == '$')
    {
        const unsigned long long line =
            ((last_line - from.line) > (times - 1ULL)) ? from.line + times - 1ULL : last_line;
        const unsigned long long end = EMBED_CALL(motions_length, CliVimCtx, .key = args->key, .line = line);
        found->to = (CliPosition){line, (end > 0ULL) ? end - 1ULL : 0ULL};
        found->inclusive = end > 0ULL;
        found->has_end = 1;
        found->end = (CliPosition){line, end};
    }
    else if ((name == CLI_VIM_TOP) || (name == 'G'))
    {
        unsigned long long line = (name == CLI_VIM_TOP) ? 0ULL : last_line;
        if (args->counted)
        {
            line = (args->count - 1ULL < last_line) ? args->count - 1ULL : last_line;
        }
        found->to = (CliPosition){
            line, EMBED_CALL(cli_vim_motions_internal.first_letter, CliVimCtx, .key = args->key, .line = line)};
        found->linewise = 1;
    }
    else if ((name == 'f') || (name == 'F') || (name == 't') || (name == 'T'))
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = g_cli_vim_state.view,
            .offset = ((unsigned long long)(args->at) * sizeof(CliVimView)) + offsetof(CliVimView, found_how),
            .src = &(unsigned int){name},
            .bytes = sizeof(unsigned int)});
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = g_cli_vim_state.view,
            .offset = ((unsigned long long)(args->at) * sizeof(CliVimView)) + offsetof(CliVimView, found_letter),
            .src = &(unsigned int){args->letter},
            .bytes = sizeof(unsigned int)});
        EMBED_CALL(motions_to_letter, CliVimCtx, .key = args->key, .from = from, .how = name, .letter = args->letter,
                   .count = times, .found = found);
    }
    else if ((name == ';') || (name == ','))
    {
        unsigned int how = view->found_how;
        if (name == ',')
        {
            how = (how == 'f') ? 'F' : ((how == 'F') ? 'f' : ((how == 't') ? 'T' : ((how == 'T') ? 't' : 0u)));
        }
        found->found = 0;
        if (how != 0u)
        {
            EMBED_CALL(motions_to_letter, CliVimCtx, .key = args->key, .from = from, .how = how,
                       .letter = view->found_letter, .count = times, .found = found);
        }
    }
    else if (name == '%')
    {
        found->found = 0;
        EMBED_CALL(motions_pair_of, CliVimCtx, .key = args->key, .from = from, .found = found);
    }
    else if ((name == '{') || (name == '}'))
    {
        found->to = EMBED_CALL(motions_paragraph, CliVimCtx, .key = args->key, .from = from, .back = name == '{',
                               .count = times);
    }
    else if ((name == 'n') || (name == 'N'))
    {
        CliPosition at = from;
        int seen = view->searched;
        for (unsigned long long time = 0ULL; seen && (time < times); time += 1ULL)
        {
            seen = EMBED_CALL(motions_searched, CliVimCtx, .at = args->at, .key = args->key, .from = at,
                              .back = view->searched_back != (name == 'N'), .place_out = &at);
        }
        found->found = seen;
        found->to = at;
    }
    else
    {
        found->found = 0;
    }
}

void cli_vim_move_by(const CliVimCtx *args)
{
    CliVimFound found;
    EMBED_CALL(cli_vim_motions_internal.motion, CliVimCtx, .at = args->at, .key = args->key, .name = args->name,
               .letter = args->letter, .count = args->count, .counted = args->counted,
               .from = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key),
               .found = &found);
    if (!found.found)
    {
        return;
    }
    CliVimView view;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_vim_state.view,
                            .offset = (unsigned long long)(args->at) * sizeof(CliVimView),
                            .dst = &view,
                            .bytes = sizeof(CliVimView)});
    if ((view.mode == CLI_VIM_VISUAL) || (view.mode == CLI_VIM_LINE))
    {
        const unsigned long long last =
            EMBED_CALL(cli_vim_motions_internal.last_col, CliVimCtx, .key = args->key, .line = found.to.line);
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = g_cli_vim_state.view,
            .offset = ((unsigned long long)(args->at) * sizeof(CliVimView)) + offsetof(CliVimView, visual_head),
            .src = &(CliPosition){((CliPosition){found.to.line, (found.to.col < last) ? found.to.col : last}).line,
                                  ((CliPosition){found.to.line, (found.to.col < last) ? found.to.col : last}).col},
            .bytes = sizeof(CliPosition)});
        EMBED_CALL(cli_vim_commands_internal.chosen_drawn, CliVimCtx, .at = args->at, .key = args->key);
        if (!found.goal_stays)
        {
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = g_cli_vim_state.view,
                .offset = ((unsigned long long)(args->at) * sizeof(CliVimView)) + offsetof(CliVimView, goal_set),
                .src = &(int){0},
                .bytes = sizeof(int)});
        }
        return;
    }
    EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key, .place = found.to,
               .goal_stays = found.goal_stays);
}
