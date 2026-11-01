// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_report.c: what a report on a program reports, line by line, and the libraries it finds through a PATH copied
// beside the program
#include "../text/text.h"
#include "binary.h"

// one line of a report reported
static void cli_report_line_print(QuerySink sink, const char *first, const char *name, const char *by,
                                  const char *middle, const char *at, const char *last)
{
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea made = {line, sizeof(line), 0ull};
    mmgr_line_put(&made, first);
    mmgr_line_put(&made, name);
    if (by != NULL)
    {
        mmgr_line_put(&made, ", which ");
        mmgr_line_put(&made, by);
        mmgr_line_put(&made, " loads, ");
    }
    mmgr_line_put(&made, middle);
    mmgr_line_put(&made, at);
    mmgr_line_put(&made, last);
    const unsigned long long length = mmgr_line_finish(&made);
    sink.print(sink.context, line, length);
}

void cli_binary_report_print(const CliBinaryReport *report, QuerySink sink)
{
    for (unsigned int at = 0u; at < report->libraries; at += 1u)
    {
        const CliLibrary *const one = &report->library[at];
        const char *const where = one->has_at ? one->at : "";
        switch (one->found)
        {
        case CLI_FOUND_ORIOR_PATH:
            cli_report_line_print(
                sink, "", one->name, one->by, "is found through the folders orior_cli puts on its PATH, at ", where,
                ", and not where the program is opened on its own: Copy Libraries Beside It puts it beside "
                "the program");
            break;
        case CLI_FOUND_SYSTEM_PATH:
            cli_report_line_print(
                sink, "", one->name, one->by, "is found through this machine's PATH, at ", where,
                ", which another machine's PATH may not contain: Copy Libraries Beside It puts it beside the "
                "program");
            break;
        case CLI_FOUND_MISSING:
            cli_report_line_print(sink, "", one->name, one->by,
                                  "is found nowhere: the program does not start without it", "", "");
            break;
        case CLI_FOUND_DEBUG_RUNTIME:
            cli_report_line_print(
                sink, "", one->name, one->by,
                "is Visual Studio's debug runtime, which only a machine with Visual Studio has: a release "
                "build loads the runtime any machine can install",
                "", "");
            break;
        case CLI_FOUND_REDISTRIBUTABLE:
            cli_report_line_print(sink, "", one->name, one->by,
                                  "comes with the Visual C++ Redistributable, which a machine without it lacks", "",
                                  "");
            break;
        case CLI_FOUND_BUILD_PATH:
            cli_report_line_print(sink, "", one->name, one->by,
                                  "is found by a run path of the machine that built it, at ", where, "");
            break;
        default:
            break;
        }
    }
    for (unsigned int at = 0u; at < report->build_paths; at += 1u)
    {
        cli_report_line_print(
            sink, "the program looks for its libraries in ", report->build_path[at], NULL,
            ", a folder of the machine that built it; a run path from $ORIGIN finds them beside the program on "
            "any machine",
            "", "");
    }
    if (report->has_glibc)
    {
        const char *const version =
            (cellul.starts(&(CatenaFinitaCfg){.src = report->glibc, .other = "GLIBC_", .cap = CLI_CELLS_POOL}))
                ? (report->glibc + 6)
                : report->glibc;
        cli_report_line_print(
            sink, "the program requires glibc ", version, NULL,
            " or newer, and does not start on a system with an older one; a Rust musl target, or Go with "
            "CGO_ENABLED 0, builds a program that requires none",
            "", "");
    }
}

int cli_binary_print(const char *path, const char (*orior_path)[CLI_PATH_MAX_LEN], unsigned int count, QuerySink sink)
{
    static CliBinaryReport s_report;
    static CliLock s_print_lock = CLI_LOCK_INIT;
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ull};
    cli_lock_take(&s_print_lock);
    const int read = cli_binary_read(path, orior_path, count, &s_report, &err_msg);
    if (read)
    {
        cli_binary_report_print(&s_report, sink);
    }
    cli_lock_give(&s_print_lock);
    return read;
}

int cli_binary_copy_beside(const char *path, const CliBinaryReport *report,
                           void (*copied)(void *context, const char *name), void *context, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    char *cut = folder + cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    cut = folder + cli_text_back_any(folder, (unsigned long long)(cut - folder), "/\\");
    if (cut == folder)
    {
        mmgr_line_put(err_msg, "the program has no folder");
        return 0;
    }
    cut[-1] = '\0';
    for (unsigned int at = 0u; at < report->libraries; at += 1u)
    {
        const CliLibrary *const one = &report->library[at];
        if (!one->has_at || ((one->found != CLI_FOUND_ORIOR_PATH) && (one->found != CLI_FOUND_SYSTEM_PATH)))
        {
            continue;
        }
        char to[CLI_PATH_MAX_LEN];
        VerbaLinea line = {to, sizeof(to), 0ull};
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, one->name);
        if ((mmgr_line_finish(&line) == 0ull) || !query_file_copied(one->at, to))
        {
            mmgr_line_put(err_msg, one->at);
            mmgr_line_put(err_msg, ": it could not be copied");
            return 0;
        }
        copied(context, one->name);
    }
    return 1;
}
