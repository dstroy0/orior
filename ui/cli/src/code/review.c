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

// the review's room and texts given back, and the lock with them
static void review_release(void)
{
    if (s_room != NULL)
    {
        EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_room->repositories.text);
        EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_room->touched.text);
    }
    g_cli_cells->persistent_buf_release(s_room, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(g_cli_review.compared, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(g_cli_review.call, &g_cli_code_cells);
    s_room = NULL;
    g_cli_review.compared = NULL;
    g_cli_review.texts = NULL;
    g_cli_review.text = NULL;
    g_cli_review.texts_room = 0ull;
    g_cli_review.text_room = 0ull;
    g_cli_review.call = NULL;
    g_cli_review.calls = 0u;
}

// the lock taken and the review's room and texts with it: 1, or 0 with the lock given back where the cells cannot
// meet them
static int review_lock(void)
{
    s_room = g_cli_cells->persistent_buf_alloc(sizeof(*s_room), &g_cli_code_cells);
    if (s_room != NULL)
    {
        s_room->repositories.text.bytes = NULL;
        s_room->touched.text.bytes = NULL;
    }
    const unsigned long long text_bytes = (unsigned long long)CLI_REVIEW_TEXT + CLI_REVIEW_TEXTS_MAX;
    g_cli_review.call = g_cli_cells->persistent_buf_alloc(
        ((unsigned long long)CLI_REVIEW_CALLS * sizeof(unsigned int)) + text_bytes, &g_cli_code_cells);
    g_cli_review.compared = g_cli_cells->persistent_buf_alloc(cli_review_compared_bytes(), &g_cli_code_cells);
    if ((s_room == NULL) || (g_cli_review.compared == NULL) || (g_cli_review.call == NULL))
    {
        review_release();
        return 0;
    }
    g_cli_review.text = (char *)(void *)(g_cli_review.call + CLI_REVIEW_CALLS);
    g_cli_review.texts = g_cli_review.text + CLI_REVIEW_TEXT;
    g_cli_review.text_room = CLI_REVIEW_TEXT;
    g_cli_review.texts_room = CLI_REVIEW_TEXTS_MAX;
    return 1;
}

typedef struct
{
    const char *at;
    const char *path;
    const char *was_path;
    const char *root;
    const char *id;
    VerbaLinea *by;
    VerbaLinea *subject;
    int *made;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    unsigned int at_u32;
    const char *by_text;
    unsigned long long by_size;
    const char *subject_text;
    unsigned long long subject_size;
} ReviewCtx;
// a file of the change put after the files: its path now and before
EMBED_INLINE CliReviewFile *cli_review_file_begin(const ReviewCtx *args)
{
    const char *at = args->at;
    const char *path = args->path;
    const char *was_path = args->was_path;
    if (g_cli_review.files >= CLI_REVIEW_FILES)
    {
        return NULL;
    }
    char joined[CLI_PATH_MAX_LEN];
    CliReviewFile *const file = &g_cli_review.file[g_cli_review.files];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    mmgr_line_put(&line, at);
    mmgr_line_put(&line, path);
    mmgr_line_finish(&line);
    file->path = EMBED_CALL(cli_review_text_length_internal.text_put, CliReviewTextLengthCtx, .text = joined,
                            .length = cellul.len(&(CatenaFinitaCfg){.src = joined, .cap = CLI_CELLS_POOL}));
    line.at = 0ull;
    mmgr_line_put(&line, at);
    mmgr_line_put(&line, was_path);
    mmgr_line_finish(&line);
    file->was_path = EMBED_CALL(cli_review_text_length_internal.text_put, CliReviewTextLengthCtx, .text = joined,
                                .length = cellul.len(&(CatenaFinitaCfg){.src = joined, .cap = CLI_CELLS_POOL}));
    file->has_was = 0;
    file->has_now = 0;
    g_cli_review.files += 1u;
    return file;
}

// the files commit `id` changed against its first parent, in the first repository of the tree that contains it, and who
// made it into `by` and `subject`: 1, or 0 with why given
EMBED_INLINE int cli_commit_read(const ReviewCtx *args)
{
    const char *root = args->root;
    const char *id = args->id;
    VerbaLinea *subject = args->subject;
    int *made = args->made;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = root,
               .found_repositories_pointer = &s_room->repositories);
    const char *repo = NULL;
    for (unsigned int at = 0u; (repo == NULL) && (at < s_room->repositories.count); at += 1u)
    {
        const char *const path =
            EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_room->repositories.text,
                       .at = s_room->repositories.item[at].path);
        char folder[CLI_PATH_MAX_LEN];
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea unread = {why, sizeof(why), 0ull};
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = folder);
        if (EMBED_CALL(cli_git_root_err_msg.touch, CliGitRootErrMsgCfg, .root = folder, .id = id,
                       .found_touched_list_pointer = &s_room->touched, .err_msg = &unread) &&
            (s_room->touched.count > 0u))
        {
            repo = path;
        }
    }
    if (repo == NULL)
    {
        mmgr_line_put(err_msg, "no repository of the tree contains the commit ");
        mmgr_line_put(err_msg, id);
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    char at[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = repo, .joined = folder);
    *made = EMBED_CALL(cli_git_root_id.new_by, CliGitRootIdCfg, .root = folder, .id = id,
                       .name_verba_linea_pointer = args->by, .subject = subject);
    VerbaLinea prefix = {at, sizeof(at), 0ull};
    if (repo[0] != '\0')
    {
        mmgr_line_put(&prefix, repo);
        mmgr_line_ch(&prefix, '/');
    }
    mmgr_line_finish(&prefix);
    for (unsigned int index = 0u; index < s_room->touched.count; index += 1u)
    {
        const CliTouched *const one = &s_room->touched.item[index];
        const char *const path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &s_room->touched.text, .at = one->path);
        const char *const was = one->has_was ? EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                          .text_read = &s_room->touched.text, .at = one->was)
                                             : path;
        CliReviewFile *const file =
            EMBED_CALL(cli_review_file_begin, ReviewCtx, .at = at, .path = path, .was_path = was);
        if (file == NULL)
        {
            break;
        }
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea unread = {why, sizeof(why), 0ull};
        VerbaLinea before = cli_review_text_line();
        const int read_before =
            (one->state != 'A') && EMBED_CALL(cli_git_root_file.text_before, CliGitRootFileCfg, .root = root,
                                              .file = g_cli_review.text + file->was_path, .id = id, .out = &before);
        file->has_was = EMBED_CALL(cli_review_line_internal.text_add, CliReviewLineCtx, .line = &before,
                                   .written = read_before, .at = &file->was, .size = &file->was_size);
        VerbaLinea after = cli_review_text_line();
        const int read_after = (one->state != 'D') && EMBED_CALL(cli_git_root_file.text_at, CliGitRootFileCfg,
                                                                 .root = root, .file = g_cli_review.text + file->path,
                                                                 .id = id, .out = &after, .err_msg = &unread);
        file->has_now = EMBED_CALL(cli_review_line_internal.text_add, CliReviewLineCtx, .line = &after,
                                   .written = read_after, .at = &file->now, .size = &file->now_size);
    }
    return 1;
}

// the files that differ from the last commit, each as the last commit stores it and as the disk does
EMBED_INLINE void cli_tree_read(const ReviewCtx *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = root, .repos = NULL, .count = 0u, .named = 0,
               .found_changed_list_pointer = &s_room->changed);
    for (unsigned int index = 0u; index < s_room->changed.count; index += 1u)
    {
        const char *const path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &s_room->changed.text, .at = s_room->changed.item[index].path);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if ((length > 0ull) && (path[length - 1ull] == '/'))
        {
            continue;
        }
        CliReviewFile *const file =
            EMBED_CALL(cli_review_file_begin, ReviewCtx, .at = "", .path = path, .was_path = path);
        if (file == NULL)
        {
            break;
        }
        VerbaLinea before = cli_review_text_line();
        file->has_was = EMBED_CALL(cli_review_line_internal.text_add, CliReviewLineCtx, .line = &before,
                                   .written = EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg,
                                                         .root = root, .file = path, .out = &before),
                                   .at = &file->was, .size = &file->was_size);
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                      .size_pointer = &size, .owner = &g_cli_code_cells);
        int text = bytes != NULL;
        for (unsigned long long at = 0ull; text && (at < size);)
        {
            unsigned int letter = 0u;
            const unsigned long long step =
                EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = bytes, .length = size,
                           .at = at, .letter = &letter);
            text = !((step == 1ull) && (bytes[at] >= 0x80u));
            at += step;
        }
        if (text)
        {
            file->now = EMBED_CALL(cli_review_text_length_internal.text_put, CliReviewTextLengthCtx,
                                   .text = (const char *)bytes, .length = size);
            file->now_size = size;
            file->has_now = (file->now != 0u) || (size == 0ull);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_code_cells);
    }
}

// the pieces in groups of those that call one another, each group in the order it is read: from each piece no other in
// it calls, what it calls after it, depth first; written as a JSON array of arrays of places
EMBED_INLINE void cli_groups_write(const ReviewCtx *args)
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
    mmgr_line_ch(args->out, '[');
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
            const unsigned int pivot = s_room->members[at];
            unsigned int place = at;
            while ((place > 0u) && (s_room->members[place - 1u] > pivot))
            {
                s_room->members[place] = s_room->members[place - 1u];
                place -= 1u;
            }
            s_room->members[place] = pivot;
        }
        mmgr_line_put(args->out, (groups != 0u) ? ",[" : "[");
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
                    mmgr_line_put(args->out, (written != 0u) ? "," : "");
                    mmgr_line_decimal(args->out, at, 0u);
                    written += 1u;
                    for (unsigned int call = piece[at].count; (call > 0u) && (stacked < CLI_REVIEW_CALLS); call -= 1u)
                    {
                        s_room->stack[stacked] = g_cli_review.call[piece[at].first + call - 1u];
                        stacked += 1u;
                    }
                }
            }
        }
        mmgr_line_ch(args->out, ']');
        groups += 1u;
    }
    mmgr_line_ch(args->out, ']');
}

// a text of the pieces written as a JSON string
EMBED_INLINE void cli_piece_text_write(const ReviewCtx *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = g_cli_review.texts + args->at_u32,
               .length =
                   cellul.len(&(CatenaFinitaCfg){.src = g_cli_review.texts + args->at_u32, .cap = CLI_CELLS_POOL}));
}

// the files read compared and the review written, with who made it and its subject, `by_size` and `subject_size`
// bytes of them, each NULL where none made it
static void cli_review_write(const ReviewCtx *args)
{
    VerbaLinea *out = args->out;
    cli_review_compare();
    mmgr_line_put(out, "{\"pieces\":[");
    for (unsigned int at = 0u; at < g_cli_review.pieces; at += 1u)
    {
        const CliReviewPiece *const piece = &g_cli_review.piece[at];
        mmgr_line_put(out, (at != 0u) ? ",{\"path\":" : "{\"path\":");
        EMBED_CALL(cli_piece_text_write, ReviewCtx, .out = out, .at_u32 = piece->path);
        mmgr_line_put(out, ",\"name\":");
        EMBED_CALL(cli_piece_text_write, ReviewCtx, .out = out, .at_u32 = piece->name);
        mmgr_line_put(out, ",\"kind\":");
        EMBED_CALL(cli_piece_text_write, ReviewCtx, .out = out, .at_u32 = piece->kind);
        mmgr_line_put(out, ",\"how\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = piece->how,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = piece->how, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"from\":");
        if (piece->has_from)
        {
            EMBED_CALL(cli_piece_text_write, ReviewCtx, .out = out, .at_u32 = piece->from);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ",\"line\":");
        mmgr_line_decimal(out, piece->line, 0u);
        mmgr_line_put(out, ",\"was\":");
        EMBED_CALL(cli_piece_text_write, ReviewCtx, .out = out, .at_u32 = piece->was);
        mmgr_line_put(out, ",\"now\":");
        EMBED_CALL(cli_piece_text_write, ReviewCtx, .out = out, .at_u32 = piece->now);
        mmgr_line_put(out, ",\"calls\":[");
        for (unsigned int call = 0u; call < piece->count; call += 1u)
        {
            mmgr_line_put(out, (call != 0u) ? "," : "");
            mmgr_line_decimal(out, g_cli_review.call[piece->first + call], 0u);
        }
        mmgr_line_put(out, "]}");
    }
    mmgr_line_put(out, "],\"groups\":");
    EMBED_CALL(cli_groups_write, ReviewCtx, .out = out);
    mmgr_line_put(out, ",\"other\":[");
    for (unsigned int at = 0u; at < g_cli_review.others; at += 1u)
    {
        const char *const path = g_cli_review.text + g_cli_review.other[at];
        mmgr_line_put(out, (at != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(out, "],\"by\":");
    if (args->by_text != NULL)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->by_text,
                   .length = args->by_size);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"subject\":");
    if (args->subject_text != NULL)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = args->subject_text, .length = args->subject_size);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_ch(out, '}');
}

int cli_review(const CliReviewCfg *args)
{
    static char s_by[CLI_NAME_MAX_LEN * 4u];
    static char s_subject[CLI_NAME_MAX_LEN * 16u];
    if (!review_lock())
    {
        mmgr_line_put(args->err_msg, "the review's cells could not be taken");
        return 0;
    }
    g_cli_review.files = 0u;
    g_cli_review.used = 0u;
    VerbaLinea by = {s_by, sizeof(s_by), 0ull};
    VerbaLinea subject = {s_subject, sizeof(s_subject), 0ull};
    int made = 0;
    if (args->commit != NULL)
    {
        if (!EMBED_CALL(cli_commit_read, ReviewCtx, .root = args->root, .id = args->commit, .by = &by,
                        .subject = &subject, .made = &made, .err_msg = args->err_msg))
        {
            review_release();
            return 0;
        }
    }
    else
    {
        EMBED_CALL(cli_tree_read, ReviewCtx, .root = args->root);
    }
    EMBED_CALL(cli_review_write, ReviewCtx, .by_text = made ? s_by : NULL, .by_size = by.at,
               .subject_text = made ? s_subject : NULL, .subject_size = subject.at, .out = args->out);
    review_release();
    return 1;
}

void cli_review_files(const CliReviewPathsCfg *args)
{
    const char *const *was = args->was;
    const char *const *now = args->now;
    if (!review_lock())
    {
        mmgr_line_put(args->out, "{\"files\":[],\"groups\":[]}");
        return;
    }
    g_cli_review.files = 0u;
    g_cli_review.used = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        CliReviewFile *const file = EMBED_CALL(cli_review_file_begin, ReviewCtx, .at = "", .path = args->paths[at],
                                               .was_path = args->paths[at]);
        if (file == NULL)
        {
            break;
        }
        if (was[at] != NULL)
        {
            file->was_size = cellul.len(&(CatenaFinitaCfg){.src = was[at], .cap = CLI_CELLS_POOL});
            file->was = EMBED_CALL(cli_review_text_length_internal.text_put, CliReviewTextLengthCtx, .text = was[at],
                                   .length = file->was_size);
            file->has_was = (file->was != 0u);
        }
        if (now[at] != NULL)
        {
            file->now_size = cellul.len(&(CatenaFinitaCfg){.src = now[at], .cap = CLI_CELLS_POOL});
            file->now = EMBED_CALL(cli_review_text_length_internal.text_put, CliReviewTextLengthCtx, .text = now[at],
                                   .length = file->now_size);
            file->has_now = (file->now != 0u);
        }
    }
    EMBED_CALL(cli_review_write, ReviewCtx, .by_text = NULL, .by_size = 0ull, .subject_text = NULL,
               .subject_size = 0ull, .out = args->out);
    review_release();
}
