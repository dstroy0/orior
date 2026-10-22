// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_reading.c: one script's reading held in a cell taken for the script, each list of it sized to the
// script's bytes within the most a reading holds, and given back once what it found is read
#include "python_paths_internal.h"

// the expressions and items a reading holds past its script's bytes, the bytes of texts it holds for each byte of its
// script, past the bytes it holds for any, as a script's names are put again as the paths it joins
#define CLI_PY_SLACK 256u
#define CLI_PY_TEXT_EACH 8ull
#define CLI_PY_TEXT_ANY (256ull << 10u)

CliPy g_cli_py;
CliCellsTally g_cli_python_cells = CLI_CELLS_TALLY("cli_python_paths");

// the reading's cell, taken by cli_py_reading_taken
static unsigned char *s_reading;

// the first place at or past `at` bytes that is a multiple of eight, where a list of any element starts aligned
static unsigned long long cli_py_aligned(unsigned long long at)
{
    return (at + 7ull) & ~7ull;
}

int cli_py_reading_taken(unsigned long long size)
{
    cli_py_reading_given();
    // a script is no more than CLI_PY_LETTERS bytes of letters, and a reading of one no more than the most of each
    const unsigned long long bytes = (size < CLI_PY_LETTERS) ? size : CLI_PY_LETTERS;
    const unsigned int exprs =
        (unsigned int)(((bytes + CLI_PY_SLACK) < CLI_PY_EXPRS) ? (bytes + CLI_PY_SLACK) : CLI_PY_EXPRS);
    const unsigned int items =
        (unsigned int)(((bytes + CLI_PY_SLACK) < CLI_PY_ITEMS) ? (bytes + CLI_PY_SLACK) : CLI_PY_ITEMS);
    const unsigned long long texts = (CLI_PY_TEXT_EACH * bytes) + CLI_PY_TEXT_ANY;
    const unsigned int text_most = (unsigned int)((texts < CLI_PY_TEXT) ? texts : CLI_PY_TEXT);
    // a logical line is no longer than the script with a space joining each of its lines, and its letters no more
    const unsigned int line_text = (unsigned int)((2ull * bytes) + 2ull);
    const unsigned int letters = line_text;
    const unsigned int lines = (unsigned int)(bytes + 1ull);
    // the lists laid one after another in the cell, each at a place its elements align to
    unsigned long long at = 0ull;
    const unsigned long long expr_at = at;
    at = cli_py_aligned(at + ((unsigned long long)exprs * sizeof(CliExpr)));
    const unsigned long long item_at = at;
    at = cli_py_aligned(at + ((unsigned long long)items * sizeof(unsigned int)));
    const unsigned long long stack_at = at;
    at = cli_py_aligned(at + ((unsigned long long)items * sizeof(unsigned int)));
    const unsigned long long range_at = at;
    at = cli_py_aligned(at + ((unsigned long long)items * sizeof(CliValues)));
    const unsigned long long letter_at = at;
    at = cli_py_aligned(at + ((unsigned long long)letters * sizeof(unsigned int)));
    const unsigned long long value_at = at;
    at = cli_py_aligned(at + ((unsigned long long)CLI_PY_VALUES * sizeof(unsigned int)));
    const unsigned long long found_at = at;
    at = cli_py_aligned(at + ((unsigned long long)CLI_PY_VALUES * sizeof(unsigned int)));
    const unsigned long long start_at = at;
    at = cli_py_aligned(at + ((unsigned long long)lines * sizeof(unsigned int)));
    const unsigned long long length_at = at;
    at = cli_py_aligned(at + ((unsigned long long)lines * sizeof(unsigned int)));
    const unsigned long long text_at = at;
    at += text_most;
    const unsigned long long line_text_at = at;
    at += line_text;
    s_reading = cli_cell_taken_by(&g_cli_python_cells, at, "a Python script's reading");
    if (s_reading == NULL)
    {
        return 0;
    }
    g_cli_py.expr = (CliExpr *)(void *)(s_reading + expr_at);
    g_cli_py.exprs_most = exprs;
    g_cli_py.item = (unsigned int *)(void *)(s_reading + item_at);
    g_cli_py.stack = (unsigned int *)(void *)(s_reading + stack_at);
    g_cli_py.range = (CliValues *)(void *)(s_reading + range_at);
    g_cli_py.items_most = items;
    g_cli_py.letter = (unsigned int *)(void *)(s_reading + letter_at);
    g_cli_py.letters_most = letters;
    g_cli_py.value = (unsigned int *)(void *)(s_reading + value_at);
    g_cli_py.found = (unsigned int *)(void *)(s_reading + found_at);
    g_cli_py.text = (char *)(s_reading + text_at);
    g_cli_py.text_most = text_most;
    cli_py_lines_placed((char *)(s_reading + line_text_at), line_text, (unsigned int *)(void *)(s_reading + start_at),
                        (unsigned int *)(void *)(s_reading + length_at), lines);
    return 1;
}

void cli_py_reading_given(void)
{
    cli_cell_given_by(&g_cli_python_cells, s_reading);
    s_reading = NULL;
    memor.set(&(MemoriaCfg){.dst = &g_cli_py, .val = 0, .bytes = sizeof(g_cli_py)});
    cli_py_lines_placed(NULL, 0u, NULL, NULL, 0u);
}
