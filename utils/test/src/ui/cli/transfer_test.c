// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// transfer_test.c: the shared regions' check: a region handed over holds what was written, a region said to hold more
// than it does is refused, a region given to a process reads and cannot write, and a region is sealed against every
// change once written
#include "../../../../../ui/cli/src/comms/transfer.h"
#include "cli_check.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

static unsigned char s_bytes[CLI_TRANSFER_SMALL * 3u + 7u];

// `length` bytes of the pattern into s_bytes
static void transfer_pattern(unsigned long long length)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        // the pattern's byte is below 251
        s_bytes[at] = (unsigned char)((at * 31ull) % 251ull);
    }
}

// the region `region` made taken again into `taken`, as another process takes it: 1, or 0 with why said
static int transfer_taken(const CliRegion *region, CliRegion *taken, unsigned long long length, CliLine *said)
{
#if defined(_WIN32)
    return cli_region_taken(taken, cli_process_own(), region->section, length, said);
#else
    // the region's descriptor is held in its section, an int's worth
    return cli_region_received(taken, (unsigned long long)dup((int)region->section), length, said);
#endif
}

// a region handed over holds what was written
static void transfer_handed(void)
{
    const unsigned long long length = sizeof(s_bytes);
    transfer_pattern(length);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliRegion region;
    CliRegion taken;
    memor.set(&(MemoriaCfg){.dst = &region, .val = 0, .bytes = sizeof(region)});
    memor.set(&(MemoriaCfg){.dst = &taken, .val = 0, .bytes = sizeof(taken)});
    cli_check(cli_region_with(&region, s_bytes, length, &said) && (region.length == length) &&
                  (memor.cmp(&(MemoriaCfg){.src = region.view, .other = s_bytes, .bytes = length}) == 0),
              "a region holds what was written");
    cli_check(transfer_taken(&region, &taken, length, &said), "it is taken as another process takes it");
    cli_region_closed(&region);
    cli_check((taken.view != NULL) &&
                  (memor.cmp(&(MemoriaCfg){.src = taken.view, .other = s_bytes, .bytes = length}) == 0),
              "the region outlives the handle it was made by");
    cli_region_closed(&taken);
}

// a region said to hold more than it does is refused
static void transfer_refused(void)
{
    transfer_pattern(4096ull);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliRegion region;
    CliRegion taken;
    memor.set(&(MemoriaCfg){.dst = &region, .val = 0, .bytes = sizeof(region)});
    memor.set(&(MemoriaCfg){.dst = &taken, .val = 0, .bytes = sizeof(taken)});
    (void)cli_region_with(&region, s_bytes, 4096ull, &said);
    said.at = 0ull;
    cli_check(!transfer_taken(&region, &taken, 1ull << 20u, &said) && (said.at > 0ull),
              "a region said to hold more than it does is refused");
    cli_region_closed(&region);
}

int main(void)
{
    transfer_handed();
    transfer_refused();
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliRegion region;
    memor.set(&(MemoriaCfg){.dst = &region, .val = 0, .bytes = sizeof(region)});
    transfer_pattern(100ull);
    (void)cli_region_with(&region, s_bytes, 100ull, &said);
#if defined(_WIN32)
    unsigned long long given = 0ull;
    cli_check(cli_region_given(&region, cli_process_own(), &given, &said), "a region is given to a process");
    // the handle was made in this process, and a view the system refuses is NULL
    void *const written = MapViewOfFile((HANDLE)(uintptr_t)given, FILE_MAP_WRITE, 0, 0, 100);
    cli_check(written == NULL, "a given region cannot be written");
    if (written != NULL)
    {
        (void)UnmapViewOfFile(written);
    }
    CliRegion read;
    memor.set(&(MemoriaCfg){.dst = &read, .val = 0, .bytes = sizeof(read)});
    cli_check(cli_region_received(&read, given, 100ull, &said) &&
                  (memor.cmp(&(MemoriaCfg){.src = read.view, .other = s_bytes, .bytes = 100u}) == 0),
              "and reads what it holds");
    cli_region_closed(&read);
#elif defined(__linux__)
    // the region's descriptor is held in its section, an int's worth; the call fails, as the check asks
    cli_check(ftruncate((int)region.section, 10) != 0, "a sealed region cannot be shrunk");
#endif
    cli_region_closed(&region);
    return cli_checked();
}
