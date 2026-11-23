// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_compile_route.cu: caching programs, the route and the device's code generator
#include "../../../../../includes/file_defs/krs/ruleset_reader.h"
#include "../../engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "cycle_compile_internal.h"
#include "memoria_operor/memoria_operor.h"

// the programs compiled in this process, a CycleCompiledProgram each; the array begins at its first program
static PlatformArray s_cycle_programs;

// the bytes a lane's text holds at max
#define CYCLE_LANE_TEXT_BYTES (1ULL << 30u)

// the program of this process whose source is the `length` bytes at `text`, else NULL
static CycleCompiledProgram *cycle_program_found(const char *text, unsigned long long length)
{
    for (unsigned long long at = 0ULL; at < s_cycle_programs.count; at += 1ULL)
    {
        CycleCompiledProgram *const program = (CycleCompiledProgram *)platform_array_at(&s_cycle_programs, at);
        if ((program->length == length) && platform_movable_same(program->source, 0ULL, text, length))
        {
            return program;
        }
    }
    return NULL;
}

// the program found in this process by its text, else in the cache, else built and cached in both, then loaded as a
// library: PTX where `ptx`, which nvJitLink assembles as it links it alone, else C source that
// NVRTC compiles first. `written` is the whole nanoseconds the text took to write, for the report. 0 where the build or
// the load failed
static int cycle_program_load(const EngineRecordLayout *layout, CycleRecord *record, const CycleTarget *lane_target,
                              const char *text, unsigned long long length, int ptx, int lto, unsigned long long written,
                              int report)
{
    const char *const type = (ptx != 0) ? "PTX" : ((lto != 0) ? "LTO-IR" : "relocatable cubin");
    if (s_cycle_programs.element == 0ULL)
    {
        platform_array_begun(&s_cycle_programs, sizeof(CycleCompiledProgram), platform_place_g_program_owner, 0);
    }
    CycleCompiledProgram *const held = cycle_program_found(text, length);
    if (held != NULL)
    {
        held->holders += 1ULL;
        record->kernel = held->kernel;
        record->compiled = 1u;
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps found in this process, as ");
            mmgr_line_put(&engine_note.line, type);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    char folder[PLATFORM_PATH_BYTES];
    char path[PLATFORM_PATH_BYTES];
    const int cached = cycle_cache_folder(folder, sizeof(folder));
    if (cached != 0)
    {
        cycle_cache_path(folder, text, length, path, sizeof(path));
    }
    CycleImage cubin = {NULL, 0ULL};
    const int found = (cached != 0) && cycle_cache_read(path, text, length, &cubin);
    unsigned long long object_bytes = 0ULL;
    unsigned long long compile_nanoseconds = 0ULL;
    unsigned long long link_nanoseconds = 0ULL;
    if (!found)
    {
        const unsigned long long began = platform_clock_ns();
        // PTX goes to nvJitLink as its text with the NUL that ends it
        CycleImage object = {NULL, 0ULL};
        const int compiled_ok = (ptx != 0) ? cycle_image_taken(&object, text, length + 1ULL)
                                           : cycle_program_compile(text, "cycle_program.cu", lane_target->major,
                                                                   lane_target->minor, lto, report, &object);
        const unsigned long long compiled = platform_clock_ns();
        (void)((compiled_ok != 0) &&
               cycle_program_link(lane_target, object.bytes, object.count, lto, ptx, report, &cubin));
        const unsigned long long linked = platform_clock_ns();
        object_bytes = object.count;
        cycle_image_ended(&object);
        // the monotonic clock never runs back
        compile_nanoseconds = compiled - began;
        link_nanoseconds = linked - compiled;
        if ((cubin.bytes != NULL) && (cached != 0))
        {
            cycle_cache_write(folder, path, text, length, cubin.bytes, cubin.count);
        }
    }
    CycleCompiledProgram program;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &program, .bytes = sizeof(program), .val = 0);
    program.holders = 1ULL;
    program.length = length;
    // the source the process keeps to find the program by, in a movable cell
    program.source = cycle_kept(text, length);
    const int loaded =
        (cubin.bytes != NULL) && (program.source != NULL) &&
        (cudaLibraryLoadData(&program.library, cubin.bytes, NULL, NULL, 0u, NULL, NULL, 0u) == cudaSuccess) &&
        (cudaLibraryGetKernel(&program.kernel, program.library, "cycle_program") == cudaSuccess) &&
        platform_array_appended(&s_cycle_programs, &program);
    const unsigned long long cubin_bytes = cubin.count;
    const int had_cubin = cubin.bytes != NULL;
    cycle_image_ended(&cubin);
    if (!loaded)
    {
        platform_movable_released(program.source, platform_place_g_program_owner);
        if (program.library != NULL)
        {
            cudaLibraryUnload(program.library);
        }
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps as ");
            mmgr_line_put(&engine_note.line, type);
            mmgr_line_put(&engine_note.line, " did not build (");
            mmgr_line_put(&engine_note.line,
                          !had_cubin ? ((ptx != 0) ? "nvJitLink errored on it" : "NVRTC or nvJitLink errored on it")
                                     : "its cubin did not load");
            mmgr_line_put(&engine_note.line, ")");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    record->kernel = program.kernel;
    record->compiled = 1u;
    if ((report != 0) && found)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps read from the cache, ");
        mmgr_line_decimal(&engine_note.line, cubin_bytes, 1u);
        mmgr_line_put(&engine_note.line, " bytes of cubin, as ");
        mmgr_line_put(&engine_note.line, type);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if ((report != 0) && (ptx != 0))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps for sm_");
        mmgr_line_decimal_signed(&engine_note.line, lane_target->major);
        mmgr_line_decimal_signed(&engine_note.line, lane_target->minor);
        mmgr_line_put(&engine_note.line, " as PTX: written in ");
        mmgr_line_decimal(&engine_note.line, written, 1u);
        mmgr_line_put(&engine_note.line, " ns to ");
        mmgr_line_decimal(&engine_note.line, object_bytes, 1u);
        mmgr_line_put(&engine_note.line, " bytes, assembled and linked in ");
        mmgr_line_decimal(&engine_note.line, link_nanoseconds, 1u);
        mmgr_line_put(&engine_note.line, " ns to ");
        mmgr_line_decimal(&engine_note.line, cubin_bytes, 1u);
        mmgr_line_put(&engine_note.line, " bytes of cubin");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if (report != 0)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps for sm_");
        mmgr_line_decimal_signed(&engine_note.line, lane_target->major);
        mmgr_line_decimal_signed(&engine_note.line, lane_target->minor);
        mmgr_line_put(&engine_note.line, " as ");
        mmgr_line_put(&engine_note.line, type);
        mmgr_line_put(&engine_note.line, ": written in ");
        mmgr_line_decimal(&engine_note.line, written, 1u);
        mmgr_line_put(&engine_note.line, " ns, compiled in ");
        mmgr_line_decimal(&engine_note.line, compile_nanoseconds, 1u);
        mmgr_line_put(&engine_note.line, " ns to ");
        mmgr_line_decimal(&engine_note.line, object_bytes, 1u);
        mmgr_line_put(&engine_note.line, " bytes, linked in ");
        mmgr_line_decimal(&engine_note.line, link_nanoseconds, 1u);
        mmgr_line_put(&engine_note.line, " ns to ");
        mmgr_line_decimal(&engine_note.line, cubin_bytes, 1u);
        mmgr_line_put(&engine_note.line, " bytes of cubin");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return 1;
}

// a reference to a program loaded in this process given back, where `kernel` is one; the last reference released
// unloads it
void cycle_program_release(cudaKernel_t kernel)
{
    for (unsigned long long at = 0ULL; (kernel != NULL) && (at < s_cycle_programs.count); at += 1ULL)
    {
        CycleCompiledProgram *const program = (CycleCompiledProgram *)platform_array_at(&s_cycle_programs, at);
        if (program->kernel == kernel)
        {
            program->holders -= 1ULL;
            if (program->holders == 0ULL)
            {
                cudaLibraryUnload(program->library);
                platform_movable_released(program->source, platform_place_g_program_owner);
                // the programs after it move down one place, and the order of the rest holds
                const unsigned long long after = s_cycle_programs.count - at - 1ULL;
                (((const unsigned char *)(program) <= (const unsigned char *)(program + 1))
                     ? EMBED_CALL(memor.move_down, MemoriaCfg, .dst = program, .src = program + 1,
                                  .bytes = (size_t)(after * sizeof(CycleCompiledProgram)))
                     : EMBED_CALL(memor.move_up, MemoriaCfg, .dst = program, .src = program + 1,
                                  .bytes = (size_t)(after * sizeof(CycleCompiledProgram))));
                (void)platform_array_counted(&s_cycle_programs, s_cycle_programs.count - 1ULL);
            }
            break;
        }
    }
}

// the target the code generator writes a lane for: the target's hash, its device, the NVRTC loaded, and the prelude
static TargetInfo cycle_target_info(const CycleTarget *lane_target)
{
    return TargetInfo{lane_target->hash,      lane_target->major,     lane_target->minor,
                      g_cycle_compiler.major, g_cycle_compiler.minor, g_cycle_prelude};
}

// Rule (i): a program stored as PTX routed between its two rulesets by its local frame against the device's stack
// limit. A frame past the limit has the runtime grow the stack for every resident thread at a run's first launch, and
// cycle_stack_return gives it back once the run is done: 7.449 to 10.130 ms a run on the record tests, against 0.127 to
// 1.630 ms for frames within the limit (engine_table item 11(f)). Past the limit the program is built as
// C source as well, and the smaller frame runs, the C source's wherever it fits and the PTX's does not; the
// other's reference is given back. Where the C source does not build, or its frame cannot be read, the PTX runs.
// CYCLE_RECORD_RETAIN_PTX=1 turns the rule off: a program stored as PTX runs as PTX
static void cycle_record_route(const EngineRecordLayout *layout, CycleRecord *record, const CycleTarget *lane_target,
                               int lto, int report)
{
    cudaFuncAttributes attributes;
    size_t limit = 0u;
    if ((cudaDeviceGetLimit(&limit, cudaLimitStackSize) != cudaSuccess) ||
        (cudaFuncGetAttributes(&attributes, (const void *)record->kernel) != cudaSuccess) ||
        (attributes.localSizeBytes <= limit))
    {
        // an error the runtime still retains is the attempt's own, dropped before a run reads it as its own
        cudaGetLastError();
        return;
    }
    const cudaKernel_t ptx = record->kernel;
    const unsigned int ptx_places = record->places;
    const size_t ptx_frame = attributes.localSizeBytes;
    CodeGenerator &generator = code_generator("c.krs");
    const Ruleset *const rules = generator.ruleset(report);
    const unsigned long long began = platform_clock_ns();
    const TargetInfo target = cycle_target_info(lane_target);
    unsigned int source_places = 0u;
    unsigned int source_live = 0u;
    PlatformText source;
    platform_text_begun(&source, CYCLE_LANE_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    const int wrote =
        (rules != NULL) && generator.program(layout, &target, target.prelude, &source, &source_places, &source_live);
    // the monotonic clock never runs back
    const unsigned long long written = platform_clock_ns() - began;
    const int built = wrote && cycle_program_load(layout, record, lane_target, platform_text_bytes(&source),
                                                  platform_text_size(&source), 0, lto, written, report);
    platform_text_ended(&source);
    const int read = built && (cudaFuncGetAttributes(&attributes, (const void *)record->kernel) == cudaSuccess);
    const size_t source_frame = (read != 0) ? attributes.localSizeBytes : 0u;
    const int source_runs = (read != 0) && (source_frame < ptx_frame);
    if (source_runs != 0)
    {
        cycle_program_release(ptx);
        record->places = source_places;
    }
    else
    {
        if (built != 0)
        {
            cycle_program_release(record->kernel);
        }
        record->kernel = ptx;
        record->places = ptx_places;
    }
    cudaGetLastError();
    if (report != 0)
    {
        char character[64];
        {
            VerbaLinea buffer_line = mmgr_line_over(character, sizeof(character));
            mmgr_line_put(&buffer_line, "a ");
            mmgr_line_decimal(&buffer_line, source_frame, 1u);
            mmgr_line_put(&buffer_line, "-byte frame");
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps as PTX uses a ");
            mmgr_line_decimal(&engine_note.line, ptx_frame, 1u);
            mmgr_line_put(&engine_note.line, "-byte local frame, past the ");
            mmgr_line_decimal(&engine_note.line, limit, 1u);
            mmgr_line_put(&engine_note.line, "-byte stack limit; as C source, ");
            mmgr_line_put(&engine_note.line, (read != 0) ? character : "not built");
            mmgr_line_put(&engine_note.line, ": it runs as ");
            mmgr_line_put(&engine_note.line, (source_runs != 0) ? "C" : "PTX");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
    }
}

// How deep the device's writing of lanes is in this thread. 0 where none is under way. 1 while the device writes a
// lane: the assembly printer's own program is compiled then, and the device writes its lane too. 2 while the device
// writes the assembly printer's own lane: the assembly printer then runs on the interpreter, the one program not
// compiled, and the bootstrap closes there
static thread_local int s_cycle_codegen_depth = 0;

// the depth at which the assembly printer writes its own lane
#define CYCLE_CODEGEN_OWN_LANE 2

// a text that the device wrote in this process and that the host's matched: its movable cell and its bytes
struct CycleWritten
{
    const void *cell;
    unsigned long long count;
};

// the texts that the device wrote in this process and that the host's matched, a CycleWritten each. The device writes
// each text once. The array begins at its first text
static PlatformArray s_cycle_codegen_written;

// 1 where the device wrote the `length` bytes at `text` before in this process
static int cycle_codegen_written_before(const char *text, unsigned long long length)
{
    for (unsigned long long at = 0ULL; at < s_cycle_codegen_written.count; at += 1ULL)
    {
        const CycleWritten *const written = (const CycleWritten *)platform_array_at(&s_cycle_codegen_written, at);
        if ((written->count == length) && platform_movable_same(written->cell, 0ULL, text, length))
        {
            return 1;
        }
    }
    return 0;
}

// a report on stderr of how the device read a ruleset: `head`, the ruleset's `name`, `how`, and `why` in brackets
// where it is not NULL
static void cycle_codegen_read_noted(const char *head, const char *name, const char *how, const char *why)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, head);
    mmgr_line_put(&engine_note.line, name);
    mmgr_line_put(&engine_note.line, how);
    if (why != NULL)
    {
        mmgr_line_put(&engine_note.line, " (");
        mmgr_line_put(&engine_note.line, why);
        mmgr_line_put(&engine_note.line, ")");
    }
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
}

// The lane as the device writes it from the step table (codegen_device.h), the path every lane takes: the ruleset's
// file read on the device and its written forms arranged once for the target and the header, the forms decided on the
// device a thread a step, arranged as the assembly printer's records there, written by the record machine a lane a byte
// and gathered. The host code generator's text is the check: the device's ruleset, its written forms and its text are
// checked against the host's word for word and byte for byte. Returns 1 where the device's text is built, and 0, with
// the step that differed on stderr, where the device did not write it or any of the three is apart from the host's.
// The device splits the lane where `generator`'s program() splits it. A text the device wrote before in this process
// is not written again
static int cycle_codegen_on_device(const CodeGenerator &generator, const Ruleset *rules, const TargetInfo *target,
                                   const char *header, const EngineRecordLayout *layout, unsigned int places,
                                   int report, PlatformText *text)
{
    const char *const host_text = platform_text_bytes(text);
    const unsigned long long host_length = platform_text_size(text);
    if (s_cycle_codegen_written.element == 0ULL)
    {
        platform_array_begun(&s_cycle_codegen_written, sizeof(CycleWritten), platform_place_g_program_owner, 0);
    }
    if (cycle_codegen_written_before(host_text, host_length) != 0)
    {
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: the device wrote a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps, ");
            mmgr_line_decimal(&engine_note.line, host_length, 1u);
            mmgr_line_put(&engine_note.line, " bytes, before in this process");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    AsmPrinterRuleset text_rules;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &text_rules, .bytes = sizeof(text_rules), .val = 0);
    char error_text[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(error_text, sizeof(error_text));
    PlatformText written;
    platform_text_begun(&written, CYCLE_LANE_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    ScheduleCosts costs;
    const int has_costs = generator.program_schedule_costs(&costs);
    s_cycle_codegen_depth += 1;
    const int built = asm_printer_ruleset_build(rules, target, header, &text_rules, &error);
    // the ruleset's file read on the device as well, and the device's built from where it is the host's as read
    Ruleset *const device_read =
        (Ruleset *)platform_cell_cleared(1ULL, sizeof(Ruleset), platform_place_g_program_owner, 0);
    if (device_read != NULL)
    {
        device_read->schema = rules->schema;
        platform_array_begun(&device_read->folds, sizeof(RulesetFold), platform_place_g_program_owner, 0);
        ruleset_released(device_read);
    }
    char read_error_text[RULESET_ERROR_BYTES];
    VerbaLinea read_error = mmgr_line_over(read_error_text, sizeof(read_error_text));
    const int read_ran = built && (device_read != NULL) && ruleset_read_device(device_read, rules->path, &read_error);
    (void)mmgr_line_finish(&read_error);
    const int read_same = read_ran && ruleset_same(device_read, rules);
    if (read_same != 0)
    {
        device_read->tried = rules->tried;
        device_read->ready = rules->ready;
        if (report != 0)
        {
            cycle_codegen_read_noted("  cycle: the device read the ruleset ", rules->name, " as the host did", NULL);
        }
    }
    else if (read_ran != 0)
    {
        cycle_codegen_read_noted("  cycle: the device read the ruleset ", rules->name, " apart from the host", NULL);
    }
    else if (built != 0)
    {
        cycle_codegen_read_noted("  cycle: the device did not read the ruleset ", rules->name, "",
                                 (device_read != NULL) ? read_error_text : "the pool cannot meet the ruleset");
    }
    // the ruleset built on the device as well, and the device's written from where it is the host's word for word
    AsmPrinterRuleset device_rules;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &device_rules, .bytes = sizeof(device_rules), .val = 0);
    char device_error_text[RULESET_ERROR_BYTES];
    VerbaLinea device_error = mmgr_line_over(device_error_text, sizeof(device_error_text));
    const int device_built = built && asm_printer_ruleset_device((read_same != 0) ? device_read : rules, target, header,
                                                                 &device_rules, &device_error);
    (void)mmgr_line_finish(&device_error);
    const int rules_same = device_built && asm_printer_ruleset_same(&device_rules, &text_rules);
    if ((rules_same != 0) && (report != 0))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line,
                      "  cycle: the device built the assembly printer's ruleset, word for word the host's");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if ((rules_same == 0) && (device_built != 0))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line,
                      "  cycle: the device built the assembly printer's ruleset apart from the host's");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if ((rules_same == 0) && (built != 0))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: the device did not build the assembly printer's ruleset (");
        mmgr_line_put(&engine_note.line, device_error_text);
        mmgr_line_put(&engine_note.line, ")");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    // the lane is written only from the device's own ruleset, read and built where it is the host's
    if ((built != 0) && ((read_same == 0) || (rules_same == 0)))
    {
        mmgr_line_put(&error, "its ruleset on the device is not the host's");
    }
    const int ran = (read_same != 0) && (rules_same != 0) &&
                    codegen_device(layout, &device_rules, places, has_costs ? &costs : NULL, &written, &error);
    (void)mmgr_line_finish(&error);
    if (built)
    {
        asm_printer_ruleset_release(&text_rules);
    }
    if (device_built)
    {
        asm_printer_ruleset_release(&device_rules);
    }
    if (device_read != NULL)
    {
        ruleset_released(device_read);
    }
    platform_cell_released(device_read, platform_place_g_program_owner);
    s_cycle_codegen_depth -= 1;
    const char *const device_text = platform_text_bytes(&written);
    const unsigned long long device_length = platform_text_size(&written);
    unsigned long long differs = 0ULL;
    while ((differs < device_length) && (differs < host_length) && (device_text[differs] == host_text[differs]))
    {
        differs += 1ULL;
    }
    const int same = (ran != 0) && (device_length == host_length) && (differs == host_length);
    if ((same != 0) && (report != 0))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: the device wrote a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps, ");
        mmgr_line_decimal(&engine_note.line, device_length, 1u);
        mmgr_line_put(&engine_note.line, " bytes, byte for byte the host's");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if ((same == 0) && (ran != 0))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: the device wrote a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps as ");
        mmgr_line_decimal(&engine_note.line, device_length, 1u);
        mmgr_line_put(&engine_note.line, " bytes against the host's ");
        mmgr_line_decimal(&engine_note.line, host_length, 1u);
        mmgr_line_put(&engine_note.line, ", apart from byte ");
        mmgr_line_decimal(&engine_note.line, differs, 1u);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if (same == 0)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: the device did not write a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps (");
        mmgr_line_put(&engine_note.line, error_text);
        mmgr_line_put(&engine_note.line, ")");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    if (same != 0)
    {
        // the device's text is the host's byte for byte, and the host's stays where it is
        const CycleWritten kept = {cycle_kept(device_text, device_length), device_length};
        if ((kept.cell != NULL) && !platform_array_appended(&s_cycle_codegen_written, &kept))
        {
            platform_movable_released(kept.cell, platform_place_g_program_owner);
        }
    }
    platform_text_ended(&written);
    return same;
}

// the program's lane written as PTX and built, else its C source compiled by NVRTC and built, found in this process or
// the cache where either was built before, and the places it occupies in shared memory set. PTX is not written where
// the block is LTO-IR, CYCLE_RECORD_NVRTC=1 or CYCLE_RECORD_HOST_C=1, and a program stored as PTX is routed by rule
// (i). Under CYCLE_RECORD_HOST_C=1 the C source is built by the host's compiler in place of NVRTC. 0 where it stays on
// the interpreter: no NVRTC or nvJitLink, a step neither supports, a C source whose ruleset c.krs errored, a compile,
// link or load that failed, or the assembly printer run while it writes its own lane. Every lane is written by the
// device (cycle_codegen_on_device) but the assembly printer's own: CYCLE_ERROR, with ENGINE_ERROR_LOGIC in `error`,
// where the device's lane is not the host code generator's
int cycle_record_compile(const EngineRecordLayout *layout, CycleRecord *record, EngineError *error)
{
    const int report = cycle_environment_set("CYCLE_RECORD_REPORT");
    const int lto = cycle_environment_set("CYCLE_RECORD_LTO");
    if (s_cycle_codegen_depth >= CYCLE_CODEGEN_OWN_LANE)
    {
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: the assembly printer's program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps runs on the interpreter to write its own lane");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    int device = 0;
    int major = 0;
    int minor = 0;
    if (!cycle_compiler_ready() || !cycle_linker_ready() || (cudaGetDevice(&device) != cudaSuccess) ||
        (cudaDeviceGetAttribute(&major, cudaDevAttrComputeCapabilityMajor, device) != cudaSuccess) ||
        (cudaDeviceGetAttribute(&minor, cudaDevAttrComputeCapabilityMinor, device) != cudaSuccess))
    {
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line,
                          " steps runs on the interpreter (NVRTC, nvJitLink or the device could not be read)");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    const CycleTarget *const lane_target = cycle_target(major, minor, lto, report);
    const TargetInfo target = cycle_target_info(lane_target);
    const int host = cycle_environment_set("CYCLE_RECORD_HOST_C");
    if ((lto == 0) && (host == 0) && (cycle_environment_set("CYCLE_RECORD_NVRTC") == 0))
    {
        CodeGenerator &generator = code_generator("ptx.krs");
        const Ruleset *const rules = generator.ruleset(report);
        const char *const header = cycle_ptx_header(major, minor, report);
        unsigned int places = 0u;
        unsigned int live = 0u;
        const unsigned long long began = platform_clock_ns();
        PlatformText ptx;
        platform_text_begun(&ptx, CYCLE_LANE_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
        const int wrote =
            (rules != NULL) && (header[0] != '\0') && generator.program(layout, &target, header, &ptx, &places, &live);
        // the monotonic clock never runs back
        const unsigned long long written = platform_clock_ns() - began;
        if (wrote &&
            !CYCLE_CHECK(cycle_codegen_on_device(generator, rules, &target, header, layout, places, report, &ptx),
                         layout, error, ENGINE_ERROR_LOGIC))
        {
            platform_text_ended(&ptx);
            return (int)CYCLE_ERROR;
        }
        if ((report != 0) && wrote)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps as PTX uses at max ");
            mmgr_line_decimal(&engine_note.line, live, 1u);
            mmgr_line_put(&engine_note.line, " words live a lane");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        const int loaded = wrote && cycle_program_load(layout, record, lane_target, platform_text_bytes(&ptx),
                                                       platform_text_size(&ptx), 1, lto, written, report);
        platform_text_ended(&ptx);
        if (loaded)
        {
            record->places = places;
            if (cycle_environment_set("CYCLE_RECORD_RETAIN_PTX") == 0)
            {
                cycle_record_route(layout, record, lane_target, lto, report);
            }
            return 1;
        }
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps goes to NVRTC (");
            mmgr_line_put(&engine_note.line,
                          (rules == NULL)       ? "its ruleset, ptx.krs, errored"
                          : (header[0] == '\0') ? "PTX's header could not be read"
                          : !wrote ? "a step the lane does not support, or a form given other arguments than it takes"
                                   : "its PTX did not build");
            mmgr_line_put(&engine_note.line, ")");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
    }
    CodeGenerator &source_generator = code_generator("c.krs");
    const Ruleset *const source_rules = source_generator.ruleset(report);
    const unsigned long long began = platform_clock_ns();
    unsigned int source_places = 0u;
    unsigned int source_live = 0u;
    PlatformText source;
    platform_text_begun(&source, CYCLE_LANE_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    const int wrote = (source_rules != NULL) &&
                      source_generator.program(layout, &target, target.prelude, &source, &source_places, &source_live);
    // the monotonic clock never runs back
    const unsigned long long written = platform_clock_ns() - began;
    if (wrote && !CYCLE_CHECK(cycle_codegen_on_device(source_generator, source_rules, &target, target.prelude, layout,
                                                      source_places, report, &source),
                              layout, error, ENGINE_ERROR_LOGIC))
    {
        platform_text_ended(&source);
        return (int)CYCLE_ERROR;
    }
    if (!wrote)
    {
        platform_text_ended(&source);
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps runs on the interpreter (");
            mmgr_line_put(&engine_note.line,
                          (source_rules == NULL)
                              ? "its C ruleset, c.krs, errored"
                              : "a step it does not support, or a form given other arguments than it takes");
            mmgr_line_put(&engine_note.line, ")");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    const char *const source_text = platform_text_bytes(&source);
    const int loaded = (host != 0)
                           ? cycle_host_program_load(layout, record, source_text, source_places, written, report)
                           : cycle_program_load(layout, record, lane_target, source_text, platform_text_size(&source),
                                                0, lto, written, report);
    platform_text_ended(&source);
    if (loaded == 0)
    {
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps runs on the interpreter");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    record->places = source_places;
    return 1;
}
