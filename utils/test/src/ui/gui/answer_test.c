// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer_test.c: the answers' check: a whole number is read off a node, a number that is no whole number, a string
// and a missing member read as the otherwise, and a member is true where it is
#include "../../../../../ui/gui/src/answer.h"
#include "../cli/cli_check.h"

static const char s_text[] = "{\"lines\":1234,\"size\":-5,\"name\":\"7\",\"windowed\":true,\"paged\":false,\"zero\":0}";

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    CliJson json;
    char err_text[1024];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    const unsigned int root = cli_json_taken(&json, s_text, sizeof(s_text) - 1u, 0u, 0ull, &err_msg);
    cli_check(root != CLI_JSON_NONE, "the text is read as JSON");
    cli_check(gui_answer_member_whole(&json, root, "lines", 9ull) == 1234ull, "a whole number is read off its member");
    cli_check(gui_answer_member_whole(&json, root, "zero", 9ull) == 0ull, "zero is read as zero");
    cli_check(gui_answer_member_whole(&json, root, "size", 9ull) == 9ull, "a number below zero reads as the otherwise");
    cli_check(gui_answer_member_whole(&json, root, "name", 9ull) == 9ull, "a string reads as the otherwise");
    cli_check(gui_answer_member_whole(&json, root, "none", 9ull) == 9ull, "a member not there reads as the otherwise");
    cli_check(gui_answer_whole(&json, CLI_JSON_NONE, 3ull) == 3ull, "no node reads as the otherwise");
    cli_check(gui_answer_member_true(&json, root, "windowed"), "a true member is true");
    cli_check(!gui_answer_member_true(&json, root, "paged") && !gui_answer_member_true(&json, root, "none"),
              "a false member and a missing one are not");
    cli_json_given(&json);
    return cli_checked();
}
