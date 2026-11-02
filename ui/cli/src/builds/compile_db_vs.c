// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_vs.c: Visual Studio's environment, as its vcvars64.bat sets it, read once for a program that runs as cl
// does
#include "../text/text.h"
#include "compile_db_internal.h"

typedef struct
{
#if (defined(_WIN32))
    char letter;
    const char *name;
#endif
} CompileDbVsCtx;

#if defined(_WIN32)
// whether a letter is whitespace as a line is read
EMBED_INLINE int cli_white(const CompileDbVsCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

// whether a variable's name is INCLUDE, ASCII's case aside
EMBED_INLINE int cli_name_include(const CompileDbVsCtx *args)
{
    static const char s_include[8] = "include";
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        const char letter =
            ((args->name[at] >= 'A') && (args->name[at] <= 'Z')) ? (char)(args->name[at] + 32) : args->name[at];
        if (letter != s_include[at])
        {
            return 0;
        }
    }
    return args->name[7] == '\0';
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
    char base[CLI_PATH_MAX_LEN];
    char vswhere[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ProgramFiles(x86)",
                    .value_out = base, .room = sizeof(base)))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base,
               .relative = "Microsoft Visual Studio/Installer/vswhere.exe", .joined = vswhere);
    const char *const vswhere_args[9] = {
        vswhere,     "-latest",          "-products",
        "*",         "-requires",        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property", "installationPath", NULL};
    CliRan *const ran = cli_ran_take();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = vswhere_args, .folder = NULL, .settings = NULL,
                    .count = 0u, .ran = ran, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = ran->out_size;
    while ((from < to) && EMBED_CALL(cli_white, CompileDbVsCtx, .letter = ran->out[from]))
    {
        from += 1ull;
    }
    while ((to > from) && EMBED_CALL(cli_white, CompileDbVsCtx, .letter = ran->out[to - 1ull]))
    {
        to -= 1ull;
    }
    char place[CLI_PATH_MAX_LEN];
    const unsigned long long length = ((to - from) < CLI_PATH_MAX_LEN) ? (to - from) : 0ull;
    memor.cpy(&(MemoriaCfg){.dst = place, .src = ran->out + from, .bytes = length});
    place[length] = '\0';
    char vcvars[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = place, .relative = "VC/Auxiliary/Build/vcvars64.bat",
               .joined = vcvars);
    if ((length == 0ull) || !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = vcvars))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea called = {line, sizeof(line), 0ull};
    mmgr_line_put(&called, "call \"");
    mmgr_line_put(&called, vcvars);
    mmgr_line_put(&called, "\" >nul && set");
    mmgr_line_finish(&called);
    const char *const shell[5] = {"cmd", "/d", "/c", line, NULL};
    unstarted.at = 0ull;
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = shell, .folder = NULL, .settings = NULL, .count = 0u,
                    .ran = ran, .err_msg = &unstarted) ||
        (ran->code != 0ll))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    VerbaLinea vs_text = {s_vs.text, sizeof(s_vs.text), 0ull};
    int include = 0;
    s_vs.count = 0u;
    for (unsigned long long at = 0ull; at < ran->out_size;)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = ran->out, .at = at, .size = ran->out_size);
        const unsigned long long end = bounds.stop;
        unsigned long long size = end - at;
        size -= bounds.stop - bounds.cut;
        const char *const row = ran->out + at;
        at = end + 1ull;
        const char *const equals = memor.chr(&(MemoriaCfg){.src = row, .val = '=', .bytes = size});
        if ((equals == NULL) || (s_vs.count >= 1024u))
        {
            continue;
        }
        // the name before the equals sign and the value after it
        const unsigned long long name_size = (unsigned long long)(equals - row);
        const char *const name = vs_text.out + vs_text.at;
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &vs_text,
                   .text_bytes = (const unsigned char *)row, .length = name_size);
        mmgr_line_ch(&vs_text, '\0');
        const char *const value = vs_text.out + vs_text.at;
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &vs_text,
                   .text_bytes = (const unsigned char *)equals + 1, .length = size - name_size - 1ull);
        mmgr_line_ch(&vs_text, '\0');
        if (!mmgr_line_ok(&vs_text))
        {
            break;
        }
        s_vs.name[s_vs.count] = name;
        s_vs.value[s_vs.count] = value;
        s_vs.count += 1u;
        include = include || EMBED_CALL(cli_name_include, CompileDbVsCtx, .name = name);
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return include;
#else
    return 0;
#endif
}

const CliVsEnvironment *cli_compile_db_vs_environment(void)
{
    if (!s_vs_read)
    {
        s_vs_found = cli_vs_read();
        s_vs_read = 1;
    }
    return s_vs_found ? &s_vs : NULL;
}
