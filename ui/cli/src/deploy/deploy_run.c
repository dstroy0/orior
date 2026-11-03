// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_run.c: a deployment previewed, sent or rolled back: what each place has shown, the files sent to every
// place at once, a machine's release going in beside the one it runs, and every machine turned to the new release
// only once every place has it; a place that fails leaves each on the release it had
#include "../code/git.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

CliDeployRun g_cli_deploy;

// A place's send: its thread, the place's index, whether it sent, and why where it did not
typedef struct
{
    CliThread thread;
    unsigned int index;
    int sent;
    char why[CLI_ERR_MSG_MAX_LEN];
    unsigned long long error_size;
} CliDeploySending;

// the tree deploy.json is read into for a run; each place's send, in the run's pool at the count of the places; and
// the count of the sends still going, with their lock and wake
static CliDeployTree s_tree;
static CliDeploySending *s_sending;
static unsigned int s_going;
static CliWake s_send_wake = CLI_WAKE_INIT;
// what each place's send is given
static const char *s_release;
static unsigned long long s_time;
static int s_replace;
static int s_remove;

void cli_deploy_print(const CliDeployCtx *args)
{
    EMBED_CALL(g_cli_deploy.print, CliEachLineSizeCfg, .context = g_cli_deploy.context, .line = args->line->out,
               .size = args->line->at);
}

typedef struct
{
    unsigned int index;
    const char *before;
    const char *after;
    const char *root;
    VerbaLinea *err_msg;
    int act;
    int replace;
    int remove;
    const char *release;
} DeployRunCtx;
// whether the place at `index` takes a release beside the one it runs, as a machine does unless the deployment goes in
// place
EMBED_INLINE int run_in_release(const DeployRunCtx *args)
{
    return !g_cli_deploy.deployment.in_position && !g_cli_deploy.place[args->index].here;
}

// a line printed: `before`, the place's name, `after`
EMBED_INLINE void run_line_print(const DeployRunCtx *args)
{
    char line[(3u * CLI_PATH_MAX_LEN) + 256u];
    VerbaLinea print_line = {line, sizeof(line), 0ULL};
    mmgr_line_put(&print_line, args->before);
    EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = &g_cli_deploy.place[args->index],
               .out = &print_line);
    mmgr_line_put(&print_line, args->after);
    EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_line);
}

// the files sent to one place, on a thread of its own
static void run_send(void *context)
{
    CliDeploySending *const sending = (CliDeploySending *)context;
    VerbaLinea why = {sending->why, sizeof(sending->why), 0ULL};
    const int sent =
        EMBED_CALL(cli_deploy_err_msg_internal.send_to, CliDeployErrMsgCtx, .index = sending->index,
                   .releases = EMBED_CALL(run_in_release, DeployRunCtx, .index = sending->index), .release = s_release,
                   .time = s_time, .replace = s_replace, .remove = s_remove, .err_msg = &why);
    sending->sent = sent;
    sending->error_size = why.at;
    s_going -= 1u;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_send_wake);
}

// the deployment's places read, each place's lists and send placed in the run's pool at the count of the places, and
// its patterns set: 1, or 0 with why given
EMBED_INLINE int run_positions(const DeployRunCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    CliDeployRun *const run = &g_cli_deploy;
    const unsigned int tos = run->deployment.tos;
    if (tos == 0u)
    {
        mmgr_line_put(err_msg, run->deployment.name);
        mmgr_line_put(err_msg, " names no place to send to");
        return 0;
    }
    run->place = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                            .size = (unsigned long long)tos * sizeof(run->place[0]));
    run->changed_list = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                                   .size = (unsigned long long)tos * sizeof(run->changed_list[0]));
    run->changed = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                              .size = (unsigned long long)tos * sizeof(run->changed[0]));
    run->removed_list = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                                   .size = (unsigned long long)tos * sizeof(run->removed_list[0]));
    run->deleted = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                              .size = (unsigned long long)tos * sizeof(run->deleted[0]));
    s_sending = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                           .size = (unsigned long long)tos * sizeof(s_sending[0]));
    run->pattern = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = &run->pool,
                              .size = ((unsigned long long)run->deployment.excludes + 1u) * sizeof(run->pattern[0]));
    if (run->pool.refused)
    {
        mmgr_line_put(err_msg, "deploy.json: to names more than orior_cli supports");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = run->changed_list, .bytes = tos * sizeof(run->changed_list[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = run->changed, .bytes = tos * sizeof(run->changed[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = run->removed_list, .bytes = tos * sizeof(run->removed_list[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = run->deleted, .bytes = tos * sizeof(run->deleted[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = s_sending, .bytes = tos * sizeof(s_sending[0]), .val = 0u});
    run->positions = 0u;
    for (unsigned int at = 0u; at < tos; at += 1u)
    {
        if (!EMBED_CALL(cli_deploy_text_internal.place, CliDeployTextCtx, .text = run->deployment.to[at], .root = root,
                        .position_out = &run->place[run->positions], .err_msg = err_msg))
        {
            return 0;
        }
        run->positions += 1u;
    }
    run->pattern[0] = ".git/";
    run->patterns = 1u;
    for (unsigned int at = 0u; at < run->deployment.excludes; at += 1u)
    {
        run->pattern[run->patterns] = run->deployment.exclude[at];
        run->patterns += 1u;
    }
    return 1;
}

// what the run sends printed, and what each place has against it: 1, or 0 with why given where a place is not reached
// and the run sends
EMBED_INLINE int run_show(const DeployRunCtx *args)
{
    int replace = args->replace;
    int remove = args->remove;
    VerbaLinea *err_msg = args->err_msg;
    CliDeployRun *const run = &g_cli_deploy;
    char line[(4u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea print_buf = {line, sizeof(line), 0ULL};
    mmgr_line_decimal(&print_buf, run->sents, 1u);
    mmgr_line_put(&print_buf, " files of ");
    mmgr_line_put(&print_buf, run->deployment.from);
    mmgr_line_put(&print_buf, " go to ");
    mmgr_line_decimal(&print_buf, run->positions, 1u);
    mmgr_line_put(&print_buf, " places");
    EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_buf);
    for (unsigned int at = 0u; at < run->outs; at += 1u)
    {
        print_buf.at = 0ULL;
        mmgr_line_put(&print_buf, "left out: ");
        mmgr_line_put(&print_buf, run->out[at].path);
        mmgr_line_put(&print_buf, ", by ");
        mmgr_line_put(&print_buf, run->out[at].text);
        EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_buf);
    }
    for (unsigned int index = 0u; index < run->positions; index += 1u)
    {
        const int releases = EMBED_CALL(run_in_release, DeployRunCtx, .index = index);
        const char *const base =
            EMBED_CALL(cli_deploy_size_internal.position_folder, CliDeploySizeCtx, .place = &run->place[index]);
        char folder[CLI_PATH_MAX_LEN + 16u];
        VerbaLinea folder_line = {folder, sizeof(folder), 0ULL};
        mmgr_line_put(&folder_line, base);
        mmgr_line_put(&folder_line, releases ? "/current" : "");
        mmgr_line_finish(&folder_line);
        if (args->act == CLI_DEPLOY_PREVIEW)
        {
            EMBED_CALL(
                run_line_print, DeployRunCtx, .before = "", .index = index,
                .after =
                    releases
                        ? " takes a new release, turned to once every place has one"
                        : " takes the files into its folder itself, and retains some of them where it fails part way");
            for (unsigned int at = 0u; at < run->sents; at += 1u)
            {
                print_buf.at = 0ULL;
                mmgr_line_put(&print_buf, run->sent[at].path);
                mmgr_line_put(&print_buf, " goes to ");
                mmgr_line_put(&print_buf, base);
                mmgr_line_put(&print_buf, releases ? "/releases/<new>/" : "/");
                mmgr_line_put(&print_buf, run->sent[at].path);
                EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_buf);
            }
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea error_line = {why, sizeof(why) - 1u, 0ULL};
        run->changed[index] = 0u;
        run->deleted[index] = 0u;
        if (!EMBED_CALL(cli_deploy_err_msg_index_internal.existing, CliDeployErrMsgIndexCtx, .index = index,
                        .folder = folder, .replace = replace, .remove = remove, .err_msg = &error_line))
        {
            why[error_line.at] = '\0';
            VerbaLinea *const into = (args->act == CLI_DEPLOY_PREVIEW) ? &print_buf : err_msg;
            print_buf.at = 0ULL;
            EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = &run->place[index],
                       .out = into);
            mmgr_line_put(into, " is not reached: ");
            mmgr_line_put_n(into, why, error_line.at);
            if (args->act != CLI_DEPLOY_PREVIEW)
            {
                return 0;
            }
            EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_buf);
        }
    }
    return 1;
}

// the files sent to every place at once, each on a thread of its own: 1 where every place took them
static int run_sends(void)
{
    const CliDeployRun *const run = &g_cli_deploy;
    s_going = run->positions;
    for (unsigned int index = 0u; index < run->positions; index += 1u)
    {
        s_sending[index].index = index;
        s_sending[index].thread = (CliThread){run_send, &s_sending[index]};
        if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_sending[index].thread))
        {
            run_send(&s_sending[index]);
        }
    }
    while (s_going > 0u)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &s_send_wake);
    }
    int every = 1;
    for (unsigned int index = 0u; index < run->positions; index += 1u)
    {
        every = every && s_sending[index].sent;
    }
    return every;
}

// every release sent taken away again where a place failed, and the run failed with each place's reason
EMBED_INLINE int run_fail(const DeployRunCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    const CliDeployRun *const run = &g_cli_deploy;
    unsigned int failed = 0u;
    // the words before the places, and each place's name and its reason
    const unsigned long long capacity =
        ((unsigned long long)run->positions * (CLI_ERR_MSG_MAX_LEN + (2ULL * CLI_PATH_MAX_LEN) + 8ULL)) + 128ULL;
    char *const print_text = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_deploy_cells);
    VerbaLinea print_buf = mmgr_line_over(print_text, capacity);
    mmgr_line_put(&print_buf, "no machine was turned to the new release, and each runs the release it had: ");
    for (unsigned int index = 0u; index < run->positions; index += 1u)
    {
        if (s_sending[index].sent && EMBED_CALL(run_in_release, DeployRunCtx, .index = index))
        {
            char line[(2u * CLI_PATH_MAX_LEN) + 64u];
            VerbaLinea command = {line, sizeof(line), 0ULL};
            char deleted[CLI_PATH_MAX_LEN + 64u];
            VerbaLinea removed_line = {deleted, sizeof(deleted), 0ULL};
            mmgr_line_put(&removed_line, run->place[index].address.folder);
            mmgr_line_put(&removed_line, "/releases/");
            mmgr_line_put(&removed_line, args->release);
            mmgr_line_put(&command, "rm -rf ");
            EMBED_CALL(cli_link_text_size.quote, CliLinkTextSizeCfg, .text = deleted, .size = removed_line.at,
                       .out = &command);
            mmgr_line_finish(&command);
            char why[CLI_ERR_MSG_MAX_LEN];
            VerbaLinea none = {why, sizeof(why), 0ULL};
            VerbaLinea nothing = {NULL, 0ULL, 0ULL};
            (void)EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx, .place = &run->place[index],
                             .line_text = line, .input = NULL, .size = 0ULL, .out = &nothing, .err_msg = &none);
        }
        if (!s_sending[index].sent)
        {
            mmgr_line_put(&print_buf, (failed > 0u) ? "; " : "");
            EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = &run->place[index],
                       .out = &print_buf);
            mmgr_line_put(&print_buf, ": ");
            mmgr_line_put_n(&print_buf, s_sending[index].why, s_sending[index].error_size);
            failed += 1u;
        }
    }
    if (mmgr_line_ok(&print_buf))
    {
        EMBED_CALL(cli_deploy_internal.print, CliDeployCtx, .line = &print_buf);
    }
    g_cli_cells->persistent_buf_release(print_text, &g_cli_deploy_cells);
    mmgr_line_decimal(err_msg, failed, 1u);
    mmgr_line_put(err_msg, " of ");
    mmgr_line_decimal(err_msg, run->positions, 1u);
    mmgr_line_put(err_msg, " places failed");
    return 0;
}

// each place turned to the new release, or left containing the files sent, and its record written: 1, or 0 with why
// given
EMBED_INLINE int run_turn(const DeployRunCtx *args)
{
    const char *release = args->release;
    int replace = args->replace;
    VerbaLinea *err_msg = args->err_msg;
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int index = 0u; index < run->positions; index += 1u)
    {
        const CliDeployPosition *const place = &run->place[index];
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea error_line = {why, sizeof(why), 0ULL};
        if (EMBED_CALL(run_in_release, DeployRunCtx, .index = index))
        {
            char line[(2u * CLI_PATH_MAX_LEN) + 256u];
            VerbaLinea command = {line, sizeof(line), 0ULL};
            EMBED_CALL(cli_deploy_out_internal.switch_line, CliDeployOutCtx, .folder = place->address.folder,
                       .release = release, .out = &command);
            mmgr_line_finish(&command);
            VerbaLinea nothing = {NULL, 0ULL, 0ULL};
            if (!EMBED_CALL(cli_deploy_size_internal.remote_run, CliDeploySizeCtx, .place = place, .line_text = line,
                            .input = NULL, .size = 0ULL, .out = &nothing, .err_msg = &error_line))
            {
                EMBED_CALL(cli_deploy_size_internal.position_name, CliDeploySizeCtx, .place = place, .out = err_msg);
                mmgr_line_put(err_msg, ": ");
                mmgr_line_put_n(err_msg, why, error_line.at);
                return 0;
            }
            EMBED_CALL(cli_deploy_index_internal.prune, CliDeployIndexCtx, .index = index,
                       .retain = run->deployment.retain);
            char print_buf[64];
            VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ULL};
            mmgr_line_put(&print_line, " now runs release ");
            mmgr_line_put(&print_line, release);
            (void)mmgr_line_finish(&print_line);
            EMBED_CALL(run_line_print, DeployRunCtx, .before = "", .index = index, .after = print_buf);
        }
        else
        {
            if (args->remove && !EMBED_CALL(cli_deploy_err_msg_index_internal.remove, CliDeployErrMsgIndexCtx,
                                            .index = index, .err_msg = err_msg))
            {
                return 0;
            }
            EMBED_CALL(run_line_print, DeployRunCtx, .before = "", .index = index, .after = " has the files sent");
        }
        EMBED_CALL(cli_deploy_err_msg_index_internal.record, CliDeployErrMsgIndexCtx, .index = index,
                   .replace = replace);
    }
    return 1;
}

// every cell of the run given back with the tree it was read from and what its places had, its lists emptied
static void run_free(void)
{
    CliDeployRun *const run = &g_cli_deploy;
    EMBED_CALL(cli_deploy_pool_internal.pool_free, CliDeployPoolCtx, .pool = &run->pool);
    EMBED_CALL(cli_deploy_root_internal.tree_free, CliDeployRootCtx, .tree = &s_tree);
    cli_deploy_there_free();
    run->place = NULL;
    run->positions = 0u;
    run->pattern = NULL;
    run->patterns = 0u;
    run->sent = NULL;
    run->sents = 0u;
    run->out = NULL;
    run->outs = 0u;
    run->changed_list = NULL;
    run->changed = NULL;
    run->removed_list = NULL;
    run->deleted = NULL;
    s_sending = NULL;
}

int cli_deploy_run(const CliDeployRootCfg *args)
{
    const char *root = args->root;
    CliDeployPrintFn print = args->print;
    void *context = args->context;
    CliDeployRun *const run = &g_cli_deploy;
    run_free();
    run->root = root;
    run->print = print;
    run->context = context;
    unsigned int count = 0u;
    const CliDeployment *deployments = NULL;
    int done = EMBED_CALL(cli_deploy_root_internal.read, CliDeployRootCtx, .root = root, .tree = &s_tree,
                          .deployments = &deployments, .count_pointer = &count, .err_msg = args->err_msg);
    unsigned int found = 0u;
    while ((found < count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = deployments[found].name, .other = args->name, .cap = CLI_CELLS_POOL}))
    {
        found += 1u;
    }
    if (done && (found == count))
    {
        mmgr_line_put(args->err_msg, "deploy.json names no deployment ");
        mmgr_line_put(args->err_msg, args->name);
        done = 0;
    }
    if (done)
    {
        run->deployment = deployments[found];
        done = EMBED_CALL(run_positions, DeployRunCtx, .root = root, .err_msg = args->err_msg);
    }
    if (done && (args->act == CLI_DEPLOY_ROLL_BACK))
    {
        done = EMBED_CALL(cli_deploy_text_internal.roll_back, CliDeployTextCtx, .err_msg = args->err_msg);
    }
    else if (done)
    {
        done = EMBED_CALL(cli_deploy_text_internal.gather, CliDeployTextCtx, .err_msg = args->err_msg) &&
               EMBED_CALL(run_show, DeployRunCtx, .act = args->act, .replace = args->replace, .remove = args->remove,
                          .err_msg = args->err_msg);
        if (done && (args->act != CLI_DEPLOY_PREVIEW))
        {
            char commit[CLI_NAME_MAX_LEN];
            long long when = 0;
            int clean = 0;
            const unsigned long long now = cli_wall_milliseconds() / 1000u;
            // a commit's time before 1970 is taken as 1970
            s_time = EMBED_CALL(cli_git_root_commit.head, CliGitRootCommitCfg, .root = root, .commit = commit,
                                .when = &when, .clean = &clean)
                         ? ((when > 0) ? (unsigned long long)when : 0ULL)
                         : now;
            char release[32];
            VerbaLinea release_line = {release, sizeof(release), 0ULL};
            EMBED_CALL(cli_deploy_out_seconds_internal.release_name, CliDeployOutSecondsCtx, .seconds = now,
                       .out = &release_line);
            mmgr_line_finish(&release_line);
            s_release = release;
            s_replace = args->replace;
            s_remove = args->remove;
            done = run_sends() ? EMBED_CALL(run_turn, DeployRunCtx, .release = release, .replace = args->replace,
                                            .remove = args->remove, .err_msg = args->err_msg)
                               : EMBED_CALL(run_fail, DeployRunCtx, .release = release, .err_msg = args->err_msg);
        }
    }
    run_free();
    return done;
}
