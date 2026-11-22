// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_listing.cu: the listing, geff arrays, lanes and the iapx codec
#include "engine_internal.h"
#include "../../platform/platform.h"
#include "engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

static int entry_order_names(const void *left, const void *right)
{
    return mmgr_text_order(*(const char *const *)left, *(const char *const *)right, PLATFORM_TEXT_TO_TERMINATOR);
}

static unsigned int entry_listing(const char *directory, char ***names, int samples_of_set)
{
    std::vector<char *> found;
#ifdef _WIN32
    char pattern[ENTRY_PATH_CAPACITY];
    {
        VerbaLinea buffer_line = mmgr_line_over(pattern, sizeof(pattern));
        mmgr_line_put(&buffer_line, directory);
        mmgr_line_put(&buffer_line, "/*");
        (void)mmgr_line_finish(&buffer_line);
    }
    struct __finddata64_t entry;
    const intptr_t search = _findfirst64(pattern, &entry);
    for (int more = (search != -1) ? 0 : -1; more == 0; more = _findnext64(search, &entry))
    {
        const char *const name = entry.name;
#else
    DIR *const search = opendir(directory);
    for (struct dirent *entry = (search != NULL) ? readdir(search) : NULL; entry != NULL; entry = readdir(search))
    {
        const char *const name = entry->d_name;
#endif
        if (name[0] == '.')
        {
            continue;
        }
        char stem[ENTRY_PATH_CAPACITY];
        {
            VerbaLinea buffer_line = mmgr_line_over(stem, sizeof(stem));
            mmgr_line_put(&buffer_line, name);
            (void)mmgr_line_finish(&buffer_line);
        }
        int has_sample = 0;
        if (samples_of_set != 0)
        {
            char character[ENTRY_PATH_CAPACITY];
            has_sample = engine_sample_path(character, sizeof(character), directory, name, ENTRY_CRYSTAL_SUFFIX) &&
                         entry_exists(character);
        }
        for (unsigned int suffix = 0u;
             (samples_of_set == 0) && (has_sample == 0) && (suffix < ENTRY_SOURCE_SUFFIX_COUNT); suffix += 1u)
        {
            if (entry_ends(stem, ENTRY_SOURCE_SUFFIXES[suffix]))
            {
                stem[mmgr_text_length(stem, PLATFORM_TEXT_TO_TERMINATOR) -
                     mmgr_text_length(ENTRY_SOURCE_SUFFIXES[suffix], PLATFORM_TEXT_TO_TERMINATOR)] = '\0';
                has_sample = 1;
            }
        }
        if (has_sample != 0)
        {
            const size_t size = mmgr_text_length(stem, PLATFORM_TEXT_TO_TERMINATOR) + 1u;
            char *const copy = (char *)platform_cell_taken(size, platform_place_g_program_owner, 0);
            if (copy != NULL)
            {
                mmgr_bytes_copy(copy, stem, size);
                found.push_back(copy);
            }
        }
    }
#ifdef _WIN32
    if (search != -1)
    {
        _findclose(search);
    }
#else
    if (search != NULL)
    {
        closedir(search);
    }
#endif
    char **const listed =
        (char **)platform_cell_cleared(found.size() + 1u, sizeof(char *), platform_place_g_program_owner, 0);
    if (listed == NULL)
    {
        for (size_t found_at = 0u; found_at < found.size(); found_at += 1u)
        {
            platform_cell_released(found[found_at], platform_place_g_program_owner);
        }
        *names = NULL;
        return 0u;
    }
    for (size_t found_at = 0u; found_at < found.size(); found_at += 1u)
    {
        listed[found_at] = found[found_at];
    }
    qsort(listed, found.size(), sizeof(char *), entry_order_names);
    *names = listed;
    return (unsigned int)found.size();
}

extern "C" unsigned int engine_source_samples(const char *source, char ***names)
{
    // a parquet file's samples are its members, named and ordered as parquet_members gives them
    const unsigned int members = entry_is_file(source) ? parquet_members(entry_ingest_tools(), source, names) : 0u;
    return (members != 0u) ? members : entry_listing(source, names, 0);
}

extern "C" unsigned int engine_set_samples(const char *set, char ***names)
{
    return entry_listing(set, names, 1);
}

static long long entry_geff_array(const char *root, const char *leaf, unsigned long long want_rank,
                                  unsigned long long *rows, unsigned char **bytes)
{
    char path[ENTRY_PATH_CAPACITY];
    char array_root[ENTRY_PATH_CAPACITY];
    ZarrLayout layout;
    *bytes = NULL;
    if ((entry_joined(path, sizeof(path), root, leaf) == 0) ||
        (entry_zarr_describe(path, NULL, &layout, array_root, sizeof(array_root)) == 0) ||
        (layout.extent.rank != want_rank) || (layout.extent.element_bytes != 8u) ||
        (layout.extent.element_type == ENGINE_ELEMENT_FLOAT))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  ");
            mmgr_line_put(&engine_note.line, root);
            mmgr_line_put(&engine_note.line, "/");
            mmgr_line_put(&engine_note.line, leaf);
            mmgr_line_put(&engine_note.line, ": not an 8 byte integer array of rank ");
            mmgr_line_decimal(&engine_note.line, want_rank, 1u);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return -1LL;
    }
    unsigned long long elements = 1ULL;
    for (unsigned int axis = 0u; axis < layout.extent.rank; axis += 1u)
    {
        elements *= layout.extent.sizes[axis];
    }
    *rows = layout.extent.sizes[0];
    unsigned char *const out =
        (unsigned char *)platform_cell_taken((size_t)(elements * 8ULL) + 8u, platform_place_g_program_owner, 0);
    ZarrReadRequest read;
    read.root = array_root;
    read.layout = &layout;
    read.tools = entry_ingest_tools();
    read.first = 0ULL;
    read.end = layout.extent.sizes[0];
    read.out = out;
    read.out_capacity = elements * 8ULL;
    if ((out == NULL) || ((elements != 0ULL) && (zarr_read(&read) != (long long)(elements * 8ULL))))
    {
        platform_cell_released(out, platform_place_g_program_owner);
        return -1LL;
    }
    *bytes = out;
    return (long long)elements;
}

extern "C" void engine_geff_release(EngineGeff *geff)
{
    platform_cell_released(geff->node_identity, platform_place_g_program_owner);
    platform_cell_released(geff->node_place, platform_place_g_program_owner);
    platform_cell_released(geff->edge_ends, platform_place_g_program_owner);
    mmgr_bytes_set(geff, 0, sizeof(*geff));
}

extern "C" long engine_geff_read(const char *path, EngineGeff *geff)
{
    mmgr_bytes_set(geff, 0, sizeof(*geff));
    static const char *const PLACES[4] = {"nodes/props/t/values", "nodes/props/z/values", "nodes/props/y/values",
                                          "nodes/props/x/values"};
    unsigned long long rows = 0ULL;
    unsigned char *ids = NULL;
    unsigned char *edges = NULL;
    unsigned char *places[4] = {NULL, NULL, NULL, NULL};
    int ok = (entry_geff_array(path, "nodes/ids", 1ULL, &geff->nodes, &ids) >= 0LL);
    for (unsigned int axis = 0u; ok && (axis < 4u); axis += 1u)
    {
        ok = (entry_geff_array(path, PLACES[axis], 1ULL, &rows, &places[axis]) >= 0LL) && (rows == geff->nodes);
    }
    ok = ok && (entry_geff_array(path, "edges/ids", 2ULL, &geff->edges, &edges) >= 0LL);
    geff->node_identity =
        ok ? (unsigned long long *)platform_cell_taken((size_t)(geff->nodes + 1ULL) * sizeof(unsigned long long),
                                                       platform_place_g_program_owner, 0)
           : NULL;
    geff->node_place = ok ? (long long *)platform_cell_taken((size_t)(geff->nodes + 1ULL) * 4u * sizeof(long long),
                                                             platform_place_g_program_owner, 0)
                          : NULL;
    ok = ok && (geff->node_identity != NULL) && (geff->node_place != NULL);
    for (unsigned long long node = 0ULL; ok && (node < geff->nodes); node += 1ULL)
    {
        mmgr_bytes_copy(&geff->node_identity[node], &ids[8ULL * node], 8u);
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            mmgr_bytes_copy(&geff->node_place[(4ULL * node) + axis], &places[axis][8ULL * node], 8u);
        }
    }
    if (ok)
    {
        geff->edge_ends = (unsigned long long *)edges;
        edges = NULL;
    }
    platform_cell_released(ids, platform_place_g_program_owner);
    platform_cell_released(edges, platform_place_g_program_owner);
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        platform_cell_released(places[axis], platform_place_g_program_owner);
    }
    if (ok == 0)
    {
        engine_geff_release(geff);
        return ENGINE_ERROR;
    }
    return 0L;
}

extern "C" long engine_iapx_head(const char *set, const char *sample, unsigned long long extent[4], EngineError *error)
{
    if (error == NULL)
    {
        return ENGINE_ERROR;
    }
    char path[ENTRY_PATH_CAPACITY];
    EngineStream stream;
    mmgr_bytes_set(&stream, 0, sizeof(stream));
    if ((ENGINE_CHECK(engine_sample_path(path, sizeof(path), set, sample, ENTRY_CRYSTAL_SUFFIX) != 0, sample, error,
                      ENGINE_ERROR_REQUEST) == 0) ||
        (krep_crystal_head(path, &stream, NULL, error) == 0))
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    mmgr_bytes_copy(extent, stream.extent, sizeof(stream.extent));
    return 0L;
}

unsigned long long entry_lanes(const unsigned long long extent[4])
{
    unsigned long long lanes = 1ULL;
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        if ((extent[axis] == 0ULL) || (extent[axis] > (1ULL << 40u)) || (lanes > ((1ULL << 40u) / extent[axis])))
        {
            return 0ULL;
        }
        lanes *= extent[axis];
    }
    return lanes;
}

int entry_iapx_decode(const EngineStream *stream, const unsigned short *device_lanes, unsigned short *rebuilt,
                      unsigned long long *mismatches, const unsigned short **device_rebuilt, EngineError *error)
{
    int *coefficients = NULL;
    if (tower_capacity(stream->extent, &coefficients, error) != 0L)
    {
        return 0;
    }
    CompressionDecodeRequest code;
    mmgr_bytes_set(&code, 0, sizeof(code));
    code.offsets = stream->offsets;
    code.chunks = stream->chunks;
    code.stream = stream->stream;
    code.bits = stream->bits;
    code.count = entry_lanes(stream->extent);
    code.device_coefficients = coefficients;
    code.error = error;
    if (compression_decode(&code) != 0L)
    {
        return 0;
    }
    TowerLowerRequest lower;
    mmgr_bytes_set(&lower, 0, sizeof(lower));
    lower.device_lanes = device_lanes;
    mmgr_bytes_copy(lower.extent, stream->extent, sizeof(lower.extent));
    lower.mismatches = mismatches;
    lower.device_rebuilt = device_rebuilt;
    lower.rebuilt = rebuilt;
    lower.error = error;
    return (tower_lower(&lower) == 0L) ? 1 : 0;
}

int entry_iapx_encode(const unsigned short *device_lanes, const unsigned long long extent[4], EngineStream *stream,
                      unsigned int *floors, EngineError *error)
{
    mmgr_bytes_set(stream, 0, sizeof(*stream));
    mmgr_bytes_copy(stream->extent, extent, sizeof(stream->extent));
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    TowerLiftRequest lift;
    mmgr_bytes_set(&lift, 0, sizeof(lift));
    lift.device_lanes = device_lanes;
    mmgr_bytes_copy(lift.extent, extent, sizeof(lift.extent));
    lift.coefficients = &coefficients;
    lift.scratch = &scratch;
    lift.floors = floors;
    lift.error = error;
    if (tower_lift(&lift) != 0L)
    {
        return 0;
    }
    CompressionEncodeRequest code;
    mmgr_bytes_set(&code, 0, sizeof(code));
    code.device_coefficients = coefficients;
    code.count = entry_lanes(extent);
    code.device_scratch = scratch;
    code.chunks = &stream->chunks;
    code.bits = &stream->bits;
    code.offsets = &stream->offsets;
    code.stream = &stream->stream;
    code.error = error;
    return (compression_encode(&code) == 0L) ? 1 : 0;
}

extern "C" long engine_lattice_bits(const int *values, const unsigned long long extent[4], unsigned long long *bits,
                                    EngineError *error)
{
    if (error == NULL)
    {
        return ENGINE_ERROR;
    }
    const unsigned long long lanes = (extent != NULL) ? entry_lanes(extent) : 0ULL;
    int *device_values = NULL;
    int ok =
        ENGINE_CHECK((values != NULL) && (bits != NULL) && (lanes != 0ULL), &values, error, ENGINE_ERROR_REQUEST) &&
        ENGINE_STATUS_CHECK(cudaMalloc((void **)&device_values, (size_t)lanes * sizeof(int)), &device_values, error) &&
        ENGINE_STATUS_CHECK(cudaMemcpy(device_values, values, (size_t)lanes * sizeof(int), cudaMemcpyHostToDevice),
                            device_values, error);
    const int *coefficients = NULL;
    unsigned int *scratch = NULL;
    unsigned int floors = 0u;
    if (ok != 0)
    {
        TowerLiftRequest lift;
        mmgr_bytes_set(&lift, 0, sizeof(lift));
        lift.device_values = device_values;
        mmgr_bytes_copy(lift.extent, extent, sizeof(lift.extent));
        lift.coefficients = &coefficients;
        lift.scratch = &scratch;
        lift.floors = &floors;
        lift.error = error;
        ok = tower_lift(&lift) == 0L;
    }
    if (ok != 0)
    {
        // the offsets and the stream are compression's own, retained for its next call; only the bits are retained
        EngineStream stream;
        mmgr_bytes_set(&stream, 0, sizeof(stream));
        CompressionEncodeRequest code;
        mmgr_bytes_set(&code, 0, sizeof(code));
        code.device_coefficients = coefficients;
        code.count = lanes;
        code.device_scratch = scratch;
        code.chunks = &stream.chunks;
        code.bits = bits;
        code.offsets = &stream.offsets;
        code.stream = &stream.stream;
        code.error = error;
        ok = compression_encode(&code) == 0L;
    }
    cudaFree(device_values);
    if (ok == 0)
    {
        engine_error_frame(error);
        engine_error_latch(error);
        return ENGINE_ERROR;
    }
    return 0L;
}
