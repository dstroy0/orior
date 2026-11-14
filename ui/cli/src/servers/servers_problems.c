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

// One file's problems, a record of the table's cell: the token of its report's cell, the place its name starts in its
// path, how many of its diagnostics are errors, and whether its rows are closed, which a press sets
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

// A report on its way to the view's thread, in a movable cell whose token rides the ring: the file's path, and its
// head where it has diagnostics, `has_file` 0 where the file is taken out
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    ServersProblemFile file;
    int has_file;
} ServersProblemsReport;

// The problems: the reports' ticket ring, its turnstiles, its slots the problems' part of the rings block, the next
// ticket a reporting thread takes, the wake a reporting thread sleeps on while every slot is in flight, whether the
// problems are open to reports, the check's progress as it was last reported, and what wakes the view's thread as a
// report comes; then, the view's thread's alone: the next ticket it pops, the table's cell, NULL for none, the files in
// it in the order their reports last came and their errors, and the check's progress as its last step read it
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
static ServersProblemFile problems_file_at(unsigned int at)
{
    ServersProblemFile file = {NULL, 0ULL, 0u, 0};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_problems.table, .offset = CLI_PROBLEMS_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
    return file;
}

// the head of a report's cell, and the diagnostic `at` of it, each read out of the cell
static CliServerReportHead problems_head(const CarcerMovableEntry *report)
{
    CliServerReportHead head = {0u, 0ULL};
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = report, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    return head;
}

static CliServerStoredDiagnostic problems_item(const CarcerMovableEntry *report, unsigned int at)
{
    CliServerStoredDiagnostic item = {0ULL, 0ULL, 0ULL, 0ULL, 0u, 0ULL, 0ULL};
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = report, .offset = CLI_SERVER_REPORT_ITEM_AT(at), .dst = &item, .bytes = sizeof(item)});
    return item;
}

// the path of a report read out of its cell into `out`, CLI_PATH_MAX_LEN bytes: its length
static unsigned long long problems_path(const CarcerMovableEntry *report, char *out)
{
    const CliServerReportHead head = problems_head(report);
    const unsigned long long length = g_cli_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = report, .offset = head.path_at, .bytes = CLI_PATH_MAX_LEN - 1ULL});
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = report, .offset = head.path_at, .dst = out, .bytes = length});
    out[length] = '\0';
    return length;
}

// the message of the diagnostic `item` read out of its report's cell into `out`, `capacity` bytes, cut to the first
// line and to what fits: its length
static unsigned long long problems_message(const CarcerMovableEntry *report, const CliServerStoredDiagnostic *item,
                                           char *out, unsigned long long capacity)
{
    const unsigned long long length =
        (item->message_length < (capacity - 1ULL)) ? item->message_length : (capacity - 1ULL);
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = report, .offset = item->message_at, .dst = out, .bytes = length});
    out[length] = '\0';
    return length;
}

// the head of a file whose report has diagnostics: each message cut to its first line where it lies, the diagnostics
// in the order displayed, written back into the report's cell, the errors counted and the name found from the path's end
static void problems_file_built(const CarcerMovableEntry *report, ServersProblemFile *file)
{
    file->report = report;
    file->errors = 0u;
    file->closed = 0;
    const CliServerReportHead head = problems_head(report);
    // the diagnostics read into a cell of this call, cut, sorted and written back
    CliServerStoredDiagnostic *const items = g_cli_guard->temporary_buf_alloc(
        ((unsigned long long)head.count + 1ULL) * sizeof(CliServerStoredDiagnostic), g_cli_servers_owner);
    for (unsigned int at = 0u; (items != NULL) && (at < head.count); at += 1u)
    {
        items[at] = problems_item(report, at);
        char message[CLI_PROBLEMS_MESSAGE];
        const unsigned long long read = problems_message(report, &items[at], message, sizeof(message));
        const unsigned long long line = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = message, .at = 0ULL, .size = read, .set = "\n");
        items[at].message_length = (line < read) ? line : items[at].message_length;
        file->errors += (items[at].severity == CLI_PROBLEMS_ERROR) ? 1u : 0u;
    }
    if (items != NULL)
    {
        EMBED_CALL(cli_sort, CliSortCfg, .base = items, .count = head.count, .size = sizeof(items[0]),
                   .order = problems_order);
        for (unsigned int at = 0u; at < head.count; at += 1u)
        {
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = report,
                                                                     .offset = CLI_SERVER_REPORT_ITEM_AT(at),
                                                                     .src = &items[at],
                                                                     .bytes = sizeof(items[at])});
        }
    }
    g_cli_guard->persistent_buf_release(items, g_cli_servers_owner);
    char path[CLI_PATH_MAX_LEN];
    const unsigned long long length = problems_path(report, path);
    file->name = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = path, .at = length,
                            .set = "/\\");
}

// whether the report `report` is of the file at `path`
static int problems_report_is(const CarcerMovableEntry *report, const char *path)
{
    const CliServerReportHead head = problems_head(report);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
    return g_cli_guard->movable_buf_compare(
               &(CarcerMovableCfg){.cell = report, .offset = head.path_at, .src = path, .bytes = length + 1ULL}) == 0;
}

// The table with the file of `path` taken out and `file` put at the end where `has_file`, the place the file of
// `path` was in into `found`, the count where none did, the new table's files and errors into `count` and `errors`:
// the new table's cell, or NULL where nothing changes or its cell could not be taken
static const CarcerMovableEntry *problems_table_built(const char *path, const ServersProblemFile *file, int has_file,
                                                      unsigned int *found, unsigned int *count, unsigned int *errors)
{
    const unsigned int had = s_problems.count;
    *found = had;
    for (unsigned int at = 0u; at < had; at += 1u)
    {
        const ServersProblemFile one = problems_file_at(at);
        if (problems_report_is(one.report, path))
        {
            *found = at;
            break;
        }
    }
    if ((*found == had) && !has_file)
    {
        return NULL;
    }
    const CarcerMovableEntry *const built =
        g_cli_guard->movable_buf_alloc(CLI_PROBLEMS_FILE_AT(had + 1u), g_cli_servers_owner);
    if (built == NULL)
    {
        return NULL;
    }
    *count = 0u;
    *errors = 0u;
    for (unsigned int at = 0u; at < had; at += 1u)
    {
        if (at != *found)
        {
            const ServersProblemFile one = problems_file_at(at);
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = built, .offset = CLI_PROBLEMS_FILE_AT(*count), .src = &one, .bytes = sizeof(one)});
            *errors += one.errors;
            *count += 1u;
        }
    }
    if (has_file)
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = built, .offset = CLI_PROBLEMS_FILE_AT(*count), .src = file, .bytes = sizeof(*file)});
        *errors += file->errors;
        *count += 1u;
    }
    return built;
}

// the view's thread woken, where the problems have a wake
static void problems_woken(void)
{
    // Explicit cast reads the word as the function pointer stored in it
    void (*const woken)(void) = (void (*)(void))(embed_word)embed_atomic_uptr_load(&s_problems.woken);
    if (woken != NULL)
    {
        woken();
    }
}

// From any thread: a file's report taken and its head built where it has diagnostics, the report pushed on the ring
// under a ticket of its own, this thread asleep while every slot is in flight; a report that comes once the problems
// are closed is let go
static void problems_reported(const CliServerDiagnosticsCfg *args)
{
    ServersProblemsReport report = {{0}, {NULL, 0ULL, 0u, 0}, 0};
    if ((args->report != NULL) && (problems_head(args->report).count != 0u) && (args->taken != NULL))
    {
        problems_file_built(args->report, &report.file);
        report.has_file = 1;
        *args->taken = 1;
    }
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(sizeof(report), g_cli_servers_owner);
    if (cell == NULL)
    {
        if (report.has_file)
        {
            cli_servers_report_release(report.file.report);
        }
        return;
    }
    VerbaLinea path = {report.path, sizeof(report.path), 0ULL};
    mmgr_line_put(&path, args->path);
    (void)mmgr_line_finish(&path);
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = &report, .bytes = sizeof(report)});
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&s_problems.ticket, 1u);
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), ticket);
    // Explicit cast carries the cell's token as the pass's entry, and as the word the slot holds
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)cell;
    embed_u64 seen = 0u;
    for (;;)
    {
        const unsigned int room = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_problems.room);
        if (cli_ticket_room(&s_problems.ring, ticket) &&
            embed_ticket_ring_granted(&s_problems.turnstiles, address, &s_problems.ring, stamp, &seen))
        {
            break;
        }
        if (embed_atomic_u32_load(&s_problems.open) == 0u)
        {
            if (report.has_file)
            {
                cli_servers_report_release(report.file.report);
            }
            (void)g_cli_guard->movable_buf_release(cell, g_cli_servers_owner);
            return;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_problems.room, .seen = room);
    }
    (void)embed_ticket_ring_pushed(&s_problems.turnstiles, address, &s_problems.ring, stamp, (void *)(embed_word)cell,
                                   &seen);
    problems_woken();
}

// from any thread: how far the tree's check has gone
static void problems_check_moved(const CliServerCheckCfg *args)
{
    embed_atomic_u64_store(&s_problems.check_done, args->done);
    embed_atomic_u64_store(&s_problems.check_total, args->total);
    problems_woken();
}

CliServersSink cli_servers_problems_sink(const char *root)
{
    CliServersSink sink;
    memor.set(&(MemoriaCfg){.dst = &sink, .val = 0, .bytes = sizeof(sink)});
    sink.diagnostics = problems_reported;
    sink.check_moved = problems_check_moved;
    VerbaLinea tree = {sink.tree, sizeof(sink.tree), 0ULL};
    mmgr_line_put(&tree, (root != NULL) ? root : "");
    (void)mmgr_line_finish(&tree);
    return sink;
}

void cli_servers_problems_begin(void (*woken)(void))
{
    memor.set(&(MemoriaCfg){.dst = &s_problems.turnstiles, .val = 0, .bytes = sizeof(s_problems.turnstiles)});
    EmbedTicketRingSlot *const slots =
        (EmbedTicketRingSlot *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_PROBLEMS);
    memor.set(&(MemoriaCfg){.dst = slots, .val = 0, .bytes = CLI_PROBLEMS_RING_BYTES});
    embed_ticket_ring_begin(&s_problems.ring, slots, CLI_PROBLEMS_RING_SHIFT);
    embed_atomic_u32_store(&s_problems.ticket, 0u);
    s_problems.room = (CliWake)CLI_WAKE_INIT;
    embed_atomic_u64_store(&s_problems.check_done, 0ULL);
    embed_atomic_u64_store(&s_problems.check_total, 0ULL);
    // Explicit cast stores the function pointer as the word that holds it
    embed_atomic_uptr_store(&s_problems.woken, (embed_atomic_uptr_value)(embed_word)woken);
    s_problems.taken = 0u;
    s_problems.table = NULL;
    s_problems.count = 0u;
    s_problems.errors = 0u;
    s_problems.done = 0ULL;
    s_problems.total = 0ULL;
    embed_atomic_u32_store(&s_problems.open, 1u);
}

// On the view's thread: every report pushed popped in the order of its ticket and the table built over it, the
// table's cell and the report each replaces released, each slot released and a reporting thread waiting for room
// woken: whether a report came
static int problems_taken(void)
{
    int moved = 0;
    // Explicit cast carries the problems' address as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)&s_problems;
    embed_u64 stamp = 0u;
    while (cli_ticket_pushed(&s_problems.ring, s_problems.taken, &stamp))
    {
        void *job = NULL;
        embed_u64 seen = 0u;
        if (!embed_ticket_ring_popped(&s_problems.turnstiles, address, &s_problems.ring, stamp, &job, &seen))
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
            problems_table_built(report.path, &report.file, report.has_file, &found, &count, &errors);
        if (built != NULL)
        {
            if (found < s_problems.count)
            {
                cli_servers_report_release(problems_file_at(found).report);
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
            cli_servers_report_release(report.file.report);
        }
        (void)g_cli_guard->movable_buf_release(cell, g_cli_servers_owner);
        (void)embed_ticket_ring_released(&s_problems.turnstiles, &s_problems.ring, stamp, &seen);
        s_problems.taken += 1u;
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_problems.room);
    }
    return moved;
}

int cli_servers_problems_step(void)
{
    int moved = problems_taken();
    const unsigned long long total = embed_atomic_u64_load(&s_problems.check_total);
    const unsigned long long done = embed_atomic_u64_load(&s_problems.check_done);
    moved = moved || (done != s_problems.done) || (total != s_problems.total);
    s_problems.done = done;
    s_problems.total = total;
    return moved;
}

void cli_servers_problems_end(void)
{
    cli_servers_stop_check();
    embed_atomic_u32_store(&s_problems.open, 0u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_problems.room);
    (void)problems_taken();
    for (unsigned int at = 0u; at < s_problems.count; at += 1u)
    {
        cli_servers_report_release(problems_file_at(at).report);
    }
    if (s_problems.table != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_problems.table, g_cli_servers_owner);
    }
    s_problems.table = NULL;
    s_problems.count = 0u;
    s_problems.errors = 0u;
    embed_atomic_uptr_store(&s_problems.woken, 0u);
}

CliProblemsCounts cli_servers_problems_counts(void)
{
    CliProblemsCounts counts = {0u, 0u, s_problems.errors, s_problems.done, s_problems.total, 0u};
    for (unsigned int at = 0u; at < s_problems.count; at += 1u)
    {
        const ServersProblemFile file = problems_file_at(at);
        const CliServerReportHead head = problems_head(file.report);
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

int cli_servers_problems_row(unsigned int at, CliProblemRow *row)
{
    // a row's texts are read out of the report's cell into the view's thread's own buffers, which are until its next
    // row
    static char s_path[CLI_PATH_MAX_LEN];
    static char s_message[CLI_PROBLEMS_MESSAGE];
    unsigned int first = 0u;
    for (unsigned int place = 0u; place < s_problems.count; place += 1u)
    {
        const ServersProblemFile file = problems_file_at(place);
        const CliServerReportHead head = problems_head(file.report);
        const unsigned int under = file.closed ? 0u : head.count;
        if (at == first)
        {
            (void)problems_path(file.report, s_path);
            *row = (CliProblemRow){1,
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
        if (at < (first + 1u + under))
        {
            const CliServerStoredDiagnostic item = problems_item(file.report, at - first - 1u);
            (void)problems_path(file.report, s_path);
            const unsigned long long length = problems_message(file.report, &item, s_message, sizeof(s_message));
            *row = (CliProblemRow){0,      s_path,         "",           0ULL, 0u, 0, item.severity, s_message,
                                   length, item.from_line, item.from_col};
            return 1;
        }
        first += 1u + under;
    }
    return 0;
}

int cli_servers_problems_span(const char *path, unsigned int at, CliProblemSpan *span)
{
    for (unsigned int place = 0u; (path != NULL) && (place < s_problems.count); place += 1u)
    {
        const CarcerMovableEntry *const report = problems_file_at(place).report;
        if (problems_report_is(report, path))
        {
            if (at >= problems_head(report).count)
            {
                return 0;
            }
            const CliServerStoredDiagnostic item = problems_item(report, at);
            *span = (CliProblemSpan){item.severity, item.from_line, item.from_col, item.to_line, item.to_col};
            return 1;
        }
    }
    return 0;
}

int cli_servers_problems_press(unsigned int at, CliProblemRow *row)
{
    unsigned int first = 0u;
    for (unsigned int place = 0u; place < s_problems.count; place += 1u)
    {
        ServersProblemFile file = problems_file_at(place);
        if (at == first)
        {
            file.closed = !file.closed;
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = s_problems.table, .offset = CLI_PROBLEMS_FILE_AT(place), .src = &file, .bytes = sizeof(file)});
            return 0;
        }
        first += 1u + (file.closed ? 0u : problems_head(file.report).count);
    }
    return cli_servers_problems_row(at, row) && !row->is_file;
}

void cli_servers_problems_check(const char *root)
{
    if (cli_servers_problems_checking())
    {
        cli_servers_stop_check();
        return;
    }
    const CliFiles *const files =
        (root != NULL) ? EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root) : NULL;
    if (files == NULL)
    {
        return;
    }
    const char **const paths = g_cli_guard->temporary_buf_alloc(
        ((unsigned long long)files->count + 1ULL) * sizeof(*paths), g_cli_servers_owner);
    if (paths != NULL)
    {
        for (unsigned int at = 0u; at < files->count; at += 1u)
        {
            paths[at] = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        }
        const CliServersSink sink = cli_servers_problems_sink(root);
        (void)EMBED_CALL(cli_servers_root_sink_count.check_tree, CliServersRootSinkCountCfg, .root = root,
                         .files = paths, .count = files->count, .sink = &sink);
        g_cli_guard->persistent_buf_release(paths, g_cli_servers_owner);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}
