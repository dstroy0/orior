// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.c: the dispatch: words in, lines out through a sink, the code back. Each loop runs under a context: the
// tree, the programs, the sink and the leave, which starts its run, builds its tools and runs its programs
#include "../../cli/src/text/text.h"
#include "query_internal.h"
#include "query_loops.h"

// the most bytes of a program's output a run records
#define QUERY_OUTPUT_MOST (64ull << 20u)

// the name of the record this process's own lines go to, `<loop>.console`
static char s_console[QRY_NAME_MAX_LEN + 1u];

void query_print(QueryContext *context, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    context->sink.print(context->sink.context, text, length);
    // the line handed to the run this process is a member of, as it is given; nothing where it is a member of none
    if ((s_console[0] != '\0') && (qry_member_run() != NULL))
    {
        static char s_line[QUERY_LINE_MAX_LEN + 1u];
        const unsigned long long copied_len = (length < QUERY_LINE_MAX_LEN) ? length : QUERY_LINE_MAX_LEN;
        memor.cpy(&(MemoriaCfg){.dst = s_line, .src = text, .bytes = (size_t)copied_len});
        s_line[copied_len] = '\n';
        qry_member_append(s_console, s_line, copied_len + 1ull, QRY_ORDINARY);
    }
}

void query_at(const QueryContext *context, const char *under, char *path)
{
    VerbaLinea line = {path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, context->root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, under);
    mmgr_line_finish(&line);
}

int query_context_started(QueryContext *context, QueryRun *run, const char *folder, VerbaLinea *err_msg)
{
    if (context->open[0] != '\0')
    {
        return query_run_joining(run, context->root, context->open, context->programs, err_msg);
    }
    char path[QUERY_PATH_MAX_LEN];
    query_at(context, folder, path);
    return query_run_started(run, context->root, path, context->programs, context, err_msg);
}

int query_context_tool(const QueryContext *context, const char *name, char *binary, VerbaLinea *err_msg)
{
    return query_tool_built(context->root, context->programs, name, binary, QUERY_PATH_MAX_LEN, err_msg);
}

// every setting of the context appended to the run before a program starts
static int query_settings_appended(const QueryContext *context, const QueryRun *run, VerbaLinea *err_msg)
{
    for (unsigned int at = 0u; at < context->settings; at += 1u)
    {
        if (!query_writer_append(&run->writer, context->setting[at].name, context->setting[at].blob,
                                 context->setting[at].size, QRY_ORDINARY, err_msg))
        {
            return 0;
        }
    }
    return 1;
}

int query_context_teed(QueryContext *context, const QueryRun *run, const char *name, const char *const *words,
                       long long *code, VerbaLinea *err_msg)
{
    return query_settings_appended(context, run, err_msg) &&
           query_writer_tee(&run->writer, context, name, words, code, err_msg);
}

int query_context_ran(QueryContext *context, const QueryRun *run, const char *const *words, long long *code,
                      VerbaLinea *err_msg)
{
    if (!query_settings_appended(context, run, err_msg))
    {
        return 0;
    }
    query_environment_set("QRY", run->writer.qry);
    query_environment_set("QRY_TOP", run->writer.top);
    query_environment_set("PATH", run->writer.path);
    QueryProcess process;
    if (!query_process_start(&process, words, context->root, QUERY_OUTPUT_OWN))
    {
        mmgr_line_put(err_msg, words[0]);
        mmgr_line_put(err_msg, ": it could not be started");
        return 0;
    }
    *code = query_process_wait(&process);
    return 1;
}

int query_context_handed(QueryContext *context, const QueryRun *run, const char *name, const char *const *words,
                         long long *code, VerbaLinea *err_msg)
{
    if (!query_settings_appended(context, run, err_msg))
    {
        return 0;
    }
    query_environment_set("QRY", run->writer.qry);
    query_environment_set("QRY_TOP", run->writer.top);
    query_environment_set("PATH", run->writer.path);
    QueryProcess process;
    if (!query_process_start(&process, words, context->root, QUERY_OUTPUT_PIPED))
    {
        mmgr_line_put(err_msg, words[0]);
        mmgr_line_put(err_msg, ": it could not be started");
        return 0;
    }
    // the output read to its end into a cell of the most it holds; what it writes past that is read and let go, for the
    // program to run to its end, and the output refused
    char *const output = g_cli_cells->temporary_buf_alloc(QUERY_OUTPUT_MOST, &g_query_cells);
    const unsigned long long room = (output != NULL) ? QUERY_OUTPUT_MOST - 1ull : 0ull;
    unsigned long long used = 0ull;
    int whole = (output != NULL);
    for (;;)
    {
        char over[4096];
        const int fits = (used < room);
        const unsigned long long got = fits ? query_process_read(&process, output + used, room - used)
                                            : query_process_read(&process, over, sizeof(over));
        if (got == 0ull)
        {
            break;
        }
        used += fits ? got : 0ull;
        whole = whole && fits;
    }
    *code = query_process_wait(&process);
    if (!whole)
    {
        g_cli_cells->persistent_buf_release(output, &g_query_cells);
        mmgr_line_put(err_msg, words[0]);
        mmgr_line_put(err_msg, ": its output could not be stored");
        return 0;
    }
    const int appended = query_writer_append(&run->writer, name, output, used, QRY_ORDINARY, err_msg);
    g_cli_cells->persistent_buf_release(output, &g_query_cells);
    return appended;
}

int query_context_ended(QueryContext *context, QueryRun *run, VerbaLinea *err_msg)
{
    return query_run_ended(run, context->leave, context->prompter, context, err_msg);
}

// `--leave <word>` taken off `words`, `*count` of them, where it is there, into `leave`: 1, or 0 with why given
int query_leave_taken(const char **words, unsigned int *count, int *given, QueryLeave *leave, VerbaLinea *err_msg)
{
    *given = 0;
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = words[at], .other = "--leave", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        if ((at + 1u) >= *count)
        {
            mmgr_line_put(err_msg, "--leave takes all, none or prompt");
            return 0;
        }
        const char *const word = words[at + 1u];
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "all", .cap = CLI_CELLS_POOL}))
        {
            *leave = QUERY_LEAVE_ALL;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "none", .cap = CLI_CELLS_POOL}))
        {
            *leave = QUERY_LEAVE_NONE;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "prompt", .cap = CLI_CELLS_POOL}))
        {
            *leave = QUERY_LEAVE_PROMPT;
        }
        else
        {
            mmgr_line_put(err_msg, "--leave takes all, none or prompt, not ");
            mmgr_line_put(err_msg, word);
            return 0;
        }
        memor.move_down(&(MemoriaCfg){
            .dst = &words[at], .src = &words[at + 2u], .bytes = (size_t)(*count - at - 2u) * sizeof(words[0])});
        *count -= 2u;
        *given = 1;
        return 1;
    }
    return 1;
}

int query_run(const char *root, const QueryPrograms *programs, const char *const *words, unsigned int count,
              QuerySink sink)
{
    static QueryContext s_context;
    static char s_root[QUERY_PATH_MAX_LEN];
    static const char *s_words[QUERY_WORDS_MOST];
    char err_buf[QUERY_LINE_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ull};
    memor.set(&(MemoriaCfg){.dst = &s_context, .val = 0, .bytes = sizeof(s_context)});
    // a path as the compilers and the tools are given it: forward slashes throughout
    VerbaLinea root_line = {s_root, sizeof(s_root), 0ull};
    (void)cli_text_put_swapped(&root_line, root, '\\', '/');
    mmgr_line_finish(&root_line);
    s_context.root = s_root;
    s_context.programs = programs;
    s_context.sink = sink;
    s_context.prompter = query_terminal();
    unsigned int word_count = (count < QUERY_WORDS_MOST) ? count : QUERY_WORDS_MOST;
    memor.cpy(&(MemoriaCfg){.dst = s_words, .src = words, .bytes = (size_t)word_count * sizeof(words[0])});
    int given = 0;
    QueryLeave leave = QUERY_LEAVE_ALL;
    int code = -1;
    if (!query_leave_taken(s_words, &word_count, &given, &leave, &err_msg))
    {
        // nothing to run
    }
    else if (word_count == 0u)
    {
        mmgr_line_put(&err_msg, QUERY_USAGE);
    }
    else
    {
        s_context.leave = given ? leave : (query_terminal_is() ? QUERY_LEAVE_PROMPT : QUERY_LEAVE_ALL);
        // the loop's own lines go to its run as `<loop>.console`
        VerbaLinea console = {s_console, sizeof(s_console), 0ull};
        mmgr_line_put(&console, s_words[0]);
        mmgr_line_put(&console, ".console");
        mmgr_line_finish(&console);
        code = query_loop_run(&s_context, s_words[0], &s_words[1], word_count - 1u, &err_msg);
    }
    if ((code < 0) && (mmgr_line_finish(&err_msg) != 0ull))
    {
        sink.print(sink.context, err_buf, err_msg.at);
    }
    return code;
}
