// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frames.c: the pieces set aside from the accumulated run read alone by a pool of reader threads, each frame read
// against the vendor, and what the cross reports of an instruction read clear or blocked
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "msq_internal.h"

// the pool marked failed for `why`, the first reason given recorded as the pool's
static void msq_failed(MsqPool *pool, const char *why)
{
    if (!atomic_load(&pool->failed))
    {
        pool->why = why;
    }
    atomic_store(&pool->failed, 1);
}

// One piece set aside, read alone. A probe encoding the vendor stopped at in the accumulated run is illegal by that
// stop and is read no more. One set aside unread is read alone, its reading the line at its start, or illegal where
// the vendor reports so. A frame is read whole (msq_frame_read). A tail's registers that address memory are read off
// what the vendor prints of it alone. 1, or 0 where the vendor could not be run or the system gave no memory
static int msq_piece_read(MsqReader *reader, MsqPool *pool, const MsqPiece *piece)
{
    if (piece->type == MSQ_PIECE_FRAME)
    {
        return msq_frame_read(reader, &piece->bytes, &pool->frame_read[piece->index]);
    }
    if ((piece->type == MSQ_PIECE_WORD) && (piece->state == MSQ_PIECE_STOPPED))
    {
        pool->readings[piece->index].type = MSQ_ILLEGAL;
        return 1;
    }
    const int status = msq_read(reader, &piece->bytes, 1u);
    if (status < 0)
    {
        return 0;
    }
    MsqListing listing;
    if (!msq_listing_read(reader->output, (size_t)reader->output_size, &listing))
    {
        return 0;
    }
    if (piece->type == MSQ_PIECE_WORD)
    {
        // the reading points into the listing's text, which the cross retains until it ends
        pool->readings[piece->index] =
            (status == 0) ? msq_listing_at(&listing, 0ULL) : (MsqReading){MSQ_NONE, "", NULL};
        pool->readings[piece->index].type =
            ((status != 0) && msq_illegal_found(reader->output, (size_t)reader->output_size))
                ? MSQ_ILLEGAL
                : pool->readings[piece->index].type;
        listing.text = (status == 0) ? NULL : listing.text;
        msq_listing_free(&listing);
        return 1;
    }
    msq_addresses(&listing, 0u, listing.count, &pool->addressed[piece->index]);
    msq_listing_free(&listing);
    return 1;
}

// a reader thread's work: the pieces set aside taken one after another until none is left, each read under the
// thread's own ident and stream
void *msq_pool_worker(void *argument)
{
    MsqPool *const pool = (MsqPool *)argument;
    MsqReader reader;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &reader, .bytes = sizeof(reader), .val = 0);
    reader.output_room = 8ULL << 20u;
    reader.output = (char *)platform_cell_taken((size_t)reader.output_room, platform_place_g_program_owner, 0);
    if (reader.output == NULL)
    {
        msq_failed(pool, "no memory for a reader's output");
        return NULL;
    }
    reader.stream = atomic_fetch_add(&pool->readers, 1u) + 1u;
    reader.ident = reader.stream;
    while (!atomic_load(&pool->failed))
    {
        const size_t taken = atomic_fetch_add(&pool->next, (size_t)1u);
        if (taken >= pool->aside_count)
        {
            break;
        }
        if (!msq_piece_read(&reader, pool, &pool->pieces[pool->aside[taken]]))
        {
            msq_failed(pool, "a piece set aside did not read: the disassembler could not be run on it, its listing "
                             "did not read, or there was no memory");
        }
    }
    atomic_fetch_add(&pool->calls, reader.calls);
    platform_cell_released(reader.output, platform_place_g_program_owner);
    platform_cell_released(reader.raw, platform_place_g_program_owner);
    return NULL;
}

// the processors the system gives the program
unsigned int msq_processors(void)
{
#if defined(_WIN32)
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    return (system.dwNumberOfProcessors != 0u) ? (unsigned int)system.dwNumberOfProcessors : 4u;
#else
    const long online = sysconf(_SC_NPROCESSORS_ONLN);
    return (online > 0) ? (unsigned int)online : 4u;
#endif
}

// The frame `frame` read whole by the vendor, read again with each instruction it stops at put to a NOP, until it reads
// clean or stops at no instruction it names. 1, or 0 where a reading could not be run or the system gave no memory
int msq_frame_read(MsqReader *reader, const MsqBytes *frame, MsqFrame *read)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = read, .bytes = sizeof(*read), .val = 0);
    const size_t instructions = (size_t)(frame->size / 16ULL);
    unsigned char *const copy =
        (unsigned char *)platform_cell_taken((size_t)frame->size + 1u, platform_place_g_program_owner, 0);
    read->illegal = (unsigned long long *)platform_cell_taken((instructions + 1u) * sizeof(unsigned long long),
                                                              platform_place_g_program_owner, 0);
    if ((copy == NULL) || (read->illegal == NULL))
    {
        platform_cell_released(copy, platform_place_g_program_owner);
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy, .src = frame->bytes, .bytes = (size_t)frame->size);
    const MsqBytes run = {copy, frame->size};
    while (1)
    {
        const int status = msq_read(reader, &run, 1u);
        if (status < 0)
        {
            platform_cell_released(copy, platform_place_g_program_owner);
            return 0;
        }
        if (status == 0)
        {
            read->readable = msq_listing_read(reader->output, (size_t)reader->output_size, &read->listing);
            platform_cell_released(copy, platform_place_g_program_owner);
            return read->readable;
        }
        unsigned long long address = 0ULL;
        int stepped = msq_stopped_at(reader->output, (size_t)reader->output_size, &address) &&
                      ((address % 16ULL) == 0ULL) && ((address + 16ULL) <= frame->size);
        for (size_t at = 0u; stepped && (at < read->illegal_count); at += 1u)
        {
            stepped = (read->illegal[at] != address);
        }
        if (!stepped)
        {
            platform_cell_released(copy, platform_place_g_program_owner);
            return 1;
        }
        read->illegal[read->illegal_count] = address;
        read->illegal_count += 1u;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy + address, .src = s_nop, .bytes = sizeof(s_nop));
    }
}

void msq_frame_free(MsqFrame *read)
{
    platform_cell_released(read->illegal, platform_place_g_program_owner);
    msq_listing_free(&read->listing);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = read, .bytes = sizeof(*read), .val = 0);
}

// 1 where the instruction `body` is a control transfer or a wait (s_control) other than the EXIT a kernel ends in, by
// its first word short of its modifiers
int msq_control_read(const char *body)
{
    size_t start = 0u;
    const size_t word = msq_first_word(body, &start);
    size_t length = 0u;
    while ((length < word) && (body[start + length] != '.'))
    {
        length += 1u;
    }
    if ((length == 4u) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = body + start, .other = "EXIT", .bytes = 4u) == 0))
    {
        return 0;
    }
    for (size_t at = 0u; at < (sizeof(s_control) / sizeof(s_control[0])); at += 1u)
    {
        if ((EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_control[at], .cap = PLATFORM_TEXT_TO_TERMINATOR) ==
             length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = body + start, .other = s_control[at], .bytes = length) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// one instruction of `question` the cross read clear, its text `body` and its sixteen bytes at `encoding`, added to
// the probes and instructions read clear the teacher reads: `<stem> <operation> <low> <high>`, the operation the
// instruction's first word short of its modifiers
void msq_clear_reported(MsqVerdicts *verdicts, const MsqQuestion *question, const char *body,
                        const unsigned char *encoding)
{
    size_t start = 0u;
    const size_t word = msq_first_word(body, &start);
    size_t length = 0u;
    while ((length < word) && (body[start + length] != '.'))
    {
        length += 1u;
    }
    unsigned long long low = 0ULL;
    unsigned long long high = 0ULL;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &low, .src = encoding, .bytes = 8u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &high, .src = encoding + 8u, .bytes = 8u);
    char clear[400];
    VerbaLinea formed_line_1 = mmgr_line_over(clear, sizeof(clear));
    mmgr_line_put(&formed_line_1, question->stem);
    mmgr_line_put(&formed_line_1, " ");
    mmgr_line_put_max(&formed_line_1, body + start, (size_t)((int)length));
    mmgr_line_put(&formed_line_1, " ");
    mmgr_line_hex(&formed_line_1, low, 16u);
    mmgr_line_put(&formed_line_1, " ");
    mmgr_line_hex(&formed_line_1, high, 16u);
    mmgr_line_put(&formed_line_1, "\n");
    (void)mmgr_line_finish(&formed_line_1);
    const int written = (int)formed_line_1.at;
    msq_added(&verdicts->read, &verdicts->read_size, &verdicts->read_room, clear, (size_t)written);
}

// the count of registers that clears a refusal of a register past the count, `reached` the highest register it reaches:
// one past it, or 0 where that is past MSQ_ALLOTTED_MAX
unsigned int msq_clearing(unsigned long long reached)
{
    // a count at max MSQ_ALLOTTED_MAX fits an unsigned int
    return (reached < MSQ_ALLOTTED_MAX) ? (unsigned int)(reached + 1ULL) : 0u;
}

// One refusal of `question`: the encoding at `encoding`, NULL where there is none to block, added to the gated list,
// the refusal and `why` to the report and to what is reported, and to the refusals the teacher reads with `clearing`,
// the count of registers that clears it, 0 where none does
void msq_gate_reported(MsqVerdicts *verdicts, const MsqQuestion *question, const unsigned char *encoding,
                       const char *why, unsigned int clearing)
{
    char where[48] = "(no encoding)";
    if (encoding != NULL)
    {
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &low, .src = encoding, .bytes = 8u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &high, .src = encoding + 8u, .bytes = 8u);
        char line[48];
        VerbaLinea formed_line_2 = mmgr_line_over(line, sizeof(line));
        mmgr_line_hex(&formed_line_2, low, 16u);
        mmgr_line_put(&formed_line_2, " ");
        mmgr_line_hex(&formed_line_2, high, 16u);
        mmgr_line_put(&formed_line_2, "\n");
        (void)mmgr_line_finish(&formed_line_2);
        const int length = (int)formed_line_2.at;
        msq_added(&verdicts->gated, &verdicts->gated_size, &verdicts->gated_room, line, (size_t)length);
        {
            VerbaLinea buffer_line = mmgr_line_over(where, sizeof(where));
            mmgr_line_hex(&buffer_line, low, 16u);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_hex(&buffer_line, high, 16u);
            (void)mmgr_line_finish(&buffer_line);
        }
    }
    char report_line[640];
    VerbaLinea formed_line_3 = mmgr_line_over(report_line, sizeof(report_line));
    mmgr_line_put(&formed_line_3, "  ");
    mmgr_line_put(&formed_line_3, question->stem);
    mmgr_line_put(&formed_line_3, "  ");
    mmgr_line_put(&formed_line_3, why);
    mmgr_line_put(&formed_line_3, "  [");
    mmgr_line_put(&formed_line_3, where);
    mmgr_line_put(&formed_line_3, "]\n");
    (void)mmgr_line_finish(&formed_line_3);
    int length = (int)formed_line_3.at;
    msq_added(&verdicts->report, &verdicts->report_size, &verdicts->report_room, report_line, (size_t)length);
    VerbaLinea formed_line_4 = mmgr_line_over(report_line, sizeof(report_line));
    mmgr_line_put(&formed_line_4, "    ");
    mmgr_line_put(&formed_line_4, question->stem);
    mmgr_line_put(&formed_line_4, "  ");
    mmgr_line_put(&formed_line_4, why);
    mmgr_line_put(&formed_line_4, "\n");
    (void)mmgr_line_finish(&formed_line_4);
    length = (int)formed_line_4.at;
    msq_added(&verdicts->report, &verdicts->report_size, &verdicts->report_room, report_line, (size_t)length);
    VerbaLinea formed_line_5 = mmgr_line_over(report_line, sizeof(report_line));
    mmgr_line_put(&formed_line_5, question->stem);
    mmgr_line_put(&formed_line_5, " ");
    mmgr_line_decimal(&formed_line_5, clearing, 1u);
    mmgr_line_put(&formed_line_5, " ");
    mmgr_line_put(&formed_line_5, why);
    mmgr_line_put(&formed_line_5, "\n");
    (void)mmgr_line_finish(&formed_line_5);
    length = (int)formed_line_5.at;
    msq_added(&verdicts->listed, &verdicts->listed_size, &verdicts->listed_room, report_line, (size_t)length);
}

// the stem of `name`: past its last `/` or `\`, short of its last `.`
void msq_stem(const char *name, char *stem, size_t room)
{
    const char *const forward =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
    const char *const base = (slash != NULL) ? (slash + 1) : name;
    const char *const dot =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = base, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    const size_t length =
        (dot != NULL) ? (size_t)(dot - base)
                      : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = base, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    {
        VerbaLinea buffer_line = mmgr_line_over(stem, room);
        mmgr_line_put_max(&buffer_line, base, (size_t)((int)length));
        (void)mmgr_line_finish(&buffer_line);
    }
}

// `name` short of its last `.`, the extension past it put in its place
void msq_extension_put(const char *name, const char *extension, char *put, size_t room)
{
    const char *const forward =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
    const char *const dot =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    const size_t length =
        ((dot != NULL) && ((slash == NULL) || (dot > slash)))
            ? (size_t)(dot - name)
            : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    {
        VerbaLinea buffer_line = mmgr_line_over(put, room);
        mmgr_line_put_max(&buffer_line, name, (size_t)((int)length));
        mmgr_line_put(&buffer_line, extension);
        (void)mmgr_line_finish(&buffer_line);
    }
}
