// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_net.c: an address's host is read from a URL, an ssh address and a host with a port; localhost names are local; and a
// folder or file on this machine is a path
#include "ui/cli/src/machine/net.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the host parsed from `address` is `required`
static void assert_host(const char *required, const char *address)
{
    char host[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_net_address_host.host_get, CliNetAddressHostCfg, .address = address, .host = host);
    TEST_ASSERT_EQUAL_STRING(required, host);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_host_of_a_url_drops_scheme_port_path_and_query(void)
{
    assert_host("api.example.com", "https://api.example.com:8443/v1/items?x=1");
}

void test_host_of_a_scp_style_ssh_address(void)
{
    assert_host("github.com", "git@github.com:dstroy0/orior.git");
}

void test_host_of_an_ssh_url_drops_the_user_and_port(void)
{
    assert_host("build.example.org", "ssh://me@build.example.org:2222/srv");
}

void test_host_with_a_port(void)
{
    assert_host("db.example.net", "db.example.net:5432");
}

void test_host_of_a_bracketed_ipv6_address(void)
{
    assert_host("::1", "[::1]:8080");
}

void test_host_of_a_url_on_an_ipv4_address(void)
{
    assert_host("127.0.0.1", "http://127.0.0.1:3000/echo");
}

void test_loopback_names_are_local(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_host.local, CliNetHostCfg, .host_read = "127.0.0.1"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_host.local, CliNetHostCfg, .host_read = "localhost"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_host.local, CliNetHostCfg, .host_read = "::1"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_host.local, CliNetHostCfg, .host_read = "tauri.localhost"));
}

void test_other_hosts_are_not_local(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_net_host.local, CliNetHostCfg, .host_read = "github.com"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_net_host.local, CliNetHostCfg, .host_read = "10.0.0.5"));
}

void test_folders_and_files_on_this_machine_are_paths(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_address.path, CliNetAddressCfg, .address = "D:/repos/settings"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_address.path, CliNetAddressCfg, .address = "C:\\remote.git"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_address.path, CliNetAddressCfg, .address = "/srv/git/one.git"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_address.path, CliNetAddressCfg, .address = "../shared"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_net_address.path, CliNetAddressCfg, .address = "file:///srv/one.git"));
}

void test_remote_addresses_are_not_paths(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_net_address.path, CliNetAddressCfg, .address = "git@github.com:dstroy0/orior.git"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_net_address.path, CliNetAddressCfg, .address = "https://github.com/dstroy0/orior"));
}
