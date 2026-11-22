// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "schedule.h"
#include "../../../../platform/platform.h"
#include "../../engine_note.h"
#include "memoria_operor/memoria_linea.h"

#include "../../engine.h"

#include <cuda_runtime.h>


const char *g_schedule_path = NULL;

int schedule_program(const char *set, char *const *names, unsigned int count)
{
    size_t free_bytes = 0u;
    size_t total_bytes = 0u;
    if (cudaMemGetInfo(&free_bytes, &total_bytes) != cudaSuccess)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  the tower did not answer how much it allocates");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    const unsigned long long cap = (unsigned long long)free_bytes / 3ULL * 2ULL;
    PlatformFile *const out = platform_file_open(g_schedule_path, "w");
    if (out == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  could not open ");
            mmgr_line_put(&engine_note.line, g_schedule_path);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tower: ");
        mmgr_line_decimal(output_line, (unsigned long long)free_bytes >> 20u, 1u);
        mmgr_line_put(output_line, " MiB free of ");
        mmgr_line_decimal(output_line, (unsigned long long)total_bytes >> 20u, 1u);
        mmgr_line_put(output_line, ", scheduling against ");
        mmgr_line_decimal(output_line, cap >> 20u, 1u);
        mmgr_line_put(output_line, " MiB");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        mmgr_line_put(platform_file_line(out), "{\n  \"scheme\": \"cell_tracking.program\",\n  \"version\": 1,\n");
    }
    {
        mmgr_line_put(platform_file_line(out), "  \"tower\": {\"free\": ");
        mmgr_line_decimal(platform_file_line(out), (unsigned long long)free_bytes, 1u);
        mmgr_line_put(platform_file_line(out), ", \"total\": ");
        mmgr_line_decimal(platform_file_line(out), (unsigned long long)total_bytes, 1u);
        mmgr_line_put(platform_file_line(out), ", \"cap\": ");
        mmgr_line_decimal(platform_file_line(out), cap, 1u);
        mmgr_line_put(platform_file_line(out), "},\n");
    }
    {
        mmgr_line_put(platform_file_line(out), "  \"stages\": [\n");
    }
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned long long head[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        EngineError error;
        mmgr_bytes_set(&error, 0, sizeof(error));
        if (engine_iapx_head(set, names[at], head, &error) != 0L)
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  ");
                mmgr_line_put(&engine_note.line, names[at]);
                mmgr_line_put(&engine_note.line, ": no .iapx in ");
                mmgr_line_put(&engine_note.line, set);
                mmgr_line_put(&engine_note.line, "; not scheduled");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            continue;
        }
        const unsigned long long frames = head[0];
        const unsigned long long voxels = head[1] * head[2] * head[3];
        const unsigned long long each = (voxels * 2ULL) + (voxels * ENGINE_RESIDUAL_LIMBS * sizeof(unsigned int)) +
                                        (voxels * sizeof(unsigned int)) +
                                        ((voxels + 63ULL) / 64ULL * sizeof(unsigned long long));
        unsigned long long chunk_frames = (each != 0ULL) ? (cap / each) : 0ULL;
        chunk_frames = (chunk_frames < 2ULL) ? 2ULL : chunk_frames;
        chunk_frames = (chunk_frames > frames) ? frames : chunk_frames;
        const unsigned long long steps = (chunk_frames > 1ULL) ? (chunk_frames - 1ULL) : 1ULL;
        const unsigned long long chunks = ((frames > 1ULL) ? (frames - 2ULL + steps) : 0ULL) / steps;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_put_columns(output_line, names[at], 24u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, frames, 1u);
            mmgr_line_put(output_line, " frames of ");
            mmgr_line_decimal(output_line, voxels, 1u);
            mmgr_line_put(output_line, " voxels, ");
            mmgr_line_decimal(output_line, each >> 20u, 1u);
            mmgr_line_put(output_line, " MiB each, ");
            mmgr_line_decimal(output_line, chunk_frames, 1u);
            mmgr_line_put(output_line, " per chunk, ");
            mmgr_line_decimal(output_line, chunks, 1u);
            mmgr_line_put(output_line, " chunks");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        for (unsigned long long chunk = 0u; chunk < chunks; chunk += 1u)
        {
            const unsigned long long first = chunk * steps;
            unsigned long long end = first + chunk_frames;
            end = (end > frames) ? frames : end;
            {
                mmgr_line_put(platform_file_line(out), (written != 0u) ? ",\n" : "");
                mmgr_line_put(platform_file_line(out), "    {\"sample\": \"");
                mmgr_line_put(platform_file_line(out), names[at]);
                mmgr_line_put(platform_file_line(out), "\", \"first\": ");
                mmgr_line_decimal(platform_file_line(out), first, 1u);
                mmgr_line_put(platform_file_line(out), ", \"past\": ");
                mmgr_line_decimal(platform_file_line(out), end, 1u);
                mmgr_line_put(platform_file_line(out), ", \"bytes\": ");
                mmgr_line_decimal(platform_file_line(out), (end - first) * each, 1u);
                mmgr_line_put(platform_file_line(out), "}");
            }
            written += 1u;
        }
    }
    {
        mmgr_line_put(platform_file_line(out), "\n  ]\n}\n");
    }
    platform_file_close(out);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, written, 1u);
        mmgr_line_put(output_line, " stages written to ");
        mmgr_line_put(output_line, g_schedule_path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}
