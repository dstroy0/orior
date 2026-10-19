// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_compare.c: the program a build made compared with the last build of the same commit, and the command
// that runs it, in the folder the job names, with the variables of the tree's .env
#include "../code/git.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "executables_internal.h"

// the most ranges of difference told, and the most bytes of a tree's .env read
#define CLI_COMPARE_RANGES 3u

#define CLI_ENV_BYTES (1u << 16u)

// a line said through `say`
static void cli_say(QuerySink say, const char *text)
{
    say.said(say.context, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// The places two builds of a program differ, as ranges of bytes, the first CLI_COMPARE_RANGES of them, and the count
// of bytes that differ; files of two lengths differ from the shorter one's end as well
typedef struct
{
    unsigned long long start[CLI_COMPARE_RANGES];
    unsigned long long end[CLI_COMPARE_RANGES];
    unsigned int ranges;
    unsigned long long count;
} CliDifferences;

// a range kept where there is room for it
static void cli_range(CliDifferences *found, unsigned long long start, unsigned long long end)
{
    if (found->ranges < CLI_COMPARE_RANGES)
    {
        found->start[found->ranges] = start;
        found->end[found->ranges] = end;
        found->ranges += 1u;
    }
}

static void cli_differences(const unsigned char *old, unsigned long long old_size, const unsigned char *now,
                            unsigned long long now_size, CliDifferences *found)
{
    found->ranges = 0u;
    found->count = 0ull;
    const unsigned long long shared = (old_size < now_size) ? old_size : now_size;
    int open = 0;
    unsigned long long start = 0ull;
    for (unsigned long long at = 0ull; at < shared; at += 1ull)
    {
        if (old[at] != now[at])
        {
            found->count += 1ull;
            start = open ? start : at;
            open = 1;
        }
        else if (open)
        {
            cli_range(found, start, at);
            open = 0;
        }
    }
    if (open)
    {
        cli_range(found, start, shared);
    }
    if (old_size != now_size)
    {
        const unsigned long long longest = (old_size > now_size) ? old_size : now_size;
        found->count += longest - shared;
        cli_range(found, shared, longest);
    }
}

void cli_executables_compare_build(const char *root, const char *id, const char *file, QuerySink say)
{
    char commit[CLI_NAME_LONGEST];
    long long when = 0ll;
    int clean = 0;
    if (!cli_git_head(root, commit, &when, &clean) || !clean)
    {
        return;
    }
    // the job's id with every letter but a letter, a digit, `.` and `-` made `_`
    char slug[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = slug, .src = id, .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = slug; at[0] != '\0'; at += 1)
    {
        const char one = at[0];
        const int kept = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) ||
                         ((one >= '0') && (one <= '9')) || (one == '.') || (one == '-') ||
                         ((unsigned char)one >= 0x80u);
        at[0] = kept ? one : '_';
    }
    char folder[CLI_PATH_LONGEST];
    char kept_folder[CLI_PATH_LONGEST];
    char kept[CLI_PATH_LONGEST];
    CliLine line = {folder, sizeof(folder), 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/build/orior/builds/");
    cli_line_text(&line, slug);
    const char *const name =
        (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') + 1)
        : (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') != NULL)
            ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') + 1)
            : file;
    if ((cli_line_closed(&line) == 0ull) || !cli_executables_joined(folder, commit, kept_folder) ||
        !cli_executables_joined(kept_folder, name, kept))
    {
        return;
    }
    unsigned long long now_size = 0ull;
    const unsigned char *const now = qry_member_file_mapped(file, &now_size);
    if (now == NULL)
    {
        return;
    }
    unsigned long long old_size = 0ull;
    const unsigned char *const old = qry_member_file_mapped(kept, &old_size);
    if (old != NULL)
    {
        char said[CLI_PATH_LONGEST * 2u];
        CliLine told = {said, sizeof(said), 0ull};
        char short_commit[16];
        CliLine shortened = {short_commit, sizeof(short_commit), 0ull};
        cli_line_counted(&shortened, commit,
                         (cellul.len(&(CatenaFinitaCfg){.src = commit, .cap = CLI_CELLS_POOL}) < 10u)
                             ? cellul.len(&(CatenaFinitaCfg){.src = commit, .cap = CLI_CELLS_POOL})
                             : 10u);
        cli_line_closed(&shortened);
        CliDifferences found;
        cli_differences(old, old_size, now, now_size, &found);
        if (found.count == 0ull)
        {
            cli_line_text(&told, name);
            cli_line_text(&told, " is the same, byte for byte, as the last build of commit ");
            cli_line_text(&told, short_commit);
        }
        else
        {
            // a Windows program's header's time stamp: four bytes, eight past the PE header
            int stamped_file = (now_size >= 0x40ull) && (now[0] == 'M') && (now[1] == 'Z');
            unsigned long long stamp = 0ull;
            for (unsigned int byte = 0u; stamped_file && (byte < 4u); byte += 1u)
            {
                stamp |= (unsigned long long)now[0x3cu + byte] << (8u * byte);
            }
            stamp += 8ull;
            const unsigned long long stamp_end = stamp + 4ull;
            cli_line_text(&told, name);
            cli_line_text(&told, " differs from the last build of commit ");
            cli_line_text(&told, short_commit);
            cli_line_text(&told, " in ");
            cli_line_decimal(&told, found.count, 1u);
            cli_line_text(&told, " bytes, first at ");
            int stamped = 0;
            for (unsigned int at = 0u; at < found.ranges; at += 1u)
            {
                const int field = stamped_file && (stamp < found.end[at]) && (found.start[at] < stamp_end);
                stamped = stamped || field;
                cli_line_text(&told, (at != 0u) ? ", 0x" : "0x");
                cli_line_hex(&told, found.start[at], 1u);
                cli_line_text(&told, " to 0x");
                cli_line_hex(&told, found.end[at], 1u);
                cli_line_text(&told, field ? " (the header's time stamp)" : "");
            }
            // MSVC's linker writes its Rich header between the DOS stub and the PE header
            int msvc = 0;
            for (unsigned long long at = 0x80ull;
                 stamped_file && !msvc && ((at + 4ull) <= stamp) && ((at + 4ull) <= now_size); at += 1ull)
            {
                msvc = memor.cmp(&(MemoriaCfg){.src = now + at, .other = "Rich", .bytes = 4u}) == 0;
            }
            if (msvc && stamped)
            {
                cli_line_text(&told,
                              "; MSVC's linker writes the time and a new id into each build, and its /Brepro flag "
                              "keeps them out: -C link-arg=/Brepro in the tree's rustflags, or the flag among a C "
                              "or C++ build's linker flags");
            }
        }
        cli_line_closed(&told);
        cli_say(say, said);
        qry_member_file_unmapped(old, old_size);
    }
    query_removed(folder);
    if (query_folder_made(kept_folder))
    {
        const CliFilePiece piece = {now, now_size};
        cli_file_pieces_written(kept, &piece, 1u);
    }
    qry_member_file_unmapped(now, now_size);
}

// each `NAME=value` line of a tree's .env handed to `each`, in order: a leading `export` taken off, a value in quotes
// taken without them, and comments and blank lines passed over
static void cli_dot_env(const char *text, void (*each)(void *context, const char *name, const char *value),
                        void *context)
{
    for (const char *line = text; line[0] != '\0';)
    {
        const char *stop = line;
        while ((stop[0] != '\0') && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        cli_scan_trim(&from, &length);
        line = (stop[0] != '\0') ? (stop + 1) : stop;
        if ((length == 0ull) || (from[0] == '#'))
        {
            continue;
        }
        if ((length >= 7ull) && (memor.cmp(&(MemoriaCfg){.src = from, .other = "export ", .bytes = 7u}) == 0))
        {
            from += 7;
            length -= 7ull;
        }
        unsigned long long equals = 0ull;
        while ((equals < length) && (from[equals] != '='))
        {
            equals += 1ull;
        }
        if (equals == length)
        {
            continue;
        }
        const char *name = from;
        unsigned long long name_length = equals;
        cli_scan_trim(&name, &name_length);
        const char *value = from + equals + 1;
        unsigned long long value_length = length - equals - 1ull;
        cli_scan_trim(&value, &value_length);
        int named = name_length != 0ull;
        for (unsigned long long at = 0ull; named && (at < name_length); at += 1ull)
        {
            const char one = name[at];
            named = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) ||
                    ((one >= '0') && (one <= '9')) || (one == '_') || ((unsigned char)one >= 0x80u);
        }
        if (!named)
        {
            continue;
        }
        char kept_name[CLI_NAME_LONGEST];
        char kept_value[CLI_PATH_LONGEST];
        cli_scan_copied(name, name_length, kept_name, sizeof(kept_name));
        if ((value_length > 0ull) && ((value[0] == '"') || (value[0] == '\'')))
        {
            unsigned long long end = 1ull;
            while ((end < value_length) && (value[end] != value[0]))
            {
                end += 1ull;
            }
            cli_scan_copied(value + 1, end - 1ull, kept_value, sizeof(kept_value));
        }
        else
        {
            // a bare value up to ` #`, trimmed
            unsigned long long end = 0ull;
            while (((end + 1ull) < value_length) && !((value[end] == ' ') && (value[end + 1ull] == '#')))
            {
                end += 1ull;
            }
            end = ((end + 1ull) < value_length) ? end : value_length;
            const char *bare = value;
            cli_scan_trim(&bare, &end);
            cli_scan_copied(bare, end, kept_value, sizeof(kept_value));
        }
        each(context, kept_name, kept_value);
    }
}

// A command being made with the tree's .env: the command, and the line shown for it
typedef struct
{
    CliCommand *command;
    CliLine *shown;
} CliEnvMade;

static void cli_env_each(void *context, const char *name, const char *value)
{
    CliEnvMade *const made = (CliEnvMade *)context;
    cli_command_set(made->command, name, value);
    cli_line_text(made->shown, (made->shown->at != 0ull) ? " " : "");
    cli_line_text(made->shown, name);
    // the value not shown, an ellipsis in its place
    cli_line_text(made->shown, "=\xe2\x80\xa6");
}

void cli_executables_run_command(const char *root, const char *file, const CliValues *values, int system,
                                 CliCommand *command, CliLine *shown)
{
    static char s_env[CLI_ENV_BYTES];
    static char s_folders[CLI_PATH_FOLDERS][CLI_PATH_LONGEST];
    static char s_path[CLI_PATH_FOLDERS * 64u];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_command_begun(command, file);
    const char *const folder = cli_first(values, "folder", "");
    char full[CLI_PATH_LONGEST];
    if (folder[0] == '\0')
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_full(root, folder, full);
    }
    cli_command_folder(command, full);
    if (folder[0] != '\0')
    {
        cli_line_text(shown, "cd ");
        cli_line_text(shown, folder);
        cli_line_text(shown, " &&");
    }
    cli_lock_take(&s_lock);
    char env[CLI_PATH_LONGEST];
    if (cli_executables_joined(root, ".env", env) && cli_text_read(env, s_env, sizeof(s_env)))
    {
        CliEnvMade made = {command, shown};
        cli_dot_env(s_env, cli_env_each, &made);
    }
    if (system)
    {
        const unsigned int count = cli_toolchains_system_path(s_folders, CLI_PATH_FOLDERS);
        cli_path_joined((const char (*)[CLI_PATH_LONGEST])s_folders, count, s_path, sizeof(s_path));
        cli_command_set(command, "PATH", s_path);
        cli_line_text(shown, (shown->at != 0ull) ? " " : "");
        cli_line_text(shown, "PATH=<the system's PATH>");
    }
    else
    {
        char config[CLI_PATH_LONGEST];
        const char *const name =
            (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') != NULL)
                ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/') + 1)
            : (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') != NULL)
                ? (cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '\\') + 1)
                : file;
        const char *const dot =
            (name[0] != '\0')
                ? cli_text_last(name + 1, cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}), '.')
                : NULL;
        CliLine named = {config, sizeof(config), 0ull};
        cli_line_counted(&named, file,
                         (dot != NULL) ? (unsigned long long)(dot - file)
                                       : cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
        cli_line_text(&named, ".runtimeconfig.json");
        char dotnet[CLI_PATH_LONGEST];
        char dotnet_folder[CLI_PATH_LONGEST];
        char why[CLI_NAME_LONGEST];
        CliLine unfound = {why, sizeof(why), 0ull};
        if ((cli_line_closed(&named) != 0ull) && query_is_file(config) &&
            cli_executables_program("dotnet", "dotnet", dotnet, &unfound) && cli_path_parent(dotnet, dotnet_folder))
        {
            cli_line_text(shown, (shown->at != 0ull) ? " " : "");
            cli_line_text(shown, "DOTNET_ROOT=");
            cli_line_text(shown, dotnet_folder);
            cli_command_set(command, "DOTNET_ROOT", dotnet_folder);
        }
    }
    cli_lock_give(&s_lock);
    char relative[CLI_PATH_LONGEST];
    const unsigned long long root_length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    const int under = cli_text_same(file, root, root_length) &&
                      ((file[root_length] == '/') || (file[root_length] == '\\') || (file[root_length] == '\0'));
    if (under)
    {
        cli_relative(root, file, relative);
    }
    cli_line_text(shown, (shown->at != 0ull) ? " " : "");
    cli_line_text(shown, under ? relative : file);
}
