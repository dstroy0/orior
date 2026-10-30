// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_writing.h: the growing line, a VerbaLinea whose flush moves it into a cell twice its room, its context the line
// itself, so a growing line is not copied while it is written; every write MMgr's own (verba_scribo/verba_linea.h)
// (file_writing.c)
#ifndef FILE_WRITING_H
#define FILE_WRITING_H

#include "locus_carcerum/locus_carcerum.h"
#include "verba_scribo/verba_linea.h"

// `line` made growing in a cell of `first` bytes of its own counted to `owner`, growing for the owner its cell counts
// to and given back with g_cli_cells->persistent_buf_release(line->out, owner) once it is read: 1, or 0 with the cell refused and the line
// full; and room made on `line` for `size` bytes more, a growing line moved into a larger cell where they do not fit:
// 1, or 0 where they do not
int cli_growing_line_made(VerbaLinea *line, size_t first, CellOwner *owner);
// the same line made in a cell of the lane the thread is on, where it is on one, given back as the lane is rewound or
// with persistent_buf_release; a line grown past it moves to a cell of the shared cells
// a growing line whose cell the call that makes it gives back, its cell and each larger one from the temporary tier
int cli_growing_line_temporary(VerbaLinea *line, size_t first, CellOwner *owner);

// a growing line written over `cell`, which its holder keeps and takes back from the line, as it may have grown
void cli_growing_line_over(VerbaLinea *line, char *cell, size_t capacity);
int cli_growing_line_room(VerbaLinea *line, size_t size);

#endif
