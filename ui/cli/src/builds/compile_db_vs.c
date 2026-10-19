// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_vs.c: Visual Studio's environment, as its vcvars64.bat sets it, read once for a program that runs as cl
// does
#include "compile_db_internal.h"

#if defined(_WIN32)
// whether a letter is whitespace as a line is read
static int cli_white(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

// whether a variable's name is INCLUDE, ASCII's case aside
static int cli_named_include(const char *name)
{
    static const char s_include[8] = "include";
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        const char letter = ((name[at] >= 'A') && (name[at] <= 'Z')) ? (char)(name[at] + 32) : name[at];
        if (letter != s_include[at])
        {
            return 0;
        }
    }
    return name[7] == '\0';
}
#endif

// Visual Studio's environment once read, and whether it has been
static CliVsEnvironment s_vs;
static int s_vs_read;
static int s_vs_found;

// Visual Studio's environment read from vcvars64.bat: 1, or 0 where none is found
static int cli_vs_read(void)
{
#if defined(_WIN32)
    char base[CLI_PATH_LONGEST];
    char vswhere[CLI_PATH_LONGEST];
    if (!query_environment_read("ProgramFiles(x86)", base, sizeof(base)))
    {
        return 0;
    }
    cli_full(base, "Microsoft Visual Studio/Installer/vswhere.exe", vswhere);
    const char *const ask[9] = {vswhere,     "-latest",          "-products",
                                "*",         "-requires",        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                "-property", "installationPath", NULL};
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (!cli_ran(ask, NULL, NULL, 0u, ran, &unstarted))
    {
        cli_ran_given(ran);
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = ran->out_size;
    while ((from < to) && cli_white(ran->out[from]))
    {
        from += 1ull;
    }
    while ((to > from) && cli_white(ran->out[to - 1ull]))
    {
        to -= 1ull;
    }
    char place[CLI_PATH_LONGEST];
    const unsigned long long length = ((to - from) < CLI_PATH_LONGEST) ? (to - from) : 0ull;
    memor.cpy(&(MemoriaCfg){.dst = place, .src = ran->out + from, .bytes = length});
    place[length] = '\0';
    char vcvars[CLI_PATH_LONGEST];
    cli_full(place, "VC/Auxiliary/Build/vcvars64.bat", vcvars);
    if ((length == 0ull) || !query_is_file(vcvars))
    {
        cli_ran_given(ran);
        return 0;
    }
    char line[CLI_PATH_LONGEST * 2u];
    CliLine called = {line, sizeof(line), 0ull};
    cli_line_text(&called, "call \"");
    cli_line_text(&called, vcvars);
    cli_line_text(&called, "\" >nul && set");
    cli_line_closed(&called);
    const char *const shell[5] = {"cmd", "/d", "/c", line, NULL};
    unstarted.at = 0ull;
    if (!cli_ran(shell, NULL, NULL, 0u, ran, &unstarted) || (ran->code != 0ll))
    {
        cli_ran_given(ran);
        return 0;
    }
    CliLine kept = {s_vs.text, sizeof(s_vs.text), 0ull};
    int include = 0;
    s_vs.count = 0u;
    for (unsigned long long at = 0ull; at < ran->out_size;)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = ran->out + at, .val = '\n', .bytes = ran->out_size - at});
        // a newline found lies in the output, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - ran->out) : ran->out_size;
        unsigned long long size = end - at;
        size -= ((newline != NULL) && (size > 0ull) && (ran->out[end - 1ull] == '\r')) ? 1ull : 0ull;
        const char *const row = ran->out + at;
        at = end + 1ull;
        const char *const equals = memor.chr(&(MemoriaCfg){.src = row, .val = '=', .bytes = size});
        if ((equals == NULL) || (s_vs.count >= 1024u))
        {
            continue;
        }
        // the name before the equals sign and the value after it
        const unsigned long long name_size = (unsigned long long)(equals - row);
        const char *const name = kept.out + kept.at;
        cli_lossy(&kept, (const unsigned char *)row, name_size);
        cli_line_character(&kept, '\0');
        const char *const value = kept.out + kept.at;
        cli_lossy(&kept, (const unsigned char *)equals + 1, size - name_size - 1ull);
        cli_line_character(&kept, '\0');
        if (!cli_line_fits(&kept))
        {
            break;
        }
        s_vs.name[s_vs.count] = name;
        s_vs.value[s_vs.count] = value;
        s_vs.count += 1u;
        include = include || cli_named_include(name);
    }
    cli_ran_given(ran);
    return include;
#else
    return 0;
#endif
}

const CliVsEnvironment *cli_compile_db_vs_environment(void)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    if (!s_vs_read)
    {
        s_vs_found = cli_vs_read();
        s_vs_read = 1;
    }
    cli_lock_give(&s_lock);
    return s_vs_found ? &s_vs : NULL;
}
