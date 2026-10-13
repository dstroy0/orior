// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frames.c: the readings: the questions read in chunks by reader threads, the tails read a code at a time, each frame read against the vendor, and what the cross says of an instruction read clear or held
#include "msq_internal.h"

// The slots of one chunk read: the run read again with every slot the vendor calls illegal put to a NOP, until it reads
// clean, and that clean run gives every legal slot's operation at once; a chunk takes a reading for each illegal it
// holds and one more
static void msq_chunk_read(MsqReader *reader, const MsqDistinct *words, size_t first, size_t count,
                           MsqReading *readings, _Atomic int *failed)
{
    MsqBytes runs[MSQ_CHUNK];
    unsigned char illegal[MSQ_CHUNK];
    memset(illegal, 0, sizeof(illegal));
    for (size_t at = 0u; at < count; at += 1u)
    {
        runs[at] = words->items[first + at];
    }
    MsqBytes nop = {s_nop, sizeof(s_nop)};
    while (1)
    {
        const int status = msq_read(reader, runs, count);
        if (status < 0)
        {
            atomic_store(failed, 1);
            return;
        }
        if (status == 0)
        {
            MsqListing listing;
            if (!msq_listing_read(reader->output, (size_t)reader->output_size, &listing))
            {
                atomic_store(failed, 1);
                return;
            }
            for (size_t at = 0u; at < count; at += 1u)
            {
                if (illegal[at])
                {
                    readings[first + at].kind = MSQ_ILLEGAL;
                }
                else
                {
                    readings[first + at] = msq_listing_at(&listing, (unsigned long long)at * 16ull);
                }
            }
            msq_listing_free(&listing);
            return;
        }
        unsigned long long address = 0ull;
        const int named = msq_stopped_at(reader->output, (size_t)reader->output_size, &address);
        const unsigned long long index = address / 16ull;
        if (!named || (index >= count) || illegal[index])
        {
            msq_readings_each(reader, words, first, count, illegal, readings, failed);
            return;
        }
        illegal[index] = 1u;
        runs[index] = nop;
    }
}

// a reader thread's work: chunks taken one after another until none is left, each read under the chunk's own ident
void *msq_chunks_worker(void *argument)
{
    MsqChunks *const work = (MsqChunks *)argument;
    MsqReader reader;
    memset(&reader, 0, sizeof(reader));
    reader.output_room = 8ull << 20u;
    reader.output = (char *)malloc((size_t)reader.output_room);
    if (reader.output == NULL)
    {
        atomic_store(&work->failed, 1);
        return NULL;
    }
    // the thread's own stream, from 1: the reader after the chunks hands the stream 0
    reader.stream = atomic_fetch_add(&work->readers, 1u) + 1u;
    for (size_t chunk = atomic_fetch_add(&work->next, 1u); chunk < work->chunks;
         chunk = atomic_fetch_add(&work->next, 1u))
    {
        // the chunk's index names its blobs
        reader.ident = (unsigned int)chunk;
        reader.calls = 0ull;
        const size_t first = chunk * work->chunk_size;
        const size_t count = ((first + work->chunk_size) <= work->words->count) ? work->chunk_size
                                                                                 : (work->words->count - first);
        msq_chunk_read(&reader, work->words, first, count, work->readings, &work->failed);
        atomic_fetch_add(&work->calls, reader.calls);
    }
    free(reader.output);
    free(reader.raw);
    return NULL;
}

// a reader thread's work over the frames: frames taken one after another until none is left, each read whole under
// the thread's own ident and stream, both past every chunk reader's
void *msq_frames_worker(void *argument)
{
    MsqFrames *const work = (MsqFrames *)argument;
    MsqReader reader;
    memset(&reader, 0, sizeof(reader));
    reader.output_room = 8ull << 20u;
    reader.output = (char *)malloc((size_t)reader.output_room);
    if (reader.output == NULL)
    {
        atomic_store(&work->failed, 1);
        return NULL;
    }
    reader.stream = atomic_fetch_add(&work->readers, 1u) + 1u;
    reader.ident = 0xfffffffeu - reader.stream;
    for (size_t frame = atomic_fetch_add(&work->next, 1u); frame < work->count;
         frame = atomic_fetch_add(&work->next, 1u))
    {
        reader.calls = 0ull;
        if (!msq_frame_read(&reader, &work->frames[frame], &work->read[frame]))
        {
            atomic_store(&work->failed, 1);
        }
        atomic_fetch_add(&work->calls, reader.calls);
    }
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

// The registers each distinct code past a slot reads an address from, by the code: the codes read together as one raw
// run, each one's listing the lines at its own offsets. The vendor stops at the first instruction it calls illegal: the
// codes before it are taken from that run, the code it stops in is read alone, and the reading goes on past it. A stop
// that names no address, or one past the run, is read again a code at a time. 1, or 0 where a reading could not be run
int msq_tails_read(MsqReader *reader, const MsqDistinct *tails, MsqRegisters *addressed)
{
    size_t from = 0u;
    unsigned long long *const starts = (unsigned long long *)malloc((tails->count + 1u) * sizeof(unsigned long long));
    if (starts == NULL)
    {
        return 0;
    }
    int whole = 1;
    while (whole && (from < tails->count))
    {
        const size_t count = tails->count - from;
        unsigned long long at = 0ull;
        for (size_t place = 0u; place < count; place += 1u)
        {
            starts[place] = at;
            at += tails->items[from + place].size;
        }
        const int status = msq_read(reader, &tails->items[from], count);
        if (status < 0)
        {
            whole = 0;
            break;
        }
        unsigned long long address = 0ull;
        const int named = (status != 0) && msq_stopped_at(reader->output, (size_t)reader->output_size, &address);
        MsqListing listing;
        if ((status != 0) && (!named || (address >= at)))
        {
            for (size_t place = 0u; whole && (place < count); place += 1u)
            {
                whole = (msq_read(reader, &tails->items[from + place], 1u) >= 0) &&
                        msq_listing_read(reader->output, (size_t)reader->output_size, &listing);
                if (whole)
                {
                    msq_addresses(&listing, 0u, listing.count, &addressed[from + place]);
                    msq_listing_free(&listing);
                }
            }
            break;
        }
        // the code the vendor stopped in, the last whose start lies at or before the stop
        size_t stopped = count;
        if (named)
        {
            stopped = 0u;
            while (((stopped + 1u) < count) && (starts[stopped + 1u] <= address))
            {
                stopped += 1u;
            }
        }
        if (!msq_listing_read(reader->output, (size_t)reader->output_size, &listing))
        {
            whole = 0;
            break;
        }
        // each code before the stop takes the lines at its own offsets, the first at or past its start and before its
        // end
        size_t line = 0u;
        for (size_t place = 0u; place < stopped; place += 1u)
        {
            const unsigned long long begin = starts[place];
            const unsigned long long end = begin + tails->items[from + place].size;
            while ((line < listing.count) && (listing.lines[line].offset < begin))
            {
                line += 1u;
            }
            size_t last = line;
            while ((last < listing.count) && (listing.lines[last].offset < end))
            {
                last += 1u;
            }
            msq_addresses(&listing, line, last, &addressed[from + place]);
        }
        msq_listing_free(&listing);
        if (stopped == count)
        {
            break;
        }
        whole = (msq_read(reader, &tails->items[from + stopped], 1u) >= 0) &&
                msq_listing_read(reader->output, (size_t)reader->output_size, &listing);
        if (whole)
        {
            msq_addresses(&listing, 0u, listing.count, &addressed[from + stopped]);
            msq_listing_free(&listing);
        }
        from += stopped + 1u;
    }
    free(starts);
    return whole;
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
