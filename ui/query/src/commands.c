// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// commands.c: the dispatch: words in, lines out through a sink, the code back. Each loop runs under a context: the
// tree, the programs, the sink and the leave, which starts its run, builds its tools and runs its programs
#include "../../cli/src/command_line/callback_cfg.h"
#include "../../cli/src/text/text.h"
#include "query_internal.h"
#include "query_loops.h"

// the most bytes of a program's output a run records
#define QUERY_OUTPUT_MAX (64ULL << 20u)

// the name of the record this process's own lines go to, `<loop>.console`
static char s_console[QRY_NAME_MAX_LEN + 1u];

void query_print(const QueryTextCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    EMBED_CALL(args->context->sink.print, CliEachLineSizeCfg, .context = args->context->sink.context,
               .line = args->text, .size = length);
    // the line handed to the run this process is a member of, as it is given; nothing where it is a member of none
    if ((s_console[0] != '\0') && (qry_member_run() != NULL))
    {
        static char s_line[QUERY_LINE_MAX_LEN + 1u];
        const unsigned long long copied_len = (length < QUERY_LINE_MAX_LEN) ? length : QUERY_LINE_MAX_LEN;
        memor.cpy(&(MemoriaCfg){.dst = s_line, .src = args->text, .bytes = (size_t)copied_len});
        s_line[copied_len] = '\n';
        qry_member_append(s_console, s_line, copied_len + 1ULL, QRY_ORDINARY);
    }
}

void query_at(const QueryContextUnderCtx *args)
{
    VerbaLinea line = {args->path_out, QUERY_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->context_read->root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->under);
    mmgr_line_finish(&line);
}

int query_context_start(const QueryLoopsContextRunCfg *args)
{
    QueryContext *context = args->context;
    QueryRun *run = args->run_out;
    const char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
    if (context->open[0] != '\0')
    {
        return EMBED_CALL(query_err_msg_root_internal.run_joining, QueryErrMsgRootCtx, .run = run,
                          .root = context->root, .qry = context->open, .programs = context->programs,
                          .err_msg = err_msg);
    }
    char path[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_context_under_internal.at, QueryContextUnderCtx, .context_read = context, .under = folder,
               .path_out = path);
    return EMBED_CALL(query_folder_context_internal.run_start, QueryFolderContextCtx, .run = run, .root = context->root,
                      .folder = path, .programs = context->programs, .context = context, .err_msg = err_msg);
}

int query_context_tool(const QueryLoopsErrMsgContextCfg *args)
{
    const char *name = args->name;
    char *binary = args->binary;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(query_name_capacity_internal.tool_build, QueryNameCapacityCtx, .root = args->context_read->root,
                      .programs = args->context_read->programs, .name = name, .binary = binary,
                      .capacity = QUERY_PATH_MAX_LEN, .err_msg = err_msg);
}

typedef struct
{
    const QueryContext *context;
    const QueryRun *run;
    VerbaLinea *err_msg;
} QueryCommandsCtx;
// every setting of the context appended to the run before a program starts
EMBED_INLINE int query_settings_append(const QueryCommandsCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    for (unsigned int at = 0u; at < args->context->settings; at += 1u)
    {
        if (!EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx, .writer = &args->run->writer,
                        .name = args->context->setting[at].name, .bytes = args->context->setting[at].blob,
                        .size = args->context->setting[at].size, .priority = QRY_ORDINARY, .err_msg = err_msg))
        {
            return 0;
        }
    }
    return 1;
}

int query_context_teed(const QueryLoopsContextErrMsgNameRunCfg *args)
{
    QueryContext *context = args->context;
    const char *name = args->name;
    const char *const *words = args->words;
    long long *code = args->code;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(query_settings_append, QueryCommandsCtx, .context = context, .run = args->run,
                      .err_msg = err_msg) &&
           EMBED_CALL(query_words_name_internal.writer_tee, QueryWordsNameCtx, .writer = &args->run->writer,
                      .context = context, .name = name, .words = words, .code = code, .err_msg = err_msg);
}

int query_context_run(const QueryLoopsContextErrMsgNameRunCfg *args)
{
    const char *const *words = args->words;
    long long *code = args->code;
    if (!EMBED_CALL(query_settings_append, QueryCommandsCtx, .context = args->context, .run = args->run,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY", .value = args->run->writer.qry);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY_TOP", .value = args->run->writer.top);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "PATH", .value = args->run->writer.path);
    QueryProcess process;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &process, .words = words,
                    .folder = args->context->root, .output = QUERY_OUTPUT_OWN))
    {
        mmgr_line_put(args->err_msg, words[0]);
        mmgr_line_put(args->err_msg, ": it could not be started");
        return 0;
    }
    *code = EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &process);
    return 1;
}

int query_context_dispatch(const QueryLoopsContextErrMsgNameRunCfg *args)
{
    const char *name = args->name;
    const char *const *words = args->words;
    long long *code = args->code;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(query_settings_append, QueryCommandsCtx, .context = args->context, .run = args->run,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY", .value = args->run->writer.qry);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY_TOP", .value = args->run->writer.top);
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "PATH", .value = args->run->writer.path);
    QueryProcess process;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &process, .words = words,
                    .folder = args->context->root, .output = QUERY_OUTPUT_PIPED))
    {
        mmgr_line_put(err_msg, words[0]);
        mmgr_line_put(err_msg, ": it could not be started");
        return 0;
    }
    // the output read to its end into a cell of the most it holds; what it writes past that is read and let go, for the
    // program to run to its end, and the output refused
    char *const output = g_cli_cells->temporary_buf_alloc(QUERY_OUTPUT_MAX, &g_query_cells);
    const unsigned long long capacity = (output != NULL) ? QUERY_OUTPUT_MAX - 1ULL : 0ULL;
    unsigned long long used = 0ULL;
    int whole = (output != NULL);
    for (;;)
    {
        char over[4096];
        const int fits = (used < capacity);
        const unsigned long long got =
            fits ? EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx, .process = &process,
                              .piece = output + used, .capacity_u64 = capacity - used)
                 : EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx, .process = &process,
                              .piece = over, .capacity_u64 = sizeof(over));
        if (got == 0ULL)
        {
            break;
        }
        used += fits ? got : 0ULL;
        whole = whole && fits;
    }
    *code = EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &process);
    if (!whole)
    {
        g_cli_cells->persistent_buf_release(output, &g_query_cells);
        mmgr_line_put(err_msg, words[0]);
        mmgr_line_put(err_msg, ": its output could not be stored");
        return 0;
    }
    const int appended =
        EMBED_CALL(query_name_bytes_internal.writer_append, QueryNameBytesCtx, .writer = &args->run->writer,
                   .name = name, .bytes = output, .size = used, .priority = QRY_ORDINARY, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(output, &g_query_cells);
    return appended;
}

int query_context_end(const QueryLoopsContextRunCfg *args)
{
    QueryContext *context = args->context;
    QueryRun *run = args->run_out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(query_context_err_msg_run_internal.run_end, QueryContextErrMsgRunCtx, .run = run,
                      .leave = context->leave, .prompter = context->prompter, .context = context, .err_msg = err_msg);
}

// `--leave <word>` taken off `words`, `*count` of them, where it is there, into `leave`: 1, or 0 with why given
int query_leave_alloc(const QueryLoopsErrMsgWordsCountInputCfg *args)
{
    unsigned int *count = args->count_pointer;
    int *supplied = args->supplied;
    QueryLeave *leave = args->leave;
    *supplied = 0;
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = args->words_read[at], .other = "--leave", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        if ((at + 1u) >= *count)
        {
            mmgr_line_put(args->err_msg, "--leave takes all, none or prompt");
            return 0;
        }
        const char *const word = args->words_read[at + 1u];
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
            mmgr_line_put(args->err_msg, "--leave takes all, none or prompt, not ");
            mmgr_line_put(args->err_msg, word);
            return 0;
        }
        memor.move_down(&(MemoriaCfg){.dst = &args->words_read[at],
                                      .src = &args->words_read[at + 2u],
                                      .bytes = (size_t)(*count - at - 2u) * sizeof(args->words_read[0])});
        *count -= 2u;
        *supplied = 1;
        return 1;
    }
    return 1;
}

int query_run(const QueryCfg *args)
{
    const char *root = args->root;
    const QueryPrograms *programs = args->programs;
    const char *const *words = args->words;
    unsigned int count = args->count;
    QuerySink sink = args->sink;
    static QueryContext s_context;
    static char s_root[QUERY_PATH_MAX_LEN];
    static const char *s_words[QUERY_WORDS_MAX];
    char err_buf[QUERY_LINE_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    memor.set(&(MemoriaCfg){.dst = &s_context, .val = 0, .bytes = sizeof(s_context)});
    // a path as the compilers and the tools are given it: forward slashes throughout
    VerbaLinea root_line = {s_root, sizeof(s_root), 0ULL};
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &root_line, .text = root, .from = '\\',
                     .to = '/');
    mmgr_line_finish(&root_line);
    s_context.root = s_root;
    s_context.programs = programs;
    s_context.sink = sink;
    s_context.prompter = query_terminal();
    unsigned int word_count = (count < QUERY_WORDS_MAX) ? count : QUERY_WORDS_MAX;
    memor.cpy(&(MemoriaCfg){.dst = s_words, .src = words, .bytes = (size_t)word_count * sizeof(words[0])});
    int supplied = 0;
    QueryLeave leave = QUERY_LEAVE_ALL;
    int code = -1;
    if (!EMBED_CALL(query_loops_err_msg_words_count_input.leave_alloc, QueryLoopsErrMsgWordsCountInputCfg,
                    .words_read = s_words, .count_pointer = &word_count, .supplied = &supplied, .leave = &leave,
                    .err_msg = &err_msg))
    {
        // nothing to run
    }
    else if (word_count == 0u)
    {
        mmgr_line_put(&err_msg, QUERY_USAGE);
    }
    else
    {
        s_context.leave = supplied ? leave : (query_terminal_is() ? QUERY_LEAVE_PROMPT : QUERY_LEAVE_ALL);
        // the loop's own lines go to its run as `<loop>.console`
        VerbaLinea console = {s_console, sizeof(s_console), 0ULL};
        mmgr_line_put(&console, s_words[0]);
        mmgr_line_put(&console, ".console");
        mmgr_line_finish(&console);
        code =
            EMBED_CALL(query_loops_context_err_msg_name.loop_run, QueryLoopsContextErrMsgNameCfg, .context = &s_context,
                       .name = s_words[0], .words = &s_words[1], .count = word_count - 1u, .err_msg = &err_msg);
    }
    if ((code < 0) && (mmgr_line_finish(&err_msg) != 0ULL))
    {
        EMBED_CALL(sink.print, CliEachLineSizeCfg, .context = sink.context, .line = err_buf, .size = err_msg.at);
    }
    return code;
}
