// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_overseer.c: each event gives its state, a blinking state shows for half of each blink, the records list by name,
// and the tick ends a process past its limits
#include "ui/cli/src/machine/overseer.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// how often the fake end ran
static unsigned int s_ended;

static void overseer_fake_end(void *context)
{
    (void)context;
    s_ended += 1u;
}

// the state of the record `name` of the `role` given, CLI_OVERSEER_NONE where it is absent
static unsigned int overseer_state_of(const char *name, unsigned int role)
{
    CliOverseerSeen seen[8];
    const unsigned int count = EMBED_CALL(cli_overseer.seen, CliOverseerCfg, .role = role, .seen_out = seen, .max = 8u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = seen[at].name, .other = name, .cap = CLI_NAME_MAX_LEN}))
        {
            return seen[at].state;
        }
    }
    return CLI_OVERSEER_NONE;
}

void setUp(void)
{
    cli_unity_begun();
    cli_overseer_end();
    s_ended = 0u;
}

void tearDown(void)
{
    cli_overseer_end();
}

void test_a_spawned_server_starts_and_ready_makes_it_up(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "clangd", .role = CLI_OVERSEER_SERVER,
               .event = CLI_OVERSEER_SPAWNED, .pid = 1ULL);
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_STARTING, overseer_state_of("clangd", CLI_OVERSEER_SERVER));
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "clangd", .event = CLI_OVERSEER_READY);
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_UP, overseer_state_of("clangd", CLI_OVERSEER_SERVER));
}

void test_each_event_gives_its_state(void)
{
    static const unsigned int s_events[] = {CLI_OVERSEER_CONNECTING, CLI_OVERSEER_SHUTDOWN, CLI_OVERSEER_LINK_ERROR,
                                            CLI_OVERSEER_ENDED,      CLI_OVERSEER_FAILED,   CLI_OVERSEER_LINK_LOST};
    static const unsigned int s_states[] = {CLI_OVERSEER_STARTING, CLI_OVERSEER_STOPPING, CLI_OVERSEER_STOPPING,
                                            CLI_OVERSEER_ERROR,    CLI_OVERSEER_CRITICAL, CLI_OVERSEER_CRITICAL};
    for (unsigned int at = 0u; at < (sizeof(s_events) / sizeof(s_events[0])); at += 1u)
    {
        EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "remote", .role = CLI_OVERSEER_REMOTE,
                   .event = s_events[at]);
        TEST_ASSERT_EQUAL_UINT(s_states[at], overseer_state_of("remote", CLI_OVERSEER_REMOTE));
    }
}

void test_an_ask_keeps_the_state(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "plugin", .role = CLI_OVERSEER_PLUGIN,
               .event = CLI_OVERSEER_READY);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "plugin", .event = CLI_OVERSEER_ASKED);
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_UP, overseer_state_of("plugin", CLI_OVERSEER_PLUGIN));
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "plugin", .event = CLI_OVERSEER_ANSWERED);
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_UP, overseer_state_of("plugin", CLI_OVERSEER_PLUGIN));
}

void test_stopped_removes_the_record(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "one", .role = CLI_OVERSEER_SERVER,
               .event = CLI_OVERSEER_READY);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "two", .role = CLI_OVERSEER_SERVER,
               .event = CLI_OVERSEER_READY);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "one", .event = CLI_OVERSEER_STOPPED);
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_NONE, overseer_state_of("one", CLI_OVERSEER_SERVER));
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_UP, overseer_state_of("two", CLI_OVERSEER_SERVER));
}

void test_the_records_list_by_name_and_by_type(void)
{
    static const char *const s_names[] = {"pyright", "clangd", "rust-analyzer", "gopls", "bash"};
    for (unsigned int at = 0u; at < (sizeof(s_names) / sizeof(s_names[0])); at += 1u)
    {
        EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = s_names[at], .role = CLI_OVERSEER_SERVER,
                   .event = CLI_OVERSEER_READY);
    }
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "host", .role = CLI_OVERSEER_REMOTE,
               .event = CLI_OVERSEER_READY);
    CliOverseerSeen seen[16];
    const unsigned int count =
        EMBED_CALL(cli_overseer.seen, CliOverseerCfg, .role = CLI_OVERSEER_SERVER, .seen_out = seen, .max = 16u);
    TEST_ASSERT_EQUAL_UINT(5u, count);
    TEST_ASSERT_EQUAL_STRING("bash", seen[0].name);
    TEST_ASSERT_EQUAL_STRING("clangd", seen[1].name);
    TEST_ASSERT_EQUAL_STRING("gopls", seen[2].name);
    TEST_ASSERT_EQUAL_STRING("pyright", seen[3].name);
    TEST_ASSERT_EQUAL_STRING("rust-analyzer", seen[4].name);
}

void test_the_worst_state_of_a_type_wins(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "a", .role = CLI_OVERSEER_REMOTE,
               .event = CLI_OVERSEER_READY);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "b", .role = CLI_OVERSEER_REMOTE,
               .event = CLI_OVERSEER_LINK_ERROR);
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_STOPPING,
                           EMBED_CALL(cli_overseer.worst, CliOverseerCfg, .role = CLI_OVERSEER_REMOTE));
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_NONE,
                           EMBED_CALL(cli_overseer.worst, CliOverseerCfg, .role = CLI_OVERSEER_SERVER));
}

void test_a_blinking_state_shows_for_half_of_each_blink_and_a_solid_one_always(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_overseer.lit, CliOverseerCfg, .state = CLI_OVERSEER_STARTING, .now_ms = 0ULL));
    TEST_ASSERT_FALSE(
        EMBED_CALL(cli_overseer.lit, CliOverseerCfg, .state = CLI_OVERSEER_STARTING, .now_ms = CLI_OVERSEER_BLINK_MS));
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_overseer.lit, CliOverseerCfg, .state = CLI_OVERSEER_UP, .now_ms = CLI_OVERSEER_BLINK_MS));
    TEST_ASSERT_TRUE(
        EMBED_CALL(cli_overseer.lit, CliOverseerCfg, .state = CLI_OVERSEER_CRITICAL, .now_ms = CLI_OVERSEER_BLINK_MS));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_overseer.lit, CliOverseerCfg, .state = CLI_OVERSEER_NONE, .now_ms = 0ULL));
}

void test_the_next_wake_is_none_without_records_and_the_next_half_blink_while_one_blinks(void)
{
    TEST_ASSERT_EQUAL_UINT64(0ULL, EMBED_CALL(cli_overseer.due_ms, CliOverseerCfg, .now_ms = 100ULL));
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "clangd", .role = CLI_OVERSEER_SERVER,
               .event = CLI_OVERSEER_CONNECTING);
    // before the first tick, the tick is due at once
    TEST_ASSERT_EQUAL_UINT64(101ULL, EMBED_CALL(cli_overseer.due_ms, CliOverseerCfg, .now_ms = 100ULL));
    (void)EMBED_CALL(cli_overseer.tick, CliOverseerCfg, .now_ms = 100ULL);
    TEST_ASSERT_EQUAL_UINT64(CLI_OVERSEER_BLINK_MS, EMBED_CALL(cli_overseer.due_ms, CliOverseerCfg, .now_ms = 100ULL));
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "clangd", .event = CLI_OVERSEER_READY);
    TEST_ASSERT_EQUAL_UINT64(100ULL + CLI_OVERSEER_TICK_MS,
                             EMBED_CALL(cli_overseer.due_ms, CliOverseerCfg, .now_ms = 100ULL));
}

void test_a_process_past_its_memory_is_ended_and_its_record_shows_the_error(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "greedy", .role = CLI_OVERSEER_PLUGIN,
               .event = CLI_OVERSEER_SPAWNED, .pid = (unsigned long long)cli_process_own());
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "greedy", .event = CLI_OVERSEER_READY);
    EMBED_CALL(cli_overseer.limits, CliOverseerCfg, .name = "greedy", .memory_bytes = 1ULL, .end = overseer_fake_end,
               .context = NULL);
    (void)EMBED_CALL(cli_overseer.tick, CliOverseerCfg, .now_ms = cli_clock_milliseconds());
    TEST_ASSERT_EQUAL_UINT(1u, s_ended);
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_ERROR, overseer_state_of("greedy", CLI_OVERSEER_PLUGIN));
}

void test_a_process_within_its_limits_is_not_ended_and_reads_its_working_set(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "modest", .role = CLI_OVERSEER_PLUGIN,
               .event = CLI_OVERSEER_SPAWNED, .pid = (unsigned long long)cli_process_own());
    EMBED_CALL(cli_overseer.limits, CliOverseerCfg, .name = "modest", .memory_bytes = 1ULL << 40u,
               .end = overseer_fake_end, .context = NULL);
    (void)EMBED_CALL(cli_overseer.tick, CliOverseerCfg, .now_ms = cli_clock_milliseconds());
    TEST_ASSERT_EQUAL_UINT(0u, s_ended);
    CliOverseerSeen seen[1];
    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(cli_overseer.seen, CliOverseerCfg, .role = CLI_OVERSEER_PLUGIN, .seen_out = seen, .max = 1u));
    TEST_ASSERT_GREATER_THAN_UINT64(0ULL, seen[0].working);
}

void test_an_ask_past_its_time_is_ended(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "slow", .role = CLI_OVERSEER_PLUGIN,
               .event = CLI_OVERSEER_SPAWNED, .pid = (unsigned long long)cli_process_own());
    EMBED_CALL(cli_overseer.limits, CliOverseerCfg, .name = "slow", .ask_ms = 50ULL, .end = overseer_fake_end,
               .context = NULL);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "slow", .event = CLI_OVERSEER_ASKED);
    (void)EMBED_CALL(cli_overseer.tick, CliOverseerCfg, .now_ms = cli_clock_milliseconds() + 1000ULL);
    TEST_ASSERT_EQUAL_UINT(1u, s_ended);
}

void test_a_remote_link_whose_process_is_gone_is_lost(void)
{
    // a pid far past any the system gives out
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "host", .role = CLI_OVERSEER_REMOTE,
               .event = CLI_OVERSEER_SPAWNED, .pid = 0xFFFFFFF0ULL);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "host", .event = CLI_OVERSEER_READY);
    (void)EMBED_CALL(cli_overseer.tick, CliOverseerCfg, .now_ms = cli_clock_milliseconds());
    TEST_ASSERT_EQUAL_UINT(CLI_OVERSEER_CRITICAL, overseer_state_of("host", CLI_OVERSEER_REMOTE));
}

void test_a_tick_says_whether_a_state_changed(void)
{
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = "clangd", .role = CLI_OVERSEER_SERVER,
               .event = CLI_OVERSEER_READY);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_overseer.tick, CliOverseerCfg, .now_ms = 10000ULL));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_overseer.tick, CliOverseerCfg, .now_ms = 10000ULL + CLI_OVERSEER_TICK_MS));
}
