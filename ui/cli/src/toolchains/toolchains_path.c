// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_path.c: the tree whose own tools come first, its environment, the PATH's folders as orior_cli reads them,
// the PATH every step runs with, and a folder added to the reader's own PATH
#include "../command_line/cells.h"
#include "toolchains_internal.h"

// how long the login shell is given to say its PATH, in milliseconds
#define CLI_SHELL_PATIENCE 10000ull

#if defined(_WIN32)
#define CLI_SEPARATOR ';'
#else
#define CLI_SEPARATOR ':'
#endif

// the tree open, its environment, and the PATH the reader's login shell sets, read once on a thread of its own the
// first time the PATH's folders are asked for and none until it is read: a program opened from the desktop starts with
// less of a PATH than a terminal gives, and a slow shell profile holds up nothing
static char s_tree[CLI_PATH_LONGEST];
static int s_has_tree;
static CliTreeEnvironment s_environment;
static int s_has_environment;
static char s_shell_path[CLI_PATH_TEXT];
static int s_shell_asked;
static int s_shell_read;
static CliThread s_shell_thread;
static CliLock s_lock = CLI_LOCK_INIT;

void cli_toolchains_set_tree(const char *root)
{
    cli_lock_take(&s_lock);
    memor.cpy(&(MemoriaCfg){
        .dst = s_tree, .src = root, .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_has_tree = 1;
    cli_lock_give(&s_lock);
}

unsigned int cli_toolchains_tree_tools(char (*folders)[CLI_PATH_LONGEST], unsigned int room)
{
    static const char *const s_folders[CLI_TREE_TOOLS] = {"/node_modules/.bin", "/vendor/bin"};
    char root[CLI_PATH_LONGEST];
    cli_lock_take(&s_lock);
    const int has = s_has_tree;
    memor.cpy(&(MemoriaCfg){.dst = root,
                            .src = s_tree,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_tree, .cap = CLI_CELLS_POOL}) + 1u});
    cli_lock_give(&s_lock);
    unsigned int count = 0u;
    for (unsigned int at = 0u; has && (at < CLI_TREE_TOOLS) && (count < room); at += 1u)
    {
        CliLine line = {folders[count], CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, root);
        cli_line_text(&line, s_folders[at]);
        count += ((cli_line_closed(&line) != 0ull) && query_is_folder(folders[count])) ? 1u : 0u;
    }
    return count;
}

void cli_toolchains_set_environment(const CliTreeEnvironment *environment)
{
    cli_lock_take(&s_lock);
    s_has_environment = environment != NULL;
    if (environment != NULL)
    {
        s_environment = *environment;
    }
    cli_lock_give(&s_lock);
}

int cli_toolchains_environment(CliTreeEnvironment *environment)
{
    cli_lock_take(&s_lock);
    const int has = s_has_environment;
    if (has)
    {
        *environment = s_environment;
    }
    cli_lock_give(&s_lock);
    return has;
}

// The PATH `shell` sets as a login shell, or none where it says none within the patience: 1, or 0
int cli_toolchains_path_of_shell(const char *shell, char *path, unsigned long long size)
{
    const char *const words[5] = {shell, "-l", "-c", "printf '%s' \"$PATH\"", NULL};
    CliSpawned spawned;
    char unsaid[CLI_PATH_LONGEST];
    CliLine said = {unsaid, sizeof(unsaid), 0ull};
    if (!cli_spawn(&spawned, words, NULL, NULL, 0u, 1, &said))
    {
        return 0;
    }
    const unsigned long long began = cli_clock_milliseconds();
    long long code = 0ll;
    while (!cli_spawn_poll(&spawned, &code))
    {
        if ((cli_clock_milliseconds() - began) >= CLI_SHELL_PATIENCE)
        {
            cli_spawn_tree_end(spawned.id);
            cli_spawn_wait(&spawned);
            cli_spawn_closed(&spawned);
            return 0;
        }
        query_paused(20u);
    }
    unsigned long long read = 0ull;
    for (;;)
    {
        const unsigned long long got = cli_spawn_read(spawned.out, path + read, size - read - 1ull);
        if (got == 0ull)
        {
            break;
        }
        read += got;
    }
    cli_spawn_closed(&spawned);
    while ((read != 0ull) && ((path[read - 1u] == ' ') || ((path[read - 1u] >= '\t') && (path[read - 1u] <= '\r'))))
    {
        read -= 1ull;
    }
    path[read] = '\0';
    return read != 0ull;
}

static void cli_shell_reading(void *context)
{
    const char *const shell = (const char *)context;
    char *const text = cli_cell_taken(CLI_PATH_TEXT, "the login shell's PATH, as it is read");
    const int read = (text != NULL) && cli_toolchains_path_of_shell(shell, text, CLI_PATH_TEXT);
    cli_lock_take(&s_lock);
    if (read)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_shell_path,
                                .src = text,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
        s_shell_read = 1;
    }
    cli_lock_give(&s_lock);
    cli_cell_given(text);
}

// the login shell's PATH into `path`: 1 once it has been read, its reading begun the first time it is asked for
static int cli_shell_path(char *path, unsigned long long size)
{
#if defined(_WIN32)
    (void)path;
    (void)size;
    return 0;
#else
    static char s_shell[CLI_PATH_LONGEST];
    cli_lock_take(&s_lock);
    const int ask = !s_shell_asked;
    s_shell_asked = 1;
    const int read = s_shell_read;
    if (read)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = path,
                          .src = s_shell_path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_shell_path, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_lock_give(&s_lock);
    if (ask && query_environment_read("SHELL", s_shell, sizeof(s_shell)))
    {
        s_shell_thread.run = cli_shell_reading;
        s_shell_thread.context = s_shell;
        cli_thread_started(&s_shell_thread);
    }
    return read;
#endif
}

// each folder of a PATH's text put after the folders, each once by its key: the count after
static unsigned int cli_split(const char *text, char (*folders)[CLI_PATH_LONGEST], unsigned int count,
                              unsigned int room)
{
    const char *from = text;
    for (const char *at = text;; at += 1)
    {
        if ((at[0] == CLI_SEPARATOR) || (at[0] == '\0'))
        {
            char folder[CLI_PATH_LONGEST];
            CliLine line = {folder, sizeof(folder), 0ull};
            cli_line_counted(&line, from, (unsigned long long)(at - from));
            if ((at > from) && (cli_line_closed(&line) != 0ull))
            {
                char key[CLI_PATH_LONGEST];
                cli_toolchains_key_of(folder, key);
                int seen = 0;
                for (unsigned int each = 0u; !seen && (each < count); each += 1u)
                {
                    char held[CLI_PATH_LONGEST];
                    cli_toolchains_key_of(folders[each], held);
                    seen = cellul.eq(&(CatenaFinitaCfg){.src = held, .other = key, .cap = CLI_CELLS_POOL});
                }
                if (!seen && (count < room))
                {
                    memor.cpy(&(MemoriaCfg){
                        .dst = folders[count],
                        .src = folder,
                        .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
                    count += 1u;
                }
            }
            if (at[0] == '\0')
            {
                break;
            }
            from = at + 1;
        }
    }
    return count;
}

// the system's and the reader's PATH as the registry holds them now, put after the folders: the count after
static unsigned int cli_registry_folders(char (*folders)[CLI_PATH_LONGEST], unsigned int count, unsigned int room)
{
    // the PATH as the registry holds it and as it reads expanded, a cell of the two
    char *const held = cli_cell_taken(2ull * CLI_PATH_TEXT, "the registry's PATH");
    char *const expanded = (held != NULL) ? held + CLI_PATH_TEXT : NULL;
    if ((held != NULL) && cli_registry_machine_path(held, CLI_PATH_TEXT))
    {
        cli_registry_expand(held, expanded, CLI_PATH_TEXT);
        count = cli_split(expanded, folders, count, room);
    }
    if ((held != NULL) && cli_registry_user_value("Path", held, CLI_PATH_TEXT))
    {
        cli_registry_expand(held, expanded, CLI_PATH_TEXT);
        count = cli_split(expanded, folders, count, room);
    }
    cli_cell_given(held);
    return count;
}

unsigned int cli_toolchains_path_folders(char (*folders)[CLI_PATH_LONGEST], unsigned int room)
{
    char *const text = cli_cell_taken(CLI_PATH_TEXT, "the PATH's text");
    unsigned int count = cli_toolchains_tree_tools(folders, room);
    count = cli_registry_folders(folders, count, room);
    if ((text != NULL) && query_environment_read("PATH", text, CLI_PATH_TEXT))
    {
        count = cli_split(text, folders, count, room);
    }
    if ((text != NULL) && cli_shell_path(text, CLI_PATH_TEXT))
    {
        count = cli_split(text, folders, count, room);
    }
    cli_cell_given(text);
    return count;
}

// the folders a list the separator parts names at the most: one for each separator, and one more
static unsigned int cli_parts(const char *text)
{
    unsigned int parts = 1u;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        parts += (*at == CLI_SEPARATOR) ? 1u : 0u;
    }
    return parts;
}

char (*cli_toolchains_path_taken(unsigned int *count)) [CLI_PATH_LONGEST] {
    // the most folders the tree's tools, the registry's two paths, the PATH and the shell's PATH name, counted from
    // their texts as cli_toolchains_path_folders reads them
    char *const text = cli_cell_taken(2ull * CLI_PATH_TEXT, "the PATH's texts, as they are counted");
    char *const expanded = (text != NULL) ? text + CLI_PATH_TEXT : NULL;
    if (text == NULL)
    {
        *count = 0u;
        return NULL;
    }
    unsigned int most = CLI_TREE_TOOLS;
    if (cli_registry_machine_path(text, CLI_PATH_TEXT))
    {
        cli_registry_expand(text, expanded, CLI_PATH_TEXT);
        most += cli_parts(expanded);
    }
    if (cli_registry_user_value("Path", text, CLI_PATH_TEXT))
    {
        cli_registry_expand(text, expanded, CLI_PATH_TEXT);
        most += cli_parts(expanded);
    }
    most += query_environment_read("PATH", text, CLI_PATH_TEXT) ? cli_parts(text) : 0u;
    most += cli_shell_path(text, CLI_PATH_TEXT) ? cli_parts(text) : 0u;
    cli_cell_given(text);
    char (*const folders)[CLI_PATH_LONGEST] =
        cli_cell_taken((unsigned long long)most * CLI_PATH_LONGEST, "the PATH's folders");
    *count = (folders != NULL) ? cli_toolchains_path_folders(folders, most) : 0u;
    return folders;
}

int cli_toolchains_on_path(const char *const *names, unsigned int count, char *program)
{
    unsigned int folders = 0u;
    char (*const path)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&folders);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < folders); at += 1u)
    {
        found = cli_toolchains_program_in(path[at], names, count, program);
    }
    cli_cell_given(path);
    return found;
}

unsigned int cli_toolchains_system_path(char (*folders)[CLI_PATH_LONGEST], unsigned int room)
{
#if defined(_WIN32)
    return cli_registry_folders(folders, 0u, room);
#else
    char *const text = cli_cell_taken(CLI_PATH_TEXT, "the PATH's text");
    const unsigned int count = ((text != NULL) && query_environment_read("PATH", text, CLI_PATH_TEXT))
                                   ? cli_split(text, folders, 0u, room)
                                   : 0u;
    cli_cell_given(text);
    return count;
#endif
}

// The folders the reader gave for toolchains, put after the folders where each is a folder, or only counted where
// `folders` is NULL
typedef struct
{
    char (*folders)[CLI_PATH_LONGEST];
    unsigned int count;
    unsigned int room;
} CliChosenFolders;

static void cli_chosen_each(void *context, const char *id, const char *folder)
{
    CliChosenFolders *const chosen = (CliChosenFolders *)context;
    (void)id;
    if (!query_is_folder(folder))
    {
        return;
    }
    if (chosen->folders == NULL)
    {
        chosen->count += 1u;
        return;
    }
    if (chosen->count < chosen->room)
    {
        memor.cpy(&(MemoriaCfg){.dst = chosen->folders[chosen->count],
                                .src = folder,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
        chosen->count += 1u;
    }
}

// `count` folders of `from` put after the folders, `listed` of them, which hold `room`: the count after
static unsigned int cli_folders_put(char (*folders)[CLI_PATH_LONGEST], unsigned int listed, unsigned int room,
                                    const char (*from)[CLI_PATH_LONGEST], unsigned int count)
{
    for (unsigned int at = 0u; (at < count) && (listed < room); at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = folders[listed],
                                .src = from[at],
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = from[at], .cap = CLI_CELLS_POOL}) + 1u});
        listed += 1u;
    }
    return listed;
}

void cli_toolchains_run_path_with(const char (*ahead)[CLI_PATH_LONGEST], unsigned int count, char *path,
                                  unsigned long long size)
{
    unsigned int path_count = 0u;
    char (*const path_folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&path_count);
    CliTreeEnvironment environment;
    const unsigned int bins = cli_toolchains_environment(&environment) ? environment.bins : 0u;
    CliChosenFolders counted = {NULL, 0u, 0u};
    cli_toolchains_chosen_each(cli_chosen_each, &counted);
    // the folders ahead, the environment's, the tree's own tools', those the reader chose and the PATH's, in a cell
    // of their count
    const unsigned int most = count + bins + CLI_TREE_TOOLS + counted.count + path_count;
    char (*const folders)[CLI_PATH_LONGEST] =
        cli_cell_taken((unsigned long long)most * CLI_PATH_LONGEST, "the folders a step's PATH names");
    if (folders == NULL)
    {
        cli_cell_given(path_folders);
        query_environment_read("PATH", path, size);
        return;
    }
    unsigned int listed = cli_folders_put(folders, 0u, most, ahead, count);
    listed = cli_folders_put(folders, listed, most, (const char (*)[CLI_PATH_LONGEST])environment.bin, bins);
    listed += cli_toolchains_tree_tools(folders + listed, most - listed);
    CliChosenFolders chosen = {folders, listed, most};
    cli_toolchains_chosen_each(cli_chosen_each, &chosen);
    listed = cli_folders_put(folders, chosen.count, most, (const char (*)[CLI_PATH_LONGEST])path_folders, path_count);
    cli_cell_given(path_folders);
    // each folder once by its key, in order
    CliLine line = {path, size, 0ull};
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        char key[CLI_PATH_LONGEST];
        cli_toolchains_key_of(folders[at], key);
        int seen = folders[at][0] == '\0';
        for (unsigned int before = 0u; !seen && (before < at); before += 1u)
        {
            char held[CLI_PATH_LONGEST];
            cli_toolchains_key_of(folders[before], held);
            seen = cellul.eq(&(CatenaFinitaCfg){.src = held, .other = key, .cap = CLI_CELLS_POOL});
        }
        if (!seen)
        {
            if (kept != 0u)
            {
                cli_line_character(&line, CLI_SEPARATOR);
            }
            cli_line_text(&line, folders[at]);
            kept += 1u;
        }
    }
    cli_cell_given(folders);
    if (cli_line_closed(&line) == 0ull)
    {
        query_environment_read("PATH", path, size);
    }
}

void cli_toolchains_run_path(char *path, unsigned long long size)
{
    cli_toolchains_run_path_with(NULL, 0u, path, size);
}

unsigned int cli_toolchains_folders_for(const CliJson *json, unsigned int by_system, char (*folders)[CLI_PATH_LONGEST],
                                        unsigned int count, unsigned int room)
{
    const unsigned int patterns = cli_toolchains_for_system(json, by_system);
    for (unsigned int item = cli_json_is(json, patterns, CFG_JSON_ARRAY) ? json->node[patterns].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const pattern = cli_json_string(json, item, NULL);
        if (pattern != NULL)
        {
            count = cli_toolchains_folders_of(pattern, folders, count, room);
        }
    }
    return count;
}

int cli_toolchains_joined(const char *path, const char *folder, char *out, unsigned long long size)
{
    char key[CLI_PATH_LONGEST];
    cli_toolchains_key_of(folder, key);
    const char *from = path;
    for (const char *at = path;; at += 1)
    {
        if ((at[0] == CLI_SEPARATOR) || (at[0] == '\0'))
        {
            char dir[CLI_PATH_LONGEST];
            char held[CLI_PATH_LONGEST];
            CliLine line = {dir, sizeof(dir), 0ull};
            cli_line_counted(&line, from, (unsigned long long)(at - from));
            if ((at > from) && (cli_line_closed(&line) != 0ull))
            {
                cli_toolchains_key_of(dir, held);
                if (cellul.eq(&(CatenaFinitaCfg){.src = held, .other = key, .cap = CLI_CELLS_POOL}))
                {
                    return 0;
                }
            }
            if (at[0] == '\0')
            {
                break;
            }
            from = at + 1;
        }
    }
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((length != 0ull) && (path[length - 1u] == CLI_SEPARATOR))
    {
        length -= 1ull;
    }
    CliLine line = {out, size, 0ull};
    cli_line_counted(&line, path, length);
    if (length != 0ull)
    {
        cli_line_character(&line, CLI_SEPARATOR);
    }
    cli_line_text(&line, folder);
    return cli_line_closed(&line) != 0ull;
}
