// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The record machine's lane as C source checked against the host oracle word for word, off the device (engine_table.md
// item 11(f), C a language of the register lane). record_test's programs, drawn from the same stream in the same
// order (record_image.h), are encoded, arranged and run by cycle_record_run_host; each is written as C by
// code_generator("c.krs") (lstar/protocol/table/c.krs) into one translation unit with a host shim ahead of it, which
// writes what NVRTC gives a device's C (__device__, __shared__, threadIdx, blockDim, atomicAdd) for one thread, and a
// bench after it, which reads the memory image, turns its byte offsets into the host's addresses, runs every lane and
// writes the errors and the records. The host's C++ compiler builds it and it runs in a work folder, and its records
// are compared with the host's word for word. Its lines give the same input digests as the host test's. Each lane of at
// max RECORD_C_TEXT_BYTES_MAX bytes is written again as the device writes it, on the host: the core's forms arranged as
// the assembly printer's records and written by the host oracle (asm_printer.h), checked against the code generator's
// text byte for byte.
#include "../../../../../../../src/cu/engine/engine_note.h"
#include "../../../../../../../src/platform/platform.h"
#include "asm_printer.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "code_generator.h"
#include "memoria_operor/memoria_linea.h"
#include "record_image.h"

#include <stddef.h>

// the longest lane the assembly printer writes on the host oracle here: a lane a byte, each a few hundred steps
#define RECORD_C_TEXT_BYTES_MAX 32768u

// the checks made and failed, and the programs the code generator did not write; where the test runs, and the compiler
// that builds each program
struct CResults
{
    unsigned int checks;
    unsigned int failed;
    unsigned int unsupported;
    const char *work;
    const char *compiler;
};

// the bytes of what a check names, its terminator among them
#define RECORD_C_WHAT_BYTES 512u

// the bytes a lane's program holds at max: the shim, the lane and the bench
#define RECORD_C_PROGRAM_BYTES (1ULL << 30u)

static void c_check(CResults *results, int passed, const char *name, const char *rest)
{
    results->checks += 1u;
    results->failed += passed ? 0u : 1u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, passed ? "ok  " : "FAIL");
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, rest);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

// what NVRTC gives a device's C, written for one thread on the host, and the launch as the prelude positions it out.
// The program's resident kernel (c.krs, program_unit) compiles against it and is not run: the bench runs the lanes
static const char s_c_shim[] = "#include <stdio.h>\n"
                               "#include <stdlib.h>\n"
                               "\n"
                               "typedef unsigned int u32;\n"
                               "typedef unsigned long long u64;\n"
                               "typedef signed char s8;\n"
                               "struct CycleHot\n"
                               "{\n"
                               "    u64 next_lane;\n"
                               "    u64 launch_start;\n"
                               "    u64 finished;\n"
                               "    u64 checkins;\n"
                               "};\n"
                               "struct CycleCompiledLaunch\n"
                               "{\n"
                               "    const u32 *in[3];\n"
                               "    const u32 *index;\n"
                               "    const u32 *tables;\n"
                               "    u32 *out;\n"
                               "    u32 *errored;\n"
                               "    u64 bodies[3];\n"
                               "    u64 count;\n"
                               "    CycleHot *hot;\n"
                               "    u64 *block;\n"
                               "    u64 ttl;\n"
                               "    u64 checkin_every;\n"
                               "    u64 launch_number;\n"
                               "    u64 places;\n"
                               "};\n"
                               "#define __device__\n"
                               "#define __shared__\n"
                               "#define __global__\n"
                               "#define __forceinline__ inline\n"
                               "#define __launch_bounds__(threads_)\n"
                               "static const struct\n"
                               "{\n"
                               "    u32 x;\n"
                               "} threadIdx = {0u}, blockDim = {1u}, gridDim = {1u};\n"
                               "static u32 atomicAdd(u32 *const at, const u32 value)\n"
                               "{\n"
                               "    const u32 previous = *at;\n"
                               "    *at = previous + value;\n"
                               "    return previous;\n"
                               "}\n"
                               "static u64 atomicAdd(u64 *const at, const u64 value)\n"
                               "{\n"
                               "    const u64 previous = *at;\n"
                               "    *at = previous + value;\n"
                               "    return previous;\n"
                               "}\n"
                               "static u64 atomicCAS(u64 *const at, const u64 compared, const u64 value)\n"
                               "{\n"
                               "    const u64 previous = *at;\n"
                               "    *at = (previous == compared) ? value : previous;\n"
                               "    return previous;\n"
                               "}\n"
                               "static void __syncthreads(void)\n"
                               "{\n"
                               "}\n"
                               "static void __threadfence(void)\n"
                               "{\n"
                               "}\n";

// The bench after the lane onto `text`. It reads the image from memory.txt and moves the launch's addresses from
// byte offsets to the host's, which an address of 0 is not. It runs every lane, and writes the errors, the records
// and a clock count of 0 to records.txt. `places` is the lane's in shared memory, a word and a sign byte each
static void c_bench(unsigned int places, PlatformText *text)
{
    VerbaLinea *const line = &text->line;
    const unsigned int words = places + ((places + 3u) / 4u) + 1u;
    mmgr_line_put(line, "\nu32 cycle_words[");
    mmgr_line_decimal(line, words, 1u);
    mmgr_line_put(line, "];\n");
    mmgr_line_put(line, "\nint main(void)\n{\n");
    mmgr_line_put(line, "    FILE *const given = fopen(\"memory.txt\", \"rb\");\n");
    mmgr_line_put(line, "    unsigned long long image_words = 0ull;\n");
    mmgr_line_put(line, "    unsigned long long lanes = 0ull;\n");
    mmgr_line_put(line, "    unsigned long long records = 0ull;\n");
    mmgr_line_put(line, "    unsigned long long record_words = 0ull;\n");
    mmgr_line_put(line, "    unsigned long long errored = 0ull;\n");
    mmgr_line_put(line, "    if ((given == NULL) || (fscanf(given, \"%llu %llu %llu %llu %llu\", &image_words, &lanes, "
                        "&records, &record_words, &errored) != 5))\n");
    mmgr_line_put(line, "    {\n        return 2;\n    }\n");
    mmgr_line_put(line, "    u32 *const memory = (u32 *)calloc((size_t)image_words + 2u, sizeof(u32));\n");
    mmgr_line_put(line, "    for (unsigned long long at = 0ull; at < image_words; at += 1ull)\n");
    mmgr_line_put(line,
                  "    {\n        if (fscanf(given, \"%x\", &memory[at]) != 1)\n        {\n            return 2;\n"
                  "        }\n    }\n");
    mmgr_line_put(line, "    fclose(given);\n");
    mmgr_line_put(line, "    const u64 base = (u64)memory;\n");
    const size_t addresses[] = {offsetof(CycleCompiledLaunch, in),       offsetof(CycleCompiledLaunch, in) + 8u,
                                offsetof(CycleCompiledLaunch, in) + 16u, offsetof(CycleCompiledLaunch, index),
                                offsetof(CycleCompiledLaunch, tables),   offsetof(CycleCompiledLaunch, out),
                                offsetof(CycleCompiledLaunch, error)};
    for (const size_t address : addresses)
    {
        mmgr_line_put(line, "    if (*(u64 *)((char *)memory + ");
        mmgr_line_decimal(line, address, 1u);
        mmgr_line_put(line, "u) != 0ull)\n");
        mmgr_line_put(line, "    {\n        *(u64 *)((char *)memory + ");
        mmgr_line_decimal(line, address, 1u);
        mmgr_line_put(line, "u) += base;\n    }\n");
    }
    mmgr_line_put(line, "    for (u64 lane = 0ull; lane < lanes; lane += 1ull)\n");
    mmgr_line_put(line, "    {\n        cycle_lane((const CycleCompiledLaunch *)memory, lane);\n    }\n");
    mmgr_line_put(line, "    FILE *const written = fopen(\"records.txt\", \"wb\");\n");
    mmgr_line_put(line, "    fprintf(written, \"%u\\n\", memory[errored / 4ull]);\n");
    mmgr_line_put(line, "    for (unsigned long long at = 0ull; at < record_words; at += 1ull)\n");
    mmgr_line_put(line, "    {\n        fprintf(written, \"%08X\\n\", memory[(records / 4ull) + at]);\n    }\n");
    mmgr_line_put(line, "    fprintf(written, \"0\\n\");\n");
    mmgr_line_put(line, "    return (fclose(written) == 0) ? 0 : 1;\n}\n");
}

// the file `name` in the folder `work` into `path`, `room` bytes of it
static const char *c_path(const char *work, const char *name, char *path, size_t room)
{
    VerbaLinea line = mmgr_line_over(path, room);
    mmgr_line_put(&line, work);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    (void)mmgr_line_finish(&line);
    return path;
}

// the first 30 lines of the log at `path` on stdout, as head shows them, after a line that names it
static void c_log_shown(const char *path)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "==> ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, " <==\n");
        platform_line_out_ended();
    }
    PlatformFile *const log = platform_file_open(path, "r");
    char log_text[1024];
    unsigned int shown = 0u;
    while ((log != NULL) && (shown < 30u) && (platform_file_text_read(log_text, (int)sizeof(log_text), log) != NULL))
    {
        mmgr_line_put(platform_line_out(), log_text);
        const size_t length = mmgr_text_length(log_text, PLATFORM_TEXT_TO_TERMINATOR);
        shown += ((length != 0u) && (log_text[length - 1u] == '\n')) ? 1u : 0u;
    }
    if (log != NULL)
    {
        (void)platform_file_close(log);
    }
    platform_line_flushed();
}

// The lane, the program in the work folder, built by the compiler and run there. The answer is 0 where both ran and
// succeeded, else the exit code of the step that failed
static long c_built_and_run(const CResults *results)
{
    char program[PLATFORM_PATH_BYTES];
    char build_log[PLATFORM_PATH_BYTES];
    char run_log[PLATFORM_PATH_BYTES];
    (void)c_path(results->work, "program", program, sizeof(program));
    (void)c_path(results->work, "build.log", build_log, sizeof(build_log));
    (void)c_path(results->work, "run.log", run_log, sizeof(run_log));
    const char *const build[] = {results->compiler,
                                 "-std=c++17",
                                 "-O1",
                                 "-Wall",
                                 "-Wextra",
                                 "-Wno-unused-variable",
                                 "-Wno-unused-but-set-variable",
                                 "-Wno-unused-label",
                                 "-Wno-unused-function",
                                 "program.cpp",
                                 "-o",
                                 "program",
                                 NULL};
    const char *const run[] = {program, NULL};
    const long built = platform_program_run_in(build, results->work, build_log, NULL, 0);
    return (built == 0L) ? platform_program_run_in(run, results->work, run_log, NULL, 0) : built;
}

// what one program of the test shares: its name, its layout, the host's records and their words, and its lane
struct CProgram
{
    char name[RECORD_C_WHAT_BYTES];
    const EngineRecordLayout *layout;
    unsigned int *host_records;
    unsigned long long record_words;
    unsigned int places;
};

// The lane as the device writes it, on the host, checked against `lane`. The functions the device runs arrange the
// core's forms as the assembly printer's records, and the host oracle writes them (asm_printer.h)
static void c_lane_from_items(CResults *results, const CProgram *program, CodeGenerator &generator,
                              const TargetInfo *target, const PlatformText *lane)
{
    PlatformArray items;
    platform_array_begun(&items, sizeof(MachineInstr), platform_place_g_program_owner, 0);
    unsigned int item_places = 0u;
    AsmPrinterRuleset text_rules;
    mmgr_bytes_set(&text_rules, 0, sizeof(text_rules));
    char error_text[RECORD_C_WHAT_BYTES];
    VerbaLinea error = mmgr_line_over(error_text, sizeof(error_text));
    PlatformText from_items;
    platform_text_begun(&from_items, RECORD_C_PROGRAM_BYTES, platform_place_g_program_owner, 0);
    const int decided = generator.decided(program->layout, &item_places, &items);
    const int built = decided && asm_printer_ruleset_build(generator.ruleset(0), target, s_c_shim, &text_rules, &error);
    const int ran =
        built && asm_printer_host(&text_rules, (const MachineInstr *)items.cell, items.count, &from_items, &error);
    (void)mmgr_line_finish(&error);
    if (built)
    {
        asm_printer_ruleset_release(&text_rules);
    }
    if (ran == 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, program->name);
        mmgr_line_put(output_line, ": the assembly printer did not write the lane (");
        mmgr_line_put(output_line, decided ? error_text : "the core's forms were not decided");
        mmgr_line_put(output_line, ")");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long length = platform_text_size(lane);
    const int same = ran && (platform_text_size(&from_items) == length) &&
                     (mmgr_bytes_compare(from_items.cell, lane->cell, (size_t)length) == 0);
    c_check(results, same && (item_places == program->places), program->name,
            " written from the core's forms by the assembly printer is the code generator's text");
    platform_text_ended(&from_items);
    platform_array_ended(&items);
}

// The lane after the shim, with the bench after it, written as the work folder's program.cpp, and the image as its
// memory.txt. The test builds and runs it there, and reads back its errors, records and clocks. The answer is 1 where
// the records read back whole, and `status` is the build's or the run's exit code
static int c_lane_run(const CResults *results, const PlatformText *lane, unsigned int places, const RecordImage *image,
                      unsigned int *error, unsigned int *records, unsigned long long *clocks, long *status)
{
    char records_path[PLATFORM_PATH_BYTES];
    char program_path[PLATFORM_PATH_BYTES];
    char memory_path[PLATFORM_PATH_BYTES];
    (void)c_path(results->work, "records.txt", records_path, sizeof(records_path));
    (void)c_path(results->work, "program.cpp", program_path, sizeof(program_path));
    (void)c_path(results->work, "memory.txt", memory_path, sizeof(memory_path));
    (void)platform_file_removed(records_path);
    PlatformText program;
    platform_text_begun(&program, RECORD_C_PROGRAM_BYTES, platform_place_g_program_owner, 0);
    (void)mmgr_line_put_n(&program.line, lane->cell, (size_t)platform_text_size(lane));
    c_bench(places, &program);
    const int written =
        (mmgr_line_ok(&program.line) != 0) &&
        platform_file_written(program_path, platform_text_bytes(&program), platform_text_size(&program)) &&
        record_image_write(memory_path, image);
    platform_text_ended(&program);
    *status = written ? c_built_and_run(results) : -1L;
    return (*status == 0L) && record_image_read(records_path, image, error, records, clocks);
}

// The program arranged, run on the host, written as C, built and run over the same atoms; its line printed and its
// check made. `errors` is 1 for a program the host must error: the lane as C must then error on a lane
static void c_run(void *context, const HostProgram *host_program, int reuse, unsigned int *const *atoms,
                  const unsigned long long *bodies, const unsigned int *index, int errors)
{
    CResults *const results = (CResults *)context;
    CProgram program;
    mmgr_bytes_set(&program, 0, sizeof(program));
    VerbaLinea name_line = mmgr_line_over(program.name, sizeof(program.name));
    mmgr_line_put(&name_line, host_program->name);
    mmgr_line_put(&name_line, reuse ? " (registers reused)" : "");
    (void)mmgr_line_finish(&name_line);
    HostLoaded loaded;
    if (host_load(host_program, reuse, &loaded) == 0)
    {
        c_check(results, 0, program.name, " is arranged");
        return;
    }
    const EngineRecordLayout *const layout = &loaded.layout;
    program.layout = layout;
    program.record_words = (unsigned long long)HOST_TEST_LANES * layout->out_limbs;
    const unsigned long long room = (program.record_words != 0ull) ? program.record_words : 1ull;
    program.host_records =
        (unsigned int *)platform_cell_cleared(room, sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const records =
        (unsigned int *)platform_cell_cleared(room, sizeof(unsigned int), platform_place_g_program_owner, 0);
    if ((program.host_records == NULL) || (records == NULL))
    {
        c_check(results, 0, program.name, " has room for its records");
        platform_cell_released(records, platform_place_g_program_owner);
        platform_cell_released(program.host_records, platform_place_g_program_owner);
        host_free(&loaded);
        return;
    }
    CycleRecordHostRequest request;
    mmgr_bytes_set(&request, 0, sizeof(request));
    request.layout = layout;
    for (unsigned int member = 0u; member < layout->members; member += 1u)
    {
        request.in[member] = atoms[member];
        request.bodies[member] = bodies[member];
    }
    request.index = index;
    request.count = HOST_TEST_LANES;
    request.out = program.host_records;
    request.error = &loaded.error;
    const long ran = cycle_record_run_host(&request);
    unsigned long long inputs = HOST_TEST_FNV_BASIS;
    for (unsigned int member = 0u; member < layout->members; member += 1u)
    {
        inputs ^= host_digest(atoms[member], bodies[member] * layout->in_limbs[member]);
    }
    CodeGenerator &generator = code_generator("c.krs");
    const TargetInfo target = {0ull, 0, 0, 0, 0, ""};
    unsigned int live = 0u;
    PlatformText lane;
    platform_text_begun(&lane, RECORD_C_PROGRAM_BYTES, platform_place_g_program_owner, 0);
    const int wrote =
        (generator.ruleset(1) != NULL) && generator.program(layout, &target, s_c_shim, &lane, &program.places, &live);
    if (!wrote)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, program.name);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, layout->steps, 1u);
            mmgr_line_put(output_line,
                          " steps, not supported in C (a step the lane does not support, or its ruleset errored)");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        results->unsupported += 1u;
        platform_text_ended(&lane);
        platform_cell_released(records, platform_place_g_program_owner);
        platform_cell_released(program.host_records, platform_place_g_program_owner);
        host_free(&loaded);
        return;
    }
    if (platform_text_size(&lane) <= RECORD_C_TEXT_BYTES_MAX)
    {
        c_lane_from_items(results, &program, generator, &target, &lane);
    }
    RecordImage image;
    unsigned int error = 0u;
    unsigned long long clocks = 0ull;
    long status = -1L;
    const int read = record_image(layout, atoms, bodies, index, HOST_TEST_LANES, &image) &&
                     c_lane_run(results, &lane, program.places, &image, &error, records, &clocks, &status);
    record_image_ended(&image);
    if (read == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, program.name);
            mmgr_line_put(output_line, ": the lane as C did not build or run (status ");
            mmgr_line_decimal_signed(output_line, status);
            mmgr_line_put(output_line, "); its logs begin:");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        char build_log[PLATFORM_PATH_BYTES];
        char run_log[PLATFORM_PATH_BYTES];
        c_log_shown(c_path(results->work, "build.log", build_log, sizeof(build_log)));
        c_log_shown(c_path(results->work, "run.log", run_log, sizeof(run_log)));
    }
    const int host_ran = ran == (long)HOST_TEST_LANES;
    unsigned long long lines = 0ull;
    for (unsigned long long at = 0ull; at < platform_text_size(&lane); at += 1ull)
    {
        lines += (lane.cell[at] == '\n') ? 1ull : 0ull;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, program.name);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, layout->steps, 1u);
        mmgr_line_put(output_line, " steps, ");
        mmgr_line_decimal(output_line, layout->out_limbs, 1u);
        mmgr_line_put(output_line, " out limbs, ");
        mmgr_line_decimal(output_line, lines, 1u);
        mmgr_line_put(output_line, " lines of C, ");
        mmgr_line_decimal(output_line, program.places, 1u);
        mmgr_line_put(output_line, " places, inputs ");
        mmgr_line_hex(output_line, inputs, 16u);
        mmgr_line_put(output_line, ", records ");
        mmgr_line_hex(output_line, read ? host_digest(records, program.record_words) : 0ull, 16u);
        mmgr_line_put(output_line, " as C, ");
        mmgr_line_hex(output_line, host_ran ? host_digest(program.host_records, program.record_words) : 0ull, 16u);
        mmgr_line_put(output_line, " on the host, ");
        mmgr_line_decimal(output_line, error, 1u);
        mmgr_line_put(output_line, " lanes errored as C");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (errors != 0)
    {
        c_check(results, read && !host_ran && (error != 0u), program.name,
                " as C errors on a lane where the host errors on the run");
    }
    else
    {
        const int same = mmgr_bytes_compare(records, program.host_records,
                                            (size_t)(program.record_words * sizeof(unsigned int))) == 0;
        c_check(results, read && host_ran && (error == 0u) && same, program.name,
                " as C writes the host's records word for word");
    }
    platform_text_ended(&lane);
    platform_cell_released(records, platform_place_g_program_owner);
    platform_cell_released(program.host_records, platform_place_g_program_owner);
    host_free(&loaded);
}

int program_main(int argc, char **argv)
{
    if (argc != 3)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "usage: record_c_test <work folder> <C++ compiler>\n");
        engine_note_written(&engine_note);
        return 2;
    }
    CResults results = {0u, 0u, 0u, argv[1], argv[2]};
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  record c test: ");
        mmgr_line_decimal(output_line, HOST_TEST_LANES, 1u);
        mmgr_line_put(output_line, " lanes a program, ORIOR_EXACT_LIMBS ");
        mmgr_line_decimal(output_line, (unsigned int)ORIOR_EXACT_LIMBS, 1u);
        mmgr_line_put(output_line, ", built by ");
        mmgr_line_put(output_line, argv[2]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    record_image_programs(&results, c_run);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  record c test: ");
        mmgr_line_decimal(output_line, results.checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, results.failed, 1u);
        mmgr_line_put(output_line, " failed, ");
        mmgr_line_decimal(output_line, results.unsupported, 1u);
        mmgr_line_put(output_line, " programs not supported in C");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (results.failed == 0u) ? 0 : 1;
}
