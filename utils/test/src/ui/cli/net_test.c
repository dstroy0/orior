// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// net_test.c: reaching's check: an address's host is read from a URL, an ssh address and a host with its port; this
// machine's hosts are its own; and a folder or a file of the machine is a path
#include "../../../../../ui/cli/src/machine/net.h"
#include "cli_check.h"

// the host of `address` checked against `wanted`
static void net_host_checked(const char *address, const char *wanted)
{
    char host[CLI_PATH_LONGEST];
    cli_net_host_of(address, host);
    char what[256];
    CliLine line = {what, sizeof(what), 0ull};
    cli_line_text(&line, address);
    cli_line_text(&line, " is on ");
    cli_line_text(&line, wanted);
    (void)cli_line_closed(&line);
    cli_check_text(host, cellul.len(&(CatenaFinitaCfg){.src = host, .cap = CLI_CELLS_POOL}), wanted, what);
}

int main(void)
{
    net_host_checked("https://api.example.com:8443/v1/items?x=1", "api.example.com");
    net_host_checked("git@github.com:dstroy0/orior.git", "github.com");
    net_host_checked("ssh://me@build.example.org:2222/srv", "build.example.org");
    net_host_checked("db.example.net:5432", "db.example.net");
    net_host_checked("[::1]:8080", "::1");
    net_host_checked("http://127.0.0.1:3000/echo", "127.0.0.1");
    cli_check(cli_net_local("127.0.0.1") && cli_net_local("localhost") && cli_net_local("::1") &&
                  cli_net_local("tauri.localhost"),
              "this machine's hosts are its own");
    cli_check(!cli_net_local("github.com") && !cli_net_local("10.0.0.5"), "another machine's are not");
    cli_check(cli_net_path("D:/repos/settings") && cli_net_path("C:\\remote.git") && cli_net_path("/srv/git/one.git") &&
                  cli_net_path("../shared") && cli_net_path("file:///srv/one.git"),
              "folders and files of the machine are paths");
    cli_check(!cli_net_path("git@github.com:dstroy0/orior.git") && !cli_net_path("https://github.com/dstroy0/orior"),
              "remote addresses are not");
    return cli_checked();
}
