// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// runner_run.c: a job's steps run one after another on a thread of their own, each step's process started, its lines
// carried and its exit read; the program a build made found, signed and run; and a run stopped with all it started
#include "../command_line/cells.h"
#include "../programs/binary.h"
#include "../programs/executables.h"
#include "../toolchains/toolchains.h"
#include "runner_internal.h"

#include "../text/text.h"
#include <math.h>

// the runs open, the next run's number, whether runs share this process's console, and the lock and wake over them
CliRun g_cli_runs[CLI_RUNS_MOST];
unsigned long long g_cli_next_run;
static int s_attached;
CliLock g_cli_runs_lock = CLI_LOCK_INIT;
CliWake g_cli_runs_wake = CLI_WAKE_INIT;

void cli_runs_take(void)
{
    cli_lock_take(&g_cli_runs_lock);
}

void cli_runs_give(void)
{
    cli_lock_give(&g_cli_runs_lock);
}

void cli_runs_attached(void)
{
    cli_lock_take(&g_cli_runs_lock);
    s_attached = 1;
    cli_lock_give(&g_cli_runs_lock);
}

double cli_run_since(const CliRun *run)
{
    // the clock that only goes forward, read to the millisecond
    return (double)(cli_clock_milliseconds() - run->began);
}

// whether a reader has stopped the run
static int cli_stopping(const CliRun *run)
{
    cli_lock_take(&g_cli_runs_lock);
    const int stopped = run->stopped;
    cli_lock_give(&g_cli_runs_lock);
    return stopped;
}

// A line the run prints of its own, on its errors
static void cli_run_print(void *context, const char *text, unsigned long long length)
{
    char line[CLI_PATH_MAX_LEN * 4u];
    VerbaLinea line_out = {line, sizeof(line), 0ull};
    mmgr_line_put_n(&line_out, text, length);
    mmgr_line_finish(&line_out);
    cli_run_line((CliRun *)context, "stderr", line);
}

static void cli_run_print_text(CliRun *run, const char *text)
{
    cli_run_print(run, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// The program the build made, found once from the lines the steps before printed, and what it loads and how it compares
// reported as it is found: 1, its path in `file`, or 0 with why given into `why`
typedef struct
{
    int looked;
    int found;
    char file[CLI_PATH_MAX_LEN];
    char why[CLI_PATH_MAX_LEN];
} CliMade;

static int cli_find_made(CliRun *run, CliMade *made)
{
    const char *const id = cli_catalog_at(&run->catalog, run->catalog.job[0].id);
    const QuerySink sink = {cli_run_print, run};
    if (!made->looked)
    {
        made->looked = 1;
        VerbaLinea why = {made->why, sizeof(made->why), 0ull};
        made->found = cli_executables_made(run->root, id, run->values, run->lines, made->file, &why);
        mmgr_line_finish(&why);
        if (made->found)
        {
            static char s_path[1u << 16u];
            static char s_folders[256][CLI_PATH_MAX_LEN];
            cli_toolchains_run_path(s_path, sizeof(s_path));
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
            cli_binary_print(made->file, (const char (*)[CLI_PATH_MAX_LEN])s_folders, count, sink);
            cli_executables_built_advice(run->root, id, run->lines, sink);
            cli_executables_compare_build(run->root, id, made->file, sink);
        }
        else
        {
            cli_run_print_text(run, made->why);
        }
    }
    return made->found;
}

// One step's command run: its line printed, its process started in its own group where the run is detached, its lines
// carried, its exit read into `code` and when it began into `step_began`: 1 where it ran, 0 where it could not start or
// was stopped first
static int cli_step(CliRun *run, CliCommand *command, const char *shown, const char *view_out, int *has_code,
                    long long *code, double *step_began)
{
    static char s_path[1u << 16u];
    const CliRunEvent line = {CLI_RUN_EVENT_LINE, run->run, NULL, "command", shown, 0, 0ll, 0, NULL, 0u, NULL,
                              cli_run_since(run)};
    run->sink.emit(run->sink.context, &line);
    if (cellul.eq(&(CatenaFinitaCfg){.src = run->catalog.job[0].opens, .other = "views", .cap = CLI_CELLS_POOL}))
    {
        query_folder_made(view_out);
        cli_command_set(command, "VIEW_OUT", view_out);
    }
    cli_command_set(command, "PYTHONUNBUFFERED", "1");
    cli_command_set(command, "PYTHONIOENCODING", "utf-8");
    if (!cli_command_sets(command, "PATH"))
    {
        cli_toolchains_run_path(s_path, sizeof(s_path));
        cli_command_set(command, "PATH", s_path);
    }
    char number[24];
    VerbaLinea named = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&named, run->run, 1u);
    mmgr_line_finish(&named);
    // the run's number in the step's environment: a command that would reach a window open on the tree sees it is a
    // job's own and runs where it is
    cli_command_set(command, CLI_RUN_VARIABLE, number);
    *step_began = cli_run_since(run);
    // the run is looked at last, and the process joined to it, under one acquisition of the lock: Stop, which marks the
    // run and then reads it, either finds the process there or prevents it from starting
    cli_lock_take(&g_cli_runs_lock);
    if (run->stopped)
    {
        cli_lock_give(&g_cli_runs_lock);
        return 0;
    }
    CliSpawned spawned;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int started = cli_spawn(&spawned, command->words.word, command->has_folder ? command->folder : NULL,
                                  command->setting, command->settings, !s_attached, &err_msg);
    if (started)
    {
        run->process = spawned.id;
        run->group = spawned.group;
        run->has_first = 0;
    }
    cli_lock_give(&g_cli_runs_lock);
    if (!started)
    {
        mmgr_line_finish(&err_msg);
        char line_text[CLI_PATH_MAX_LEN + 32u];
        VerbaLinea could = {line_text, sizeof(line_text), 0ull};
        mmgr_line_put(&could, "could not start: ");
        mmgr_line_put(&could, why);
        mmgr_line_finish(&could);
        cli_run_print_text(run, line_text);
        *has_code = 0;
        return 0;
    }
    cli_run_carry(run, &spawned);
    const long long exit_code = cli_spawn_wait(&spawned);
    cli_lock_take(&g_cli_runs_lock);
    run->process = 0ull;
    run->group = 0ull;
    cli_lock_give(&g_cli_runs_lock);
    cli_spawn_closed(&spawned);
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
static int cli_after_step(CliRun *run, int made_step, double step_began, int has_code, long long code, int *built)
{
    const char *const id = cli_catalog_at(&run->catalog, run->catalog.job[0].id);
    if (made_step)
    {
        if (run->has_first)
        {
            char line[96];
            VerbaLinea line_out = {line, sizeof(line), 0ull};
            mmgr_line_put(&line_out, "the program wrote its first line ");
            const double after = run->first - step_began;
            // a span in milliseconds, rounded, is far inside what a long long represents
            mmgr_line_decimal_signed(&line_out, (long long)llround(after));
            mmgr_line_put(&line_out, " ms after it started");
            mmgr_line_finish(&line_out);
            cli_run_print_text(run, line);
        }
        const QuerySink sink = {cli_run_print, run};
        cli_executables_ran_advice(run->root, id, has_code, code, run->lines, sink);
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
static int cli_signed(CliRun *run, CliMade *made, const char *view_out, int *has_code, long long *code, int *built)
{
    if (!cli_find_made(run, made))
    {
        *has_code = 0;
        *built = 0;
        return 0;
    }
    cli_lock_take(&s_signing_lock);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int steps = cli_executables_sign_steps(made->file, s_signing, s_signing_shown, 4u, &err_msg);
    if (steps < 0)
    {
        cli_lock_give(&s_signing_lock);
        mmgr_line_finish(&err_msg);
        cli_run_print_text(run, why);
        *has_code = 0;
        *built = 0;
        return 0;
    }
    int goes = 1;
    for (int at = 0; goes && (at < steps); at += 1)
    {
        double began = 0.0;
        if (cli_stopping(run) || !cli_step(run, &s_signing[at], s_signing_shown[at], view_out, has_code, code, &began))
        {
            *has_code = cli_stopping(run) ? *has_code : 0;
            goes = 0;
            break;
        }
        goes = cli_after_step(run, 0, began, *has_code, *code, built);
    }
    cli_lock_give(&s_signing_lock);
    if (goes)
    {
        const QuerySink sink = {cli_run_print, run};
        cli_executables_signature_print(made->file, sink);
    }
    return goes;
}

// a run's end reported, with the code of its last step, the pages it wrote and the program it made, its cells given
// back and its end reported to those waiting on it
static void cli_run_ended(CliRun *run, int has_code, long long code, const char (*pages_found)[CLI_PATH_MAX_LEN],
                          unsigned int pages, const char *made)
{
    const CliRunEvent end = {
        CLI_RUN_EVENT_END, run->run, NULL, NULL, NULL, has_code, code, cli_stopping(run), pages_found, pages, made,
        cli_run_since(run)};
    run->sink.emit(run->sink.context, &end);
    cli_lock_take(&g_cli_runs_lock);
    cli_run_cells_given(run);
    run->ended = 1;
    cli_wake_all(&g_cli_runs_wake);
    cli_lock_give(&g_cli_runs_lock);
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
        cli_run_print_text(run, "a run's command could not be taken");
        cli_run_ended(run, 0, 0ll, NULL, 0u, NULL);
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
        query_removed(view_out);
    }
    // the steps in turn: the job's own that run with these values, the signing put before the first made program's
    unsigned int ready[CLI_RUN_STEPS + 1u][2];
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < job->steps; at += 1u)
    {
        const CliStep *const step = &catalog->step[job->step_first + at];
        if (cli_runner_runs(catalog, step, run->values))
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
        if (cli_stopping(run))
        {
            break;
        }
        if (ready[at][0] == CLI_READY_SIGN)
        {
            if (!cli_signed(run, &made, view_out, &has_code, &code, &built))
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
            if (!cli_find_made(run, &made))
            {
                has_code = 0;
                break;
            }
            cli_executables_run_command(run->root, made.file, run->values, step->system, command, &shown);
        }
        else
        {
            char why[CLI_PATH_MAX_LEN];
            VerbaLinea err_msg = {why, sizeof(why), 0ull};
            if (!cli_runner_command(run->root, catalog, step, run->values, command, &shown, &err_msg))
            {
                mmgr_line_finish(&err_msg);
                cli_run_print_text(run, why);
                has_code = 0;
                break;
            }
        }
        mmgr_line_finish(&shown);
        double began = 0.0;
        if (!cli_step(run, command, run_command->shown, view_out, &has_code, &code, &began))
        {
            built = built && made_step;
            break;
        }
        if (!cli_after_step(run, made_step, began, has_code, code, &built))
        {
            break;
        }
    }
    g_cli_cells->persistent_buf_release(run_command, &g_cli_runs_cells);
    char (*const pages_found)[CLI_PATH_MAX_LEN] =
        views ? g_cli_cells->persistent_buf_alloc((unsigned long long)CLI_VIEWS_MOST * CLI_PATH_MAX_LEN,
                                                  &g_cli_runs_cells)
              : NULL;
    const unsigned int pages = (pages_found != NULL) ? cli_run_pages(run->root, view_out, run->lines, pages_found) : 0u;
    const int stopped = cli_stopping(run);
    const char *const id = cli_catalog_at(catalog, job->id);
    if (program && !built && !stopped)
    {
        const QuerySink sink = {cli_run_print, run};
        cli_executables_advice(id, run->lines, sink);
    }
    char made_text[CLI_PATH_MAX_LEN];
    const int has_made = program && built && !stopped && cli_find_made(run, &made);
    if (has_made)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = run->root, .cap = CLI_CELLS_POOL});
        if (cli_text_same(made.file, run->root, length))
        {
            cli_relative(run->root, made.file, made_text);
        }
        else
        {
            memor.cpy(
                &(MemoriaCfg){.dst = made_text,
                              .src = made.file,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = made.file, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    cli_run_ended(run, has_code, code, (const char (*)[CLI_PATH_MAX_LEN])pages_found, pages,
                  has_made ? made_text : NULL);
    g_cli_cells->persistent_buf_release(pages_found, &g_cli_runs_cells);
}
