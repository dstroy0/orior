// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_file.c: a report filed: through the reporter's GitHub CLI where it is signed in, as nothing where its error is
// already an open issue, and otherwise as the new-issue page filled in, opened in the browser
#include "../text/text.h"
#include "net.h"
#include "report_internal.h"

// the most reports a run files on its own, and the bytes a struck field of a report is begun in before it grows
#define CLI_REPORT_AUTO 5u

#define CLI_REPORT_FIELD_FIRST (4ull << 10u)

// the fingerprints of the errors this run filed, under the lock
static char s_filed[CLI_REPORT_AUTO][17];
static unsigned int s_filed_count;
static CliLock s_filed_lock = CLI_LOCK_INIT;

// the GitHub CLI run with `args`, `count` of them, `input` on its standard input where it is given: what it wrote,
// trimmed, into `out`, sized for `room`, 1 where it ends well, or 0
static int cli_gh(const char *const *args, unsigned int count, const char *input, char *out, unsigned long long room)
{
    const char *words[20] = {"gh"};
    for (unsigned int at = 0u; (at < count) && (at < 18u); at += 1u)
    {
        words[at + 1u] = args[at];
    }
    words[((count < 18u) ? count : 18u) + 1u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    const int ran_well =
        cli_ran_fed(words, NULL, NULL, 0u, input,
                    (input != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = input, .cap = CLI_CELLS_POOL}) : 0ull, ran,
                    &unstarted) &&
        (ran->code == 0ll);
    if (ran_well)
    {
        const char *from = ran->out;
        const char *to = ran->out + ran->out_size;
        while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
        {
            from += 1;
        }
        while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
        {
            to -= 1;
        }
        VerbaLinea line = {out, room, 0ull};
        mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
        mmgr_line_finish(&line);
    }
    cli_ran_given(ran);
    return ran_well;
}

int cli_report_open_page(const char *url, VerbaLinea *err_msg)
{
    static const char s_home[] = "https://github.com/" CLI_REPORT_REPOSITORY;
    const unsigned long long length = sizeof(s_home) - 1u;
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = url, .other = s_home, .cap = CLI_CELLS_POOL})) &&
        !(cli_text_same(url, s_home, length) && (url[length] == '/')))
    {
        mmgr_line_put(err_msg, url);
        mmgr_line_put(err_msg, " is not a page of " CLI_REPORT_REPOSITORY);
        return 0;
    }
    return cli_report_open_url(url, err_msg);
}

int cli_report_open_url(const char *url, VerbaLinea *err_msg)
{
#if defined(_WIN32)
    const char *const words[4] = {"rundll32", "url.dll,FileProtocolHandler", url, NULL};
#elif defined(__APPLE__)
    const char *const words[3] = {"open", url, NULL};
#else
    const char *const words[3] = {"xdg-open", url, NULL};
#endif
    // the browser is the reader's, and stays as orior_cli ends
    unsigned int id = 0u;
    if (!cli_started_outside(words, &id))
    {
        mmgr_line_put(err_msg, words[0]);
        mmgr_line_put(err_msg, " did not start");
        return 0;
    }
    return 1;
}

// the lock a report is struck and filed under, one at a time
static CliLock s_report_lock = CLI_LOCK_INIT;

// a field of a report struck into `into`, a growing line begun here and given back with
// g_cli_cells->persistent_buf_release(into->out, &g_cli_machine_cells): the field, "" where its cells are refused
static const char *cli_struck(const char *text, const char *root, VerbaLinea *into)
{
    if (!cli_growing_line_made(into, CLI_REPORT_FIELD_FIRST, &g_cli_machine_cells))
    {
        return "";
    }
    cli_report_scrub(text, root, into);
    return (mmgr_line_finish(into) != 0ull) ? into->out : "";
}

// the report struck as `report` filed through the reporter's GitHub CLI, as nothing where its error is already an
// open issue, the lock acquired: 1 with `filed` set, or 0 where the CLI is not signed in or did not file it
static int cli_file_by_gh(const CliReport *report, const char *root, const char *print, CliFiled *filed)
{
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea gh_err = {why, sizeof(why), 0ull};
    static const char *const s_auth[2] = {"auth", "status"};
    char unused[CLI_PATH_MAX_LEN];
    if (!cli_net_reach("https://github.com/" CLI_REPORT_REPOSITORY, "an error report", &gh_err) ||
        !cli_gh(s_auth, 2u, NULL, unused, sizeof(unused)))
    {
        return 0;
    }
    char search[64];
    VerbaLinea searched = {search, sizeof(search), 0ull};
    mmgr_line_put(&searched, "\"orior-report ");
    mmgr_line_put(&searched, print);
    mmgr_line_put(&searched, "\" in:body");
    mmgr_line_finish(&searched);
    const char *const listed[12] = {"issue",   "list", "--repo",   CLI_REPORT_REPOSITORY,
                                    "--state", "open", "--search", search,
                                    "--json",  "url",  "--jq",     ".[0].url"};
    if (cli_gh(listed, 12u, NULL, filed->url, sizeof(filed->url)) && (filed->url[0] != '\0'))
    {
        filed->kind = CLI_FILED_KNOWN;
        return 1;
    }
    VerbaLinea body;
    if (!cli_growing_line_temporary(&body, CLI_REPORT_FIELD_FIRST, &g_cli_machine_cells))
    {
        return 0;
    }
    cli_report_body(report, root, print, &body);
    const char *const written = (mmgr_line_finish(&body) != 0ull) ? body.out : "";
    char title[CLI_REPORT_URL];
    VerbaLinea titled = {title, sizeof(title), 0ull};
    mmgr_line_put(&titled, "[bug] ");
    mmgr_line_put(&titled, report->title);
    mmgr_line_finish(&titled);
    const char *const labelled[12] = {"issue",   "create", "--repo",      CLI_REPORT_REPOSITORY,
                                      "--title", title,    "--body-file", "-",
                                      "--label", "bug",    "--label",     report->category};
    // a repository without a category's label still takes the issue, unlabelled
    const int made = cli_gh(labelled, 12u, written, filed->url, sizeof(filed->url)) ||
                     cli_gh(labelled, 8u, written, filed->url, sizeof(filed->url));
    g_cli_cells->persistent_buf_release(body.out, &g_cli_machine_cells);
    if (made)
    {
        // the last line gh writes, the issue's address
        const char *const last =
            cli_text_last(filed->url, cellul.len(&(CatenaFinitaCfg){.src = filed->url, .cap = CLI_CELLS_POOL}), '\n');
        if (last != NULL)
        {
            char url_buf[CLI_REPORT_URL];
            memor.cpy(
                &(MemoriaCfg){.dst = url_buf,
                              .src = last + 1,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = last + 1, .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(
                &(MemoriaCfg){.dst = filed->url,
                              .src = url_buf,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = url_buf, .cap = CLI_CELLS_POOL}) + 1u});
        }
        filed->kind = CLI_FILED_FILED;
        return 1;
    }
    return 0;
}

// a report filed as cli_report_file files it, the page it falls back to opened only where `open`
static void cli_file_as(const CliReport *given, const char *root, int open, CliFiled *filed)
{
    cli_lock_take(&s_report_lock);
    VerbaLinea field[4];
    const CliReport report = {cli_report_category_of(given), cli_struck(given->title, root, &field[0]),
                              cli_struck(given->what, root, &field[1]), cli_struck(given->steps, root, &field[2]),
                              cli_struck(given->logs, root, &field[3])};
    char print[17];
    VerbaLinea joined;
    if (cli_growing_line_temporary(&joined, CLI_REPORT_FIELD_FIRST, &g_cli_machine_cells))
    {
        mmgr_line_put(&joined, report.title);
        mmgr_line_ch(&joined, '\n');
        mmgr_line_put(&joined, report.logs);
        cli_report_fingerprint(report.category, (mmgr_line_finish(&joined) != 0ull) ? joined.out : "", print);
        g_cli_cells->persistent_buf_release(joined.out, &g_cli_machine_cells);
    }
    else
    {
        cli_report_fingerprint(report.category, "", print);
    }
    const int by_gh = cli_file_by_gh(&report, root, print, filed);
    if (!by_gh)
    {
        VerbaLinea page = {filed->url, sizeof(filed->url), 0ull};
        cli_report_page(&report, root, &page);
        mmgr_line_finish(&page);
        filed->kind = CLI_FILED_PAGE;
    }
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        g_cli_cells->persistent_buf_release(field[at].out, &g_cli_machine_cells);
    }
    cli_lock_give(&s_report_lock);
    if (!by_gh && open)
    {
        char unopened[CLI_PATH_MAX_LEN];
        VerbaLinea refused = {unopened, sizeof(unopened), 0ull};
        cli_report_open_page(filed->url, &refused);
    }
}

void cli_report_file(const CliReport *report, const char *root, CliFiled *filed)
{
    cli_file_as(report, root, 1, filed);
}

int cli_report_error(const char *category, const char *message, const char *detail, const char *root, CliFiled *filed)
{
    static char s_scrubbed[CLI_NAME_MAX_LEN * 2u];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    VerbaLinea report_line;
    if (cli_growing_line_temporary(&report_line, CLI_REPORT_FIELD_FIRST, &g_cli_machine_cells))
    {
        mmgr_line_put(&report_line, message);
        mmgr_line_ch(&report_line, '\n');
        mmgr_line_put(&report_line, detail);
        if (mmgr_line_finish(&report_line) != 0ull)
        {
            cli_report_saved(report_line.out, root);
        }
        g_cli_cells->persistent_buf_release(report_line.out, &g_cli_machine_cells);
    }
    if (!cli_report_auto() || cli_net_is_disabled())
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    // the message's first line, trimmed, and the title its category and that line, a hundred letters at most
    const char *first = message;
    const char *end = message;
    while ((end[0] != '\0') && (end[0] != '\n'))
    {
        end += 1;
    }
    end -= ((end > first) && (end[-1] == '\r')) ? 1 : 0;
    while ((first < end) && ((first[0] == ' ') || ((first[0] >= '\t') && (first[0] <= '\r'))))
    {
        first += 1;
    }
    while ((end > first) && ((end[-1] == ' ') || ((end[-1] >= '\t') && (end[-1] <= '\r'))))
    {
        end -= 1;
    }
    char full[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea titled = {full, sizeof(full), 0ull};
    mmgr_line_put(&titled, category);
    mmgr_line_put(&titled, ": ");
    if (message[0] == '\0')
    {
        mmgr_line_put(&titled, "error");
    }
    else
    {
        mmgr_line_put_n(&titled, first, (unsigned long long)(end - first));
    }
    mmgr_line_finish(&titled);
    char title[CLI_NAME_MAX_LEN * 2u];
    unsigned long long letters = 0ull;
    unsigned long long at = 0ull;
    for (; full[at] != '\0'; at += 1ull)
    {
        const int starts = ((unsigned char)full[at] & 0xc0u) != 0x80u;
        if (starts && (letters == 100ull))
        {
            break;
        }
        letters += starts ? 1ull : 0ull;
    }
    VerbaLinea cut = {title, sizeof(title), 0ull};
    mmgr_line_put_n(&cut, full, at);
    mmgr_line_finish(&cut);
    VerbaLinea scrubbed = {s_scrubbed, sizeof(s_scrubbed), 0ull};
    cli_report_scrub(title, root, &scrubbed);
    mmgr_line_finish(&scrubbed);
    char print[17];
    cli_report_fingerprint(category, s_scrubbed, print);
    cli_lock_take(&s_filed_lock);
    int known = 0;
    for (unsigned int each = 0u; !known && (each < s_filed_count); each += 1u)
    {
        known = cellul.eq(&(CatenaFinitaCfg){.src = s_filed[each], .other = print, .cap = CLI_CELLS_POOL});
    }
    const int files = !known && (s_filed_count < CLI_REPORT_AUTO);
    if (files)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_filed[s_filed_count],
                                .src = print,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = print, .cap = CLI_CELLS_POOL}) + 1u});
        s_filed_count += 1u;
    }
    cli_lock_give(&s_filed_lock);
    if (!files)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    VerbaLinea what;
    const char *err_text = "";
    if (cli_growing_line_temporary(&what, CLI_REPORT_FIELD_FIRST, &g_cli_machine_cells))
    {
        mmgr_line_put(&what, "orior_cli reported this error on its own.\n\n");
        mmgr_line_put(&what, message);
        err_text = (mmgr_line_finish(&what) != 0ull) ? what.out : "";
    }
    const CliReport report = {category, title, err_text, "", detail};
    cli_file_as(&report, root, 0, filed);
    g_cli_cells->persistent_buf_release(what.out, &g_cli_machine_cells);
    cli_lock_give(&s_lock);
    return 1;
}
