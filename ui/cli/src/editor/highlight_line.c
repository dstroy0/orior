// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_line.c: a line colored by a grammar: its tokens as runs, each a column in UTF-16 units and the place of
// its name, running to the next run's column or the line's end, and the state the line ends in. Run on any thread with
// no lock taken, each coloring in a work state of its own and the grammar saved for it until it ends
#include "../text/text.h"
#include "highlight_internal.h"

// One line's work: its characters, how many are colored, its runs, the states stacked, each stacked state's place among
// the grammar's states, found once as it is stacked, and root's place; each stacked state's indent, that of the
// statement it was entered in; the indent of the statement under way; and the runs before the last mark put, which no
// run put at a mark's column takes the place of
typedef struct
{
    unsigned long long characters;
    unsigned int runs;
    unsigned int depth;
    unsigned int root;
    unsigned int state_at[CLI_HIGHLIGHT_STACK];
    unsigned long long indent_at[CLI_HIGHLIGHT_STACK];
    unsigned long long statement;
    unsigned int marked;
} CliColoring;
// the work state the coloring under way on this thread works in, the one the store passed it, that of a coloring it was
// begun within put back as it ends
static _Thread_local CliHighlightWorkState *t_work_state;

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
    const char **from_characters;
    const char *state;
    const CliHighlightList *rules;
    const CliHighlightList **rules_pointer;
    unsigned long long *at_pointer;
    unsigned int *stalled_pointer;
} HighlightLineCtx;
// the characters from `from` to `to` written in UTF-8 into `out`, sized for four bytes a character, lower case where
// `lower` is 1: their length
EMBED_INLINE unsigned long long cli_text_get(const HighlightLineCtx *args)
{
    VerbaLinea line = {args->out, (4ULL * t_work_state->max) + 1ULL, 0ULL};
    for (unsigned long long at = args->from; at < args->to; at += 1ULL)
    {
        const unsigned int character =
            args->lower ? EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = t_work_state->characters[at])
                        : t_work_state->characters[at];
        // an ASCII character is its own byte; the rest take the UTF-8 writer
        if ((character < 128u) && (line.at < line.capacity))
        {
            line.out[line.at] = (char)character;
            line.at += 1ULL;
        }
        else
        {
            EMBED_CALL(cli_line_character_internal.utf8_append, CliLineLetterCtx, .line = &line, .character = character);
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
    if (token_at->type != CLI_TOKEN_CASES)
    {
        return (token_at->type == CLI_TOKEN_NAME) ? token_at->name : 0u;
    }
    // the matched text as UTF-8, and the key the word lists are searched with: the text itself, or lowered where the
    // grammar sets case aside
    const unsigned long long text_length = EMBED_CALL(cli_text_get, HighlightLineCtx, .from = args->from,
                                                      .to = args->to, .out = t_work_state->picked, .lower = 0);
    const char *const key = grammar->ignore_case ? t_work_state->key : t_work_state->picked;
    const unsigned long long key_length = grammar->ignore_case
                                              ? EMBED_CALL(cli_text_get, HighlightLineCtx, .from = args->from,
                                                           .to = args->to, .out = t_work_state->key, .lower = 1)
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
                             .list = &grammar->words[entry->list], .word_text = key, .length = key_length);
        }
        else if (name[0] != '@')
        {
            // the case's text is terminated and the matched text contains no terminator, so a compare over the matched
            // text's length with the byte after it distinguishes the two in one pass
            hit = (memor.cmp(&(MemoriaCfg){.src = name, .other = t_work_state->picked, .bytes = text_length}) == 0) &&
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

// a run put at character `at` with `name`: each token a run of its own, but plain text, which joins the plain run before
// it, and the run before taken off where it starts at the same column, a mark never
EMBED_INLINE void cli_push(const HighlightLineCtx *args)
{
    const unsigned int col = t_work_state->cols[args->at];
    if ((args->coloring->runs != 0u) && (args->name == 0u) && (t_work_state->run_name[args->coloring->runs - 1u] == 0u))
    {
        return;
    }
    if ((args->coloring->runs > args->coloring->marked) && (t_work_state->run_col[args->coloring->runs - 1u] == col))
    {
        args->coloring->runs -= 1u;
        if ((args->coloring->runs != 0u) && (args->name == 0u) &&
            (t_work_state->run_name[args->coloring->runs - 1u] == 0u))
        {
            return;
        }
    }
    t_work_state->run_col[args->coloring->runs] = col;
    t_work_state->run_name[args->coloring->runs] = args->name;
    args->coloring->runs += 1u;
}

// a mark put at the column `col` with `name`, a run of no characters that no run put after it at its column takes the
// place of
EMBED_INLINE void cli_mark(const HighlightLineCtx *args)
{
    t_work_state->run_col[args->coloring->runs] = args->col;
    t_work_state->run_name[args->coloring->runs] = args->name;
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
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->next, .other = "@pop", .cap = CLI_POOL_BYTES}))
    {
        args->coloring->depth -= (args->coloring->depth > 1u) ? 1u : 0u;
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->next, .other = "@popall", .cap = CLI_POOL_BYTES}))
    {
        args->coloring->depth = (args->coloring->depth > 1u) ? 1u : args->coloring->depth;
        return;
    }
    // `@pop@<state>` leaves the state entered last and enters `<state>` in its place
    static const char s_switch[] = "@pop@";
    const int switching =
        (cellul.len(&(CatenaFinitaCfg){.src = args->next, .cap = CLI_POOL_BYTES}) >= (sizeof(s_switch) - 1u)) &&
        (memor.cmp(&(MemoriaCfg){.src = args->next, .other = s_switch, .bytes = sizeof(s_switch) - 1u}) == 0);
    args->coloring->depth -= (switching && (args->coloring->depth > 1u)) ? 1u : 0u;
    const char *name = switching ? (args->next + sizeof(s_switch) - 1u) : args->next;
    name += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = name, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .set = "@");
    if (args->coloring->depth < CLI_HIGHLIGHT_STACK)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES});
        memor.cpy(&(MemoriaCfg){.dst = t_work_state->stack[args->coloring->depth], .src = name, .bytes = length + 1u});
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
                      .text_u32s = t_work_state->characters, .length = args->coloring_read->characters, .at = at,
                      .groups = args->found);
}

// the match of a rule taken at `at`: each group's token put where its group is, or the whole match's token; the
// characters it took
EMBED_INLINE unsigned long long cli_on_alloc(const HighlightLineCtx *args)
{
    const unsigned long long start = args->found_read[0].set ? args->found_read[0].from : args->at;
    const unsigned long long end = args->found_read[0].set ? args->found_read[0].to : args->at;
    const unsigned long long size = end - start;
    const CliHighlightToken *const token = (args->rule_highlight_rule_pointer->token != CLI_HIGHLIGHT_NONE)
                                               ? &args->grammar->token[args->rule_highlight_rule_pointer->token]
                                               : NULL;
    if ((token != NULL) && (token->type == CLI_TOKEN_GROUPS))
    {
        unsigned long long place = args->at;
        for (unsigned int index = 0u; index < token->count; index += 1u)
        {
            const int matched =
                ((index + 1u) <= args->rule_highlight_rule_pointer->groups) && args->found_read[index + 1u].set;
            const unsigned long long part =
                matched ? (args->found_read[index + 1u].to - args->found_read[index + 1u].from) : 0ULL;
            if (part > 0ULL)
            {
                const unsigned long long put = (place < args->coloring->characters) ? place : args->coloring->characters;
                EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = put,
                           .name = EMBED_CALL(cli_resolve, HighlightLineCtx, .grammar = args->grammar,
                                              .token = args->grammar->cases[token->first + index].token,
                                              .from = args->found_read[index + 1u].from,
                                              .to = args->found_read[index + 1u].to));
            }
            place += part;
        }
    }
    else if ((token != NULL) && (size > 0ULL))
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
                   .coloring_read = args->coloring, .at = 0ULL, .found = found) &&
        found[0].set)
    {
        return;
    }
    unsigned long long indent = 0ULL;
    for (unsigned long long at = 0ULL;
         (at < args->coloring->characters) && ((t_work_state->characters[at] == ' ') || (t_work_state->characters[at] == '\t'));
         at += 1ULL)
    {
        indent = (t_work_state->characters[at] == '\t') ? (((indent / args->grammar->tab) + 1ULL) * args->grammar->tab)
                                                     : (indent + 1ULL);
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

// the end of the run of plain characters from `at` to before `to` that no rule of `rules` can start at, each an ASCII
// character the state's table names no rule for: `at` where none is plain there, or where a rule starts at a character
// outside ASCII or takes nothing
EMBED_INLINE unsigned long long cli_plain_stride(const HighlightLineCtx *args)
{
    const CliHighlightList *const rules = args->rules;
    if ((rules == NULL) || rules->starts.empty || rules->starts.other)
    {
        return args->at;
    }
    const unsigned int *const characters = t_work_state->characters;
    const unsigned char *const starts_at = rules->starts_at;
    unsigned long long stride = args->at;
    while ((stride < args->to) && (characters[stride] < 128u) && (starts_at[characters[stride]] == 0u))
    {
        stride += 1ULL;
    }
    return stride;
}

// the first rule of `*rules_pointer` that matches at `*at_pointer` taken: its tokens put, `*at_pointer` moved past
// what it matched, and the state it steps to entered, its rules into `*rules_pointer`; a rule that cannot start at the
// character is passed over before its pattern is made, and one that matches nothing is taken only where it steps and the
// line has not stalled on such rules. Whether one is taken
EMBED_INLINE int cli_rule_taken(const HighlightLineCtx *args)
{
    const CliHighlightList *const rules = *args->rules_pointer;
    const unsigned int *const characters = t_work_state->characters;
    const unsigned long long at = *args->at_pointer;
    CliRegexpSpan *const found = args->found;
    for (unsigned int each = 0u; (rules != NULL) && (each < rules->count); each += 1u)
    {
        const unsigned int index = args->grammar->list_entries[rules->first + each];
        const CliHighlightRule *const rule = &args->grammar->rule[index];
        if ((rule->anchored && (at != 0ULL)) ||
            (!rule->first.empty &&
             !EMBED_CALL(cli_regexp.allows, CliRegexpCfg, .first = &rule->first, .character = characters[at])) ||
            !EMBED_CALL(cli_try, HighlightLineCtx, .grammar = args->grammar, .rule = index,
                        .coloring_read = args->coloring, .at = at, .found = found))
        {
            continue;
        }
        const unsigned long long size = found[0].set ? (found[0].to - found[0].from) : 0ULL;
        if ((size == 0ULL) && ((rule->next == CLI_HIGHLIGHT_NONE) || (*args->stalled_pointer > 2u)))
        {
            continue;
        }
        EMBED_CALL(cli_on_alloc, HighlightLineCtx, .grammar = args->grammar, .rule_highlight_rule_pointer = rule,
                   .coloring = args->coloring, .at = at, .found_read = found);
        *args->stalled_pointer = (size > 0ULL) ? 0u : (*args->stalled_pointer + 1u);
        *args->at_pointer = at + size;
        if (rule->next != CLI_HIGHLIGHT_NONE)
        {
            EMBED_CALL(cli_step, HighlightLineCtx, .grammar = args->grammar, .coloring = args->coloring,
                       .next = args->grammar->text + rule->next);
            *args->rules_pointer = EMBED_CALL(cli_rules, HighlightLineCtx, .grammar = args->grammar,
                                              .coloring_read = args->coloring, .or_root = 1);
        }
        return 1;
    }
    return 0;
}

// a rule of the state the line ends in that matches nothing at its end and changes the state or names a token applied
// there, once: it is how a state that lasts to the end of its line ends, and a mark at a line's end is put
EMBED_INLINE void cli_line_ending(const HighlightLineCtx *args)
{
    CliRegexpSpan *const found = args->found;
    const CliHighlightList *const ending = EMBED_CALL(cli_rules, HighlightLineCtx, .grammar = args->grammar,
                                                      .coloring_read = args->coloring, .or_root = 0);
    for (unsigned int each = 0u; (ending != NULL) && (each < ending->count); each += 1u)
    {
        const unsigned int index = args->grammar->list_entries[ending->first + each];
        const CliHighlightRule *const rule = &args->grammar->rule[index];
        const unsigned int name =
            EMBED_CALL(cli_resolve, HighlightLineCtx, .grammar = args->grammar, .token = rule->token,
                       .from = args->coloring->characters, .to = args->coloring->characters);
        if (((rule->next == CLI_HIGHLIGHT_NONE) && (name == 0u)) ||
            !EMBED_CALL(cli_try, HighlightLineCtx, .grammar = args->grammar, .rule = index,
                        .coloring_read = args->coloring, .at = args->coloring->characters, .found = found) ||
            !found[0].set || (found[0].from != found[0].to))
        {
            continue;
        }
        if (name != 0u)
        {
            EMBED_CALL(cli_mark, HighlightLineCtx, .coloring = args->coloring,
                       .col = t_work_state->cols[args->coloring->characters], .name = name);
        }
        if (rule->next != CLI_HIGHLIGHT_NONE)
        {
            EMBED_CALL(cli_step, HighlightLineCtx, .grammar = args->grammar, .coloring = args->coloring,
                       .next = args->grammar->text + rule->next);
        }
        return;
    }
}

// One line colored: its runs and the stack it ends with in the coloring
static void cli_line(const HighlightLineCtx *args)
{
    args->coloring->characters =
        EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = args->line,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->line, .cap = CLI_POOL_BYTES}),
                   .characters = t_work_state->characters, .capacity_u64 = t_work_state->max);
    args->coloring->runs = 0u;
    args->coloring->marked = 0u;
    unsigned int col = 0u;
    for (unsigned long long at = 0ULL; at < args->coloring->characters; at += 1ULL)
    {
        t_work_state->cols[at] = col;
        col += (t_work_state->characters[at] >= 0x10000u) ? 2u : 1u;
    }
    t_work_state->cols[args->coloring->characters] = col;
    // the line colored whole, left to right, so the state it ends in is the one its last character leaves
    const unsigned long long length = args->coloring->characters;
    if (args->grammar->indents)
    {
        cli_line_indent(args);
    }
    CliRegexpSpan found[CLI_REGEXP_GROUPS + 1u];
    const CliHighlightList *rules = EMBED_CALL(cli_rules, HighlightLineCtx, .grammar = args->grammar,
                                               .coloring_read = args->coloring, .or_root = 1);
    unsigned long long at = 0ULL;
    unsigned int stalled = 0u;
    while (at < length)
    {
        // the characters no rule of the state can start at are plain, taken in one stride, the state's table read once a
        // character and no rule tried at any of them; where a rule starts at a character outside ASCII, or takes nothing,
        // every character is tried
        const unsigned long long stride =
            EMBED_CALL(cli_plain_stride, HighlightLineCtx, .rules = rules, .at = at, .to = length);
        if (stride > at)
        {
            EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = at, .name = 0u);
            at = stride;
            stalled = 0u;
        }
        else if (!EMBED_CALL(cli_rule_taken, HighlightLineCtx, .grammar = args->grammar, .coloring = args->coloring,
                             .rules_pointer = &rules, .at_pointer = &at, .stalled_pointer = &stalled, .found = found))
        {
            EMBED_CALL(cli_push, HighlightLineCtx, .coloring = args->coloring, .at = at, .name = 0u);
            at += 1ULL;
            stalled = 0u;
        }
    }
    EMBED_CALL(cli_line_ending, HighlightLineCtx, .grammar = args->grammar, .coloring = args->coloring, .found = found);
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
        mmgr_line_put(args->line_verba_linea_pointer, t_work_state->stack[at]);
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
    const char **from = args->from_characters;
    unsigned long long value = 0ULL;
    unsigned long long end = 0ULL;
    value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = *from, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = *from, .cap = CLI_POOL_BYTES}), .end = &end);
    *from += end;
    return value;
}

// a coloring begun with the state given stacked, each of its names between `/` one state, an indent after `:` its
// indent, and an indent before `|` the statement's
static void cli_stack(const HighlightLineCtx *args)
{
    const CliGrammar *grammar = args->grammar;
    args->coloring->characters = 0ULL;
    args->coloring->runs = 0u;
    args->coloring->marked = 0u;
    args->coloring->depth = 0u;
    args->coloring->statement = 0ULL;
    args->coloring->root = EMBED_CALL(cli_highlight_name_internal.state, CliHighlightNameCtx, .grammar = grammar,
                                      .name = "root", .length = 4u);
    const char *from = args->state;
    const char *const bar = cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES, .byte = '|'});
    if (bar != NULL)
    {
        args->coloring->statement = EMBED_CALL(cli_digits_read, HighlightLineCtx, .from_characters = &from);
        from = bar + 1;
    }
    for (;;)
    {
        const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES, .byte = '/'});
        const unsigned long long whole = (slash != NULL)
                                             ? (unsigned long long)(slash - from)
                                             : cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES});
        // the name to its `:`, where one is within it
        unsigned long long length = 0ULL;
        length = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = from, .at = length,
                            .size = whole, .set = ":");
        if (args->coloring->depth < CLI_HIGHLIGHT_STACK)
        {
            VerbaLinea name = {t_work_state->stack[args->coloring->depth], CLI_NAME_MAX_LEN, 0ULL};
            mmgr_line_put_n(&name, from, length);
            mmgr_line_finish(&name);
            args->coloring->state_at[args->coloring->depth] =
                EMBED_CALL(cli_highlight_name_internal.state, CliHighlightNameCtx, .grammar = grammar, .name = from,
                           .length = length);
            const char *indent = from + length + ((length < whole) ? 1u : 0u);
            args->coloring->indent_at[args->coloring->depth] =
                (length < whole) ? EMBED_CALL(cli_digits_read, HighlightLineCtx, .from_characters = &indent) : 0ULL;
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
    memor.cpy(&(MemoriaCfg){.dst = t_work_state->stack[0],
                            .src = "root",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "root", .cap = CLI_POOL_BYTES}) + 1u});
    args->coloring->state_at[0] = args->coloring->root;
    args->coloring->depth = 1u;
}

void cli_highlight_leaves_get(const CliHighlightWorkStateStateCtx *args)
{
    const char *state = args->state;
    VerbaLinea *ended = args->ended;
    CliHighlightWorkState *const before = t_work_state;
    t_work_state = args->work_state;
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
                       .col = t_work_state->cols[coloring.characters]);
        }
        char inner_state[CLI_HIGHLIGHT_STACK * 4u];
        VerbaLinea state_line = {inner_state, sizeof(inner_state), 0ULL};
        EMBED_CALL(cli_state_write, HighlightLineCtx, .grammar = args->grammar, .coloring_read = &coloring,
                   .line_verba_linea_pointer = &state_line);
        // a state longer than its work state is read as root, as a request's last line's is
        const char *const inner_ended = (mmgr_line_finish(&state_line) != 0ULL) ? inner_state : "root";
        const CliLeaves leaves = {
            args->lines[line],   coloring.runs,       t_work_state->run_col, t_work_state->run_name,
            args->grammar->name, args->grammar->text, inner_ended,           t_work_state->cols[coloring.characters]};
        EMBED_CALL(args->each, CliLeavesEachCfg, .context = args->context, .leaves = &leaves);
    }
    EMBED_CALL(cli_state_write, HighlightLineCtx, .grammar = args->grammar, .coloring_read = &coloring,
               .line_verba_linea_pointer = ended);
    t_work_state = before;
}

void cli_highlight_lines(const CliHighlightWorkStateCtx *args)
{
    CliHighlightWorkState *const before = t_work_state;
    t_work_state = args->work_state;
    CliColoring coloring;
    EMBED_CALL(cli_stack, HighlightLineCtx, .grammar = args->grammar, .coloring = &coloring, .state = args->state);
    mmgr_line_put(args->out, "{\"names\":[");
    for (unsigned int at = 0u; at < args->grammar->names; at += 1u)
    {
        const char *const name = args->grammar->text + args->grammar->name[at];
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    }
    mmgr_line_put(args->out, "],\"runs\":[");
    // the state each line ends in, as JSON, among the work state's endings
    VerbaLinea endings = {t_work_state->endings, t_work_state->endings_size, 0ULL};
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
            mmgr_line_decimal(args->out, t_work_state->run_col[run], 1u);
            mmgr_line_ch(args->out, ',');
            mmgr_line_decimal(args->out, t_work_state->run_name[run], 1u);
            mmgr_line_ch(args->out, ']');
        }
        mmgr_line_ch(args->out, ']');
        // each state put as JSON among the endings, a comma before each but the first
        char ended[CLI_HIGHLIGHT_STACK * 4u];
        VerbaLinea state_line = {ended, sizeof(ended), 0ULL};
        EMBED_CALL(cli_state_write, HighlightLineCtx, .grammar = args->grammar, .coloring_read = &coloring,
                   .line_verba_linea_pointer = &state_line);
        const unsigned long long length = mmgr_line_ok(&state_line) ? state_line.at : 0ULL;
        mmgr_line_put(&endings, (each != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &endings, .text = ended,
                   .length = length);
    }
    mmgr_line_put(args->out, "],\"states\":[");
    mmgr_line_put_n(args->out, t_work_state->endings, mmgr_line_ok(&endings) ? endings.at : 0ULL);
    mmgr_line_put(args->out, "]}");
    t_work_state = before;
}
