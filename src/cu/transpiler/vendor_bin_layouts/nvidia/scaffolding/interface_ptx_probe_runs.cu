// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_ptx_probe_runs.cu: the probe's runs: PTX's header and the kernel, the cubins assembled, built, written
// and run, and the membership, single and clocks probes
#include "interface_ptx_probe_internal.h"

// PTX's header for the device into `header`, `room` bytes of it, requested of NVRTC by compiling an empty kernel to
// PTX: the .version, .target and .address_size lines. The answer is its length, 0 where NVRTC does not answer
size_t probe_header(int major, int minor, char *header, size_t room)
{
    const char question[] = "extern \"C\" __global__ void interface_header(void)\n{\n}\n";
    VerbaLinea lines = mmgr_line_over(header, room);
    nvrtcProgram program = NULL;
    if (nvrtcCreateProgram(&program, question, "interface_header.cu", 0, NULL, NULL) != NVRTC_SUCCESS)
    {
        return mmgr_line_finish(&lines);
    }
    char architecture[48];
    {
        VerbaLinea buffer_line = mmgr_line_over(architecture, sizeof(architecture));
        mmgr_line_put(&buffer_line, "--gpu-architecture=compute_");
        mmgr_line_decimal_signed(&buffer_line, major);
        mmgr_line_decimal_signed(&buffer_line, minor);
        (void)mmgr_line_finish(&buffer_line);
    }
    const char *const options[] = {architecture};
    size_t size = 0u;
    if ((nvrtcCompileProgram(program, 1, options) == NVRTC_SUCCESS) &&
        (nvrtcGetPTXSize(program, &size) == NVRTC_SUCCESS) && (size > 1u))
    {
        // the PTX is ephemeral work, given back before the call returns
        char *const ptx = (char *)platform_cell_taken(size, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if ((ptx != NULL) && (nvrtcGetPTX(program, ptx) == NVRTC_SUCCESS))
        {
            ptx[size - 1u] = '\0';
            const char *const wanted[] = {".version ", ".target ", ".address_size "};
            for (unsigned int at = 0u; at < 3u; at += 1u)
            {
                const char *const found = mmgr_text_find(ptx, wanted[at], PLATFORM_TEXT_TO_TERMINATOR);
                const char *const end =
                    (found != NULL) ? mmgr_text_char(found, '\n', PLATFORM_TEXT_TO_TERMINATOR) : NULL;
                if (end != NULL)
                {
                    // a line ends past its start
                    (void)mmgr_line_put_n(&lines, found, (size_t)(end - found));
                    mmgr_line_ch(&lines, '\n');
                }
            }
        }
        platform_cell_released(ptx, platform_place_g_program_owner);
    }
    nvrtcDestroyProgram(&program);
    const size_t length = mmgr_line_finish(&lines);
    return mmgr_line_ok(&lines) ? length : 0u;
}

// `value` in decimal into `text`, a buffer of 24 bytes
static const char *probe_decimal(unsigned long long value, char *text)
{
    VerbaLinea line = mmgr_line_over(text, 24u);
    mmgr_line_decimal(&line, value, 1u);
    (void)mmgr_line_finish(&line);
    return text;
}

// The kernel onto `text`: the header, then an entry of this probe's own that finds its case, loads its eight words
// into temporaries 0 to 7 through the ruleset's global_load_constant_word, sets outputs 8 to 11 to 0, runs the body,
// and stores the outputs through the ruleset's record_store_word, the record register set to the case's output
static void probe_kernel(ProbeWriter *writer, const char *header, const char *body, PlatformText *text)
{
    VerbaLinea *const line = &text->line;
    mmgr_line_put(line, header);
    mmgr_line_put(line, "\n.visible .entry interface_probe(\n\t.param .u64 interface_probe_in,\n\t.param .u64 "
                        "interface_probe_out,\n\t.param .u32 "
                        "interface_probe_count\n)\n{\n");
    mmgr_line_put(
        line,
        "\t.reg .pred \t%interface_past;\n\t.reg .b32 \t%interface_word<4>;\n\t.reg .b64 \t%interface_wide<4>;\n");
    char declared[24];
    (void)probe_decimal(PROBE_DECLARED, declared);
    PROBE_FORM(writer, line, "declare_predicates", declared);
    probe_form(writer, line, "declare_fixed_predicates", NULL, 0u);
    PROBE_FORM(writer, line, "declare_temporaries", declared);
    PROBE_FORM(writer, line, "declare_wides", declared);
    probe_form(writer, line, "declare_fixed_words", NULL, 0u);
    probe_form(writer, line, "declare_fixed_wides", NULL, 0u);
    const char *const zero = ruleset_fixed_named(writer->rules, "zero");
    const char *const record = ruleset_fixed_named(writer->rules, "record");
    writer->broken = writer->broken || (zero == NULL) || (record == NULL);
    PROBE_FORM(writer, line, "word_set", (zero != NULL) ? zero : "", "0");
    mmgr_line_put(line, "\tld.param.u64 \t%interface_wide0, [interface_probe_in];\n\tld.param.u64 \t%interface_wide1, "
                        "[interface_probe_out];\n");
    mmgr_line_put(
        line, "\tld.param.u32 \t%interface_word0, [interface_probe_count];\n\tmov.u32 \t%interface_word1, %ctaid.x;\n");
    mmgr_line_put(line, "\tmov.u32 \t%interface_word2, %ntid.x;\n\tmov.u32 \t%interface_word3, %tid.x;\n");
    mmgr_line_put(line, "\tmad.lo.u32 \t%interface_word1, %interface_word1, %interface_word2, %interface_word3;\n");
    mmgr_line_put(line, "\tsetp.ge.u32 \t%interface_past, %interface_word1, %interface_word0;\n\t@%interface_past bra "
                        "\t$Linterface_done;\n");
    mmgr_line_put(line, "\tcvta.to.global.u64 \t%interface_wide0, %interface_wide0;\n\tcvta.to.global.u64 "
                        "\t%interface_wide1, %interface_wide1;\n");
    mmgr_line_put(line, "\tmul.wide.u32 \t%interface_wide2, %interface_word1, 32;\n\tadd.s64 \t%interface_wide2, "
                        "%interface_wide0, %interface_wide2;\n");
    mmgr_line_put(line, "\tmul.wide.u32 \t%interface_wide3, %interface_word1, 16;\n\tadd.s64 \t%interface_wide3, "
                        "%interface_wide1, %interface_wide3;\n");
    for (unsigned int word = 0u; word < PROBE_IN_WORDS; word += 1u)
    {
        const ProbeName in = probe_temporary(writer, word);
        char offset[24];
        PROBE_FORM(writer, line, "global_load_constant_word", in.text, "%interface_wide2",
                   probe_decimal(word * 4u, offset));
    }
    for (unsigned int word = 0u; word < PROBE_OUT_WORDS; word += 1u)
    {
        const ProbeName out = probe_temporary(writer, 8u + word);
        PROBE_FORM(writer, line, "word_set", out.text, "0");
    }
    mmgr_line_put(line, body);
    mmgr_line_put(line, "\tmov.b64 \t");
    mmgr_line_put(line, (record != NULL) ? record : "");
    mmgr_line_put(line, ", %interface_wide3;\n");
    for (unsigned int word = 0u; word < PROBE_OUT_WORDS; word += 1u)
    {
        const ProbeName out = probe_temporary(writer, 8u + word);
        char offset[24];
        PROBE_FORM(writer, line, "record_store_word", probe_decimal(word * 4u, offset), out.text);
    }
    mmgr_line_put(line, "$Linterface_done:\n");
    probe_form(writer, line, "return", NULL, 0u);
    mmgr_line_put(line, "}\n");
}

// a kernel's text begun, empty: a movable cell, which a text of any size grows in with no hole left behind
static void probe_text_begun(PlatformText *text)
{
    platform_text_begun(text, PROBE_KERNEL_BYTES, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
}

// A text's bytes and their count into `size`, NULL where the pool cannot meet the copy that a movable text reads into
// [BORROWS]
static const char *probe_text(PlatformText *text, unsigned long long *size)
{
    *size = platform_text_size(text);
    return platform_text_bytes(text);
}

// The kernel's text assembled by nvJitLink for the device: 1 with the cubin, `*size` bytes of it, in a cell of the
// temporary tier that the caller gives back before its work ends. 0 with the log printed
static int probe_assemble(PlatformText *text, int major, int minor, char **cubin, size_t *size)
{
    *cubin = NULL;
    *size = 0u;
    char architecture[32];
    {
        VerbaLinea buffer_line = mmgr_line_over(architecture, sizeof(architecture));
        mmgr_line_put(&buffer_line, "-arch=sm_");
        mmgr_line_decimal_signed(&buffer_line, major);
        mmgr_line_decimal_signed(&buffer_line, minor);
        (void)mmgr_line_finish(&buffer_line);
    }
    const char *options[] = {architecture};
    nvJitLinkHandle handle = NULL;
    if (nvJitLinkCreate(&handle, 1u, options) != NVJITLINK_SUCCESS)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "errored: nvJitLink could not be made");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    unsigned long long length = 0ULL;
    const char *const bytes = probe_text(text, &length);
    size_t linked_size = 0u;
    const int linked = (bytes != NULL) &&
                       (nvJitLinkAddData(handle, NVJITLINK_INPUT_PTX, bytes, (size_t)length + 1u, "interface_probe") ==
                        NVJITLINK_SUCCESS) &&
                       (nvJitLinkComplete(handle) == NVJITLINK_SUCCESS) &&
                       (nvJitLinkGetLinkedCubinSize(handle, &linked_size) == NVJITLINK_SUCCESS) && (linked_size != 0u);
    char *const taken_cell =
        linked ? (char *)platform_cell_taken(linked_size, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
               : NULL;
    const int taken = (taken_cell != NULL) && (nvJitLinkGetLinkedCubin(handle, taken_cell) == NVJITLINK_SUCCESS);
    if (!taken)
    {
        platform_cell_released(taken_cell, platform_place_g_program_owner);
        size_t log_size = 0u;
        char *log = NULL;
        if ((nvJitLinkGetErrorLogSize(handle, &log_size) == NVJITLINK_SUCCESS) && (log_size > 1u))
        {
            log = (char *)platform_cell_taken(log_size, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        }
        if (log != NULL)
        {
            nvJitLinkGetErrorLog(handle, log);
            log[log_size - 1u] = '\0';
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "errored: nvJitLink did not assemble the kernel\n");
            mmgr_line_put(output_line, (log != NULL) ? log : "");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_cell_released(log, platform_place_g_program_owner);
    }
    else
    {
        *cubin = taken_cell;
        *size = linked_size;
    }
    nvJitLinkDestroy(&handle);
    return taken;
}

// the kernel's text assembled and loaded: 1 and the kernel, or 0 with the log printed
static int probe_build(PlatformText *text, int major, int minor, cudaLibrary_t *library, cudaKernel_t *kernel)
{
    char *cubin = NULL;
    size_t size = 0u;
    const int built = probe_assemble(text, major, minor, &cubin, &size) &&
                      (cudaLibraryLoadData(library, cubin, NULL, NULL, 0u, NULL, NULL, 0u) == cudaSuccess) &&
                      (cudaLibraryGetKernel(kernel, *library, "interface_probe") == cudaSuccess);
    platform_cell_released(cubin, platform_place_g_program_owner);
    return built;
}

// the kernel's text assembled and written to `path`: 1, or 0 with the reason printed
static int probe_write(PlatformText *text, int major, int minor, const char *path)
{
    char *cubin = NULL;
    size_t size = 0u;
    if (!probe_assemble(text, major, minor, &cubin, &size))
    {
        return 0;
    }
    const int written = platform_file_written(path, cubin, size);
    platform_cell_released(cubin, platform_place_g_program_owner);
    if (!written)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "errored: ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, " could not be written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return written;
}

// The machine code of each membership question for the SASS probe (interface_sass_probe.c): the kernel with no body
// assembled into `folder`/kernel.cubin and each question's kernel into `folder`/form_<number>.cubin, a line
// "cubin <number> <name>" printed for each. Exit 0 where every cubin was written, 2 where one was not.
//
// The program resident is its own module: an empty lane for the resident's call to reach, then program_unit with the
// launch's own layout in its 25 parameters. No question covers this part of a program, and no hand writes it twice.
// The resident is 110 instructions of PTX that already runs, and the part's own compiler turns it into SASS that
// already runs. The SASS probe requests this of every other form, and what comes back is the floor a rearrangement has
// to beat
static void probe_resident(ProbeWriter *writer, const char *header, PlatformText *text)
{
    VerbaLinea *const line = &text->line;
    mmgr_line_put(line, header);
    probe_form(writer, line, "lane_open", NULL, 0u);
    probe_form(writer, line, "return", NULL, 0u);
    probe_form(writer, line, "lane_close", NULL, 0u);
    char values[PROGRAM_UNIT_PARAMETERS][24];
    const char *arguments[PROGRAM_UNIT_PARAMETERS];
    for (unsigned int at = 0u; at < PROGRAM_UNIT_PARAMETERS; at += 1u)
    {
        arguments[at] = probe_decimal(codegen_unit(at), values[at]);
    }
    probe_form(writer, line, "program_unit", arguments, PROGRAM_UNIT_PARAMETERS);
}

// `folder`/`name` into `path`
static const char *probe_path(const char *folder, const char *name, unsigned long long number, char *path)
{
    VerbaLinea line = mmgr_line_over(path, PLATFORM_PATH_BYTES);
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (number != ~0ULL)
    {
        mmgr_line_decimal(&line, number, 1u);
        mmgr_line_put(&line, ".cubin");
    }
    (void)mmgr_line_finish(&line);
    return path;
}

// a line of stdout: `name`, then `tail`
static void probe_noted(const char *name, const char *tail)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, name);
    mmgr_line_put(output_line, tail);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

int probe_cubins(ProbeWriter *writer, const char *header, int major, int minor, const char *folder)
{
    unsigned int count = 0u;
    const ProbeQuestion *const questions = probe_questions(writer, &count);
    char path[PLATFORM_PATH_BYTES];
    PlatformText text;
    probe_text_begun(&text);
    probe_kernel(writer, header, "", &text);
    const int kernel =
        !writer->broken && probe_write(&text, major, minor, probe_path(folder, "kernel.cubin", ~0ULL, path));
    platform_text_ended(&text);
    if (!kernel)
    {
        probe_noted("kernel", ": not written");
        return 2;
    }
    probe_text_begun(&text);
    probe_resident(writer, header, &text);
    const int resident =
        !writer->broken && probe_write(&text, major, minor, probe_path(folder, "resident.cubin", ~0ULL, path));
    platform_text_ended(&text);
    if (!resident)
    {
        probe_noted("resident", ": not written");
        return 2;
    }
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        probe_text_begun(&text);
        probe_kernel(writer, header, questions[number].body, &text);
        const int written =
            !writer->broken && probe_write(&text, major, minor, probe_path(folder, "form_", number, path));
        platform_text_ended(&text);
        if (!written)
        {
            probe_noted(questions[number].name, ": not written");
            return 2;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "cubin ");
            mmgr_line_decimal(output_line, number, 1u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, questions[number].name);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "cubins: ");
        mmgr_line_decimal(output_line, count, 1u);
        mmgr_line_put(output_line, " questions");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}

// the kernel run over `count` cases of `in`, its outputs into `out`; the CUDA error the run gave
static cudaError_t probe_run(cudaKernel_t kernel, const unsigned int *in, unsigned int *out, unsigned int count)
{
    unsigned int *device_in = NULL;
    unsigned int *device_out = NULL;
    const size_t in_bytes = (size_t)count * PROBE_IN_WORDS * sizeof(unsigned int);
    const size_t out_bytes = (size_t)count * PROBE_OUT_WORDS * sizeof(unsigned int);
    cudaError_t status = cudaMalloc((void **)&device_in, in_bytes);
    status = (status == cudaSuccess) ? cudaMalloc((void **)&device_out, out_bytes) : status;
    status = (status == cudaSuccess) ? cudaMemcpy(device_in, in, in_bytes, cudaMemcpyHostToDevice) : status;
    void *arguments[] = {(void *)&device_in, (void *)&device_out, (void *)&count};
    const unsigned int blocks = (count + PROBE_THREADS - 1u) / PROBE_THREADS;
    status = (status == cudaSuccess)
                 ? cudaLaunchKernel((const void *)kernel, dim3(blocks), dim3(PROBE_THREADS), arguments, 0u, NULL)
                 : status;
    status = (status == cudaSuccess) ? cudaDeviceSynchronize() : status;
    status = (status == cudaSuccess) ? cudaMemcpy(out, device_out, out_bytes, cudaMemcpyDeviceToHost) : status;
    cudaFree(device_in);
    cudaFree(device_out);
    return status;
}

// the error a question the device errored gave, then the error the next allocation gives: whether the context
// survived the error
static int probe_error(cudaError_t status)
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "error ");
        mmgr_line_decimal_signed(output_line, (int)status);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, cudaGetErrorName(status));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    void *after = NULL;
    const cudaError_t next = cudaMalloc(&after, 4u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "after ");
        mmgr_line_decimal_signed(output_line, (int)next);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, cudaGetErrorName(next));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 3;
}

// The cases' input and output words of the membership probe. The calls of this module share them, and the probe fills
// them once a process
static unsigned int s_probe_in[PROBE_CASES * PROBE_IN_WORDS];
static unsigned int s_probe_out[PROBE_CASES * PROBE_OUT_WORDS];

// the words of a case in hex, after `label`
static void probe_words_put(VerbaLinea *line, const char *label, const unsigned int *words, unsigned int count)
{
    mmgr_line_put(line, label);
    for (unsigned int word = 0u; word < count; word += 1u)
    {
        mmgr_line_put(line, (word == 0u) ? "" : " ");
        mmgr_line_hex(line, words[word], 8u);
    }
}

// One question's kernel run over every case, and its answers checked against the host's rule. The answer is 1 where a
// defined case differs, 0 where none does, and -1 where the kernel did not build or the device errored, with the
// probe's exit code in `code`
static int probe_question_checked(ProbeWriter *writer, const char *header, const ProbeQuestion *question, int major,
                                  int minor, int *code)
{
    PlatformText text;
    probe_text_begun(&text);
    probe_kernel(writer, header, question->body, &text);
    cudaLibrary_t library = NULL;
    cudaKernel_t kernel = NULL;
    const int built = !writer->broken && probe_build(&text, major, minor, &library, &kernel);
    platform_text_ended(&text);
    if (!built)
    {
        probe_noted(question->name, ": not built");
        *code = 2;
        return -1;
    }
    const cudaError_t status = probe_run(kernel, s_probe_in, s_probe_out, PROBE_CASES);
    cudaLibraryUnload(library);
    if (status != cudaSuccess)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, question->name);
            mmgr_line_put(output_line, ": ");
            platform_line_out_ended();
        }
        *code = probe_error(status);
        return -1;
    }
    unsigned int agree = 0u;
    unsigned int differ = 0u;
    unsigned int undefined = 0u;
    unsigned int seen[4];
    unsigned int seen_count = 0u;
    for (unsigned int number = 0u; number < PROBE_CASES; number += 1u)
    {
        const unsigned int *const case_in = &s_probe_in[(size_t)number * PROBE_IN_WORDS];
        const unsigned int *const case_out = &s_probe_out[(size_t)number * PROBE_OUT_WORDS];
        unsigned int wanted[PROBE_OUT_WORDS] = {0u, 0u, 0u, 0u};
        if (!question->rule(case_in, wanted))
        {
            undefined += 1u;
            // the device's answer to a question the rule leaves undefined, each distinct word once, the first four
            for (unsigned int word = 0u; (word < question->outputs) && (seen_count < 4u); word += 1u)
            {
                int known = 0;
                for (unsigned int earlier = 0u; earlier < seen_count; earlier += 1u)
                {
                    known = known || (seen[earlier] == case_out[word]);
                }
                if (!known)
                {
                    seen[seen_count] = case_out[word];
                    seen_count += 1u;
                }
            }
            continue;
        }
        int agrees = 1;
        for (unsigned int word = 0u; word < question->outputs; word += 1u)
        {
            agrees = agrees && (case_out[word] == wanted[word]);
        }
        agree += agrees ? 1u : 0u;
        if (!agrees && (differ < 3u))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, question->name);
            mmgr_line_put(output_line, " differs on case ");
            mmgr_line_decimal(output_line, number, 1u);
            probe_words_put(output_line, ": in ", case_in, 6u);
            probe_words_put(output_line, ", device ", case_out, PROBE_OUT_WORDS);
            probe_words_put(output_line, ", host ", wanted, PROBE_OUT_WORDS);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        differ += agrees ? 0u : 1u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "form ");
        mmgr_line_put(output_line, question->name);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, agree + differ, 1u);
        mmgr_line_put(output_line, " cases, ");
        mmgr_line_decimal(output_line, agree, 1u);
        mmgr_line_put(output_line, " agree, ");
        mmgr_line_decimal(output_line, differ, 1u);
        mmgr_line_put(output_line, " differ");
        if (undefined != 0u)
        {
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, undefined, 1u);
            mmgr_line_put(output_line, " undefined, the device answering");
            for (unsigned int at = 0u; at < seen_count; at += 1u)
            {
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, seen[at], 8u);
            }
        }
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (differ != 0u) ? 1 : 0;
}

int probe_membership(ProbeWriter *writer, const char *header, int major, int minor)
{
    unsigned int count = 0u;
    const ProbeQuestion *const questions = probe_questions(writer, &count);
    if (writer->broken)
    {
        return 2;
    }
    for (unsigned int number = 0u; number < PROBE_CASES; number += 1u)
    {
        probe_case(number, &s_probe_in[(size_t)number * PROBE_IN_WORDS]);
    }
    unsigned int disagreed = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        int code = 0;
        const int checked = probe_question_checked(writer, header, &questions[at], major, minor, &code);
        if (checked < 0)
        {
            return code;
        }
        disagreed += (checked != 0) ? 1u : 0u;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "membership: ");
        mmgr_line_decimal(output_line, count, 1u);
        mmgr_line_put(output_line, " questions, ");
        mmgr_line_decimal(output_line, disagreed, 1u);
        mmgr_line_put(output_line, " with a case that differs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (disagreed == 0u) ? 0 : 1;
}

// the four output words of a run printed after "answered"
static void probe_answered(const unsigned int *out)
{
    VerbaLinea *const output_line = platform_line_out();
    probe_words_put(output_line, "answered ", out, PROBE_OUT_WORDS);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// a kernel whose body is `body` alone, run over one case; `address` names the case's input, which the body may
// replace. Exit 0 where the device answers, 3 where it errors on the run, 4 where the toolchain errors on the kernel
int probe_single(ProbeWriter *writer, const char *header, const char *body, int major, int minor)
{
    PlatformText text;
    probe_text_begun(&text);
    probe_kernel(writer, header, body, &text);
    cudaLibrary_t library = NULL;
    cudaKernel_t kernel = NULL;
    if (writer->broken)
    {
        platform_text_ended(&text);
        return 2;
    }
    const int built = probe_build(&text, major, minor, &library, &kernel);
    platform_text_ended(&text);
    if (!built)
    {
        return 4;
    }
    unsigned int in[PROBE_IN_WORDS] = {6u, 7u, 0u, 0u, 0u, 0u, 0u, 0u};
    unsigned int out[PROBE_OUT_WORDS] = {0u, 0u, 0u, 0u};
    const cudaError_t status = probe_run(kernel, in, out, 1u);
    if (status != cudaSuccess)
    {
        return probe_error(status);
    }
    probe_answered(out);
    cudaLibraryUnload(library);
    return 0;
}

// Every clock the part will name, queried of it instead of assumed. A part is a clocked thing and everything it does
// is transitions at some rate: what rates it has is a question it can answer, and the answer is the unit every
// cost is read in. The part's own clock register is not needed for that, and is not in the machine: the host's clock
// times a run from outside (probe_cubin_run), and these report what one tick is worth
int probe_clocks(int device)
{
    static const struct
    {
        cudaDeviceAttr attribute;
        const char *name;
        const char *unit;
    } clocks_queried[] = {
        {cudaDevAttrClockRate, "the part's clock", "kHz"},
        {cudaDevAttrMemoryClockRate, "the memory's clock", "kHz"},
        {cudaDevAttrGlobalMemoryBusWidth, "the memory's bus", "bits"},
        {cudaDevAttrMultiProcessorCount, "the part's multiprocessors", ""},
        {cudaDevAttrWarpSize, "a warp", "lanes"},
        {cudaDevAttrMaxThreadsPerMultiProcessor, "a multiprocessor's threads", ""},
        // what the register file contains against what a thread may take: their ratio is how many registers a thread
        // can use and still fill the part, and a lane past it loses threads in step. That is where a knee is
        {cudaDevAttrMaxRegistersPerMultiprocessor, "a multiprocessor's registers", ""},
        {cudaDevAttrMaxRegistersPerBlock, "a block's registers", ""},
        {cudaDevAttrL2CacheSize, "the second level cache", "bytes"},
    };
    for (unsigned int at = 0u; at < (sizeof(clocks_queried) / sizeof(clocks_queried[0])); at += 1u)
    {
        int value = 0;
        const cudaError_t got = cudaDeviceGetAttribute(&value, clocks_queried[at].attribute, device);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, clocks_queried[at].name, 28u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, (got == cudaSuccess) ? "" : "the part does not report\n");
            platform_line_out_ended();
        }
        if (got == cudaSuccess)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_decimal_signed(output_line, value);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, clocks_queried[at].unit);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    return 0;
}

// The cubin at `path` loaded and its kernel interface_probe run over one case, whose input words are `arguments`: the
// four output words printed. Exit 0 where the device answers, 3 where it errors on the run, 4 where it will not load
int probe_cubin_run(const char *path, int count, char **arguments)
{
    unsigned long long size = 0ULL;
    // the cubin is ephemeral work, given back before the call returns
    char *const cubin = ((platform_file_size(path, &size) != 0) && (size != 0ULL))
                            ? (char *)platform_cell_taken(size, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
                            : NULL;
    unsigned long long read = 0ULL;
    const int whole = (cubin != NULL) && platform_file_read(path, cubin, size, &read) && (read == size);
    if (whole == 0)
    {
        platform_cell_released(cubin, platform_place_g_program_owner);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "the cubin at ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " was not read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 4;
    }
    cudaLibrary_t library = NULL;
    cudaKernel_t kernel = NULL;
    const cudaError_t loaded = cudaLibraryLoadData(&library, cubin, NULL, NULL, 0u, NULL, NULL, 0u);
    platform_cell_released(cubin, platform_place_g_program_owner);
    if ((loaded != cudaSuccess) || (cudaLibraryGetKernel(&kernel, library, "interface_probe") != cudaSuccess))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "the cubin at ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " did not load (");
            mmgr_line_put(output_line, cudaGetErrorName(loaded));
            mmgr_line_put(output_line, ")");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 4;
    }
    unsigned int in[PROBE_IN_WORDS] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    for (int word = 0; (word < (count - 3)) && (word < (int)PROBE_IN_WORDS); word += 1)
    {
        // a word the caller gives in hex, which fits an unsigned int
        in[word] = (unsigned int)mmgr_text_unsigned_in(arguments[3 + word], NULL, 16);
    }
    unsigned int out[PROBE_OUT_WORDS] = {0u, 0u, 0u, 0u};
    const cudaError_t status = probe_run(kernel, in, out, 1u);
    if (status != cudaSuccess)
    {
        return probe_error(status);
    }
    probe_answered(out);
    // The run timed by our own clock, which is all a cost needs: the part's clock register is not in the machine,
    // and a run measured from outside shows what a kernel costs whatever the part will name. The run above is not
    // counted, since it carries the library load and the first launch
    char repeats_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const repeats =
        (platform_environment("PROBE_REPEATS", repeats_text, sizeof(repeats_text)) != 0ULL) ? repeats_text : NULL;
    const unsigned long runs = (repeats != NULL) ? mmgr_text_unsigned_in(repeats, NULL, 10) : 0UL;
    if (runs != 0UL)
    {
        const unsigned long long opened = platform_clock_ns();
        for (unsigned long run = 0UL; run < runs; run += 1UL)
        {
            probe_run(kernel, in, out, 1u);
        }
        const double taken = (double)(platform_clock_ns() - opened);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "timed ");
            mmgr_line_decimal(output_line, runs, 1u);
            mmgr_line_put(output_line, " runs, ");
            mmgr_line_fixed(output_line, (double)(taken / (double)runs), 1u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " ns a run");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    cudaLibraryUnload(library);
    return 0;
}
