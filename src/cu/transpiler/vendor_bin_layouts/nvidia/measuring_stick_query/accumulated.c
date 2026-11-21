// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// accumulated.c: every piece of the cross's work read as one raw run at one start of the vendor, a piece it stops at
// set aside, and the listing cut into each piece's reading
#include "msq_internal.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"

// The pieces, `count` of them, every one not set aside read as one raw run, each placed after the one before it: the
// vendor reads them all at one start. Where it stops, the piece the stop lies in is set aside, STOPPED, and the run
// read again without it, until it reads clean. The clean run's listing is `listing`, every line at its offset in that
// run, and each piece's `start` its place in it. A stop at no address the run contains sets every piece left aside,
// UNREAD, to be read alone. 1, or 0 where the vendor could not be run or the system gave no memory
int msq_accumulated_read(MsqReader *reader, MsqPiece *pieces, size_t count, MsqListing *listing)
{
    mmgr_bytes_set(listing, 0, sizeof(*listing));
    MsqBytes *const runs = (MsqBytes *)platform_cell_taken((unsigned long long)((count + 1u) * sizeof(MsqBytes)),
                                                           (unsigned int)platform_place_g_program_owner, 0);
    if (runs == NULL)
    {
        return 0;
    }
    while (1)
    {
        size_t joined = 0u;
        unsigned long long at = 0ULL;
        for (size_t piece = 0u; piece < count; piece += 1u)
        {
            pieces[piece].start = at;
            if (pieces[piece].state == MSQ_PIECE_READ)
            {
                runs[joined] = pieces[piece].bytes;
                joined += 1u;
                at += pieces[piece].bytes.size;
            }
        }
        if (joined == 0u)
        {
            platform_cell_released(runs, (unsigned int)platform_place_g_program_owner);
            return 1;
        }
        const int status = msq_read(reader, runs, joined);
        if (status < 0)
        {
            platform_cell_released(runs, (unsigned int)platform_place_g_program_owner);
            return 0;
        }
        if (status == 0)
        {
            platform_cell_released(runs, (unsigned int)platform_place_g_program_owner);
            return msq_listing_read(reader->output, (size_t)reader->output_size, listing);
        }
        unsigned long long address = 0ULL;
        size_t stopped = count;
        if (msq_stopped_at(reader->output, (size_t)reader->output_size, &address) && (address < at))
        {
            // the piece the stop lies in, the last read whose start lies at or before it
            for (size_t piece = 0u; piece < count; piece += 1u)
            {
                stopped =
                    ((pieces[piece].state == MSQ_PIECE_READ) && (pieces[piece].start <= address)) ? piece : stopped;
            }
        }
        if (stopped == count)
        {
            for (size_t piece = 0u; piece < count; piece += 1u)
            {
                pieces[piece].state = (pieces[piece].state == MSQ_PIECE_READ) ? MSQ_PIECE_UNREAD : pieces[piece].state;
            }
            platform_cell_released(runs, (unsigned int)platform_place_g_program_owner);
            return 1;
        }
        pieces[stopped].state = MSQ_PIECE_STOPPED;
    }
}

// The clean run's `listing` cut into each piece read in it, the lines from its start to its end, each line's offset
// put as its offset in the piece: the reading of an encoding at a probe point the line at its start, none where the
// vendor printed none there; a frame's listing a view of its own lines, read whole; a tail's registers that address
// memory read off its lines
void msq_pieces_cut(MsqListing *listing, const MsqPiece *pieces, size_t count, MsqReading *readings, MsqFrame *frames,
                    MsqRegisters *addressed)
{
    size_t line = 0u;
    for (size_t piece = 0u; piece < count; piece += 1u)
    {
        const MsqPiece *const one = &pieces[piece];
        if (one->state != MSQ_PIECE_READ)
        {
            continue;
        }
        const unsigned long long end = one->start + one->bytes.size;
        while ((line < listing->count) && (listing->lines[line].offset < one->start))
        {
            line += 1u;
        }
        size_t last = line;
        while ((last < listing->count) && (listing->lines[last].offset < end))
        {
            listing->lines[last].offset -= one->start;
            last += 1u;
        }
        MsqListing view = {&listing->lines[line], last - line, 0u, NULL};
        if (one->type == MSQ_PIECE_WORD)
        {
            readings[one->index] = msq_listing_at(&view, 0ULL);
        }
        else if (one->type == MSQ_PIECE_FRAME)
        {
            mmgr_bytes_set(&frames[one->index], 0, sizeof(frames[one->index]));
            frames[one->index].listing = view;
            frames[one->index].readable = 1;
        }
        else
        {
            msq_addresses(listing, line, last, &addressed[one->index]);
        }
        line = last;
    }
}
