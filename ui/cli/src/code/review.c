// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review.c: a change read file by file, its texts before and now, its pieces grouped by the calls between them, and
// the review written as the window reads it
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "git.h"
#include "review_internal.h"

CliReview g_cli_review;

// The lists git gives a review, and the room the pieces are grouped in
typedef struct
{
    CliRepositories repositories;
    CliTouchedList touched;
    CliChangedList changed;
    unsigned int caller_first[CLI_REVIEW_PIECES];
    unsigned int caller_count[CLI_REVIEW_PIECES];
    unsigned int caller[CLI_REVIEW_CALLS];
    unsigned char reached[CLI_REVIEW_PIECES];
    unsigned char visited[CLI_REVIEW_PIECES];
    unsigned char placed[CLI_REVIEW_PIECES];
    unsigned int queue[CLI_REVIEW_PIECES];
    unsigned int members[CLI_REVIEW_PIECES];
    unsigned int stack[CLI_REVIEW_CALLS];
} CliReviewRoom;

static CliReviewRoom *s_room;
static CliLock s_lock = CLI_LOCK_INIT;

// the review's room and texts given back, and the lock with them
static void review_released(void)
{
    cli_cell_given(s_room);
    cli_cell_given(g_cli_review.compared);
    cli_cell_given(g_cli_review.kept);
    cli_cell_given(g_cli_review.text);
    s_room = NULL;
    g_cli_review.compared = NULL;
    g_cli_review.kept = NULL;
    g_cli_review.text = NULL;
    cli_lock_give(&s_lock);
}

// the lock taken and the review's room and texts with it: 1, or 0 with the lock given back where the cells cannot
// meet them
static int review_held(void)
{
    cli_lock_take(&s_lock);
    s_room = cli_cell_taken(sizeof(*s_room), "a review's walk");
    g_cli_review.text = cli_cell_taken(CLI_REVIEW_TEXT, "a review's file texts");
    g_cli_review.kept = cli_cell_taken(CLI_REVIEW_KEPT, "a review's piece texts");
    g_cli_review.compared = cli_cell_taken(cli_review_compared_bytes(), "a review's comparison");
    if ((s_room == NULL) || (g_cli_review.text == NULL) || (g_cli_review.kept == NULL) ||
        (g_cli_review.compared == NULL))
    {
        review_released();
        return 0;
    }
    return 1;
}

// `length` bytes put in the files' texts, a 0 after them: their place, 0 where they do not fit
static unsigned int cli_text_put(const char *text, unsigned long long length)
{
    if ((g_cli_review.used + length + 1ull) > CLI_REVIEW_TEXT)
    {
        return 0u;
    }
    const unsigned int at = g_cli_review.used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_review.text + at, .src = text, .bytes = length});
    g_cli_review.text[at + length] = '\0';
    // the texts are below CLI_REVIEW_TEXT bytes, which an unsigned int holds
    g_cli_review.used += (unsigned int)length + 1u;
    return at;
}

// a text written into the room after the files' texts, kept there where it was written whole: 1, or 0
static int cli_text_kept(const CliLine *line, int written, unsigned int *at, unsigned long long *size)
{
    if (!written || !cli_line_fits(line) || ((g_cli_review.used + line->at + 1ull) > CLI_REVIEW_TEXT))
    {
        return 0;
    }
    *at = g_cli_review.used;
    *size = line->at;
    g_cli_review.text[g_cli_review.used + line->at] = '\0';
    // the texts are below CLI_REVIEW_TEXT bytes, which an unsigned int holds
    g_cli_review.used += (unsigned int)line->at + 1u;
    return 1;
}

// a file of the change put after the files: its path now and before
static CliReviewFile *cli_review_file_begun(const char *at, const char *path, const char *was_path)
{
    if (g_cli_review.files >= CLI_REVIEW_FILES)
    {
        return NULL;
    }
    char joined[CLI_PATH_LONGEST];
    CliReviewFile *const file = &g_cli_review.file[g_cli_review.files];
    CliLine line = {joined, sizeof(joined), 0ull};
    cli_line_text(&line, at);
    cli_line_text(&line, path);
    cli_line_closed(&line);
    file->path = cli_text_put(joined, cellul.len(&(CatenaFinitaCfg){.src = joined, .cap = CLI_CELLS_POOL}));
    line.at = 0ull;
    cli_line_text(&line, at);
    cli_line_text(&line, was_path);
    cli_line_closed(&line);
    file->was_path = cli_text_put(joined, cellul.len(&(CatenaFinitaCfg){.src = joined, .cap = CLI_CELLS_POOL}));
    file->has_was = 0;
    file->has_now = 0;
    g_cli_review.files += 1u;
    return file;
}

// the files commit `id` changed against its first parent, in the first repository of the tree that holds it, and who
// made it into `by` and `subject`: 1, or 0 with why said
static int cli_commit_read(const char *root, const char *id, CliLine *by, CliLine *subject, int *made, CliLine *said)
{
    cli_git_repositories(root, &s_room->repositories);
    const char *repo = NULL;
    for (unsigned int at = 0u; (repo == NULL) && (at < s_room->repositories.count); at += 1u)
    {
        const char *const path = cli_git_text_of(&s_room->repositories.text, s_room->repositories.item[at].path);
        char folder[CLI_PATH_LONGEST];
        char why[CLI_NAME_LONGEST];
        CliLine unread = {why, sizeof(why), 0ull};
        cli_full(root, path, folder);
        if (cli_git_touched(folder, id, &s_room->touched, &unread) && (s_room->touched.count > 0u))
        {
            repo = path;
        }
    }
    if (repo == NULL)
    {
        cli_line_text(said, "no repository of the tree holds the commit ");
        cli_line_text(said, id);
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    char at[CLI_PATH_LONGEST];
    cli_full(root, repo, folder);
    *made = cli_git_made_by(folder, id, by, subject);
    CliLine prefix = {at, sizeof(at), 0ull};
    if (repo[0] != '\0')
    {
        cli_line_text(&prefix, repo);
        cli_line_character(&prefix, '/');
    }
    cli_line_closed(&prefix);
    for (unsigned int index = 0u; index < s_room->touched.count; index += 1u)
    {
        const CliTouched *const one = &s_room->touched.item[index];
        const char *const path = cli_git_text_of(&s_room->touched.text, one->path);
        const char *const was = one->has_was ? cli_git_text_of(&s_room->touched.text, one->was) : path;
        CliReviewFile *const file = cli_review_file_begun(at, path, was);
        if (file == NULL)
        {
            break;
        }
        char why[CLI_NAME_LONGEST];
        CliLine unread = {why, sizeof(why), 0ull};
        CliLine before = {g_cli_review.text + g_cli_review.used, CLI_REVIEW_TEXT - g_cli_review.used, 0ull};
        file->has_was =
            (one->state != 'A') &&
            cli_text_kept(&before, cli_git_text_before(root, g_cli_review.text + file->was_path, id, &before),
                          &file->was, &file->was_size);
        CliLine after = {g_cli_review.text + g_cli_review.used, CLI_REVIEW_TEXT - g_cli_review.used, 0ull};
        file->has_now =
            (one->state != 'D') &&
            cli_text_kept(&after, cli_git_text_at(root, g_cli_review.text + file->path, id, &after, &unread),
                          &file->now, &file->now_size);
    }
    return 1;
}

// the files that differ from the last commit, each as the last commit holds it and as the disk does
static void cli_tree_read(const char *root)
{
    cli_git_changed(root, NULL, 0u, 0, &s_room->changed);
    for (unsigned int index = 0u; index < s_room->changed.count; index += 1u)
    {
        const char *const path = cli_git_text_of(&s_room->changed.text, s_room->changed.item[index].path);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if ((length > 0ull) && (path[length - 1ull] == '/'))
        {
            continue;
        }
        CliReviewFile *const file = cli_review_file_begun("", path, path);
        if (file == NULL)
        {
            break;
        }
        CliLine before = {g_cli_review.text + g_cli_review.used, CLI_REVIEW_TEXT - g_cli_review.used, 0ull};
        file->has_was = cli_text_kept(&before, cli_git_head_text(root, path, &before), &file->was, &file->was_size);
        char full[CLI_PATH_LONGEST];
        cli_full(root, path, full);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = qry_member_file_mapped(full, &size);
        int text = bytes != NULL;
        for (unsigned long long at = 0ull; text && (at < size);)
        {
            unsigned int letter = 0u;
            const unsigned long long step = cli_lossy_letter(bytes, size, at, &letter);
            text = !((step == 1ull) && (bytes[at] >= 0x80u));
            at += step;
        }
        if (text)
        {
            file->now = cli_text_put((const char *)bytes, size);
            file->now_size = size;
            file->has_now = (file->now != 0u) || (size == 0ull);
        }
        if (bytes != NULL)
        {
            qry_member_file_unmapped(bytes, size);
        }
    }
}

// the pieces in groups of those that call one another, each group in the order it is read: from each piece no other in
// it calls, what it calls after it, depth first; written as a JSON array of arrays of places
static void cli_groups_written(CliLine *out)
{
    const unsigned int count = g_cli_review.pieces;
    const CliReviewPiece *const piece = g_cli_review.piece;
    memor.set(&(MemoriaCfg){.dst = s_room->caller_count, .val = 0, .bytes = count * sizeof(unsigned int)});
    for (unsigned int at = 0u; at < g_cli_review.calls; at += 1u)
    {
        s_room->caller_count[g_cli_review.call[at]] += 1u;
    }
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_room->caller_first[at] = first;
        first += s_room->caller_count[at];
        s_room->caller_count[at] = 0u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        for (unsigned int call = piece[at].first; call < (piece[at].first + piece[at].count); call += 1u)
        {
            const unsigned int to = g_cli_review.call[call];
            s_room->caller[s_room->caller_first[to] + s_room->caller_count[to]] = at;
            s_room->caller_count[to] += 1u;
        }
    }
    memor.set(&(MemoriaCfg){.dst = s_room->placed, .val = 0, .bytes = count});
    memor.set(&(MemoriaCfg){.dst = s_room->reached, .val = 0, .bytes = count});
    memor.set(&(MemoriaCfg){.dst = s_room->visited, .val = 0, .bytes = count});
    cli_line_character(out, '[');
    unsigned int groups = 0u;
    for (unsigned int start = 0u; start < count; start += 1u)
    {
        if (s_room->placed[start])
        {
            continue;
        }
        // the group: every piece reached from `start` by the calls either way, by place
        unsigned int members = 0u;
        unsigned int head = 0u;
        unsigned int tail = 0u;
        s_room->queue[tail] = start;
        tail += 1u;
        s_room->reached[start] = 1u;
        while (head < tail)
        {
            const unsigned int at = s_room->queue[head];
            head += 1u;
            s_room->members[members] = at;
            members += 1u;
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                const unsigned int from = (side == 0u) ? piece[at].first : s_room->caller_first[at];
                const unsigned int many = (side == 0u) ? piece[at].count : s_room->caller_count[at];
                for (unsigned int one = 0u; one < many; one += 1u)
                {
                    const unsigned int next = (side == 0u) ? g_cli_review.call[from + one] : s_room->caller[from + one];
                    if (!s_room->reached[next])
                    {
                        s_room->reached[next] = 1u;
                        s_room->queue[tail] = next;
                        tail += 1u;
                    }
                }
            }
        }
        for (unsigned int at = 1u; at < members; at += 1u)
        {
            const unsigned int held = s_room->members[at];
            unsigned int place = at;
            while ((place > 0u) && (s_room->members[place - 1u] > held))
            {
                s_room->members[place] = s_room->members[place - 1u];
                place -= 1u;
            }
            s_room->members[place] = held;
        }
        cli_line_text(out, (groups != 0u) ? ",[" : "[");
        unsigned int written = 0u;
        // from the pieces nothing calls, then from every piece, each visited before what it calls
        for (unsigned int pass = 0u; pass < 2u; pass += 1u)
        {
            for (unsigned int member = 0u; member < members; member += 1u)
            {
                const unsigned int root = s_room->members[member];
                if ((pass == 0u) && (s_room->caller_count[root] != 0u))
                {
                    continue;
                }
                unsigned int stacked = 0u;
                s_room->stack[stacked] = root;
                stacked += 1u;
                while (stacked > 0u)
                {
                    stacked -= 1u;
                    const unsigned int at = s_room->stack[stacked];
                    if (s_room->visited[at])
                    {
                        continue;
                    }
                    s_room->visited[at] = 1u;
                    s_room->placed[at] = 1u;
                    cli_line_text(out, (written != 0u) ? "," : "");
                    cli_line_decimal(out, at, 0u);
                    written += 1u;
                    for (unsigned int call = piece[at].count; (call > 0u) && (stacked < CLI_REVIEW_CALLS); call -= 1u)
                    {
                        s_room->stack[stacked] = g_cli_review.call[piece[at].first + call - 1u];
                        stacked += 1u;
                    }
                }
            }
        }
        cli_line_character(out, ']');
        groups += 1u;
    }
    cli_line_character(out, ']');
}

// a text of the pieces written as a JSON string
static void cli_kept_written(CliLine *out, unsigned int at)
{
    cli_json_quoted(out, g_cli_review.kept + at,
                    cellul.len(&(CatenaFinitaCfg){.src = g_cli_review.kept + at, .cap = CLI_CELLS_POOL}));
}

// the files read compared and the review written, with who made it and its subject, `by_size` and `subject_size`
// bytes of them, each NULL where none made it
static void cli_review_written(const char *by, unsigned long long by_size, const char *subject,
                               unsigned long long subject_size, CliLine *out)
{
    cli_review_compare();
    cli_line_text(out, "{\"pieces\":[");
    for (unsigned int at = 0u; at < g_cli_review.pieces; at += 1u)
    {
        const CliReviewPiece *const piece = &g_cli_review.piece[at];
        cli_line_text(out, (at != 0u) ? ",{\"path\":" : "{\"path\":");
        cli_kept_written(out, piece->path);
        cli_line_text(out, ",\"name\":");
        cli_kept_written(out, piece->name);
        cli_line_text(out, ",\"kind\":");
        cli_kept_written(out, piece->kind);
        cli_line_text(out, ",\"how\":");
        cli_json_quoted(out, piece->how, cellul.len(&(CatenaFinitaCfg){.src = piece->how, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"from\":");
        if (piece->has_from)
        {
            cli_kept_written(out, piece->from);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_text(out, ",\"line\":");
        cli_line_decimal(out, piece->line, 0u);
        cli_line_text(out, ",\"was\":");
        cli_kept_written(out, piece->was);
        cli_line_text(out, ",\"now\":");
        cli_kept_written(out, piece->now);
        cli_line_text(out, ",\"calls\":[");
        for (unsigned int call = 0u; call < piece->count; call += 1u)
        {
            cli_line_text(out, (call != 0u) ? "," : "");
            cli_line_decimal(out, g_cli_review.call[piece->first + call], 0u);
        }
        cli_line_text(out, "]}");
    }
    cli_line_text(out, "],\"groups\":");
    cli_groups_written(out);
    cli_line_text(out, ",\"other\":[");
    for (unsigned int at = 0u; at < g_cli_review.others; at += 1u)
    {
        const char *const path = g_cli_review.text + g_cli_review.other[at];
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, "],\"by\":");
    if (by != NULL)
    {
        cli_json_quoted(out, by, by_size);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"subject\":");
    if (subject != NULL)
    {
        cli_json_quoted(out, subject, subject_size);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_character(out, '}');
}

int cli_review(const char *root, const char *commit, CliLine *out, CliLine *said)
{
    static char s_by[CLI_NAME_LONGEST * 4u];
    static char s_subject[CLI_NAME_LONGEST * 16u];
    if (!review_held())
    {
        cli_line_text(said, "the review's cells could not be taken");
        return 0;
    }
    g_cli_review.files = 0u;
    g_cli_review.used = 0u;
    CliLine by = {s_by, sizeof(s_by), 0ull};
    CliLine subject = {s_subject, sizeof(s_subject), 0ull};
    int made = 0;
    if (commit != NULL)
    {
        if (!cli_commit_read(root, commit, &by, &subject, &made, said))
        {
            review_released();
            return 0;
        }
    }
    else
    {
        cli_tree_read(root);
    }
    cli_review_written(made ? s_by : NULL, by.at, made ? s_subject : NULL, subject.at, out);
    review_released();
    return 1;
}

void cli_review_files(const char *const *paths, const char *const *was, const char *const *now, unsigned int count,
                      CliLine *out)
{
    if (!review_held())
    {
        cli_line_text(out, "{\"files\":[],\"groups\":[]}");
        return;
    }
    g_cli_review.files = 0u;
    g_cli_review.used = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        CliReviewFile *const file = cli_review_file_begun("", paths[at], paths[at]);
        if (file == NULL)
        {
            break;
        }
        if (was[at] != NULL)
        {
            file->was_size = cellul.len(&(CatenaFinitaCfg){.src = was[at], .cap = CLI_CELLS_POOL});
            file->was = cli_text_put(was[at], file->was_size);
            file->has_was = (file->was != 0u);
        }
        if (now[at] != NULL)
        {
            file->now_size = cellul.len(&(CatenaFinitaCfg){.src = now[at], .cap = CLI_CELLS_POOL});
            file->now = cli_text_put(now[at], file->now_size);
            file->has_now = (file->now != 0u);
        }
    }
    cli_review_written(NULL, 0ull, NULL, 0ull, out);
    review_released();
}
