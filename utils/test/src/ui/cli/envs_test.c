// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_test.c: the environments' check: environments are found at any depth with what beside them requires
#include "../../../../../ui/cli/src/toolchains/envs.h"
#include "cli_check.h"

static char s_root[CLI_PATH_LONGEST];

// `text` written to the file `name` under the root, its folder made
static void envs_written(const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    if (cli_path_parent(path, folder))
    {
        (void)query_folder_made(folder);
    }
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

int main(void)
{
    cli_check_folder("envs", s_root);
    envs_written(".venv/pyvenv.cfg", "home = x\n");
    envs_written(".venv/Lib/site-packages/pyvenv.cfg", "not searched\n");
    envs_written("setup.cfg", "[metadata]\nname = a\n\n[options]\ninstall_requires =\n    numpy>=1.20\n    scipy\n"
                              "python_requires = >=3.10\n");
    envs_written("tools/sub/Pipfile", "[[source]]\nurl = \"x\"\n\n[packages]\nrequests = \"*\"\n\"click\" = \">=8\"\n\n"
                                      "[dev-packages]\npytest = \"*\"\n");
    envs_written("nixed/shell.nix", "{ }\n");
    envs_written("node_modules/x/pyvenv.cfg", "not searched\n");
    static char s_found[1u << 16u];
    CliLine out = {s_found, sizeof(s_found), 0ull};
    cli_envs_found(s_root, &out);
    unsigned int found = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_found, out.at, &found);
    static char s_shown[4096];
    CliLine shown = {s_shown, sizeof(s_shown), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, found, at);
        cli_line_text(&shown, (at > 0u) ? "|" : "");
        cli_line_text(&shown, cli_check_member_text(one, "kind"));
        cli_line_character(&shown, ' ');
        cli_line_text(&shown, cli_check_member_text(one, "place"));
        cli_line_character(&shown, ' ');
        const unsigned int needs = cli_json_member(json, one, "requires");
        for (unsigned int item = 0u; cli_json_item(json, needs, item) != CLI_JSON_NONE; item += 1u)
        {
            const char *const name = cli_json_string(json, cli_json_item(json, needs, item), NULL);
            cli_line_text(&shown, (item > 0u) ? "," : "");
            cli_line_text(&shown, (name != NULL) ? name : "");
        }
    }
    cli_check_text(s_shown, shown.at, "venv .venv numpy,scipy|nix nixed |pipenv tools/sub requests,click",
                   "each environment is found with what beside it requires, nearest the top first");
    return cli_checked();
}
