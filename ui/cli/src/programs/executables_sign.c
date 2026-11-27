// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_sign.c: what each system's signing shares: the settings that say how the programs a build makes are
// signed, and a line printed of a signature. Each system's steps and its report of a signature are in its platform
// file, windows_sign.c and posix_sign.c
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "executables_internal.h"

void cli_executables_sign_print(const CliExecutablesSignCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

int cli_executables_signing(const CliExecutablesSignCtx *args)
{
    char *out = args->out;
    char home[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home) ||
        !EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = home,
                    .name = "settings.json", .out = path))
    {
        return 0;
    }
    // settings.json read into a tree sized to it. The tree goes back after the setting's copy
    CliJson json = {0};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &json, .path = path,
                   .nodes_more = 0u, .text_more = 0ULL, .err_msg = &reason, .owner = g_cli_programs_owner);
    char named[CLI_NAME_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ULL};
    mmgr_line_put(&line, "signing.");
    mmgr_line_put(&line, args->key);
    mmgr_line_finish(&line);
    unsigned long long length = 0ULL;
    const char *value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                      .json_read = &json, .object = root, .name = named),
                                   .length_pointer = &length);
    if (value != NULL)
    {
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &value,
                   .length_pointer = &length);
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = value,
                   .length = length, .out = out, .capacity = CLI_PATH_MAX_LEN);
    }
    g_platform_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(platform_pool_series(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return (value != NULL) && (length != 0ULL);
}
