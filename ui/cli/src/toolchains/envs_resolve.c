// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_resolve.c: an environment of the tree made ready to use, Pipenv and Nix queried where theirs are, and the
// packages its Python has not installed
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs.h"

// The work state a resolving works in: the toolchain found, the PATH it runs with, and what Pipenv or Nix answered, a
// cell sized to the answer
typedef struct
{
    CliFound found;
    char path[CLI_PATH_TEXT];
    char *output;
    unsigned long long output_size;
} CliEnvWorkState;

static CliEnvWorkState *s_work_state;

typedef struct
{
    const char *folder;
    CliTreeEnvironment *environment;
    VerbaLinea *err_msg;
    const char *id;
    const char *const *request;
    unsigned int count;
    const char *one;
    unsigned long long one_length;
    const char *other;
    unsigned long long other_length;
} EnvsResolveCtx;
// the Python and the bin folder of a virtual environment's folder into `environment`: 1, or 0 with why given
EMBED_INLINE int cli_venv_at(const EnvsResolveCtx *args)
{
#if defined(_WIN32)
    static const char *const s_bin = "Scripts";
    static const char *const s_python = "python.exe";
#else
    static const char *const s_bin = "bin";
    static const char *const s_python = "python";
#endif
    char bin[CLI_PATH_MAX_LEN];
    char python[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = s_bin, .joined = bin);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = bin, .relative = s_python, .joined = python);
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = python))
    {
        mmgr_line_put(args->err_msg, args->folder);
        mmgr_line_put(args->err_msg, " contains no Python");
        return 0;
    }
    args->environment->bins = 1u;
    memor.cpy(&(MemoriaCfg){.dst = args->environment->bin[0],
                            .src = bin,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = bin, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->environment->python,
                            .src = python,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_POOL_BYTES}) + 1u});
    args->environment->has_python = 1;
    args->environment->place[0] = '\0';
    return 1;
}

// the output of the program the manifest names `id`, run in `folder` with `args`, into the work state: 1, or 0 with why
// given
static int cli_tool_output_read(const EnvsResolveCtx *args)
{
    const char *id = args->id;
    const char *folder = args->folder;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unknown = {why, sizeof(why), 0ULL};
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &json, .nodes_more = 0u, .text_more = 0ULL),
                   .id = id, .err_msg = &unknown);
    if (tool == CLI_JSON_NONE)
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        mmgr_line_put(err_msg, id);
        mmgr_line_put(err_msg, " is not in the manifest");
        return 0;
    }
    const char *const displayed =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = tool, .name = "name"),
                   .length_pointer = NULL);
    char name[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = name, .text = displayed, .otherwise = id);
    unsigned int folders = 0u;
    char (*const path)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&folders, g_cli_toolchains_owner);
    EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = tool,
               .path_callback = (const char (*)[CLI_PATH_MAX_LEN])path, .count = folders,
               .found = &s_work_state->found);
    g_cli_guard->persistent_buf_release(path, g_cli_toolchains_owner);
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if (!s_work_state->found.has_program)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    const char *words[16] = {s_work_state->found.program};
    for (unsigned int at = 0u; (at < count) && (at < 14u); at += 1u)
    {
        words[at + 1u] = args->request[at];
    }
    words[((count < 14u) ? count : 14u) + 1u] = NULL;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_work_state->path,
               .size = sizeof(s_work_state->path));
    const CliSetting settings[2] = {{"PATH", s_work_state->path}, {"PIPENV_IGNORE_VIRTUALENVS", "1"}};
    CliRunResult *const run_result = cli_run_result_alloc();
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = folder, .settings = settings,
                    .count = 2u, .run_result = run_result, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    if (run_result->code != 0LL)
    {
        // the last line of what it printed, trimmed, or that it failed
        unsigned long long end = run_result->err_size;
        end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = run_result->err, .at = end,
                         .set = CLI_TEXT_WHITE);
        unsigned long long start = end;
        start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = run_result->err,
                           .at = start, .set = "\n");
        unsigned long long first = 0ULL;
        first = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = run_result->err,
                           .at = first, .size = end, .set = CLI_TEXT_WHITE);
        start = (start > first) ? start : first;
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": ");
        if (end > start)
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = err_msg,
                       .text_bytes = (const unsigned char *)run_result->err + start, .length = end - start);
        }
        else
        {
            mmgr_line_put(err_msg, "it failed");
        }
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    g_cli_guard->persistent_buf_release(s_work_state->output, g_cli_toolchains_owner);
    s_work_state->output = g_cli_guard->temporary_buf_alloc(run_result->out_size + 1ULL, g_cli_toolchains_owner);
    if (s_work_state->output == NULL)
    {
        s_work_state->output_size = 0ULL;
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": its answer could not be stored");
        return 0;
    }
    s_work_state->output_size = run_result->out_size;
    memor.cpy(&(MemoriaCfg){.dst = s_work_state->output, .src = run_result->out, .bytes = run_result->out_size});
    s_work_state->output[run_result->out_size] = '\0';
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return 1;
}

// the folders of a Nix development shell's PATH that are in its store, and its Python where one of them contains it
EMBED_INLINE int cli_nix_read(const EnvsResolveCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = s_work_state->output, .size = s_work_state->output_size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &unread, .owner = g_cli_toolchains_owner);
    if (root == CLI_JSON_NONE)
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        mmgr_line_put(err_msg, "Nix: ");
        mmgr_line_put_n(err_msg, why, unread.at);
        return 0;
    }
    const unsigned int path = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = &json, .object = root, .name = "variables"),
                                         .name = "PATH");
    unsigned long long length = 0ULL;
    const char *folders =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = path, .name = "value"),
                   .length_pointer = &length);
    length = (folders != NULL) ? length : 0ULL;
    folders = (folders != NULL) ? folders : "";
#if defined(_WIN32)
    const char parting = ';';
#else
    const char parting = ':';
#endif
    args->environment->bins = 0u;
    args->environment->has_python = 0;
    unsigned long long from = 0ULL;
    for (unsigned long long at = 0ULL; at <= length; at += 1ULL)
    {
        if ((at < length) && (folders[at] != parting))
        {
            continue;
        }
        char folder[CLI_PATH_MAX_LEN];
        const unsigned long long size = at - from;
        const char *const start = folders + from;
        from = at + 1ULL;
        if ((size >= CLI_PATH_MAX_LEN) || (args->environment->bins >= CLI_ENVIRONMENT_BINS))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = start, .bytes = size});
        folder[size] = '\0';
        if (cellul.find(&(CatenaFinitaCfg){
                .src = folder, .cap = CLI_POOL_BYTES, .other = "/nix/store/", .other_cap = CLI_POOL_BYTES}) == NULL)
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->environment->bin[args->environment->bins],
                                .src = folder,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}) + 1u});
        args->environment->bins += 1u;
        char python[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "python3", .joined = python);
        if (!args->environment->has_python &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = python))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->environment->python,
                              .src = python,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_POOL_BYTES}) + 1u});
            args->environment->has_python = 1;
        }
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return 1;
}

int cli_envs_resolve(const CliEnvsRootCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (args->place[0] == '\0')
    {
        memor.cpy(
            &(MemoriaCfg){.dst = folder,
                          .src = args->root,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES}) + 1u});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->place, .joined = folder);
    }
    s_work_state = g_cli_guard->temporary_buf_alloc(sizeof(*s_work_state), g_cli_toolchains_owner);
    if (s_work_state == NULL)
    {
        mmgr_line_put(args->err_msg, "an environment's resolving could not be taken");
        return 0;
    }
    s_work_state->output = NULL;
    s_work_state->output_size = 0ULL;
    int made = 0;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->type, .other = "venv", .cap = CLI_POOL_BYTES}))
    {
        made = EMBED_CALL(cli_venv_at, EnvsResolveCtx, .folder = folder, .environment = args->environment,
                          .err_msg = args->err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->type, .other = "pipenv", .cap = CLI_POOL_BYTES}))
    {
        static const char *const s_args[1] = {"--venv"};
        made = EMBED_CALL(cli_tool_output_read, EnvsResolveCtx, .id = "pipenv", .folder = folder, .request = s_args,
                          .count = 1u, .err_msg = args->err_msg);
        if (made)
        {
            // the folder Pipenv gave, trimmed
            unsigned long long start = 0ULL;
            unsigned long long end = s_work_state->output_size;
            start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = s_work_state->output,
                               .at = start, .size = end, .set = CLI_TEXT_WHITE);
            end = (start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                      .text = s_work_state->output + start, .at = end - start, .set = CLI_TEXT_WHITE));
            s_work_state->output[end] = '\0';
            made = EMBED_CALL(cli_venv_at, EnvsResolveCtx, .folder = s_work_state->output + start,
                              .environment = args->environment, .err_msg = args->err_msg);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->type, .other = "nix", .cap = CLI_POOL_BYTES}))
    {
        static const char *const s_args[2] = {"print-dev-env", "--json"};
        made = EMBED_CALL(cli_tool_output_read, EnvsResolveCtx, .id = "nix", .folder = folder, .request = s_args,
                          .count = 2u, .err_msg = args->err_msg) &&
               EMBED_CALL(cli_nix_read, EnvsResolveCtx, .environment = args->environment, .err_msg = args->err_msg);
    }
    else
    {
        mmgr_line_put(args->err_msg, args->type);
        mmgr_line_put(args->err_msg, " is no type of environment");
    }
    if (made)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->environment->place,
                          .src = args->place,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->place, .cap = CLI_POOL_BYTES}) + 1u});
    }
    g_cli_guard->persistent_buf_release(s_work_state->output, g_cli_toolchains_owner);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
    s_work_state = NULL;
    return made;
}
