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
// Each code's listing is passed to the run as `<number>.dis`: its instruction lines as the vendor prints the code read
// alone (cubin_listing.h). Exit 0 where every code was read, 2 where the run, the pattern, a code or the disassembler
// was not reached
#include "cubin_listing.h"
#include "../../../../platform/platform.h"
#include "../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "cubin_write.h"
#include "memoria_operor/memoria_operor.h"

#include "../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../lstar/interface/interface.h"

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

// the output the disassembler printed, grown to contain the max it prints
static char *s_output = NULL;
static unsigned long long s_output_room = 0ULL;
static unsigned long long s_output_size = 0ULL;
static unsigned int s_starts = 0u;

// The disassembler run over the stream at `path`, raw as `architecture` where it is not NULL, what it printed stored in
// s_output: 0 where it ended clean, 1 where it did not, -1 where it could not be run
static int listing_read(const char *nvdisasm, const char *architecture, char *path)
{
    char binary[] = "--binary";
    char *const raw[] = {(char *)nvdisasm, binary, (char *)architecture, path, NULL};
    char *const whole[] = {(char *)nvdisasm, path, NULL};
    for (unsigned int tried = 0u; tried < 2u; tried += 1u)
    {
        InterfaceAnswer answer;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &answer, .bytes = sizeof(answer), .val = 0);
        answer.output = s_output;
        answer.output_capacity = s_output_room;
        const InterfaceProbe probe = {(architecture != NULL) ? raw : whole, NULL, 0ULL};
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        s_starts += 1u;
        if ((interface_probe_run(&probe, &answer, &error) != 0L) || (answer.ending == INTERFACE_ENDING_NOT_STARTED))
        {
            return -1;
        }
        if (answer.output_bytes < s_output_room)
        {
            s_output_size = answer.output_bytes;
            return ((answer.ending == INTERFACE_ENDING_EXITED) && (answer.code == 0ULL)) ? 0 : 1;
        }
        // the output ran past its room: the room grown to contain it, and the read made again
        const unsigned long long room = answer.output_bytes + 1048576ULL;
        char *const larger =
            (char *)platform_cell_grown(s_output, s_output_room, (size_t)room, platform_place_g_program_owner, 0);
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
    static const char s_address_marker[] = "at address 0x";
    const size_t marker_length = sizeof(s_address_marker) - 1u;
    for (unsigned long long at = 0ULL; (at + marker_length) < s_output_size; at += 1ULL)
    {
        if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = s_output + at, .other = s_address_marker,
                       .bytes = marker_length) != 0)
        {
            continue;
        }
        unsigned long long value = 0ULL;
        size_t digits = 0u;
        for (unsigned long long walk = at + marker_length; walk < s_output_size; walk += 1ULL)
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
// until it reads clean. 1, or 0 where the vendor could not be run, the run could not be passed, or it stopped at no
// address it named, every code left then set aside
static int listing_raw_cleared(QryShared *run, ListedCode *codes, unsigned int count, const char *nvdisasm,
                               const char *architecture)
{
    unsigned long long total = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        total += codes[at].size;
    }
    unsigned char *const joined =
        (unsigned char *)platform_cell_taken((size_t)total + 1u, platform_place_g_program_owner, 0);
    unsigned long long *const start = (unsigned long long *)platform_cell_cleared(
        (size_t)count + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    if ((joined == NULL) || (start == NULL))
    {
        platform_cell_released(joined, platform_place_g_program_owner);
        platform_cell_released(start, platform_place_g_program_owner);
        return 0;
    }
    int cleared = 0;
    while (!cleared)
    {
        unsigned long long size = 0ULL;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            start[at] = size;
            if (!codes[at].aside)
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = joined + size, .src = codes[at].code,
                           .bytes = (size_t)codes[at].size);
                size += codes[at].size;
            }
        }
        char path[1400];
        if (size == 0ULL)
        {
            cleared = 1;
            break;
        }
        if ((qry_member_streamed("cubin_listing_raw", joined, size, path, sizeof(path)) == 0ULL))
        {
            break;
        }
        const int status = listing_read(nvdisasm, architecture, path);
        unsigned long long address = 0ULL;
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
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  orior_cubin_listing: ");
                mmgr_line_put(output_line, codes[stopped].number);
                mmgr_line_put(output_line, " stops the vendor at byte ");
                mmgr_line_decimal(output_line, address - start[stopped], 1u);
                mmgr_line_put(output_line, " of it, and is read alone");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    platform_cell_released(joined, platform_place_g_program_owner);
    platform_cell_released(start, platform_place_g_program_owner);
    for (unsigned int at = 0u; !cleared && (at < count); at += 1u)
    {
        codes[at].aside = 1;
    }
    return cleared;
}

// the cubin of every code not set aside written from the pattern and read once, its listing cut into each code's and
// passed to the run: 1 where it read clean, 0 where it did not, nothing then passed
static int listing_cubin_read(QryShared *run, ListedCode *codes, unsigned int count, const unsigned char *pattern,
                              unsigned long long pattern_size, const char *nvdisasm)
{
    ContainerPart *const parts = (ContainerPart *)platform_cell_cleared((size_t)count + 1u, sizeof(ContainerPart),
                                                                        platform_place_g_program_owner, 0);
    const char **const names = (const char **)platform_cell_cleared((size_t)count + 1u, sizeof(const char *),
                                                                    platform_place_g_program_owner, 0);
    unsigned int *const of = (unsigned int *)platform_cell_cleared((size_t)count + 1u, sizeof(unsigned int),
                                                                   platform_place_g_program_owner, 0);
    unsigned long long total = 0ULL;
    unsigned int retained = 0u;
    for (unsigned int at = 0u; (parts != NULL) && (names != NULL) && (of != NULL) && (at < count); at += 1u)
    {
        if (codes[at].aside)
        {
            continue;
        }
        parts[retained] = (ContainerPart){codes[at].kernel, codes[at].code,      codes[at].size, 0u,
                                          codes[at].exits,  codes[at].exit_count};
        names[retained] = codes[at].kernel;
        of[retained] = at;
        total += codes[at].size;
        retained += 1u;
    }
    // every section may move by its alignment, and every part's attributes grow by four bytes an exit
    const unsigned long long room = (pattern_size * 2ULL) + (total * 2ULL) + 1048576ULL;
    unsigned char *const cubin = (unsigned char *)platform_cell_taken((size_t)room, platform_place_g_program_owner, 0);
    unsigned long long cubin_size = 0ULL;
    const CubinKernels kernels = {pattern, pattern_size, parts, retained};
    char path[1400];
    int read = (parts != NULL) && (names != NULL) && (of != NULL) && (cubin != NULL) && (retained != 0u) &&
               cubin_kernels_write(&kernels, cubin, room, &cubin_size) &&
               (qry_member_streamed("cubin_listing_cubin", cubin, cubin_size, path, sizeof(path)) != 0ULL) &&
               (listing_read(nvdisasm, NULL, path) == 0);
    platform_cell_released(cubin, platform_place_g_program_owner);
    CubinListingCut cut;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &cut, .bytes = sizeof(cut), .val = 0);
    read = read && cubin_listing_cut(s_output, (size_t)s_output_size, names, retained, &cut);
    for (unsigned int at = 0u; read && (at < retained); at += 1u)
    {
        read = cut.found[at];
    }
    for (unsigned int at = 0u; read && (at < retained); at += 1u)
    {
        char name[96];
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, codes[of[at]].number);
            mmgr_line_put(&buffer_line, ".dis");
            (void)mmgr_line_finish(&buffer_line);
        }
        read = qry_member_append(name, cut.text + cut.start[at], cut.size[at], QRY_ORDINARY) != 0;
    }
    cubin_listing_cut_freed(&cut);
    platform_cell_released(parts, platform_place_g_program_owner);
    platform_cell_released(names, platform_place_g_program_owner);
    platform_cell_released(of, platform_place_g_program_owner);
    return read || (retained == 0u);
}

// the line of `list` at `*at` into `line`, which has room for `room`, `*at` moved past it: 1, or 0 where the list has
// ended
static int listing_line(const unsigned char *list, unsigned long long size, unsigned long long *at, char *line,
                        size_t room)
{
    if (*at >= size)
    {
        return 0;
    }
    const unsigned char *const end = (const unsigned char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = list + *at,
                                                                       .bytes = (size_t)(size - *at), .val = '\n');
    size_t length = (end != NULL) ? (size_t)(end - (list + *at)) : (size_t)(size - *at);
    const unsigned long long next = *at + length + 1ULL;
    length = ((length != 0u) && (list[*at + length - 1u] == '\r')) ? (length - 1u) : length;
    {
        VerbaLinea buffer_line = mmgr_line_over(line, room);
        mmgr_line_put_max(&buffer_line, (const char *)(list + *at), (size_t)((int)length));
        (void)mmgr_line_finish(&buffer_line);
    }
    *at = next;
    return 1;
}

int program_main(int count, char **words)
{
    if (count != 6)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line,
                          "orior_cubin_listing <pattern> <kernel prefix> <architecture> <nvdisasm> <codes>");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const char *const pattern_path = words[1];
    const char *const prefix = words[2];
    const char *const architecture = words[3];
    const char *const nvdisasm = words[4];
    QryShared *const run = qry_member_run();
    const unsigned char *list = NULL;
    unsigned long long list_size = 0ULL;
    const unsigned char *pattern = NULL;
    unsigned long long pattern_size = 0ULL;
    if ((run == NULL) || !qry_member_blob(words[5], &list, &list_size, NULL) ||
        !qry_member_file_blob(pattern_path, &pattern, &pattern_size))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_cubin_listing: no run, no list ");
            mmgr_line_put(&engine_note.line, words[5]);
            mmgr_line_put(&engine_note.line, " in it, or no pattern ");
            mmgr_line_put(&engine_note.line, pattern_path);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    unsigned int lines = 0u;
    for (unsigned long long at = 0ULL; at < list_size; at += 1ULL)
    {
        lines += (list[at] == '\n') ? 1u : 0u;
    }
    ListedCode *const codes =
        (ListedCode *)platform_cell_cleared((size_t)lines + 2u, sizeof(ListedCode), platform_place_g_program_owner, 0);
    if (codes == NULL)
    {
        return 2;
    }
    // the pattern's own EXIT encoding, read where its first kernel's info specifies its first exit lies
    unsigned long long exit_low = 0ULL;
    int exit_known = 0;
    unsigned int code_count = 0u;
    unsigned long long at = 0ULL;
    char line[1400];
    while (listing_line(list, list_size, &at, line, sizeof(line)))
    {
        if (line[0] == '\0')
        {
            continue;
        }
        ListedCode *const code = &codes[code_count];
        const char *const forward =
            EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
        const char *const back =
            EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
        const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
        const char *const base = (slash != NULL) ? (slash + 1) : line;
        const char *const dot =
            EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = base, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
        {
            VerbaLinea buffer_line = mmgr_line_over(code->number, sizeof(code->number));
            mmgr_line_put_max(&buffer_line, base,
                              (size_t)((int)((dot != NULL) ? (size_t)(dot - base)
                                                           : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = base,
                                                                        .cap = PLATFORM_TEXT_TO_TERMINATOR))));
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(code->kernel, sizeof(code->kernel));
            mmgr_line_put(&buffer_line, prefix);
            mmgr_line_put(&buffer_line, code->number);
            (void)mmgr_line_finish(&buffer_line);
        }
        if (!qry_member_file_blob(line, &code->code, &code->size) || ((code->size % 16ULL) != 0ULL))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "orior_cubin_listing: the code ");
                mmgr_line_put(&engine_note.line, line);
                mmgr_line_put(&engine_note.line, " could not be read, or is no whole count of instructions");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 2;
        }
        unsigned long long offset = 0ULL;
        unsigned long long size = 0ULL;
        unsigned int first_exit[1] = {0u};
        if (!exit_known && cubin_code_of(pattern, pattern_size, code->kernel, &offset, &size) &&
            (cubin_exits_read(pattern, pattern_size, code->kernel, first_exit, 1u) == 1u) &&
            ((first_exit[0] + 16ULL) <= size))
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &exit_low, .src = pattern + offset + first_exit[0], .bytes = 8u);
            exit_known = 1;
        }
        code_count += 1u;
    }
    for (unsigned int one = 0u; one < code_count; one += 1u)
    {
        ListedCode *const code = &codes[one];
        unsigned long long offset = 0ULL;
        unsigned long long size = 0ULL;
        const unsigned int room = (unsigned int)(code->size / 16ULL) + 1u;
        code->exits =
            (unsigned int *)platform_cell_cleared(room, sizeof(unsigned int), platform_place_g_program_owner, 0);
        code->exit_count = (exit_known && (code->exits != NULL))
                               ? cubin_exits_find(code->code, code->size, exit_low, code->exits, room)
                               : 0u;
        // a code whose kernel the pattern does not contain, or that contains no exit, is read alone
        code->aside = !cubin_code_of(pattern, pattern_size, code->kernel, &offset, &size) || (code->exit_count == 0u);
    }
    s_output_room = ((unsigned long long)code_count * 65536ULL) + 16777216ULL;
    s_output = (char *)platform_cell_taken((size_t)s_output_room, platform_place_g_program_owner, 0);
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
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, codes[one].number);
            mmgr_line_put(&buffer_line, ".dis");
            (void)mmgr_line_finish(&buffer_line);
        }
        const int streamed =
            qry_member_streamed("cubin_listing_alone", codes[one].code, codes[one].size, path, sizeof(path)) != 0ULL;
        const int status = streamed ? listing_read(nvdisasm, architecture, path) : -1;
        reached = reached && (status >= 0);
        if (status >= 0)
        {
            qry_member_append(name, s_output, s_output_size, QRY_ORDINARY);
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_cubin_listing: ");
        mmgr_line_decimal(output_line, code_count, 1u);
        mmgr_line_put(output_line, " codes read in ");
        mmgr_line_decimal(output_line, s_starts, 1u);
        mmgr_line_put(output_line, " starts of the vendor, ");
        mmgr_line_decimal(output_line, aside, 1u);
        mmgr_line_put(output_line, " of them read alone");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int one = 0u; one < code_count; one += 1u)
    {
        platform_cell_released(codes[one].exits, platform_place_g_program_owner);
    }
    platform_cell_released(codes, platform_place_g_program_owner);
    platform_cell_released(s_output, platform_place_g_program_owner);
    return reached ? 0 : 2;
}
