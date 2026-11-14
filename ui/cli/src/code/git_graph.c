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
    int *opened;
    unsigned int *colors;
    unsigned int lane_at;
    unsigned int nth;
    unsigned int parent;
} GitGraphCtx;
// `length` bytes of `text` put in lower case after `line`
EMBED_INLINE void cli_on_lower(const GitGraphCtx *args)
{
    VerbaLinea *line = args->line;
    const char *text = args->text;
    static unsigned int s_letters[CLI_GIT_GRAPH_TEXT_MAX];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = args->length, .letters = s_letters,
                   .capacity_u64 = CLI_GIT_GRAPH_TEXT_MAX);
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = line,
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
    const unsigned int far = (args->from > args->to) ? args->from : args->to;
    args->drawn->across = (far + 1u > args->drawn->across) ? far + 1u : args->drawn->across;
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

// the first of the `count_value` columns of `lanes` that waits for the commit whose id is `text`, CLI_GIT_LANES where
// none waits
EMBED_INLINE unsigned int cli_lane_waiting(const GitGraphCtx *args)
{
    for (unsigned int at = 0u; at < args->count_value; at += 1u)
    {
        if (args->lanes[at].has &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                     .text_read = &args->found_read->text, .at = args->lanes[at].id),
                                   .other = args->text,
                                   .cap = CLI_POOL_BYTES})))
        {
            return at;
        }
    }
    return CLI_GIT_LANES;
}

// the top half of the row of `drawn`, the commit whose id is `text` in column `lane_at`: each of the `count` columns of
// `lanes` drawn on, those waiting for the commit bent into its column and ended there, and none `opened` yet
EMBED_INLINE void cli_lanes_met(const GitGraphCtx *args)
{
    CliLane *const lanes = args->lanes;
    for (unsigned int at = 0u; at < *args->count; at += 1u)
    {
        args->opened[at] = 0;
        if (!lanes[at].has)
        {
            continue;
        }
        const int waits =
            cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                           .text_read = &args->found->text, .at = lanes[at].id),
                                         .other = args->text,
                                         .cap = CLI_POOL_BYTES});
        EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = args->drawn, .from = at,
                   .to = waits ? args->lane_at : at, .half = 0u, .color = lanes[at].color);
        if (waits)
        {
            lanes[at].has = 0;
        }
    }
}

// the `nth` parent `parent` of the commit of `drawn`, in column `lane_at` of `color`, drawn in the bottom half of its
// row: joined to the column waiting for it; else the first parent carried on in the commit's column, and a further one
// in a free column of its own with the next of the `colors`, each `opened`
EMBED_INLINE void cli_parent_joined(const GitGraphCtx *args)
{
    CliLane *const lanes = args->lanes;
    const unsigned int lane_at = args->lane_at;
    const unsigned int joins = EMBED_CALL(cli_lane_waiting, GitGraphCtx, .lanes = lanes, .count_value = *args->count,
                                          .found_read = args->found,
                                          .text = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                             .text_read = &args->found->text, .at = args->parent));
    if (joins < CLI_GIT_LANES)
    {
        EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = args->drawn, .from = lane_at, .to = joins,
                   .half = 1u, .color = lanes[joins].color);
    }
    else if (args->nth == 0u)
    {
        lanes[lane_at] = (CliLane){1, args->parent, args->color};
        args->opened[lane_at] = 1;
        EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = args->drawn, .from = lane_at, .to = lane_at,
                   .half = 1u, .color = args->color);
    }
    else
    {
        const unsigned int at = EMBED_CALL(cli_free, GitGraphCtx, .lanes = lanes, .count = args->count);
        lanes[at] = (CliLane){1, args->parent, *args->colors};
        args->opened[at] = 1;
        EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = args->drawn, .from = lane_at, .to = at,
                   .half = 1u, .color = *args->colors);
        *args->colors += 1u;
    }
}

// the bottom half of the row of `drawn` for each of the `count` columns of `lanes` that goes on past it: those waiting
// that no parent `opened`
EMBED_INLINE void cli_lanes_carried(const GitGraphCtx *args)
{
    for (unsigned int at = 0u; at < *args->count; at += 1u)
    {
        if (args->lanes[at].has && !args->opened[at])
        {
            EMBED_CALL(cli_stroke, GitGraphCtx, .found = args->found, .drawn = args->drawn, .from = at, .to = at,
                       .half = 1u, .color = args->lanes[at].color);
        }
    }
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
        commit->across = 0u;
        unsigned int lane_at = EMBED_CALL(cli_lane_waiting, GitGraphCtx, .lanes = s_lanes, .count_value = lanes,
                                          .found_read = args->found, .text = id);
        unsigned int color = 0u;
        if (lane_at < CLI_GIT_LANES)
        {
            color = s_lanes[lane_at].color;
        }
        else
        {
            lane_at = EMBED_CALL(cli_free, GitGraphCtx, .lanes = s_lanes, .count = &lanes);
            color = colors;
            colors += 1u;
        }
        EMBED_CALL(cli_lanes_met, GitGraphCtx, .lanes = s_lanes, .count = &lanes, .opened = s_opened,
                   .found = args->found, .drawn = commit, .text = id, .lane_at = lane_at);
        for (unsigned int nth = 0u; nth < commit->parents; nth += 1u)
        {
            EMBED_CALL(cli_parent_joined, GitGraphCtx, .lanes = s_lanes, .count = &lanes, .opened = s_opened,
                       .colors = &colors, .found = args->found, .drawn = commit, .lane_at = lane_at, .color = color,
                       .nth = nth, .parent = commit->parent[nth]);
        }
        EMBED_CALL(cli_lanes_carried, GitGraphCtx, .lanes = s_lanes, .count = &lanes, .opened = s_opened,
                   .found = args->found, .drawn = commit);
        while ((lanes > 0u) && !s_lanes[lanes - 1u].has)
        {
            lanes -= 1u;
        }
        commit->lane_at = lane_at;
        commit->color = color;
        commit->across = (lane_at + 1u > commit->across) ? lane_at + 1u : commit->across;
    }
}

// whether a commit's subject, author where `authors`, id and names, in lower case, contain each word of the query
EMBED_INLINE int cli_commit_matches(const GitGraphCtx *args)
{
    static char s_lower_text[CLI_GIT_GRAPH_TEXT_MAX];
    VerbaLinea line = {s_lower_text, sizeof(s_lower_text), 0ULL};
    const char *const subject = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                           .text_read = &args->found_read->text, .at = args->drawn_read->subject);
    const char *const author = args->authors
                                   ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                .text_read = &args->found_read->text, .at = args->drawn_read->author)
                                   : "";
    const char *const id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                      .text_read = &args->found_read->text, .at = args->drawn_read->id);
    EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = subject,
               .length = cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_POOL_BYTES}));
    mmgr_line_ch(&line, ' ');
    EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = author,
               .length = cellul.len(&(CatenaFinitaCfg){.src = author, .cap = CLI_POOL_BYTES}));
    mmgr_line_ch(&line, ' ');
    EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_POOL_BYTES}));
    mmgr_line_ch(&line, ' ');
    for (unsigned int at = 0u; at < args->drawn_read->refs; at += 1u)
    {
        const char *const name = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &args->found_read->text, .at = args->drawn_read->ref[at]);
        mmgr_line_put(&line, (at != 0u) ? " " : "");
        EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    }
    mmgr_line_finish(&line);
    for (unsigned int at = 0u; at < args->count_value; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){
                .src = s_lower_text, .cap = CLI_POOL_BYTES, .other = args->words[at], .other_cap = CLI_POOL_BYTES}) ==
            NULL)
        {
            return 0;
        }
    }
    return 1;
}

// the commit of a line `id\x1fparents\x1fauthor\x1fwhen\x1fnames\x1fsubject` put after those found: 1, or 0 where the
// line is no commit's
EMBED_INLINE int cli_draw_append(const GitGraphCtx *args)
{
    const char *stop = args->stop;
    const char *at = args->line_text;
    unsigned long long lengths[5] = {0ULL, 0ULL, 0ULL, 0ULL, 0ULL};
    const char *fields[5] = {NULL, NULL, NULL, NULL, NULL};
    for (unsigned int field = 0u; field < 5u; field += 1u)
    {
        fields[field] = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop, .stop = '\x1f',
                                   .length_pointer = &lengths[field]);
    }
    long long when = 0LL;
    if ((fields[4] == NULL) || !EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = fields[3],
                                           .length = lengths[3], .value = &when))
    {
        return 0;
    }
    CliDrawn *const drawn = &args->found->item[args->found->count];
    drawn->id = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = fields[0],
                           .length = lengths[0]);
    drawn->parents = 0u;
    const char *word_text = fields[1];
    const char *const parents_end = fields[1] + lengths[1];
    while (word_text < parents_end)
    {
        word_text += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = word_text,
                                .at = 0ULL, .size = (unsigned long long)(parents_end - word_text), .set = " ");
        const char *word_past = word_text;
        word_past += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = word_past, .at = 0ULL,
                                .size = (unsigned long long)(parents_end - word_past), .set = " ");
        if ((word_past > word_text) && (drawn->parents < CLI_GIT_PARENTS))
        {
            drawn->parent[drawn->parents] =
                EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = word_text,
                           .length = (unsigned long long)(word_past - word_text));
            drawn->parents += 1u;
        }
        word_text = word_past;
    }
    drawn->author = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = fields[2],
                               .length = lengths[2]);
    drawn->when = when;
    drawn->refs = 0u;
    const char *name = fields[4];
    const char *const names_end = fields[4] + lengths[4];
    while (name < names_end)
    {
        const char *name_end =
            name + EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = name, .at = 0ULL,
                              .size = (unsigned long long)(names_end - name), .first = ',', .second = ' ');
        name_end = ((name_end + 1) < names_end) ? name_end : names_end;
        if ((name_end > name) && (drawn->refs < CLI_GIT_REFS))
        {
            drawn->ref[drawn->refs] = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text,
                                                 .from = name, .length = (unsigned long long)(name_end - name));
            drawn->refs += 1u;
        }
        name = name_end + 2;
    }
    drawn->subject = (at <= stop) ? EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text,
                                               .from = at, .length = (unsigned long long)(stop - at))
                                  : 0u;
    drawn->lane_at = 0u;
    drawn->color = 0u;
    drawn->first_stroke = 0u;
    drawn->stroke_count = 0u;
    drawn->across = 0u;
    return 1;
}

void cli_git_graph(const CliGitRootQueryCfg *args)
{
    static char s_words[CLI_GIT_QUERY_WORDS][CLI_NAME_MAX_LEN];
    args->found_graph_pointer->count = 0u;
    args->found_graph_pointer->strokes = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_graph_pointer->text);
    unsigned int words = 0u;
    const char *word_text = args->query;
    while ((word_text[0] != '\0') && (words < CLI_GIT_QUERY_WORDS))
    {
        word_text += EMBED_CALL(
            cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = word_text, .at = 0ULL,
            .size = cellul.len(&(CatenaFinitaCfg){.src = word_text, .cap = CLI_POOL_BYTES}), .set = CLI_TEXT_WHITE);
        const char *end = word_text;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_POOL_BYTES}),
                          .set = CLI_TEXT_WHITE);
        if (end > word_text)
        {
            VerbaLinea line = {s_words[words], CLI_NAME_MAX_LEN, 0ULL};
            EMBED_CALL(cli_on_lower, GitGraphCtx, .line = &line, .text = word_text,
                       .length = (unsigned long long)(end - word_text));
            mmgr_line_finish(&line);
            words += 1u;
        }
        word_text = end;
    }
    static const char *const s_format = "--format=%H%x1f%P%x1f%an%x1f%at%x1f%D%x1f%s";
    const char *const drawn[5] = {"log", "-n2000", "--all", "--date-order", s_format};
    const char *const searched[4] = {"log", "--all", "--date-order", s_format};
    CliGitResult *const result = cli_git_alloc();
    const int queried = (words == 0u) ? EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root,
                                                   .request = drawn, .count = 5u, .result = result)
                                      : EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root,
                                                   .request = searched, .count = 4u, .result = result);
    const char *line = result->out;
    const char *const end = result->out + result->out_size;
    while (queried && (line < end) && (args->found_graph_pointer->count < CLI_GIT_GRAPH))
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        // a commit the search does not find leaves its texts in place, and is taken back
        const unsigned long long used = args->found_graph_pointer->text.used;
        if (EMBED_CALL(cli_draw_append, GitGraphCtx, .found = args->found_graph_pointer, .line_text = line,
                       .stop = stop))
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
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    if (words == 0u)
    {
        EMBED_CALL(cli_git_found_internal.layout_out, CliGitFoundCtx, .found = args->found_graph_pointer);
    }
    EMBED_CALL(cli_git_text.text_end, CliGitTextCfg, .text = &args->found_graph_pointer->text);
}
