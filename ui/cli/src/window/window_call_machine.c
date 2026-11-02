// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_machine.c: the window's calls on the machine itself: the session a window starts in, from the machine's
// start or the reader's log in to the next, the page playing the name's flight on the first start in a session and
// showing the eye in the bar at once on every start after it; and the system's picker, waited on while the reader
// picks
#include "window_internal.h"

// the longest name of a log in the system gives
#define CLI_MACHINE_LOGON_MAX_LEN 128u

// the session as the system names it, `{booted, logon}`: when the machine started, in seconds since 1970 as this start
// reads it, a few seconds either way from one start to the next; and the reader's log in, where the system names one,
// empty where it does not
static int machine_session_key(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char logon[CLI_MACHINE_LOGON_MAX_LEN];
    EMBED_CALL(cli_room_logon_internal.logon_by_name, CliRoomLogonCtx, .logon = logon, .room_size = sizeof(logon));
    mmgr_line_put(out, "{\"booted\":");
    mmgr_line_decimal(out, cli_booted_seconds(), 1u);
    mmgr_line_put(out, ",\"logon\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = logon);
    mmgr_line_ch(out, '}');
    return 1;
}

// the system's picker shown for the reader to pick a file to open, a folder, `dir` or `save-dir`, or a file to save,
// `save`: the path chosen, or null where none was
static int machine_pick(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    const char *const kind = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "kind", .otherwise = "");
    const int folder = cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "dir", .cap = 16u}) ||
                       cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "save-dir", .cap = 16u});
    const int save = cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "save", .cap = 16u});
    char path[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_room_internal.file_pick, CliPathRoomCtx,
                   .kind = folder ? CLI_PICK_FOLDER : (save ? CLI_PICK_SAVE : CLI_PICK_FILE), .path_out = path,
                   .room_size = sizeof(path)))
    {
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = path);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

const CliServeCall g_cli_window_machine_calls[] = {
    {"session_key", machine_session_key},
    {"pick", machine_pick},
};
const unsigned int g_cli_window_machine_count =
    sizeof(g_cli_window_machine_calls) / sizeof(g_cli_window_machine_calls[0]);
