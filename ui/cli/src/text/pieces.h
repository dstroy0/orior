// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pieces.h: a text held in pieces as it is written, each piece a cell of its size (pieces.c)
#ifndef PIECES_H
#define PIECES_H

#include "../command_line/cli_internal.h"

// A piece of a text held: the piece after it, its size, and its bytes
typedef struct CliPieceStruct
{
    struct CliPieceStruct *next;
    unsigned long long size;
    char bytes[];
} CliPiece;

// A text held in pieces: the first and the last, the bytes of all of them, and 1 in `refused` where a piece's cell
// could not be taken and the text is not whole
typedef struct
{
    CliPiece *first;
    CliPiece *last;
    unsigned long long size;
    int refused;
} CliPieces;

// `line` made over `piece`, which holds `piece_size`, a line whose every piece is held in `pieces`; what the line
// holds yet put after them: 1, or 0 where the text is not whole; the pieces handed to `each` in order, or copied one
// after another into `into`, which holds their size; and the pieces given back
void cli_pieces_begun(CliPieces *pieces, CliLine *line, char *piece, unsigned long long piece_size);
// `size` bytes put after the pieces held, a piece of their own: 1, or 0 where its cell could not be taken
int cli_pieces_added(CliPieces *pieces, const char *bytes, unsigned long long size);
int cli_pieces_ended(CliPieces *pieces, CliLine *line);
void cli_pieces_each(const CliPieces *pieces, void (*each)(void *context, const char *bytes, unsigned long long size),
                     void *context);
void cli_pieces_copied(const CliPieces *pieces, char *into);
void cli_pieces_given(CliPieces *pieces);
// the line ended and its pieces joined into a cell of their size and a 0 after, its size into `size`, the pieces
// given back: the cell, or NULL where the text is not whole or the cell could not be taken
char *cli_pieces_joined(CliPieces *pieces, CliLine *line, const char *what, unsigned long long *size);

#endif
