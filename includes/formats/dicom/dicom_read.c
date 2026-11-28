// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dicom_read.c: caching, describing and reading a series
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "dicom_internal.h"
#include "memoria_operor/memoria_operor.h"

static int dicom_open(const EngineIngestTools *tools, const char *path, const char *member, EngineError *error)
{
    DicomResident *const resident = &g_dicom_resident;
    if (!DICOM_CHECK((tools != NULL) && (path != NULL) && (member != NULL), &path, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    if ((resident->valid != 0) &&
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = resident->path, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = path, .ci = EMBED_FALSE)) &&
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = resident->member, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = member, .ci = EMBED_FALSE)))
    {
        return 1;
    }
    dicom_resident_release();
    const ZipArchive *const archive = zip_archive_cached(tools, path, error);
    unsigned long long first = 0ULL;
    unsigned long long count = 0ULL;
    if ((archive == NULL) || !zip_folder_find(archive, member, &first, &count) ||
        !DICOM_CHECK(count != 0ULL, member, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    if (!dicom_gather(tools, path, archive, first, count, error) || !dicom_agree(error) || !dicom_sort(error))
    {
        dicom_resident_release();
        return 0;
    }
    resident->path = (char *)platform_cell_taken(
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
        platform_place_g_program_owner, 0);
    resident->member = (char *)platform_cell_taken(
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = member, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
        platform_place_g_program_owner, 0);
    if (!DICOM_CHECK((resident->path != NULL) && (resident->member != NULL), resident, error, ENGINE_ERROR_RESOURCE))
    {
        dicom_resident_release();
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = resident->path, .src = path,
               .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = resident->member, .src = member,
               .bytes =
                   EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = member, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    resident->valid = 1;
    return 1;
}

static void dicom_extent(EngineArrayExtent *extent)
{
    const DicomResident *const resident = &g_dicom_resident;
    const DicomSlice *const lead = &resident->slices[0];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = extent, .bytes = sizeof(*extent), .val = 0);
    extent->rank = 4u;
    extent->sizes[0] = 1ULL;
    extent->sizes[1] = resident->count;
    extent->sizes[2] = lead->rows;
    extent->sizes[3] = lead->columns;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = extent->axes, .src = "tzyx", .bytes = 4u);
    extent->element_bytes = lead->bits_allocated / 8u;
    extent->element_type = (lead->pixel_representation != 0u) ? ENGINE_ELEMENT_SIGNED : ENGINE_ELEMENT_UNSIGNED;
}

int dicom_zip_has_member(const EngineIngestTools *tools, const char *path, const char *member)
{
    // the module gathered and parsed a series that it already contains as DICOM whole, and the series is a member of
    // it
    const DicomResident *const resident = &g_dicom_resident;
    if ((resident->valid != 0) && (path != NULL) && (member != NULL) &&
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = resident->path, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = path, .ci = EMBED_FALSE)) &&
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = resident->member, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = member, .ci = EMBED_FALSE)))
    {
        return 1;
    }
    EngineError probe;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &probe, .bytes = sizeof(probe), .val = 0);
    const ZipArchive *const archive = (member != NULL) ? zip_archive_cached(tools, path, &probe) : NULL;
    unsigned long long first = 0ULL;
    unsigned long long count = 0ULL;
    if ((archive == NULL) || !zip_folder_find(archive, member, &first, &count))
    {
        return 0;
    }
    for (unsigned long long at = first; at < (first + count); at += 1ULL)
    {
        ZipEntry entry;
        if (!zip_entry_at(archive, at, &entry, &probe))
        {
            return 0;
        }
        if ((entry.name_length == 0ULL) || (entry.name[entry.name_length - 1ULL] == '/'))
        {
            continue;
        }
        unsigned char *const member =
            (unsigned char *)platform_cell_taken((size_t)entry.uncompressed + 1u, platform_place_g_program_owner, 0);
        const int read = (member != NULL) && (zip_member_read(tools, path, archive, &entry, member, entry.uncompressed,
                                                              &probe) != ZIP_ERROR);
        const int preamble_valid =
            read && (entry.uncompressed >= (DICOM_PREAMBLE + 4ULL)) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = member + DICOM_PREAMBLE, .other = "DICM", .bytes = 4u) == 0);
        platform_cell_released(member, platform_place_g_program_owner);
        return preamble_valid ? 1 : 0;
    }
    return 0;
}

long dicom_describe(const EngineDescribeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return -1L;
    }
    if (!DICOM_CHECK(request->extent != NULL, request, request->error, ENGINE_ERROR_REQUEST) ||
        !dicom_open(request->tools, request->path, request->member, request->error))
    {
        return -1L;
    }
    dicom_extent(request->extent);
    return 0L;
}

static unsigned long long dicom_sign_extend(const DicomSlice *slice, unsigned char *pixels)
{
    const unsigned int element_bytes = slice->bits_allocated / 8u;
    const unsigned int width = 8u * element_bytes;
    const unsigned long long mask = (width >= 64u) ? ~0ULL : ((1ULL << width) - 1ULL);
    const unsigned int low = (slice->high_bit + 1u) - slice->bits_stored;
    const unsigned long long field = (slice->bits_stored >= 64u) ? ~0ULL : ((1ULL << slice->bits_stored) - 1ULL);
    const unsigned long long sign = 1ULL << (slice->bits_stored - 1u);
    const unsigned long long elements = slice->pixel_bytes / element_bytes;
    // a stored field as wide as the whole element is its own sign extension: the call leaves every element as it is
    if ((low == 0u) && (slice->bits_stored == width))
    {
        return 0ULL;
    }
    unsigned long long extended_any = 0ULL;
    if (element_bytes == 2u)
    {
        // the same extension on a little-endian 16-bit element, read and written as one word
        for (unsigned long long element = 0ULL; element < elements; element += 1ULL)
        {
            unsigned char *const at = pixels + (element * 2ULL);
            const unsigned long long word = (unsigned long long)at[0] | ((unsigned long long)at[1] << 8u);
            const unsigned long long stored = (word >> low) & field;
            const unsigned long long extended = ((stored & sign) != 0ULL) ? ((stored | ~field) & mask) : stored;
            extended_any |= (((extended << low) & mask) != word) ? 1ULL : 0ULL;
            // the extended element is 16 bits, taken a byte at a time
            at[0] = (unsigned char)(extended & 0xFFULL);
            at[1] = (unsigned char)((extended >> 8u) & 0xFFULL);
        }
        return extended_any;
    }
    for (unsigned long long element = 0ULL; element < elements; element += 1ULL)
    {
        unsigned char *const at = pixels + (element * element_bytes);
        unsigned long long word = 0ULL;
        for (unsigned int place = element_bytes; place > 0u; place -= 1u)
        {
            word = (word << 8u) | (unsigned long long)at[place - 1u];
        }
        const unsigned long long stored = (word >> low) & field;
        const unsigned long long extended = ((stored & sign) != 0ULL) ? ((stored | ~field) & mask) : stored;
        const unsigned long long canonical = (extended << low) & mask;
        extended_any |= (canonical != word) ? 1ULL : 0ULL;
        for (unsigned int place = 0u; place < element_bytes; place += 1u)
        {
            // the call takes one byte of the extended word whole
            at[place] = (unsigned char)((extended >> (8u * place)) & 0xFFULL);
        }
    }
    return extended_any;
}

static void dicom_side_release(EngineSideBytes *side)
{
    platform_cell_released(side->pixel_at, platform_place_g_program_owner);
    platform_cell_released(side->pixel_stored, platform_place_g_program_owner);
    platform_cell_released(side->byte_start, platform_place_g_program_owner);
    platform_cell_released(side->bytes, platform_place_g_program_owner);
    platform_cell_released(side->name_start, platform_place_g_program_owner);
    platform_cell_released(side->names, platform_place_g_program_owner);
    platform_cell_released(side->member_crc, platform_place_g_program_owner);
    platform_cell_released(side->member_bytes, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = side, .bytes = sizeof(*side), .val = 0);
}

static int dicom_side_fill(EngineSideBytes *side, EngineError *error)
{
    const DicomResident *const resident = &g_dicom_resident;
    unsigned long long byte_total = 0ULL;
    unsigned long long named = 0ULL;
    for (unsigned long long at = 0ULL; at < resident->count; at += 1ULL)
    {
        const DicomSlice *const slice = &resident->slices[at];
        byte_total += slice->member_bytes - ((slice->pixel_stored != 0ULL) ? 0ULL : slice->pixel_bytes);
        named += slice->name_length;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = side, .bytes = sizeof(*side), .val = 0);
    side->pixel_at = (unsigned long long *)platform_cell_cleared(
        (size_t)resident->count + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    side->pixel_stored = (unsigned long long *)platform_cell_cleared(
        (size_t)resident->count + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    side->byte_start = (unsigned long long *)platform_cell_cleared(
        (size_t)resident->count + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    side->bytes = (unsigned char *)platform_cell_taken((size_t)byte_total + 1u, platform_place_g_program_owner, 0);
    side->name_start = (unsigned long long *)platform_cell_cleared(
        (size_t)resident->count + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    side->names = (char *)platform_cell_taken((size_t)named + 1u, platform_place_g_program_owner, 0);
    side->member_crc = (unsigned long long *)platform_cell_cleared(
        (size_t)resident->count + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    side->member_bytes = (unsigned long long *)platform_cell_cleared(
        (size_t)resident->count + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    if (!DICOM_CHECK((side->pixel_at != NULL) && (side->pixel_stored != NULL) && (side->byte_start != NULL) &&
                         (side->bytes != NULL) && (side->name_start != NULL) && (side->names != NULL) &&
                         (side->member_crc != NULL) && (side->member_bytes != NULL),
                     side, error, ENGINE_ERROR_RESOURCE))
    {
        dicom_side_release(side);
        return 0;
    }
    unsigned long long byte_at = 0ULL;
    unsigned long long name_at = 0ULL;
    for (unsigned long long place = 0ULL; place < resident->count; place += 1ULL)
    {
        const DicomSlice *const slice = &resident->slices[resident->order[place]];
        const unsigned long long after =
            (slice->pixel_stored != 0ULL) ? slice->pixel_at : (slice->pixel_at + slice->pixel_bytes);
        side->pixel_at[place] = slice->pixel_at;
        side->pixel_stored[place] = slice->pixel_stored;
        side->byte_start[place] = byte_at;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = side->bytes + byte_at, .src = slice->member,
                   .bytes = (size_t)slice->pixel_at);
        byte_at += slice->pixel_at;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = side->bytes + byte_at, .src = slice->member + after,
                   .bytes = (size_t)(slice->member_bytes - after));
        byte_at += slice->member_bytes - after;
        side->name_start[place] = name_at;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = side->names + name_at, .src = slice->name,
                   .bytes = (size_t)slice->name_length);
        name_at += slice->name_length;
        side->member_crc[place] = slice->member_crc;
        side->member_bytes[place] = slice->member_bytes;
    }
    side->byte_start[resident->count] = byte_at;
    side->name_start[resident->count] = name_at;
    side->leaves = resident->count;
    return 1;
}

long long dicom_read(const EngineArrayRead *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return -1LL;
    }
    EngineError *const error = request->error;
    if (!DICOM_CHECK((request->extent != NULL) && (request->out != NULL), request, error, ENGINE_ERROR_REQUEST) ||
        !dicom_open(request->tools, request->path, request->member, error))
    {
        return -1LL;
    }
    EngineArrayExtent found;
    dicom_extent(&found);
    const DicomResident *const resident = &g_dicom_resident;
    const unsigned long long slice_bytes = resident->slices[0].pixel_bytes;
    const unsigned long long total = slice_bytes * resident->count;
    int agrees = (found.rank == request->extent->rank) && (found.element_bytes == request->extent->element_bytes) &&
                 (found.element_type == request->extent->element_type) && (request->first == 0ULL) &&
                 (request->end == 1ULL) && (total <= request->out_capacity);
    for (unsigned int axis = 0u; agrees && (axis < found.rank); axis += 1u)
    {
        agrees = (found.sizes[axis] == request->extent->sizes[axis]);
    }
    if (!DICOM_CHECK(agrees, request->extent, error, ENGINE_ERROR_REQUEST))
    {
        dicom_resident_release();
        return -1LL;
    }
    for (unsigned long long place = 0ULL; place < resident->count; place += 1ULL)
    {
        DicomSlice *const slice = &resident->slices[resident->order[place]];
        unsigned char *const pixels = request->out + (place * slice_bytes);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = pixels, .src = slice->member + slice->pixel_at,
                   .bytes = (size_t)slice_bytes);
        if (slice->pixel_representation != 0u)
        {
            slice->pixel_stored = dicom_sign_extend(slice, pixels);
        }
    }
    const int sided = (request->side == NULL) || dicom_side_fill(request->side, error);
    dicom_resident_release();
    // the total fits the caller's capacity, a size in memory, and so fits a long long
    return sided ? (long long)total : -1LL;
}
