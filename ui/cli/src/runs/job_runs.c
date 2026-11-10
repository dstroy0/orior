// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs.c: the Run pane's store (job_runs.h): the catalog listed by group, the job chosen, the runs the pane starts
// and stops, and the sink their threads report to. A report is put in the inbox, one cell of reports each a record and
// its text, under the lock, and the view's thread woken; the step takes the inbox whole under the lock and reads it on
// the view's thread into the runs: a run's start a record, a line its plain text and its spans read once through the
// xterm, an end its state, the oldest lines past CLI_LINES_MAX and the oldest ended runs past CLI_JOB_RUNS_RETAINED
// released with their cells
#include "job_runs_internal.h"

#include "../command_line/command_line_internal.h"
#include "embed_atomic.h"

// the bytes a report's record and its text are aligned to in the inbox, the spans one line carries at most, the
// sentence a start with no job chosen is refused with, and the most bytes of a run's state
#define JOB_RUNS_ALIGN 8ULL
#define JOB_RUNS_LINE_SPANS 64u
#define JOB_RUNS_NONE_CHOSEN "No job is chosen."

CliJobRunsStore g_cli_job_runs_store = {.chosen = CLI_CATALOG_NONE, .focus = CLI_JOB_RUNS_FILTER};

// A report in the inbox: its type, its run, its line's stream, its text's bytes, which follow the record, and for an
// end its code where it has one and whether the run was stopped
typedef struct
{
    unsigned int type;
    unsigned int stream;
    unsigned long long run;
    unsigned long long length;
    long long code;
    int has_code;
    int stopped;
} JobRunsReport;

// the inbox, its cell, the bytes in use and those it can take, standing under the lock; the lock; and the wake of the
// view's thread
static unsigned char *s_inbox;
static unsigned long long s_inbox_used;
static unsigned long long s_inbox_capacity;
static embed_atomic_u32 s_lock;
static void (*s_woken)(void);

typedef struct
{
    const JobRunsReport *report;
    const char *text;
    unsigned int at;
    unsigned long long run;
} JobRunsCtx;

// the lock taken, the thread yielding while another has it
EMBED_INLINE void lock_taken(const JobRunsCtx *args)
{
    (void)args;
    while (EMBED_ATOMIC_EXCHANGE_32(&s_lock, 1u) != 0u)
    {
        cli_thread_yield();
    }
}

// the lock let go
EMBED_INLINE void lock_left(const JobRunsCtx *args)
{
    (void)args;
    EMBED_ATOMIC_STORE(&s_lock, 0u);
}

// the place of the run numbered `run` among the store's runs, `runs` where none is
EMBED_INLINE unsigned int run_place(const JobRunsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    for (unsigned int at = store->runs; at > 0u; at -= 1u)
    {
        if (store->run[at - 1u].run == args->run)
        {
            return at - 1u;
        }
    }
    return store->runs;
}

// the run `at`'s state written anew from what it reports: `#n running`, `#n stopped`, `#n exit N` or `#n ended`, the
// place of the word after the number standing beside it
EMBED_INLINE void state_written(const JobRunsCtx *args)
{
    CliJobRunsRun *const run = &g_cli_job_runs_store.run[args->at];
    VerbaLinea line = {run->state, sizeof(run->state), 0ULL};
    mmgr_line_ch(&line, '#');
    mmgr_line_decimal(&line, run->run, 1u);
    mmgr_line_ch(&line, ' ');
    // the state's bytes are fewer than its array's, far inside an unsigned int
    run->word = (unsigned int)line.at;
    if (run->live)
    {
        mmgr_line_put(&line, "running");
    }
    else if (run->stopped)
    {
        mmgr_line_put(&line, "stopped");
    }
    else if (run->has_code)
    {
        mmgr_line_put(&line, "exit ");
        mmgr_line_put(&line, (run->code < 0LL) ? "-" : "");
        // the code's magnitude is a long long's, read as unsigned
        mmgr_line_decimal(&line, (unsigned long long)((run->code < 0LL) ? -run->code : run->code), 1u);
    }
    else
    {
        mmgr_line_put(&line, "ended");
    }
    // the state's bytes are fewer than its array's
    run->state_length = mmgr_line_ok(&line) ? (unsigned int)line.at : 0u;
}

// the run `at` released: its cells let go, the runs after it moved up, the runs in a cell sized to those left, and the
// run shown let go where it was this one
EMBED_INLINE void run_released(const JobRunsCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    CliJobRunsRun *const run = &store->run[args->at];
    store->shown = (store->shown == run->run) ? 0ULL : store->shown;
    g_cli_guard->persistent_buf_release(run->job_id, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(run->text, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(run->line, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(run->span, g_cli_job_runs_owner);
    for (unsigned int from = args->at + 1u; from < store->runs; from += 1u)
    {
        store->run[from - 1u] = store->run[from];
    }
    const unsigned int left = store->runs - 1u;
    CliJobRunsRun *const fewer =
        (left != 0u) ? g_cli_guard->persistent_buf_alloc(left * sizeof(CliJobRunsRun), g_cli_job_runs_owner) : NULL;
    if (fewer != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = fewer, .src = store->run, .bytes = left * sizeof(CliJobRunsRun)});
    }
    if ((left == 0u) || (fewer != NULL))
    {
        g_cli_guard->persistent_buf_release(store->run, g_cli_job_runs_owner);
        store->run = fewer;
    }
    store->runs = left;
}

// a run's start read: a record put after the runs in a cell sized to them, its job's id with its end in a cell of its
// own, its job found in the catalog listed, and the run shown where its job is the one chosen
EMBED_INLINE void start_read(const JobRunsCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    CliJobRunsRun *const grown =
        g_cli_guard->persistent_buf_alloc((store->runs + 1ULL) * sizeof(CliJobRunsRun), g_cli_job_runs_owner);
    char *const job_id = g_cli_guard->persistent_buf_alloc(args->report->length + 1ULL, g_cli_job_runs_owner);
    if ((grown == NULL) || (job_id == NULL))
    {
        g_cli_guard->persistent_buf_release(grown, g_cli_job_runs_owner);
        g_cli_guard->persistent_buf_release(job_id, g_cli_job_runs_owner);
        return;
    }
    if (store->runs != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = store->run, .bytes = store->runs * sizeof(CliJobRunsRun)});
    }
    g_cli_guard->persistent_buf_release(store->run, g_cli_job_runs_owner);
    store->run = grown;
    memor.cpy(&(MemoriaCfg){.dst = job_id, .src = args->text, .bytes = args->report->length});
    job_id[args->report->length] = '\0';
    CliJobRunsRun *const run = &grown[store->runs];
    *run = (CliJobRunsRun){0};
    run->run = args->report->run;
    run->job_id = job_id;
    run->live = 1;
    run->job = (store->catalog != NULL)
                   ? EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = store->catalog, .id = job_id)
                   : CLI_CATALOG_NONE;
    store->runs += 1u;
    EMBED_CALL(state_written, JobRunsCtx, .at = store->runs - 1u);
    store->shown = ((run->job != CLI_CATALOG_NONE) && (run->job == store->chosen)) ? run->run : store->shown;
}

// a line of the run `at` read: its plain text put after the run's in its cell and its spans after the run's, each cell
// taken anew half again as large where the line outgrows it, the line's record after the run's lines the same
EMBED_INLINE void line_read(const JobRunsCtx *args)
{
    CliJobRunsRun *const run = &g_cli_job_runs_store.run[args->at];
    const unsigned long long length = args->report->length;
    if (run->text_used + length > run->text_capacity)
    {
        const unsigned long long needed = run->text_used + length;
        const unsigned long long capacity = needed + (needed / 2ULL) + 1ULL;
        char *const grown = g_cli_guard->persistent_buf_alloc(capacity, g_cli_job_runs_owner);
        if (grown == NULL)
        {
            return;
        }
        if (run->text_used != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = run->text, .bytes = run->text_used});
        }
        g_cli_guard->persistent_buf_release(run->text, g_cli_job_runs_owner);
        run->text = grown;
        run->text_capacity = capacity;
    }
    if (run->lines == run->lines_capacity)
    {
        const unsigned int capacity = run->lines + (run->lines / 2u) + 1u;
        CliJobRunsLine *const grown =
            g_cli_guard->persistent_buf_alloc(capacity * sizeof(CliJobRunsLine), g_cli_job_runs_owner);
        if (grown == NULL)
        {
            return;
        }
        if (run->lines != 0u)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = run->line, .bytes = run->lines * sizeof(CliJobRunsLine)});
        }
        g_cli_guard->persistent_buf_release(run->line, g_cli_job_runs_owner);
        run->line = grown;
        run->lines_capacity = capacity;
    }
    CliTerminalXtermSpan spans[JOB_RUNS_LINE_SPANS];
    unsigned long long plain = 0ULL;
    const unsigned int count =
        EMBED_CALL(cli_terminal_xterm_spans_table.spans, CliTerminalXtermSpansCfg, .text = args->text, .size = length,
                   .plain_out = run->text + run->text_used, .plain_capacity = length, .plain_size_out = &plain,
                   .spans_out = spans, .spans_capacity = JOB_RUNS_LINE_SPANS);
    if (run->spans + count > run->spans_capacity)
    {
        const unsigned int needed = run->spans + count;
        const unsigned int capacity = needed + (needed / 2u);
        CliTerminalXtermSpan *const grown =
            g_cli_guard->persistent_buf_alloc(capacity * sizeof(CliTerminalXtermSpan), g_cli_job_runs_owner);
        if (grown == NULL)
        {
            return;
        }
        if (run->spans != 0u)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = grown, .src = run->span, .bytes = run->spans * sizeof(CliTerminalXtermSpan)});
        }
        g_cli_guard->persistent_buf_release(run->span, g_cli_job_runs_owner);
        run->span = grown;
        run->spans_capacity = capacity;
    }
    if (count != 0u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = run->span + run->spans, .src = spans, .bytes = count * sizeof(CliTerminalXtermSpan)});
    }
    // a run's text is far below CLI_LINES_TEXT's reach of an unsigned int, its lines bounded by CLI_LINES_MAX
    run->line[run->lines] =
        (CliJobRunsLine){(unsigned int)run->text_used, (unsigned int)plain, args->report->stream, run->spans, count};
    run->text_used += plain;
    run->spans += count;
    run->lines += 1u;
}

// the oldest lines of the run `at` past CLI_LINES_MAX released: the lines left, their text and their spans each moved
// into a cell of its own sized to them, their places counted from the first left
EMBED_INLINE void lines_bounded(const JobRunsCtx *args)
{
    CliJobRunsRun *const run = &g_cli_job_runs_store.run[args->at];
    if (run->lines <= CLI_LINES_MAX)
    {
        return;
    }
    const unsigned int dropped = run->lines - CLI_LINES_MAX;
    const unsigned int text_from = run->line[dropped].start;
    const unsigned int span_from = run->line[dropped].span_first;
    const unsigned long long text_left = run->text_used - text_from;
    const unsigned int spans_left = run->spans - span_from;
    char *const text = g_cli_guard->persistent_buf_alloc(text_left + 1ULL, g_cli_job_runs_owner);
    CliJobRunsLine *const line =
        g_cli_guard->persistent_buf_alloc(CLI_LINES_MAX * sizeof(CliJobRunsLine), g_cli_job_runs_owner);
    CliTerminalXtermSpan *const span =
        (spans_left != 0u)
            ? g_cli_guard->persistent_buf_alloc(spans_left * sizeof(CliTerminalXtermSpan), g_cli_job_runs_owner)
            : NULL;
    if ((text == NULL) || (line == NULL) || ((spans_left != 0u) && (span == NULL)))
    {
        g_cli_guard->persistent_buf_release(text, g_cli_job_runs_owner);
        g_cli_guard->persistent_buf_release(line, g_cli_job_runs_owner);
        g_cli_guard->persistent_buf_release(span, g_cli_job_runs_owner);
        return;
    }
    if (text_left != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = text, .src = run->text + text_from, .bytes = text_left});
    }
    if (spans_left != 0u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = span, .src = run->span + span_from, .bytes = spans_left * sizeof(CliTerminalXtermSpan)});
    }
    for (unsigned int at = 0u; at < CLI_LINES_MAX; at += 1u)
    {
        line[at] = run->line[dropped + at];
        line[at].start -= text_from;
        line[at].span_first -= span_from;
    }
    g_cli_guard->persistent_buf_release(run->text, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(run->line, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(run->span, g_cli_job_runs_owner);
    run->text = text;
    run->text_used = text_left;
    run->text_capacity = text_left + 1ULL;
    run->line = line;
    run->lines = CLI_LINES_MAX;
    run->lines_capacity = CLI_LINES_MAX;
    run->span = span;
    run->spans = spans_left;
    run->spans_capacity = spans_left;
}

// the ended runs of the job of the run `at` past CLI_JOB_RUNS_RETAINED released, the oldest first; a run that goes
// is never released
EMBED_INLINE void runs_retained(const JobRunsCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    const char *const job_id = store->run[args->at].job_id;
    unsigned int ended = 0u;
    for (unsigned int at = 0u; at < store->runs; at += 1u)
    {
        ended += (!store->run[at].live &&
                  cellul.eq(&(CatenaFinitaCfg){.src = store->run[at].job_id, .other = job_id, .cap = CLI_PATH_MAX_LEN}))
                     ? 1u
                     : 0u;
    }
    // the run that ended stands, its id's cell with it, while the older ones before it go
    for (unsigned int at = 0u; (ended > CLI_JOB_RUNS_RETAINED) && (at < store->runs);)
    {
        const CliJobRunsRun *const one = &store->run[at];
        if (!one->live && (one->job_id != job_id) &&
            cellul.eq(&(CatenaFinitaCfg){.src = one->job_id, .other = job_id, .cap = CLI_PATH_MAX_LEN}))
        {
            EMBED_CALL(run_released, JobRunsCtx, .at = at);
            ended -= 1u;
            continue;
        }
        at += 1u;
    }
}

// a run's end read: its state, its code where it has one and whether it was stopped, the job's ended runs retained
EMBED_INLINE void end_read(const JobRunsCtx *args)
{
    CliJobRunsRun *const run = &g_cli_job_runs_store.run[args->at];
    run->live = 0;
    run->stopped = args->report->stopped;
    run->has_code = args->report->has_code;
    run->code = args->report->code;
    EMBED_CALL(state_written, JobRunsCtx, .at = args->at);
    EMBED_CALL(runs_retained, JobRunsCtx, .at = args->at);
}

void cli_job_runs_begin(const CliJobRunsWokenCfg *args)
{
    s_woken = args->woken;
}

// what a run the pane starts reports, on the run's thread, stored as the store's report: its type and its stream read
// off the runner's words
static void job_runs_emit(const CliEachEventCfg *args)
{
    const CliRunEvent *const event = args->event;
    const int stderr_stream =
        (event->stream != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = event->stream, .other = "stderr", .cap = 8ULL});
    const int command_stream =
        (event->stream != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = event->stream, .other = "command", .cap = 8ULL});
    EMBED_CALL(cli_job_runs_report.store, CliJobRunsReportCfg,
               .type = (event->type == CLI_RUN_EVENT_START)  ? CLI_JOB_RUNS_STARTED
                       : (event->type == CLI_RUN_EVENT_LINE) ? CLI_JOB_RUNS_LINE
                                                             : CLI_JOB_RUNS_ENDED,
               .run = event->run, .job = event->job,
               .stream =
                   stderr_stream ? CLI_JOB_RUN_STDERR : (command_stream ? CLI_JOB_RUN_COMMAND : CLI_JOB_RUN_STDOUT),
               .text = event->text, .has_code = event->has_code, .code = event->code, .stopped = event->stopped);
}

const CliRunSink cli_job_runs_sink = {job_runs_emit, NULL};

void cli_job_runs_reported(const CliJobRunsReportCfg *args)
{
    const char *const text = (args->type == CLI_JOB_RUNS_STARTED) ? args->job
                             : (args->type == CLI_JOB_RUNS_LINE)  ? args->text
                                                                  : NULL;
    const unsigned long long length =
        (text != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_LINES_TEXT}) : 0ULL;
    const unsigned long long record = (sizeof(JobRunsReport) + JOB_RUNS_ALIGN - 1ULL) & ~(JOB_RUNS_ALIGN - 1ULL);
    const unsigned long long bytes = record + ((length + JOB_RUNS_ALIGN - 1ULL) & ~(JOB_RUNS_ALIGN - 1ULL));
    const JobRunsReport report = {args->type, args->stream,   args->run,    length,
                                  args->code, args->has_code, args->stopped};
    EMBED_CALL(lock_taken, JobRunsCtx, .at = 0u);
    if (s_inbox_used + bytes > s_inbox_capacity)
    {
        // the inbox taken anew half again as large as it has to be, the reports in it moved over
        const unsigned long long needed = s_inbox_used + bytes;
        const unsigned long long capacity = needed + (needed / 2ULL);
        unsigned char *const grown = g_cli_guard->persistent_buf_alloc(capacity, g_cli_job_runs_owner);
        if (grown == NULL)
        {
            EMBED_CALL(lock_left, JobRunsCtx, .at = 0u);
            return;
        }
        if (s_inbox_used != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_inbox, .bytes = s_inbox_used});
        }
        g_cli_guard->persistent_buf_release(s_inbox, g_cli_job_runs_owner);
        s_inbox = grown;
        s_inbox_capacity = capacity;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_inbox + s_inbox_used, .src = &report, .bytes = sizeof(report)});
    if (length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_inbox + s_inbox_used + record, .src = text, .bytes = length});
    }
    s_inbox_used += bytes;
    EMBED_CALL(lock_left, JobRunsCtx, .at = 0u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

int cli_job_runs_step(const CliJobRunsWokenCfg *args)
{
    (void)args;
    EMBED_CALL(lock_taken, JobRunsCtx, .at = 0u);
    unsigned char *const inbox = s_inbox;
    const unsigned long long used = s_inbox_used;
    s_inbox = NULL;
    s_inbox_used = 0ULL;
    s_inbox_capacity = 0ULL;
    EMBED_CALL(lock_left, JobRunsCtx, .at = 0u);
    if (inbox == NULL)
    {
        return 0;
    }
    const unsigned long long record = (sizeof(JobRunsReport) + JOB_RUNS_ALIGN - 1ULL) & ~(JOB_RUNS_ALIGN - 1ULL);
    for (unsigned long long at = 0ULL; at < used;)
    {
        JobRunsReport report;
        memor.cpy(&(MemoriaCfg){.dst = &report, .src = inbox + at, .bytes = sizeof(report)});
        const char *const text = (const char *)(inbox + at + record);
        at += record + ((report.length + JOB_RUNS_ALIGN - 1ULL) & ~(JOB_RUNS_ALIGN - 1ULL));
        if (report.type == CLI_JOB_RUNS_STARTED)
        {
            EMBED_CALL(start_read, JobRunsCtx, .report = &report, .text = text);
            continue;
        }
        const unsigned int place = EMBED_CALL(run_place, JobRunsCtx, .run = report.run);
        if (place == g_cli_job_runs_store.runs)
        {
            continue;
        }
        if (report.type == CLI_JOB_RUNS_LINE)
        {
            EMBED_CALL(line_read, JobRunsCtx, .report = &report, .text = text, .at = place);
        }
        else
        {
            EMBED_CALL(end_read, JobRunsCtx, .report = &report, .at = place);
        }
    }
    g_cli_guard->persistent_buf_release(inbox, g_cli_job_runs_owner);
    for (unsigned int at = 0u; at < g_cli_job_runs_store.runs; at += 1u)
    {
        EMBED_CALL(lines_bounded, JobRunsCtx, .at = at);
    }
    EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
    return 1;
}

void cli_job_runs_read(const CliJobRunsRootCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    // the groups' open flags read by name before the groups go: a group's name is the catalog's own text
    CliJobRunsGroup *const was = store->group;
    const unsigned int was_groups = store->groups;
    store->group = NULL;
    store->groups = 0u;
    if (store->own.cells != NULL)
    {
        g_cli_guard->persistent_buf_release(store->own.job, store->own.cells);
        store->own = (CliCatalog){0};
    }
    store->catalog = args->catalog;
    if ((args->catalog == NULL) && (args->root != NULL))
    {
        EMBED_CALL(cli_catalog_cells.take, CliCatalogCellsCfg, .catalog = &store->own, .cells = g_cli_job_runs_owner,
                   .jobs_max = CLI_LINE_JOBS);
        EMBED_CALL(cli_catalog_root.read, CliCatalogRootCfg, .root = args->root, .catalog = &store->own);
        store->catalog = (store->own.job != NULL) ? &store->own : NULL;
    }
    const CliCatalog *const catalog = store->catalog;
    const unsigned int jobs = (catalog != NULL) ? catalog->jobs : 0u;
    // the groups in the order their first jobs come, each counted, in a cell sized to them
    unsigned int groups = 0u;
    for (unsigned int job = 0u; job < jobs; job += 1u)
    {
        unsigned int before = 0u;
        while ((before < job) &&
               !cellul.eq(&(CatenaFinitaCfg){
                   .src = catalog->job[before].group, .other = catalog->job[job].group, .cap = CLI_NAME_MAX_LEN}))
        {
            before += 1u;
        }
        groups += (before == job) ? 1u : 0u;
    }
    g_cli_guard->persistent_buf_release(store->order, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->passes, g_cli_job_runs_owner);
    store->order =
        (jobs != 0u) ? g_cli_guard->persistent_buf_alloc(jobs * sizeof(unsigned int), g_cli_job_runs_owner) : NULL;
    store->passes = (jobs != 0u) ? g_cli_guard->persistent_buf_alloc(jobs, g_cli_job_runs_owner) : NULL;
    store->group = (groups != 0u)
                       ? g_cli_guard->persistent_buf_alloc(groups * sizeof(CliJobRunsGroup), g_cli_job_runs_owner)
                       : NULL;
    if ((jobs != 0u) && ((store->order == NULL) || (store->passes == NULL) || (store->group == NULL)))
    {
        g_cli_guard->persistent_buf_release(store->order, g_cli_job_runs_owner);
        g_cli_guard->persistent_buf_release(store->passes, g_cli_job_runs_owner);
        g_cli_guard->persistent_buf_release(store->group, g_cli_job_runs_owner);
        store->order = NULL;
        store->passes = NULL;
        store->group = NULL;
        groups = 0u;
    }
    unsigned int placed = 0u;
    for (unsigned int job = 0u; (store->group != NULL) && (job < jobs); job += 1u)
    {
        unsigned int group = 0u;
        while ((group < store->groups) &&
               !cellul.eq(&(CatenaFinitaCfg){
                   .src = store->group[group].name, .other = catalog->job[job].group, .cap = CLI_NAME_MAX_LEN}))
        {
            group += 1u;
        }
        if (group < store->groups)
        {
            continue;
        }
        // a group new to the list: its jobs in the catalog's order after the groups before it, its open flag the
        // one its name had
        CliJobRunsGroup *const one = &store->group[store->groups];
        *one = (CliJobRunsGroup){.name = catalog->job[job].group, .first = placed};
        for (unsigned int old = 0u; old < was_groups; old += 1u)
        {
            one->open = cellul.eq(&(CatenaFinitaCfg){.src = was[old].name, .other = one->name, .cap = CLI_NAME_MAX_LEN})
                            ? was[old].open
                            : one->open;
        }
        for (unsigned int member = job; member < jobs; member += 1u)
        {
            if (cellul.eq(
                    &(CatenaFinitaCfg){.src = catalog->job[member].group, .other = one->name, .cap = CLI_NAME_MAX_LEN}))
            {
                store->order[placed] = member;
                placed += 1u;
                one->count += 1u;
            }
        }
        store->groups += 1u;
    }
    g_cli_guard->persistent_buf_release(was, g_cli_job_runs_owner);
    // the job chosen followed by its id, its form standing where it is still listed, and each run's job found again
    const unsigned int chosen =
        ((store->chosen_id != NULL) && (catalog != NULL))
            ? EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = catalog, .id = store->chosen_id)
            : CLI_CATALOG_NONE;
    if (chosen == CLI_CATALOG_NONE)
    {
        g_cli_guard->persistent_buf_release(store->chosen_id, g_cli_job_runs_owner);
        store->chosen_id = NULL;
    }
    const int form_stands = (chosen != CLI_CATALOG_NONE) && (catalog->job[chosen].params == store->form_params);
    store->chosen = chosen;
    if (!form_stands)
    {
        EMBED_CALL(cli_job_runs_internal.form_clear, CliJobRunsCtx, .values = NULL);
    }
    for (unsigned int at = 0u; at < store->runs; at += 1u)
    {
        store->run[at].job = (catalog != NULL) ? EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg,
                                                            .catalog_read = catalog, .id = store->run[at].job_id)
                                               : CLI_CATALOG_NONE;
    }
    EMBED_CALL(cli_job_runs_internal.filter_apply, CliJobRunsCtx, .values = NULL);
}

int cli_job_runs_start(const CliJobRunsRunCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    g_cli_guard->persistent_buf_release(store->refusal, g_cli_job_runs_owner);
    store->refusal = NULL;
    store->refusal_length = 0ULL;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    unsigned long long run = 0ULL;
    CliValues *const values = (store->chosen != CLI_CATALOG_NONE)
                                  ? g_cli_guard->temporary_buf_alloc(sizeof(CliValues), g_cli_job_runs_owner)
                                  : NULL;
    if (values != NULL)
    {
        EMBED_CALL(cli_job_runs_internal.values_fill, CliJobRunsCtx, .values = values);
    }
    const int started =
        (values != NULL) && EMBED_CALL(cli_runner_err_msg.runs_start, CliRunnerErrMsgCfg, .err_msg = &err_msg,
                                       .root = args->root, .catalog = store->catalog, .job = store->chosen,
                                       .values = values, .sink = cli_job_runs_sink, .run_number_pointer = &run);
    g_cli_guard->persistent_buf_release(values, g_cli_job_runs_owner);
    if (started)
    {
        store->shown = run;
        return 1;
    }
    // the refusal in a cell sized to it with its end: the runner's, or why nothing could start
    if (store->chosen == CLI_CATALOG_NONE)
    {
        mmgr_line_put(&err_msg, JOB_RUNS_NONE_CHOSEN);
    }
    const unsigned long long refused = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL;
    store->refusal = (refused != 0ULL) ? g_cli_guard->persistent_buf_alloc(refused + 1ULL, g_cli_job_runs_owner) : NULL;
    if (store->refusal != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = store->refusal, .src = why, .bytes = refused});
        store->refusal[refused] = '\0';
        store->refusal_length = refused;
    }
    return 0;
}

int cli_job_runs_live(const CliJobRunsRunCfg *args)
{
    (void)args;
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const unsigned int place = EMBED_CALL(run_place, JobRunsCtx, .run = store->shown);
    return (store->shown != 0ULL) && (place < store->runs) && store->run[place].live;
}

int cli_job_runs_stop(const CliJobRunsRunCfg *args)
{
    if (!EMBED_CALL(cli_job_runs_run.live, CliJobRunsRunCfg, .root = args->root))
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_runner_err_msg_run_number.runs_stop, CliRunnerErrMsgRunNumberCfg, .err_msg = &err_msg,
                      .run_number = g_cli_job_runs_store.shown);
}

const char *cli_job_runs_refusal(const CliJobRunsRunCfg *args)
{
    *args->length_out = g_cli_job_runs_store.refusal_length;
    return (g_cli_job_runs_store.refusal != NULL) ? g_cli_job_runs_store.refusal : "";
}

void cli_job_runs_end(const CliJobRunsWokenCfg *args)
{
    (void)args;
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    EMBED_CALL(lock_taken, JobRunsCtx, .at = 0u);
    g_cli_guard->persistent_buf_release(s_inbox, g_cli_job_runs_owner);
    s_inbox = NULL;
    s_inbox_used = 0ULL;
    s_inbox_capacity = 0ULL;
    EMBED_CALL(lock_left, JobRunsCtx, .at = 0u);
    while (store->runs != 0u)
    {
        EMBED_CALL(run_released, JobRunsCtx, .at = store->runs - 1u);
    }
    if (store->own.cells != NULL)
    {
        g_cli_guard->persistent_buf_release(store->own.job, store->own.cells);
    }
    g_cli_guard->persistent_buf_release(store->group, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->order, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->passes, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->chosen_id, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->section, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->form, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->refusal, g_cli_job_runs_owner);
    g_cli_guard->persistent_buf_release(store->filter.bytes, g_cli_job_runs_owner);
    *store = (CliJobRunsStore){.chosen = CLI_CATALOG_NONE, .focus = CLI_JOB_RUNS_FILTER};
    s_woken = NULL;
}
