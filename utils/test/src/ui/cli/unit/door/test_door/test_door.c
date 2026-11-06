// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_ipc.c: a process joins by its key and receives and sends on its channels; a key sent to another process or to no part is refused at the door; a
// persistent key is accepted from any process and grants its own channels alone; a key of a process is accepted from every process under it; a body past
// the most a frame accepts goes through the transfer layer each way; a frame past the most is not read; and every place a process joined into is released
// to the cellblock once its process is gone
#include "ui/cli/src/comms/comms.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/comms/door.h"
#include "ui/cli/src/comms/door_internal.h"
#include "ui/cli/src/comms/door_serve.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// what a part's listener received: the last channel, speaker and body, and the last body's size
typedef struct
{
    char channel[CLI_NAME_MAX_LEN];
    char from[CLI_NAME_MAX_LEN];
    char body[256];
    unsigned long long size;
    unsigned int count;
} DoorReceived;

static char s_address[CLI_PATH_MAX_LEN];
static DoorReceived s_received = {"", "", "", 0ull, 0u};
static char s_big[CLI_IPC_MAX + (CLI_IPC_MAX / 5u) + 16u];
static char s_next[CLI_IPC_MAX + (CLI_IPC_MAX / 5u) + 16u];
static char s_view[1u << 16u];
static int s_ipc_open;
static CliIpcEnd s_server;
static embed_atomic_usize s_accepted;

static int s_channels_run_result;
static int s_tracker_joined;
static char s_tracker_name[CLI_NAME_MAX_LEN];
static int s_tracker_listened;
static unsigned long long s_channel_delivered;
static int s_next_read;
static char s_next_channel[CLI_NAME_MAX_LEN];
static char s_next_from[CLI_NAME_MAX_LEN];
static char s_next_body[64];
static unsigned long long s_next_size;
static int s_note_sent;
static unsigned long long s_note_delivered;
static char s_note_from[CLI_NAME_MAX_LEN];
static int s_note_read;
static char s_note_channel[CLI_NAME_MAX_LEN];
static int s_send_refused;
static char s_send_error[256];
static int s_listen_refused;
static char s_listen_error[256];

static int s_refused_run_result;
static int s_elsewhere_joined;
static char s_elsewhere_error[256];
static int s_forged_joined;
static char s_forged_error[256];
static int s_window_joined;
static char s_window_error[256];

static int s_persistent_run_result;
static int s_persistent_created;
static int s_persistent_joined;
static char s_persistent_name[CLI_NAME_MAX_LEN];
static int s_persistent_sent;
static unsigned long long s_persistent_delivered;
static char s_persistent_received[256];
static int s_persistent_answered;
static unsigned long long s_persistent_answer_size;
static char s_persistent_answer[64];
static int s_persistent_refused_send;
static int s_other_joined;

static int s_big_run_result;
static int s_big_joined;
static unsigned long long s_big_size;
static int s_big_sent;
static unsigned long long s_big_delivered;
static unsigned long long s_big_received_size;
static int s_big_back_read;
static char s_big_back_channel[CLI_NAME_MAX_LEN];
static char s_big_back_from[CLI_NAME_MAX_LEN];
static unsigned long long s_big_back_size;
static int s_small_sent;
static unsigned long long s_small_delivered;
static unsigned long long s_big_moved;
static unsigned long long s_back_moved;

// a message a part received recorded
static int message_recorded(const CliCommsRecvCfg *args)
{
    unsigned long long size = args->size;
    DoorReceived *const received = args->context;
    memor.cpy(&(MemoriaCfg){.dst = received->channel, .src = args->channel, .bytes = CLI_UNITY_LENGTH(args->channel) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = received->from, .src = args->from, .bytes = CLI_UNITY_LENGTH(args->from) + 1u});
    const unsigned long long copy_len = (size < sizeof(received->body) - 1u) ? size : sizeof(received->body) - 1u;
    memor.cpy(&(MemoriaCfg){.dst = received->body, .src = args->body, .bytes = copy_len});
    received->body[copy_len] = '\0';
    received->size = size;
    received->count += 1u;
    return 1;
}

// the count of messages received, waited on up to `wait` milliseconds until it passes `count`
static unsigned int received_end(unsigned int count, unsigned long long wait)
{
    const unsigned long long until = cli_clock_milliseconds() + wait;
    for (;;)
    {
        const unsigned int now = s_received.count;
        if ((now > count) || (cli_clock_milliseconds() > until))
        {
            return now;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
}

// the count of messages received
static unsigned int received_count(void)
{
    const unsigned int count = s_received.count;
    return count;
}

// a part registered: its key into `key`, which is sized for CLI_COMMS_KEY
static void part_registered(const char *name, int has_process, unsigned int process, const char *const *listen, unsigned int listens, const char *const *send_grant, unsigned int send_grants, char *key)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = name, .has_process = has_process, .process = process, .listen = listen, .listens = listens, .send_grant = send_grant, .send_grants = send_grants, .key_out = key, .err_msg = &err_msg), name);
}

// the door joined with `key`: the client, NULL with why into `why`, which is sized for 256
static CliIpcClient *ipc_joined(const char *key, char *why)
{
    VerbaLinea err_msg = {why, 256u, 0ull};
    CliIpcClient *const client = EMBED_CALL(cli_ipc_err_msg_address.join, CliIpcErrMsgAddressCfg, .address_read = s_address, .key = key, .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    return client;
}

// `text` copied into `out`, which is sized for `capacity`
static void text_stored(char *out, unsigned long long capacity, const char *text)
{
    VerbaLinea line = {out, capacity, 0ull};
    mmgr_line_put(&line, text);
    (void)mmgr_line_finish(&line);
}

// a process joined, listening, receiving and sending on its channels, what each answered recorded, once for the process
static void channels_run_result(void)
{
    static const char *const s_runs[1] = {"runs/*"};
    static const char *const s_notes[1] = {"runs/notes"};
    char here[CLI_COMMS_KEY];
    char runner[CLI_COMMS_KEY];
    char why[256];
    char channel[CLI_NAME_MAX_LEN];
    char from[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (s_channels_run_result)
    {
        return;
    }
    s_channels_run_result = 1;
    part_registered("tracker", 1, cli_process_own(), s_runs, 1u, s_notes, 1u, here);
    part_registered("runner", 0, 0u, s_notes, 1u, s_runs, 1u, runner);
    unsigned int listener = 0u;
    (void)EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = runner, .channel = "runs/notes", .queued = 0, .recv = message_recorded, .context = &s_received, .listener = &listener, .err_msg = &err_msg);
    CliIpcClient *const client = ipc_joined(here, why);
    s_tracker_joined = client != NULL;
    if (client == NULL)
    {
        return;
    }
    text_stored(s_tracker_name, sizeof(s_tracker_name), EMBED_CALL(cli_ipc_table.name, CliIpcCfg, .client_read = client));
    s_tracker_listened = EMBED_CALL(cli_ipc_err_msg.listen, CliIpcErrMsgCfg, .client = client, .channel = "runs/*", .err_msg = &err_msg);
    (void)EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = runner, .channel = "runs/7", .body = "{\"state\":\"live\"}", .size = 16ull, .delivered = &s_channel_delivered, .err_msg = &err_msg);
    VerbaLinea body = {s_next, sizeof(s_next), 0ull};
    s_next_read = EMBED_CALL(cli_ipc_client.next, CliIpcClientCfg, .client = client, .wait = 5000ull, .channel_out = s_next_channel, .from = s_next_from, .body_verba_linea_pointer = &body);
    s_next_size = body.at;
    memor.cpy(&(MemoriaCfg){.dst = s_next_body, .src = s_next, .bytes = (body.at < sizeof(s_next_body)) ? body.at : sizeof(s_next_body)});
    // the tracker listens on runs/*, and so receives its own note beside the runner
    const unsigned int before = received_count();
    s_note_sent = EMBED_CALL(cli_ipc_err_msg.send, CliIpcErrMsgCfg, .client = client, .channel = "runs/notes", .body = "\"seen\"", .size = 6ull, .delivered = &s_note_delivered, .err_msg = &err_msg);
    (void)received_end(before, 5000ull);
    text_stored(s_note_from, sizeof(s_note_from), s_received.from);
    body.at = 0ull;
    s_note_read = EMBED_CALL(cli_ipc_client.next, CliIpcClientCfg, .client = client, .wait = 5000ull, .channel_out = channel, .from = from, .body_verba_linea_pointer = &body);
    text_stored(s_note_channel, sizeof(s_note_channel), channel);
    VerbaLinea send_error = {s_send_error, sizeof(s_send_error), 0ull};
    s_send_refused = !EMBED_CALL(cli_ipc_err_msg.send, CliIpcErrMsgCfg, .client = client, .channel = "runs/7", .body = "null", .size = 4ull, .delivered = &s_note_delivered, .err_msg = &send_error);
    (void)mmgr_line_finish(&send_error);
    VerbaLinea listen_error = {s_listen_error, sizeof(s_listen_error), 0ull};
    s_listen_refused = !EMBED_CALL(cli_ipc_err_msg.listen, CliIpcErrMsgCfg, .client = client, .channel = "lsp/rust", .err_msg = &listen_error);
    (void)mmgr_line_finish(&listen_error);
    EMBED_CALL(cli_ipc_client.leave, CliIpcClientCfg, .client = client);
}

// the keys the door refuses tried, what each answered recorded, once for the process
static void refused_run_result(void)
{
    static const char *const s_all[1] = {"*"};
    char elsewhere[CLI_COMMS_KEY];
    char window[CLI_COMMS_KEY];
    if (s_refused_run_result)
    {
        return;
    }
    s_refused_run_result = 1;
    part_registered("someone else's", 1, cli_process_own() + 1u, s_all, 1u, s_all, 1u, elsewhere);
    s_elsewhere_joined = ipc_joined(elsewhere, s_elsewhere_error) != NULL;
    s_forged_joined = ipc_joined("not a key", s_forged_error) != NULL;
    part_registered("the window", 0, 0u, s_all, 1u, NULL, 0u, window);
    s_window_joined = ipc_joined(window, s_window_error) != NULL;
}

// a persistent key joined and sending, what each answered recorded, once for the process
static void persistent_run_result(void)
{
    static const char *const s_command[1] = {"command"};
    static const char *const s_answer[1] = {"answer/*"};
    char window[CLI_COMMS_KEY];
    char persistent_key[CLI_COMMS_KEY];
    char other[CLI_COMMS_KEY];
    char why[256];
    char channel[CLI_NAME_MAX_LEN];
    char from[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (s_persistent_run_result)
    {
        return;
    }
    s_persistent_run_result = 1;
    part_registered("window", 0, 0u, s_command, 1u, s_answer, 1u, window);
    unsigned int listener = 0u;
    (void)EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = window, .channel = "command", .queued = 0, .recv = message_recorded, .context = &s_received, .listener = &listener, .err_msg = &err_msg);
    s_persistent_created = EMBED_CALL(cli_comms_name.register_persistent, CliCommsNameCfg, .name = "command line", .listen = s_answer, .listens = 1u, .send_grant = s_command, .send_grants = 1u, .key_out = persistent_key, .err_msg = &err_msg);
    CliIpcClient *const client = ipc_joined(persistent_key, why);
    s_persistent_joined = client != NULL;
    if (client == NULL)
    {
        return;
    }
    text_stored(s_persistent_name, sizeof(s_persistent_name), EMBED_CALL(cli_ipc_table.name, CliIpcCfg, .client_read = client));
    (void)EMBED_CALL(cli_ipc_err_msg.listen, CliIpcErrMsgCfg, .client = client, .channel = "answer/1", .err_msg = &err_msg);
    const unsigned int before = received_count();
    s_persistent_sent = EMBED_CALL(cli_ipc_err_msg.send, CliIpcErrMsgCfg, .client = client, .channel = "command", .body = "{\"id\":\"1\"}", .size = 10ull, .delivered = &s_persistent_delivered, .err_msg = &err_msg);
    (void)received_end(before, 5000ull);
    text_stored(s_persistent_received, sizeof(s_persistent_received), s_received.body);
    unsigned long long delivered = 0ull;
    (void)EMBED_CALL(cli_comms_channel_err_msg.send, CliCommsChannelErrMsgCfg, .key = window, .channel = "answer/1", .body = "{\"done\":true}", .size = 13ull, .delivered = &delivered, .err_msg = &err_msg);
    VerbaLinea body = {s_next, sizeof(s_next), 0ull};
    s_persistent_answered = EMBED_CALL(cli_ipc_client.next, CliIpcClientCfg, .client = client, .wait = 5000ull, .channel_out = channel, .from = from, .body_verba_linea_pointer = &body);
    s_persistent_answer_size = body.at;
    memor.cpy(&(MemoriaCfg){.dst = s_persistent_answer, .src = s_next, .bytes = (body.at < sizeof(s_persistent_answer)) ? body.at : sizeof(s_persistent_answer)});
    s_persistent_refused_send = !EMBED_CALL(cli_ipc_err_msg.send, CliIpcErrMsgCfg, .client = client, .channel = "runs/1", .body = "{}", .size = 2ull, .delivered = &delivered, .err_msg = &err_msg);
    part_registered("other", 0, 0u, NULL, 0u, s_command, 1u, other);
    s_other_joined = ipc_joined(other, why) != NULL;
    EMBED_CALL(cli_ipc_client.leave, CliIpcClientCfg, .client = client);
}

// the other end of a frame case accepted on a thread of its own
static void end_accepted(void *context)
{
    CliIpcListener *const listener = context;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    int has_peer = 0;
    unsigned int peer = 0u;
    // the answer, 0 or 1, widened into the atomic word
    embed_atomic_usize_store(&s_accepted, (embed_atomic_usize_value)EMBED_CALL(cli_ipc_listener_internal.accept, CliIpcListenerCtx, .listener = listener, .end = &s_server, .has_peer = &has_peer, .peer = &peer, .err_msg = &err_msg));
}

// a pair of ends joined over `listener`, the server's end in s_server
static void ends_joined(CliIpcListener *listener, CliThread *thread, CliIpcEnd *client)
{
    char address[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    memor.set(&(MemoriaCfg){.dst = client, .val = 0, .bytes = sizeof(*client)});
    embed_atomic_usize_store(&s_accepted, 0u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_ipc_listener_address_internal.listening, CliIpcListenerAddressCtx, .listener = listener, .address = address, .err_msg = &err_msg));
    (void)EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = thread);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_ipc_internal.connect, CliIpcCtx, .address_read = address, .end = client, .err_msg = &err_msg));
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((embed_atomic_usize_load(&s_accepted) == 0u) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
}

void setUp(void)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_begun();
    if (!s_ipc_open)
    {
        s_ipc_open = 1;
        TEST_ASSERT_TRUE(EMBED_CALL(cli_ipc_address.open, CliIpcAddressCfg, .address = s_address, .err_msg = &err_msg));
    }
}

void tearDown(void)
{
}

void test_a_process_joins_by_its_key(void)
{
    channels_run_result();
    TEST_ASSERT_TRUE(s_tracker_joined);
    TEST_ASSERT_EQUAL_STRING("tracker", s_tracker_name);
}

void test_a_process_listens_on_its_channels(void)
{
    channels_run_result();
    TEST_ASSERT_TRUE(s_tracker_listened);
}

void test_a_message_on_its_channel_reaches_it(void)
{
    channels_run_result();
    TEST_ASSERT_EQUAL_UINT64(1ull, s_channel_delivered);
}

void test_a_message_is_received_with_its_speaker(void)
{
    channels_run_result();
    TEST_ASSERT_TRUE(s_next_read);
    TEST_ASSERT_EQUAL_STRING("runs/7", s_next_channel);
    TEST_ASSERT_EQUAL_STRING("runner", s_next_from);
    TEST_ASSERT_EQUAL_UINT64(16ull, s_next_size);
    TEST_ASSERT_EQUAL_MEMORY("{\"state\":\"live\"}", s_next_body, 16u);
}

void test_a_note_is_received_by_both_its_listeners(void)
{
    channels_run_result();
    TEST_ASSERT_TRUE(s_note_sent);
    TEST_ASSERT_TRUE(s_note_read);
    TEST_ASSERT_EQUAL_STRING("tracker", s_note_from);
    TEST_ASSERT_EQUAL_STRING("runs/notes", s_note_channel);
}

void test_a_part_may_not_send_where_its_key_does_not_permit(void)
{
    channels_run_result();
    TEST_ASSERT_TRUE(s_send_refused);
    CLI_TEST_ASSERT_CONTAINS("may not send on runs/7", s_send_error);
}

void test_a_part_may_not_listen_where_its_key_does_not_permit(void)
{
    channels_run_result();
    TEST_ASSERT_TRUE(s_listen_refused);
    CLI_TEST_ASSERT_CONTAINS("may not listen on lsp/rust", s_listen_error);
}

void test_a_key_of_another_process_is_refused(void)
{
    refused_run_result();
    TEST_ASSERT_FALSE(s_elsewhere_joined);
    CLI_TEST_ASSERT_CONTAINS("given to another process", s_elsewhere_error);
}

void test_a_key_of_no_part_is_refused(void)
{
    refused_run_result();
    TEST_ASSERT_FALSE(s_forged_joined);
    CLI_TEST_ASSERT_CONTAINS("no part's", s_forged_error);
}

void test_a_key_of_the_containers_own_is_refused(void)
{
    refused_run_result();
    TEST_ASSERT_FALSE(s_window_joined);
    CLI_TEST_ASSERT_CONTAINS("given to no process", s_window_error);
}

void test_a_persistent_key_is_accepted_from_any_process(void)
{
    persistent_run_result();
    TEST_ASSERT_TRUE(s_persistent_created);
    TEST_ASSERT_TRUE(s_persistent_joined);
    TEST_ASSERT_EQUAL_STRING("command line", s_persistent_name);
}

void test_a_persistent_key_sends_on_its_own_channel(void)
{
    persistent_run_result();
    TEST_ASSERT_TRUE(s_persistent_sent);
    TEST_ASSERT_EQUAL_UINT64(1ull, s_persistent_delivered);
    TEST_ASSERT_EQUAL_STRING("{\"id\":\"1\"}", s_persistent_received);
}

void test_a_persistent_key_receives_its_answer(void)
{
    persistent_run_result();
    TEST_ASSERT_TRUE(s_persistent_answered);
    TEST_ASSERT_EQUAL_UINT64(13ull, s_persistent_answer_size);
    TEST_ASSERT_EQUAL_MEMORY("{\"done\":true}", s_persistent_answer, 13u);
}

void test_a_persistent_key_is_refused_a_channel_it_does_not_name(void)
{
    persistent_run_result();
    TEST_ASSERT_TRUE(s_persistent_refused_send);
}

void test_a_key_not_persistent_is_accepted_from_its_own_process_alone(void)
{
    persistent_run_result();
    TEST_ASSERT_FALSE(s_other_joined);
}

void test_a_key_of_a_process_is_accepted_from_every_process_under_it(void)
{
    static const char *const s_runs[1] = {"runs/*"};
    static CliProcess s_processes[4096];
    char shell[CLI_COMMS_KEY];
    char why[256];
    const unsigned int count = EMBED_CALL(cli_capacity_processes_internal.processes_list, CliCapacityProcessesCtx, .processes = s_processes, .capacity_u32 = 4096u);
    const unsigned int own = cli_process_own();
    unsigned int parent = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        parent = (s_processes[at].id == own) ? s_processes[at].parent : parent;
    }
    TEST_ASSERT_NOT_EQUAL(0u, parent);
    part_registered("terminal", 0, 0u, s_runs, 1u, s_runs, 1u, shell);
    EMBED_CALL(cli_comms_process.bind, CliCommsProcessCfg, .key = shell, .process = parent);
    CliIpcClient *const client = ipc_joined(shell, why);
    TEST_ASSERT_NOT_NULL_MESSAGE(client, why);
    TEST_ASSERT_EQUAL_STRING("terminal", EMBED_CALL(cli_ipc_table.name, CliIpcCfg, .client_read = client));
    EMBED_CALL(cli_ipc_client.leave, CliIpcClientCfg, .client = client);
}

void test_a_frame_end_the_max_a_frame_accepts_is_not_read(void)
{
    static CliIpcListener s_listener;
    static CliThread s_thread = {end_accepted, &s_listener};
    static char s_frame[64];
    char why[256];
    unsigned char head[6];
    CliIpcEnd client;
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    ends_joined(&s_listener, &s_thread, &client);
    const unsigned long long most = CLI_IPC_MAX + 1u;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        // a byte of the length, its low eight bits
        head[at] = (unsigned char)((most >> (8u * at)) & 0xffu);
    }
    memor.cpy(&(MemoriaCfg){.dst = head + 4, .src = "{}", .bytes = 2u});
    (void)EMBED_CALL(cli_ipc_end_size_internal.write, CliIpcEndSizeCtx, .end = &client, .bytes_read = head, .size = sizeof(head), .region = NULL);
    unsigned long long size = 0ull;
    const int read = EMBED_CALL(cli_ipc_err_msg_end_internal.frame_read, CliIpcErrMsgEndCtx, .end = &s_server, .frame = s_frame, .capacity = sizeof(s_frame), .size_pointer = &size, .err_msg = &err_msg);
    EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &client);
    EMBED_CALL(cli_ipc_internal.close, CliIpcCtx, .end = &s_server);
    TEST_ASSERT_EQUAL_INT(CLI_IPC_FRAME_BROKEN, read);
}

void test_every_position_is_released_once_its_process_is_removed(void)
{
    channels_run_result();
    // a place's threads end after its process does; its cell is therefore waited for up to five seconds
    for (unsigned int waited = 0u; (mmgr_owner_in_use(g_cli_ipc_owner) != 0ull) && (waited < 250u); waited += 1u)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    }
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_ipc_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_ipc_owner));
}
