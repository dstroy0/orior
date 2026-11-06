// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// host.c: orior_cli in a Flatpak's sandbox runs the machine's own programs on the machine. Inside the sandbox none of
// the machine's compilers, SDKs and tools are there: each program on the machine's PATH is given a stand-in of the same
// name in a folder of orior's own, put first on the PATH of orior_cli and of all it starts: a script that runs the
// program on the machine through `flatpak-spawn --host`, in the folder it was started in, with the terminal's kind, the
// language, and each variable ORIOR_HOST_ENV names. A job, a tool, a language server, git and the terminal's shell all
// reach the machine's own that way. Outside a sandbox nothing changes
#include "host.h"
#include "../text/text.h"

// the programs that stay running inside the sandbox: the one that reaches the machine, and orior
static const char *const s_inside[3] = {"flatpak-spawn", "orior", "orior_cli"};

// the variables every stand-in passes to the machine's program, where they are set
static const char *const s_passed[4] = {"TERM", "COLORTERM", "LANG", "LC_ALL"};

// whether orior_cli runs inside a Flatpak's sandbox
int cli_sandbox(void)
{
#if defined(__linux__)
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = "/.flatpak-info");
#else
    return 0;
#endif
}

typedef struct
{
    VerbaLinea *line;
    const char *text;
    char *folder;
} HostCtx;
// `text` in single quotes as the shell reads one word, each quote of it closed, escaped and opened again
EMBED_INLINE void cli_shell_word(const HostCtx *args)
{
    const char *text = args->text;
    mmgr_line_ch(args->line, '\'');
    EMBED_CALL(cli_text_set_table.append_replace, CliTextSetCfg, .out = args->line, .text = text, .set = "'",
               .with = "'\\''");
    mmgr_line_ch(args->line, '\'');
}

// The stand-in for the machine's program `name`: the program and its words, after each variable passed to it, run on
// the machine in the folder the stand-in was started in, into `line`
void cli_stand_in(const CliHostCfg *args)
{
    mmgr_line_put(args->line, "#!/bin/sh\n");
    mmgr_line_put(args->line, "set -- ");
    EMBED_CALL(cli_shell_word, HostCtx, .line = args->line, .text = args->name);
    mmgr_line_put(args->line, " \"$@\"\n");
    mmgr_line_put(args->line, "for name in $ORIOR_HOST_ENV; do\n");
    mmgr_line_put(args->line, "  eval \"value=\\${$name-}\"\n");
    mmgr_line_put(args->line, "  set -- \"--env=$name=$value\" \"$@\"\n");
    mmgr_line_put(args->line, "done\n");
    mmgr_line_put(args->line, "exec flatpak-spawn --host --watch-bus --directory=\"$PWD\"");
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        mmgr_line_put(args->line, " ${");
        mmgr_line_put(args->line, s_passed[at]);
        mmgr_line_put(args->line, "+\"--env=");
        mmgr_line_put(args->line, s_passed[at]);
        mmgr_line_put(args->line, "=$");
        mmgr_line_put(args->line, s_passed[at]);
        mmgr_line_put(args->line, "\"}");
    }
    mmgr_line_put(args->line, " \"$@\"\n");
}

// the folder the stand-ins are in, into `folder`
EMBED_INLINE int cli_host_folder(const HostCtx *args)
{
    char base[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = base);
    VerbaLinea line = {args->folder, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, base);
    mmgr_line_put(&line, "/orior-host");
    return mmgr_line_finish(&line) != 0ULL;
}

// A stand-in written in `into` for each program of `names`, one a line, but those that stay running inside: how many,
// or -1 with why given
long long cli_stand_ins_write(const CliHostIntoCfg *args)
{
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->into))
    {
        mmgr_line_put(args->err_msg, args->into);
        mmgr_line_put(args->err_msg, ": it could not be made");
        return -1LL;
    }
    long long count = 0LL;
    const char *line = args->names;
    while (*line != '\0')
    {
        const size_t length = EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = line, .set = "\r\n");
        char name[CLI_NAME_MAX_LEN];
        int valid_name = (length != 0u) && (length < sizeof(name));
        if (valid_name)
        {
            memor.cpy(&(MemoriaCfg){.dst = name, .src = line, .bytes = length});
            name[length] = '\0';
            valid_name =
                (name[EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = name, .set = "/\\")] == '\0') &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "..", .cap = CLI_POOL_BYTES}));
        }
        {
            unsigned int at = 0u;
            while (valid_name && (at < 3u))
            {
                valid_name = !cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_inside[at], .cap = CLI_POOL_BYTES});
                at += 1u;
            }
        }
        if (valid_name)
        {
            char path[CLI_PATH_MAX_LEN];
            static char s_text[4096];
            VerbaLinea path_line = {path, sizeof(path), 0ULL};
            mmgr_line_put(&path_line, args->into);
            mmgr_line_ch(&path_line, '/');
            mmgr_line_put(&path_line, name);
            VerbaLinea text = {s_text, sizeof(s_text), 0ULL};
            EMBED_CALL(cli_host.stand_in, CliHostCfg, .name = name, .line = &text);
            if ((mmgr_line_finish(&path_line) == 0ULL) || !mmgr_line_ok(&text) ||
                !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                            .bytes = s_text, .size = text.at) ||
                !EMBED_CALL(cli_path_file_internal.file_runnable, CliPathFileCtx, .path = path))
            {
                mmgr_line_put(args->err_msg, path);
                mmgr_line_put(args->err_msg, ": it could not be written");
                return -1LL;
            }
            count += 1LL;
        }
        line += length;
        line += EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = line, .set = "\r\n");
    }
    return count;
}

// Inside a sandbox, a stand-in made for each program on the machine's PATH and their folder put first on orior's own
// PATH, which all it starts take: how many it made, -1 with why given, or -2 outside a sandbox. Run as orior_cli
// starts, before it starts anything else
long long cli_host_prepare(const CliHostIntoCfg *args)
{
    if (!cli_sandbox())
    {
        return -2LL;
    }
    const char *const listing[5] = {"flatpak-spawn", "--host", "sh", "-c",
                                    "IFS=:; for dir in $PATH; do ls -1 \"$dir\" 2>/dev/null; done"};
    const char *const words[6] = {listing[0], listing[1], listing[2], listing[3], listing[4], NULL};
    QueryProcess list_entries;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &list_entries, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_PIPED))
    {
        mmgr_line_put(args->err_msg, "flatpak-spawn: it could not be started");
        return -1LL;
    }
    static char s_names[1u << 22u];
    unsigned long long length = 0ULL;
    for (;;)
    {
        const unsigned long long got =
            EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx, .process = &list_entries,
                       .piece = s_names + length, .capacity_u64 = sizeof(s_names) - 1u - length);
        if (got == 0ULL)
        {
            break;
        }
        length += got;
    }
    s_names[length] = '\0';
    if (EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &list_entries) != 0LL)
    {
        mmgr_line_put(args->err_msg, "flatpak-spawn --host: it did not list the machine's programs");
        return -1LL;
    }
    char into[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_host_folder, HostCtx, .folder = into))
    {
        mmgr_line_put(args->err_msg, "the stand-ins have no folder");
        return -1LL;
    }
    const long long made = EMBED_CALL(cli_host_into.stand_ins_write, CliHostIntoCfg, .into = into, .names = s_names,
                                      .err_msg = args->err_msg);
    if (made >= 0LL)
    {
        static char s_path[32768];
        static char s_old[QUERY_ENVIRONMENT_MAX_LEN];
        VerbaLinea path = {s_path, sizeof(s_path), 0ULL};
        mmgr_line_put(&path, into);
        if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH",
                       .value_out = s_old, .capacity = sizeof(s_old)))
        {
            mmgr_line_ch(&path, ':');
            mmgr_line_put(&path, s_old);
        }
        // orior_cli starts nothing before this, and no thread of its own runs yet
        if (mmgr_line_finish(&path) != 0ULL)
        {
            EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "PATH", .value = s_path);
        }
    }
    return made;
}
