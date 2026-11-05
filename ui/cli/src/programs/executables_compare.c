// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_compare.c: the program a build made compared with the last build of the same commit, and the command
// that runs it, in the folder the job names, with the variables of the tree's .env
#include "../code/git.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "executables_internal.h"

// the most ranges of difference reported, and the most bytes of a tree's .env read
#define CLI_COMPARE_RANGES 3u
// The places two builds of a program differ, as ranges of bytes, the first CLI_COMPARE_RANGES of them, and the count
// of bytes that differ; files of two lengths differ from the shorter one's end as well
typedef struct
{
    unsigned long long start[CLI_COMPARE_RANGES];
    unsigned long long end[CLI_COMPARE_RANGES];
    unsigned int ranges;
    unsigned long long count;
} CliDifferences;

#define CLI_ENV_BYTES (1u << 16u)

typedef struct
{
    QuerySink sink;
    const char *text;
    CliDifferences *found;
    unsigned long long start;
    unsigned long long end;
    const unsigned char *old;
    unsigned long long old_size;
    const unsigned char *now;
    unsigned long long now_size;
    void (*each)(const CliEachNameValueCfg *args);
    void *context;
} ExecutablesCompareCtx;
// a line printed through `sink`
EMBED_INLINE void cli_print(const ExecutablesCompareCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}));
}

// a range recorded where there is capacity for it
EMBED_INLINE void cli_range(const ExecutablesCompareCtx *args)
{
    if (args->found->ranges < CLI_COMPARE_RANGES)
    {
        args->found->start[args->found->ranges] = args->start;
        args->found->end[args->found->ranges] = args->end;
        args->found->ranges += 1u;
    }
}

EMBED_INLINE void cli_differences(const ExecutablesCompareCtx *args)
{
    args->found->ranges = 0u;
    args->found->count = 0ULL;
    const unsigned long long shared = (args->old_size < args->now_size) ? args->old_size : args->now_size;
    int open = 0;
    unsigned long long start = 0ULL;
    for (unsigned long long at = 0ULL; at < shared; at += 1ULL)
    {
        if (args->old[at] != args->now[at])
        {
            args->found->count += 1ULL;
            start = open ? start : at;
            open = 1;
        }
        else if (open)
        {
            EMBED_CALL(cli_range, ExecutablesCompareCtx, .found = args->found, .start = start, .end = at);
            open = 0;
        }
    }
    if (open)
    {
        EMBED_CALL(cli_range, ExecutablesCompareCtx, .found = args->found, .start = start, .end = shared);
    }
    if (args->old_size != args->now_size)
    {
        const unsigned long long longest = (args->old_size > args->now_size) ? args->old_size : args->now_size;
        args->found->count += longest - shared;
        EMBED_CALL(cli_range, ExecutablesCompareCtx, .found = args->found, .start = shared, .end = longest);
    }
}

void cli_executables_compare_build(const CliExecutablesCfg *args)
{
    char commit[CLI_NAME_MAX_LEN];
    long long when = 0LL;
    int clean = 0;
    if (!EMBED_CALL(cli_git_root_commit.head, CliGitRootCommitCfg, .root = args->root, .commit = commit, .when = &when,
                    .clean = &clean) ||
        !clean)
    {
        return;
    }
    // the job's id with every letter but a letter, a digit, `.` and `-` made `_`
    char slug[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = slug,
                            .src = args->id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_text_size.save, CliTextSizeCfg, .text_out = slug,
               .size = cellul.len(&(CatenaFinitaCfg){.src = slug, .cap = CLI_CELLS_POOL}),
               .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS | CLI_TEXT_END_ASCII, .set = ".-", .other = '_');
    char folder[CLI_PATH_MAX_LEN];
    char folder_buf[CLI_PATH_MAX_LEN];
    char allowed[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ULL};
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "/build/orior/builds/");
    mmgr_line_put(&line, slug);
    const char *name;
    if (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                   .byte = '/') != NULL)
    {
        name = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '/') +
               1;
    }
    else if (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                        .byte = '\\') != NULL)
    {
        name = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                          .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                          .byte = '\\') +
               1;
    }
    else
    {
        name = args->file;
    }
    if ((mmgr_line_finish(&line) == 0ULL) ||
        !EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = folder,
                    .name = commit, .out = folder_buf) ||
        !EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = folder_buf,
                    .name = name, .out = allowed))
    {
        return;
    }
    unsigned long long now_size = 0ULL;
    const unsigned char *const now = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->file,
                                                .size_pointer = &now_size, .owner = &g_cli_executables_cells);
    if (now == NULL)
    {
        return;
    }
    unsigned long long old_size = 0ULL;
    const unsigned char *const old = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = allowed,
                                                .size_pointer = &old_size, .owner = &g_cli_executables_cells);
    if (old != NULL)
    {
        char print_buf[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ULL};
        char short_commit[16];
        VerbaLinea shortened = {short_commit, sizeof(short_commit), 0ULL};
        mmgr_line_put_n(&shortened, commit,
                        (cellul.len(&(CatenaFinitaCfg){.src = commit, .cap = CLI_CELLS_POOL}) < 10u)
                            ? cellul.len(&(CatenaFinitaCfg){.src = commit, .cap = CLI_CELLS_POOL})
                            : 10u);
        mmgr_line_finish(&shortened);
        CliDifferences found;
        EMBED_CALL(cli_differences, ExecutablesCompareCtx, .old = old, .old_size = old_size, .now = now,
                   .now_size = now_size, .found = &found);
        if (found.count == 0ULL)
        {
            mmgr_line_put(&print_line, name);
            mmgr_line_put(&print_line, " is the same, byte for byte, as the last build of commit ");
            mmgr_line_put(&print_line, short_commit);
        }
        else
        {
            // a Windows program's header's time stamp: four bytes, eight past the PE header
            int stamped_file = (now_size >= 0x40ULL) && (now[0] == 'M') && (now[1] == 'Z');
            unsigned long long stamp = 0ULL;
            for (unsigned int byte = 0u; stamped_file && (byte < 4u); byte += 1u)
            {
                stamp |= (unsigned long long)now[0x3cu + byte] << (8u * byte);
            }
            stamp += 8ULL;
            const unsigned long long stamp_end = stamp + 4ULL;
            mmgr_line_put(&print_line, name);
            mmgr_line_put(&print_line, " differs from the last build of commit ");
            mmgr_line_put(&print_line, short_commit);
            mmgr_line_put(&print_line, " in ");
            mmgr_line_decimal(&print_line, found.count, 1u);
            mmgr_line_put(&print_line, " bytes, first at ");
            int stamped = 0;
            for (unsigned int at = 0u; at < found.ranges; at += 1u)
            {
                const int field = stamped_file && (stamp < found.end[at]) && (found.start[at] < stamp_end);
                stamped = stamped || field;
                mmgr_line_put(&print_line, (at != 0u) ? ", 0x" : "0x");
                mmgr_line_hex(&print_line, found.start[at], 1u);
                mmgr_line_put(&print_line, " to 0x");
                mmgr_line_hex(&print_line, found.end[at], 1u);
                mmgr_line_put(&print_line, field ? " (the header's time stamp)" : "");
            }
            // MSVC's linker writes its Rich header between the DOS stub and the PE header
            int msvc = 0;
            {
                unsigned long long at = 0x80ULL;
                while (stamped_file && !msvc && ((at + 4ULL) <= stamp) && ((at + 4ULL) <= now_size))
                {
                    msvc = memor.cmp(&(MemoriaCfg){.src = now + at, .other = "Rich", .bytes = 4u}) == 0;
                    at += 1ULL;
                }
            }
            if (msvc && stamped)
            {
                mmgr_line_put(&print_line,
                              "; MSVC's linker writes the time and a new id into each build, and its /Brepro flag "
                              "leaves them out: -C link-arg=/Brepro in the tree's rustflags, or the flag among a C "
                              "or C++ build's linker flags");
            }
        }
        mmgr_line_finish(&print_line);
        EMBED_CALL(cli_print, ExecutablesCompareCtx, .sink = args->sink, .text = print_buf);
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)old, (&g_cli_executables_cells));
    }
    EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = folder);
    if (EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder_buf))
    {
        const CliFilePiece piece = {now, now_size};
        EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = allowed, .pieces = &piece,
                   .count = 1u);
    }
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)now, (&g_cli_executables_cells));
}

// each `NAME=value` line of a tree's .env handed to `each`, in order: a leading `export` taken off, a value in quotes
// taken without them, and comments and blank lines passed over
EMBED_INLINE void cli_dot_env(const ExecutablesCompareCtx *args)
{
    const char *text = args->text;
    for (const char *line = text; line[0] != '\0';)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *from = line;
        unsigned long long length = (unsigned long long)(stop - line);
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        line = (stop[0] != '\0') ? (stop + 1) : stop;
        if ((length == 0ULL) || (from[0] == '#'))
        {
            continue;
        }
        if ((length >= 7ULL) && (memor.cmp(&(MemoriaCfg){.src = from, .other = "export ", .bytes = 7u}) == 0))
        {
            from += 7;
            length -= 7ULL;
        }
        unsigned long long equals = 0ULL;
        equals = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = from, .at = equals,
                            .size = length, .set = "=");
        if (equals == length)
        {
            continue;
        }
        const char *name = from;
        unsigned long long name_length = equals;
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &name,
                   .length_pointer = &name_length);
        const char *value = from + equals + 1;
        unsigned long long value_length = length - equals - 1ULL;
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &value,
                   .length_pointer = &value_length);
        int named = name_length != 0ULL;
        {
            unsigned long long at = 0ULL;
            while (named && (at < name_length))
            {
                const char one = name[at];
                named = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) ||
                        ((one >= '0') && (one <= '9')) || (one == '_') || ((unsigned char)one >= 0x80u);
                at += 1ULL;
            }
        }
        if (!named)
        {
            continue;
        }
        char name_buf[CLI_NAME_MAX_LEN];
        char value_buf[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = name,
                   .length = name_length, .out = name_buf, .capacity = sizeof(name_buf));
        if ((value_length > 0ULL) && ((value[0] == '"') || (value[0] == '\'')))
        {
            unsigned long long end = 1ULL;
            while ((end < value_length) && (value[end] != value[0]))
            {
                end += 1ULL;
            }
            EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = value + 1,
                       .length = end - 1ULL, .out = value_buf, .capacity = sizeof(value_buf));
        }
        else
        {
            // a bare value up to ` #`, trimmed
            unsigned long long end = 0ULL;
            end = EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = value, .at = 0ULL,
                             .size = value_length, .first = ' ', .second = '#');
            end = ((end + 1ULL) < value_length) ? end : value_length;
            const char *bare = value;
            EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx,
                       .text_pointer = &bare, .length_pointer = &end);
            EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = bare,
                       .length = end, .out = value_buf, .capacity = sizeof(value_buf));
        }
        EMBED_CALL(args->each, CliEachNameValueCfg, .context = args->context, .name = name_buf, .value = value_buf);
    }
}

// A command being made with the tree's .env: the command, and the line shown for it
typedef struct
{
    CliCommand *command;
    VerbaLinea *displayed;
} CliEnvCreated;

static void cli_env_each(const CliEachNameValueCfg *args)
{
    const char *name = args->name;
    const char *value = args->value;
    CliEnvCreated *const made = (CliEnvCreated *)args->context;
    EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = made->command, .name = name,
               .value = value);
    mmgr_line_put(made->displayed, (made->displayed->at != 0ULL) ? " " : "");
    mmgr_line_put(made->displayed, name);
    // the value not shown, an ellipsis in its place
    mmgr_line_put(made->displayed, "=\xe2\x80\xa6");
}

void cli_executables_run_command(const CliExecutablesValuesCfg *args)
{
    CliCommand *command = args->command;
    static char s_env[CLI_ENV_BYTES];
    static char s_folders[CLI_PATH_FOLDERS][CLI_PATH_MAX_LEN];
    static char s_path[CLI_PATH_FOLDERS * 64u];
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = command, .program = args->file);
    const char *const folder = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                          .values = args->values, .key = "folder", .fallback = "");
    char full[CLI_PATH_MAX_LEN];
    if (folder[0] == '\0')
    {
        memor.cpy(
            &(MemoriaCfg){.dst = full,
                          .src = args->root,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = folder, .joined = full);
    }
    EMBED_CALL(cli_runner_command_folder.command_folder, CliRunnerCommandFolderCfg, .command = command, .folder = full);
    if (folder[0] != '\0')
    {
        mmgr_line_put(args->displayed_verba_linea_pointer, "cd ");
        mmgr_line_put(args->displayed_verba_linea_pointer, folder);
        mmgr_line_put(args->displayed_verba_linea_pointer, " &&");
    }
    char env[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = args->root,
                   .name = ".env", .out = env) &&
        EMBED_CALL(cli_executables_capacity_internal.text_read, CliExecutablesCapacityCtx, .path = env, .bytes = s_env,
                   .capacity = sizeof(s_env)))
    {
        CliEnvCreated made = {command, args->displayed_verba_linea_pointer};
        EMBED_CALL(cli_dot_env, ExecutablesCompareCtx, .text = s_env, .each = cli_env_each, .context = &made);
    }
    if (args->system)
    {
        const unsigned int count = EMBED_CALL(cli_toolchains_count.system_path, CliToolchainsCountCfg,
                                              .folders = s_folders, .capacity = CLI_PATH_FOLDERS);
        EMBED_CALL(cli_path_count_size_internal.path_join, CliPathCountSizeCtx,
                   .folders_callback = (const char (*)[CLI_PATH_MAX_LEN])s_folders, .count = count, .path_out = s_path,
                   .size = sizeof(s_path));
        EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command, .name = "PATH",
                   .value = s_path);
        mmgr_line_put(args->displayed_verba_linea_pointer,
                      (args->displayed_verba_linea_pointer->at != 0ULL) ? " " : "");
        mmgr_line_put(args->displayed_verba_linea_pointer, "PATH=<the system's PATH>");
    }
    else
    {
        char config[CLI_PATH_MAX_LEN];
        const char *name;
        if (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                       .byte = '/') != NULL)
        {
            name = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                              .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                              .byte = '/') +
                   1;
        }
        else if (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                            .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                            .byte = '\\') != NULL)
        {
            name = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->file,
                              .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}),
                              .byte = '\\') +
                   1;
        }
        else
        {
            name = args->file;
        }
        const char *const dot =
            (name[0] != '\0')
                ? EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name + 1,
                             .length_value = cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}),
                             .byte = '.')
                : NULL;
        VerbaLinea named = {config, sizeof(config), 0ULL};
        mmgr_line_put_n(&named, args->file,
                        (dot != NULL) ? (unsigned long long)(dot - args->file)
                                      : cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(&named, ".runtimeconfig.json");
        char dotnet[CLI_PATH_MAX_LEN];
        char dotnet_folder[CLI_PATH_MAX_LEN];
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea unfound = {why, sizeof(why), 0ULL};
        if ((mmgr_line_finish(&named) != 0ULL) &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = config) &&
            EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "dotnet", .name = "dotnet",
                       .path = dotnet, .err_msg = &unfound) &&
            EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = dotnet, .out = dotnet_folder))
        {
            mmgr_line_put(args->displayed_verba_linea_pointer,
                          (args->displayed_verba_linea_pointer->at != 0ULL) ? " " : "");
            mmgr_line_put(args->displayed_verba_linea_pointer, "DOTNET_ROOT=");
            mmgr_line_put(args->displayed_verba_linea_pointer, dotnet_folder);
            EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command,
                       .name = "DOTNET_ROOT", .value = dotnet_folder);
        }
    }
    char relative[CLI_PATH_MAX_LEN];
    const unsigned long long root_length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    const int under =
        EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->file, .other_pointer = args->root,
                   .size = root_length) &&
        ((args->file[root_length] == '/') || (args->file[root_length] == '\\') || (args->file[root_length] == '\0'));
    if (under)
    {
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = args->file,
                   .relative_out = relative);
    }
    mmgr_line_put(args->displayed_verba_linea_pointer, (args->displayed_verba_linea_pointer->at != 0ULL) ? " " : "");
    mmgr_line_put(args->displayed_verba_linea_pointer, under ? relative : args->file);
}
