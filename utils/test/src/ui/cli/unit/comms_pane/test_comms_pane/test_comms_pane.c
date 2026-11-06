// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_comms_pane.c: the Comms pane's Channels view as the CLI lists it: no parts and no channels list two empty groups;
// two parts and a channel list by name under their groups, each with its counts and its grants as the page writes
// them; a part whose message was refused is marked and one with none is not; a channel opened lists its stored
// messages under it, the oldest first, and closes again; the layer is listed again only as its stamp moves; the
// deadline is a second on while the view is in sight and none while it is not; a view chosen is the view shown; the
// head's labels; and the catalog's eight Comms commands are run by name and no other. The cases share the one comms
// layer a process has, run in order
#include "ui/cli/src/comms/comms.h"
#include "ui/cli/src/machine/net.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_alpha[CLI_COMMS_KEY];
static char s_zeta[CLI_COMMS_KEY];

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the row `at` of the Channels view, the test failed where there is none
static CliCommsRow row_at(unsigned int at)
{
    CliCommsRow row;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_pane.row, CliCommsPaneCfg, .at = at, .row = &row));
    return row;
}

// the view read with its stamp looked at, in sight: 1 where it was listed again
static int stepped(int force)
{
    return EMBED_CALL(cli_comms_pane.step, CliCommsPaneCfg, .shown = 1, .force = force, .now_ms = 5000ULL);
}

// a part `name` registered listening on and sending on `grant`, its key into `key`
static void part_registered(const char *name, const char *grant, char *key)
{
    const char *const grants[1] = {grant};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = name, .listen = grants,
                                .listens = 1u, .send_grant = grants, .send_grants = 1u, .key_out = key,
                                .err_msg = &err_msg));
}

// `body` sent on `channel` as the part of `key`: whether it went
static int sent(const char *key, const char *channel, const char *body)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    unsigned long long delivered = 0ULL;
    return EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = key, .channel = channel,
                      .body = body, .size = CLI_UNITY_LENGTH(body), .delivered = &delivered, .err_msg = &err_msg);
}

// a message's receiving, which takes it and does nothing
static int received(const CliCommsRecvCfg *args)
{
    (void)args;
    return 1;
}

void test_no_parts_and_no_channels_list_two_empty_groups(void)
{
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u));
    TEST_ASSERT_EQUAL_INT(-1, EMBED_CALL(cli_comms_pane.current, CliCommsPaneCfg, .at = 0u));
    TEST_ASSERT_TRUE(stepped(1));
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u));
    const CliCommsRow parts = row_at(0u);
    const CliCommsRow channels = row_at(1u);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_ROW_GROUP, parts.kind);
    CLI_TEST_ASSERT_TEXT("Parts", parts.name, parts.name_length);
    CLI_TEST_ASSERT_TEXT("0", parts.status, parts.status_length);
    CLI_TEST_ASSERT_TEXT("Channels", channels.name, channels.name_length);
    CLI_TEST_ASSERT_TEXT("0", channels.status, channels.status_length);
    unsigned long long length = 0ULL;
    const char *const summary = EMBED_CALL(cli_comms_pane.summary, CliCommsPaneCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("0 parts, 0 channels", summary, length);
}

void test_two_parts_and_a_channel_list_by_name_with_their_counts(void)
{
    part_registered("zeta", "demo/*", s_zeta);
    part_registered("alpha", "demo/*", s_alpha);
    unsigned int listener = 0u;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = s_zeta, .channel = "demo/one",
                                .recv = received, .listener = &listener, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(sent(s_alpha, "demo/one", "{\"n\":1}"));
    TEST_ASSERT_TRUE(stepped(0));
    TEST_ASSERT_EQUAL_UINT(5u, EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u));
    const CliCommsRow parts = row_at(0u);
    const CliCommsRow alpha = row_at(1u);
    const CliCommsRow zeta = row_at(2u);
    const CliCommsRow channels = row_at(3u);
    const CliCommsRow channel = row_at(4u);
    CLI_TEST_ASSERT_TEXT("2", parts.status, parts.status_length);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_ROW_PART, alpha.kind);
    CLI_TEST_ASSERT_TEXT("alpha", alpha.name, alpha.name_length);
    CLI_TEST_ASSERT_TEXT("1 sent", alpha.status, alpha.status_length);
    CLI_TEST_ASSERT_TEXT("in orior; listens on demo/*; speaks on demo/*", alpha.detail, alpha.detail_length);
    CLI_TEST_ASSERT_TEXT("zeta", zeta.name, zeta.name_length);
    CLI_TEST_ASSERT_TEXT("0 sent", zeta.status, zeta.status_length);
    CLI_TEST_ASSERT_TEXT("1", channels.status, channels.status_length);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_ROW_CHANNEL, channel.kind);
    CLI_TEST_ASSERT_TEXT("demo/one", channel.name, channel.name_length);
    CLI_TEST_ASSERT_TEXT("1 message", channel.status, channel.status_length);
    CLI_TEST_ASSERT_TEXT("7 bytes; heard by zeta", channel.detail, channel.detail_length);
    unsigned long long length = 0ULL;
    const char *const summary = EMBED_CALL(cli_comms_pane.summary, CliCommsPaneCfg, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("2 parts, 1 channel", summary, length);
}

void test_a_part_refused_is_marked_and_one_with_none_is_not(void)
{
    TEST_ASSERT_FALSE(sent(s_zeta, "other/x", "{}"));
    TEST_ASSERT_TRUE(stepped(0));
    const CliCommsRow alpha = row_at(1u);
    const CliCommsRow zeta = row_at(2u);
    TEST_ASSERT_FALSE(alpha.refused);
    TEST_ASSERT_TRUE(zeta.refused);
    CLI_TEST_ASSERT_TEXT("0 sent, 1 refused", zeta.status, zeta.status_length);
    TEST_ASSERT_EQUAL_UINT(6u, EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u));
    const CliCommsRow refusing = row_at(5u);
    CLI_TEST_ASSERT_TEXT("other/x", refusing.name, refusing.name_length);
    CLI_TEST_ASSERT_TEXT("0 messages, 1 refused", refusing.status, refusing.status_length);
    TEST_ASSERT_TRUE(refusing.refused);
}

void test_the_layer_is_listed_again_only_as_its_stamp_moves(void)
{
    TEST_ASSERT_FALSE(stepped(0));
    TEST_ASSERT_TRUE(sent(s_alpha, "demo/one", "{\"n\":2}"));
    TEST_ASSERT_TRUE(stepped(0));
    TEST_ASSERT_FALSE(stepped(0));
    TEST_ASSERT_TRUE(stepped(1));
}

void test_a_channel_opened_lists_its_messages_the_oldest_first_and_closes(void)
{
    EMBED_CALL(cli_comms_pane.toggle, CliCommsPaneCfg, .at = 4u);
    TEST_ASSERT_EQUAL_UINT(8u, EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u));
    const CliCommsRow channel = row_at(4u);
    TEST_ASSERT_TRUE(channel.open);
    TEST_ASSERT_TRUE(channel.current);
    TEST_ASSERT_EQUAL_INT(4, EMBED_CALL(cli_comms_pane.current, CliCommsPaneCfg, .at = 0u));
    const CliCommsRow first = row_at(5u);
    const CliCommsRow second = row_at(6u);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_ROW_MESSAGE, first.kind);
    CLI_TEST_ASSERT_TEXT("alpha", first.name, first.name_length);
    CLI_TEST_ASSERT_TEXT("{\"n\":1}", first.status, first.status_length);
    CLI_TEST_ASSERT_TEXT("{\"n\":2}", second.status, second.status_length);
    CLI_TEST_ASSERT_TEXT("other/x", row_at(7u).name, row_at(7u).name_length);
    EMBED_CALL(cli_comms_pane.toggle, CliCommsPaneCfg, .at = 5u);
    TEST_ASSERT_EQUAL_UINT(8u, EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u));
    EMBED_CALL(cli_comms_pane.toggle, CliCommsPaneCfg, .at = 4u);
    TEST_ASSERT_EQUAL_UINT(6u, EMBED_CALL(cli_comms_pane.rows, CliCommsPaneCfg, .at = 0u));
    TEST_ASSERT_FALSE(row_at(4u).open);
    CliCommsRow past;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_comms_pane.row, CliCommsPaneCfg, .at = 6u, .row = &past));
}

void test_the_deadline_is_a_second_on_while_in_sight_and_none_while_not(void)
{
    TEST_ASSERT_EQUAL_UINT64(5000ULL + CLI_COMMS_REFRESH_MS,
                             EMBED_CALL(cli_comms_pane.deadline_ms, CliCommsPaneCfg, .shown = 1));
    TEST_ASSERT_EQUAL_UINT64(0ULL, EMBED_CALL(cli_comms_pane.deadline_ms, CliCommsPaneCfg, .shown = 0));
}

void test_a_view_chosen_is_the_view_shown_and_channels_returns(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_VIEW_CHANNELS, EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u));
    EMBED_CALL(cli_comms_pane.choose, CliCommsPaneCfg, .view = CLI_COMMS_VIEW_NETWORK);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_VIEW_NETWORK, EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u));
    TEST_ASSERT_EQUAL_UINT64(0ULL, EMBED_CALL(cli_comms_pane.deadline_ms, CliCommsPaneCfg, .shown = 1));
    EMBED_CALL(cli_comms_pane.choose, CliCommsPaneCfg, .view = CLI_COMMS_VIEWS);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_VIEW_NETWORK, EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u));
    EMBED_CALL(cli_comms_pane.choose, CliCommsPaneCfg, .view = CLI_COMMS_VIEW_CHANNELS);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_VIEW_CHANNELS, EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u));
}

void test_the_heads_labels_are_the_views_and_read_again(void)
{
    unsigned long long length = 0ULL;
    const char *const requests = EMBED_CALL(cli_comms_pane.label, CliCommsPaneCfg,
                                            .at = CLI_COMMS_CHOOSE + CLI_COMMS_VIEW_REQUESTS, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("Requests", requests, length);
    const char *const again =
        EMBED_CALL(cli_comms_pane.label, CliCommsPaneCfg, .at = CLI_COMMS_READ, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("Read Again", again, length);
    const char *const none = EMBED_CALL(cli_comms_pane.label, CliCommsPaneCfg, .at = 0u, .length_out = &length);
    CLI_TEST_ASSERT_TEXT("", none, length);
}

void test_the_eight_comms_commands_run_by_name_and_no_other(void)
{
    static const char *const s_names[8] = {"send-request", "copy-as-curl", "private-environment", "network",
                                           "network-off",  "report",       "remote-rejoin",       "remote-copy-address"};
    char clipboard_text[256];
    char open_text[CLI_PATH_MAX_LEN];
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        VerbaLinea clipboard = {clipboard_text, sizeof(clipboard_text), 0ULL};
        VerbaLinea open_out = {open_text, sizeof(open_text), 0ULL};
        int shown = 0;
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = s_names[at],
                                            .clipboard = &clipboard, .open_out = &open_out, .shown_out = &shown),
                                 s_names[at]);
    }
    VerbaLinea clipboard = {clipboard_text, sizeof(clipboard_text), 0ULL};
    VerbaLinea open_out = {open_text, sizeof(open_text), 0ULL};
    int shown = 0;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "terminal-view",
                                 .clipboard = &clipboard, .open_out = &open_out, .shown_out = &shown));
    TEST_ASSERT_FALSE(shown);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "report", .clipboard = &clipboard,
                                .open_out = &open_out, .shown_out = &shown));
    TEST_ASSERT_TRUE(shown);
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_VIEW_REPORT, EMBED_CALL(cli_comms_pane.view, CliCommsPaneCfg, .at = 0u));
    const char *const on[1] = {"on"};
    const char *const off[1] = {"off"};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "network-off", .words = on,
                                .count = 1u, .clipboard = &clipboard, .open_out = &open_out, .shown_out = &shown));
    TEST_ASSERT_TRUE(cli_net_is_disable());
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "network-off",
                                .clipboard = &clipboard, .open_out = &open_out, .shown_out = &shown));
    TEST_ASSERT_FALSE(cli_net_is_disable());
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_command.run, CliCommsCommandCfg, .name = "network-off", .words = off,
                                .count = 1u, .clipboard = &clipboard, .open_out = &open_out, .shown_out = &shown));
    TEST_ASSERT_FALSE(cli_net_is_disable());
    TEST_ASSERT_EQUAL_UINT64(0ULL, clipboard.at);
    TEST_ASSERT_EQUAL_UINT64(0ULL, open_out.at);
    EMBED_CALL(cli_comms_pane.choose, CliCommsPaneCfg, .view = CLI_COMMS_VIEW_CHANNELS);
}
