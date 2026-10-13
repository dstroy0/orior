// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// listing.c: the vendor's listing: the disassembler run over a container and its output read, the listing cut into lines, each line's guard, word and body, the line at an address, and the address the vendor stopped at
#include "msq_internal.h"
QryBuffer *s_run = NULL;
const char *s_nvdisasm = NULL;
const char *s_architecture = NULL;

// the bytes `size` of them at `bytes` added to the end of `*memory`, which holds `*held` of `*room`: 1, or 0 where the
// system gives no memory
int msq_added(unsigned char **memory, size_t *held, size_t *room, const void *bytes, size_t size)
{
    if ((*held + size) > *room)
    {
        size_t grown = (*room != 0u) ? *room : 65536u;
        while (grown < (*held + size))
        {
            grown *= 2u;
        }
        unsigned char *const larger = (unsigned char *)realloc(*memory, grown);
        if (larger == NULL)
        {
            return 0;
        }
        *memory = larger;
        *room = grown;
    }
    memcpy(*memory + *held, bytes, size);
    *held += size;
    return 1;
}

// `count` runs at `runs` read by the vendor as one raw run: handed to the run's writer as the reader's stream of the
// .qry (qry_streamed), the disassembler pointed at that stream and run over it, and everything it printed kept in the
// reader's output and handed to the run as the cross's trace. Nothing but the run's writer writes a file. 0 where the
// disassembler ended clean, 1 where it did not, and -1 where it could not be run
int msq_read(MsqReader *reader, const MsqBytes *runs, size_t count)
{
    reader->calls += 1ull;
    reader->raw_size = 0u;
    int joined = 1;
    for (size_t at = 0u; joined && (at < count); at += 1u)
    {
        // a run is at most a container's code, which a size_t counts
        joined = (runs[at].size == 0ull) ||
                 msq_added(&reader->raw, &reader->raw_size, &reader->raw_room, runs[at].bytes, (size_t)runs[at].size);
    }
    char stream_name[64];
    char stream_path[1400];
    snprintf(stream_name, sizeof(stream_name), "msq_stream_%u", reader->stream);
    if (!joined || (qry_streamed(s_run, stream_name, reader->raw, (unsigned long long)reader->raw_size, stream_path,
                                 sizeof(stream_path)) == 0ull))
    {
        return -1;
    }
    // the interface's command is a list of words it does not write to; the casts only meet its declared type
    char *const command[] = {(char *)s_nvdisasm, (char *)"--binary", (char *)s_architecture, stream_path, NULL};
    InterfaceAnswer answer;
    // read again where the output ran past the room it was given, the room grown to hold it
    for (unsigned int tried = 0u; tried < 2u; tried += 1u)
    {
        memset(&answer, 0, sizeof(answer));
        answer.output = reader->output;
        answer.output_capacity = reader->output_room;
        const InterfaceProbe probe = {command, NULL, 0ull};
        EngineError error;
        memset(&error, 0, sizeof(error));
        if ((interface_probe_run(&probe, &answer, &error) != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            return -1;
        }
        if (answer.output_bytes < reader->output_room)
        {
            break;
        }
        const unsigned long long room = answer.output_bytes + 65536ull;
        char *const larger = (char *)realloc(reader->output, (size_t)room);
        if (larger == NULL)
        {
            return -1;
        }
        reader->output = larger;
        reader->output_room = room;
    }
    reader->output_size =
        (answer.output_bytes < reader->output_room) ? answer.output_bytes : (reader->output_room - 1ull);
    // what the vendor printed of the run, the cross's trace (qry_buffer.h, QRY_TRACE)
    if (qry_traced("measuring_stick_query", QRY_ORDINARY))
    {
        char name[64];
        snprintf(name, sizeof(name), "run_%u_%llu", reader->ident, reader->calls);
        qry_hand(s_run, name, reader->output, reader->output_size, QRY_ORDINARY);
    }
    return ((answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ull)) ? 0 : 1;
}

// 1 where `c` is a space as the vendor's listing reads it
int msq_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\v') || (c == '\f');
}

// 1 where `c` is a letter, a digit or `_`
int msq_word(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_');
}

// a copy of `length` bytes at `text`, ended by a zero byte
static char *msq_copied(const char *text, size_t length)
{
    char *const copy = (char *)malloc(length + 1u);
    if (copy != NULL)
    {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }
    return copy;
}

void msq_listing_free(MsqListing *listing)
{
    for (size_t at = 0u; at < listing->count; at += 1u)
    {
        free(listing->lines[at].raw);
        free(listing->lines[at].body);
    }
    free(listing->lines);
    memset(listing, 0, sizeof(*listing));
}

// Every instruction of the listing `text` of `length` bytes, in the order printed: a comment of four or more hex digits
// giving its offset, `/*<offset>*/`, then space, then its text to the first `;` on its line. Its text past a guard,
// `@P0 `, `@!UPT `, is its body. 1, or 0 where the system gives no memory
int msq_listing_read(const char *text, size_t length, MsqListing *listing)
{
    memset(listing, 0, sizeof(*listing));
    size_t at = 0u;
    while ((at + 1u) < length)
    {
        const char *const open = (const char *)memchr(text + at, '/', length - at);
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
        unsigned long long offset = 0ull;
        size_t digits = 0u;
        while ((walk < length) &&
               (((text[walk] >= '0') && (text[walk] <= '9')) || ((text[walk] >= 'a') && (text[walk] <= 'f'))))
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
        while ((walk < length) && msq_space(text[walk]))
        {
            walk += 1u;
        }
        // the text to the first `;` on its line
        size_t end = walk;
        while ((end < length) && (text[end] != ';') && (text[end] != '\n'))
        {
            end += 1u;
        }
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
                while ((guard < last) && ((text[guard] == 'T') || ((text[guard] >= '0') && (text[guard] <= '9'))))
                {
                    guard += 1u;
                }
                if ((guard > first_mark) && (guard < last) && msq_space(text[guard]))
                {
                    while ((guard < last) && msq_space(text[guard]))
                    {
                        guard += 1u;
                    }
                    body = guard;
                }
            }
        }
        if (listing->count == listing->room)
        {
            const size_t room = (listing->room != 0u) ? (listing->room * 2u) : 1024u;
            MsqLine *const larger = (MsqLine *)realloc(listing->lines, room * sizeof(MsqLine));
            if (larger == NULL)
            {
                return 0;
            }
            listing->lines = larger;
            listing->room = room;
        }
        MsqLine *const line = &listing->lines[listing->count];
        line->offset = offset;
        line->raw = msq_copied(text + walk, last - walk);
        line->body = msq_copied(text + body, last - body);
        if ((line->raw == NULL) || (line->body == NULL))
        {
            free(line->raw);
            free(line->body);
            return 0;
        }
        listing->count += 1u;
    }
    return 1;
}

// the reading of the instruction at `offset` the listing printed, the last printed there: its operation, the first
// word of its body before any `.`, and its body
MsqReading msq_listing_at(const MsqListing *listing, unsigned long long offset)
{
    MsqReading reading;
    memset(&reading, 0, sizeof(reading));
    for (size_t at = listing->count; at > 0u; at -= 1u)
    {
        const MsqLine *const line = &listing->lines[at - 1u];
        if (line->offset != offset)
        {
            continue;
        }
        reading.kind = MSQ_READ;
        size_t length = 0u;
        while ((line->body[length] != '\0') && !msq_space(line->body[length]) && (line->body[length] != '.') &&
               (length < (sizeof(reading.operation) - 1u)))
        {
            length += 1u;
        }
        memcpy(reading.operation, line->body, length);
        reading.operation[length] = '\0';
        reading.body = msq_copied(line->body, strlen(line->body));
        break;
    }
    return reading;
}

// the address the vendor stopped at, read off `at address 0x<hex>`: 1, or 0 where it names none
int msq_stopped_at(const char *text, size_t length, unsigned long long *address)
{
    static const char s_said[] = "at address 0x";
    const size_t said = sizeof(s_said) - 1u;
    for (size_t at = 0u; (at + said) < length; at += 1u)
    {
        if (memcmp(text + at, s_said, said) != 0)
        {
            continue;
        }
        size_t walk = at + said;
        unsigned long long value = 0ull;
        size_t digits = 0u;
        while ((walk < length) &&
               (((text[walk] >= '0') && (text[walk] <= '9')) || ((text[walk] >= 'a') && (text[walk] <= 'f'))))
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
