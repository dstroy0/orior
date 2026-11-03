// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables.c: a toolchain's program found where the toolchains window finds the tool, a tool's output read, and the
// file a job's build made read back from its id and the lines its build printed
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "executables_internal.h"

int cli_executables_program(const CliExecutablesToolCfg *args)
{
    char *path = args->path;
    VerbaLinea *err_msg = args->err_msg;
    // the manifest's tree and the PATH's folders each a cell of the search's, and the program found
    CliFound found_tool;
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unknown = {why, sizeof(why), 0ULL};
    const unsigned int one =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &json, .nodes_more = 0u, .text_more = 0ULL),
                   .id = args->tool, .err_msg = &unknown);
    if (one == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        mmgr_line_put(err_msg, "no toolchain ");
        mmgr_line_put(err_msg, args->tool);
        return 0;
    }
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&count, &g_cli_executables_cells);
    EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = one,
               .path_callback = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count, .found = &found_tool);
    g_cli_cells->persistent_buf_release(folders, &g_cli_executables_cells);
    char folder[CLI_PATH_MAX_LEN];
    const char *const names[1] = {args->name};
    const int found = found_tool.has_program &&
                      EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = found_tool.program,
                                 .out = folder) &&
                      EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folder, .names = names,
                                 .count = 1u, .path = path);
    if (!found)
    {
        const char *const displayed =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = one, .name = "name"),
                       .length_pointer = NULL);
        mmgr_line_put(err_msg, (displayed != NULL) ? displayed : args->tool);
        mmgr_line_put(err_msg,
                      " builds it, and is not found: File, Toolchains opens its install page or takes its folder");
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return found;
}

int cli_executables_output(const CliExecutablesNameCtx *args)
{
    const char *tool = args->tool;
    const char *name = args->name;
    VerbaLinea *err_msg = args->err_msg;
    char program[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = tool, .name = name, .path = program,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const char *words[16] = {program};
    for (unsigned int at = 0u; (at < args->count) && (at < 14u); at += 1u)
    {
        words[at + 1u] = args->request[at];
    }
    words[((args->count < 14u) ? args->count : 14u) + 1u] = NULL;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    const int started =
        EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = args->folder, .settings = NULL,
                   .count = 0u, .run_result = args->run_result, .err_msg = &unstarted);
    if (!started || (args->run_result->code != 0LL))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_ch(err_msg, ' ');
        mmgr_line_put(err_msg, (args->count != 0u) ? args->request[0] : "");
        mmgr_line_put(err_msg, ": ");
        const char *from = started ? args->run_result->err : why;
        const char *to = started ? (args->run_result->err + args->run_result->err_size) : (why + unstarted.at);
        while ((from < to) && EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = from[0]))
        {
            from += 1;
        }
        while ((to > from) && EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = to[-1]))
        {
            to -= 1;
        }
        mmgr_line_put_n(err_msg, from, (unsigned long long)(to - from));
        return 0;
    }
    return 1;
}

typedef struct
{
    const char *manifest;
    char *out;
    VerbaLinea *err_msg;
    const char *const *parts;
    unsigned int count;
} ExecutablesCtx;
// the folder Cargo builds the package of `manifest` into, as `cargo metadata` run in the package's folder reports,
// where Cargo reads the package's own settings as its build does, into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 with
// why given
EMBED_INLINE int cli_cargo_target(const ExecutablesCtx *args)
{
    char *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char cargo[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "rust", .name = "cargo", .path = cargo,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const char *const words[9] = {
        cargo, "metadata", "--no-deps", "--format-version", "1", "--offline", "--manifest-path", args->manifest, NULL};
    char folder[CLI_PATH_MAX_LEN];
    const int has_folder =
        EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->manifest, .out = folder);
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    const int started =
        EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = has_folder ? folder : NULL,
                   .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &unstarted);
    if (!started || (run_result->code != 0LL))
    {
        const char *from = started ? run_result->err : why;
        const char *to = started ? (run_result->err + run_result->err_size) : (why + unstarted.at);
        while ((from < to) && EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = from[0]))
        {
            from += 1;
        }
        while ((to > from) && EMBED_CALL(cli_executables_internal.scan_white, CliExecutablesCtx, .letter = to[-1]))
        {
            to -= 1;
        }
        mmgr_line_put(err_msg, "cargo metadata: ");
        mmgr_line_put_n(err_msg, from, (unsigned long long)(to - from));
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    // Cargo's answer read into a tree sized to it
    CliJson json;
    char reason[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {reason, sizeof(reason), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = run_result->out, .size = run_result->out_size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &unread, .owner = &g_cli_executables_cells);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    if (root == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        mmgr_line_put(err_msg, "cargo metadata: ");
        mmgr_line_put_n(err_msg, reason, unread.at);
        return 0;
    }
    const char *const target =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = root, .name = "target_directory"),
                   .length_pointer = NULL);
    if (target == NULL)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        mmgr_line_put(err_msg, "cargo metadata names no target_directory");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = target, .bytes = cellul.len(&(CatenaFinitaCfg){.src = target, .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}

// the program .NET's build made, from the last line it printed for the project, into `out`, sized for
// CLI_PATH_MAX_LEN: a folder it published to contains the program, and an assembly has the program beside it. 1, or 0
// with why given
int cli_dotnet_artifact(const CliExecutablesNameOutErrMsgCtx *args)
{
    const CliLines *lines = args->lines;
    char mark[CLI_NAME_MAX_LEN + 4u];
    VerbaLinea marked = {mark, sizeof(mark), 0ULL};
    mmgr_line_put(&marked, args->name);
    mmgr_line_put(&marked, " -> ");
    mmgr_line_finish(&marked);
    const unsigned long long mark_length = cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
    for (unsigned int index = lines->count; index > 0u; index -= 1u)
    {
        const char *from = EMBED_CALL(cli_runner_lines_index.lines_at, CliRunnerLinesIndexCfg, .lines_read = lines,
                                      .index = index - 1u);
        unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &from,
                   .length_pointer = &length);
        if ((length < mark_length) || (memor.cmp(&(MemoriaCfg){.src = from, .other = mark, .bytes = mark_length}) != 0))
        {
            continue;
        }
        const char *path = from + mark_length;
        unsigned long long size = length - mark_length;
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &path,
                   .length_pointer = &size);
        char file[CLI_NAME_MAX_LEN];
        EMBED_CALL(cli_executables_name_out_folder_internal.program_file, CliExecutablesNameOutFolderCtx,
                   .name = args->name, .out = file);
        VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put_n(&line, path, size);
        mmgr_line_finish(&line);
        const int folder = ((size > 0ULL) && ((path[size - 1ULL] == '/') || (path[size - 1ULL] == '\\'))) ||
                           EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->out);
        const int assembly = (size > 4ULL) && (path[size - 4ULL] == '.') && ((path[size - 3ULL] | 0x20) == 'd') &&
                             ((path[size - 2ULL] | 0x20) == 'l') && ((path[size - 1ULL] | 0x20) == 'l');
        if (folder || assembly)
        {
            char artifact[CLI_PATH_MAX_LEN];
            if (folder)
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = artifact,
                    .src = args->out,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->out, .cap = CLI_CELLS_POOL}) + 1u});
            }
            else if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->out,
                                 .out = artifact))
            {
                artifact[0] = '\0';
            }
            VerbaLinea joined = {args->out, CLI_PATH_MAX_LEN, 0ULL};
            mmgr_line_put(&joined, artifact);
            const unsigned long long artifact_length =
                cellul.len(&(CatenaFinitaCfg){.src = artifact, .cap = CLI_CELLS_POOL});
            const int slashed = (artifact_length > 0ULL) && ((artifact[artifact_length - 1ULL] == '/') ||
                                                             (artifact[artifact_length - 1ULL] == '\\'));
            mmgr_line_put(&joined, ((artifact_length > 0ULL) && !slashed) ? "/" : "");
            mmgr_line_put(&joined, file);
            mmgr_line_finish(&joined);
        }
        return 1;
    }
    mmgr_line_put(args->err_msg, "dotnet named no file for ");
    mmgr_line_put(args->err_msg, args->name);
    return 0;
}

// `parts`, `count` of them, joined by `/` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_parts(const ExecutablesCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? "/" : "");
        mmgr_line_put(&line, args->parts[at]);
    }
    mmgr_line_finish(&line);
}

int cli_executables_init(const CliExecutablesPathCfg *args)
{
    const char *const colon =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->id,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}), .byte = ':');
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = args->id, .other = "executable/", .cap = CLI_CELLS_POOL})) ||
        (colon == NULL) || (colon < (args->id + 11)))
    {
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is no executable's job");
        return 0;
    }
    char script[CLI_PATH_MAX_LEN];
    VerbaLinea scripted = {script, sizeof(script), 0ULL};
    mmgr_line_put_n(&scripted, args->id + 11, (unsigned long long)(colon - (args->id + 11)));
    mmgr_line_finish(&scripted);
    const char *const name = colon + 1;
    char full[CLI_PATH_MAX_LEN];
    const char *const halves[2] = {args->root, script};
    EMBED_CALL(cli_parts, ExecutablesCtx, .parts = halves, .count = 2u, .out = full);
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = full, .out = folder))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = folder,
                          .src = args->root,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    const char *base = full;
    const unsigned long long base_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = full,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}), .set = "/\\");
    base = (base_end != 0ULL) ? (full + base_end) : base;
    const unsigned int kind =
        EMBED_CALL(cli_executables_name_out_folder_internal.kind_get, CliExecutablesNameOutFolderCtx, .name = base);
    if (kind == CLI_KIND_NONE)
    {
        mmgr_line_put(args->err_msg, script);
        mmgr_line_put(args->err_msg, " is no build script");
        return 0;
    }
    char file[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_executables_name_out_folder_internal.program_file, CliExecutablesNameOutFolderCtx, .name = name,
               .out = file);
    int found = 1;
    if (kind == CLI_KIND_CARGO)
    {
        const char *const profile = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                               .values = args->values, .key = "profile", .fallback = "dev");
        const char *const profile_folder =
            (cellul.eq(&(CatenaFinitaCfg){.src = profile, .other = "dev", .cap = CLI_CELLS_POOL})) ? "debug" : profile;
        const char *const target = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                              .values = args->values, .key = "target", .fallback = "this machine");
        char built[CLI_PATH_MAX_LEN] = "";
        found = EMBED_CALL(cli_cargo_target, ExecutablesCtx, .manifest = full, .out = built, .err_msg = args->err_msg);
        if (cellul.eq(&(CatenaFinitaCfg){.src = target, .other = "this machine", .cap = CLI_CELLS_POOL}))
        {
            const char *const parts[3] = {built, profile_folder, file};
            EMBED_CALL(cli_parts, ExecutablesCtx, .parts = parts, .count = 3u, .out = args->path);
        }
        else
        {
            char named[CLI_NAME_MAX_LEN];
            VerbaLinea line = {named, sizeof(named), 0ULL};
            mmgr_line_put(&line, name);
            mmgr_line_put(
                &line,
                (cellul.find(&(CatenaFinitaCfg){
                     .src = target, .cap = CLI_CELLS_POOL, .other = "windows", .other_cap = CLI_CELLS_POOL}) != NULL)
                    ? ".exe"
                    : "");
            mmgr_line_finish(&line);
            const char *const parts[4] = {built, target, profile_folder, named};
            EMBED_CALL(cli_parts, ExecutablesCtx, .parts = parts, .count = 4u, .out = args->path);
        }
    }
    else if ((kind == CLI_KIND_CMAKE) || (kind == CLI_KIND_MESON))
    {
        const int cmake = kind == CLI_KIND_CMAKE;
        const char *const config = cmake ? EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                                      .values = args->values, .key = "config", .fallback = "Debug")
                                         : EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                                      .values = args->values, .key = "buildtype", .fallback = "debug");
        char relative[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_executables_out_internal.build_folder, CliExecutablesOutCtx, .root = args->root, .script = full,
                   .tool = cmake ? "cmake" : "meson", .out = relative);
        char build[CLI_PATH_MAX_LEN];
        VerbaLinea line = {build, sizeof(build), 0ULL};
        mmgr_line_put(&line, args->root);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, relative);
        mmgr_line_put(&line, config);
        mmgr_line_finish(&line);
        found = cmake ? EMBED_CALL(cli_executables_name_out_internal.cmake_artifact, CliExecutablesNameOutCtx,
                                   .build = build, .name = name, .config = config, .out = args->path,
                                   .err_msg = args->err_msg)
                      : EMBED_CALL(cli_executables_name_out_internal.meson_artifact, CliExecutablesNameOutCtx,
                                   .build = build, .name = name, .out = args->path, .err_msg = args->err_msg);
    }
    else if (kind == CLI_KIND_GO)
    {
        const char *host_system = NULL;
        const char *host_processor = NULL;
        EMBED_CALL(cli_executables_system_internal.go_host, CliExecutablesSystemCtx, .system = &host_system,
                   .processor = &host_processor);
        const char *const system = EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                              .values = args->values, .key = "GOOS", .fallback = host_system);
        char pair[CLI_NAME_MAX_LEN];
        VerbaLinea line = {pair, sizeof(pair), 0ULL};
        mmgr_line_put(&line, system);
        mmgr_line_ch(&line, '-');
        mmgr_line_put(&line, EMBED_CALL(cli_executables_key_internal.first, CliExecutablesKeyCtx,
                                        .values = args->values, .key = "GOARCH", .fallback = host_processor));
        mmgr_line_finish(&line);
        char named[CLI_NAME_MAX_LEN];
        VerbaLinea program = {named, sizeof(named), 0ULL};
        mmgr_line_put(&program, name);
        mmgr_line_put(&program,
                      (cellul.eq(&(CatenaFinitaCfg){.src = system, .other = "windows", .cap = CLI_CELLS_POOL})) ? ".exe"
                                                                                                                : "");
        mmgr_line_finish(&program);
        const char *const parts[5] = {args->root, "build", "exe", pair, named};
        EMBED_CALL(cli_parts, ExecutablesCtx, .parts = parts, .count = 5u, .out = args->path);
    }
    else if (kind == CLI_KIND_NPM)
    {
        const char *const parts[4] = {args->root, "build", "exe", file};
        EMBED_CALL(cli_parts, ExecutablesCtx, .parts = parts, .count = 4u, .out = args->path);
    }
    else if (kind == CLI_KIND_DOTNET)
    {
        found = EMBED_CALL(cli_executables_name_out_err_msg_internal.dotnet_artifact, CliExecutablesNameOutErrMsgCtx,
                           .name = name, .lines = args->lines, .out = args->path, .err_msg = args->err_msg);
    }
    else if (kind == CLI_KIND_ZIG)
    {
        const char *const parts[4] = {folder, "zig-out", "bin", file};
        EMBED_CALL(cli_parts, ExecutablesCtx, .parts = parts, .count = 4u, .out = args->path);
    }
    else
    {
        char exe[CLI_NAME_MAX_LEN];
        char under[CLI_PATH_MAX_LEN];
        // the spec read, a cell of its size taken for the reading
        char *const spec = EMBED_CALL(cli_executables_walk_internal.text_alloc, CliExecutablesWalkCtx, .path = full);
        if (spec == NULL)
        {
            mmgr_line_put(args->err_msg, script);
            mmgr_line_put(args->err_msg, ": it could not be read");
            return 0;
        }
        found = EMBED_CALL(cli_executables_text_exe_internal.spec_init, CliExecutablesTextExeCtx, .text = spec,
                           .exe_capacity = exe, .path_out = under);
        g_cli_cells->persistent_buf_release(spec, &g_cli_executables_cells);
        if (!found)
        {
            mmgr_line_put(args->err_msg, script);
            mmgr_line_put(args->err_msg, " is no PyInstaller spec");
            return 0;
        }
        const char *const parts[2] = {folder, under};
        EMBED_CALL(cli_parts, ExecutablesCtx, .parts = parts, .count = 2u, .out = args->path);
    }
    if (found && !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->path))
    {
        mmgr_line_put(args->err_msg, "the build made no ");
        mmgr_line_put(args->err_msg, args->path);
        return 0;
    }
    return found;
}
