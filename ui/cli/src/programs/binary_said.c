// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_said.c: what a report on a program tells, line by line, and the libraries it finds through a PATH copied
// beside the program
#include "binary.h"

// one line of a report said
static void cli_said_line(QuerySink say, const char *first, const char *name, const char *by, const char *middle,
                          const char *at, const char *last)
{
    char line[CLI_PATH_LONGEST * 2u];
    CliLine made = {line, sizeof(line), 0ull};
    cli_line_text(&made, first);
    cli_line_text(&made, name);
    cli_line_text(&made, (by != NULL) ? ", which " : "");
    cli_line_text(&made, (by != NULL) ? by : "");
    cli_line_text(&made, (by != NULL) ? " loads, " : "");
    cli_line_text(&made, middle);
    cli_line_text(&made, at);
    cli_line_text(&made, last);
    const unsigned long long length = cli_line_closed(&made);
    say.said(say.context, line, length);
}

void cli_binary_report_said(const CliBinaryReport *report, QuerySink say)
{
    for (unsigned int at = 0u; at < report->libraries; at += 1u)
    {
        const CliLibrary *const one = &report->library[at];
        const char *const where = one->has_at ? one->at : "";
        switch (one->found)
        {
        case CLI_FOUND_ORIOR_PATH:
            cli_said_line(say, "", one->name, one->by, "is found through the folders orior_cli puts on its PATH, at ",
                          where,
                          ", and not where the program is opened on its own: Copy Libraries Beside It puts it beside "
                          "the program");
            break;
        case CLI_FOUND_SYSTEM_PATH:
            cli_said_line(say, "", one->name, one->by, "is found through this machine's PATH, at ", where,
                          ", which another machine's PATH may not hold: Copy Libraries Beside It puts it beside the "
                          "program");
            break;
        case CLI_FOUND_MISSING:
            cli_said_line(say, "", one->name, one->by, "is found nowhere: the program does not start without it", "",
                          "");
            break;
        case CLI_FOUND_DEBUG_RUNTIME:
            cli_said_line(say, "", one->name, one->by,
                          "is Visual Studio's debug runtime, which only a machine with Visual Studio has: a release "
                          "build loads the runtime any machine can install",
                          "", "");
            break;
        case CLI_FOUND_REDISTRIBUTABLE:
            cli_said_line(say, "", one->name, one->by,
                          "comes with the Visual C++ Redistributable, which a machine without it lacks", "", "");
            break;
        case CLI_FOUND_BUILD_PATH:
            cli_said_line(say, "", one->name, one->by, "is found by a run path of the machine that built it, at ",
                          where, "");
            break;
        default:
            break;
        }
    }
    for (unsigned int at = 0u; at < report->build_paths; at += 1u)
    {
        cli_said_line(
            say, "the program looks for its libraries in ", report->build_path[at], NULL,
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
        cli_said_line(say, "the program asks for glibc ", version, NULL,
                      " or newer, and does not start on a system with an older one; a Rust musl target, or Go with "
                      "CGO_ENABLED 0, builds a program that asks for none",
                      "", "");
    }
}

int cli_binary_said(const char *path, const char (*orior_path)[CLI_PATH_LONGEST], unsigned int count, QuerySink say)
{
    static CliBinaryReport s_report;
    static CliLock s_saying = CLI_LOCK_INIT;
    char unsaid[CLI_PATH_LONGEST];
    CliLine said = {unsaid, sizeof(unsaid), 0ull};
    cli_lock_take(&s_saying);
    const int read = cli_binary_read(path, orior_path, count, &s_report, &said);
    if (read)
    {
        cli_binary_report_said(&s_report, say);
    }
    cli_lock_give(&s_saying);
    return read;
}

int cli_binary_copy_beside(const char *path, const CliBinaryReport *report,
                           void (*copied)(void *context, const char *name), void *context, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    char *cut = folder + cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    while ((cut > folder) && (cut[-1] != '/') && (cut[-1] != '\\'))
    {
        cut -= 1;
    }
    if (cut == folder)
    {
        cli_line_text(said, "the program has no folder");
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
        char to[CLI_PATH_LONGEST];
        CliLine line = {to, sizeof(to), 0ull};
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
        cli_line_text(&line, one->name);
        if ((cli_line_closed(&line) == 0ull) || !query_file_copied(one->at, to))
        {
            cli_line_text(said, one->at);
            cli_line_text(said, ": it could not be copied");
            return 0;
        }
        copied(context, one->name);
    }
    return 1;
}
