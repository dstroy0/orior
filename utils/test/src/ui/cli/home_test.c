// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// home_test.c: orior's own folder's check: the menus made for the reader are JSON with nothing laid out
#include "cli_check.h"

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char home[CLI_PATH_MAX_LEN];
    cli_check_folder("home_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    char made_path[CLI_PATH_MAX_LEN];
    cli_check(cli_home_ensure_menus(made_path, &err_msg), "the reader's menus are made");
    static char s_text[1u << 16u];
    const unsigned long long size = qry_member_file_read(made_path, (unsigned char *)s_text, sizeof(s_text));
    unsigned int made = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_text, (size == QRY_MEMBER_UNREAD) ? 0ull : size, &made);
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = cli_check_member_text(made, "about"),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "hide",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "what the file states of itself names hide");
    static const char *const s_keys[3] = {"menus", "hide", "toolbar"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned int list = cli_json_member(json, made, s_keys[at]);
        char what[64];
        VerbaLinea line = {what, sizeof(what), 0ull};
        mmgr_line_put(&line, s_keys[at]);
        mmgr_line_put(&line, " is an empty list");
        (void)mmgr_line_finish(&line);
        cli_check(cli_json_is(json, list, CFG_JSON_ARRAY) && (json->node[list].count == 0u), what);
    }
    return cli_checked();
}
