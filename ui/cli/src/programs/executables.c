// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables.c: a toolchain's program found where the toolchains window finds the tool, a tool's output read, and the
// file a job's build made read back from its id and the lines its build printed
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "executables_internal.h"

int cli_executables_program(const char *tool, const char *name, char *path, CliLine *said)
{
    // the manifest's tree and the PATH's folders each a cell of the search's, and the program found
    CliFound found_tool;
    CliJson json;
    char why[CLI_NAME_LONGEST];
    CliLine unknown = {why, sizeof(why), 0ull};
    const unsigned int one = cli_toolchains_tool(&json, cli_toolchains_manifest(&json, 0u, 0ull), tool, &unknown);
    if (one == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        cli_line_text(said, "no toolchain ");
        cli_line_text(said, tool);
        return 0;
    }
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_LONGEST] = cli_toolchains_path_taken(&count);
    cli_toolchains_find(&json, one, (const char (*)[CLI_PATH_LONGEST])folders, count, &found_tool);
    cli_cell_given(folders);
    char folder[CLI_PATH_LONGEST];
    const char *const names[1] = {name};
    const int found = found_tool.has_program && cli_path_parent(found_tool.program, folder) &&
                      cli_toolchains_program_in(folder, names, 1u, path);
    if (!found)
    {
        const char *const shown = cli_json_string(&json, cli_json_member(&json, one, "name"), NULL);
        cli_line_text(said, (shown != NULL) ? shown : tool);
        cli_line_text(said,
                      " builds it, and is not found: File, Toolchains opens its install page or takes its folder");
    }
    cli_json_given(&json);
    return found;
}

int cli_executables_output(const char *tool, const char *name, const char *const *args, unsigned int count,
                           const char *folder, CliRan *ran, CliLine *said)
{
    char program[CLI_PATH_LONGEST];
    if (!cli_executables_program(tool, name, program, said))
    {
        return 0;
    }
    const char *words[16] = {program};
    for (unsigned int at = 0u; (at < count) && (at < 14u); at += 1u)
    {
        words[at + 1u] = args[at];
    }
    words[((count < 14u) ? count : 14u) + 1u] = NULL;
    char why[CLI_PATH_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    const int started = cli_ran(words, folder, NULL, 0u, ran, &unstarted);
    if (!started || (ran->code != 0ll))
    {
        cli_line_text(said, name);
        cli_line_character(said, ' ');
        cli_line_text(said, (count != 0u) ? args[0] : "");
        cli_line_text(said, ": ");
        const char *from = started ? ran->err : why;
        const char *to = started ? (ran->err + ran->err_size) : (why + unstarted.at);
        while ((from < to) && cli_scan_white(from[0]))
        {
            from += 1;
        }
        while ((to > from) && cli_scan_white(to[-1]))
        {
            to -= 1;
        }
        cli_line_counted(said, from, (unsigned long long)(to - from));
        return 0;
    }
    return 1;
}

// the folder Cargo builds the package of `manifest` into, as `cargo metadata` run in the package's folder says, where
// Cargo reads the package's own settings as its build does, into `out`, which holds CLI_PATH_LONGEST: 1, or 0 with why
// said
static int cli_cargo_target(const char *manifest, char *out, CliLine *said)
{
    char cargo[CLI_PATH_LONGEST];
    if (!cli_executables_program("rust", "cargo", cargo, said))
    {
        return 0;
    }
    const char *const words[9] = {
        cargo, "metadata", "--no-deps", "--format-version", "1", "--offline", "--manifest-path", manifest, NULL};
    char folder[CLI_PATH_LONGEST];
    const int has_folder = cli_path_parent(manifest, folder);
    CliRan *const ran = cli_ran_taken();
    char why[CLI_PATH_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    const int started = cli_ran(words, has_folder ? folder : NULL, NULL, 0u, ran, &unstarted);
    if (!started || (ran->code != 0ll))
    {
        const char *from = started ? ran->err : why;
        const char *to = started ? (ran->err + ran->err_size) : (why + unstarted.at);
        while ((from < to) && cli_scan_white(from[0]))
        {
            from += 1;
        }
        while ((to > from) && cli_scan_white(to[-1]))
        {
            to -= 1;
        }
        cli_line_text(said, "cargo metadata: ");
        cli_line_counted(said, from, (unsigned long long)(to - from));
        cli_ran_given(ran);
        return 0;
    }
    // Cargo's answer read into a tree sized to it
    CliJson json;
    char reason[CLI_NAME_LONGEST];
    CliLine unread = {reason, sizeof(reason), 0ull};
    const unsigned int root = cli_json_taken(&json, ran->out, ran->out_size, 0u, 0ull, &unread);
    cli_ran_given(ran);
    if (root == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        cli_line_text(said, "cargo metadata: ");
        cli_line_counted(said, reason, unread.at);
        return 0;
    }
    const char *const target = cli_json_string(&json, cli_json_member(&json, root, "target_directory"), NULL);
    if (target == NULL)
    {
        cli_json_given(&json);
        cli_line_text(said, "cargo metadata names no target_directory");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = target, .bytes = cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}) + 1u});
    cli_json_given(&json);
    return 1;
}

// the program .NET's build made, from the last line it printed for the project, into `out`, which holds
// CLI_PATH_LONGEST: a folder it published to holds the program, and an assembly has the program beside it. 1, or 0
// with why said
int cli_dotnet_artifact(const char *name, const CliLines *lines, char *out, CliLine *said)
{
    char mark[CLI_NAME_LONGEST + 4u];
    CliLine marked = {mark, sizeof(mark), 0ull};
    cli_line_text(&marked, name);
    cli_line_text(&marked, " -> ");
    cli_line_closed(&marked);
    const unsigned long long mark_length = cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
    for (unsigned int index = lines->count; index > 0u; index -= 1u)
    {
        const char *from = cli_lines_at(lines, index - 1u);
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
        cli_scan_trim(&from, &length);
        if ((length < mark_length) || (memor.cmp(&(MemoriaCfg){.src = from, .other = mark, .bytes = mark_length}) != 0))
        {
            continue;
        }
        const char *path = from + mark_length;
        unsigned long long size = length - mark_length;
        cli_scan_trim(&path, &size);
        char file[CLI_NAME_LONGEST];
        cli_program_file(name, file);
        CliLine line = {out, CLI_PATH_LONGEST, 0ull};
        cli_line_counted(&line, path, size);
        cli_line_closed(&line);
        const int folder =
            ((size > 0ull) && ((path[size - 1ull] == '/') || (path[size - 1ull] == '\\'))) || query_is_folder(out);
        const int assembly = (size > 4ull) && (path[size - 4ull] == '.') && ((path[size - 3ull] | 0x20) == 'd') &&
                             ((path[size - 2ull] | 0x20) == 'l') && ((path[size - 1ull] | 0x20) == 'l');
        if (folder || assembly)
        {
            char held[CLI_PATH_LONGEST];
            if (folder)
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = held,
                                  .src = out,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL}) + 1u});
            }
            else if (!cli_path_parent(out, held))
            {
                held[0] = '\0';
            }
            CliLine joined = {out, CLI_PATH_LONGEST, 0ull};
            cli_line_text(&joined, held);
            const unsigned long long held_length = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL});
            const int slashed =
                (held_length > 0ull) && ((held[held_length - 1ull] == '/') || (held[held_length - 1ull] == '\\'));
            cli_line_text(&joined, ((held_length > 0ull) && !slashed) ? "/" : "");
            cli_line_text(&joined, file);
            cli_line_closed(&joined);
        }
        return 1;
    }
    cli_line_text(said, "dotnet named no file for ");
    cli_line_text(said, name);
    return 0;
}

// `parts`, `count` of them, joined by `/` into `out`, which holds CLI_PATH_LONGEST
static void cli_parts(const char *const *parts, unsigned int count, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&line, (at != 0u) ? "/" : "");
        cli_line_text(&line, parts[at]);
    }
    cli_line_closed(&line);
}

int cli_executables_made(const char *root, const char *id, const CliValues *values, const CliLines *lines, char *path,
                         CliLine *said)
{
    const char *const colon = cli_text_last(id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}), ':');
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = id, .other = "executable/", .cap = CLI_CELLS_POOL})) ||
        (colon == NULL) || (colon < (id + 11)))
    {
        cli_line_text(said, id);
        cli_line_text(said, " is no executable's job");
        return 0;
    }
    char script[CLI_PATH_LONGEST];
    CliLine scripted = {script, sizeof(script), 0ull};
    cli_line_counted(&scripted, id + 11, (unsigned long long)(colon - (id + 11)));
    cli_line_closed(&scripted);
    const char *const name = colon + 1;
    char full[CLI_PATH_LONGEST];
    const char *const halves[2] = {root, script};
    cli_parts(halves, 2u, full);
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(full, folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    const char *base = full;
    for (const char *at = full; at[0] != '\0'; at += 1)
    {
        base = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : base;
    }
    const unsigned int kind = cli_executables_kind_of(base);
    if (kind == CLI_KIND_NONE)
    {
        cli_line_text(said, script);
        cli_line_text(said, " is no build script");
        return 0;
    }
    char file[CLI_NAME_LONGEST];
    cli_program_file(name, file);
    int found = 1;
    if (kind == CLI_KIND_CARGO)
    {
        const char *const profile = cli_first(values, "profile", "dev");
        const char *const profile_folder =
            (cellul.eq(&(CatenaFinitaCfg){.src = profile, .other = "dev", .cap = CLI_CELLS_POOL})) ? "debug" : profile;
        const char *const target = cli_first(values, "target", "this machine");
        char built[CLI_PATH_LONGEST] = "";
        found = cli_cargo_target(full, built, said);
        if (cellul.eq(&(CatenaFinitaCfg){.src = target, .other = "this machine", .cap = CLI_CELLS_POOL}))
        {
            const char *const parts[3] = {built, profile_folder, file};
            cli_parts(parts, 3u, path);
        }
        else
        {
            char named[CLI_NAME_LONGEST];
            CliLine line = {named, sizeof(named), 0ull};
            cli_line_text(&line, name);
            cli_line_text(
                &line,
                (cellul.find(&(CatenaFinitaCfg){
                     .src = target, .cap = CLI_CELLS_POOL, .other = "windows", .other_cap = CLI_CELLS_POOL}) != NULL)
                    ? ".exe"
                    : "");
            cli_line_closed(&line);
            const char *const parts[4] = {built, target, profile_folder, named};
            cli_parts(parts, 4u, path);
        }
    }
    else if ((kind == CLI_KIND_CMAKE) || (kind == CLI_KIND_MESON))
    {
        const int cmake = kind == CLI_KIND_CMAKE;
        const char *const config =
            cmake ? cli_first(values, "config", "Debug") : cli_first(values, "buildtype", "debug");
        char relative[CLI_PATH_LONGEST];
        cli_build_folder(root, full, cmake ? "cmake" : "meson", relative);
        char build[CLI_PATH_LONGEST];
        CliLine line = {build, sizeof(build), 0ull};
        cli_line_text(&line, root);
        cli_line_character(&line, '/');
        cli_line_text(&line, relative);
        cli_line_text(&line, config);
        cli_line_closed(&line);
        found =
            cmake ? cli_cmake_artifact(build, name, config, path, said) : cli_meson_artifact(build, name, path, said);
    }
    else if (kind == CLI_KIND_GO)
    {
        const char *host_system = NULL;
        const char *host_processor = NULL;
        cli_go_host(&host_system, &host_processor);
        const char *const system = cli_first(values, "GOOS", host_system);
        char pair[CLI_NAME_LONGEST];
        CliLine line = {pair, sizeof(pair), 0ull};
        cli_line_text(&line, system);
        cli_line_character(&line, '-');
        cli_line_text(&line, cli_first(values, "GOARCH", host_processor));
        cli_line_closed(&line);
        char named[CLI_NAME_LONGEST];
        CliLine program = {named, sizeof(named), 0ull};
        cli_line_text(&program, name);
        cli_line_text(&program,
                      (cellul.eq(&(CatenaFinitaCfg){.src = system, .other = "windows", .cap = CLI_CELLS_POOL})) ? ".exe"
                                                                                                                : "");
        cli_line_closed(&program);
        const char *const parts[5] = {root, "build", "exe", pair, named};
        cli_parts(parts, 5u, path);
    }
    else if (kind == CLI_KIND_NPM)
    {
        const char *const parts[4] = {root, "build", "exe", file};
        cli_parts(parts, 4u, path);
    }
    else if (kind == CLI_KIND_DOTNET)
    {
        found = cli_dotnet_artifact(name, lines, path, said);
    }
    else if (kind == CLI_KIND_ZIG)
    {
        const char *const parts[4] = {folder, "zig-out", "bin", file};
        cli_parts(parts, 4u, path);
    }
    else
    {
        char exe[CLI_NAME_LONGEST];
        char under[CLI_PATH_LONGEST];
        // the spec read, a cell of its size taken for the reading
        char *const spec = cli_text_taken(full);
        if (spec == NULL)
        {
            cli_line_text(said, script);
            cli_line_text(said, ": it could not be read");
            return 0;
        }
        found = cli_spec_made(spec, exe, under);
        cli_cell_given_by(&g_cli_executables_cells, spec);
        if (!found)
        {
            cli_line_text(said, script);
            cli_line_text(said, " is no PyInstaller spec");
            return 0;
        }
        const char *const parts[2] = {folder, under};
        cli_parts(parts, 2u, path);
    }
    if (found && !query_is_file(path))
    {
        cli_line_text(said, "the build made no ");
        cli_line_text(said, path);
        return 0;
    }
    return found;
}
