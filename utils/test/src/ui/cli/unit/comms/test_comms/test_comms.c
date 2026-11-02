// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_comms.c: a grant names its channel and every channel under it, a key is a secret of the system's own, a listener receives what is sent on its
// channels and nothing else, a channel the key does not name is refused, a part grants what it starts only channels of its own, the container stores
// each channel's traffic, a listener by call receives in the speaker's thread in the order sent, and a part that leaves receives nothing more. The
// cases share the one container a process maintains, each group with parts and channels of its own, run once for the process
#include "ui/cli/src/comms/comms.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/comms/comms_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// what a listener received: its count, and the last channel, speaker and body
typedef struct
{
    unsigned int count;
    char channel[CLI_NAME_MAX_LEN];
    char from[CLI_NAME_MAX_LEN];
    char body[256];
    char order[256];
} CommsReceived;

static char s_view[1u << 18u];
static char s_text[512];

static int s_keys_ran;
static char s_key_one[CLI_COMMS_KEY];
static char s_key_two[CLI_COMMS_KEY];

static int s_channels_ran;
static CommsReceived s_received;
static long long s_sent_delivered;
static int s_process_read;
static unsigned int s_process;
static int s_listen_refused;
static char s_listen_why[256];
static long long s_send_refused;
static char s_send_why[256];
static long long s_forged_sent;
static char s_rows_messages[512];
static char s_rows_refused[512];
static char s_speaker_refused[512];

static int s_under_ran;
static int s_under_made;
static unsigned int s_under_process;
static int s_greedy_made;
static char s_greedy_why[256];
static int s_all_made;

static int s_traffic_ran;
static char s_files_messages[512];
static unsigned int s_files_stored;
static char s_files_oldest[512];
static char s_files_listeners[512];
static unsigned long long s_stored_in_use;
static unsigned long long s_stored_in_use_after;
static int s_fitting_uses_oldest_cell;
static unsigned long long s_fitting_size;
static char s_fitting_byte;
static int s_longer_uses_oldest_cell;
static unsigned long long s_longer_size;
static CommsReceived s_order;
static unsigned int s_order_after_first;

static int s_leave_ran;
static long long s_left_delivered;
static unsigned int s_left_count;
static int s_left_listens;

// a message received recorded, its channel put on the order received
static int message_recorded(const CliCommsRecvCfg *args)
{
    CommsReceived *const received = args->context;
    received->count += 1u;
    memor.cpy(&(MemoriaCfg){.dst = received->channel, .src = args->channel, .bytes = CLI_UNITY_LENGTH(args->channel) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = received->from, .src = args->from, .bytes = CLI_UNITY_LENGTH(args->from) + 1u});
    const unsigned long long copy_len = (args->size < sizeof(received->body) - 1u) ? args->size : sizeof(received->body) - 1u;
    memor.cpy(&(MemoriaCfg){.dst = received->body, .src = args->body, .bytes = copy_len});
    received->body[copy_len] = '\0';
    const unsigned long long at = CLI_UNITY_LENGTH(received->order);
    VerbaLinea order = {received->order + at, sizeof(received->order) - at, 0ull};
    mmgr_line_put(&order, (at > 0ull) ? "|" : "");
    mmgr_line_put(&order, args->channel);
    (void)mmgr_line_finish(&order);
    return 1;
}

// a part registered: 1 with its key into `key`, which is sized for CLI_COMMS_KEY, or 0 with why into `why`, which is sized for 256
static int part_registered(const char *name, int has_process, unsigned int process, const char *const *listen, unsigned int listens, const char *const *send_grant, unsigned int send_grants, char *key, char *why)
{
    VerbaLinea err_msg = {why, 256u, 0ull};
    const int made = EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = name, .has_process = has_process, .process = process, .listen = listen, .listens = listens, .send_grant = send_grant, .send_grants = send_grants, .key_out = key, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    return made;
}

// `body` sent on `channel` as the part of `key`: how many received it, -1 where it was refused with why into `why`
static long long body_sent(const char *key, const char *channel, const char *body, char *why)
{
    unsigned long long delivered = 0ull;
    VerbaLinea err_msg = {why, 256u, 0ull};
    const int sent = EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = key, .channel = channel, .body = body, .size = CLI_UNITY_LENGTH(body), .delivered = &delivered, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    // a count of listeners fits a signed word
    return sent ? (long long)delivered : -1ll;
}

// `key`'s part listening on `channel`: 1, or 0 with why into `why`
static int part_listening(const char *key, const char *channel, CommsReceived *received, char *why)
{
    unsigned int listener = 0u;
    VerbaLinea err_msg = {why, 256u, 0ull};
    const int listening = EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = key, .channel = channel, .queued = 0, .recv = message_recorded, .context = received, .listener = &listener, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    return listening;
}

// the item named `name` of a view, read into the shared tree: its node
static unsigned int view_item(int parts, const char *name, const CliJson **json)
{
    VerbaLinea out = {s_view, sizeof(s_view), 0ull};
    if (parts)
    {
        EMBED_CALL(cli_comms_table.parts, CliCommsCfg, .out = &out);
    }
    else
    {
        EMBED_CALL(cli_comms_table.channels, CliCommsCfg, .out = &out);
    }
    unsigned int list = CLI_JSON_NONE;
    *json = cli_unity_parsed(s_view, out.at, &list);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = *json, .array = list, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int item = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = *json, .array = list, .index = at);
        if (cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(item, "name"), .other = name, .cap = CLI_CELLS_POOL}))
        {
            return item;
        }
    }
    return CLI_JSON_NONE;
}

// a member of a node of the shared tree written compact into `out`, which is sized for 512
static const char *member_written(const CliJson *json, unsigned int node, const char *name, char *out)
{
    VerbaLinea line = {out, 512u, 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = node, .name = name), .out = &line, .indent = 0u, .sorted = 0);
    (void)mmgr_line_finish(&line);
    return out;
}

// two parts registered, their keys recorded, once for the process
static void keys_ran(void)
{
    char why[256];
    if (s_keys_ran)
    {
        return;
    }
    s_keys_ran = 1;
    (void)part_registered("one", 0, 0u, NULL, 0u, NULL, 0u, s_key_one, why);
    (void)part_registered("two", 0, 0u, NULL, 0u, NULL, 0u, s_key_two, why);
}

// a window listening on lsp/* and a server sending on lsp/rust, and each refused the channel it was not granted, recorded once for the process
static void channels_ran(void)
{
    static const char *const s_window_listens[2] = {"lsp/*", "runs"};
    static const char *const s_rust[1] = {"lsp/rust"};
    char window[CLI_COMMS_KEY];
    char server[CLI_COMMS_KEY];
    char why[256];
    if (s_channels_ran)
    {
        return;
    }
    s_channels_ran = 1;
    (void)part_registered("window", 0, 0u, s_window_listens, 2u, NULL, 0u, window, why);
    (void)part_registered("rust server", 1, 42u, NULL, 0u, s_rust, 1u, server, why);
    (void)part_listening(window, "lsp/*", &s_received, why);
    s_sent_delivered = body_sent(server, "lsp/rust", "{\"n\":1}", why);
    s_process_read = EMBED_CALL(cli_comms_key_process.process_get, CliCommsKeyProcessCfg, .key = server, .process_pointer = &s_process);
    s_listen_refused = !part_listening(window, "db/rows", &s_received, s_listen_why);
    s_send_refused = body_sent(server, "db/rows", "null", s_send_why);
    s_forged_sent = body_sent("forged", "lsp/rust", "null", why);
    const CliJson *json = NULL;
    const unsigned int rows = view_item(0, "db/rows", &json);
    (void)member_written(json, rows, "messages", s_rows_messages);
    (void)member_written(json, rows, "refused", s_rows_refused);
    const unsigned int part = view_item(1, "rust server", &json);
    (void)member_written(json, part, "refused", s_speaker_refused);
}

// a part of runs/* registering others under it, recorded once for the process
static void under_ran(void)
{
    static const char *const s_runs[1] = {"runs/*"};
    static const char *const s_run[1] = {"runs/7"};
    static const char *const s_rust[1] = {"lsp/rust"};
    static const char *const s_all[1] = {"*"};
    char runs[CLI_COMMS_KEY];
    char run[CLI_COMMS_KEY];
    char greedy[CLI_COMMS_KEY];
    char why[256];
    if (s_under_ran)
    {
        return;
    }
    s_under_ran = 1;
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)part_registered("runs", 0, 0u, s_runs, 1u, s_runs, 1u, runs, why);
    s_under_made = EMBED_CALL(cli_comms_name.register_under, CliCommsNameCfg, .parent = runs, .name = "run 7", .has_process = 1, .process = 7u, .listen = s_run, .listens = 1u, .send_grant = s_run, .send_grants = 1u, .key_out = run, .err_msg = &err_msg);
    (void)EMBED_CALL(cli_comms_key_process.process_get, CliCommsKeyProcessCfg, .key = run, .process_pointer = &s_under_process);
    VerbaLinea greedy_why = {s_greedy_why, sizeof(s_greedy_why), 0ull};
    s_greedy_made = EMBED_CALL(cli_comms_name.register_under, CliCommsNameCfg, .parent = runs, .name = "greedy", .has_process = 0, .process = 0u, .listen = s_rust, .listens = 1u, .send_grant = NULL, .send_grants = 0u, .key_out = greedy, .err_msg = &greedy_why);
    (void)mmgr_line_finish(&greedy_why);
    err_msg.at = 0ull;
    s_all_made = EMBED_CALL(cli_comms_name.register_under, CliCommsNameCfg, .parent = runs, .name = "greedy", .has_process = 0, .process = 0u, .listen = NULL, .listens = 0u, .send_grant = s_all, .send_grants = 1u, .key_out = greedy, .err_msg = &err_msg);
}

// the newest message stored on `traffic`
static const CliCommsStoredMsg *newest_stored(const CliCommsTraffic *traffic)
{
    return &traffic->last[(traffic->first + traffic->count - 1u) % CLI_COMMS_STORED_MAX];
}

// a channel's stored messages and their cells, and a listener's order, recorded once for the process
static void traffic_ran(void)
{
    static const char *const s_all[1] = {"*"};
    static const char *const s_files[1] = {"files"};
    static CommsReceived s_files_received;
    char window[CLI_COMMS_KEY];
    char server[CLI_COMMS_KEY];
    char why[256];
    if (s_traffic_ran)
    {
        return;
    }
    s_traffic_ran = 1;
    (void)part_registered("files window", 0, 0u, s_all, 1u, NULL, 0u, window, why);
    (void)part_registered("files server", 0, 0u, NULL, 0u, s_files, 1u, server, why);
    (void)part_listening(window, "files", &s_files_received, why);
    for (unsigned int at = 0u; at < 25u; at += 1u)
    {
        char body[16];
        VerbaLinea line = {body, sizeof(body), 0ull};
        mmgr_line_decimal(&line, at, 1u);
        (void)mmgr_line_finish(&line);
        (void)body_sent(server, "files", body, why);
    }
    const CliJson *json = NULL;
    const unsigned int files = view_item(0, "files", &json);
    const unsigned int last = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = files, .name = "last");
    (void)member_written(json, files, "messages", s_files_messages);
    s_files_stored = json->node[last].count;
    (void)member_written(json, EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = last, .index = 0u), "body", s_files_oldest);
    (void)member_written(json, files, "listeners", s_files_listeners);
    for (unsigned int at = 0u; at < CLI_COMMS_STORED_MAX; at += 1u)
    {
        (void)body_sent(server, "files", "x", why);
    }
    s_stored_in_use = embed_atomic_usize_load(&g_cli_comms_cells.in_use);
    for (unsigned int at = 0u; at < CLI_COMMS_STORED_MAX; at += 1u)
    {
        (void)body_sent(server, "files", "x", why);
    }
    s_stored_in_use_after = embed_atomic_usize_load(&g_cli_comms_cells.in_use);
    const CliCommsTraffic *traffic = g_cli_comms.traffic;
    while (!cellul.eq(&(CatenaFinitaCfg){.src = traffic->name, .other = "files", .cap = CLI_NAME_MAX_LEN}))
    {
        traffic += 1;
    }
    const char *const oldest = traffic->last[traffic->first].from;
    (void)body_sent(server, "files", "y", why);
    const CliCommsStoredMsg *newest = newest_stored(traffic);
    s_fitting_uses_oldest_cell = newest->from == oldest;
    s_fitting_size = newest->size;
    s_fitting_byte = newest->body[0];
    const char *const smaller = traffic->last[traffic->first].from;
    (void)body_sent(server, "files", "a body longer than the cell of the message before it", why);
    newest = newest_stored(traffic);
    s_longer_uses_oldest_cell = newest->from == smaller;
    s_longer_size = newest->size;
    char server_key[CLI_COMMS_KEY];
    char window_key[CLI_COMMS_KEY];
    (void)part_registered("order window", 0, 0u, s_all, 1u, NULL, 0u, window_key, why);
    (void)part_registered("order server", 0, 0u, NULL, 0u, s_all, 1u, server_key, why);
    (void)part_listening(window_key, "run/*", &s_order, why);
    (void)body_sent(server_key, "run/started", "1", why);
    s_order_after_first = s_order.count;
    (void)body_sent(server_key, "run/line", "2", why);
}

// a listener that leaves, recorded once for the process
static void leave_ran(void)
{
    static const char *const s_all[1] = {"*"};
    static const char *const s_files[1] = {"left"};
    static CommsReceived s_left;
    char window[CLI_COMMS_KEY];
    char server[CLI_COMMS_KEY];
    char why[256];
    if (s_leave_ran)
    {
        return;
    }
    s_leave_ran = 1;
    (void)part_registered("leaving window", 0, 0u, s_all, 1u, NULL, 0u, window, why);
    (void)part_registered("leaving server", 0, 0u, NULL, 0u, s_files, 1u, server, why);
    (void)part_listening(window, "left", &s_left, why);
    EMBED_CALL(cli_comms_process.leave, CliCommsProcessCfg, .key = window);
    s_left_delivered = body_sent(server, "left", "1", why);
    s_left_count = s_left.count;
    s_left_listens = part_listening(window, "left", &s_left, why);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_grant_names_its_channel_and_every_channel_under_it(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = "*", .channel = "lsp/rust"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = "lsp/rust", .channel = "lsp/rust"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = "lsp/*", .channel = "lsp/rust"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = "lsp/*", .channel = "lsp/rust/diagnostics"));
}

void test_a_grant_names_no_other_channel(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = "lsp/*", .channel = "lsp"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = "lsp/*", .channel = "lspx/rust"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_comms_grant.grants, CliCommsGrantCfg, .grant = "lsp/rust", .channel = "lsp/python"));
}

void test_a_grant_is_within_one_that_names_every_channel_it_names(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = "lsp/rust", .outer = "lsp/*"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = "lsp/rust/*", .outer = "lsp/*"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = "anything/at/all", .outer = "*"));
}

void test_a_grant_is_not_within_a_narrower_one(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = "lsp/*", .outer = "lsp/rust"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_comms_inner.within, CliCommsInnerCfg, .inner = "*", .outer = "lsp/*"));
}

void test_a_key_is_64_hex_digits_and_no_two_are_alike(void)
{
    keys_ran();
    TEST_ASSERT_EQUAL_UINT64(64ull, CLI_UNITY_LENGTH(s_key_one));
    TEST_ASSERT_FALSE(cellul.eq(&(CatenaFinitaCfg){.src = s_key_one, .other = s_key_two, .cap = CLI_CELLS_POOL}));
}

void test_a_key_names_its_part(void)
{
    char name[CLI_NAME_MAX_LEN];
    keys_ran();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_comms_key.name_get, CliCommsKeyCfg, .key = s_key_two, .name_out = name));
    TEST_ASSERT_EQUAL_STRING("two", name);
}

void test_a_listener_receives_what_is_sent_on_its_channels(void)
{
    channels_ran();
    TEST_ASSERT_EQUAL_INT64(1ll, s_sent_delivered);
}

void test_a_message_comes_with_its_channel_speaker_and_body(void)
{
    channels_ran();
    TEST_ASSERT_EQUAL_STRING("lsp/rust", s_received.channel);
    TEST_ASSERT_EQUAL_STRING("rust server", s_received.from);
    TEST_ASSERT_EQUAL_STRING("{\"n\":1}", s_received.body);
}

void test_a_key_names_the_process_of_its_part(void)
{
    channels_ran();
    TEST_ASSERT_TRUE(s_process_read);
    TEST_ASSERT_EQUAL_UINT(42u, s_process);
}

void test_a_channel_the_key_does_not_name_is_refused_to_the_listener(void)
{
    channels_ran();
    TEST_ASSERT_TRUE(s_listen_refused);
    CLI_TEST_ASSERT_CONTAINS("may not listen on db/rows", s_listen_why);
}

void test_a_channel_the_key_does_not_name_is_refused_to_the_speaker(void)
{
    channels_ran();
    TEST_ASSERT_EQUAL_INT64(-1ll, s_send_refused);
    CLI_TEST_ASSERT_CONTAINS("may not send on db/rows", s_send_why);
}

void test_a_key_of_no_part_opens_nothing(void)
{
    channels_ran();
    TEST_ASSERT_EQUAL_INT64(-1ll, s_forged_sent);
}

void test_a_channel_counts_what_it_refused(void)
{
    channels_ran();
    TEST_ASSERT_EQUAL_STRING("0", s_rows_messages);
    TEST_ASSERT_EQUAL_STRING("1", s_rows_refused);
}

void test_a_speaker_counts_what_it_was_refused(void)
{
    channels_ran();
    TEST_ASSERT_EQUAL_STRING("1", s_speaker_refused);
}

void test_a_part_starts_another_with_channels_of_its_own(void)
{
    under_ran();
    TEST_ASSERT_TRUE(s_under_made);
    TEST_ASSERT_EQUAL_UINT(7u, s_under_process);
}

void test_a_part_may_not_grant_a_channel_it_does_not_have(void)
{
    under_ran();
    TEST_ASSERT_FALSE(s_greedy_made);
    CLI_TEST_ASSERT_CONTAINS("may not grant lsp/rust", s_greedy_why);
}

void test_a_part_may_not_grant_every_channel(void)
{
    under_ran();
    TEST_ASSERT_FALSE(s_all_made);
}

void test_a_channel_counts_its_messages(void)
{
    traffic_ran();
    TEST_ASSERT_EQUAL_STRING("25", s_files_messages);
}

void test_a_channel_stores_its_last_messages_the_oldest_first(void)
{
    traffic_ran();
    TEST_ASSERT_EQUAL_UINT(CLI_COMMS_STORED_MAX, s_files_stored);
    TEST_ASSERT_EQUAL_STRING("5", s_files_oldest);
}

void test_a_channel_lists_its_listeners(void)
{
    traffic_ran();
    TEST_ASSERT_EQUAL_STRING("[\"files window\"]", s_files_listeners);
}

void test_a_channels_stored_messages_use_the_same_bytes_as_more_are_sent(void)
{
    traffic_ran();
    TEST_ASSERT_NOT_EQUAL(0ull, s_stored_in_use);
    TEST_ASSERT_EQUAL_UINT64(s_stored_in_use, s_stored_in_use_after);
}

void test_a_message_that_fits_the_oldests_cell_is_written_into_that_cell(void)
{
    traffic_ran();
    TEST_ASSERT_TRUE(s_fitting_uses_oldest_cell);
    TEST_ASSERT_EQUAL_UINT64(1ull, s_fitting_size);
    TEST_ASSERT_EQUAL_CHAR('y', s_fitting_byte);
}

void test_a_message_past_the_oldests_cell_uses_a_larger_one(void)
{
    traffic_ran();
    TEST_ASSERT_FALSE(s_longer_uses_oldest_cell);
    TEST_ASSERT_EQUAL_UINT64(52ull, s_longer_size);
}

void test_a_listener_by_call_receives_in_the_speakers_call_in_the_order_sent(void)
{
    traffic_ran();
    TEST_ASSERT_EQUAL_UINT(1u, s_order_after_first);
    TEST_ASSERT_EQUAL_STRING("run/started|run/line", s_order.order);
}

void test_a_part_that_leaves_receives_nothing_more(void)
{
    leave_ran();
    TEST_ASSERT_EQUAL_INT64(0ll, s_left_delivered);
    TEST_ASSERT_EQUAL_UINT(0u, s_left_count);
}

void test_a_key_of_a_part_that_left_opens_nothing(void)
{
    leave_ran();
    TEST_ASSERT_FALSE(s_left_listens);
}
