// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_graph.c: every branch's commits laid out as a graph, or those a search finds over the whole history
#include "../text/regexp.h"
#include "../text/text.h"
#include "git_internal.h"

// A column of the graph: the commit it waits for, a place in the graph's texts, and the color of its line
typedef struct
{
    int has;
    unsigned int id;
    unsigned int color;
} CliLane;
// the most columns the graph has, the most words a search has, and the most bytes of a commit's text searched
#define CLI_GIT_LANES 512u

#define CLI_GIT_QUERY_WORDS 64u

#define CLI_GIT_GRAPH_TEXT_MAX (1u << 14u)

typedef struct
{
    VerbaLinea *line;
    const char *text;
    unsigned long long length;
    CliGraph *found;
    CliDrawn *drawn;
    unsigned int from;
    unsigned int to;
    unsigned int half;
    unsigned int color;
    CliLane *lanes;
    unsigned int *count;
    const CliGraph *found_read;
    const CliDrawn *drawn_read;
    int authors;
    const char (*words)[CLI_NAME_MAX_LEN];
    unsigned int count_value;
    const char *line_text;
    const char *stop;
} GitGraphCtx;
// `length` bytes of `text` put in lower case after `line`
EMBED_INLINE void cli_on_lower(const GitGraphCtx *args)
{
    VerbaLinea *line = args->line;
    const char *text = args->text;
    static unsigned int s_letters[CLI_GIT_GRAPH_TEXT_MAX];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = args->length, .letters = s_letters,
                   .room_u64 = CLI_GIT_GRAPH_TEXT_MAX);
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = line,
                   .letter = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = s_letters[at]));
    }
}

// a stroke of the row of `drawn` recorded: from column `from` to column `to`, in the row's top half or its bottom
EMBED_INLINE void cli_stroke(const GitGraphCtx *args)
{
    if (args->found->strokes >= CLI_GIT_STROKES)
    {
        return;
    }
    args->found->stroke[args->found->strokes][0] = args->from;
    args->found->stroke[args->found->strokes][1] = args->to;
    args->found->stroke[args->found->strokes][2] = args->half;
    args->found->stroke[args->found->strokes][3] = args->color;
    args->found->strokes += 1u;
    args->drawn->stroke_count += 1u;
}

// the first free column, one put after the last where none is
EMBED_INLINE unsigned int cli_free(const GitGraphCtx *args)
{
    unsigned int *count = args->count;
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (!args->lanes[at].has)
        {
            return at;
        }
    }
    if (*count < CLI_GIT_LANES)
    {
        args->lanes[*count].has = 0;
        *count += 1u;
    }
    return *count - 1u;
}

// Each commit given its column and the strokes of its row. Each column tracks the commit it waits for, the next on its
// line, and the line's color. A commit takes the first column waiting for it, or the first free one where none waits,
// a branch's newest commit; every other column waiting for it ends at it. Its first parent goes on in its column, or,
// where another column already waits for that parent, joins that column; each further parent, a merge's, joins the
// column that waits for it or opens one of its own
void cli_git_layout_out(const CliGitFoundCtx *args)
{
    static CliLane s_lanes[CLI_GIT_LANES];
    static int s_opened[CLI_GIT_LANES];
    unsigned int lanes = 0u;
    unsigned int colors = 0u;
    for (unsigned int index = 0u; index < args->found->count; index += 1u)
    {
        CliDrawn *const commit = &args->found->item[index];
        const char *const id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                          .text_read = &args->found->text, .at = commit->id);
        commit->first_stroke = args->found->strokes;
        commit->stroke_count = 0u;
        unsigned int lane = CLI_GIT_LANES;
        for (unsigned int at = 0u; (lane == CLI_GIT_LANES) && (at < lanes); at += 1u)
        {
            lane =
                (s_lanes[at].has &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                                 .text_read = &args->found->text, .at = s_lanes[at].id),
                                               .other = id,
                                               .cap = CLI_CELLS_POOL})))
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
            lane = EMBED_CALL(cli_free, GitGraphCtx, .lanes = s_lanes, .count = &lanes);
            color = colors;
            colors += 1u;
        }
        for (unsigned int at = 0u; at < lanes; at += 1u)
        {
            if (s_lanes[at].has)
            {
                const int waits = cellul.eq(
                    &(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                         .text_read = &args->found->text, .at = s_lanes[at].id),
                                       .other = id,
                                       .cap = CLI_CELLS_POOL});
                EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = commit, .from = at,
                           .to = waits ? lane : at, .half = 0u, .color = s_lanes[at].color);
            }
        }
        for (unsigned int at = 0u; at < lanes; at += 1u)
        {
            if (s_lanes[at].has &&
                (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                                .text_read = &args->found->text, .at = s_lanes[at].id),
                                              .other = id,
                                              .cap = CLI_CELLS_POOL})))
            {
                s_lanes[at].has = 0;
            }
            s_opened[at] = 0;
        }
        for (unsigned int nth = 0u; nth < commit->parents; nth += 1u)
        {
            const unsigned int parent = commit->parent[nth];
            const char *const parent_id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                     .text_read = &args->found->text, .at = parent);
            unsigned int joins = CLI_GIT_LANES;
            for (unsigned int at = 0u; (joins == CLI_GIT_LANES) && (at < lanes); at += 1u)
            {
                joins =
                    (s_lanes[at].has && (cellul.eq(&(CatenaFinitaCfg){
                                            .src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                              .text_read = &args->found->text, .at = s_lanes[at].id),
                                            .other = parent_id,
                                            .cap = CLI_CELLS_POOL})))
                        ? at
                        : joins;
            }
            if (joins < CLI_GIT_LANES)
            {
                EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = commit, .from = lane, .to = joins,
                           .half = 1u, .color = s_lanes[joins].color);
            }
            else if (nth == 0u)
            {
                s_lanes[lane] = (CliLane){1, parent, color};
                s_opened[lane] = 1;
                EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = commit, .from = lane, .to = lane,
                           .half = 1u, .color = color);
            }
            else
            {
                const unsigned int at = EMBED_CALL(cli_free, GitGraphCtx, .lanes = s_lanes, .count = &lanes);
                s_lanes[at] = (CliLane){1, parent, colors};
                s_opened[at] = 1;
                EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = commit, .from = lane, .to = at,
                           .half = 1u, .color = colors);
                colors += 1u;
            }
        }
        for (unsigned int at = 0u; at < lanes; at += 1u)
        {
            if (s_lanes[at].has && !s_opened[at])
            {
                EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = commit, .from = at, .to = at,
                           .half = 1u, .color = s_lanes[at].color);
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

// whether a commit's subject, author where `authors`, id and names, in lower case, contain each word of the query
EMBED_INLINE int cli_commit_matches(const GitGraphCtx *args)
{
    static char s_lower_text[CLI_GIT_GRAPH_TEXT_MAX];
    VerbaLinea line = {s_lower_text, sizeof(s_lower_text), 0ull};
    const char *const subject = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                           .text_read = &args->found_read->text, .at = args->drawn_read->subject);
    const char *const author = args->authors
                                   ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                .text_read = &args->found_read->text, .at = args->drawn_read->author)
                                   : "";
    const char *const id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                      .text_read = &args->found_read->text, .at = args->drawn_read->id);
    EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = subject,
               .length = cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&line, ' ');
    EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = author,
               .length = cellul.len(&(CatenaFinitaCfg){.src = author, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&line, ' ');
    EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&line, ' ');
    for (unsigned int at = 0u; at < args->drawn_read->refs; at += 1u)
    {
        const char *const name = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &args->found_read->text, .at = args->drawn_read->ref[at]);
        mmgr_line_put(&line, (at != 0u) ? " " : "");
        EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_finish(&line);
    for (unsigned int at = 0u; at < args->count_value; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){
                .src = s_lower_text, .cap = CLI_CELLS_POOL, .other = args->words[at], .other_cap = CLI_CELLS_POOL}) ==
            NULL)
        {
            return 0;
        }
    }
    return 1;
}

// the commit of a line `id\x1fparents\x1fauthor\x1fwhen\x1fnames\x1fsubject` put after those found: 1, or 0 where the
// line is no commit's
EMBED_INLINE int cli_draw_put(const GitGraphCtx *args)
{
    const char *stop = args->stop;
    const char *at = args->line_text;
    unsigned long long lengths[5] = {0ull, 0ull, 0ull, 0ull, 0ull};
    const char *fields[5] = {NULL, NULL, NULL, NULL, NULL};
    for (unsigned int field = 0u; field < 5u; field += 1u)
    {
        fields[field] = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop, .stop = '\x1f',
                                   .length_pointer = &lengths[field]);
    }
    long long when = 0ll;
    if ((fields[4] == NULL) || !EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = fields[3],
                                           .length = lengths[3], .value = &when))
    {
        return 0;
    }
    CliDrawn *const drawn = &args->found->item[args->found->count];
    drawn->id = EMBED_CALL(cli_git_text.text_put, CliGitTextCfg, .text = &args->found->text, .from = fields[0],
                           .length = lengths[0]);
    drawn->parents = 0u;
    const char *word = fields[1];
    const char *const parents_end = fields[1] + lengths[1];
    while (word < parents_end)
    {
        word += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = word, .at = 0ull,
                           .size = (unsigned long long)(parents_end - word), .set = " ");
        const char *word_end = word;
        word_end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = word_end, .at = 0ull,
                               .size = (unsigned long long)(parents_end - word_end), .set = " ");
        if ((word_end > word) && (drawn->parents < CLI_GIT_PARENTS))
        {
            drawn->parent[drawn->parents] = EMBED_CALL(cli_git_text.text_put, CliGitTextCfg, .text = &args->found->text,
                                                       .from = word, .length = (unsigned long long)(word_end - word));
            drawn->parents += 1u;
        }
        word = word_end;
    }
    drawn->author = EMBED_CALL(cli_git_text.text_put, CliGitTextCfg, .text = &args->found->text, .from = fields[2],
                               .length = lengths[2]);
    drawn->when = when;
    drawn->refs = 0u;
    const char *name = fields[4];
    const char *const names_end = fields[4] + lengths[4];
    while (name < names_end)
    {
        const char *name_end = name;
        name_end = name + EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = name, .at = 0ull,
                                     .size = (unsigned long long)(names_end - name), .first = ',', .second = ' ');
        name_end = ((name_end + 1) < names_end) ? name_end : names_end;
        if ((name_end > name) && (drawn->refs < CLI_GIT_REFS))
        {
            drawn->ref[drawn->refs] = EMBED_CALL(cli_git_text.text_put, CliGitTextCfg, .text = &args->found->text,
                                                 .from = name, .length = (unsigned long long)(name_end - name));
            drawn->refs += 1u;
        }
        name = name_end + 2;
    }
    drawn->subject = (at <= stop) ? EMBED_CALL(cli_git_text.text_put, CliGitTextCfg, .text = &args->found->text,
                                               .from = at, .length = (unsigned long long)(stop - at))
                                  : 0u;
    drawn->lane = 0u;
    drawn->color = 0u;
    drawn->first_stroke = 0u;
    drawn->stroke_count = 0u;
    return 1;
}

void cli_git_graph(const CliGitRootQueryCfg *args)
{
    static char s_words[CLI_GIT_QUERY_WORDS][CLI_NAME_MAX_LEN];
    static CliLock s_lock = CLI_LOCK_INIT;
    args->found_graph_pointer->count = 0u;
    args->found_graph_pointer->strokes = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_graph_pointer->text);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    unsigned int words = 0u;
    const char *word = args->query;
    while ((word[0] != '\0') && (words < CLI_GIT_QUERY_WORDS))
    {
        word += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = word, .at = 0ull,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}),
                           .set = CLI_TEXT_WHITE);
        const char *end = word;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ull,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}),
                          .set = CLI_TEXT_WHITE);
        if (end > word)
        {
            VerbaLinea line = {s_words[words], CLI_NAME_MAX_LEN, 0ull};
            EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = word,
                       .length = (unsigned long long)(end - word));
            mmgr_line_finish(&line);
            words += 1u;
        }
        word = end;
    }
    static const char *const s_format = "--format=%H%x1f%P%x1f%an%x1f%at%x1f%D%x1f%s";
    const char *const drawn[5] = {"log", "-n2000", "--all", "--date-order", s_format};
    const char *const searched[4] = {"log", "--all", "--date-order", s_format};
    CliGitGave *const gave = cli_git_take();
    const int queried = (words == 0u) ? EMBED_CALL(cli_git_gave_internal.query, CliGitGaveCtx, .root = args->root,
                                                   .request = drawn, .count = 5u, .gave = gave)
                                      : EMBED_CALL(cli_git_gave_internal.query, CliGitGaveCtx, .root = args->root,
                                                   .request = searched, .count = 4u, .gave = gave);
    const char *line = gave->out;
    const char *const end = gave->out + gave->out_size;
    while (queried && (line < end) && (args->found_graph_pointer->count < CLI_GIT_GRAPH))
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        // a commit the search does not find leaves its texts in place, and is taken back
        const unsigned long long used = args->found_graph_pointer->text.used;
        if (EMBED_CALL(cli_draw_put, GitGraphCtx, .found = args->found_graph_pointer, .line_text = line, .stop = stop))
        {
            if ((words == 0u) ||
                EMBED_CALL(cli_commit_matches, GitGraphCtx, .found_read = args->found_graph_pointer,
                           .drawn_read = &args->found_graph_pointer->item[args->found_graph_pointer->count],
                           .authors = args->authors, .words = s_words, .count_value = words))
            {
                args->found_graph_pointer->count += 1u;
            }
            else
            {
                args->found_graph_pointer->text.used = used;
            }
        }
        line = next;
    }
    EMBED_CALL(cli_git_gave_internal.free, CliGitGaveCtx, .gave = gave);
    if (words == 0u)
    {
        EMBED_CALL(cli_git_found_internal.layout_out, CliGitFoundCtx, .found = args->found_graph_pointer);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
