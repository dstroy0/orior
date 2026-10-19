// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_graph.c: every branch's commits laid out as a graph, or those a search finds over the whole history
#include "../text/regexp.h"
#include "git_internal.h"

// the most columns the graph holds, the most words a search holds, and the most bytes of a commit's text searched
#define CLI_GIT_LANES 512u

#define CLI_GIT_QUERY_WORDS 64u

#define CLI_GIT_HELD (1u << 14u)

// A column of the graph: the commit it waits for, a place in the graph's texts, and the color of its line
typedef struct
{
    int has;
    unsigned int id;
    unsigned int color;
} CliLane;

// `length` bytes of `text` put in lower case after `line`
static void cli_lowered(CliLine *line, const char *text, unsigned long long length)
{
    static unsigned int s_letters[CLI_GIT_HELD];
    const unsigned long long count = cli_regexp_letters(text, length, s_letters, CLI_GIT_HELD);
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        cli_utf8_put(line, cli_regexp_lower(s_letters[at]));
    }
}

// a stroke of the row of `drawn` kept: from column `from` to column `to`, in the row's top half or its bottom
static void cli_stroke(CliGraph *found, CliDrawn *drawn, unsigned int from, unsigned int to, unsigned int half,
                       unsigned int color)
{
    if (found->strokes >= CLI_GIT_STROKES)
    {
        return;
    }
    found->stroke[found->strokes][0] = from;
    found->stroke[found->strokes][1] = to;
    found->stroke[found->strokes][2] = half;
    found->stroke[found->strokes][3] = color;
    found->strokes += 1u;
    drawn->stroke_count += 1u;
}

// the first free column, one put after the last where none is
static unsigned int cli_free(CliLane *lanes, unsigned int *count)
{
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (!lanes[at].has)
        {
            return at;
        }
    }
    if (*count < CLI_GIT_LANES)
    {
        lanes[*count].has = 0;
        *count += 1u;
    }
    return *count - 1u;
}

// Each commit given its column and the strokes of its row. Each column holds the commit it waits for, the next on its
// line, and the line's color. A commit takes the first column waiting for it, or the first free one where none waits,
// a branch's newest commit; every other column waiting for it ends at it. Its first parent goes on in its column, or,
// where another column already waits for that parent, joins that column; each further parent, a merge's, joins the
// column that waits for it or opens one of its own
void cli_git_laid_out(CliGraph *found)
{
    static CliLane s_lanes[CLI_GIT_LANES];
    static int s_opened[CLI_GIT_LANES];
    unsigned int lanes = 0u;
    unsigned int colors = 0u;
    for (unsigned int index = 0u; index < found->count; index += 1u)
    {
        CliDrawn *const commit = &found->item[index];
        const char *const id = cli_git_text_of(&found->text, commit->id);
        commit->first_stroke = found->strokes;
        commit->stroke_count = 0u;
        unsigned int lane = CLI_GIT_LANES;
        for (unsigned int at = 0u; (lane == CLI_GIT_LANES) && (at < lanes); at += 1u)
        {
            lane = (s_lanes[at].has &&
                    (cellul.eq(&(CatenaFinitaCfg){
                        .src = cli_git_text_of(&found->text, s_lanes[at].id), .other = id, .cap = CLI_CELLS_POOL})))
                       ? at
                       : lane;
        }
        unsigned int color = 0u;
        if (lane < CLI_GIT_LANES)
        {
            color = s_lanes[lane].color;
        }
        else
        {
            lane = cli_free(s_lanes, &lanes);
            color = colors;
            colors += 1u;
        }
        for (unsigned int at = 0u; at < lanes; at += 1u)
        {
            if (s_lanes[at].has)
            {
                const int waits = cellul.eq(&(CatenaFinitaCfg){
                    .src = cli_git_text_of(&found->text, s_lanes[at].id), .other = id, .cap = CLI_CELLS_POOL});
                cli_stroke(found, commit, at, waits ? lane : at, 0u, s_lanes[at].color);
            }
        }
        for (unsigned int at = 0u; at < lanes; at += 1u)
        {
            if (s_lanes[at].has &&
                (cellul.eq(&(CatenaFinitaCfg){
                    .src = cli_git_text_of(&found->text, s_lanes[at].id), .other = id, .cap = CLI_CELLS_POOL})))
            {
                s_lanes[at].has = 0;
            }
            s_opened[at] = 0;
        }
        for (unsigned int nth = 0u; nth < commit->parents; nth += 1u)
        {
            const unsigned int parent = commit->parent[nth];
            const char *const parent_id = cli_git_text_of(&found->text, parent);
            unsigned int joins = CLI_GIT_LANES;
            for (unsigned int at = 0u; (joins == CLI_GIT_LANES) && (at < lanes); at += 1u)
            {
                joins = (s_lanes[at].has &&
                         (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&found->text, s_lanes[at].id),
                                                       .other = parent_id,
                                                       .cap = CLI_CELLS_POOL})))
                            ? at
                            : joins;
            }
            if (joins < CLI_GIT_LANES)
            {
                cli_stroke(found, commit, lane, joins, 1u, s_lanes[joins].color);
            }
            else if (nth == 0u)
            {
                s_lanes[lane] = (CliLane){1, parent, color};
                s_opened[lane] = 1;
                cli_stroke(found, commit, lane, lane, 1u, color);
            }
            else
            {
                const unsigned int at = cli_free(s_lanes, &lanes);
                s_lanes[at] = (CliLane){1, parent, colors};
                s_opened[at] = 1;
                cli_stroke(found, commit, lane, at, 1u, colors);
                colors += 1u;
            }
        }
        for (unsigned int at = 0u; at < lanes; at += 1u)
        {
            if (s_lanes[at].has && !s_opened[at])
            {
                cli_stroke(found, commit, at, at, 1u, s_lanes[at].color);
            }
        }
        while ((lanes > 0u) && !s_lanes[lanes - 1u].has)
        {
            lanes -= 1u;
        }
        commit->lane = lane;
        commit->color = color;
    }
}

// whether a commit's subject, author where `authors`, id and names, in lower case, hold each word of the query
static int cli_held(const CliGraph *found, const CliDrawn *drawn, int authors, const char (*words)[CLI_NAME_LONGEST],
                    unsigned int count)
{
    static char s_held[CLI_GIT_HELD];
    CliLine line = {s_held, sizeof(s_held), 0ull};
    const char *const subject = cli_git_text_of(&found->text, drawn->subject);
    const char *const author = authors ? cli_git_text_of(&found->text, drawn->author) : "";
    const char *const id = cli_git_text_of(&found->text, drawn->id);
    cli_lowered(&line, subject, cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_CELLS_POOL}));
    cli_line_character(&line, ' ');
    cli_lowered(&line, author, cellul.len(&(CatenaFinitaCfg){.src = author, .cap = CLI_CELLS_POOL}));
    cli_line_character(&line, ' ');
    cli_lowered(&line, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    cli_line_character(&line, ' ');
    for (unsigned int at = 0u; at < drawn->refs; at += 1u)
    {
        const char *const name = cli_git_text_of(&found->text, drawn->ref[at]);
        cli_line_text(&line, (at != 0u) ? " " : "");
        cli_lowered(&line, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    cli_line_closed(&line);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){
                .src = s_held, .cap = CLI_CELLS_POOL, .other = words[at], .other_cap = CLI_CELLS_POOL}) == NULL)
        {
            return 0;
        }
    }
    return 1;
}

// the commit of a line `id\x1fparents\x1fauthor\x1fwhen\x1fnames\x1fsubject` put after those found: 1, or 0 where the
// line is no commit's
static int cli_drawn_put(CliGraph *found, const char *line, const char *stop)
{
    const char *at = line;
    unsigned long long lengths[5] = {0ull, 0ull, 0ull, 0ull, 0ull};
    const char *fields[5] = {NULL, NULL, NULL, NULL, NULL};
    for (unsigned int field = 0u; field < 5u; field += 1u)
    {
        fields[field] = cli_git_field(&at, stop, '\x1f', &lengths[field]);
    }
    long long when = 0ll;
    if ((fields[4] == NULL) || !cli_git_seconds(fields[3], lengths[3], &when))
    {
        return 0;
    }
    CliDrawn *const drawn = &found->item[found->count];
    drawn->id = cli_git_text_put(&found->text, fields[0], lengths[0]);
    drawn->parents = 0u;
    const char *word = fields[1];
    const char *const parents_end = fields[1] + lengths[1];
    while (word < parents_end)
    {
        while ((word < parents_end) && (word[0] == ' '))
        {
            word += 1;
        }
        const char *word_end = word;
        while ((word_end < parents_end) && (word_end[0] != ' '))
        {
            word_end += 1;
        }
        if ((word_end > word) && (drawn->parents < CLI_GIT_PARENTS))
        {
            drawn->parent[drawn->parents] = cli_git_text_put(&found->text, word, (unsigned long long)(word_end - word));
            drawn->parents += 1u;
        }
        word = word_end;
    }
    drawn->author = cli_git_text_put(&found->text, fields[2], lengths[2]);
    drawn->when = when;
    drawn->refs = 0u;
    const char *name = fields[4];
    const char *const names_end = fields[4] + lengths[4];
    while (name < names_end)
    {
        const char *name_end = name;
        while (((name_end + 1) < names_end) && !((name_end[0] == ',') && (name_end[1] == ' ')))
        {
            name_end += 1;
        }
        name_end = ((name_end + 1) < names_end) ? name_end : names_end;
        if ((name_end > name) && (drawn->refs < CLI_GIT_REFS))
        {
            drawn->ref[drawn->refs] = cli_git_text_put(&found->text, name, (unsigned long long)(name_end - name));
            drawn->refs += 1u;
        }
        name = name_end + 2;
    }
    drawn->subject = (at <= stop) ? cli_git_text_put(&found->text, at, (unsigned long long)(stop - at)) : 0u;
    drawn->lane = 0u;
    drawn->color = 0u;
    drawn->first_stroke = 0u;
    drawn->stroke_count = 0u;
    return 1;
}

void cli_git_graph(const char *root, const char *query, int authors, CliGraph *found)
{
    static char s_words[CLI_GIT_QUERY_WORDS][CLI_NAME_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    found->count = 0u;
    found->strokes = 0u;
    cli_git_text_begun(&found->text);
    cli_lock_take(&s_lock);
    unsigned int words = 0u;
    const char *word = query;
    while ((word[0] != '\0') && (words < CLI_GIT_QUERY_WORDS))
    {
        while ((word[0] == ' ') || ((word[0] >= '\t') && (word[0] <= '\r')))
        {
            word += 1;
        }
        const char *end = word;
        while ((end[0] != '\0') && (end[0] != ' ') && !((end[0] >= '\t') && (end[0] <= '\r')))
        {
            end += 1;
        }
        if (end > word)
        {
            CliLine line = {s_words[words], CLI_NAME_LONGEST, 0ull};
            cli_lowered(&line, word, (unsigned long long)(end - word));
            cli_line_closed(&line);
            words += 1u;
        }
        word = end;
    }
    static const char *const s_format = "--format=%H%x1f%P%x1f%an%x1f%at%x1f%D%x1f%s";
    const char *const drawn[5] = {"log", "-n2000", "--all", "--date-order", s_format};
    const char *const searched[4] = {"log", "--all", "--date-order", s_format};
    CliGitGave *const gave = cli_git_taken();
    const int asked = (words == 0u) ? cli_git_asked(root, drawn, 5u, gave) : cli_git_asked(root, searched, 4u, gave);
    const char *line = gave->out;
    const char *const end = gave->out + gave->out_size;
    while (asked && (line < end) && (found->count < CLI_GIT_GRAPH))
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        // a commit the search does not find leaves its texts kept, and is taken back
        const unsigned long long used = found->text.used;
        if (cli_drawn_put(found, line, stop))
        {
            if ((words == 0u) || cli_held(found, &found->item[found->count], authors, s_words, words))
            {
                found->count += 1u;
            }
            else
            {
                found->text.used = used;
            }
        }
        line = next;
    }
    cli_git_given(gave);
    if (words == 0u)
    {
        cli_git_laid_out(found);
    }
    cli_lock_give(&s_lock);
}
