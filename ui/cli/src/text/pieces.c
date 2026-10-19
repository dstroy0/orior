// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pieces.c: a text of a length not known before it is written held as it is written: a line whose flush puts each
// piece it fills in a cell of that piece's size, the cells linked in the order written, handed on in that order and
// given back once the text is
#include "pieces.h"

#include "../command_line/cells.h"

// `size` bytes of a piece put in a cell of their own after the pieces before: 1, or 0 with the pieces refused where
// the cell cannot be taken
static int pieces_put(void *context, const char *bytes, unsigned long long size)
{
    CliPieces *const pieces = (CliPieces *)context;
    CliPiece *const piece = cli_cell_taken(sizeof(CliPiece) + size, "a piece of a text held");
    if (piece == NULL)
    {
        pieces->refused = 1;
        return 0;
    }
    piece->next = NULL;
    piece->size = size;
    memor.cpy(&(MemoriaCfg){.dst = piece->bytes, .src = bytes, .bytes = (size_t)size});
    if (pieces->last != NULL)
    {
        pieces->last->next = piece;
    }
    else
    {
        pieces->first = piece;
    }
    pieces->last = piece;
    pieces->size += size;
    return 1;
}

int cli_pieces_added(CliPieces *pieces, const char *bytes, unsigned long long size)
{
    return (size == 0ull) || pieces_put(pieces, bytes, size);
}

void cli_pieces_begun(CliPieces *pieces, CliLine *line, char *piece, unsigned long long piece_size)
{
    *pieces = (CliPieces){NULL, NULL, 0ull, 0};
    *line = (CliLine){piece, piece_size, 0ull, pieces_put, pieces, 0ull};
}

int cli_pieces_ended(CliPieces *pieces, CliLine *line)
{
    return (line->at == 0ull) ? !pieces->refused : (cli_line_flushed(line) && !pieces->refused);
}

void cli_pieces_each(const CliPieces *pieces, void (*each)(void *context, const char *bytes, unsigned long long size),
                     void *context)
{
    for (const CliPiece *piece = pieces->first; piece != NULL; piece = piece->next)
    {
        each(context, piece->bytes, piece->size);
    }
}

void cli_pieces_copied(const CliPieces *pieces, char *into)
{
    unsigned long long at = 0ull;
    for (const CliPiece *piece = pieces->first; piece != NULL; piece = piece->next)
    {
        memor.cpy(&(MemoriaCfg){.dst = into + at, .src = piece->bytes, .bytes = (size_t)piece->size});
        at += piece->size;
    }
}

void cli_pieces_given(CliPieces *pieces)
{
    CliPiece *piece = pieces->first;
    while (piece != NULL)
    {
        CliPiece *const next = piece->next;
        cli_cell_given(piece);
        piece = next;
    }
    *pieces = (CliPieces){NULL, NULL, 0ull, 0};
}

char *cli_pieces_joined(CliPieces *pieces, CliLine *line, const char *what, unsigned long long *size)
{
    const int whole = cli_pieces_ended(pieces, line);
    *size = pieces->size;
    char *const joined = whole ? cli_cell_taken(*size + 1ull, what) : NULL;
    if (joined != NULL)
    {
        cli_pieces_copied(pieces, joined);
        joined[*size] = '\0';
    }
    cli_pieces_given(pieces);
    return joined;
}
