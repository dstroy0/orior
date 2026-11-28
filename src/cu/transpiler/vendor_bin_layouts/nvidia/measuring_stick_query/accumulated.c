// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// accumulated.c: every piece of the cross's work read as one raw run at one start of the vendor, a piece it stops at
// set aside, and the listing cut into each piece's reading
#include "../../../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "msq_internal.h"

// The pieces, `count` of them, with every one not set aside read as one raw run, each placed after the one before it.
// The vendor reads them all at one start. Where it stops, the call sets aside the piece that the stop lies in, STOPPED.
// It reads the run again without it, until it reads clean.
//
// The listing of the clean run is `listing`, with every line at its offset in that run. The `start` of each piece is
// its place in it. A stop at no address that the run holds sets every piece left aside, UNREAD, for a read alone. The
// return is 1, or 0 where the vendor could not run or the system gave no memory
int msq_accumulated_read(MsqReader *reader, MsqPiece *pieces, size_t count, MsqListing *listing)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = listing, .bytes = sizeof(*listing), .val = 0);
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

// The `listing` of the clean run, cut into each piece read in it: the lines from its start to its end. The offset of
// each line goes as its offset in the piece:
//
// - the reading of an encoding at a probe point is the line at its start, none where the vendor printed none there
// - the listing of a frame is a view of its own lines, read whole
// - the call reads from its lines the registers of a tail that address memory
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
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &frames[one->index], .bytes = sizeof(frames[one->index]),
                       .val = 0);
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
