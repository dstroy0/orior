// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// listing.c: the vendor's listing: the disassembler run over a container and its output read, the listing cut into
// lines, each line's guard, word and body, the line at an address, and the address the vendor stopped at
#include "../../../../../../includes/platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "msq_internal.h"
QryShared *s_run = NULL;
const char *s_nvdisasm = NULL;
const char *s_architecture = NULL;

// the time that the readings spent to give their runs to the streams of the writer, and the time that the vendor took
// over them. The cross sums each over every reader, as its account of where its time went
_Atomic unsigned long long s_streamed_nanoseconds;
_Atomic unsigned long long s_vendor_nanoseconds;

// the host's clock in nanoseconds, from a start of its own and only ever read against itself
unsigned long long msq_now(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (unsigned long long)(((double)counter.QuadPart * 1000000000.0) / (double)frequency.QuadPart);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((unsigned long long)now.tv_sec * 1000000000ULL) + (unsigned long long)now.tv_nsec;
#endif
}

// the bytes `size` of them at `bytes` added to the end of `*memory`, which contains `*size_used` of `*room`: 1, or 0
// where the system gives no memory
int msq_added(unsigned char **memory, size_t *size_used, size_t *room, const void *bytes, size_t size)
{
    if ((*size_used + size) > *room)
    {
        size_t grown = (*room != 0u) ? *room : 65536u;
        while (grown < (*size_used + size))
        {
            grown *= 2u;
        }
        unsigned char *const larger =
            (unsigned char *)platform_cell_grown(*memory, *room, grown, platform_place_g_program_owner, 0);
        if (larger == NULL)
        {
            return 0;
        }
        *memory = larger;
        *room = grown;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = *memory + *size_used, .src = bytes, .bytes = size);
    *size_used += size;
    return 1;
}

// `count` runs at `runs`, read by the vendor as one raw run. The call passes them to the writer of the run as the
// stream of the reader of the .qry (qry_streamed). It points the disassembler at that stream, and runs it over it. It
// stores everything that it printed in the output of the reader, and passes it to the run as the trace of the cross.
//
// Only the writer of the run writes a file. The return is 0 where the disassembler ended clean, 1 where it did not,
// and -1 where it could not run
int msq_read(MsqReader *reader, const MsqBytes *runs, size_t count)
{
    reader->calls += 1ULL;
    reader->raw_size = 0u;
    int joined = 1;
    for (size_t at = 0u; joined && (at < count); at += 1u)
    {
        // a run is at max a container's code, which a size_t counts
        joined = (runs[at].size == 0ULL) ||
                 msq_added(&reader->raw, &reader->raw_size, &reader->raw_room, runs[at].bytes, (size_t)runs[at].size);
    }
    char stream_name[64];
    char stream_path[1400];
    {
        VerbaLinea buffer_line = mmgr_line_over(stream_name, sizeof(stream_name));
        mmgr_line_put(&buffer_line, "msq_stream_");
        mmgr_line_decimal(&buffer_line, reader->stream, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    const unsigned long long streamed_began = msq_now();
    const int streamed = joined && (qry_member_streamed(stream_name, reader->raw, (unsigned long long)reader->raw_size,
                                                        stream_path, sizeof(stream_path)) != 0ULL);
    atomic_fetch_add(&s_streamed_nanoseconds, msq_now() - streamed_began);
    if (!streamed)
    {
        return -1;
    }
    // the interface's command is a list of words it does not write to; the casts only meet its declared type
    char *const command[] = {(char *)s_nvdisasm, (char *)"--binary", (char *)s_architecture, stream_path, NULL};
    InterfaceAnswer answer;
    const unsigned long long vendor_began = msq_now();
    // the call reads again where the output ran past the room that it got, with the room grown to hold it
    for (unsigned int tried = 0u; tried < 2u; tried += 1u)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &answer, .bytes = sizeof(answer), .val = 0);
        answer.output = reader->output;
        answer.output_capacity = reader->output_room;
        const InterfaceProbe probe = {command, NULL, 0ULL};
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        if ((interface_probe_run(&probe, &answer, &error) != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            return -1;
        }
        if (answer.output_bytes < reader->output_room)
        {
            break;
        }
        const unsigned long long room = answer.output_bytes + 65536ULL;
        char *const larger = (char *)platform_cell_grown(reader->output, reader->output_room, (size_t)room,
                                                         platform_place_g_program_owner, 0);
        if (larger == NULL)
        {
            return -1;
        }
        reader->output = larger;
        reader->output_room = room;
    }
    atomic_fetch_add(&s_vendor_nanoseconds, msq_now() - vendor_began);
    reader->output_size =
        (answer.output_bytes < reader->output_room) ? answer.output_bytes : (reader->output_room - 1ULL);
    // what the vendor printed of the run, the cross's trace (qry_member.h)
    if (qry_member_run() != NULL)
    {
        char name[64];
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "run_");
            mmgr_line_decimal(&buffer_line, reader->ident, 1u);
            mmgr_line_put(&buffer_line, "_");
            mmgr_line_decimal(&buffer_line, reader->calls, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        qry_member_append(name, reader->output, reader->output_size, QRY_ORDINARY);
    }
    return ((answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ULL)) ? 0 : 1;
}

// 1 where `c` is a space as the vendor's listing reads it. Explicit cast reads the byte as its code point
int msq_space(char c)
{
    return EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_SPACE, .byte = (uint8_t)c);
}

// 1 where `c` is a character, a digit or `_`. Explicit cast reads the byte as its code point
int msq_word(char c)
{
    return EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (uint8_t)c) || (c == '_');
}

void msq_listing_free(MsqListing *listing)
{
    // a view owns neither room nor text: its lines are another listing's
    if (listing->room != 0u)
    {
        platform_cell_released(listing->lines, platform_place_g_program_owner);
    }
    platform_cell_released(listing->text, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = listing, .bytes = sizeof(*listing), .val = 0);
}

// Every instruction of the listing `printed` of `length` bytes, in the order printed. It is a comment of four or more
// hex digits that gives its offset, `/*<offset>*/`, then space, then its text to the first `;` on its line. Its text
// past a guard, `@P0 `, `@!UPT `, is its body. The listing stores one copy of the text, with the text of each
// instruction ended in it where it ends. The return is 1, or 0 where the system gives no memory
int msq_listing_read(const char *printed, size_t length, MsqListing *listing)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = listing, .bytes = sizeof(*listing), .val = 0);
    char *const text = (char *)platform_cell_taken(length + 1u, platform_place_g_program_owner, 0);
    if (text == NULL)
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = text, .src = printed, .bytes = length);
    text[length] = '\0';
    listing->text = text;
    size_t at = 0u;
    while ((at + 1u) < length)
    {
        const char *const open =
            (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = text + at, .bytes = length - at, .val = '/');
        if (open == NULL)
        {
            break;
        }
        size_t walk = (size_t)(open - text);
        at = walk + 1u;
        if (((walk + 1u) >= length) || (text[walk + 1u] != '*'))
        {
            continue;
        }
        walk += 2u;
        unsigned long long offset = 0ULL;
        size_t digits = 0u;
        // Explicit cast reads the byte as its code point. The listing writes its offsets in lower case hex
        while ((walk < length) &&
               (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)text[walk]) ||
                ((text[walk] >= 'a') && (text[walk] <= 'f'))))
        {
            offset = (offset << 4u) |
                     (unsigned long long)((text[walk] <= '9') ? (text[walk] - '0') : (text[walk] - 'a' + 10));
            digits += 1u;
            walk += 1u;
        }
        if ((digits < 4u) || ((walk + 1u) >= length) || (text[walk] != '*') || (text[walk + 1u] != '/'))
        {
            continue;
        }
        walk += 2u;
        if ((walk >= length) || !msq_space(text[walk]))
        {
            continue;
        }
        walk += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[walk], .cap = length - walk, .other = MSQ_SPACES);
        // the text to the first `;` on its line
        const size_t end = walk + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[walk], .cap = length - walk,
                                             .other = ";\n");
        if ((end >= length) || (text[end] != ';'))
        {
            continue;
        }
        at = end + 1u;
        size_t last = end;
        while ((last > walk) && msq_space(text[last - 1u]))
        {
            last -= 1u;
        }
        // the guard, `@`, an optional `!`, an optional `U`, `P`, then digits or `T`, then space
        size_t body = walk;
        if ((body < last) && (text[body] == '@'))
        {
            size_t guard = body + 1u;
            guard += ((guard < last) && (text[guard] == '!')) ? 1u : 0u;
            guard += ((guard < last) && (text[guard] == 'U')) ? 1u : 0u;
            if ((guard < last) && (text[guard] == 'P'))
            {
                guard += 1u;
                const size_t first_mark = guard;
                guard += (guard < last) ? EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[guard],
                                                     .cap = last - guard, .other = "T0123456789")
                                        : 0u;
                if ((guard > first_mark) && (guard < last) && msq_space(text[guard]))
                {
                    guard += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[guard], .cap = last - guard,
                                        .other = MSQ_SPACES);
                    body = guard;
                }
            }
        }
        if (listing->count == listing->room)
        {
            const size_t room = (listing->room != 0u) ? (listing->room * 2u) : 1024u;
            MsqLine *const larger =
                (MsqLine *)platform_cell_grown(listing->lines, listing->room * sizeof(MsqLine), room * sizeof(MsqLine),
                                               platform_place_g_program_owner, 0);
            if (larger == NULL)
            {
                return 0;
            }
            listing->lines = larger;
            listing->room = room;
        }
        MsqLine *const line = &listing->lines[listing->count];
        line->offset = offset;
        line->raw = text + walk;
        line->body = text + body;
        // the instruction's text ended in the copy where it ends, past which the walk has already read
        text[last] = '\0';
        listing->count += 1u;
    }
    return 1;
}

// the reading of the instruction at `offset` that the listing printed, the last printed there. It is its operation,
// the first word of its body before any `.`, and its body
MsqReading msq_listing_at(const MsqListing *listing, unsigned long long offset)
{
    MsqReading reading;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &reading, .bytes = sizeof(reading), .val = 0);
    for (size_t at = listing->count; at > 0u; at -= 1u)
    {
        const MsqLine *const line = &listing->lines[at - 1u];
        if (line->offset != offset)
        {
            continue;
        }
        reading.type = MSQ_READ;
        // the operation up to its space or its point, cut to the room
        const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line->body,
                                         .cap = sizeof(reading.operation) - 1u, .other = MSQ_SPACES ".");
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = reading.operation, .src = line->body, .bytes = length);
        reading.operation[length] = '\0';
        reading.body = line->body;
        break;
    }
    return reading;
}

// the address the vendor stopped at, read off `at address 0x<hex>`: 1, or 0 where it names none
int msq_stopped_at(const char *text, size_t length, unsigned long long *address)
{
    static const char s_address_marker[] = "at address 0x";
    const size_t marker_length = sizeof(s_address_marker) - 1u;
    // each marker in the text, reached from the one before it. Explicit casts read each found byte as an offset into
    // the text
    for (const char *found = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = text, .cap = length,
                                        .other = s_address_marker, .other_len = marker_length);
         found != NULL; found = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = found + 1,
                                           .cap = length - ((size_t)(found - text) + 1u), .other = s_address_marker,
                                           .other_len = marker_length))
    {
        size_t walk = (size_t)(found - text) + marker_length;
        unsigned long long value = 0ULL;
        size_t digits = 0u;
        // Explicit cast reads the byte as its code point. The listing writes its offsets in lower case hex
        while ((walk < length) &&
               (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)text[walk]) ||
                ((text[walk] >= 'a') && (text[walk] <= 'f'))))
        {
            value = (value << 4u) |
                    (unsigned long long)((text[walk] <= '9') ? (text[walk] - '0') : (text[walk] - 'a' + 10));
            digits += 1u;
            walk += 1u;
        }
        if (digits != 0u)
        {
            *address = value;
            return 1;
        }
    }
    return 0;
}
