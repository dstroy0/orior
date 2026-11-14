// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// binary_report.c: what a report on a program reports, line by line, and the libraries it finds through a PATH copied
// beside the program
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "binary.h"

typedef struct
{
    QuerySink sink;
    const char *first;
    const char *name;
    const char *by;
    const char *middle;
    const char *at;
    const char *last;
} BinaryReportCtx;

// one line of a report reported
EMBED_INLINE void cli_report_line_print(const BinaryReportCtx *args)
{
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea made = {line, sizeof(line), 0ULL};
    mmgr_line_put(&made, args->first);
    mmgr_line_put(&made, args->name);
    if (args->by != NULL)
    {
        mmgr_line_put(&made, ", which ");
        mmgr_line_put(&made, args->by);
        mmgr_line_put(&made, " loads, ");
    }
    mmgr_line_put(&made, args->middle);
    mmgr_line_put(&made, args->at);
    mmgr_line_put(&made, args->last);
    const unsigned long long length = mmgr_line_finish(&made);
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = line, .size = length);
}

void cli_binary_report_print(const CliBinaryReportCfg *args)
{
    for (unsigned int at = 0u; at < args->report_read->libraries; at += 1u)
    {
        const CliLibrary *const one = &args->report_read->library[at];
        const char *const where = one->has_at ? one->at : "";
        switch (one->found)
        {
        case CLI_FOUND_ORIOR_PATH:
            EMBED_CALL(
                cli_report_line_print, BinaryReportCtx, .sink = args->sink, .first = "", .name = one->name,
                .by = one->by, .middle = "is found through the folders impero puts on its PATH, at ", .at = where,
                .last = ", and not where the program is opened on its own: Copy Libraries Beside It puts it beside "
                        "the program");
            break;
        case CLI_FOUND_SYSTEM_PATH:
            EMBED_CALL(
                cli_report_line_print, BinaryReportCtx, .sink = args->sink, .first = "", .name = one->name,
                .by = one->by, .middle = "is found through this machine's PATH, at ", .at = where,
                .last = ", which another machine's PATH may not contain: Copy Libraries Beside It puts it beside the "
                        "program");
            break;
        case CLI_FOUND_MISSING:
            EMBED_CALL(cli_report_line_print, BinaryReportCtx, .sink = args->sink, .first = "", .name = one->name,
                       .by = one->by, .middle = "is found nowhere: the program does not start without it", .at = "",
                       .last = "");
            break;
        case CLI_FOUND_DEBUG_RUNTIME:
            EMBED_CALL(cli_report_line_print, BinaryReportCtx, .sink = args->sink, .first = "", .name = one->name,
                       .by = one->by,
                       .middle =
                           "is Visual Studio's debug runtime, which only a machine with Visual Studio has: a release "
                           "build loads the runtime any machine can install",
                       .at = "", .last = "");
            break;
        case CLI_FOUND_REDISTRIBUTABLE:
            EMBED_CALL(cli_report_line_print, BinaryReportCtx, .sink = args->sink, .first = "", .name = one->name,
                       .by = one->by,
                       .middle = "comes with the Visual C++ Redistributable, which a machine without it lacks",
                       .at = "", .last = "");
            break;
        case CLI_FOUND_BUILD_PATH:
            EMBED_CALL(cli_report_line_print, BinaryReportCtx, .sink = args->sink, .first = "", .name = one->name,
                       .by = one->by, .middle = "is found by a run path of the machine that built it, at ", .at = where,
                       .last = "");
            break;
        default:
            break;
        }
    }
    for (unsigned int at = 0u; at < args->report_read->build_paths; at += 1u)
    {
        EMBED_CALL(
            cli_report_line_print, BinaryReportCtx, .sink = args->sink,
            .first = "the program looks for its libraries in ", .name = args->report_read->build_path[at], .by = NULL,
            .middle =
                ", a folder of the machine that built it; a run path from $ORIGIN finds them beside the program on "
                "any machine",
            .at = "", .last = "");
    }
    if (args->report_read->has_glibc)
    {
        const char *const version = (cellul.starts(&(CatenaFinitaCfg){
                                        .src = args->report_read->glibc, .other = "GLIBC_", .cap = CLI_POOL_BYTES}))
                                        ? (args->report_read->glibc + 6)
                                        : args->report_read->glibc;
        EMBED_CALL(cli_report_line_print, BinaryReportCtx, .sink = args->sink, .first = "the program requires glibc ",
                   .name = version, .by = NULL,
                   .middle =
                       " or newer, and does not start on a system with an older one; a Rust musl target, or Go with "
                       "CGO_ENABLED 0, builds a program that requires none",
                   .at = "", .last = "");
    }
}

int cli_binary_print(const CliBinaryPathOriorPathCountCfg *args)
{
    QuerySink sink = args->sink;
    static CliBinaryReport s_report;
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    const int read =
        EMBED_CALL(cli_binary_path_orior_path.read, CliBinaryPathOriorPathCfg, .path = args->path,
                   .orior_path = args->orior_path, .count = args->count, .report = &s_report, .err_msg = &err_msg);
    if (read)
    {
        EMBED_CALL(cli_binary_report.report_print, CliBinaryReportCfg, .report_read = &s_report, .sink = sink);
    }
    return read;
}

int cli_binary_copy_adjacent(const CliBinaryPathCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = folder,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    char *cut = folder + cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES});
    cut = folder + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = folder,
                              .at = (unsigned long long)(cut - folder), .set = "/\\");
    if (cut == folder)
    {
        mmgr_line_put(args->err_msg, "the program has no folder");
        return 0;
    }
    cut[-1] = '\0';
    for (unsigned int at = 0u; at < args->report_read->libraries; at += 1u)
    {
        const CliLibrary *const one = &args->report_read->library[at];
        if (!one->has_at || ((one->found != CLI_FOUND_ORIOR_PATH) && (one->found != CLI_FOUND_SYSTEM_PATH)))
        {
            continue;
        }
        char to[CLI_PATH_MAX_LEN];
        VerbaLinea line = {to, sizeof(to), 0ULL};
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, one->name);
        if ((mmgr_line_finish(&line) == 0ULL) ||
            !EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = one->at, .to = to))
        {
            mmgr_line_put(args->err_msg, one->at);
            mmgr_line_put(args->err_msg, ": it could not be copied");
            return 0;
        }
        EMBED_CALL(args->copied, CliEachNameCfg, .context = args->context, .name = one->name);
    }
    return 1;
}
