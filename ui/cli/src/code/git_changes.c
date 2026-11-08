// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_changes.c: the changes as the window reads them, and the next commit. Each read lists the tree's changed files
// through the repositories' status into the list read from git, whose texts stand in the persistent tier until the
// next read, and every row points into it, and reads how far the branch stands from the remote it follows. The next
// commit takes every file but those left out: whether each file of the list is left out stands as a byte a file in a
// cell sized to the list, and the paths left out stand as one text of their own, so the choice outlasts the list's
// next read. A file's changes open under its row: the document store's changes of its text, the open view's where one
// is open on it and else the file stored for the changes alone, against the file as the last commit has it, which the
// store has beside the text; the changes stand in one cell a file with a byte a line of them, 1 where the line is
// left out of the next commit, which then takes the file with the text of its lines taken. The message typed stands
// in a cell sized to it, and git's refusal of the last commit, pull, push or roll back in one sized to the refusal. The
// watch marks the changes stale from its thread, and the step reads them again
#include "git.h"

#include "../command_line/commands.h"
#include "../editor/document.h"
#include "../editor/files.h"
#include "../editor/view.h"
#include "../text/typed.h"
#include "embed_atomic.h"

// why nothing was committed, the bytes git's own words of a commit, a pull or a push may take, the bytes a file's text
// as the last commit has it may take, the key a file is stored under for its changes alone, and no place
#define CHANGES_EMPTY "The commit message is empty."
#define CHANGES_NONE "No file is taken."
#define CHANGES_OUT 4096ULL
#define CHANGES_TEXT_MAX (1ULL << 20u)
#define CHANGES_KEY "changes:"
#define CHANGES_NONE_AT 0xffffffffu

// the list, standing between the window's frames until the next read
static CliChangedList s_changed;

// A file whose changes are open under its row, in one cell: its path with its end, the key its text is stored under in
// the document store, its changes as the diff hands them, four numbers each, the newer text's first line and the line
// after its last then the older's, the row under the file's row each change starts at, one more than the changes, so
// that a change's first line's byte is its row less its place, how many lines of each change are left out, and a byte a
// line of the changes in the changes' order, each change's lines taken out then its lines put in, 1 where the line is
// left out. Beside the cell: how many changes, how many line bytes, how many are 1, how many rows stand under the
// file's row, whether the text was stored for the changes alone and goes as they close, and the file's place in the
// list
typedef struct
{
    unsigned char *cell;
    const char *path;
    const char *key;
    unsigned int *hunk;
    unsigned int *hunk_row;
    unsigned int *hunk_out;
    unsigned char *out;
    unsigned int hunks;
    unsigned int outs;
    unsigned int out_count;
    unsigned int rows;
    int own;
    unsigned int file;
} ChangesPart;

// the changes of a text counted as the diff hands them: how many, and how many lines they take out and put in
typedef struct
{
    unsigned int hunks;
    unsigned int keys;
} ChangesCount;

// The next commit: its message, the paths left out as one text each with its end and the bytes of that text, a byte a
// file of the list, 1 where the file is left out, how many files are taken, the files whose changes are open and how
// many, the row each file's row stands at, one more than the files, each file's open changes' place, CHANGES_NONE_AT
// for none, git's refusal of the last commit, pull, push or roll back with its bytes, whether the list was read since
// the tree was opened, how far the branch stands from the remote it follows, 1 where it follows one, and the
// millisecond a roll back was armed at, 0 for not armed
typedef struct
{
    CliTyped message;
    char *left;
    unsigned long long left_bytes;
    unsigned char *out;
    unsigned int taken;
    ChangesPart *part;
    unsigned int parts;
    unsigned int *first_row;
    unsigned int *file_part;
    char *refusal;
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
} ChangesCtx;

// whether `path` is among the paths left out
EMBED_INLINE int left_has(const ChangesCtx *args)
{
    for (unsigned long long at = 0ULL; at < s_changes.left_bytes;)
    {
        const char *const one = s_changes.left + at;
        if (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
        {
            return 1;
        }
        at += cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    return 0;
}

// the path of the file `file` of the list
EMBED_INLINE const char *file_path(const ChangesCtx *args)
{
    return EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_changed.text,
                      .at = s_changed.item[args->file].path);
}

// the choice settled after the bytes a file changed: the files taken counted, and the paths left out written again
// from the files marked out, in a cell sized to them, none where none is
EMBED_INLINE void choice_settled(const ChangesCtx *args)
{
    (void)args;
    unsigned long long bytes = 0ULL;
    unsigned int taken = 0u;
    for (unsigned int at = 0u; (s_changes.out != NULL) && (at < s_changed.count); at += 1u)
    {
        const char *const path = EMBED_CALL(file_path, ChangesCtx, .file = at);
        bytes += s_changes.out[at] ? cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL : 0ULL;
        taken += s_changes.out[at] ? 0u : 1u;
    }
    s_changes.taken = taken;
    char *const left = (bytes != 0ULL) ? g_cli_guard->persistent_buf_alloc(bytes, g_cli_git_owner) : NULL;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; (left != NULL) && (at < s_changed.count); at += 1u)
    {
        if (!s_changes.out[at])
        {
            continue;
        }
        const char *const path = EMBED_CALL(file_path, ChangesCtx, .file = at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = left + used, .src = path, .bytes = length + 1ULL});
        used += length + 1ULL;
    }
    g_cli_guard->persistent_buf_release(s_changes.left, g_cli_git_owner);
    s_changes.left = left;
    s_changes.left_bytes = (left != NULL) ? bytes : 0ULL;
}

// git `run` at `root`, pull or push: its words into a cell of this run alone, 1, or 0 with the refusal in a cell sized
// to why
EMBED_INLINE int remote_run(const ChangesCtx *args)
{
    g_cli_guard->persistent_buf_release(s_changes.refusal, g_cli_git_owner);
    s_changes.refusal = NULL;
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
    s_changes.refusal = (refused != 0ULL) ? g_cli_guard->persistent_buf_alloc(refused + 1ULL, g_cli_git_owner) : NULL;
    s_changes.refusal_length = (s_changes.refusal != NULL) ? refused : 0ULL;
    if (s_changes.refusal != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_changes.refusal, .src = scratch + CHANGES_OUT, .bytes = refused});
        s_changes.refusal[refused] = '\0';
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_git_owner);
    return ran;
}

// a change handed by the diff counted, with the lines it takes out and puts in
static void changes_counted(const CliEachHunkCfg *args)
{
    ChangesCount *const count = args->context;
    count->hunks += 1u;
    count->keys += (args->now_to - args->now_from) + (args->then_to - args->then_from);
}

// a change handed by the diff written into the part's changes as the next
static void changes_filled(const CliEachHunkCfg *args)
{
    ChangesPart *const part = args->context;
    unsigned int *const hunk = part->hunk + (4u * part->hunks);
    hunk[0] = args->now_from;
    hunk[1] = args->now_to;
    hunk[2] = args->then_from;
    hunk[3] = args->then_to;
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

// the part's cell laid for `path`, `key_text` and the changes counted in `count`, the changes written in from the store
// and every line taken; the cell it had released: 1, or 0 where the cells cannot meet it, the part as it was
EMBED_INLINE int part_laid(const ChangesCtx *args)
{
    ChangesPart *const part = args->part;
    const ChangesCount *const count = args->count;
    const unsigned long long path_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    const unsigned long long key_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = args->key_text, .cap = CLI_PATH_MAX_LEN + sizeof(CHANGES_KEY)}) + 1ULL;
    const unsigned long long texts = (path_bytes + key_bytes + 7ULL) & ~7ULL;
    const unsigned long long numbers =
        ((4ULL * count->hunks) + (count->hunks + 1ULL) + count->hunks) * sizeof(unsigned int);
    unsigned char *const cell = g_cli_guard->persistent_buf_alloc(texts + numbers + count->keys, g_cli_git_owner);
    if (cell == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->path, .bytes = path_bytes});
    memor.cpy(&(MemoriaCfg){.dst = cell + path_bytes, .src = args->key_text, .bytes = key_bytes});
    g_cli_guard->persistent_buf_release(part->cell, g_cli_git_owner);
    part->cell = cell;
    part->path = (const char *)cell;
    part->key = (const char *)(cell + path_bytes);
    part->hunk = (unsigned int *)(void *)(cell + texts);
    part->hunk_row = part->hunk + (4u * count->hunks);
    part->hunk_out = part->hunk_row + count->hunks + 1u;
    part->out = cell + texts + numbers;
    part->hunks = 0u;
    (void)EMBED_CALL(cli_document_out_key_head_input.changes, CliDocumentOutKeyHeadInputCfg, .key = part->key,
                     .each = changes_filled, .context = part);
    part->hunk_row[0] = 0u;
    for (unsigned int at = 0u; at < part->hunks; at += 1u)
    {
        const unsigned int *const hunk = part->hunk + (4u * at);
        part->hunk_row[at + 1u] = part->hunk_row[at] + 1u + (hunk[1] - hunk[0]) + (hunk[3] - hunk[2]);
        part->hunk_out[at] = 0u;
    }
    if (count->keys != 0u)
    {
        memor.set(&(MemoriaCfg){.dst = part->out, .val = 0, .bytes = count->keys});
    }
    part->outs = count->keys;
    part->out_count = 0u;
    part->rows = part->hunk_row[part->hunks];
    return 1;
}

// the part `at` taken out of the parts: the text stored for its changes alone let go, its cell released, the ones
// after it moved up, and the parts in a cell sized to what is left
EMBED_INLINE void part_closed(const ChangesCtx *args)
{
    ChangesPart *const part = &s_changes.part[args->at];
    if (part->own)
    {
        EMBED_CALL(cli_document.release, CliDocumentCfg, .key = part->key);
    }
    g_cli_guard->persistent_buf_release(part->cell, g_cli_git_owner);
    for (unsigned int from = args->at + 1u; from < s_changes.parts; from += 1u)
    {
        s_changes.part[from - 1u] = s_changes.part[from];
    }
    const unsigned int left = s_changes.parts - 1u;
    ChangesPart *const fewer =
        (left != 0u) ? g_cli_guard->persistent_buf_alloc(left * sizeof(ChangesPart), g_cli_git_owner) : NULL;
    if (fewer != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = fewer, .src = s_changes.part, .bytes = left * sizeof(ChangesPart)});
    }
    if ((left == 0u) || (fewer != NULL))
    {
        g_cli_guard->persistent_buf_release(s_changes.part, g_cli_git_owner);
        s_changes.part = fewer;
    }
    s_changes.parts = left;
}

// the row each file's row stands at and each file's part settled after the list or the parts changed, in cells sized to
// the list
EMBED_INLINE void rows_settled(const ChangesCtx *args)
{
    (void)args;
    g_cli_guard->persistent_buf_release(s_changes.first_row, g_cli_git_owner);
    g_cli_guard->persistent_buf_release(s_changes.file_part, g_cli_git_owner);
    s_changes.first_row =
        g_cli_guard->persistent_buf_alloc((s_changed.count + 1ULL) * sizeof(unsigned int), g_cli_git_owner);
    s_changes.file_part =
        (s_changed.count != 0u)
            ? g_cli_guard->persistent_buf_alloc(s_changed.count * sizeof(unsigned int), g_cli_git_owner)
            : NULL;
    if ((s_changes.first_row == NULL) || ((s_changed.count != 0u) && (s_changes.file_part == NULL)))
    {
        g_cli_guard->persistent_buf_release(s_changes.first_row, g_cli_git_owner);
        g_cli_guard->persistent_buf_release(s_changes.file_part, g_cli_git_owner);
        s_changes.first_row = NULL;
        s_changes.file_part = NULL;
        return;
    }
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        s_changes.file_part[at] = CHANGES_NONE_AT;
    }
    for (unsigned int at = 0u; at < s_changes.parts; at += 1u)
    {
        s_changes.file_part[s_changes.part[at].file] = at;
    }
    s_changes.first_row[0] = 0u;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        const unsigned int part = s_changes.file_part[at];
        s_changes.first_row[at + 1u] =
            s_changes.first_row[at] + 1u + ((part != CHANGES_NONE_AT) ? s_changes.part[part].rows : 0u);
    }
}

// every open part matched to the list read anew by its path and its changes found again against the last commit's
// file read anew, its lines' choices standing where the changes are the same; closed where its file no longer differs
// or its changes cannot be found
EMBED_INLINE void parts_matched(const ChangesCtx *args)
{
    for (unsigned int at = 0u; at < s_changes.parts;)
    {
        ChangesPart *const part = &s_changes.part[at];
        unsigned int file = CHANGES_NONE_AT;
        for (unsigned int each = 0u; (file == CHANGES_NONE_AT) && (each < s_changed.count); each += 1u)
        {
            file = cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(file_path, ChangesCtx, .file = each),
                                                .other = part->path,
                                                .cap = CLI_PATH_MAX_LEN})
                       ? each
                       : CHANGES_NONE_AT;
        }
        ChangesCount count;
        if ((file == CHANGES_NONE_AT) ||
            !EMBED_CALL(head_stored, ChangesCtx, .path = part->path, .key_text = part->key, .root = args->root,
                        .state = s_changed.item[file].state, .count = &count))
        {
            EMBED_CALL(part_closed, ChangesCtx, .at = at);
            continue;
        }
        part->file = file;
        // the changes as they were, for the choices to stand where they are the same
        const unsigned long long was_bytes = 4ULL * part->hunks * sizeof(unsigned int) + part->outs;
        unsigned char *const was = g_cli_guard->temporary_buf_alloc(was_bytes + 1ULL, g_cli_git_owner);
        const unsigned int was_hunks = part->hunks;
        const unsigned int was_outs = part->outs;
        const unsigned int was_out_count = part->out_count;
        if (was != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = was, .src = part->hunk, .bytes = 4ULL * was_hunks * sizeof(unsigned int)});
            memor.cpy(&(MemoriaCfg){
                .dst = was + (4ULL * was_hunks * sizeof(unsigned int)), .src = part->out, .bytes = was_outs});
        }
        ChangesPart laid = *part;
        laid.cell = NULL;
        const int relaid = EMBED_CALL(part_laid, ChangesCtx, .part = &laid, .path = part->path, .key_text = part->key,
                                      .count = &count);
        if (relaid)
        {
            const int same =
                (was != NULL) && (was_hunks == laid.hunks) && (was_outs == laid.outs) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = was, .other = laid.hunk, .bytes = 4ULL * was_hunks * sizeof(unsigned int)}) == 0);
            if (same && (was_outs != 0u))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = laid.out, .src = was + (4ULL * was_hunks * sizeof(unsigned int)), .bytes = was_outs});
                laid.out_count = was_out_count;
                for (unsigned int hunk = 0u; hunk < laid.hunks; hunk += 1u)
                {
                    const unsigned int first = laid.hunk_row[hunk] - hunk;
                    const unsigned int last = laid.hunk_row[hunk + 1u] - hunk - 1u;
                    for (unsigned int key = first; key < last; key += 1u)
                    {
                        laid.hunk_out[hunk] += laid.out[key];
                    }
                }
            }
            g_cli_guard->persistent_buf_release(part->cell, g_cli_git_owner);
            *part = laid;
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

// the part of the file `file`, NULL where its changes are not open
EMBED_INLINE ChangesPart *part_of(const ChangesCtx *args)
{
    const unsigned int part = (s_changes.file_part != NULL) ? s_changes.file_part[args->file] : CHANGES_NONE_AT;
    return (part != CHANGES_NONE_AT) ? &s_changes.part[part] : NULL;
}

// the last of `at` starts in `part->hunk_row` that is at most `key`: the file or the change a row stands under
EMBED_INLINE unsigned int place_under(const ChangesCtx *args)
{
    const unsigned int *const first = args->part->hunk_row;
    unsigned int low = 0u;
    unsigned int high = args->at;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (first[middle] <= args->key)
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

// the byte of the line `key` of the part set to `on`, 1 for left out, the counts following
EMBED_INLINE void key_set(const ChangesCtx *args)
{
    ChangesPart *const part = args->part;
    const unsigned char was = part->out[args->key];
    const unsigned char now = args->on ? 1u : 0u;
    if (was == now)
    {
        return;
    }
    part->out[args->key] = now;
    part->out_count += now ? 1u : 0u;
    part->out_count -= was ? 1u : 0u;
    part->hunk_out[args->hunk] += now ? 1u : 0u;
    part->hunk_out[args->hunk] -= was ? 1u : 0u;
}

// after a part's lines changed: the file taken again where a line of it is, left out where its every line is, its
// lines' choices cleared then
EMBED_INLINE void part_followed(const ChangesCtx *args)
{
    ChangesPart *const part = args->part;
    if (s_changes.out == NULL)
    {
        return;
    }
    if ((part->outs != 0u) && (part->out_count == part->outs))
    {
        s_changes.out[part->file] = 1u;
        memor.set(&(MemoriaCfg){.dst = part->out, .val = 0, .bytes = part->outs});
        part->out_count = 0u;
        for (unsigned int at = 0u; at < part->hunks; at += 1u)
        {
            part->hunk_out[at] = 0u;
        }
    }
    else if (args->on)
    {
        s_changes.out[part->file] = 0u;
    }
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
}

// whether the part has lines left out and lines taken, so that its file is committed in part
EMBED_INLINE int part_partial(const ChangesCtx *args)
{
    return (args->part != NULL) && (args->part->out_count != 0u) && (args->part->out_count < args->part->outs);
}

// a line of the part's texts put after the lines written so far, the older text's line `at` where `on`, else the
// newer's, as UTF-8 after a line end where a line stands before it; into `written` as a line of `size` bytes where it
// is not NULL: the units the line takes, for the bytes it may take
EMBED_INLINE unsigned long long part_line_written(const ChangesCtx *args)
{
    unsigned long long count = 0ULL;
    const unsigned short *const units = args->on
                                            ? EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg,
                                                         .key = args->part->key, .line = args->at, .count = &count)
                                            : EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg,
                                                         .key = args->part->key, .line = args->at, .count = &count);
    if (args->written != NULL)
    {
        VerbaLinea *const line = (VerbaLinea *)(void *)args->written;
        mmgr_line_put(line, (args->key != 0u) ? (args->state ? "\r\n" : "\n") : "");
        EMBED_CALL(cli_document_units_count.units_utf8, CliDocumentUnitsCountCfg, .out = line, .units = units,
                   .count = count);
    }
    return count;
}

// the text the commit takes for the part: the older text's lines, each line taken out that is left out standing, each
// line put in that is taken standing, joined by the stored text's line ends; written into the line `written` where it
// is not NULL: the units its lines take, which bound its bytes at three a unit with two a line end
EMBED_INLINE unsigned long long part_text_written(const ChangesCtx *args)
{
    ChangesPart *const part = args->part;
    const int crlf = EMBED_CALL(cli_document.crlf, CliDocumentCfg, .key = part->key);
    unsigned long long units = 0ULL;
    unsigned int lines = 0u;
    unsigned int at = 0u;
    for (unsigned int hunk = 0u; hunk <= part->hunks; hunk += 1u)
    {
        const unsigned int *const one = (hunk < part->hunks) ? part->hunk + (4u * hunk) : NULL;
        unsigned long long count = 0ULL;
        // the older text's lines before the change, or after the last change to its end
        while (((one != NULL) && (at < one[2])) ||
               ((one == NULL) && (EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg,
                                             .key = part->key, .line = at, .count = &count) != NULL)))
        {
            units += EMBED_CALL(part_line_written, ChangesCtx, .part = part, .on = 1, .at = at, .key = lines,
                                .state = (char)crlf, .written = args->written);
            lines += 1u;
            at += 1u;
        }
        if (one == NULL)
        {
            break;
        }
        const unsigned int key_first = part->hunk_row[hunk] - hunk;
        const unsigned int outs = one[3] - one[2];
        for (unsigned int line = 0u; line < outs + (one[1] - one[0]); line += 1u)
        {
            const int gone = line < outs;
            const int taken = gone ? part->out[key_first + line] : !part->out[key_first + line];
            if (!taken)
            {
                continue;
            }
            units += EMBED_CALL(part_line_written, ChangesCtx, .part = part, .on = gone,
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
    g_cli_guard->persistent_buf_release(s_changed.text.bytes, g_cli_git_owner);
    s_changed.text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
    s_changed.count = 0u;
    g_cli_guard->persistent_buf_release(s_changes.out, g_cli_git_owner);
    s_changes.out = NULL;
    s_changes.taken = 0u;
    s_changes.listed = args->root != NULL;
    if (args->root == NULL)
    {
        g_cli_guard->persistent_buf_release(s_changes.left, g_cli_git_owner);
        s_changes.left = NULL;
        s_changes.left_bytes = 0ULL;
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
    s_changes.out =
        (s_changed.count != 0u) ? g_cli_guard->persistent_buf_alloc(s_changed.count, g_cli_git_owner) : NULL;
    for (unsigned int at = 0u; (s_changes.out != NULL) && (at < s_changed.count); at += 1u)
    {
        // whether a file is left out is 0 or 1, which a byte carries
        s_changes.out[at] =
            (unsigned char)EMBED_CALL(left_has, ChangesCtx, .path = EMBED_CALL(file_path, ChangesCtx, .file = at));
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
    return (s_changes.first_row != NULL) ? s_changes.first_row[s_changed.count] : s_changed.count;
}

unsigned int cli_changes_files(const CliGitChangesCfg *args)
{
    (void)args;
    return s_changed.count;
}

int cli_changes_row(const CliGitChangesCfg *args)
{
    if ((s_changes.first_row == NULL) || (args->at >= s_changes.first_row[s_changed.count]))
    {
        return 0;
    }
    // the file whose rows the row stands among, the last whose first row is at most it
    ChangesPart files = {.hunk_row = s_changes.first_row};
    const unsigned int file =
        EMBED_CALL(place_under, ChangesCtx, .part = &files, .at = s_changed.count, .key = args->at);
    const CliChanged *const changed = &s_changed.item[file];
    const char *const path = EMBED_CALL(file_path, ChangesCtx, .file = file);
    ChangesPart *const part = EMBED_CALL(part_of, ChangesCtx, .file = file);
    const unsigned int inner = args->at - s_changes.first_row[file];
    CliChangedRow *const row = args->row;
    *row = (CliChangedRow){0};
    row->path = path;
    row->file = file;
    row->state = changed->state;
    if (inner == 0u)
    {
        row->type = CLI_CHANGES_ROW_FILE;
        row->name = path + changed->name;
        row->folder_length = (changed->name > 0u) ? changed->name - 1u : 0u;
        row->taken = (s_changes.out == NULL) || !s_changes.out[file];
        row->part = EMBED_CALL(part_partial, ChangesCtx, .part = part);
        row->opens = (changed->state == 'M') || (changed->state == 'A') || (changed->state == 'U');
        row->open = part != NULL;
        return 1;
    }
    const unsigned int under = inner - 1u;
    const unsigned int hunk = EMBED_CALL(place_under, ChangesCtx, .part = part, .at = part->hunks, .key = under);
    const unsigned int *const one = part->hunk + (4u * hunk);
    const unsigned int offset = under - part->hunk_row[hunk];
    const unsigned int outs = one[3] - one[2];
    const unsigned int lines = outs + (one[1] - one[0]);
    row->hunk = hunk;
    if (offset == 0u)
    {
        row->type = CLI_CHANGES_ROW_HUNK;
        row->from = one[0];
        row->to = one[1];
        row->taken = !s_changes.out[file] && (part->hunk_out[hunk] < lines);
        row->part = (part->hunk_out[hunk] != 0u) && (part->hunk_out[hunk] < lines);
        return 1;
    }
    const unsigned int line = offset - 1u;
    row->type = CLI_CHANGES_ROW_LINE;
    row->from = one[0];
    row->to = one[1];
    row->gone = line < outs;
    row->units = row->gone ? EMBED_CALL(cli_document_line.head_line_units, CliDocumentLineUnitsCfg, .key = part->key,
                                        .line = one[2] + line, .count = &row->count)
                           : EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = part->key,
                                        .line = one[0] + (line - outs), .count = &row->count);
    row->taken = !s_changes.out[file] && !part->out[(part->hunk_row[hunk] - hunk) + line];
    return 1;
}

void cli_changes_toggle(const CliGitChangesCfg *args)
{
    CliChangedRow row;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &row) || (s_changes.out == NULL))
    {
        return;
    }
    ChangesPart *const part = EMBED_CALL(part_of, ChangesCtx, .file = row.file);
    if (row.type == CLI_CHANGES_ROW_FILE)
    {
        s_changes.out[row.file] ^= 1u;
        // the file taken whole or left out whole: its lines' choices go
        if ((part != NULL) && (part->outs != 0u))
        {
            memor.set(&(MemoriaCfg){.dst = part->out, .val = 0, .bytes = part->outs});
            part->out_count = 0u;
            for (unsigned int at = 0u; at < part->hunks; at += 1u)
            {
                part->hunk_out[at] = 0u;
            }
        }
        EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
        return;
    }
    const unsigned int *const one = part->hunk + (4u * row.hunk);
    const unsigned int key_first = part->hunk_row[row.hunk] - row.hunk;
    const unsigned int lines = (one[1] - one[0]) + (one[3] - one[2]);
    if (row.type == CLI_CHANGES_ROW_HUNK)
    {
        // every line taken where some is left out or the file is, else every line left out
        const int on = s_changes.out[row.file] || (part->hunk_out[row.hunk] != 0u);
        for (unsigned int line = 0u; line < lines; line += 1u)
        {
            EMBED_CALL(key_set, ChangesCtx, .part = part, .hunk = row.hunk, .key = key_first + line, .on = !on);
        }
        EMBED_CALL(part_followed, ChangesCtx, .part = part, .on = on);
        return;
    }
    const unsigned int under = args->at - s_changes.first_row[row.file] - 1u;
    const unsigned int key = key_first + (under - part->hunk_row[row.hunk] - 1u);
    const int on = s_changes.out[row.file] || part->out[key];
    EMBED_CALL(key_set, ChangesCtx, .part = part, .hunk = row.hunk, .key = key, .on = !on);
    EMBED_CALL(part_followed, ChangesCtx, .part = part, .on = on);
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
        EMBED_CALL(part_closed, ChangesCtx, .at = s_changes.file_part[row.file]);
        EMBED_CALL(rows_settled, ChangesCtx, .size = 0ULL);
        return 1;
    }
    // the text's key: the view's where one is open on the file, else the file stored for the changes alone
    const char *key = NULL;
    const unsigned int views = cli_view_count();
    for (unsigned int at = 0u; (key == NULL) && (at < views); at += 1u)
    {
        const char *const open = cli_view_path_at(at);
        key = ((open != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = open, .other = row.path, .cap = CLI_PATH_MAX_LEN}))
                  ? cli_view_key(at)
                  : NULL;
    }
    char keyed[CLI_PATH_MAX_LEN + sizeof(CHANGES_KEY)];
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
    // the parts grown by one for the file's
    ChangesCount count;
    ChangesPart *const grown =
        g_cli_guard->persistent_buf_alloc((s_changes.parts + 1ULL) * sizeof(ChangesPart), g_cli_git_owner);
    if ((grown == NULL) || !EMBED_CALL(head_stored, ChangesCtx, .path = row.path, .key_text = key, .root = args->root,
                                       .state = row.state, .count = &count))
    {
        g_cli_guard->persistent_buf_release(grown, g_cli_git_owner);
        if (own)
        {
            EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
        }
        return 0;
    }
    if (s_changes.parts != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_changes.part, .bytes = s_changes.parts * sizeof(ChangesPart)});
    }
    g_cli_guard->persistent_buf_release(s_changes.part, g_cli_git_owner);
    s_changes.part = grown;
    s_changes.parts += 1u;
    ChangesPart *const part = &grown[s_changes.parts - 1u];
    *part = (ChangesPart){0};
    part->file = row.file;
    part->own = own;
    if (!EMBED_CALL(part_laid, ChangesCtx, .part = part, .path = row.path, .key_text = key, .count = &count))
    {
        if (own)
        {
            EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
        }
        s_changes.parts -= 1u;
        EMBED_CALL(rows_settled, ChangesCtx, .size = 0ULL);
        return 0;
    }
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
    if (s_changes.out == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = s_changes.out, .val = args->on ? 0 : 1, .bytes = s_changed.count});
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
}

int cli_changes_menu(const CliGitChangesCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &read))
    {
        return 0;
    }
    // the rows run with the file's path, which stands in the list while the menu is open
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
               .most = CLI_CHANGES_MESSAGE_MAX, .owner = g_cli_git_owner);
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

const char *cli_changes_message(const CliGitChangesCfg *args)
{
    *args->length_out = s_changes.message.length;
    return (s_changes.message.bytes != NULL) ? s_changes.message.bytes : "";
}

int cli_changes_commit(const CliGitChangesCfg *args)
{
    g_cli_guard->persistent_buf_release(s_changes.refusal, g_cli_git_owner);
    s_changes.refusal = NULL;
    s_changes.refusal_length = 0ULL;
    const unsigned int taken = s_changes.taken;
    if ((s_changes.message.length == 0ULL) || (taken == 0u))
    {
        // why nothing is to commit, in a cell sized to the sentence with its end
        const char *const why = (s_changes.message.length == 0ULL) ? CHANGES_EMPTY : CHANGES_NONE;
        const unsigned long long size =
            (s_changes.message.length == 0ULL) ? sizeof(CHANGES_EMPTY) : sizeof(CHANGES_NONE);
        s_changes.refusal = g_cli_guard->persistent_buf_alloc(size, g_cli_git_owner);
        s_changes.refusal_length = (s_changes.refusal != NULL) ? size - 1ULL : 0ULL;
        if (s_changes.refusal != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_changes.refusal, .src = why, .bytes = size});
        }
        return 0;
    }
    // the files taken whole and the files taken in part counted, with the bytes their texts may take
    unsigned int whole = 0u;
    unsigned int partial = 0u;
    unsigned long long texts = 0ULL;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if ((s_changes.out != NULL) && s_changes.out[at])
        {
            continue;
        }
        ChangesPart *const part = EMBED_CALL(part_of, ChangesCtx, .file = at);
        const int in_part = EMBED_CALL(part_partial, ChangesCtx, .part = part);
        partial += in_part ? 1u : 0u;
        whole += in_part ? 0u : 1u;
        texts += in_part ? EMBED_CALL(part_text_written, ChangesCtx, .part = part) : 0ULL;
    }
    // the paths taken whole, the parts with their texts, git's own words and the error line, for this commit alone
    const unsigned long long paths_bytes = (unsigned long long)whole * sizeof(const char *);
    const unsigned long long parts_bytes = (unsigned long long)partial * sizeof(CliGitPart);
    char *const scratch = g_cli_guard->temporary_buf_alloc(
        paths_bytes + parts_bytes + texts + CHANGES_OUT + CLI_ERR_MSG_MAX_LEN, g_cli_git_owner);
    if (scratch == NULL)
    {
        return 0;
    }
    const char **const paths = (const char **)(void *)scratch;
    CliGitPart *const parts = (CliGitPart *)(void *)(scratch + paths_bytes);
    char *text_at = scratch + paths_bytes + parts_bytes;
    VerbaLinea out = {scratch + paths_bytes + parts_bytes + texts, CHANGES_OUT, 0ULL};
    VerbaLinea err_msg = {scratch + paths_bytes + parts_bytes + texts + CHANGES_OUT, CLI_ERR_MSG_MAX_LEN, 0ULL};
    unsigned int put = 0u;
    unsigned int put_parts = 0u;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if ((s_changes.out != NULL) && s_changes.out[at])
        {
            continue;
        }
        ChangesPart *const part = EMBED_CALL(part_of, ChangesCtx, .file = at);
        if (EMBED_CALL(part_partial, ChangesCtx, .part = part))
        {
            VerbaLinea line = {text_at, EMBED_CALL(part_text_written, ChangesCtx, .part = part), 0ULL};
            (void)EMBED_CALL(part_text_written, ChangesCtx, .part = part, .written = (char *)(void *)&line);
            parts[put_parts] = (CliGitPart){part->path, text_at, line.at};
            put_parts += 1u;
            text_at += line.capacity;
        }
        else
        {
            paths[put] = EMBED_CALL(file_path, ChangesCtx, .file = at);
            put += 1u;
        }
    }
    const int committed =
        (partial != 0u)
            ? EMBED_CALL(cli_git_root.commit_parts, CliGitRootCfg, .root = args->root, .out = &out,
                         .message = s_changes.message.bytes, .err_msg = &err_msg, .whole = paths,
                         .full_files_count = whole, .parts = parts, .part_count = partial, .push = args->push)
            : EMBED_CALL(cli_git_root_out_count.commit, CliGitRootOutCountCfg, .root = args->root, .out = &out,
                         .count = taken, .message = s_changes.message.bytes, .paths = paths, .err_msg = &err_msg,
                         .push = args->push);
    if (committed)
    {
        g_cli_guard->persistent_buf_release(s_changes.message.bytes, g_cli_git_owner);
        s_changes.message = (CliTyped){NULL, 0ULL};
        EMBED_CALL(parts_released, ChangesCtx, .size = 0ULL);
    }
    // git's refusal in a cell sized to it with its end
    const unsigned long long refused = (!committed && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
    s_changes.refusal = (refused != 0ULL) ? g_cli_guard->persistent_buf_alloc(refused + 1ULL, g_cli_git_owner) : NULL;
    s_changes.refusal_length = (s_changes.refusal != NULL) ? refused : 0ULL;
    if (s_changes.refusal != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_changes.refusal, .src = err_msg.out, .bytes = refused});
        s_changes.refusal[refused] = '\0';
    }
    g_cli_guard->persistent_buf_release(scratch, g_cli_git_owner);
    if (committed)
    {
        EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    }
    return committed;
}

const char *cli_changes_refusal(const CliGitChangesCfg *args)
{
    *args->length_out = s_changes.refusal_length;
    return (s_changes.refusal != NULL) ? s_changes.refusal : "";
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
    g_cli_guard->persistent_buf_release(s_changes.refusal, g_cli_git_owner);
    s_changes.refusal = NULL;
    s_changes.refusal_length = 0ULL;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if ((s_changes.out != NULL) && s_changes.out[at])
        {
            continue;
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const int rolled = EMBED_CALL(cli_git_root_err_msg_path.rollback, CliGitRootErrMsgPathCfg, .root = args->root,
                                      .path = EMBED_CALL(file_path, ChangesCtx, .file = at), .err_msg = &err_msg);
        const unsigned long long refused = (!rolled && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
        if (refused == 0ULL)
        {
            continue;
        }
        // the file's refusal put after the refusals before it on a line of its own, in a cell sized to them all
        const unsigned long long before = s_changes.refusal_length;
        const unsigned long long joined = before + ((before != 0ULL) ? 1ULL : 0ULL);
        char *const grown = g_cli_guard->persistent_buf_alloc(joined + refused + 1ULL, g_cli_git_owner);
        if (grown == NULL)
        {
            continue;
        }
        if (before != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_changes.refusal, .bytes = before});
            grown[before] = '\n';
        }
        memor.cpy(&(MemoriaCfg){.dst = grown + joined, .src = why, .bytes = refused});
        grown[joined + refused] = '\0';
        g_cli_guard->persistent_buf_release(s_changes.refusal, g_cli_git_owner);
        s_changes.refusal = grown;
        s_changes.refusal_length = joined + refused;
    }
    return 1;
}
