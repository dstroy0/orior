// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// home_test.c: orior's own folder's check: the menus made for the reader are JSON with nothing laid out
#include "../../../../../ui/cli/src/window/serve_internal.h"
#include "../../../../../ui/cli/src/window/window_internal.h"
#include "cli_check.h"

// one of the window's calls on orior's folder, `name`, answered from `size` bytes of its arguments into `out`: 1, or 0
// with why into `err_msg`, a name the folder's calls do not hold among them
static int home_called(const char *name, const char *args, unsigned long long size, VerbaLinea *out, VerbaLinea *err_msg)
{
    for (unsigned int at = 0u; at < g_cli_window_home_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_window_home_calls[at].name, .other = name, .cap = 64ull}))
        {
            return cli_serve_answered(g_cli_window_home_calls[at].answer, args, size, out, err_msg);
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
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

    // the entries stored: a write stores the entries it changes with the rest as they stand, and takes out one given
    // as null
    char out_text[4096];
    VerbaLinea out = {out_text, sizeof(out_text), 0ull};
    static const char s_first[] = "{\"changes\":{\"orior.one\":\"a\",\"orior.two\":\"b\"}}";
    cli_check(home_called("state_write", s_first, sizeof(s_first) - 1u, &out, &err_msg), "two entries are stored");
    static const char s_second[] = "{\"changes\":{\"orior.one\":\"c\",\"orior.three\":\"x\"}}";
    cli_check(home_called("state_write", s_second, sizeof(s_second) - 1u, &out, &err_msg), "one changed and one added are stored");
    out.at = 0ull;
    cli_check(home_called("state_read", "{}", 2ull, &out, &err_msg), "the entries read back");
    unsigned int stored = CLI_JSON_NONE;
    const CliJson *read = cli_check_parsed(out.out, out.at, &stored);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_json_string(read, cli_json_member(read, stored, "orior.one"), NULL), .other = "c", .cap = 8ull}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = cli_json_string(read, cli_json_member(read, stored, "orior.two"), NULL), .other = "b", .cap = 8ull}) &&
                  (cli_json_member(read, stored, "orior.three") != CLI_JSON_NONE),
              "the second write changed one entry, kept the other and added the third");
    static const char s_third[] = "{\"changes\":{\"orior.two\":null}}";
    out.at = 0ull;
    cli_check(home_called("state_write", s_third, sizeof(s_third) - 1u, &out, &err_msg), "an entry given as null is stored");
    out.at = 0ull;
    cli_check(home_called("state_read", "{}", 2ull, &out, &err_msg), "the entries read back again");
    read = cli_check_parsed(out.out, out.at, &stored);
    cli_check((cli_json_member(read, stored, "orior.two") == CLI_JSON_NONE) && (cli_json_member(read, stored, "orior.one") != CLI_JSON_NONE) &&
                  (cli_json_member(read, stored, "orior.three") != CLI_JSON_NONE),
              "the null took its entry out and left the others");
    return cli_checked();
}
