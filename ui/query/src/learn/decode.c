// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// decode.c: a run's tables and blobs read from the command line through the decoder (kdecode.c): `decode` hands its
// words to the decoder as they stand, `--table <table> <run.qry>`, `--blob <name> <run.qry>` and the rest, the decoder
// a member of a run of its own, and reports what the decoder handed that run as `orior_kdecode`
#include "../query_loops.h"

// the record the decoder hands its text in (KDECODE_SHOWN, kdecode_internal.h)
#define QUERY_DECODE_DISPLAYED "orior_kdecode"

int query_decode_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    const char *const *words = args->words;
    VerbaLinea *err_msg = args->err_msg;
    if (args->count == 0u)
    {
        mmgr_line_put(err_msg, "decode takes the decoder's words: [--raw] [--from <key>] [--to <key>] [--count <rows>] "
                               "--table <table> <run.qry>, or [--latest] --blob <name> <run.qry>");
        return -1;
    }
    static char s_decoder[QUERY_PATH_MAX_LEN];
    if (!EMBED_CALL(query_loops_err_msg_context.context_tool, QueryLoopsErrMsgContextCfg, .context_read = context,
                    .name = "orior_kdecode", .binary = s_decoder, .err_msg = err_msg))
    {
        return -1;
    }
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = "build/engine/kdecode/qry", .err_msg = err_msg))
    {
        return -1;
    }
    static const char *s_words[QUERY_WORDS_MAX + 2u];
    s_words[0] = s_decoder;
    for (unsigned int at = 0u; (at < args->count) && (at < QUERY_WORDS_MAX); at += 1u)
    {
        s_words[at + 1u] = words[at];
    }
    s_words[((args->count < QUERY_WORDS_MAX) ? args->count : QUERY_WORDS_MAX) + 1u] = NULL;
    long long code = -1LL;
    if (!EMBED_CALL(query_loops_context_err_msg_name_run.context_run, QueryLoopsContextErrMsgNameRunCfg,
                    .context = context, .run = &s_run, .words = s_words, .code = &code, .err_msg = err_msg))
    {
        EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                   .err_msg = err_msg);
        return -1;
    }
    unsigned long long displayed_size = 0ULL;
    char *const displayed =
        EMBED_CALL(query_count_name_internal.writer_read_alloc, QueryCountNameCtx, .writer = &s_run.writer,
                   .name = QUERY_DECODE_DISPLAYED, .size_pointer = &displayed_size);
    if (displayed != NULL)
    {
        // each line printed as it stands
        unsigned long long at = 0ULL;
        while (at < displayed_size)
        {
            const char *const line = displayed + at;
            const char *const end = (const char *)memor.chr(
                &(MemoriaCfg){.src = line, .val = '\n', .bytes = (size_t)(displayed_size - at)});
            const unsigned long long length = (end != NULL) ? (unsigned long long)(end - line) : (displayed_size - at);
            char text[QUERY_LINE_MAX_LEN];
            VerbaLinea text_line = {text, sizeof(text), 0ULL};
            mmgr_line_put_most(&text_line, line,
                               length - (((length != 0ULL) && (line[length - 1u] == '\r')) ? 1u : 0u));
            if (mmgr_line_finish(&text_line) != 0ULL)
            {
                EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = text);
            }
            at += length + 1ULL;
        }
    }
    g_cli_cells->persistent_buf_release(displayed, &g_query_cells);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg))
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}
