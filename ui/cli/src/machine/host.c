// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// host.c: orior in a Flatpak's sandbox runs the machine's own programs on the machine. Inside the sandbox none of the
// machine's compilers, SDKs and tools are there: each program on the machine's PATH is given a stand-in of the same
// name in a folder of orior's own, put first on the PATH of orior and of all it starts: a script that runs the program
// on the machine through `flatpak-spawn --host`, in the folder it was started in, with the terminal's kind, the
// language, and each variable ORIOR_HOST_ENV names. A job, a tool, a language server, git and the terminal's shell all
// reach the machine's own that way. Outside a sandbox nothing changes
#include "host.h"
#include "../text/text.h"

// the programs that keep running inside the sandbox: the one that reaches the machine, and orior
static const char *const s_inside[3] = {"flatpak-spawn", "orior", "orior-cli"};

// the variables every stand-in passes to the machine's program, where they are set
static const char *const s_passed[4] = {"TERM", "COLORTERM", "LANG", "LC_ALL"};

// whether orior runs inside a Flatpak's sandbox
int cli_sandboxed(void)
{
#if defined(__linux__)
    return query_is_file("/.flatpak-info");
#else
    return 0;
#endif
}

// `text` in single quotes as the shell reads one word, each quote of it closed, escaped and opened again
static void cli_shell_word(CliLine *line, const char *text)
{
    cli_line_character(line, '\'');
    for (const char *at = text; *at != '\0'; at += 1)
    {
        if (*at == '\'')
        {
            cli_line_text(line, "'\\''");
        }
        else
        {
            cli_line_character(line, *at);
        }
    }
    cli_line_character(line, '\'');
}

// The stand-in for the machine's program `name`: the program and its words, after each variable passed to it, run on
// the machine in the folder the stand-in was started in, into `line`
void cli_stand_in(const char *name, CliLine *line)
{
    cli_line_text(line, "#!/bin/sh\n");
    cli_line_text(line, "set -- ");
    cli_shell_word(line, name);
    cli_line_text(line, " \"$@\"\n");
    cli_line_text(line, "for name in $ORIOR_HOST_ENV; do\n");
    cli_line_text(line, "  eval \"value=\\${$name-}\"\n");
    cli_line_text(line, "  set -- \"--env=$name=$value\" \"$@\"\n");
    cli_line_text(line, "done\n");
    cli_line_text(line, "exec flatpak-spawn --host --watch-bus --directory=\"$PWD\"");
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        cli_line_text(line, " ${");
        cli_line_text(line, s_passed[at]);
        cli_line_text(line, "+\"--env=");
        cli_line_text(line, s_passed[at]);
        cli_line_text(line, "=$");
        cli_line_text(line, s_passed[at]);
        cli_line_text(line, "\"}");
    }
    cli_line_text(line, " \"$@\"\n");
}

// the folder the stand-ins are in, into `folder`
static int cli_host_folder(char *folder)
{
    char base[CLI_PATH_LONGEST];
    if (!query_environment_read("XDG_RUNTIME_DIR", base, sizeof(base)) &&
        !query_environment_read("TMPDIR", base, sizeof(base)))
    {
        memor.cpy(&(MemoriaCfg){.dst = base,
                                .src = "/tmp",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "/tmp", .cap = CLI_CELLS_POOL}) + 1u});
    }
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, base);
    cli_line_text(&line, "/orior-host");
    return cli_line_closed(&line) != 0ull;
}

// A stand-in written in `into` for each program of `names`, one a line, but those that keep running inside: how many,
// or -1 with why said
long long cli_stand_ins_written(const char *into, const char *names, CliLine *said)
{
    if (!query_folder_made(into))
    {
        cli_line_text(said, into);
        cli_line_text(said, ": it could not be made");
        return -1ll;
    }
    long long count = 0ll;
    const char *line = names;
    while (*line != '\0')
    {
        const size_t length = cli_text_upto(line, "\r\n");
        char name[CLI_NAME_LONGEST];
        int kept = (length != 0u) && (length < sizeof(name));
        if (kept)
        {
            memor.cpy(&(MemoriaCfg){.dst = name, .src = line, .bytes = length});
            name[length] = '\0';
            kept = (name[cli_text_upto(name, "/\\")] == '\0') &&
                   (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "..", .cap = CLI_CELLS_POOL}));
        }
        for (unsigned int at = 0u; kept && (at < 3u); at += 1u)
        {
            kept = !cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_inside[at], .cap = CLI_CELLS_POOL});
        }
        if (kept)
        {
            char path[CLI_PATH_LONGEST];
            static char s_text[4096];
            CliLine path_line = {path, sizeof(path), 0ull};
            cli_line_text(&path_line, into);
            cli_line_character(&path_line, '/');
            cli_line_text(&path_line, name);
            CliLine text = {s_text, sizeof(s_text), 0ull};
            cli_stand_in(name, &text);
            if ((cli_line_closed(&path_line) == 0ull) || !cli_line_fits(&text) ||
                !query_file_written(path, s_text, text.at) || !cli_file_runnable(path))
            {
                cli_line_text(said, path);
                cli_line_text(said, ": it could not be written");
                return -1ll;
            }
            count += 1ll;
        }
        line += length;
        line += cli_text_over(line, "\r\n");
    }
    return count;
}

// Inside a sandbox, a stand-in made for each program on the machine's PATH and their folder put first on orior's own
// PATH, which all it starts take: how many it made, -1 with why said, or -2 outside a sandbox. Run as orior starts,
// before it starts anything else
long long cli_host_prepared(CliLine *said)
{
    if (!cli_sandboxed())
    {
        return -2ll;
    }
    const char *const listing[5] = {"flatpak-spawn", "--host", "sh", "-c",
                                    "IFS=:; for dir in $PATH; do ls -1 \"$dir\" 2>/dev/null; done"};
    const char *const words[6] = {listing[0], listing[1], listing[2], listing[3], listing[4], NULL};
    QueryProcess listed;
    if (!query_process_start(&listed, words, NULL, QUERY_OUTPUT_PIPED))
    {
        cli_line_text(said, "flatpak-spawn: it could not be started");
        return -1ll;
    }
    static char s_names[1u << 22u];
    unsigned long long length = 0ull;
    for (;;)
    {
        const unsigned long long got = query_process_read(&listed, s_names + length, sizeof(s_names) - 1u - length);
        if (got == 0ull)
        {
            break;
        }
        length += got;
    }
    s_names[length] = '\0';
    if (query_process_wait(&listed) != 0ll)
    {
        cli_line_text(said, "flatpak-spawn --host: it did not list the machine's programs");
        return -1ll;
    }
    char into[CLI_PATH_LONGEST];
    if (!cli_host_folder(into))
    {
        cli_line_text(said, "the stand-ins have no folder");
        return -1ll;
    }
    const long long made = cli_stand_ins_written(into, s_names, said);
    if (made >= 0ll)
    {
        static char s_path[32768];
        static char s_old[QUERY_ENVIRONMENT_LONGEST];
        CliLine path = {s_path, sizeof(s_path), 0ull};
        cli_line_text(&path, into);
        if (query_environment_read("PATH", s_old, sizeof(s_old)))
        {
            cli_line_character(&path, ':');
            cli_line_text(&path, s_old);
        }
        // orior starts nothing before this, and no thread of its own runs yet
        if (cli_line_closed(&path) != 0ull)
        {
            query_environment_set("PATH", s_path);
        }
    }
    return made;
}
