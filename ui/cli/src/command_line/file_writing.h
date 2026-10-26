// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_writing.h: the growing line, a VerbaLinea whose flush moves it into a cell twice its room, its context the line
// itself, so a growing line is not copied while it is written; every write MMgr's own (verba_scribo/verba_linea.h)
// (file_writing.c)
#ifndef FILE_WRITING_H
#define FILE_WRITING_H

#include "verba_scribo/verba_linea.h"

// `line` made growing in a cell of `first` bytes of its own, given back with
// g_cli_cells->persistent_buf_release(line->out, NULL) once it is read: 1, or 0 with the cell refused and the line
// full; and room made on `line` for `size` bytes more, a growing line moved into a larger cell where they do not fit:
// 1, or 0 where they do not
int cli_growing_line_made(VerbaLinea *line, size_t first);
int cli_growing_line_room(VerbaLinea *line, size_t size);

#endif
