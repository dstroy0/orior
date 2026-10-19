// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// leaves.c: what a run leaves held against what the tree holds, shown and asked before the writer writes it in place:
// a text file apart a line diff, a record file its byte counts; `y`, `n`, `all` or `none` at the terminal, or
// `--leave` where no one is there. A refused file is handed back as the tree holds it with the writer's `file` word,
// so that the one writer still writes every current file and the tree's copy comes out unchanged. Nothing here
// touches the disk
#include "../../../cli/src/text/pieces.h"
#include "../../../cli/src/text/text.h"
#include "../query_internal.h"

// the bytes of each piece what is shown of a file is written in
#define QUERY_SHOWN_PIECE 4096u

static QueryAnswer query_terminal_asked(void *context, const char *path)
{
    (void)context;
    for (;;)
    {
        char question[QUERY_PATH_LONGEST + 64u];
        CliLine line = {question, sizeof(question), 0ull};
        cli_line_text(&line, "leave ");
        cli_line_text(&line, path);
        cli_line_text(&line, " as the run leaves it? [y/n/all/none] ");
        query_terminal_written(1, question, line.at);
        char answer[64];
        const unsigned long long read = query_terminal_line_read(answer, sizeof(answer));
        if ((read == QRY_MEMBER_UNREAD) || (read == 0ull))
        {
            return QUERY_ANSWER_NONE;
        }
        // an answer longer than any of the four is no answer, and the question is asked again
        if (read >= sizeof(answer))
        {
            continue;
        }
        const char *const word = answer + cli_text_over(answer, " \t");
        const size_t length = cli_text_upto(word, " \t");
        if (((length == 1u) && (word[0] == 'y')) ||
            ((length == 3u) && (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "yes", .cap = CLI_CELLS_POOL}))))
        {
            return QUERY_ANSWER_YES;
        }
        if (((length == 1u) && (word[0] == 'n')) ||
            ((length == 2u) && (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "no", .cap = CLI_CELLS_POOL}))))
        {
            return QUERY_ANSWER_NO;
        }
        if ((length == 3u) && (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "all", .cap = CLI_CELLS_POOL})))
        {
            return QUERY_ANSWER_ALL;
        }
        if ((length == 4u) && (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "none", .cap = CLI_CELLS_POOL})))
        {
            return QUERY_ANSWER_NONE;
        }
    }
}

QueryAsker query_terminal(void)
{
    const QueryAsker asker = {query_terminal_asked, NULL};
    return asker;
}

// the lines of `size` bytes at `bytes` cut at their ends into `line` and `length`, or counted where `line` is NULL:
// their count
static unsigned int query_lines_cut(const unsigned char *bytes, unsigned long long size, const char **line,
                                    unsigned int *length)
{
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned char *const end = (const unsigned char *)memor.chr(
            &(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = (size_t)(size - at)});
        const unsigned long long past = (end != NULL) ? (unsigned long long)(end - bytes) : size;
        unsigned long long cut = past - at;
        // a line read as Rust's `lines` reads it: its `\r` before the `\n` left off
        cut -= ((cut != 0ull) && (bytes[at + cut - 1ull] == '\r')) ? 1ull : 0ull;
        if (line != NULL)
        {
            line[count] = (const char *)(bytes + at);
            // a line is far shorter than an unsigned int counts
            length[count] = (unsigned int)cut;
        }
        count += 1u;
        at = past + 1ull;
    }
    return count;
}

// 1 where line `a` of one side and line `b` of the other are the same
static int query_line_same(const char *a, unsigned int a_length, const char *b, unsigned int b_length)
{
    return (a_length == b_length) && (memor.cmp(&(MemoriaCfg){.src = a, .other = b, .bytes = a_length}) == 0);
}

// the most edits a diff walks: past them a text file is shown by its byte counts
#define QUERY_DIFF_EDITS 2048u

// A diff of two texts' lines, each part in a cell of the texts' counts given back as the diff is: the lines of each
// side and their lengths; the shortest edit script's reach, Myers' way, the furthest x on each diagonal k held at
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

#define QUERY_REACH(diff_, k_) (diff_)->reach[(k_) + (diff_)->reach_middle]

// the reach on diagonal `k` before step `d`, as the band of that step holds it
#define QUERY_TRACED(diff_, d_, k_) (diff_)->band[((d_) * (d_)) + ((k_) + (d_))]

// `count` items of `size` bytes each in a cell of their own, NULL where none are or the cells cannot meet them
static void *query_diff_part(unsigned long long count, size_t size, const char *what)
{
    return (count > 0ull) ? cli_cell_taken_by(&g_query_cells, count * size, what) : NULL;
}

// the diff's parts given back
static void query_diff_given(QueryDiff *diff)
{
    cli_cell_given_by(&g_query_cells, (void *)diff->before);
    cli_cell_given_by(&g_query_cells, diff->before_length);
    cli_cell_given_by(&g_query_cells, (void *)diff->after);
    cli_cell_given_by(&g_query_cells, diff->after_length);
    cli_cell_given_by(&g_query_cells, diff->reach);
    cli_cell_given_by(&g_query_cells, diff->band);
    cli_cell_given_by(&g_query_cells, diff->change);
    cli_cell_given_by(&g_query_cells, (void *)diff->change_line);
    cli_cell_given_by(&g_query_cells, diff->change_length);
    memor.set(&(MemoriaCfg){.dst = diff, .val = 0, .bytes = sizeof(*diff)});
}

// the lines of `before` and `after` counted, then cut into cells of their counts, with the reach over every diagonal
// they span: 1, or 0 where the cells cannot meet them
static int query_diff_taken(QueryDiff *diff, const unsigned char *before, unsigned long long before_size,
                            const unsigned char *after, unsigned long long after_size)
{
    memor.set(&(MemoriaCfg){.dst = diff, .val = 0, .bytes = sizeof(*diff)});
    diff->n = query_lines_cut(before, before_size, NULL, NULL);
    diff->m = query_lines_cut(after, after_size, NULL, NULL);
    // the counts of two texts' lines, which an int holds
    diff->reach_middle = (int)(diff->n + diff->m) + 1;
    diff->before = query_diff_part(diff->n, sizeof(char *), "a diff's lines before");
    diff->before_length = query_diff_part(diff->n, sizeof(unsigned int), "a diff's lines before");
    diff->after = query_diff_part(diff->m, sizeof(char *), "a diff's lines after");
    diff->after_length = query_diff_part(diff->m, sizeof(unsigned int), "a diff's lines after");
    diff->reach = query_diff_part(2ull * (unsigned long long)diff->reach_middle + 1ull, sizeof(int), "a diff's reach");
    const int taken = ((diff->n == 0u) || ((diff->before != NULL) && (diff->before_length != NULL))) &&
                      ((diff->m == 0u) || ((diff->after != NULL) && (diff->after_length != NULL))) &&
                      (diff->reach != NULL);
    if (taken)
    {
        (void)query_lines_cut(before, before_size, diff->before, diff->before_length);
        (void)query_lines_cut(after, after_size, diff->after, diff->after_length);
    }
    return taken;
}

// the shortest edit script of the diff's lines, Myers' way, each step's furthest reach traced into the band where
// the diff holds one: its steps, or -1 where it takes more than QUERY_DIFF_EDITS edits
static int query_diff_steps(QueryDiff *diff)
{
    // the counts of two texts' lines, which an int holds
    const int lines_before = (int)diff->n;
    const int lines_after = (int)diff->m;
    const int most = lines_before + lines_after;
    memor.set(
        &(MemoriaCfg){.dst = diff->reach, .val = 0, .bytes = ((2u * (size_t)diff->reach_middle) + 1u) * sizeof(int)});
    for (int d = 0; (d <= most) && (d <= (int)QUERY_DIFF_EDITS); d += 1)
    {
        for (int k = -d; (diff->band != NULL) && (k <= d); k += 1)
        {
            QUERY_TRACED(diff, d, k) = QUERY_REACH(diff, k);
        }
        for (int k = -d; k <= d; k += 2)
        {
            int x = ((k == -d) || ((k != d) && (QUERY_REACH(diff, k - 1) < QUERY_REACH(diff, k + 1))))
                        ? QUERY_REACH(diff, k + 1)
                        : (QUERY_REACH(diff, k - 1) + 1);
            int y = x - k;
            while ((x < lines_before) && (y < lines_after) &&
                   query_line_same(diff->before[x], diff->before_length[x], diff->after[y], diff->after_length[y]))
            {
                x += 1;
                y += 1;
            }
            QUERY_REACH(diff, k) = x;
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
static unsigned int query_diff(QueryDiff *diff)
{
    const int steps = query_diff_steps(diff);
    if (steps < 0)
    {
        return 0u;
    }
    // a step bounded by QUERY_DIFF_EDITS, whose square is far inside what a size_t counts
    const unsigned long long traced = ((unsigned long long)steps + 1ull) * ((unsigned long long)steps + 1ull);
    diff->band = query_diff_part(traced, sizeof(int), "a diff's trace");
    // the lines of the script: each the same once, and each step a line added or removed
    const unsigned int lines = (diff->n + diff->m + (unsigned int)steps) / 2u;
    diff->change = query_diff_part(lines, sizeof(unsigned char), "a diff's changes");
    diff->change_line = query_diff_part(lines, sizeof(char *), "a diff's changes");
    diff->change_length = query_diff_part(lines, sizeof(unsigned int), "a diff's changes");
    if ((lines == 0u) || (diff->band == NULL) || (diff->change == NULL) || (diff->change_line == NULL) ||
        (diff->change_length == NULL))
    {
        return 0u;
    }
    (void)query_diff_steps(diff);
    // walked back from the end, each step's changes put down from the end
    unsigned int count = lines;
    int x = (int)diff->n;
    int y = (int)diff->m;
    for (int d = steps; d >= 0; d -= 1)
    {
        const int k = x - y;
        const int previous_k =
            ((k == -d) || ((k != d) && (QUERY_TRACED(diff, d, k - 1) < QUERY_TRACED(diff, d, k + 1)))) ? (k + 1)
                                                                                                       : (k - 1);
        const int previous_x = (d > 0) ? QUERY_TRACED(diff, d, previous_k) : 0;
        const int previous_y = previous_x - previous_k;
        while ((x > previous_x) && (y > previous_y))
        {
            x -= 1;
            y -= 1;
            count -= 1u;
            diff->change[count] = 0u;
            diff->change_line[count] = diff->before[x];
            diff->change_length[count] = diff->before_length[x];
        }
        if (d > 0)
        {
            count -= 1u;
            if (x == previous_x)
            {
                y -= 1;
                diff->change[count] = 1u;
                diff->change_line[count] = diff->after[y];
                diff->change_length[count] = diff->after_length[y];
            }
            else
            {
                x -= 1;
                diff->change[count] = 2u;
                diff->change_line[count] = diff->before[x];
                diff->change_length[count] = diff->before_length[x];
            }
        }
    }
    return lines;
}

// a run of `count` unchanged lines of the diff from `first` written, each on a line of its own begun by its break:
// more than six folded to the first three, a count, and the last three
static void query_same_flushed(const QueryDiff *diff, unsigned int first, unsigned int count, CliLine *shown)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((count > 6u) && (at == 3u))
        {
            cli_line_text(shown, "\n ... ");
            cli_line_decimal(shown, (unsigned long long)count - 6ull, 0u);
            cli_line_text(shown, " lines the same ...");
            at = count - 4u;
            continue;
        }
        cli_line_text(shown, "\n ");
        cli_line_counted(shown, diff->change_line[first + at], diff->change_length[first + at]);
    }
}

// the diff's changes as lines, each begun by its break, `-`, `+` and ` ` led, a run of more than six unchanged lines
// folded
static void query_rendered(const QueryDiff *diff, unsigned int changes, CliLine *shown)
{
    unsigned int same_first = 0u;
    unsigned int same = 0u;
    for (unsigned int at = 0u; at < changes; at += 1u)
    {
        if (diff->change[at] == 0u)
        {
            same_first = (same == 0u) ? at : same_first;
            same += 1u;
            continue;
        }
        query_same_flushed(diff, same_first, same, shown);
        same = 0u;
        cli_line_character(shown, '\n');
        cli_line_character(shown, (diff->change[at] == 1u) ? '+' : '-');
        cli_line_counted(shown, diff->change_line[at], diff->change_length[at]);
    }
    query_same_flushed(diff, same_first, same, shown);
}

void query_leaf_shown(const char *path, const unsigned char *tree, unsigned long long tree_size,
                      const unsigned char *held, unsigned long long held_size, CliLine *shown)
{
    static const char *const s_text[] = {"khw", "krs", "klq", "klm", "md", "tsv", "txt", "h", "gsm", NULL};
    const char *name = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        name = ((*at == '/') || (*at == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    int is_text = 0;
    for (unsigned int at = 0u; (dot != NULL) && (s_text[at] != NULL); at += 1u)
    {
        is_text =
            is_text || (cellul.eq(&(CatenaFinitaCfg){.src = dot + 1, .other = s_text[at], .cap = CLI_CELLS_POOL}));
    }
    if (is_text)
    {
        QueryDiff diff;
        const int taken = query_diff_taken(&diff, tree, tree_size, held, held_size);
        const unsigned int changes = taken ? query_diff(&diff) : 0u;
        if (taken && ((changes != 0u) || ((diff.n == 0u) && (diff.m == 0u))))
        {
            cli_line_text(shown, name);
            cli_line_character(shown, ':');
            query_rendered(&diff, changes, shown);
            query_diff_given(&diff);
            return;
        }
        query_diff_given(&diff);
    }
    cli_line_text(shown, name);
    if (held_size > tree_size)
    {
        cli_line_text(shown, " grew by ");
        cli_line_decimal(shown, held_size - tree_size, 0u);
        cli_line_text(shown, " bytes");
        return;
    }
    cli_line_text(shown, ": ");
    cli_line_decimal(shown, tree_size, 0u);
    cli_line_text(shown, " bytes in the tree, ");
    cli_line_decimal(shown, held_size, 0u);
    cli_line_text(shown, " as the run leaves it");
}

int query_leaves_settled(const QueryRun *run, QueryLeave leave, QueryAsker asker, QueryContext *context, CliLine *said)
{
    QueryLeave standing = leave;
    const unsigned int count = run->reads + run->lefts;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const path = (at < run->reads) ? run->read[at] : run->left[at - run->reads];
        // each path once, in the order first named
        int named_before = 0;
        for (unsigned int before = 0u; !named_before && (before < at); before += 1u)
        {
            const char *const other = (before < run->reads) ? run->read[before] : run->left[before - run->reads];
            named_before = cellul.eq(&(CatenaFinitaCfg){.src = other, .other = path, .cap = CLI_CELLS_POOL});
        }
        const unsigned char *held = NULL;
        unsigned long long held_size = 0ull;
        if (named_before || !query_writer_current(&run->writer, path, &held, &held_size))
        {
            continue;
        }
        unsigned long long tree_size = 0ull;
        const unsigned char *tree = qry_member_file_mapped(path, &tree_size);
        const int same = (tree_size == held_size) &&
                         ((tree_size == 0ull) ||
                          (memor.cmp(&(MemoriaCfg){.src = tree, .other = held, .bytes = (size_t)held_size}) == 0));
        if (!same)
        {
            // what is shown of the file written through pieces into a cell of its size
            char piece[QUERY_SHOWN_PIECE];
            CliPieces pieces;
            CliLine shown;
            cli_pieces_begun(&pieces, &shown, piece, sizeof(piece));
            query_leaf_shown(path, (tree != NULL) ? tree : (const unsigned char *)"", tree_size, held, held_size,
                             &shown);
            unsigned long long shown_size = 0ull;
            char *const text = cli_pieces_joined(&pieces, &shown, "what is shown of a file left", &shown_size);
            if (text != NULL)
            {
                query_said(context, text);
            }
            cli_cell_given(text);
            const QueryAnswer answer = (standing == QUERY_LEAVE_ALL)    ? QUERY_ANSWER_YES
                                       : (standing == QUERY_LEAVE_NONE) ? QUERY_ANSWER_NO
                                                                        : asker.asked(asker.context, path);
            standing = (answer == QUERY_ANSWER_ALL) ? QUERY_LEAVE_ALL : standing;
            standing = (answer == QUERY_ANSWER_NONE) ? QUERY_LEAVE_NONE : standing;
            if ((answer == QUERY_ANSWER_NO) || (answer == QUERY_ANSWER_NONE))
            {
                if (!query_writer_file(&run->writer, path, (tree != NULL) ? tree : (const unsigned char *)"", tree_size,
                                       said))
                {
                    if (tree != NULL)
                    {
                        qry_member_file_unmapped(tree, tree_size);
                    }
                    return 0;
                }
                char kept_text[QUERY_PATH_LONGEST + 64u];
                CliLine kept = {kept_text, sizeof(kept_text), 0ull};
                cli_line_text(&kept, "  ");
                cli_line_text(&kept, path);
                cli_line_text(&kept, " is left as the tree holds it");
                if (cli_line_closed(&kept) != 0ull)
                {
                    query_said(context, kept_text);
                }
            }
        }
        if (tree != NULL)
        {
            qry_member_file_unmapped(tree, tree_size);
        }
    }
    return 1;
}
