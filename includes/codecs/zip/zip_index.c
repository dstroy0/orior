// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// zip_index.c: the index, folders and member data
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "zip_internal.h"

static const unsigned char zip_local[4u] = {'P', 'K', 0x03u, 0x04u};

static const unsigned char *s_zip_sorting;

static void zip_folder_of(const unsigned char *name, unsigned long long length, unsigned long long *start,
                          unsigned long long *span)
{
    unsigned long long last = length;
    while ((last > 0ULL) && (name[last - 1ULL] != '/'))
    {
        last -= 1ULL;
    }
    if (last == 0ULL)
    {
        *start = 0ULL;
        *span = 0ULL;
        return;
    }
    unsigned long long before = last - 1ULL;
    while ((before > 0ULL) && (name[before - 1ULL] != '/'))
    {
        before -= 1ULL;
    }
    *start = before;
    *span = (last - 1ULL) - before;
}

static int zip_order_spans(const unsigned char *left, unsigned long long left_length, const unsigned char *right,
                           unsigned long long right_length)
{
    const unsigned long long shorter = (left_length < right_length) ? left_length : right_length;
    const int differ = (shorter != 0ULL)
                           ? EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = (size_t)shorter)
                           : 0;
    if (differ != 0)
    {
        return differ;
    }
    return (left_length < right_length) ? -1 : ((left_length > right_length) ? 1 : 0);
}

static int zip_order_entries(const void *left, const void *right)
{
    const unsigned char *const one = s_zip_sorting + *(const unsigned long long *)left;
    const unsigned char *const other = s_zip_sorting + *(const unsigned long long *)right;
    const unsigned long long one_length = zip_load(one + 28u, 2u);
    const unsigned long long other_length = zip_load(other + 28u, 2u);
    unsigned long long one_start = 0ULL;
    unsigned long long one_span = 0ULL;
    unsigned long long other_start = 0ULL;
    unsigned long long other_span = 0ULL;
    zip_folder_of(one + 46u, one_length, &one_start, &one_span);
    zip_folder_of(other + 46u, other_length, &other_start, &other_span);
    const int folder = zip_order_spans(one + 46u + one_start, one_span, other + 46u + other_start, other_span);
    return (folder != 0) ? folder : zip_order_spans(one + 46u, one_length, other + 46u, other_length);
}

int zip_archive_index(ZipArchive *archive, EngineError *error)
{
    if (!ZIP_CHECK(zip_fits_memory((archive->entries + 1ULL) * sizeof(unsigned long long)), &archive->entries, error,
                   ENGINE_ERROR_RESOURCE))
    {
        return 0;
    }
    unsigned long long *const entry_at = platform_cell_taken(
        (size_t)(archive->entries + 1ULL) * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    if (!ZIP_CHECK(entry_at != NULL, &archive->entries, error, ENGINE_ERROR_RESOURCE))
    {
        return 0;
    }
    unsigned long long at = 0ULL;
    for (unsigned long long index = 0ULL; index < archive->entries; index += 1ULL)
    {
        ZipEntry entry = zip_empty_entry;
        entry_at[index] = at;
        if (!zip_entry_next(archive, &at, &entry, error))
        {
            platform_cell_released(entry_at, platform_place_g_program_owner);
            return 0;
        }
    }
    s_zip_sorting = archive->directory;
    {
        // the room of the sort, on the temporary tier, which goes back as the sort ends
        void *const sort_room = platform_cell_taken((unsigned long long)((size_t)archive->entries) *
                                                        (unsigned long long)(sizeof(unsigned long long)),
                                                    platform_place_g_program_owner, 1);
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = entry_at, .bytes = sizeof(unsigned long long),
                         .scratch = sort_room, .count = (size_t)archive->entries, .order = zip_order_entries);
        platform_cell_released(sort_room, platform_place_g_program_owner);
    }
    s_zip_sorting = NULL;
    platform_cell_released(archive->entry_at, platform_place_g_program_owner);
    archive->entry_at = entry_at;
    return 1;
}

static int zip_folder_compare(const ZipArchive *archive, unsigned long long at, const char *folder,
                              unsigned long long folder_length)
{
    const unsigned char *const fixed = archive->directory + archive->entry_at[at];
    unsigned long long start = 0ULL;
    unsigned long long span = 0ULL;
    zip_folder_of(fixed + 46u, zip_load(fixed + 28u, 2u), &start, &span);
    return zip_order_spans(fixed + 46u + start, span, (const unsigned char *)folder, folder_length);
}

int zip_folder_find(const ZipArchive *archive, const char *folder, unsigned long long *first, unsigned long long *count)
{
    *first = 0ULL;
    *count = 0ULL;
    if ((archive->entry_at == NULL) || (folder == NULL))
    {
        return 0;
    }
    const unsigned long long folder_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = folder, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    unsigned long long low = 0ULL;
    unsigned long long high = archive->entries;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ULL);
        if (zip_folder_compare(archive, middle, folder, folder_length) < 0)
        {
            low = middle + 1ULL;
        }
        else
        {
            high = middle;
        }
    }
    unsigned long long end = low;
    while ((end < archive->entries) && (zip_folder_compare(archive, end, folder, folder_length) == 0))
    {
        end += 1ULL;
    }
    *first = low;
    *count = end - low;
    return 1;
}

int zip_entry_at(const ZipArchive *archive, unsigned long long index, ZipEntry *entry, EngineError *error)
{
    if (!ZIP_CHECK((archive->entry_at != NULL) && (index < archive->entries), &index, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    unsigned long long at = archive->entry_at[index];
    return zip_entry_next(archive, &at, entry, error);
}

static ZipResident s_zip_resident;

const ZipArchive *zip_archive_cached(const EngineIngestTools *tools, const char *path, EngineError *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    if (!ZIP_CHECK((path != NULL) && (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path,
                                                 .cap = PLATFORM_TEXT_TO_TERMINATOR) < ZIP_PATH_CAPACITY),
                   &path, error, ENGINE_ERROR_REQUEST))
    {
        return NULL;
    }
    ZipResident *const resident = &s_zip_resident;
    if ((resident->valid != 0) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = resident->path,
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = path, .ci = EMBED_FALSE)))
    {
        return &resident->archive;
    }
    zip_resident_release();
    if (!zip_archive_open(tools, path, &resident->archive, error) || !zip_archive_index(&resident->archive, error))
    {
        zip_archive_release(&resident->archive);
        return NULL;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = resident->path, .src = path,
               .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    resident->valid = 1;
    return &resident->archive;
}

void zip_resident_release(void)
{
    ZipResident *const resident = &s_zip_resident;
    if (resident->valid != 0)
    {
        zip_archive_release(&resident->archive);
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = resident, .bytes = sizeof(*resident), .val = 0);
}

int zip_member_data(const EngineIngestTools *tools, const char *path, const ZipArchive *archive, const ZipEntry *entry,
                    unsigned long long *data_start, EngineError *error)
{
    unsigned char local[30u];
    if (!ZIP_CHECK(((entry->flags & 0x41ULL) == 0ULL) && ((entry->method == 0ULL) || (entry->method == 8ULL)) &&
                       (entry->local_offset <= archive->file_bytes) &&
                       ((archive->file_bytes - entry->local_offset) >= 30ULL),
                   entry, error, ENGINE_ERROR_REQUEST) ||
        !ZIP_IO(zip_fetch(tools, path, entry->local_offset, 30ULL, local), local, error) ||
        !ZIP_CHECK(EMBED_CALL(memor.cmp, MemoriaCfg, .src = local, .other = zip_local, .bytes = sizeof zip_local) == 0,
                   local, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    const unsigned long long start =
        entry->local_offset + 30ULL + zip_load(local + 26u, 2u) + zip_load(local + 28u, 2u);
    if (!ZIP_CHECK((start <= archive->file_bytes) && (entry->compressed <= (archive->file_bytes - start)), local, error,
                   ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    *data_start = start;
    return 1;
}

// the stored bytes of a member into `out`. The call copies a stored member and inflates a deflated one. Then it checks
// the bytes against the CRC
static int zip_member_unpack(const EngineIngestTools *tools, const unsigned char *packed, const ZipEntry *entry,
                             unsigned char *out, EngineError *error)
{
    if (entry->method == 0ULL)
    {
        if (!ZIP_CHECK(entry->compressed == entry->uncompressed, entry, error, ENGINE_ERROR_REQUEST))
        {
            return 0;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = out, .src = packed, .bytes = (size_t)entry->uncompressed);
    }
    else
    {
        const EngineBytesDecode decode = tools->decode[ENGINE_CODEC_DEFLATE];
        if (!ZIP_CHECK(decode != NULL, tools, error, ENGINE_ERROR_REQUEST))
        {
            return 0;
        }
        const EngineBytesRequest request = {packed, entry->compressed, out, entry->uncompressed};
        const long long made = decode(&request);
        // a non-negative long long count converts to unsigned long long exactly
        if (!ZIP_CHECK((made >= 0LL) && ((unsigned long long)made == entry->uncompressed), packed, error,
                       ENGINE_ERROR_REQUEST))
        {
            return 0;
        }
    }
    return ZIP_CHECK(zip_crc32(out, entry->uncompressed) == entry->crc, &entry->crc, error, ENGINE_ERROR_REQUEST);
}

long long zip_member_read(const EngineIngestTools *tools, const char *path, const ZipArchive *archive,
                          const ZipEntry *entry, unsigned char *out, unsigned long long capacity, EngineError *error)
{
    if (error == NULL)
    {
        return ZIP_ERROR;
    }
    unsigned long long data_start = 0ULL;
    if (!ZIP_CHECK((tools != NULL) && (path != NULL) && (archive != NULL) && (entry != NULL) && (out != NULL) &&
                       (entry->uncompressed <= capacity) && zip_fits_memory(entry->uncompressed) &&
                       zip_fits_memory(entry->compressed + 1ULL),
                   entry, error, ENGINE_ERROR_REQUEST) ||
        !zip_member_data(tools, path, archive, entry, &data_start, error))
    {
        return ZIP_ERROR;
    }
    unsigned char *const packed =
        platform_cell_taken((size_t)entry->compressed + 1u, platform_place_g_program_owner, 0);
    const int ok = ZIP_CHECK(packed != NULL, entry, error, ENGINE_ERROR_RESOURCE) &&
                   ZIP_IO(zip_fetch(tools, path, data_start, entry->compressed, packed), packed, error) &&
                   zip_member_unpack(tools, packed, entry, out, error);
    platform_cell_released(packed, platform_place_g_program_owner);
    // the uncompressed size fits memory, and so fits a long long
    return ok ? (long long)entry->uncompressed : ZIP_ERROR;
}

int zip_span_read(const EngineIngestTools *tools, const char *path, const ZipArchive *archive, const ZipEntry *entries,
                  unsigned long long count, ZipSpan *span, EngineError *error)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = span, .bytes = sizeof(*span), .val = 0);
    if (!ZIP_CHECK((tools != NULL) && (path != NULL) && (archive != NULL) && (entries != NULL) && (count != 0ULL),
                   entries, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    // the span runs from the first member's local header to the end of the last member's data. The last member's
    // local header gives where its data starts, since a local header's extra field need not be the directory's
    unsigned long long first = 0ULL;
    for (unsigned long long at = 1ULL; at < count; at += 1ULL)
    {
        first = (entries[at].local_offset < entries[first].local_offset) ? at : first;
    }
    unsigned long long last = 0ULL;
    for (unsigned long long at = 1ULL; at < count; at += 1ULL)
    {
        last = (entries[at].local_offset > entries[last].local_offset) ? at : last;
    }
    unsigned long long data_start = 0ULL;
    if (!zip_member_data(tools, path, archive, &entries[last], &data_start, error))
    {
        return 0;
    }
    const unsigned long long begin = entries[first].local_offset;
    const unsigned long long end = data_start + entries[last].compressed;
    if (!ZIP_CHECK((end > begin) && zip_fits_memory(end - begin), &entries[last], error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    span->bytes = (unsigned char *)platform_cell_taken((size_t)(end - begin) + 1u, platform_place_g_program_owner, 0);
    if (!ZIP_CHECK(span->bytes != NULL, span, error, ENGINE_ERROR_RESOURCE) ||
        !ZIP_IO(zip_fetch(tools, path, begin, end - begin, span->bytes), span->bytes, error))
    {
        zip_span_release(span);
        return 0;
    }
    span->first = begin;
    span->length = end - begin;
    return 1;
}

long long zip_span_member(const EngineIngestTools *tools, const ZipSpan *span, const ZipEntry *entry,
                          unsigned char *out, unsigned long long capacity, EngineError *error)
{
    if (error == NULL)
    {
        return ZIP_ERROR;
    }
    if (!ZIP_CHECK((tools != NULL) && (span != NULL) && (span->bytes != NULL) && (entry != NULL) && (out != NULL) &&
                       (entry->uncompressed <= capacity) && ((entry->flags & 0x41ULL) == 0ULL) &&
                       ((entry->method == 0ULL) || (entry->method == 8ULL)) && (entry->local_offset >= span->first) &&
                       ((entry->local_offset - span->first) <= span->length) &&
                       ((span->length - (entry->local_offset - span->first)) >= 30ULL),
                   entry, error, ENGINE_ERROR_REQUEST))
    {
        return ZIP_ERROR;
    }
    const unsigned char *const local = span->bytes + (entry->local_offset - span->first);
    if (!ZIP_CHECK(EMBED_CALL(memor.cmp, MemoriaCfg, .src = local, .other = zip_local, .bytes = sizeof zip_local) == 0,
                   local, error, ENGINE_ERROR_REQUEST))
    {
        return ZIP_ERROR;
    }
    const unsigned long long data_at =
        (entry->local_offset - span->first) + 30ULL + zip_load(local + 26u, 2u) + zip_load(local + 28u, 2u);
    if (!ZIP_CHECK((data_at <= span->length) && (entry->compressed <= (span->length - data_at)), local, error,
                   ENGINE_ERROR_REQUEST) ||
        !zip_member_unpack(tools, span->bytes + data_at, entry, out, error))
    {
        return ZIP_ERROR;
    }
    // the uncompressed size fits memory, and so fits a long long
    return (long long)entry->uncompressed;
}

void zip_span_release(ZipSpan *span)
{
    platform_cell_released(span->bytes, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = span, .bytes = sizeof(*span), .val = 0);
}
