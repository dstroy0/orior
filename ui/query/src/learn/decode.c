// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// decode.c: a run's tables and blobs read from the command line through the decoder (kdecode.c): `decode` hands its
// words to the decoder as they stand, `--table <table> <run.qry>`, `--blob <name> <run.qry>` and the rest, the decoder
// a member of a run of its own, and reports what the decoder handed that run as `orior_kdecode`
#include "../query_loops.h"

// the record the decoder hands its text in (KDECODE_SHOWN, kdecode_internal.h)
#define QUERY_DECODE_SHOWN "orior_kdecode"

int query_decode_run(QueryContext *context, const char *const *words, unsigned int count, VerbaLinea *err_msg)
{
    if (count == 0u)
    {
        mmgr_line_put(err_msg, "decode takes the decoder's words: [--raw] [--from <key>] [--to <key>] [--count <rows>] "
                               "--table <table> <run.qry>, or [--latest] --blob <name> <run.qry>");
        return -1;
    }
    static char s_decoder[QUERY_PATH_MAX_LEN];
    if (!query_context_tool(context, "orior_kdecode", s_decoder, err_msg))
    {
        return -1;
    }
    static QueryRun s_run;
    if (!query_context_started(context, &s_run, "build/engine/kdecode/qry", err_msg))
    {
        return -1;
    }
    static const char *s_words[QUERY_WORDS_MOST + 2u];
    s_words[0] = s_decoder;
    for (unsigned int at = 0u; (at < count) && (at < QUERY_WORDS_MOST); at += 1u)
    {
        s_words[at + 1u] = words[at];
    }
    s_words[((count < QUERY_WORDS_MOST) ? count : QUERY_WORDS_MOST) + 1u] = NULL;
    long long code = -1ll;
    if (!query_context_ran(context, &s_run, s_words, &code, err_msg))
    {
        query_context_ended(context, &s_run, err_msg);
        return -1;
    }
    unsigned long long shown_size = 0ull;
    char *const shown = query_writer_read_taken(&s_run.writer, QUERY_DECODE_SHOWN, &shown_size);
    if (shown != NULL)
    {
        // each line printed as it stands
        unsigned long long at = 0ull;
        while (at < shown_size)
        {
            const char *const line = shown + at;
            const char *const end =
                (const char *)memor.chr(&(MemoriaCfg){.src = line, .val = '\n', .bytes = (size_t)(shown_size - at)});
            const unsigned long long length = (end != NULL) ? (unsigned long long)(end - line) : (shown_size - at);
            char text[QUERY_LINE_MAX_LEN];
            VerbaLinea text_line = {text, sizeof(text), 0ull};
            mmgr_line_put_most(&text_line, line,
                               length - (((length != 0ull) && (line[length - 1u] == '\r')) ? 1u : 0u));
            if (mmgr_line_finish(&text_line) != 0ull)
            {
                query_print(context, text);
            }
            at += length + 1ull;
        }
    }
    g_cli_cells->persistent_buf_release(shown, &g_query_cells);
    if (!query_context_ended(context, &s_run, err_msg))
    {
        return -1;
    }
    // an exit code is far below what an int represents
    return (int)code;
}
