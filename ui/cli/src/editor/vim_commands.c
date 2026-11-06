// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim_commands.c: Vim's operators and commands: what each does to the text through the view's own steps, the
// registers it fills and reads, insert mode entered and left, the search prompt opened, and the searches. A text taken
// out of the view or written for an edit stands in a cell of the command's own, taken where the command runs and
// released before it ends
#include "vim_internal.h"

#include "../text/text.h"

// the register `register_name` stands at among the registers: " and none at the unnamed one, 0 at the last yank's,
// a to z and A to Z at their letter's, and CLI_VIM_REGISTERS for a letter that is no register
EMBED_INLINE unsigned int commands_register_at(const CliVimCtx *args)
{
    const unsigned int name = args->register_name;
    if ((name == 0u) || (name == '"'))
    {
        return CLI_VIM_REGISTER_UNNAMED;
    }
    if (name == '0')
    {
        return CLI_VIM_REGISTER_YANKED;
    }
    if ((name >= 'a') && (name <= 'z'))
    {
        return CLI_VIM_REGISTER_LETTERS + (name - 'a');
    }
    if ((name >= 'A') && (name <= 'Z'))
    {
        return CLI_VIM_REGISTER_LETTERS + (name - 'A');
    }
    return CLI_VIM_REGISTERS;
}

// the units the line `line` takes
EMBED_INLINE unsigned long long commands_length(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key, .line = args->line,
                     .length_out = &length);
    return length;
}

// the blanks that lead the line `line`, written into `bytes_out`, which is sized for `count` bytes: how many
EMBED_INLINE unsigned long long commands_lead(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = args->line, .length_out = &length);
    unsigned long long lead = 0ULL;
    while ((units != NULL) && (lead < length) && (lead < args->count) &&
           ((units[lead] == ' ') || (units[lead] == '\t')))
    {
        // a blank is a byte of the basic Latin
        args->bytes_out[lead] = (char)units[lead];
        lead += 1ULL;
    }
    return lead;
}

// the span from `from` to `to` replaced by `length` bytes of `text`, one of the view's steps
EMBED_INLINE void commands_edit(const CliVimCtx *args)
{
    (void)EMBED_CALL(cli_view_vim.edit, CliViewVimCfg, .key = args->key, .from = args->from, .to = args->to,
                     .text = args->text, .length = args->length, .err_msg = args->err_msg);
}

// the cursor of the view `key`
EMBED_INLINE CliPosition commands_cursor(const CliVimCtx *args)
{
    CliPosition cursor = {0ULL, 0ULL};
    CliPosition anchor = {0ULL, 0ULL};
    (void)EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = args->key, .cursor = &cursor,
                     .anchor = &anchor);
    return cursor;
}

void cli_vim_store(const CliVimCtx *args)
{
    CliVimState *const state = &g_cli_vim_state;
    const unsigned int name = args->register_name;
    const unsigned int at = EMBED_CALL(commands_register_at, CliVimCtx, .register_name = name);
    // a byte that is 0 or 1 is what whether a register takes lines carries
    const unsigned char lines = args->lines ? 1u : 0u;
    if ((name >= 'a') && (name <= 'z'))
    {
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = at, .text = args->text, .length = args->length);
        state->register_lines[at] = lines;
    }
    else if ((name >= 'A') && (name <= 'Z'))
    {
        unsigned long long before = 0ULL;
        (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = at, .length_out = &before);
        const int joined_lines = args->lines || (state->register_lines[at] != 0u);
        const unsigned long long between = ((before != 0ULL) && joined_lines) ? 1ULL : 0ULL;
        char *const joined = g_cli_guard->temporary_buf_alloc(before + between + args->length + 1ULL, g_cli_vim_owner);
        if (joined != NULL)
        {
            const char *const was =
                EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = at, .length_out = &before);
            VerbaLinea line = {joined, before + between + args->length + 1ULL, 0ULL};
            mmgr_line_put_n(&line, was, before);
            mmgr_line_put_n(&line, "\n", between);
            mmgr_line_put_n(&line, args->text, args->length);
            (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = at, .text = joined,
                             .length = mmgr_line_finish(&line));
            state->register_lines[at] = (before == 0ULL) ? lines : (joined_lines ? 1u : 0u);
        }
        g_cli_guard->persistent_buf_release(joined, g_cli_vim_owner);
    }
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = CLI_VIM_REGISTER_UNNAMED, .text = args->text,
                     .length = args->length);
    state->register_lines[CLI_VIM_REGISTER_UNNAMED] = lines;
    if (args->yanked)
    {
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = CLI_VIM_REGISTER_YANKED, .text = args->text,
                         .length = args->length);
        state->register_lines[CLI_VIM_REGISTER_YANKED] = lines;
    }
}

void cli_vim_insert_at(const CliVimCtx *args)
{
    EMBED_CALL(cli_vim_internal.mode_set, CliVimCtx, .at = args->at, .mode = CLI_VIM_INSERT);
    EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key, .place = args->place);
}

void cli_vim_insert_left(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    const CliPosition head = EMBED_CALL(commands_cursor, CliVimCtx, .key = args->key);
    EMBED_CALL(cli_vim_internal.mode_set, CliVimCtx, .at = args->at, .mode = CLI_VIM_NORMAL);
    EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
               .place = (CliPosition){head.line, (head.col > 0ULL) ? head.col - 1ULL : 0ULL});
    if (!view->taking)
    {
        return;
    }
    // the change taken becomes the last, its keys and the input typed after them
    for (unsigned int each = 0u; each < 2u; each += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const taken = EMBED_CALL(
            cli_vim_internal.part_text, CliVimCtx, .length_out = &length,
            .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at,
                               .part = (each == 0u) ? CLI_VIM_PART_TAKING_KEYS : CLI_VIM_PART_TAKING_INSERT));
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx,
                         .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at,
                                            .part = (each == 0u) ? CLI_VIM_PART_LAST_KEYS : CLI_VIM_PART_LAST_INSERT),
                         .text = taken, .length = length);
    }
    view->taking = 0;
}

void cli_vim_apply(const CliVimCtx *args)
{
    const CliVimSpan *const span = args->span_read;
    const unsigned int name = args->name;
    const unsigned long long bytes =
        EMBED_CALL(cli_view_vim.span_bytes, CliViewVimCfg, .key = args->key, .from = span->start, .to = span->end);
    char *const text = g_cli_guard->temporary_buf_alloc(bytes + 1ULL, g_cli_vim_owner);
    if (text == NULL)
    {
        return;
    }
    VerbaLinea out = {text, bytes + 1ULL, 0ULL};
    EMBED_CALL(cli_view_vim.span, CliViewVimCfg, .key = args->key, .from = span->start, .to = span->end, .out = &out);
    const unsigned long long length = mmgr_line_finish(&out);
    if (name == 'y')
    {
        EMBED_CALL(cli_vim_commands_internal.store, CliVimCtx, .register_name = args->register_name, .text = text,
                   .length = length, .lines = span->lines, .yanked = 1);
        const CliPosition head = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
                   .place = span->lines ? (CliPosition){span->start.line, head.col} : span->start);
    }
    else if ((name == '>') || (name == '<'))
    {
        (void)EMBED_CALL(cli_view_vim.indent, CliViewVimCfg, .key = args->key, .from = span->start, .to = span->end,
                         .deeper = name == '>', .err_msg = args->err_msg);
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
                   .place = (CliPosition){span->start.line, EMBED_CALL(cli_vim_motions_internal.first_letter, CliVimCtx,
                                                                       .key = args->key, .line = span->start.line)});
    }
    else if (name == 'd')
    {
        EMBED_CALL(cli_vim_commands_internal.store, CliVimCtx, .register_name = args->register_name, .text = text,
                   .length = length, .lines = span->lines, .yanked = 0);
        const unsigned long long lines = EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
        CliPosition from = span->start;
        CliPosition to = span->end;
        if (span->lines && (span->end.line + 1ULL < lines))
        {
            to = (CliPosition){span->end.line + 1ULL, 0ULL};
        }
        else if (span->lines && (span->start.line > 0ULL))
        {
            from = (CliPosition){span->start.line - 1ULL, EMBED_CALL(commands_length, CliVimCtx, .key = args->key,
                                                                     .line = span->start.line - 1ULL)};
        }
        EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = from, .to = to, .text = "", .length = 0ULL,
                   .err_msg = args->err_msg);
        const unsigned long long left = EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
        const unsigned long long line =
            (span->start.line < left) ? span->start.line : ((left > 0ULL) ? left - 1ULL : 0ULL);
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
                   .place = span->lines ? (CliPosition){line, EMBED_CALL(cli_vim_motions_internal.first_letter,
                                                                         CliVimCtx, .key = args->key, .line = line)}
                                        : span->start);
    }
    else if (name == 'c')
    {
        EMBED_CALL(cli_vim_commands_internal.store, CliVimCtx, .register_name = args->register_name, .text = text,
                   .length = length, .lines = span->lines, .yanked = 0);
        const unsigned long long most =
            span->lines ? EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = span->start.line) : 0ULL;
        char *const lead = (most != 0ULL) ? g_cli_guard->temporary_buf_alloc(most, g_cli_vim_owner) : NULL;
        const unsigned long long blanks = (lead != NULL)
                                              ? EMBED_CALL(commands_lead, CliVimCtx, .key = args->key,
                                                           .line = span->start.line, .bytes_out = lead, .count = most)
                                              : 0ULL;
        EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = span->start, .to = span->end,
                   .text = (lead != NULL) ? lead : "", .length = blanks, .err_msg = args->err_msg);
        g_cli_guard->persistent_buf_release(lead, g_cli_vim_owner);
        EMBED_CALL(cli_vim_commands_internal.insert_at, CliVimCtx, .at = args->at, .key = args->key,
                   .place = (CliPosition){span->start.line, span->start.col + blanks});
    }
    g_cli_guard->persistent_buf_release(text, g_cli_vim_owner);
}

void cli_vim_operate(const CliVimCtx *args)
{
    const CliVimCommand *const command = args->command;
    const CliPosition from = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
    const unsigned long long times = command->counted ? command->count : 1ULL;
    CliVimSpan span = {0, from, from, 0};
    if (command->lines)
    {
        const unsigned long long lines = EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
        const unsigned long long last_line = (lines > 0ULL) ? lines - 1ULL : 0ULL;
        const unsigned long long last =
            ((last_line - from.line) > (times - 1ULL)) ? from.line + times - 1ULL : last_line;
        span = (CliVimSpan){
            1, {from.line, 0ULL}, {last, EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = last)}, 1};
    }
    else if (command->object_kind != 0u)
    {
        EMBED_CALL(cli_vim_motions_internal.object, CliVimCtx, .key = args->key, .from = from,
                   .how = command->object_kind, .letter = command->object_of, .count = command->count,
                   .counted = command->counted, .span = &span);
        if (!span.found)
        {
            return;
        }
    }
    else
    {
        unsigned int motion = command->motion;
        const int word = (motion == 'w') || (motion == 'W');
        if ((command->name == 'c') && word &&
            (EMBED_CALL(cli_vim_motions_internal.kind, CliVimCtx, .big = motion == 'W',
                        .letter = EMBED_CALL(cli_vim_motions_internal.letter_at, CliVimCtx, .key = args->key,
                                             .place = from)) != 0u))
        {
            motion = (motion == 'w') ? 'e' : 'E';
        }
        CliVimFound found;
        EMBED_CALL(cli_vim_motions_internal.motion, CliVimCtx, .at = args->at, .key = args->key, .name = motion,
                   .letter = command->letter, .count = command->count, .counted = command->counted, .from = from,
                   .found = &found);
        if (!found.found)
        {
            return;
        }
        CliPosition to = found.to;
        if (((motion == 'w') || (motion == 'W')) && (to.line > from.line))
        {
            to = (CliPosition){from.line, EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = from.line)};
        }
        const int ahead = (from.line < to.line) || ((from.line == to.line) && (from.col <= to.col));
        CliPosition start = ahead ? from : to;
        CliPosition end = ahead ? to : from;
        if (found.inclusive)
        {
            const unsigned long long length =
                EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = end.line);
            end = found.has_end ? found.end
                                : (CliPosition){end.line, (end.col + 1ULL < length) ? end.col + 1ULL : length};
        }
        if (found.linewise)
        {
            start = (CliPosition){start.line, 0ULL};
            end = (CliPosition){end.line, EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = end.line)};
        }
        span = (CliVimSpan){1, start, end, found.linewise};
    }
    g_cli_vim_state.view[args->at].goal_set = 0;
    EMBED_CALL(cli_vim_commands_internal.apply, CliVimCtx, .at = args->at, .key = args->key, .name = command->name,
               .span_read = &span, .register_name = command->register_name, .err_msg = args->err_msg);
}

// the case of a unit turned, a small letter of the basic Latin, Latin-1, Greek or Cyrillic made its capital and a
// capital its small letter, any other unit as it is; the unit stays as many bytes of UTF-8 as it was
EMBED_INLINE unsigned short commands_case(const CliVimCtx *args)
{
    const unsigned int unit = args->letter;
    const int small = ((unit >= 'a') && (unit <= 'z')) || ((unit >= 0xe0u) && (unit <= 0xfeu) && (unit != 0xf7u)) ||
                      ((unit >= 0x3b1u) && (unit <= 0x3c9u) && (unit != 0x3c2u)) ||
                      ((unit >= 0x430u) && (unit <= 0x44fu));
    const int capital = ((unit >= 'A') && (unit <= 'Z')) || ((unit >= 0xc0u) && (unit <= 0xdeu) && (unit != 0xd7u)) ||
                        ((unit >= 0x391u) && (unit <= 0x3a9u)) || ((unit >= 0x410u) && (unit <= 0x42fu));
    unsigned int turned = unit;
    turned = small ? unit - 0x20u : turned;
    turned = capital ? unit + 0x20u : turned;
    turned = ((unit >= 0x450u) && (unit <= 0x45fu)) ? unit - 0x50u : turned;
    turned = ((unit >= 0x400u) && (unit <= 0x40fu)) ? unit + 0x50u : turned;
    // a unit stays a unit, turned within its plane's first page
    return (unsigned short)turned;
}

void cli_vim_case_turned(const CliVimCtx *args)
{
    const unsigned long long bytes =
        EMBED_CALL(cli_view_vim.span_bytes, CliViewVimCfg, .key = args->key, .from = args->from, .to = args->to);
    // the turned text takes the bytes the span takes; its units, a unit at the most a byte, after it
    char *const cell =
        g_cli_guard->temporary_buf_alloc((bytes + 1ULL) + (bytes * sizeof(unsigned short)) + 2ULL, g_cli_vim_owner);
    if (cell == NULL)
    {
        return;
    }
    unsigned short *const units = (unsigned short *)(void *)(cell + ((bytes + 2ULL) & ~1ULL));
    VerbaLinea out = {cell, bytes + 1ULL, 0ULL};
    for (unsigned long long line = args->from.line; line <= args->to.line; line += 1ULL)
    {
        unsigned long long length = 0ULL;
        const unsigned short *const read =
            EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key, .line = line, .length_out = &length);
        const unsigned long long start = (line == args->from.line) ? args->from.col : 0ULL;
        const unsigned long long end = ((line == args->to.line) && (args->to.col < length)) ? args->to.col : length;
        for (unsigned long long at = start; (read != NULL) && (at < end); at += 1ULL)
        {
            units[at - start] = EMBED_CALL(commands_case, CliVimCtx, .letter = read[at]);
        }
        if (end > start)
        {
            EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &out, .units = units,
                       .count = end - start);
        }
        if (line < args->to.line)
        {
            mmgr_line_ch(&out, '\n');
        }
    }
    const unsigned long long length = mmgr_line_finish(&out);
    EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = args->from, .to = args->to, .text = cell,
               .length = length, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(cell, g_cli_vim_owner);
}

void cli_vim_join(const CliVimCtx *args)
{
    const unsigned long long lines = EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
    const unsigned long long last_line = (lines > 0ULL) ? lines - 1ULL : 0ULL;
    const unsigned long long by = (args->count > 2ULL) ? args->count - 1ULL : 1ULL;
    const unsigned long long first = args->line;
    const unsigned long long last = ((last_line - first) > by) ? first + by : last_line;
    if (last <= first)
    {
        return;
    }
    // the bytes the joined text may take: each line after the first at three a unit, and a space before it
    unsigned long long most = 0ULL;
    for (unsigned long long line = first + 1ULL; line <= last; line += 1ULL)
    {
        most += (3ULL * EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = line)) + 1ULL;
    }
    char *const cell = g_cli_guard->temporary_buf_alloc(most + 1ULL, g_cli_vim_owner);
    if (cell == NULL)
    {
        return;
    }
    VerbaLinea out = {cell, most + 1ULL, 0ULL};
    const unsigned long long col = EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = first);
    unsigned long long lead_last = 0ULL;
    for (unsigned long long line = first; line < last; line += 1ULL)
    {
        unsigned long long length = 0ULL;
        const unsigned short *const units =
            EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key, .line = line, .length_out = &length);
        const int ends_blank = (units != NULL) && (length != 0ULL) &&
                               EMBED_CALL(cli_text_table.white, CliTextCfg, .letter = units[length - 1ULL]);
        const unsigned short *const next = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                      .line = line + 1ULL, .length_out = &length);
        unsigned long long lead = 0ULL;
        while ((next != NULL) && (lead < length) && EMBED_CALL(cli_text_table.white, CliTextCfg, .letter = next[lead]))
        {
            lead += 1ULL;
        }
        const int spaced = !((lead == length) || ((next != NULL) && (next[lead] == ')')) || ends_blank);
        mmgr_line_put_n(&out, " ", spaced ? 1ULL : 0ULL);
        if ((line + 1ULL < last) && (next != NULL) && (length > lead))
        {
            EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &out, .units = next + lead,
                       .count = length - lead);
        }
        lead_last = lead;
    }
    const unsigned long long length = mmgr_line_finish(&out);
    EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = (CliPosition){first, col},
               .to = (CliPosition){last, lead_last}, .text = cell, .length = length, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(cell, g_cli_vim_owner);
    EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
               .place = (CliPosition){first, col});
}

void cli_vim_search(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    const unsigned int search_part =
        EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_SEARCH);
    if (!EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = search_part, .text = args->text,
                    .length = args->length))
    {
        return;
    }
    view->searched = 1;
    view->searched_back = args->back;
    view->searched_word = args->word_text;
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = search_part, .length_out = &length);
    CliPosition to = {0ULL, 0ULL};
    if (EMBED_CALL(cli_view_vim.find, CliViewVimCfg, .key = args->key, .text = text, .length = length,
                   .from = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key),
                   .back = args->back, .word_text = args->word_text, .place = &to))
    {
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key, .place = to);
    }
}

// the register the command names put at the cursor, before it for P, `count` times: whole lines on the line below or
// above, letters after the cursor's letter or before it
EMBED_INLINE void commands_paste(const CliVimCtx *args)
{
    const CliVimState *const state = &g_cli_vim_state;
    const CliVimCommand *const command = args->command;
    const unsigned int at = EMBED_CALL(commands_register_at, CliVimCtx, .register_name = command->register_name);
    unsigned long long length = 0ULL;
    if (at >= CLI_VIM_REGISTERS)
    {
        return;
    }
    (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = at, .length_out = &length);
    if (length == 0ULL)
    {
        return;
    }
    const int lines = state->register_lines[at] != 0u;
    const int before = args->back;
    const unsigned long long times = command->counted ? command->count : 1ULL;
    const unsigned long long bytes = (times * (length + 1ULL)) + 1ULL;
    char *const cell = g_cli_guard->temporary_buf_alloc(bytes + 1ULL, g_cli_vim_owner);
    if (cell == NULL)
    {
        return;
    }
    const char *const kept = EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = at, .length_out = &length);
    VerbaLinea out = {cell, bytes + 1ULL, 0ULL};
    mmgr_line_put_n(&out, "\n", (lines && !before) ? 1ULL : 0ULL);
    for (unsigned long long time = 0ULL; time < times; time += 1ULL)
    {
        mmgr_line_put_n(&out, "\n", (lines && (time > 0ULL)) ? 1ULL : 0ULL);
        mmgr_line_put_n(&out, kept, length);
    }
    mmgr_line_put_n(&out, "\n", (lines && before) ? 1ULL : 0ULL);
    const unsigned long long written = mmgr_line_finish(&out);
    const CliPosition head = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
    const unsigned long long line_length = EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = head.line);
    if (lines)
    {
        const CliPosition where = before ? (CliPosition){head.line, 0ULL} : (CliPosition){head.line, line_length};
        EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = where, .to = where, .text = cell,
                   .length = written, .err_msg = args->err_msg);
        const unsigned long long line = before ? head.line : head.line + 1ULL;
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
                   .place = (CliPosition){line, EMBED_CALL(cli_vim_motions_internal.first_letter, CliVimCtx,
                                                           .key = args->key, .line = line)});
    }
    else
    {
        const CliPosition where = {head.line, (before || (line_length == 0ULL)) ? head.col : head.col + 1ULL};
        EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = where, .to = where, .text = cell,
                   .length = written, .err_msg = args->err_msg);
        // the view's cursor stands after the text the edit put in
        const CliPosition after = EMBED_CALL(commands_cursor, CliVimCtx, .key = args->key);
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
                   .place = (CliPosition){after.line, (after.col > 0ULL) ? after.col - 1ULL : 0ULL});
    }
    g_cli_guard->persistent_buf_release(cell, g_cli_vim_owner);
}

// the word under the cursor or after it on its line, searched for as a whole word, back where `back`
EMBED_INLINE void commands_word_searched(const CliVimCtx *args)
{
    const CliPosition head = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = head.line, .length_out = &length);
    if ((units == NULL) || (length == 0ULL))
    {
        return;
    }
    const unsigned long long col = (head.col < length) ? head.col : length - 1ULL;
    unsigned long long start = col + 1ULL;
    while ((start > 0ULL) &&
           (EMBED_CALL(cli_vim_motions_internal.kind, CliVimCtx, .letter = units[start - 1ULL]) == 1u))
    {
        start -= 1ULL;
    }
    unsigned long long end = col + 1ULL;
    while ((end < length) && (EMBED_CALL(cli_vim_motions_internal.kind, CliVimCtx, .letter = units[end]) == 1u))
    {
        end += 1ULL;
    }
    if (start == end)
    {
        return;
    }
    const unsigned long long most = (3ULL * (end - start)) + 1ULL;
    char *const word = g_cli_guard->temporary_buf_alloc(most, g_cli_vim_owner);
    if (word == NULL)
    {
        return;
    }
    VerbaLinea out = {word, most, 0ULL};
    EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = &out, .units = units + start,
               .count = end - start);
    const unsigned long long written = mmgr_line_finish(&out);
    EMBED_CALL(cli_vim_commands_internal.search, CliVimCtx, .at = args->at, .key = args->key, .text = word,
               .length = written, .back = args->back, .word_text = 1);
    g_cli_guard->persistent_buf_release(word, g_cli_vim_owner);
}

// o and O: a line opened below the cursor's, or above it for O, with its blanks, and insert mode entered on it
EMBED_INLINE void commands_line_opened(const CliVimCtx *args)
{
    const CliPosition head = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
    const unsigned long long length = EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = head.line);
    char *const cell = g_cli_guard->temporary_buf_alloc(length + 2ULL, g_cli_vim_owner);
    if (cell == NULL)
    {
        return;
    }
    const int below = !args->back;
    const unsigned long long lead = EMBED_CALL(commands_lead, CliVimCtx, .key = args->key, .line = head.line,
                                               .bytes_out = cell + (below ? 1ULL : 0ULL), .count = length);
    cell[below ? 0ULL : lead] = '\n';
    const CliPosition where = below ? (CliPosition){head.line, length} : (CliPosition){head.line, 0ULL};
    EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = where, .to = where, .text = cell,
               .length = lead + 1ULL, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(cell, g_cli_vim_owner);
    EMBED_CALL(cli_vim_commands_internal.insert_at, CliVimCtx, .at = args->at, .key = args->key,
               .place = (CliPosition){below ? head.line + 1ULL : head.line, lead});
}

// x X D C s ~ r: the letters under and after the cursor, before it, to the line's end, changed, turned or replaced,
// `count` of them
EMBED_INLINE void commands_letters(const CliVimCtx *args)
{
    const CliVimCommand *const command = args->command;
    const CliPosition head = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
    const unsigned long long times = command->counted ? command->count : 1ULL;
    const unsigned long long length = EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = head.line);
    const CliPosition ahead = {head.line, (head.col + times < length) ? head.col + times : length};
    const unsigned int name = command->name;
    if (((name == 'x') || (name == 's')) && (length != 0ULL))
    {
        const CliVimSpan span = {1, head, ahead, 0};
        EMBED_CALL(cli_vim_commands_internal.apply, CliVimCtx, .at = args->at, .key = args->key,
                   .name = (name == 'x') ? 'd' : 'c', .span_read = &span, .register_name = command->register_name,
                   .err_msg = args->err_msg);
    }
    else if (name == 's')
    {
        EMBED_CALL(cli_vim_commands_internal.insert_at, CliVimCtx, .at = args->at, .key = args->key, .place = head);
    }
    else if ((name == 'X') && (head.col > 0ULL))
    {
        const CliVimSpan span = {1, {head.line, (head.col > times) ? head.col - times : 0ULL}, head, 0};
        EMBED_CALL(cli_vim_commands_internal.apply, CliVimCtx, .at = args->at, .key = args->key, .name = 'd',
                   .span_read = &span, .register_name = command->register_name, .err_msg = args->err_msg);
    }
    else if ((name == 'D') || (name == 'C'))
    {
        const unsigned long long lines = EMBED_CALL(cli_vim_motions_internal.lines, CliVimCtx, .key = args->key);
        const unsigned long long last_line = (lines > 0ULL) ? lines - 1ULL : 0ULL;
        const unsigned long long last =
            ((last_line - head.line) > (times - 1ULL)) ? head.line + times - 1ULL : last_line;
        const CliVimSpan span = {
            1, head, {last, EMBED_CALL(commands_length, CliVimCtx, .key = args->key, .line = last)}, 0};
        EMBED_CALL(cli_vim_commands_internal.apply, CliVimCtx, .at = args->at, .key = args->key,
                   .name = (name == 'D') ? 'd' : 'c', .span_read = &span, .register_name = command->register_name,
                   .err_msg = args->err_msg);
    }
    else if ((name == '~') && (length != 0ULL))
    {
        EMBED_CALL(cli_vim_commands_internal.case_turned, CliVimCtx, .key = args->key, .from = head, .to = ahead,
                   .err_msg = args->err_msg);
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key, .place = ahead);
    }
    else if ((name == 'r') && (head.col + times <= length))
    {
        char bytes[4];
        const unsigned long long letter_bytes =
            EMBED_CALL(cli_vim_internal.letter_written, CliVimCtx, .letter = command->letter, .bytes_out = bytes);
        char *const cell = g_cli_guard->temporary_buf_alloc((times * letter_bytes) + 1ULL, g_cli_vim_owner);
        if (cell == NULL)
        {
            return;
        }
        VerbaLinea out = {cell, (times * letter_bytes) + 1ULL, 0ULL};
        for (unsigned long long time = 0ULL; time < times; time += 1ULL)
        {
            mmgr_line_put_n(&out, bytes, letter_bytes);
        }
        const unsigned long long written = mmgr_line_finish(&out);
        EMBED_CALL(commands_edit, CliVimCtx, .key = args->key, .from = head,
                   .to = (CliPosition){head.line, head.col + times}, .text = cell, .length = written,
                   .err_msg = args->err_msg);
        g_cli_guard->persistent_buf_release(cell, g_cli_vim_owner);
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
                   .place = (CliPosition){head.line, head.col + times - 1ULL});
    }
}

// u and Ctrl+R: the view's last step undone, or its next redone, `count` times, the cursor put where the view left it
EMBED_INLINE void commands_stepped(const CliVimCtx *args)
{
    const CliVimCommand *const command = args->command;
    const unsigned long long times = command->counted ? command->count : 1ULL;
    for (unsigned long long time = 0ULL; time < times; time += 1ULL)
    {
        (void)EMBED_CALL(cli_view_vim.step, CliViewVimCfg, .key = args->key, .redo = command->name == CLI_VIM_REDO,
                         .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key,
               .place = EMBED_CALL(commands_cursor, CliVimCtx, .key = args->key));
}

// i a I A: insert mode at the cursor, after its letter, at the line's first letter that is not blank, or at its end
EMBED_INLINE void commands_inserted(const CliVimCtx *args)
{
    const CliPosition head = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
    unsigned long long length = 0ULL;
    const unsigned short *const units = EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key,
                                                   .line = head.line, .length_out = &length);
    unsigned long long first = 0ULL;
    while ((units != NULL) && (first < length) && EMBED_CALL(cli_text_table.white, CliTextCfg, .letter = units[first]))
    {
        first += 1ULL;
    }
    CliPosition place = head;
    if (args->name == 'a')
    {
        place.col = (head.col + 1ULL < length) ? head.col + 1ULL : length;
    }
    else if (args->name == 'I')
    {
        place.col = first;
    }
    else if (args->name == 'A')
    {
        place.col = length;
    }
    EMBED_CALL(cli_vim_commands_internal.insert_at, CliVimCtx, .at = args->at, .key = args->key, .place = place);
}

void cli_vim_command_run(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    const CliVimCommand *const command = args->command;
    const unsigned int name = command->name;
    const unsigned long long times = command->counted ? command->count : 1ULL;
    if ((name == 'x') || (name == 'X') || (name == 'D') || (name == 'C') || (name == 's') || (name == '~') ||
        (name == 'r'))
    {
        EMBED_CALL(commands_letters, CliVimCtx, .at = args->at, .key = args->key, .command = command,
                   .err_msg = args->err_msg);
    }
    else if ((name == 'Y') || (name == 'S'))
    {
        const CliVimCommand lines = {
            CLI_VIM_PARSE_WHOLE, command->register_name, 1, times, (name == 'Y') ? 'y' : 'c', 0u, 0u, 0u, 0u, 1, 0};
        EMBED_CALL(cli_vim_commands_internal.operate, CliVimCtx, .at = args->at, .key = args->key, .command = &lines,
                   .err_msg = args->err_msg);
    }
    else if (name == 'J')
    {
        EMBED_CALL(cli_vim_commands_internal.join, CliVimCtx, .at = args->at, .key = args->key,
                   .line = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key).line,
                   .count = (times > 2ULL) ? times : 2ULL, .err_msg = args->err_msg);
    }
    else if ((name == 'p') || (name == 'P'))
    {
        EMBED_CALL(commands_paste, CliVimCtx, .at = args->at, .key = args->key, .command = command, .back = name == 'P',
                   .err_msg = args->err_msg);
    }
    else if ((name == 'u') || (name == CLI_VIM_REDO))
    {
        EMBED_CALL(commands_stepped, CliVimCtx, .at = args->at, .key = args->key, .command = command,
                   .err_msg = args->err_msg);
    }
    else if ((name == 'i') || (name == 'a') || (name == 'I') || (name == 'A'))
    {
        EMBED_CALL(commands_inserted, CliVimCtx, .at = args->at, .key = args->key, .name = name);
    }
    else if ((name == 'o') || (name == 'O'))
    {
        EMBED_CALL(commands_line_opened, CliVimCtx, .at = args->at, .key = args->key, .back = name == 'O',
                   .err_msg = args->err_msg);
    }
    else if ((name == 'v') || (name == 'V'))
    {
        const CliPosition head = EMBED_CALL(cli_vim_motions_internal.head, CliVimCtx, .at = args->at, .key = args->key);
        view->visual_anchor = head;
        view->visual_head = head;
        EMBED_CALL(cli_vim_internal.mode_set, CliVimCtx, .at = args->at,
                   .mode = (name == 'v') ? CLI_VIM_VISUAL : CLI_VIM_LINE);
        EMBED_CALL(cli_vim_commands_internal.chosen_drawn, CliVimCtx, .at = args->at, .key = args->key);
    }
    else if ((name == '/') || (name == '?'))
    {
        view->prompt_open = 1;
        view->prompt_back = name == '?';
        (void)EMBED_CALL(
            cli_vim_internal.part_put, CliVimCtx,
            .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_PROMPT),
            .text = "", .length = 0ULL);
    }
    else if ((name == '*') || (name == '#'))
    {
        EMBED_CALL(commands_word_searched, CliVimCtx, .at = args->at, .key = args->key, .back = name == '#');
    }
}
