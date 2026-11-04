// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_problems.c: the problems: each file's diagnostics, worst first, a file replaced whole as its report comes
// again. A report comes on the thread that read it, decoded once where the servers store it (servers_emit.c), and the
// files with problems are a table that thread builds over the last inside a pass of its own and publishes whole with a
// compare and swap, the table it replaced pushed on the retired tables with another; no writer waits on a reader. A
// reading thread passes through a turnstile from one step to its next and reads the table it found in place; at each
// step it ends its pass, waits out every other pass begun before the tables it takes off the retired were replaced, and
// releases them (atomic_threads.h). A press on a row closes a file's rows or points to the file and the place a
// diagnostic stands at; the tree's check is started and stopped here
#include "servers.h"

#include "../command_line/atomic_threads.h"
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../text/text.h"

#include <stdlib.h>

// the severity that counts as an error
#define CLI_PROBLEMS_ERROR 1u

// One file's problems: its report, the place its name starts in its path, how many of its diagnostics are errors, and
// whether its rows are closed, which a reading thread's press sets
typedef struct
{
    CliServerReport *report;
    unsigned long long name;
    unsigned int errors;
    _Atomic int closed;
} ServersProblemFile;

// The files with problems, in the order their reports last came, and their errors. Once replaced: the next retired
// table, and the file the replacing report took out, released with it
typedef struct ServersProblemsTable
{
    struct ServersProblemsTable *retired;
    ServersProblemFile *removed;
    unsigned int count;
    unsigned int errors;
    ServersProblemFile *file[];
} ServersProblemsTable;

// The problems: the table published last, NULL for none, and how many tables have been published; the tables replaced
// and not yet released; the check's progress as it was last reported; the turnstiles the readers pass through; and what
// wakes the window's thread as a report comes
typedef struct
{
    _Alignas(64) ServersProblemsTable *_Atomic table;
    _Atomic unsigned long long published;
    _Alignas(64) ServersProblemsTable *_Atomic retired;
    _Alignas(64) _Atomic unsigned long long check_done;
    _Atomic unsigned long long check_total;
    void (*_Atomic woken)(void);
    CliTurnstiles turnstiles;
} ServersProblems;

static ServersProblems s_problems;

// A thread's pass through the problems: whether it passes and the turnstile it passes through; and what its last step
// read: the table, the count of tables published, and the check's progress
typedef struct
{
    int passing;
    unsigned int at;
    const ServersProblemsTable *table;
    unsigned long long published;
    unsigned long long done;
    unsigned long long total;
} ServersProblemsPass;

static _Thread_local ServersProblemsPass t_pass;

// two diagnostics in the order shown: the worse first, then the earlier
static int problems_order(const void *one, const void *other)
{
    const CliServerDiagnostic *const first = (const CliServerDiagnostic *)one;
    const CliServerDiagnostic *const second = (const CliServerDiagnostic *)other;
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

// a file's report and its head released
static void problems_file_release(ServersProblemFile *file)
{
    if (file != NULL)
    {
        cli_servers_report_release(file->report);
        g_cli_cells->persistent_buf_release(file, &g_cli_servers_cells);
    }
}

// The retired tables taken off whole, every pass through the turnstiles begun before they were replaced waited out,
// and each released with the file its replacement took out. A thread calls this between its passes, never inside one
static void problems_reclaimed(void)
{
    ServersProblemsTable *table = atomic_exchange_explicit(&s_problems.retired, NULL, memory_order_acquire);
    if (table == NULL)
    {
        return;
    }
    cli_turnstiles_drain(&(CliAtomicThreadsCfg){.turnstiles = &s_problems.turnstiles});
    while (table != NULL)
    {
        ServersProblemsTable *const next = table->retired;
        problems_file_release(table->removed);
        g_cli_cells->persistent_buf_release(table, &g_cli_servers_cells);
        table = next;
    }
}

// the head of a file whose report has diagnostics: each message cut to its first line where it lies, the diagnostics
// in the order shown, the errors counted and the name found from the path's end; NULL where its cell could not be taken
static ServersProblemFile *problems_file_built(CliServerReport *report)
{
    ServersProblemFile *const file = g_cli_cells->persistent_buf_alloc(sizeof(*file), &g_cli_servers_cells);
    if (file == NULL)
    {
        return NULL;
    }
    file->report = report;
    file->errors = 0u;
    atomic_init(&file->closed, 0);
    for (unsigned int at = 0u; at < report->count; at += 1u)
    {
        CliServerDiagnostic *const item = &report->item[at];
        item->message_length = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                          .text = item->message, .at = 0ULL, .size = item->message_length, .set = "\n");
        file->errors += (item->severity == CLI_PROBLEMS_ERROR) ? 1u : 0u;
    }
    qsort(report->item, report->count, sizeof(*report->item), problems_order);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = report->path, .cap = CLI_PATH_MAX_LEN});
    file->name = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = report->path,
                            .at = length, .set = "/\\");
    return file;
}

// The table `table` with the file of `path` taken out and `file` put at the end where it is not NULL, the place the
// file of `path` stood in into `found`, the count where none did: the new table, or NULL where nothing changes or its
// cell could not be taken
static ServersProblemsTable *problems_table_built(const ServersProblemsTable *table, const char *path,
                                                  ServersProblemFile *file, unsigned int *found)
{
    const unsigned int count = (table != NULL) ? table->count : 0u;
    *found = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = table->file[at]->report->path, .other = path, .cap = CLI_PATH_MAX_LEN}))
        {
            *found = at;
            break;
        }
    }
    if ((*found == count) && (file == NULL))
    {
        return NULL;
    }
    ServersProblemsTable *const built = g_cli_cells->persistent_buf_alloc(
        sizeof(*built) + (((unsigned long long)count + 1ULL) * sizeof(built->file[0])), &g_cli_servers_cells);
    if (built == NULL)
    {
        return NULL;
    }
    built->retired = NULL;
    built->removed = NULL;
    built->count = 0u;
    built->errors = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (at != *found)
        {
            built->file[built->count] = table->file[at];
            built->errors += table->file[at]->errors;
            built->count += 1u;
        }
    }
    if (file != NULL)
    {
        built->file[built->count] = file;
        built->errors += file->errors;
        built->count += 1u;
    }
    return built;
}

// from any thread: a file's report taken and published in the table in place of the file's last, or the file taken out
// where it has no diagnostics; the table replaced pushed on the retired
static void problems_reported(const CliServerDiagnosticsCfg *args)
{
    ServersProblemFile *file = NULL;
    if ((args->report != NULL) && (args->report->count != 0u) && (args->taken != NULL))
    {
        file = problems_file_built(args->report);
        *args->taken = (file != NULL);
        if (file == NULL)
        {
            return;
        }
    }
    // the table read and built over inside a pass, so no reader releases it while it is read, and none of its
    // addresses comes back as another table's before the compare and swap
    const unsigned int pass = cli_turnstile_enter(&(CliAtomicThreadsCfg){.turnstiles = &s_problems.turnstiles});
    ServersProblemsTable *old = atomic_load_explicit(&s_problems.table, memory_order_acquire);
    ServersProblemsTable *built = NULL;
    unsigned int found = 0u;
    for (;;)
    {
        built = problems_table_built(old, args->path, file, &found);
        if (built == NULL)
        {
            cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_problems.turnstiles, .at = pass});
            problems_file_release(file);
            return;
        }
        // the compare and swap is the publish: it fails where another report published first, and the table is built
        // again over that one
        if (atomic_compare_exchange_strong_explicit(&s_problems.table, &old, built, memory_order_acq_rel,
                                                    memory_order_acquire))
        {
            break;
        }
        g_cli_cells->persistent_buf_release(built, &g_cli_servers_cells);
    }
    (void)atomic_fetch_add_explicit(&s_problems.published, 1ULL, memory_order_release);
    if (old != NULL)
    {
        // the table replaced is this thread's alone now: no other swap can replace it again
        old->removed = (found < old->count) ? old->file[found] : NULL;
        ServersProblemsTable *head = atomic_load_explicit(&s_problems.retired, memory_order_relaxed);
        // the compare and swap is the push: it fails where another table was pushed first, and the link is set again
        do
        {
            old->retired = head;
        } while (!atomic_compare_exchange_weak_explicit(&s_problems.retired, &head, old, memory_order_release,
                                                        memory_order_relaxed));
    }
    cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_problems.turnstiles, .at = pass});
    void (*const woken)(void) = atomic_load_explicit(&s_problems.woken, memory_order_acquire);
    if (woken != NULL)
    {
        woken();
    }
}

// from any thread: how far the tree's check has gone
static void problems_check_moved(const CliServerCheckCfg *args)
{
    atomic_store_explicit(&s_problems.check_done, args->done, memory_order_relaxed);
    atomic_store_explicit(&s_problems.check_total, args->total, memory_order_release);
    void (*const woken)(void) = atomic_load_explicit(&s_problems.woken, memory_order_acquire);
    if (woken != NULL)
    {
        woken();
    }
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
    atomic_store_explicit(&s_problems.check_done, 0ULL, memory_order_relaxed);
    atomic_store_explicit(&s_problems.check_total, 0ULL, memory_order_relaxed);
    atomic_store_explicit(&s_problems.woken, woken, memory_order_release);
    t_pass = (ServersProblemsPass){0, 0u, NULL, 0ULL, 0ULL, 0ULL};
}

// this thread's pass ended, where it passes
static void problems_pass_ended(void)
{
    if (t_pass.passing)
    {
        cli_turnstile_leave(&(CliAtomicThreadsCfg){.turnstiles = &s_problems.turnstiles, .at = t_pass.at});
        t_pass.passing = 0;
        t_pass.table = NULL;
    }
}

int cli_servers_problems_step(void)
{
    problems_pass_ended();
    problems_reclaimed();
    t_pass.at = cli_turnstile_enter(&(CliAtomicThreadsCfg){.turnstiles = &s_problems.turnstiles});
    t_pass.passing = 1;
    // the count read before the table: a publish between the two is seen again at the next step, never missed
    const unsigned long long published = atomic_load_explicit(&s_problems.published, memory_order_acquire);
    t_pass.table = atomic_load_explicit(&s_problems.table, memory_order_acquire);
    const unsigned long long total = atomic_load_explicit(&s_problems.check_total, memory_order_acquire);
    const unsigned long long done = atomic_load_explicit(&s_problems.check_done, memory_order_relaxed);
    const int moved = (published != t_pass.published) || (done != t_pass.done) || (total != t_pass.total);
    t_pass.published = published;
    t_pass.done = done;
    t_pass.total = total;
    return moved;
}

void cli_servers_problems_end(void)
{
    cli_servers_stop_check();
    problems_pass_ended();
    ServersProblemsTable *const last = atomic_exchange_explicit(&s_problems.table, NULL, memory_order_acq_rel);
    problems_reclaimed();
    cli_turnstiles_drain(&(CliAtomicThreadsCfg){.turnstiles = &s_problems.turnstiles});
    for (unsigned int at = 0u; (last != NULL) && (at < last->count); at += 1u)
    {
        problems_file_release(last->file[at]);
    }
    g_cli_cells->persistent_buf_release(last, &g_cli_servers_cells);
    atomic_store_explicit(&s_problems.woken, NULL, memory_order_release);
    t_pass = (ServersProblemsPass){0, 0u, NULL, 0ULL, 0ULL, 0ULL};
}

CliProblemsCounts cli_servers_problems_counts(void)
{
    const ServersProblemsTable *const table = t_pass.table;
    CliProblemsCounts counts = {0u, 0u, (table != NULL) ? table->errors : 0u, t_pass.done, t_pass.total, 0u};
    for (unsigned int at = 0u; (table != NULL) && (at < table->count); at += 1u)
    {
        const ServersProblemFile *const file = table->file[at];
        counts.files += 1u;
        counts.items += file->report->count;
        counts.rows += 1u + (atomic_load_explicit(&file->closed, memory_order_relaxed) ? 0u : file->report->count);
    }
    return counts;
}

int cli_servers_problems_checking(void)
{
    return (t_pass.total != 0ULL) && (t_pass.done < t_pass.total);
}

int cli_servers_problems_row(unsigned int at, CliProblemRow *row)
{
    const ServersProblemsTable *const table = t_pass.table;
    unsigned int first = 0u;
    for (unsigned int place = 0u; (table != NULL) && (place < table->count); place += 1u)
    {
        const ServersProblemFile *const file = table->file[place];
        const CliServerReport *const report = file->report;
        const int closed = atomic_load_explicit(&file->closed, memory_order_relaxed);
        const unsigned int under = closed ? 0u : report->count;
        if (at == first)
        {
            *row = (CliProblemRow){1,
                                   report->path,
                                   report->path + file->name,
                                   (file->name != 0ULL) ? (file->name - 1ULL) : 0ULL,
                                   report->count,
                                   closed,
                                   0u,
                                   "",
                                   0ULL,
                                   0ULL,
                                   0ULL};
            return 1;
        }
        if (at < (first + 1u + under))
        {
            const CliServerDiagnostic *const item = &report->item[at - first - 1u];
            *row = (CliProblemRow){0,
                                   report->path,
                                   "",
                                   0ULL,
                                   0u,
                                   0,
                                   item->severity,
                                   item->message,
                                   item->message_length,
                                   item->from_line,
                                   item->from_col};
            return 1;
        }
        first += 1u + under;
    }
    return 0;
}

int cli_servers_problems_span(const char *path, unsigned int at, CliProblemSpan *span)
{
    const ServersProblemsTable *const table = t_pass.table;
    for (unsigned int place = 0u; (path != NULL) && (table != NULL) && (place < table->count); place += 1u)
    {
        const CliServerReport *const report = table->file[place]->report;
        if (cellul.eq(&(CatenaFinitaCfg){.src = report->path, .other = path, .cap = CLI_PATH_MAX_LEN}))
        {
            if (at >= report->count)
            {
                return 0;
            }
            const CliServerDiagnostic *const item = &report->item[at];
            *span = (CliProblemSpan){item->severity, item->from_line, item->from_col, item->to_line, item->to_col};
            return 1;
        }
    }
    return 0;
}

int cli_servers_problems_press(unsigned int at, CliProblemRow *row)
{
    const ServersProblemsTable *const table = t_pass.table;
    unsigned int first = 0u;
    for (unsigned int place = 0u; (table != NULL) && (place < table->count); place += 1u)
    {
        ServersProblemFile *const file = table->file[place];
        const int closed = atomic_load_explicit(&file->closed, memory_order_relaxed);
        if (at == first)
        {
            atomic_store_explicit(&file->closed, !closed, memory_order_relaxed);
            return 0;
        }
        first += 1u + (closed ? 0u : file->report->count);
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
    const char **const paths = g_cli_cells->persistent_buf_alloc(
        ((unsigned long long)files->count + 1ULL) * sizeof(*paths), &g_cli_servers_cells);
    if (paths != NULL)
    {
        for (unsigned int at = 0u; at < files->count; at += 1u)
        {
            paths[at] = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        }
        const CliServersSink sink = cli_servers_problems_sink(root);
        (void)EMBED_CALL(cli_servers_root_sink_count.check_tree, CliServersRootSinkCountCfg, .root = root,
                         .files = paths, .count = files->count, .sink = &sink);
        g_cli_cells->persistent_buf_release(paths, &g_cli_servers_cells);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}
