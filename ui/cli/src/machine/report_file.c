// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_file.c: a report filed: through the reporter's GitHub CLI where it is signed in, as nothing where its error is
// already an open issue, and otherwise as the new-issue page filled in, opened in the browser
#include "../text/text.h"
#include "net.h"
#include "report_internal.h"

// the most reports a run files on its own, and the bytes a struck field of a report is written in
#define CLI_REPORT_AUTO 5u

#define CLI_REPORT_FIELD_CAPACITY (4ULL << 20u)

// the fingerprints of the errors this run filed, under the lock
static char s_filed[CLI_REPORT_AUTO][17];
static unsigned int s_filed_count;

typedef struct
{
    const char *const *request;
    unsigned int count;
    const char *input;
    char *out;
    unsigned long long capacity;
    const char *text;
    const char *root;
    VerbaLinea *into;
    const CliReport *report;
    const char *print;
    CliFiled *filed;
    const CliReport *supplied;
    int open;
} ReportFileCtx;
// the GitHub CLI run with `args`, `count` of them, `input` on its standard input where it is given: what it wrote,
// trimmed, into `out`, sized for `capacity`, 1 where it ends well, or 0
EMBED_INLINE int cli_gh(const ReportFileCtx *args)
{
    unsigned int count = args->count;
    const char *input = args->input;
    const char *words[20] = {"gh"};
    for (unsigned int at = 0u; (at < count) && (at < 18u); at += 1u)
    {
        words[at + 1u] = args->request[at];
    }
    words[((count < 18u) ? count : 18u) + 1u] = NULL;
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    const int run_result_well =
        EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL,
                   .count = 0u, .input = input,
                   .length =
                       (input != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = input, .cap = CLI_CELLS_POOL}) : 0ULL,
                   .run_result = run_result, .err_msg = &unstarted) &&
        (run_result->code == 0LL);
    if (run_result_well)
    {
        const char *from = run_result->out;
        const char *to = run_result->out + run_result->out_size;
        from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                           .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
        to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                               .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
        VerbaLinea line = {args->out, args->capacity, 0ULL};
        mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
        mmgr_line_finish(&line);
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return run_result_well;
}

int cli_report_open_page(const CliReportErrMsgCfg *args)
{
    static const char s_home[] = "https://github.com/" CLI_REPORT_REPOSITORY;
    const unsigned long long length = sizeof(s_home) - 1u;
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->url, .other = s_home, .cap = CLI_CELLS_POOL})) &&
        !(EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->url, .other_pointer = s_home,
                     .size = length) &&
          (args->url[length] == '/')))
    {
        mmgr_line_put(args->err_msg, args->url);
        mmgr_line_put(args->err_msg, " is not a page of " CLI_REPORT_REPOSITORY);
        return 0;
    }
    return cli_report_err_msg.open_url(args);
}

int cli_report_open_url(const CliReportErrMsgCfg *args)
{
#if defined(_WIN32)
    const char *const words[4] = {"rundll32", "url.dll,FileProtocolHandler", args->url, NULL};
#elif defined(__APPLE__)
    const char *const words[3] = {"open", args->url, NULL};
#else
    const char *const words[3] = {"xdg-open", args->url, NULL};
#endif
    // the browser is the reader's, and stays as orior_cli ends
    unsigned int id = 0u;
    if (!EMBED_CALL(cli_words_id_internal.started_outside, CliWordsIdCtx, .words = words, .id = &id))
    {
        mmgr_line_put(args->err_msg, words[0]);
        mmgr_line_put(args->err_msg, " did not start");
        return 0;
    }
    return 1;
}

// the lock a report is struck and filed under, one at a time

// a field of a report struck into `into`, a line begun here and given back with
// g_cli_cells->persistent_buf_release(into->out, &g_cli_machine_cells): the field, "" where its cells are refused
EMBED_INLINE const char *cli_on_strike(const ReportFileCtx *args)
{
    const char *text = args->text;
    const char *root = args->root;
    VerbaLinea *into = args->into;
    char *const cell = g_cli_cells->temporary_buf_alloc(CLI_REPORT_FIELD_CAPACITY, &g_cli_machine_cells);
    *into = mmgr_line_over(cell, CLI_REPORT_FIELD_CAPACITY);
    if (cell == NULL)
    {
        return "";
    }
    EMBED_CALL(cli_report_text.scrub, CliReportTextCfg, .text = text, .root = root, .out = into);
    return (mmgr_line_finish(into) != 0ULL) ? into->out : "";
}

// the report struck as `report` filed through the reporter's GitHub CLI, as nothing where its error is already an
// open issue, the lock acquired: 1 with `filed` set, or 0 where the CLI is not signed in or did not file it
EMBED_INLINE int cli_file_by_gh(const ReportFileCtx *args)
{
    const CliReport *report = args->report;
    const char *root = args->root;
    const char *print = args->print;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea gh_err = {why, sizeof(why), 0ULL};
    static const char *const s_auth[2] = {"auth", "status"};
    char unused[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = "https://github.com/" CLI_REPORT_REPOSITORY,
                    .purpose = "an error report", .err_msg = &gh_err) ||
        !EMBED_CALL(cli_gh, ReportFileCtx, .request = s_auth, .count = 2u, .input = NULL, .out = unused,
                    .capacity = sizeof(unused)))
    {
        return 0;
    }
    char search[64];
    VerbaLinea searched = {search, sizeof(search), 0ULL};
    mmgr_line_put(&searched, "\"orior-report ");
    mmgr_line_put(&searched, print);
    mmgr_line_put(&searched, "\" in:body");
    mmgr_line_finish(&searched);
    const char *const list_entries[12] = {"issue",   "list", "--repo",   CLI_REPORT_REPOSITORY,
                                          "--state", "open", "--search", search,
                                          "--json",  "url",  "--jq",     ".[0].url"};
    if (EMBED_CALL(cli_gh, ReportFileCtx, .request = list_entries, .count = 12u, .input = NULL, .out = args->filed->url,
                   .capacity = sizeof(args->filed->url)) &&
        (args->filed->url[0] != '\0'))
    {
        args->filed->kind = CLI_FILED_KNOWN;
        return 1;
    }
    const unsigned long long body_capacity = 5ULL * CLI_REPORT_FIELD_CAPACITY;
    char *const body_cell = g_cli_cells->temporary_buf_alloc(body_capacity, &g_cli_machine_cells);
    VerbaLinea body = mmgr_line_over(body_cell, body_capacity);
    if (body.out == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_report_internal.body, CliReportCtx, .report = report, .root = root, .print = print, .out = &body);
    const char *const written = (mmgr_line_finish(&body) != 0ULL) ? body.out : "";
    char title[CLI_REPORT_URL];
    VerbaLinea titled = {title, sizeof(title), 0ULL};
    mmgr_line_put(&titled, "[bug] ");
    mmgr_line_put(&titled, report->title);
    mmgr_line_finish(&titled);
    const char *const labelled[12] = {"issue",   "create", "--repo",      CLI_REPORT_REPOSITORY,
                                      "--title", title,    "--body-file", "-",
                                      "--label", "bug",    "--label",     report->category};
    // a repository without a category's label still takes the issue, unlabelled
    const int made = EMBED_CALL(cli_gh, ReportFileCtx, .request = labelled, .count = 12u, .input = written,
                                .out = args->filed->url, .capacity = sizeof(args->filed->url)) ||
                     EMBED_CALL(cli_gh, ReportFileCtx, .request = labelled, .count = 8u, .input = written,
                                .out = args->filed->url, .capacity = sizeof(args->filed->url));
    g_cli_cells->persistent_buf_release(body.out, &g_cli_machine_cells);
    if (made)
    {
        // the last line gh writes, the issue's address
        const char *const last =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->filed->url,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->filed->url, .cap = CLI_CELLS_POOL}),
                       .byte = '\n');
        if (last != NULL)
        {
            char url_buf[CLI_REPORT_URL];
            memor.cpy(
                &(MemoriaCfg){.dst = url_buf,
                              .src = last + 1,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = last + 1, .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(
                &(MemoriaCfg){.dst = args->filed->url,
                              .src = url_buf,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = url_buf, .cap = CLI_CELLS_POOL}) + 1u});
        }
        args->filed->kind = CLI_FILED_FILED;
        return 1;
    }
    return 0;
}

// a report filed as cli_report_file files it, the page it falls back to opened only where `open`
static void cli_file_as(const ReportFileCtx *args)
{
    const char *root = args->root;
    VerbaLinea field[4];
    const CliReport report = {
        EMBED_CALL(cli_report_internal.category_get, CliReportCtx, .report = args->supplied),
        EMBED_CALL(cli_on_strike, ReportFileCtx, .text = args->supplied->title, .root = root, .into = &field[0]),
        EMBED_CALL(cli_on_strike, ReportFileCtx, .text = args->supplied->what, .root = root, .into = &field[1]),
        EMBED_CALL(cli_on_strike, ReportFileCtx, .text = args->supplied->steps, .root = root, .into = &field[2]),
        EMBED_CALL(cli_on_strike, ReportFileCtx, .text = args->supplied->logs, .root = root, .into = &field[3])};
    char print[17];
    char *const joined_cell = g_cli_cells->temporary_buf_alloc(2ULL * CLI_REPORT_FIELD_CAPACITY, &g_cli_machine_cells);
    VerbaLinea joined = mmgr_line_over(joined_cell, 2ULL * CLI_REPORT_FIELD_CAPACITY);
    if (joined_cell != NULL)
    {
        mmgr_line_put(&joined, report.title);
        mmgr_line_ch(&joined, '\n');
        mmgr_line_put(&joined, report.logs);
        EMBED_CALL(cli_report_category_text.fingerprint, CliReportCategoryTextCfg, .category = report.category,
                   .text = (mmgr_line_finish(&joined) != 0ULL) ? joined.out : "", .out_capacity = print);
        g_cli_cells->persistent_buf_release(joined.out, &g_cli_machine_cells);
    }
    else
    {
        EMBED_CALL(cli_report_category_text.fingerprint, CliReportCategoryTextCfg, .category = report.category,
                   .text = "", .out_capacity = print);
    }
    const int by_gh = EMBED_CALL(cli_file_by_gh, ReportFileCtx, .report = &report, .root = root, .print = print,
                                 .filed = args->filed);
    if (!by_gh)
    {
        VerbaLinea page = {args->filed->url, sizeof(args->filed->url), 0ULL};
        EMBED_CALL(cli_report_root_out.page, CliReportRootOutCfg, .report = &report, .root = root, .out = &page);
        mmgr_line_finish(&page);
        args->filed->kind = CLI_FILED_PAGE;
    }
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        g_cli_cells->persistent_buf_release(field[at].out, &g_cli_machine_cells);
    }
    if (!by_gh && args->open)
    {
        char unopened[CLI_PATH_MAX_LEN];
        VerbaLinea refused = {unopened, sizeof(unopened), 0ULL};
        EMBED_CALL(cli_report_err_msg.open_page, CliReportErrMsgCfg, .url = args->filed->url, .err_msg = &refused);
    }
}

void cli_report_file(const CliReportRootCfg *args)
{
    EMBED_CALL(cli_file_as, ReportFileCtx, .supplied = args->report, .root = args->root, .open = 1,
               .filed = args->filed);
}

int cli_report_error(const CliReportCategoryCfg *args)
{
    static char s_scrubbed[CLI_NAME_MAX_LEN * 2u];
    char *const report_cell = g_cli_cells->temporary_buf_alloc(2ULL * CLI_REPORT_FIELD_CAPACITY, &g_cli_machine_cells);
    VerbaLinea report_line = mmgr_line_over(report_cell, 2ULL * CLI_REPORT_FIELD_CAPACITY);
    mmgr_line_put(&report_line, args->message);
    mmgr_line_ch(&report_line, '\n');
    mmgr_line_put(&report_line, args->detail);
    if (mmgr_line_finish(&report_line) != 0ULL)
    {
        EMBED_CALL(cli_report_text.save, CliReportTextCfg, .text = report_line.out, .root = args->root);
    }
    g_cli_cells->persistent_buf_release(report_line.out, &g_cli_machine_cells);
    if (!cli_report_auto() || cli_net_is_disable())
    {
        return 0;
    }
    // the message's first line, trimmed, and the title its category and that line, a hundred letters at most
    const char *first = args->message;
    const char *end = args->message;
    end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                      .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = "\n");
    end -= ((end > first) && (end[-1] == '\r')) ? 1 : 0;
    first += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = first, .at = 0ULL,
                        .size = (unsigned long long)(end - first), .set = CLI_TEXT_WHITE);
    end = first + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = first,
                             .at = (unsigned long long)(end - first), .set = CLI_TEXT_WHITE);
    char full[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea titled = {full, sizeof(full), 0ULL};
    mmgr_line_put(&titled, args->category);
    mmgr_line_put(&titled, ": ");
    if (args->message[0] == '\0')
    {
        mmgr_line_put(&titled, "error");
    }
    else
    {
        mmgr_line_put_n(&titled, first, (unsigned long long)(end - first));
    }
    mmgr_line_finish(&titled);
    char title[CLI_NAME_MAX_LEN * 2u];
    unsigned long long letters = 0ULL;
    unsigned long long at = 0ULL;
    for (; full[at] != '\0'; at += 1ULL)
    {
        const int starts = ((unsigned char)full[at] & 0xc0u) != 0x80u;
        if (starts && (letters == 100ULL))
        {
            break;
        }
        letters += starts ? 1ULL : 0ULL;
    }
    VerbaLinea cut = {title, sizeof(title), 0ULL};
    mmgr_line_put_n(&cut, full, at);
    mmgr_line_finish(&cut);
    VerbaLinea scrubbed = {s_scrubbed, sizeof(s_scrubbed), 0ULL};
    EMBED_CALL(cli_report_text.scrub, CliReportTextCfg, .text = title, .root = args->root, .out = &scrubbed);
    mmgr_line_finish(&scrubbed);
    char print[17];
    EMBED_CALL(cli_report_category_text.fingerprint, CliReportCategoryTextCfg, .category = args->category,
               .text = s_scrubbed, .out_capacity = print);
    int known = 0;
    unsigned int each = 0u;
    while (!known && (each < s_filed_count))
    {
        known = cellul.eq(&(CatenaFinitaCfg){.src = s_filed[each], .other = print, .cap = CLI_CELLS_POOL});
        each += 1u;
    }
    const int files = !known && (s_filed_count < CLI_REPORT_AUTO);
    if (files)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_filed[s_filed_count],
                                .src = print,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = print, .cap = CLI_CELLS_POOL}) + 1u});
        s_filed_count += 1u;
    }
    if (!files)
    {
        return 0;
    }
    char *const what_cell = g_cli_cells->temporary_buf_alloc(CLI_REPORT_FIELD_CAPACITY, &g_cli_machine_cells);
    VerbaLinea what = mmgr_line_over(what_cell, CLI_REPORT_FIELD_CAPACITY);
    const char *err_text = "";
    if (what_cell != NULL)
    {
        mmgr_line_put(&what, "orior_cli reported this error on its own.\n\n");
        mmgr_line_put(&what, args->message);
        err_text = (mmgr_line_finish(&what) != 0ULL) ? what.out : "";
    }
    const CliReport report = {args->category, title, err_text, "", args->detail};
    EMBED_CALL(cli_file_as, ReportFileCtx, .supplied = &report, .root = args->root, .open = 0, .filed = args->filed);
    g_cli_cells->persistent_buf_release(what.out, &g_cli_machine_cells);
    return 1;
}
