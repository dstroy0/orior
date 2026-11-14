// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_changes.c: the changes as the window reads them, and the next commit. Each read lists the tree's changed files
// through the repositories' status into the list read from git, whose texts are in a movable cell until the next
// read, and every row points into it, and reads how far the branch is from the remote it follows. The next commit
// takes every file but those left out: whether each file of the list is left out is as a byte a file in one movable
// cell, and the paths left out are in one movable cell of CHANGES_LEFT_MAX paths, so the choice outlasts the list's
// next read. A file's changes open under its row: the document store's changes of its text, the open view's where one
// is open on it and else the file stored for the changes alone, against the file as the last commit has it, which the
// store has beside the text; the open parts are as CHANGES_PARTS_MAX records in one movable cell, each part's
// changes in a movable cell of its own with a byte a line of them, 1 where the line is left out, which then takes the
// file with the text of its lines taken. The message typed is in the typed text's cell, and git's refusal of the
// last commit, pull, push or roll back in one movable cell of CLI_CHANGES_REFUSAL_MAX bytes. Every cell but a part's is
// taken once at its max; a part's cell is sized to the changes the diff counts. The watch marks the changes stale from
// its thread, and the step reads them again
#include "git.h"

#include "../command_line/commands.h"
#include "../editor/document.h"
#include "../editor/files.h"
#include "../editor/view.h"
#include "../text/typed.h"
#include "embed_atomic.h"

// why nothing was committed, the bytes git's own words of a commit, a pull or a push may take, the bytes a file's text
// as the last commit has it may take, the key a file is stored under for its changes alone, no place, the max parts
// open at once, and the max paths left out retained across the list's reads
#define CHANGES_EMPTY "The commit message is empty."
#define CHANGES_NONE "No file is taken."
#define CHANGES_OUT 4096ULL
#define CHANGES_TEXT_MAX (1ULL << 20u)
#define CHANGES_KEY "changes:"
#define CHANGES_NONE_AT 0xffffffffu
#define CHANGES_PARTS_MAX 64u
#define CHANGES_LEFT_MAX 256u
// the bytes a key is copied into, and the bytes zeroed at a time as a part's line bytes are cleared
#define CHANGES_KEY_MAX (CLI_PATH_MAX_LEN + sizeof(CHANGES_KEY))
#define CHANGES_ZERO_STEP 4096ULL

// the list, located between the window's frames until the next read
static CliChangedList s_changed;

// A file whose changes are open under its row: its cell and where each span is in it, its path with its end and
// its bytes, the key its text is stored under in the document store with its bytes, its changes as the diff passes
// them, four numbers each, the newer text's first line and the line after its last then the older's, the row under the
// file's row each change starts at, one more than the changes, so that a change's first line's byte is its row less
// its place, how many lines of each change are left out, and a byte a line of the changes in the changes' order, each
// change's lines taken out then its lines put in, 1 where the line is left out. Beside the cell: how many changes, how
// many line bytes, how many are 1, how many rows are under the file's row, whether the text was stored for the
// changes alone and goes as they close, and the file's place in the list
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long path_at;
    unsigned long long path_bytes;
    unsigned long long key_at;
    unsigned long long key_bytes;
    unsigned long long hunk_at;
    unsigned long long hunk_row_at;
    unsigned long long hunk_out_at;
    unsigned long long out_at;
    unsigned int hunks;
    unsigned int outs;
    unsigned int out_count;
    unsigned int rows;
    int own;
    unsigned int file;
} ChangesPart;

// the changes of a text counted as the diff passes them: how many, and how many lines they take out and put in
typedef struct
{
    unsigned int hunks;
    unsigned int keys;
} ChangesCount;

// The next commit: its message, the paths left out, CHANGES_LEFT_MAX of CLI_PATH_MAX_LEN each with its end, and how
// many, a byte a file of the list, 1 where the file is left out, and whether the list's bytes are, how many files
// are taken, the parts open and how many, the row each file's row is at, one more than the files, each file's open
// part's place, CHANGES_NONE_AT for none, and whether the two are for the list, git's refusal of the last commit,
// pull, push or roll back with its bytes, whether the list was read since the tree was opened, how far the branch
// is from the remote it follows, 1 where it follows one, and the millisecond a roll back was armed at, 0 for not
// armed
typedef struct
{
    CliTyped message;
    const CarcerMovableEntry *left;
    unsigned int lefts;
    const CarcerMovableEntry *out;
    int out_located;
    unsigned int taken;
    const CarcerMovableEntry *part;
    unsigned int parts;
    const CarcerMovableEntry *first_row;
    const CarcerMovableEntry *file_part;
    int rows_located;
    const CarcerMovableEntry *refusal;
    unsigned long long refusal_length;
    int listed;
    int followed;
    unsigned int ahead;
    unsigned int behind;
    unsigned long long armed_ms;
} Changes;

static Changes s_changes;
static void (*s_woken)(void);
static embed_atomic_u32 s_stale;

typedef struct
{
    const char *path;
    const char *text;
    unsigned long long size;
    const char *root;
    int (*run)(const CliGitRootOutCountErrMsgCfg *args);
    ChangesPart *part;
    ChangesCount *count;
    unsigned int file;
    unsigned int at;
    unsigned int hunk;
    unsigned int key;
    int on;
    char state;
    const char *key_text;
    char *written;
    char *path_out;
    const CarcerMovableEntry *cell;
    unsigned long long offset;
    unsigned int index;
} ChangesCtx;

// A cell taken once at its max, `bytes`, where none is: 1, or 0 where it could not be taken
static int cell_taken(const CarcerMovableEntry **cell, unsigned long long bytes)
{
    if (*cell == NULL)
    {
        *cell = g_cli_guard->movable_buf_alloc(bytes, g_cli_git_owner);
    }
    return *cell != NULL;
}

// `size` bytes at `offset` of `cell` zeroed, CHANGES_ZERO_STEP at a time
EMBED_INLINE void cell_zeroed(const ChangesCtx *args)
{
    static const unsigned char s_zeros[CHANGES_ZERO_STEP] = {0};
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned long long step = (args->size - at < CHANGES_ZERO_STEP) ? args->size - at : CHANGES_ZERO_STEP;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = args->cell, .offset = args->offset + at, .src = s_zeros, .bytes = step});
        at += step;
    }
}

// the byte of the file `file` of the list, 1 where it is left out, 0 where the list's bytes do not are; and one
// written
EMBED_INLINE unsigned char file_out(const ChangesCtx *args)
{
    unsigned char byte_7 = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_changes.out, .offset = 0ULL + args->file, .dst = &byte_7, .bytes = 1ULL});

    return s_changes.out_located ? byte_7 : 0u;
}

EMBED_INLINE void file_out_put(const ChangesCtx *args)
{
    if (s_changes.out_located)
    {
        {
            // whether a line or a file is left out is 0 or 1, which a byte carries
            const unsigned char value = (unsigned char)args->on;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = s_changes.out, .offset = 0ULL + args->file, .src = &value, .bytes = 1ULL});
        }
    }
}

// the row each file's row is at, `file` from 0 to the files' count, and the part of the file `file`,
// CHANGES_NONE_AT where its changes are not open
EMBED_INLINE unsigned int file_first_row(const ChangesCtx *args)
{
    unsigned int value_3 = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_changes.first_row,
                            .offset = 0ULL + ((unsigned long long)args->file * sizeof(value_3)),
                            .dst = &value_3,
                            .bytes = sizeof(value_3)});

    return value_3;
}

EMBED_INLINE unsigned int file_part_at(const ChangesCtx *args)
{
    unsigned int value_4 = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = s_changes.file_part,
                            .offset = 0ULL + ((unsigned long long)args->file * sizeof(value_4)),
                            .dst = &value_4,
                            .bytes = sizeof(value_4)});

    return s_changes.rows_located ? value_4 : CHANGES_NONE_AT;
}

// `path` written as the path left out at `at`, its end after it
EMBED_INLINE void left_put(const ChangesCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN - 1ULL});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_changes.left,
                                                             .offset = (unsigned long long)args->at * CLI_PATH_MAX_LEN,
                                                             .src = args->path,
                                                             .bytes = length});
    const char end = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_changes.left,
                            .offset = ((unsigned long long)args->at * CLI_PATH_MAX_LEN) + length,
                            .src = &end,
                            .bytes = 1ULL});
}

// whether `path` is among the paths left out
EMBED_INLINE int left_has(const ChangesCtx *args)
{
    char one[CLI_PATH_MAX_LEN];
    for (unsigned int at = 0u; (s_changes.left != NULL) && (at < s_changes.lefts); at += 1u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.left,
                                                                .offset = (unsigned long long)at * CLI_PATH_MAX_LEN,
                                                                .dst = one,
                                                                .bytes = CLI_PATH_MAX_LEN});
        if (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
        {
            return 1;
        }
    }
    return 0;
}

// the path of the file `file` of the list copied into `path_out`, CLI_PATH_MAX_LEN bytes
EMBED_INLINE const char *file_path(const ChangesCtx *args)
{
    return EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_changed.text,
                      .at = s_changed.item[args->file].path, .dst = args->path_out, .capacity = CLI_PATH_MAX_LEN);
}

// the choice settled after the bytes a file changed: the files taken counted, and the paths left out written again
// from the files marked out, CHANGES_LEFT_MAX of them at max, the rest taken again at the next read
EMBED_INLINE void choice_settled(const ChangesCtx *args)
{
    (void)args;
    unsigned int taken = 0u;
    unsigned int lefts = 0u;
    char path[CLI_PATH_MAX_LEN];
    const int retained = s_changes.out_located && cell_taken(&s_changes.left, CHANGES_LEFT_MAX * CLI_PATH_MAX_LEN);
    for (unsigned int at = 0u; s_changes.out_located && (at < s_changed.count); at += 1u)
    {
        const unsigned char out = EMBED_CALL(file_out, ChangesCtx, .file = at);
        taken += out ? 0u : 1u;
        if (out && retained && (lefts < CHANGES_LEFT_MAX))
        {
            (void)EMBED_CALL(file_path, ChangesCtx, .file = at, .path_out = path);
            EMBED_CALL(left_put, ChangesCtx, .at = lefts, .path = path);
            lefts += 1u;
        }
    }
    s_changes.taken = taken;
    s_changes.lefts = lefts;
}

// git's refusal set to `size` bytes of `text`, CLI_CHANGES_REFUSAL_MAX - 1 at max, with its end
EMBED_INLINE void refusal_put(const ChangesCtx *args)
{
    s_changes.refusal_length = 0ULL;
    if ((args->size == 0ULL) || !cell_taken(&s_changes.refusal, CLI_CHANGES_REFUSAL_MAX))
    {
        return;
    }
    const unsigned long long length =
        (args->size < CLI_CHANGES_REFUSAL_MAX - 1ULL) ? args->size : CLI_CHANGES_REFUSAL_MAX - 1ULL;
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_changes.refusal, .src = args->text, .bytes = length});
    const char end = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_changes.refusal, .offset = length, .src = &end, .bytes = 1ULL});
    s_changes.refusal_length = length;
}

// `size` bytes of `text` put after the refusal located, on a line of its own where one is, CLI_CHANGES_REFUSAL_MAX
// - 1 bytes at max
EMBED_INLINE void refusal_appended(const ChangesCtx *args)
{
    char joined[CLI_CHANGES_REFUSAL_MAX];
    VerbaLinea line = {joined, sizeof(joined), 0ULL};
    if (s_changes.refusal_length != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = s_changes.refusal, .dst = joined, .bytes = s_changes.refusal_length});
        line.at = s_changes.refusal_length;
        mmgr_line_ch(&line, '\n');
    }
    mmgr_line_put_n(&line, args->text, args->size);
    (void)mmgr_line_finish(&line);
    EMBED_CALL(refusal_put, ChangesCtx, .text = joined, .size = line.at);
}

// git `run` at `root`, pull or push: its words into a cell of this run alone, 1, or 0 with the refusal located
EMBED_INLINE int remote_run(const ChangesCtx *args)
{
    s_changes.refusal_length = 0ULL;
    char *const scratch = g_cli_guard->temporary_buf_alloc(CHANGES_OUT + CLI_ERR_MSG_MAX_LEN, g_cli_git_owner);
    if (scratch == NULL)
    {
        return 0;
    }
    VerbaLinea out = {scratch, CHANGES_OUT, 0ULL};
    VerbaLinea err_msg = {scratch + CHANGES_OUT, CLI_ERR_MSG_MAX_LEN, 0ULL};
    const int ran = args->run(&(CliGitRootOutCountErrMsgCfg){.root = args->root, .out = &out, .err_msg = &err_msg});
    const unsigned long long refused = (!ran && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
    EMBED_CALL(refusal_put, ChangesCtx, .text = scratch + CHANGES_OUT, .size = refused);
    g_cli_guard->persistent_buf_release(scratch, g_cli_git_owner);
    return ran;
}

// a change passed by the diff counted, with the lines it takes out and puts in
static void changes_counted(const CliEachHunkCfg *args)
{
    ChangesCount *const count = args->context;
    count->hunks += 1u;
    count->keys += (args->now_to - args->now_from) + (args->then_to - args->then_from);
}

// a change passed by the diff written into the part's cell as the next
static void changes_filled(const CliEachHunkCfg *args)
{
    ChangesPart *const part = args->context;
    const unsigned int hunk[4] = {args->now_from, args->now_to, args->then_from, args->then_to};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = part->cell,
                            .offset = part->hunk_at + ((unsigned long long)part->hunks * sizeof(hunk)),
                            .src = hunk,
                            .bytes = sizeof(hunk)});
    part->hunks += 1u;
}

// the text of the file at `path` as the last commit has it, read from git into a cell for this read alone and stored
// beside the text under `key_text` in the document store, empty for a file whose `state` is not yet tracked or added,
// and the changes between the two counted into `count`: 1, or 0 where the texts are too far apart or the cells cannot
// meet the read
EMBED_INLINE int head_stored(const ChangesCtx *args)
{
    char *const scratch = g_cli_guard->temporary_buf_alloc(CHANGES_TEXT_MAX, g_cli_git_owner);
    if (scratch == NULL)
    {
        return 0;
    }
    VerbaLinea out = {scratch, CHANGES_TEXT_MAX, 0ULL};
    const int tracked = (args->state != 'U') && (args->state != 'A') &&
                        EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg, .root = args->root,
                                   .file = args->path, .out = &out) &&
                        mmgr_line_ok(&out);
    *args->count = (ChangesCount){0u, 0u};
    const int found = EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg,
                                 .key = args->key_text, .head_given = 1, .head = scratch,
                                 .head_size = tracked ? out.at : 0ULL, .each = changes_counted, .context = args->count);
    g_cli_guard->persistent_buf_release(scratch, g_cli_git_owner);
    return found;
}

// the part's cell positioned for `path`, `key_text` and the changes counted in `count`, sized to them, the changes written
// in from the store and every line taken; the cell it had released: 1, or 0 where the cells cannot meet it, the part as
// it was
EMBED_INLINE int part_positioned(const ChangesCtx *args)
{
    ChangesPart *const part = args->part;
    const ChangesCount *const count = args->count;
    const unsigned long long path_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN - 1ULL}) + 1ULL;
    const unsigned long long key_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->key_text, .cap = CHANGES_KEY_MAX - 1ULL}) + 1ULL;
    const unsigned long long texts = (path_bytes + key_bytes + 7ULL) & ~7ULL;
    const unsigned long long numbers =
        ((4ULL * count->hunks) + (count->hunks + 1ULL) + count->hunks) * sizeof(unsigned int);
    const CarcerMovableEntry *const cell =
        g_cli_guard->movable_buf_alloc(texts + numbers + count->keys + 1ULL, g_cli_git_owner);
    if (cell == NULL)
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = cell, .src = args->path, .bytes = path_bytes});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = path_bytes, .src = args->key_text, .bytes = key_bytes});
    (void)g_cli_guard->movable_buf_release(part->cell, g_cli_git_owner);
    part->cell = cell;
    part->path_at = 0ULL;
    part->path_bytes = path_bytes;
    part->key_at = path_bytes;
    part->key_bytes = key_bytes;
    part->hunk_at = texts;
    part->hunk_row_at = texts + (4ULL * count->hunks * sizeof(unsigned int));
    part->hunk_out_at = part->hunk_row_at + ((count->hunks + 1ULL) * sizeof(unsigned int));
    part->out_at = texts + numbers;
    part->hunks = 0u;
    (void)EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = args->key_text,
                     .each = changes_filled, .context = part);
    unsigned int row = 0u;
    {
        const unsigned int value = row;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell,
                                .offset = part->hunk_row_at + ((unsigned long long)0u * sizeof(value)),
                                .src = &value,
                                .bytes = sizeof(value)});
    }
    for (unsigned int at = 0u; at < part->hunks; at += 1u)
    {
        unsigned int hunk[4];
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = part->cell,
                                .offset = part->hunk_at + ((unsigned long long)at * 4ULL * sizeof(unsigned int)),
                                .dst = hunk,
                                .bytes = 4ULL * sizeof(unsigned int)});
        row += 1u + (hunk[1] - hunk[0]) + (hunk[3] - hunk[2]);
        {
            const unsigned int value = row;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = cell,
                                    .offset = part->hunk_row_at + ((unsigned long long)(at + 1u) * sizeof(value)),
                                    .src = &value,
                                    .bytes = sizeof(value)});
        }
        {
            const unsigned int value = 0u;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = part->cell,
                                    .offset = part->hunk_out_at + ((unsigned long long)at * sizeof(value)),
                                    .src = &value,
                                    .bytes = sizeof(value)});
        }
    }
    EMBED_CALL(cell_zeroed, ChangesCtx, .cell = cell, .offset = part->out_at, .size = count->keys);
    part->outs = count->keys;
    part->out_count = 0u;
    part->rows = row;
    return 1;
}

// the part `at` taken out of the parts: the text stored for its changes alone let go, its cell released, and the
// records after it moved up
EMBED_INLINE void part_closed(const ChangesCtx *args)
{
    ChangesPart part = {0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.part,
                                                            .offset = (unsigned long long)args->at * sizeof(part),
                                                            .dst = &part,
                                                            .bytes = sizeof(part)});
    if (part.own)
    {
        char key[CHANGES_KEY_MAX];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = (&part)->cell, .offset = (&part)->key_at, .dst = key, .bytes = (&part)->key_bytes});
        EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
    }
    (void)g_cli_guard->movable_buf_release(part.cell, g_cli_git_owner);
    for (unsigned int from = args->at + 1u; from < s_changes.parts; from += 1u)
    {
        ChangesPart moved = {0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.part,
                                                                .offset = (unsigned long long)from * sizeof(moved),
                                                                .dst = &moved,
                                                                .bytes = sizeof(moved)});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = s_changes.part,
                                .offset = (unsigned long long)(from - 1u) * sizeof(ChangesPart),
                                .src = &moved,
                                .bytes = sizeof(ChangesPart)});
    }
    s_changes.parts -= 1u;
}

// the row each file's row is at and each file's part settled after the list or the parts changed, in the two cells
// taken once at the list's max
EMBED_INLINE void rows_settled(const ChangesCtx *args)
{
    (void)args;
    s_changes.rows_located = cell_taken(&s_changes.first_row, (CLI_GIT_CHANGED + 1ULL) * sizeof(unsigned int)) &&
                              cell_taken(&s_changes.file_part, CLI_GIT_CHANGED * sizeof(unsigned int));
    if (!s_changes.rows_located)
    {
        return;
    }
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        {
            const unsigned int value = CHANGES_NONE_AT;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = s_changes.file_part,
                                    .offset = 0ULL + ((unsigned long long)at * sizeof(value)),
                                    .src = &value,
                                    .bytes = sizeof(value)});
        }
    }
    for (unsigned int at = 0u; at < s_changes.parts; at += 1u)
    {
        ChangesPart part = {0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.part,
                                                                .offset = (unsigned long long)at * sizeof(part),
                                                                .dst = &part,
                                                                .bytes = sizeof(part)});
        {
            const unsigned int value = at;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = s_changes.file_part,
                                    .offset = 0ULL + ((unsigned long long)part.file * sizeof(value)),
                                    .src = &value,
                                    .bytes = sizeof(value)});
        }
    }
    unsigned int row = 0u;
    {
        const unsigned int value = row;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = s_changes.first_row,
                                .offset = 0ULL + ((unsigned long long)0u * sizeof(value)),
                                .src = &value,
                                .bytes = sizeof(value)});
    }
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        const unsigned int part = EMBED_CALL(file_part_at, ChangesCtx, .file = at);
        ChangesPart opened = {0};
        if (part != CHANGES_NONE_AT)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.part,
                                                                    .offset = (unsigned long long)part * sizeof(opened),
                                                                    .dst = &opened,
                                                                    .bytes = sizeof(opened)});
        }
        row += 1u + opened.rows;
        {
            const unsigned int value = row;
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = s_changes.first_row,
                                    .offset = 0ULL + ((unsigned long long)(at + 1u) * sizeof(value)),
                                    .src = &value,
                                    .bytes = sizeof(value)});
        }
    }
}

// every open part matched to the list read anew by its path and its changes found again against the last commit's
// file read anew, its lines' choices located where the changes are the same; closed where its file no longer differs
// or its changes cannot be found
EMBED_INLINE void parts_matched(const ChangesCtx *args)
{
    for (unsigned int at = 0u; at < s_changes.parts;)
    {
        ChangesPart part = {0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.part,
                                                                .offset = (unsigned long long)at * sizeof(part),
                                                                .dst = &part,
                                                                .bytes = sizeof(part)});
        char part_path_text[CLI_PATH_MAX_LEN];
        char part_key_text[CHANGES_KEY_MAX];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = (&part)->cell, .offset = (&part)->path_at, .dst = part_path_text, .bytes = (&part)->path_bytes});
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = (&part)->cell, .offset = (&part)->key_at, .dst = part_key_text, .bytes = (&part)->key_bytes});
        unsigned int file = CHANGES_NONE_AT;
        char path[CLI_PATH_MAX_LEN];
        for (unsigned int each = 0u; (file == CHANGES_NONE_AT) && (each < s_changed.count); each += 1u)
        {
            (void)EMBED_CALL(file_path, ChangesCtx, .file = each, .path_out = path);
            file = cellul.eq(&(CatenaFinitaCfg){.src = path, .other = part_path_text, .cap = CLI_PATH_MAX_LEN})
                       ? each
                       : CHANGES_NONE_AT;
        }
        ChangesCount count;
        if ((file == CHANGES_NONE_AT) ||
            !EMBED_CALL(head_stored, ChangesCtx, .path = part_path_text, .key_text = part_key_text, .root = args->root,
                        .state = s_changed.item[file].state, .count = &count))
        {
            EMBED_CALL(part_closed, ChangesCtx, .at = at);
            continue;
        }
        part.file = file;
        // the changes as they were, for the choices to be where they are the same: a cell of the temporary tier
        const unsigned long long was_hunk_bytes = 4ULL * part.hunks * sizeof(unsigned int);
        unsigned char *const was = g_cli_guard->temporary_buf_alloc(was_hunk_bytes + part.outs + 1ULL, g_cli_git_owner);
        const unsigned int was_hunks = part.hunks;
        const unsigned int was_outs = part.outs;
        const unsigned int was_out_count = part.out_count;
        if (was != NULL)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = part.cell, .offset = part.hunk_at, .dst = was, .bytes = was_hunk_bytes});
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = part.cell, .offset = part.out_at, .dst = was + was_hunk_bytes, .bytes = was_outs});
        }
        ChangesPart positioned = part;
        positioned.cell = NULL;
        const int relaid = EMBED_CALL(part_positioned, ChangesCtx, .part = &positioned, .path = part_path_text,
                                      .key_text = part_key_text, .count = &count);
        if (relaid)
        {
            const int same =
                (was != NULL) && (was_hunks == positioned.hunks) && (was_outs == positioned.outs) &&
                (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
                     .cell = positioned.cell, .offset = positioned.hunk_at, .src = was, .bytes = was_hunk_bytes}) == 0);
            if (same && (was_outs != 0u))
            {
                (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                    .cell = positioned.cell, .offset = positioned.out_at, .src = was + was_hunk_bytes, .bytes = was_outs});
                positioned.out_count = was_out_count;
                for (unsigned int hunk = 0u; hunk < positioned.hunks; hunk += 1u)
                {
                    unsigned int hunk_row_8 = 0u;
                    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                        .cell = (&positioned)->cell,
                        .offset = (&positioned)->hunk_row_at + ((unsigned long long)hunk * sizeof(hunk_row_8)),
                        .dst = &hunk_row_8,
                        .bytes = sizeof(hunk_row_8)});

                    const unsigned int first = hunk_row_8 - hunk;
                    unsigned int hunk_row_9 = 0u;
                    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                        .cell = (&positioned)->cell,
                        .offset = (&positioned)->hunk_row_at + ((unsigned long long)(hunk + 1u) * sizeof(hunk_row_9)),
                        .dst = &hunk_row_9,
                        .bytes = sizeof(hunk_row_9)});

                    const unsigned int last = hunk_row_9 - hunk - 1u;
                    unsigned int out = 0u;
                    for (unsigned int key = first; key < last; key += 1u)
                    {
                        out += was[was_hunk_bytes + key];
                    }
                    {
                        const unsigned int value = out;
                        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                            .cell = (&positioned)->cell,
                            .offset = (&positioned)->hunk_out_at + ((unsigned long long)hunk * sizeof(value)),
                            .src = &value,
                            .bytes = sizeof(value)});
                    }
                }
            }
            (void)g_cli_guard->movable_buf_release(part.cell, g_cli_git_owner);
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = s_changes.part,
                                    .offset = (unsigned long long)at * sizeof(ChangesPart),
                                    .src = &positioned,
                                    .bytes = sizeof(ChangesPart)});
        }
        g_cli_guard->persistent_buf_release(was, g_cli_git_owner);
        if (!relaid)
        {
            EMBED_CALL(part_closed, ChangesCtx, .at = at);
            continue;
        }
        at += 1u;
    }
}

// the last of `at` numbers from `index` of the array at `offset` of `cell` that is at max `key`: the file or the
// change a row is under
EMBED_INLINE unsigned int place_under(const ChangesCtx *args)
{
    unsigned int low = 0u;
    unsigned int high = args->at;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        unsigned int value_5 = 0u;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = args->cell,
                                .offset = args->offset + ((unsigned long long)middle * sizeof(value_5)),
                                .dst = &value_5,
                                .bytes = sizeof(value_5)});

        if (value_5 <= args->key)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// the byte of the line `key` of the part set to `on`, 1 for left out, the counts following in the record, which the
// caller writes back
EMBED_INLINE void key_set(const ChangesCtx *args)
{
    ChangesPart *const part = args->part;
    unsigned char was = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = part->cell, .offset = part->out_at + args->key, .dst = &was, .bytes = 1ULL});
    const unsigned char now = args->on ? 1u : 0u;
    if (was == now)
    {
        return;
    }
    {
        // whether a line is left out is 0 or 1, which a byte carries
        const unsigned char value = (unsigned char)now;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = part->cell, .offset = part->out_at + args->key, .src = &value, .bytes = 1ULL});
    }
    part->out_count += now ? 1u : 0u;
    part->out_count -= was ? 1u : 0u;
    unsigned int hunk_out_11 = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = part->cell,
                            .offset = part->hunk_out_at + ((unsigned long long)args->hunk * sizeof(hunk_out_11)),
                            .dst = &hunk_out_11,
                            .bytes = sizeof(hunk_out_11)});

    const unsigned int out = hunk_out_11 + (now ? 1u : 0u) - (was ? 1u : 0u);
    {
        const unsigned int value = out;
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = part->cell,
                                .offset = part->hunk_out_at + ((unsigned long long)args->hunk * sizeof(value)),
                                .src = &value,
                                .bytes = sizeof(value)});
    }
}

// after a part's lines changed: the file taken again where a line of it is, left out where its every line is, its
// lines' choices cleared then; the record changed, which the caller writes back
EMBED_INLINE void part_followed(const ChangesCtx *args)
{
    ChangesPart *const part = args->part;
    if (!s_changes.out_located)
    {
        return;
    }
    if ((part->outs != 0u) && (part->out_count == part->outs))
    {
        EMBED_CALL(file_out_put, ChangesCtx, .file = part->file, .on = 1);
        EMBED_CALL(cell_zeroed, ChangesCtx, .cell = part->cell, .offset = part->out_at, .size = part->outs);
        part->out_count = 0u;
        for (unsigned int at = 0u; at < part->hunks; at += 1u)
        {
            {
                const unsigned int value = 0u;
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = part->cell,
                                        .offset = part->hunk_out_at + ((unsigned long long)at * sizeof(value)),
                                        .src = &value,
                                        .bytes = sizeof(value)});
            }
        }
    }
    else if (args->on)
    {
        EMBED_CALL(file_out_put, ChangesCtx, .file = part->file, .on = 0);
    }
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
}

// whether the part has lines left out and lines taken, so that its file is committed in part
EMBED_INLINE int part_partial(const ChangesCtx *args)
{
    return (args->part != NULL) && (args->part->out_count != 0u) && (args->part->out_count < args->part->outs);
}

// a line of the part's texts put after the lines written so far, the older text's line `at` where `on`, else the
// newer's, as UTF-8 after a line end where a line is before it; into `written` as a line of `size` bytes where it
// is not NULL: the units the line takes, for the bytes it may take. `key_text` is the part's key
EMBED_INLINE unsigned long long part_line_written(const ChangesCtx *args)
{
    unsigned long long count = 0ULL;
    const CliDocumentLineUnitsNs *const read = &cli_document_line;
    unsigned long long (*const units_read)(const CliDocumentLineUnitsCfg *) =
        args->on ? read->head_line_units : read->line_units;
    (void)EMBED_CALL(units_read, CliDocumentLineUnitsCfg, .key = args->key_text, .line = args->at, .count = &count);
    if (args->written != NULL)
    {
        VerbaLinea *const line = (VerbaLinea *)(void *)args->written;
        mmgr_line_put(line, (args->key != 0u) ? (args->state ? "\r\n" : "\n") : "");
        // the line's units copied out into a cell of the temporary tier and written as UTF-8
        unsigned short *const units =
            (count != 0ULL) ? g_cli_guard->temporary_buf_alloc(count * sizeof(*units), g_cli_git_owner) : NULL;
        if (units != NULL)
        {
            (void)EMBED_CALL(units_read, CliDocumentLineUnitsCfg, .key = args->key_text, .line = args->at,
                             .count = &count, .dst = units, .capacity = count);
        }
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = line, .units = units,
                   .count = (units != NULL) ? count : 0ULL);
        g_cli_guard->persistent_buf_release(units, g_cli_git_owner);
    }
    return count;
}

// the text the commit takes for the part: the older text's lines, each line taken out that is left out located, each
// line put in that is taken located, joined by the stored text's line ends; written into the line `written` where it
// is not NULL: the units its lines take, which bound its bytes at three a unit with two a line end
EMBED_INLINE unsigned long long part_text_written(const ChangesCtx *args)
{
    const ChangesPart *const part = args->part;
    char key[CHANGES_KEY_MAX];
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = part->cell, .offset = part->key_at, .dst = key, .bytes = part->key_bytes});
    const int crlf = EMBED_CALL(cli_document.crlf, CliDocumentCfg, .key = key);
    unsigned long long units = 0ULL;
    unsigned int lines = 0u;
    unsigned int at = 0u;
    for (unsigned int hunk = 0u; hunk <= part->hunks; hunk += 1u)
    {
        unsigned int one[4] = {0u, 0u, 0u, 0u};
        const int last = hunk == part->hunks;
        if (!last)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = part->cell,
                                    .offset = part->hunk_at + ((unsigned long long)hunk * 4ULL * sizeof(unsigned int)),
                                    .dst = one,
                                    .bytes = 4ULL * sizeof(unsigned int)});
        }
        unsigned long long count = 0ULL;
        // the older text's lines before the change, or after the last change to its end
        while ((!last && (at < one[2])) ||
               (last && (EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = key, .line = at,
                                    .count = &count) != 0ULL)))
        {
            units += EMBED_CALL(part_line_written, ChangesCtx, .key_text = key, .on = 1, .at = at, .key = lines,
                                .state = (char)crlf, .written = args->written);
            lines += 1u;
            at += 1u;
        }
        if (last)
        {
            break;
        }
        unsigned int hunk_row_10 = 0u;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = part->cell,
                                .offset = part->hunk_row_at + ((unsigned long long)hunk * sizeof(hunk_row_10)),
                                .dst = &hunk_row_10,
                                .bytes = sizeof(hunk_row_10)});

        const unsigned int key_first = hunk_row_10 - hunk;
        const unsigned int outs = one[3] - one[2];
        for (unsigned int line = 0u; line < outs + (one[1] - one[0]); line += 1u)
        {
            const int gone = line < outs;
            unsigned char out = 0u;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = part->cell, .offset = part->out_at + (key_first + line), .dst = &out, .bytes = 1ULL});
            const int taken = gone ? out : !out;
            if (!taken)
            {
                continue;
            }
            units += EMBED_CALL(part_line_written, ChangesCtx, .key_text = key, .on = gone,
                                .at = gone ? (one[2] + line) : (one[0] + (line - outs)), .key = lines,
                                .state = (char)crlf, .written = args->written);
            lines += 1u;
        }
        at = one[3];
    }
    return (3ULL * units) + (2ULL * (unsigned long long)lines) + 1ULL;
}

// every part released, none open
EMBED_INLINE void parts_released(const ChangesCtx *args)
{
    (void)args;
    while (s_changes.parts != 0u)
    {
        EMBED_CALL(part_closed, ChangesCtx, .at = s_changes.parts - 1u);
    }
}

void cli_changes_begin(const CliGitChangesCfg *args)
{
    s_woken = args->woken;
}

void cli_changes_stale(const CliGitChangesCfg *args)
{
    (void)args;
    EMBED_ATOMIC_STORE(&s_stale, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

int cli_changes_step(const CliGitChangesCfg *args)
{
    if (!args->on || (EMBED_ATOMIC_EXCHANGE_32(&s_stale, 0u) == 0u) || !s_changes.listed)
    {
        return 0;
    }
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    return 1;
}

void cli_changes_read(const CliGitChangesCfg *args)
{
    // the list's texts are between the window's frames
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &s_changed.text);
    s_changed.text.located = 1;
    s_changed.count = 0u;
    s_changes.out_located = 0;
    s_changes.taken = 0u;
    s_changes.listed = args->root != NULL;
    if (args->root == NULL)
    {
        s_changes.lefts = 0u;
        s_changes.followed = 0;
        s_changes.ahead = 0u;
        s_changes.behind = 0u;
        EMBED_CALL(parts_released, ChangesCtx, .size = 0ULL);
        EMBED_CALL(rows_settled, ChangesCtx, .size = 0ULL);
        return;
    }
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = args->root, .named = 0,
               .found_changed_list_pointer = &s_changed);
    // a byte a file, 1 where the file was left out before this read; the paths left out that are no longer in the
    // list go with the settling
    s_changes.out_located = (s_changed.count != 0u) && cell_taken(&s_changes.out, CLI_GIT_CHANGED);
    for (unsigned int at = 0u; s_changes.out_located && (at < s_changed.count); at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        (void)EMBED_CALL(file_path, ChangesCtx, .file = at, .path_out = path);
        EMBED_CALL(file_out_put, ChangesCtx, .file = at, .on = EMBED_CALL(left_has, ChangesCtx, .path = path));
    }
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
    EMBED_CALL(parts_matched, ChangesCtx, .root = args->root);
    EMBED_CALL(rows_settled, ChangesCtx, .size = 0ULL);
    s_changes.followed = EMBED_CALL(cli_git_root_ahead.ahead_behind, CliGitRootAheadCfg, .root = args->root,
                                    .ahead = &s_changes.ahead, .behind = &s_changes.behind);
    if (!s_changes.followed)
    {
        s_changes.ahead = 0u;
        s_changes.behind = 0u;
    }
}

unsigned int cli_changes_rows(const CliGitChangesCfg *args)
{
    (void)args;
    return s_changes.rows_located ? EMBED_CALL(file_first_row, ChangesCtx, .file = s_changed.count) : s_changed.count;
}

unsigned int cli_changes_files(const CliGitChangesCfg *args)
{
    (void)args;
    return s_changed.count;
}

int cli_changes_row(const CliGitChangesCfg *args)
{
    if (!s_changes.rows_located || (args->at >= EMBED_CALL(file_first_row, ChangesCtx, .file = s_changed.count)))
    {
        return 0;
    }
    // the file whose rows the row is among, the last whose first row is at max it
    const unsigned int file = EMBED_CALL(place_under, ChangesCtx, .cell = s_changes.first_row, .offset = 0ULL,
                                         .at = s_changed.count, .key = args->at);
    const CliChanged *const changed = &s_changed.item[file];
    const unsigned int part_place = EMBED_CALL(file_part_at, ChangesCtx, .file = file);
    const unsigned int inner = args->at - EMBED_CALL(file_first_row, ChangesCtx, .file = file);
    const unsigned char out = EMBED_CALL(file_out, ChangesCtx, .file = file);
    CliChangedRow *const row = args->row;
    *row = (CliChangedRow){0};
    (void)EMBED_CALL(file_path, ChangesCtx, .file = file, .path_out = row->path);
    row->file = file;
    row->state = changed->state;
    if (inner == 0u)
    {
        ChangesPart part;
        const int open = part_place != CHANGES_NONE_AT;
        if (open)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_changes.part,
                                    .offset = (unsigned long long)part_place * sizeof(part),
                                    .dst = &part,
                                    .bytes = sizeof(part)});
        }
        row->type = CLI_CHANGES_ROW_FILE;
        row->name = row->path + changed->name;
        row->folder_length = (changed->name > 0u) ? changed->name - 1u : 0u;
        row->taken = !out;
        row->part = open && EMBED_CALL(part_partial, ChangesCtx, .part = &part);
        row->opens = (changed->state == 'M') || (changed->state == 'A') || (changed->state == 'U');
        row->open = open;
        return 1;
    }
    ChangesPart part = {0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.part,
                                                            .offset = (unsigned long long)part_place * sizeof(part),
                                                            .dst = &part,
                                                            .bytes = sizeof(part)});
    const unsigned int under = inner - 1u;
    const unsigned int hunk = EMBED_CALL(place_under, ChangesCtx, .cell = part.cell, .offset = part.hunk_row_at,
                                         .at = part.hunks, .key = under);
    unsigned int one[4];
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = (&part)->cell,
                            .offset = (&part)->hunk_at + ((unsigned long long)hunk * 4ULL * sizeof(unsigned int)),
                            .dst = one,
                            .bytes = 4ULL * sizeof(unsigned int)});
    unsigned int hunk_row = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = (&part)->cell,
                            .offset = (&part)->hunk_row_at + ((unsigned long long)hunk * sizeof(hunk_row)),
                            .dst = &hunk_row,
                            .bytes = sizeof(hunk_row)});
    unsigned int hunk_out = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = (&part)->cell,
                            .offset = (&part)->hunk_out_at + ((unsigned long long)hunk * sizeof(hunk_out)),
                            .dst = &hunk_out,
                            .bytes = sizeof(hunk_out)});
    const unsigned int offset = under - hunk_row;
    const unsigned int outs = one[3] - one[2];
    const unsigned int lines = outs + (one[1] - one[0]);
    row->hunk = hunk;
    if (offset == 0u)
    {
        row->type = CLI_CHANGES_ROW_HUNK;
        row->from = one[0];
        row->to = one[1];
        row->taken = !out && (hunk_out < lines);
        row->part = (hunk_out != 0u) && (hunk_out < lines);
        return 1;
    }
    const unsigned int line = offset - 1u;
    char key[CHANGES_KEY_MAX];
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = (&part)->cell, .offset = (&part)->key_at, .dst = key, .bytes = (&part)->key_bytes});
    row->type = CLI_CHANGES_ROW_LINE;
    row->from = one[0];
    row->to = one[1];
    row->gone = line < outs;
    unsigned long long count = 0ULL;
    if (row->gone)
    {
        (void)EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = key, .line = one[2] + line,
                         .count = &count, .dst = row->units, .capacity = CLI_CHANGED_ROW_UNITS);
    }
    else
    {
        (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = key,
                         .line = one[0] + (line - outs), .count = &count, .dst = row->units,
                         .capacity = CLI_CHANGED_ROW_UNITS);
    }
    row->count = (count < CLI_CHANGED_ROW_UNITS) ? count : CLI_CHANGED_ROW_UNITS;
    unsigned char out_byte_12 = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = (&part)->cell,
                                                            .offset = (&part)->out_at + ((hunk_row - hunk) + line),
                                                            .dst = &out_byte_12,
                                                            .bytes = 1ULL});

    row->taken = !out && !out_byte_12;
    return 1;
}

void cli_changes_toggle(const CliGitChangesCfg *args)
{
    CliChangedRow row;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &row) || !s_changes.out_located)
    {
        return;
    }
    const unsigned int part_place = EMBED_CALL(file_part_at, ChangesCtx, .file = row.file);
    const int open = part_place != CHANGES_NONE_AT;
    ChangesPart part;
    memor.set(&(MemoriaCfg){.dst = &part, .val = 0, .bytes = sizeof(part)});
    if (open)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_changes.part,
                                                                .offset = (unsigned long long)part_place * sizeof(part),
                                                                .dst = &part,
                                                                .bytes = sizeof(part)});
    }
    const unsigned char out = EMBED_CALL(file_out, ChangesCtx, .file = row.file);
    if (row.type == CLI_CHANGES_ROW_FILE)
    {
        EMBED_CALL(file_out_put, ChangesCtx, .file = row.file, .on = !out);
        // the file taken whole or left out whole: its lines' choices go
        if (open && (part.outs != 0u))
        {
            EMBED_CALL(cell_zeroed, ChangesCtx, .cell = part.cell, .offset = part.out_at, .size = part.outs);
            part.out_count = 0u;
            for (unsigned int at = 0u; at < part.hunks; at += 1u)
            {
                {
                    const unsigned int value = 0u;
                    (void)g_cli_guard->movable_buf_write(
                        &(CarcerMovableCfg){.cell = (&part)->cell,
                                            .offset = (&part)->hunk_out_at + ((unsigned long long)at * sizeof(value)),
                                            .src = &value,
                                            .bytes = sizeof(value)});
                }
            }
            (void)g_cli_guard->movable_buf_write(
                &(CarcerMovableCfg){.cell = s_changes.part,
                                    .offset = (unsigned long long)part_place * sizeof(ChangesPart),
                                    .src = &part,
                                    .bytes = sizeof(ChangesPart)});
        }
        EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
        return;
    }
    if (!open)
    {
        return;
    }
    unsigned int one[4];
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = (&part)->cell,
                            .offset = (&part)->hunk_at + ((unsigned long long)row.hunk * 4ULL * sizeof(unsigned int)),
                            .dst = one,
                            .bytes = 4ULL * sizeof(unsigned int)});
    unsigned int hunk_row = 0u;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = (&part)->cell,
                            .offset = (&part)->hunk_row_at + ((unsigned long long)row.hunk * sizeof(hunk_row)),
                            .dst = &hunk_row,
                            .bytes = sizeof(hunk_row)});
    const unsigned int key_first = hunk_row - row.hunk;
    const unsigned int lines = (one[1] - one[0]) + (one[3] - one[2]);
    if (row.type == CLI_CHANGES_ROW_HUNK)
    {
        // every line taken where some is left out or the file is, else every line left out
        unsigned int hunk_out = 0u;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = part.cell,
                                .offset = part.hunk_out_at + ((unsigned long long)row.hunk * sizeof(hunk_out)),
                                .dst = &hunk_out,
                                .bytes = sizeof(hunk_out)});
        const int on = out || (hunk_out != 0u);
        for (unsigned int line = 0u; line < lines; line += 1u)
        {
            EMBED_CALL(key_set, ChangesCtx, .part = &part, .hunk = row.hunk, .key = key_first + line, .on = !on);
        }
        EMBED_CALL(part_followed, ChangesCtx, .part = &part, .on = on);
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = s_changes.part,
                                .offset = (unsigned long long)part_place * sizeof(ChangesPart),
                                .src = &part,
                                .bytes = sizeof(ChangesPart)});
        return;
    }
    const unsigned int under = args->at - EMBED_CALL(file_first_row, ChangesCtx, .file = row.file) - 1u;
    const unsigned int key = key_first + (under - hunk_row - 1u);
    unsigned char out_byte_13 = 0u;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = (&part)->cell, .offset = (&part)->out_at + key, .dst = &out_byte_13, .bytes = 1ULL});

    const int on = out || out_byte_13;
    EMBED_CALL(key_set, ChangesCtx, .part = &part, .hunk = row.hunk, .key = key, .on = !on);
    EMBED_CALL(part_followed, ChangesCtx, .part = &part, .on = on);
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_changes.part,
                            .offset = (unsigned long long)part_place * sizeof(ChangesPart),
                            .src = &part,
                            .bytes = sizeof(ChangesPart)});
}

int cli_changes_open(const CliGitChangesCfg *args)
{
    CliChangedRow row;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &row) ||
        (row.type != CLI_CHANGES_ROW_FILE) || !row.opens)
    {
        return 0;
    }
    if (row.open)
    {
        EMBED_CALL(part_closed, ChangesCtx, .at = EMBED_CALL(file_part_at, ChangesCtx, .file = row.file));
        EMBED_CALL(rows_settled, ChangesCtx, .size = 0ULL);
        return 1;
    }
    // the parts are at their max
    if ((s_changes.parts >= CHANGES_PARTS_MAX) ||
        !cell_taken(&s_changes.part, (unsigned long long)CHANGES_PARTS_MAX * sizeof(ChangesPart)))
    {
        return 0;
    }
    // the text's key: the view's where one is open on the file, else the file stored for the changes alone
    const char *key = NULL;
    const unsigned int views = cli_view_count();
    char open[CLI_PATH_MAX_LEN];
    char opened_key[CLI_NAME_MAX_LEN];
    for (unsigned int at = 0u; (key == NULL) && (at < views); at += 1u)
    {
        key = (cli_view_path_at(at, open) &&
               cellul.eq(&(CatenaFinitaCfg){.src = open, .other = row.path, .cap = CLI_PATH_MAX_LEN}) &&
               cli_view_key(at, opened_key))
                  ? opened_key
                  : NULL;
    }
    char keyed[CHANGES_KEY_MAX];
    const int own = key == NULL;
    if (own)
    {
        VerbaLinea keyed_line = {keyed, sizeof(keyed), 0ULL};
        mmgr_line_put(&keyed_line, CHANGES_KEY);
        mmgr_line_put(&keyed_line, row.path);
        (void)mmgr_line_finish(&keyed_line);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if (EMBED_CALL(cli_files_opening.open, CliFilesOpenCfg, .root = args->root, .file = row.path, .key = keyed,
                       .err_msg = &err_msg) != CLI_FILES_OPEN_WHOLE)
        {
            return 0;
        }
        key = keyed;
    }
    ChangesCount count;
    if (!EMBED_CALL(head_stored, ChangesCtx, .path = row.path, .key_text = key, .root = args->root, .state = row.state,
                    .count = &count))
    {
        if (own)
        {
            EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
        }
        return 0;
    }
    ChangesPart part;
    memor.set(&(MemoriaCfg){.dst = &part, .val = 0, .bytes = sizeof(part)});
    part.file = row.file;
    part.own = own;
    if (!EMBED_CALL(part_positioned, ChangesCtx, .part = &part, .path = row.path, .key_text = key, .count = &count))
    {
        if (own)
        {
            EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
        }
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_changes.part,
                            .offset = (unsigned long long)s_changes.parts * sizeof(ChangesPart),
                            .src = &part,
                            .bytes = sizeof(ChangesPart)});
    s_changes.parts += 1u;
    EMBED_CALL(rows_settled, ChangesCtx, .size = 0ULL);
    return 1;
}

unsigned int cli_changes_taken(const CliGitChangesCfg *args)
{
    (void)args;
    return s_changes.taken;
}

void cli_changes_take_all(const CliGitChangesCfg *args)
{
    if (!s_changes.out_located)
    {
        return;
    }
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        EMBED_CALL(file_out_put, ChangesCtx, .file = at, .on = !args->on);
    }
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
}

int cli_changes_menu(const CliGitChangesCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &read))
    {
        return 0;
    }
    // the rows run with the file's path, which is in the list while the menu is open
    cli_menus_own_begin();
    cli_menus_own_row(
        &(CliItem){"Open File", "file-open", "", read.path, (read.state == 'D') ? "never" : "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Copy Path", "file-copy-path", "", read.path, "", "", 0, 0, 0, 0, 0u});
    return 1;
}

void cli_changes_type(const CliGitChangesCfg *args)
{
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &s_changes.message, .text = args->text, .size = args->size,
               .max = CLI_CHANGES_MESSAGE_MAX, .owner = g_cli_git_owner);
}

int cli_changes_key(const CliGitChangesCfg *args)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        return 0;
    }
    EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &s_changes.message, .owner = g_cli_git_owner);
    return 1;
}

unsigned long long cli_changes_message(const CliGitChangesCfg *args)
{
    return EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &s_changes.message, .dst = args->text_out,
                      .capacity = CLI_CHANGES_MESSAGE_MAX);
}

int cli_changes_commit(const CliGitChangesCfg *args)
{
    s_changes.refusal_length = 0ULL;
    const unsigned int taken = s_changes.taken;
    if ((s_changes.message.length == 0ULL) || (taken == 0u))
    {
        // why nothing is to commit
        const char *const why = (s_changes.message.length == 0ULL) ? CHANGES_EMPTY : CHANGES_NONE;
        const unsigned long long size =
            (s_changes.message.length == 0ULL) ? sizeof(CHANGES_EMPTY) - 1ULL : sizeof(CHANGES_NONE) - 1ULL;
        EMBED_CALL(refusal_put, ChangesCtx, .text = why, .size = size);
        return 0;
    }
    // the files taken whole and the files taken in part counted, with the bytes their paths and texts may take
    unsigned int whole = 0u;
    unsigned int partial = 0u;
    unsigned long long texts = 0ULL;
    unsigned long long path_bytes = 0ULL;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if (EMBED_CALL(file_out, ChangesCtx, .file = at))
        {
            continue;
        }
        const unsigned int part_place = EMBED_CALL(file_part_at, ChangesCtx, .file = at);
        ChangesPart part;
        const int open = part_place != CHANGES_NONE_AT;
        if (open)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_changes.part,
                                    .offset = (unsigned long long)part_place * sizeof(part),
                                    .dst = &part,
                                    .bytes = sizeof(part)});
        }
        const int in_part = open && EMBED_CALL(part_partial, ChangesCtx, .part = &part);
        partial += in_part ? 1u : 0u;
        whole += in_part ? 0u : 1u;
        texts += in_part ? EMBED_CALL(part_text_written, ChangesCtx, .part = &part) : 0ULL;
        char path[CLI_PATH_MAX_LEN];
        (void)EMBED_CALL(file_path, ChangesCtx, .file = at, .path_out = path);
        path_bytes += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    // the paths taken whole with their texts, the parts with their paths and texts, git's own words and the error
    // line, for this commit alone
    const unsigned long long paths_bytes = (unsigned long long)whole * sizeof(const char *);
    const unsigned long long parts_bytes = (unsigned long long)partial * sizeof(CliGitPart);
    char *const scratch = g_cli_guard->temporary_buf_alloc(
        paths_bytes + parts_bytes + texts + path_bytes + CHANGES_OUT + CLI_ERR_MSG_MAX_LEN, g_cli_git_owner);
    if (scratch == NULL)
    {
        return 0;
    }
    const char **const paths = (const char **)(void *)scratch;
    CliGitPart *const parts = (CliGitPart *)(void *)(scratch + paths_bytes);
    char *text_at = scratch + paths_bytes + parts_bytes;
    char *path_at = scratch + paths_bytes + parts_bytes + texts;
    VerbaLinea out = {scratch + paths_bytes + parts_bytes + texts + path_bytes, CHANGES_OUT, 0ULL};
    VerbaLinea err_msg = {scratch + paths_bytes + parts_bytes + texts + path_bytes + CHANGES_OUT, CLI_ERR_MSG_MAX_LEN,
                          0ULL};
    unsigned int put = 0u;
    unsigned int put_parts = 0u;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if (EMBED_CALL(file_out, ChangesCtx, .file = at))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        (void)EMBED_CALL(file_path, ChangesCtx, .file = at, .path_out = path);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = path_at, .src = path, .bytes = length + 1ULL});
        const char *const path_copy = path_at;
        path_at += length + 1ULL;
        const unsigned int part_place = EMBED_CALL(file_part_at, ChangesCtx, .file = at);
        ChangesPart part;
        const int open = part_place != CHANGES_NONE_AT;
        if (open)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = s_changes.part,
                                    .offset = (unsigned long long)part_place * sizeof(part),
                                    .dst = &part,
                                    .bytes = sizeof(part)});
        }
        if (open && EMBED_CALL(part_partial, ChangesCtx, .part = &part))
        {
            VerbaLinea line = {text_at, EMBED_CALL(part_text_written, ChangesCtx, .part = &part), 0ULL};
            (void)EMBED_CALL(part_text_written, ChangesCtx, .part = &part, .written = (char *)(void *)&line);
            parts[put_parts] = (CliGitPart){path_copy, text_at, line.at};
            put_parts += 1u;
            text_at += line.capacity;
        }
        else
        {
            paths[put] = path_copy;
            put += 1u;
        }
    }
    // the message copied out of the typed text's cell for git
    char message[CLI_CHANGES_MESSAGE_MAX];
    (void)EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &s_changes.message, .dst = message,
                     .capacity = sizeof(message));
    const int committed =
        (partial != 0u)
            ? EMBED_CALL(cli_git_root.commit_parts, CliGitRootCfg, .root = args->root, .out = &out, .message = message,
                         .err_msg = &err_msg, .whole = paths, .full_files_count = whole, .parts = parts,
                         .part_count = partial, .push = args->push)
            : EMBED_CALL(cli_git_root_out_count.commit, CliGitRootOutCountCfg, .root = args->root, .out = &out,
                         .count = taken, .message = message, .paths = paths, .err_msg = &err_msg, .push = args->push);
    if (committed)
    {
        EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &s_changes.message, .owner = g_cli_git_owner);
        EMBED_CALL(parts_released, ChangesCtx, .size = 0ULL);
    }
    // git's refusal
    const unsigned long long refused = (!committed && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
    EMBED_CALL(refusal_put, ChangesCtx, .text = err_msg.out, .size = refused);
    g_cli_guard->persistent_buf_release(scratch, g_cli_git_owner);
    if (committed)
    {
        EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    }
    return committed;
}

unsigned long long cli_changes_refusal(const CliGitChangesCfg *args)
{
    if (args->text_out != NULL)
    {
        if (s_changes.refusal_length != 0ULL)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = s_changes.refusal, .dst = args->text_out, .bytes = s_changes.refusal_length});
        }
        args->text_out[s_changes.refusal_length] = '\0';
    }
    return s_changes.refusal_length;
}

int cli_changes_sync(const CliGitChangesCfg *args)
{
    *args->ahead = s_changes.ahead;
    *args->behind = s_changes.behind;
    return s_changes.followed;
}

int cli_changes_pull(const CliGitChangesCfg *args)
{
    const int pulled =
        EMBED_CALL(remote_run, ChangesCtx, .root = args->root, .run = cli_git_root_out_count_err_msg.pull);
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    return pulled;
}

int cli_changes_push(const CliGitChangesCfg *args)
{
    const int pushed =
        EMBED_CALL(remote_run, ChangesCtx, .root = args->root, .run = cli_git_root_out_count_err_msg.push);
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    return pushed;
}

int cli_changes_rollback(const CliGitChangesCfg *args)
{
    if (s_changes.taken == 0u)
    {
        return 0;
    }
    if ((s_changes.armed_ms == 0ULL) || (args->now_ms - s_changes.armed_ms > CLI_CHANGES_ROLLBACK_MS))
    {
        s_changes.armed_ms = args->now_ms;
        return 0;
    }
    s_changes.armed_ms = 0ULL;
    s_changes.refusal_length = 0ULL;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if (EMBED_CALL(file_out, ChangesCtx, .file = at))
        {
            continue;
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        char path[CLI_PATH_MAX_LEN];
        (void)EMBED_CALL(file_path, ChangesCtx, .file = at, .path_out = path);
        const int rolled = EMBED_CALL(cli_git_root_err_msg_path.rollback, CliGitRootErrMsgPathCfg, .root = args->root,
                                      .path = path, .err_msg = &err_msg);
        const unsigned long long refused = (!rolled && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
        if (refused != 0ULL)
        {
            // the file's refusal put after the refusals before it on a line of its own
            EMBED_CALL(refusal_appended, ChangesCtx, .text = why, .size = refused);
        }
    }
    return 1;
}
