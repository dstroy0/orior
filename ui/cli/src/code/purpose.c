// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose.c: what a file says it is for, and the files of the tree that use it, read on the cores the work may take
#include "purpose.h"
#include "../command_line/cells.h"
#include "../editor/files_internal.h"
#include "../machine/cores.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the most threads the tree's files are read on, and the bytes a file's text takes once read as UTF-8
#define CLI_PURPOSE_THREADS 16u
#define CLI_PURPOSE_TEXT ((CLI_PURPOSE_READ * 3u) + 1u)

// whether a letter joins a word: a letter, a number or `_`
static int cli_joins(unsigned int letter)
{
    return cli_hit_wordy(letter);
}

// whether the span of `length` bytes at `at` of `size` bytes of `within` stands as a word: no letter, number, `_` or
// `-` before it, and no letter, number or `_` after it
static int cli_word(const char *within, unsigned long long size, unsigned long long at, unsigned long long length)
{
    if (at > 0ull)
    {
        unsigned long long start = at - 1ull;
        while ((start > 0ull) && ((((unsigned char)within[start]) & 0xc0u) == 0x80u))
        {
            start -= 1ull;
        }
        unsigned int before = 0u;
        cli_lossy_letter((const unsigned char *)within, at, start, &before);
        if (cli_joins(before) || (before == '-'))
        {
            return 0;
        }
    }
    if ((at + length) < size)
    {
        unsigned int after = 0u;
        cli_lossy_letter((const unsigned char *)within, size, at + length, &after);
        if (cli_joins(after))
        {
            return 0;
        }
    }
    return 1;
}

// whether `name` stands as a word in `size` bytes of `within`, its matches read without overlapping
static int cli_named_in(const char *within, unsigned long long size, const char *name)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (length == 0ull)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; (at + length) <= size;)
    {
        if (memor.cmp(&(MemoriaCfg){.src = within + at, .other = name, .bytes = length}) != 0)
        {
            at += 1ull;
            continue;
        }
        if (cli_word(within, size, at, length))
        {
            return 1;
        }
        at += length;
    }
    return 0;
}

int cli_purpose_uses(const char *text, unsigned long long size, const char *name, const char *stem)
{
    static const char *const s_imports[12] = {"import ", "from ",    "use ",    "mod ",      "pub use ", "pub mod ",
                                              "require", "#include", "\\input", "\\include", "source ",  "load("};
    if (cli_named_in(text, size, name))
    {
        return 1;
    }
    if (cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}) < 2ull)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        const char *const line = text + at;
        at = end + 1ull;
        unsigned long long lead = 0ull;
        while (lead < length)
        {
            unsigned int letter = 0u;
            const unsigned long long step = cli_lossy_letter((const unsigned char *)line, length, lead, &letter);
            if (!cli_fuzzy_white(letter))
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
        if (imports && cli_named_in(line, length, stem))
        {
            return 1;
        }
    }
    return 0;
}

// `size` bytes read as UTF-8 into `text`, which holds CLI_PURPOSE_TEXT, each byte that starts no whole letter read as
// U+FFFD, a 0 after them: their length
static unsigned long long cli_text_of(const unsigned char *bytes, unsigned long long size, char *text)
{
    CliLine line = {text, CLI_PURPOSE_TEXT, 0ull};
    cli_lossy(&line, bytes, size);
    cli_line_closed(&line);
    return line.at;
}

int cli_purpose_said(const char *root, const char *path, CliLine *out, CliLine *said)
{
    static char s_text[(CLI_PURPOSE_HEAD * 3u) + 1u];
    static char s_said[1u << 16u];
    static CliLock s_lock = CLI_LOCK_INIT;
    char file[CLI_PATH_LONGEST];
    if (!cli_inside(root, path, file, said))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(file, &size);
    if (bytes == NULL)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    const unsigned long long head = (size < CLI_PURPOSE_HEAD) ? size : CLI_PURPOSE_HEAD;
    cli_lock_take(&s_lock);
    CliLine comment = {s_said, sizeof(s_said), 0ull};
    if ((head == 0ull) || (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = head}) == NULL))
    {
        CliLine text = {s_text, sizeof(s_text), 0ull};
        cli_lossy(&text, bytes, head);
        cli_line_closed(&text);
        cli_purpose_first_comment(file, s_text, text.at, &comment);
    }
    cli_file_let_go(bytes, size);
    cli_json_quoted(out, s_said, comment.at);
    cli_lock_give(&s_lock);
    return 1;
}

// One reader of the tree's files: the files it reads, from `from` to `to`, and the room a file's text is read into
typedef struct
{
    unsigned int from;
    unsigned int to;
    char text[CLI_PURPOSE_TEXT];
    CliThread thread;
} CliPurposePart;

// The search for users: the tree's files, the file used, its name and stem, the files that use it, the readers, and
// how many have ended, and what the file says of itself; a cell taken with the users' lock and given back with it
typedef struct
{
    char said[1u << 17u];
    const char *root;
    const char *path;
    char name[CLI_PATH_LONGEST];
    char stem[CLI_PATH_LONGEST];
    const CliFiles *files;
    unsigned char used[CLI_FILES_MOST];
    CliPurposePart part[CLI_PURPOSE_THREADS];
    unsigned int ended;
    CliWake wake;
} CliUsers;

static CliUsers *s_users;
static CliLock s_ending = CLI_LOCK_INIT;
static CliLock s_users_lock = CLI_LOCK_INIT;

// a reader's files read, each marked where it uses the file
static void cli_part_read(CliPurposePart *part)
{
    for (unsigned int index = part->from; index < part->to; index += 1u)
    {
        const char *const other = cli_files_at(s_users->files, index);
        s_users->used[index] = 0u;
        if (cellul.eq(&(CatenaFinitaCfg){.src = other, .other = s_users->path, .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char full[CLI_PATH_LONGEST];
        cli_full(s_users->root, other, full);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_file_held(full, &size);
        if (bytes == NULL)
        {
            continue;
        }
        const unsigned long long read = (size < CLI_PURPOSE_READ) ? size : CLI_PURPOSE_READ;
        if ((read == 0ull) || (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = read}) == NULL))
        {
            const unsigned long long length = cli_text_of(bytes, read, part->text);
            s_users->used[index] = (unsigned char)cli_purpose_uses(part->text, length, s_users->name, s_users->stem);
        }
        cli_file_let_go(bytes, size);
    }
}

static void cli_part_thread(void *context)
{
    cli_part_read((CliPurposePart *)context);
    cli_lock_take(&s_ending);
    s_users->ended += 1u;
    cli_wake_all(&s_users->wake);
    cli_lock_give(&s_ending);
}

// the users found written: the first CLI_PURPOSE_USERS by path, and how many in all
static void cli_users_written(CliLine *out)
{
    cli_line_text(out, "\"users\":[");
    unsigned int all = 0u;
    for (unsigned int index = 0u; (s_users->files != NULL) && (index < s_users->files->count); index += 1u)
    {
        if (!s_users->used[index])
        {
            continue;
        }
        if (all < CLI_PURPOSE_USERS)
        {
            const char *const path = cli_files_at(s_users->files, index);
            cli_line_text(out, (all != 0u) ? "," : "");
            cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        }
        all += 1u;
    }
    cli_line_text(out, "],\"all\":");
    cli_line_decimal(out, all, 0u);
}

// the files of the tree that use the file at `path` found: 1, or 0 with why said
static int cli_users_found(const char *root, const char *path, CliLine *said)
{
    char file[CLI_PATH_LONGEST];
    if (!cli_inside(root, path, file, said))
    {
        return 0;
    }
    const char *name = file;
    for (const char *at = file; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_users->name,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = s_users->stem,
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    char *const dot =
        cli_text_last(s_users->stem, cellul.len(&(CatenaFinitaCfg){.src = s_users->stem, .cap = CLI_CELLS_POOL}), '.');
    if ((dot != NULL) && (dot != s_users->stem))
    {
        dot[0] = '\0';
    }
    s_users->root = root;
    s_users->path = path;
    s_users->files = cli_files_held(root);
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
        if (cli_thread_started(&part->thread))
        {
            started += 1u;
        }
        else
        {
            cli_part_read(part);
        }
    }
    cli_lock_take(&s_ending);
    while (s_users->ended < started)
    {
        cli_wake_wait(&s_users->wake, &s_ending);
    }
    cli_lock_give(&s_ending);
    return 1;
}

// the users' lock taken and the search a cell under it: 1, or 0 with why said and the lock given back
static int cli_users_held(CliLine *said)
{
    cli_lock_take(&s_users_lock);
    s_users = cli_cell_taken(sizeof(*s_users), "a search for users");
    if (s_users == NULL)
    {
        cli_lock_give(&s_users_lock);
        cli_line_text(said, "a search for users could not be taken");
        return 0;
    }
    const CliWake woken = CLI_WAKE_INIT;
    s_users->wake = woken;
    s_users->files = NULL;
    return 1;
}

// the search given back with the view of the tree's files it held, and the users' lock with it
static void cli_users_released(void)
{
    cli_files_let(s_users->files);
    cli_cell_given(s_users);
    s_users = NULL;
    cli_lock_give(&s_users_lock);
}

int cli_purpose_users(const char *root, const char *path, CliLine *out, CliLine *said)
{
    if (!cli_users_held(said))
    {
        return 0;
    }
    const int found = cli_users_found(root, path, said);
    if (found)
    {
        cli_line_character(out, '{');
        cli_users_written(out);
        cli_line_character(out, '}');
    }
    cli_users_released();
    return found;
}

int cli_purpose(const char *root, const char *path, CliLine *out, CliLine *said)
{
    if (!cli_users_held(said))
    {
        return 0;
    }
    CliLine comment = {s_users->said, sizeof(s_users->said), 0ull};
    const int found = cli_purpose_said(root, path, &comment, said) && cli_users_found(root, path, said);
    if (found)
    {
        cli_line_text(out, "{\"said\":");
        cli_line_counted(out, s_users->said, comment.at);
        cli_line_character(out, ',');
        cli_users_written(out);
        cli_line_character(out, '}');
    }
    cli_users_released();
    return found;
}
