// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// diff_view_open.c: the view opened from its source. The sources are copied into a cell of their own with the bar's
// label, the sides sentence and the sentence for two texts that do not differ; each side's text is read, a file's from
// the open view on it or else from the file stored under a key of the view's own for the read alone, a commit's, the
// last commit's or a snapshot's from git or the history into a cell of the read alone, and laid as lines of units in
// one cell sized to the two; the lines are compared, with white space set aside where the option is on, the changes
// handed by the diff laid into a cell of the read alone, and the rows laid from them into one cell sized to the rows
// with the changes' first rows after them
#include "../command_line/pool.h"
#include "../editor/document.h"
#include "../editor/files.h"
#include "../editor/history.h"
#include "../editor/view.h"
#include "diff_view_internal.h"
#include "git.h"

// the most bytes a text read from git or the history may take, the start of the key a file is stored under for the
// view alone, a commit's id as its bar shows it, and the most bytes of the bar's sentences
#define DIFF_VIEW_TEXT_MAX (1ULL << 20u)
#define DIFF_VIEW_KEY "diff:"
#define DIFF_VIEW_SHORT_ID 7ULL
#define DIFF_VIEW_SENTENCE_MAX ((4ULL * CLI_PATH_MAX_LEN) + (4ULL * CLI_NAME_MAX_LEN) + 256ULL)

// the sentences the bar and the body show, as the page shows them
#define DIFF_VIEW_HEAD_SIDES "last commit, then as it stands"
#define DIFF_VIEW_UNTRACKED_SIDES "not in the last commit"
#define DIFF_VIEW_HEAD_SAME "No change from the last commit."
#define DIFF_VIEW_SAME "The two are the same."
#define DIFF_VIEW_CLIPBOARD_SIDES "the clipboard, then the file"
#define DIFF_VIEW_NOTHING "nothing"
#define DIFF_VIEW_NO_FILE "No file is shown to compare."

CliDiffView g_cli_diff_view;

// A side as it is read: the key its text is stored under, else `size` bytes of `text`, none where `absent`; whether
// the key is the view's own, let go once the side is laid; and the cell of the read alone its text stands in
typedef struct
{
    const char *key;
    const char *text;
    unsigned long long size;
    int absent;
    int own;
    char *scratch;
    char keyed[CLI_PATH_MAX_LEN + sizeof(DIFF_VIEW_KEY)];
} DiffViewSide;

// The changes handed by the diff: each four numbers, the newer text's first line and the line after its last then the
// older's, how many, and how many fit
typedef struct
{
    unsigned int *hunk;
    unsigned int hunks;
    unsigned int capacity;
} DiffViewHunks;

typedef struct
{
    const CliDiffViewSourceErrMsgCfg *source;
    DiffViewSide *side;
    const char *path;
    const char *id;
    const char *other;
    const char *root;
    const char *const *request;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    CliLines *lines;
    const CliLines *lines_read;
    unsigned char **cell_out;
    unsigned int file;
    int on;
} DiffViewCtx;

// every cell of the open view released, none open
EMBED_INLINE void view_released(const DiffViewCtx *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(g_cli_diff_view.sources, g_cli_diff_view_owner);
    g_cli_guard->persistent_buf_release(g_cli_diff_view.lines_cell, g_cli_diff_view_owner);
    g_cli_guard->persistent_buf_release(g_cli_diff_view.rows_cell, g_cli_diff_view_owner);
    const int space = g_cli_diff_view.space;
    const int structure = g_cli_diff_view.structure;
    memor.set(&(MemoriaCfg){.dst = &g_cli_diff_view, .val = 0, .bytes = sizeof(g_cli_diff_view)});
    g_cli_diff_view.space = space;
    g_cli_diff_view.structure = structure;
}

// the bytes of `path`, none for NULL
EMBED_INLINE unsigned long long text_bytes(const DiffViewCtx *args)
{
    return (args->path != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) : 0ULL;
}

// a commit `id` as the bar shows it after `out`: its short id, and who made it where git reads it
EMBED_INLINE void commit_by(const DiffViewCtx *args)
{
    const unsigned long long length = EMBED_CALL(text_bytes, DiffViewCtx, .path = args->id);
    mmgr_line_put_n(args->out, args->id, (length < DIFF_VIEW_SHORT_ID) ? length : DIFF_VIEW_SHORT_ID);
    char author[CLI_NAME_MAX_LEN];
    char subject[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea author_line = {author, sizeof(author), 0ULL};
    VerbaLinea subject_line = {subject, sizeof(subject), 0ULL};
    if (EMBED_CALL(cli_git_root_id.new_by, CliGitRootIdCfg, .root = args->root, .id = args->id,
                   .name_verba_linea_pointer = &author_line, .subject = &subject_line) &&
        mmgr_line_ok(&author_line) && (author_line.at != 0ULL))
    {
        mmgr_line_put(args->out, " by ");
        mmgr_line_put_n(args->out, author, author_line.at);
    }
}

// git run at `root` with the words of `request`, `count` of them, its words into `out`: 1, or 0 where git refuses
EMBED_INLINE int git_asked(const DiffViewCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                      .out = args->out, .count = args->count, .err_msg = &err_msg, .request = args->request);
}

// the first parent of commit `id` as the bar shows it after `out`, its short id, or `nothing` where it has none
EMBED_INLINE void parent_put(const DiffViewCtx *args)
{
    char parent[CLI_NAME_MAX_LEN];
    VerbaLinea parent_line = {parent, sizeof(parent), 0ULL};
    mmgr_line_put(&parent_line, args->id);
    mmgr_line_ch(&parent_line, '^');
    char answer[CLI_NAME_MAX_LEN];
    VerbaLinea answer_line = {answer, sizeof(answer), 0ULL};
    const char *const request[3] = {"rev-parse", "--short=7", parent};
    const int found =
        (mmgr_line_finish(&parent_line) != 0ULL) &&
        EMBED_CALL(git_asked, DiffViewCtx, .root = args->root, .out = &answer_line, .request = request, .count = 3u) &&
        mmgr_line_ok(&answer_line);
    unsigned long long length = found ? answer_line.at : 0ULL;
    while ((length != 0ULL) && ((answer[length - 1ULL] == '\n') || (answer[length - 1ULL] == '\r')))
    {
        length -= 1ULL;
    }
    if (length == 0ULL)
    {
        mmgr_line_put(args->out, DIFF_VIEW_NOTHING);
        return;
    }
    mmgr_line_put_n(args->out, answer, length);
}

// whether commit `id` stands among the commits before commit `other`, which sets it on the left
EMBED_INLINE int commit_older(const DiffViewCtx *args)
{
    char answer[CLI_NAME_MAX_LEN];
    VerbaLinea answer_line = {answer, sizeof(answer), 0ULL};
    const char *const request[4] = {"merge-base", "--is-ancestor", args->id, args->other};
    return EMBED_CALL(git_asked, DiffViewCtx, .root = args->root, .out = &answer_line, .request = request, .count = 4u);
}

// a snapshot's moment `file`, its milliseconds read from `source`, after `out` as YYYY-MM-DD HH:MM in the machine's
// zone
EMBED_INLINE void stamp_put(const DiffViewCtx *args)
{
    CliCivil civil;
    // the milliseconds since 1970 of a snapshot are far inside what a long long counts in seconds
    EMBED_CALL(cli_clock_civil, CliClockCivilCtx, .seconds = (long long)(args->source->at_ms / 1000ULL),
               .civil_out = &civil);
    mmgr_line_decimal(args->out, civil.year, 4u);
    mmgr_line_ch(args->out, '-');
    mmgr_line_decimal(args->out, civil.month, 2u);
    mmgr_line_ch(args->out, '-');
    mmgr_line_decimal(args->out, civil.day, 2u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_decimal(args->out, civil.hour, 2u);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, civil.minute, 2u);
}

// the bar's label, the sides sentence and the sentence for texts that do not differ written for `source` and `path`,
// each ended by a 0, one after another into `out`; whether commit `id` is the older of two into `on`
EMBED_INLINE void sentences_written(const DiffViewCtx *args)
{
    const CliDiffViewSourceErrMsgCfg *const source = args->source;
    VerbaLinea *const out = args->out;
    mmgr_line_put(out, args->path);
    if (source->source == CLI_DIFF_VIEW_FILES)
    {
        mmgr_line_put(out, " \xc2\xb7 ");
        mmgr_line_put(out, source->other_path);
    }
    mmgr_line_ch(out, '\0');
    switch (source->source)
    {
    case CLI_DIFF_VIEW_HEAD:
        mmgr_line_put(out, args->on ? DIFF_VIEW_HEAD_SIDES : DIFF_VIEW_UNTRACKED_SIDES);
        mmgr_line_ch(out, '\0');
        mmgr_line_put(out, DIFF_VIEW_HEAD_SAME);
        break;
    case CLI_DIFF_VIEW_COMMIT:
        EMBED_CALL(commit_by, DiffViewCtx, .out = out, .root = source->root, .id = source->id);
        mmgr_line_put(out, ", then as it stands");
        mmgr_line_ch(out, '\0');
        mmgr_line_put(out, DIFF_VIEW_SAME);
        break;
    case CLI_DIFF_VIEW_COMMITS:
        EMBED_CALL(commit_by, DiffViewCtx, .out = out, .root = source->root,
                   .id = args->on ? source->id : source->other_id);
        mmgr_line_put(out, ", then ");
        EMBED_CALL(commit_by, DiffViewCtx, .out = out, .root = source->root,
                   .id = args->on ? source->other_id : source->id);
        mmgr_line_ch(out, '\0');
        mmgr_line_put(out, DIFF_VIEW_SAME);
        break;
    case CLI_DIFF_VIEW_PARENT:
        EMBED_CALL(parent_put, DiffViewCtx, .out = out, .root = source->root, .id = source->id);
        mmgr_line_put(out, ", then ");
        EMBED_CALL(commit_by, DiffViewCtx, .out = out, .root = source->root, .id = source->id);
        mmgr_line_ch(out, '\0');
        mmgr_line_put(out, DIFF_VIEW_SAME);
        break;
    case CLI_DIFF_VIEW_CLIPBOARD:
        mmgr_line_put(out, DIFF_VIEW_CLIPBOARD_SIDES);
        mmgr_line_ch(out, '\0');
        mmgr_line_put(out, DIFF_VIEW_SAME);
        break;
    case CLI_DIFF_VIEW_FILES:
        mmgr_line_put(out, args->path);
        mmgr_line_put(out, ", then ");
        mmgr_line_put(out, source->other_path);
        mmgr_line_ch(out, '\0');
        mmgr_line_put(out, DIFF_VIEW_SAME);
        break;
    default:
        mmgr_line_put(out, "saved ");
        EMBED_CALL(stamp_put, DiffViewCtx, .out = out, .source = source);
        mmgr_line_put(out, ", then as it stands");
        mmgr_line_ch(out, '\0');
        mmgr_line_put(out, "No change from the save at ");
        EMBED_CALL(stamp_put, DiffViewCtx, .out = out, .source = source);
        mmgr_line_ch(out, '.');
        break;
    }
    mmgr_line_ch(out, '\0');
}

// the sources of `source` with the file `path` copied into a cell of their own with the sentences written into
// `out`, `out->at` bytes, the view's pointers set into it: 1, or 0 where the cells cannot meet it
EMBED_INLINE int sources_copied(const DiffViewCtx *args)
{
    const CliDiffViewSourceErrMsgCfg *const source = args->source;
    const char *const texts[5] = {source->root, args->path, source->other_path, source->id, source->other_id};
    unsigned long long bytes = args->out->at + source->size + 1ULL;
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        bytes += EMBED_CALL(text_bytes, DiffViewCtx, .path = texts[at]) + 1ULL;
    }
    char *const cell = g_cli_guard->persistent_buf_alloc(bytes, g_cli_diff_view_owner);
    if (cell == NULL)
    {
        return 0;
    }
    const char *placed[5];
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        const unsigned long long length = EMBED_CALL(text_bytes, DiffViewCtx, .path = texts[at]);
        placed[at] = (texts[at] != NULL) ? cell + used : NULL;
        if (length != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = cell + used, .src = texts[at], .bytes = length});
        }
        cell[used + length] = '\0';
        used += length + 1ULL;
    }
    const char *const clipboard = cell + used;
    if (source->size != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell + used, .src = source->text, .bytes = source->size});
    }
    cell[used + source->size] = '\0';
    used += source->size + 1ULL;
    memor.cpy(&(MemoriaCfg){.dst = cell + used, .src = args->out->out, .bytes = args->out->at});
    // the view open before goes once its sources are copied, those of a view laid again among them
    EMBED_CALL(view_released, DiffViewCtx, .on = 0);
    CliDiffView *const view = &g_cli_diff_view;
    view->sources = (unsigned char *)cell;
    view->source = source->source;
    view->root = placed[0];
    view->path = placed[1];
    view->other_path = placed[2];
    view->id = placed[3];
    view->other_id = placed[4];
    view->text = clipboard;
    view->size = source->size;
    view->at_ms = source->at_ms;
    view->label = cell + used;
    view->label_length = EMBED_CALL(text_bytes, DiffViewCtx, .path = view->label);
    view->sides = view->label + view->label_length + 1ULL;
    view->sides_length = EMBED_CALL(text_bytes, DiffViewCtx, .path = view->sides);
    view->same = view->sides + view->sides_length + 1ULL;
    view->same_length = EMBED_CALL(text_bytes, DiffViewCtx, .path = view->same);
    return 1;
}

// the side of the file `path` as it stands: the open view's text where one is open on it, else the file stored under a
// key of the view's own, else an empty text where it cannot be read
EMBED_INLINE void file_side(const DiffViewCtx *args)
{
    DiffViewSide *const side = args->side;
    const unsigned int views = cli_view_count();
    for (unsigned int at = 0u; (side->key == NULL) && (at < views); at += 1u)
    {
        const char *const open = cli_view_path_at(at);
        side->key =
            ((open != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = open, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
                ? cli_view_key(at)
                : NULL;
    }
    if (side->key != NULL)
    {
        return;
    }
    VerbaLinea keyed = {side->keyed, sizeof(side->keyed), 0ULL};
    mmgr_line_put(&keyed, DIFF_VIEW_KEY);
    mmgr_line_put(&keyed, args->path);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (mmgr_line_finish(&keyed) &&
        (EMBED_CALL(cli_files_opening.open, CliFilesOpenCfg, .root = args->root, .file = args->path, .key = side->keyed,
                    .err_msg = &err_msg) == CLI_FILES_OPEN_WHOLE))
    {
        side->key = side->keyed;
        side->own = 1;
        return;
    }
    side->text = "";
    side->size = 0ULL;
}

// the side of a text git or the history reads, `file` the source it is read for: the file as the last commit has it,
// as commit `id` has it, or as its first parent has it, or the open view's snapshot; into a cell of the read alone: 1
// where it was read, or 0 with the side absent and why in `err_msg`, which may be NULL
EMBED_INLINE int read_side(const DiffViewCtx *args)
{
    DiffViewSide *const side = args->side;
    side->absent = 1;
    side->scratch = g_cli_guard->temporary_buf_alloc(DIFF_VIEW_TEXT_MAX, g_cli_diff_view_owner);
    if (side->scratch == NULL)
    {
        return 0;
    }
    VerbaLinea out = {side->scratch, DIFF_VIEW_TEXT_MAX, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea quiet = {why, sizeof(why), 0ULL};
    VerbaLinea *const err_msg = (args->err_msg != NULL) ? args->err_msg : &quiet;
    int read = 0;
    switch (args->file)
    {
    case CLI_DIFF_VIEW_HEAD:
        read = EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg, .root = args->root,
                          .file = args->path, .out = &out);
        break;
    case CLI_DIFF_VIEW_PARENT:
        read = EMBED_CALL(cli_git_root_file.text_before, CliGitRootFileCfg, .root = args->root, .file = args->path,
                          .out = &out, .err_msg = err_msg, .id = args->id);
        break;
    case CLI_DIFF_VIEW_SNAPSHOT:
        read = EMBED_CALL(cli_history.read, CliHistoryCfg, .root = args->root, .path = args->path, .err_msg = err_msg,
                          .out = &out, .at = g_cli_diff_view.at_ms);
        break;
    default:
        read = EMBED_CALL(cli_git_root_file.text_at, CliGitRootFileCfg, .root = args->root, .file = args->path,
                          .out = &out, .err_msg = err_msg, .id = args->id);
        break;
    }
    if (!read || !mmgr_line_ok(&out))
    {
        return 0;
    }
    side->absent = 0;
    side->text = side->scratch;
    side->size = out.at;
    return 1;
}

// the side laid as lines of units into a cell of the read alone, into `lines` with the cell into `cell_out`: 1, or 0
// where the cells cannot meet it; an absent side has no line and no cell
EMBED_INLINE int side_laid(const DiffViewCtx *args)
{
    const DiffViewSide *const side = args->side;
    CliLines *const lines = args->lines;
    *lines = (CliLines){0};
    *args->cell_out = NULL;
    if (side->absent)
    {
        return 1;
    }
    unsigned long long count = 1ULL;
    unsigned long long units = side->size;
    if (side->key != NULL)
    {
        CliShape shape = {0ULL, 0ULL};
        if (!EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = side->key, .shape = &shape))
        {
            return 0;
        }
        count = shape.lines;
        units = shape.units;
    }
    else
    {
        for (unsigned long long at = 0ULL; at < side->size;)
        {
            const char *const newline =
                memor.chr(&(MemoriaCfg){.src = side->text + at, .val = '\n', .bytes = side->size - at});
            if (newline == NULL)
            {
                break;
            }
            count += 1ULL;
            // the newline found lies in the text, at or past `at`
            at = (unsigned long long)(newline - side->text) + 1ULL;
        }
    }
    unsigned char *const cell =
        g_cli_guard->temporary_buf_alloc((16ULL * count) + (2ULL * units) + 2ULL, g_cli_diff_view_owner);
    if (cell == NULL)
    {
        return 0;
    }
    *args->cell_out = cell;
    lines->start = (unsigned long long *)(void *)cell;
    lines->length = lines->start + count;
    lines->unit = (unsigned short *)(void *)(lines->length + count);
    lines->units_max = units + 1ULL;
    // a text's lines are far fewer than an unsigned int counts, as the diff bounds them
    lines->lines_max = (unsigned int)count;
    if (side->key == NULL)
    {
        EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = side->text, .size = side->size,
                   .lines_out = lines);
        return !lines->full;
    }
    for (unsigned int line = 0u; line < lines->lines_max; line += 1u)
    {
        unsigned long long length = 0ULL;
        const unsigned short *const from = EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg,
                                                      .key = side->key, .line = line, .count = &length);
        const unsigned long long fits = (lines->used + length <= units) ? length : 0ULL;
        lines->start[line] = lines->used;
        lines->length[line] = fits;
        if ((from != NULL) && (fits != 0ULL))
        {
            memor.cpy(&(MemoriaCfg){.dst = lines->unit + lines->used, .src = from, .bytes = 2ULL * fits});
        }
        lines->used += fits;
        lines->lines += 1u;
    }
    return 1;
}

// the side's lines `lines_read` copied into the view's lines `lines` from `cell_out`'s start, its starts and lengths
// then its units, the place after them into `cell_out`
EMBED_INLINE void side_copied(const DiffViewCtx *args)
{
    const CliLines *const from = args->lines_read;
    CliLines *const to = args->lines;
    unsigned char *const at = *args->cell_out;
    *to = (CliLines){0};
    to->start = (unsigned long long *)(void *)at;
    to->length = to->start + from->lines;
    to->unit = (unsigned short *)(void *)(to->length + from->lines);
    to->lines = from->lines;
    to->lines_max = from->lines;
    to->used = from->used;
    to->units_max = from->used;
    if (from->lines != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = to->start, .src = from->start, .bytes = 8ULL * from->lines});
        memor.cpy(&(MemoriaCfg){.dst = to->length, .src = from->length, .bytes = 8ULL * from->lines});
    }
    if (from->used != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = to->unit, .src = from->unit, .bytes = 2ULL * from->used});
    }
    *args->cell_out = at + (16ULL * from->lines) + (((2ULL * from->used) + 7ULL) & ~7ULL);
}

// the two sides read for the view's source: the older into `side[0]` and the newer into `side[1]`, `on` where the
// last commit contains the file: 1, or 0 with why in `err_msg`
EMBED_INLINE int sides_read(const DiffViewCtx *args)
{
    const CliDiffView *const view = &g_cli_diff_view;
    DiffViewSide *const older = &args->side[0];
    DiffViewSide *const newer = &args->side[1];
    switch (view->source)
    {
    case CLI_DIFF_VIEW_HEAD:
        (void)EMBED_CALL(read_side, DiffViewCtx, .side = older, .root = view->root, .path = view->path,
                         .file = CLI_DIFF_VIEW_HEAD);
        EMBED_CALL(file_side, DiffViewCtx, .side = newer, .root = view->root, .path = view->path);
        return 1;
    case CLI_DIFF_VIEW_COMMIT:
        EMBED_CALL(file_side, DiffViewCtx, .side = newer, .root = view->root, .path = view->path);
        return EMBED_CALL(read_side, DiffViewCtx, .side = older, .root = view->root, .path = view->path, .id = view->id,
                          .file = CLI_DIFF_VIEW_COMMIT, .err_msg = args->err_msg);
    case CLI_DIFF_VIEW_COMMITS:
        return EMBED_CALL(read_side, DiffViewCtx, .side = older, .root = view->root, .path = view->path,
                          .id = args->on ? view->id : view->other_id, .file = CLI_DIFF_VIEW_COMMIT,
                          .err_msg = args->err_msg) &&
               EMBED_CALL(read_side, DiffViewCtx, .side = newer, .root = view->root, .path = view->path,
                          .id = args->on ? view->other_id : view->id, .file = CLI_DIFF_VIEW_COMMIT,
                          .err_msg = args->err_msg);
    case CLI_DIFF_VIEW_PARENT:
        (void)EMBED_CALL(read_side, DiffViewCtx, .side = older, .root = view->root, .path = view->path, .id = view->id,
                         .file = CLI_DIFF_VIEW_PARENT);
        if (!EMBED_CALL(read_side, DiffViewCtx, .side = newer, .root = view->root, .path = view->path, .id = view->id,
                        .file = CLI_DIFF_VIEW_COMMIT))
        {
            newer->absent = 0;
            newer->text = "";
            newer->size = 0ULL;
        }
        return 1;
    case CLI_DIFF_VIEW_CLIPBOARD:
        older->text = view->text;
        older->size = view->size;
        EMBED_CALL(file_side, DiffViewCtx, .side = newer, .root = view->root, .path = view->path);
        return 1;
    case CLI_DIFF_VIEW_FILES:
        EMBED_CALL(file_side, DiffViewCtx, .side = older, .root = view->root, .path = view->path);
        EMBED_CALL(file_side, DiffViewCtx, .side = newer, .root = view->root, .path = view->other_path);
        return 1;
    default:
        EMBED_CALL(file_side, DiffViewCtx, .side = newer, .root = view->root, .path = view->path);
        return EMBED_CALL(read_side, DiffViewCtx, .side = older, .root = view->root, .path = view->path,
                          .file = CLI_DIFF_VIEW_SNAPSHOT, .err_msg = args->err_msg);
    }
}

// the two sides' cells of the read alone released and the files stored for the view alone let go
EMBED_INLINE void sides_released(const DiffViewCtx *args)
{
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        DiffViewSide *const side = &args->side[at];
        g_cli_guard->persistent_buf_release(side->scratch, g_cli_diff_view_owner);
        side->scratch = NULL;
        if (side->own)
        {
            EMBED_CALL(cli_document.release, CliDocumentCfg, .key = side->key);
            side->own = 0;
        }
    }
}

// the two sides read and laid into the view's lines cell, sized to the two: 1, or 0 with why in `err_msg`
EMBED_INLINE int lines_laid(const DiffViewCtx *args)
{
    DiffViewSide sides[2];
    memor.set(&(MemoriaCfg){.dst = sides, .val = 0, .bytes = sizeof(sides)});
    const int read = EMBED_CALL(sides_read, DiffViewCtx, .side = sides, .err_msg = args->err_msg, .on = args->on);
    CliLines laid[2];
    unsigned char *cells[2] = {NULL, NULL};
    int fits = read;
    for (unsigned int at = 0u; fits && (at < 2u); at += 1u)
    {
        fits = EMBED_CALL(side_laid, DiffViewCtx, .side = &sides[at], .lines = &laid[at], .cell_out = &cells[at]);
    }
    EMBED_CALL(sides_released, DiffViewCtx, .side = sides);
    unsigned char *cell = NULL;
    if (fits)
    {
        const unsigned long long bytes = (16ULL * ((unsigned long long)laid[0].lines + laid[1].lines)) +
                                         (((2ULL * laid[0].used) + 7ULL) & ~7ULL) +
                                         (((2ULL * laid[1].used) + 7ULL) & ~7ULL) + 8ULL;
        cell = g_cli_guard->persistent_buf_alloc(bytes, g_cli_diff_view_owner);
    }
    if (cell != NULL)
    {
        g_cli_diff_view.lines_cell = cell;
        unsigned char *at = cell;
        EMBED_CALL(side_copied, DiffViewCtx, .lines_read = &laid[0], .lines = &g_cli_diff_view.left, .cell_out = &at);
        EMBED_CALL(side_copied, DiffViewCtx, .lines_read = &laid[1], .lines = &g_cli_diff_view.right, .cell_out = &at);
    }
    g_cli_guard->persistent_buf_release(cells[0], g_cli_diff_view_owner);
    g_cli_guard->persistent_buf_release(cells[1], g_cli_diff_view_owner);
    if (read && (cell == NULL))
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the two texts of ");
        mmgr_line_put(args->err_msg, g_cli_diff_view.path);
    }
    return cell != NULL;
}

// the lines `lines_read` with white space set aside into `lines` from a cell of the read alone, into `cell_out`: each
// run of white space one space and none at a line's ends: 1, or 0 where the cells cannot meet it
EMBED_INLINE int lines_loosened(const DiffViewCtx *args)
{
    const CliLines *const from = args->lines_read;
    CliLines *const to = args->lines;
    *to = (CliLines){0};
    unsigned char *const cell =
        g_cli_guard->temporary_buf_alloc((16ULL * from->lines) + (2ULL * from->used) + 8ULL, g_cli_diff_view_owner);
    *args->cell_out = cell;
    if (cell == NULL)
    {
        return 0;
    }
    to->start = (unsigned long long *)(void *)cell;
    to->length = to->start + from->lines;
    to->unit = (unsigned short *)(void *)(to->length + from->lines);
    to->lines = from->lines;
    to->lines_max = from->lines;
    for (unsigned int line = 0u; line < from->lines; line += 1u)
    {
        to->start[line] = to->used;
        const unsigned short *const units = from->unit + from->start[line];
        int spaced = 0;
        for (unsigned long long at = 0ULL; at < from->length[line]; at += 1ULL)
        {
            const unsigned short unit = units[at];
            const int white = (unit == 0x20u) || (unit == 0x09u) || (unit == 0x0bu) || (unit == 0x0cu) ||
                              (unit == 0x0du) || (unit == 0xa0u);
            if (white)
            {
                spaced = to->used > to->start[line];
                continue;
            }
            if (spaced)
            {
                to->unit[to->used] = 0x20u;
                to->used += 1ULL;
                spaced = 0;
            }
            to->unit[to->used] = unit;
            to->used += 1ULL;
        }
        to->length[line] = to->used - to->start[line];
    }
    to->units_max = to->used;
    return 1;
}

// a change handed by the diff put after the changes found
static void hunk_found(const CliEachHunkCfg *args)
{
    DiffViewHunks *const found = args->context;
    if (found->hunks >= found->capacity)
    {
        return;
    }
    unsigned int *const hunk = found->hunk + (4u * found->hunks);
    hunk[0] = args->now_from;
    hunk[1] = args->now_to;
    hunk[2] = args->then_from;
    hunk[3] = args->then_to;
    found->hunks += 1u;
}

// the rows laid from the changes `found` handed, the lines both sides share level, each change's lines paired, its
// first row among the changes' first rows, into a cell sized to them: 1, or 0 where the cells cannot meet it
EMBED_INLINE int rows_laid(const DiffViewCtx *args)
{
    const DiffViewHunks *const found = (const DiffViewHunks *)(const void *)args->lines_read;
    CliDiffView *const view = &g_cli_diff_view;
    if (found->hunks == 0u)
    {
        return 1;
    }
    unsigned int rows = 0u;
    unsigned int right = 0u;
    for (unsigned int at = 0u; at < found->hunks; at += 1u)
    {
        const unsigned int *const hunk = found->hunk + (4u * at);
        const unsigned int gone = hunk[3] - hunk[2];
        const unsigned int come = hunk[1] - hunk[0];
        rows += (hunk[0] - right) + ((gone > come) ? gone : come);
        right = hunk[1];
    }
    rows += view->right.lines - right;
    const unsigned int *const last = found->hunk + (4u * (found->hunks - 1u));
    const unsigned int left_after = last[3] + (view->right.lines - last[1]);
    rows += (view->left.lines > left_after) ? view->left.lines - left_after : 0u;
    unsigned char *const cell = g_cli_guard->persistent_buf_alloc(
        ((unsigned long long)rows * sizeof(CliDiffViewLaid)) + (4ULL * found->hunks), g_cli_diff_view_owner);
    if (cell == NULL)
    {
        return 0;
    }
    view->rows_cell = cell;
    view->row = (CliDiffViewLaid *)(void *)cell;
    view->change_row = (unsigned int *)(void *)(cell + ((unsigned long long)rows * sizeof(CliDiffViewLaid)));
    view->changes = found->hunks;
    unsigned int laid = 0u;
    unsigned int left = 0u;
    right = 0u;
    for (unsigned int at = 0u; at <= found->hunks; at += 1u)
    {
        const unsigned int *const hunk = (at < found->hunks) ? found->hunk + (4u * at) : NULL;
        const unsigned int to = (hunk != NULL) ? hunk[0] : view->right.lines;
        while (right < to)
        {
            view->row[laid] = (CliDiffViewLaid){left, right, CLI_SAME, right};
            laid += 1u;
            left += 1u;
            right += 1u;
        }
        if (hunk == NULL)
        {
            break;
        }
        view->change_row[at] = laid;
        const unsigned int gone = hunk[3] - hunk[2];
        const unsigned int come = hunk[1] - hunk[0];
        for (unsigned int line = 0u; line < ((gone > come) ? gone : come); line += 1u)
        {
            const unsigned int from = (line < gone) ? left + line : CLI_DIFF_VIEW_NONE;
            const unsigned int into = (line < come) ? right + line : CLI_DIFF_VIEW_NONE;
            const unsigned int kind = ((from != CLI_DIFF_VIEW_NONE) && (into != CLI_DIFF_VIEW_NONE)) ? CLI_CHANGED
                                      : (from == CLI_DIFF_VIEW_NONE) ? CLI_DIFF_VIEW_ADDED
                                                                     : CLI_DIFF_VIEW_REMOVED;
            view->row[laid] = (CliDiffViewLaid){from, into, kind, right + ((line < come) ? line : come)};
            laid += 1u;
        }
        left += gone;
        right += come;
    }
    while ((left < view->left.lines) && (laid < rows))
    {
        view->row[laid] = (CliDiffViewLaid){left, CLI_DIFF_VIEW_NONE, CLI_DIFF_VIEW_REMOVED, right};
        laid += 1u;
        left += 1u;
    }
    view->rows = laid;
    return 1;
}

// the view's lines compared, white space set aside where the option is on, and the rows laid from the changes: 1, or
// 0 where the cells cannot meet it; texts too far apart lay no row and mark the view refused
EMBED_INLINE int compared(const DiffViewCtx *args)
{
    CliDiffView *const view = &g_cli_diff_view;
    CliLines loose[2];
    unsigned char *cells[2] = {NULL, NULL};
    const int space = view->space && !view->structure;
    int ready = 1;
    if (space)
    {
        ready = EMBED_CALL(lines_loosened, DiffViewCtx, .lines_read = &view->left, .lines = &loose[0],
                           .cell_out = &cells[0]) &&
                EMBED_CALL(lines_loosened, DiffViewCtx, .lines_read = &view->right, .lines = &loose[1],
                           .cell_out = &cells[1]);
    }
    const unsigned long long capacity = (unsigned long long)view->left.lines + view->right.lines + 1ULL;
    DiffViewHunks found = {NULL, 0u, 0u};
    found.hunk = ready ? g_cli_guard->temporary_buf_alloc(16ULL * capacity, g_cli_diff_view_owner) : NULL;
    // the changes of two texts are fewer than their lines and one, far inside an unsigned int
    found.capacity = (unsigned int)capacity;
    int laid = 0;
    if (found.hunk != NULL)
    {
        const int changed = EMBED_CALL(cli_document_out_then.line_changes, CliDocumentOutThenCfg,
                                       .then = space ? &loose[0] : &view->left, .now = space ? &loose[1] : &view->right,
                                       .each = hunk_found, .context = &found);
        view->refused = !changed;
        laid = !changed || EMBED_CALL(rows_laid, DiffViewCtx, .lines_read = (const CliLines *)(const void *)&found);
    }
    g_cli_guard->persistent_buf_release(found.hunk, g_cli_diff_view_owner);
    g_cli_guard->persistent_buf_release(cells[0], g_cli_diff_view_owner);
    g_cli_guard->persistent_buf_release(cells[1], g_cli_diff_view_owner);
    (void)args;
    return laid;
}

int cli_diff_view_open(const CliDiffViewSourceErrMsgCfg *args)
{
    // the file: the one asked for, else the shown view's
    const int shown = cli_view_shown();
    // the view shown is among the views
    const char *const path =
        (args->path != NULL) ? args->path : ((shown >= 0) ? cli_view_path_at((unsigned int)shown) : NULL);
    if ((args->root == NULL) || (path == NULL) ||
        ((args->source == CLI_DIFF_VIEW_FILES) && (args->other_path == NULL)) ||
        (((args->source == CLI_DIFF_VIEW_COMMIT) || (args->source == CLI_DIFF_VIEW_COMMITS) ||
          (args->source == CLI_DIFF_VIEW_PARENT)) &&
         (args->id == NULL)) ||
        ((args->source == CLI_DIFF_VIEW_COMMITS) && (args->other_id == NULL)))
    {
        mmgr_line_put(args->err_msg, DIFF_VIEW_NO_FILE);
        EMBED_CALL(view_released, DiffViewCtx, .on = 0);
        return 0;
    }
    // which of two commits is the older
    const int on = (args->source != CLI_DIFF_VIEW_COMMITS) ||
                   EMBED_CALL(commit_older, DiffViewCtx, .root = args->root, .id = args->id, .other = args->other_id);
    char *const scratch = g_cli_guard->temporary_buf_alloc(DIFF_VIEW_SENTENCE_MAX, g_cli_diff_view_owner);
    if (scratch == NULL)
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the view's sentences");
        return 0;
    }
    VerbaLinea sentences = {scratch, DIFF_VIEW_SENTENCE_MAX, 0ULL};
    EMBED_CALL(sentences_written, DiffViewCtx, .source = args, .path = path, .out = &sentences, .on = on);
    const int copied = mmgr_line_ok(&sentences) &&
                       EMBED_CALL(sources_copied, DiffViewCtx, .source = args, .path = path, .out = &sentences);
    g_cli_guard->persistent_buf_release(scratch, g_cli_diff_view_owner);
    if (!copied || !EMBED_CALL(lines_laid, DiffViewCtx, .err_msg = args->err_msg, .on = on) ||
        !EMBED_CALL(compared, DiffViewCtx, .on = on))
    {
        if (copied && (args->err_msg->at == 0ULL))
        {
            mmgr_line_put(args->err_msg, "the cells cannot meet the changes of ");
            mmgr_line_put(args->err_msg, g_cli_diff_view.path);
        }
        EMBED_CALL(view_released, DiffViewCtx, .on = 0);
        return 0;
    }
    // a file the last commit does not contain has no line on the left, and the sides sentence for such a file
    if ((g_cli_diff_view.source == CLI_DIFF_VIEW_HEAD) && (g_cli_diff_view.left.lines == 0u))
    {
        g_cli_diff_view.sides = DIFF_VIEW_UNTRACKED_SIDES;
        g_cli_diff_view.sides_length = sizeof(DIFF_VIEW_UNTRACKED_SIDES) - 1ULL;
    }
    g_cli_diff_view.open = 1;
    g_cli_diff_view.change_at = 0u;
    cli_diff_view_where_written(&(CliDiffViewAtRowLengthCfg){.at = 0u});
    return 1;
}

void cli_diff_view_close(const CliDiffViewSourceErrMsgCfg *args)
{
    (void)args;
    EMBED_CALL(view_released, DiffViewCtx, .on = 0);
}
