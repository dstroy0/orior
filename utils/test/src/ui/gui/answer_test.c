// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer_test.c: the answers' check: a whole number is read off a node, a number that is no whole number, a string
// and a missing member read as the otherwise, and a member is true where it is
#include "../../../../../ui/gui/src/answer.h"
#include "../cli/cli_check.h"

static const char s_text[] = "{\"lines\":1234,\"size\":-5,\"name\":\"7\",\"windowed\":true,\"paged\":false,\"zero\":0}";

int main(void)
{
    (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
    CliJson json;
    char err_text[1024];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = s_text, .size = sizeof(s_text) - 1u, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    cli_check(root != CLI_JSON_NONE, "the text is read as JSON");
    cli_check(EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "lines", .otherwise = 9ull) == 1234ull, "a whole number is read off its member");
    cli_check(EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "zero", .otherwise = 9ull) == 0ull, "zero is read as zero");
    cli_check(EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "size", .otherwise = 9ull) == 9ull, "a number below zero reads as the otherwise");
    cli_check(EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "name", .otherwise = 9ull) == 9ull, "a string reads as the otherwise");
    cli_check(EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "none", .otherwise = 9ull) == 9ull, "a member not there reads as the otherwise");
    cli_check(EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = &json, .node = CLI_JSON_NONE, .otherwise = 3ull) == 3ull, "no node reads as the otherwise");
    cli_check(EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "windowed"), "a true member is true");
    cli_check(!EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "paged") && !EMBED_CALL(gui_answer_object.member_true, GuiAnswerObjectCfg, .json = &json, .object = root, .name = "none"),
              "a false member and a missing one are not");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return cli_on_check();
}
