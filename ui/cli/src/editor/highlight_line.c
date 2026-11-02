// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_line.c: a line colored by a grammar: its tokens as runs, each a column in UTF-16 units and the place of
// its name, running to the next run's column or the line's end, and the state the line ends in. Run on any thread with
// no lock taken, each coloring in a room of its own and the grammar saved for it until it ends
#include "../text/text.h"
#include "highlight_internal.h"

// One line's work: its letters, how many are colored, its runs, the states stacked, each stacked state's place among
// the grammar's states, found once as it is stacked, and root's place; each stacked state's indent, that of the
// statement it was entered in; the indent of the statement under way; and the runs before the last mark put, which no
// run put at a mark's column takes the place of
typedef struct
{
    unsigned long long letters;
    unsigned int runs;
    unsigned int depth;
    unsigned int root;
    unsigned int state_at[CLI_HIGHLIGHT_STACK];
    unsigned long long indent_at[CLI_HIGHLIGHT_STACK];
    unsigned long long statement;
    unsigned int marked;
} CliColoring;
// the room the coloring under way on this thread works in, the one the store handed it, that of a coloring it was
// begun within put back as it ends
static _Thread_local CliHighlightRoom *t_room;

typedef struct
{
    unsigned long long from;
    unsigned long long to;
    char *out;
    int lower;
    const CliGrammar *grammar;
    unsigned int token;
    CliColoring *coloring;
    unsigned long long at;
    unsigned int name;
    unsigned int col;
    const char *next;
    const CliColoring *coloring_read;
    int or_root;
    unsigned int rule;
    CliRegexpSpan *found;
    const CliHighlightRule *rule_highlight_rule_pointer;
    const CliRegexpSpan *found_read;
    const char *line;
    VerbaLinea *line_verba_linea_pointer;
    const char **from_letters;
    const char *state;
} HighlightLineCtx;
// the letters from `from` to `to` written in UTF-8 into `out`, sized for four bytes a letter, lower case where
// `lower` is 1: their length
EMBED_INLINE unsigned long long cli_text_get(const HighlightLineCtx *args)
{
    VerbaLinea line = {args->out, (4ull * t_room->most) + 1ull, 0ull};
    for (unsigned long long at = args->from; at < args->to; at += 1ull)
    {
        const unsigned int letter =
            args->lower ? EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = t_room->letters[at])
                        : t_room->letters[at];
        // an ASCII letter is its own byte; the rest take the UTF-8 writer
        if ((letter < 128u) && (line.at < line.capacity))
        {
            line.out[line.at] = (char)letter;
            line.at += 1ull;
        }
        else
        {
            EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &line, .letter = letter);
        }
    }
    return mmgr_line_finish(&line);
}

// The name a token takes for the matched text, from `from` to `to`, which is read only where the token picks by it
static unsigned int cli_resolve(const HighlightLineCtx *args)
{
    const CliGrammar *grammar = args->grammar;
    if (args->token == CLI_HIGHLIGHT_NONE)
    {
        return 0u;
    }
    const CliHighlightToken *const token_at = &grammar->token[args->token];
    if (token_at->kind != CLI_TOKEN_CASES)
    {
        return (token_at->kind == CLI_TOKEN_NAME) ? token_at->name : 0u;
    }
    // the matched text as UTF-8, and the key the word lists are searched with: the text itself, or lowered where the
    // grammar sets case aside
    const unsigned long long text_length = EMBED_CALL(cli_text_get, HighlightLineCtx, .from = args->from,
                                                      .to = args->to, .out = t_room->picked, .lower = 0);
    const char *const key = grammar->ignore_case ? t_room->key : t_room->picked;
    const unsigned long long key_length = grammar->ignore_case
                                              ? EMBED_CALL(cli_text_get, HighlightLineCtx, .from = args->from,
                                                           .to = args->to, .out = t_room->key, .lower = 1)
                                              : text_length;
    unsigned int fallback = CLI_HIGHLIGHT_NONE;
    for (unsigned int at = token_at->first; at < (token_at->first + token_at->count); at += 1u)
    {
        const CliHighlightCase *const entry = &grammar->cases[at];
        if (entry->fallback)
        {
            fallback = (fallback == CLI_HIGHLIGHT_NONE) ? entry->token : fallback;
            continue;
        }
        const char *const name = grammar->text + entry->key;
        int hit = 0;
        if (entry->list != CLI_HIGHLIGHT_NONE)
        {
            hit = EMBED_CALL(cli_highlight_length_internal.has_word, CliHighlightLengthCtx, .grammar = grammar,
                             .list = &grammar->words[entry->list], .word = key, .length = key_length);
        }
        else if (name[0] != '@')
        {
            // the case's text is terminated and the matched text contains no terminator, so a compare over the matched
            // text's length with the byte after it distinguishes the two in one pass
            hit = (memor.cmp(&(MemoriaCfg){.src = name, .other = t_room->picked, .bytes = text_length}) == 0) &&
                  (name[text_length] == '\0');
        }
        if (hit)
        {
            return EMBED_CALL(cli_resolve, HighlightLineCtx, .grammar = grammar, .token = grammar->cases[at].token,
                              .from = args->from, .to = args->to);
        }
    }
    return EMBED_CALL(cli_resolve, HighlightLineCtx, .grammar = grammar, .token = fallback, .from = args->from,
                      .to = args->to);
}

// a run put at letter `at` with `name`: each token a run of its own, but plain text, which joins the plain run before
// it, and the run before taken off where it starts at the same column, a mark never
EMBED_INLINE void cli_push(const HighlightLineCtx *args)
{
    const unsigned int col = t_room->cols[args->at];
    if ((args->coloring->runs != 0u) && (args->name == 0u) && (t_room->run_name[args->coloring->runs - 1u] == 0u))
    {
        return;
    }
    if ((args->coloring->runs > args->coloring->marked) && (t_room->run_col[args->coloring->runs - 1u] == col))
    {
        args->coloring->runs -= 1u;
        if ((args->coloring->runs != 0u) && (args->name == 0u) && (t_room->run_name[args->coloring->runs - 1u] == 0u))
        {
            return;
        }
    }
    t_room->run_col[args->coloring->runs] = col;
    t_room->run_name[args->coloring->runs] = args->name;
    args->coloring->runs += 1u;
}

// a mark put at the column `col` with `name`, a run of no letters that no run put after it at its column takes the
// place of
EMBED_INLINE void cli_mark(const HighlightLineCtx *args)
{
    t_room->run_col[args->coloring->runs] = args->col;
    t_room->run_name[args->coloring->runs] = args->name;
    args->coloring->runs += 1u;
    args->coloring->marked = args->coloring->runs;
}

// the state stacked changed by a next: none, `@pop` leaving the one entered last, `@popall` returning to root, or a
// state entered
EMBED_INLINE void cli_step(const HighlightLineCtx *args)
{
    const CliGrammar *grammar = args->grammar;
    if ((args->next == NULL) || (args->next[0] == '\0'))
    {
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->next, .other = "@pop", .cap = CLI_CELLS_POOL}))
    {
        args->coloring->depth -= (args->coloring->depth > 1u) ? 1u : 0u;
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->next, .other = "@popall", .cap = CLI_CELLS_POOL}))
    {
        args->coloring->depth = (args->coloring->depth > 1u) ? 1u : args->coloring->depth;
        return;
    }
    // `@pop@<state>` leaves the state entered last and enters `<state>` in its place
    static const char s_switch[] = "@pop@";
    const int switching =
        (cellul.len(&(CatenaFinitaCfg){.src = args->next, .cap = CLI_CELLS_POOL}) >= (sizeof(s_switch) - 1u)) &&
        (memor.cmp(&(MemoriaCfg){.src = args->next, .other = s_switch, .bytes = sizeof(s_switch) - 1u}) == 0);
    args->coloring->depth -= (switching && (args->coloring->depth > 1u)) ? 1u : 0u;
    const char *name = switching ? (args->next + sizeof(s_switch) - 1u) : args->next;
    name += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = name, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .set = "@");
    if (args->coloring->depth < CLI_HIGHLIGHT_STACK)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        memor.cpy(&(MemoriaCfg){.dst = t_room->stack[args->coloring->depth], .src = name, .bytes = length + 1u});
        args->coloring->state_at[args->coloring->depth] = EMBED_CALL(
            cli_highlight_name_internal.state, CliHighlightNameCtx, .grammar = grammar, .name = name, .length = length);
        args->coloring->indent_at[args->coloring->depth] = args->coloring->statement;
        args->coloring->depth += 1u;
    }
}

// the place among the names of the name the close of the state on top takes, none where it is none of the states of
// the grammar's `indent`
EMBED_INLINE unsigned int cli_top_close(const HighlightLineCtx *args)
{
    const unsigned int state = (args->coloring_read->depth != 0u)
                                   ? args->coloring_read->state_at[args->coloring_read->depth - 1u]
                                   : CLI_HIGHLIGHT_NONE;
    return (state != CLI_HIGHLIGHT_NONE) ? args->grammar->state[state].close : CLI_HIGHLIGHT_NONE;
}

// each state of the grammar's `indent` on top of the stack left, a mark of its close put at the column `col` for each
EMBED_INLINE void cli_close(const HighlightLineCtx *args)
{
    while ((args->coloring->depth > 1u) && (EMBED_CALL(cli_top_close, HighlightLineCtx, .grammar = args->grammar,
                                                       .coloring_read = args->coloring) != CLI_HIGHLIGHT_NONE))
    {
        EMBED_CALL(cli_mark, HighlightLineCtx, .coloring = args->coloring, .col = args->col,
                   .name = EMBED_CALL(cli_top_close, HighlightLineCtx, .grammar = args->grammar,
                                      .coloring_read = args->coloring));
        args->coloring->depth -= 1u;
    }
}

// the rules of the state on top, its place found as it was stacked; root's where the grammar has no such state, none
// where it has neither
EMBED_INLINE const CliHighlightList *cli_rules(const HighlightLineCtx *args)
{
    unsigned int state = (args->coloring_read->depth != 0u)
                             ? args->coloring_read->state_at[args->coloring_read->depth - 1u]
                             : args->coloring_read->root;
    state = ((state == CLI_HIGHLIGHT_NONE) && args->or_root) ? args->coloring_read->root : state;
    return (state != CLI_HIGHLIGHT_NONE) ? &args->grammar->state[state] : NULL;
}

// a rule's pattern tried at `at`: 1 with the spans of the match in `found`, or 0
EMBED_INLINE int cli_try(const HighlightLineCtx *args)
{
    const CliGrammar *grammar = args->grammar;
    unsigned int rule = args->rule;
    unsigned long long at = args->at;
    CliRegexp pattern;
    EMBED_CALL(cli_highlight_internal.pattern, CliHighlightCtx, .grammar = grammar, .rule = &grammar->rule[rule],
               .pattern = &pattern);
    return EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &pattern,
                      .text_u32s = t_room->letters, .length = args->coloring_read->letters, .at = at,
                      .groups = args->found);
}

// the match of a rule taken at `at`: each group's token put where its group stands, or the whole match's token; the
// letters it took
EMBED_INLINE unsigned long long cli_on_take(const HighlightLineCtx *args)
{
    const unsigned long long start = args->found_read[0].set ? args->found_read[0].from : args->at;
    const unsigned long long end = args->found_read[0].set ? args->found_read[0].to : args->at;
    const unsigned long long size = end - start;
    const CliHighlightToken *const token = (args->rule_highlight_rule_pointer->token != CLI_HIGHLIGHT_NONE)
                                               ? &args->grammar->token[args->rule_highlight_rule_pointer->token]
                                               : NULL;
    if ((token != NULL) && (token->kind == CLI_TOKEN_GROUPS))
    {
        unsigned long long place = args->at;
        for (unsigned int index = 0u; index < token->count; index += 1u)
        {
            const int matched =
                ((index + 1u) <= args->rule_highlight_rule_pointer->groups) && args->found_read[index + 1u].set;
            const unsigned long long part =
                matched ? (args->found_read[index + 1u].to - args->found_read[index + 1u].from) : 0ull;
            if (part > 0ull)
            {
                const unsigned long long put = (place < args->coloring->letters) ? place : args->coloring->letters;
                EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = put,
                           .name = EMBED_CALL(cli_resolve, HighlightLineCtx, .grammar = args->grammar,
                                              .token = args->grammar->cases[token->first + index].token,
                                              .from = args->found_read[index + 1u].from,
                                              .to = args->found_read[index + 1u].to));
            }
            place += part;
        }
    }
    else if ((token != NULL) && (size > 0ull))
    {
        EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = args->at,
                   .name = EMBED_CALL(cli_resolve, HighlightLineCtx, .grammar = args->grammar,
                                      .token = args->rule_highlight_rule_pointer->token, .from = start, .to = end));
    }
    return size;
}

// a line written in a grammar with an `indent`, where its `skip` does not match the line: each state of its `states` on
// top of the stack whose indent is as deep as the line's or deeper left, a mark of its close put at the line's start,
// and the line's indent made the statement's where the state on top is one of its `begins`. A line's indent counts a
// space one column and a tab to the next multiple of the grammar's `tab`
EMBED_INLINE void cli_line_indent(const HighlightLineCtx *args)
{
    CliRegexpSpan found[CLI_REGEXP_GROUPS + 1u];
    if ((args->grammar->skip != CLI_HIGHLIGHT_NONE) &&
        EMBED_CALL(cli_try, HighlightLineCtx, .grammar = args->grammar, .rule = args->grammar->skip,
                   .coloring_read = args->coloring, .at = 0ull, .found = found) &&
        found[0].set)
    {
        return;
    }
    unsigned long long indent = 0ull;
    for (unsigned long long at = 0ull;
         (at < args->coloring->letters) && ((t_room->letters[at] == ' ') || (t_room->letters[at] == '\t')); at += 1ull)
    {
        indent = (t_room->letters[at] == '\t') ? (((indent / args->grammar->tab) + 1ull) * args->grammar->tab)
                                               : (indent + 1ull);
    }
    while ((args->coloring->depth > 1u) &&
           (EMBED_CALL(cli_top_close, HighlightLineCtx, .grammar = args->grammar, .coloring_read = args->coloring) !=
            CLI_HIGHLIGHT_NONE) &&
           (indent <= args->coloring->indent_at[args->coloring->depth - 1u]))
    {
        EMBED_CALL(cli_mark, HighlightLineCtx, .coloring = args->coloring, .col = 0u,
                   .name = EMBED_CALL(cli_top_close, HighlightLineCtx, .grammar = args->grammar,
                                      .coloring_read = args->coloring));
        args->coloring->depth -= 1u;
    }
    const unsigned int top =
        (args->coloring->depth != 0u) ? args->coloring->state_at[args->coloring->depth - 1u] : CLI_HIGHLIGHT_NONE;
    if ((top != CLI_HIGHLIGHT_NONE) && args->grammar->state[top].begins)
    {
        args->coloring->statement = indent;
    }
}

// One line colored: its runs and the stack it ends with in the coloring
static void cli_line(const HighlightLineCtx *args)
{
    args->coloring->letters =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->line,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->line, .cap = CLI_CELLS_POOL}),
                   .letters = t_room->letters, .room_u64 = t_room->most);
    args->coloring->runs = 0u;
    args->coloring->marked = 0u;
    unsigned int col = 0u;
    for (unsigned long long at = 0ull; at < args->coloring->letters; at += 1ull)
    {
        t_room->cols[at] = col;
        col += (t_room->letters[at] >= 0x10000u) ? 2u : 1u;
    }
    t_room->cols[args->coloring->letters] = col;
    // the line colored whole, left to right, so the state it ends in is the one its last letter leaves
    const unsigned long long length = args->coloring->letters;
    if (args->grammar->indents)
    {
        cli_line_indent(args);
    }
    CliRegexpSpan found[CLI_REGEXP_GROUPS + 1u];
    const CliHighlightList *rules = EMBED_CALL(cli_rules, HighlightLineCtx, .grammar = args->grammar,
                                               .coloring_read = args->coloring, .or_root = 1);
    const unsigned int *const letters = t_room->letters;
    unsigned long long at = 0ull;
    unsigned int stalled = 0u;
    while (at < length)
    {
        // the letters no rule of the state can start at are plain, taken in one stride, the state's table read once a
        // letter and no rule tried at any of them; where a rule starts at a letter outside ASCII, or takes nothing,
        // every letter is tried
        if ((rules != NULL) && !rules->starts.empty && !rules->starts.other)
        {
            const unsigned char *const starts_at = rules->starts_at;
            unsigned long long stride = at;
            while ((stride < length) && (letters[stride] < 128u) && (starts_at[letters[stride]] == 0u))
            {
                stride += 1ull;
            }
            if (stride > at)
            {
                EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = at, .name = 0u);
                at = stride;
                stalled = 0u;
                continue;
            }
        }
        int taken = 0;
        for (unsigned int each = 0u; (rules != NULL) && !taken && (each < rules->count); each += 1u)
        {
            const unsigned int index = args->grammar->listed[rules->first + each];
            const CliHighlightRule *const rule = &args->grammar->rule[index];
            // a rule that cannot start at this letter is passed over before its pattern is made
            if ((rule->anchored && (at != 0ull)) ||
                (!rule->first.empty &&
                 !EMBED_CALL(cli_regexp.allows, CliRegexpCfg, .first = &rule->first, .letter = letters[at])))
            {
                continue;
            }
            if (!EMBED_CALL(cli_try, HighlightLineCtx, .grammar = args->grammar, .rule = index,
                            .coloring_read = args->coloring, .at = at, .found = found))
            {
                continue;
            }
            const unsigned long long size = found[0].set ? (found[0].to - found[0].from) : 0ull;
            if ((size == 0ull) && ((rule->next == CLI_HIGHLIGHT_NONE) || (stalled > 2u)))
            {
                continue;
            }
            EMBED_CALL(cli_on_take, HighlightLineCtx, .grammar = args->grammar, .rule_highlight_rule_pointer = rule,
                       .coloring = args->coloring, .at = at, .found_read = found);
            stalled = (size > 0ull) ? 0u : (stalled + 1u);
            at += size;
            if (rule->next != CLI_HIGHLIGHT_NONE)
            {
                EMBED_CALL(cli_step, HighlightLineCtx, .grammar = args->grammar, .coloring = args->coloring,
                           .next = args->grammar->text + rule->next);
                rules = EMBED_CALL(cli_rules, HighlightLineCtx, .grammar = args->grammar,
                                   .coloring_read = args->coloring, .or_root = 1);
            }
            taken = 1;
        }
        if (!taken)
        {
            EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = at, .name = 0u);
            at += 1ull;
            stalled = 0u;
        }
    }
    // A rule that matches nothing at the line's end and changes the state or names a token still applies there, once:
    // it is how a state that lasts to the end of its line ends, and a mark at a line's end is put
    const CliHighlightList *const ending = EMBED_CALL(cli_rules, HighlightLineCtx, .grammar = args->grammar,
                                                      .coloring_read = args->coloring, .or_root = 0);
    for (unsigned int each = 0u; (ending != NULL) && (each < ending->count); each += 1u)
    {
        const unsigned int index = args->grammar->listed[ending->first + each];
        const CliHighlightRule *const rule = &args->grammar->rule[index];
        const unsigned int name =
            EMBED_CALL(cli_resolve, HighlightLineCtx, .grammar = args->grammar, .token = rule->token,
                       .from = args->coloring->letters, .to = args->coloring->letters);
        if (((rule->next != CLI_HIGHLIGHT_NONE) || (name != 0u)) &&
            EMBED_CALL(cli_try, HighlightLineCtx, .grammar = args->grammar, .rule = index,
                       .coloring_read = args->coloring, .at = args->coloring->letters, .found = found) &&
            found[0].set && (found[0].from == found[0].to))
        {
            if (name != 0u)
            {
                EMBED_CALL(cli_mark, HighlightLineCtx, .coloring = args->coloring,
                           .col = t_room->cols[args->coloring->letters], .name = name);
            }
            if (rule->next != CLI_HIGHLIGHT_NONE)
            {
                EMBED_CALL(cli_step, HighlightLineCtx, .grammar = args->grammar, .coloring = args->coloring,
                           .next = args->grammar->text + rule->next);
            }
            break;
        }
    }
    if (args->coloring->runs == 0u)
    {
        EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = 0u, .name = 0u);
    }
}

// the states stacked written joined by `/`, or root where the grammar starts every line in root; in a grammar with an
// `indent`, after the statement's indent and `|`, each state of its `states` followed by `:` and its indent
EMBED_INLINE void cli_state_write(const HighlightLineCtx *args)
{
    if (args->grammar->per_line)
    {
        mmgr_line_put(args->line_verba_linea_pointer, "root");
        return;
    }
    if (args->grammar->indents)
    {
        mmgr_line_decimal(args->line_verba_linea_pointer, args->coloring_read->statement, 1u);
        mmgr_line_ch(args->line_verba_linea_pointer, '|');
    }
    for (unsigned int at = 0u; at < args->coloring_read->depth; at += 1u)
    {
        mmgr_line_put(args->line_verba_linea_pointer, (at != 0u) ? "/" : "");
        mmgr_line_put(args->line_verba_linea_pointer, t_room->stack[at]);
        const unsigned int state = args->coloring_read->state_at[at];
        if ((state != CLI_HIGHLIGHT_NONE) && (args->grammar->state[state].close != CLI_HIGHLIGHT_NONE))
        {
            mmgr_line_ch(args->line_verba_linea_pointer, ':');
            mmgr_line_decimal(args->line_verba_linea_pointer, args->coloring_read->indent_at[at], 1u);
        }
    }
}

// the number the digits from `*from` write, `*from` moved past them
EMBED_INLINE unsigned long long cli_digits_read(const HighlightLineCtx *args)
{
    const char **from = args->from_letters;
    unsigned long long value = 0ull;
    unsigned long long end = 0ull;
    value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = *from, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = *from, .cap = CLI_CELLS_POOL}), .end = &end);
    *from += end;
    return value;
}

// a coloring begun with the state given stacked, each of its names between `/` one state, an indent after `:` its
// indent, and an indent before `|` the statement's
static void cli_stack(const HighlightLineCtx *args)
{
    const CliGrammar *grammar = args->grammar;
    args->coloring->letters = 0ull;
    args->coloring->runs = 0u;
    args->coloring->marked = 0u;
    args->coloring->depth = 0u;
    args->coloring->statement = 0ull;
    args->coloring->root = EMBED_CALL(cli_highlight_name_internal.state, CliHighlightNameCtx, .grammar = grammar,
                                      .name = "root", .length = 4u);
    const char *from = args->state;
    const char *const bar = cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL, .byte = '|'});
    if (bar != NULL)
    {
        args->coloring->statement = EMBED_CALL(cli_digits_read, HighlightLineCtx, .from_letters = &from);
        from = bar + 1;
    }
    for (;;)
    {
        const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL, .byte = '/'});
        const unsigned long long whole = (slash != NULL)
                                             ? (unsigned long long)(slash - from)
                                             : cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
        // the name to its `:`, where one stands within it
        unsigned long long length = 0ull;
        length = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = from, .at = length,
                            .size = whole, .set = ":");
        if (args->coloring->depth < CLI_HIGHLIGHT_STACK)
        {
            VerbaLinea name = {t_room->stack[args->coloring->depth], CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put_n(&name, from, length);
            mmgr_line_finish(&name);
            args->coloring->state_at[args->coloring->depth] =
                EMBED_CALL(cli_highlight_name_internal.state, CliHighlightNameCtx, .grammar = grammar, .name = from,
                           .length = length);
            const char *indent = from + length + ((length < whole) ? 1u : 0u);
            args->coloring->indent_at[args->coloring->depth] =
                (length < whole) ? EMBED_CALL(cli_digits_read, HighlightLineCtx, .from_letters = &indent) : 0ull;
            args->coloring->depth += 1u;
        }
        if (slash == NULL)
        {
            break;
        }
        from = slash + 1;
    }
}

// a grammar that starts every line in root ends each line there, and the next starts where it ended
EMBED_INLINE void cli_root(const HighlightLineCtx *args)
{
    memor.cpy(&(MemoriaCfg){.dst = t_room->stack[0],
                            .src = "root",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "root", .cap = CLI_CELLS_POOL}) + 1u});
    args->coloring->state_at[0] = args->coloring->root;
    args->coloring->depth = 1u;
}

void cli_highlight_leaves_get(const CliHighlightRoomStateCtx *args)
{
    const char *state = args->state;
    VerbaLinea *ended = args->ended;
    CliHighlightRoom *const before = t_room;
    t_room = args->room;
    CliColoring coloring;
    EMBED_CALL(cli_stack, HighlightLineCtx, .grammar = args->grammar, .coloring = &coloring, .state = state);
    for (unsigned int line = 0u; line < args->count; line += 1u)
    {
        if (args->grammar->per_line && (line != 0u))
        {
            EMBED_CALL(cli_root, HighlightLineCtx, .coloring = &coloring);
        }
        EMBED_CALL(cli_line, HighlightLineCtx, .grammar = args->grammar, .line = args->lines[line],
                   .coloring = &coloring);
        // the text's end, a line of no indent, leaves every state of the grammar's `indent`, a mark at the last line's
        // end for each
        if (args->ends && ((line + 1u) == args->count))
        {
            EMBED_CALL(cli_close, HighlightLineCtx, .grammar = args->grammar, .coloring = &coloring,
                       .col = t_room->cols[coloring.letters]);
        }
        char state[CLI_HIGHLIGHT_STACK * 4u];
        VerbaLinea state_line = {state, sizeof(state), 0ull};
        EMBED_CALL(cli_state_write, HighlightLineCtx, .grammar = args->grammar, .coloring_read = &coloring,
                   .line_verba_linea_pointer = &state_line);
        // a state longer than its room is read as root, as a request's last line's is
        const char *const ended = (mmgr_line_finish(&state_line) != 0ull) ? state : "root";
        const CliLeaves leaves = {
            args->lines[line],   coloring.runs,       t_room->run_col, t_room->run_name,
            args->grammar->name, args->grammar->text, ended,           t_room->cols[coloring.letters]};
        EMBED_CALL(args->each, CliLeavesEachCfg, .context = args->context, .leaves = &leaves);
    }
    EMBED_CALL(cli_state_write, HighlightLineCtx, .grammar = args->grammar, .coloring_read = &coloring,
               .line_verba_linea_pointer = ended);
    t_room = before;
}

void cli_highlight_lines(const CliHighlightRoomCtx *args)
{
    CliHighlightRoom *const before = t_room;
    t_room = args->room;
    CliColoring coloring;
    EMBED_CALL(cli_stack, HighlightLineCtx, .grammar = args->grammar, .coloring = &coloring, .state = args->state);
    mmgr_line_put(args->out, "{\"names\":[");
    for (unsigned int at = 0u; at < args->grammar->names; at += 1u)
    {
        const char *const name = args->grammar->text + args->grammar->name[at];
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(args->out, "],\"runs\":[");
    // the state each line ends in, as JSON, among the room's endings
    VerbaLinea endings = {t_room->endings, t_room->endings_size, 0ull};
    for (unsigned int each = 0u; each < args->count; each += 1u)
    {
        if (args->grammar->per_line && (each != 0u))
        {
            EMBED_CALL(cli_root, HighlightLineCtx, .coloring = &coloring);
        }
        EMBED_CALL(cli_line, HighlightLineCtx, .grammar = args->grammar, .line = args->lines[each],
                   .coloring = &coloring);
        mmgr_line_put(args->out, (each != 0u) ? ",[" : "[");
        for (unsigned int run = 0u; run < coloring.runs; run += 1u)
        {
            mmgr_line_put(args->out, (run != 0u) ? ",[" : "[");
            mmgr_line_decimal(args->out, t_room->run_col[run], 1u);
            mmgr_line_ch(args->out, ',');
            mmgr_line_decimal(args->out, t_room->run_name[run], 1u);
            mmgr_line_ch(args->out, ']');
        }
        mmgr_line_ch(args->out, ']');
        // each state put as JSON among the endings, a comma before each but the first
        char ended[CLI_HIGHLIGHT_STACK * 4u];
        VerbaLinea state_line = {ended, sizeof(ended), 0ull};
        EMBED_CALL(cli_state_write, HighlightLineCtx, .grammar = args->grammar, .coloring_read = &coloring,
                   .line_verba_linea_pointer = &state_line);
        const unsigned long long length = mmgr_line_ok(&state_line) ? state_line.at : 0ull;
        mmgr_line_put(&endings, (each != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &endings, .text = ended,
                   .length = length);
    }
    mmgr_line_put(args->out, "],\"states\":[");
    mmgr_line_put_n(args->out, t_room->endings, mmgr_line_ok(&endings) ? endings.at : 0ull);
    mmgr_line_put(args->out, "]}");
    t_room = before;
}
