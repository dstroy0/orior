// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose.c: what a file declares it is for, and the files of the tree that use it, read on the cores the work may take
#include "purpose.h"
#include "../command_line/cells.h"
#include "../editor/files_internal.h"
#include "../machine/cores.h"
#include "../text/json_tree.h"
#include "../text/text.h"

#define CLI_PURPOSE_TEXT ((CLI_PURPOSE_READ * 3u) + 1u)
// One reader of the tree's files: the files it reads, from `from` to `to`, and the room a file's text is read into
typedef struct
{
    unsigned int from;
    unsigned int to;
    char text[CLI_PURPOSE_TEXT];
    CliThread thread;
} CliPurposePart;
// the most threads the tree's files are read on, and the bytes a file's text takes once read as UTF-8
#define CLI_PURPOSE_THREADS 16u

typedef struct
{
    unsigned int letter;
    const char *within;
    unsigned long long size;
    unsigned long long at;
    unsigned long long length;
    const char *name;
    const unsigned char *bytes;
    char *text;
    CliPurposePart *part;
    VerbaLinea *out;
    const char *root;
    const char *path;
    VerbaLinea *err_msg;
} PurposeCtx;
// whether a letter joins a word: a letter, a number or `_`
EMBED_INLINE int cli_joins(const PurposeCtx *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_files_letter_internal.hit_wordy, CliFilesLetterCtx, .letter = letter);
}

// whether the span of `length` bytes at `at` of `size` bytes of `within` is a whole word: no letter, number, `_` or
// `-` before it, and no letter, number or `_` after it
EMBED_INLINE int cli_word(const PurposeCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long length = args->length;
    if (at > 0ull)
    {
        unsigned long long start = at - 1ull;
        while ((start > 0ull) && ((((unsigned char)args->within[start]) & 0xc0u) == 0x80u))
        {
            start -= 1ull;
        }
        unsigned int before = 0u;
        EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                   .text_bytes = (const unsigned char *)args->within, .length = at, .at = start, .letter = &before);
        if (EMBED_CALL(cli_joins, PurposeCtx, .letter = before) || (before == '-'))
        {
            return 0;
        }
    }
    if ((at + length) < args->size)
    {
        unsigned int after = 0u;
        EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                   .text_bytes = (const unsigned char *)args->within, .length = args->size, .at = at + length,
                   .letter = &after);
        if (EMBED_CALL(cli_joins, PurposeCtx, .letter = after))
        {
            return 0;
        }
    }
    return 1;
}

// whether `name` is a whole word in `size` bytes of `within`, its matches read without overlapping
EMBED_INLINE int cli_name_in(const PurposeCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    if (length == 0ull)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; (at + length) <= args->size;)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->within + at, .other = args->name, .bytes = length}) != 0)
        {
            at += 1ull;
            continue;
        }
        if (EMBED_CALL(cli_word, PurposeCtx, .within = args->within, .size = args->size, .at = at, .length = length))
        {
            return 1;
        }
        at += length;
    }
    return 0;
}

int cli_purpose_uses(const CliPurposeTextCfg *args)
{
    static const char *const s_imports[12] = {"import ", "from ",    "use ",    "mod ",      "pub use ", "pub mod ",
                                              "require", "#include", "\\input", "\\include", "source ",  "load("};
    if (EMBED_CALL(cli_name_in, PurposeCtx, .within = args->text, .size = args->size, .name = args->name))
    {
        return 1;
    }
    if (cellul.len(&(CatenaFinitaCfg){.src = args->stem, .cap = CLI_CELLS_POOL}) < 2ull)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < args->size;)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->text, .at = at, .size = args->size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        const char *const line = args->text + at;
        at = end + 1ull;
        unsigned long long lead = 0ull;
        while (lead < length)
        {
            unsigned int letter = 0u;
            const unsigned long long step =
                EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                           .text_bytes = (const unsigned char *)line, .length = length, .at = lead, .letter = &letter);
            if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
            {
                break;
            }
            lead += step;
        }
        int imports = 0;
        for (unsigned int keyword = 0u; !imports && (keyword < 12u); keyword += 1u)
        {
            const unsigned long long size_of =
                cellul.len(&(CatenaFinitaCfg){.src = s_imports[keyword], .cap = CLI_CELLS_POOL});
            imports =
                ((length - lead) >= size_of) &&
                (memor.cmp(&(MemoriaCfg){.src = line + lead, .other = s_imports[keyword], .bytes = size_of}) == 0);
        }
        if (imports && EMBED_CALL(cli_name_in, PurposeCtx, .within = line, .size = length, .name = args->stem))
        {
            return 1;
        }
    }
    return 0;
}

// `size` bytes read as UTF-8 into `text`, sized for CLI_PURPOSE_TEXT, each byte that starts no whole letter read as
// U+FFFD, a 0 after them: their length
EMBED_INLINE unsigned long long cli_text_get(const PurposeCtx *args)
{
    char *text = args->text;
    VerbaLinea line = {text, CLI_PURPOSE_TEXT, 0ull};
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &line, .text_bytes = args->bytes,
               .length = args->size);
    mmgr_line_finish(&line);
    return line.at;
}

int cli_purpose_write(const CliPurposeCfg *args)
{
    static char s_text[(CLI_PURPOSE_HEAD * 3u) + 1u];
    static char s_comment_buf[1u << 16u];
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->path, .joined = file,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = file,
                                                  .size_pointer = &size, .owner = &g_cli_code_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    const unsigned long long head = (size < CLI_PURPOSE_HEAD) ? size : CLI_PURPOSE_HEAD;
    VerbaLinea comment = {s_comment_buf, sizeof(s_comment_buf), 0ull};
    if ((head == 0ull) || (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = head}) == NULL))
    {
        VerbaLinea text = {s_text, sizeof(s_text), 0ull};
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &text, .text_bytes = bytes,
                   .length = head);
        mmgr_line_finish(&text);
        EMBED_CALL(cli_purpose_path_text.first_comment, CliPurposePathTextCfg, .path = file, .text = s_text,
                   .size = text.at, .out = &comment);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_code_cells);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = s_comment_buf,
               .length = comment.at);
    return 1;
}

// The search for users: the tree's files, the file used, its name and stem, the files that use it, the readers, and
// how many have ended, and what the file declares about itself; a cell taken with the users' lock and given back with
// it
typedef struct
{
    char comment_buf[1u << 17u];
    const char *root;
    const char *path;
    char name[CLI_PATH_MAX_LEN];
    char stem[CLI_PATH_MAX_LEN];
    const CliFiles *files;
    unsigned char used[CLI_FILES_MOST];
    CliPurposePart part[CLI_PURPOSE_THREADS];
    unsigned int ended;
    CliWake wake;
} CliUsers;

static CliUsers *s_users;

// a reader's files read, each marked where it uses the file
EMBED_INLINE void cli_part_read(const PurposeCtx *args)
{
    for (unsigned int index = args->part->from; index < args->part->to; index += 1u)
    {
        const char *const other = EMBED_CALL(cli_files.at, CliFilesCfg, .files = s_users->files, .index = index);
        s_users->used[index] = 0u;
        if (cellul.eq(&(CatenaFinitaCfg){.src = other, .other = s_users->path, .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_users->root, .relative = other, .joined = full);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                      .size_pointer = &size, .owner = &g_cli_code_cells);
        if (bytes == NULL)
        {
            continue;
        }
        const unsigned long long read = (size < CLI_PURPOSE_READ) ? size : CLI_PURPOSE_READ;
        if ((read == 0ull) || (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = read}) == NULL))
        {
            const unsigned long long length =
                EMBED_CALL(cli_text_get, PurposeCtx, .bytes = bytes, .size = read, .text = args->part->text);
            s_users->used[index] =
                (unsigned char)EMBED_CALL(cli_purpose_text.uses, CliPurposeTextCfg, .text = args->part->text,
                                          .size = length, .name = s_users->name, .stem = s_users->stem);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_code_cells);
    }
}

static void cli_part_thread(void *context)
{
    EMBED_CALL(cli_part_read, PurposeCtx, .part = (CliPurposePart *)context);
    s_users->ended += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_users->wake);
}

// the users found written: the first CLI_PURPOSE_USERS by path, and how many in all
EMBED_INLINE void cli_users_write(const PurposeCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "\"users\":[");
    unsigned int all = 0u;
    for (unsigned int index = 0u; (s_users->files != NULL) && (index < s_users->files->count); index += 1u)
    {
        if (!s_users->used[index])
        {
            continue;
        }
        if (all < CLI_PURPOSE_USERS)
        {
            const char *const path = EMBED_CALL(cli_files.at, CliFilesCfg, .files = s_users->files, .index = index);
            mmgr_line_put(out, (all != 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = path,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        }
        all += 1u;
    }
    mmgr_line_put(out, "],\"all\":");
    mmgr_line_decimal(out, all, 0u);
}

// the files of the tree that use the file at `path` found: 1, or 0 with why given
static int cli_users_find(const PurposeCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = path, .joined = file,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const char *name = file;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = file,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (file + name_past) : name;
    memor.cpy(&(MemoriaCfg){.dst = s_users->name,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_users->stem,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = s_users->stem,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = s_users->stem, .cap = CLI_CELLS_POOL}), .byte = '.');
    if ((dot != NULL) && (dot != s_users->stem))
    {
        dot[0] = '\0';
    }
    s_users->root = root;
    s_users->path = path;
    s_users->files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    const unsigned int count = (s_users->files != NULL) ? s_users->files->count : 0u;
    const unsigned int cores = cli_cores_for_work();
    const unsigned int parts = (cores < 1u) ? 1u : ((cores > CLI_PURPOSE_THREADS) ? CLI_PURPOSE_THREADS : cores);
    const unsigned int share = ((count + parts - 1u) / parts > 0u) ? ((count + parts - 1u) / parts) : 1u;
    s_users->ended = 0u;
    unsigned int started = 0u;
    unsigned int used_parts = 0u;
    for (unsigned int from = 0u; from < count; from += share)
    {
        CliPurposePart *const part = &s_users->part[used_parts];
        part->from = from;
        part->to = ((from + share) < count) ? (from + share) : count;
        part->thread = (CliThread){cli_part_thread, part};
        used_parts += 1u;
        if (EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &part->thread))
        {
            started += 1u;
        }
        else
        {
            EMBED_CALL(cli_part_read, PurposeCtx, .part = part);
        }
    }
    while (s_users->ended < started)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_users->wake);
    }
    return 1;
}

// the users' lock taken and the search a cell under it: 1, or 0 with why given and the lock given back
EMBED_INLINE int cli_users_lock(const PurposeCtx *args)
{
    s_users = g_cli_cells->persistent_buf_alloc(sizeof(*s_users), &g_cli_code_cells);
    if (s_users == NULL)
    {
        mmgr_line_put(args->err_msg, "a search for users could not be taken");
        return 0;
    }
    const CliWake woken = CLI_WAKE_INIT;
    s_users->wake = woken;
    s_users->files = NULL;
    return 1;
}

// the search given back with the list of the tree's files it laid flat, and the users' lock with it
static void cli_users_release(void)
{
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = s_users->files);
    g_cli_cells->persistent_buf_release(s_users, &g_cli_code_cells);
    s_users = NULL;
}

int cli_purpose_users(const CliPurposeCfg *args)
{
    if (!EMBED_CALL(cli_users_lock, PurposeCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    const int found =
        EMBED_CALL(cli_users_find, PurposeCtx, .root = args->root, .path = args->path, .err_msg = args->err_msg);
    if (found)
    {
        mmgr_line_ch(args->out, '{');
        EMBED_CALL(cli_users_write, PurposeCtx, .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    cli_users_release();
    return found;
}

int cli_purpose(const CliPurposeCfg *args)
{
    if (!EMBED_CALL(cli_users_lock, PurposeCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    VerbaLinea comment = {s_users->comment_buf, sizeof(s_users->comment_buf), 0ull};
    const int found =
        EMBED_CALL(cli_purpose_table.write, CliPurposeCfg, .root = args->root, .path = args->path, .out = &comment,
                   .err_msg = args->err_msg) &&
        EMBED_CALL(cli_users_find, PurposeCtx, .root = args->root, .path = args->path, .err_msg = args->err_msg);
    if (found)
    {
        mmgr_line_put(args->out, "{\"comment\":");
        mmgr_line_put_n(args->out, s_users->comment_buf, comment.at);
        mmgr_line_ch(args->out, ',');
        EMBED_CALL(cli_users_write, PurposeCtx, .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    cli_users_release();
    return found;
}
