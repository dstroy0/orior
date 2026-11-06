// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// leaves.c: what a run leaves compared with what the tree has, shown and confirmed before the writer writes it in
// place: a text file apart a line diff, a record file its byte counts; `y`, `n`, `all` or `none` at the terminal, or
// `--leave` where no one is there. A refused file is handed back as the tree has it with the writer's `file` word,
// so that the one writer still writes every current file and the tree's copy comes out unchanged. Nothing here
// touches the disk
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

// A diff of two texts' lines, each part in a cell of the texts' counts given back as the diff is: the lines of each
// side and their lengths; the shortest edit script's reach, Myers' way, the furthest x on each diagonal k recorded at
// k + `reach_middle`, and its trace, the reach on diagonals -d to d before each step d, step d's band from d * d; and
// each line of the diff, 0 the same, 1 added by the run, 2 removed by it, and the line, in order
typedef struct
{
    const char **before;
    unsigned int *before_length;
    unsigned int n;
    const char **after;
    unsigned int *after_length;
    unsigned int m;
    int *reach;
    int reach_middle;
    int *band;
    unsigned char *change;
    const char **change_line;
    unsigned int *change_length;
} QueryDiff;
// the most bytes shown of a file, and the most a byte of a file shows as: an empty line is 1 byte, its lead and its
// break 2, and a fold of the lines beside it its own line
#define QUERY_DISPLAYED_MAX (8ULL << 20u)
#define QUERY_DISPLAYED_WIDER 16ULL

static QueryAnswer query_terminal_prompt(const CliEachPathCfg *args)
{
    for (;;)
    {
        char question[QUERY_PATH_MAX_LEN + 64u];
        VerbaLinea line = {question, sizeof(question), 0ULL};
        mmgr_line_put(&line, "leave ");
        mmgr_line_put(&line, args->path);
        mmgr_line_put(&line, " as the run leaves it? [y/n/all/none] ");
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = question,
                   .length = line.at);
        char answer[64];
        const unsigned long long read = EMBED_CALL(query_capacity_line_internal.terminal_line_read,
                                                   QueryCapacityLineCtx, .line = answer, .capacity = sizeof(answer));
        if ((read == QRY_MEMBER_UNREAD) || (read == 0ULL))
        {
            return QUERY_ANSWER_NONE;
        }
        // an answer longer than any of the four is no answer, and the question is repeated again
        if (read >= sizeof(answer))
        {
            continue;
        }
        const char *const word_text =
            answer + EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = answer, .set = " \t");
        const size_t length = EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = word_text, .set = " \t");
        if (((length == 1u) && (word_text[0] == 'y')) ||
            ((length == 3u) &&
             (cellul.starts(&(CatenaFinitaCfg){.src = word_text, .other = "yes", .cap = CLI_POOL_BYTES}))))
        {
            return QUERY_ANSWER_YES;
        }
        if (((length == 1u) && (word_text[0] == 'n')) ||
            ((length == 2u) &&
             (cellul.starts(&(CatenaFinitaCfg){.src = word_text, .other = "no", .cap = CLI_POOL_BYTES}))))
        {
            return QUERY_ANSWER_NO;
        }
        if ((length == 3u) &&
            (cellul.starts(&(CatenaFinitaCfg){.src = word_text, .other = "all", .cap = CLI_POOL_BYTES})))
        {
            return QUERY_ANSWER_ALL;
        }
        if ((length == 4u) &&
            (cellul.starts(&(CatenaFinitaCfg){.src = word_text, .other = "none", .cap = CLI_POOL_BYTES})))
        {
            return QUERY_ANSWER_NONE;
        }
    }
}

QueryPrompter query_terminal(void)
{
    const QueryPrompter prompter = {query_terminal_prompt, NULL};
    return prompter;
}

typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    const char **line;
    unsigned int *length;
    const char *a;
    unsigned int a_length;
    const char *b;
    unsigned int b_length;
    unsigned long long count;
    size_t size_size;
    const char *what;
    QueryDiff *diff;
    const unsigned char *before;
    unsigned long long before_size;
    const unsigned char *after;
    unsigned long long after_size;
    const QueryDiff *diff_read;
    unsigned int first;
    unsigned int count_u32;
    VerbaLinea *displayed;
    unsigned int changes;
    const QueryRun *run_read;
    unsigned int at;
    QueryContext *context;
    QueryLeave leave;
    QueryAnswer answer;
    const QueryPrompter *prompter;
    VerbaLinea *err_msg;
} LeavesCtx;
// the lines of `size` bytes at `bytes` cut at their ends into `line` and `length`, or counted where `line` is NULL:
// their count
EMBED_INLINE unsigned int query_lines_cut(const LeavesCtx *args)
{
    const unsigned char *bytes = args->bytes;
    unsigned int count = 0u;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const unsigned char *const newline = (const unsigned char *)memor.chr(
            &(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = (size_t)(args->size - at)});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : args->size;
        unsigned long long cut = end - at;
        // a line read as Rust's `lines` reads it: its `\r` before the `\n` left off
        cut -= ((cut != 0ULL) && (bytes[at + cut - 1ULL] == '\r')) ? 1ULL : 0ULL;
        if (args->line != NULL)
        {
            args->line[count] = (const char *)(bytes + at);
            // a line is far shorter than an unsigned int counts
            args->length[count] = (unsigned int)cut;
        }
        count += 1u;
        at = end + 1ULL;
    }
    return count;
}

// 1 where line `a` of one side and line `b` of the other are the same
EMBED_INLINE int query_line_same(const LeavesCtx *args)
{
    return (args->a_length == args->b_length) &&
           (memor.cmp(&(MemoriaCfg){.src = args->a, .other = args->b, .bytes = args->a_length}) == 0);
}

// the most edits a diff walks: past them a text file is shown by its byte counts
#define QUERY_DIFF_EDITS 2048u

#define QUERY_REACH(diff_, k_) (diff_)->reach[(k_) + (diff_)->reach_middle]

// the reach on diagonal `k` before step `d`, as the band of that step records it
#define QUERY_TRACED(diff_, d_, k_) (diff_)->band[((d_) * (d_)) + ((k_) + (d_))]

// `count` items of `size` bytes each in a cell of their own, NULL where none are or the cells cannot meet them
EMBED_INLINE void *query_diff_part(const LeavesCtx *args)
{
    size_t size = args->size_size;
    return (args->count > 0ULL) ? g_cli_guard->persistent_buf_alloc(args->count * size, g_query_owner) : NULL;
}

// the lines of `before` and `after` counted, then cut into cells of their counts, with the reach over every diagonal
// they span: 1, or 0 where the cells cannot meet them
EMBED_INLINE int query_diff_alloc(const LeavesCtx *args)
{
    const unsigned char *before = args->before;
    const unsigned char *after = args->after;
    memor.set(&(MemoriaCfg){.dst = args->diff, .val = 0, .bytes = sizeof(*args->diff)});
    args->diff->n = EMBED_CALL(query_lines_cut, LeavesCtx, .bytes = before, .size = args->before_size, .line = NULL,
                               .length = NULL);
    args->diff->m =
        EMBED_CALL(query_lines_cut, LeavesCtx, .bytes = after, .size = args->after_size, .line = NULL, .length = NULL);
    // the counts of two texts' lines, which an int represents
    args->diff->reach_middle = (int)(args->diff->n + args->diff->m) + 1;
    args->diff->before = EMBED_CALL(query_diff_part, LeavesCtx, .count = args->diff->n, .size_size = sizeof(char *),
                                    .what = "a diff's lines before");
    args->diff->before_length = EMBED_CALL(query_diff_part, LeavesCtx, .count = args->diff->n,
                                           .size_size = sizeof(unsigned int), .what = "a diff's lines before");
    args->diff->after = EMBED_CALL(query_diff_part, LeavesCtx, .count = args->diff->m, .size_size = sizeof(char *),
                                   .what = "a diff's lines after");
    args->diff->after_length = EMBED_CALL(query_diff_part, LeavesCtx, .count = args->diff->m,
                                          .size_size = sizeof(unsigned int), .what = "a diff's lines after");
    args->diff->reach =
        EMBED_CALL(query_diff_part, LeavesCtx, .count = 2ULL * (unsigned long long)args->diff->reach_middle + 1ULL,
                   .size_size = sizeof(int), .what = "a diff's reach");
    const int reserved =
        ((args->diff->n == 0u) || ((args->diff->before != NULL) && (args->diff->before_length != NULL))) &&
        ((args->diff->m == 0u) || ((args->diff->after != NULL) && (args->diff->after_length != NULL))) &&
        (args->diff->reach != NULL);
    if (reserved)
    {
        (void)EMBED_CALL(query_lines_cut, LeavesCtx, .bytes = before, .size = args->before_size,
                         .line = args->diff->before, .length = args->diff->before_length);
        (void)EMBED_CALL(query_lines_cut, LeavesCtx, .bytes = after, .size = args->after_size,
                         .line = args->diff->after, .length = args->diff->after_length);
    }
    return reserved;
}

// the shortest edit script of the diff's lines, Myers' way, each step's furthest reach traced into the band where
// the diff has one: its steps, or -1 where it takes more than QUERY_DIFF_EDITS edits
EMBED_INLINE int query_diff_steps(const LeavesCtx *args)
{
    // the counts of two texts' lines, which an int represents
    const int lines_before = (int)args->diff->n;
    const int lines_after = (int)args->diff->m;
    const int most = lines_before + lines_after;
    memor.set(&(MemoriaCfg){
        .dst = args->diff->reach, .val = 0, .bytes = ((2u * (size_t)args->diff->reach_middle) + 1u) * sizeof(int)});
    for (int d = 0; (d <= most) && (d <= (int)QUERY_DIFF_EDITS); d += 1)
    {
        for (int k = -d; (args->diff->band != NULL) && (k <= d); k += 1)
        {
            QUERY_TRACED(args->diff, d, k) = QUERY_REACH(args->diff, k);
        }
        for (int k = -d; k <= d; k += 2)
        {
            int x = ((k == -d) || ((k != d) && (QUERY_REACH(args->diff, k - 1) < QUERY_REACH(args->diff, k + 1))))
                        ? QUERY_REACH(args->diff, k + 1)
                        : (QUERY_REACH(args->diff, k - 1) + 1);
            int y = x - k;
            while ((x < lines_before) && (y < lines_after) &&
                   EMBED_CALL(query_line_same, LeavesCtx, .a = args->diff->before[x],
                              .a_length = args->diff->before_length[x], .b = args->diff->after[y],
                              .b_length = args->diff->after_length[y]))
            {
                x += 1;
                y += 1;
            }
            QUERY_REACH(args->diff, k) = x;
            if ((x >= lines_before) && (y >= lines_after))
            {
                return d;
            }
        }
    }
    return -1;
}

// The diff's lines before against after: the shortest edit script's steps found, then walked again with its trace
// in a band of their square, and walked back from the end into the diff's changes, each in a cell of the lines the
// script leaves. Their count, or 0 where the script takes more than QUERY_DIFF_EDITS edits or the cells cannot meet it
EMBED_INLINE unsigned int query_diff(const LeavesCtx *args)
{
    const int steps = query_diff_steps(args);
    if (steps < 0)
    {
        return 0u;
    }
    // a step bounded by QUERY_DIFF_EDITS, whose square is far inside what a size_t counts
    const unsigned long long traced = ((unsigned long long)steps + 1ULL) * ((unsigned long long)steps + 1ULL);
    args->diff->band =
        EMBED_CALL(query_diff_part, LeavesCtx, .count = traced, .size_size = sizeof(int), .what = "a diff's trace");
    // the lines of the script: each the same once, and each step a line added or removed
    const unsigned int lines = (args->diff->n + args->diff->m + (unsigned int)steps) / 2u;
    args->diff->change = EMBED_CALL(query_diff_part, LeavesCtx, .count = lines, .size_size = sizeof(unsigned char),
                                    .what = "a diff's changes");
    args->diff->change_line =
        EMBED_CALL(query_diff_part, LeavesCtx, .count = lines, .size_size = sizeof(char *), .what = "a diff's changes");
    args->diff->change_length = EMBED_CALL(query_diff_part, LeavesCtx, .count = lines,
                                           .size_size = sizeof(unsigned int), .what = "a diff's changes");
    if ((lines == 0u) || (args->diff->band == NULL) || (args->diff->change == NULL) ||
        (args->diff->change_line == NULL) || (args->diff->change_length == NULL))
    {
        return 0u;
    }
    (void)query_diff_steps(args);
    // walked back from the end, each step's changes put down from the end
    unsigned int count = lines;
    int x = (int)args->diff->n;
    int y = (int)args->diff->m;
    for (int d = steps; d >= 0; d -= 1)
    {
        const int k = x - y;
        const int previous_k =
            ((k == -d) || ((k != d) && (QUERY_TRACED(args->diff, d, k - 1) < QUERY_TRACED(args->diff, d, k + 1))))
                ? (k + 1)
                : (k - 1);
        const int previous_x = (d > 0) ? QUERY_TRACED(args->diff, d, previous_k) : 0;
        const int previous_y = previous_x - previous_k;
        while ((x > previous_x) && (y > previous_y))
        {
            x -= 1;
            y -= 1;
            count -= 1u;
            args->diff->change[count] = 0u;
            args->diff->change_line[count] = args->diff->before[x];
            args->diff->change_length[count] = args->diff->before_length[x];
        }
        if (d > 0)
        {
            count -= 1u;
            if (x == previous_x)
            {
                y -= 1;
                args->diff->change[count] = 1u;
                args->diff->change_line[count] = args->diff->after[y];
                args->diff->change_length[count] = args->diff->after_length[y];
            }
            else
            {
                x -= 1;
                args->diff->change[count] = 2u;
                args->diff->change_line[count] = args->diff->before[x];
                args->diff->change_length[count] = args->diff->before_length[x];
            }
        }
    }
    return lines;
}

// a run of `count` unchanged lines of the diff from `first` written, each on a line of its own begun by its break:
// more than six folded to the first three, a count, and the last three
EMBED_INLINE void query_same_flush(const LeavesCtx *args)
{
    for (unsigned int at = 0u; at < args->count_u32; at += 1u)
    {
        if ((args->count_u32 > 6u) && (at == 3u))
        {
            mmgr_line_put(args->displayed, "\n ... ");
            mmgr_line_decimal(args->displayed, (unsigned long long)args->count_u32 - 6ULL, 0u);
            mmgr_line_put(args->displayed, " lines the same ...");
            at = args->count_u32 - 4u;
            continue;
        }
        mmgr_line_put(args->displayed, "\n ");
        mmgr_line_put_n(args->displayed, args->diff_read->change_line[args->first + at],
                        args->diff_read->change_length[args->first + at]);
    }
}

// the diff's changes as lines, each begun by its break, `-`, `+` and ` ` led, a run of more than six unchanged lines
// folded
EMBED_INLINE void query_render(const LeavesCtx *args)
{
    unsigned int same_first = 0u;
    unsigned int same = 0u;
    for (unsigned int at = 0u; at < args->changes; at += 1u)
    {
        if (args->diff_read->change[at] == 0u)
        {
            same_first = (same == 0u) ? at : same_first;
            same += 1u;
            continue;
        }
        EMBED_CALL(query_same_flush, LeavesCtx, .diff_read = args->diff_read, .first = same_first, .count_u32 = same,
                   .displayed = args->displayed);
        same = 0u;
        mmgr_line_ch(args->displayed, '\n');
        mmgr_line_ch(args->displayed, (args->diff_read->change[at] == 1u) ? '+' : '-');
        mmgr_line_put_n(args->displayed, args->diff_read->change_line[at], args->diff_read->change_length[at]);
    }
    EMBED_CALL(query_same_flush, LeavesCtx, .diff_read = args->diff_read, .first = same_first, .count_u32 = same,
               .displayed = args->displayed);
}

void query_leaf_show(const QueryPathTreeCtx *args)
{
    static const char *const s_text[] = {"khw", "krs", "klq", "klm", "md", "tsv", "txt", "h", "gsm", NULL};
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .byte = '.');
    int is_text = 0;
    for (unsigned int at = 0u; (dot != NULL) && (s_text[at] != NULL); at += 1u)
    {
        is_text =
            is_text || (cellul.eq(&(CatenaFinitaCfg){.src = dot + 1, .other = s_text[at], .cap = CLI_POOL_BYTES}));
    }
    if (is_text)
    {
        QueryDiff diff;
        const int reserved =
            EMBED_CALL(query_diff_alloc, LeavesCtx, .diff = &diff, .before = args->tree, .before_size = args->tree_size,
                       .after = args->stored, .after_size = args->stored_size);
        const unsigned int changes = reserved ? EMBED_CALL(query_diff, LeavesCtx, .diff = &diff) : 0u;
        if (reserved && ((changes != 0u) || ((diff.n == 0u) && (diff.m == 0u))))
        {
            mmgr_line_put(args->displayed, name);
            mmgr_line_ch(args->displayed, ':');
            EMBED_CALL(query_render, LeavesCtx, .diff_read = &diff, .changes = changes, .displayed = args->displayed);
            g_cli_guard->persistent_buf_release((void *)diff.before, g_query_owner);
            g_cli_guard->persistent_buf_release(diff.before_length, g_query_owner);
            g_cli_guard->persistent_buf_release((void *)diff.after, g_query_owner);
            g_cli_guard->persistent_buf_release(diff.after_length, g_query_owner);
            g_cli_guard->persistent_buf_release(diff.reach, g_query_owner);
            g_cli_guard->persistent_buf_release(diff.band, g_query_owner);
            g_cli_guard->persistent_buf_release(diff.change, g_query_owner);
            g_cli_guard->persistent_buf_release((void *)diff.change_line, g_query_owner);
            g_cli_guard->persistent_buf_release(diff.change_length, g_query_owner);
            memor.set(&(MemoriaCfg){.dst = (&diff), .val = 0, .bytes = sizeof(*(&diff))});
            return;
        }
        g_cli_guard->persistent_buf_release((void *)diff.before, g_query_owner);
        g_cli_guard->persistent_buf_release(diff.before_length, g_query_owner);
        g_cli_guard->persistent_buf_release((void *)diff.after, g_query_owner);
        g_cli_guard->persistent_buf_release(diff.after_length, g_query_owner);
        g_cli_guard->persistent_buf_release(diff.reach, g_query_owner);
        g_cli_guard->persistent_buf_release(diff.band, g_query_owner);
        g_cli_guard->persistent_buf_release(diff.change, g_query_owner);
        g_cli_guard->persistent_buf_release((void *)diff.change_line, g_query_owner);
        g_cli_guard->persistent_buf_release(diff.change_length, g_query_owner);
        memor.set(&(MemoriaCfg){.dst = (&diff), .val = 0, .bytes = sizeof(*(&diff))});
    }
    mmgr_line_put(args->displayed, name);
    if (args->stored_size > args->tree_size)
    {
        mmgr_line_put(args->displayed, " grew by ");
        mmgr_line_decimal(args->displayed, args->stored_size - args->tree_size, 0u);
        mmgr_line_put(args->displayed, " bytes");
        return;
    }
    mmgr_line_put(args->displayed, ": ");
    mmgr_line_decimal(args->displayed, args->tree_size, 0u);
    mmgr_line_put(args->displayed, " bytes in the tree, ");
    mmgr_line_decimal(args->displayed, args->stored_size, 0u);
    mmgr_line_put(args->displayed, " as the run leaves it");
}

// the path at `at` among those `run_read` read and then those it may leave
EMBED_INLINE const char *query_run_path(const LeavesCtx *args)
{
    const unsigned int reads = args->run_read->read.count;
    return (args->at < reads) ? args->run_read->read.path[args->at] : args->run_read->left.path[args->at - reads];
}

// whether the path at `at` among `run_read`'s is named before it
EMBED_INLINE int query_named_before(const LeavesCtx *args)
{
    const char *const path = query_run_path(args);
    for (unsigned int before = 0u; before < args->at; before += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = EMBED_CALL(query_run_path, LeavesCtx, .run_read = args->run_read, .at = before),
                .other = path,
                .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// the file `what` as the run leaves it, the `after_size` bytes at `after`, shown against the `before_size` bytes at
// `before` the tree has, NULL where it has none, and printed to `context`: what is shown is in a cell of the most it
// writes to, up to the most shown, a line that fills it holding what fit and its 0
EMBED_INLINE void query_leaf_printed(const LeavesCtx *args)
{
    const unsigned long long required =
        ((args->before_size + args->after_size) * QUERY_DISPLAYED_WIDER) + QUERY_PATH_MAX_LEN + 256ULL;
    const unsigned long long capacity = (required < QUERY_DISPLAYED_MAX) ? required : QUERY_DISPLAYED_MAX;
    char *const text = g_cli_guard->temporary_buf_alloc(capacity, g_query_owner);
    VerbaLinea displayed = mmgr_line_over(text, capacity);
    EMBED_CALL(query_path_tree_internal.leaf_show, QueryPathTreeCtx, .path = args->what,
               .tree = (args->before != NULL) ? args->before : (const unsigned char *)"",
               .tree_size = args->before_size, .stored = args->after, .stored_size = args->after_size,
               .displayed = &displayed);
    if (text != NULL)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = text);
    }
    g_cli_guard->persistent_buf_release(text, g_query_owner);
}

// the answer for the file `what`: yes where every file is left, no where none is, else the `prompter`'s
EMBED_INLINE QueryAnswer query_leave_answer(const LeavesCtx *args)
{
    if (args->leave == QUERY_LEAVE_ALL)
    {
        return QUERY_ANSWER_YES;
    }
    if (args->leave == QUERY_LEAVE_NONE)
    {
        return QUERY_ANSWER_NO;
    }
    return EMBED_CALL(args->prompter->prompt, CliEachPathCfg, .context = args->prompter->context, .path = args->what);
}

// what every later file is answered with after `answer`, `leave` before it: all after all, none after none
EMBED_INLINE QueryLeave query_leave_after(const LeavesCtx *args)
{
    if (args->answer == QUERY_ANSWER_ALL)
    {
        return QUERY_LEAVE_ALL;
    }
    return (args->answer == QUERY_ANSWER_NONE) ? QUERY_LEAVE_NONE : args->leave;
}

// the file `what` handed back to the writer of `run_read` as the tree has it, the `before_size` bytes at `before`, NULL
// where it has none, and told to `context`; 1, or 0 with why given
EMBED_INLINE int query_leaf_refused(const LeavesCtx *args)
{
    if (!EMBED_CALL(query_path_bytes_size_internal.writer_file, QueryPathBytesSizeCtx,
                    .writer = &args->run_read->writer, .path = args->what,
                    .bytes = (args->before != NULL) ? args->before : (const unsigned char *)"",
                    .size = args->before_size, .err_msg = args->err_msg))
    {
        return 0;
    }
    char left_text[QUERY_PATH_MAX_LEN + 64u];
    VerbaLinea left_line = {left_text, sizeof(left_text), 0ULL};
    mmgr_line_put(&left_line, "  ");
    mmgr_line_put(&left_line, args->what);
    mmgr_line_put(&left_line, " is left as the tree has it");
    if (mmgr_line_finish(&left_line) != 0ULL)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = args->context, .text = left_text);
    }
    return 1;
}

int query_leaves_settle(const QueryContextErrMsgCtx *args)
{
    QueryLeave existing = args->leave;
    const unsigned int count = args->run_read->read.count + args->run_read->left.count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = EMBED_CALL(query_run_path, LeavesCtx, .run_read = args->run_read, .at = at);
        const unsigned char *stored = NULL;
        unsigned long long stored_size = 0ULL;
        // each path once, in the order first named
        if (EMBED_CALL(query_named_before, LeavesCtx, .run_read = args->run_read, .at = at) ||
            !EMBED_CALL(query_count_name_internal.writer_current, QueryCountNameCtx, .writer = &args->run_read->writer,
                        .path = path, .bytes_u8s = &stored, .size_pointer = &stored_size))
        {
            continue;
        }
        unsigned long long tree_size = 0ULL;
        const unsigned char *tree = qry_member_file_mapped(path, &tree_size);
        const int same = (tree_size == stored_size) &&
                         ((tree_size == 0ULL) ||
                          (memor.cmp(&(MemoriaCfg){.src = tree, .other = stored, .bytes = (size_t)stored_size}) == 0));
        int refused = 0;
        if (!same)
        {
            EMBED_CALL(query_leaf_printed, LeavesCtx, .what = path, .before = tree, .before_size = tree_size,
                       .after = stored, .after_size = stored_size, .context = args->context);
            const QueryAnswer answer =
                EMBED_CALL(query_leave_answer, LeavesCtx, .leave = existing, .prompter = &args->prompter, .what = path);
            existing = EMBED_CALL(query_leave_after, LeavesCtx, .leave = existing, .answer = answer);
            refused = (answer == QUERY_ANSWER_NO) || (answer == QUERY_ANSWER_NONE);
        }
        if (refused &&
            !EMBED_CALL(query_leaf_refused, LeavesCtx, .run_read = args->run_read, .what = path, .before = tree,
                        .before_size = tree_size, .context = args->context, .err_msg = args->err_msg))
        {
            qry_member_file_unmapped(tree, tree_size);
            return 0;
        }
        qry_member_file_unmapped(tree, tree_size);
    }
    return 1;
}
