// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_container.c: a group's end ends every process its first started and their output, descendants are every process under a root and no other, a
// process dying too often is not restarted, orior_cli serve ends what it started as its input ends, and a container's members end with it while what
// it starts outside does not. The last two run this program again as the head of a container: setUp sees the variable and the program ends as the
// head before any case runs
#include <stdlib.h>

#include "ui/cli/src/command_line/command_line.h"
#include "ui/cli/src/comms/container.h"
#include "ui/cli/src/runs/runner.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const CliProcess s_tree[6] = {{2u, 1u}, {3u, 2u}, {4u, 3u}, {5u, 1u}, {6u, 9u}, {7u, 6u}};
static const CliProcess s_walk[7] = {{2u, 1u}, {3u, 2u}, {4u, 3u}, {5u, 1u}, {6u, 9u}, {7u, 6u}, {8u, 8u}};
static const CliProcess s_loop[2] = {{2u, 3u}, {3u, 2u}};

static CliProcess s_listed[1u << 14u];
static char s_output[1u << 16u];
static CliRestarts s_restarts;
static unsigned int s_found[8];
static int s_heads_ran;
static unsigned int s_serve_ids;
static int s_serve_child_still;
static unsigned int s_container_ids;
static int s_member_still;
static int s_outside_alive;

// the words of a program that waits a while, as a server does
static const char *const *waiting_words(void)
{
#if defined(_WIN32)
    static const char *const s_words[5] = {"ping", "-n", "30", "127.0.0.1", NULL};
#else
    static const char *const s_words[3] = {"sleep", "30", NULL};
#endif
    return s_words;
}

// whether the process `id` runs: the system lists it, and on Linux it is no zombie
static int process_alive(unsigned int id)
{
    const unsigned int count = EMBED_CALL(cli_room_processes_internal.processes_list, CliRoomProcessesCtx, .processes = s_listed, .room_u32 = sizeof(s_listed) / sizeof(s_listed[0]));
    int listed = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        listed |= s_listed[at].id == id;
    }
#if defined(__linux__)
    char path[64];
    char stat[512];
    VerbaLinea line = {path, sizeof(path), 0ull};
    mmgr_line_put(&line, "/proc/");
    mmgr_line_decimal(&line, id, 1u);
    mmgr_line_put(&line, "/stat");
    (void)mmgr_line_finish(&line);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)stat, sizeof(stat) - 1u);
    stat[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    const char *const close = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = stat, .length_value = CLI_UNITY_LENGTH(stat), .byte = ')');
    listed &= (close != NULL) && (close[1] == ' ') && (close[2] != 'Z');
#endif
    return listed;
}

// the process `id` waited for to end, up to five seconds: whether it still runs
static int process_still(unsigned int id)
{
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (process_alive(id) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
    }
    return process_alive(id);
}

// what a started process writes to its output read whole into s_output, until its end
static unsigned long long output_read(CliSpawned *spawned)
{
    unsigned long long size = 0ull;
    for (;;)
    {
        const unsigned long long piece = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx, .pipe_handle = spawned->out, .piece = s_output + size, .room = sizeof(s_output) - 1u - size);
        if (piece == 0ull)
        {
            break;
        }
        size += piece;
    }
    s_output[size] = '\0';
    return size;
}

// whether `id` is among the `count` ids of s_found
static int found_contains(unsigned int count, unsigned int id)
{
    int seen = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        seen |= s_found[at] == id;
    }
    return seen;
}

// this program run again with `variable` set: the ids its head reports, into `ids`, which is sized for 2: their count
static unsigned int head_ran(const char *variable, unsigned int *ids)
{
    char program[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(cli_path_room_internal.program_own, CliPathRoomCtx, .path_out = program, .room_size = sizeof(program));
    const char *const words[2] = {program, NULL};
    const CliSetting settings[1] = {{variable, "1"}};
    CliSpawned spawned;
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = words, .folder = NULL, .settings = settings, .count = 1u, .detached = 0, .err_msg = &err_msg))
    {
        return 0u;
    }
    (void)output_read(&spawned);
    const long long code = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    const char *at = cellul.find(&(CatenaFinitaCfg){.src = s_output, .cap = CLI_CELLS_POOL, .other = "ids ", .other_cap = CLI_CELLS_POOL});
    unsigned int count = 0u;
    if ((code != 0ll) || (at == NULL))
    {
        return 0u;
    }
    at += 4;
    while ((count < 2u) && (at[0] >= '0') && (at[0] <= '9'))
    {
        ids[count] = 0u;
        for (; (at[0] >= '0') && (at[0] <= '9'); at += 1)
        {
            ids[count] = ids[count] * 10u + (unsigned int)(at[0] - '0');
        }
        count += 1u;
        at += (at[0] == ' ') ? 1 : 0;
    }
    return count;
}

// the ids a head reports written to the output, `ids` then each
static void ids_written(const unsigned int *ids, unsigned int count)
{
    char text[64];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "ids");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_ch(&line, ' ');
        mmgr_line_decimal(&line, ids[at], 1u);
    }
    mmgr_line_ch(&line, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = text, .length = line.at);
}

// the head of a container: it starts a waiting program inside, and, where `outside`, one outside, reports their ids and ends; where not, it serves
// until its input ends, at once, first: the process's exit code
static int head_code(int outside)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_container_init())
    {
        return 1;
    }
    CliSpawned inside;
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &inside, .words = waiting_words(), .folder = NULL, .settings = NULL, .count = 0u, .detached = 0, .err_msg = &err_msg))
    {
        return 1;
    }
    unsigned int ids[2] = {(unsigned int)inside.id, 0u};
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 300u);
    if (!outside)
    {
        static CliOutcome s_outcome;
        static const char *const s_serve[1] = {"serve"};
        EMBED_CALL(cli_command_line_given.run, CliCommandLineGivenCfg, .given = s_serve, .count = 1u, .outcome = &s_outcome);
        ids_written(ids, 1u);
        return 0;
    }
    if (!EMBED_CALL(cli_words_id_internal.started_outside, CliWordsIdCtx, .words = waiting_words(), .id = &ids[1]))
    {
        return 1;
    }
    unsigned int members[256];
    const unsigned int count = EMBED_CALL(cli_ids_internal.container_members, CliIdsCtx, .ids = members, .room_u32 = 256u);
    int lists_inside = 0;
    int lists_outside = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        lists_inside |= members[at] == ids[0];
        lists_outside |= members[at] == ids[1];
    }
    if (!lists_inside || lists_outside)
    {
        return 1;
    }
    ids_written(ids, 2u);
#if !defined(_WIN32)
    cli_container_members_end();
#endif
    return 0;
}

// both heads run and what they left recorded, once for the process
static void heads_ran(void)
{
    if (s_heads_ran)
    {
        return;
    }
    s_heads_ran = 1;
    unsigned int ids[2] = {0u, 0u};
    s_serve_ids = head_ran("ORIOR_SERVE_HEAD", ids);
    s_serve_child_still = (s_serve_ids == 1u) && process_still(ids[0]);
    ids[0] = 0u;
    ids[1] = 0u;
    s_container_ids = head_ran("ORIOR_CONTAINER_HEAD", ids);
    s_member_still = (s_container_ids == 2u) && process_still(ids[0]);
    s_outside_alive = (s_container_ids == 2u) && process_alive(ids[1]);
    if (s_container_ids == 2u)
    {
        EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = ids[1]);
    }
}

void setUp(void)
{
    char head[8];
    cli_unity_begun();
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_SERVE_HEAD", .value_out = head, .room = sizeof(head)))
    {
        exit(head_code(0));
    }
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_CONTAINER_HEAD", .value_out = head, .room = sizeof(head)))
    {
        exit(head_code(1));
    }
    EMBED_CALL(cli_container_restarts.restarts_init, CliContainerRestartsCfg, .restarts = &s_restarts, .most = 3u, .within = 60000ull);
}

void tearDown(void)
{
}

void test_windows_a_process_started_in_a_group_has_its_own_job_and_runs(void)
{
#if defined(_WIN32)
    static const char *const s_ran[4] = {"cmd", "/c", "echo ran", NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliSpawned spawned;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = s_ran, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, spawned.group);
    (void)output_read(&spawned);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_output, .other = "ran", .cap = CLI_CELLS_POOL}));
#else
    TEST_IGNORE_MESSAGE("a job object is Windows's");
#endif
}

void test_windows_a_groups_end_ends_every_process_and_their_output(void)
{
#if defined(_WIN32)
    static const char *const s_late[4] = {"cmd", "/c", "cmd /c ping -n 30 127.0.0.1 >nul & echo late", NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliSpawned spawned;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = s_late, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg));
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 800u);
    const unsigned long long ended = cli_clock_milliseconds();
    EMBED_CALL(cli_group_internal.spawn_group_end, CliGroupCtx, .group = spawned.group);
    (void)output_read(&spawned);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    TEST_ASSERT_LESS_THAN_UINT64(5000ull, cli_clock_milliseconds() - ended);
    CLI_TEST_ASSERT_NOT_CONTAINS("late", s_output);
#else
    TEST_IGNORE_MESSAGE("a job object is Windows's");
#endif
}

void test_windows_a_group_released_as_orior_ends_ends_every_process_in_it(void)
{
#if defined(_WIN32)
    static const char *const s_late[4] = {"cmd", "/c", "cmd /c ping -n 30 127.0.0.1 >nul & echo late", NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliSpawned spawned;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = s_late, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg));
    // a process id fits 32 bits
    const unsigned int left = (unsigned int)spawned.id;
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 800u);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    TEST_ASSERT_FALSE(process_still(left));
#else
    TEST_IGNORE_MESSAGE("a job object is Windows's");
#endif
}

void test_windows_a_groups_end_ends_a_scripts_processes_under_gits_bash_launcher(void)
{
#if defined(_WIN32)
    char bash[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = bash, .err_msg = &err_msg))
    {
        TEST_IGNORE_MESSAGE("no bash on this machine");
    }
    const char *const script[4] = {bash, "-c", "echo began; sleep 20; echo late", NULL};
    CliSpawned spawned;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = script, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg));
    char first[8] = "";
    unsigned long long size = 0ull;
    while (size < 5ull)
    {
        const unsigned long long piece = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx, .pipe_handle = spawned.out, .piece = first + size, .room = 5ull - size);
        size += piece;
        if (piece == 0ull)
        {
            break;
        }
    }
    TEST_ASSERT_EQUAL_MEMORY("began", first, 5u);
    const unsigned long long ended = cli_clock_milliseconds();
    EMBED_CALL(cli_group_internal.spawn_group_end, CliGroupCtx, .group = spawned.group);
    (void)output_read(&spawned);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    TEST_ASSERT_LESS_THAN_UINT64(5000ull, cli_clock_milliseconds() - ended);
    CLI_TEST_ASSERT_NOT_CONTAINS("late", s_output);
#else
    TEST_IGNORE_MESSAGE("git's bash launcher is Windows's");
#endif
}

void test_posix_a_tree_end_ends_a_detached_groups_processes_and_their_output(void)
{
#if defined(_WIN32)
    TEST_IGNORE_MESSAGE("a process group is POSIX's");
#else
    static const char *const s_late[4] = {"sh", "-c", "echo began; sleep 20; echo late", NULL};
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliSpawned spawned;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned, .words = s_late, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg));
    char first[8] = "";
    unsigned long long size = 0ull;
    while (size < 5ull)
    {
        const unsigned long long piece = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx, .pipe_handle = spawned.out, .piece = first + size, .room = 5ull - size);
        size += piece;
        if (piece == 0ull)
        {
            break;
        }
    }
    TEST_ASSERT_EQUAL_MEMORY("began", first, 5u);
    const unsigned long long ended = cli_clock_milliseconds();
    EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = spawned.id);
    (void)output_read(&spawned);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    TEST_ASSERT_LESS_THAN_UINT64(5000ull, cli_clock_milliseconds() - ended);
    CLI_TEST_ASSERT_NOT_CONTAINS("late", s_output);
#endif
}

void test_descendants_are_every_process_under_a_root(void)
{
    const unsigned int count = EMBED_CALL(cli_container_root.descendants, CliContainerRootCfg, .root = 1u, .parents = s_tree, .count = 6u, .found = s_found, .room = 8u);
    TEST_ASSERT_EQUAL_UINT(4u, count);
    for (unsigned int want = 2u; want <= 5u; want += 1u)
    {
        TEST_ASSERT_TRUE(found_contains(count, want));
    }
}

void test_a_leaf_has_no_descendants(void)
{
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_container_root.descendants, CliContainerRootCfg, .root = 4u, .parents = s_tree, .count = 6u, .found = s_found, .room = 8u));
}

void test_children_come_after_their_parents(void)
{
    TEST_ASSERT_EQUAL_UINT(2u, EMBED_CALL(cli_container_root.descendants, CliContainerRootCfg, .root = 9u, .parents = s_tree, .count = 6u, .found = s_found, .room = 8u));
    TEST_ASSERT_EQUAL_UINT(6u, s_found[0]);
    TEST_ASSERT_EQUAL_UINT(7u, s_found[1]);
}

void test_a_process_is_under_each_process_above_it(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = 1u, .id = 4u, .parents = s_walk, .count = 7u));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = 4u, .id = 4u, .parents = s_walk, .count = 7u));
}

void test_a_process_is_under_no_other(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = 5u, .id = 4u, .parents = s_walk, .count = 7u));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = 1u, .id = 7u, .parents = s_walk, .count = 7u));
}

void test_a_process_listed_as_its_own_parent_ends_the_walk(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = 1u, .id = 8u, .parents = s_walk, .count = 7u));
}

void test_a_loop_of_reused_ids_ends_the_walk(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = 1u, .id = 2u, .parents = s_loop, .count = 2u));
}

void test_this_process_is_under_its_parent(void)
{
    const unsigned int listed = EMBED_CALL(cli_room_processes_internal.processes_list, CliRoomProcessesCtx, .processes = s_listed, .room_u32 = sizeof(s_listed) / sizeof(s_listed[0]));
    const unsigned int own = cli_process_own();
    unsigned int parent = 0u;
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        parent = (s_listed[at].id == own) ? s_listed[at].parent : parent;
    }
    TEST_ASSERT_NOT_EQUAL(0u, parent);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_container_root_id.under, CliContainerRootIdCfg, .root = parent, .id = own, .parents = s_listed, .count = listed));
}

void test_a_process_may_die_three_times_a_minute(void)
{
    const unsigned long long start = cli_clock_milliseconds();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 1000ull));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 2000ull));
}

void test_a_fourth_death_within_a_minute_stops_restarts_and_is_counted(void)
{
    const unsigned long long start = cli_clock_milliseconds();
    (void)EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start);
    (void)EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 1000ull);
    (void)EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 2000ull);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 3000ull));
    TEST_ASSERT_EQUAL_UINT(4u, EMBED_CALL(cli_container_at.restarts_count, CliContainerAtCfg, .restarts_read = &s_restarts, .at = start + 3000ull));
}

void test_deaths_long_past_are_not_counted(void)
{
    const unsigned long long start = cli_clock_milliseconds();
    (void)EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start);
    (void)EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 1000ull);
    (void)EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 2000ull);
    (void)EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 3000ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_container_restarts_at.restarts_dy, CliContainerRestartsAtCfg, .restarts = &s_restarts, .at = start + 200000ull));
    TEST_ASSERT_EQUAL_UINT(1u, EMBED_CALL(cli_container_at.restarts_count, CliContainerAtCfg, .restarts_read = &s_restarts, .at = start + 200000ull));
}

void test_what_serve_started_ended_as_its_input_did(void)
{
    heads_ran();
    TEST_ASSERT_EQUAL_UINT(1u, s_serve_ids);
    TEST_ASSERT_FALSE(s_serve_child_still);
}

void test_the_head_starts_a_member_and_one_outside(void)
{
    heads_ran();
    TEST_ASSERT_EQUAL_UINT(2u, s_container_ids);
}

void test_the_member_ended_with_the_container(void)
{
    heads_ran();
    TEST_ASSERT_FALSE(s_member_still);
}

void test_what_was_started_outside_lives_on(void)
{
    heads_ran();
    TEST_ASSERT_TRUE(s_outside_alive);
}
