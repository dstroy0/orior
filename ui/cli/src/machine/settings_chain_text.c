// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_chain_text.c: a chain of settings as the window reads it and hands it back, and a chain's tree given back
#include "../text/text.h"
#include "settings_internal.h"

int cli_settings_chain_read(const CliSettingsChainCfg *args)
{
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->chain->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    args->chain->files = 0u;
    args->chain->problems = 0u;
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &args->chain->json,
                   .text = args->text, .size = args->length, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = args->err_msg, .owner = &g_cli_machine_cells);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &args->chain->json,
                    .node = root, .kind = CFG_JSON_OBJECT))
    {
        return 0;
    }
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &args->chain->json, .object = root, .name = "files");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = &args->chain->json, .node = files, .kind = CFG_JSON_ARRAY)
                                 ? args->chain->json.node[files].first
                                 : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (args->chain->files < CLI_SETTINGS_DEEPEST);
         item = args->chain->json.node[item].next)
    {
        const char *const file =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &args->chain->json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &args->chain->json, .object = item, .name = "file"),
                       .length_pointer = NULL);
        const unsigned int values = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = &args->chain->json, .object = item, .name = "values");
        if ((file == NULL) || !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &args->chain->json, .node = values, .kind = CFG_JSON_OBJECT))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = args->chain->file[args->chain->files],
                                .src = file,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
        args->chain->values[args->chain->files] = values;
        args->chain->files += 1u;
    }
    const unsigned int problems = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = &args->chain->json, .object = root, .name = "problems");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = &args->chain->json, .node = problems, .kind = CFG_JSON_ARRAY)
                                 ? args->chain->json.node[problems].first
                                 : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (args->chain->problems < CLI_SETTINGS_PROBLEMS);
         item = args->chain->json.node[item].next)
    {
        const char *const problem = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                               .json_read = &args->chain->json, .node = item, .length_pointer = NULL);
        if (problem != NULL)
        {
            VerbaLinea line = {args->chain->problem[args->chain->problems], CLI_PATH_MAX_LEN * 2u, 0ULL};
            mmgr_line_put(&line, problem);
            mmgr_line_finish(&line);
            args->chain->problems += 1u;
        }
    }
    return 1;
}

void cli_settings_chain_free(const CliSettingsPathCfg *args)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->chain->json);
    args->chain->files = 0u;
    args->chain->problems = 0u;
}
