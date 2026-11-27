// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// parquet_read.c: a parquet file's members, the one cached decoded, and its lanes and side
#include "../../platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "parquet_internal.h"

#define PARQUET_FOOTER_MEMBER "footer"

// the side leaves of a member, in their order of storage. A footer has the first alone, a fixed-width leaf the first
// three, and a BYTE_ARRAY leaf all four
#define PARQUET_SIDE_LEAVES_MAX 4u

static const char *const PARQUET_SIDE_NAMES[PARQUET_SIDE_LEAVES_MAX] = {"bytes", "definition", "repetition", "lengths"};

// the member, kept decoded, its file and name, and the footer that led to it. A footer member contains the
// footer's own bytes as its values and no levels
typedef struct
{
    char *path;
    char *member;
    ParquetFooter *footer;
    unsigned int leaf;
    int leafed;
    ParquetColumn column;
    int valid;
} ParquetResident;

static ParquetResident s_parquet_resident;

static void parquet_resident_release(void)
{
    ParquetResident *const resident = &s_parquet_resident;
    platform_cell_released(resident->path, platform_place_g_program_owner);
    platform_cell_released(resident->member, platform_place_g_program_owner);
    platform_cell_released(resident->footer, platform_place_g_program_owner);
    parquet_column_release(&resident->column);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = resident, .bytes = sizeof(*resident), .val = 0);
}

// the row group and leaf a member names, `<row group>/<leaf path>` with the row group in decimal and no leading zero
static int parquet_member_find(const ParquetFooter *footer, const char *member, unsigned int *row_group,
                               unsigned int *leaf)
{
    unsigned int group = 0u;
    size_t at = 0u;
    // the loop stops once the count passes the row groups; the count stays far below an unsigned int's ceiling.
    // Explicit cast reads the byte as its code point
    while (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)member[at]) &&
           (group <= footer->row_group_count))
    {
        group = (group * 10u) + (unsigned int)(member[at] - '0');
        at += 1u;
    }
    if ((at == 0u) || (member[at] != '/') || (group >= footer->row_group_count) || ((member[0] == '0') && (at > 1u)))
    {
        return 0;
    }
    for (unsigned int place = 0u; place < footer->leaf_count; place += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = footer->leaf[place].path, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = &member[at + 1u], .ci = EMBED_FALSE))
        {
            *row_group = group;
            *leaf = place;
            return 1;
        }
    }
    return 0;
}

static char *parquet_copy(const char *text)
{
    const size_t size = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u;
    char *const copy = (char *)platform_cell_taken(size, platform_place_g_program_owner, 0);
    if (copy != NULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy, .src = text, .bytes = size);
    }
    return copy;
}

static int parquet_open(const EngineIngestTools *tools, const char *path, const char *member, EngineError *error)
{
    ParquetResident *const resident = &s_parquet_resident;
    if (!PARQUET_CHECK((tools != NULL) && (path != NULL) && (member != NULL), &path, error, ENGINE_ERROR_REQUEST))
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
    parquet_resident_release();
    resident->footer = (ParquetFooter *)platform_cell_taken(sizeof(ParquetFooter), platform_place_g_program_owner, 0);
    resident->path = parquet_copy(path);
    resident->member = parquet_copy(member);
    if (!PARQUET_CHECK((resident->footer != NULL) && (resident->path != NULL) && (resident->member != NULL), resident,
                       error, ENGINE_ERROR_RESOURCE) ||
        !parquet_footer_read(tools, path, resident->footer, error))
    {
        parquet_resident_release();
        return 0;
    }
    int ok = 1;
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = member, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = PARQUET_FOOTER_MEMBER, .ci = EMBED_FALSE))
    {
        ParquetBytes *const values = &resident->column.values;
        values->room = resident->footer->footer_bytes + 1ULL;
        values->bytes = (unsigned char *)platform_cell_taken((size_t)values->room, platform_place_g_program_owner, 0);
        ok = PARQUET_CHECK(values->bytes != NULL, values, error, ENGINE_ERROR_RESOURCE);
        if (ok)
        {
            EngineFileRange range;
            range.path = path;
            range.offset = resident->footer->footer_at;
            range.bytes = resident->footer->footer_bytes;
            range.out = values->bytes;
            values->count = resident->footer->footer_bytes;
            // the read of the footer limited its bytes to 16 MiB, a count that fits a long long
            ok = PARQUET_CHECK(tools->read(&range) == (long long)values->count, path, error, ENGINE_ERROR_REQUEST);
        }
    }
    else
    {
        unsigned int row_group = 0u;
        ok = PARQUET_CHECK(parquet_member_find(resident->footer, member, &row_group, &resident->leaf), member, error,
                           ENGINE_ERROR_REQUEST) &&
             parquet_column_read(tools, path, resident->footer, row_group, resident->leaf, &resident->column, error);
        resident->leafed = 1;
    }
    if (!ok)
    {
        parquet_resident_release();
        return 0;
    }
    resident->valid = 1;
    return 1;
}

// the lanes the cached member's bytes take, two bytes to a lane and one lane where it has none
static unsigned long long parquet_lanes(void)
{
    const unsigned long long bytes = s_parquet_resident.column.values.count;
    return (bytes == 0ULL) ? 1ULL : ((bytes / 2ULL) + (bytes % 2ULL));
}

static void parquet_extent(EngineArrayExtent *extent)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = extent, .bytes = sizeof(*extent), .val = 0);
    extent->rank = 1u;
    extent->sizes[0] = parquet_lanes();
    extent->axes[0] = 'x';
    extent->element_bytes = 2u;
    extent->element_type = ENGINE_ELEMENT_UNSIGNED;
}

long parquet_describe(const EngineDescribeRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return -1L;
    }
    if (!PARQUET_CHECK(request->extent != NULL, request, request->error, ENGINE_ERROR_REQUEST) ||
        !parquet_open(request->tools, request->path, request->member, request->error))
    {
        return -1L;
    }
    parquet_extent(request->extent);
    return 0L;
}

static void parquet_side_release(EngineSideBytes *side)
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

// the side leaves of the kept member: its byte count, eight bytes little-endian, and the levels of a leaf. Then the
// lengths of a BYTE_ARRAY leaf, as the decode gave them. No lane sits inside a side leaf, and the code stores each
// whole
static int parquet_side_fill(EngineSideBytes *side, EngineError *error)
{
    const ParquetResident *const resident = &s_parquet_resident;
    const ParquetColumn *const column = &resident->column;
    unsigned char count[8];
    for (unsigned int place = 0u; place < 8u; place += 1u)
    {
        // the code takes one byte of the byte count of the member whole
        count[place] = (unsigned char)((column->values.count >> (8u * place)) & 0xFFULL);
    }
    const unsigned char *const parts[PARQUET_SIDE_LEAVES_MAX] = {count, column->definition, column->repetition,
                                                                 column->lengths.bytes};
    const unsigned long long sizes[PARQUET_SIDE_LEAVES_MAX] = {8ULL, column->levels, column->levels,
                                                               column->lengths.count};
    const int byte_array =
        (resident->leafed != 0) && (resident->footer->leaf[resident->leaf].physical == PARQUET_BYTE_ARRAY);
    const unsigned long long leaves = (resident->leafed == 0) ? 1ULL : (byte_array ? 4ULL : 3ULL);
    unsigned long long byte_total = 0ULL;
    unsigned long long named = 0ULL;
    for (unsigned long long leaf = 0ULL; leaf < leaves; leaf += 1ULL)
    {
        byte_total += sizes[leaf];
        named += EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = PARQUET_SIDE_NAMES[leaf],
                            .cap = PLATFORM_TEXT_TO_TERMINATOR);
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = side, .bytes = sizeof(*side), .val = 0);
    side->pixel_at = (unsigned long long *)platform_cell_cleared((size_t)leaves + 1u, sizeof(unsigned long long),
                                                                 platform_place_g_program_owner, 0);
    side->pixel_stored = (unsigned long long *)platform_cell_cleared((size_t)leaves + 1u, sizeof(unsigned long long),
                                                                     platform_place_g_program_owner, 0);
    side->byte_start = (unsigned long long *)platform_cell_cleared((size_t)leaves + 1u, sizeof(unsigned long long),
                                                                   platform_place_g_program_owner, 0);
    side->bytes = (unsigned char *)platform_cell_taken((size_t)byte_total + 1u, platform_place_g_program_owner, 0);
    side->name_start = (unsigned long long *)platform_cell_cleared((size_t)leaves + 1u, sizeof(unsigned long long),
                                                                   platform_place_g_program_owner, 0);
    side->names = (char *)platform_cell_taken((size_t)named + 1u, platform_place_g_program_owner, 0);
    side->member_crc = (unsigned long long *)platform_cell_cleared((size_t)leaves + 1u, sizeof(unsigned long long),
                                                                   platform_place_g_program_owner, 0);
    side->member_bytes = (unsigned long long *)platform_cell_cleared((size_t)leaves + 1u, sizeof(unsigned long long),
                                                                     platform_place_g_program_owner, 0);
    if (!PARQUET_CHECK((side->pixel_at != NULL) && (side->pixel_stored != NULL) && (side->byte_start != NULL) &&
                           (side->bytes != NULL) && (side->name_start != NULL) && (side->names != NULL) &&
                           (side->member_crc != NULL) && (side->member_bytes != NULL),
                       side, error, ENGINE_ERROR_RESOURCE))
    {
        parquet_side_release(side);
        return 0;
    }
    unsigned long long byte_at = 0ULL;
    unsigned long long name_at = 0ULL;
    for (unsigned long long leaf = 0ULL; leaf < leaves; leaf += 1ULL)
    {
        const size_t name_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = PARQUET_SIDE_NAMES[leaf],
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR);
        side->byte_start[leaf] = byte_at;
        if (sizes[leaf] != 0ULL)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = side->bytes + byte_at, .src = parts[leaf],
                       .bytes = (size_t)sizes[leaf]);
        }
        side->member_crc[leaf] = zip_crc32(side->bytes + byte_at, sizes[leaf]);
        side->member_bytes[leaf] = sizes[leaf];
        byte_at += sizes[leaf];
        side->name_start[leaf] = name_at;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = side->names + name_at, .src = PARQUET_SIDE_NAMES[leaf],
                   .bytes = name_length);
        name_at += name_length;
    }
    side->byte_start[leaves] = byte_at;
    side->name_start[leaves] = name_at;
    side->leaves = leaves;
    return 1;
}

long long parquet_read(const EngineArrayRead *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return -1LL;
    }
    EngineError *const error = request->error;
    if (!PARQUET_CHECK((request->extent != NULL) && (request->out != NULL), request, error, ENGINE_ERROR_REQUEST) ||
        !parquet_open(request->tools, request->path, request->member, error))
    {
        return -1LL;
    }
    EngineArrayExtent found;
    parquet_extent(&found);
    const unsigned long long total = 2ULL * found.sizes[0];
    const int agrees = (found.rank == request->extent->rank) && (found.sizes[0] == request->extent->sizes[0]) &&
                       (found.element_bytes == request->extent->element_bytes) &&
                       (found.element_type == request->extent->element_type) && (request->first == 0ULL) &&
                       (request->end == found.sizes[0]) && (total <= request->out_capacity);
    if (!PARQUET_CHECK(agrees, request->extent, error, ENGINE_ERROR_REQUEST))
    {
        parquet_resident_release();
        return -1LL;
    }
    const ParquetBytes *const values = &s_parquet_resident.column.values;
    if (values->count != 0ULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request->out, .src = values->bytes, .bytes = (size_t)values->count);
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request->out + values->count, .bytes = (size_t)(total - values->count),
               .val = 0);
    const int sided = (request->side == NULL) || parquet_side_fill(request->side, error);
    parquet_resident_release();
    // the total fits the caller's capacity, a size in memory, and so fits a long long
    return sided ? (long long)total : -1LL;
}

unsigned int parquet_members(const EngineIngestTools *tools, const char *path, char ***names)
{
    *names = NULL;
    EngineError probe;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &probe, .bytes = sizeof(probe), .val = 0);
    ParquetFooter *const footer =
        (ParquetFooter *)platform_cell_taken(sizeof(ParquetFooter), platform_place_g_program_owner, 0);
    if ((footer == NULL) || !parquet_footer_read(tools, path, footer, &probe))
    {
        platform_cell_released(footer, platform_place_g_program_owner);
        return 0u;
    }
    // at max 256 row groups of 64 leaves each and the footer, a count far below an unsigned int's ceiling
    const unsigned int count = 1u + (footer->row_group_count * footer->leaf_count);
    char **const listed =
        (char **)platform_cell_cleared((size_t)count + 1u, sizeof(char *), platform_place_g_program_owner, 0);
    int ok = (listed != NULL);
    if (ok)
    {
        listed[0] = parquet_copy(PARQUET_FOOTER_MEMBER);
        ok = (listed[0] != NULL);
    }
    unsigned int at = 1u;
    for (unsigned int group = 0u; ok && (group < footer->row_group_count); group += 1u)
    {
        for (unsigned int leaf = 0u; ok && (leaf < footer->leaf_count); leaf += 1u)
        {
            // a row group's decimal, a slash, the leaf's path and its terminator
            const size_t size = 12u + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = footer->leaf[leaf].path,
                                                 .cap = PLATFORM_TEXT_TO_TERMINATOR);
            listed[at] = (char *)platform_cell_taken(size, platform_place_g_program_owner, 0);
            VerbaLinea formed_line_1;
            ok = (listed[at] != NULL) &&
                 ((formed_line_1 = mmgr_line_over(listed[at], size), mmgr_line_decimal(&formed_line_1, group, 1u),
                   mmgr_line_put(&formed_line_1, "/"), mmgr_line_put(&formed_line_1, footer->leaf[leaf].path),
                   (void)mmgr_line_finish(&formed_line_1), (int)formed_line_1.at) > 0);
            at += 1u;
        }
    }
    platform_cell_released(footer, platform_place_g_program_owner);
    if (!ok)
    {
        for (unsigned int index = 0u; (listed != NULL) && (index < count); index += 1u)
        {
            platform_cell_released(listed[index], platform_place_g_program_owner);
        }
        platform_cell_released(listed, platform_place_g_program_owner);
        return 0u;
    }
    *names = listed;
    return count;
}
