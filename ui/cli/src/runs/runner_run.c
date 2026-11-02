// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_run.c: a job's steps run one after another on a thread of their own, each step's process started, its lines
// carried and its exit read; the program a build made found, signed and run; and a run stopped with all it started
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
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
} CliMade;
// the runs open, the next run's number, whether runs share this process's console, and the lock and wake over them
CliRun g_cli_runs[CLI_RUNS_MOST];
unsigned long long g_cli_next_run;
static int s_attached;
CliLock g_cli_runs_lock = CLI_LOCK_INIT;
CliWake g_cli_runs_wake = CLI_WAKE_INIT;

void cli_runs_take(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
}

void cli_runs_give(void)
{
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
}

void cli_runs_attach(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    s_attached = 1;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
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
    CliMade *made;
    CliCommand *command;
    const char *shown;
    const char *view_out;
    int *has_code;
    long long *code;
    double *step_began;
    int made_step;
    double step_began_value;
    int has_code_value;
    long long code_value;
    int *built;
    const char (*pages_found)[CLI_PATH_MAX_LEN];
    unsigned int pages;
    const char *made_text;
} RunnerRunCtx;
// whether a reader has stopped the run
EMBED_INLINE int cli_stopping(const RunnerRunCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    const int stopped = args->run_read->stopped;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
    return stopped;
}

// A line the run prints of its own, on its errors
static void cli_run_print(const CliEachLineSizeCfg *args)
{
    const char *text = args->line;
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea line_out = {line, sizeof(line), 0ull};
    mmgr_line_put_n(&line_out, text, args->size);
    mmgr_line_finish(&line_out);
    EMBED_CALL(cli_runner_run_stream_internal.run_line, CliRunnerRunStreamCtx, .run = (CliRun *)args->context,
               .stream = "stderr", .text = line);
}

EMBED_INLINE void cli_run_print_text(const RunnerRunCtx *args)
{
    EMBED_CALL(cli_run_print, CliEachLineSizeCfg, .context = args->run, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}));
}

static int cli_find_init(const RunnerRunCtx *args)
{
    const char *const id = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &args->run->catalog,
                                      .text_u32 = args->run->catalog.job[0].id);
    const QuerySink sink = {cli_run_print, args->run};
    if (!args->made->looked)
    {
        args->made->looked = 1;
        VerbaLinea why = {args->made->why, sizeof(args->made->why), 0ull};
        args->made->found = EMBED_CALL(cli_executables_path.init, CliExecutablesPathCfg, .root = args->run->root,
                                       .id = id, .values = args->run->values, .lines = args->run->lines,
                                       .path = args->made->file, .err_msg = &why);
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
                    VerbaLinea line = {s_folders[count], CLI_PATH_MAX_LEN, 0ull};
                    mmgr_line_put_n(&line, from, (unsigned long long)(at - from));
                    count += (mmgr_line_finish(&line) != 0ull) ? 1u : 0u;
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
                              args->shown,
                              0,
                              0ll,
                              0,
                              NULL,
                              0u,
                              NULL,
                              EMBED_CALL(cli_runner_internal.run_since, CliRunnerCtx, .run_read = run)};
    EMBED_CALL(run->sink.emit, CliEachEventCfg, .context = run->sink.context, .event = &line);
    if (cellul.eq(&(CatenaFinitaCfg){.src = run->catalog.job[0].opens, .other = "views", .cap = CLI_CELLS_POOL}))
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
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run->run, 1u);
    mmgr_line_finish(&named);
    // the run's number in the step's environment: a command that would reach a window open on the tree sees it is a
    // job's own and runs where it is
    EMBED_CALL(cli_runner_command_name.command_set, CliRunnerCommandNameCfg, .command = command,
               .name = CLI_RUN_VARIABLE, .value = number);
    *step_began = EMBED_CALL(cli_runner_internal.run_since, CliRunnerCtx, .run_read = run);
    // the run is looked at last, and the process joined to it, under one acquisition of the lock: Stop, which marks the
    // run and then reads it, either finds the process there or prevents it from starting
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    if (run->stopped)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
        return 0;
    }
    CliSpawned spawned;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int started = EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &spawned,
                                   .words = command->words.word, .folder = command->has_folder ? command->folder : NULL,
                                   .settings = command->setting, .count = command->settings, .detached = !s_attached,
                                   .err_msg = &err_msg);
    if (started)
    {
        run->process = spawned.id;
        run->group = spawned.group;
        run->has_first = 0;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
    if (!started)
    {
        mmgr_line_finish(&err_msg);
        char line_text[CLI_PATH_MAX_LEN + 32u];
        VerbaLinea could = {line_text, sizeof(line_text), 0ull};
        mmgr_line_put(&could, "could not start: ");
        mmgr_line_put(&could, why);
        mmgr_line_finish(&could);
        EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = run, .text = line_text);
        *has_code = 0;
        return 0;
    }
    EMBED_CALL(cli_runner_run_spawned_internal.run_carry, CliRunnerRunSpawnedCtx, .run = run, .spawned = &spawned);
    const long long exit_code = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &spawned);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    run->process = 0ull;
    run->group = 0ull;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &spawned);
    *has_code = exit_code >= 0ll;
    *code = exit_code;
    return 1;
}

// what a step of a run is: one of the job's own, the program the build made, or the signing of that program
enum
{
    CLI_READY_NOW,
    CLI_READY_MADE,
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
    if (args->made_step)
    {
        if (args->run->has_first)
        {
            char line[96];
            VerbaLinea line_out = {line, sizeof(line), 0ull};
            mmgr_line_put(&line_out, "the program wrote its first line ");
            const double after = args->run->first - args->step_began_value;
            // a span in milliseconds, rounded, is far inside what a long long represents
            mmgr_line_decimal_signed(&line_out, (long long)llround(after));
            mmgr_line_put(&line_out, " ms after it started");
            mmgr_line_finish(&line_out);
            EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = args->run, .text = line);
        }
        const QuerySink sink = {cli_run_print, args->run};
        EMBED_CALL(cli_executables_root.ran_advice, CliExecutablesRootCfg, .root = args->run->root, .id = id,
                   .has_code = has_code, .code = code, .lines = args->run->lines, .sink = sink);
    }
    else if (!has_code || (code != 0ll))
    {
        *built = 0;
    }
    return has_code && (code == 0ll);
}

// the commands that sign the program, and the lock they are made under, one run signing at a time
static CliCommand s_signing[4];
static char s_signing_shown[4][CLI_PATH_MAX_LEN * 4u];
static CliLock s_signing_lock = CLI_LOCK_INIT;

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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_signing_lock);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int steps = EMBED_CALL(cli_executables_err_msg.sign_steps, CliExecutablesErrMsgCfg, .file = args->made->file,
                                 .commands = s_signing, .shown = s_signing_shown, .room = 4u, .err_msg = &err_msg);
    if (steps < 0)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_signing_lock);
        mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = args->run, .text = why);
        *has_code = 0;
        *built = 0;
        return 0;
    }
    int goes = 1;
    for (int at = 0; goes && (at < steps); at += 1)
    {
        double began = 0.0;
        if (EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = args->run) ||
            !EMBED_CALL(cli_step, RunnerRunCtx, .run = args->run, .command = &s_signing[at],
                        .shown = s_signing_shown[at], .view_out = args->view_out, .has_code = has_code,
                        .code = args->code, .step_began = &began))
        {
            *has_code = EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = args->run) ? *has_code : 0;
            goes = 0;
            break;
        }
        goes = EMBED_CALL(cli_after_step, RunnerRunCtx, .run = args->run, .made_step = 0, .step_began_value = began,
                          .has_code_value = *has_code, .code_value = *args->code, .built = built);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_signing_lock);
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
                             args->made_text,
                             EMBED_CALL(cli_runner_internal.run_since, CliRunnerCtx, .run_read = run)};
    EMBED_CALL(run->sink.emit, CliEachEventCfg, .context = run->sink.context, .event = &end);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_runs_lock);
    EMBED_CALL(cli_runner_run_spawned_internal.run_cells_free, CliRunnerRunSpawnedCtx, .run = run);
    run->ended = 1;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &g_cli_runs_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_runs_lock);
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
        char shown[CLI_COMMAND_TEXT];
    } CliRunCommand;
    CliRunCommand *const run_command = g_cli_cells->temporary_buf_alloc(sizeof(CliRunCommand), &g_cli_runs_cells);
    if (run_command == NULL)
    {
        EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = run, .text = "a run's command could not be taken");
        EMBED_CALL(cli_run_end, RunnerRunCtx, .run = run, .has_code_value = 0, .code_value = 0ll, .pages_found = NULL,
                   .pages = 0u, .made_text = NULL);
        return;
    }
    CliCommand *const command = &run_command->command;
    char view_out[CLI_PATH_MAX_LEN];
    VerbaLinea named = {view_out, sizeof(view_out), 0ull};
    mmgr_line_put(&named, run->root);
    mmgr_line_put(&named, "/build/ui/views/");
    mmgr_line_decimal(&named, cli_process_own(), 1u);
    mmgr_line_ch(&named, '-');
    mmgr_line_decimal(&named, run->run, 1u);
    mmgr_line_finish(&named);
    const int views = cellul.eq(&(CatenaFinitaCfg){.src = job->opens, .other = "views", .cap = CLI_CELLS_POOL});
    const int program = cellul.eq(&(CatenaFinitaCfg){.src = job->opens, .other = "program", .cap = CLI_CELLS_POOL});
    if (views)
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = view_out);
    }
    // the steps in turn: the job's own that run with these values, the signing put before the first made program's
    unsigned int ready[CLI_RUN_STEPS + 1u][2];
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < job->steps; at += 1u)
    {
        const CliStep *const step = &catalog->step[job->step_first + at];
        if (EMBED_CALL(cli_runner_command_table.runs, CliRunnerCommandCfg, .catalog = catalog, .step = step,
                       .values = run->values))
        {
            ready[count][0] = (step->program == CLI_PROGRAM_MADE) ? CLI_READY_MADE : CLI_READY_NOW;
            ready[count][1] = job->step_first + at;
            count += 1u;
        }
    }
    if (program && cli_executables_signs())
    {
        unsigned int place = 0u;
        while ((place < count) && (ready[place][0] != CLI_READY_MADE))
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
    int has_code = 1;
    long long code = 0ll;
    int built = 1;
    CliMade made;
    memor.set(&(MemoriaCfg){.dst = &made, .val = 0, .bytes = sizeof(made)});
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (EMBED_CALL(cli_stopping, RunnerRunCtx, .run_read = run))
        {
            break;
        }
        if (ready[at][0] == CLI_READY_SIGN)
        {
            if (!EMBED_CALL(cli_sign, RunnerRunCtx, .run = run, .made = &made, .view_out = view_out,
                            .has_code = &has_code, .code = &code, .built = &built))
            {
                break;
            }
            continue;
        }
        const CliStep *const step = &catalog->step[ready[at][1]];
        VerbaLinea shown = {run_command->shown, CLI_COMMAND_TEXT, 0ull};
        const int made_step = ready[at][0] == CLI_READY_MADE;
        if (made_step)
        {
            if (!EMBED_CALL(cli_find_init, RunnerRunCtx, .run = run, .made = &made))
            {
                has_code = 0;
                break;
            }
            EMBED_CALL(cli_executables_values.run_command, CliExecutablesValuesCfg, .root = run->root,
                       .file = made.file, .values = run->values, .system = step->system, .command = command,
                       .shown_verba_linea_pointer = &shown);
        }
        else
        {
            char why[CLI_PATH_MAX_LEN];
            VerbaLinea err_msg = {why, sizeof(why), 0ull};
            if (!EMBED_CALL(cli_runner_command_table.command, CliRunnerCommandCfg, .root = run->root,
                            .catalog = catalog, .step = step, .values = run->values, .command = command,
                            .shown = &shown, .err_msg = &err_msg))
            {
                mmgr_line_finish(&err_msg);
                EMBED_CALL(cli_run_print_text, RunnerRunCtx, .run = run, .text = why);
                has_code = 0;
                break;
            }
        }
        mmgr_line_finish(&shown);
        double began = 0.0;
        if (!EMBED_CALL(cli_step, RunnerRunCtx, .run = run, .command = command, .shown = run_command->shown,
                        .view_out = view_out, .has_code = &has_code, .code = &code, .step_began = &began))
        {
            built = built && made_step;
            break;
        }
        if (!EMBED_CALL(cli_after_step, RunnerRunCtx, .run = run, .made_step = made_step, .step_began_value = began,
                        .has_code_value = has_code, .code_value = code, .built = &built))
        {
            break;
        }
    }
    g_cli_cells->persistent_buf_release(run_command, &g_cli_runs_cells);
    char (*const pages_found)[CLI_PATH_MAX_LEN] =
        views ? g_cli_cells->persistent_buf_alloc((unsigned long long)CLI_VIEWS_MOST * CLI_PATH_MAX_LEN,
                                                  &g_cli_runs_cells)
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
    char made_text[CLI_PATH_MAX_LEN];
    const int has_made =
        program && built && !stopped && EMBED_CALL(cli_find_init, RunnerRunCtx, .run = run, .made = &made);
    if (has_made)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = run->root, .cap = CLI_CELLS_POOL});
        if (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = made.file, .other_pointer = run->root,
                       .size = length))
        {
            EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = run->root, .path = made.file,
                       .relative_out = made_text);
        }
        else
        {
            memor.cpy(
                &(MemoriaCfg){.dst = made_text,
                              .src = made.file,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = made.file, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    EMBED_CALL(cli_run_end, RunnerRunCtx, .run = run, .has_code_value = has_code, .code_value = code,
               .pages_found = (const char (*)[CLI_PATH_MAX_LEN])pages_found, .pages = pages,
               .made_text = has_made ? made_text : NULL);
    g_cli_cells->persistent_buf_release(pages_found, &g_cli_runs_cells);
}
