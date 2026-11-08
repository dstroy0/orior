// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_run.c: a job's steps run one after another on a thread of their own, each step's process started, its lines
// carried and its exit read; the program a build made found, signed and run; and a run stopped with all it started
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../programs/binary.h"
#include "../programs/executables.h"
#include "../toolchains/toolchains.h"
#include "runner_internal.h"

#include "../text/text.h"
#include <math.h>

// The program the build made, found once from the lines the steps before printed, and what it loads and how it compares
// reported as it is found: 1, its path in `file`, or 0 with why given into `why`
typedef struct
{
    int looked;
    int found;
    char file[CLI_PATH_MAX_LEN];
    char why[CLI_PATH_MAX_LEN];
} CliCreated;
// the runs open, the next run's number, whether runs share this process's console, and the lock and wake over them
CliRun g_cli_runs[CLI_RUNS_MAX];
unsigned long long g_cli_next_run;
static int s_attached;
CliWake g_cli_runs_wake = CLI_WAKE_INIT;

void cli_runs_attach(void)
{
    s_attached = 1;
}

double cli_run_since(const CliRunnerCtx *args)
{
    // the clock that only goes forward, read to the millisecond
    return (double)(cli_clock_milliseconds() - args->run_read->began);
}

typedef struct
{
    const CliRun *run_read;
    CliRun *run;
    const char *text;
    CliCreated *made;
    CliCommand *command;
    const char *displayed;
    const char *view_out;
    int *has_code;
    long long *code;
    double *step_began;
    int created_step;
    double step_began_value;
    int has_code_value;
    long long code_value;
    int *built;
    const char (*pages_found)[CLI_PATH_MAX_LEN];
    unsigned int pages;
    const char *created_text;
    unsigned int (*ready)[2];
    int program;
    unsigned int step;
    char *displayed_out;
    char *created_out;
} RunnerRunCtx;
// whether a reader has stopped the run
EMBED_INLINE int cli_stopping(const RunnerRunCtx *args)
{
    const int stopped = args->run_read->stopped;
    return stopped;
}

// A line the run prints of its own, on its errors
static void cli_run_print(const CliEachLineSizeCfg *args)
{
    const char *text = args->line;
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea line_out = {line, sizeof(line), 0ULL};
    mmgr_line_put_n(&line_out, text, args->size);
    mmgr_line_finish(&line_out);
    EMBED_CALL(cli_runner_run_stream_internal.run_line, CliRunnerRunStreamCtx, .run = (CliRun *)args->context,
               .stream = "stderr", .text = line);
}

EMBED_INLINE void cli_run_print_text(const RunnerRunCtx *args)
{
    EMBED_CALL(cli_run_print, CliEachLineSizeCfg, .context = args->run, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

static int cli_find_init(const RunnerRunCtx *args)
{
    const char *const id = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &args->run->catalog,
                                      .text_u32 = args->run->catalog.job[0].id);
    const QuerySink sink = {cli_run_print, args->run};
    if (args->made->looked)
    {
        return args->made->found;
    }
    args->made->looked = 1;
    VerbaLinea why = {args->made->why, sizeof(args->made->why), 0ULL};
    args->made->found =
        EMBED_CALL(cli_executables_path.init, CliExecutablesPathCfg, .root = args->run->root, .id = id,
                   .values = args->run->values, .lines = args->run->lines, .path = args->made->file, .err_msg = &why);
    mmgr_line_finish(&why);
    if (args->made->found)
    {
        static char s_path[1u << 16u];
        static char s_folders[256][CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path,
                   .size = sizeof(s_path));
#if defined(_WIN32)
        const char separator = ';';
#else
        const char separator = ':';
#endif
        unsigned int count = 0u;
        const char *from = s_path;
        for (const char *at = s_path;; at += 1)
        {
            if (((at[0] == separator) || (at[0] == '\0')) && (at > from) && (count < 256u))
            {
                VerbaLinea line = {s_folders[count], CLI_PATH_MAX_LEN, 0ULL};
                mmgr_line_put_n(&line, from, (unsigned long long)(at - from));
                count += (mmgr_line_finish(&line) != 0ULL) ? 1u : 0u;
            }
            if (at[0] == '\0')
            {
                break;
            }
            from = (at[0] == separator) ? (at + 1) : from;
        }
        EMBED_CALL(cli_binary_path_orior_path_count.print, CliBinaryPathOriorPathCountCfg, .path = args->made->file,
                   .orior_path = (const char (*)[CLI_PATH_MAX_LEN])s_folders, .count = count, .sink = sink);
        EMBED_CALL(cli_executables_root.built_advice, CliExecutablesRootCfg, .root = args->run->root, .id = id,
                   .lines = args->run->lines, .sink = sink);
        EMBED_CALL(cli_executables_table.compare_build, CliExecutablesCfg, .root = args->run->root, .id = id,
                   .file = args->made->file, .sink = sink);
    }
    else
    {
        EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = args->run, .text = args->made->why);
    }
    return args->made->found;
}

// One step's command run: its line printed, its process started in its own group where the run is detached, its lines
// carried, its exit read into `code` and when it began into `step_began`: 1 where it ran, 0 where it could not start or
// was stopped first
static int cli_step(const RunnerRunCtx *args)
{
    CliRun *run = args->run;
    CliCommand *command = args->command;
    int *has_code = args->has_code;
    long long *code = args->code;
    double *step_began = args->step_began;
    static char s_path[1u << 16u];
    const CliRunEvent line = {CLI_RUN_EVENT_LINE,
                              run->run,
                              NULL,
                              "command",
                              args->displayed,
                              0,
                              0LL,
                              0,
                              NULL,
                              0u,
                              NULL,
                              EMBED_CALL(cli_runner_internal.run_since, CliRunnerCtx, .run_read = run)};
    EMBED_CALL(run->sink.emit, CliEachEventCfg, .context = run->sink.context, .event = &line);
    if (cellul.eq(&(CatenaFinitaCfg){.src = run->catalog.job[0].opens, .other = "views", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->view_out);
        EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command, .name = "VIEW_OUT",
                   .value = args->view_out);
    }
    EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command,
               .name = "PYTHONUNBUFFERED", .value = "1");
    EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command,
               .name = "PYTHONIOENCODING", .value = "utf-8");
    if (!EMBED_CALL(cli_runner_name_command.command_sets, CliRunnerNameCommandCfg, .command_read = command,
                    .name = "PATH"))
    {
        EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path,
                   .size = sizeof(s_path));
        EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command, .name = "PATH",
                   .value = s_path);
    }
    char number[24];
    VerbaLinea named = {number, sizeof(number), 0ULL};
    mmgr_line_decimal(&named, run->run, 1u);
    mmgr_line_finish(&named);
    // the run's number in the step's environment: a command that would reach a window open on the tree sees it is a
    // job's own and runs where it is
    EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command,
               .name = CLI_RUN_VARIABLE, .value = number);
    *step_began = EMBED_CALL(cli_runner_internal.run_since, CliRunnerCtx, .run_read = run);
    // the run is looked at last, and the process joined to it, under one acquisition of the lock: Stop, which marks the
    // run and then reads it, either finds the process there or prevents it from starting
    if (run->stopped)
    {
        return 0;
    }
    CliSpawned spawned;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int started = EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned,
                                   .words = command->words.word_text,
                                   .folder = command->has_folder ? command->folder : NULL, .settings = command->setting,
                                   .count = command->settings, .detached = !s_attached, .err_msg = &err_msg);
    if (started)
    {
        run->process = spawned.id;
        run->group = spawned.group;
        run->has_first = 0;
    }
    if (!started)
    {
        mmgr_line_finish(&err_msg);
        char line_text[CLI_PATH_MAX_LEN + 32u];
        VerbaLinea could = {line_text, sizeof(line_text), 0ULL};
        mmgr_line_put(&could, "could not start: ");
        mmgr_line_put(&could, why);
        mmgr_line_finish(&could);
        EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = run, .text = line_text);
        *has_code = 0;
        return 0;
    }
    EMBED_CALL(cli_runner_run_spawned_internal.run_output_relay, CliRunnerRunSpawnedCtx, .run = run,
               .spawned = &spawned);
    const long long exit_code = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    run->process = 0ULL;
    run->group = 0ULL;
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    *has_code = exit_code >= 0LL;
    *code = exit_code;
    return 1;
}

// what a step of a run is: one of the job's own, the program the build made, or the signing of that program
enum
{
    CLI_READY_NOW,
    CLI_READY_CREATED,
    CLI_READY_SIGN
};

// After a step ran: what is reported of the program it ran where it ran the program the build made, and whether the run
// goes on. 1 where it goes on
EMBED_INLINE int cli_after_step(const RunnerRunCtx *args)
{
    int has_code = args->has_code_value;
    long long code = args->code_value;
    int *built = args->built;
    const char *const id = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &args->run->catalog,
                                      .text_u32 = args->run->catalog.job[0].id);
    if (args->created_step)
    {
        if (args->run->has_first)
        {
            char line[96];
            VerbaLinea line_out = {line, sizeof(line), 0ULL};
            mmgr_line_put(&line_out, "the program wrote its first line ");
            const double after = args->run->first - args->step_began_value;
            mmgr_line_decimal_signed(&line_out, llround(after));
            mmgr_line_put(&line_out, " ms after it started");
            mmgr_line_finish(&line_out);
            EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = args->run, .text = line);
        }
        const QuerySink sink = {cli_run_print, args->run};
        EMBED_CALL(cli_executables_root.run_result_advice, CliExecutablesRootCfg, .root = args->run->root, .id = id,
                   .has_code = has_code, .code = code, .lines = args->run->lines, .sink = sink);
    }
    else if (!has_code || (code != 0LL))
    {
        *built = 0;
    }
    return has_code && (code == 0LL);
}

// the commands that sign the program, and the lock they are made under, one run signing at a time
static CliCommand s_signing[4];
static char s_signing_displayed[4][CLI_PATH_MAX_LEN * 4u];

// The program the build made signed: each signing command run as a step, then what signing it reported. 1 where the run
// goes on
EMBED_INLINE int cli_sign(const RunnerRunCtx *args)
{
    int *has_code = args->has_code;
    int *built = args->built;
    if (!cli_find_init(args))
    {
        *has_code = 0;
        *built = 0;
        return 0;
    }
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int steps =
        EMBED_CALL(cli_executables_err_msg.sign_steps, CliExecutablesErrMsgCfg, .file = args->made->file,
                   .commands = s_signing, .displayed = s_signing_displayed, .capacity = 4u, .err_msg = &err_msg);
    if (steps < 0)
    {
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = args->run, .text = why);
        *has_code = 0;
        *built = 0;
        return 0;
    }
    int goes = 1;
    int at = 0;
    while (goes && (at < steps))
    {
        double began = 0.0;
        if (EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = args->run) ||
            !EMBED_CALL(cli_step, RunnerRunCtx, .run = args->run, .command = &s_signing[at],
                        .displayed = s_signing_displayed[at], .view_out = args->view_out, .has_code = has_code,
                        .code = args->code, .step_began = &began))
        {
            *has_code = EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = args->run) ? *has_code : 0;
            goes = 0;
            break;
        }
        goes = EMBED_CALL(cli_after_step, RunnerRunCtx, .run = args->run, .created_step = 0, .step_began_value = began,
                          .has_code_value = *has_code, .code_value = *args->code, .built = built);
        at += 1;
    }
    if (goes)
    {
        const QuerySink sink = {cli_run_print, args->run};
        EMBED_CALL(cli_executables_table.signature_print, CliExecutablesCfg, .file = args->made->file, .sink = sink);
    }
    return goes;
}

// a run's end reported, with the code of its last step, the pages it wrote and the program it made, its cells given
// back and its end reported to those waiting on it
EMBED_INLINE void cli_run_end(const RunnerRunCtx *args)
{
    CliRun *run = args->run;
    const CliRunEvent end = {CLI_RUN_EVENT_END,
                             run->run,
                             NULL,
                             NULL,
                             NULL,
                             args->has_code_value,
                             args->code_value,
                             EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = run),
                             args->pages_found,
                             args->pages,
                             args->created_text,
                             EMBED_CALL(cli_runner_internal.run_since, CliRunnerCtx, .run_read = run)};
    EMBED_CALL(run->sink.emit, CliEachEventCfg, .context = run->sink.context, .event = &end);
    g_cli_guard->persistent_buf_release(run->job_cell, g_cli_runs_owner);
    g_cli_guard->persistent_buf_release(run->lines, g_cli_runs_owner);
    run->job_cell = NULL;
    run->lines = NULL;
    run->ended = 1;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_runs_wake);
}

// the steps of the run's job that run with its values into `ready`, each its kind and the place of its step, and the
// signing put before the first that runs the made program where the job opens a `program` and the system signs: how
// many
EMBED_INLINE unsigned int cli_steps_ready(const RunnerRunCtx *args)
{
    const CliCatalog *const catalog = &args->run->catalog;
    const CliJob *const job = &catalog->job[0];
    unsigned int (*const ready)[2] = args->ready;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < job->steps; at += 1u)
    {
        const CliStep *const step = &catalog->step[job->step_first + at];
        if (EMBED_CALL(cli_runner_command_table.runs, CliRunnerCommandCfg, .catalog = catalog, .step = step,
                       .values = args->run->values))
        {
            ready[count][0] = (step->program == CLI_PROGRAM_CREATED) ? CLI_READY_CREATED : CLI_READY_NOW;
            ready[count][1] = job->step_first + at;
            count += 1u;
        }
    }
    if (args->program && cli_executables_signs())
    {
        unsigned int place = 0u;
        while ((place < count) && (ready[place][0] != CLI_READY_CREATED))
        {
            place += 1u;
        }
        for (unsigned int at = count; at > place; at -= 1u)
        {
            ready[at][0] = ready[at - 1u][0];
            ready[at][1] = ready[at - 1u][1];
        }
        ready[place][0] = CLI_READY_SIGN;
        count += 1u;
    }
    return count;
}

// the job's step at `step` taken: its command written into `command` and its line into `displayed_out`, for the
// program the build made, `made`, where it is a `created_step`, then run, and what follows it read; 1 where the run
// goes on
EMBED_INLINE int cli_job_step(const RunnerRunCtx *args)
{
    CliRun *const run = args->run;
    const CliStep *const step = &run->catalog.step[args->step];
    VerbaLinea displayed = {args->displayed_out, CLI_COMMAND_TEXT, 0ULL};
    if (args->created_step)
    {
        if (!EMBED_CALL(cli_find_init, RunnerRunCtx, .run = run, .made = args->made))
        {
            *args->has_code = 0;
            return 0;
        }
        EMBED_CALL(cli_executables_values.run_command, CliExecutablesValuesCfg, .root = run->root,
                   .file = args->made->file, .values = run->values, .system = step->system, .command = args->command,
                   .displayed_verba_linea_pointer = &displayed);
    }
    else
    {
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if (!EMBED_CALL(cli_runner_command_table.command, CliRunnerCommandCfg, .root = run->root,
                        .catalog = &run->catalog, .step = step, .values = run->values, .command = args->command,
                        .displayed = &displayed, .err_msg = &err_msg))
        {
            mmgr_line_finish(&err_msg);
            EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = run, .text = why);
            *args->has_code = 0;
            return 0;
        }
    }
    mmgr_line_finish(&displayed);
    double began = 0.0;
    if (!EMBED_CALL(cli_step, RunnerRunCtx, .run = run, .command = args->command, .displayed = args->displayed_out,
                    .view_out = args->view_out, .has_code = args->has_code, .code = args->code, .step_began = &began))
    {
        *args->built = *args->built && args->created_step;
        return 0;
    }
    return EMBED_CALL(cli_after_step, RunnerRunCtx, .run = run, .created_step = args->created_step,
                      .step_began_value = began, .has_code_value = *args->has_code, .code_value = *args->code,
                      .built = args->built);
}

// the path of the program the build made, `made`, written into `created_out`: from the run's tree where it lies under
// it, else whole
EMBED_INLINE void cli_created_written(const RunnerRunCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->run->root, .cap = CLI_POOL_BYTES});
    if (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->made->file, .other_pointer = args->run->root,
                   .size = length))
    {
        EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->run->root, .path = args->made->file,
                   .relative_out = args->created_out);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->created_out,
            .src = args->made->file,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->made->file, .cap = CLI_POOL_BYTES}) + 1u});
    }
}

// A run's steps taken in turn on its thread, its end reported after the last
void cli_run_steps(void *context)
{
    CliRun *const run = (CliRun *)context;
    const CliCatalog *const catalog = &run->catalog;
    const CliJob *const job = &catalog->job[0];
    // the command a step runs and the line it is shown as, a cell of the run's allocated while it runs; and the pages
    // it wrote, a cell taken as it ends
    typedef struct
    {
        CliCommand command;
        char displayed[CLI_COMMAND_TEXT];
    } CliRunCommand;
    CliRunCommand *const run_command = g_cli_guard->temporary_buf_alloc(sizeof(CliRunCommand), g_cli_runs_owner);
    if (run_command == NULL)
    {
        EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = run, .text = "a run's command could not be taken");
        EMBED_CALL(cli_run_end, RunnerRunCtx, .run = run, .has_code_value = 0, .code_value = 0LL, .pages_found = NULL,
                   .pages = 0u, .created_text = NULL);
        return;
    }
    CliCommand *const command = &run_command->command;
    char view_out[CLI_PATH_MAX_LEN];
    VerbaLinea named = {view_out, sizeof(view_out), 0ULL};
    mmgr_line_put(&named, run->root);
    mmgr_line_put(&named, "/build/ui/views/");
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '-');
    mmgr_line_decimal(&named, run->run, 1u);
    mmgr_line_finish(&named);
    const int views = cellul.eq(&(CatenaFinitaCfg){.src = job->opens, .other = "views", .cap = CLI_POOL_BYTES});
    const int program = cellul.eq(&(CatenaFinitaCfg){.src = job->opens, .other = "program", .cap = CLI_POOL_BYTES});
    if (views)
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = view_out);
    }
    // the steps in turn: the job's own that run with these values, the signing put before the first made program's
    unsigned int ready[CLI_RUN_STEPS + 1u][2];
    const unsigned int count =
        EMBED_CALL(cli_steps_ready, RunnerRunCtx, .run = run, .ready = ready, .program = program);
    int has_code = 1;
    long long code = 0LL;
    int built = 1;
    CliCreated made;
    memor.set(&(MemoriaCfg){.dst = &made, .val = 0, .bytes = sizeof(made)});
    unsigned int at = 0u;
    int going = 1;
    while (going && (at < count))
    {
        if (EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = run))
        {
            going = 0;
        }
        else if (ready[at][0] == CLI_READY_SIGN)
        {
            going = EMBED_CALL(cli_sign, RunnerRunCtx, .run = run, .made = &made, .view_out = view_out,
                               .has_code = &has_code, .code = &code, .built = &built);
        }
        else
        {
            going = EMBED_CALL(cli_job_step, RunnerRunCtx, .run = run, .step = ready[at][1],
                               .created_step = ready[at][0] == CLI_READY_CREATED, .made = &made, .command = command,
                               .displayed_out = run_command->displayed, .view_out = view_out, .has_code = &has_code,
                               .code = &code, .built = &built);
        }
        at += 1u;
    }
    g_cli_guard->persistent_buf_release(run_command, g_cli_runs_owner);
    char (*const pages_found)[CLI_PATH_MAX_LEN] =
        views
            ? g_cli_guard->persistent_buf_alloc((unsigned long long)CLI_VIEWS_MAX * CLI_PATH_MAX_LEN, g_cli_runs_owner)
            : NULL;
    const unsigned int pages = (pages_found != NULL)
                                   ? EMBED_CALL(cli_runner_root_internal.run_pages, CliRunnerRootCtx, .root = run->root,
                                                .view_out = view_out, .lines = run->lines, .views = pages_found)
                                   : 0u;
    const int stopped = EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = run);
    const char *const id =
        EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = catalog, .text_u32 = job->id);
    if (program && !built && !stopped)
    {
        const QuerySink sink = {cli_run_print, run};
        EMBED_CALL(cli_executables_root.advice, CliExecutablesRootCfg, .id = id, .lines = run->lines, .sink = sink);
    }
    char created_text[CLI_PATH_MAX_LEN];
    const int has_created =
        program && built && !stopped && EMBED_CALL(cli_find_init, RunnerRunCtx, .run = run, .made = &made);
    if (has_created)
    {
        EMBED_CALL(cli_created_written, RunnerRunCtx, .run = run, .made = &made, .created_out = created_text);
    }
    EMBED_CALL(cli_run_end, RunnerRunCtx, .run = run, .has_code_value = has_code, .code_value = code,
               .pages_found = (const char (*)[CLI_PATH_MAX_LEN])pages_found, .pages = pages,
               .created_text = has_created ? created_text : NULL);
    g_cli_guard->persistent_buf_release(pages_found, g_cli_runs_owner);
}
