// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// defs_test.c: the definitions' check: the tree gives every type of a ruleset's files with its columns, the head of
// a record, and the languages' tables
#include "../../../../../ui/cli/src/editor/defs.h"
#include "cli_check.h"

int main(void)
{
    static char s_read[1u << 22u];
    static const char *const s_exts[4] = {"g", "gsm", "krs", "kcr"};
    static const char *const s_columns[5] = {"where", "who", "kind", "form", "gloss"};
    char root[CLI_PATH_LONGEST];
    if (!cli_tree_found(root))
    {
        cli_check(1, "no tree is found here, and the definitions are not read");
        return cli_checked();
    }
    CliLine out = {s_read, sizeof(s_read), 0ull};
    cli_defs_read(root, &out);
    cli_check((g_cli_defs_cells.holds == 0ull) && (g_cli_defs_cells.hw > 0ull),
              "the definitions are read through cells of their size, each given back");
    unsigned int read = 0u;
    const CliJson *const json = cli_check_parsed(s_read, out.at, &read);
    const unsigned int types = cli_json_member(json, read, "types");
    for (unsigned int ext = 0u; ext < 4u; ext += 1u)
    {
        unsigned int found = CLI_JSON_NONE;
        for (unsigned int at = 0u; cli_json_item(json, types, at) != CLI_JSON_NONE; at += 1u)
        {
            const unsigned int type = cli_json_item(json, types, at);
            found = (cellul.eq(&(CatenaFinitaCfg){
                        .src = cli_check_member_text(type, "ext"), .other = s_exts[ext], .cap = CLI_CELLS_POOL}))
                        ? type
                        : found;
        }
        const unsigned int columns = cli_json_member(json, cli_json_member(json, found, "table"), "columns");
        int same = (found != CLI_JSON_NONE) && (cli_json_item(json, columns, 5u) == CLI_JSON_NONE);
        for (unsigned int column = 0u; same && (column < 5u); column += 1u)
        {
            const char *const name = cli_json_string(json, cli_json_item(json, columns, column), NULL);
            same = (name != NULL) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_columns[column], .cap = CLI_CELLS_POOL}));
        }
        char what[64];
        CliLine line = {what, sizeof(what), 0ull};
        cli_line_text(&line, "the ");
        cli_line_text(&line, s_exts[ext]);
        cli_line_text(&line, " table has its five columns");
        (void)cli_line_closed(&line);
        cli_check(same, what);
    }
    const unsigned int head = cli_json_member(json, read, "head");
    const unsigned int bytes = cli_json_member(json, head, "bytes");
    cli_check(
        cli_json_is(json, bytes, CFG_JSON_NUMBER) && (json->node[bytes].length == 2u) &&
            (memor.cmp(&(MemoriaCfg){.src = json->text + json->node[bytes].text, .other = "16", .bytes = 2u}) == 0) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(head, "magic"), .other = "KREP", .cap = CLI_CELLS_POOL})),
        "the head is 16 bytes, its magic KREP");
    const unsigned int languages = cli_json_member(json, read, "languages");
    int asm_table = 0;
    for (unsigned int at = 0u; cli_json_item(json, languages, at) != CLI_JSON_NONE; at += 1u)
    {
        asm_table |=
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, languages, at), "name"),
                                          .other = "gnascor_asm_lng.tsv",
                                          .cap = CLI_CELLS_POOL}));
    }
    cli_check(asm_table, "the languages hold gnascor_asm_lng.tsv");
    return cli_checked();
}
