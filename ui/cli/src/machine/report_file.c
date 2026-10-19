// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_file.c: a report filed: through the reporter's GitHub CLI where it is signed in, as nothing where its error is
// already an open issue, and otherwise as the new-issue page filled in, opened in the browser
#include "../text/text.h"
#include "net.h"
#include "report_internal.h"

// the most reports a run files on its own, and the most bytes a struck field of a report holds
#define CLI_REPORT_AUTO 5u

#define CLI_REPORT_FIELD (1u << 20u)

// the fingerprints of the errors this run filed, under the lock
static char s_filed[CLI_REPORT_AUTO][17];
static unsigned int s_filed_count;
static CliLock s_filed_lock = CLI_LOCK_INIT;

// the GitHub CLI run with `args`, `count` of them, `input` on its standard input where it is given: what it wrote,
// trimmed, into `out`, which holds `room`, 1 where it ends well, or 0
static int cli_gh(const char *const *args, unsigned int count, const char *input, char *out, unsigned long long room)
{
    const char *words[20] = {"gh"};
    for (unsigned int at = 0u; (at < count) && (at < 18u); at += 1u)
    {
        words[at + 1u] = args[at];
    }
    words[((count < 18u) ? count : 18u) + 1u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
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
        CliLine line = {out, room, 0ull};
        cli_line_counted(&line, from, (unsigned long long)(to - from));
        cli_line_closed(&line);
    }
    cli_ran_given(ran);
    return ran_well;
}

int cli_report_open_page(const char *url, CliLine *said)
{
    static const char s_home[] = "https://github.com/" CLI_REPORT_REPOSITORY;
    const unsigned long long length = sizeof(s_home) - 1u;
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = url, .other = s_home, .cap = CLI_CELLS_POOL})) &&
        !(cli_text_same(url, s_home, length) && (url[length] == '/')))
    {
        cli_line_text(said, url);
        cli_line_text(said, " is not a page of " CLI_REPORT_REPOSITORY);
        return 0;
    }
    return cli_report_open_url(url, said);
}

int cli_report_open_url(const char *url, CliLine *said)
{
#if defined(_WIN32)
    const char *const words[4] = {"rundll32", "url.dll,FileProtocolHandler", url, NULL};
#elif defined(__APPLE__)
    const char *const words[3] = {"open", url, NULL};
#else
    const char *const words[3] = {"xdg-open", url, NULL};
#endif
    // the browser is the reader's, and stays as orior ends
    unsigned int id = 0u;
    if (!cli_started_outside(words, &id))
    {
        cli_line_text(said, words[0]);
        cli_line_text(said, " did not start");
        return 0;
    }
    return 1;
}

// A report struck of what identifies the reporter, its fields kept under the lock
typedef struct
{
    char title[CLI_REPORT_FIELD];
    char what[CLI_REPORT_FIELD];
    char steps[CLI_REPORT_FIELD];
    char logs[CLI_REPORT_FIELD];
    char joined[CLI_REPORT_FIELD * 2u];
    char body[CLI_REPORT_FIELD * 4u];
} CliStruckReport;

static CliStruckReport s_report;
static CliLock s_report_lock = CLI_LOCK_INIT;

// a field of a report struck into `into`
static void cli_struck(const char *text, const char *root, char *into)
{
    CliLine line = {into, CLI_REPORT_FIELD, 0ull};
    cli_report_scrub(text, root, &line);
    cli_line_closed(&line);
}

// a report filed as cli_report_file files it, the page it falls back to opened only where `open`
static void cli_file_as(const CliReport *given, const char *root, int open, CliFiled *filed)
{
    cli_lock_take(&s_report_lock);
    CliStruckReport *const struck = &s_report;
    cli_struck(given->title, root, struck->title);
    cli_struck(given->what, root, struck->what);
    cli_struck(given->steps, root, struck->steps);
    cli_struck(given->logs, root, struck->logs);
    const CliReport report = {cli_report_category_of(given), struck->title, struck->what, struck->steps, struck->logs};
    CliLine joined = {struck->joined, sizeof(struck->joined), 0ull};
    cli_line_text(&joined, report.title);
    cli_line_character(&joined, '\n');
    cli_line_text(&joined, report.logs);
    cli_line_closed(&joined);
    char print[17];
    cli_report_fingerprint(report.category, struck->joined, print);
    char why[CLI_PATH_LONGEST];
    CliLine held = {why, sizeof(why), 0ull};
    static const char *const s_auth[2] = {"auth", "status"};
    char unused[CLI_PATH_LONGEST];
    if (cli_net_reach("https://github.com/" CLI_REPORT_REPOSITORY, "an error report", &held) &&
        cli_gh(s_auth, 2u, NULL, unused, sizeof(unused)))
    {
        char search[64];
        CliLine searched = {search, sizeof(search), 0ull};
        cli_line_text(&searched, "\"orior-report ");
        cli_line_text(&searched, print);
        cli_line_text(&searched, "\" in:body");
        cli_line_closed(&searched);
        const char *const listed[12] = {"issue",   "list", "--repo",   CLI_REPORT_REPOSITORY,
                                        "--state", "open", "--search", search,
                                        "--json",  "url",  "--jq",     ".[0].url"};
        if (cli_gh(listed, 12u, NULL, filed->url, sizeof(filed->url)) && (filed->url[0] != '\0'))
        {
            filed->kind = CLI_FILED_KNOWN;
            cli_lock_give(&s_report_lock);
            return;
        }
        CliLine body = {struck->body, sizeof(struck->body), 0ull};
        cli_report_body(&report, root, print, &body);
        cli_line_closed(&body);
        char title[CLI_REPORT_URL];
        CliLine titled = {title, sizeof(title), 0ull};
        cli_line_text(&titled, "[bug] ");
        cli_line_text(&titled, report.title);
        cli_line_closed(&titled);
        const char *const labelled[12] = {"issue",   "create", "--repo",      CLI_REPORT_REPOSITORY,
                                          "--title", title,    "--body-file", "-",
                                          "--label", "bug",    "--label",     report.category};
        // a repository without a category's label still takes the issue, unlabelled
        if (cli_gh(labelled, 12u, struck->body, filed->url, sizeof(filed->url)) ||
            cli_gh(labelled, 8u, struck->body, filed->url, sizeof(filed->url)))
        {
            // the last line gh writes, the issue's address
            const char *const last = cli_text_last(
                filed->url, cellul.len(&(CatenaFinitaCfg){.src = filed->url, .cap = CLI_CELLS_POOL}), '\n');
            if (last != NULL)
            {
                char kept[CLI_REPORT_URL];
                memor.cpy(&(MemoriaCfg){
                    .dst = kept,
                    .src = last + 1,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = last + 1, .cap = CLI_CELLS_POOL}) + 1u});
                memor.cpy(
                    &(MemoriaCfg){.dst = filed->url,
                                  .src = kept,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = kept, .cap = CLI_CELLS_POOL}) + 1u});
            }
            filed->kind = CLI_FILED_FILED;
            cli_lock_give(&s_report_lock);
            return;
        }
    }
    CliLine page = {filed->url, sizeof(filed->url), 0ull};
    cli_report_page(&report, root, &page);
    cli_line_closed(&page);
    cli_lock_give(&s_report_lock);
    filed->kind = CLI_FILED_PAGE;
    if (open)
    {
        char unopened[CLI_PATH_LONGEST];
        CliLine refused = {unopened, sizeof(unopened), 0ull};
        cli_report_open_page(filed->url, &refused);
    }
}

void cli_report_file(const CliReport *report, const char *root, CliFiled *filed)
{
    cli_file_as(report, root, 1, filed);
}

int cli_report_error(const char *category, const char *message, const char *detail, const char *root, CliFiled *filed)
{
    static char s_kept[CLI_REPORT_FIELD];
    static char s_what[CLI_REPORT_FIELD];
    static char s_scrubbed[CLI_NAME_LONGEST * 2u];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    CliLine kept = {s_kept, sizeof(s_kept), 0ull};
    cli_line_text(&kept, message);
    cli_line_character(&kept, '\n');
    cli_line_text(&kept, detail);
    cli_line_closed(&kept);
    cli_report_keep(s_kept, root);
    if (!cli_report_auto() || cli_net_held())
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
    char full[CLI_NAME_LONGEST * 2u];
    CliLine titled = {full, sizeof(full), 0ull};
    cli_line_text(&titled, category);
    cli_line_text(&titled, ": ");
    if (message[0] == '\0')
    {
        cli_line_text(&titled, "error");
    }
    else
    {
        cli_line_counted(&titled, first, (unsigned long long)(end - first));
    }
    cli_line_closed(&titled);
    char title[CLI_NAME_LONGEST * 2u];
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
    CliLine cut = {title, sizeof(title), 0ull};
    cli_line_counted(&cut, full, at);
    cli_line_closed(&cut);
    CliLine scrubbed = {s_scrubbed, sizeof(s_scrubbed), 0ull};
    cli_report_scrub(title, root, &scrubbed);
    cli_line_closed(&scrubbed);
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
    CliLine what = {s_what, sizeof(s_what), 0ull};
    cli_line_text(&what, "orior reported this error on its own.\n\n");
    cli_line_text(&what, message);
    cli_line_closed(&what);
    const CliReport report = {category, title, s_what, "", detail};
    cli_file_as(&report, root, 0, filed);
    cli_lock_give(&s_lock);
    return 1;
}
