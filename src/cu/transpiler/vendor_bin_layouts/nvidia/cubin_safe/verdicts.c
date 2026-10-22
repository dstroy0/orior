// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// verdicts.c: the verdicts: the held encodings cleared, read line by line, added and loaded, whether a verdict is ready
// and holds, the admitted encodings cleared, read and held, and a verdict's name
#include "cubin_safe_internal.h"
#include "../../../../includes/parsers/line_read.h"

static unsigned long long *s_held_low = NULL;
static unsigned long long *s_held_high = NULL;
static unsigned int s_held_count = 0u;
static unsigned int s_held_room = 0u;
static int s_verdict_ready = 0;

// the held list let go and the verdict put back to not ready: the airlock holds every code off the part until a list
// loads whole
static void cubin_safe_verdict_clear(void)
{
    free(s_held_low);
    free(s_held_high);
    s_held_low = NULL;
    s_held_high = NULL;
    s_held_count = 0u;
    s_held_room = 0u;
    s_verdict_ready = 0;
}

// every line `<low> <high>` of the `size` bytes at `bytes` added to the held list: 1, or 0 where it did not fit memory
static int cubin_safe_verdict_lines(const char *bytes, unsigned long long size)
{
    char line[CUBIN_SAFE_HELD_LINE];
    int grown = 1;
    unsigned long long at = 0ull;
    while (grown && (at < size))
    {
        // the next line, as much of it as a held line holds
        const char *const line_end = (const char *)memchr(bytes + at, '\n', (size_t)(size - at));
        const size_t length = (line_end != NULL) ? (size_t)(line_end - (bytes + at)) : (size_t)(size - at);
        const size_t kept = (length < (sizeof(line) - 1u)) ? length : (sizeof(line) - 1u);
        memcpy(line, bytes + at, kept);
        line[kept] = '\0';
        at += (unsigned long long)length + 1ull;
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        const char *read = line;
        if (!line_number(&read, 1, &low) || !line_number(&read, 1, &high))
        {
            continue;
        }
        if (s_held_count == s_held_room)
        {
            const unsigned int room = (s_held_room != 0u) ? (s_held_room * 2u) : CUBIN_SAFE_HELD_FIRST;
            unsigned long long *const low_room =
                (unsigned long long *)realloc(s_held_low, (size_t)room * sizeof(*s_held_low));
            unsigned long long *const high_room =
                (unsigned long long *)realloc(s_held_high, (size_t)room * sizeof(*s_held_high));
            // whichever grew is kept so the clear below frees it; the list grows only where both did
            s_held_low = (low_room != NULL) ? low_room : s_held_low;
            s_held_high = (high_room != NULL) ? high_room : s_held_high;
            grown = (low_room != NULL) && (high_room != NULL);
            s_held_room = grown ? room : s_held_room;
        }
        if (grown)
        {
            s_held_low[s_held_count] = low;
            s_held_high[s_held_count] = high & CUBIN_SAFE_SCHED_KEPT_HIGH;
            s_held_count += 1u;
        }
    }
    return grown;
}

int cubin_safe_verdict_read(const char *bytes, unsigned long long size)
{
    cubin_safe_verdict_clear();
    // a list that did not fit holds nothing: the verdict stays unloaded and the airlock keeps every code off the part
    if (!cubin_safe_verdict_lines(bytes, size))
    {
        cubin_safe_verdict_clear();
        return 0;
    }
    s_verdict_ready = 1;
    return 1;
}

int cubin_safe_verdict_added(const char *bytes, unsigned long long size)
{
    // a list that did not fit leaves none at all: the verdict unloaded and every code held off the part
    if (!cubin_safe_verdict_lines(bytes, size))
    {
        cubin_safe_verdict_clear();
        return 0;
    }
    return 1;
}

int cubin_safe_verdict_load(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    // the file read whole, then read as the list a buffer holds
    char *bytes = NULL;
    size_t size = 0u;
    size_t room = 0u;
    int whole = 1;
    while (whole)
    {
        if (size == room)
        {
            room = (room != 0u) ? (room * 2u) : 65536u;
            char *const larger = (char *)realloc(bytes, room);
            whole = (larger != NULL);
            bytes = (larger != NULL) ? larger : bytes;
            if (!whole)
            {
                break;
            }
        }
        const size_t read = fread(bytes + size, 1u, room - size, file);
        if (read == 0u)
        {
            break;
        }
        size += read;
    }
    whole = whole && (ferror(file) == 0);
    fclose(file);
    const int loaded = whole && cubin_safe_verdict_read(bytes, (unsigned long long)size);
    free(bytes);
    if (!loaded)
    {
        cubin_safe_verdict_clear();
    }
    return loaded;
}

int cubin_safe_verdict_ready(void)
{
    return s_verdict_ready;
}

int cubin_safe_verdict_holds(unsigned long long low, unsigned long long high)
{
    const unsigned long long kept = high & CUBIN_SAFE_SCHED_KEPT_HIGH;
    for (unsigned int at = 0u; at < s_held_count; at += 1u)
    {
        if ((s_held_low[at] == low) && (s_held_high[at] == kept))
        {
            return 1;
        }
    }
    return 0;
}

// the encodings a conference admitted past rule 3, the lessons of the cause admitted the carrier loads (orior_cubin_run
// --lessons): each its low word and the bits kept of its high word, as the held list keeps them, and as many as the
// lessons name
static unsigned long long *s_admitted_low = NULL;

static unsigned long long *s_admitted_high = NULL;
static unsigned int s_admitted_count = 0u;
static unsigned int s_admitted_room = 0u;

static void cubin_safe_admitted_clear(void)
{
    free(s_admitted_low);
    free(s_admitted_high);
    s_admitted_low = NULL;
    s_admitted_high = NULL;
    s_admitted_count = 0u;
    s_admitted_room = 0u;
}

int cubin_safe_admitted_read(const char *bytes, unsigned long long size)
{
    cubin_safe_admitted_clear();
    char line[CUBIN_SAFE_HELD_LINE];
    unsigned long long at = 0ull;
    while (at < size)
    {
        // the next line, as much of it as a held line holds
        const char *const line_end = (const char *)memchr(bytes + at, '\n', (size_t)(size - at));
        const size_t length = (line_end != NULL) ? (size_t)(line_end - (bytes + at)) : (size_t)(size - at);
        const size_t kept = (length < (sizeof(line) - 1u)) ? length : (sizeof(line) - 1u);
        memcpy(line, bytes + at, kept);
        line[kept] = '\0';
        at += (unsigned long long)length + 1ull;
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        const char *read = line;
        if (!line_letters(&read, "lesson ") || !line_letters(&read, "admitted ") || !line_letters(&read, "* ") ||
            !line_number(&read, 1, &low) || !line_number(&read, 1, &high))
        {
            continue;
        }
        if (s_admitted_count == s_admitted_room)
        {
            const unsigned int room = (s_admitted_room != 0u) ? (s_admitted_room * 2u) : CUBIN_SAFE_HELD_FIRST;
            unsigned long long *const low_room =
                (unsigned long long *)realloc(s_admitted_low, (size_t)room * sizeof(*s_admitted_low));
            unsigned long long *const high_room =
                (unsigned long long *)realloc(s_admitted_high, (size_t)room * sizeof(*s_admitted_high));
            s_admitted_low = (low_room != NULL) ? low_room : s_admitted_low;
            s_admitted_high = (high_room != NULL) ? high_room : s_admitted_high;
            // a list that did not fit admits nothing: rule 3 holds every unknown key as before
            if ((low_room == NULL) || (high_room == NULL))
            {
                cubin_safe_admitted_clear();
                return 0;
            }
            s_admitted_room = room;
        }
        s_admitted_low[s_admitted_count] = low;
        s_admitted_high[s_admitted_count] = high & CUBIN_SAFE_SCHED_KEPT_HIGH;
        s_admitted_count += 1u;
    }
    return 1;
}

int cubin_safe_admitted(unsigned long long low, unsigned long long high)
{
    const unsigned long long kept = high & CUBIN_SAFE_SCHED_KEPT_HIGH;
    for (unsigned int at = 0u; at < s_admitted_count; at += 1u)
    {
        if ((s_admitted_low[at] == low) && (s_admitted_high[at] == kept))
        {
            return 1;
        }
    }
    return 0;
}

const char *cubin_safe_name(unsigned int verdict)
{
    static const char *const s_names[] = {
        "safe",
        "not a whole count of instructions",
        "an instruction no form holds, stalled short of the longest",
        "a wait short of all six barriers",
        "a control transfer or a wait",
        "an operation key unknown or holding a control transfer or a wait",
        "a second instruction no form holds",
        "no EXIT without a guard",
        "a barrier set by an operation whose result is back in a measured count",
        "a launch the driver timed out, or not back inside the bound",
        "a launch that faulted the part, its context left corrupt",
        "a register the kernel did not allot",
        "an encoding the vendor held off the part",
        "a read of the constant bank past the bank, or of a bank the container has none of",
        "an encoding its class's conditions name illegal, a register out of range or misaligned"};
    return (verdict < (sizeof(s_names) / sizeof(s_names[0]))) ? s_names[verdict] : "unknown";
}
