// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_plugins_program.c: what a plugin's server starts as: the program its toolchain finds, and the arguments and
// options it starts with, `{folder}` in each the folder the program was found in
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the program a plugin's server starts as, found by its toolchain where `server` names one, into `program`, and the
// folder it was found in into `folder`, each sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_servers_plugin_program(const CliServersErrMsgJsonCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->server_u32, .name = "program"),
                   .length_pointer = NULL);
    const char *const toolchain =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->server_u32, .name = "toolchain"),
                   .length_pointer = NULL);
    args->folder_out[0] = '\0';
    if (name == NULL)
    {
        mmgr_line_put(err_msg, "the plugin's server names no program");
        return 0;
    }
    if (toolchain == NULL)
    {
        VerbaLinea whole = {args->program, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&whole, name);
        return mmgr_line_finish(&whole) != 0ull;
    }
    CliJson manifest_json;
    const unsigned int tool =
        EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &manifest_json,
                   .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                          .json_out = &manifest_json, .nodes_more = 0u, .text_more = 0ull),
                   .id = toolchain, .err_msg = err_msg);
    CliFound found = {"missing", {0}, {0}, 0};
    if (tool != CLI_JSON_NONE)
    {
        unsigned int count = 0u;
        char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_taken(&count, &g_cli_servers_cells);
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &manifest_json, .tool = tool,
                   .path_callback = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count, .found = &found);
        g_cli_cells->persistent_buf_release(folders, &g_cli_servers_cells);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &manifest_json);
    const char *const names[1] = {name};
    if ((tool == CLI_JSON_NONE) || (found.folder[0] == '\0') ||
        !EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = found.folder, .names = names,
                    .count = 1u, .path = args->program))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens ");
        mmgr_line_put(err_msg, toolchain);
        mmgr_line_put(err_msg, "'s install page or takes its folder");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->folder_out, .src = found.folder, .bytes = CLI_PATH_MAX_LEN});
    return 1;
}

// the `length` bytes of `text` written into `written`, each `{folder}` in them written as `folder`, escaped for a JSON
// string where `escaped`
void cli_servers_plugin_folder_put(const CliServersTextCtx *args)
{
    const unsigned long long folder_length = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        const int named =
            ((args->length - at) >= 8ull) &&
            cellul.starts(&(CatenaFinitaCfg){.src = args->text + at, .other = "{folder}", .cap = CLI_CELLS_POOL});
        if (named && args->escaped)
        {
            EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->written,
                       .text_bytes = (const unsigned char *)args->folder, .length = folder_length);
        }
        else if (named)
        {
            mmgr_line_put_n(args->written, args->folder, folder_length);
        }
        else
        {
            mmgr_line_ch(args->written, args->text[at]);
        }
        at += named ? 7ull : 0ull;
    }
}

// the arguments of `server` put into `words`, each `{folder}` in one written as `folder`
void cli_servers_plugin_args(const CliServersJsonServerCtx *args)
{
    const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = args->server_u32, .name = "args");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = arguments, .kind = CFG_JSON_ARRAY)
                                 ? args->json->node[arguments].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = args->json->node[item].next)
    {
        const char *const arg = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                           .json_read = args->json, .node = item, .length_pointer = NULL);
        const char *const text = (arg != NULL) ? arg : "";
        char word[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea written = {word, sizeof(word), 0ull};
        EMBED_CALL(cli_servers_text_internal.plugin_folder_put, CliServersTextCtx, .written = &written, .text = text,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .folder = args->folder,
                   .escaped = 0);
        if (mmgr_line_finish(&written) != 0ull)
        {
            EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = args->words,
                       .word = word);
        }
    }
}
