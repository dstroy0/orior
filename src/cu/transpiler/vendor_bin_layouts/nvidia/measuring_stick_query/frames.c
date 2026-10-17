// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frames.c: the pieces set aside from the accumulated run read alone by a pool of reader threads, each frame read
// against the vendor, and what the cross says of an instruction read clear or held
#include "msq_internal.h"

// the pool marked failed for `why`, the first reason given kept as the pool's
static void msq_failed(MsqPool *pool, const char *why)
{
    if (!atomic_load(&pool->failed))
    {
        pool->why = why;
    }
    atomic_store(&pool->failed, 1);
}

// One piece set aside, read alone. A slot encoding the vendor stopped at in the accumulated run is illegal by that
// stop and is read no more; one set aside unread is read alone, its reading the line at its start, or illegal where
// the vendor says so. A frame is read whole (msq_frame_read). A tail's registers that address memory are read off what
// the vendor prints of it alone. 1, or 0 where the vendor could not be run or the system gave no memory
static int msq_piece_read(MsqReader *reader, MsqPool *pool, const MsqPiece *piece)
{
    if (piece->kind == MSQ_PIECE_FRAME)
    {
        return msq_frame_read(reader, &piece->bytes, &pool->frame_read[piece->index]);
    }
    if ((piece->kind == MSQ_PIECE_WORD) && (piece->state == MSQ_PIECE_STOPPED))
    {
        pool->readings[piece->index].kind = MSQ_ILLEGAL;
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
    if (piece->kind == MSQ_PIECE_WORD)
    {
        // the reading points into the listing's text, which the cross keeps until it ends
        pool->readings[piece->index] =
            (status == 0) ? msq_listing_at(&listing, 0ull) : (MsqReading){MSQ_NONE, "", NULL};
        pool->readings[piece->index].kind =
            ((status != 0) && msq_illegal_said(reader->output, (size_t)reader->output_size))
                ? MSQ_ILLEGAL
                : pool->readings[piece->index].kind;
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
    memset(&reader, 0, sizeof(reader));
    reader.output_room = 8ull << 20u;
    reader.output = (char *)malloc((size_t)reader.output_room);
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
    free(reader.output);
    free(reader.raw);
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
    memset(read, 0, sizeof(*read));
    const size_t instructions = (size_t)(frame->size / 16ull);
    unsigned char *const copy = (unsigned char *)malloc((size_t)frame->size + 1u);
    read->illegal = (unsigned long long *)malloc((instructions + 1u) * sizeof(unsigned long long));
    if ((copy == NULL) || (read->illegal == NULL))
    {
        free(copy);
        return 0;
    }
    memcpy(copy, frame->bytes, (size_t)frame->size);
    const MsqBytes run = {copy, frame->size};
    while (1)
    {
        const int status = msq_read(reader, &run, 1u);
        if (status < 0)
        {
            free(copy);
            return 0;
        }
        if (status == 0)
        {
            read->readable = msq_listing_read(reader->output, (size_t)reader->output_size, &read->listing);
            free(copy);
            return read->readable;
        }
        unsigned long long address = 0ull;
        int stepped = msq_stopped_at(reader->output, (size_t)reader->output_size, &address) &&
                      ((address % 16ull) == 0ull) && ((address + 16ull) <= frame->size);
        for (size_t at = 0u; stepped && (at < read->illegal_count); at += 1u)
        {
            stepped = (read->illegal[at] != address);
        }
        if (!stepped)
        {
            free(copy);
            return 1;
        }
        read->illegal[read->illegal_count] = address;
        read->illegal_count += 1u;
        memcpy(copy + address, s_nop, sizeof(s_nop));
    }
}

void msq_frame_free(MsqFrame *read)
{
    free(read->illegal);
    msq_listing_free(&read->listing);
    memset(read, 0, sizeof(*read));
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
    if ((length == 4u) && (memcmp(body + start, "EXIT", 4u) == 0))
    {
        return 0;
    }
    for (size_t at = 0u; at < (sizeof(s_control) / sizeof(s_control[0])); at += 1u)
    {
        if ((strlen(s_control[at]) == length) && (memcmp(body + start, s_control[at], length) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// one instruction of `question` the cross read clear, its text `body` and its sixteen bytes at `encoding`, added to
// the slots and instructions read clear the teacher reads: `<stem> <operation> <low> <high>`, the operation the
// instruction's first word short of its modifiers
void msq_clear_said(MsqHolds *holds, const MsqQuestion *question, const char *body,
                           const unsigned char *encoding)
{
    size_t start = 0u;
    const size_t word = msq_first_word(body, &start);
    size_t length = 0u;
    while ((length < word) && (body[start + length] != '.'))
    {
        length += 1u;
    }
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    memcpy(&low, encoding, 8u);
    memcpy(&high, encoding + 8u, 8u);
    char clear[400];
    const int written = snprintf(clear, sizeof(clear), "%s %.*s %016llx %016llx\n", question->stem, (int)length,
                                 body + start, low, high);
    msq_added(&holds->read, &holds->read_size, &holds->read_room, clear, (size_t)written);
}

// the count of registers that clears a hold of a register past the count, `reached` the highest register it reaches:
// one past it, or 0 where that is past MSQ_ALLOTTED_MOST
unsigned int msq_clearing(unsigned long long reached)
{
    // a count at most MSQ_ALLOTTED_MOST fits an unsigned int
    return (reached < MSQ_ALLOTTED_MOST) ? (unsigned int)(reached + 1ull) : 0u;
}

// One hold of `question`: the encoding at `encoding`, NULL where there is none to hold, added to the held list, the
// hold and `why` to the report and to what is said, and to the holds the teacher reads with `clearing`, the count of
// registers that clears it, 0 where none does
void msq_hold_said(MsqHolds *holds, const MsqQuestion *question, const unsigned char *encoding, const char *why,
                          unsigned int clearing)
{
    char where[48] = "(no encoding)";
    if (encoding != NULL)
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        memcpy(&low, encoding, 8u);
        memcpy(&high, encoding + 8u, 8u);
        char line[48];
        const int length = snprintf(line, sizeof(line), "%016llx %016llx\n", low, high);
        msq_added(&holds->held, &holds->held_size, &holds->held_room, line, (size_t)length);
        snprintf(where, sizeof(where), "%016llx %016llx", low, high);
    }
    char said[640];
    int length = snprintf(said, sizeof(said), "  %s  %s  [%s]\n", question->stem, why, where);
    msq_added(&holds->report, &holds->report_size, &holds->report_room, said, (size_t)length);
    length = snprintf(said, sizeof(said), "    %s  %s\n", question->stem, why);
    msq_added(&holds->said, &holds->said_size, &holds->said_room, said, (size_t)length);
    length = snprintf(said, sizeof(said), "%s %u %s\n", question->stem, clearing, why);
    msq_added(&holds->listed, &holds->listed_size, &holds->listed_room, said, (size_t)length);
}

// the stem of `name`: past its last `/` or `\`, short of its last `.`
void msq_stem(const char *name, char *stem, size_t room)
{
    const char *const forward = strrchr(name, '/');
    const char *const back = strrchr(name, '\\');
    const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
    const char *const base = (slash != NULL) ? (slash + 1) : name;
    const char *const dot = strrchr(base, '.');
    const size_t length = (dot != NULL) ? (size_t)(dot - base) : strlen(base);
    snprintf(stem, room, "%.*s", (int)length, base);
}

// `name` short of its last `.`, the extension past it put in its place
void msq_extension_put(const char *name, const char *extension, char *put, size_t room)
{
    const char *const forward = strrchr(name, '/');
    const char *const back = strrchr(name, '\\');
    const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
    const char *const dot = strrchr(name, '.');
    const size_t length = ((dot != NULL) && ((slash == NULL) || (dot > slash))) ? (size_t)(dot - name) : strlen(name);
    snprintf(put, room, "%.*s%s", (int)length, name, extension);
}
