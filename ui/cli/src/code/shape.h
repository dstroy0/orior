// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape.h: structural search and replace: code found by its shape, and each match written again from a template.
//
// A pattern is code of the file's language with placeholders in it: `$x$` stands for an expression, `$x:name$` for one
// name, and `$x:stmt$` for one statement or more. The pattern and the code are each read as tokens, names, numbers,
// strings and operators, past spaces and comments: a match does not hang on how the code is laid out. In Python a
// line's end outside brackets ends a statement, and elsewhere a semicolon or a block's closing brace does. An
// expression is a run of tokens whose brackets close inside it and that ends no statement; it takes as few tokens as
// let the rest of the pattern match, and, last in the pattern, the rest of its statement. Statements are taken as few
// as let the rest match, and one where they end the pattern. A placeholder named twice matches the same code both
// times. Matches do not overlap, the first found in the text taken first.
//
// A template is text with the same placeholders, each written as the code its placeholder took (shape.c,
// shape_match.c, shape_search.c)
#ifndef SHAPE_H
#define SHAPE_H

#include "../text/edits.h"

// A match: its span, its first line as the file has it, its whitespace at its end taken off, and what the template
// writes in its place where one is given
typedef struct
{
    CliTextPlace from;
    CliTextPlace to;
    const char *line;
    unsigned long long line_size;
    const char *with;
    unsigned long long with_size;
    int has_with;
} CliShapeFound;

// A match found, handed on with its texts kept while the call lasts
typedef void (*CliShapeEach)(void *context, const CliShapeFound *found);

// every match of `pattern` in `size` bytes of `src`, a file of `language`, each handed to `each` under the search's
// lock with what `template_text` writes in its place, where it is not NULL; a match whose written text does not fit
// 1 MiB is handed on without it
void cli_shape_search(const char *language, const char *src, unsigned long long size, const char *pattern,
                      const char *template_text, CliShapeEach each, void *context);

// a match of the file at `path` written as the page reads one, `{"path", "from", "to", "line", "with"}`
void cli_shape_found_written(const char *path, const CliShapeFound *found, CliLine *out);

#endif
