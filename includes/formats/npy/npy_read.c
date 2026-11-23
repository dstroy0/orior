// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// npy_read.c: the layout, zip archives and the read
#include "../../../src/cu/engine/engine_note.h"
#include "../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "npy_internal.h"

static const NpySource npy_empty_source = {NULL, NULL, 0ULL, 0ULL, NULL};

static const NpyLayout npy_empty_layout = {{0u, {0ULL}, {'\0'}, 0u, ENGINE_ELEMENT_UNSIGNED}, 0u, 0u, 0ULL, 0ULL};

static const ZipEntry npy_empty_entry = {NULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};

static const unsigned char npy_magic[6u] = {0x93u, 'N', 'U', 'M', 'P', 'Y'};

static const unsigned char npy_zip_local[4u] = {'P', 'K', 0x03u, 0x04u};

static const unsigned char npy_zip_end[4u] = {'P', 'K', 0x05u, 0x06u};

static unsigned int npy_total(const EngineArrayExtent *extent, unsigned int from_axis, unsigned long long *total)
{
    unsigned long long product = extent->element_bytes;
    unsigned int empty = 0u;
    for (unsigned int axis = from_axis; axis < extent->rank; axis += 1u)
    {
        empty |= (extent->sizes[axis] == 0ULL) ? 1u : 0u;
    }
    for (unsigned int axis = from_axis; (empty == 0u) && (axis < extent->rank); axis += 1u)
    {
        if (!npy_multiply(product, extent->sizes[axis], &product))
        {
            return 0u;
        }
    }
    *total = (empty != 0u) ? 0ULL : product;
    return 1u;
}

static unsigned int npy_layout(const NpySource *source, NpyLayout *layout)
{
    unsigned char preamble[12u];
    const unsigned long long preamble_bytes = (source->length < 12ULL) ? source->length : 12ULL;
    if ((preamble_bytes < 10ULL) || !npy_source_fetch(source, 0ULL, preamble_bytes, preamble) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = preamble, .other = npy_magic, .bytes = sizeof npy_magic) != 0))
    {
        return 0u;
    }
    const unsigned int major = preamble[6u];
    const unsigned int minor = preamble[7u];
    if ((minor != 0u) || (major < 1u) || (major > 3u) || ((major > 1u) && (preamble_bytes < 12ULL)))
    {
        return 0u;
    }
    const unsigned long long header_start = (major == 1u) ? 10ULL : 12ULL;
    const unsigned long long header_length = npy_load(preamble + 8u, (major == 1u) ? 2u : 4u);
    if (header_length > (source->length - header_start))
    {
        return 0u;
    }
    unsigned char *const header = platform_cell_taken((size_t)header_length + 1u, platform_place_g_program_owner, 0);
    if (header == NULL)
    {
        return 0u;
    }
    NpyLayout parsed = npy_empty_layout;
    const int understood =
        npy_source_fetch(source, header_start, header_length, header) && npy_dictionary(header, header_length, &parsed);
    platform_cell_released(header, platform_place_g_program_owner);
    unsigned long long data_bytes = 0ULL;
    if (!understood || !npy_total(&parsed.extent, 0u, &data_bytes))
    {
        return 0u;
    }
    parsed.data_offset = header_start + header_length;
    parsed.data_bytes = data_bytes;
    if (data_bytes > (source->length - parsed.data_offset))
    {
        return 0u;
    }
    *layout = parsed;
    return 1u;
}

static unsigned int npy_zip_open(const EngineIngestTools *tools, const char *path, const ZipArchive *archive,
                                 const ZipEntry *entry, NpySource *source, unsigned char **owned, EngineError *error)
{
    if (entry->method == 0ULL)
    {
        unsigned long long data_start = 0ULL;
        if (!zip_member_data(tools, path, archive, entry, &data_start, error) ||
            !NPY_CHECK(entry->compressed == entry->uncompressed, entry, error, ENGINE_ERROR_REQUEST))
        {
            return 0u;
        }
        const NpySource stored = {tools, path, data_start, entry->uncompressed, NULL};
        *source = stored;
        return 1u;
    }
    unsigned char *const unpacked =
        npy_fits_memory(entry->uncompressed + 1ULL)
            ? platform_cell_taken((size_t)entry->uncompressed + 1u, platform_place_g_program_owner, 0)
            : NULL;
    if (!NPY_CHECK(unpacked != NULL, entry, error, ENGINE_ERROR_RESOURCE))
    {
        return 0u;
    }
    if (zip_member_read(tools, path, archive, entry, unpacked, entry->uncompressed, error) == ZIP_ERROR)
    {
        platform_cell_released(unpacked, platform_place_g_program_owner);
        return 0u;
    }
    const NpySource inflated = {tools, path, 0ULL, entry->uncompressed, unpacked};
    *source = inflated;
    *owned = unpacked;
    return 1u;
}

static unsigned int npy_zip_candidate(const ZipEntry *entry, const char *member)
{
    const int directory = (entry->name_length > 0ULL) && (entry->name[entry->name_length - 1ULL] == '/');
    if (directory)
    {
        return 0u;
    }
    if (member == NULL)
    {
        return 1u;
    }
    const size_t member_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = member, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const int exact =
        (entry->name_length == member_length) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = entry->name, .other = member, .bytes = member_length) == 0);
    const int suffixed =
        (entry->name_length == (member_length + 4u)) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = entry->name, .other = member, .bytes = member_length) == 0) &&
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = entry->name + member_length, .other = ".npy", .bytes = 4u) == 0);
    return (exact || suffixed) ? 1u : 0u;
}

static void npy_zip_list(const EngineIngestTools *tools, const char *path, const char *member,
                         const ZipArchive *archive, unsigned long long matches)
{
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "npy: ");
        mmgr_line_put(&engine_note.line, path);
        mmgr_line_put(&engine_note.line, " contains ");
        mmgr_line_decimal(&engine_note.line, matches, 1u);
        mmgr_line_put(&engine_note.line, " candidate arrays");
        mmgr_line_put(&engine_note.line, (member != NULL) ? " named " : "");
        mmgr_line_put(&engine_note.line, (member != NULL) ? member : "");
        mmgr_line_put(&engine_note.line, "; name one as the member");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    unsigned long long at = 0ULL;
    for (unsigned long long entry_index = 0ULL; entry_index < archive->entries; entry_index += 1ULL)
    {
        EngineError probe;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &probe, .bytes = sizeof(probe), .val = 0);
        ZipEntry entry = npy_empty_entry;
        if (!zip_entry_next(archive, &at, &entry, &probe))
        {
            return;
        }
        if (!npy_zip_candidate(&entry, member))
        {
            continue;
        }
        NpySource source = npy_empty_source;
        unsigned char *owned = NULL;
        NpyLayout layout = npy_empty_layout;
        const int parsed =
            npy_zip_open(tools, path, archive, &entry, &source, &owned, &probe) && npy_layout(&source, &layout);
        platform_cell_released(owned, platform_place_g_program_owner);
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "npy:   ");
            mmgr_line_put_max(&engine_note.line, (const char *)entry.name, (size_t)((int)entry.name_length));
            mmgr_line_put(&engine_note.line, " ");
            engine_note_written(&engine_note);
        }
        if (!parsed)
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "unreadable");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            continue;
        }
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "(");
            engine_note_written(&engine_note);
        }
        for (unsigned int axis = 0u; axis < layout.extent.rank; axis += 1u)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, (axis == 0u) ? "" : ", ");
            mmgr_line_decimal(&engine_note.line, (uint64_t)layout.extent.sizes[axis], 1u);
            engine_note_written(&engine_note);
        }
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, ")");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
    }
}

static unsigned int npy_zip_select(const EngineIngestTools *tools, const char *path, const char *member,
                                   const ZipArchive *archive, ZipEntry *chosen, EngineError *error)
{
    unsigned long long matches = 0ULL;
    unsigned long long at = 0ULL;
    for (unsigned long long entry_index = 0ULL; entry_index < archive->entries; entry_index += 1ULL)
    {
        ZipEntry entry = npy_empty_entry;
        if (!zip_entry_next(archive, &at, &entry, error))
        {
            return 0u;
        }
        if (npy_zip_candidate(&entry, member))
        {
            matches += 1ULL;
            *chosen = entry;
        }
    }
    if (matches > 1ULL)
    {
        npy_zip_list(tools, path, member, archive, matches);
    }
    return NPY_CHECK(matches == 1ULL, &matches, error, ENGINE_ERROR_REQUEST) ? 1u : 0u;
}

static unsigned int npy_resolve(const char *path, const char *member, const EngineIngestTools *tools, NpySource *source,
                                unsigned char **owned, NpyLayout *layout, EngineError *error)
{
    *owned = NULL;
    const long long size = tools->size(path);
    unsigned char magic[4u];
    if (!NPY_CHECK((size >= 4LL) && npy_file_fetch(tools, path, 0ULL, 4ULL, magic), path, error, ENGINE_ERROR_REQUEST))
    {
        return 0u;
    }
    // a size of at least four converts to unsigned long long exactly
    const unsigned long long file_bytes = (unsigned long long)size;
    if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = magic, .other = npy_magic, .bytes = sizeof magic) == 0)
    {
        const NpySource entire_file = {tools, path, 0ULL, file_bytes, NULL};
        *source = entire_file;
        return NPY_CHECK((member == NULL) && npy_layout(source, layout), path, error, ENGINE_ERROR_REQUEST) ? 1u : 0u;
    }
    if (!NPY_CHECK(
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = magic, .other = npy_zip_local, .bytes = sizeof magic) == 0) ||
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = magic, .other = npy_zip_end, .bytes = sizeof magic) == 0),
            magic, error, ENGINE_ERROR_REQUEST))
    {
        return 0u;
    }
    ZipArchive archive;
    if (!zip_archive_open(tools, path, &archive, error))
    {
        return 0u;
    }
    ZipEntry entry = npy_empty_entry;
    const int ok = npy_zip_select(tools, path, member, &archive, &entry, error) &&
                   npy_zip_open(tools, path, &archive, &entry, source, owned, error) &&
                   NPY_CHECK(npy_layout(source, layout), source, error, ENGINE_ERROR_REQUEST);
    zip_archive_release(&archive);
    if (!ok)
    {
        platform_cell_released(*owned, platform_place_g_program_owner);
        *owned = NULL;
    }
    return ok ? 1u : 0u;
}

static unsigned int npy_same_extent(const EngineArrayExtent *found, const EngineArrayExtent *given)
{
    int same = (found->rank == given->rank) && (found->element_bytes == given->element_bytes) &&
               (found->element_type == given->element_type);
    for (unsigned int axis = 0u; same && (axis < found->rank); axis += 1u)
    {
        same = (found->sizes[axis] == given->sizes[axis]) && (found->axes[axis] == given->axes[axis]);
    }
    return same ? 1u : 0u;
}

static unsigned int npy_fortran_copy(const NpySource *source, const NpyLayout *layout, unsigned long long first,
                                     unsigned long long count, unsigned char *out)
{
    const EngineArrayExtent *const extent = &layout->extent;
    const unsigned long long element = extent->element_bytes;
    const unsigned long long stride = extent->sizes[0u];
    unsigned long long output_step[ENGINE_ARRAY_RANK];
    unsigned long long position[ENGINE_ARRAY_RANK];
    unsigned long long columns = 1ULL;
    for (unsigned int axis = extent->rank; axis > 0u; axis -= 1u)
    {
        output_step[axis - 1u] = columns;
        position[axis - 1u] = 0ULL;
        columns *= (axis > 1u) ? extent->sizes[axis - 1u] : 1ULL;
    }
    if (columns == 0ULL)
    {
        return 1u;
    }
    const unsigned long long run = count * element;
    const unsigned long long column_bytes = stride * element;
    const unsigned long long range =
        (run < NPY_FORTRAN_BLOCK) ? (1ULL + ((NPY_FORTRAN_BLOCK - run) / column_bytes)) : 1ULL;
    const unsigned long long group = (range < columns) ? range : columns;
    unsigned char *const block =
        platform_cell_taken((size_t)(((group - 1ULL) * column_bytes) + run), platform_place_g_program_owner, 0);
    if (block == NULL)
    {
        return 0u;
    }
    unsigned long long place = 0ULL;
    unsigned long long column = 0ULL;
    unsigned int ok = 1u;
    while ((ok != 0u) && (column < columns))
    {
        const unsigned long long taken = ((columns - column) < group) ? (columns - column) : group;
        const unsigned long long span = ((taken - 1ULL) * column_bytes) + run;
        ok = npy_source_fetch(source, layout->data_offset + (((column * stride) + first) * element), span, block);
        for (unsigned long long within = 0ULL; (ok != 0u) && (within < taken); within += 1ULL)
        {
            const unsigned char *const from = block + (within * column_bytes);
            for (unsigned long long row = 0ULL; row < count; row += 1ULL)
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = out + (((row * columns) + place) * element),
                           .src = from + (row * element), .bytes = (size_t)element);
            }
            for (unsigned int axis = 1u; axis < extent->rank; axis += 1u)
            {
                position[axis] += 1ULL;
                place += output_step[axis];
                if (position[axis] < extent->sizes[axis])
                {
                    break;
                }
                place -= position[axis] * output_step[axis];
                position[axis] = 0ULL;
            }
        }
        column += taken;
    }
    platform_cell_released(block, platform_place_g_program_owner);
    return ok;
}

long npy_describe(const EngineDescribeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return -1L;
    }
    NpySource source = npy_empty_source;
    unsigned char *owned = NULL;
    NpyLayout layout = npy_empty_layout;
    const unsigned int resolved =
        npy_resolve(request->path, request->member, request->tools, &source, &owned, &layout, request->error);
    platform_cell_released(owned, platform_place_g_program_owner);
    if (!resolved)
    {
        return -1L;
    }
    *request->extent = layout.extent;
    return 0L;
}

long long npy_read(const EngineArrayRead *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return -1LL;
    }
    NpySource source = npy_empty_source;
    unsigned char *owned = NULL;
    NpyLayout layout = npy_empty_layout;
    const unsigned int resolved =
        npy_resolve(request->path, request->member, request->tools, &source, &owned, &layout, request->error);
    unsigned long long row_bytes = 0ULL;
    const int agrees = resolved && npy_same_extent(&layout.extent, request->extent) &&
                       (request->first <= request->end) && (request->end <= layout.extent.sizes[0u]) &&
                       npy_total(&layout.extent, 1u, &row_bytes);
    const unsigned long long count = agrees ? (request->end - request->first) : 0ULL;
    const unsigned long long wanted = count * row_bytes;
    if (!agrees || (wanted > request->out_capacity) || !npy_fits_memory(wanted))
    {
        platform_cell_released(owned, platform_place_g_program_owner);
        return -1LL;
    }
    if (wanted == 0ULL)
    {
        platform_cell_released(owned, platform_place_g_program_owner);
        return 0LL;
    }
    const unsigned int copied =
        (layout.fortran_order != 0u)
            ? npy_fortran_copy(&source, &layout, request->first, count, request->out)
            : npy_source_fetch(&source, layout.data_offset + (request->first * row_bytes), wanted, request->out);
    platform_cell_released(owned, platform_place_g_program_owner);
    if (!copied)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = request->out, .bytes = (size_t)wanted, .val = 0);
        return -1LL;
    }
    if (layout.big_endian != 0u)
    {
        npy_swap(request->out, wanted, layout.extent.element_bytes);
    }
    return (long long)wanted;
}
