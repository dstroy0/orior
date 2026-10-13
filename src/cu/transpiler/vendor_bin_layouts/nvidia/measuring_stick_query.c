// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_query.c: the query's questions read against the vendor's own disassembler, the object the carrier
// popped out under --diff-output-against-vendor. A question whose slot instruction the vendor calls illegal, reads as a
// control transfer our gate's operation key could not, reads as naming a register at or past the count its kernel
// allots, reads as writing a register the code after the slot addresses memory through, or reads as a memory access of
// its own, is one the gate passed that must be held before a series reaches the part. Our gate judges by operation key
// and by the fields a form has learned: it cannot know a whole encoding illegal while the operation it probes is still
// unlearned, nor where a register sits in a form whose fields are not yet asked. The vendor's reader prints both. A
// register past the count faults the part at once, an out-of-range register warp exception, and a run of them takes the
// display and the machine down with it. A slot that overwrites an address the kernel stores through, or that reads or
// writes memory through registers the question set no address in, faults it with an illegal address. Nothing here runs
// on the part.
//
//     measuring_stick_query <question list> <nvdisasm> <architecture> <report> <held> [<holds> [<read>]]
//
// Every name but the disassembler's is a blob of the run's buffer, the .qry QRY names (qry_buffer.h): the list, a line
// a question, `<code> <registers> <cases> <answers> <slot> [<launches>]` (run_channel.h), each question's code, and the
// container the carrier popped for it as <answers stem>.cubin. Every container is the kernel the system accepted with
// the question's code in it; the code of each, as the part would be handed it, is read out of the container's code
// section.
//
// The vendor reads a raw run of encodings (nvdisasm --binary <architecture>) only from a file's path: each run is
// handed to the run's writer as a stream of the .qry (qry_streamed), and the vendor is pointed at that stream. The
// writer is the only thing that writes a file, and everything the vendor prints of a run is a blob of the run.
//
// The slot instructions are read each distinct encoding once, CHUNK of them a run and the runs read many at once. The
// vendor stops at the first instruction it calls illegal and names its address: that slot is put to a NOP and the run
// read again, until it reads clean, and the clean run gives every legal slot's operation at once. A stop that names no
// address is read again a slot at a time. Each slot the vendor reads whole has every register it names held to the
// question's <registers>, RZ aside, a register read as a pair (.64, .WIDE) reaching its second word and one of .128 its
// fourth. The code past each slot is read to its first EXIT with no guard, every distinct one together as one raw run,
// for the registers its memory operands read an address from; a slot that writes one of them is held, as is a slot with
// a memory operand of its own.
//
// Every question's frame is read whole too: its code with its slot put to a NOP, or its whole code where it holds no
// slot, each distinct frame once. Each instruction of it the vendor calls illegal is put to a NOP and the frame read
// again; each such instruction, and each the vendor reads naming a register at or past the question's <registers>, is
// held. The gate holds a whole code where any instruction of it is held (cubin_safe.c). A frame the vendor stops in at
// no instruction it names holds its questions by no encoding.
//
// The report, handed as <report>, lists every question held; <held> is the feedback the carrier reads (--held): a line
// `<low> <high>` an encoding held, for the carrier to hold every code that holds it off the part. <holds>, where it is
// given, is the feedback the teacher reads before the round is carried (run_channel.h): a line a hold,
// `<answers stem> <registers> <why>`, the count of registers that clears it where it is a register past the count its
// kernel allots and that count is at most MSQ_ALLOTTED_MOST, 0 where no count clears it. The vendor names no
// operation and nothing it says enters a form: the held list is only the exact words not to ask, and a count that
// clears a hold changes how a question is put and not what it asks. Exit 1 where any
// question is held, 2 where a blob, a tool or the run's buffer was not reached, or a frame was read at no stop it
// named: the round is then not carried
#if !defined(_WIN32)
// the threads and the processor count are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif

#include "../../../types/file_defs/qry/qry_buffer.h"
#include "../../lstar/interface/interface.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

// the encodings one run carries, enough that the readings a run takes are a few and the runs read many at once
#define MSQ_CHUNK 512u

// a NOP for the architecture: put in place of an instruction the vendor calls illegal so the run reads past it. The
// instruction it stands in for is already held; its own reading is never used
static const unsigned char s_nop[16] = {0x18u, 0x79u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xc0u, 0x0fu, 0x00u};

// the operations that transfer control or wait, which a question's slot must never be (cubin_safe.c, the same list)
static const char *const s_control[] = {"BRA",   "BRX",   "JMP",  "JMX",      "CALL",   "RET",   "EXIT",   "BSSY",
                                        "BSYNC", "BREAK", "BMOV", "WARPSYNC", "YIELD",  "BAR",   "DEPBAR", "NANOSLEEP",
                                        "BPT",   "RTT",   "KILL", "RPCMOV",   "RETIRE", "PMTRIG"};

// an executable section of an ELF, by the flag its header carries
#define MSQ_EXECUTABLE 0x4ull

// the most registers a reading names, past the most a kernel allots and the widest operand past it
#define MSQ_REGISTERS 512u
// the most registers a kernel allots a thread, R254 the highest a field names short of RZ
#define MSQ_ALLOTTED_MOST 255u

static const char s_illegal_why[] = "the vendor reads an illegal instruction our gate's key passed";

// a slot's reading: none where the vendor printed no instruction at its offset, its operation and its text past the
// guard where it read one, or illegal
typedef enum
{
    MSQ_NONE = 0,
    MSQ_READ = 1,
    MSQ_ILLEGAL = 2
} MsqKind;

typedef struct
{
    MsqKind kind;
    char operation[48];
    char *body;
} MsqReading;

// one instruction of a listing: its offset, its text as printed, and that text past its guard
typedef struct
{
    unsigned long long offset;
    char *raw;
    char *body;
} MsqLine;

typedef struct
{
    MsqLine *lines;
    size_t count;
    size_t room;
} MsqListing;

// a run of bytes in the run's buffer, or of memory of the program's own
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
} MsqBytes;

// a question: its answers' stem, its code's name, its slot, the registers its kernel allots, its container's code, and
// its slot instruction as the part would be handed it, where the code holds the slot
typedef struct
{
    char stem[256];
    char code_name[256];
    unsigned long long slot;
    unsigned int registers;
    MsqBytes code;
    int slotted;
    unsigned char word[16];
    size_t word_index;
    size_t whole_index;
    size_t tail_index;
    size_t prefix_index;
    size_t piece_index;
    size_t piece_key[2];
} MsqQuestion;

static QryBuffer *s_run = NULL;
static const char *s_nvdisasm = NULL;
static const char *s_architecture = NULL;

// A reader of the vendor's disassembler over raw runs: the stream its runs are handed as, the run it is handing, the
// output it was handed back, and the count of runs it read. `ident` keeps the blob names of readers that run at once
// apart, and `stream` their streams
typedef struct
{
    unsigned int ident;
    unsigned int stream;
    unsigned long long calls;
    unsigned char *raw;
    size_t raw_size;
    size_t raw_room;
    char *output;
    unsigned long long output_room;
    unsigned long long output_size;
} MsqReader;

// the bytes `size` of them at `bytes` added to the end of `*memory`, which holds `*held` of `*room`: 1, or 0 where the
// system gives no memory
static int msq_added(unsigned char **memory, size_t *held, size_t *room, const void *bytes, size_t size)
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
static int msq_read(MsqReader *reader, const MsqBytes *runs, size_t count)
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
static int msq_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\v') || (c == '\f');
}

// 1 where `c` is a letter, a digit or `_`
static int msq_word(char c)
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

static void msq_listing_free(MsqListing *listing)
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
static int msq_listing_read(const char *text, size_t length, MsqListing *listing)
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
static MsqReading msq_listing_at(const MsqListing *listing, unsigned long long offset)
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
static int msq_stopped_at(const char *text, size_t length, unsigned long long *address)
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

// 1 where the vendor's output says it found an illegal instruction
static int msq_illegal_said(const char *text, size_t length)
{
    static const char s_said[] = "Illegal instruction found";
    const size_t said = sizeof(s_said) - 1u;
    for (size_t at = 0u; (at + said) <= length; at += 1u)
    {
        if (memcmp(text + at, s_said, said) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// a set of general registers, a bit a register
typedef struct
{
    unsigned long long bits[MSQ_REGISTERS / 64u];
} MsqRegisters;

static void msq_register_add(MsqRegisters *set, unsigned long long number)
{
    if (number < MSQ_REGISTERS)
    {
        set->bits[number / 64u] |= 1ull << (number % 64u);
    }
}

// The general registers the line `raw` reads an address from, added to `set`: each `[` that opens no constant bank, one
// not after `]`, `c` or `C`, then space, then `R<number>`, and the register after it where it reads `.64`. 1 where the
// line is an EXIT with no guard, the end of what is read
static int msq_addresses_added(const char *raw, MsqRegisters *set)
{
    for (size_t at = 0u; raw[at] != '\0'; at += 1u)
    {
        if ((raw[at] != '[') ||
            ((at > 0u) && ((raw[at - 1u] == ']') || (raw[at - 1u] == 'c') || (raw[at - 1u] == 'C'))))
        {
            continue;
        }
        size_t walk = at + 1u;
        while (msq_space(raw[walk]))
        {
            walk += 1u;
        }
        if ((raw[walk] != 'R') || (raw[walk + 1u] < '0') || (raw[walk + 1u] > '9'))
        {
            continue;
        }
        walk += 1u;
        unsigned long long number = 0ull;
        while ((raw[walk] >= '0') && (raw[walk] <= '9'))
        {
            number = (number * 10ull) + (unsigned long long)(raw[walk] - '0');
            walk += 1u;
        }
        msq_register_add(set, number);
        if (strncmp(raw + walk, ".64", 3u) == 0)
        {
            msq_register_add(set, number + 1ull);
        }
    }
    return (strncmp(raw, "EXIT", 4u) == 0) && !msq_word(raw[4]);
}

// every register the lines `first` to `end` of `listing` read an address from, to the first EXIT with no guard
static void msq_addresses(const MsqListing *listing, size_t first, size_t end, MsqRegisters *set)
{
    memset(set, 0, sizeof(*set));
    for (size_t at = first; at < end; at += 1u)
    {
        if (msq_addresses_added(listing->lines[at].raw, set))
        {
            break;
        }
    }
}

// the widest a register the operation `operation` names reaches past itself: 3 for .128, 1 for .WIDE or .64, else 0
static unsigned int msq_widened(const char *operation, size_t length)
{
    char kept[96];
    const size_t copied = (length < (sizeof(kept) - 1u)) ? length : (sizeof(kept) - 1u);
    memcpy(kept, operation, copied);
    kept[copied] = '\0';
    return (strstr(kept, ".128") != NULL)
               ? 3u
               : (((strstr(kept, ".WIDE") != NULL) || (strstr(kept, ".64") != NULL)) ? 1u : 0u);
}

// the length of the first word of `text`, its spaces before skipped into `*start`
static size_t msq_first_word(const char *text, size_t *start)
{
    size_t at = 0u;
    while ((text[at] != '\0') && msq_space(text[at]))
    {
        at += 1u;
    }
    *start = at;
    size_t end = at;
    while ((text[end] != '\0') && !msq_space(text[end]))
    {
        end += 1u;
    }
    return end - at;
}

// The highest register the instruction `body` reaches at or past `registers`, the count its kernel allots: a pair
// reaches the register after the one named and a quad the third after it, as does every register of an operation read
// .WIDE, .64 or .128. Each `R<number>` with a word's edge before it and after it, past the operation, is a register. 1
// with
// `*reached` set, or 0 where every register it reaches lies below the count, or no count is given
static int msq_register_reached(const char *body, unsigned int registers, unsigned long long *reached)
{
    if (registers == 0u)
    {
        return 0;
    }
    size_t start = 0u;
    const size_t operation = msq_first_word(body, &start);
    const unsigned int widened = msq_widened(body + start, operation);
    const char *const rest = body + start + operation;
    int found = 0;
    for (size_t at = 0u; rest[at] != '\0'; at += 1u)
    {
        // the start of what follows the operation is a word's edge, as is any place after a character no word holds
        if ((rest[at] != 'R') || ((at > 0u) && msq_word(rest[at - 1u])) || (rest[at + 1u] < '0') ||
            (rest[at + 1u] > '9'))
        {
            continue;
        }
        size_t walk = at + 1u;
        unsigned long long number = 0ull;
        while ((rest[walk] >= '0') && (rest[walk] <= '9'))
        {
            number = (number * 10ull) + (unsigned long long)(rest[walk] - '0');
            walk += 1u;
        }
        // the width the operand names where `.64` or `.128` follows it and a word's edge after that, else the
        // operation's
        unsigned int width = widened;
        if ((strncmp(rest + walk, ".128", 4u) == 0) && !msq_word(rest[walk + 4u]))
        {
            width = 3u;
        }
        else if ((strncmp(rest + walk, ".64", 3u) == 0) && !msq_word(rest[walk + 3u]))
        {
            width = 1u;
        }
        else if (msq_word(rest[walk]))
        {
            continue;
        }
        const unsigned long long reaching = number + width;
        if ((reaching >= registers) && (!found || (reaching > *reached)))
        {
            *reached = reaching;
            found = 1;
        }
    }
    return found;
}

// the general registers the instruction `body` writes: its first operand where that is one, `R<number>` and a word's
// edge, and the rest of a pair or a quad where the operation reads .WIDE, .64 or .128
static void msq_registers_written(const char *body, MsqRegisters *set)
{
    memset(set, 0, sizeof(*set));
    size_t start = 0u;
    const size_t operation = msq_first_word(body, &start);
    const char *const rest = body + start + operation;
    size_t operand = 0u;
    if (msq_first_word(rest, &operand) == 0u)
    {
        return;
    }
    const char *const first = rest + operand;
    if ((first[0] != 'R') || (first[1] < '0') || (first[1] > '9'))
    {
        return;
    }
    size_t walk = 1u;
    unsigned long long number = 0ull;
    while ((first[walk] >= '0') && (first[walk] <= '9'))
    {
        number = (number * 10ull) + (unsigned long long)(first[walk] - '0');
        walk += 1u;
    }
    if (msq_word(first[walk]))
    {
        return;
    }
    const unsigned int width = msq_widened(body + start, operation);
    for (unsigned int place = 0u; place <= width; place += 1u)
    {
        msq_register_add(set, number + place);
    }
}

// 1 where the operands of `body`, the text past its first word, hold a memory operand: a `[` not after `]`, `c` or `C`
static int msq_memory(const char *body)
{
    size_t start = 0u;
    const size_t operation = msq_first_word(body, &start);
    const char *const operands = body + start + operation;
    for (size_t at = 0u; operands[at] != '\0'; at += 1u)
    {
        if ((operands[at] == '[') &&
            ((at == 0u) || ((operands[at - 1u] != ']') && (operands[at - 1u] != 'c') && (operands[at - 1u] != 'C'))))
        {
            return 1;
        }
    }
    return 0;
}

// the first executable section of the container `bytes`, its code: 1, or 0 where it is no ELF or holds none
static int msq_container_code(const MsqBytes *container, MsqBytes *code)
{
    const unsigned char *const data = container->bytes;
    const unsigned long long size = container->size;
    if ((size < 0x40ull) || (memcmp(data,
                                    "\x7f"
                                    "ELF",
                                    4u) != 0))
    {
        return 0;
    }
    unsigned long long section_offset = 0ull;
    memcpy(&section_offset, data + 0x28u, 8u);
    unsigned short entry_size = 0u;
    unsigned short entries = 0u;
    memcpy(&entry_size, data + 0x3Au, 2u);
    memcpy(&entries, data + 0x3Cu, 2u);
    for (unsigned int index = 0u; index < entries; index += 1u)
    {
        const unsigned long long header = section_offset + ((unsigned long long)index * entry_size);
        if ((header + 0x28ull) > size)
        {
            return 0;
        }
        unsigned long long flags = 0ull;
        unsigned long long offset = 0ull;
        unsigned long long length = 0ull;
        memcpy(&flags, data + header + 0x08u, 8u);
        memcpy(&offset, data + header + 0x18u, 8u);
        memcpy(&length, data + header + 0x20u, 8u);
        if (((flags & MSQ_EXECUTABLE) != 0ull) && (length != 0ull))
        {
            // the section as far as the container holds it
            code->bytes = data + ((offset < size) ? offset : size);
            code->size = (offset < size) ? (((offset + length) <= size) ? length : (size - offset)) : 0ull;
            return 1;
        }
    }
    return 0;
}

// A set of distinct runs of bytes, each found by its bytes: the index of each, by the order first seen
typedef struct
{
    MsqBytes *items;
    size_t count;
    size_t room;
    size_t *slots;
    size_t slot_count;
} MsqDistinct;

static unsigned long long msq_bytes_hash(const MsqBytes *item)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < item->size; at += 1ull)
    {
        hash = (hash ^ item->bytes[at]) * 0x100000001b3ull;
    }
    return hash;
}

// the set made ready for as many as `most` distinct runs: 1, or 0 where the system gives no memory
static int msq_distinct_made(MsqDistinct *set, size_t most)
{
    memset(set, 0, sizeof(*set));
    size_t slots = 64u;
    while (slots < (2u * most))
    {
        slots *= 2u;
    }
    set->slots = (size_t *)malloc(slots * sizeof(size_t));
    set->items = (MsqBytes *)malloc((most + 1u) * sizeof(MsqBytes));
    if ((set->slots == NULL) || (set->items == NULL))
    {
        return 0;
    }
    for (size_t at = 0u; at < slots; at += 1u)
    {
        set->slots[at] = (size_t)-1;
    }
    set->slot_count = slots;
    set->room = most + 1u;
    return 1;
}

// the index of `item` in the set, added where it is not there
static size_t msq_distinct_index(MsqDistinct *set, const MsqBytes *item)
{
    const size_t mask = set->slot_count - 1u;
    for (size_t probe = (size_t)msq_bytes_hash(item) & mask;; probe = (probe + 1u) & mask)
    {
        const size_t held = set->slots[probe];
        if (held == (size_t)-1)
        {
            set->slots[probe] = set->count;
            set->items[set->count] = *item;
            set->count += 1u;
            return set->count - 1u;
        }
        if ((set->items[held].size == item->size) &&
            ((item->size == 0ull) || (memcmp(set->items[held].bytes, item->bytes, (size_t)item->size) == 0)))
        {
            return held;
        }
    }
}

static void msq_distinct_free(MsqDistinct *set)
{
    free(set->items);
    free(set->slots);
    memset(set, 0, sizeof(*set));
}

// the readings of the distinct slot encodings, read in chunks by threads at once, and what the threads count
typedef struct
{
    const MsqDistinct *words;
    MsqReading *readings;
    size_t chunks;
    _Atomic size_t next;
    _Atomic unsigned int readers;
    _Atomic unsigned long long calls;
    _Atomic int failed;
} MsqChunks;

// The slots of a chunk not already in `illegal` read one at a time: the way out where the vendor stops at an address no
// slot of the chunk holds, and no single illegal can be found to read past
static void msq_readings_each(MsqReader *reader, const MsqDistinct *words, size_t first, size_t count,
                              const unsigned char *illegal, MsqReading *readings, _Atomic int *failed)
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        if (illegal[at])
        {
            readings[first + at].kind = MSQ_ILLEGAL;
            continue;
        }
        const int status = msq_read(reader, &words->items[first + at], 1u);
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
            readings[first + at] = msq_listing_at(&listing, 0ull);
            msq_listing_free(&listing);
        }
        else
        {
            readings[first + at].kind =
                msq_illegal_said(reader->output, (size_t)reader->output_size) ? MSQ_ILLEGAL : MSQ_NONE;
        }
    }
}

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
static void *msq_chunks_worker(void *argument)
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
        const size_t first = chunk * MSQ_CHUNK;
        const size_t count = ((first + MSQ_CHUNK) <= work->words->count) ? MSQ_CHUNK : (work->words->count - first);
        msq_chunk_read(&reader, work->words, first, count, work->readings, &work->failed);
        atomic_fetch_add(&work->calls, reader.calls);
    }
    free(reader.output);
    free(reader.raw);
    return NULL;
}

// the processors the system gives the program
static unsigned int msq_processors(void)
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
static int msq_tails_read(MsqReader *reader, const MsqDistinct *tails, MsqRegisters *addressed)
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

// A frame's reading, a question's code with its slot put to a NOP where it holds one: the place of every instruction
// the vendor calls illegal, and the listing it reads once each of those is put to a NOP. `readable` 0 where the vendor
// stopped at no instruction it named, and the frame can be held by no encoding
typedef struct
{
    unsigned long long *illegal;
    size_t illegal_count;
    MsqListing listing;
    int readable;
} MsqFrame;

// The frame `frame` read whole by the vendor, read again with each instruction it stops at put to a NOP, until it reads
// clean or stops at no instruction it names. 1, or 0 where a reading could not be run or the system gave no memory
static int msq_frame_read(MsqReader *reader, const MsqBytes *frame, MsqFrame *read)
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

static void msq_frame_free(MsqFrame *read)
{
    free(read->illegal);
    msq_listing_free(&read->listing);
    memset(read, 0, sizeof(*read));
}

// what the cross holds, kept as it goes: the held list, the report, the lines it says of each hold, the holds the
// teacher reads, and the slots it read clear, a line `<stem> <operation>` each, for a conference the gate may call
typedef struct
{
    unsigned char *held;
    size_t held_size;
    size_t held_room;
    unsigned char *report;
    size_t report_size;
    size_t report_room;
    unsigned char *said;
    size_t said_size;
    size_t said_room;
    unsigned char *listed;
    size_t listed_size;
    size_t listed_room;
    unsigned char *read;
    size_t read_size;
    size_t read_room;
} MsqHolds;

// 1 where the instruction `body` is a control transfer or a wait (s_control) other than the EXIT a kernel ends in, by
// its first word short of its modifiers
static int msq_control_read(const char *body)
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
static void msq_clear_said(MsqHolds *holds, const MsqQuestion *question, const char *body,
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
static unsigned int msq_clearing(unsigned long long reached)
{
    // a count at most MSQ_ALLOTTED_MOST fits an unsigned int
    return (reached < MSQ_ALLOTTED_MOST) ? (unsigned int)(reached + 1ull) : 0u;
}

// One hold of `question`: the encoding at `encoding`, NULL where there is none to hold, added to the held list, the
// hold and `why` to the report and to what is said, and to the holds the teacher reads with `clearing`, the count of
// registers that clears it, 0 where none does
static void msq_hold_said(MsqHolds *holds, const MsqQuestion *question, const unsigned char *encoding, const char *why,
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
static void msq_stem(const char *name, char *stem, size_t room)
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
static void msq_extension_put(const char *name, const char *extension, char *put, size_t room)
{
    const char *const forward = strrchr(name, '/');
    const char *const back = strrchr(name, '\\');
    const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
    const char *const dot = strrchr(name, '.');
    const size_t length = ((dot != NULL) && ((slash == NULL) || (dot > slash))) ? (size_t)(dot - name) : strlen(name);
    snprintf(put, room, "%.*s%s", (int)length, name, extension);
}

int main(int count, char **words)
{
    if ((count < 6) || (count > 8))
    {
        fprintf(stderr, "measuring_stick_query <question list> <nvdisasm> <architecture> <report> <held> [<holds> "
                        "[<read>]]\n");
        return 2;
    }
    const char *const list_name = words[1];
    s_nvdisasm = words[2];
    s_architecture = words[3];
    const char *const report_name = words[4];
    const char *const held_name = words[5];
    const char *const holds_name = (count >= 7) ? words[6] : NULL;
    const char *const read_name = (count == 8) ? words[7] : NULL;
    s_run = qry_run();
    if (s_run == NULL)
    {
        fprintf(stderr, "measuring_stick_query: no query writer runs: %s names no .qry a writer holds open\n",
                QRY_ENVIRONMENT);
        return 2;
    }
    const unsigned char *list = NULL;
    unsigned long long list_size = 0ull;
    if (!qry_latest(s_run, list_name, &list, &list_size, NULL))
    {
        fprintf(stderr, "measuring_stick_query: the run's buffer holds no %s\n", list_name);
        return 2;
    }
    // a question with a slot is read by its slot instruction, among every other's; one with none, the kernel's own, is
    // read whole, once a code
    size_t lines = 0u;
    for (unsigned long long at = 0ull; at < list_size; at += 1ull)
    {
        lines += (list[at] == '\n') ? 1u : 0u;
    }
    MsqQuestion *const questions = (MsqQuestion *)calloc(lines + 1u, sizeof(MsqQuestion));
    if (questions == NULL)
    {
        return 2;
    }
    size_t question_count = 0u;
    for (unsigned long long at = 0ull; at < list_size;)
    {
        const unsigned char *const end = (const unsigned char *)memchr(list + at, '\n', (size_t)(list_size - at));
        const size_t length = (end != NULL) ? (size_t)(end - (list + at)) : (size_t)(list_size - at);
        char line[1024];
        snprintf(line, sizeof(line), "%.*s", (int)length, (const char *)(list + at));
        at += (unsigned long long)length + 1ull;
        char code_name[256];
        char cases_name[256];
        char answers_name[256];
        unsigned int registers = 0u;
        unsigned long long slot = 0ull;
        if (sscanf(line, "%255s %u %255s %255s %llu", code_name, &registers, cases_name, answers_name, &slot) < 5)
        {
            continue;
        }
        char container_name[300];
        msq_extension_put(answers_name, ".cubin", container_name, sizeof(container_name));
        MsqBytes container;
        MsqQuestion *const question = &questions[question_count];
        if (!qry_latest(s_run, container_name, &container.bytes, &container.size, NULL) ||
            !msq_container_code(&container, &question->code))
        {
            continue;
        }
        msq_stem(answers_name, question->stem, sizeof(question->stem));
        snprintf(question->code_name, sizeof(question->code_name), "%s", code_name);
        question->slot = slot;
        question->registers = registers;
        question->slotted = (slot < (question->code.size / 16ull)) && (((slot * 16ull) + 16ull) <= question->code.size);
        if (question->slotted)
        {
            memcpy(question->word, question->code.bytes + (slot * 16ull), 16u);
        }
        question_count += 1u;
    }
    // the distinct slot encodings, whole codes, codes before and past a slot, and frames, each read once: a slotted
    // question's frame is the code before its slot, a NOP, and the code past it, found by the two
    MsqDistinct slot_words;
    MsqDistinct whole_codes;
    MsqDistinct tails;
    MsqDistinct prefixes;
    MsqDistinct pieces;
    size_t *const piece_of = (size_t *)calloc(question_count + 1u, sizeof(size_t));
    if (!msq_distinct_made(&slot_words, question_count) || !msq_distinct_made(&whole_codes, question_count) ||
        !msq_distinct_made(&tails, question_count) || !msq_distinct_made(&prefixes, question_count) ||
        !msq_distinct_made(&pieces, question_count) || (piece_of == NULL))
    {
        return 2;
    }
    for (size_t place = 0u; place < question_count; place += 1u)
    {
        MsqQuestion *const question = &questions[place];
        if (question->slotted)
        {
            const MsqBytes word = {question->word, 16ull};
            question->word_index = msq_distinct_index(&slot_words, &word);
            const MsqBytes tail = {question->code.bytes + ((question->slot + 1ull) * 16ull),
                                   question->code.size - ((question->slot + 1ull) * 16ull)};
            question->tail_index = msq_distinct_index(&tails, &tail);
            const MsqBytes prefix = {question->code.bytes, question->slot * 16ull};
            question->prefix_index = msq_distinct_index(&prefixes, &prefix);
            question->piece_key[0] = question->prefix_index;
            question->piece_key[1] = question->tail_index;
            const MsqBytes key = {(const unsigned char *)question->piece_key, sizeof(question->piece_key)};
            const size_t before = pieces.count;
            question->piece_index = msq_distinct_index(&pieces, &key);
            // the question a frame is first found by gives its bytes
            if (pieces.count != before)
            {
                piece_of[question->piece_index] = place;
            }
        }
        else
        {
            question->whole_index = msq_distinct_index(&whole_codes, &question->code);
        }
    }
    // the slot encodings read in chunks, the chunks read by threads at once
    MsqReading *const readings = (MsqReading *)calloc(slot_words.count + 1u, sizeof(MsqReading));
    if (readings == NULL)
    {
        return 2;
    }
    MsqChunks work;
    memset(&work, 0, sizeof(work));
    work.words = &slot_words;
    work.readings = readings;
    work.chunks = (slot_words.count + MSQ_CHUNK - 1u) / MSQ_CHUNK;
    atomic_store(&work.next, 0u);
    const unsigned int processors = msq_processors() * 2u;
    const unsigned int threads =
        (work.chunks == 0u) ? 0u : ((work.chunks < processors) ? (unsigned int)work.chunks : processors);
    pthread_t *const started = (pthread_t *)calloc((size_t)threads + 1u, sizeof(pthread_t));
    for (unsigned int thread = 0u; (started != NULL) && (thread < threads); thread += 1u)
    {
        if (pthread_create(&started[thread], NULL, msq_chunks_worker, &work) != 0)
        {
            atomic_store(&work.failed, 1);
            break;
        }
    }
    for (unsigned int thread = 0u; (started != NULL) && (thread < threads); thread += 1u)
    {
        pthread_join(started[thread], NULL);
    }
    free(started);
    // the frame and tail readings run after the chunks, their reader's ident past every chunk's so its blob names stand
    // apart, and its stream the 0th
    MsqReader reader;
    memset(&reader, 0, sizeof(reader));
    reader.ident = 0xffffffffu;
    reader.stream = 0u;
    reader.output_room = 64ull << 20u;
    reader.output = (char *)malloc((size_t)reader.output_room);
    MsqFrame *const whole_frames = (MsqFrame *)calloc(whole_codes.count + 1u, sizeof(MsqFrame));
    MsqFrame *const piece_frames = (MsqFrame *)calloc(pieces.count + 1u, sizeof(MsqFrame));
    MsqRegisters *const addressed = (MsqRegisters *)calloc(tails.count + 1u, sizeof(MsqRegisters));
    int whole = (reader.output != NULL) && (whole_frames != NULL) && (piece_frames != NULL) && (addressed != NULL) &&
                !atomic_load(&work.failed);
    // every frame read whole: a question with no slot by its whole code, a slotted one by the code before its slot, a
    // NOP and the code past it, the code past a slot being the same in every question of one kernel
    for (size_t code = 0u; whole && (code < whole_codes.count); code += 1u)
    {
        whole = msq_frame_read(&reader, &whole_codes.items[code], &whole_frames[code]);
    }
    unsigned char *joined = NULL;
    size_t joined_size = 0u;
    size_t joined_room = 0u;
    for (size_t piece = 0u; whole && (piece < pieces.count); piece += 1u)
    {
        const MsqQuestion *const first = &questions[piece_of[piece]];
        const MsqBytes *const prefix = &prefixes.items[first->prefix_index];
        const MsqBytes *const tail = &tails.items[first->tail_index];
        joined_size = 0u;
        // a code is at most a container's, which a size_t counts
        whole = msq_added(&joined, &joined_size, &joined_room, prefix->bytes, (size_t)prefix->size) &&
                msq_added(&joined, &joined_size, &joined_room, s_nop, sizeof(s_nop)) &&
                msq_added(&joined, &joined_size, &joined_room, tail->bytes, (size_t)tail->size);
        const MsqBytes frame = {joined, (unsigned long long)joined_size};
        whole = whole && msq_frame_read(&reader, &frame, &piece_frames[piece]);
    }
    free(joined);
    // the registers the code past each slot reads an address from: every distinct code past a slot read together, as
    // one raw run, the kernel's own code being the same in every question but the slot
    whole = whole && msq_tails_read(&reader, &tails, addressed);
    if (!whole)
    {
        fprintf(stderr,
                "measuring_stick_query: the vendor's disassembler %s could not be run, or there was no memory\n",
                s_nvdisasm);
        return 2;
    }
    // the questions held, each with why, the held list and the report handed to the run's buffer. A question is held
    // by its slot where the vendor reads it as below, and by its frame where the vendor calls an instruction of it
    // illegal or reads one naming a register at or past the count its kernel allots: each such instruction's encoding
    // is held, and the gate holds every code that holds it. A frame the vendor stopped in at no instruction it named
    // holds its questions by no encoding, and the round they came in is not carried
    MsqHolds kept;
    memset(&kept, 0, sizeof(kept));
    size_t holds = 0u;
    size_t unread = 0u;
    for (size_t place = 0u; place < question_count; place += 1u)
    {
        const MsqQuestion *const question = &questions[place];
        const MsqFrame *const frame =
            question->slotted ? &piece_frames[question->piece_index] : &whole_frames[question->whole_index];
        int held_question = 0;
        if (!frame->readable)
        {
            msq_hold_said(&kept, question, NULL,
                          "the vendor stopped in its code at no instruction it named, and the round is not carried",
                          0u);
            unread += 1u;
            holds += 1u;
            continue;
        }
        for (size_t at = 0u; at < frame->illegal_count; at += 1u)
        {
            char why[160];
            snprintf(why, sizeof(why), "the vendor reads an illegal instruction at byte %llu of the code",
                     frame->illegal[at]);
            msq_hold_said(&kept, question, question->code.bytes + frame->illegal[at], why, 0u);
            held_question = 1;
        }
        for (size_t line = 0u; line < frame->listing.count; line += 1u)
        {
            const MsqLine *const read_line = &frame->listing.lines[line];
            unsigned long long frame_reached = 0ull;
            const int in_code = (read_line->offset + 16ull) <= question->code.size;
            if (in_code && msq_register_reached(read_line->body, question->registers, &frame_reached))
            {
                char why[200];
                snprintf(why, sizeof(why),
                         "the vendor reads R%llu at byte %llu of the code, past the %u registers its "
                         "kernel allots",
                         frame_reached, read_line->offset, question->registers);
                msq_hold_said(&kept, question, question->code.bytes + read_line->offset, why,
                              msq_clearing(frame_reached));
                held_question = 1;
            }
            else if (in_code && !question->slotted && !msq_control_read(read_line->body))
            {
                // an instruction of a whole question read clear: legal, within the registers its kernel allots and no
                // control transfer but the EXIT a kernel ends in; said to the teacher with its encoding, for the
                // conference the gate calls where it holds the instruction's key unknown (overseer.h)
                msq_clear_said(&kept, question, read_line->body, question->code.bytes + read_line->offset);
            }
        }
        holds += held_question ? 1u : 0u;
        if (!question->slotted)
        {
            continue;
        }
        const MsqReading *const reading = &readings[question->word_index];
        const MsqKind kind = reading->kind;
        const char *const body = ((kind == MSQ_READ) && (reading->body != NULL)) ? reading->body : "";
        char why[256] = "";
        unsigned int clearing = 0u;
        unsigned long long reached = 0ull;
        MsqRegisters written;
        msq_registers_written(body, &written);
        long long overwritten = -1;
        if (question->slotted)
        {
            const MsqRegisters *const read_from = &addressed[question->tail_index];
            for (unsigned int number = 0u; (overwritten < 0) && (number < MSQ_REGISTERS); number += 1u)
            {
                if ((written.bits[number / 64u] & read_from->bits[number / 64u] & (1ull << (number % 64u))) != 0ull)
                {
                    overwritten = (long long)number;
                }
            }
        }
        int control = 0;
        for (size_t at = 0u; (kind == MSQ_READ) && (at < (sizeof(s_control) / sizeof(s_control[0]))); at += 1u)
        {
            control |= (strcmp(reading->operation, s_control[at]) == 0);
        }
        if (kind == MSQ_ILLEGAL)
        {
            snprintf(why, sizeof(why), "%s", s_illegal_why);
        }
        else if (control)
        {
            snprintf(why, sizeof(why), "the vendor reads %s at the slot, a control transfer", reading->operation);
        }
        else if (msq_register_reached(body, question->registers, &reached))
        {
            snprintf(why, sizeof(why), "the vendor reads R%llu at the slot, past the %u registers its kernel allots",
                     reached, question->registers);
            clearing = msq_clearing(reached);
        }
        else if (overwritten >= 0)
        {
            snprintf(
                why, sizeof(why),
                "the vendor reads R%lld written at the slot, a register the code after it addresses memory through",
                overwritten);
        }
        else if ((body[0] != '\0') && msq_memory(body))
        {
            snprintf(why, sizeof(why),
                     "the vendor reads a memory access at the slot, through registers the question sets no address in");
        }
        if ((why[0] == '\0') && (kind == MSQ_READ))
        {
            // the slot read clear: a legal instruction that is no control transfer, names no register past the count,
            // overwrites no register the code addresses memory through and reaches no memory; said to the teacher
            // with its encoding, whose conference admits it where the gate holds its key unknown (overseer.h)
            msq_clear_said(&kept, question, body, question->word);
        }
        if (why[0] == '\0')
        {
            continue;
        }
        holds += held_question ? 0u : 1u;
        // the held encoding, the question's own code at its slot
        const unsigned char *code = NULL;
        unsigned long long code_size = 0ull;
        const int coded = qry_latest(s_run, question->code_name, &code, &code_size, NULL) &&
                          (question->slot < (code_size / 16ull)) && (((question->slot * 16ull) + 16ull) <= code_size);
        msq_hold_said(&kept, question, coded ? (code + (question->slot * 16ull)) : NULL, why, clearing);
    }
    const unsigned long long readings_count = atomic_load(&work.calls) + reader.calls;
    free(reader.output);
    char head[256];
    const int head_length =
        snprintf(head, sizeof(head), "questions read: %zu in %llu readings; held off the part: %zu\n", question_count,
                 readings_count, holds);
    unsigned char *whole_report = NULL;
    size_t whole_report_size = 0u;
    size_t whole_report_room = 0u;
    msq_added(&whole_report, &whole_report_size, &whole_report_room, head, (size_t)head_length);
    if (kept.report_size != 0u)
    {
        msq_added(&whole_report, &whole_report_size, &whole_report_room, kept.report, kept.report_size);
    }
    qry_hand(s_run, held_name, kept.held, kept.held_size, QRY_ORDINARY);
    qry_hand(s_run, report_name, whole_report, whole_report_size, QRY_ORDINARY);
    // the holds the teacher reads, handed even where there are none, so that it knows this round was read
    if (holds_name != NULL)
    {
        qry_hand(s_run, holds_name, kept.listed, kept.listed_size, QRY_ORDINARY);
    }
    // the slots read clear, handed even where there are none, so that the teacher knows the round was read
    if (read_name != NULL)
    {
        qry_hand(s_run, read_name, kept.read, kept.read_size, QRY_ORDINARY);
    }
    printf("  questions read: %zu in %llu readings; the vendor holds %zu off the part\n", question_count,
           readings_count, holds);
    printf("  the held list is %s and the report %s, blobs of %s\n", held_name, report_name, qry_path(s_run));
    if (kept.said_size != 0u)
    {
        fwrite(kept.said, 1u, kept.said_size, stdout);
    }
    for (size_t code = 0u; code < whole_codes.count; code += 1u)
    {
        msq_frame_free(&whole_frames[code]);
    }
    for (size_t piece = 0u; piece < pieces.count; piece += 1u)
    {
        msq_frame_free(&piece_frames[piece]);
    }
    free(whole_frames);
    free(piece_frames);
    free(piece_of);
    free(reader.raw);
    msq_distinct_free(&slot_words);
    msq_distinct_free(&whole_codes);
    msq_distinct_free(&tails);
    msq_distinct_free(&prefixes);
    msq_distinct_free(&pieces);
    // a frame the vendor named no stop in is no question it read: the round is not carried
    if (unread != 0u)
    {
        fprintf(stderr,
                "measuring_stick_query: %zu questions' codes the vendor stopped in at no instruction it named\n",
                unread);
        return 2;
    }
    return (holds != 0u) ? 1 : 0;
}
