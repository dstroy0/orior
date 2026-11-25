// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_problems.c: the problems: each file's diagnostics, worst first, a file replaced whole as its report comes
// again. A report comes on the thread that read it, decoded once where the servers store it (servers_emit.c); that
// thread builds the file's head and pushes it, in a movable cell whose token rides the reports' ticket ring, under a
// ticket of its own, and sleeps while every slot is in flight. The view's thread pops the reports in the order of their
// tickets at each step, builds the table, one movable cell of the files' heads, over the last from each and releases
// the table and the report each replaced: the table is that thread's alone, and no other thread reads it. A press on a
// row closes a file's rows or points to the file and the place a diagnostic is at; the tree's check is started and
// stopped here
#include "../text/sort.h"
#include "servers.h"
#include "servers_internal.h"

#include "../command_line/pool.h"
#include "../command_line/rings.h"
#include "../editor/files.h"
#include "../text/text.h"

// the severity that counts as an error
#define CLI_PROBLEMS_ERROR 1u

// One file's problems, a record of the table's cell. It holds the token of its report's cell, the place its name
// starts in its path, and how many of its diagnostics are errors. Last, whether a press closed its rows
typedef struct
{
    const CarcerMovableEntry *report;
    unsigned long long name;
    unsigned int errors;
    int closed;
} ServersProblemFile;

// the max bytes of a diagnostic's message a row shows
#define CLI_PROBLEMS_MESSAGE 4096u

// the place in the table's cell of the file `at_`
#define CLI_PROBLEMS_FILE_AT(at_) ((unsigned long long)(at_) * sizeof(ServersProblemFile))

// A report on its way to the view's thread, in a movable cell whose token rides the ring. It holds the file's path,
// and its head where it has diagnostics. `has_file` is 0 where the report takes the file out
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    ServersProblemFile file;
    int has_file;
} ServersProblemsReport;

// The problems. They hold the reports' ticket ring, its turnstiles, and its entries, the problems' part of the rings
// block. They hold the next ticket a reporting thread takes, and the wake a reporting thread sleeps on while every
// entry is in flight. Then whether the problems are open to reports, and the check's progress as its last report gave
// it. Then what wakes the view's thread as a report comes.
//
// The rest is the view's thread's alone. It holds the next ticket it pops, and the table's cell, NULL for none. It
// holds the files in it, in the order their reports last came, and their errors. Last, the check's progress as its
// last step read it
typedef struct
{
    EmbedTurnstiles turnstiles;
    EmbedTicketRing ring;
    embed_atomic_u32 ticket;
    CliWake room;
    embed_atomic_u32 open;
    embed_atomic_u64 check_done;
    embed_atomic_u64 check_total;
    embed_atomic_uptr woken;
    embed_u32 taken;
    const CarcerMovableEntry *table;
    unsigned int count;
    unsigned int errors;
    unsigned long long done;
    unsigned long long total;
} ServersProblems;

typedef struct
{
    unsigned int at;
    const CarcerMovableEntry *report;
    char *out;
    const CliServerStoredDiagnostic *item;
    unsigned long long capacity;
    ServersProblemFile *file;
    const ServersProblemFile *file_read;
    const char *path;
    int has_file;
    unsigned int *found;
    unsigned int *count;
    unsigned int *errors;
} ProblemsCtx;

static ServersProblems s_problems;

// two diagnostics in the order displayed: the worse first, then the earlier
static int problems_order(const void *one, const void *other)
{
    const CliServerStoredDiagnostic *const first = (const CliServerStoredDiagnostic *)one;
    const CliServerStoredDiagnostic *const second = (const CliServerStoredDiagnostic *)other;
    if (first->severity != second->severity)
    {
        return (first->severity < second->severity) ? -1 : 1;
    }
    if (first->from_line != second->from_line)
    {
        return (first->from_line < second->from_line) ? -1 : 1;
    }
    if (first->from_col != second->from_col)
    {
        return (first->from_col < second->from_col) ? -1 : 1;
    }
    return 0;
}

// the file `at` of the table, read out of the table's cell
static ServersProblemFile problems_file_at(const ProblemsCtx *args)
{
    ServersProblemFile file = {NULL, 0ULL, 0u, 0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_problems.table, .offset = CLI_PROBLEMS_FILE_AT(args->at), .dst = &file, .bytes = sizeof(file)});
    return file;
}

// the path of a report read out of its cell into `out`, CLI_PATH_MAX_LEN bytes: its length
static unsigned long long problems_path(const ProblemsCtx *args)
{
    CliServerReportHead head = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    const unsigned long long length = g_cli_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = args->report, .offset = head.path_at, .bytes = CLI_PATH_MAX_LEN - 1ULL});
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->report, .offset = head.path_at, .dst = args->out, .bytes = length});
    args->out[length] = '\0';
    return length;
}

// the message of the diagnostic `item` read out of its report's cell into `out`, `capacity` bytes, cut to the first
// line and to what fits: its length
static unsigned long long problems_message(const ProblemsCtx *args)
{
    const unsigned long long length =
        (args->item->message_length < (args->capacity - 1ULL)) ? args->item->message_length : (args->capacity - 1ULL);
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->report, .offset = args->item->message_at, .dst = args->out, .bytes = length});
    args->out[length] = '\0';
    return length;
}

// The head of a file whose report has diagnostics. The build cuts each message to its first line where it lies. It
// writes the diagnostics back into the report's cell in the order on the screen. It counts the errors and finds the
// name from the path's end
static void problems_file_built(const ProblemsCtx *args)
{
    args->file->report = args->report;
    args->file->errors = 0u;
    args->file->closed = 0;
    CliServerReportHead head = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    // the diagnostics read into a cell of this call, cut, sorted and written back
    CliServerStoredDiagnostic *const items = g_cli_guard->temporary_buf_alloc(
        ((unsigned long long)head.count + 1ULL) * sizeof(CliServerStoredDiagnostic), g_cli_servers_owner);
    for (unsigned int at = 0u; (items != NULL) && (at < head.count); at += 1u)
    {
        items[at] = (CliServerStoredDiagnostic){0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->report,
                                                                .offset = CLI_SERVER_REPORT_ITEM_AT(at),
                                                                .dst = &items[at],
                                                                .bytes = sizeof(items[at])});
        char message[CLI_PROBLEMS_MESSAGE];
        const unsigned long long read = EMBED_CALL(problems_message, ProblemsCtx, .report = args->report,
                                                   .item = &items[at], .out = message, .capacity = sizeof(message));
        const unsigned long long line = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = message, .at = 0ULL, .size = read, .set = "\n");
        items[at].message_length = (line < read) ? line : items[at].message_length;
        args->file->errors += (items[at].severity == CLI_PROBLEMS_ERROR) ? 1u : 0u;
    }
    if (items != NULL)
    {
        EMBED_CALL(cli_sort, CliSortCfg, .base = items, .count = head.count, .size = sizeof(items[0]),
                   .order = problems_order);
        for (unsigned int at = 0u; at < head.count; at += 1u)
        {
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = args->report,
                                                                     .offset = CLI_SERVER_REPORT_ITEM_AT(at),
                                                                     .src = &items[at],
                                                                     .bytes = sizeof(items[at])});
        }
    }
    g_cli_guard->persistent_buf_release(items, g_cli_servers_owner);
    char path[CLI_PATH_MAX_LEN];
    const unsigned long long length = EMBED_CALL(problems_path, ProblemsCtx, .report = args->report, .out = path);
    args->file->name = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = path,
                                  .at = length, .set = "/\\");
}

// whether the report `report` is of the file at `path`
static int problems_report_is(const ProblemsCtx *args)
{
    CliServerReportHead head = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    return g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){
               .cell = args->report, .offset = head.path_at, .src = args->path, .bytes = length + 1ULL}) == 0;
}

// The table with the file of `path` taken out, and `file` put at the end where `has_file`. The place of the file of
// `path` goes into `found`, the count where none did. The new table's files and errors go into `count` and `errors`.
// Returns the new table's cell, or NULL where nothing changes or the pool has no cell for it
static const CarcerMovableEntry *problems_table_built(const ProblemsCtx *args)
{
    const unsigned int had = s_problems.count;
    *args->found = had;
    for (unsigned int at = 0u; at < had; at += 1u)
    {
        const ServersProblemFile one = EMBED_CALL(problems_file_at, ProblemsCtx, .at = at);
        if (EMBED_CALL(problems_report_is, ProblemsCtx, .report = one.report, .path = args->path))
        {
            *args->found = at;
            break;
        }
    }
    if ((*args->found == had) && !args->has_file)
    {
        return NULL;
    }
    const CarcerMovableEntry *const built =
        g_cli_guard->movable_buf_alloc(CLI_PROBLEMS_FILE_AT(had + 1u), g_cli_servers_owner);
    if (built == NULL)
    {
        return NULL;
    }
    *args->count = 0u;
    *args->errors = 0u;
    for (unsigned int at = 0u; at < had; at += 1u)
    {
        if (at != *args->found)
        {
            const ServersProblemFile one = EMBED_CALL(problems_file_at, ProblemsCtx, .at = at);
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = built, .offset = CLI_PROBLEMS_FILE_AT(*args->count), .src = &one, .bytes = sizeof(one)});
            *args->errors += one.errors;
            *args->count += 1u;
        }
    }
    if (args->has_file)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = built,
                                                                 .offset = CLI_PROBLEMS_FILE_AT(*args->count),
                                                                 .src = args->file_read,
                                                                 .bytes = sizeof(*args->file_read)});
        *args->errors += args->file_read->errors;
        *args->count += 1u;
    }
    return built;
}

// the view's thread woken, where the problems have a wake
static void problems_woken(void)
{
    // Explicit cast reads the word as the function pointer stored in it
    void (*const woken)(void) = (void (*)(void))(embed_word)EMBED_CALL(
        embed_atomic_uptr_table.load, EmbedAtomicUptrLoadCfg, .word = &s_problems.woken);
    if (woken != NULL)
    {
        woken();
    }
}

// From any thread: a file's report taken, and its head built where it has diagnostics. The report goes on the ring
// under a ticket of its own, and this thread sleeps while every entry is in flight. A report that comes after the
// problems close goes back
static void problems_reported(const CliServerDiagnosticsCfg *args)
{
    ServersProblemsReport report = {{0}, {NULL, 0ULL, 0u, 0}, 0};
    CliServerReportHead head = {0};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    if ((args->report != NULL) && (head.count != 0u) && (args->taken != NULL))
    {
        EMBED_CALL(problems_file_built, ProblemsCtx, .report = args->report, .file = &report.file);
        report.has_file = 1;
        *args->taken = 1;
    }
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(sizeof(report), g_cli_servers_owner);
    if (cell == NULL)
    {
        if (report.has_file)
        {
            cli_servers_report_release(&(CliServersReportCfg){.report = report.file.report});
        }
        return;
    }
    VerbaLinea path = {report.path, sizeof(report.path), 0ULL};
    mmgr_line_put(&path, args->path);
    (void)mmgr_line_finish(&path);
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = &report, .bytes = sizeof(report)});
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&s_problems.ticket, 1u);
    const embed_word stamp =
        EMBED_CALL(embed_ticket_stamp, EmbedTicketStampCfg, .epoch = cli_ticket_epoch(), .ticket = ticket);
    // Explicit cast carries the cell's token as the pass's entry, and as the word the ring's entry keeps
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)cell;
    embed_word seen = 0u;
    for (;;)
    {
        const unsigned int room = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_problems.room);
        if (cli_ticket_room(&s_problems.ring, ticket) &&
            EMBED_CALL(embed_ticket_ring_pass_grant, EmbedTicketRingPassGrantCfg, .set = &s_problems.turnstiles,
                       .address = address, .ring = &s_problems.ring, .stamp = stamp, .found_stamp = &seen))
        {
            break;
        }
        if (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_problems.open) == 0u)
        {
            if (report.has_file)
            {
                cli_servers_report_release(&(CliServersReportCfg){.report = report.file.report});
            }
            (void)g_cli_guard->movable_buf_release(cell, g_cli_servers_owner);
            return;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_problems.room, .seen = room);
    }
    (void)EMBED_CALL(embed_ticket_ring_pass_push, EmbedTicketRingPassPushCfg, .set = &s_problems.turnstiles,
                     .address = address, .ring = &s_problems.ring, .stamp = stamp, .job = (void *)(embed_word)cell,
                     .found_stamp = &seen);
    problems_woken();
}

// from any thread: how far the tree's check is
static void problems_check_moved(const CliServerCheckCfg *args)
{
    EMBED_CALL(embed_atomic_u64_table.store, EmbedAtomicU64StoreCfg, .word = &s_problems.check_done,
               .value = args->done);
    EMBED_CALL(embed_atomic_u64_table.store, EmbedAtomicU64StoreCfg, .word = &s_problems.check_total,
               .value = args->total);
    problems_woken();
}

CliServersSink cli_servers_problems_sink(const CliServersProblemsCfg *args)
{
    CliServersSink sink;
    memor.set(&(MemoriaCfg){.dst = &sink, .val = 0, .bytes = sizeof(sink)});
    sink.diagnostics = problems_reported;
    sink.check_moved = problems_check_moved;
    VerbaLinea tree = {sink.tree, sizeof(sink.tree), 0ULL};
    mmgr_line_put(&tree, (args->root != NULL) ? args->root : "");
    (void)mmgr_line_finish(&tree);
    return sink;
}

void cli_servers_problems_begin(void (*woken)(void))
{
    memor.set(&(MemoriaCfg){.dst = &s_problems.turnstiles, .val = 0, .bytes = sizeof(s_problems.turnstiles)});
    EmbedTicketRingSlot *const slots =
        (EmbedTicketRingSlot *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_PROBLEMS);
    memor.set(&(MemoriaCfg){.dst = slots, .val = 0, .bytes = CLI_PROBLEMS_RING_BYTES});
    EMBED_CALL(embed_ticket_ring_begin, EmbedTicketRingBeginCfg, .ring = &s_problems.ring, .slots = slots,
               .shift = CLI_PROBLEMS_RING_SHIFT);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_problems.ticket, .value = 0u);
    s_problems.room = (CliWake)CLI_WAKE_INIT;
    EMBED_CALL(embed_atomic_u64_table.store, EmbedAtomicU64StoreCfg, .word = &s_problems.check_done, .value = 0ULL);
    EMBED_CALL(embed_atomic_u64_table.store, EmbedAtomicU64StoreCfg, .word = &s_problems.check_total, .value = 0ULL);
    // Explicit cast stores the function pointer as the word that holds it
    EMBED_CALL(embed_atomic_uptr_table.store, EmbedAtomicUptrStoreCfg, .word = &s_problems.woken,
               .value = (embed_atomic_uptr_value)(embed_word)woken);
    s_problems.taken = 0u;
    s_problems.table = NULL;
    s_problems.count = 0u;
    s_problems.errors = 0u;
    s_problems.done = 0ULL;
    s_problems.total = 0ULL;
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_problems.open, .value = 1u);
}

// On the view's thread: every report pushed popped in the order of its ticket, and the table built over it. The
// table's cell and the report each replaces go back. Each entry goes back, and a reporting thread that waits for room
// wakes. Returns whether a report came
static int problems_taken(void)
{
    int moved = 0;
    // Explicit cast carries the problems' address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)&s_problems;
    embed_word stamp = 0u;
    while (cli_ticket_pushed(&s_problems.ring, s_problems.taken, &stamp))
    {
        void *job = NULL;
        embed_word seen = 0u;
        if (!EMBED_CALL(embed_ticket_ring_pass_pop, EmbedTicketRingPassPopCfg, .set = &s_problems.turnstiles,
                        .address = address, .ring = &s_problems.ring, .stamp = stamp, .job = &job,
                        .found_stamp = &seen))
        {
            break;
        }
        const CarcerMovableEntry *const cell = job;
        ServersProblemsReport report;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .dst = &report, .bytes = sizeof(report)});
        unsigned int found = 0u;
        unsigned int count = 0u;
        unsigned int errors = 0u;
        const CarcerMovableEntry *const built =
            EMBED_CALL(problems_table_built, ProblemsCtx, .path = report.path, .file_read = &report.file,
                       .has_file = report.has_file, .found = &found, .count = &count, .errors = &errors);
        if (built != NULL)
        {
            if (found < s_problems.count)
            {
                cli_servers_report_release(
                    &(CliServersReportCfg){.report = EMBED_CALL(problems_file_at, ProblemsCtx, .at = found).report});
            }
            if (s_problems.table != NULL)
            {
                (void)g_cli_guard->movable_buf_release(s_problems.table, g_cli_servers_owner);
            }
            s_problems.table = built;
            s_problems.count = count;
            s_problems.errors = errors;
            moved = 1;
        }
        else if (report.has_file)
        {
            cli_servers_report_release(&(CliServersReportCfg){.report = report.file.report});
        }
        (void)g_cli_guard->movable_buf_release(cell, g_cli_servers_owner);
        (void)EMBED_CALL(embed_ticket_ring_drain_release, EmbedTicketRingDrainReleaseCfg, .set = &s_problems.turnstiles,
                         .ring = &s_problems.ring, .stamp = stamp, .found_stamp = &seen);
        s_problems.taken += 1u;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_problems.room);
    }
    return moved;
}

int cli_servers_problems_step(void)
{
    int moved = problems_taken();
    const unsigned long long total =
        EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &s_problems.check_total);
    const unsigned long long done =
        EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &s_problems.check_done);
    moved = moved || (done != s_problems.done) || (total != s_problems.total);
    s_problems.done = done;
    s_problems.total = total;
    return moved;
}

void cli_servers_problems_end(void)
{
    cli_servers_stop_check();
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_problems.open, .value = 0u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_problems.room);
    (void)problems_taken();
    for (unsigned int at = 0u; at < s_problems.count; at += 1u)
    {
        cli_servers_report_release(
            &(CliServersReportCfg){.report = EMBED_CALL(problems_file_at, ProblemsCtx, .at = at).report});
    }
    if (s_problems.table != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_problems.table, g_cli_servers_owner);
    }
    s_problems.table = NULL;
    s_problems.count = 0u;
    s_problems.errors = 0u;
    EMBED_CALL(embed_atomic_uptr_table.store, EmbedAtomicUptrStoreCfg, .word = &s_problems.woken, .value = 0u);
}

CliProblemsCounts cli_servers_problems_counts(void)
{
    CliProblemsCounts counts = {0u, 0u, s_problems.errors, s_problems.done, s_problems.total, 0u};
    for (unsigned int at = 0u; at < s_problems.count; at += 1u)
    {
        const ServersProblemFile file = EMBED_CALL(problems_file_at, ProblemsCtx, .at = at);
        CliServerReportHead head = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = file.report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
        counts.files += 1u;
        counts.items += head.count;
        counts.rows += 1u + (file.closed ? 0u : head.count);
    }
    return counts;
}

int cli_servers_problems_checking(void)
{
    return (s_problems.total != 0ULL) && (s_problems.done < s_problems.total);
}

int cli_servers_problems_row(const CliServersProblemsCfg *args)
{
    // the row reads its texts out of the report's cell into the view's thread's own buffers, which last until its
    // next row
    static char s_path[CLI_PATH_MAX_LEN];
    static char s_message[CLI_PROBLEMS_MESSAGE];
    unsigned int first = 0u;
    for (unsigned int place = 0u; place < s_problems.count; place += 1u)
    {
        const ServersProblemFile file = EMBED_CALL(problems_file_at, ProblemsCtx, .at = place);
        CliServerReportHead head = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = file.report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
        const unsigned int under = file.closed ? 0u : head.count;
        if (args->at == first)
        {
            (void)EMBED_CALL(problems_path, ProblemsCtx, .report = file.report, .out = s_path);
            *args->row = (CliProblemRow){1,
                                         s_path,
                                         s_path + file.name,
                                         (file.name != 0ULL) ? (file.name - 1ULL) : 0ULL,
                                         head.count,
                                         file.closed,
                                         0u,
                                         "",
                                         0ULL,
                                         0ULL,
                                         0ULL};
            return 1;
        }
        if (args->at < (first + 1u + under))
        {
            CliServerStoredDiagnostic item = {0};
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = file.report,
                                    .offset = CLI_SERVER_REPORT_ITEM_AT(args->at - first - 1u),
                                    .dst = &item,
                                    .bytes = sizeof(item)});
            (void)EMBED_CALL(problems_path, ProblemsCtx, .report = file.report, .out = s_path);
            const unsigned long long length =
                EMBED_CALL(problems_message, ProblemsCtx, .report = file.report, .item = &item, .out = s_message,
                           .capacity = sizeof(s_message));
            *args->row = (CliProblemRow){0,      s_path,         "",           0ULL, 0u, 0, item.severity, s_message,
                                         length, item.from_line, item.from_col};
            return 1;
        }
        first += 1u + under;
    }
    return 0;
}

int cli_servers_problems_span(const CliServersProblemsCfg *args)
{
    for (unsigned int place = 0u; (args->path != NULL) && (place < s_problems.count); place += 1u)
    {
        const CarcerMovableEntry *const report = EMBED_CALL(problems_file_at, ProblemsCtx, .at = place).report;
        if (EMBED_CALL(problems_report_is, ProblemsCtx, .report = report, .path = args->path))
        {
            CliServerReportHead head = {0};
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
            if (args->at >= head.count)
            {
                return 0;
            }
            CliServerStoredDiagnostic item = {0};
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
                .cell = report, .offset = CLI_SERVER_REPORT_ITEM_AT(args->at), .dst = &item, .bytes = sizeof(item)});
            *args->span = (CliProblemSpan){item.severity, item.from_line, item.from_col, item.to_line, item.to_col};
            return 1;
        }
    }
    return 0;
}

int cli_servers_problems_press(const CliServersProblemsCfg *args)
{
    unsigned int first = 0u;
    for (unsigned int place = 0u; place < s_problems.count; place += 1u)
    {
        ServersProblemFile file = EMBED_CALL(problems_file_at, ProblemsCtx, .at = place);
        if (args->at == first)
        {
            file.closed = !file.closed;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = s_problems.table, .offset = CLI_PROBLEMS_FILE_AT(place), .src = &file, .bytes = sizeof(file)});
            return 0;
        }
        CliServerReportHead head = {0};
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = file.report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
        first += 1u + (file.closed ? 0u : head.count);
    }
    return cli_servers_problems_row(&(CliServersProblemsCfg){.at = args->at, .row = args->row}) && !args->row->is_file;
}

// the tree's check run over every file of the tree, the problems pane its sink, with the check's args the context
static void problems_files_checked(const CliFilesEachCfg *view)
{
    const CliServersProblemsCfg *const args = view->context;
    const CliPicked *const files = view->files;
    // The paths read out of the view into one cell of this call, with the list of them first. A first pass counts the
    // paths' bytes and their ends
    unsigned long long text_bytes = 0ULL;
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        (void)EMBED_CALL(cli_files_view_read.at, CliFilesViewCfg, .view = files, .index = at, .path_out = path);
        text_bytes += cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    const unsigned long long list_bytes = ((unsigned long long)files->count + 1ULL) * sizeof(const char *);
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(list_bytes + text_bytes, g_cli_servers_owner);
    if (cell == NULL)
    {
        return;
    }
    const char **const paths = (const char **)(void *)cell;
    char *const text = (char *)(cell + list_bytes);
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; at < files->count; at += 1u)
    {
        (void)EMBED_CALL(cli_files_view_read.at, CliFilesViewCfg, .view = files, .index = at, .path_out = text + used);
        paths[at] = text + used;
        used += cellul.len(&(CatenaFinitaCfg){.src = text + used, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    const CliServersSink sink = cli_servers_problems_sink(&(CliServersProblemsCfg){.root = args->root});
    (void)EMBED_CALL(cli_servers_root_sink_count.check_tree, CliServersRootSinkCountCfg, .root = args->root,
                     .files = paths, .count = files->count, .sink = &sink);
    g_cli_guard->persistent_buf_release(cell, g_cli_servers_owner);
}

void cli_servers_problems_check(const CliServersProblemsCfg *args)
{
    if (cli_servers_problems_checking())
    {
        cli_servers_stop_check();
        return;
    }
    if (args->root == NULL)
    {
        return;
    }
    CliServersProblemsCfg checked = *args;
    (void)EMBED_CALL(cli_files_root_paths.each, CliFilesRootPathsCfg, .root = args->root,
                     .each = problems_files_checked, .context = &checked);
}
