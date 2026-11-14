// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_internal.h: what report_file.c and report_page.c share
#ifndef REPORT_INTERNAL_H
#define REPORT_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "report.h"

typedef struct CliReportCtx CliReportCtx;

// a report's category as the template's dropdown names it, "unknown" for any other; and the issue's body, arranged
// as the template's form arranges what is entered in it, its fingerprint `print` last, into `out` (report_page.c)
const char *cli_report_category_get(const CliReportCtx *args);
void cli_report_body(const CliReportCtx *args);

struct CliReportCtx
{
    const CliReport *report;
    const char *root;
    const char *print;
    VerbaLinea *out;
};

typedef struct
{
    const char *(*category_get)(const CliReportCtx *args);
    void (*body)(const CliReportCtx *args);
} CliReportNs;
EMBED_TABLE_LAYOUT(CliReportNs, category_get, body);
EMBED_TABLE_STORAGE CliReportNs cli_report_internal EMBED_UNUSED = {
    .category_get = cli_report_category_get,
    .body = cli_report_body,
};

#endif
