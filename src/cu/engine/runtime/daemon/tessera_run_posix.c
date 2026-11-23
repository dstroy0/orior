// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_run_posix.c: starting, timing and ending a job's process tree on POSIX
#include "../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "tessera_run_internal.h"

#if !(defined(_WIN32))

int run_cpu(const RunChild *child, unsigned long long *microseconds)
{
    unsigned long long count = 0ULL;
    RunProcess *const processes = run_tree(child->pid, &count);
    if (processes == NULL)
    {
        return 0;
    }
    unsigned long long ticks = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        ticks += processes[at].in_tree ? processes[at].ticks : 0ULL;
    }
    platform_cell_released(processes, platform_place_g_program_owner);
    const long hertz = sysconf(_SC_CLK_TCK);
    // the clock's ticks a second are positive where it answers
    *microseconds = (hertz > 0L) ? ((ticks * RUN_MILLION) / (unsigned long long)hertz) : 0ULL;
    return hertz > 0L;
}

int run_processors_named(unsigned long long processors)
{
    char text[24];
    VerbaLinea formed_line_1 = mmgr_line_over(text, sizeof(text));
    mmgr_line_decimal(&formed_line_1, processors, 1u);
    (void)mmgr_line_finish(&formed_line_1);
    const int written = (int)formed_line_1.at;
    return (written > 0) && platform_environment_set("TESSERA_RUN_PROCESSORS", text);
}

int run_start(RunChild *child, char *const *words, int count, unsigned long long mask, RunChannel *channel)
{
    (void)count;
    // the command waits on this pipe until its pid is recorded, as a Windows command waits suspended
    int go[2] = {-1, -1};
    if (pipe2(go, O_CLOEXEC) != 0)
    {
        return 0;
    }
    const pid_t made = fork();
    if (made < 0)
    {
        close(go[0]);
        close(go[1]);
        return 0;
    }
    if (made == 0)
    {
        close(go[1]);
        char word = 0;
        // a parent gone before it recorded this pid leaves nothing read, and nothing run
        if (read(go[0], &word, 1u) != 1)
        {
            _exit(RUN_NOT_STARTED);
        }
        cpu_set_t set;
        CPU_ZERO(&set);
        for (unsigned int processor = 0u; processor < RUN_PROCESSORS; processor += 1u)
        {
            if (((mask >> processor) & 1ULL) != 0ULL)
            {
                CPU_SET(processor, &set);
            }
        }
        sched_setaffinity(0, sizeof(set), &set);
        // below normal: at least ten nicer than normal, and no nicer again under a parent tessera_run that made it so
        const int current = nice(0);
        const int niced = (current < RUN_NICER) ? nice(RUN_NICER - current) : current;
        (void)niced;
        execvp(words[0], words);
        _exit(RUN_NOT_STARTED);
    }
    close(go[0]);
    child->pid = made;
    // a pid is positive. It converts exactly
    run_record_launched(channel, (unsigned long long)made);
    const char word = 1;
    const ssize_t go_written = write(go[1], &word, 1u);
    (void)go_written;
    close(go[1]);
#if defined(SYS_pidfd_open)
    // a descriptor number fits an int
    child->watch = (int)syscall(SYS_pidfd_open, made, 0u);
#else
    child->watch = -1;
#endif
    // an interrupt or a hangup from the terminal reaches the command, which ends. This process waits for it, then
    // releases its job or writes its last record
    (void)platform_signal_handled(SIGINT, SIG_IGN);
    (void)platform_signal_handled(SIGQUIT, SIG_IGN);
    (void)platform_signal_handled(SIGHUP, SIG_IGN);
    return 1;
}

// 1 once the command ended; it is left unreaped, and its tree's last reading still counts its own ticks
int run_ended(RunChild *child, unsigned long long microseconds)
{
    if (child->watch >= 0)
    {
        struct pollfd watch;
        watch.fd = child->watch;
        watch.events = POLLIN;
        watch.revents = 0;
        // a sweep's milliseconds fit an int
        poll(&watch, 1u, (int)(microseconds / RUN_THOUSAND));
    }
    else
    {
        run_pause(microseconds);
    }
    siginfo_t ended;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &ended, .bytes = sizeof(ended), .val = 0);
    const int wait_result = waitid(P_PID, (id_t)child->pid, &ended, WEXITED | WNOHANG | WNOWAIT);
    return ((wait_result == 0) && (ended.si_pid == child->pid)) || ((wait_result != 0) && (errno != EINTR));
}

int run_finish(RunChild *child)
{
    int status = 0;
    pid_t reaped = waitpid(child->pid, &status, 0);
    while ((reaped < 0) && (errno == EINTR))
    {
        reaped = waitpid(child->pid, &status, 0);
    }
    if (child->watch >= 0)
    {
        close(child->watch);
    }
    if (reaped != child->pid)
    {
        return RUN_FAILED;
    }
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    return WIFSIGNALED(status) ? (128 + WTERMSIG(status)) : RUN_FAILED;
}

// the processes of the tree still living (not ended, not waiting to be reaped), each sent the signal where one is
// given; their count
static unsigned long long run_living(const RunProcess *processes, unsigned long long count, int signal_number)
{
    unsigned long long living = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        RunProcess now;
        const int alive = processes[at].in_tree && run_process_read(processes[at].pid, &now) && (now.state != 'Z');
        if (alive && (signal_number != 0))
        {
            // a pid read from /proc is positive, and a pid_t represents it
            kill((pid_t)processes[at].pid, signal_number);
        }
        living += alive ? 1ULL : 0ULL;
    }
    return living;
}

// the command's tree is requested to end (SIGTERM); what is left of it after the grace is ended by force (SIGKILL). The
// count ended by force
unsigned long long run_end_command(RunChild *child, unsigned long long grace)
{
    unsigned long long count = 0ULL;
    RunProcess *const processes = run_tree(child->pid, &count);
    if (processes == NULL)
    {
        kill(child->pid, SIGKILL);
        return 1ULL;
    }
    unsigned long long living = run_living(processes, count, SIGTERM);
    const unsigned long long term_sent_at = run_now();
    while ((living != 0ULL) && ((run_now() - term_sent_at) < grace))
    {
        run_pause(RUN_ENDING_MICROSECONDS);
        living = run_living(processes, count, 0);
    }
    const unsigned long long forced = (living != 0ULL) ? run_living(processes, count, SIGKILL) : 0ULL;
    platform_cell_released(processes, platform_place_g_program_owner);
    return forced;
}

void run_folder_make(const char *path)
{
    mkdir(path, 0755);
}

int run_folder_exists(const char *path)
{
    struct stat found;
    return (stat(path, &found) == 0) && S_ISDIR(found.st_mode);
}

// the parent's record, locked for this process's life and let go with it: how its child finds that it lives. The
// descriptor closes on exec: no child takes the lock with it
int run_record_make(RunChannel *channel, const char *path)
{
    channel->record = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (channel->record < 0)
    {
        return 0;
    }
    if (flock(channel->record, LOCK_EX | LOCK_NB) != 0)
    {
        close(channel->record);
        return 0;
    }
    return 1;
}

void run_record_put(RunChannel *channel, const char *line, size_t length)
{
    const ssize_t written = pwrite(channel->record, line, length, 0);
    (void)written;
}

void run_record_close(RunChannel *channel)
{
    close(channel->record);
}

// 1 while the parent lives. A Windows parent (this child runs in WSL) shares its record only for reading, and an open
// for writing errors while it lives; a Linux parent takes a lock on it. Either is let go when the parent ends
int run_parent_alive(const char *parent)
{
    const int opened = open(parent, O_WRONLY | O_CLOEXEC);
    if (opened < 0)
    {
        return errno != ENOENT;
    }
    const int locked = flock(opened, LOCK_EX | LOCK_NB) == 0;
    close(opened);
    return !locked;
}

// the white space that the read of /proc's stat skips before each field
#define RUN_STAT_SPACE " \t\n\v\f\r"

// The fields of /proc's stat after the name: the state, the parent, nine fields skipped, then four counts of ticks.
// The answer is the count of fields read, 6 where all of them came, as the read of a format counts them
static int run_stat_fields(const char *at, RunProcess *process, unsigned long long ticks[4])
{
    at += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = RUN_STAT_SPACE);
    if (*at == '\0')
    {
        return 0;
    }
    process->state = *at;
    at += 1;
    const char *end = at;
    process->parent = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = at, .end = &end, .base = 10u);
    if (end == at)
    {
        return 1;
    }
    at = end;
    for (unsigned int skipped = 0u; skipped < 9u; skipped += 1u)
    {
        at += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = RUN_STAT_SPACE);
        const size_t word = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                       .other = RUN_STAT_SPACE);
        if (word == 0u)
        {
            return 2;
        }
        at += word;
    }
    for (unsigned int tick = 0u; tick < 4u; tick += 1u)
    {
        end = at;
        ticks[tick] = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = at, .end = &end, .base = 10u);
        if (end == at)
        {
            // Explicit cast narrows a count of at max 4 to the int of the answer
            return 2 + (int)tick;
        }
        at = end;
    }
    return 6;
}

int run_process_read(unsigned long long pid, RunProcess *process)
{
    char path[64];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, "/proc/");
        mmgr_line_decimal(&buffer_line, pid, 1u);
        mmgr_line_put(&buffer_line, "/stat");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const stat = platform_file_open(path, "r");
    if (stat == NULL)
    {
        return 0;
    }
    char line[1024];
    const int read = platform_file_text_read(line, sizeof(line), stat) != NULL;
    platform_file_close(stat);
    // the name may contain spaces and parentheses: the fields are read after its last ')', the state, the parent, nine
    // skipped, then the process's own user and system ticks and those of the children it waited for
    const char *const after_name = read ? EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = line,
                                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ')')
                                        : NULL;
    unsigned long long ticks[4] = {0ULL, 0ULL, 0ULL, 0ULL};
    if ((after_name == NULL) || (run_stat_fields(after_name + 1, process, ticks) != 6))
    {
        return 0;
    }
    process->pid = pid;
    process->ticks = ticks[0] + ticks[1] + ticks[2] + ticks[3];
    process->in_tree = 0;
    return 1;
}

// every process /proc lists, the command's tree marked: its own process, then each whose parent is in it, until a pass
// adds none. NULL where /proc cannot be read
RunProcess *run_tree(pid_t root, unsigned long long *count)
{
    DIR *const listing = opendir("/proc");
    if (listing == NULL)
    {
        return NULL;
    }
    RunProcess *processes = NULL;
    unsigned long long capacity = 0ULL;
    int ok = 1;
    *count = 0ULL;
    for (const struct dirent *entry = readdir(listing); ok && (entry != NULL); entry = readdir(listing))
    {
        const char *end = NULL;
        const unsigned long long pid =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = entry->d_name, .end = &end, .base = 10);
        RunProcess read;
        if ((end == entry->d_name) || (*end != '\0') || !run_process_read(pid, &read))
        {
            continue;
        }
        if (*count == capacity)
        {
            capacity = (capacity == 0ULL) ? 256ULL : (capacity * 2ULL);
            RunProcess *const grown = (RunProcess *)platform_cell_grown(processes, (size_t)*count * sizeof(RunProcess),
                                                                        (size_t)capacity * sizeof(RunProcess),
                                                                        platform_place_g_program_owner, 0);
            ok = grown != NULL;
            processes = ok ? grown : processes;
        }
        if (ok)
        {
            processes[*count] = read;
            *count += 1ULL;
        }
    }
    closedir(listing);
    if (!ok || (processes == NULL))
    {
        platform_cell_released(processes, platform_place_g_program_owner);
        return NULL;
    }
    for (unsigned long long at = 0ULL; at < *count; at += 1ULL)
    {
        // a child's pid is positive. It converts exactly
        processes[at].in_tree = processes[at].pid == (unsigned long long)root;
    }
    int added = 1;
    while (added)
    {
        added = 0;
        for (unsigned long long at = 0ULL; at < *count; at += 1ULL)
        {
            for (unsigned long long parent = 0ULL; !processes[at].in_tree && (parent < *count); parent += 1ULL)
            {
                processes[at].in_tree = processes[parent].in_tree && (processes[parent].pid == processes[at].parent);
                added = added || processes[at].in_tree;
            }
        }
    }
    return processes;
}
#endif
