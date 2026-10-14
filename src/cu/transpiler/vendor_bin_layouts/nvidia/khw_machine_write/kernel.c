// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kernel.c: the kernel: its code read, its forms kept, the runs and the answers read, and the listing given
#include "khw_machine_write_internal.h"

// the kernel, read out of the container the layout at `path` holds: the container's bytes, the part it enters at, the
// registers a thread of it holds, and its code. 1, or 0 with the reason printed
int khw_kernel_read(const char *path)
{
    s_pattern_size = cubin_pattern_read(path, s_pattern, sizeof(s_pattern), s_kernel, sizeof(s_kernel));
    if (s_pattern_size == 0ull)
    {
        return 0;
    }
    unsigned long long offsets[KHW_PLACES];
    unsigned long long sizes[KHW_PLACES];
    const unsigned int sections = cubin_code_sections(s_pattern, s_pattern_size, offsets, sizes, KHW_PLACES);
    if (sections == 0u)
    {
        printf("  khw_machine_write: the container at %s holds no code the kernel is read from\n", path);
        return 0;
    }
    if ((sizes[0] == 0ull) || ((sizes[0] % KHW_INSTRUCTION) != 0ull) ||
        (sizes[0] > (unsigned long long)sizeof(s_kernel_text)))
    {
        printf("  khw_machine_write: the kernel is %llu bytes, which is no whole count of places\n", sizes[0]);
        return 0;
    }
    memcpy(s_kernel_text, &s_pattern[offsets[0]], (size_t)sizes[0]);
    s_kernel_places = (unsigned int)(sizes[0] / KHW_INSTRUCTION);
    s_registers = cubin_registers_read(s_pattern, s_pattern_size, s_kernel);
    if (s_registers == 0u)
    {
        printf("  khw_machine_write: the container says how many registers no thread of %s holds\n", s_kernel);
        return 0;
    }
    return 1;
}

// Every place of the kernel kept in the machine as a form, so that the gate reads a question against what the part has
// already run (cubin_safe.h, its rules 2 and 3). A place the container records as an ending carries the table's name
// for the thread's end, which the gate reads to find where the code stops; every other place carries its own encoding
// whole, so that the gate holds it as the system wrote it until the part names it. The count kept
unsigned int khw_kernel_forms_kept(void)
{
    unsigned int exits[KHW_EXITS];
    const unsigned int endings = cubin_exits_read(s_pattern, s_pattern_size, s_kernel, exits, KHW_EXITS);
    const char *const ends = khw_piece_reading("operation", "control_coherence");
    unsigned int kept = 0u;
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        int ending = 0;
        for (unsigned int at = 0u; at < endings; at += 1u)
        {
            ending = ending || (exits[at] == (place * KHW_INSTRUCTION));
        }
        char text[KHW_NAME];
        if ((ending != 0) && (ends != NULL))
        {
            snprintf(text, sizeof(text), "%s", ends);
        }
        else
        {
            snprintf(text, sizeof(text), "0x%016llx%016llx", low, high);
        }
        SassForm *taken = NULL;
        const int took = sass_machine_take(&s_machine, text, low, high, &taken);
        // an ending whose encoding differs from the one already held under its name is held whole beside it, so that
        // the gate holds every ending as the system wrote it
        if (took && (taken != NULL) && ((taken->low != low) || (taken->high != high)) &&
            (s_machine.forms < SASS_MACHINE_FORMS))
        {
            s_machine.form[s_machine.forms] = *taken;
            s_machine.form[s_machine.forms].low = low;
            s_machine.form[s_machine.forms].high = high;
            s_machine.forms += 1u;
        }
        kept += took ? 1u : 0u;
    }
    return kept;
}

// the runs of `held` read from `text`, `held->runs` of them: three words a run, its first bit, its last and its place
// with its kind's letter after it, a register where none; or two, its first bit and its last, where the line holds no
// place, every run then holding none. 1, or 0 where the words are neither
static int khw_runs_read(KhwForm *held, const char *text)
{
    unsigned int words = 0u;
    for (const char *at = text; *at != '\0';)
    {
        at += strspn(at, " \t\r\n");
        const size_t length = strcspn(at, " \t\r\n");
        words += (length != 0u) ? 1u : 0u;
        at += length;
    }
    const unsigned int each = (words == (3u * held->runs)) ? 3u : ((words == (2u * held->runs)) ? 2u : 0u);
    if (each == 0u)
    {
        return 0;
    }
    const char *rest = text;
    for (unsigned int run = 0u; run < held->runs; run += 1u)
    {
        int step = 0;
        char place[16];
        held->place[run] = SASS_RUN_NO_PLACE;
        held->kind[run] = KHW_KIND_REGISTER;
        if (each == 2u)
        {
            if (sscanf(rest, "%u %u %n", &held->first[run], &held->last[run], &step) != 2)
            {
                return 0;
            }
        }
        else
        {
            if (sscanf(rest, "%u %u %15s %n", &held->first[run], &held->last[run], place, &step) != 3)
            {
                return 0;
            }
            char *end = NULL;
            held->place[run] = (strcmp(place, SASS_RUN_NO_PLACE_TEXT) == 0) ? SASS_RUN_NO_PLACE
                                                                            : (unsigned int)strtoul(place, &end, 10);
            const char *const letter = ((end != NULL) && (*end != '\0')) ? strchr(KHW_KIND_LETTERS, *end) : NULL;
            // the letter's place among the four is its kind
            held->kind[run] = (letter != NULL) ? (unsigned int)(letter - KHW_KIND_LETTERS) : KHW_KIND_REGISTER;
        }
        rest += step;
    }
    return 1;
}

// the forms the protocol answered for, read from `answers` into `form`, as many as `room` holds: the latest blob of
// that name in the run's buffer (qry_buffer.h). The count read, 0 where no run's writer runs or the run holds none
unsigned int khw_answers_read(const char *answers, KhwForm *form, unsigned int room)
{
    QryBuffer *const run = qry_run();
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (run == NULL)
    {
        printf("  khw_machine_write: no run's writer runs to read %s from\n", answers);
        return 0u;
    }
    if (!qry_latest(run, answers, &bytes, &size, NULL))
    {
        printf("  khw_machine_write: the run's buffer holds no %s\n", answers);
        return 0u;
    }
    char line[2048];
    unsigned int read = 0u;
    unsigned long long next = 0ull;
    while ((next < size) && (read < room))
    {
        const unsigned char *const end = (const unsigned char *)memchr(bytes + next, '\n', (size_t)(size - next));
        const size_t length = (end != NULL) ? (size_t)(end - (bytes + next)) : (size_t)(size - next);
        snprintf(line, sizeof(line), "%.*s", (int)length, (const char *)(bytes + next));
        next += (unsigned long long)length + 1ull;
        if ((line[0] == '#') || (line[0] == '\0') || (line[0] == '\r'))
        {
            continue;
        }
        KhwForm held;
        memset(&held, 0, sizeof(held));
        int at = 0;
        if (sscanf(line, "%255s %d %d %llx %llx %u %n", held.relation, &held.signed_read, &held.signedness_asked,
                   &held.low, &held.high, &held.runs, &at) != 6)
        {
            continue;
        }
        if ((held.runs > SASS_MACHINE_RUNS) || !khw_runs_read(&held, line + at))
        {
            continue;
        }
        form[read] = held;
        read += 1u;
    }
    return read;
}

// a line added to `listing`, the room grown once to fit it
void khw_listed(KhwListing *listing, const char *format, ...)
{
    for (unsigned int tried = 0u; listing->whole && (tried < 2u); tried += 1u)
    {
        char *const end = (listing->bytes != NULL) ? (listing->bytes + listing->size) : NULL;
        va_list words;
        va_start(words, format);
        const int length = vsnprintf(end, listing->room - listing->size, format, words);
        va_end(words);
        // a length vsnprintf returns is never negative for these formats
        if ((length >= 0) && (((size_t)length + 1u) <= (listing->room - listing->size)))
        {
            listing->size += (size_t)length;
            return;
        }
        const size_t room = (listing->room * 2u) + (size_t)length + 1u;
        char *const larger = (char *)realloc(listing->bytes, room);
        listing->whole = (larger != NULL);
        listing->bytes = (larger != NULL) ? larger : listing->bytes;
        listing->room = (larger != NULL) ? room : listing->room;
    }
    listing->whole = 0;
}

// `listing` given to the protocol as `answers`: the blob of that name in the run's buffer (qry_buffer.h). 1, or 0 with
// the reason printed, where no run's writer runs or it was not handed
int khw_listing_given(KhwListing *listing, const char *answers)
{
    QryBuffer *const run = qry_run();
    const int given = listing->whole && (run != NULL) &&
                      (qry_hand(run, answers, listing->bytes, listing->size, QRY_ORDINARY) != 0ull);
    free(listing->bytes);
    listing->bytes = NULL;
    if (!given)
    {
        printf("  khw_machine_write: %s could not be handed to the run's buffer%s\n", answers,
               (run == NULL) ? ": no run's writer runs" : "");
    }
    return given;
}
