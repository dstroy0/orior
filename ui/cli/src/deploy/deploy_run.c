// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_run.c: a deployment previewed, sent or rolled back: what each place holds shown, the files sent to every
// place at once, a machine's release going in beside the one it runs, and every machine turned to the new release
// only once every place holds it; a place that fails leaves each on the release it had
#include "../code/git.h"
#include "../command_line/cells.h"
#include "../text/pieces.h"
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
    char why[CLI_SAID_LONGEST];
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

void cli_deploy_say(const CliLine *line)
{
    g_cli_deploy.say(g_cli_deploy.context, line->out, line->at);
}

// whether the place at `index` takes a release beside the one it runs, as a machine does unless the deployment goes in
// place
static int run_in_release(unsigned int index)
{
    return !g_cli_deploy.deployment.in_place && !g_cli_deploy.place[index].here;
}

// a line said: `before`, the place's name, `after`
static void run_said(const char *before, unsigned int index, const char *after)
{
    char line[(3u * CLI_PATH_LONGEST) + 256u];
    CliLine said = {line, sizeof(line), 0ull};
    cli_line_text(&said, before);
    cli_deploy_place_name(&g_cli_deploy.place[index], &said);
    cli_line_text(&said, after);
    cli_deploy_say(&said);
}

// the files sent to one place, on a thread of its own
static void run_send(void *context)
{
    CliDeploySending *const sending = (CliDeploySending *)context;
    CliLine why = {sending->why, sizeof(sending->why), 0ull};
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
// its patterns set: 1, or 0 with why said
static int run_places(const char *root, CliLine *said)
{
    CliDeployRun *const run = &g_cli_deploy;
    const unsigned int tos = run->deployment.tos;
    if (tos == 0u)
    {
        cli_line_text(said, run->deployment.name);
        cli_line_text(said, " names no place to send to");
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
        cli_line_text(said, "deploy.json: to names more than orior holds");
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
        if (!cli_deploy_place(run->deployment.to[at], root, &run->place[run->places], said))
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

// what the run sends said, and what each place holds against it: 1, or 0 with why said where a place is not reached
// and the run sends
static int run_shown(int act, int replace, int remove, CliLine *said)
{
    CliDeployRun *const run = &g_cli_deploy;
    char line[(4u * CLI_PATH_LONGEST) + 128u];
    CliLine told = {line, sizeof(line), 0ull};
    cli_line_decimal(&told, run->sents, 1u);
    cli_line_text(&told, " files of ");
    cli_line_text(&told, run->deployment.from);
    cli_line_text(&told, " go to ");
    cli_line_decimal(&told, run->places, 1u);
    cli_line_text(&told, " places");
    cli_deploy_say(&told);
    for (unsigned int at = 0u; at < run->outs; at += 1u)
    {
        told.at = 0ull;
        cli_line_text(&told, "left out: ");
        cli_line_text(&told, run->out[at].path);
        cli_line_text(&told, ", by ");
        cli_line_text(&told, run->out[at].text);
        cli_deploy_say(&told);
    }
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const int releases = run_in_release(index);
        const char *const base = cli_deploy_place_folder(&run->place[index]);
        char folder[CLI_PATH_LONGEST + 16u];
        CliLine folder_line = {folder, sizeof(folder), 0ull};
        cli_line_text(&folder_line, base);
        cli_line_text(&folder_line, releases ? "/current" : "");
        cli_line_closed(&folder_line);
        if (act == CLI_DEPLOY_PREVIEW)
        {
            run_said("", index,
                     releases
                         ? " takes a new release, turned to once every place holds one"
                         : " takes the files into its folder itself, and holds some of them where it fails part way");
            for (unsigned int at = 0u; at < run->sents; at += 1u)
            {
                told.at = 0ull;
                cli_line_text(&told, run->sent[at].path);
                cli_line_text(&told, " goes to ");
                cli_line_text(&told, base);
                cli_line_text(&told, releases ? "/releases/<new>/" : "/");
                cli_line_text(&told, run->sent[at].path);
                cli_deploy_say(&told);
            }
        }
        char why[CLI_SAID_LONGEST];
        CliLine why_line = {why, sizeof(why) - 1u, 0ull};
        run->changed[index] = 0u;
        run->gone[index] = 0u;
        if (!cli_deploy_standing(index, folder, replace, remove, &why_line))
        {
            why[why_line.at] = '\0';
            CliLine *const into = (act == CLI_DEPLOY_PREVIEW) ? &told : said;
            told.at = 0ull;
            cli_deploy_place_name(&run->place[index], into);
            cli_line_text(into, " is not reached: ");
            cli_line_counted(into, why, why_line.at);
            if (act != CLI_DEPLOY_PREVIEW)
            {
                return 0;
            }
            cli_deploy_say(&told);
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
static int run_failed(const char *release, CliLine *said)
{
    const CliDeployRun *const run = &g_cli_deploy;
    unsigned int failed = 0u;
    char piece[CLI_DEPLOY_PIECE];
    CliPieces pieces;
    CliLine told;
    cli_pieces_begun(&pieces, &told, piece, sizeof(piece));
    cli_line_text(&told, "no machine was turned to the new release, and each runs the release it had: ");
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        if (s_sending[index].sent && run_in_release(index))
        {
            char line[(2u * CLI_PATH_LONGEST) + 64u];
            CliLine command = {line, sizeof(line), 0ull};
            char gone[CLI_PATH_LONGEST + 64u];
            CliLine gone_line = {gone, sizeof(gone), 0ull};
            cli_line_text(&gone_line, run->place[index].address.folder);
            cli_line_text(&gone_line, "/releases/");
            cli_line_text(&gone_line, release);
            cli_line_text(&command, "rm -rf ");
            cli_link_quote(gone, gone_line.at, &command);
            cli_line_closed(&command);
            char why[CLI_SAID_LONGEST];
            CliLine none = {why, sizeof(why), 0ull};
            CliLine nothing = {NULL, 0ull, 0ull};
            (void)cli_deploy_ask(&run->place[index], line, NULL, 0ull, &nothing, &none);
        }
        if (!s_sending[index].sent)
        {
            cli_line_text(&told, (failed > 0u) ? "; " : "");
            cli_deploy_place_name(&run->place[index], &told);
            cli_line_text(&told, ": ");
            cli_line_counted(&told, s_sending[index].why, s_sending[index].why_size);
            failed += 1u;
        }
    }
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(&pieces, &told, "a deployment's failure", &size);
    if (joined != NULL)
    {
        const CliLine whole = {joined, size, size};
        cli_deploy_say(&whole);
    }
    cli_cell_given(joined);
    cli_line_decimal(said, failed, 1u);
    cli_line_text(said, " of ");
    cli_line_decimal(said, run->places, 1u);
    cli_line_text(said, " places failed");
    return 0;
}

// each place turned to the new release, or left holding the files sent, and its record written: 1, or 0 with why said
static int run_turned(const char *release, int replace, int remove, CliLine *said)
{
    const CliDeployRun *const run = &g_cli_deploy;
    for (unsigned int index = 0u; index < run->places; index += 1u)
    {
        const CliDeployPlace *const place = &run->place[index];
        char why[CLI_SAID_LONGEST];
        CliLine why_line = {why, sizeof(why), 0ull};
        if (run_in_release(index))
        {
            char line[(2u * CLI_PATH_LONGEST) + 256u];
            CliLine command = {line, sizeof(line), 0ull};
            cli_deploy_switch_line(place->address.folder, release, &command);
            cli_line_closed(&command);
            CliLine nothing = {NULL, 0ull, 0ull};
            if (!cli_deploy_ask(place, line, NULL, 0ull, &nothing, &why_line))
            {
                cli_deploy_place_name(place, said);
                cli_line_text(said, ": ");
                cli_line_counted(said, why, why_line.at);
                return 0;
            }
            cli_deploy_prune(index, run->deployment.keep);
            char told[64];
            CliLine told_line = {told, sizeof(told), 0ull};
            cli_line_text(&told_line, " now runs release ");
            cli_line_text(&told_line, release);
            run_said("", index, told);
        }
        else
        {
            if (remove && !cli_deploy_removed(index, said))
            {
                return 0;
            }
            run_said("", index, " holds the files sent");
        }
        cli_deploy_recorded(index, replace);
    }
    return 1;
}

// every cell of the run given back with the tree it was read from and what its places held, its lists emptied
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

int cli_deploy_run(const char *root, const char *name, int act, int replace, int remove, CliDeploySay say,
                   void *context, CliLine *said)
{
    cli_lock_take(&g_cli_deploy_lock);
    CliDeployRun *const run = &g_cli_deploy;
    run_given();
    run->root = root;
    run->say = say;
    run->context = context;
    unsigned int count = 0u;
    const CliDeployment *deployments = NULL;
    int done = cli_deploy_read(root, &s_tree, &deployments, &count, said);
    unsigned int found = 0u;
    while ((found < count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = deployments[found].name, .other = name, .cap = CLI_CELLS_POOL}))
    {
        found += 1u;
    }
    if (done && (found == count))
    {
        cli_line_text(said, "deploy.json names no deployment ");
        cli_line_text(said, name);
        done = 0;
    }
    if (done)
    {
        run->deployment = deployments[found];
        done = run_places(root, said);
    }
    if (done && (act == CLI_DEPLOY_ROLL_BACK))
    {
        done = cli_deploy_roll_back(said);
    }
    else if (done)
    {
        done = cli_deploy_gather(said) && run_shown(act, replace, remove, said);
        if (done && (act != CLI_DEPLOY_PREVIEW))
        {
            char commit[CLI_NAME_LONGEST];
            long long when = 0;
            int clean = 0;
            const unsigned long long now = cli_wall_milliseconds() / 1000u;
            // a commit's time before 1970 is taken as 1970
            s_time = cli_git_head(root, commit, &when, &clean) ? ((when > 0) ? (unsigned long long)when : 0ull) : now;
            char release[32];
            CliLine release_line = {release, sizeof(release), 0ull};
            cli_deploy_release_name(now, &release_line);
            cli_line_closed(&release_line);
            s_release = release;
            s_replace = replace;
            s_remove = remove;
            done = run_sends() ? run_turned(release, replace, remove, said) : run_failed(release, said);
        }
    }
    run_given();
    cli_lock_give(&g_cli_deploy_lock);
    return done;
}
