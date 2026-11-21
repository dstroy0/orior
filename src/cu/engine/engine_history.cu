// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_history.cu: the history and the public entry points
#include "engine_internal.h"
#include "../../platform/platform.h"
#include "engine_note.h"
#include "memoria_operor/memoria_linea.h"

static int entry_history_same(const EngineHistory *one, const EngineHistory *other)
{
    const size_t entries = (size_t)(one->windows * one->windows);
    const size_t words = (size_t)(one->windows * one->extent[1] * one->extent[2] * one->extent[3]);
    return (mmgr_bytes_compare(one->extent, other->extent, sizeof(one->extent)) == 0) &&
           (one->windows == other->windows) && entry_signum_same(&one->sample, &other->sample) &&
           (one->payload_crc == other->payload_crc) && (one->cloud_crc == other->cloud_crc) &&
           (mmgr_bytes_compare(one->cloud, other->cloud, entries * sizeof(unsigned long long)) == 0) &&
           (mmgr_bytes_compare(one->history, other->history, words * sizeof(unsigned long long)) == 0);
}

extern "C" long engine_entropy_set(const EngineEntropySetRequest *request)
{
    if ((request == NULL) || (request->set.error == NULL))
    {
        return ENGINE_ERROR;
    }
    EngineError *const error = request->set.error;
    if (ENGINE_CHECK((request->set.set != NULL) && (request->set.samples != NULL), request, error,
                     ENGINE_ERROR_REQUEST) == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    int ok = 1;
    for (unsigned int sample = 0u; ok && (sample < request->set.count); sample += 1u)
    {
        const char *const name = request->set.samples[sample];
        char path[ENTRY_PATH_CAPACITY];
        char iapx_path[ENTRY_PATH_CAPACITY];
        ok = ENGINE_CHECK(engine_sample_path(path, sizeof(path), request->set.set, name, ".oapx") != 0, name, error,
                          ENGINE_ERROR_REQUEST) &&
             ENGINE_CHECK(
                 engine_sample_path(iapx_path, sizeof(iapx_path), request->set.set, name, ENTRY_CRYSTAL_SUFFIX) != 0,
                 name, error, ENGINE_ERROR_REQUEST);
        if (ok == 0)
        {
            break;
        }
        const unsigned long long began = engine_clock_microseconds();
        EngineStream located;
        EngineSignum located_root;
        EngineError probe;
        mmgr_bytes_set(&probe, 0, sizeof(probe));
        if ((request->reuse != 0u) && krep_crystal_head(iapx_path, &located, &located_root, &probe))
        {
            EngineHistory stored_history;
            const int read_back = krep_history_read(path, &stored_history, 1u, &probe);
            const int same = read_back && entry_signum_same(&stored_history.sample, &located_root);
            krep_history_release(&stored_history);
            if (same)
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put_columns(output_line, name, 24u);
                mmgr_line_put(output_line,
                              " positioned down already: read back whole, projected from the " ENTRY_CRYSTAL_SUFFIX
                              " as it is, in ");
                mmgr_line_decimal(output_line, (engine_clock_microseconds() - began) / 1000ULL, 1u);
                mmgr_line_put(output_line, " ms\n");
                platform_line_flushed();
                continue;
            }
        }

        unsigned long long extent[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        unsigned short *volume = NULL;
        EngineSignum sample_root;
        mmgr_bytes_set(&sample_root, 0, sizeof(sample_root));
        ok = (engine_iapx_load(request->set.set, name, extent, &volume, &sample_root, NULL, error) == 0L);
        const long windows = ok ? entropy_history_windows(extent, error) : ENTROPY_HISTORY_ERROR;
        ok = ok && (windows != ENTROPY_HISTORY_ERROR);
        const unsigned long long window_count = ok ? (unsigned long long)windows : 0ULL;
        std::vector<unsigned long long> cloud((size_t)(window_count * window_count));
        std::vector<unsigned long long> payload((size_t)(window_count * extent[1] * extent[2] * extent[3]));
        EngineHistory history;
        mmgr_bytes_set(&history, 0, sizeof(history));
        history.cloud = cloud.data();
        history.history = payload.data();
        EntropyHistoryProjectRequest project;
        mmgr_bytes_set(&project, 0, sizeof(project));
        project.volume = volume;
        mmgr_bytes_copy(project.extent, extent, sizeof(project.extent));
        project.sample = sample_root;
        project.history = &history;
        project.error = error;
        unsigned long long broken = 0ULL;
        ok = ok && (entropy_history_project(&project, &broken) == 0L);
        free(volume);
        if (ENGINE_CHECK(broken == 0ULL, &broken, error, ENGINE_ERROR_LOGIC) == 0)
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  ");
                mmgr_line_put(&engine_note.line, name);
                mmgr_line_put(&engine_note.line, ": entropy not conserved: ");
                mmgr_line_decimal(&engine_note.line, broken, 1u);
                mmgr_line_put(&engine_note.line, " voxels' flips break parity with their net change");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            ok = 0;
            break;
        }

        EngineHistory read;
        mmgr_bytes_set(&read, 0, sizeof(read));
        ok = ok && ENGINE_IO(engine_directories_make(path, 0) != 0, path, error) &&
             krep_history_write(path, &history, error) && krep_history_read(path, &read, 1u, error) &&
             ENGINE_CHECK(entry_history_same(&history, &read) != 0, &read, error, ENGINE_ERROR_LOGIC);
        krep_history_release(&read);
        if (ok == 0)
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  ");
                mmgr_line_put(&engine_note.line, name);
                mmgr_line_put(&engine_note.line, ": the entropy history was not written and read back whole");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            break;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, name, 24u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, extent[0], 1u);
            mmgr_line_put(output_line, " frames, ");
            mmgr_line_decimal(output_line, window_count, 1u);
            mmgr_line_put(output_line, " windows of ");
            mmgr_line_decimal(output_line, ENGINE_HISTORY_WINDOW, 1u);
            mmgr_line_put(output_line, " transitions, entropy conserved at every voxel, ");
            mmgr_line_decimal(output_line, (unsigned long long)(payload.size() * sizeof(unsigned long long)) >> 20u,
                              1u);
            mmgr_line_put(output_line, " MiB, read back whole, CRC-64 ");
            mmgr_line_hex(output_line, history.payload_crc, 16u);
            mmgr_line_put(output_line, ", in ");
            mmgr_line_decimal(output_line, (engine_clock_microseconds() - began) / 1000ULL, 1u);
            mmgr_line_put(output_line, " ms");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        entropy_history_report(&history);
        platform_line_flushed();
    }
    if (ok == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    return 0L;
}

extern "C" long engine_entropy_cloud(const char *path, unsigned int *windows, unsigned long long *cloud,
                                     EngineError *error)
{
    if (error == NULL)
    {
        return ENGINE_ERROR;
    }
    EngineHistory history;
    if ((ENGINE_CHECK((windows != NULL) && (cloud != NULL), &cloud, error, ENGINE_ERROR_REQUEST) == 0) ||
        (krep_history_read(path, &history, 0u, error) == 0))
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    *windows = (unsigned int)history.windows;
    mmgr_bytes_copy(cloud, history.cloud, (size_t)(history.windows * history.windows) * sizeof(unsigned long long));
    krep_history_release(&history);
    return 0L;
}

extern "C" long engine_entropy_history_read(const char *path, EngineHistory *history, EngineError *error)
{
    if (error == NULL)
    {
        return ENGINE_ERROR;
    }
    if (krep_history_read(path, history, 1u, error) == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    return 0L;
}

extern "C" void engine_entropy_history_release(EngineHistory *history)
{
    krep_history_release(history);
}

extern "C" long engine_bodies_write(const char *set, const char *sample, EngineBodyTable *table, EngineError *error)
{
    if (error == NULL)
    {
        return ENGINE_ERROR;
    }
    if (ENGINE_CHECK(table != NULL, &table, error, ENGINE_ERROR_REQUEST) == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    char path[ENTRY_PATH_CAPACITY];
    table->crc = crc_words(CRC_TABLE, table->words, (size_t)(table->bodies * ENGINE_BODY_WORDS));
    if ((ENGINE_CHECK(engine_sample_path(path, sizeof(path), set, sample, ".bapx") != 0, sample, error,
                      ENGINE_ERROR_REQUEST) == 0) ||
        (krep_bodies_write(path, table, error) == 0))
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    EngineBodyTable back;
    const int ok = krep_bodies_read(path, &back, error);
    const int same =
        (ok != 0) &&
        ENGINE_CHECK(
            (back.bodies == table->bodies) && (back.crc == table->crc) &&
                (mmgr_bytes_compare(back.frame_start, table->frame_start,
                                    (size_t)(table->frames + 1ULL) * sizeof(unsigned long long)) == 0) &&
                (mmgr_bytes_compare(back.words, table->words,
                                    (size_t)(table->bodies * ENGINE_BODY_WORDS) * sizeof(unsigned long long)) == 0),
            &back, error, ENGINE_ERROR_LOGIC);
    krep_bodies_release(&back);
    if (same == 0)
    {
        remove(path);
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    return 0L;
}

extern "C" long engine_bodies_read(const char *set, const char *sample, EngineBodyTable *table, EngineError *error)
{
    if (error == NULL)
    {
        return ENGINE_ERROR;
    }
    char path[ENTRY_PATH_CAPACITY];
    if ((ENGINE_CHECK(engine_sample_path(path, sizeof(path), set, sample, ".bapx") != 0, sample, error,
                      ENGINE_ERROR_REQUEST) == 0) ||
        (krep_bodies_read(path, table, error) == 0))
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    return 0L;
}

extern "C" void engine_bodies_release(EngineBodyTable *table)
{
    krep_bodies_release(table);
}
