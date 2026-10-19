// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.c: the dispatch: words in, lines out through a sink, the code back. Each loop runs under a context: the
// tree, the programs, the sink and the leave, which starts its run, builds its tools and runs its programs
#include "../../cli/src/text/pieces.h"
#include "query_internal.h"
#include "query_loops.h"

CliCellsTally g_query_cells = CLI_CELLS_TALLY("cli_query");

// the name of the record this process's own lines go to, `<loop>.console`
static char s_console[QRY_NAME_LONGEST + 1u];

void query_said(QueryContext *context, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    context->sink.said(context->sink.context, text, length);
    // the line handed to the run this process is a member of, as it is said; nothing where it is a member of none
    if ((s_console[0] != '\0') && (qry_member_run() != NULL))
    {
        static char s_line[QUERY_SAID_LONGEST + 1u];
        const unsigned long long kept = (length < QUERY_SAID_LONGEST) ? length : QUERY_SAID_LONGEST;
        memor.cpy(&(MemoriaCfg){.dst = s_line, .src = text, .bytes = (size_t)kept});
        s_line[kept] = '\n';
        qry_member_append(s_console, s_line, kept + 1ull, QRY_ORDINARY);
    }
}

void query_at(const QueryContext *context, const char *under, char *path)
{
    CliLine line = {path, QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, context->root);
    cli_line_character(&line, '/');
    cli_line_text(&line, under);
    cli_line_closed(&line);
}

int query_context_started(QueryContext *context, QueryRun *run, const char *folder, CliLine *said)
{
    if (context->open[0] != '\0')
    {
        return query_run_joining(run, context->root, context->open, context->programs, said);
    }
    char path[QUERY_PATH_LONGEST];
    query_at(context, folder, path);
    return query_run_started(run, context->root, path, context->programs, context, said);
}

int query_context_tool(const QueryContext *context, const char *name, char *binary, CliLine *said)
{
    return query_tool_built(context->root, context->programs, name, binary, QUERY_PATH_LONGEST, said);
}

// every setting of the context appended to the run before a program starts
static int query_settings_appended(const QueryContext *context, const QueryRun *run, CliLine *said)
{
    for (unsigned int at = 0u; at < context->settings; at += 1u)
    {
        if (!query_writer_append(&run->writer, context->setting[at].name, context->setting[at].blob,
                                 context->setting[at].size, QRY_ORDINARY, said))
        {
            return 0;
        }
    }
    return 1;
}

int query_context_teed(QueryContext *context, const QueryRun *run, const char *name, const char *const *words,
                       long long *code, CliLine *said)
{
    return query_settings_appended(context, run, said) &&
           query_writer_tee(&run->writer, context, name, words, code, said);
}

int query_context_ran(QueryContext *context, const QueryRun *run, const char *const *words, long long *code,
                      CliLine *said)
{
    if (!query_settings_appended(context, run, said))
    {
        return 0;
    }
    query_environment_set("QRY", run->writer.qry);
    query_environment_set("QRY_TOP", run->writer.top);
    query_environment_set("PATH", run->writer.path);
    QueryProcess process;
    if (!query_process_start(&process, words, context->root, QUERY_OUTPUT_OWN))
    {
        cli_line_text(said, words[0]);
        cli_line_text(said, ": it could not be started");
        return 0;
    }
    *code = query_process_wait(&process);
    return 1;
}

int query_context_handed(QueryContext *context, const QueryRun *run, const char *name, const char *const *words,
                         long long *code, CliLine *said)
{
    if (!query_settings_appended(context, run, said))
    {
        return 0;
    }
    query_environment_set("QRY", run->writer.qry);
    query_environment_set("QRY_TOP", run->writer.top);
    query_environment_set("PATH", run->writer.path);
    QueryProcess process;
    if (!query_process_start(&process, words, context->root, QUERY_OUTPUT_PIPED))
    {
        cli_line_text(said, words[0]);
        cli_line_text(said, ": it could not be started");
        return 0;
    }
    // the output read to its end a piece at a time, each piece held, and joined into a cell of its size
    char piece[1u << 16u];
    CliPieces pieces = {NULL, NULL, 0ull, 0};
    for (;;)
    {
        const unsigned long long got = query_process_read(&process, piece, sizeof(piece));
        if (got == 0ull)
        {
            break;
        }
        (void)cli_pieces_added(&pieces, piece, got);
    }
    *code = query_process_wait(&process);
    char *const output = cli_cell_taken_by(&g_query_cells, pieces.size + 1ull, "a program's output");
    if ((output == NULL) || pieces.refused)
    {
        cli_pieces_given(&pieces);
        cli_cell_given_by(&g_query_cells, output);
        cli_line_text(said, words[0]);
        cli_line_text(said, ": its output could not be held");
        return 0;
    }
    cli_pieces_copied(&pieces, output);
    const int appended = query_writer_append(&run->writer, name, output, pieces.size, QRY_ORDINARY, said);
    cli_pieces_given(&pieces);
    cli_cell_given_by(&g_query_cells, output);
    return appended;
}

int query_context_ended(QueryContext *context, QueryRun *run, CliLine *said)
{
    return query_run_ended(run, context->leave, context->asker, context, said);
}

// `--leave <word>` taken off `words`, `*count` of them, where it is there, into `leave`: 1, or 0 with why said
int query_leave_taken(const char **words, unsigned int *count, int *given, QueryLeave *leave, CliLine *said)
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
            cli_line_text(said, "--leave takes all, none or ask");
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
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "ask", .cap = CLI_CELLS_POOL}))
        {
            *leave = QUERY_LEAVE_ASK;
        }
        else
        {
            cli_line_text(said, "--leave takes all, none or ask, not ");
            cli_line_text(said, word);
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
    static char s_root[QUERY_PATH_LONGEST];
    static const char *s_words[QUERY_WORDS_MOST];
    char said_text[QUERY_SAID_LONGEST];
    CliLine said = {said_text, sizeof(said_text), 0ull};
    memor.set(&(MemoriaCfg){.dst = &s_context, .val = 0, .bytes = sizeof(s_context)});
    // a path as the compilers and the tools are given it: forward slashes throughout
    CliLine root_line = {s_root, sizeof(s_root), 0ull};
    for (const char *at = root; *at != '\0'; at += 1)
    {
        cli_line_character(&root_line, (*at == '\\') ? '/' : *at);
    }
    cli_line_closed(&root_line);
    s_context.root = s_root;
    s_context.programs = programs;
    s_context.sink = sink;
    s_context.asker = query_terminal();
    unsigned int kept = (count < QUERY_WORDS_MOST) ? count : QUERY_WORDS_MOST;
    memor.cpy(&(MemoriaCfg){.dst = s_words, .src = words, .bytes = (size_t)kept * sizeof(words[0])});
    int given = 0;
    QueryLeave leave = QUERY_LEAVE_ALL;
    int code = -1;
    if (!query_leave_taken(s_words, &kept, &given, &leave, &said))
    {
        // nothing to run
    }
    else if (kept == 0u)
    {
        cli_line_text(&said, QUERY_USAGE);
    }
    else
    {
        s_context.leave = given ? leave : (query_terminal_is() ? QUERY_LEAVE_ASK : QUERY_LEAVE_ALL);
        // the loop's own lines go to its run as `<loop>.console`
        CliLine console = {s_console, sizeof(s_console), 0ull};
        cli_line_text(&console, s_words[0]);
        cli_line_text(&console, ".console");
        cli_line_closed(&console);
        code = query_loop_run(&s_context, s_words[0], &s_words[1], kept - 1u, &said);
    }
    if ((code < 0) && (cli_line_closed(&said) != 0ull))
    {
        sink.said(sink.context, said_text, said.at);
    }
    return code;
}
