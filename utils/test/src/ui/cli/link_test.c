// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_test.c: the link's check: addresses read each way in, a word with a quote in it stays one word, and a machine's
// target is read from uname
#include "../../../../../ui/cli/src/comms/link.h"
#include "cli_check.h"

// whether `text` reads as an address, into `address`
static int link_parsed(const char *text, CliLinkAddress *address)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_link_address_parse(text, address, &said);
}

// addresses read each way in
static void link_addresses(void)
{
    CliLinkAddress ssh;
    CliLinkAddress again;
    cli_check(link_parsed("me@box.lan:/home/me/tree", &ssh) && (ssh.way == CLI_LINK_SSH) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = ssh.name, .other = "me@box.lan", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = ssh.folder, .other = "/home/me/tree", .cap = CLI_CELLS_POOL})),
              "an address of `user@host:folder` is ssh's");
    char machine[CLI_PATH_LONGEST];
    CliLine line = {machine, sizeof(machine), 0ull};
    cli_link_machine(&ssh, &line);
    (void)cli_line_closed(&line);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = machine, .other = "box.lan", .cap = CLI_CELLS_POOL}),
              "its machine is its host");
    char text[CLI_PATH_LONGEST * 2u];
    CliLine written = {text, sizeof(text), 0ull};
    cli_link_address_text(&ssh, &written);
    (void)cli_line_closed(&written);
    cli_check(link_parsed(text, &again) && (again.way == ssh.way) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = again.name, .other = ssh.name, .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = again.folder, .other = ssh.folder, .cap = CLI_CELLS_POOL})),
              "an address written reads back as itself");
    CliLinkAddress docker;
    cli_check(
        link_parsed("docker:dev-1:/workspaces/tree", &docker) && (docker.way == CLI_LINK_DOCKER) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = docker.name, .other = "dev-1", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = docker.folder, .other = "/workspaces/tree", .cap = CLI_CELLS_POOL})),
        "an address in a container is docker's");
    CliLinkAddress wsl;
    cli_check(link_parsed("wsl:Ubuntu:/home/me/tree", &wsl) && (wsl.way == CLI_LINK_WSL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = wsl.name, .other = "Ubuntu", .cap = CLI_CELLS_POOL})),
              "and one in a distribution WSL's");
    CliLinkAddress none;
    cli_check(!link_parsed("box", &none) && !link_parsed("docker:dev", &none) && !link_parsed("box:", &none),
              "an address with no folder is none");
}

int main(void)
{
    link_addresses();
    char quoted[64];
    CliLine line = {quoted, sizeof(quoted), 0ull};
    cli_link_quote("it's", 4ull, &line);
    cli_check_text(quoted, line.at, "'it'\\''s'", "a word with a quote in it stays one word");
    char target[CLI_NAME_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(
        cli_link_target("Linux x86_64", target, &said) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = target, .other = "x86_64-unknown-linux-musl", .cap = CLI_CELLS_POOL})),
        "a Linux machine's target is read from uname");
    cli_check(cli_link_target("Linux aarch64", target, &said) &&
                  (cellul.eq(
                      &(CatenaFinitaCfg){.src = target, .other = "aarch64-unknown-linux-musl", .cap = CLI_CELLS_POOL})),
              "on either processor");
    cli_check(!cli_link_target("Darwin arm64", target, &said), "and a Mac has none");
    cli_check(cli_link_is_windows("MINGW64_NT-10.0-26200 x86_64"), "a Windows machine is known by its uname");
    return cli_checked();
}
