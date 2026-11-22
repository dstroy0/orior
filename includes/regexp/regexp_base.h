// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_base.h: what the regular expressions read from outside themselves: MMgr's text calls and line, and the cells
// a read and a match take. The program that links the module defines the two calls of its cells: the CLI over its own
// pool (ui/cli/src/text/regexp_cells.c), and a program of the platform over the platform's cells
// (includes/regexp/platform/regexp_cells.c)
#ifndef REGEXP_BASE_H
#define REGEXP_BASE_H

#include "embed_compiler_directives.h"
#include "verba_scribo/verba_linea.h"

// MMgr's tables of text and byte calls, which the module's C reads, and a C++ caller does not
#if !defined(__cplusplus)
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#endif

#include <stddef.h>

// the bytes a text call reads at max where the text's own terminator ends it
#define REGEXP_TEXT_CAP (~(size_t)0)

EMBED_BEGIN_DECLS

// a cell of `bytes` for the time of one read or one match, or NULL where the program's pool cannot meet it
void *regexp_cell_taken(unsigned long long bytes);

// a cell that regexp_cell_taken gave, given back; NULL gives nothing back
void regexp_cell_released(void *cell);

EMBED_END_DECLS

#endif
