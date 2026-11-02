// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex.c: a LaTeX file made into pages: the engine its text names, the programs found, the make run in orior's own
// folder for the file, and what the make left written as the preview reads it
#include "../command_line/cells.h"
#include "../text/digest.h"
#include "../text/text.h"
#include "archive.h"
#include "latex_internal.h"

CliLatexWorkState *g_cli_latex;

int cli_latex_lock(void)
{
    g_cli_latex = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_latex), &g_cli_editor_cells);
    if (g_cli_latex == NULL)
    {
    }
    return g_cli_latex != NULL;
}

void cli_latex_release(void)
{
    g_cli_cells->persistent_buf_release(g_cli_latex, &g_cli_editor_cells);
    g_cli_latex = NULL;
}

typedef struct
{
    const char *path;
    unsigned long long *start;
    unsigned long long *size;
    const char *name;
    char *program;
    const char *program_read;
    const char *const *request;
    unsigned int count;
    const char *dir;
    int *ended;
    VerbaLinea *err_msg;
    const char *log;
    unsigned long long size_value;
    const char *engine;
    VerbaLinea *out;
    const char *job;
    const char *extension;
    char *path_out;
    const char *root;
    const char *text;
    unsigned long long length;
    const char *file;
    const char *folder;
} LatexCtx;
// the stem of the file at `path`, its name without its last extension, as its start and its size
EMBED_INLINE void latex_stem(const LatexCtx *args)
{
    unsigned long long *start = args->start;
    unsigned long long *size = args->size;
    unsigned long long from = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    const unsigned long long end = from;
#if defined(_WIN32)
    from = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path, .at = from,
                      .set = "/\\");
#else
    from = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path, .at = from,
                      .set = "/");
#endif
    unsigned long long dot = end;
    dot = (from + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path + from,
                             .at = dot - from, .set = "."));
    // a name with no dot, or whose only dot opens it, is its own stem
    *start = from;
    *size = ((dot > from + 1ull) ? dot - 1ull : end) - from;
}

void cli_latex_job_get(const CliLatexFileCtx *args)
{
    unsigned long long start = 0ull;
    unsigned long long size = 0ull;
    EMBED_CALL(latex_stem, LatexCtx, .path = args->file_read, .start = &start, .size = &size);
    int plain = (size > 0ull);
    for (unsigned long long at = start; plain && (at < start + size); at += 1ull)
    {
        const char letter = args->file_read[at];
        plain = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                ((letter >= '0') && (letter <= '9')) || (letter == '-') || (letter == '_');
    }
    if (!plain)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->job_out, .src = "preview", .bytes = 8u});
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->job_out, .src = args->file_read + start, .bytes = size});
    args->job_out[size] = '\0';
}
