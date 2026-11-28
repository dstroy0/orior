// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_run.c: a list of cubins run on the part in one process, one answer a cubin. Nothing is compiled here:
// the driver is passed each cubin as it was written and queried for its kernel.
//
//     interface_sass_run [--gated <file>] <machine file> <list> <first> <answers> [<cases> [<max>]]
//
// It joins the safety airlock of the gate, as the carrier does. --gated reads the verdict of the vendor
// (cubin_safe_verdict_*). A line reaches the part only with a verdict in hand, and an encoding that the verdict blocks
// never reaches it. With no verdict, the program blocks a line from the part. Code that the reader of the vendor never
// read for a fault can be the code that crashes the host.
//
// The list holds one cubin to a line, as `<path> <kernel>`. From line `first` on, the program reads each cubin on the
// host first, and cubin_safe checks it against the machine file. The program never passes a cubin that breaks a rule
// to the driver, and adds `<number> skipped cubin_safe_<verdict>`. The program loads each other cubin and runs its
// kernel over the case of the probe. Then it adds a line `<number> answered <w0> <w1> <w2> <w3>` to the answers file.
//
// A cases file has one case to a line, as up to eight words in hex. With a cases file, the program runs the kernel
// once over every case, one thread to a case. The line then holds the first word of the answer of each case, in the
// order of the file. With `max`, a pass runs that many lines at max, and ends with exit 4 where lines remain. That
// makes a pass short enough for a cap on its time to bound one cubin.
//
// A cubin that the part refuses kills the context past recovery. A reset of the primary context of the part in place
// makes the next launch fail. A refusal adds `<number> refused <error>` and ends the process with exit 3, and the
// caller starts it again past that line. A pause after each launch spaces the work of the part. Without it, a run of
// launches can flood the part and take the display down with it.
//
// Exit 0 where every line ran, 2 where the program did not reach the list or the device.
#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../../includes/parsers/line_read.h"
#include "../../../../../../includes/platform/platform.h"
#include "../../../../engine/engine_note.h"
#include "../cubin_safe.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include <cuda.h>
#include <windows.h>

// the max bytes a cubin takes, and the longest line of the list
#define SASS_RUN_CUBIN 262144u
#define SASS_RUN_LINE 1024u
// the words of the case and of an answer, and the threads that a launch gives the kernel. Each thread past the case
// count leaves at once
#define SASS_RUN_IN_WORDS 8u
#define SASS_RUN_OUT_WORDS 4u
#define SASS_RUN_THREADS 256u
// the wait after a launch, in milliseconds. It spaces the work of the part, so that a run of launches does not flood
// it
#define SASS_RUN_PAUSE_MS 25u

static unsigned char s_image[SASS_RUN_CUBIN];
static SassMachine s_machine;
// the cases that the program runs every cubin over, and the answers that they give. Where the caller gives no cases
// file, this is the one case of the probe
static unsigned int s_cases[SASS_RUN_THREADS][SASS_RUN_IN_WORDS] = {{0xbu, 0x7u, 0x3u, 0x5u, 0x2u, 0x9u, 0x1u, 0x4u}};
static unsigned int s_case_count = 1u;
static unsigned int s_answers[SASS_RUN_THREADS][SASS_RUN_OUT_WORDS];

// the cases file at `path`, read into s_cases, one case to a line as up to eight words in hex. The words that a line
// leaves out are zero. The return is 1, or 0 where it does not read, holds no case, or holds more cases than a launch
// has threads
static int sass_run_cases(const char *path)
{
    // the cases as the run stores them, a descent's written through the run's writer, or the disk where it stores none
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    unsigned long long at = 0ULL;
    if (!qry_member_file_blob(path, &bytes, &size))
    {
        return 0;
    }
    char line[SASS_RUN_LINE];
    unsigned int count = 0u;
    int fits = 1;
    while (fits && line_read(bytes, size, &at, line, sizeof(line)))
    {
        unsigned int words[SASS_RUN_IN_WORDS] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        const char *at_word = line;
        unsigned int read = 0u;
        unsigned long long word = 0ULL;
        while ((read < SASS_RUN_IN_WORDS) && line_number(&at_word, 1, &word) && (word <= 0xffffffffULL))
        {
            // the test above bounds the word inside 32 bits
            words[read] = (unsigned int)word;
            read += 1u;
        }
        if (read == 0u)
        {
            continue;
        }
        fits = (count < SASS_RUN_THREADS);
        if (fits)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_cases[count], .src = words, .bytes = sizeof(words));
            count += 1u;
        }
    }
    s_case_count = count;
    return fits && (count != 0u);
}

// `path` read whole into s_image: the bytes read, or 0 where the read failed or the file does not fit
static size_t sass_run_image(const char *path)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0u;
    }
    const size_t read = platform_file_read_items(s_image, 1u, sizeof(s_image), file);
    platform_file_close(file);
    return (read < sizeof(s_image)) ? read : 0u;
}

// the cubin in s_image, loaded, with its kernel run over every case, one thread to a case. The four words of each
// case go into s_answers. The return is CUDA_SUCCESS, or the error that the driver gave
static CUresult sass_run_one(const char *kernel)
{
    CUmodule module = NULL;
    CUfunction function = NULL;
    CUdeviceptr in = 0u;
    CUdeviceptr out = 0u;
    unsigned int count = s_case_count;
    const size_t in_bytes = (size_t)s_case_count * sizeof(s_cases[0]);
    const size_t out_bytes = (size_t)s_case_count * sizeof(s_answers[0]);
    CUresult status = cuModuleLoadData(&module, s_image);
    status = (status == CUDA_SUCCESS) ? cuModuleGetFunction(&function, module, kernel) : status;
    status = (status == CUDA_SUCCESS) ? cuMemAlloc(&in, in_bytes) : status;
    status = (status == CUDA_SUCCESS) ? cuMemAlloc(&out, out_bytes) : status;
    status = (status == CUDA_SUCCESS) ? cuMemcpyHtoD(in, s_cases, in_bytes) : status;
    status = (status == CUDA_SUCCESS) ? cuMemsetD8(out, 0u, out_bytes) : status;
    void *arguments[] = {&in, &out, &count};
    status = (status == CUDA_SUCCESS)
                 ? cuLaunchKernel(function, 1u, 1u, 1u, SASS_RUN_THREADS, 1u, 1u, 0u, NULL, arguments, NULL)
                 : status;
    status = (status == CUDA_SUCCESS) ? cuCtxSynchronize() : status;
    status = (status == CUDA_SUCCESS) ? cuMemcpyDtoH(s_answers, out, out_bytes) : status;
    if (status == CUDA_SUCCESS)
    {
        cuMemFree(in);
        cuMemFree(out);
        cuModuleUnload(module);
    }
    return status;
}

int program_main(int count, char **words)
{
    // the safety airlock: the gate reads the verdict of the vendor where the caller gives it, and keeps it for the
    // part
    if ((count >= 3) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "--gated", .ci = EMBED_FALSE)))
    {
        if (!cubin_safe_verdict_load(words[2]))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "the gated list ");
                mmgr_line_put(&engine_note.line, words[2]);
                mmgr_line_put(&engine_note.line, " did not read");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 2;
        }
        words[2] = words[0];
        words += 2;
        count -= 2;
    }
    if ((count < 5) || (count > 7))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(
                &engine_note.line,
                "interface_sass_run [--gated <file>] <machine file> <list> <first> <answers> [<cases> [<max>]]");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const unsigned long max =
        (count == 7) ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[6], .end = NULL, .base = 10) : 0UL;
    if ((count >= 6) && !sass_run_cases(words[5]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the cases ");
            mmgr_line_put(&engine_note.line, words[5]);
            mmgr_line_put(&engine_note.line, " did not read, or contain more than ");
            mmgr_line_decimal(&engine_note.line, SASS_RUN_THREADS, 1u);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    if (!sass_machine_read(&s_machine, words[1]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, words[1]);
            mmgr_line_put(&engine_note.line, " did not read, and nothing goes to the part unread");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    PlatformFile *const list = platform_file_open(words[2], "rb");
    PlatformFile *const answers = platform_file_open(words[4], "ab");
    CUdevice device = 0;
    CUcontext context = NULL;
    if ((list == NULL) || (answers == NULL) || (cuInit(0u) != CUDA_SUCCESS) ||
        (cuDeviceGet(&device, 0) != CUDA_SUCCESS) || (cuDevicePrimaryCtxRetain(&context, device) != CUDA_SUCCESS) ||
        (cuCtxSetCurrent(context) != CUDA_SUCCESS))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the list ");
            mmgr_line_put(&engine_note.line, words[2]);
            mmgr_line_put(&engine_note.line, ", the answers ");
            mmgr_line_put(&engine_note.line, words[4]);
            mmgr_line_put(&engine_note.line, " or the device was not reached");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const unsigned long first =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = words[3], .end = NULL, .base = 10);
    char line[SASS_RUN_LINE];
    unsigned long number = 0UL;
    while (platform_file_text_read(line, sizeof(line), list) != NULL)
    {
        char path[SASS_RUN_LINE];
        char kernel[SASS_RUN_LINE];
        const char *read = line;
        if ((number < first) || !line_word(&read, path, sizeof(path)) || !line_word(&read, kernel, sizeof(kernel)))
        {
            number += 1UL;
            continue;
        }
        if ((max != 0UL) && (number >= (first + max)))
        {
            platform_file_close(answers);
            platform_file_close(list);
            return 4;
        }
        // a line that the writer blocked from the part. Its turned operation key holds a control transfer, a wait or
        // no form, and a run of it could loop or stall the part. The program answers it without the device
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "skip",
                       .ci = EMBED_FALSE))
        {
            {
                mmgr_line_decimal(platform_file_line(answers), number, 1u);
                mmgr_line_put(platform_file_line(answers), " skipped ");
                mmgr_line_put(platform_file_line(answers), kernel);
                mmgr_line_put(platform_file_line(answers), "\n");
            }
            number += 1UL;
            continue;
        }
        // the host reads the cubin before the driver sees it. The program answers a cubin that does not read, or
        // that breaks a rule of cubin_safe, without the device
        const size_t size = sass_run_image(path);
        unsigned long long at = 0ULL;
        const unsigned int verdict =
            (size == 0u) ? CUBIN_SAFE_SIZE : cubin_safe_image(&s_machine, s_image, size, 0u, &at);
        if (verdict != CUBIN_SAFE)
        {
            {
                mmgr_line_decimal(platform_file_line(answers), number, 1u);
                mmgr_line_put(platform_file_line(answers), " skipped cubin_safe_");
                mmgr_line_decimal(platform_file_line(answers), verdict, 1u);
                mmgr_line_put(platform_file_line(answers), "\n");
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, path);
                mmgr_line_put(output_line, " blocked from the part: ");
                mmgr_line_put(output_line, cubin_safe_name(verdict));
                mmgr_line_put(output_line, " at byte ");
                mmgr_line_decimal(output_line, at, 1u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            number += 1UL;
            continue;
        }
        // the airlock: no line reaches the part without the vendor's verdict in hand, the cross must read it first
        if (!cubin_safe_verdict_ready())
        {
            {
                mmgr_line_decimal(platform_file_line(answers), number, 1u);
                mmgr_line_put(platform_file_line(answers), " skipped no vendor verdict\n");
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, path);
                mmgr_line_put(output_line, " blocked from the part: the cross must read this code before the part");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            number += 1UL;
            continue;
        }
        const CUresult status = sass_run_one(kernel);
        if (status != CUDA_SUCCESS)
        {
            const char *name = NULL;
            cuGetErrorName(status, &name);
            {
                mmgr_line_decimal(platform_file_line(answers), number, 1u);
                mmgr_line_put(platform_file_line(answers), " refused ");
                mmgr_line_put(platform_file_line(answers), (name != NULL) ? name : "unnamed");
                mmgr_line_put(platform_file_line(answers), "\n");
            }
            platform_file_close(answers);
            return 3;
        }
        if (count >= 6)
        {
            {
                mmgr_line_decimal(platform_file_line(answers), number, 1u);
                mmgr_line_put(platform_file_line(answers), " answered");
            }
            for (unsigned int at = 0u; at < s_case_count; at += 1u)
            {
                mmgr_line_put(platform_file_line(answers), " ");
                mmgr_line_hex(platform_file_line(answers), s_answers[at][0], 8u);
            }
            {
                mmgr_line_put(platform_file_line(answers), "\n");
            }
        }
        else
        {
            mmgr_line_decimal(platform_file_line(answers), number, 1u);
            mmgr_line_put(platform_file_line(answers), " answered ");
            mmgr_line_hex(platform_file_line(answers), s_answers[0][0], 8u);
            mmgr_line_put(platform_file_line(answers), " ");
            mmgr_line_hex(platform_file_line(answers), s_answers[0][1], 8u);
            mmgr_line_put(platform_file_line(answers), " ");
            mmgr_line_hex(platform_file_line(answers), s_answers[0][2], 8u);
            mmgr_line_put(platform_file_line(answers), " ");
            mmgr_line_hex(platform_file_line(answers), s_answers[0][3], 8u);
            mmgr_line_put(platform_file_line(answers), "\n");
        }
        // the pause spaces the launches, so that a run of them does not flood the part
        platform_paused(SASS_RUN_PAUSE_MS);
        number += 1UL;
    }
    platform_file_close(answers);
    platform_file_close(list);
    return 0;
}
