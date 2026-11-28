// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef RECORD_IMAGE_H
#define RECORD_IMAGE_H

// The tests that run a language's lane off the device and check it against the host oracle (record_vhdl_test.cpp,
// record_c_test.cpp): the programs drawn as record_test draws them, in its order, each passed to the test's own
// run; the memory image a lane runs over, arranged as the device arranges its launch; and the files a lane's bench
// reads the image from and writes its records to. It compiles as C++

#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "record_programs.h"
#include "target.h"

#include <stddef.h>

// the launch's words at address 0 of the memory image, before every array
#define RECORD_IMAGE_LAUNCH_WORDS 64u

static_assert(sizeof(CycleCompiledLaunch) <= (4u * RECORD_IMAGE_LAUNCH_WORDS),
              "record image: the launch fits its words");

// the launch's number, which the program's block names as its owner
#define RECORD_IMAGE_LAUNCH_NUMBER 1ull

// The image: its words, an array of unsigned int, each address in it a byte offset from its first word. Also the
// lanes it holds records for, the records' address and words, and the errors' address
struct RecordImage
{
    PlatformArray memory;
    unsigned long long lanes;
    unsigned long long records;
    unsigned long long record_words;
    unsigned long long error;
};

// the words of the image [BORROWS]
static unsigned int *record_image_words(const RecordImage *image)
{
    return (unsigned int *)image->memory.cell;
}

// `count` words arranged at the end of the image, and their byte address into `address`: 1, or 0 where the pool cannot
// meet the room
static int record_image_layout(PlatformArray *memory, const unsigned int *words, unsigned long long count,
                               unsigned long long *address)
{
    const unsigned long long held = memory->count;
    *address = 4ull * held;
    if (platform_array_counted(memory, held + count) == 0)
    {
        return 0;
    }
    if (count != 0ull)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = platform_array_at(memory, held), .src = words,
                   .bytes = (size_t)(count * sizeof(unsigned int)));
    }
    return 1;
}

// the image's words grown by `count` zero words: 1, or 0 where the pool cannot meet the room
static int record_image_grown(PlatformArray *memory, unsigned long long count)
{
    return platform_array_counted(memory, memory->count + count);
}

// a 64-bit field of the launch, little-endian, at its byte offset
static void record_image_wide(PlatformArray *memory, size_t offset, unsigned long long value)
{
    unsigned int *const words = (unsigned int *)memory->cell;
    words[offset / 4u] = (unsigned int)(value & 0xFFFFFFFFull);
    words[(offset / 4u) + 1u] = (unsigned int)(value >> 32u);
}

// The image of the first `lanes` lanes of a program into `image`, at max HOST_TEST_LANES. The launch is at 0, arranged
// as CycleCompiledLaunch, then the members' atoms, the index, the tables, the records, the errors, the hot words and
// the program's block. The image is ephemeral work on the temporary tier, and its words come in one take at their
// full size. 1, or 0 where the pool cannot meet the image, which record_image_ended then gives back
static int record_image(const EngineRecordLayout *layout, unsigned int *const *atoms, const unsigned long long *bodies,
                        const unsigned int *index, unsigned long long lanes, RecordImage *image)
{
    platform_array_begun(&image->memory, sizeof(unsigned int), platform_place_g_program_owner, 1);
    image->lanes = lanes;
    image->records = 0ull;
    image->record_words = lanes * layout->out_limbs;
    image->error = 0ull;
    PlatformArray *const memory = &image->memory;
    // every word of the image, and one more that the hot words' alignment can take
    unsigned long long words = RECORD_IMAGE_LAUNCH_WORDS + image->record_words + 2ull + (sizeof(CycleHot) / 4u) +
                               (sizeof(EngineProgramBlock) / 4u) + layout->table_word_count;
    for (unsigned int member = 0u; member < layout->members; member += 1u)
    {
        words += bodies[member] * layout->in_limbs[member];
    }
    words += (index != NULL) ? (lanes * layout->members) : 0ull;
    int ok = (lanes <= HOST_TEST_LANES) && platform_array_counted(memory, words) &&
             platform_array_counted(memory, 0ull) && record_image_grown(memory, RECORD_IMAGE_LAUNCH_WORDS);
    for (unsigned int member = 0u; (ok != 0) && (member < layout->members); member += 1u)
    {
        unsigned long long address = 0ull;
        ok = record_image_layout(memory, atoms[member], bodies[member] * layout->in_limbs[member], &address);
        if (ok != 0)
        {
            record_image_wide(memory, offsetof(CycleCompiledLaunch, in) + (8u * (size_t)member), address);
            record_image_wide(memory, offsetof(CycleCompiledLaunch, bodies) + (8u * (size_t)member), bodies[member]);
        }
    }
    unsigned long long address = 0ull;
    if ((ok != 0) && (index != NULL))
    {
        ok = record_image_layout(memory, index, lanes * layout->members, &address);
        if (ok != 0)
        {
            record_image_wide(memory, offsetof(CycleCompiledLaunch, index), address);
        }
    }
    if ((ok != 0) && (layout->table_word_count != 0ull))
    {
        ok = record_image_layout(memory, layout->table_values, layout->table_word_count, &address);
        if (ok != 0)
        {
            record_image_wide(memory, offsetof(CycleCompiledLaunch, tables), address);
        }
    }
    image->records = 4ull * memory->count;
    ok = ok && record_image_grown(memory, image->record_words);
    image->error = 4ull * memory->count;
    ok = ok && record_image_grown(memory, 1ull);
    // the hot words and the program's block, each at an address of whole 64-bit words: the next lane 0, and the block
    // owned by the launch's number and told to run, as the host positions them out before the first launch
    ok = ok && record_image_grown(memory, memory->count % 2ull);
    const unsigned long long hot = 4ull * memory->count;
    ok = ok && record_image_grown(memory, sizeof(CycleHot) / 4u);
    const unsigned long long block = 4ull * memory->count;
    ok = ok && record_image_grown(memory, sizeof(EngineProgramBlock) / 4u);
    if (ok == 0)
    {
        return 0;
    }
    record_image_wide(memory, (size_t)block + offsetof(EngineProgramBlock, owner), RECORD_IMAGE_LAUNCH_NUMBER);
    record_image_wide(memory, (size_t)block + offsetof(EngineProgramBlock, command), ENGINE_PROGRAM_RUN);
    record_image_wide(memory, offsetof(CycleCompiledLaunch, out), image->records);
    record_image_wide(memory, offsetof(CycleCompiledLaunch, error), image->error);
    record_image_wide(memory, offsetof(CycleCompiledLaunch, count), lanes);
    record_image_wide(memory, offsetof(CycleCompiledLaunch, hot), hot);
    record_image_wide(memory, offsetof(CycleCompiledLaunch, block), block);
    record_image_wide(memory, offsetof(CycleCompiledLaunch, launch_number), RECORD_IMAGE_LAUNCH_NUMBER);
    return 1;
}

// the cell of the image given back
static void record_image_ended(RecordImage *image)
{
    platform_array_ended(&image->memory);
}

// the bytes the image's file holds at max
#define RECORD_IMAGE_FILE_BYTES (1ULL << 30u)

// The image and its line of places as a bench reads them: its words, the lanes, the records' address and words and the
// errors' address, then each word in hex. 1 where the whole file went
static int record_image_write(const char *path, const RecordImage *image)
{
    PlatformText text;
    platform_text_begun(&text, RECORD_IMAGE_FILE_BYTES, platform_place_g_program_owner, 0);
    VerbaLinea *const line = &text.line;
    mmgr_line_decimal(line, image->memory.count, 1u);
    mmgr_line_ch(line, ' ');
    mmgr_line_decimal(line, image->lanes, 1u);
    mmgr_line_ch(line, ' ');
    mmgr_line_decimal(line, image->records, 1u);
    mmgr_line_ch(line, ' ');
    mmgr_line_decimal(line, image->record_words, 1u);
    mmgr_line_ch(line, ' ');
    mmgr_line_decimal(line, image->error, 1u);
    mmgr_line_ch(line, '\n');
    const unsigned int *const words = record_image_words(image);
    for (unsigned long long at = 0ull; at < image->memory.count; at += 1ull)
    {
        mmgr_line_hex(line, words[at], 8u);
        mmgr_line_ch(line, '\n');
    }
    const int written =
        (mmgr_line_ok(line) != 0) && platform_file_written(path, platform_text_bytes(&text), platform_text_size(&text));
    platform_text_ended(&text);
    return written;
}

// The next number of the text at `*at` in `base`, past the white space before it, into `value`, and `*at` after it. 1,
// or 0 where no digit comes
static int record_image_number(const char **at, unsigned int base, unsigned long long *value)
{
    const char *end = NULL;
    *value = (unsigned long long)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = *at, .end = &end, .base = base);
    const int read = (end != NULL) && (end != *at);
    *at = (end != NULL) ? end : *at;
    return read;
}

// The errors, the records' words and the clocks a bench wrote, into `error`, `records` (room for the image's record
// words) and `clocks`, the clocks 0 from a language that is not clocked. 0 where the call cannot read them whole
static int record_image_read(const char *path, const RecordImage *image, unsigned int *error, unsigned int *records,
                             unsigned long long *clocks)
{
    unsigned long long size = 0ull;
    char *const text = (platform_file_size(path, &size) != 0)
                           ? (char *)platform_cell_taken(size + 1ull, platform_place_g_program_owner, 1)
                           : NULL;
    unsigned long long read = 0ull;
    if ((text == NULL) || (platform_file_read(path, text, size, &read) == 0))
    {
        platform_cell_released(text, platform_place_g_program_owner);
        return 0;
    }
    text[read] = '\0';
    const char *at = text;
    unsigned long long value = 0ull;
    int passed = record_image_number(&at, 10u, &value);
    // a bench's error count is a count of lanes, far under 2^32
    *error = (unsigned int)value;
    for (unsigned long long word = 0ull; passed && (word < image->record_words); word += 1ull)
    {
        passed = record_image_number(&at, 16u, &value);
        // a record's word is the 32 bits the bench wrote in hex
        records[word] = (unsigned int)value;
    }
    passed = passed && record_image_number(&at, 10u, clocks);
    platform_cell_released(text, platform_place_g_program_owner);
    return passed;
}

// a test's run of one program: the program, 1 where its registers are reused, its members' atoms and bodies, the
// index or NULL, and 1 where the host must error on it
typedef void (*RecordImageRun)(void *context, const HostProgram *program, int reuse, unsigned int *const *atoms,
                               const unsigned long long *bodies, const unsigned int *index, int errors);

// every program record_test runs, drawn as it draws them, in its order, each passed to `run`
static void record_image_programs(void *context, RecordImageRun run)
{
    HostProgram program;
    const unsigned long long one_body[1] = {HOST_TEST_LANES};

    host_arithmetic(&program);
    unsigned int *atoms[2] = {host_atoms(program.in_limbs[0], HOST_TEST_LANES), NULL};
    run(context, &program, 0, atoms, one_body, NULL, 0);
    run(context, &program, 1, atoms, one_body, NULL, 0);
    platform_cell_released(atoms[0], platform_place_g_program_owner);

    host_division(&program);
    atoms[0] = host_atoms(program.in_limbs[0], HOST_TEST_LANES);
    run(context, &program, 0, atoms, one_body, NULL, 0);
    HostProgram bare;
    host_bare_divisor(&bare);
    host_zero_divisor(atoms[0]);
    run(context, &bare, 0, atoms, one_body, NULL, 1);
    HostProgram inexact;
    host_inexact(&inexact, &bare);
    run(context, &inexact, 0, atoms, one_body, NULL, 1);
    platform_cell_released(atoms[0], platform_place_g_program_owner);

    host_bitwise(&program);
    atoms[0] = host_atoms(program.in_limbs[0], HOST_TEST_LANES);
    run(context, &program, 0, atoms, one_body, NULL, 0);
    platform_cell_released(atoms[0], platform_place_g_program_owner);

    unsigned int *const wide =
        (unsigned int *)platform_cell_taken(512u * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const narrow =
        (unsigned int *)platform_cell_taken(4096u * sizeof(unsigned int), platform_place_g_program_owner, 0);
    host_tables(&program, wide, narrow);
    atoms[0] = host_atoms(program.in_limbs[0], HOST_TEST_LANES);
    run(context, &program, 0, atoms, one_body, NULL, 0);
    platform_cell_released(atoms[0], platform_place_g_program_owner);
    platform_cell_released(wide, platform_place_g_program_owner);
    platform_cell_released(narrow, platform_place_g_program_owner);

    host_members(&program);
    atoms[0] = host_atoms(program.in_limbs[0], HOST_TEST_FIRST_BODIES);
    atoms[1] = host_atoms(program.in_limbs[1], HOST_TEST_SECOND_BODIES);
    const unsigned long long bodies[2] = {HOST_TEST_FIRST_BODIES, HOST_TEST_SECOND_BODIES};
    unsigned int *const index = (unsigned int *)platform_cell_taken((size_t)HOST_TEST_LANES * 2u * sizeof(unsigned int),
                                                                    platform_place_g_program_owner, 0);
    for (unsigned int lane = 0u; lane < HOST_TEST_LANES; lane += 1u)
    {
        index[2u * lane] = host_random() % HOST_TEST_FIRST_BODIES;
        index[(2u * lane) + 1u] = host_random() % HOST_TEST_SECOND_BODIES;
    }
    run(context, &program, 0, atoms, bodies, index, 0);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  index ");
        mmgr_line_hex(output_line, host_digest(index, (unsigned long long)HOST_TEST_LANES * 2u), 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(index, platform_place_g_program_owner);
    platform_cell_released(atoms[0], platform_place_g_program_owner);
    platform_cell_released(atoms[1], platform_place_g_program_owner);

    host_affine_limit(&program);
    atoms[0] =
        (unsigned int *)platform_cell_cleared(HOST_TEST_LANES, sizeof(unsigned int), platform_place_g_program_owner, 0);
    atoms[1] = NULL;
    run(context, &program, 0, atoms, one_body, NULL, 0);
    platform_cell_released(atoms[0], platform_place_g_program_owner);
}

#endif
