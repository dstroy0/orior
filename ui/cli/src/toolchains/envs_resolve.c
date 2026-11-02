// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_resolve.c: an environment of the tree made ready to use, Pipenv and Nix queried where theirs are, and the
// packages its Python has not installed
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "envs.h"

// The room a resolving works in: the toolchain found, the PATH it runs with, and what Pipenv or Nix answered, a cell
// sized to the answer
typedef struct
{
    CliFound found;
    char path[CLI_PATH_TEXT];
    char *output;
    unsigned long long output_size;
} CliEnvRoom;

static CliEnvRoom *s_room;

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
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = bin, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->environment->python,
                            .src = python,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL}) + 1u});
    args->environment->has_python = 1;
    args->environment->place[0] = '\0';
    return 1;
}

// the output of the program the manifest names `id`, run in `folder` with `args`, into the room: 1, or 0 with why
// given
static int cli_tool_output_read(const EnvsResolveCtx *args)
{
    const char *id = args->id;
    const char *folder = args->folder;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unknown = {why, sizeof(why), 0ull};
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &json, .nodes_more = 0u, .text_more = 0ull),
                   .id = id, .err_msg = &unknown);
    if (tool == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        mmgr_line_put(err_msg, id);
        mmgr_line_put(err_msg, " is not in the manifest");
        return 0;
    }
    const char *const shown =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = tool, .name = "name"),
                   .length_pointer = NULL);
    char name[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = name, .text = shown, .otherwise = id);
    unsigned int folders = 0u;
    char (*const path)[CLI_PATH_MAX_LEN] = cli_toolchains_path_taken(&folders, &g_cli_toolchains_cells);
    EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = tool,
               .path_callback = (const char (*)[CLI_PATH_MAX_LEN])path, .count = folders, .found = &s_room->found);
    g_cli_cells->persistent_buf_release(path, &g_cli_toolchains_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    if (!s_room->found.has_program)
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    const char *words[16] = {s_room->found.program};
    for (unsigned int at = 0u; (at < count) && (at < 14u); at += 1u)
    {
        words[at + 1u] = args->request[at];
    }
    words[((count < 14u) ? count : 14u) + 1u] = NULL;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_room->path,
               .size = sizeof(s_room->path));
    const CliSetting settings[2] = {{"PATH", s_room->path}, {"PIPENV_IGNORE_VIRTUALENVS", "1"}};
    CliRan *const ran = cli_ran_take();
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = folder, .settings = settings,
                    .count = 2u, .ran = ran, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    if (ran->code != 0ll)
    {
        // the last line of what it printed, trimmed, or that it failed
        unsigned long long end = ran->err_size;
        end = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = ran->err, .at = end,
                         .set = CLI_TEXT_WHITE);
        unsigned long long start = end;
        start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = ran->err, .at = start,
                           .set = "\n");
        unsigned long long first = 0ull;
        first = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = ran->err, .at = first,
                           .size = end, .set = CLI_TEXT_WHITE);
        start = (start > first) ? start : first;
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": ");
        if (end > start)
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = err_msg,
                       .text_bytes = (const unsigned char *)ran->err + start, .length = end - start);
        }
        else
        {
            mmgr_line_put(err_msg, "it failed");
        }
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    g_cli_cells->persistent_buf_release(s_room->output, &g_cli_toolchains_cells);
    s_room->output = g_cli_cells->persistent_buf_alloc(ran->out_size + 1ull, &g_cli_toolchains_cells);
    if (s_room->output == NULL)
    {
        s_room->output_size = 0ull;
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": its answer could not be stored");
        return 0;
    }
    s_room->output_size = ran->out_size;
    memor.cpy(&(MemoriaCfg){.dst = s_room->output, .src = ran->out, .bytes = ran->out_size});
    s_room->output[ran->out_size] = '\0';
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return 1;
}

// the folders of a Nix development shell's PATH that are in its store, and its Python where one of them contains it
EMBED_INLINE int cli_nix_read(const EnvsResolveCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = s_room->output, .size = s_room->output_size, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = &unread, .owner = &g_cli_toolchains_cells);
    if (root == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        mmgr_line_put(err_msg, "Nix: ");
        mmgr_line_put_n(err_msg, why, unread.at);
        return 0;
    }
    const unsigned int path = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = &json, .object = root, .name = "variables"),
                                         .name = "PATH");
    unsigned long long length = 0ull;
    const char *folders =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = path, .name = "value"),
                   .length_pointer = &length);
    folders = (folders != NULL) ? folders : "";
#if defined(_WIN32)
    const char parting = ';';
#else
    const char parting = ':';
#endif
    args->environment->bins = 0u;
    args->environment->has_python = 0;
    unsigned long long from = 0ull;
    for (unsigned long long at = 0ull; at <= length; at += 1ull)
    {
        if ((at < length) && (folders[at] != parting))
        {
            continue;
        }
        char folder[CLI_PATH_MAX_LEN];
        const unsigned long long size = at - from;
        const char *const start = folders + from;
        from = at + 1ull;
        if ((size >= CLI_PATH_MAX_LEN) || (args->environment->bins >= CLI_ENVIRONMENT_BINS))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = start, .bytes = size});
        folder[size] = '\0';
        if (cellul.find(&(CatenaFinitaCfg){
                .src = folder, .cap = CLI_CELLS_POOL, .other = "/nix/store/", .other_cap = CLI_CELLS_POOL}) == NULL)
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->environment->bin[args->environment->bins],
                                .src = folder,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
        args->environment->bins += 1u;
        char python[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "python3", .joined = python);
        if (!args->environment->has_python &&
            EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = python))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->environment->python,
                              .src = python,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL}) + 1u});
            args->environment->has_python = 1;
        }
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
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
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = args->place, .joined = folder);
    }
    s_room = g_cli_cells->persistent_buf_alloc(sizeof(*s_room), &g_cli_toolchains_cells);
    if (s_room == NULL)
    {
        mmgr_line_put(args->err_msg, "an environment's resolving could not be taken");
        return 0;
    }
    s_room->output = NULL;
    s_room->output_size = 0ull;
    int made = 0;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "venv", .cap = CLI_CELLS_POOL}))
    {
        made = EMBED_CALL(cli_venv_at, EnvsResolveCtx, .folder = folder, .environment = args->environment,
                          .err_msg = args->err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "pipenv", .cap = CLI_CELLS_POOL}))
    {
        static const char *const s_args[1] = {"--venv"};
        made = EMBED_CALL(cli_tool_output_read, EnvsResolveCtx, .id = "pipenv", .folder = folder, .request = s_args,
                          .count = 1u, .err_msg = args->err_msg);
        if (made)
        {
            // the folder Pipenv gave, trimmed
            unsigned long long start = 0ull;
            unsigned long long end = s_room->output_size;
            start = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = s_room->output,
                               .at = start, .size = end, .set = CLI_TEXT_WHITE);
            end = (start + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg,
                                      .text = s_room->output + start, .at = end - start, .set = CLI_TEXT_WHITE));
            s_room->output[end] = '\0';
            made = EMBED_CALL(cli_venv_at, EnvsResolveCtx, .folder = s_room->output + start,
                              .environment = args->environment, .err_msg = args->err_msg);
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "nix", .cap = CLI_CELLS_POOL}))
    {
        static const char *const s_args[2] = {"print-dev-env", "--json"};
        made = EMBED_CALL(cli_tool_output_read, EnvsResolveCtx, .id = "nix", .folder = folder, .request = s_args,
                          .count = 2u, .err_msg = args->err_msg) &&
               EMBED_CALL(cli_nix_read, EnvsResolveCtx, .environment = args->environment, .err_msg = args->err_msg);
    }
    else
    {
        mmgr_line_put(args->err_msg, args->kind);
        mmgr_line_put(args->err_msg, " is no kind of environment");
    }
    if (made)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->environment->place,
                          .src = args->place,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->place, .cap = CLI_CELLS_POOL}) + 1u});
    }
    g_cli_cells->persistent_buf_release(s_room->output, &g_cli_toolchains_cells);
    g_cli_cells->persistent_buf_release(s_room, &g_cli_toolchains_cells);
    s_room = NULL;
    return made;
}

// `text` of `length` bytes as pip names a package, in lower case with `_` written as `-`, compared with another
EMBED_INLINE int cli_package_same(const EnvsResolveCtx *args)
{
    if (args->one_length != args->other_length)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < args->one_length; at += 1ull)
    {
        char left = ((args->one[at] >= 'A') && (args->one[at] <= 'Z')) ? (char)(args->one[at] + 32) : args->one[at];
        char right =
            ((args->other[at] >= 'A') && (args->other[at] <= 'Z')) ? (char)(args->other[at] + 32) : args->other[at];
        left = (left == '_') ? '-' : left;
        right = (right == '_') ? '-' : right;
        if (left != right)
        {
            return 0;
        }
    }
    return 1;
}

void cli_envs_missing(const CliEnvsOutCfg *args)
{
    // pip's list read into a tree sized to it; where it cannot be read none is reported to be missing, as where the
    // environment has no Python
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    unsigned int listed = CLI_JSON_NONE;
    int started = 0;
    if (args->environment_read->has_python)
    {
        const char *const words[8] = {args->environment_read->python, "-m", "pip", "list", "--format", "json",
                                      "--disable-pip-version-check",  NULL};
        CliRan *const ran = cli_ran_take();
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea unstarted = {why, sizeof(why), 0ull};
        started = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                             .count = 0u, .ran = ran, .err_msg = &unstarted);
        if (started)
        {
            VerbaLinea unread = {why, sizeof(why), 0ull};
            listed = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                                .text = ran->out, .size = ran->out_size, .nodes_more = 0u, .text_more = 0ull,
                                .err_msg = &unread, .owner = &g_cli_toolchains_cells);
        }
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    }
    mmgr_line_ch(args->out, '[');
    unsigned int written = 0u;
    for (unsigned int index = 0u; started && (index < args->count); index += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = args->requires[index], .cap = CLI_CELLS_POOL});
        int installed = 0;
        for (unsigned int at = 0u;
             !installed && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                       .json_read = &json, .array = listed, .index = at) != CLI_JSON_NONE);
             at += 1u)
        {
            unsigned long long name_length = 0ull;
            const char *const name = EMBED_CALL(
                cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                   .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                        .json_read = &json, .array = listed, .index = at),
                                   .name = "name"),
                .length_pointer = &name_length);
            installed =
                (name != NULL) && EMBED_CALL(cli_package_same, EnvsResolveCtx, .one = name, .one_length = name_length,
                                             .other = args->requires[index], .other_length = length);
        }
        if (!installed)
        {
            mmgr_line_put(args->out, (written != 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                       .text = args->requires[index], .length = length);
            written += 1u;
        }
    }
    mmgr_line_ch(args->out, ']');
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}
