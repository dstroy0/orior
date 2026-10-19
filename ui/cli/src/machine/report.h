// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report.h: error reports, filed as issues on orior's repository in the form its bug report template gives (report.c,
// report_file.c, report_page.c).
//
// A report goes through the reporter's own GitHub CLI where it is signed in, filed with no prompt under the reporter's
// account. Where it is not, the issue page opens in the browser with every field filled in, for the reporter to
// submit. A report whose error is already an open issue files nothing and answers with that issue.
//
// A report's category is the part of orior it is about: the library the command line and the window stand on, the
// command line, the window, or unknown.
//
// Before anything leaves the machine the reporter's home folder, the tree's path and the reporter's name in a path are
// struck from it. Errors file on their own once the reporter says they may, a setting kept in orior's own folder and
// shared by the window and the command line, and nothing files before the reporter answers. The reporter is asked
// once, yes the answer given by default: by the Windows installer, and otherwise on the first run, in the window or at
// the terminal. One error files once a run, and a run files at most five on its own.
#ifndef REPORT_H
#define REPORT_H

#include "../command_line/cli_internal.h"

// the repository issues are filed on, and the question the reporter is asked once, at installation or on the first run
#define CLI_REPORT_REPOSITORY "dstroy0/orior"

#define CLI_REPORT_QUESTION                                                                                            \
    "orior files the errors it meets as issues on dstroy0/orior on its own: through your GitHub CLI where it is "      \
    "signed in, else as a page opened for you to submit. Help, Automatic Error Reports turns it on or off later."

// the longest a filled-in issue page's address grows, which browsers and GitHub both read whole, with room past it
#define CLI_REPORT_ADDRESS 7000u

#define CLI_REPORT_URL 8192u

// What a report says: its category, its title, what happened, how to make it happen again, and the error's own text
typedef struct
{
    const char *category;
    const char *title;
    const char *what;
    const char *steps;
    const char *logs;
} CliReport;

// Where a report went: an issue filed for it, an open issue that already holds its error, or the filled-in page opened
// for the reporter to submit
enum
{
    CLI_FILED_FILED,
    CLI_FILED_KNOWN,
    CLI_FILED_PAGE
};

typedef struct
{
    unsigned int kind;
    char url[CLI_REPORT_URL];
} CliFiled;

// whether `category` is one the template's dropdown names: "library", "cli", "ui" or "unknown"
int cli_report_category_known(const char *category);
// whether errors file on their own: as the reporter answered, and not where they have not answered; whether the
// reporter has answered; the reporter asked once at the terminal, where they have not answered, nothing turned reports
// off, and the run is at a terminal that can answer, Enter answering yes; and the answer kept: 1, or 0 with why said
// (report.c)
int cli_report_auto(void);
int cli_report_asked(void);
void cli_report_ask_once(void);
int cli_report_set_auto(int on, CliLine *said);
// `text` with the reporter's home folder, the tree's path and the reporter's name in a path struck, each written either
// way its slashes lean, into `out`; a short name for an error that stays the same each time it happens, its category
// and its text with the numbers in it struck, hashed, sixteen hex digits into `out`, which holds 17; an error kept
// among the run's recent ones, struck of what identifies the reporter; the run's recent errors, oldest first, into
// `out`; and the category of a panic, from where its backtrace first enters orior's own code (report.c)
void cli_report_scrub(const char *text, const char *root, CliLine *out);
void cli_report_fingerprint(const char *category, const char *text, char *out);
void cli_report_keep(const char *text, const char *root);
void cli_report_recent(CliLine *out);
const char *cli_report_panic_category(const char *trace);
// the new-issue page with the template's fields filled in, its logs cut to keep the address within CLI_REPORT_ADDRESS,
// into `out`; a page of orior's repository opened in the browser, any other address refused; `url` opened in the
// reader's browser, started outside orior's container so it stays as orior ends; a report filed, as an issue through
// the reporter's GitHub CLI where it is signed in, as nothing where its error is already an open issue, and otherwise
// as the filled-in page, opened; and an error filed on its own where the reporter lets errors file, where it has not
// filed this run, and where the run has filed fewer than five, 1 where it went somewhere (report_file.c)
void cli_report_page(const CliReport *report, const char *root, CliLine *out);
int cli_report_open_page(const char *url, CliLine *said);
int cli_report_open_url(const char *url, CliLine *said);
void cli_report_file(const CliReport *report, const char *root, CliFiled *filed);
int cli_report_error(const char *category, const char *message, const char *detail, const char *root, CliFiled *filed);

#endif
