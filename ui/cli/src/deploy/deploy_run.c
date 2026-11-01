// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_run.c: a deployment previewed, sent or rolled back: what each place has shown, the files sent to every
// place at once, a machine's release going in beside the one it runs, and every machine turned to the new release
// only once every place has it; a place that fails leaves each on the release it had
#include "../code/git.h"
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

CliDeployRun g_cli_deploy;
CliLock g_cli_deploy_lock = CLI_LOCK_INIT;

// A place's send: its thread, the place's index, whether it sent, and why where it did not
typedef struct
{
    CliThread thread;
    unsigned int index;
    int sent;
    char why[CLI_ERR_MSG_MAX_LEN];
    unsigned long long why_size;
} CliDeploySending;

// the tree deploy.json is read into for a run; each place's send, in the run's pool at the count of the places; and
// the count of the sends still going, with their lock and wake
static CliDeployTree s_tree;
static CliDeploySending *s_sending;
static unsigned int s_going;
static CliLock s_send_lock = CLI_LOCK_INIT;
static CliWake s_send_wake = CLI_WAKE_INIT;
// what each place's send is given
static const char *s_release;
static unsigned long long s_time;
static int s_replace;
static int s_remove;

void cli_deploy_print(const VerbaLinea *line)
{
    g_cli_deploy.print(g_cli_deploy.context, line->out, line->at);
}

// whether the place at `index` takes a release beside the one it runs, as a machine does unless the deployment goes in
// place
static int run_in_release(unsigned int index)
{
    return !g_cli_deploy.deployment.in_place && !g_cli_deploy.place[index].here;
}

// a line printed: `before`, the place's name, `after`
static void run_line_printed(const char *before, unsigned int index, const char *after)
{
    char line[(3u * CLI_PATH_MAX_LEN) + 256u];
    VerbaLinea print_line = {line, sizeof(line), 0ull};
    mmgr_line_put(&print_line, before);
    cli_deploy_place_name(&g_cli_deploy.place[index], &print_line);
    mmgr_line_put(&print_line, after);
    cli_deploy_print(&print_line);
}

// the files sent to one place, on a thread of its own
static void run_send(void *context)
{
    CliDeploySending *const sending = (CliDeploySending *)context;
    VerbaLinea why = {sending->why, sizeof(sending->why), 0ull};
    const int sent = cli_deploy_send_to(sending->index, run_in_release(sending->index), s_release, s_time, s_replace,
                                        s_remove, &why);
    cli_lock_take(&s_send_lock);
    sending->sent = sent;
    sending->why_size = why.at;
    s_going -= 1u;
    cli_wake_all(&s_send_wake);
    cli_lock_give(&s_send_lock);
}

// the deployment's places read, each place's lists and send placed in the run's pool at the count of the places, and
// its patterns set: 1, or 0 with why given
static int run_places(const char *root, VerbaLinea *err_msg)
{
    CliDeployRun *const run = &g_cli_deploy;
    const unsigned int tos = run->deployment.tos;
    if (tos == 0u)
    {
        mmgr_line_put(err_msg, run->deployment.name);
        mmgr_line_put(err_msg, " names no place to send to");
        return 0;
    }
    run->place = cli_deploy_placed(&run->pool, (unsigned long long)tos * sizeof(run->place[0]));
    run->changed_list = cli_deploy_placed(&run->pool, (unsigned long long)tos * sizeof(run->changed_list[0]));
    run->changed = cli_deploy_placed(&run->pool, (unsigned long long)tos * sizeof(run->changed[0]));
    run->gone_list = cli_deploy_placed(&run->pool, (unsigned long long)tos * sizeof(run->gone_list[0]));
    run->gone = cli_deploy_placed(&run->pool, (unsigned long long)tos * sizeof(run->gone[0]));
    s_sending = cli_deploy_placed(&run->pool, (unsigned long long)tos * sizeof(s_sending[0]));
    run->pattern =
        cli_deploy_placed(&run->pool, ((unsigned long long)run->deployment.excludes + 1u) * sizeof(run->pattern[0]));
    if (run->pool.refused)
    {
        mmgr_line_put(err_msg, "deploy.json: to names more than orior_cli supports");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = run->changed_list, .bytes = tos * sizeof(run->changed_list[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = run->changed, .bytes = tos * sizeof(run->changed[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = run->gone_list, .bytes = tos * sizeof(run->gone_list[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = run->gone, .bytes = tos * sizeof(run->gone[0]), .val = 0u});
    memor.set(&(MemoriaCfg){.dst = s_sending, .bytes = tos * sizeof(s_sending[0]), .val = 0u});
    run->places = 0u;
    for (unsigned int at = 0u; at < tos; at += 1u)
    {
        if (!cli_deploy_place(run->deployment.to[at], root, &run->place[run->places], err_msg))
        {
            return 0;
        }
        run->places += 1u;
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
static int run_shown(int act, int replace, int remove, VerbaLinea *err_msg)
{
    CliDeployRun *const run = &g_cli_deploy;
    char line[(4u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea print_buf = {line, sizeof(line), 0ull};
    mmgr_line_decimal(&print_buf, run->sents, 1u);
    mmgr_line_put(&print_buf, " files of ");
    mmgr_line_put(&print_buf, run->deployment.from);
    mmgr_line_put(&print_buf, " go to ");
    mmgr_line_decimal(&print_buf, run->places, 1u);
    mmgr_line_put(&print_buf, " places");
    cli_deploy_print(&print_buf);
    for (unsigned int at = 0u; at < run->outs; at += 1u)
    {
        print_buf.at = 0ull;
        mmgr_line_put(&print_buf, "left out: ");
        mmgr_line_put(&print_buf, run->out[at].path);
        mmgr_line_put(&print_buf, ", by ");
        mmgr_line_put(&print_buf, run->out[at].text);
        cli_deploy_print(&print_buf);
    }
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const int releases = run_in_release(index);
        const char *const base = cli_deploy_place_folder(&run->place[index]);
        char folder[CLI_PATH_MAX_LEN + 16u];
        VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
        mmgr_line_put(&folder_line, base);
        mmgr_line_put(&folder_line, releases ? "/current" : "");
        mmgr_line_finish(&folder_line);
        if (act == CLI_DEPLOY_PREVIEW)
        {
            run_line_printed(
                "", index,
                releases ? " takes a new release, turned to once every place has one"
                         : " takes the files into its folder itself, and retains some of them where it fails part way");
            for (unsigned int at = 0u; at < run->sents; at += 1u)
            {
                print_buf.at = 0ull;
                mmgr_line_put(&print_buf, run->sent[at].path);
                mmgr_line_put(&print_buf, " goes to ");
                mmgr_line_put(&print_buf, base);
                mmgr_line_put(&print_buf, releases ? "/releases/<new>/" : "/");
                mmgr_line_put(&print_buf, run->sent[at].path);
                cli_deploy_print(&print_buf);
            }
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea why_line = {why, sizeof(why) - 1u, 0ull};
        run->changed[index] = 0u;
        run->gone[index] = 0u;
        if (!cli_deploy_standing(index, folder, replace, remove, &why_line))
        {
            why[why_line.at] = '\0';
            VerbaLinea *const into = (act == CLI_DEPLOY_PREVIEW) ? &print_buf : err_msg;
            print_buf.at = 0ull;
            cli_deploy_place_name(&run->place[index], into);
            mmgr_line_put(into, " is not reached: ");
            mmgr_line_put_n(into, why, why_line.at);
            if (act != CLI_DEPLOY_PREVIEW)
            {
                return 0;
            }
            cli_deploy_print(&print_buf);
        }
    }
    return 1;
}

// the files sent to every place at once, each on a thread of its own: 1 where every place took them
static int run_sends(void)
{
    const CliDeployRun *const run = &g_cli_deploy;
    cli_lock_take(&s_send_lock);
    s_going = run->places;
    cli_lock_give(&s_send_lock);
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        s_sending[index].index = index;
        s_sending[index].thread = (CliThread){run_send, &s_sending[index]};
        if (!cli_thread_started(&s_sending[index].thread))
        {
            run_send(&s_sending[index]);
        }
    }
    cli_lock_take(&s_send_lock);
    while (s_going > 0u)
    {
        cli_wake_wait(&s_send_wake, &s_send_lock);
    }
    int every = 1;
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        every = every && s_sending[index].sent;
    }
    cli_lock_give(&s_send_lock);
    return every;
}

// every release sent taken away again where a place failed, and the run failed with each place's reason
static int run_failed(const char *release, VerbaLinea *err_msg)
{
    const CliDeployRun *const run = &g_cli_deploy;
    unsigned int failed = 0u;
    // the words before the places, and each place's name and its reason
    const unsigned long long room =
        ((unsigned long long)run->places * (CLI_ERR_MSG_MAX_LEN + (2ull * CLI_PATH_MAX_LEN) + 8ull)) + 128ull;
    char *const print_text = g_cli_cells->temporary_buf_alloc(room, &g_cli_deploy_cells);
    VerbaLinea print_buf = mmgr_line_over(print_text, room);
    mmgr_line_put(&print_buf, "no machine was turned to the new release, and each runs the release it had: ");
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        if (s_sending[index].sent && run_in_release(index))
        {
            char line[(2u * CLI_PATH_MAX_LEN) + 64u];
            VerbaLinea command = {line, sizeof(line), 0ull};
            char gone[CLI_PATH_MAX_LEN + 64u];
            VerbaLinea gone_line = {gone, sizeof(gone), 0ull};
            mmgr_line_put(&gone_line, run->place[index].address.folder);
            mmgr_line_put(&gone_line, "/releases/");
            mmgr_line_put(&gone_line, release);
            mmgr_line_put(&command, "rm -rf ");
            cli_link_quote(gone, gone_line.at, &command);
            mmgr_line_finish(&command);
            char why[CLI_ERR_MSG_MAX_LEN];
            VerbaLinea none = {why, sizeof(why), 0ull};
            VerbaLinea nothing = {NULL, 0ull, 0ull};
            (void)cli_deploy_remote_run(&run->place[index], line, NULL, 0ull, &nothing, &none);
        }
        if (!s_sending[index].sent)
        {
            mmgr_line_put(&print_buf, (failed > 0u) ? "; " : "");
            cli_deploy_place_name(&run->place[index], &print_buf);
            mmgr_line_put(&print_buf, ": ");
            mmgr_line_put_n(&print_buf, s_sending[index].why, s_sending[index].why_size);
            failed += 1u;
        }
    }
    if (mmgr_line_ok(&print_buf))
    {
        cli_deploy_print(&print_buf);
    }
    g_cli_cells->persistent_buf_release(print_text, &g_cli_deploy_cells);
    mmgr_line_decimal(err_msg, failed, 1u);
    mmgr_line_put(err_msg, " of ");
    mmgr_line_decimal(err_msg, run->places, 1u);
    mmgr_line_put(err_msg, " places failed");
    return 0;
}

// each place turned to the new release, or left containing the files sent, and its record written: 1, or 0 with why
// given
static int run_turned(const char *release, int replace, int remove, VerbaLinea *err_msg)
{
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const CliDeployPlace *const place = &run->place[index];
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea why_line = {why, sizeof(why), 0ull};
        if (run_in_release(index))
        {
            char line[(2u * CLI_PATH_MAX_LEN) + 256u];
            VerbaLinea command = {line, sizeof(line), 0ull};
            cli_deploy_switch_line(place->address.folder, release, &command);
            mmgr_line_finish(&command);
            VerbaLinea nothing = {NULL, 0ull, 0ull};
            if (!cli_deploy_remote_run(place, line, NULL, 0ull, &nothing, &why_line))
            {
                cli_deploy_place_name(place, err_msg);
                mmgr_line_put(err_msg, ": ");
                mmgr_line_put_n(err_msg, why, why_line.at);
                return 0;
            }
            cli_deploy_prune(index, run->deployment.retain);
            char print_buf[64];
            VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ull};
            mmgr_line_put(&print_line, " now runs release ");
            mmgr_line_put(&print_line, release);
            (void)mmgr_line_finish(&print_line);
            run_line_printed("", index, print_buf);
        }
        else
        {
            if (remove && !cli_deploy_removed(index, err_msg))
            {
                return 0;
            }
            run_line_printed("", index, " has the files sent");
        }
        cli_deploy_recorded(index, replace);
    }
    return 1;
}

// every cell of the run given back with the tree it was read from and what its places had, its lists emptied
static void run_given(void)
{
    CliDeployRun *const run = &g_cli_deploy;
    cli_deploy_pool_given(&run->pool);
    cli_deploy_tree_given(&s_tree);
    cli_deploy_there_given();
    run->place = NULL;
    run->places = 0u;
    run->pattern = NULL;
    run->patterns = 0u;
    run->sent = NULL;
    run->sents = 0u;
    run->out = NULL;
    run->outs = 0u;
    run->changed_list = NULL;
    run->changed = NULL;
    run->gone_list = NULL;
    run->gone = NULL;
    s_sending = NULL;
}

int cli_deploy_run(const char *root, const char *name, int act, int replace, int remove, CliDeployPrintFn print,
                   void *context, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_deploy_lock);
    CliDeployRun *const run = &g_cli_deploy;
    run_given();
    run->root = root;
    run->print = print;
    run->context = context;
    unsigned int count = 0u;
    const CliDeployment *deployments = NULL;
    int done = cli_deploy_read(root, &s_tree, &deployments, &count, err_msg);
    unsigned int found = 0u;
    while ((found < count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = deployments[found].name, .other = name, .cap = CLI_CELLS_POOL}))
    {
        found += 1u;
    }
    if (done && (found == count))
    {
        mmgr_line_put(err_msg, "deploy.json names no deployment ");
        mmgr_line_put(err_msg, name);
        done = 0;
    }
    if (done)
    {
        run->deployment = deployments[found];
        done = run_places(root, err_msg);
    }
    if (done && (act == CLI_DEPLOY_ROLL_BACK))
    {
        done = cli_deploy_roll_back(err_msg);
    }
    else if (done)
    {
        done = cli_deploy_gather(err_msg) && run_shown(act, replace, remove, err_msg);
        if (done && (act != CLI_DEPLOY_PREVIEW))
        {
            char commit[CLI_NAME_MAX_LEN];
            long long when = 0;
            int clean = 0;
            const unsigned long long now = cli_wall_milliseconds() / 1000u;
            // a commit's time before 1970 is taken as 1970
            s_time = cli_git_head(root, commit, &when, &clean) ? ((when > 0) ? (unsigned long long)when : 0ull) : now;
            char release[32];
            VerbaLinea release_line = {release, sizeof(release), 0ull};
            cli_deploy_release_name(now, &release_line);
            mmgr_line_finish(&release_line);
            s_release = release;
            s_replace = replace;
            s_remove = remove;
            done = run_sends() ? run_turned(release, replace, remove, err_msg) : run_failed(release, err_msg);
        }
    }
    run_given();
    cli_lock_give(&g_cli_deploy_lock);
    return done;
}
