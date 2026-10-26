// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_line.c: a line colored by a grammar: its tokens as runs, each a column in UTF-16 units and the place of
// its name, running to the next run's column or the line's end, and the state the line ends in. Run under the lock
// the grammar is stored under
#include "highlight_internal.h"

// the room the coloring under way works in, the one the keeper handed it
static CliHighlightRoom *s_room;

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

// the letters from `from` to `to` written in UTF-8 into `out`, sized for four bytes a letter, lower case where
// `lower` is 1: their length
static unsigned long long cli_text_of(unsigned long long from, unsigned long long to, char *out, int lower)
{
    VerbaLinea line = {out, (4ull * s_room->most) + 1ull, 0ull};
    for (unsigned long long at = from; at < to; at += 1ull)
    {
        const unsigned int letter = lower ? cli_regexp_lower(s_room->letters[at]) : s_room->letters[at];
        // an ASCII letter is its own byte; the rest take the UTF-8 writer
        if ((letter < 128u) && (line.at < line.capacity))
        {
            line.out[line.at] = (char)letter;
            line.at += 1ull;
        }
        else
        {
            cli_utf8_put(&line, letter);
        }
    }
    return mmgr_line_finish(&line);
}

// The name a token takes for the matched text, from `from` to `to`, which is read only where the token picks by it
static unsigned int cli_resolve(const CliGrammar *grammar, unsigned int token, unsigned long long from,
                                unsigned long long to)
{
    if (token == CLI_HIGHLIGHT_NONE)
    {
        return 0u;
    }
    const CliHighlightToken *const token_at = &grammar->token[token];
    if (token_at->kind != CLI_TOKEN_CASES)
    {
        return (token_at->kind == CLI_TOKEN_NAME) ? token_at->name : 0u;
    }
    // the matched text as UTF-8, and the key the word lists are searched with: the text itself, or lowered where the
    // grammar sets case aside
    const unsigned long long text_length = cli_text_of(from, to, s_room->picked, 0);
    const char *const key = grammar->ignore_case ? s_room->key : s_room->picked;
    const unsigned long long key_length = grammar->ignore_case ? cli_text_of(from, to, s_room->key, 1) : text_length;
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
            hit = cli_highlight_has_word(grammar, &grammar->words[entry->list], key, key_length);
        }
        else if (name[0] != '@')
        {
            // the case's text is terminated and the matched text contains no terminator, so a compare over the matched
            // text's length with the byte after it distinguishes the two in one pass
            hit = (memor.cmp(&(MemoriaCfg){.src = name, .other = s_room->picked, .bytes = text_length}) == 0) &&
                  (name[text_length] == '\0');
        }
        if (hit)
        {
            return cli_resolve(grammar, grammar->cases[at].token, from, to);
        }
    }
    return cli_resolve(grammar, fallback, from, to);
}

// a run put at letter `at` with `name`: each token a run of its own, but plain text, which joins the plain run before
// it, and the run before taken off where it starts at the same column, a mark never
static void cli_push(CliColoring *coloring, unsigned long long at, unsigned int name)
{
    const unsigned int col = s_room->cols[at];
    if ((coloring->runs != 0u) && (name == 0u) && (s_room->run_name[coloring->runs - 1u] == 0u))
    {
        return;
    }
    if ((coloring->runs > coloring->marked) && (s_room->run_col[coloring->runs - 1u] == col))
    {
        coloring->runs -= 1u;
        if ((coloring->runs != 0u) && (name == 0u) && (s_room->run_name[coloring->runs - 1u] == 0u))
        {
            return;
        }
    }
    s_room->run_col[coloring->runs] = col;
    s_room->run_name[coloring->runs] = name;
    coloring->runs += 1u;
}

// a mark put at the column `col` with `name`, a run of no letters that no run put after it at its column takes the
// place of
static void cli_mark(CliColoring *coloring, unsigned int col, unsigned int name)
{
    s_room->run_col[coloring->runs] = col;
    s_room->run_name[coloring->runs] = name;
    coloring->runs += 1u;
    coloring->marked = coloring->runs;
}

// the state stacked changed by a next: none, `@pop` leaving the one entered last, `@popall` returning to root, or a
// state entered
static void cli_step(const CliGrammar *grammar, CliColoring *coloring, const char *next)
{
    if ((next == NULL) || (next[0] == '\0'))
    {
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = next, .other = "@pop", .cap = CLI_CELLS_POOL}))
    {
        coloring->depth -= (coloring->depth > 1u) ? 1u : 0u;
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = next, .other = "@popall", .cap = CLI_CELLS_POOL}))
    {
        coloring->depth = (coloring->depth > 1u) ? 1u : coloring->depth;
        return;
    }
    const char *name = next;
    while (name[0] == '@')
    {
        name += 1;
    }
    if (coloring->depth < CLI_HIGHLIGHT_STACK)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        memor.cpy(&(MemoriaCfg){.dst = s_room->stack[coloring->depth], .src = name, .bytes = length + 1u});
        coloring->state_at[coloring->depth] = cli_highlight_state(grammar, name, length);
        coloring->indent_at[coloring->depth] = coloring->statement;
        coloring->depth += 1u;
    }
}

// the place among the names of the name the close of the state on top takes, none where it is none of the states of
// the grammar's `indent`
static unsigned int cli_top_close(const CliGrammar *grammar, const CliColoring *coloring)
{
    const unsigned int state = (coloring->depth != 0u) ? coloring->state_at[coloring->depth - 1u] : CLI_HIGHLIGHT_NONE;
    return (state != CLI_HIGHLIGHT_NONE) ? grammar->state[state].close : CLI_HIGHLIGHT_NONE;
}

// each state of the grammar's `indent` on top of the stack left, a mark of its close put at the column `col` for each
static void cli_closed(const CliGrammar *grammar, CliColoring *coloring, unsigned int col)
{
    while ((coloring->depth > 1u) && (cli_top_close(grammar, coloring) != CLI_HIGHLIGHT_NONE))
    {
        cli_mark(coloring, col, cli_top_close(grammar, coloring));
        coloring->depth -= 1u;
    }
}

// the rules of the state on top, its place found as it was stacked; root's where the grammar has no such state, none
// where it has neither
static const CliHighlightList *cli_rules(const CliGrammar *grammar, const CliColoring *coloring, int or_root)
{
    unsigned int state = (coloring->depth != 0u) ? coloring->state_at[coloring->depth - 1u] : coloring->root;
    state = ((state == CLI_HIGHLIGHT_NONE) && or_root) ? coloring->root : state;
    return (state != CLI_HIGHLIGHT_NONE) ? &grammar->state[state] : NULL;
}

// a rule's pattern tried at `at`: 1 with the spans of the match in `found`, or 0
static int cli_tried(const CliGrammar *grammar, unsigned int rule, const CliColoring *coloring, unsigned long long at,
                     CliRegexpSpan *found)
{
    CliRegexp pattern;
    cli_highlight_pattern(grammar, &grammar->rule[rule], &pattern);
    return cli_regexp_match_into(&pattern, s_room->letters, coloring->letters, at, found);
}

// the match of a rule taken at `at`: each group's token put where its group stands, or the whole match's token; the
// letters it took
static unsigned long long cli_taken(const CliGrammar *grammar, const CliHighlightRule *rule, CliColoring *coloring,
                                    unsigned long long at, const CliRegexpSpan *found)
{
    const unsigned long long start = found[0].set ? found[0].from : at;
    const unsigned long long end = found[0].set ? found[0].to : at;
    const unsigned long long size = end - start;
    const CliHighlightToken *const token = (rule->token != CLI_HIGHLIGHT_NONE) ? &grammar->token[rule->token] : NULL;
    if ((token != NULL) && (token->kind == CLI_TOKEN_GROUPS))
    {
        unsigned long long place = at;
        for (unsigned int index = 0u; index < token->count; index += 1u)
        {
            const int matched = ((index + 1u) <= rule->groups) && found[index + 1u].set;
            const unsigned long long part = matched ? (found[index + 1u].to - found[index + 1u].from) : 0ull;
            if (part > 0ull)
            {
                const unsigned long long put = (place < coloring->letters) ? place : coloring->letters;
                cli_push(coloring, put,
                         cli_resolve(grammar, grammar->cases[token->first + index].token, found[index + 1u].from,
                                     found[index + 1u].to));
            }
            place += part;
        }
    }
    else if ((token != NULL) && (size > 0ull))
    {
        cli_push(coloring, at, cli_resolve(grammar, rule->token, start, end));
    }
    return size;
}

// a line begun in a grammar with an `indent`, where its `skip` does not match the line: each state of its `states` on
// top of the stack whose indent is as deep as the line's or deeper left, a mark of its close put at the line's start,
// and the line's indent made the statement's where the state on top is one of its `begins`. A line's indent counts a
// space one column and a tab to the next multiple of the grammar's `tab`
static void cli_line_indented(const CliGrammar *grammar, CliColoring *coloring)
{
    CliRegexpSpan found[CLI_REGEXP_GROUPS + 1u];
    if ((grammar->skip != CLI_HIGHLIGHT_NONE) && cli_tried(grammar, grammar->skip, coloring, 0ull, found) &&
        found[0].set)
    {
        return;
    }
    unsigned long long indent = 0ull;
    for (unsigned long long at = 0ull;
         (at < coloring->letters) && ((s_room->letters[at] == ' ') || (s_room->letters[at] == '\t')); at += 1ull)
    {
        indent = (s_room->letters[at] == '\t') ? (((indent / grammar->tab) + 1ull) * grammar->tab) : (indent + 1ull);
    }
    while ((coloring->depth > 1u) && (cli_top_close(grammar, coloring) != CLI_HIGHLIGHT_NONE) &&
           (indent <= coloring->indent_at[coloring->depth - 1u]))
    {
        cli_mark(coloring, 0u, cli_top_close(grammar, coloring));
        coloring->depth -= 1u;
    }
    const unsigned int top = (coloring->depth != 0u) ? coloring->state_at[coloring->depth - 1u] : CLI_HIGHLIGHT_NONE;
    if ((top != CLI_HIGHLIGHT_NONE) && grammar->state[top].begins)
    {
        coloring->statement = indent;
    }
}

// One line colored: its runs and the stack it ends with in the coloring
static void cli_line(const CliGrammar *grammar, const char *line, CliColoring *coloring)
{
    coloring->letters = cli_regexp_letters(line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}),
                                           s_room->letters, s_room->most);
    coloring->runs = 0u;
    coloring->marked = 0u;
    unsigned int col = 0u;
    unsigned long long length = coloring->letters;
    for (unsigned long long at = 0ull; at < coloring->letters; at += 1ull)
    {
        s_room->cols[at] = col;
        length = ((length == coloring->letters) && (col >= CLI_HIGHLIGHT_MAX_LEN)) ? at : length;
        col += (s_room->letters[at] >= 0x10000u) ? 2u : 1u;
    }
    s_room->cols[coloring->letters] = col;
    length = ((length == coloring->letters) && (col >= CLI_HIGHLIGHT_MAX_LEN)) ? coloring->letters : length;
    if (grammar->indents)
    {
        cli_line_indented(grammar, coloring);
    }
    CliRegexpSpan found[CLI_REGEXP_GROUPS + 1u];
    const CliHighlightList *rules = cli_rules(grammar, coloring, 1);
    const unsigned int *const letters = s_room->letters;
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
                cli_push(coloring, at, 0u);
                at = stride;
                stalled = 0u;
                continue;
            }
        }
        int taken = 0;
        for (unsigned int each = 0u; (rules != NULL) && !taken && (each < rules->count); each += 1u)
        {
            const unsigned int index = grammar->listed[rules->first + each];
            const CliHighlightRule *const rule = &grammar->rule[index];
            // a rule that cannot start at this letter is passed over before its pattern is made
            if ((rule->anchored && (at != 0ull)) ||
                (!rule->first.empty && !cli_regexp_allows(&rule->first, letters[at])))
            {
                continue;
            }
            if (!cli_tried(grammar, index, coloring, at, found))
            {
                continue;
            }
            const unsigned long long size = found[0].set ? (found[0].to - found[0].from) : 0ull;
            if ((size == 0ull) && ((rule->next == CLI_HIGHLIGHT_NONE) || (stalled > 2u)))
            {
                continue;
            }
            cli_taken(grammar, rule, coloring, at, found);
            stalled = (size > 0ull) ? 0u : (stalled + 1u);
            at += size;
            if (rule->next != CLI_HIGHLIGHT_NONE)
            {
                cli_step(grammar, coloring, grammar->text + rule->next);
                rules = cli_rules(grammar, coloring, 1);
            }
            taken = 1;
        }
        if (!taken)
        {
            cli_push(coloring, at, 0u);
            at += 1ull;
            stalled = 0u;
        }
    }
    if (length < coloring->letters)
    {
        cli_push(coloring, length, 0u);
    }
    // A rule that matches nothing at the line's end and changes the state or names a token still applies there, once:
    // it is how a state that lasts to the end of its line ends, and a mark at a line's end is put
    const CliHighlightList *const ending = cli_rules(grammar, coloring, 0);
    for (unsigned int each = 0u; (ending != NULL) && (each < ending->count); each += 1u)
    {
        const unsigned int index = grammar->listed[ending->first + each];
        const CliHighlightRule *const rule = &grammar->rule[index];
        const unsigned int name = cli_resolve(grammar, rule->token, coloring->letters, coloring->letters);
        if (((rule->next != CLI_HIGHLIGHT_NONE) || (name != 0u)) &&
            cli_tried(grammar, index, coloring, coloring->letters, found) && found[0].set &&
            (found[0].from == found[0].to))
        {
            if (name != 0u)
            {
                cli_mark(coloring, s_room->cols[coloring->letters], name);
            }
            if (rule->next != CLI_HIGHLIGHT_NONE)
            {
                cli_step(grammar, coloring, grammar->text + rule->next);
            }
            break;
        }
    }
    if (coloring->runs == 0u)
    {
        cli_push(coloring, 0u, 0u);
    }
}

// the states stacked written joined by `/`, or root where the grammar starts every line in root; in a grammar with an
// `indent`, after the statement's indent and `|`, each state of its `states` followed by `:` and its indent
static void cli_state_written(const CliGrammar *grammar, const CliColoring *coloring, VerbaLinea *line)
{
    if (grammar->per_line)
    {
        mmgr_line_put(line, "root");
        return;
    }
    if (grammar->indents)
    {
        mmgr_line_decimal(line, coloring->statement, 1u);
        mmgr_line_ch(line, '|');
    }
    for (unsigned int at = 0u; at < coloring->depth; at += 1u)
    {
        mmgr_line_put(line, (at != 0u) ? "/" : "");
        mmgr_line_put(line, s_room->stack[at]);
        const unsigned int state = coloring->state_at[at];
        if ((state != CLI_HIGHLIGHT_NONE) && (grammar->state[state].close != CLI_HIGHLIGHT_NONE))
        {
            mmgr_line_ch(line, ':');
            mmgr_line_decimal(line, coloring->indent_at[at], 1u);
        }
    }
}

// the number the digits from `*from` write, `*from` moved past them
static unsigned long long cli_digits_read(const char **from)
{
    unsigned long long value = 0ull;
    while (((*from)[0] >= '0') && ((*from)[0] <= '9'))
    {
        value = (value * 10ull) + (unsigned long long)((*from)[0] - '0');
        *from += 1;
    }
    return value;
}

// a coloring begun with the state given stacked, each of its names between `/` one state, an indent after `:` its
// indent, and an indent before `|` the statement's
static void cli_stacked(const CliGrammar *grammar, CliColoring *coloring, const char *state)
{
    coloring->letters = 0ull;
    coloring->runs = 0u;
    coloring->marked = 0u;
    coloring->depth = 0u;
    coloring->statement = 0ull;
    coloring->root = cli_highlight_state(grammar, "root", 4u);
    const char *from = state;
    const char *const bar = cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL, .byte = '|'});
    if (bar != NULL)
    {
        coloring->statement = cli_digits_read(&from);
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
        while ((length < whole) && (from[length] != ':'))
        {
            length += 1ull;
        }
        if (coloring->depth < CLI_HIGHLIGHT_STACK)
        {
            VerbaLinea name = {s_room->stack[coloring->depth], CLI_NAME_MAX_LEN, 0ull};
            mmgr_line_put_n(&name, from, length);
            mmgr_line_finish(&name);
            coloring->state_at[coloring->depth] = cli_highlight_state(grammar, from, length);
            const char *indent = from + length + ((length < whole) ? 1u : 0u);
            coloring->indent_at[coloring->depth] = (length < whole) ? cli_digits_read(&indent) : 0ull;
            coloring->depth += 1u;
        }
        if (slash == NULL)
        {
            break;
        }
        from = slash + 1;
    }
}

// a grammar that starts every line in root ends each line there, and the next starts where it ended
static void cli_rooted(CliColoring *coloring)
{
    memor.cpy(&(MemoriaCfg){.dst = s_room->stack[0],
                            .src = "root",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "root", .cap = CLI_CELLS_POOL}) + 1u});
    coloring->state_at[0] = coloring->root;
    coloring->depth = 1u;
}

void cli_highlight_leaves_of(const CliGrammar *grammar, CliHighlightRoom *room, const char *state,
                             const char *const *lines, unsigned int count, int ends, CliLeavesEach each,
                             void *context, VerbaLinea *ended)
{
    s_room = room;
    CliColoring coloring;
    cli_stacked(grammar, &coloring, state);
    for (unsigned int line = 0u; line < count; line += 1u)
    {
        if (grammar->per_line && (line != 0u))
        {
            cli_rooted(&coloring);
        }
        cli_line(grammar, lines[line], &coloring);
        // the text's end, a line of no indent, leaves every state of the grammar's `indent`, a mark at the last line's
        // end for each
        if (ends && ((line + 1u) == count))
        {
            cli_closed(grammar, &coloring, s_room->cols[coloring.letters]);
        }
        char state[CLI_HIGHLIGHT_STACK * 4u];
        VerbaLinea state_line = {state, sizeof(state), 0ull};
        cli_state_written(grammar, &coloring, &state_line);
        // a state longer than its room is read as root, as a request's last line's is
        const char *const ended = (mmgr_line_finish(&state_line) != 0ull) ? state : "root";
        const CliLeaves leaves = {lines[line],   coloring.runs, s_room->run_col, s_room->run_name,
                                  grammar->name, grammar->text, ended};
        each(context, &leaves);
    }
    cli_state_written(grammar, &coloring, ended);
    s_room = NULL;
}

void cli_highlight_lines(const CliGrammar *grammar, CliHighlightRoom *room, const char *state, const char *const *lines,
                         unsigned int count, VerbaLinea *out)
{
    s_room = room;
    CliColoring coloring;
    cli_stacked(grammar, &coloring, state);
    mmgr_line_put(out, "{\"names\":[");
    for (unsigned int at = 0u; at < grammar->names; at += 1u)
    {
        const char *const name = grammar->text + grammar->name[at];
        mmgr_line_put(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(out, "],\"runs\":[");
    // the state each line ends in, as JSON, among the room's endings
    VerbaLinea endings = {s_room->endings, s_room->endings_size, 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (grammar->per_line && (each != 0u))
        {
            cli_rooted(&coloring);
        }
        cli_line(grammar, lines[each], &coloring);
        mmgr_line_put(out, (each != 0u) ? ",[" : "[");
        for (unsigned int run = 0u; run < coloring.runs; run += 1u)
        {
            mmgr_line_put(out, (run != 0u) ? ",[" : "[");
            mmgr_line_decimal(out, s_room->run_col[run], 1u);
            mmgr_line_ch(out, ',');
            mmgr_line_decimal(out, s_room->run_name[run], 1u);
            mmgr_line_ch(out, ']');
        }
        mmgr_line_ch(out, ']');
        // each state put as JSON among the endings, a comma before each but the first
        char ended[CLI_HIGHLIGHT_STACK * 4u];
        VerbaLinea state_line = {ended, sizeof(ended), 0ull};
        cli_state_written(grammar, &coloring, &state_line);
        const unsigned long long length = mmgr_line_ok(&state_line) ? state_line.at : 0ull;
        mmgr_line_put(&endings, (each != 0u) ? "," : "");
        cli_json_quoted(&endings, ended, length);
    }
    mmgr_line_put(out, "],\"states\":[");
    mmgr_line_put_n(out, s_room->endings, mmgr_line_ok(&endings) ? endings.at : 0ull);
    mmgr_line_put(out, "]}");
    s_room = NULL;
}
