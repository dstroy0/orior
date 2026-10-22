// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// highlight_line.c: a line colored by a grammar: its tokens as runs, each a column in UTF-16 units and the place of
// its name, running to the next run's column or the line's end, and the state the line ends in. Run under the lock
// the grammar is kept under
#include "highlight_internal.h"

// the room the coloring under way works in, the one the keeper handed it
static CliHighlightRoom *s_room;

// One line's work: its letters, how many are colored, its runs, the states stacked, each stacked state's place among
// the grammar's states, found once as it is stacked, and root's place
typedef struct
{
    unsigned long long letters;
    unsigned int runs;
    unsigned int depth;
    unsigned int root;
    unsigned int state_at[CLI_HIGHLIGHT_STACK];
} CliColoring;

// the letters from `from` to `to` written in UTF-8 into `out`, which holds four bytes a letter, lower case where
// `lower` is 1: their length
static unsigned long long cli_text_of(unsigned long long from, unsigned long long to, char *out, int lower)
{
    CliLine line = {out, (4ull * s_room->most) + 1ull, 0ull};
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
    return cli_line_closed(&line);
}

// The name a token takes for the matched text, from `from` to `to`, which is read only where the token picks by it
static unsigned int cli_resolve(const CliGrammar *grammar, unsigned int token, unsigned long long from,
                                unsigned long long to)
{
    if (token == CLI_HIGHLIGHT_NONE)
    {
        return 0u;
    }
    const CliHighlightToken *const held = &grammar->token[token];
    if (held->kind != CLI_TOKEN_CASES)
    {
        return (held->kind == CLI_TOKEN_NAME) ? held->name : 0u;
    }
    // the matched text as UTF-8, and the key the word lists are searched with: the text itself, or lowered where the
    // grammar sets case aside
    const unsigned long long text_length = cli_text_of(from, to, s_room->picked, 0);
    const char *const key = grammar->ignore_case ? s_room->key : s_room->picked;
    const unsigned long long key_length = grammar->ignore_case ? cli_text_of(from, to, s_room->key, 1) : text_length;
    unsigned int fallback = CLI_HIGHLIGHT_NONE;
    for (unsigned int at = held->first; at < (held->first + held->count); at += 1u)
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
            // the case's text is terminated and the matched text holds no terminator, so a compare over the matched
            // text's length with the byte after it tells the two apart in one pass
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

// a run put at letter `at` with `name`: none where the run before has the name, and the run before taken off where it
// starts at the same column
static void cli_push(CliColoring *coloring, unsigned long long at, unsigned int name)
{
    const unsigned int col = s_room->cols[at];
    if ((coloring->runs != 0u) && (s_room->run_name[coloring->runs - 1u] == name))
    {
        return;
    }
    if ((coloring->runs != 0u) && (s_room->run_col[coloring->runs - 1u] == col))
    {
        coloring->runs -= 1u;
        if ((coloring->runs != 0u) && (s_room->run_name[coloring->runs - 1u] == name))
        {
            return;
        }
    }
    s_room->run_col[coloring->runs] = col;
    s_room->run_name[coloring->runs] = name;
    coloring->runs += 1u;
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
        coloring->depth += 1u;
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
            const int held = ((index + 1u) <= rule->groups) && found[index + 1u].set;
            const unsigned long long part = held ? (found[index + 1u].to - found[index + 1u].from) : 0ull;
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

// One line colored: its runs and the stack it ends with in the coloring
static void cli_line(const CliGrammar *grammar, const char *line, CliColoring *coloring)
{
    coloring->letters = cli_regexp_letters(line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}),
                                           s_room->letters, s_room->most);
    coloring->runs = 0u;
    unsigned int col = 0u;
    unsigned long long length = coloring->letters;
    for (unsigned long long at = 0ull; at < coloring->letters; at += 1ull)
    {
        s_room->cols[at] = col;
        length = ((length == coloring->letters) && (col >= CLI_HIGHLIGHT_LONGEST)) ? at : length;
        col += (s_room->letters[at] >= 0x10000u) ? 2u : 1u;
    }
    s_room->cols[coloring->letters] = col;
    length = ((length == coloring->letters) && (col >= CLI_HIGHLIGHT_LONGEST)) ? coloring->letters : length;
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
    // A rule that matches nothing at the line's end and changes the state still applies there, once: it is how a
    // state that lasts to the end of its line ends
    const CliHighlightList *const ending = cli_rules(grammar, coloring, 0);
    for (unsigned int each = 0u; (ending != NULL) && (each < ending->count); each += 1u)
    {
        const unsigned int index = grammar->listed[ending->first + each];
        if ((grammar->rule[index].next != CLI_HIGHLIGHT_NONE) &&
            cli_tried(grammar, index, coloring, coloring->letters, found) && found[0].set &&
            (found[0].from == found[0].to))
        {
            cli_step(grammar, coloring, grammar->text + grammar->rule[index].next);
            break;
        }
    }
    if (coloring->runs == 0u)
    {
        cli_push(coloring, 0u, 0u);
    }
}

// the states stacked written joined by `/`, or root where the grammar starts every line in root
static void cli_state_written(const CliGrammar *grammar, const CliColoring *coloring, CliLine *line)
{
    if (grammar->per_line)
    {
        cli_line_text(line, "root");
        return;
    }
    for (unsigned int at = 0u; at < coloring->depth; at += 1u)
    {
        cli_line_text(line, (at != 0u) ? "/" : "");
        cli_line_text(line, s_room->stack[at]);
    }
}

void cli_highlight_lines(const CliGrammar *grammar, CliHighlightRoom *room, const char *state, const char *const *lines,
                         unsigned int count, CliLine *out)
{
    s_room = room;
    CliColoring coloring;
    coloring.letters = 0ull;
    coloring.runs = 0u;
    coloring.depth = 0u;
    coloring.root = cli_highlight_state(grammar, "root", 4u);
    // the state given stacked, each of its names between `/` one state
    const char *from = state;
    for (;;)
    {
        const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL, .byte = '/'});
        const unsigned long long length = (slash != NULL)
                                              ? (unsigned long long)(slash - from)
                                              : cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
        if (coloring.depth < CLI_HIGHLIGHT_STACK)
        {
            CliLine name = {s_room->stack[coloring.depth], CLI_NAME_LONGEST, 0ull};
            cli_line_counted(&name, from, length);
            cli_line_closed(&name);
            coloring.state_at[coloring.depth] = cli_highlight_state(grammar, from, length);
            coloring.depth += 1u;
        }
        if (slash == NULL)
        {
            break;
        }
        from = slash + 1;
    }
    cli_line_text(out, "{\"names\":[");
    for (unsigned int at = 0u; at < grammar->names; at += 1u)
    {
        const char *const name = grammar->text + grammar->name[at];
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, "],\"runs\":[");
    // the state each line ends in, as JSON, among the room's endings
    CliLine endings = {s_room->endings, s_room->endings_size, 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        // a grammar that starts every line in root ends each line there, and the next starts where it ended
        if (grammar->per_line && (each != 0u))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_room->stack[0],
                              .src = "root",
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = "root", .cap = CLI_CELLS_POOL}) + 1u});
            coloring.state_at[0] = coloring.root;
            coloring.depth = 1u;
        }
        cli_line(grammar, lines[each], &coloring);
        cli_line_text(out, (each != 0u) ? ",[" : "[");
        for (unsigned int run = 0u; run < coloring.runs; run += 1u)
        {
            cli_line_text(out, (run != 0u) ? ",[" : "[");
            cli_line_decimal(out, s_room->run_col[run], 1u);
            cli_line_character(out, ',');
            cli_line_decimal(out, s_room->run_name[run], 1u);
            cli_line_character(out, ']');
        }
        cli_line_character(out, ']');
        // each state put as JSON among the endings, a comma before each but the first
        char ended[CLI_HIGHLIGHT_STACK * 4u];
        CliLine state_line = {ended, sizeof(ended), 0ull};
        cli_state_written(grammar, &coloring, &state_line);
        const unsigned long long length = cli_line_fits(&state_line) ? state_line.at : 0ull;
        cli_line_text(&endings, (each != 0u) ? "," : "");
        cli_json_quoted(&endings, ended, length);
    }
    cli_line_text(out, "],\"states\":[");
    cli_line_counted(out, s_room->endings, cli_line_fits(&endings) ? endings.at : 0ull);
    cli_line_text(out, "]}");
    s_room = NULL;
}
