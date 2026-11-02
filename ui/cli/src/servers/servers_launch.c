// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_launch.c: a server started: its program found as its toolchain is, its options the projects it is given and
// what a tree whose code is disabled or runs adds, its environment the PATH every step runs with; clangd given the
// build's database and its compilers, and Visual Studio's environment where one is cl
#include "../builds/compile_db.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../machine/trust.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

#include "../text/text.h"

// how many folders below the top a search for projects goes; and the bytes the projects found and a server's options
// are written in
#define CLI_SERVERS_PROJECT_DEPTH 6u
#define CLI_SERVERS_FOUND_CAPACITY (1ull << 20u)
#define CLI_SERVERS_OPTIONS_CAPACITY (1ull << 20u)

// what parts the folders of a PATH
#if defined(_WIN32)
#define CLI_SERVERS_PATH_PARTS ';'
#else
#define CLI_SERVERS_PATH_PARTS ':'
#endif

// where what each server sends unprompted goes, a place for each server started, taken in turn
static CliServersSink s_server_sinks[64];
static unsigned int s_server_sink_next;

// The search for projects: the file name looked for, the folder read and its depth, and the projects found, their paths
// one after another, each with its zero after it, in a line, and their count
typedef struct
{
    const char *name;
    const char *at;
    unsigned int at_depth;
    VerbaLinea found;
    unsigned int count;
} CliServersProjects;

// What a server is started with, a cell taken as it starts and given back once it has: its program's folder, its
// words, the build's database clangd is given, and the PATH every step runs with
typedef struct
{
    CliFound found;
    CliWords words;
    CliDatabase database;
    char path[CLI_PATH_TEXT];
} CliServersLaunching;

// an entry of a folder the search reads: a folder gone into, no deeper than the search goes, or a project
static void launch_project_entry(const CliEachNameIsFolderCfg *args)
{
    void *context = args->context;
    CliServersProjects *const projects = context;
    static const char *const s_not[4] = {"target", "build", "node_modules", "archive"};
    char path[CLI_PATH_MAX_LEN];
    if (args->is_folder)
    {
        int skipped = (args->name[0] == '.') || (projects->at_depth >= CLI_SERVERS_PROJECT_DEPTH);
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            skipped |= (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_not[at], .cap = CLI_CELLS_POOL}));
        }
        if (!skipped)
        {
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = projects->at, .relative = args->name,
                       .joined = path);
            const char *const above = projects->at;
            projects->at = path;
            projects->at_depth += 1u;
            (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path,
                             .each = launch_project_entry, .context_bytess = projects);
            projects->at = above;
            projects->at_depth -= 1u;
        }
        return;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = projects->name, .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = projects->at, .relative = args->name, .joined = path);
        mmgr_line_put_n(&projects->found, path,
                        cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u);
        projects->count += 1u;
    }
}

void cli_servers_projects_in(const CliServersRootOutCtx *args)
{
    char *const found_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_FOUND_CAPACITY, &g_cli_servers_launch_cells);
    CliServersProjects projects = {args->name, args->root, 0u, mmgr_line_over(found_cell, CLI_SERVERS_FOUND_CAPACITY),
                                   0u};
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->root,
                     .each = launch_project_entry, .context_bytess = &projects);
    // the projects in order, each by where it starts among those found
    const unsigned int count = mmgr_line_ok(&projects.found) ? projects.count : 0u;
    const char **const found =
        (count != 0u) ? g_cli_cells->temporary_buf_alloc(count * sizeof(*found), &g_cli_servers_launch_cells) : NULL;
    const unsigned int list_entries = (found != NULL) ? count : 0u;
    const char *path = projects.found.out;
    for (unsigned int at = 0u; at < list_entries; at += 1u)
    {
        found[at] = path;
        path += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u;
    }
    for (unsigned int at = 1u; at < list_entries; at += 1u)
    {
        const char *const swap = found[at];
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = found[place - 1u],
                                           .other_pointer = swap) > 0))
        {
            found[place] = found[place - 1u];
            place -= 1u;
        }
        found[place] = swap;
    }
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < list_entries; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)found[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = found[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(args->out, ']');
    g_cli_cells->persistent_buf_release(found, &g_cli_servers_launch_cells);
    g_cli_cells->persistent_buf_release(projects.found.out, &g_cli_servers_launch_cells);
}

typedef struct
{
    const char *root;
    const CliServerSpec *spec;
    VerbaLinea *out;
} ServersLaunchCtx;
// the options a server starts with: the projects it is given, then what the tree adds, `null` where neither is
EMBED_INLINE void launch_options(const ServersLaunchCtx *args)
{
    const char *root = args->root;
    VerbaLinea *out = args->out;
    const char *const added = EMBED_CALL(cli_trust_tree_words.block, CliTrustTreeWordsCfg, .tree = root)
                                  ? args->spec->blocked_args
                                  : args->spec->trusted;
    const int adds = (added[0] == '{');
    if ((args->spec->projects[0] == '\0') && !adds)
    {
        mmgr_line_put(out, "null");
        return;
    }
    mmgr_line_ch(out, '{');
    int members = 0;
    if (args->spec->projects[0] != '\0')
    {
        mmgr_line_put(out, "\"linkedProjects\":");
        EMBED_CALL(cli_servers_root_out_internal.projects_in, CliServersRootOutCtx, .root = root,
                   .name = args->spec->projects, .out = out);
        members = 1;
    }
    if (adds)
    {
        unsigned long long from = 1ull;
        unsigned long long to = cellul.len(&(CatenaFinitaCfg){.src = added, .cap = CLI_CELLS_POOL});
        to -= ((to > 1u) && (added[to - 1u] == '}')) ? 1u : 0u;
        unsigned long long inner_from = 0ull;
        unsigned long long inner_to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = added + from, .size = to - from,
                   .from_u64s = &inner_from, .to_u64s = &inner_to);
        if (inner_to > inner_from)
        {
            mmgr_line_put(out, members ? "," : "");
            mmgr_line_put_n(out, added + from + inner_from, inner_to - inner_from);
        }
    }
    mmgr_line_ch(out, '}');
}

CliLsp *cli_servers_launch(const CliServersRootCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    CliJson manifest_json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &manifest_json, .nodes_more = 0u, .text_more = 0ull);
    char quiet[256];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &manifest_json,
                   .manifest = manifest, .id = args->spec_read->tool, .err_msg = &ignored);
    CliServersLaunching *const launching =
        g_cli_cells->temporary_buf_alloc(sizeof(*launching), &g_cli_servers_launch_cells);
    if (launching != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = &launching->found, .val = 0, .bytes = sizeof(launching->found)});
    }
    if ((launching != NULL) && (tool != CLI_JSON_NONE))
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&count, &g_cli_servers_launch_cells);
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &manifest_json, .tool = tool,
                   .path_callback = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count,
                   .found = &launching->found);
        g_cli_cells->persistent_buf_release(folders, &g_cli_servers_launch_cells);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &manifest_json);
    if (launching == NULL)
    {
        mmgr_line_put(err_msg, "the cells cannot meet what ");
        mmgr_line_put(err_msg, args->spec_read->program);
        mmgr_line_put(err_msg, " is started with");
        return NULL;
    }
    char program[CLI_PATH_MAX_LEN];
    const char *const names[1] = {args->spec_read->program};
    if ((launching->found.folder[0] == '\0') ||
        !EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = launching->found.folder, .names = names,
                    .count = 1u, .path = program))
    {
        mmgr_line_put(err_msg, args->spec_read->program);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens ");
        mmgr_line_put(err_msg, args->spec_read->name);
        mmgr_line_put(err_msg, "'s install page or takes its folder");
        g_cli_cells->persistent_buf_release(launching, &g_cli_servers_launch_cells);
        return NULL;
    }
    char *const options_cell =
        g_cli_cells->temporary_buf_alloc(CLI_SERVERS_OPTIONS_CAPACITY, &g_cli_servers_launch_cells);
    VerbaLinea options = mmgr_line_over(options_cell, CLI_SERVERS_OPTIONS_CAPACITY);
    EMBED_CALL(launch_options, ServersLaunchCtx, .root = root, .spec = args->spec_read, .out = &options);
    CliWords *const words = &launching->words;
    words->count = 0u;
    words->used = 0u;
    words->full = 0;
    for (unsigned int at = 0u; at < args->spec_read->args; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                   .word = args->spec_read->arg[at]);
    }
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = launching->path,
               .size = sizeof(launching->path));
    const CliVsEnvironment *vs = NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->spec_read->program, .other = "clangd", .cap = CLI_CELLS_POOL}))
    {
        (void)EMBED_CALL(cli_compile_db_root_database.refresh, CliCompileDbRootDatabaseCfg, .root = root,
                         .database = &launching->database);
        EMBED_CALL(cli_compile_db_database.words, CliCompileDbDatabaseCfg, .database_read = &launching->database,
                   .words = words);
        vs = launching->database.msvc ? cli_compile_db_vs_environment() : NULL;
    }
    // the settings of the server's environment, the PATH and Visual Studio's where one is cl, in a cell of their count
    const unsigned int most = 1u + ((vs != NULL) ? vs->count : 0u);
    CliSetting *const setting = g_cli_cells->temporary_buf_alloc(most * sizeof(*setting), &g_cli_servers_launch_cells);
    char *vs_path = NULL;
    unsigned int settings = (setting != NULL) ? 1u : 0u;
    const unsigned int vs_settings = ((setting != NULL) && (vs != NULL)) ? vs->count : 0u;
    if (setting != NULL)
    {
        setting[0] = (CliSetting){"PATH", launching->path};
    }
    for (unsigned int at = 0u; at < vs_settings; at += 1u)
    {
        char upper[8] = {0};
        const unsigned long long named = cellul.len(&(CatenaFinitaCfg){.src = vs->name[at], .cap = CLI_CELLS_POOL});
        const unsigned long long copied = (named < 7ull) ? named : 7ull;
        memor.cpy(&(MemoriaCfg){.dst = upper, .src = vs->name[at], .bytes = (size_t)copied});
        EMBED_CALL(cli_text_size_from.raise, CliTextSizeFromCfg, .text_out = upper, .size = copied);
        if ((cellul.len(&(CatenaFinitaCfg){.src = vs->name[at], .cap = CLI_CELLS_POOL}) == 4u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = upper, .other = "PATH", .cap = CLI_CELLS_POOL})))
        {
            // Visual Studio's PATH before the one every step runs with, in a cell of the two and the mark between
            const unsigned long long joined_size =
                cellul.len(&(CatenaFinitaCfg){.src = vs->value[at], .cap = CLI_CELLS_POOL}) +
                cellul.len(&(CatenaFinitaCfg){.src = launching->path, .cap = CLI_CELLS_POOL}) + 2ull;
            g_cli_cells->persistent_buf_release(vs_path, &g_cli_servers_launch_cells);
            vs_path = g_cli_cells->persistent_buf_alloc(joined_size, &g_cli_servers_launch_cells);
            VerbaLinea joined = mmgr_line_over(vs_path, joined_size);
            mmgr_line_put(&joined, vs->value[at]);
            mmgr_line_ch(&joined, CLI_SERVERS_PATH_PARTS);
            mmgr_line_put(&joined, launching->path);
            setting[0].value = (mmgr_line_finish(&joined) != 0ull) ? vs_path : launching->path;
            continue;
        }
        setting[settings] = (CliSetting){vs->name[at], vs->value[at]};
        settings += 1u;
    }
    CliServersSink *const server_sink = &s_server_sinks[s_server_sink_next % 64u];
    s_server_sink_next += 1u;
    *server_sink = *args->sink;
    char why[1024];
    VerbaLinea reason = {why, sizeof(why), 0ull};
    const int met = (setting != NULL) && mmgr_line_ok(&options);
    if (!met)
    {
        mmgr_line_put(&reason, "the cells cannot meet its options or its environment");
    }
    CliLsp *const server =
        met ? EMBED_CALL(cli_lsp_err_msg.start, CliLspErrMsgCfg, .program = program, .request = words->word,
                         .count = words->count, .root = root, .options = options.out, .length = options.at,
                         .settings = setting, .setting_count = settings, .recv = cli_servers_recv,
                         .context = server_sink, .err_msg = &reason)
            : NULL;
    g_cli_cells->persistent_buf_release(vs_path, &g_cli_servers_launch_cells);
    g_cli_cells->persistent_buf_release(setting, &g_cli_servers_launch_cells);
    g_cli_cells->persistent_buf_release(options.out, &g_cli_servers_launch_cells);
    g_cli_cells->persistent_buf_release(launching, &g_cli_servers_launch_cells);
    if (server == NULL)
    {
        mmgr_line_put(err_msg, args->spec_read->program);
        mmgr_line_put(err_msg, " did not start");
        if (args->spec_read->setup[0] != '\0')
        {
            mmgr_line_put(err_msg, " (");
            mmgr_line_put_n(err_msg, why, reason.at);
            mmgr_line_put(err_msg, "): ");
            mmgr_line_put(err_msg, args->spec_read->setup);
            mmgr_line_put(err_msg, " installs it");
        }
        else
        {
            mmgr_line_put(err_msg, ": ");
            mmgr_line_put_n(err_msg, why, reason.at);
        }
    }
    return server;
}
