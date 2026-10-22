// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_search.c: the tree's Python scripts that touch `sys.path` read, and the folders they name in the tree
// counted, the most used first, the reading kept for thirty seconds
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "python_paths_internal.h"

// a file larger than this is not read; how long a tree's reading stands, in milliseconds; the most folders counted,
// and the most one script names
#define CLI_PY_LARGEST (1ull << 20u)
#define CLI_PY_FRESH_FOR 30000ull
#define CLI_PY_KEPT 1024u
#define CLI_PY_SEEN 256u

// A folder the scripts name, and how many of them name it
typedef struct
{
    char path[CLI_PATH_LONGEST];
    unsigned int count;
} CliPyCount;

// The tree's reading: the tree, when it was read, the folders counted and how many, the tree's files, the folders
// one script names, and its text
typedef struct
{
    char root[CLI_PATH_LONGEST];
    unsigned long long at;
    int kept;
    CliPyCount counted[CLI_PY_KEPT];
    unsigned int counts;
    char seen[CLI_PY_SEEN][CLI_PATH_LONGEST];
    unsigned int seens;
} CliPySearch;

static CliPySearch s_search;
static CliLock s_lock = CLI_LOCK_INIT;

// whether a file of the tree is a script a search reads: one ending in .py in no folder of an environment, of what a
// build writes, or of a cache
static int cli_searched(const char *file)
{
    static const char *const s_not[8] = {"venv",          ".venv",       "env",  "node_modules",
                                         "site-packages", "__pycache__", ".tox", ".nox"};
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
    if ((length < 3ull) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = file + length - 3ull, .other = ".py", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    const char *part = file;
    for (const char *at = file;; at += 1)
    {
        if ((at[0] == '/') || (at[0] == '\0'))
        {
            // the part lies in the path, at or past its start
            const unsigned long long size = (unsigned long long)(at - part);
            for (unsigned int one = 0u; one < 8u; one += 1u)
            {
                if ((cellul.len(&(CatenaFinitaCfg){.src = s_not[one], .cap = CLI_CELLS_POOL}) == size) &&
                    (memor.cmp(&(MemoriaCfg){.src = part, .other = s_not[one], .bytes = size}) == 0))
                {
                    return 0;
                }
            }
            if (at[0] == '\0')
            {
                return 1;
            }
            part = at + 1;
        }
    }
}

// whether `size` bytes are UTF-8 whole and hold `sys.path`
static int cli_touches_path(const unsigned char *bytes, unsigned long long size)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, size, at, &letter);
        if ((step == 1ull) && (bytes[at] >= 0x80u))
        {
            return 0;
        }
        at += step;
    }
    for (unsigned long long place = 0ull; (place + 8ull) <= size; place += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = bytes + place, .other = "sys.path", .bytes = 8u}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// a folder a script named counted, once for that script
static void cli_counted(const char *folder)
{
    for (unsigned int at = 0u; at < s_search.seens; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_search.seen[at], .other = folder, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (s_search.seens < CLI_PY_SEEN)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_search.seen[s_search.seens],
                                .src = folder,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
        s_search.seens += 1u;
    }
}

// the folders one script named, each counted
static void cli_seen_counted(void)
{
    for (unsigned int one = 0u; one < s_search.seens; one += 1u)
    {
        unsigned int at = 0u;
        while ((at < s_search.counts) &&
               (!cellul.eq(&(CatenaFinitaCfg){
                   .src = s_search.counted[at].path, .other = s_search.seen[one], .cap = CLI_CELLS_POOL})))
        {
            at += 1u;
        }
        if ((at == s_search.counts) && (s_search.counts < CLI_PY_KEPT))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = s_search.counted[at].path, .bytes = CLI_PATH_LONGEST, .src = s_search.seen[one]});
            s_search.counted[at].count = 0u;
            s_search.counts += 1u;
        }
        if (at < s_search.counts)
        {
            s_search.counted[at].count += 1u;
        }
    }
}

// the script at `file` of the tree read, and the folders in the tree other than `top` it names counted
static void cli_script_read(const char *root, const char *top, const char *file)
{
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, root);
    cli_line_character(&line, '/');
    cli_line_text(&line, file);
    unsigned long long size = 0ull;
    if ((cli_line_closed(&line) == 0ull) || !cli_file_size(path, &size) || (size > CLI_PY_LARGEST))
    {
        return;
    }
    const unsigned char *const bytes = cli_file_held(path, &size);
    if (bytes == NULL)
    {
        return;
    }
    if (cli_touches_path(bytes, size))
    {
        cli_py_paths_in(path, (const char *)bytes, size);
        s_search.seens = 0u;
        for (unsigned int at = 0u; at < g_cli_py.founds; at += 1u)
        {
            char folder[CLI_PATH_LONGEST];
            const int full = query_path_full(cli_py_text(g_cli_py.found[at]), folder, sizeof(folder));
            if (full && query_is_folder(folder) && cli_paste_under(top, folder) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = top, .cap = CLI_CELLS_POOL})))
            {
                cli_counted(folder);
            }
        }
        cli_py_reading_given();
        cli_seen_counted();
    }
    cli_file_let_go(bytes, size);
}

unsigned int cli_python_paths(const char *root, unsigned int most, void (*each)(void *context, const char *path),
                              void *context)
{
    cli_lock_take(&s_lock);
    const int fresh = s_search.kept &&
                      (cellul.eq(&(CatenaFinitaCfg){.src = s_search.root, .other = root, .cap = CLI_CELLS_POOL})) &&
                      ((cli_clock_milliseconds() - s_search.at) < CLI_PY_FRESH_FOR);
    if (!fresh)
    {
        char top[CLI_PATH_LONGEST];
        if (!query_path_full(root, top, sizeof(top)))
        {
            memor.cpy(&(MemoriaCfg){.dst = top,
                                    .src = root,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        }
        s_search.counts = 0u;
        const CliFiles *const files = cli_files_held(root);
        for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
        {
            const char *const file = cli_files_at(files, index);
            if (cli_searched(file))
            {
                cli_script_read(root, top, file);
            }
        }
        cli_files_let(files);
        // the most used first, then by path
        for (unsigned int at = 1u; at < s_search.counts; at += 1u)
        {
            const CliPyCount held = s_search.counted[at];
            unsigned int place = at;
            while ((place > 0u) && ((s_search.counted[place - 1u].count < held.count) ||
                                    ((s_search.counted[place - 1u].count == held.count) &&
                                     (cli_text_order(s_search.counted[place - 1u].path, held.path) > 0))))
            {
                s_search.counted[place] = s_search.counted[place - 1u];
                place -= 1u;
            }
            s_search.counted[place] = held;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_search.root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        s_search.at = cli_clock_milliseconds();
        s_search.kept = 1;
    }
    const unsigned int count = (s_search.counts < most) ? s_search.counts : most;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        each(context, s_search.counted[at].path);
    }
    cli_lock_give(&s_lock);
    return count;
}
