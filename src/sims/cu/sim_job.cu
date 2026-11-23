// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sim.h"

#include "../../cu/engine/runtime/daemon/tessera.h"
#include "../../cu/engine/runtime/obsignatio/obsignatio.h"

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#define SIM_JOB_RUNNING_MICROSECONDS 2000000ULL
#define SIM_JOB_SWEEP_MICROSECONDS 20000ULL
#define SIM_JOB_IDLE_MICROSECONDS 5000000ULL

static int sim_job_daemon(char *path, size_t capacity)
{
    // $TESSERA_DAEMON names the daemon; otherwise it is the tessera_daemon beside this program
    char named_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const named =
        (platform_environment("TESSERA_DAEMON", named_text, sizeof(named_text)) != 0ULL) ? named_text : NULL;
    if ((named != NULL) && (named[0] != '\0'))
    {
        VerbaLinea formed_line_1 = mmgr_line_over(path, capacity);
        mmgr_line_put(&formed_line_1, named);
        (void)mmgr_line_finish(&formed_line_1);
        const int written = (int)formed_line_1.at;
        // a non-negative length is compared whole against the capacity
        return (written > 0) && ((size_t)written < capacity);
    }
#if defined(_WIN32)
    // the capacity is a buffer size this file defines, which a DWORD counts on every Windows target
    const size_t length = (size_t)GetModuleFileNameA(NULL, path, (DWORD)capacity);
    const char *const daemon = "tessera_daemon.exe";
#else
    const ssize_t read = readlink("/proc/self/exe", path, capacity - 1u);
    // a failed read is -1 and is treated as no length at all
    const size_t length = (read > 0) ? (size_t)read : 0u;
    const char *const daemon = "tessera_daemon";
#endif
    size_t directory_length = (length < capacity) ? length : 0u;
    while ((directory_length != 0u) && (path[directory_length - 1u] != '/') && (path[directory_length - 1u] != '\\'))
    {
        directory_length -= 1u;
    }
    if ((directory_length == 0u) ||
        ((directory_length +
          EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = daemon, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u) > capacity))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = path + directory_length, .src = daemon,
               .bytes =
                   EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = daemon, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    return 1;
}

int sim_job_submit(SimResults *results, const char *name, int count, char *const *arguments,
                   unsigned long long declared)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    int device = 0;
    cudaDeviceProp properties;
    char daemon[ENGINE_PATH_CAPACITY];
    TesseraJobSubmit submit;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &submit, .bytes = sizeof(submit), .val = 0);
    // the request is the sim's name and every argument, each ended by a zero byte
    unsigned long long bytes =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1ULL;
    for (int at = 1; at < count; at += 1)
    {
        bytes +=
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1ULL;
    }
    unsigned char *const request = (unsigned char *)platform_cell_taken(
        (unsigned long long)((size_t)bytes), (unsigned int)platform_place_g_program_owner, 0);
    // the context is made before the job requests: the daemon measures it with the process and counts it beside the
    // declaration
    int ok = (request != NULL) && (declared != 0ULL) && sim_job_daemon(daemon, sizeof(daemon)) &&
             (cudaGetDevice(&device) == cudaSuccess) && (cudaGetDeviceProperties(&properties, device) == cudaSuccess) &&
             (cudaFree(0) == cudaSuccess);
    if (ok)
    {
        unsigned long long at_byte = 0ULL;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request, .src = name,
                   .bytes =
                       EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
        at_byte += EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1ULL;
        for (int at = 1; at < count; at += 1)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request + at_byte, .src = arguments[at],
                       .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = arguments[at],
                                           .cap = PLATFORM_TEXT_TO_TERMINATOR) +
                                1u);
            at_byte +=
                EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR) +
                1ULL;
        }
        const ObsignatioSignumRequest signum = {
            request, bytes, NULL, OBSIGNATIO_MODE_HASH, submit.signum.bytes, ENGINE_SIGNUM_BYTES, &error};
        ok = obsignatio_signum(&signum) == 0L;
    }
    platform_cell_released(request, (unsigned int)platform_place_g_program_owner);
    if (!ok)
    {
        sim_check(results, 0,
                  "tessera: the sim's job is requested (its signum, declaration, device and the daemon's place)");
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = submit.device, .src = properties.uuid.bytes,
               .bytes = TESSERA_DEVICE_BYTES);
#if defined(_WIN32)
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &submit.luid, .src = properties.luid, .bytes = sizeof(submit.luid));
#endif
    submit.declared = declared;
    submit.grace_microseconds = SIM_JOB_RUNNING_MICROSECONDS;
    submit.sweep_microseconds = SIM_JOB_SWEEP_MICROSECONDS;
    submit.idle_microseconds = SIM_JOB_IDLE_MICROSECONDS;
    char override_text[PLATFORM_ENVIRONMENT_BYTES];
    submit.override_budget =
        (platform_environment("TESSERA_OVERRIDE", override_text, sizeof(override_text)) != 0ULL) ? 1u : 0u;
    submit.daemon_path = daemon;
    submit.error = &error;
    TesseraTicket ticket;
    TesseraClient *client = NULL;
    ok = tessera_job_submit(&submit, &client, &ticket) == 0L;
    if (ok && (ticket.over_budget != 0u))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  tessera: ");
            mmgr_line_put(output_line, name);
            mmgr_line_put(output_line, " declares ");
            mmgr_line_decimal(output_line, declared, 1u);
            mmgr_line_put(output_line, " bytes over its recorded peak of ");
            mmgr_line_decimal(output_line, ticket.last_peak, 1u);
            mmgr_line_put(output_line, "; TESSERA_OVERRIDE=1 admits it");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        ok = (tessera_job_wait(client, &ticket, &error) == 0L) && (ticket.lost == 0u);
        if (!ok && (ticket.lost != 0u))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  tessera: ");
                mmgr_line_put(output_line, name);
                mmgr_line_put(output_line, " was reserved past its grace time and lost (ticket in ");
                mmgr_line_put(output_line, ticket.lost_path);
                mmgr_line_put(output_line, ")");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            tessera_job_precalc_saved(client, &error);
        }
    }
    sim_check(results, ok, "tessera: the device's daemon admits the sim's job");
    if (!ok)
    {
        return 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera: ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, " admitted, ");
        mmgr_line_decimal(output_line, ticket.granted, 1u);
        mmgr_line_put(output_line, " bytes reserved; it declared ");
        mmgr_line_decimal(output_line, declared, 1u);
        mmgr_line_put(output_line, " over the ");
        mmgr_line_decimal(output_line, ticket.located, 1u);
        mmgr_line_put(output_line, " its process occupied as it requested");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    results->job = client;
    results->job_declared = declared + ticket.located;
    return 1;
}

void sim_job_release(SimResults *results)
{
    if (results->job == NULL)
    {
        return;
    }
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    TesseraTicket ticket;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &ticket, .bytes = sizeof(ticket), .val = 0);
    const int released = tessera_job_release(results->job, &ticket, &error) == 0L;
    results->job = NULL;
    sim_check(results, released, "tessera: the sim's job releases");
    if (released)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera: released, peak ");
        mmgr_line_decimal(output_line, ticket.last_peak, 1u);
        mmgr_line_put(output_line, " bytes");
        mmgr_line_put(output_line, (ticket.last_peak > results->job_declared) ? ", more than it declared" : "");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}
