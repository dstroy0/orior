// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer.c: what the window reads off the CLI's answers and events
#include "answer.h"

unsigned long long gui_answer_whole(const GuiAnswerCfg *args)
{
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                    .node = args->node, .kind = CFG_JSON_NUMBER))
    {
        return args->otherwise;
    }
    const CliJsonNode *const number = &args->json->node[args->node];
    const char *const text = args->json->text + number->text;
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < number->length; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return args->otherwise;
        }
        // a digit's value is its distance from '0'
        value = (value * 10ull) + (unsigned long long)(text[at] - '0');
    }
    return (number->length != 0ull) ? value : args->otherwise;
}

unsigned long long gui_answer_member_whole(const GuiAnswerObjectCfg *args)
{
    return EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = args->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->object, .name = args->name),
                      .otherwise = args->otherwise);
}

int gui_answer_member_true(const GuiAnswerObjectCfg *args)
{
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->object, .name = args->name),
                      .kind = CFG_JSON_TRUE);
}
