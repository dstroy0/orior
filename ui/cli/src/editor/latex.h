// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex.h: a LaTeX file made into pages for the live preview, from the text in the editor, saved or not.
//
// The text is written into a folder impero reserves for the file, under the file's own name where that name is a
// plain one, and made there: each make finds the last one's .aux, and its references settle. The file's own folder is
// put on TEXINPUTS after the make's; what the file inputs, the images it takes and a bibliography made beside it are
// found where the file is, and nothing is written into the tree. The engine a `% !TEX program = ...` line at the top of
// the file names makes it, else LuaLaTeX where the file takes fontspec or unicode-math, else pdfLaTeX, each instructed
// to stop at the first error.
//
// The PDF is drawn as SVG by pdftocairo, its pages sharing one set of glyphs, and the SyncTeX file the engine writes
// is read here into the boxes the text set on each page, each with the lines that set it, in the page's own points:
// the preview takes the reader from a line to its place on a page by them, and from a place on a page back to its
// line. A make that fails gives the errors the log names, each with its file and line, and no pages (latex.c,
// latex_text.c, latex_read.c, latex_sync.c)
#ifndef LATEX_H
#define LATEX_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliLatexCfg CliLatexCfg;

// how long an engine or pdftocairo may take before it is stopped, in milliseconds, and the most errors a make gives
#define CLI_LATEX_PATIENCE 120000ull
#define CLI_LATEX_ERRORS 20u

// the engine `length` bytes of LaTeX text are made with: the one a `% !TEX program = ...` line before the text's first
// command names, else lualatex where it takes fontspec or unicode-math, else pdflatex (latex_text.c)
const char *cli_latex_engine_get(const CliLatexCfg *args);

struct CliLatexCfg
{
    const char *text;
    unsigned long long length;
    const char *root;
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *out_dir;
};

typedef struct
{
    const char *(*engine_get)(const CliLatexCfg *args);
} CliLatexNs;
EMBED_TABLE_LAYOUT(CliLatexNs, engine_get);
EMBED_TABLE_STORAGE CliLatexNs cli_latex EMBED_UNUSED = {
    .engine_get = cli_latex_engine_get,
};

#endif
