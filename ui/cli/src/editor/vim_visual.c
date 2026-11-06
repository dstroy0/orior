// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim_visual.c: visual mode and visual line mode: the selection from the anchor to the head, both letters taken, or
// their lines whole, set as the view's selection; a text object taken as the selection, the ends swapped, and the
// commands that act on what is chosen
#include "vim_internal.h"

void cli_vim_visual_span(const CliVimCtx *args)
{
    const CliVimView *const view = &g_cli_vim_state.view[args->at];
    const CliPosition anchor = view->visual_anchor;
    const CliPosition head = view->visual_head;
    const int ahead = (anchor.line < head.line) || ((anchor.line == head.line) && (anchor.col <= head.col));
    const CliPosition first = ahead ? anchor : head;
    const CliPosition second = ahead ? head : anchor;
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_vim_motions_internal.line, CliVimCtx, .key = args->key, .line = second.line,
                     .length_out = &length);
    if (view->mode == CLI_VIM_LINE)
    {
        *args->span = (CliVimSpan){1, {first.line, 0ULL}, {second.line, length}, 1};
        return;
    }
    *args->span = (CliVimSpan){1, first, {second.line, (second.col + 1ULL < length) ? second.col + 1ULL : length}, 0};
}

void cli_vim_chosen_drawn(const CliVimCtx *args)
{
    const CliVimView *const view = &g_cli_vim_state.view[args->at];
    CliVimSpan span;
    EMBED_CALL(cli_vim_commands_internal.visual_span, CliVimCtx, .at = args->at, .key = args->key, .span = &span);
    const int ahead =
        (view->visual_anchor.line < view->visual_head.line) ||
        ((view->visual_anchor.line == view->visual_head.line) && (view->visual_anchor.col <= view->visual_head.col));
    EMBED_CALL(cli_view_vim.place, CliViewVimCfg, .key = args->key, .anchor = ahead ? span.start : span.end,
               .cursor = ahead ? span.end : span.start);
}

void cli_vim_visual_left(const CliVimCtx *args)
{
    EMBED_CALL(cli_vim_internal.mode_set, CliVimCtx, .at = args->at, .mode = CLI_VIM_NORMAL);
    EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key, .place = args->place);
}

// p and P over the selection: what it holds taken into the register " and the register the command names put in its
// place, the cursor at its start
EMBED_INLINE void visual_pasted(const CliVimCtx *args)
{
    const CliVimSpan *const span = args->span_read;
    const unsigned int name = args->register_name;
    unsigned int at = CLI_VIM_REGISTERS;
    at = ((name == 0u) || (name == '"')) ? CLI_VIM_REGISTER_UNNAMED : at;
    at = (name == '0') ? CLI_VIM_REGISTER_YANKED : at;
    at = ((name >= 'a') && (name <= 'z')) ? CLI_VIM_REGISTER_LETTERS + (name - 'a') : at;
    at = ((name >= 'A') && (name <= 'Z')) ? CLI_VIM_REGISTER_LETTERS + (name - 'A') : at;
    unsigned long long kept_length = 0ULL;
    if (at < CLI_VIM_REGISTERS)
    {
        (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = at, .length_out = &kept_length);
    }
    if (kept_length == 0ULL)
    {
        return;
    }
    const unsigned long long bytes =
        EMBED_CALL(cli_view_vim.span_bytes, CliViewVimCfg, .key = args->key, .from = span->start, .to = span->end);
    // the register's text and the selection's, side by side, read out of the text cell before the store writes it
    char *const cell = g_cli_guard->temporary_buf_alloc(kept_length + bytes + 2ULL, g_cli_vim_owner);
    if (cell == NULL)
    {
        return;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = cell,
                      .src = EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = at, .length_out = &kept_length),
                      .bytes = kept_length});
    VerbaLinea out = {cell + kept_length, bytes + 1ULL, 0ULL};
    EMBED_CALL(cli_view_vim.span, CliViewVimCfg, .key = args->key, .from = span->start, .to = span->end, .out = &out);
    const unsigned long long chosen = mmgr_line_finish(&out);
    EMBED_CALL(cli_vim_commands_internal.store, CliVimCtx, .register_name = 0u, .text = cell + kept_length,
               .length = chosen, .lines = span->lines, .yanked = 0);
    (void)EMBED_CALL(cli_view_vim.edit, CliViewVimCfg, .key = args->key, .from = span->start, .to = span->end,
                     .text = cell, .length = kept_length, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(cell, g_cli_vim_owner);
    EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key, .place = span->start);
}

void cli_vim_chosen_run(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    const CliVimCommand *const command = args->command;
    if (command->object_kind != 0u)
    {
        CliVimSpan object;
        EMBED_CALL(cli_vim_motions_internal.object, CliVimCtx, .key = args->key, .from = view->visual_head,
                   .how = command->object_kind, .letter = command->object_of, .count = command->count,
                   .counted = command->counted, .span = &object);
        if (!object.found)
        {
            return;
        }
        view->visual_anchor = object.start;
        CliPosition back = object.start;
        view->visual_head = EMBED_CALL(cli_vim_motions_internal.backward, CliVimCtx, .key = args->key,
                                       .place = object.end, .place_out = &back)
                                ? back
                                : object.start;
        EMBED_CALL(cli_vim_commands_internal.chosen_drawn, CliVimCtx, .at = args->at, .key = args->key);
        return;
    }
    if (command->name == 'o')
    {
        const CliPosition anchor = view->visual_anchor;
        view->visual_anchor = view->visual_head;
        view->visual_head = anchor;
        EMBED_CALL(cli_vim_commands_internal.chosen_drawn, CliVimCtx, .at = args->at, .key = args->key);
        return;
    }
    CliVimSpan span;
    EMBED_CALL(cli_vim_commands_internal.visual_span, CliVimCtx, .at = args->at, .key = args->key, .span = &span);
    unsigned int name = command->name;
    name = (name == 'x') ? 'd' : name;
    name = (name == 's') ? 'c' : name;
    EMBED_CALL(cli_vim_internal.mode_set, CliVimCtx, .at = args->at, .mode = CLI_VIM_NORMAL);
    if ((name == 'd') || (name == 'c') || (name == 'y') || (name == '>') || (name == '<'))
    {
        EMBED_CALL(cli_vim_commands_internal.apply, CliVimCtx, .at = args->at, .key = args->key, .name = name,
                   .span_read = &span, .register_name = command->register_name, .err_msg = args->err_msg);
    }
    else if (name == '~')
    {
        EMBED_CALL(cli_vim_commands_internal.case_turned, CliVimCtx, .key = args->key, .from = span.start,
                   .to = span.end, .err_msg = args->err_msg);
        EMBED_CALL(cli_vim_motions_internal.put, CliVimCtx, .at = args->at, .key = args->key, .place = span.start);
    }
    else if (name == 'J')
    {
        const unsigned long long lines = span.end.line - span.start.line + 1ULL;
        EMBED_CALL(cli_vim_commands_internal.join, CliVimCtx, .at = args->at, .key = args->key, .line = span.start.line,
                   .count = (lines > 2ULL) ? lines : 2ULL, .err_msg = args->err_msg);
    }
    else if ((name == 'p') || (name == 'P'))
    {
        EMBED_CALL(visual_pasted, CliVimCtx, .at = args->at, .key = args->key, .span_read = &span,
                   .register_name = command->register_name, .err_msg = args->err_msg);
    }
}
