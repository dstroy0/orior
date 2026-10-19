// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// templates_test.c: the templates' check: a project is begun from a template with its name written in
#include "../../../../../ui/cli/src/plugins/templates.h"
#include "cli_check.h"

int main(void)
{
    char home[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char parent[CLI_PATH_LONGEST];
    char made[CLI_PATH_LONGEST];
    cli_check_folder("templates_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
    cli_full(home, "templates/script/{{name}}", path);
    (void)query_folder_made(path);
    cli_full(home, "templates/script/{{name}}/main.py", path);
    (void)query_file_written(path, "\"\"\"{{name}}.\"\"\"\n", 16ull);
    cli_full(home, "templates/script/logo.bin", path);
    (void)query_file_written(path, "\xff\xfe\x00", 3ull);
    char list[256];
    CliLine listed = {list, sizeof(list), 0ull};
    cli_templates_list(&listed);
    cli_check_text(list, listed.at, "[\"script\"]", "the templates list");
    cli_full(home, "work", parent);
    (void)query_folder_made(parent);
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_templates_create("script", parent, "tally", made, &said), "a project begins from the template");
    char text[64];
    cli_full(made, "tally/main.py", path);
    unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text));
    cli_check_text(text, (size == QRY_MEMBER_UNREAD) ? 0ull : size, "\"\"\"tally.\"\"\"\n", "its name is written in");
    cli_full(made, "logo.bin", path);
    size = qry_member_file_read(path, (unsigned char *)text, sizeof(text));
    cli_check((size == 3ull) && (memor.cmp(&(MemoriaCfg){.src = text, .other = "\xff\xfe\x00", .bytes = 3u}) == 0),
              "a file that is no text is copied as it is");
    said.at = 0ull;
    cli_check(!cli_templates_create("script", parent, "tally", made, &said),
              "a folder there already is not written over");
    said.at = 0ull;
    const int climbs = cli_templates_create("../x", parent, "y", made, &said);
    said.at = 0ull;
    cli_check(!climbs && !cli_templates_create("script", parent, "a/b", made, &said),
              "a template or a name that leaves its folder is refused");
    return cli_checked();
}
