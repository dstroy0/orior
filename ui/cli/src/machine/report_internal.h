// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_internal.h: what report_file.c and report_page.c share
#ifndef REPORT_INTERNAL_H
#define REPORT_INTERNAL_H

#include "report.h"

// a report's category as the template's dropdown names it, "unknown" for any other; and the issue's body, laid out
// as the template's form lays out what is entered in it, its fingerprint `print` last, into `out` (report_page.c)
const char *cli_report_category_of(const CliReport *report);
void cli_report_body(const CliReport *report, const char *root, const char *print, CliLine *out);

#endif
