// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// host_test.c: the sandbox's check: each program on the machine gets a stand-in that runs it there
#include "../../../../../ui/cli/src/machine/host.h"
#include "cli_check.h"

int main(void)
{
    static char s_text[8192];
    CliLine line = {s_text, sizeof(s_text) - 1u, 0ull};
    cli_stand_in("cargo", &line);
    s_text[line.at] = '\0';
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = s_text, .other = "#!/bin/sh\n", .cap = CLI_CELLS_POOL}),
              "a stand-in is a shell script");
    cli_check(
        cellul.find(&(CatenaFinitaCfg){
            .src = s_text, .cap = CLI_CELLS_POOL, .other = "set -- 'cargo' \"$@\"", .other_cap = CLI_CELLS_POOL}) !=
            NULL,
        "it runs its program with the words it is given");
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_text,
                  .cap = CLI_CELLS_POOL,
                  .other = "exec flatpak-spawn --host --watch-bus --directory=\"$PWD\" ${TERM+\"--env=TERM=$TERM\"}",
                  .other_cap = CLI_CELLS_POOL}) != NULL,
              "it runs it on the machine, in its folder, with the terminal's kind");
    cli_check((line.at >= 5u) && (cellul.eq(&(CatenaFinitaCfg){
                                     .src = s_text + line.at - 5u, .other = "\"$@\"\n", .cap = CLI_CELLS_POOL})),
              "it ends with the words given");
    line.at = 0ull;
    cli_stand_in("it's", &line);
    s_text[line.at] = '\0';
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_text, .cap = CLI_CELLS_POOL, .other = "'it'\\''s'", .other_cap = CLI_CELLS_POOL}) != NULL,
              "a quote in a name is closed, escaped and opened again");
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("host_stand_ins", folder);
    (void)query_removed(folder);
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    const long long made = cli_stand_ins_written(folder, "git\nflatpak-spawn\norior\n../x\n\n", &said);
    cli_check(made == 1ll, "orior_cli and the program that reaches the machine keep running inside");
    cli_full(folder, "git", path);
    const int git = query_is_file(path);
    cli_full(folder, "flatpak-spawn", path);
    cli_check(git && !query_is_file(path), "git gets a stand-in, flatpak-spawn none");
    return cli_checked();
}
