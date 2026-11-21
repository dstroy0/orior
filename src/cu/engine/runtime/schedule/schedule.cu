// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "schedule.h"
#include "../../engine_note.h"
#include "memoria_operor/memoria_linea.h"

#include "../../engine.h"

#include <cuda_runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    FILE *const out = fopen(g_schedule_path, "w");
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
    printf("  tower: %llu MiB free of %llu, scheduling against %llu MiB\n", (unsigned long long)free_bytes >> 20u,
           (unsigned long long)total_bytes >> 20u, cap >> 20u);
    fprintf(out, "{\n  \"scheme\": \"cell_tracking.program\",\n  \"version\": 1,\n");
    fprintf(out, "  \"tower\": {\"free\": %llu, \"total\": %llu, \"cap\": %llu},\n", (unsigned long long)free_bytes,
            (unsigned long long)total_bytes, cap);
    fprintf(out, "  \"stages\": [\n");
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
        printf("    %-24s %llu frames of %llu voxels, %llu MiB each, %llu per chunk, %llu chunks\n", names[at], frames,
               voxels, each >> 20u, chunk_frames, chunks);
        for (unsigned long long chunk = 0u; chunk < chunks; chunk += 1u)
        {
            const unsigned long long first = chunk * steps;
            unsigned long long end = first + chunk_frames;
            end = (end > frames) ? frames : end;
            fprintf(out, "%s    {\"sample\": \"%s\", \"first\": %llu, \"past\": %llu, \"bytes\": %llu}",
                    (written != 0u) ? ",\n" : "", names[at], first, end, (end - first) * each);
            written += 1u;
        }
    }
    fprintf(out, "\n  ]\n}\n");
    fclose(out);
    printf("  %u stages written to %s\n", written, g_schedule_path);
    return 1;
}
