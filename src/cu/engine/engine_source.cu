// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_source.cu: zip names, the source's type, its description and its bytes
#include "../../../includes/platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "engine_internal.h"
#include "engine_note.h"
#include "memoria_operor/memoria_operor.h"

int entry_ends(const char *path, const char *suffix)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t size = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = suffix, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    int same = (length >= size);
    for (size_t at = 0u; same && (at < size); at += 1u)
    {
        const char character = path[length - size + at];
        // Explicit cast reads the byte as its code point
        const char lowered = (char)EMBED_CALL(ascii.lower, AsciiCfg, .byte = (uint8_t)character);
        same = (lowered == suffix[at]);
    }
    return same;
}

static int entry_zip_names(const char *path, const char *member)
{
    EngineError probe;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &probe, .bytes = sizeof(probe), .val = 0);
    const ZipArchive *const archive = zip_archive_cached(entry_ingest_tools(), path, &probe);
    unsigned long long first = 0ULL;
    unsigned long long count = 0ULL;
    if ((archive == NULL) || (zip_folder_find(archive, member, &first, &count) && (count != 0ULL)))
    {
        return archive != NULL;
    }
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = member, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    int named = 0;
    for (unsigned long long at = 0ULL; (named == 0) && (at < archive->entries); at += 1ULL)
    {
        ZipEntry entry;
        if (!zip_entry_at(archive, at, &entry, &probe))
        {
            return 0;
        }
        const int stem = (entry.name_length >= length) &&
                         (EMBED_CALL(memor.cmp, MemoriaCfg, .src = entry.name, .other = member, .bytes = length) == 0);
        named = stem &&
                ((entry.name_length == length) ||
                 ((entry.name_length == (length + 4u)) &&
                  (EMBED_CALL(memor.cmp, MemoriaCfg, .src = entry.name + length, .other = ".npy", .bytes = 4u) == 0)));
    }
    return named;
}

static EntrySourceType entry_source_type(const char *path, const char *member)
{
    char probe[ENTRY_PATH_CAPACITY];
    static const char *const MARKS[4] = {"zarr.json", ".zarray", ".zgroup", "attributes.json"};
    for (unsigned int mark = 0u; mark < 4u; mark += 1u)
    {
        if (entry_joined(probe, sizeof(probe), path, MARKS[mark]) && (stack_file_size(probe) >= 0LL))
        {
            return ENTRY_SOURCE_ZARR;
        }
    }
    if (entry_ends(path, ".stack"))
    {
        return ENTRY_SOURCE_STACK;
    }
    // The code reads a file's first bytes once and keeps them with its path. A run that describes many samples of one
    // archive reads its head once
    static char cached_path[ENTRY_PATH_CAPACITY];
    static unsigned char cached_head[352];
    static long long cached_read = -1LL;
    unsigned char head[352];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = head, .bytes = sizeof(head), .val = 0);
    long long bytes_read = -1LL;
    if ((cached_read >= 0LL) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = cached_path,
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = path, .ci = EMBED_FALSE)))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = head, .src = cached_head, .bytes = sizeof(head));
        bytes_read = cached_read;
    }
    else
    {
        EngineFileRange range;
        range.path = path;
        range.offset = 0ULL;
        range.bytes = sizeof(head);
        range.out = head;
        bytes_read = stack_file_read(&range);
        const int cacheable =
            (bytes_read >= 0LL) && (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path,
                                               .cap = PLATFORM_TEXT_TO_TERMINATOR) < sizeof(cached_path));
        cached_read = cacheable ? bytes_read : -1LL;
        if (cacheable)
        {
            EMBED_CALL(
                memor.cpy, MemoriaCfg, .dst = cached_path, .src = path,
                .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = cached_head, .src = head, .bytes = sizeof(head));
        }
    }
    if (bytes_read < 8LL)
    {
        return ENTRY_SOURCE_NONE;
    }
    const unsigned int little_header = (unsigned int)head[0] | ((unsigned int)head[1] << 8u) |
                                       ((unsigned int)head[2] << 16u) | ((unsigned int)head[3] << 24u);
    const unsigned int big_header = ((unsigned int)head[0] << 24u) | ((unsigned int)head[1] << 16u) |
                                    ((unsigned int)head[2] << 8u) | (unsigned int)head[3];
    if (((head[0] == 'I') && (head[1] == 'I') && ((head[2] == 42u) || (head[2] == 43u)) && (head[3] == 0u)) ||
        ((head[0] == 'M') && (head[1] == 'M') && (head[2] == 0u) && ((head[3] == 42u) || (head[3] == 43u))))
    {
        return ENTRY_SOURCE_TIFF;
    }
    if ((EMBED_CALL(memor.cmp, MemoriaCfg, .src = head, .other = "\x89HDF\r\n\x1a\n", .bytes = 8u) == 0) ||
        entry_ends(path, ".h5") || entry_ends(path, ".hdf5") || entry_ends(path, ".ims"))
    {
        return ENTRY_SOURCE_HDF5;
    }
    if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = head, .other = "PAR1", .bytes = 4u) == 0)
    {
        return ENTRY_SOURCE_PARQUET;
    }
    if ((EMBED_CALL(memor.cmp, MemoriaCfg, .src = head, .other = "PK\x03\x04", .bytes = 4u) == 0) &&
        dicom_zip_has_member(entry_ingest_tools(), path, member))
    {
        return ENTRY_SOURCE_DICOM;
    }
    if ((EMBED_CALL(memor.cmp, MemoriaCfg, .src = head, .other = "PK\x03\x04", .bytes = 4u) == 0) && (member != NULL) &&
        !entry_zip_names(path, member))
    {
        return ENTRY_SOURCE_NO_MEMBER;
    }
    if ((EMBED_CALL(memor.cmp, MemoriaCfg, .src = head, .other = "\x93NUMPY", .bytes = 6u) == 0) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = head, .other = "PK\x03\x04", .bytes = 4u) == 0))
    {
        return ENTRY_SOURCE_NPY;
    }
    if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = head, .other = "NRRD000", .bytes = 7u) == 0)
    {
        return ENTRY_SOURCE_NRRD;
    }
    if ((little_header == 348u) || (big_header == 348u) || (little_header == 540u) || (big_header == 540u) ||
        entry_ends(path, ".nii.gz") || entry_ends(path, ".nii"))
    {
        return ENTRY_SOURCE_NIFTI;
    }
    return ENTRY_SOURCE_NONE;
}

static int entry_source_describe(const char *path, const char *member, EntrySource *source, EngineError *error)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = source, .bytes = sizeof(*source), .val = 0);
    source->type = entry_source_type(path, member);
    source->member = member;
    const EngineIngestTools *const tools = entry_ingest_tools();
    EngineDescribeRequest describe;
    describe.path = path;
    describe.member = member;
    describe.tools = tools;
    describe.extent = &source->extent;
    describe.error = error;
    {
        VerbaLinea buffer_line = mmgr_line_over(source->path, sizeof(source->path));
        mmgr_line_put(&buffer_line, path);
        (void)mmgr_line_finish(&buffer_line);
    }
    switch (source->type)
    {
    case ENTRY_SOURCE_ZARR: {
        const int ok = entry_zarr_describe(path, member, &source->layout, source->path, sizeof(source->path));
        source->extent = source->layout.extent;
        return ok;
    }
    case ENTRY_SOURCE_TIFF:
        return tiff_describe(&describe) == 0L;
    case ENTRY_SOURCE_HDF5:
        return hdf5_describe(&describe) == 0L;
    case ENTRY_SOURCE_NPY:
        return npy_describe(&describe) == 0L;
    case ENTRY_SOURCE_DICOM:
        return dicom_describe(&describe) == 0L;
    case ENTRY_SOURCE_NRRD:
        return nrrd_describe(&describe) == 0L;
    case ENTRY_SOURCE_NIFTI:
        return nifti_describe(&describe) == 0L;
    case ENTRY_SOURCE_PARQUET:
        return parquet_describe(&describe) == 0L;
    case ENTRY_SOURCE_STACK: {
        unsigned int header[4] = {0u, 0u, 0u, 0u};
        PlatformFile *const stack = stack_open(path, header);
        if (stack == NULL)
        {
            return 0;
        }
        platform_file_close(stack);
        source->extent.rank = 4u;
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            source->extent.sizes[axis] = header[axis];
            source->extent.axes[axis] = "tzyx"[axis];
        }
        source->extent.element_bytes = 2u;
        source->extent.element_type = ENGINE_ELEMENT_UNSIGNED;
        return 1;
    }
    case ENTRY_SOURCE_NO_MEMBER: {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  ");
        mmgr_line_put(&engine_note.line, path);
        mmgr_line_put(&engine_note.line, ": the archive contains no member named ");
        mmgr_line_put(&engine_note.line, member);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
        return ENGINE_CHECK(0, member, error, ENGINE_ERROR_REQUEST);
    }
    default: {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  ");
        mmgr_line_put(&engine_note.line, path);
        mmgr_line_put(&engine_note.line, ": not a dataset format the engine reads\n");
        engine_note_written(&engine_note);
        return 0;
    }
    }
}

static long long entry_source_bytes(const EntrySource *source, unsigned char *out, unsigned long long capacity,
                                    EngineSideBytes *side, EngineError *error)
{
    const EngineIngestTools *const tools = entry_ingest_tools();
    EngineArrayRead read;
    read.path = source->path;
    read.member = source->member;
    read.tools = tools;
    read.extent = &source->extent;
    read.first = 0ULL;
    read.end = source->extent.sizes[0];
    read.out = out;
    read.out_capacity = capacity;
    read.side = side;
    read.error = error;
    switch (source->type)
    {
    case ENTRY_SOURCE_ZARR: {
        ZarrReadRequest zarr;
        zarr.root = source->path;
        zarr.layout = &source->layout;
        zarr.tools = tools;
        zarr.first = 0ULL;
        zarr.end = source->extent.sizes[0];
        zarr.out = out;
        zarr.out_capacity = capacity;
        return zarr_read(&zarr);
    }
    case ENTRY_SOURCE_TIFF:
        return tiff_read(&read);
    case ENTRY_SOURCE_HDF5:
        return hdf5_read(&read);
    case ENTRY_SOURCE_NPY:
        return npy_read(&read);
    case ENTRY_SOURCE_DICOM:
        return dicom_read(&read);
    case ENTRY_SOURCE_NRRD:
        return nrrd_read(&read);
    case ENTRY_SOURCE_NIFTI:
        return nifti_read(&read);
    case ENTRY_SOURCE_PARQUET:
        return parquet_read(&read);
    case ENTRY_SOURCE_STACK:
        return stack_read_uncached(source->path, capacity / 2ULL, (unsigned short *)out) ? (long long)capacity : -1LL;
    default:
        return -1LL;
    }
}

extern "C" long engine_source_read(const EngineSourceRequest *request, unsigned long long extent[4],
                                   unsigned short **volume)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return ENGINE_ERROR;
    }
    EngineError *const error = request->error;
    *volume = NULL;
    EntrySource *const source = (EntrySource *)platform_cell_cleared(
        1u, sizeof(EntrySource), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((ENGINE_CHECK(source != NULL, &source, error, ENGINE_ERROR_RESOURCE) == 0) ||
        (entry_source_describe(request->path, request->member, source, error) == 0))
    {
        platform_cell_released(source, platform_place_g_program_owner);
        return ENGINE_ERROR;
    }
    const EngineArrayExtent *const array_extent = &source->extent;
    const unsigned int rank = array_extent->rank;
    char axes[ENGINE_ARRAY_RANK];
    const size_t stated = (request->axes != NULL) ? EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = request->axes,
                                                               .cap = PLATFORM_TEXT_TO_TERMINATOR)
                                                  : 0u;
    int ok = (rank != 0u) && ((request->axes == NULL) || (stated == rank));
    // a c axis, at max one, gives its lanes at the channel requested; every other axis is one of t z y x
    int channel_axis = -1;
    for (unsigned int axis = 0u; ok && (axis < rank); axis += 1u)
    {
        axes[axis] = (request->axes != NULL) ? request->axes[axis] : array_extent->axes[axis];
        const int channel = (axes[axis] == 'c') && (channel_axis < 0) && (request->channel < array_extent->sizes[axis]);
        channel_axis = channel ? (int)axis : channel_axis;
        ok = (axes[axis] == 't') || (axes[axis] == 'z') || (axes[axis] == 'y') || (axes[axis] == 'x') || channel;
    }
    if (ok == 0)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  ");
            mmgr_line_put(&engine_note.line, request->path);
            mmgr_line_put(&engine_note.line, ": the source's ");
            mmgr_line_decimal(&engine_note.line, rank, 1u);
            mmgr_line_put(&engine_note.line, " axes are not all named t z y x and one c containing channel ");
            mmgr_line_decimal(&engine_note.line, request->channel, 1u);
            mmgr_line_put(&engine_note.line,
                          "; name them in the .cfg's input axes and the channel in its input channel");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        platform_cell_released(source, platform_place_g_program_owner);
        return ENGINE_ERROR;
    }
    const int one_channel = (channel_axis < 0) || (array_extent->sizes[channel_axis] == 1ULL);
    if ((array_extent->element_type == ENGINE_ELEMENT_FLOAT) ||
        ((array_extent->element_bytes != 1u) && (array_extent->element_bytes != 2u)))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  ");
            mmgr_line_put(&engine_note.line, request->path);
            mmgr_line_put(&engine_note.line, ": ");
            mmgr_line_decimal(&engine_note.line, array_extent->element_bytes, 1u);
            mmgr_line_put(&engine_note.line, " byte ");
            mmgr_line_put(&engine_note.line,
                          (array_extent->element_type == ENGINE_ELEMENT_FLOAT)
                              ? "float"
                              : ((array_extent->element_type == ENGINE_ELEMENT_SIGNED) ? "signed" : "unsigned"));
            mmgr_line_put(&engine_note.line,
                          " elements are not supported; the engine takes 8 and 16 bit integer samples");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        platform_cell_released(source, platform_place_g_program_owner);
        return ENGINE_ERROR;
    }
    int placed[4] = {-1, -1, -1, -1};
    unsigned long long stride[ENGINE_ARRAY_RANK];
    unsigned long long elements = 1ULL;
    for (unsigned int axis = rank; axis > 0u; axis -= 1u)
    {
        stride[axis - 1u] = elements;
        elements *= array_extent->sizes[axis - 1u];
    }
    for (unsigned int target = 0u; ok && (target < 4u); target += 1u)
    {
        for (unsigned int axis = 0u; axis < rank; axis += 1u)
        {
            ok = ok && !((axes[axis] == "tzyx"[target]) && (placed[target] >= 0));
            placed[target] = (axes[axis] == "tzyx"[target]) ? (int)axis : placed[target];
        }
        extent[target] = (placed[target] >= 0) ? array_extent->sizes[placed[target]] : 1ULL;
    }
    int ordered = ok;
    int last = -1;
    for (unsigned int target = 0u; target < 4u; target += 1u)
    {
        ordered = ordered && ((placed[target] < 0) || (placed[target] > last));
        last = (placed[target] >= 0) ? placed[target] : last;
    }
    const unsigned long long bytes = elements * array_extent->element_bytes;
    unsigned char *const raw =
        ok ? (unsigned char *)platform_cell_taken((size_t)bytes + 1u, platform_place_g_program_owner, 0) : NULL;
    ok = ok && (raw != NULL) && (entry_source_bytes(source, raw, bytes, request->side, error) == (long long)bytes);
    if (ok == 0)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  ");
            mmgr_line_put(&engine_note.line, request->path);
            mmgr_line_put(&engine_note.line, ": the source was not read whole");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        platform_cell_released(raw, platform_place_g_program_owner);
        platform_cell_released(source, platform_place_g_program_owner);
        return ENGINE_ERROR;
    }
    const int signed_lanes = (array_extent->element_type == ENGINE_ELEMENT_SIGNED);
    const unsigned int lane_offset = signed_lanes ? 0x8000u : 0u;
    if (request->lane_offset != NULL)
    {
        *request->lane_offset = lane_offset;
    }
    if (ordered && one_channel && (array_extent->element_bytes == 2u))
    {
        unsigned short *const words = (unsigned short *)raw;
        for (unsigned long long word = 0ULL; signed_lanes && (word < elements); word += 1ULL)
        {
            // a two's complement word plus 2^15 is the word with its top bit flipped, exactly, and stays 16 bits
            words[word] = (unsigned short)(words[word] ^ 0x8000u);
        }
        *volume = words;
        platform_cell_released(source, platform_place_g_program_owner);
        return 0L;
    }
    unsigned short *const lanes = (unsigned short *)platform_cell_taken((size_t)elements * sizeof(unsigned short),
                                                                        platform_place_g_program_owner, 0);
    if (lanes == NULL)
    {
        platform_cell_released(raw, platform_place_g_program_owner);
        platform_cell_released(source, platform_place_g_program_owner);
        return ENGINE_ERROR;
    }
    unsigned long long at = 0ULL;
    for (unsigned long long time = 0ULL; time < extent[0]; time += 1ULL)
    {
        for (unsigned long long depth = 0ULL; depth < extent[1]; depth += 1ULL)
        {
            for (unsigned long long row = 0ULL; row < extent[2]; row += 1ULL)
            {
                for (unsigned long long column = 0ULL; column < extent[3]; column += 1ULL)
                {
                    const unsigned long long where[4] = {time, depth, row, column};
                    unsigned long long from =
                        (channel_axis >= 0) ? ((unsigned long long)request->channel * stride[channel_axis]) : 0ULL;
                    for (unsigned int target = 0u; target < 4u; target += 1u)
                    {
                        from += (placed[target] >= 0) ? (where[target] * stride[placed[target]]) : 0ULL;
                    }
                    const unsigned int word =
                        (array_extent->element_bytes == 1u)
                            ? (unsigned int)raw[from]
                            : ((unsigned int)raw[2ULL * from] | ((unsigned int)raw[(2ULL * from) + 1ULL] << 8u));
                    const unsigned int sign_bit = (array_extent->element_bytes == 1u) ? 0x80u : 0x8000u;
                    const unsigned int value_span = (array_extent->element_bytes == 1u) ? 0x100u : 0x10000u;
                    const unsigned int shifted =
                        ((word & sign_bit) != 0u) ? ((lane_offset + word) - value_span) : (lane_offset + word);
                    // an unsigned word, or a signed one plus 2^15, lies in 0 to 65535 and fits in an unsigned
                    // short
                    lanes[at] = (unsigned short)(signed_lanes ? shifted : word);
                    at += 1ULL;
                }
            }
        }
    }
    platform_cell_released(raw, platform_place_g_program_owner);
    platform_cell_released(source, platform_place_g_program_owner);
    *volume = lanes;
    return 0L;
}

const char *const ENTRY_SOURCE_SUFFIXES[ENTRY_SOURCE_SUFFIX_COUNT] = {
    ".ome.zarr", ".zarr", ".n5",  ".ome.tiff", ".ome.tif", ".tiff",   ".tif", ".hdf5", ".h5",
    ".ims",      ".npz",  ".npy", ".nhdr",     ".nrrd",    ".nii.gz", ".nii", ".hdr",  ".stack"};

extern "C" int engine_source_find(const char *source, const char *sample, char *out, size_t capacity)
{
    for (unsigned int suffix = 0u; suffix < ENTRY_SOURCE_SUFFIX_COUNT; suffix += 1u)
    {
        VerbaLinea formed_line_1 = mmgr_line_over(out, capacity);
        mmgr_line_put(&formed_line_1, source);
        mmgr_line_put(&formed_line_1, "/");
        mmgr_line_put(&formed_line_1, sample);
        mmgr_line_put(&formed_line_1, ENTRY_SOURCE_SUFFIXES[suffix]);
        (void)mmgr_line_finish(&formed_line_1);
        const int written = (int)formed_line_1.at;
        if ((written > 0) && ((size_t)written < capacity) && entry_exists(out))
        {
            return 1;
        }
    }
    return 0;
}

extern "C" long engine_source_lanes(const char *source, const char *sample, unsigned long long *lanes,
                                    EngineError *error)
{
    // the code finds the sample's source as ingest finds it, and only describes it. It reads no voxel
    if ((error == NULL) || (lanes == NULL))
    {
        return ENGINE_ERROR;
    }
    *lanes = 0ULL;
    char path[ENTRY_PATH_CAPACITY];
    const int archived = (source != NULL) && entry_is_file(source);
    VerbaLinea formed_line_8;
    const int placed =
        (source != NULL) && (sample != NULL) &&
        (archived ? ((formed_line_8 = mmgr_line_over(path, sizeof(path)), mmgr_line_put(&formed_line_8, source),
                      (void)mmgr_line_finish(&formed_line_8), (int)formed_line_8.at) > 0)
                  : engine_source_find(source, sample, path, sizeof(path)));
    EntrySource *const described =
        placed ? (EntrySource *)platform_cell_cleared(1u, sizeof(EntrySource), platform_place_g_program_owner, 0)
               : NULL;
    int ok = ENGINE_CHECK(placed != 0, sample, error, ENGINE_ERROR_REQUEST) &&
             ENGINE_CHECK(described != NULL, &described, error, ENGINE_ERROR_RESOURCE) &&
             entry_source_describe(path, archived ? sample : NULL, described, error);
    unsigned long long elements = ok ? 1ULL : 0ULL;
    for (unsigned int axis = 0u; ok && (axis < described->extent.rank); axis += 1u)
    {
        const unsigned long long extent = described->extent.sizes[axis];
        ok = (extent != 0ULL) && (elements <= (~0ULL / extent));
        elements = ok ? (elements * extent) : 0ULL;
    }
    platform_cell_released(described, platform_place_g_program_owner);
    *lanes = elements;
    return (ok && (elements != 0ULL)) ? 0L : ENGINE_ERROR;
}
