// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report.h: error reports, filed as issues on orior's repository in the form its bug report template gives (report.c,
// report_file.c, report_page.c).
//
// A report goes through the reporter's own GitHub CLI where it is signed in, filed with no prompt under the reporter's
// account. Where it is not, the issue page opens in the browser with every field filled in, for the reporter to
// submit. A report whose error is already an open issue files nothing and answers with that issue.
//
// A report's category is the part of orior it is about: the library the command line and the window are on, the
// command line, the window, or unknown.
//
// Before anything leaves the machine the reporter's home folder, the tree's path and the reporter's name in a path are
// struck from it. Errors file on their own once the reporter confirms they may, a setting stored in orior's own folder
// and shared by the window and the command line, and nothing files before the reporter answers. The reporter is
// prompted once, yes the answer given by default: by the Windows installer, and otherwise on the first run, in the
// window or at the terminal. One error files once a run, and a run files at max five on its own.
#ifndef REPORT_H
#define REPORT_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliReportCategoryCfg CliReportCategoryCfg;
typedef struct CliReportRootCfg CliReportRootCfg;
typedef struct CliReportCategoryTextCfg CliReportCategoryTextCfg;
typedef struct CliReportRootOutCfg CliReportRootOutCfg;
typedef struct CliReportTextCfg CliReportTextCfg;
typedef struct CliReportErrMsgCfg CliReportErrMsgCfg;
typedef struct CliReportOnCfg CliReportOnCfg;
typedef struct CliReportCfg CliReportCfg;

// the repository issues are filed on, and the question the reporter is prompted once, at installation or on the first
// run
#define CLI_REPORT_REPOSITORY "dstroy0/orior"

#define CLI_REPORT_QUESTION                                                                                            \
    "imp files the errors it meets as issues on dstroy0/orior on its own: through your GitHub CLI where it is "        \
    "signed in, else as a page opened for you to submit. Help, Automatic Error Reports turns it on or off later."

// the longest a filled-in issue page's address grows, which browsers and GitHub both read whole, with capacity past it
#define CLI_REPORT_ADDRESS 7000u

#define CLI_REPORT_URL 8192u
// the errors of the run retained at max, the segments of their ring, a power of two over them, and the bytes the
// ring's tokens take, the errors' part of the rings block
#define CLI_REPORT_RETAIN_MAX 20u
#define CLI_REPORT_RETAIN_RING 32u
#define CLI_REPORT_ERRORS_RING_BYTES ((unsigned long long)CLI_REPORT_RETAIN_RING * sizeof(const CarcerMovableEntry *))

// What a report contains: its category, its title, what happened, how to make it happen again, and the error's own text
typedef struct
{
    const char *category;
    const char *title;
    const char *what;
    const char *steps;
    const char *logs;
} CliReport;

// Where a report went: an issue filed for it, an open issue that already contains its error, or the filled-in page
// opened for the reporter to submit
enum
{
    CLI_FILED_FILED,
    CLI_FILED_KNOWN,
    CLI_FILED_PAGE
};

typedef struct
{
    unsigned int type;
    char url[CLI_REPORT_URL];
} CliFiled;

// whether `category` is one the template's dropdown names: "library", "cli", "ui" or "unknown"
int cli_report_category_know(const CliReportCategoryTextCfg *args);
// whether errors file on their own: as the reporter answered, and not where they have not answered; whether the
// reporter has answered; the reporter prompted once at the terminal, where they have not answered, nothing turned
// reports off, and the run is at a terminal that can answer, Enter answering yes; and the answer stored: 1, or 0 with
// why given (report.c)
int cli_report_auto(void);
int cli_report_prompt(void);
void cli_report_prompt_once(void);
int cli_report_set_auto(const CliReportOnCfg *args);
// `text` with the reporter's home folder, the tree's path and the reporter's name in a path struck, each written either
// way its slashes lean, into `out`; a short name for an error that stays the same each time it happens, its category
// and its text with the numbers in it struck, hashed, sixteen hex digits into `out`, sized for 17; an error stored
// among the run's recent ones, struck of what identifies the reporter; the run's recent errors, oldest first, into
// `out`; and the category of a panic, from where its backtrace first enters orior's own code (report.c)
void cli_report_scrub(const CliReportTextCfg *args);
void cli_report_fingerprint(const CliReportCategoryTextCfg *args);
void cli_report_save(const CliReportTextCfg *args);
void cli_report_recent(const CliReportRootOutCfg *args);
const char *cli_report_panic_category(const CliReportCfg *args);
// the new-issue page with the template's fields filled in, its logs cut to bring the address within CLI_REPORT_ADDRESS,
// into `out`; a page of orior's repository opened in the browser, any other address refused; `url` opened in the
// reader's browser, started outside orior's container so it stays as impero ends; a report filed, as an issue
// through the reporter's GitHub CLI where it is signed in, as nothing where its error is already an open issue, and
// otherwise as the filled-in page, opened; and an error filed on its own where the reporter lets errors file, where it
// has not filed this run, and where the run has filed fewer than five, 1 where it went somewhere (report_file.c)
void cli_report_page(const CliReportRootOutCfg *args);
int cli_report_open_page(const CliReportErrMsgCfg *args);
int cli_report_open_url(const CliReportErrMsgCfg *args);
void cli_report_file(const CliReportRootCfg *args);
int cli_report_error(const CliReportCategoryCfg *args);

struct CliReportCategoryCfg
{
    const char *category;
    const char *root;
    CliFiled *filed;
    const char *message;
    const char *detail;
};

struct CliReportRootCfg
{
    const char *root;
    const CliReport *report;
    CliFiled *filed;
};

struct CliReportCategoryTextCfg
{
    const char *category;
    const char *text;
    char *out_capacity;
};

struct CliReportRootOutCfg
{
    const char *root;
    VerbaLinea *out;
    const CliReport *report;
};

struct CliReportTextCfg
{
    const char *text;
    const char *root;
    VerbaLinea *out;
};

struct CliReportErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *url;
};

struct CliReportOnCfg
{
    int on;
    VerbaLinea *err_msg;
};

struct CliReportCfg
{
    const char *trace;
};

typedef struct
{
    int (*category_know)(const CliReportCategoryTextCfg *args);
    void (*fingerprint)(const CliReportCategoryTextCfg *args);
} CliReportCategoryTextNs;
EMBED_TABLE_LAYOUT(CliReportCategoryTextNs, category_know, fingerprint);
EMBED_TABLE_STORAGE CliReportCategoryTextNs cli_report_category_text EMBED_UNUSED = {
    .category_know = cli_report_category_know,
    .fingerprint = cli_report_fingerprint,
};

typedef struct
{
    int (*set_auto)(const CliReportOnCfg *args);
} CliReportOnNs;
EMBED_TABLE_LAYOUT(CliReportOnNs, set_auto);
EMBED_TABLE_STORAGE CliReportOnNs cli_report_on EMBED_UNUSED = {
    .set_auto = cli_report_set_auto,
};

typedef struct
{
    void (*scrub)(const CliReportTextCfg *args);
    void (*save)(const CliReportTextCfg *args);
} CliReportTextNs;
EMBED_TABLE_LAYOUT(CliReportTextNs, scrub, save);
EMBED_TABLE_STORAGE CliReportTextNs cli_report_text EMBED_UNUSED = {
    .scrub = cli_report_scrub,
    .save = cli_report_save,
};

typedef struct
{
    void (*recent)(const CliReportRootOutCfg *args);
    void (*page)(const CliReportRootOutCfg *args);
} CliReportRootOutNs;
EMBED_TABLE_LAYOUT(CliReportRootOutNs, recent, page);
EMBED_TABLE_STORAGE CliReportRootOutNs cli_report_root_out EMBED_UNUSED = {
    .recent = cli_report_recent,
    .page = cli_report_page,
};

typedef struct
{
    const char *(*panic_category)(const CliReportCfg *args);
} CliReportNsTable;
EMBED_TABLE_LAYOUT(CliReportNsTable, panic_category);
EMBED_TABLE_STORAGE CliReportNsTable cli_report EMBED_UNUSED = {
    .panic_category = cli_report_panic_category,
};

typedef struct
{
    int (*open_page)(const CliReportErrMsgCfg *args);
    int (*open_url)(const CliReportErrMsgCfg *args);
} CliReportErrMsgNs;
EMBED_TABLE_LAYOUT(CliReportErrMsgNs, open_page, open_url);
EMBED_TABLE_STORAGE CliReportErrMsgNs cli_report_err_msg EMBED_UNUSED = {
    .open_page = cli_report_open_page,
    .open_url = cli_report_open_url,
};

typedef struct
{
    void (*file)(const CliReportRootCfg *args);
} CliReportRootNs;
EMBED_TABLE_LAYOUT(CliReportRootNs, file);
EMBED_TABLE_STORAGE CliReportRootNs cli_report_root EMBED_UNUSED = {
    .file = cli_report_file,
};

typedef struct
{
    int (*error)(const CliReportCategoryCfg *args);
} CliReportCategoryNs;
EMBED_TABLE_LAYOUT(CliReportCategoryNs, error);
EMBED_TABLE_STORAGE CliReportCategoryNs cli_report_category EMBED_UNUSED = {
    .error = cli_report_error,
};

#endif
