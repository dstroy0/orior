// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// root.c: the orior tree the app works on, and the rule that keeps every path inside it. A window can hold folders
// from other places, or single files from them, beside its tree: each is mounted under a name of its own, and a path
// that starts `@name/` is under that folder, or `@name` is that file. The rule that keeps a path inside the tree keeps
// one under a mount inside the mount
#include "../command_line/cli_internal.h"
#include "../text/text.h"

// the file whose presence marks the top of the engine's tree, which the command line finds by walking up where no
// tree is named
#define CLI_TREE_MARKER "src/cu/engine/engine_config.h"

// the folders and files mounted beside the tree, each under its name, in the order they were mounted
static char s_mount_name[CLI_MOUNTS_MOST][CLI_NAME_LONGEST];
static char s_mount_path[CLI_MOUNTS_MOST][CLI_PATH_LONGEST];
static unsigned int s_mounts;

int cli_holds_tree(const char *folder)
{
    char marker[CLI_PATH_LONGEST];
    CliLine line = {marker, sizeof(marker), 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, CLI_TREE_MARKER);
    return (cli_line_closed(&line) != 0ull) && query_is_file(marker);
}

int cli_tree_at(const char *path, char *full, CliLine *said)
{
    if (!query_path_full(path, full, CLI_PATH_LONGEST))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be made a full path");
        return 0;
    }
    if (!query_is_folder(full))
    {
        cli_line_text(said, full);
        cli_line_text(said, " is no folder");
        return 0;
    }
    return 1;
}

// the first folder from `start` up holding the tree into `tree`: 1, or 0 where none does
static int cli_tree_above(const char *start, char *tree)
{
    char folder[CLI_PATH_LONGEST];
    if (!query_path_full(start, folder, sizeof(folder)))
    {
        return 0;
    }
    for (;;)
    {
        if (cli_holds_tree(folder))
        {
            return query_path_full(folder, tree, CLI_PATH_LONGEST);
        }
        char *slash = NULL;
        for (char *at = folder; *at != '\0'; at += 1)
        {
            slash = ((*at == '/') || (*at == '\\')) ? at : slash;
        }
        // a root, `C:\` or `/`, stands where no separator is left past its first
        if ((slash == NULL) || (slash == folder) || ((slash == (folder + 2)) && (folder[1] == ':')))
        {
            if ((slash != NULL) && (slash[1] != '\0'))
            {
                slash[1] = '\0';
                continue;
            }
            return 0;
        }
        *slash = '\0';
    }
}

int cli_tree_found(char *tree)
{
    char named[CLI_PATH_LONGEST];
    char said_text[CLI_SAID_LONGEST];
    CliLine said = {said_text, sizeof(said_text), 0ull};
    if (query_environment_read("ORIOR_ROOT", named, sizeof(named)) && cli_tree_at(named, tree, &said))
    {
        return 1;
    }
    char here[CLI_PATH_LONGEST];
    if (cli_folder_current(here, sizeof(here)) && cli_tree_above(here, tree))
    {
        return 1;
    }
    char program[CLI_PATH_LONGEST];
    if (cli_program_own(program, sizeof(program)))
    {
        char *slash = NULL;
        for (char *at = program; *at != '\0'; at += 1)
        {
            slash = ((*at == '/') || (*at == '\\')) ? at : slash;
        }
        if (slash != NULL)
        {
            *slash = '\0';
            return cli_tree_above(program, tree);
        }
    }
    return 0;
}

int cli_mounted(const char *path, char *name, CliLine *said)
{
    char full[CLI_PATH_LONGEST];
    if (!query_path_full(path, full, sizeof(full)) || (!query_is_file(full) && !query_is_folder(full)))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it is not there");
        return 0;
    }
    for (unsigned int at = 0u; at < s_mounts; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_mount_path[at], .other = full, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = name,
                .src = s_mount_name[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_mount_name[at], .cap = CLI_CELLS_POOL}) + 1u});
            return name[0] != '\0';
        }
    }
    if (s_mounts >= CLI_MOUNTS_MOST)
    {
        cli_line_text(said, "the window holds as many mounts as it can");
        return 0;
    }
    // the last part of the path, each separator in it made `_`
    const char *base = full;
    for (const char *at = full; *at != '\0'; at += 1)
    {
        base = (((*at == '/') || (*at == '\\')) && (at[1] != '\0')) ? (at + 1) : base;
    }
    char stem[CLI_NAME_LONGEST];
    CliLine stem_line = {stem, sizeof(stem), 0ull};
    for (const char *at = base; *at != '\0'; at += 1)
    {
        cli_line_character(&stem_line, ((*at == '/') || (*at == '\\')) ? '_' : *at);
    }
    if ((cli_line_closed(&stem_line) == 0ull) || (stem[0] == '\0'))
    {
        memor.cpy(&(MemoriaCfg){.dst = stem,
                                .src = "folder",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "folder", .cap = CLI_CELLS_POOL}) + 1u});
    }
    CliLine name_line = {name, CLI_NAME_LONGEST, 0ull};
    cli_line_text(&name_line, stem);
    cli_line_closed(&name_line);
    for (unsigned long long count = 2ull;; count += 1ull)
    {
        int taken = 0;
        for (unsigned int at = 0u; !taken && (at < s_mounts); at += 1u)
        {
            taken = cellul.eq(&(CatenaFinitaCfg){.src = s_mount_name[at], .other = name, .cap = CLI_CELLS_POOL});
        }
        if (!taken)
        {
            break;
        }
        name_line.at = 0ull;
        cli_line_text(&name_line, stem);
        cli_line_character(&name_line, '-');
        cli_line_decimal(&name_line, count, 0u);
        cli_line_closed(&name_line);
    }
    memor.cpy(&(MemoriaCfg){.dst = s_mount_name[s_mounts], .src = name, .bytes = CLI_NAME_LONGEST});
    memor.cpy(&(MemoriaCfg){.dst = s_mount_path[s_mounts], .src = full, .bytes = CLI_PATH_LONGEST});
    s_mounts += 1u;
    return 1;
}

void cli_unmounted(const char *name)
{
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < s_mounts; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = s_mount_name[at], .other = name, .cap = CLI_CELLS_POOL}))
        {
            memor.move_down(
                &(MemoriaCfg){.dst = s_mount_name[kept], .src = s_mount_name[at], .bytes = CLI_NAME_LONGEST});
            memor.move_down(
                &(MemoriaCfg){.dst = s_mount_path[kept], .src = s_mount_path[at], .bytes = CLI_PATH_LONGEST});
            kept += 1u;
        }
    }
    s_mounts = kept;
}

void cli_unmounted_all(void)
{
    s_mounts = 0u;
}

unsigned int cli_mounts(const char **names, const char **paths, unsigned int room)
{
    unsigned int at = 0u;
    for (; (at < s_mounts) && (at < room); at += 1u)
    {
        names[at] = s_mount_name[at];
        paths[at] = s_mount_path[at];
    }
    return at;
}

int cli_inside(const char *root, const char *relative, char *joined, CliLine *said)
{
    CliLine line = {joined, CLI_PATH_LONGEST, 0ull};
    const char *rest = relative;
    if (relative[0] == '@')
    {
        const char *const slash =
            cellul.chr(&(CatenaFinitaCfg){.src = relative + 1, .cap = CLI_CELLS_POOL, .byte = '/'});
        const size_t length = (slash != NULL)
                                  ? (size_t)(slash - relative - 1)
                                  : cellul.len(&(CatenaFinitaCfg){.src = relative + 1, .cap = CLI_CELLS_POOL});
        int found = 0;
        for (unsigned int at = 0u; !found && (at < s_mounts); at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = s_mount_name[at], .cap = CLI_CELLS_POOL}) == length) &&
                cli_text_same(s_mount_name[at], relative + 1, length))
            {
                cli_line_text(&line, s_mount_path[at]);
                found = 1;
            }
        }
        if (!found)
        {
            cli_line_text(said, relative);
            cli_line_text(said, " is in no folder of this window");
            return 0;
        }
        rest = (slash != NULL) ? (slash + 1) : "";
    }
    else
    {
        cli_line_text(&line, root);
    }
    // each part a name or `.`: one that climbs out, or a root of its own, is refused before anything touches the disk
    const int rooted = (rest[0] == '/') || (rest[0] == '\\') || ((rest[0] != '\0') && (rest[1] == ':'));
    const char *part = rest;
    while (!rooted && (*part != '\0'))
    {
        const size_t length = cli_text_upto(part, "/\\");
        if ((length == 2u) && (part[0] == '.') && (part[1] == '.'))
        {
            break;
        }
        if ((length != 0u) && !((length == 1u) && (part[0] == '.')))
        {
            cli_line_character(&line, '/');
            cli_line_counted(&line, part, length);
        }
        part += length;
        part += (*part != '\0') ? 1 : 0;
    }
    if (rooted || (*part != '\0'))
    {
        cli_line_text(said, relative);
        cli_line_text(said, " is outside the tree");
        return 0;
    }
    return cli_line_closed(&line) != 0ull;
}

void cli_full(const char *root, const char *relative, char *joined)
{
    char said_text[CLI_SAID_LONGEST];
    CliLine said = {said_text, sizeof(said_text), 0ull};
    if (!cli_inside(root, relative, joined, &said))
    {
        // a whole path is the path it names, and any other is written under the tree
        const int whole =
            (relative[0] == '/') || (relative[0] == '\\') || ((relative[0] != '\0') && (relative[1] == ':'));
        CliLine line = {joined, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, whole ? "" : root);
        cli_line_text(&line, whole ? "" : "/");
        cli_line_text(&line, relative);
        cli_line_closed(&line);
    }
}

// 1 where `path` is `base` or stands under it
static int cli_starts_with(const char *path, const char *base)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL});
    return cli_text_same(path, base, length) &&
           ((path[length] == '\0') || (path[length] == '/') || (path[length] == '\\'));
}

void cli_relative(const char *root, const char *path, char *relative)
{
    CliLine line = {relative, CLI_PATH_LONGEST, 0ull};
    const char *rest = path;
    if (!cli_starts_with(path, root))
    {
        // the mount whose folder is the longest that holds the path
        unsigned int found = s_mounts;
        size_t longest = 0u;
        for (unsigned int at = 0u; at < s_mounts; at += 1u)
        {
            const size_t length = cellul.len(&(CatenaFinitaCfg){.src = s_mount_path[at], .cap = CLI_CELLS_POOL});
            if (cli_starts_with(path, s_mount_path[at]) && (length >= longest))
            {
                found = at;
                longest = length;
            }
        }
        if (found < s_mounts)
        {
            cli_line_character(&line, '@');
            cli_line_text(&line, s_mount_name[found]);
            rest = path + longest;
            if (*rest != '\0')
            {
                cli_line_character(&line, '/');
                rest += 1;
            }
            for (; *rest != '\0'; rest += 1)
            {
                cli_line_character(&line, (*rest == '\\') ? '/' : *rest);
            }
            cli_line_closed(&line);
            return;
        }
    }
    else
    {
        rest = path + cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
        rest += ((*rest == '/') || (*rest == '\\')) ? 1 : 0;
    }
    for (; *rest != '\0'; rest += 1)
    {
        cli_line_character(&line, (*rest == '\\') ? '/' : *rest);
    }
    cli_line_closed(&line);
}

int cli_path_parent(const char *path, char *out)
{
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    const unsigned long long root = ((length >= 2ull) && (path[1] == ':')) ? 3ull : 1ull;
    while ((length > root) && ((path[length - 1ull] == '/') || (path[length - 1ull] == '\\')))
    {
        length -= 1ull;
    }
    unsigned long long slash = length;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        slash = ((path[at] == '/') || (path[at] == '\\')) ? at : slash;
    }
    if ((slash == length) || ((slash + 1ull) == length))
    {
        return 0;
    }
    const unsigned long long kept = (slash < root) ? root : slash;
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&line, path, (kept > length) ? length : kept);
    return cli_line_closed(&line) != 0ull;
}
