// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill.h: Fill Paragraph: the paragraph of documentation at a line written again to a margin, its words filled into as
// few lines as hold them.
//
// A paragraph is the lines around the line that share its comment's mark and its indent, as `#`, `//`, `///` and `--`
// write a comment a line, up to a line of the mark alone; the lines of a Python docstring around it, up to a blank line
// or the docstring's quotes, the opening quotes kept on the first line and the closing quotes on the last; or, in plain
// text, the lines around it up to a blank line. A comment of `/** */` is written again whole: each paragraph of its
// text filled, and each run of its tags of one kind lined up, `@param` and the rest, the names, then the descriptions,
// starting in one column, a description that runs past the margin going on under its own first word (fill.c,
// fill_comment.c)
#ifndef FILL_H
#define FILL_H

#include "../text/edits.h"

// the paragraph of documentation at `line` of `size` bytes of `text` filled to `width` columns: the edit that writes
// it again into `edit`, its text written into `written`: 1, or 0 with why said, or with nothing said where the
// paragraph does not fit `written`
int cli_fill(const char *text, unsigned long long size, unsigned int line, unsigned int width, CliLine *written,
             CliTextEdit *edit, CliLine *said);

#endif
