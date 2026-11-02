// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_link.c: remote links: addresses parse for ssh, docker and WSL, a quoted word stays one word, and a machine's target triple
// is read from uname
#include "ui/cli/src/comms/link.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_error[256];
static char s_target[CLI_NAME_MAX_LEN];

// `text` parsed into `address`: whether it is an address
static int parsed(const char *text, CliLinkAddress *address)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    return EMBED_CALL(cli_link_text.address_parse, CliLinkTextCfg, .text = text, .address_out = address, .err_msg = &err_msg);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_user_at_host_colon_folder_is_ssh(void)
{
    CliLinkAddress address;
    TEST_ASSERT_TRUE(parsed("me@box.lan:/home/me/tree", &address));
    TEST_ASSERT_EQUAL_INT(CLI_LINK_SSH, address.way);
    TEST_ASSERT_EQUAL_STRING("me@box.lan", address.name);
    TEST_ASSERT_EQUAL_STRING("/home/me/tree", address.folder);
}

void test_an_ssh_address_names_its_host_as_the_machine(void)
{
    CliLinkAddress address;
    TEST_ASSERT_TRUE(parsed("me@box.lan:/home/me/tree", &address));
    char machine[CLI_PATH_MAX_LEN];
    VerbaLinea line = {machine, sizeof(machine), 0ull};
    EMBED_CALL(cli_link_out.machine, CliLinkOutCfg, .address = &address, .out = &line);
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING("box.lan", machine);
}

void test_a_written_address_parses_back_to_itself(void)
{
    CliLinkAddress address;
    CliLinkAddress again;
    TEST_ASSERT_TRUE(parsed("me@box.lan:/home/me/tree", &address));
    char text[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea written = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_link_out.address_text, CliLinkOutCfg, .address = &address, .out = &written);
    (void)mmgr_line_finish(&written);
    TEST_ASSERT_TRUE(parsed(text, &again));
    TEST_ASSERT_EQUAL_INT(address.way, again.way);
    TEST_ASSERT_EQUAL_STRING(address.name, again.name);
    TEST_ASSERT_EQUAL_STRING(address.folder, again.folder);
}

void test_a_docker_address(void)
{
    CliLinkAddress address;
    TEST_ASSERT_TRUE(parsed("docker:dev-1:/workspaces/tree", &address));
    TEST_ASSERT_EQUAL_INT(CLI_LINK_DOCKER, address.way);
    TEST_ASSERT_EQUAL_STRING("dev-1", address.name);
    TEST_ASSERT_EQUAL_STRING("/workspaces/tree", address.folder);
}

void test_a_wsl_address(void)
{
    CliLinkAddress address;
    TEST_ASSERT_TRUE(parsed("wsl:Ubuntu:/home/me/tree", &address));
    TEST_ASSERT_EQUAL_INT(CLI_LINK_WSL, address.way);
    TEST_ASSERT_EQUAL_STRING("Ubuntu", address.name);
}

void test_an_address_with_no_folder_is_refused(void)
{
    CliLinkAddress address;
    TEST_ASSERT_FALSE(parsed("box", &address));
    TEST_ASSERT_FALSE(parsed("docker:dev", &address));
    TEST_ASSERT_FALSE(parsed("box:", &address));
}

void test_a_quote_in_a_word_is_escaped(void)
{
    char quoted[64];
    VerbaLinea line = {quoted, sizeof(quoted), 0ull};
    EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = "it's", .size = 4ull, .out = &line);
    CLI_TEST_ASSERT_TEXT("'it'\\''s'", quoted, line.at);
}

