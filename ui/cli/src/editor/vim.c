// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim.c: Vim's state and its entries. The views with Vim's keys stand as rows of one cell in the order of their keys,
// found by halving; every text, the registers' and each view's, is a part of one text cell, the parts end to end in
// their order, and a part written anew takes the cell anew at the size of every part. A key or a letter typed becomes
// a token, a code point, the keys no letter carries their chords' control bytes; the tokens typed of a command stand
// as a part and are read as a command each time one comes, and a whole command runs, its keys taken for `.` where it
// changes the text
#include "vim_internal.h"

#include "../text/text.h"

CliVimState g_cli_vim_state;

// each mode's label and its bytes; and the value the setting vim reads as on
static const char *const s_mode_label[4] = {"NORMAL", "INSERT", "VISUAL", "VISUAL LINE"};
static const unsigned long long s_mode_label_bytes[4] = {6ULL, 6ULL, 6ULL, 11ULL};
static const char s_setting_on[] = "true";

// the commands that change the text, which `.` does again, `y` among the operators aside
static const char s_changes[] = "dc><xXDCsSJ~rpPiaIAoO";

// the parts begun where none stand: the registers', each empty: 1, or 0 where the cells cannot meet them
EMBED_INLINE int vim_begun(const CliVimCtx *args)
{
    (void)args;
    CliVimState *const state = &g_cli_vim_state;
    if (state->part != NULL)
    {
        return 1;
    }
    state->part = g_cli_guard->persistent_buf_alloc(CLI_VIM_REGISTERS * sizeof(CliVimPart), g_cli_vim_owner);
    if (state->part == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = state->part, .val = 0u, .bytes = CLI_VIM_REGISTERS * sizeof(CliVimPart)});
    state->parts = CLI_VIM_REGISTERS;
    return 1;
}

int cli_vim_part_put(const CliVimCtx *args)
{
    CliVimState *const state = &g_cli_vim_state;
    const CliVimPart was = state->part[args->part];
    const unsigned long long bytes = state->bytes - was.length + args->length;
    char *const cell = (bytes != 0ULL) ? g_cli_guard->persistent_buf_alloc(bytes, g_cli_vim_owner) : NULL;
    if ((bytes != 0ULL) && (cell == NULL))
    {
        return 0;
    }
    const unsigned long long after = state->bytes - was.at - was.length;
    if (was.at != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = state->text, .bytes = was.at});
    }
    if (args->length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell + was.at, .src = args->text, .bytes = args->length});
    }
    if (after != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = cell + was.at + args->length, .src = state->text + was.at + was.length, .bytes = after});
    }
    g_cli_guard->persistent_buf_release(state->text, g_cli_vim_owner);
    state->text = cell;
    state->bytes = bytes;
    state->part[args->part].length = args->length;
    for (unsigned int at = args->part + 1u; at < state->parts; at += 1u)
    {
        state->part[at].at = state->part[at].at - was.length + args->length;
    }
    return 1;
}

const char *cli_vim_part_text(const CliVimCtx *args)
{
    const CliVimState *const state = &g_cli_vim_state;
    *args->length_out = state->part[args->part].length;
    return (state->text != NULL) ? state->text + state->part[args->part].at : "";
}

unsigned int cli_vim_part_of(const CliVimCtx *args)
{
    return CLI_VIM_REGISTERS + (args->at * CLI_VIM_VIEW_PARTS) + args->part;
}

void cli_vim_label_settled(const CliVimCtx *args)
{
    const CliVimView *const view = &g_cli_vim_state.view[args->at];
    const unsigned int typed_part =
        EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_TYPED);
    unsigned long long typed_length = 0ULL;
    (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = typed_part, .length_out = &typed_length);
    const unsigned long long bytes = view->on ? s_mode_label_bytes[view->mode] + 1ULL + typed_length : 0ULL;
    char *const label = (bytes != 0ULL) ? g_cli_guard->temporary_buf_alloc(bytes + 1ULL, g_cli_vim_owner) : NULL;
    VerbaLinea line = {label, (label != NULL) ? bytes + 1ULL : 0ULL, 0ULL};
    if (label != NULL)
    {
        // the keys typed read after the label's cell is taken, the text cell standing as it was
        const char *const typed =
            EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = typed_part, .length_out = &typed_length);
        mmgr_line_put(&line, s_mode_label[view->mode]);
        if (typed_length != 0ULL)
        {
            mmgr_line_ch(&line, ' ');
            mmgr_line_put_n(&line, typed, typed_length);
        }
    }
    const unsigned long long written = (label != NULL) ? mmgr_line_finish(&line) : 0ULL;
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx,
                     .part =
                         EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_LABEL),
                     .text = label, .length = written);
    g_cli_guard->persistent_buf_release(label, g_cli_vim_owner);
}

void cli_vim_mode_set(const CliVimCtx *args)
{
    g_cli_vim_state.view[args->at].mode = args->mode;
    EMBED_CALL(cli_vim_internal.label_settled, CliVimCtx, .at = args->at);
}

// whether the key's own name `text` is `required`
EMBED_INLINE int vim_named(const CliVimCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->text, .other = args->required, .cap = CLI_NAME_MAX_LEN});
}

// whether the key's name `text` starts with `required`
EMBED_INLINE int vim_starts(const CliVimCtx *args)
{
    return cellul.starts(&(CatenaFinitaCfg){.src = args->text, .other = args->required, .cap = CLI_NAME_MAX_LEN});
}

// the order of the key `key` against the view `at`'s: below 0 before it, 0 the same, above 0 after it
EMBED_INLINE int vim_key_compared(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const own =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .length_out = &length,
                   .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_KEY));
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_NAME_MAX_LEN});
    const unsigned long long shorter = (key_length < length) ? key_length : length;
    const long long compared =
        (shorter != 0ULL) ? (long long)memor.cmp(&(MemoriaCfg){.src = args->key, .other = own, .bytes = shorter}) : 0LL;
    if (compared != 0LL)
    {
        return (compared < 0LL) ? -1 : 1;
    }
    if (key_length == length)
    {
        return 0;
    }
    return (key_length < length) ? -1 : 1;
}

// the view under `key` among the views, halving them in the order of their keys: 1 with its place into
// `length_out`, or 0 with the place it would stand at
EMBED_INLINE int vim_view_found(const CliVimCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = g_cli_vim_state.views;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (EMBED_CALL(vim_key_compared, CliVimCtx, .key = args->key, .at = middle) > 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    *args->length_out = low;
    return (low < g_cli_vim_state.views) && (EMBED_CALL(vim_key_compared, CliVimCtx, .key = args->key, .at = low) == 0);
}

// a view under `key` put among the views at `at`, its parts empty, its key written, Vim's keys `on`, normal mode: 1,
// or 0 where the cells cannot meet it, the views as they were
EMBED_INLINE int vim_view_added(const CliVimCtx *args)
{
    CliVimState *const state = &g_cli_vim_state;
    const unsigned int views = state->views + 1u;
    const unsigned int parts = state->parts + CLI_VIM_VIEW_PARTS;
    CliVimView *const grown = g_cli_guard->persistent_buf_alloc(views * sizeof(CliVimView), g_cli_vim_owner);
    CliVimPart *const more = g_cli_guard->persistent_buf_alloc(parts * sizeof(CliVimPart), g_cli_vim_owner);
    if ((grown == NULL) || (more == NULL))
    {
        g_cli_guard->persistent_buf_release(grown, g_cli_vim_owner);
        g_cli_guard->persistent_buf_release(more, g_cli_vim_owner);
        return 0;
    }
    if (args->at != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = state->view, .bytes = args->at * sizeof(CliVimView)});
    }
    if (args->at < state->views)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown + args->at + 1u,
                                .src = state->view + args->at,
                                .bytes = (state->views - args->at) * sizeof(CliVimView)});
    }
    memor.set(&(MemoriaCfg){.dst = grown + args->at, .val = 0u, .bytes = sizeof(CliVimView)});
    grown[args->at].on = args->on;
    const unsigned int first = CLI_VIM_REGISTERS + (args->at * CLI_VIM_VIEW_PARTS);
    memor.cpy(&(MemoriaCfg){.dst = more, .src = state->part, .bytes = first * sizeof(CliVimPart)});
    const unsigned long long start = (first < state->parts) ? state->part[first].at : state->bytes;
    for (unsigned int each = 0u; each < CLI_VIM_VIEW_PARTS; each += 1u)
    {
        more[first + each] = (CliVimPart){start, 0ULL};
    }
    if (first < state->parts)
    {
        memor.cpy(&(MemoriaCfg){.dst = more + first + CLI_VIM_VIEW_PARTS,
                                .src = state->part + first,
                                .bytes = (state->parts - first) * sizeof(CliVimPart)});
    }
    g_cli_guard->persistent_buf_release(state->view, g_cli_vim_owner);
    g_cli_guard->persistent_buf_release(state->part, g_cli_vim_owner);
    state->view = grown;
    state->part = more;
    state->views = views;
    state->parts = parts;
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx,
                     .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_KEY),
                     .text = args->key,
                     .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_NAME_MAX_LEN}));
    EMBED_CALL(cli_vim_internal.label_settled, CliVimCtx, .at = args->at);
    return 1;
}

// the view `at` taken out of the views, its parts emptied first and the cells taken anew at the size of what is left;
// where the cells cannot meet that its keys stay off
EMBED_INLINE void vim_view_removed(const CliVimCtx *args)
{
    CliVimState *const state = &g_cli_vim_state;
    for (unsigned int each = 0u; each < CLI_VIM_VIEW_PARTS; each += 1u)
    {
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx,
                         .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = each),
                         .text = "", .length = 0ULL);
    }
    const unsigned int views = state->views - 1u;
    const unsigned int parts = state->parts - CLI_VIM_VIEW_PARTS;
    const unsigned int first = CLI_VIM_REGISTERS + (args->at * CLI_VIM_VIEW_PARTS);
    CliVimView *const fewer =
        (views != 0u) ? g_cli_guard->persistent_buf_alloc(views * sizeof(CliVimView), g_cli_vim_owner) : NULL;
    CliVimPart *const less = g_cli_guard->persistent_buf_alloc(parts * sizeof(CliVimPart), g_cli_vim_owner);
    if (((views != 0u) && (fewer == NULL)) || (less == NULL))
    {
        g_cli_guard->persistent_buf_release(fewer, g_cli_vim_owner);
        g_cli_guard->persistent_buf_release(less, g_cli_vim_owner);
        state->view[args->at].on = 0;
        return;
    }
    if (args->at != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = fewer, .src = state->view, .bytes = args->at * sizeof(CliVimView)});
    }
    if (args->at + 1u < state->views)
    {
        memor.cpy(&(MemoriaCfg){.dst = fewer + args->at,
                                .src = state->view + args->at + 1u,
                                .bytes = (state->views - args->at - 1u) * sizeof(CliVimView)});
    }
    memor.cpy(&(MemoriaCfg){.dst = less, .src = state->part, .bytes = first * sizeof(CliVimPart)});
    if (first + CLI_VIM_VIEW_PARTS < state->parts)
    {
        memor.cpy(&(MemoriaCfg){.dst = less + first,
                                .src = state->part + first + CLI_VIM_VIEW_PARTS,
                                .bytes = (state->parts - first - CLI_VIM_VIEW_PARTS) * sizeof(CliVimPart)});
    }
    g_cli_guard->persistent_buf_release(state->view, g_cli_vim_owner);
    g_cli_guard->persistent_buf_release(state->part, g_cli_vim_owner);
    state->view = fewer;
    state->part = less;
    state->views = views;
    state->parts = parts;
}

// the view `at` set back to normal mode with Vim's keys `on`, the keys typed, the prompt and the change being taken
// let go
EMBED_INLINE void vim_view_reset(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    view->on = args->on;
    view->prompt_open = 0;
    view->goal_set = 0;
    view->taking = 0;
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx,
                     .part =
                         EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_TYPED),
                     .text = "", .length = 0ULL);
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx,
                     .part =
                         EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_PROMPT),
                     .text = "", .length = 0ULL);
    EMBED_CALL(cli_vim_internal.mode_set, CliVimCtx, .at = args->at, .mode = CLI_VIM_NORMAL);
}

// the view under `key` with Vim's keys on: 1 with its place into `length_out`, a view the setting turns on put among
// the views as it first comes; or 0 where its keys are off
EMBED_INLINE int vim_view_on(const CliVimCtx *args)
{
    unsigned long long place = 0ULL;
    if ((args->key == NULL) || !EMBED_CALL(vim_begun, CliVimCtx, .at = 0u))
    {
        return 0;
    }
    // the place is among the views or where one is put, fewer than an unsigned int counts
    if (!EMBED_CALL(vim_view_found, CliVimCtx, .key = args->key, .length_out = &place) &&
        (!g_cli_vim_state.on ||
         !EMBED_CALL(vim_view_added, CliVimCtx, .key = args->key, .at = (unsigned int)place, .on = 1)))
    {
        return 0;
    }
    *args->length_out = place;
    // the place is among the views, fewer than an unsigned int counts
    return g_cli_vim_state.view[(unsigned int)place].on;
}

// `length` bytes of `text` put after the part `part`'s bytes: 1, or 0 where the cells cannot meet it
EMBED_INLINE int vim_part_added(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = args->part, .length_out = &length);
    char *const joined = g_cli_guard->temporary_buf_alloc(length + args->length + 1ULL, g_cli_vim_owner);
    if (joined == NULL)
    {
        return 0;
    }
    const char *const before =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = args->part, .length_out = &length);
    if (length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = joined, .src = before, .bytes = length});
    }
    if (args->length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = joined + length, .src = args->text, .bytes = args->length});
    }
    const int put = EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = args->part, .text = joined,
                               .length = length + args->length);
    g_cli_guard->persistent_buf_release(joined, g_cli_vim_owner);
    return put;
}

// the part `part` of the view `at` written as its part `how`'s bytes
EMBED_INLINE void vim_part_copied(const CliVimCtx *args)
{
    unsigned long long length = 0ULL;
    const char *const from =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .length_out = &length,
                   .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = args->how));
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx,
                     .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = args->part),
                     .text = from, .length = length);
}

// whether the command `name` changes the text, so that `.` does it again
EMBED_INLINE int vim_changes(const CliVimCtx *args)
{
    for (unsigned int at = 0u; at < sizeof(s_changes) - 1u; at += 1u)
    {
        // a byte of the commands' text is a letter of the basic Latin
        if ((unsigned int)(unsigned char)s_changes[at] == args->name)
        {
            return 1;
        }
    }
    return 0;
}

// whether the command `name` is an operator
EMBED_INLINE int vim_operator(const CliVimCtx *args)
{
    return (args->name == 'd') || (args->name == 'c') || (args->name == 'y') || (args->name == '>') ||
           (args->name == '<');
}

// the command parsed run on the view `at`, its keys standing as the keys typed: taken for `.` where it changes the
// text and `.` is not replaying, the change ending where the command leaves a mode other than insert
EMBED_INLINE void vim_run(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    const CliVimCommand *const command = args->command;
    const int changes = !view->replaying && !command->chosen && (command->name != 'y') &&
                        EMBED_CALL(vim_changes, CliVimCtx, .name = command->name);
    if (changes)
    {
        EMBED_CALL(vim_part_copied, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_TAKING_KEYS,
                   .how = CLI_VIM_PART_TYPED);
        (void)EMBED_CALL(
            cli_vim_internal.part_put, CliVimCtx,
            .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_TAKING_INSERT),
            .text = "", .length = 0ULL);
        view->taking = 1;
    }
    if (command->chosen || ((command->object_kind != 0u) && (command->name == 0u)))
    {
        EMBED_CALL(cli_vim_commands_internal.chosen_run, CliVimCtx, .at = args->at, .key = args->key,
                   .command = command, .err_msg = args->err_msg);
    }
    else if ((command->motion != 0u) && (command->name == 0u))
    {
        EMBED_CALL(cli_vim_motions_internal.move_by, CliVimCtx, .at = args->at, .key = args->key,
                   .name = command->motion, .letter = command->letter, .count = command->count,
                   .counted = command->counted);
    }
    else if (EMBED_CALL(vim_operator, CliVimCtx, .name = command->name))
    {
        EMBED_CALL(cli_vim_commands_internal.operate, CliVimCtx, .at = args->at, .key = args->key, .command = command,
                   .err_msg = args->err_msg);
    }
    else
    {
        EMBED_CALL(cli_vim_commands_internal.command_run, CliVimCtx, .at = args->at, .key = args->key,
                   .command = command, .err_msg = args->err_msg);
    }
    if (changes && view->taking && (view->mode != CLI_VIM_INSERT))
    {
        EMBED_CALL(vim_part_copied, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_LAST_KEYS,
                   .how = CLI_VIM_PART_TAKING_KEYS);
        EMBED_CALL(vim_part_copied, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_LAST_INSERT,
                   .how = CLI_VIM_PART_TAKING_INSERT);
        view->taking = 0;
    }
}

// the input `length` bytes of `text` taken in insert mode played into the view as it was typed, a key between
// CLI_VIM_KEY_OPEN and CLI_VIM_KEY_CLOSE as a key and the rest as text
EMBED_INLINE void vim_played(const CliVimCtx *args)
{
    const char *const text = args->text;
    unsigned long long at = 0ULL;
    while (at < args->length)
    {
        const int keyed = text[at] == CLI_VIM_KEY_OPEN;
        const unsigned long long start = keyed ? at + 1ULL : at;
        unsigned long long end = start;
        while ((end < args->length) && (text[end] != (keyed ? CLI_VIM_KEY_CLOSE : CLI_VIM_KEY_OPEN)))
        {
            end += 1ULL;
        }
        char name[CLI_NAME_MAX_LEN];
        VerbaLinea named = {name, sizeof(name), 0ULL};
        mmgr_line_put_n(&named, keyed ? text + start : "", keyed ? end - start : 0ULL);
        (void)mmgr_line_finish(&named);
        const CliViewInput input = {
            .kind = keyed ? CLI_VIEW_KEY : CLI_VIEW_TEXT, .name = name, .text = text + start, .length = end - start};
        (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = args->key, .input = &input,
                         .err_msg = args->err_msg);
        at = keyed ? end + 1ULL : end;
    }
}

// `.`: the last change's keys read again and run, `.` replaying so nothing is taken, and where they leave insert
// mode the input typed after them played and insert mode left
EMBED_INLINE void vim_repeated(const CliVimCtx *args)
{
    unsigned long long keys_length = 0ULL;
    unsigned long long insert_length = 0ULL;
    const unsigned int keys_part =
        EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_LAST_KEYS);
    const unsigned int insert_part =
        EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_LAST_INSERT);
    (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = keys_part, .length_out = &keys_length);
    (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = insert_part, .length_out = &insert_length);
    if (keys_length == 0ULL)
    {
        return;
    }
    // the keys and the input read out of the text cell, which the run writes anew
    char *const copied = g_cli_guard->temporary_buf_alloc(keys_length + insert_length + 1ULL, g_cli_vim_owner);
    if (copied == NULL)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = copied,
        .src = EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = keys_part, .length_out = &keys_length),
        .bytes = keys_length});
    if (insert_length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = copied + keys_length,
            .src = EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = insert_part, .length_out = &insert_length),
            .bytes = insert_length});
    }
    CliVimCommand command;
    EMBED_CALL(cli_vim_internal.parse, CliVimCtx, .at = args->at, .text = copied, .length = keys_length,
               .command_out = &command);
    g_cli_vim_state.view[args->at].replaying = 1;
    if (command.status == CLI_VIM_PARSE_WHOLE)
    {
        EMBED_CALL(vim_run, CliVimCtx, .at = args->at, .key = args->key, .command = &command, .err_msg = args->err_msg);
    }
    if (g_cli_vim_state.view[args->at].mode == CLI_VIM_INSERT)
    {
        EMBED_CALL(vim_played, CliVimCtx, .key = args->key, .text = copied + keys_length, .length = insert_length,
                   .err_msg = args->err_msg);
        EMBED_CALL(cli_vim_commands_internal.insert_left, CliVimCtx, .at = args->at, .key = args->key);
    }
    g_cli_vim_state.view[args->at].replaying = 0;
    g_cli_guard->persistent_buf_release(copied, g_cli_vim_owner);
}

unsigned long long cli_vim_letter_written(const CliVimCtx *args)
{
    char *const bytes = args->bytes_out;
    const unsigned int letter = args->letter;
    // each byte written is a code point's bits under its marks, which a byte carries
    if (letter < 0x80u)
    {
        bytes[0] = (char)letter;
        return 1ULL;
    }
    if (letter < 0x800u)
    {
        bytes[0] = (char)(0xc0u | (letter >> 6u));
        bytes[1] = (char)(0x80u | (letter & 0x3fu));
        return 2ULL;
    }
    if (letter < 0x10000u)
    {
        bytes[0] = (char)(0xe0u | (letter >> 12u));
        bytes[1] = (char)(0x80u | ((letter >> 6u) & 0x3fu));
        bytes[2] = (char)(0x80u | (letter & 0x3fu));
        return 3ULL;
    }
    bytes[0] = (char)(0xf0u | (letter >> 18u));
    bytes[1] = (char)(0x80u | ((letter >> 12u) & 0x3fu));
    bytes[2] = (char)(0x80u | ((letter >> 6u) & 0x3fu));
    bytes[3] = (char)(0x80u | (letter & 0x3fu));
    return 4ULL;
}

// the token `letter` taken by the view `at`: Escape lets the keys typed go and ends visual mode; another is put after
// the keys typed, which run as a command once they are whole and go where they are none
EMBED_INLINE void vim_taken(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    const unsigned int typed_part =
        EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_TYPED);
    if (args->letter == CLI_VIM_ESCAPE)
    {
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = typed_part, .text = "", .length = 0ULL);
        if ((view->mode == CLI_VIM_VISUAL) || (view->mode == CLI_VIM_LINE))
        {
            EMBED_CALL(cli_vim_commands_internal.visual_left, CliVimCtx, .at = args->at, .key = args->key,
                       .place = view->visual_head);
        }
        EMBED_CALL(cli_vim_internal.label_settled, CliVimCtx, .at = args->at);
        return;
    }
    unsigned long long typed_length = 0ULL;
    (void)EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = typed_part, .length_out = &typed_length);
    if (typed_length + 4ULL > CLI_VIM_TYPED_MOST)
    {
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = typed_part, .text = "", .length = 0ULL);
    }
    char bytes[4];
    const unsigned long long length =
        EMBED_CALL(cli_vim_internal.letter_written, CliVimCtx, .letter = args->letter, .bytes_out = bytes);
    (void)EMBED_CALL(vim_part_added, CliVimCtx, .part = typed_part, .text = bytes, .length = length);
    CliVimCommand command;
    const char *const typed =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = typed_part, .length_out = &typed_length);
    EMBED_CALL(cli_vim_internal.parse, CliVimCtx, .at = args->at, .text = typed, .length = typed_length,
               .command_out = &command);
    if (command.status == CLI_VIM_PARSE_WAIT)
    {
        EMBED_CALL(cli_vim_internal.label_settled, CliVimCtx, .at = args->at);
        return;
    }
    if ((command.status == CLI_VIM_PARSE_WHOLE) && (command.name == '.'))
    {
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = typed_part, .text = "", .length = 0ULL);
        EMBED_CALL(vim_repeated, CliVimCtx, .at = args->at, .key = args->key, .err_msg = args->err_msg);
    }
    else if (command.status == CLI_VIM_PARSE_WHOLE)
    {
        EMBED_CALL(vim_run, CliVimCtx, .at = args->at, .key = args->key, .command = &command, .err_msg = args->err_msg);
    }
    // the keys typed let go once a command ran or none is to come
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = typed_part, .text = "", .length = 0ULL);
    EMBED_CALL(cli_vim_internal.label_settled, CliVimCtx, .at = args->at);
}

// the token a key's own name `text` reads as, a chord with Ctrl where `on`: 0 for a key the text typed carries or
// that does nothing, CLI_VIM_LINE_END for a key that is the view's
EMBED_INLINE unsigned int vim_token(const CliVimCtx *args)
{
    static const struct
    {
        const char *name;
        unsigned int token;
    } s_keys[] = {{"Escape", CLI_VIM_ESCAPE},
                  {"Left", 'h'},
                  {"Right", 'l'},
                  {"Up", 'k'},
                  {"Down", 'j'},
                  {"Home", '0'},
                  {"End", '$'},
                  {"Delete", 'x'},
                  {"Enter", 'j'},
                  {"Tab", 0u},
                  {"Backspace", 0u},
                  {"Space", 0u}};
    static const struct
    {
        const char *name;
        unsigned int token;
    } s_chords[] = {{"[", CLI_VIM_ESCAPE}, {"R", CLI_VIM_REDO}, {"D", CLI_VIM_DOWN}, {"U", CLI_VIM_UP}};
    if (args->on)
    {
        for (unsigned int at = 0u; at < sizeof(s_chords) / sizeof(s_chords[0]); at += 1u)
        {
            if (EMBED_CALL(vim_named, CliVimCtx, .text = args->text, .required = s_chords[at].name))
            {
                return s_chords[at].token;
            }
        }
        return CLI_VIM_LINE_END;
    }
    for (unsigned int at = 0u; at < sizeof(s_keys) / sizeof(s_keys[0]); at += 1u)
    {
        if (EMBED_CALL(vim_named, CliVimCtx, .text = args->text, .required = s_keys[at].name))
        {
            return s_keys[at].token;
        }
    }
    // a key of one letter types it, the text carrying it
    return (cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_NAME_MAX_LEN}) == 1ULL) ? 0u : CLI_VIM_LINE_END;
}

// a key while the prompt is open on the view `at`: Escape closes it, Enter closes it and searches for its text,
// Backspace takes its last letter out, and every other key does nothing
EMBED_INLINE void vim_prompt_key(const CliVimCtx *args)
{
    CliVimView *const view = &g_cli_vim_state.view[args->at];
    const unsigned int prompt_part =
        EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = args->at, .part = CLI_VIM_PART_PROMPT);
    unsigned long long length = 0ULL;
    const char *const prompt =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .part = prompt_part, .length_out = &length);
    if (EMBED_CALL(vim_named, CliVimCtx, .text = args->text, .required = "Backspace"))
    {
        unsigned long long cut = length;
        while ((cut > 0ULL) && ((((unsigned char)prompt[cut - 1ULL]) & 0xc0u) == 0x80u))
        {
            cut -= 1ULL;
        }
        cut -= (cut > 0ULL) ? 1ULL : 0ULL;
        (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = prompt_part, .text = prompt, .length = cut);
        return;
    }
    const int entered = EMBED_CALL(vim_named, CliVimCtx, .text = args->text, .required = "Enter");
    if (!entered && !EMBED_CALL(vim_named, CliVimCtx, .text = args->text, .required = "Escape"))
    {
        return;
    }
    view->prompt_open = 0;
    if (entered && (length != 0ULL))
    {
        EMBED_CALL(cli_vim_commands_internal.search, CliVimCtx, .at = args->at, .key = args->key, .text = prompt,
                   .length = length, .back = view->prompt_back, .word_text = 0);
    }
    (void)EMBED_CALL(cli_vim_internal.part_put, CliVimCtx, .part = prompt_part, .text = "", .length = 0ULL);
}

void cli_vim_begin(const CliVimCfg *args)
{
    const char *const value = (args->entries != NULL) ? cli_state_entry_value(args->entries, "vim") : NULL;
    g_cli_vim_state.on =
        (value != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = value, .other = s_setting_on, .cap = 8u});
}

void cli_vim_set(const CliVimCfg *args)
{
    if (!EMBED_CALL(vim_begun, CliVimCtx, .at = 0u))
    {
        return;
    }
    if (args->key == NULL)
    {
        g_cli_vim_state.on = args->on;
        for (unsigned int at = 0u; at < g_cli_vim_state.views; at += 1u)
        {
            EMBED_CALL(vim_view_reset, CliVimCtx, .at = at, .on = args->on);
        }
        return;
    }
    unsigned long long place = 0ULL;
    // the place is among the views or where one is put, fewer than an unsigned int counts
    if (!EMBED_CALL(vim_view_found, CliVimCtx, .key = args->key, .length_out = &place) &&
        !EMBED_CALL(vim_view_added, CliVimCtx, .key = args->key, .at = (unsigned int)place, .on = args->on))
    {
        return;
    }
    EMBED_CALL(vim_view_reset, CliVimCtx, .at = (unsigned int)place, .on = args->on);
}

void cli_vim_close(const CliVimCfg *args)
{
    unsigned long long place = 0ULL;
    if ((args->key == NULL) || (g_cli_vim_state.part == NULL) ||
        !EMBED_CALL(vim_view_found, CliVimCtx, .key = args->key, .length_out = &place))
    {
        return;
    }
    // the place is among the views, fewer than an unsigned int counts
    EMBED_CALL(vim_view_removed, CliVimCtx, .at = (unsigned int)place);
}

int cli_vim_key(const CliVimCfg *args)
{
    unsigned long long place = 0ULL;
    if (!EMBED_CALL(vim_view_on, CliVimCtx, .key = args->key, .length_out = &place))
    {
        return 0;
    }
    // the place is among the views, fewer than an unsigned int counts
    const unsigned int at = (unsigned int)place;
    CliVimView *const view = &g_cli_vim_state.view[at];
    // the modifier keys read off the key's name, Mod and Ctrl the same
    const char *name = args->name;
    int chord = 0;
    int alt = 0;
    for (int reading = 1; reading;)
    {
        if (EMBED_CALL(vim_starts, CliVimCtx, .text = name, .required = "Mod+"))
        {
            chord = 1;
            name += 4;
        }
        else if (EMBED_CALL(vim_starts, CliVimCtx, .text = name, .required = "Ctrl+"))
        {
            chord = 1;
            name += 5;
        }
        else if (EMBED_CALL(vim_starts, CliVimCtx, .text = name, .required = "Alt+"))
        {
            alt = 1;
            name += 4;
        }
        else if (EMBED_CALL(vim_starts, CliVimCtx, .text = name, .required = "Shift+"))
        {
            name += 6;
        }
        else
        {
            reading = 0;
        }
    }
    if (view->mode == CLI_VIM_INSERT)
    {
        const int leaves = !alt && ((!chord && EMBED_CALL(vim_named, CliVimCtx, .text = name, .required = "Escape")) ||
                                    (chord && EMBED_CALL(vim_named, CliVimCtx, .text = name, .required = "[")));
        if (leaves)
        {
            EMBED_CALL(cli_vim_commands_internal.insert_left, CliVimCtx, .at = at, .key = args->key);
            EMBED_CALL(cli_vim_internal.label_settled, CliVimCtx, .at = at);
            return 1;
        }
        const int edits = !chord && !alt &&
                          (EMBED_CALL(vim_named, CliVimCtx, .text = name, .required = "Enter") ||
                           EMBED_CALL(vim_named, CliVimCtx, .text = name, .required = "Backspace") ||
                           EMBED_CALL(vim_named, CliVimCtx, .text = name, .required = "Delete") ||
                           EMBED_CALL(vim_named, CliVimCtx, .text = name, .required = "Tab"));
        if (edits && view->taking && !view->replaying)
        {
            const unsigned int taking =
                EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = at, .part = CLI_VIM_PART_TAKING_INSERT);
            const char open = CLI_VIM_KEY_OPEN;
            const char close = CLI_VIM_KEY_CLOSE;
            (void)EMBED_CALL(vim_part_added, CliVimCtx, .part = taking, .text = &open, .length = 1ULL);
            (void)EMBED_CALL(vim_part_added, CliVimCtx, .part = taking, .text = name,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_NAME_MAX_LEN}));
            (void)EMBED_CALL(vim_part_added, CliVimCtx, .part = taking, .text = &close, .length = 1ULL);
        }
        return 0;
    }
    if (view->prompt_open)
    {
        EMBED_CALL(vim_prompt_key, CliVimCtx, .at = at, .key = args->key, .text = name);
        return 1;
    }
    if (alt)
    {
        return 0;
    }
    const unsigned int token = EMBED_CALL(vim_token, CliVimCtx, .text = name, .on = chord);
    if (token == CLI_VIM_LINE_END)
    {
        return 0;
    }
    if (token != 0u)
    {
        EMBED_CALL(vim_taken, CliVimCtx, .at = at, .key = args->key, .letter = token, .err_msg = args->err_msg);
    }
    return 1;
}

int cli_vim_text(const CliVimCfg *args)
{
    unsigned long long place = 0ULL;
    if (!EMBED_CALL(vim_view_on, CliVimCtx, .key = args->key, .length_out = &place))
    {
        return 0;
    }
    // the place is among the views, fewer than an unsigned int counts
    const unsigned int at = (unsigned int)place;
    CliVimView *const view = &g_cli_vim_state.view[at];
    if (view->mode == CLI_VIM_INSERT)
    {
        if (view->taking && !view->replaying)
        {
            (void)EMBED_CALL(
                vim_part_added, CliVimCtx,
                .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = at, .part = CLI_VIM_PART_TAKING_INSERT),
                .text = args->text, .length = args->length);
        }
        return 0;
    }
    const unsigned int prompt_part =
        EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = at, .part = CLI_VIM_PART_PROMPT);
    // each letter a token; what follows a letter that opens insert mode or the prompt goes into the view or the prompt
    for (unsigned long long read = 0ULL; read < args->length;)
    {
        if (view->prompt_open)
        {
            (void)EMBED_CALL(vim_part_added, CliVimCtx, .part = prompt_part, .text = args->text + read,
                             .length = args->length - read);
            break;
        }
        if (view->mode == CLI_VIM_INSERT)
        {
            const CliViewInput rest = {.kind = CLI_VIEW_TEXT, .text = args->text + read, .length = args->length - read};
            (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = args->key, .input = &rest,
                             .err_msg = args->err_msg);
            break;
        }
        unsigned long long bytes = 0ULL;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text,
                                               .size = args->length, .at = read, .length = &bytes);
        read += (bytes != 0ULL) ? bytes : 1ULL;
        EMBED_CALL(vim_taken, CliVimCtx, .at = at, .key = args->key, .letter = letter, .err_msg = args->err_msg);
    }
    return 1;
}

const CliVimShape *cli_vim_shape(const CliVimCfg *args)
{
    CliVimState *const state = &g_cli_vim_state;
    state->shape = (CliVimShape){0, CLI_VIM_NORMAL, 0, "", 0ULL, 0, 0, "", 0ULL};
    unsigned long long place = 0ULL;
    if ((args->key == NULL) || (state->part == NULL) ||
        !EMBED_CALL(vim_view_found, CliVimCtx, .key = args->key, .length_out = &place))
    {
        // a view not yet among the views takes the setting, in normal mode
        state->shape.on = (args->key != NULL) && state->on;
        state->shape.block = state->shape.on;
        state->shape.label = state->shape.on ? s_mode_label[CLI_VIM_NORMAL] : "";
        state->shape.label_length = state->shape.on ? s_mode_label_bytes[CLI_VIM_NORMAL] : 0ULL;
        return &state->shape;
    }
    // the place is among the views, fewer than an unsigned int counts
    const unsigned int at = (unsigned int)place;
    const CliVimView *const view = &state->view[at];
    if (!view->on)
    {
        return &state->shape;
    }
    state->shape.on = 1;
    state->shape.mode = view->mode;
    state->shape.block = view->mode != CLI_VIM_INSERT;
    state->shape.label =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .length_out = &state->shape.label_length,
                   .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = at, .part = CLI_VIM_PART_LABEL));
    state->shape.prompt_open = view->prompt_open;
    state->shape.prompt_back = view->prompt_back;
    state->shape.prompt =
        EMBED_CALL(cli_vim_internal.part_text, CliVimCtx, .length_out = &state->shape.prompt_length,
                   .part = EMBED_CALL(cli_vim_internal.part_of, CliVimCtx, .at = at, .part = CLI_VIM_PART_PROMPT));
    return &state->shape;
}
