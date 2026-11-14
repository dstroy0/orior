// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_read_state.c: one script's reading stored in a cell taken for the script, each list of it sized to the
// script's bytes within the most a reading contains, and given back once what it found is read
#include "python_paths_internal.h"

// the expressions and items a reading stores past its script's bytes, the bytes of texts it stores for each byte of its
// script, past the bytes it stores for any, as a script's names are put again as the paths it joins
#define CLI_PY_SLACK 256u
#define CLI_PY_TEXT_EACH 8ULL
#define CLI_PY_TEXT_ANY (256ULL << 10u)

CliPy g_cli_py;

// the reading's cell, taken by cli_py_read_state_alloc
static unsigned char *s_read_state;

typedef struct
{
    unsigned long long at;
} PythonPathsReadingCtx;
// the first place at or past `at` bytes that is a multiple of eight, where a list of any element starts aligned
EMBED_INLINE unsigned long long cli_py_align(const PythonPathsReadingCtx *args)
{
    return (args->at + 7ULL) & ~7ULL;
}

int cli_py_read_state_alloc(const CliPythonPathsSizeCtx *args)
{
    cli_py_read_state_free();
    // a script is no more than CLI_PY_LETTERS bytes of letters, and a reading of one no more than the most of each
    const unsigned long long bytes = (args->size < CLI_PY_LETTERS) ? args->size : CLI_PY_LETTERS;
    const unsigned int exprs =
        (unsigned int)(((bytes + CLI_PY_SLACK) < CLI_PY_EXPRS) ? (bytes + CLI_PY_SLACK) : CLI_PY_EXPRS);
    const unsigned int items =
        (unsigned int)(((bytes + CLI_PY_SLACK) < CLI_PY_ITEMS) ? (bytes + CLI_PY_SLACK) : CLI_PY_ITEMS);
    const unsigned long long texts = (CLI_PY_TEXT_EACH * bytes) + CLI_PY_TEXT_ANY;
    const unsigned int text_max = (unsigned int)((texts < CLI_PY_TEXT) ? texts : CLI_PY_TEXT);
    // a logical line is no longer than the script with a space joining each of its lines, and its letters no more
    const unsigned int line_text = (unsigned int)((2ULL * bytes) + 2ULL);
    const unsigned int letters = line_text;
    const unsigned int lines = (unsigned int)(bytes + 1ULL);
    // the lists laid one after another in the cell, each at a place its elements align to
    unsigned long long at = 0ULL;
    const unsigned long long expr_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx, .at = at + ((unsigned long long)exprs * sizeof(CliExpr)));
    const unsigned long long item_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx, .at = at + ((unsigned long long)items * sizeof(unsigned int)));
    const unsigned long long stack_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx, .at = at + ((unsigned long long)items * sizeof(unsigned int)));
    const unsigned long long range_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx, .at = at + ((unsigned long long)items * sizeof(CliValues)));
    const unsigned long long letter_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx,
                    .at = at + ((unsigned long long)letters * sizeof(unsigned int)));
    const unsigned long long value_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx,
                    .at = at + ((unsigned long long)CLI_PY_VALUES * sizeof(unsigned int)));
    const unsigned long long found_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx,
                    .at = at + ((unsigned long long)CLI_PY_VALUES * sizeof(unsigned int)));
    const unsigned long long start_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx, .at = at + ((unsigned long long)lines * sizeof(unsigned int)));
    const unsigned long long length_at = at;
    at = EMBED_CALL(cli_py_align, PythonPathsReadingCtx, .at = at + ((unsigned long long)lines * sizeof(unsigned int)));
    const unsigned long long text_at = at;
    at += text_max;
    const unsigned long long line_text_at = at;
    at += line_text;
    // the reading lives for the one script: a cell of the temporary tier
    s_read_state = g_cli_guard->temporary_buf_alloc(at, g_cli_python_owner);
    if (s_read_state == NULL)
    {
        return 0;
    }
    g_cli_py.expr = (CliExpr *)(void *)(s_read_state + expr_at);
    g_cli_py.exprs_max = exprs;
    g_cli_py.item = (unsigned int *)(void *)(s_read_state + item_at);
    g_cli_py.stack = (unsigned int *)(void *)(s_read_state + stack_at);
    g_cli_py.range = (CliValues *)(void *)(s_read_state + range_at);
    g_cli_py.items_max = items;
    g_cli_py.letter = (unsigned int *)(void *)(s_read_state + letter_at);
    g_cli_py.letters_max = letters;
    g_cli_py.value = (unsigned int *)(void *)(s_read_state + value_at);
    g_cli_py.found = (unsigned int *)(void *)(s_read_state + found_at);
    g_cli_py.text = (char *)(s_read_state + text_at);
    g_cli_py.text_max = text_max;
    EMBED_CALL(cli_python_paths_text_text_max_internal.py_lines_position, CliPythonPathsTextTextMaxCtx,
               .text_out = (char *)(s_read_state + line_text_at), .text_max = line_text,
               .start = (unsigned int *)(void *)(s_read_state + start_at),
               .length_u32s = (unsigned int *)(void *)(s_read_state + length_at), .lines_max = lines);
    return 1;
}

void cli_py_read_state_free(void)
{
    g_cli_guard->persistent_buf_release(s_read_state, g_cli_python_owner);
    s_read_state = NULL;
    memor.set(&(MemoriaCfg){.dst = &g_cli_py, .val = 0, .bytes = sizeof(g_cli_py)});
    EMBED_CALL(cli_python_paths_text_text_max_internal.py_lines_position, CliPythonPathsTextTextMaxCtx,
               .text_out = NULL, .text_max = 0u, .start = NULL, .length_u32s = NULL, .lines_max = 0u);
}
