// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_tree_test.c: the trees' check: a line of a tree's written room holds whatever the tree writes on one line
#include "../../../../../ui/cli/src/command_line/cells.h"
#include "../../../../../ui/cli/src/text/json_tree.h"
#include "cli_check.h"

// `text` read, written on a line of its tree's room and checked against `wanted`
static void json_room_holds(const char *text, const char *wanted, const char *what)
{
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                             0u, 0ull, &err_msg, &g_cli_text_cells);
    cli_check(root != CLI_JSON_NONE, what);
    const unsigned long long room = cli_json_written_room(&json);
    char *const written = g_cli_cells->temporary_buf_alloc(room, &g_cli_text_cells);
    VerbaLinea line = mmgr_line_over(written, room);
    cli_json_write(&json, root, &line, 0u, 0);
    cli_check(mmgr_line_ok(&line), "the line of its written room holds what it writes");
    cli_check_text((written != NULL) ? written : "", line.at, wanted, "whole");
    g_cli_cells->persistent_buf_release(written, &g_cli_text_cells);
    cli_json_given(&json);
}

static void json_written_room(void)
{
    json_room_holds("{}", "{}", "an empty object");
    json_room_holds("[]", "[]", "an empty array");
    json_room_holds("[null,null,null,true,false]", "[null,null,null,true,false]", "literals, which have no text");
    json_room_holds("[[[[[[[[]]]]]]]]", "[[[[[[[[]]]]]]]]", "arrays in arrays, whose nodes have no text");
    json_room_holds("{\"\\u0001\\u0002\":\"\\u0003\\u0004\\u0005\"}", "{\"\\u0001\\u0002\":\"\\u0003\\u0004\\u0005\"}",
                    "a key and a string each byte of which is written as six");
    json_room_holds("{\"a\":-1.5e300,\"b\":[\"\\\"\",\"\\\\\"],\"\":\"\"}", "{\"a\":-1.5e300,\"b\":[\"\\\"\",\"\\\\\"],\"\":\"\"}",
                    "a number, escapes and an empty key");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    json_written_room();
    return cli_checked();
}
