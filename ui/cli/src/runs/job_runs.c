// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// job_runs.c: the Run pane's store (job_runs.h): the catalog listed by group, the job chosen, the runs the pane starts
// and stops, and the sink their threads report to. A report is a movable cell of its record and its text, its token
// pushed on the reports' ticket ring under a ticket of the run's thread, which waits where the ring is a lap behind,
// and the view's thread woken; the step pops the reports in ticket order on the view's thread and reads them into the
// runs: a run's start a record, a line its plain text and its spans read once through the xterm into the run's three
// rings, the oldest lines released as a new one needs the room, an end its state, and the oldest ended runs past
// CLI_JOB_RUNS_RETAINED released with their cells
#include "job_runs_internal.h"

#include "../command_line/command_line_internal.h"
#include "embed_atomic.h"

// the sentence a start with no job chosen is refused with
#define JOB_RUNS_NONE_CHOSEN "No job is chosen."

CliJobRunsStore g_cli_job_runs_store = {.chosen = CLI_CATALOG_NONE, .focus = CLI_JOB_RUNS_FILTER};

// A report on its way: its type, its run, its line's stream, its text's bytes, which follow the record in the
// report's movable cell, and for an end its code where it has one and whether the run was stopped
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

// A group as it stood before the catalog was read again: its name and whether it was open
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    int open;
} JobRunsGroupWas;

// The reports' ticket ring, its slots the reports' part of the rings block, laid as the pane begins; its turnstiles,
// zeros at rest; the next ticket a run's thread takes and the next the view's thread pops, which that thread alone
// touches; the wake a run's thread sleeps on while every slot is in flight; and the wake of the view's thread
static EmbedTicketRing s_reports = {.mask = (1u << CLI_JOB_RUNS_REPORTS_SHIFT) - 1u};
static EmbedTurnstiles s_turnstiles;
static embed_atomic_u32 s_ticket;
static embed_u32 s_taken;
static CliWake s_room = CLI_WAKE_INIT;
static void (*s_woken)(void);

typedef struct
{
    const JobRunsReport *report;
    const char *text;
    unsigned int at;
    unsigned long long run;
    CliJobRunsRun *copy;
    CliJobRunsRings *rings;
    JobRunsReport *popped;
} JobRunsCtx;

void cli_job_runs_group_read(const CliJobRunsCellCfg *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_job_runs_store.group[args->at],
                                                            .offset = 0ULL,
                                                            .dst = args->group,
                                                            .bytes = sizeof(*args->group)});
}

int cli_job_runs_group_write(const CliJobRunsCellCfg *args)
{
    const CarcerMovableEntry **const cell = &g_cli_job_runs_store.group[args->at];
    *cell = (*cell != NULL) ? *cell : g_cli_guard->movable_buf_alloc(sizeof(*args->group), g_cli_job_runs_owner);
    return (*cell != NULL) && g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                                  .cell = *cell, .offset = 0ULL, .src = args->group, .bytes = sizeof(*args->group)});
}

void cli_job_runs_run_read(const CliJobRunsCellCfg *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = g_cli_job_runs_store.run[args->at], .offset = 0ULL, .dst = args->run, .bytes = sizeof(*args->run)});
}

int cli_job_runs_run_write(const CliJobRunsCellCfg *args)
{
    const CarcerMovableEntry **const cell = &g_cli_job_runs_store.run[args->at];
    *cell = (*cell != NULL) ? *cell : g_cli_guard->movable_buf_alloc(sizeof(*args->run), g_cli_job_runs_owner);
    return (*cell != NULL) && g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                                  .cell = *cell, .offset = 0ULL, .src = args->run, .bytes = sizeof(*args->run)});
}

void cli_job_runs_section_read(const CliJobRunsCellCfg *args)
{
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = g_cli_job_runs_store.section[args->at],
                                                            .offset = 0ULL,
                                                            .dst = args->section,
                                                            .bytes = sizeof(*args->section)});
}

int cli_job_runs_section_write(const CliJobRunsCellCfg *args)
{
    const CarcerMovableEntry **const cell = &g_cli_job_runs_store.section[args->at];
    *cell = (*cell != NULL) ? *cell : g_cli_guard->movable_buf_alloc(sizeof(*args->section), g_cli_job_runs_owner);
    return (*cell != NULL) &&
           g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
               .cell = *cell, .offset = 0ULL, .src = args->section, .bytes = sizeof(*args->section)});
}

// every group's cell given back, the groups none
EMBED_INLINE void groups_released(const JobRunsCtx *args)
{
    (void)args;
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    for (unsigned int at = 0u; at < store->groups; at += 1u)
    {
        (void)g_cli_guard->movable_buf_release(store->group[at], g_cli_job_runs_owner);
        store->group[at] = NULL;
    }
    store->groups = 0u;
}

// the place of the run numbered `run` among the store's runs, `runs` where none is: each record's number read alone
EMBED_INLINE unsigned int run_place(const JobRunsCtx *args)
{
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    for (unsigned int at = store->runs; at > 0u; at -= 1u)
    {
        unsigned long long number = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = store->run[(at - 1u)],
                                                                .offset = offsetof(CliJobRunsRun, run),
                                                                .dst = &number,
                                                                .bytes = sizeof(number)});
        if (number == args->run)
        {
            return at - 1u;
        }
    }
    return store->runs;
}

// the copy's state written anew from what the run reports: `#n running`, `#n stopped`, `#n exit N` or `#n ended`,
// the place of the word after the number standing beside it
EMBED_INLINE void state_written(const JobRunsCtx *args)
{
    CliJobRunsRun *const run = args->copy;
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

// the copy's cells let go: its id, and its set of rings made free where it took one
EMBED_INLINE void run_cells_released(const JobRunsCtx *args)
{
    (void)g_cli_guard->movable_buf_release(args->copy->job_id, g_cli_job_runs_owner);
    if (args->copy->rings_taken != NULL)
    {
        *args->copy->rings_taken = 0u;
    }
}

// the run `at` released: its cells let go, its own with them, the runs after it moved up a place each, and the run
// shown let go where it was this one
EMBED_INLINE void run_released(const JobRunsCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    CliJobRunsRun run;
    EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = args->at, .run = &run);
    store->shown = (store->shown == run.run) ? 0ULL : store->shown;
    EMBED_CALL(run_cells_released, JobRunsCtx, .copy = &run);
    (void)g_cli_guard->movable_buf_release(store->run[args->at], g_cli_job_runs_owner);
    for (unsigned int from = args->at + 1u; from < store->runs; from += 1u)
    {
        store->run[from - 1u] = store->run[from];
    }
    store->run[store->runs - 1u] = NULL;
    store->runs -= 1u;
}

// a run's start read: where the runs stand at their most, the oldest ended one released, the start dropped where
// none has ended; the record put after the runs in a cell of its own, its job's id with its bytes in a cell of its own,
// its job found in the catalog listed, and the run shown where its job is the one chosen
EMBED_INLINE void start_read(const JobRunsCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    if (store->runs == CLI_JOB_RUNS_MAX)
    {
        unsigned int oldest = 0u;
        int live = 1;
        while ((oldest < store->runs) && live)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = store->run[oldest],
                                                                    .offset = offsetof(CliJobRunsRun, live),
                                                                    .dst = &live,
                                                                    .bytes = sizeof(live)});
            oldest += live ? 1u : 0u;
        }
        if (oldest == store->runs)
        {
            return;
        }
        EMBED_CALL(run_released, JobRunsCtx, .at = oldest);
    }
    // the id with its end, cut to the path bound, as the catalog's find reads it
    char id[CLI_PATH_MAX_LEN];
    const unsigned long long length =
        (args->report->length < CLI_PATH_MAX_LEN) ? args->report->length : CLI_PATH_MAX_LEN - 1ULL;
    if (length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = id, .src = args->text, .bytes = length});
    }
    id[length] = '\0';
    CliJobRunsRun run = {0};
    run.run = args->report->run;
    run.live = 1;
    run.job_id = g_cli_guard->movable_buf_alloc(length + 1ULL, g_cli_job_runs_owner);
    run.job_id_length = length;
    // the rings lie over the pane's part of the rings block, one set's span for each run the pane holds; a free set
    // taken for this run
    static CliJobRunsRings s_rings[CLI_JOB_RUNS_MAX];
    static unsigned char s_rings_taken[CLI_JOB_RUNS_MAX];
    for (unsigned int set = 0u; (run.rings == NULL) && (set < CLI_JOB_RUNS_MAX); set += 1u)
    {
        if (s_rings_taken[set] == 0u)
        {
            s_rings_taken[set] = 1u;
            run.rings = &s_rings[set];
            run.rings_taken = &s_rings_taken[set];
            unsigned char *const span = EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_JOB_RUNS) +
                                        ((unsigned long long)set * CLI_JOB_RUNS_SET_BYTES);
            run.rings->text_bytes = span + CLI_JOB_RUNS_SET_TEXT_AT;
            run.rings->line_bytes = (CliJobRunsLine *)(void *)(span + CLI_JOB_RUNS_SET_LINE_AT);
            run.rings->span_bytes = (CliTerminalXtermSpan *)(void *)(span + CLI_JOB_RUNS_SET_SPAN_AT);
        }
    }
    if ((run.job_id == NULL) || (run.rings == NULL))
    {
        EMBED_CALL(run_cells_released, JobRunsCtx, .copy = &run);
        return;
    }
    CliJobRunsRings *const rings = run.rings;
    rings->text_put = 0u;
    rings->text_taken = 0u;
    (void)anularis.init(&(AnularisCfg){
        .ring = &rings->text, .buf = rings->text_bytes, .capacity = CLI_JOB_RUNS_TEXT_MAX, .segment_count = 1u});
    (void)anularis.init(&(AnularisCfg){.ring = &rings->line,
                                       .buf = (embed_u8 *)(void *)rings->line_bytes,
                                       .capacity = (unsigned long long)CLI_LINES_MAX * sizeof(CliJobRunsLine),
                                       .segment_count = CLI_LINES_MAX,
                                       .segment_bytes = sizeof(CliJobRunsLine)});
    (void)anularis.init(
        &(AnularisCfg){.ring = &rings->span,
                       .buf = (embed_u8 *)(void *)rings->span_bytes,
                       .capacity = (unsigned long long)CLI_JOB_RUNS_SPANS_MAX * sizeof(CliTerminalXtermSpan),
                       .segment_count = CLI_JOB_RUNS_SPANS_MAX,
                       .segment_bytes = sizeof(CliTerminalXtermSpan)});
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = run.job_id, .offset = 0ULL, .src = id, .bytes = length + 1ULL});
    run.job = (store->catalog != NULL)
                  ? EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = store->catalog, .id = id)
                  : CLI_CATALOG_NONE;
    EMBED_CALL(state_written, JobRunsCtx, .copy = &run);
    (void)EMBED_CALL(cli_job_runs_cells.run_write, CliJobRunsCellCfg, .at = store->runs, .run = &run);
    store->runs += 1u;
    store->shown = ((run.job != CLI_CATALOG_NONE) && (run.job == store->chosen)) ? run.run : store->shown;
}

// the rings' oldest line released: its bytes consumed from the text ring, its spans' segments and its record's
// released
EMBED_INLINE void oldest_released(const JobRunsCtx *args)
{
    CliJobRunsRings *const rings = args->rings;
    size_t front = 0u;
    if (!anularis.seg_front(&(AnularisCfg){.ring = &rings->line, .out_index = &front}))
    {
        return;
    }
    // a segment of the line ring is one line record, laid at the record's alignment
    const CliJobRunsLine *const line =
        (const CliJobRunsLine *)(const void *)anularis.seg_at(&(AnularisCfg){.ring = &rings->line, .index = front});
    anularis.consume(&(AnularisCfg){.ring = &rings->text, .bytes = line->length});
    rings->text_taken += line->length;
    for (unsigned int at = 0u; at < line->spans; at += 1u)
    {
        anularis.seg_release(&(AnularisCfg){.ring = &rings->span});
    }
    anularis.seg_release(&(AnularisCfg){.ring = &rings->line});
}

// a line of the run `at` read: its text cut to CLI_JOB_RUNS_LINE_MAX bytes and read through the xterm into its plain
// bytes and its spans, the run's oldest lines released until the rings have the room, the plain bytes put in the text
// ring, each span published as a segment of the spans ring, and the line's record as a segment of the line ring
EMBED_INLINE void line_read(const JobRunsCtx *args)
{
    CliJobRunsRun run;
    EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = args->at, .run = &run);
    CliJobRunsRings *const rings = run.rings;
    const unsigned long long size =
        (args->report->length < CLI_JOB_RUNS_LINE_MAX) ? args->report->length : CLI_JOB_RUNS_LINE_MAX;
    char plain[CLI_JOB_RUNS_LINE_MAX];
    CliTerminalXtermSpan spans[CLI_JOB_RUNS_LINE_SPANS];
    unsigned long long plain_size = 0ULL;
    const unsigned int count =
        EMBED_CALL(cli_terminal_xterm_spans_table.spans, CliTerminalXtermSpansCfg, .text = args->text, .size = size,
                   .plain_out = plain, .plain_capacity = sizeof(plain), .plain_size_out = &plain_size,
                   .spans_out = spans, .spans_capacity = CLI_JOB_RUNS_LINE_SPANS);
    while ((anularis.seg_inflight(&(AnularisCfg){.ring = &rings->line}) != 0u) &&
           ((anularis.seg_inflight(&(AnularisCfg){.ring = &rings->line}) == CLI_LINES_MAX) ||
            (anularis.vacant(&(AnularisCfg){.ring = &rings->text}) < plain_size) ||
            (anularis.seg_inflight(&(AnularisCfg){.ring = &rings->span}) + count > CLI_JOB_RUNS_SPANS_MAX)))
    {
        EMBED_CALL(oldest_released, JobRunsCtx, .rings = rings);
    }
    const unsigned int start = rings->text_put;
    (void)anularis.put(
        &(AnularisCfg){.ring = &rings->text, .src = (const embed_u8 *)(const void *)plain, .bytes = plain_size});
    // the plain bytes are at most the line bound, inside an unsigned int
    rings->text_put += (unsigned int)plain_size;
    size_t span_first = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        size_t segment = 0u;
        (void)anularis.seg_next(&(AnularisCfg){.ring = &rings->span, .out_index = &segment});
        span_first = (at == 0u) ? segment : span_first;
        memor.cpy(&(MemoriaCfg){.dst = anularis.seg_at(&(AnularisCfg){.ring = &rings->span, .index = segment}),
                                .src = &spans[at],
                                .bytes = sizeof(spans[0])});
        anularis.seg_publish(&(AnularisCfg){.ring = &rings->span});
    }
    size_t segment = 0u;
    (void)anularis.seg_next(&(AnularisCfg){.ring = &rings->line, .out_index = &segment});
    // the span's segment is inside the spans ring's count, an unsigned int
    const CliJobRunsLine line = {start, (unsigned int)plain_size, args->report->stream, (unsigned int)span_first,
                                 count};
    memor.cpy(&(MemoriaCfg){.dst = anularis.seg_at(&(AnularisCfg){.ring = &rings->line, .index = segment}),
                            .src = &line,
                            .bytes = sizeof(line)});
    anularis.seg_publish(&(AnularisCfg){.ring = &rings->line});
}

// the ended runs of the job of the run `at` past CLI_JOB_RUNS_RETAINED released, the oldest first; a run that goes
// is never released, and the run that ended stands
EMBED_INLINE void runs_retained(const JobRunsCtx *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    CliJobRunsRun ended_run;
    EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = args->at, .run = &ended_run);
    char job_id[CLI_PATH_MAX_LEN];
    const unsigned long long job_id_length =
        (ended_run.job_id_length < CLI_PATH_MAX_LEN) ? ended_run.job_id_length : CLI_PATH_MAX_LEN - 1ULL;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = ended_run.job_id, .offset = 0ULL, .dst = job_id, .bytes = job_id_length});
    job_id[job_id_length] = '\0';
    char other_id[CLI_PATH_MAX_LEN];
    unsigned int ended = 0u;
    for (unsigned int at = 0u; at < store->runs; at += 1u)
    {
        CliJobRunsRun one;
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = at, .run = &one);
        const unsigned long long other_length =
            (one.job_id_length < CLI_PATH_MAX_LEN) ? one.job_id_length : CLI_PATH_MAX_LEN - 1ULL;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = one.job_id, .offset = 0ULL, .dst = other_id, .bytes = other_length});
        other_id[other_length] = '\0';
        ended += (!one.live && cellul.eq(&(CatenaFinitaCfg){.src = other_id, .other = job_id, .cap = CLI_PATH_MAX_LEN}))
                     ? 1u
                     : 0u;
    }
    for (unsigned int at = 0u; (ended > CLI_JOB_RUNS_RETAINED) && (at < store->runs);)
    {
        CliJobRunsRun one;
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = at, .run = &one);
        const unsigned long long other_length =
            (one.job_id_length < CLI_PATH_MAX_LEN) ? one.job_id_length : CLI_PATH_MAX_LEN - 1ULL;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = one.job_id, .offset = 0ULL, .dst = other_id, .bytes = other_length});
        other_id[other_length] = '\0';
        if (!one.live && (one.run != ended_run.run) &&
            cellul.eq(&(CatenaFinitaCfg){.src = other_id, .other = job_id, .cap = CLI_PATH_MAX_LEN}))
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
    CliJobRunsRun run;
    EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = args->at, .run = &run);
    run.live = 0;
    run.stopped = args->report->stopped;
    run.has_code = args->report->has_code;
    run.code = args->report->code;
    EMBED_CALL(state_written, JobRunsCtx, .copy = &run);
    (void)EMBED_CALL(cli_job_runs_cells.run_write, CliJobRunsCellCfg, .at = args->at, .run = &run);
    EMBED_CALL(runs_retained, JobRunsCtx, .at = args->at);
}

void cli_job_runs_begin(const CliJobRunsWokenCfg *args)
{
    s_woken = args->woken;
    // the reports ring laid over its slots, the reports' part of the rings block, before any run's thread reports
    s_reports.slots =
        (EmbedTicketRingSlot *)(void *)EMBED_CALL(cli_rings.part, CliRingsPartCfg, .part = CLI_RINGS_JOB_RUNS_REPORTS);
    // the store's own catalog cell taken as the window starts and kept until it ends, the tree's jobs read into it
    // again on each refresh
    if (g_cli_job_runs_store.own.job == NULL)
    {
        EMBED_CALL(cli_catalog_cells.take, CliCatalogCellsCfg, .catalog = &g_cli_job_runs_store.own,
                   .cells = g_cli_job_runs_owner, .jobs_max = CLI_LINE_JOBS);
    }
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
    const JobRunsReport report = {args->type, args->stream,   args->run,    length,
                                  args->code, args->has_code, args->stopped};
    // the report in a movable cell of its own, its text after its record; one the cells cannot meet is dropped
    const CarcerMovableEntry *const cell =
        (s_reports.slots != NULL) ? g_cli_guard->movable_buf_alloc(sizeof(report) + length, g_cli_job_runs_owner)
                                  : NULL;
    if (cell == NULL)
    {
        return;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = &report, .bytes = sizeof(report)});
    if (length != 0ULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = sizeof(report), .src = text, .bytes = length});
    }
    // a ticket taken, its slot waited for where the ring is a lap behind, the cell's token pushed under it; the view's
    // thread pops in ticket order
    const embed_u32 ticket = EMBED_ATOMIC_ADD_32(&s_ticket, 1u);
    for (;;)
    {
        const unsigned int seen = EMBED_CALL(cli_wake_internal.wake_read, CliWakeCtx, .wake = &s_room);
        if (cli_ticket_room(&s_reports, ticket))
        {
            break;
        }
        EMBED_CALL(cli_wake_internal.wake_wait_seen, CliWakeCtx, .wake = &s_room, .seen = seen);
    }
    const embed_u64 stamp = embed_ticket_stamp(cli_ticket_epoch(), ticket);
    // Explicit cast carries the cell's token as the pass's entry, and as the word the slot holds
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)cell;
    embed_u64 seen_word = 0u;
    (void)embed_ticket_ring_granted(&s_turnstiles, address, &s_reports, stamp, &seen_word);
    (void)embed_ticket_ring_pushed(&s_turnstiles, address, &s_reports, stamp, (void *)(embed_word)cell, &seen_word);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

// the next report popped off the ring into `report`, its text into a cell of the temporary tier with its end: the
// text's cell, or NULL where no report waits or the cells cannot meet the text, the report's own cell given back and
// its slot released either way once it is read
EMBED_INLINE char *report_popped(const JobRunsCtx *args)
{
    JobRunsReport *const report = args->popped;
    embed_u64 stamp = 0u;
    if (!cli_ticket_pushed(&s_reports, s_taken, &stamp))
    {
        return NULL;
    }
    // Explicit cast carries the pop's place as the pass's entry
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(embed_word)&s_taken;
    void *popped = NULL;
    embed_u64 seen_word = 0u;
    (void)embed_ticket_ring_popped(&s_turnstiles, address, &s_reports, stamp, &popped, &seen_word);
    const CarcerMovableEntry *const cell = popped;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .dst = report, .bytes = sizeof(*report)});
    char *const text = g_cli_guard->temporary_buf_alloc(report->length + 1ULL, g_cli_job_runs_owner);
    if (text != NULL)
    {
        if (report->length != 0ULL)
        {
            (void)g_cli_guard->movable_buf_read(
                &(CarcerMovableCfg){.cell = cell, .offset = sizeof(*report), .dst = text, .bytes = report->length});
        }
        text[report->length] = '\0';
    }
    (void)g_cli_guard->movable_buf_release(cell, g_cli_job_runs_owner);
    (void)embed_ticket_ring_released(&s_turnstiles, &s_reports, stamp, &seen_word);
    s_taken += 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_room);
    return text;
}

int cli_job_runs_step(const CliJobRunsWokenCfg *args)
{
    (void)args;
    int read = 0;
    for (;;)
    {
        JobRunsReport report;
        char *const text = EMBED_CALL(report_popped, JobRunsCtx, .popped = &report);
        if (text == NULL)
        {
            break;
        }
        read = 1;
        if (report.type == CLI_JOB_RUNS_STARTED)
        {
            EMBED_CALL(start_read, JobRunsCtx, .report = &report, .text = text);
        }
        else
        {
            const unsigned int place = EMBED_CALL(run_place, JobRunsCtx, .run = report.run);
            if (place != g_cli_job_runs_store.runs)
            {
                if (report.type == CLI_JOB_RUNS_LINE)
                {
                    EMBED_CALL(line_read, JobRunsCtx, .report = &report, .text = text, .at = place);
                }
                else
                {
                    EMBED_CALL(end_read, JobRunsCtx, .report = &report, .at = place);
                }
            }
        }
        g_cli_guard->persistent_buf_release(text, g_cli_job_runs_owner);
    }
    if (!read)
    {
        return 0;
    }
    EMBED_CALL(cli_job_runs_internal.sections_settle, CliJobRunsCtx, .values = NULL);
    return 1;
}

void cli_job_runs_read(const CliJobRunsRootCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    // the groups' names and open flags copied out before the groups go, in a cell of the call's own: a group's name
    // is the catalog's own text, which goes with it
    const unsigned int was_groups = store->groups;
    JobRunsGroupWas *const was =
        (was_groups != 0u)
            ? g_cli_guard->temporary_buf_alloc(was_groups * sizeof(JobRunsGroupWas), g_cli_job_runs_owner)
            : NULL;
    for (unsigned int old = 0u; (was != NULL) && (old < was_groups); old += 1u)
    {
        CliJobRunsGroup one;
        EMBED_CALL(cli_job_runs_cells.group_read, CliJobRunsCellCfg, .at = old, .group = &one);
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = one.name, .cap = CLI_NAME_MAX_LEN - 1ULL});
        memor.cpy(&(MemoriaCfg){.dst = was[old].name, .src = one.name, .bytes = length});
        was[old].name[length] = '\0';
        was[old].open = one.open;
    }
    EMBED_CALL(groups_released, JobRunsCtx, .at = 0u);
    store->catalog = args->catalog;
    if ((args->catalog == NULL) && (args->root != NULL) && (store->own.job != NULL))
    {
        // the store's own catalog emptied and the tree's jobs read into it again
        EMBED_CALL(cli_catalog_cells.emptied, CliCatalogCellsCfg, .catalog = &store->own);
        EMBED_CALL(cli_catalog_root.read, CliCatalogRootCfg, .root = args->root, .catalog = &store->own);
        store->catalog = &store->own;
    }
    const CliCatalog *const catalog = store->catalog;
    const unsigned int listed = (catalog != NULL) ? catalog->jobs : 0u;
    unsigned int jobs = (listed <= CLI_LINE_JOBS) ? listed : CLI_LINE_JOBS;
    // the jobs' order and their passes, cells of the jobs listed taken anew for this read, none where the cells cannot
    // meet them
    (void)g_cli_guard->movable_buf_release(store->order, g_cli_job_runs_owner);
    (void)g_cli_guard->movable_buf_release(store->passes, g_cli_job_runs_owner);
    store->order = (jobs != 0u) ? g_cli_guard->movable_buf_alloc((unsigned long long)jobs * sizeof(unsigned int),
                                                                 g_cli_job_runs_owner)
                                : NULL;
    store->passes = (jobs != 0u) ? g_cli_guard->movable_buf_alloc(jobs, g_cli_job_runs_owner) : NULL;
    if ((store->order == NULL) || (store->passes == NULL))
    {
        (void)g_cli_guard->movable_buf_release(store->order, g_cli_job_runs_owner);
        (void)g_cli_guard->movable_buf_release(store->passes, g_cli_job_runs_owner);
        store->order = NULL;
        store->passes = NULL;
        jobs = 0u;
    }
    // the groups in the order their first jobs come, each with its jobs in the catalog's order after the groups before
    // it, its open flag the one its name had
    unsigned int placed = 0u;
    for (unsigned int job = 0u; job < jobs; job += 1u)
    {
        unsigned int group = 0u;
        CliJobRunsGroup one;
        while (group < store->groups)
        {
            EMBED_CALL(cli_job_runs_cells.group_read, CliJobRunsCellCfg, .at = group, .group = &one);
            if (cellul.eq(
                    &(CatenaFinitaCfg){.src = one.name, .other = catalog->job[job].group, .cap = CLI_NAME_MAX_LEN}))
            {
                break;
            }
            group += 1u;
        }
        if (group < store->groups)
        {
            continue;
        }
        one = (CliJobRunsGroup){.name = catalog->job[job].group, .first = placed};
        for (unsigned int old = 0u; (was != NULL) && (old < was_groups); old += 1u)
        {
            one.open = cellul.eq(&(CatenaFinitaCfg){.src = was[old].name, .other = one.name, .cap = CLI_NAME_MAX_LEN})
                           ? was[old].open
                           : one.open;
        }
        for (unsigned int member = job; member < jobs; member += 1u)
        {
            if (cellul.eq(
                    &(CatenaFinitaCfg){.src = catalog->job[member].group, .other = one.name, .cap = CLI_NAME_MAX_LEN}))
            {
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = store->order,
                                        .offset = (unsigned long long)placed * sizeof(member),
                                        .src = &member,
                                        .bytes = sizeof(member)});
                placed += 1u;
                one.count += 1u;
            }
        }
        (void)EMBED_CALL(cli_job_runs_cells.group_write, CliJobRunsCellCfg, .at = store->groups, .group = &one);
        store->groups += 1u;
    }
    g_cli_guard->persistent_buf_release(was, g_cli_job_runs_owner);
    // the job chosen followed by its id, its form standing where it is still listed, and each run's job found again
    const unsigned int chosen =
        ((store->chosen_id[0] != '\0') && (catalog != NULL))
            ? EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = catalog, .id = store->chosen_id)
            : CLI_CATALOG_NONE;
    store->chosen_id[0] = (chosen == CLI_CATALOG_NONE) ? '\0' : store->chosen_id[0];
    const int form_stands = (chosen != CLI_CATALOG_NONE) && (catalog->job[chosen].params == store->form_params);
    store->chosen = chosen;
    if (!form_stands)
    {
        EMBED_CALL(cli_job_runs_internal.form_clear, CliJobRunsCtx, .values = NULL);
    }
    for (unsigned int at = 0u; at < store->runs; at += 1u)
    {
        CliJobRunsRun run;
        EMBED_CALL(cli_job_runs_cells.run_read, CliJobRunsCellCfg, .at = at, .run = &run);
        char id[CLI_PATH_MAX_LEN];
        const unsigned long long id_length =
            (run.job_id_length < CLI_PATH_MAX_LEN) ? run.job_id_length : CLI_PATH_MAX_LEN - 1ULL;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = run.job_id, .offset = 0ULL, .dst = id, .bytes = id_length});
        id[id_length] = '\0';
        run.job = (catalog != NULL)
                      ? EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = catalog, .id = id)
                      : CLI_CATALOG_NONE;
        (void)EMBED_CALL(cli_job_runs_cells.run_write, CliJobRunsCellCfg, .at = at, .run = &run);
    }
    EMBED_CALL(cli_job_runs_internal.filter_apply, CliJobRunsCtx, .values = NULL);
}

int cli_job_runs_start(const CliJobRunsRunCfg *args)
{
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    store->refusal[0] = '\0';
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
    // the refusal with its end: the runner's, or why nothing could start
    if (store->chosen == CLI_CATALOG_NONE)
    {
        mmgr_line_put(&err_msg, JOB_RUNS_NONE_CHOSEN);
    }
    const unsigned long long refused = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL;
    if (refused != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = store->refusal, .src = why, .bytes = refused});
    }
    store->refusal[refused] = '\0';
    store->refusal_length = refused;
    return 0;
}

int cli_job_runs_live(const CliJobRunsRunCfg *args)
{
    (void)args;
    const CliJobRunsStore *const store = &g_cli_job_runs_store;
    const unsigned int place = EMBED_CALL(run_place, JobRunsCtx, .run = store->shown);
    if ((store->shown == 0ULL) || (place >= store->runs))
    {
        return 0;
    }
    int live = 0;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = store->run[place], .offset = offsetof(CliJobRunsRun, live), .dst = &live, .bytes = sizeof(live)});
    return live;
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
    return g_cli_job_runs_store.refusal;
}

void cli_job_runs_end(const CliJobRunsWokenCfg *args)
{
    (void)args;
    CliJobRunsStore *const store = &g_cli_job_runs_store;
    // the reports still on the ring read off it and let go, their runs ended before this
    for (;;)
    {
        JobRunsReport report;
        char *const text = EMBED_CALL(report_popped, JobRunsCtx, .popped = &report);
        if (text == NULL)
        {
            break;
        }
        g_cli_guard->persistent_buf_release(text, g_cli_job_runs_owner);
    }
    while (store->runs != 0u)
    {
        EMBED_CALL(run_released, JobRunsCtx, .at = store->runs - 1u);
    }
    if (store->own.cells != NULL)
    {
        g_cli_guard->persistent_buf_release(store->own.job, store->own.cells);
    }
    EMBED_CALL(groups_released, JobRunsCtx, .at = 0u);
    (void)g_cli_guard->movable_buf_release(store->order, g_cli_job_runs_owner);
    (void)g_cli_guard->movable_buf_release(store->passes, g_cli_job_runs_owner);
    for (unsigned int at = 0u; at < CLI_JOB_RUNS_SECTIONS_MAX; at += 1u)
    {
        (void)g_cli_guard->movable_buf_release(store->section[at], g_cli_job_runs_owner);
    }
    (void)g_cli_guard->movable_buf_release(store->form, g_cli_job_runs_owner);
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &store->filter, .owner = g_cli_job_runs_owner);
    *store = (CliJobRunsStore){.chosen = CLI_CATALOG_NONE, .focus = CLI_JOB_RUNS_FILTER};
    s_woken = NULL;
}
