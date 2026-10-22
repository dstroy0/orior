// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_listing.c: the vendor's listing of many kernels' codes, read at one start of its disassembler: every code put
// into one cubin, the kernels' own a toolchain made, and the listing cut into each kernel's
//
//     orior_cubin_listing <pattern> <kernel prefix> <architecture> <nvdisasm> <codes>
//
// <codes> is a blob of the run, a line a code: the path of a file of a kernel's encodings, its name
// `<number>.bin`, the code of the kernel `<kernel prefix><number>` of the cubin at <pattern>. Each code is put into
// that kernel's code section, its exits found by the pattern's own EXIT encoding, and the whole cubin read once. A
// start of the disassembler costs far more than the codes it reads, and one start reads them all.
//
// The vendor reads nothing of a cubin it stops in. Where it stops, the codes are read as one raw run, which names the
// address it stops at: that code is set aside and the run read again without it, until it reads clean, and the cubin
// of the codes left is read once. Each code set aside is read alone as a raw run, as the vendor reads it, what it
// prints its listing. A code its kernel is not in the pattern for is set aside too.
//
// Each code's listing is handed to the run as `<number>.dis`: its instruction lines as the vendor prints the code read
// alone (cubin_listing.h). Exit 0 where every code was read, 2 where the run, the pattern, a code or the disassembler
// was not reached
#include "cubin_listing.h"
#include "cubin_write.h"

#include "../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../lstar/interface/interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// one code of the list: its number, its kernel's name, its bytes and their size, its exits, and 1 where it is set
// aside to be read alone
typedef struct
{
    char number[64];
    char kernel[160];
    const unsigned char *code;
    unsigned long long size;
    unsigned int *exits;
    unsigned int exit_count;
    int aside;
} ListedCode;

// the output the disassembler printed, grown to hold the most it prints
static char *s_output = NULL;
static unsigned long long s_output_room = 0ull;
static unsigned long long s_output_size = 0ull;
static unsigned int s_starts = 0u;

// The disassembler run over the stream at `path`, raw as `architecture` where it is not NULL, what it printed kept in
// s_output: 0 where it ended clean, 1 where it did not, -1 where it could not be run
static int listing_read(const char *nvdisasm, const char *architecture, char *path)
{
    char binary[] = "--binary";
    char *const raw[] = {(char *)nvdisasm, binary, (char *)architecture, path, NULL};
    char *const whole[] = {(char *)nvdisasm, path, NULL};
    for (unsigned int tried = 0u; tried < 2u; tried += 1u)
    {
        InterfaceAnswer answer;
        memset(&answer, 0, sizeof(answer));
        answer.output = s_output;
        answer.output_capacity = s_output_room;
        const InterfaceProbe probe = {(architecture != NULL) ? raw : whole, NULL, 0ull};
        EngineError error;
        memset(&error, 0, sizeof(error));
        s_starts += 1u;
        if ((interface_probe_run(&probe, &answer, &error) != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            return -1;
        }
        if (answer.output_bytes < s_output_room)
        {
            s_output_size = answer.output_bytes;
            return ((answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ull)) ? 0 : 1;
        }
        // the output ran past its room: the room grown to hold it, and the read made again
        const unsigned long long room = answer.output_bytes + 1048576ull;
        char *const larger = (char *)realloc(s_output, (size_t)room);
        if (larger == NULL)
        {
            return -1;
        }
        s_output = larger;
        s_output_room = room;
    }
    return -1;
}

// the address the vendor stopped at, read off `at address 0x<hex>`: 1, or 0 where it names none
static int listing_stopped_at(unsigned long long *address)
{
    static const char s_said[] = "at address 0x";
    const size_t said = sizeof(s_said) - 1u;
    for (unsigned long long at = 0ull; (at + said) < s_output_size; at += 1ull)
    {
        if (memcmp(s_output + at, s_said, said) != 0)
        {
            continue;
        }
        unsigned long long value = 0ull;
        size_t digits = 0u;
        for (unsigned long long walk = at + said; walk < s_output_size; walk += 1ull)
        {
            const char c = s_output[walk];
            const int digit = ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f'));
            if (!digit)
            {
                break;
            }
            value = (value << 4u) | (unsigned long long)((c <= '9') ? (c - '0') : (c - 'a' + 10));
            digits += 1u;
        }
        if (digits != 0u)
        {
            *address = value;
            return 1;
        }
    }
    return 0;
}

// the codes not set aside read as one raw run: each that stops the vendor set aside, and the run read again without it,
// until it reads clean. 1, or 0 where the vendor could not be run, the run could not be handed, or it stopped at no
// address it named, every code left then set aside
static int listing_raw_cleared(QryShared *run, ListedCode *codes, unsigned int count, const char *nvdisasm,
                               const char *architecture)
{
    unsigned long long total = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        total += codes[at].size;
    }
    unsigned char *const joined = (unsigned char *)malloc((size_t)total + 1u);
    unsigned long long *const start = (unsigned long long *)calloc((size_t)count + 1u, sizeof(unsigned long long));
    if ((joined == NULL) || (start == NULL))
    {
        free(joined);
        free(start);
        return 0;
    }
    int cleared = 0;
    while (!cleared)
    {
        unsigned long long size = 0ull;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            start[at] = size;
            if (!codes[at].aside)
            {
                memcpy(joined + size, codes[at].code, (size_t)codes[at].size);
                size += codes[at].size;
            }
        }
        char path[1400];
        if (size == 0ull)
        {
            cleared = 1;
            break;
        }
        if ((qry_member_streamed("cubin_listing_raw", joined, size, path, sizeof(path)) == 0ull))
        {
            break;
        }
        const int status = listing_read(nvdisasm, architecture, path);
        unsigned long long address = 0ull;
        if (status == 0)
        {
            cleared = 1;
        }
        else if ((status < 0) || !listing_stopped_at(&address))
        {
            break;
        }
        else
        {
            // the code the stop lies in, the last not set aside whose start lies at or before it
            unsigned int stopped = count;
            for (unsigned int at = 0u; at < count; at += 1u)
            {
                stopped = (!codes[at].aside && (start[at] <= address)) ? at : stopped;
            }
            if (stopped == count)
            {
                break;
            }
            codes[stopped].aside = 1;
            printf("  orior_cubin_listing: %s stops the vendor at byte %llu of it, and is read alone\n", codes[stopped].number,
                   address - start[stopped]);
        }
    }
    free(joined);
    free(start);
    for (unsigned int at = 0u; !cleared && (at < count); at += 1u)
    {
        codes[at].aside = 1;
    }
    return cleared;
}

// the cubin of every code not set aside written from the pattern and read once, its listing cut into each code's and
// handed to the run: 1 where it read clean, 0 where it did not, nothing then handed
static int listing_cubin_read(QryShared *run, ListedCode *codes, unsigned int count, const unsigned char *pattern,
                              unsigned long long pattern_size, const char *nvdisasm)
{
    ContainerPart *const parts = (ContainerPart *)calloc((size_t)count + 1u, sizeof(ContainerPart));
    const char **const names = (const char **)calloc((size_t)count + 1u, sizeof(const char *));
    unsigned int *const of = (unsigned int *)calloc((size_t)count + 1u, sizeof(unsigned int));
    unsigned long long total = 0ull;
    unsigned int kept = 0u;
    for (unsigned int at = 0u; (parts != NULL) && (names != NULL) && (of != NULL) && (at < count); at += 1u)
    {
        if (codes[at].aside)
        {
            continue;
        }
        parts[kept] = (ContainerPart){codes[at].kernel, codes[at].code, codes[at].size, 0u, codes[at].exits,
                                      codes[at].exit_count};
        names[kept] = codes[at].kernel;
        of[kept] = at;
        total += codes[at].size;
        kept += 1u;
    }
    // every section may move by its alignment, and every part's attributes grow by four bytes an exit
    const unsigned long long room = (pattern_size * 2ull) + (total * 2ull) + 1048576ull;
    unsigned char *const cubin = (unsigned char *)malloc((size_t)room);
    unsigned long long cubin_size = 0ull;
    const CubinKernels kernels = {pattern, pattern_size, parts, kept};
    char path[1400];
    int read = (parts != NULL) && (names != NULL) && (of != NULL) && (cubin != NULL) && (kept != 0u) &&
               cubin_kernels_write(&kernels, cubin, room, &cubin_size) &&
               (qry_member_streamed("cubin_listing_cubin", cubin, cubin_size, path, sizeof(path)) != 0ull) &&
               (listing_read(nvdisasm, NULL, path) == 0);
    free(cubin);
    CubinListingCut cut;
    memset(&cut, 0, sizeof(cut));
    read = read && cubin_listing_cut(s_output, (size_t)s_output_size, names, kept, &cut);
    for (unsigned int at = 0u; read && (at < kept); at += 1u)
    {
        read = cut.found[at];
    }
    for (unsigned int at = 0u; read && (at < kept); at += 1u)
    {
        char name[96];
        snprintf(name, sizeof(name), "%s.dis", codes[of[at]].number);
        read = qry_member_append(name, cut.text + cut.start[at], cut.size[at], QRY_ORDINARY) != 0;
    }
    cubin_listing_cut_freed(&cut);
    free(parts);
    free(names);
    free(of);
    return read || (kept == 0u);
}

// the line of `list` at `*at` into `line`, which holds `room`, `*at` moved past it: 1, or 0 where the list has ended
static int listing_line(const unsigned char *list, unsigned long long size, unsigned long long *at, char *line,
                        size_t room)
{
    if (*at >= size)
    {
        return 0;
    }
    const unsigned char *const end = (const unsigned char *)memchr(list + *at, '\n', (size_t)(size - *at));
    size_t length = (end != NULL) ? (size_t)(end - (list + *at)) : (size_t)(size - *at);
    const unsigned long long next = *at + length + 1ull;
    length = ((length != 0u) && (list[*at + length - 1u] == '\r')) ? (length - 1u) : length;
    snprintf(line, room, "%.*s", (int)length, (const char *)(list + *at));
    *at = next;
    return 1;
}

int main(int count, char **words)
{
    if (count != 6)
    {
        fprintf(stderr, "orior_cubin_listing <pattern> <kernel prefix> <architecture> <nvdisasm> <codes>\n");
        return 2;
    }
    const char *const pattern_path = words[1];
    const char *const prefix = words[2];
    const char *const architecture = words[3];
    const char *const nvdisasm = words[4];
    QryShared *const run = qry_member_run();
    const unsigned char *list = NULL;
    unsigned long long list_size = 0ull;
    const unsigned char *pattern = NULL;
    unsigned long long pattern_size = 0ull;
    if ((run == NULL) || !qry_member_blob(words[5], &list, &list_size, NULL) ||
        !qry_member_file_blob(pattern_path, &pattern, &pattern_size))
    {
        fprintf(stderr, "orior_cubin_listing: no run, no list %s in it, or no pattern %s\n", words[5], pattern_path);
        return 2;
    }
    unsigned int lines = 0u;
    for (unsigned long long at = 0ull; at < list_size; at += 1ull)
    {
        lines += (list[at] == '\n') ? 1u : 0u;
    }
    ListedCode *const codes = (ListedCode *)calloc((size_t)lines + 2u, sizeof(ListedCode));
    if (codes == NULL)
    {
        return 2;
    }
    // the pattern's own EXIT encoding, read where its first kernel's info says its first exit lies
    unsigned long long exit_low = 0ull;
    int exit_known = 0;
    unsigned int code_count = 0u;
    unsigned long long at = 0ull;
    char line[1400];
    while (listing_line(list, list_size, &at, line, sizeof(line)))
    {
        if (line[0] == '\0')
        {
            continue;
        }
        ListedCode *const code = &codes[code_count];
        const char *const forward = strrchr(line, '/');
        const char *const back = strrchr(line, '\\');
        const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
        const char *const base = (slash != NULL) ? (slash + 1) : line;
        const char *const dot = strrchr(base, '.');
        snprintf(code->number, sizeof(code->number), "%.*s", (int)((dot != NULL) ? (size_t)(dot - base) : strlen(base)),
                 base);
        snprintf(code->kernel, sizeof(code->kernel), "%s%s", prefix, code->number);
        if (!qry_member_file_blob(line, &code->code, &code->size) || ((code->size % 16ull) != 0ull))
        {
            fprintf(stderr, "orior_cubin_listing: the code %s could not be read, or is no whole count of instructions\n",
                    line);
            return 2;
        }
        unsigned long long offset = 0ull;
        unsigned long long size = 0ull;
        unsigned int first_exit[1] = {0u};
        if (!exit_known && cubin_code_of(pattern, pattern_size, code->kernel, &offset, &size) &&
            (cubin_exits_read(pattern, pattern_size, code->kernel, first_exit, 1u) == 1u) &&
            ((first_exit[0] + 16ull) <= size))
        {
            memcpy(&exit_low, pattern + offset + first_exit[0], 8u);
            exit_known = 1;
        }
        code_count += 1u;
    }
    for (unsigned int one = 0u; one < code_count; one += 1u)
    {
        ListedCode *const code = &codes[one];
        unsigned long long offset = 0ull;
        unsigned long long size = 0ull;
        const unsigned int room = (unsigned int)(code->size / 16ull) + 1u;
        code->exits = (unsigned int *)calloc(room, sizeof(unsigned int));
        code->exit_count = (exit_known && (code->exits != NULL))
                               ? cubin_exits_find(code->code, code->size, exit_low, code->exits, room)
                               : 0u;
        // a code whose kernel the pattern does not hold, or that holds no exit, is read alone
        code->aside = !cubin_code_of(pattern, pattern_size, code->kernel, &offset, &size) || (code->exit_count == 0u);
    }
    s_output_room = ((unsigned long long)code_count * 65536ull) + 16777216ull;
    s_output = (char *)malloc((size_t)s_output_room);
    if (s_output == NULL)
    {
        return 2;
    }
    // the cubin read once; where the vendor stops in it, the codes that stop it found as one raw run and set aside, and
    // the cubin of the rest read once more
    int whole = listing_cubin_read(run, codes, code_count, pattern, pattern_size, nvdisasm);
    if (!whole)
    {
        whole = listing_raw_cleared(run, codes, code_count, nvdisasm, architecture) &&
                listing_cubin_read(run, codes, code_count, pattern, pattern_size, nvdisasm);
    }
    // where the cubin of the codes the raw run read clean still did not read, every code is read alone
    for (unsigned int one = 0u; !whole && (one < code_count); one += 1u)
    {
        codes[one].aside = 1;
    }
    unsigned int aside = 0u;
    int reached = 1;
    for (unsigned int one = 0u; one < code_count; one += 1u)
    {
        if (!codes[one].aside)
        {
            continue;
        }
        aside += 1u;
        char path[1400];
        char name[96];
        snprintf(name, sizeof(name), "%s.dis", codes[one].number);
        const int streamed =
            qry_member_streamed("cubin_listing_alone", codes[one].code, codes[one].size, path, sizeof(path)) != 0ull;
        const int status = streamed ? listing_read(nvdisasm, architecture, path) : -1;
        reached = reached && (status >= 0);
        if (status >= 0)
        {
            qry_member_append(name, s_output, s_output_size, QRY_ORDINARY);
        }
    }
    printf("orior_cubin_listing: %u codes read in %u starts of the vendor, %u of them read alone\n", code_count, s_starts,
           aside);
    for (unsigned int one = 0u; one < code_count; one += 1u)
    {
        free(codes[one].exits);
    }
    free(codes);
    free(s_output);
    return reached ? 0 : 2;
}
