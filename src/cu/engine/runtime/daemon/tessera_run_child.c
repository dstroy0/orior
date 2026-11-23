// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_run_child.c: the job's folders and records, the daemon channel, the wait and the child
#include "../../../../platform/platform.h"
#include "../../engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "tessera_run_internal.h"

_Alignas(8) static const char s_run_unwatched[] =
    "  tessera_run: the command runs with no child to watch it: its records could not be made under the "
    "host's tessera state\n";
_Alignas(8) static const char s_run_children[] = "children";
#if (defined(_WIN32))
_Alignas(8) static const char s_run_wsl_unmeasured[] =
    "  tessera_run: only wsl.exe itself is measured: the Linux command needs -e or -- before it, and a Linux "
    "tessera_run ($TESSERA_RUN_WSL, or tessera_run beside this program)\n";
#endif

// every folder along the path made where it is not; 1 where the last is there
static int run_folders_make(char *path)
{
    for (size_t at = 1u; path[at] != '\0'; at += 1u)
    {
        const char character = path[at];
        if ((character == '/') || (character == RUN_SEPARATOR))
        {
            path[at] = '\0';
            run_folder_make(path);
            path[at] = character;
        }
    }
    run_folder_make(path);
    return run_folder_exists(path);
}

// the parent's record rewritten from its start: the pid launched, the child's pid once it reported it, and the
// keepalive, one more at each write. A line never shortens, and none is left from the one before
static void run_record_write(RunChannel *channel)
{
    if ((channel == NULL) || !channel->open)
    {
        return;
    }
    char line[RUN_RECORD_CAPACITY];
    VerbaLinea formed_line_12;
    VerbaLinea formed_line_13;
    const int length =
        channel->bound
            ? (formed_line_12 = mmgr_line_over(line, sizeof(line)), mmgr_line_put(&formed_line_12, "launched "),
               mmgr_line_decimal(&formed_line_12, channel->launched, 1u), mmgr_line_put(&formed_line_12, " child "),
               mmgr_line_decimal(&formed_line_12, channel->child, 1u), mmgr_line_put(&formed_line_12, " keepalive "),
               mmgr_line_decimal(&formed_line_12, channel->keepalive, 1u), mmgr_line_put(&formed_line_12, "\n"),
               (void)mmgr_line_finish(&formed_line_12), (int)formed_line_12.at)
            : (formed_line_13 = mmgr_line_over(line, sizeof(line)), mmgr_line_put(&formed_line_13, "launching "),
               mmgr_line_decimal(&formed_line_13, channel->launched, 1u), mmgr_line_put(&formed_line_13, " keepalive "),
               mmgr_line_decimal(&formed_line_13, channel->keepalive, 1u), mmgr_line_put(&formed_line_13, "\n"),
               (void)mmgr_line_finish(&formed_line_13), (int)formed_line_13.at);
    // a record's length is positive and compared whole against its capacity
    if ((length > 0) && ((size_t)length < sizeof(line)))
    {
        // a record's length is positive. It converts exactly
        run_record_put(channel, line, (size_t)length);
    }
    channel->keepalive += 1ULL;
}

void run_record_launched(RunChannel *channel, unsigned long long pid)
{
    if ((channel != NULL) && channel->open)
    {
        channel->launched = pid;
        run_record_write(channel);
    }
}

// the records' path: the host's tessera state, then children, then this process's pid and the job's signum, as a lost
// ticket's record is named. The folders are made where they are not
static int run_records_place(RunChannel *channel, const EngineSignum *signum)
{
    unsigned char host[TESSERA_DEVICE_BYTES];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = host, .bytes = sizeof(host), .val = 0);
    char *const records = channel->records;
    const size_t capacity = sizeof(channel->records);
    // the capacity is a buffer size this file allocates, which an unsigned int counts
    if (!tessera_path_state(host, records, (unsigned int)capacity))
    {
        return 0;
    }
    size_t at = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = records, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    VerbaLinea formed_line_1 = mmgr_line_over(records + at, capacity - at);
    mmgr_line_ch(&formed_line_1, (char)(RUN_SEPARATOR));
    mmgr_line_put(&formed_line_1, s_run_children);
    (void)mmgr_line_finish(&formed_line_1);
    const int folder = (int)formed_line_1.at;
    // a non-negative length is compared whole against the capacity
    if ((folder <= 0) || ((size_t)folder >= (capacity - at)) || !run_folders_make(records))
    {
        return 0;
    }
    at = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = records, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    VerbaLinea formed_line_2 = mmgr_line_over(records + at, capacity - at);
    mmgr_line_ch(&formed_line_2, (char)(RUN_SEPARATOR));
    mmgr_line_hex(&formed_line_2, run_self_pid(), 16u);
    mmgr_line_put(&formed_line_2, "-");
    (void)mmgr_line_finish(&formed_line_2);
    const int named = (int)formed_line_2.at;
    // a non-negative length is compared whole against the capacity
    if ((named <= 0) || ((size_t)named >= (capacity - at)))
    {
        return 0;
    }
    at = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = records, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (unsigned int byte = 0u; byte < ENGINE_SIGNUM_BYTES; byte += 1u)
    {
        if ((at + 3u) > capacity)
        {
            return 0;
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(records + at, capacity - at);
            mmgr_line_hex(&buffer_line, signum->bytes[byte], 2u);
            (void)mmgr_line_finish(&buffer_line);
        }
        at += 2u;
    }
    return 1;
}

// a child tessera_run is put between this process and the command where one is requested (--child), and always for a
// WSL command, whose tree only a child inside the VM can measure. The words to start, NULL-ended, or NULL for the
// command as it is
char **run_channel_open(RunChannel *channel, const RunRequest *request, char *const *words, int count,
                        const EngineSignum *signum, int *started_count)
{
    int inside_at = 0;
#if defined(_WIN32)
    channel->across = run_names_wsl(words[0]);
    if (channel->across)
    {
        inside_at = 1;
        while ((inside_at < count) &&
               (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[inside_at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "-e", .ci = EMBED_FALSE)) &&
               (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[inside_at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--exec", .ci = EMBED_FALSE)) &&
               (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[inside_at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "--", .ci = EMBED_FALSE)))
        {
            inside_at += 1;
        }
        inside_at += 1;
        if ((inside_at >= count) || !run_wsl_program(channel->program, sizeof(channel->program)))
        {
            {
                const char *const error_text = s_run_wsl_unmeasured;
                platform_terminal_write(1, error_text,
                                        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = error_text,
                                                                       .cap = PLATFORM_TEXT_TO_TERMINATOR));
            }
            return NULL;
        }
    }
#endif
    if (!channel->across && !request->child)
    {
        return NULL;
    }
    char path[ENGINE_PATH_CAPACITY];
    const int placed = (channel->across || (run_own_path(channel->program, sizeof(channel->program)) != 0u)) &&
                       run_records_place(channel, signum) &&
                       run_path_joined(channel->records, ".parent", path, sizeof(path)) &&
                       run_record_make(channel, path);
#if defined(_WIN32)
    const int named =
        placed && (channel->across
                       ? run_mounted_path(channel->records, channel->named_records, sizeof(channel->named_records))
                       : run_path_joined(channel->records, "", channel->named_records, sizeof(channel->named_records)));
#else
    const int named =
        placed && run_path_joined(channel->records, "", channel->named_records, sizeof(channel->named_records));
#endif
    // a word count is positive. It converts exactly
    char **const watched = named ? (char **)platform_cell_taken(((size_t)count + RUN_WATCH_WORDS + 1u) * sizeof(char *),
                                                                platform_place_g_program_owner, 0)
                                 : NULL;
    if (watched == NULL)
    {
        if (placed)
        {
            run_record_close(channel);
            (void)platform_file_removed(path);
        }
        {
            const char *const error_text = s_run_unwatched;
            platform_terminal_write(1, error_text,
                                    (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = error_text,
                                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR));
        }
        return NULL;
    }
    // records a child of an earlier process of the same pid left are not read as this child's
    (void)platform_file_removed(run_path_joined(channel->records, ".child", path, sizeof(path)) ? path : "");
    (void)platform_file_removed(run_path_joined(channel->records, ".log", path, sizeof(path)) ? path : "");
    // the words before the command's own, a count at least 0. It converts exactly
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = watched, .src = words, .bytes = (size_t)inside_at * sizeof(char *));
    watched[inside_at] = channel->program;
    watched[inside_at + 1] = "--parent";
    watched[inside_at + 2] = channel->named_records;
    watched[inside_at + 3] = "--";
    // the command's own words, a positive count. It converts exactly
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = watched + inside_at + RUN_WATCH_WORDS, .src = words + inside_at,
               .bytes = (size_t)(count - inside_at) * sizeof(char *));
    watched[count + RUN_WATCH_WORDS] = NULL;
    channel->open = 1;
    *started_count = count + RUN_WATCH_WORDS;
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  tessera_run: the command runs under a child tessera_run (");
        mmgr_line_put(&engine_note.line, channel->program);
        mmgr_line_put(&engine_note.line, "), their records at ");
        mmgr_line_put(&engine_note.line, channel->records);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return watched;
}

// the parent's side at each step: the child's record read, its pid recorded once it reports it lives, and the keepalive
// rewritten
static void run_channel_refresh(RunChannel *channel)
{
    if (!channel->open)
    {
        return;
    }
    char path[ENGINE_PATH_CAPACITY];
    char state[RUN_STATE_CAPACITY];
    unsigned long long numbers[RUN_NUMBERS] = {0ULL, 0ULL, 0ULL, 0ULL};
    const int fields =
        run_path_joined(channel->records, ".child", path, sizeof(path)) ? run_record_read(path, state, numbers) : -1;
    if (fields >= 2)
    {
        // on one system the child is the process this one launched, its pid the same; across the VM it has its own
        if (!channel->bound && (channel->across || (numbers[0] == channel->launched)))
        {
            channel->bound = 1;
            channel->child = numbers[0];
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  tessera_run: child ");
                mmgr_line_decimal(&engine_note.line, channel->child, 1u);
                mmgr_line_put(&engine_note.line, ", launched as ");
                mmgr_line_decimal(&engine_note.line, channel->launched, 1u);
                mmgr_line_put(&engine_note.line, ", reports it lives; its pid is recorded");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
        }
        // the command's processors between two of the child's records, on its clock, once they span half a sweep: a
        // record read late, or missed mid-write, shifts no time into the next reading
        const int spans = (fields >= 3) && (numbers[2] >= (channel->reported_wall + (RUN_SWEEP_MICROSECONDS / 2ULL)));
        if (channel->bound && (numbers[0] == channel->child) && spans)
        {
            const unsigned long long grew = (numbers[1] > channel->reported) ? (numbers[1] - channel->reported) : 0ULL;
            channel->rate = (grew * TESSERA_HOST_PROCESSOR) / (numbers[2] - channel->reported_wall);
            channel->reported = numbers[1];
            channel->reported_wall = numbers[2];
        }
        if (channel->bound && (numbers[0] == channel->child))
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = channel->state, .src = state, .bytes = sizeof(channel->state));
        }
    }
    run_record_write(channel);
}

// a child that ended its command itself leaves nothing to relaunch or resume, and the records go. Any other end retains
// them where the child's log records why
void run_channel_close(RunChannel *channel)
{
    if (!channel->open)
    {
        return;
    }
    run_record_close(channel);
    char path[ENGINE_PATH_CAPACITY];
    const int ended = EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = channel->state, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = "ended", .ci = EMBED_FALSE);
    PlatformFile *const logged = (!ended && run_path_joined(channel->records, ".log", path, sizeof(path)))
                                     ? platform_file_open(path, "rb")
                                     : NULL;
    if (logged != NULL)
    {
        platform_file_close(logged);
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  tessera_run: the child's records are retained, its log at ");
            mmgr_line_put(&engine_note.line, path);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return;
    }
    (void)platform_file_removed(run_path_joined(channel->records, ".parent", path, sizeof(path)) ? path : "");
    (void)platform_file_removed(run_path_joined(channel->records, ".child", path, sizeof(path)) ? path : "");
    (void)platform_file_removed(run_path_joined(channel->records, ".log", path, sizeof(path)) ? path : "");
}

// reports the command's processors each sweep until it ends, the last reading only when it spans half a sweep, since
// a shorter one is mostly the clock's own step. Across the VM the child's last rate is added to this job's own. A
// child not yet greeted is read, and maintained alive, every greeting step
void run_wait(RunChild *child, RunChannel *channel, TesseraClient *client, TesseraTicket *ticket, EngineError *error)
{
    const int across = channel->open && channel->across;
    unsigned long long cpu_before = 0ULL;
    int reporting = run_cpu(child, &cpu_before) || across;
    unsigned long long wall_before = run_now();
    int ended = 0;
    while (!ended)
    {
        const int greeting = channel->open && !channel->bound;
        ended = run_ended(child, greeting ? RUN_GREETING_MICROSECONDS : RUN_SWEEP_MICROSECONDS);
        run_channel_refresh(channel);
        const unsigned long long wall = run_now();
        const unsigned long long spent = wall - wall_before;
        if (!ended && (spent < RUN_SWEEP_MICROSECONDS))
        {
            continue;
        }
        unsigned long long cpu = 0ULL;
        const int read = reporting && run_cpu(child, &cpu);
        const int complete = (spent != 0ULL) && (!ended || ((2ULL * spent) >= RUN_SWEEP_MICROSECONDS));
        if (reporting && (read || across) && complete)
        {
            // the processors used over the reading, in thousandths: the tree's processor time over the wall time
            const unsigned long long own =
                read ? ((((cpu > cpu_before) ? (cpu - cpu_before) : 0ULL) * TESSERA_HOST_PROCESSOR) / spent) : 0ULL;
            const unsigned long long used = own + (across ? channel->rate : 0ULL);
            reporting = tessera_job_report(client, ticket, used, error) == 0L;
        }
        cpu_before = read ? cpu : cpu_before;
        wall_before = wall;
    }
}

// the child's record, rewritten whole: its state, its pid, its command's processor time and the wall time it was read
// at since the child began, then its exit once it has one
static void run_child_write(const char *records, const char *state, unsigned long long pid, unsigned long long cpu,
                            unsigned long long wall, int ended, int code)
{
    char path[ENGINE_PATH_CAPACITY];
    PlatformFile *const file =
        run_path_joined(records, ".child", path, sizeof(path)) ? platform_file_open(path, "wb") : NULL;
    if (file == NULL)
    {
        return;
    }
    if (ended)
    {
        // an exit code is written as the unsigned word a process ends with, which a reader parses as digits
        {
            mmgr_line_put(platform_file_line(file), state);
            mmgr_line_put(platform_file_line(file), " ");
            mmgr_line_decimal(platform_file_line(file), pid, 1u);
            mmgr_line_put(platform_file_line(file), " cpu ");
            mmgr_line_decimal(platform_file_line(file), cpu, 1u);
            mmgr_line_put(platform_file_line(file), " wall ");
            mmgr_line_decimal(platform_file_line(file), wall, 1u);
            mmgr_line_put(platform_file_line(file), " exit ");
            mmgr_line_decimal(platform_file_line(file), (unsigned int)code, 1u);
            mmgr_line_put(platform_file_line(file), "\n");
        }
    }
    else
    {
        mmgr_line_put(platform_file_line(file), state);
        mmgr_line_put(platform_file_line(file), " ");
        mmgr_line_decimal(platform_file_line(file), pid, 1u);
        mmgr_line_put(platform_file_line(file), " cpu ");
        mmgr_line_decimal(platform_file_line(file), cpu, 1u);
        mmgr_line_put(platform_file_line(file), " wall ");
        mmgr_line_decimal(platform_file_line(file), wall, 1u);
        mmgr_line_put(platform_file_line(file), "\n");
    }
    platform_file_close(file);
}

// the command's processor time read now, or the time read before where that is more: a tree that has lost processes
// no longer counts theirs, and the time it has used never falls
static unsigned long long run_child_cpu(const RunChild *child, unsigned long long before)
{
    unsigned long long read = 0ULL;
    return (run_cpu(child, &read) && (read > before)) ? read : before;
}

// the child's side. It reports it lives, reads that its parent launched it, checks its own pid against the one the
// parent recorded, and only then runs the command, its processor time in its record each sweep. A parent whose
// keepalive stops is looked for: one gone, or one that still locks its record and has not answered for the unresponsive
// time, has the child end the command, log it and exit, and whoever runs the parent may relaunch or resume it
int run_child(const RunRequest *request, char **arguments, int count, unsigned long long mask, const char *label)
{
    const char *const records = request->parent;
    const unsigned long long self = run_self_pid();
    const unsigned long long silent = run_limit("TESSERA_RUN_SILENT_MS", RUN_SILENT_MICROSECONDS);
    const unsigned long long unresponsive = run_limit("TESSERA_RUN_UNRESPONSIVE_MS", RUN_UNRESPONSIVE_MICROSECONDS);
    char parent[ENGINE_PATH_CAPACITY];
    char entry[RUN_ENTRY_CAPACITY];
    char state[RUN_STATE_CAPACITY];
    unsigned long long parent_fields[RUN_NUMBERS] = {0ULL, 0ULL, 0ULL, 0ULL};
    if (!run_path_joined(records, ".parent", parent, sizeof(parent)) ||
        (run_record_read(parent, state, parent_fields) < 2))
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
            mmgr_line_put(&buffer_line, "child ");
            mmgr_line_decimal(&buffer_line, self, 1u);
            mmgr_line_put(&buffer_line, ": no launch record at ");
            mmgr_line_put(&buffer_line, records);
            mmgr_line_put(&buffer_line, ".parent; ");
            mmgr_line_put(&buffer_line, label);
            mmgr_line_put(&buffer_line, " was not run");
            (void)mmgr_line_finish(&buffer_line);
        }
        run_log(records, entry);
        return RUN_FAILED;
    }
    run_child_write(records, "alive", self, 0ULL, 0ULL, 0, 0);
    // the parent reads this record and records this pid within its greeting step
    const unsigned long long greeted = run_now();
    int bound = 0;
    while (!bound && ((run_now() - greeted) < silent))
    {
        run_pause(RUN_GREETING_MICROSECONDS);
        const int fields = run_record_read(parent, state, parent_fields);
        bound =
            (fields == 3) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = state, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                         .other = "launched", .ci = EMBED_FALSE));
    }
    if (!bound)
    {
        const int alive = run_parent_alive(parent);
        {
            VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
            mmgr_line_put(&buffer_line, "child ");
            mmgr_line_decimal(&buffer_line, self, 1u);
            mmgr_line_put(&buffer_line, ": its parent ");
            mmgr_line_put(&buffer_line, alive ? "locks its record" : "is gone");
            mmgr_line_put(&buffer_line, " and did not record this pid; ");
            mmgr_line_put(&buffer_line, label);
            mmgr_line_put(&buffer_line, " was not run");
            (void)mmgr_line_finish(&buffer_line);
        }
        run_log(records, entry);
        run_child_write(records, "orphaned", self, 0ULL, 0ULL, 1, RUN_ORPHANED);
        return RUN_ORPHANED;
    }
    // the parent's record names the child it launched: the command runs only where that is this process
    if (parent_fields[1] != self)
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
            mmgr_line_put(&buffer_line, "child ");
            mmgr_line_decimal(&buffer_line, self, 1u);
            mmgr_line_put(&buffer_line, ": the parent's record names child ");
            mmgr_line_decimal(&buffer_line, parent_fields[1], 1u);
            mmgr_line_put(&buffer_line, ", launched as ");
            mmgr_line_decimal(&buffer_line, parent_fields[0], 1u);
            mmgr_line_put(&buffer_line, ", not this process; ");
            mmgr_line_put(&buffer_line, label);
            mmgr_line_put(&buffer_line, " was not run");
            (void)mmgr_line_finish(&buffer_line);
        }
        run_log(records, entry);
        return RUN_FAILED;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
        mmgr_line_put(&buffer_line, "child ");
        mmgr_line_decimal(&buffer_line, self, 1u);
        mmgr_line_put(&buffer_line, ", launched as ");
        mmgr_line_decimal(&buffer_line, parent_fields[0], 1u);
        mmgr_line_put(&buffer_line, ": its pid confirmed against the parent's record; ");
        mmgr_line_put(&buffer_line, label);
        mmgr_line_put(&buffer_line, " runs on processors 0x");
        mmgr_line_hex(&buffer_line, mask, 1u);
        mmgr_line_put(&buffer_line, " at below normal priority");
        (void)mmgr_line_finish(&buffer_line);
    }
    run_log(records, entry);
    run_child_write(records, "confirmed", self, 0ULL, 0ULL, 0, 0);
    RunChild child;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &child, .bytes = sizeof(child), .val = 0);
    if (!run_start(&child, arguments + request->command, count - request->command, mask, NULL))
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
            mmgr_line_put(&buffer_line, "child ");
            mmgr_line_decimal(&buffer_line, self, 1u);
            mmgr_line_put(&buffer_line, ": ");
            mmgr_line_put(&buffer_line, label);
            mmgr_line_put(&buffer_line, " could not be started");
            (void)mmgr_line_finish(&buffer_line);
        }
        run_log(records, entry);
        run_child_write(records, "ended", self, 0ULL, 0ULL, 1, RUN_NOT_STARTED);
        return RUN_NOT_STARTED;
    }
    unsigned long long keepalive = parent_fields[2];
    const unsigned long long began = run_now();
    unsigned long long seen_at = began;
    unsigned long long cpu = 0ULL;
    int ended = 0;
    int orphaned = 0;
    int alive = 1;
    int looked = 0;
    while (!ended && !orphaned)
    {
        ended = run_ended(&child, RUN_SWEEP_MICROSECONDS);
        cpu = run_child_cpu(&child, cpu);
        run_child_write(records, "confirmed", self, cpu, run_now() - began, 0, 0);
        const int fields = run_record_read(parent, state, parent_fields);
        const unsigned long long now = run_now();
        const int parent_alive = (fields == 3) && (parent_fields[2] != keepalive);
        if (parent_alive && looked)
        {
            {
                VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
                mmgr_line_put(&buffer_line, "child ");
                mmgr_line_decimal(&buffer_line, self, 1u);
                mmgr_line_put(&buffer_line, ": the parent's keepalive is received again after ");
                mmgr_line_decimal(&buffer_line, (now - seen_at) / RUN_MILLION, 1u);
                mmgr_line_put(&buffer_line, ".");
                mmgr_line_decimal(&buffer_line, ((now - seen_at) % RUN_MILLION) / RUN_THOUSAND, 3u);
                mmgr_line_put(&buffer_line, " s");
                (void)mmgr_line_finish(&buffer_line);
            }
            run_log(records, entry);
        }
        looked = parent_alive ? 0 : looked;
        seen_at = parent_alive ? now : seen_at;
        keepalive = parent_alive ? parent_fields[2] : keepalive;
        const unsigned long long silence = now - seen_at;
        // a keepalive unchanged past the silent time: the parent is looked for, by whether it still locks its record
        alive = (silence < silent) || run_parent_alive(parent);
        orphaned = !ended && (!alive || (silence >= unresponsive));
        if (!ended && !orphaned && (silence >= silent) && !looked)
        {
            {
                VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
                mmgr_line_put(&buffer_line, "child ");
                mmgr_line_decimal(&buffer_line, self, 1u);
                mmgr_line_put(&buffer_line, ": no keepalive for ");
                mmgr_line_decimal(&buffer_line, silence / RUN_MILLION, 1u);
                mmgr_line_put(&buffer_line, ".");
                mmgr_line_decimal(&buffer_line, (silence % RUN_MILLION) / RUN_THOUSAND, 3u);
                mmgr_line_put(&buffer_line, " s; the parent still locks its record, and is given until ");
                mmgr_line_decimal(&buffer_line, unresponsive / RUN_MILLION, 1u);
                mmgr_line_put(&buffer_line, " s");
                (void)mmgr_line_finish(&buffer_line);
            }
            run_log(records, entry);
            looked = 1;
        }
    }
    if (orphaned)
    {
        const unsigned long long silence = run_now() - seen_at;
        const unsigned long long forced = run_end_command(&child, RUN_GRACE_MICROSECONDS);
        cpu = run_child_cpu(&child, cpu);
        const int code = run_finish(&child);
        run_child_write(records, "orphaned", self, cpu, run_now() - began, 1, code);
        {
            VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
            mmgr_line_put(&buffer_line, "child ");
            mmgr_line_decimal(&buffer_line, self, 1u);
            mmgr_line_put(&buffer_line, ": its parent ");
            mmgr_line_put(&buffer_line, alive ? "locks its record but is not responding" : "is gone");
            mmgr_line_put(&buffer_line, ", with no keepalive for ");
            mmgr_line_decimal(&buffer_line, silence / RUN_MILLION, 1u);
            mmgr_line_put(&buffer_line, ".");
            mmgr_line_decimal(&buffer_line, (silence % RUN_MILLION) / RUN_THOUSAND, 3u);
            mmgr_line_put(&buffer_line, " s; ");
            mmgr_line_put(&buffer_line, label);
            mmgr_line_put(&buffer_line, " was requested to end, ");
            mmgr_line_decimal(&buffer_line, forced, 1u);
            mmgr_line_put(&buffer_line, " of its processes ended by force, exit ");
            mmgr_line_decimal_signed(&buffer_line, code);
            mmgr_line_put(&buffer_line, ". The parent may relaunch or resume it");
            (void)mmgr_line_finish(&buffer_line);
        }
        run_log(records, entry);
        return RUN_ORPHANED;
    }
    const int code = run_finish(&child);
    run_child_write(records, "ended", self, cpu, run_now() - began, 1, code);
    {
        VerbaLinea buffer_line = mmgr_line_over(entry, sizeof(entry));
        mmgr_line_put(&buffer_line, "child ");
        mmgr_line_decimal(&buffer_line, self, 1u);
        mmgr_line_put(&buffer_line, ": ");
        mmgr_line_put(&buffer_line, label);
        mmgr_line_put(&buffer_line, " ended, exit ");
        mmgr_line_decimal_signed(&buffer_line, code);
        mmgr_line_put(&buffer_line, ", after ");
        mmgr_line_decimal(&buffer_line, cpu / RUN_MILLION, 1u);
        mmgr_line_put(&buffer_line, ".");
        mmgr_line_decimal(&buffer_line, (cpu % RUN_MILLION) / RUN_THOUSAND, 3u);
        mmgr_line_put(&buffer_line, " s of processor time");
        (void)mmgr_line_finish(&buffer_line);
    }
    run_log(records, entry);
    return code;
}
