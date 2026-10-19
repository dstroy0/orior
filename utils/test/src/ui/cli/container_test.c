// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container_test.c: orior's container's check: a group's end ends every process its first started and their output
// with them, a process started in a group runs, a group's end ends a script's processes under git's bash launcher,
// descendants are every process under a root and no other, a process stands under each process above it and no other,
// a process dying too often is not started again, orior serve ends what it started as its input ends, and a
// container's members end with it while what it starts outside does not. The last two run this program again as the
// head of a container
#include "../../../../../ui/cli/src/command_line/command_line.h"
#include "../../../../../ui/cli/src/comms/container.h"
#include "../../../../../ui/cli/src/runs/runner.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static CliProcess s_listed[1u << 14u];
static char s_said[1u << 16u];

// the words of a program that waits a while, as a server does
static const char *const *container_waiting(void)
{
#if defined(_WIN32)
    static const char *const s_words[5] = {"ping", "-n", "30", "127.0.0.1", NULL};
#else
    static const char *const s_words[3] = {"sleep", "30", NULL};
#endif
    return s_words;
}

// whether the process `id` runs: the system lists it, and on Linux it is no zombie
static int container_alive(unsigned int id)
{
    const unsigned int count = cli_processes_listed(s_listed, sizeof(s_listed) / sizeof(s_listed[0]));
    int listed = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        listed |= s_listed[at].id == id;
    }
#if defined(__linux__)
    char path[64];
    char stat[512];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, "/proc/");
    cli_line_decimal(&line, id, 1u);
    cli_line_text(&line, "/stat");
    (void)cli_line_closed(&line);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)stat, sizeof(stat) - 1u);
    stat[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    const char *const close =
        cli_text_last(stat, cellul.len(&(CatenaFinitaCfg){.src = stat, .cap = CLI_CELLS_POOL}), ')');
    listed &= (close != NULL) && (close[1] == ' ') && (close[2] != 'Z');
#endif
    return listed;
}

// the process `id` waited for to end, up to five seconds: whether it still runs
static int container_still(unsigned int id)
{
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (container_alive(id) && (cli_clock_milliseconds() < until))
    {
        query_paused(100u);
    }
    return container_alive(id);
}

// what a started process writes to its output read whole into s_said, until its end
static unsigned long long container_read(CliSpawned *spawned)
{
    unsigned long long size = 0ull;
    for (;;)
    {
        const unsigned long long piece = cli_spawn_read(spawned->out, s_said + size, sizeof(s_said) - 1u - size);
        if (piece == 0ull)
        {
            break;
        }
        size += piece;
    }
    s_said[size] = '\0';
    return size;
}

#if defined(_WIN32)
// a group's end ends every process its first started and their output with them, and a process started in a group runs
static void container_groups(void)
{
    static const char *const s_late[4] = {"cmd", "/c", "cmd /c ping -n 30 127.0.0.1 >nul & echo late", NULL};
    static const char *const s_ran[4] = {"cmd", "/c", "echo ran", NULL};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliSpawned spawned;
    cli_check(cli_spawn(&spawned, s_ran, NULL, NULL, 0u, 1, &said) && (spawned.group != 0ull), "a job of its own");
    (void)container_read(&spawned);
    (void)cli_spawn_wait(&spawned);
    cli_spawn_closed(&spawned);
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = s_said, .other = "ran", .cap = CLI_CELLS_POOL}),
              "a process started in a group runs");
    (void)cli_spawn(&spawned, s_late, NULL, NULL, 0u, 1, &said);
    query_paused(800u);
    const unsigned long long ended = cli_clock_milliseconds();
    cli_spawn_group_end(spawned.group);
    (void)container_read(&spawned);
    (void)cli_spawn_wait(&spawned);
    cli_spawn_closed(&spawned);
    cli_check((cli_clock_milliseconds() - ended < 5000ull) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_said, .cap = CLI_CELLS_POOL, .other = "late", .other_cap = CLI_CELLS_POOL}) == NULL),
              "a group's end ends every process its first started and their output with them");
    char bash[CLI_PATH_LONGEST];
    if (!cli_runner_bash(bash, &said))
    {
        return;
    }
    const char *const script[4] = {bash, "-c", "echo began; sleep 20; echo late", NULL};
    (void)cli_spawn(&spawned, script, NULL, NULL, 0u, 1, &said);
    char first[8] = "";
    unsigned long long size = 0ull;
    while (size < 5ull)
    {
        const unsigned long long piece = cli_spawn_read(spawned.out, first + size, 5ull - size);
        size += piece;
        if (piece == 0ull)
        {
            break;
        }
    }
    cli_check(memor.cmp(&(MemoriaCfg){.src = first, .other = "began", .bytes = 5u}) == 0,
              "the script runs in the group");
    const unsigned long long ended_script = cli_clock_milliseconds();
    cli_spawn_group_end(spawned.group);
    (void)container_read(&spawned);
    (void)cli_spawn_wait(&spawned);
    cli_spawn_closed(&spawned);
    cli_check((cli_clock_milliseconds() - ended_script < 5000ull) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_said, .cap = CLI_CELLS_POOL, .other = "late", .other_cap = CLI_CELLS_POOL}) == NULL),
              "a group's end ends a script's processes under git's bash launcher");
}
#endif

// descendants are every process under a root and no other; a process stands under each process above it and no other
static void container_tree(void)
{
    static const CliProcess s_tree[6] = {{2u, 1u}, {3u, 2u}, {4u, 3u}, {5u, 1u}, {6u, 9u}, {7u, 6u}};
    static const CliProcess s_walk[7] = {{2u, 1u}, {3u, 2u}, {4u, 3u}, {5u, 1u}, {6u, 9u}, {7u, 6u}, {8u, 8u}};
    static const CliProcess s_loop[2] = {{2u, 3u}, {3u, 2u}};
    unsigned int found[8];
    unsigned int count = cli_descendants(1u, s_tree, 6u, found, 8u);
    int each = count == 4u;
    for (unsigned int want = 2u; each && (want <= 5u); want += 1u)
    {
        int seen = 0;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            seen |= found[at] == want;
        }
        each &= seen;
    }
    cli_check(each, "descendants are every process under a root");
    cli_check(cli_descendants(4u, s_tree, 6u, found, 8u) == 0u, "and none under a leaf");
    count = cli_descendants(9u, s_tree, 6u, found, 8u);
    cli_check((count == 2u) && (found[0] == 6u) && (found[1] == 7u), "children after their parents");
    cli_check(cli_under(1u, 4u, s_walk, 7u) && cli_under(4u, 4u, s_walk, 7u), "a process stands under each above it");
    cli_check(!cli_under(5u, 4u, s_walk, 7u) && !cli_under(1u, 7u, s_walk, 7u), "and no other");
    cli_check(!cli_under(1u, 8u, s_walk, 7u), "a process listed as its own parent ends the walk");
    cli_check(!cli_under(1u, 2u, s_loop, 2u), "a loop of ids given again ends the walk");
    const unsigned int listed = cli_processes_listed(s_listed, sizeof(s_listed) / sizeof(s_listed[0]));
    const unsigned int own = cli_process_own();
    unsigned int parent = 0u;
    for (unsigned int at = 0u; at < listed; at += 1u)
    {
        parent = (s_listed[at].id == own) ? s_listed[at].parent : parent;
    }
    cli_check((parent != 0u) && cli_under(parent, own, s_listed, listed), "this process stands under its parent");
}

// a process dying too often is not started again
static void container_restarts(void)
{
    static CliRestarts s_restarts;
    cli_restarts_made(&s_restarts, 3u, 60000ull);
    const unsigned long long start = cli_clock_milliseconds();
    cli_check(cli_restarts_died(&s_restarts, start) && cli_restarts_died(&s_restarts, start + 1000ull) &&
                  cli_restarts_died(&s_restarts, start + 2000ull),
              "a process may die three times a minute");
    cli_check(!cli_restarts_died(&s_restarts, start + 3000ull), "a fourth death within a minute stops it");
    cli_check(cli_restarts_count(&s_restarts, start + 3000ull) == 4u, "each death counted");
    cli_check(cli_restarts_died(&s_restarts, start + 200000ull), "deaths long past are forgotten");
    cli_check(cli_restarts_count(&s_restarts, start + 200000ull) == 1u, "and no longer counted");
}

// this program run again with `variable` set: the ids its head says, into `ids`, which holds 2: their count
static unsigned int container_head(const char *variable, unsigned int *ids)
{
    char program[CLI_PATH_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_program_own(program, sizeof(program));
    const char *const words[2] = {program, NULL};
    const CliSetting settings[1] = {{variable, "1"}};
    CliSpawned spawned;
    if (!cli_spawn(&spawned, words, NULL, settings, 1u, 0, &said))
    {
        return 0u;
    }
    (void)container_read(&spawned);
    const long long code = cli_spawn_wait(&spawned);
    cli_spawn_closed(&spawned);
    const char *at = cellul.find(
        &(CatenaFinitaCfg){.src = s_said, .cap = CLI_CELLS_POOL, .other = "ids ", .other_cap = CLI_CELLS_POOL});
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

// the ids a head says written to the output, `ids` then each
static void container_said(const unsigned int *ids, unsigned int count)
{
    char text[64];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(&line, "ids");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_character(&line, ' ');
        cli_line_decimal(&line, ids[at], 1u);
    }
    cli_line_character(&line, '\n');
    query_terminal_written(0, text, line.at);
}

// the head of a container: it starts a waiting program inside, and, where `outside`, one outside, says their ids and
// ends; where not, it serves until its input ends, at once, first
static int container_as_head(int outside)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_container_held())
    {
        return 1;
    }
    CliSpawned inside;
    if (!cli_spawn(&inside, container_waiting(), NULL, NULL, 0u, 0, &said))
    {
        return 1;
    }
    unsigned int ids[2] = {(unsigned int)inside.id, 0u};
    query_paused(300u);
    if (!outside)
    {
        static CliOutcome s_outcome;
        static const char *const s_serve[1] = {"serve"};
        cli_command_line_run(s_serve, 1u, &s_outcome);
        container_said(ids, 1u);
        return 0;
    }
    if (!cli_started_outside(container_waiting(), &ids[1]))
    {
        return 1;
    }
    unsigned int members[256];
    const unsigned int count = cli_container_members(members, 256u);
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
    container_said(ids, 2u);
#if !defined(_WIN32)
    cli_container_members_ended();
#endif
    return 0;
}

int main(void)
{
    char head[8];
    if (query_environment_read("ORIOR_SERVE_HEAD", head, sizeof(head)))
    {
        return container_as_head(0);
    }
    if (query_environment_read("ORIOR_CONTAINER_HEAD", head, sizeof(head)))
    {
        return container_as_head(1);
    }
#if defined(_WIN32)
    container_groups();
#endif
    container_tree();
    container_restarts();
    unsigned int ids[2] = {0u, 0u};
    cli_check((container_head("ORIOR_SERVE_HEAD", ids) == 1u) && !container_still(ids[0]),
              "what serve started ended as its input did");
    cli_check(container_head("ORIOR_CONTAINER_HEAD", ids) == 2u, "the head starts a member and one outside");
    cli_check(!container_still(ids[0]), "the member ended with the container");
    cli_check(container_alive(ids[1]), "what was started outside lives on");
    cli_spawn_tree_end(ids[1]);
    return cli_checked();
}
