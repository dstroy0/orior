// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// parquet_read.c: a parquet file's members, the one held decoded, and its lanes and side
#include "parquet_internal.h"

#define PARQUET_FOOTER_MEMBER "footer"

// the side leaves of a member, in the order they are kept: a footer has the first alone, a fixed-width leaf the first
// three, a BYTE_ARRAY leaf all four
#define PARQUET_SIDE_LEAVES_MOST 4u

static const char *const PARQUET_SIDE_NAMES[PARQUET_SIDE_LEAVES_MOST] = {"bytes", "definition", "repetition",
                                                                         "lengths"};

// the member held decoded, its file and name, and the footer it was found through. A footer member holds the footer's
// own bytes as its values and no levels
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
    free(resident->path);
    free(resident->member);
    free(resident->footer);
    parquet_column_release(&resident->column);
    memset(resident, 0, sizeof(*resident));
}

// the row group and leaf a member names, `<row group>/<leaf path>` with the row group in decimal and no leading zero
static int parquet_member_find(const ParquetFooter *footer, const char *member, unsigned int *row_group,
                               unsigned int *leaf)
{
    unsigned int group = 0u;
    size_t at = 0u;
    // the loop stops once the count passes the row groups; the count stays far below an unsigned int's ceiling
    while ((member[at] >= '0') && (member[at] <= '9') && (group <= footer->row_group_count))
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
        if (strcmp(footer->leaf[place].path, &member[at + 1u]) == 0)
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
    const size_t size = strlen(text) + 1u;
    char *const copy = (char *)malloc(size);
    if (copy != NULL)
    {
        memcpy(copy, text, size);
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
    if ((resident->valid != 0) && (strcmp(resident->path, path) == 0) && (strcmp(resident->member, member) == 0))
    {
        return 1;
    }
    parquet_resident_release();
    resident->footer = (ParquetFooter *)malloc(sizeof(ParquetFooter));
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
    if (strcmp(member, PARQUET_FOOTER_MEMBER) == 0)
    {
        ParquetBytes *const values = &resident->column.values;
        values->room = resident->footer->footer_bytes + 1ull;
        values->bytes = (unsigned char *)malloc((size_t)values->room);
        ok = PARQUET_CHECK(values->bytes != NULL, values, error, ENGINE_ERROR_RESOURCE);
        if (ok)
        {
            EngineFileRange range;
            range.path = path;
            range.offset = resident->footer->footer_at;
            range.bytes = resident->footer->footer_bytes;
            range.out = values->bytes;
            values->count = resident->footer->footer_bytes;
            // the footer's bytes were held to 16 MiB when it was read, a count that fits a long long
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

// the lanes the held member's bytes take, two bytes to a lane and one lane where it has none
static unsigned long long parquet_lanes(void)
{
    const unsigned long long bytes = s_parquet_resident.column.values.count;
    return (bytes == 0ull) ? 1ull : ((bytes / 2ull) + (bytes % 2ull));
}

static void parquet_extent(EngineArrayExtent *extent)
{
    memset(extent, 0, sizeof(*extent));
    extent->rank = 1u;
    extent->sizes[0] = parquet_lanes();
    extent->axes[0] = 'x';
    extent->element_bytes = 2u;
    extent->element_kind = ENGINE_ELEMENT_UNSIGNED;
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
    free(side->pixel_at);
    free(side->pixel_kept);
    free(side->byte_start);
    free(side->bytes);
    free(side->name_start);
    free(side->names);
    free(side->member_crc);
    free(side->member_bytes);
    memset(side, 0, sizeof(*side));
}

// the held member's side leaves: its byte count eight bytes little-endian, and a leaf's levels and a BYTE_ARRAY
// leaf's lengths as they were decoded. No lane sits inside a side leaf, and each is kept whole
static int parquet_side_fill(EngineSideBytes *side, EngineError *error)
{
    const ParquetResident *const resident = &s_parquet_resident;
    const ParquetColumn *const column = &resident->column;
    unsigned char count[8];
    for (unsigned int place = 0u; place < 8u; place += 1u)
    {
        // one byte of the member's byte count is taken whole
        count[place] = (unsigned char)((column->values.count >> (8u * place)) & 0xFFull);
    }
    const unsigned char *const parts[PARQUET_SIDE_LEAVES_MOST] = {count, column->definition, column->repetition,
                                                                  column->lengths.bytes};
    const unsigned long long sizes[PARQUET_SIDE_LEAVES_MOST] = {8ull, column->levels, column->levels,
                                                                column->lengths.count};
    const int byte_array =
        (resident->leafed != 0) && (resident->footer->leaf[resident->leaf].physical == PARQUET_BYTE_ARRAY);
    const unsigned long long leaves = (resident->leafed == 0) ? 1ull : (byte_array ? 4ull : 3ull);
    unsigned long long kept = 0ull;
    unsigned long long named = 0ull;
    for (unsigned long long leaf = 0ull; leaf < leaves; leaf += 1ull)
    {
        kept += sizes[leaf];
        named += strlen(PARQUET_SIDE_NAMES[leaf]);
    }
    memset(side, 0, sizeof(*side));
    side->pixel_at = (unsigned long long *)calloc((size_t)leaves + 1u, sizeof(unsigned long long));
    side->pixel_kept = (unsigned long long *)calloc((size_t)leaves + 1u, sizeof(unsigned long long));
    side->byte_start = (unsigned long long *)calloc((size_t)leaves + 1u, sizeof(unsigned long long));
    side->bytes = (unsigned char *)malloc((size_t)kept + 1u);
    side->name_start = (unsigned long long *)calloc((size_t)leaves + 1u, sizeof(unsigned long long));
    side->names = (char *)malloc((size_t)named + 1u);
    side->member_crc = (unsigned long long *)calloc((size_t)leaves + 1u, sizeof(unsigned long long));
    side->member_bytes = (unsigned long long *)calloc((size_t)leaves + 1u, sizeof(unsigned long long));
    if (!PARQUET_CHECK((side->pixel_at != NULL) && (side->pixel_kept != NULL) && (side->byte_start != NULL) &&
                           (side->bytes != NULL) && (side->name_start != NULL) && (side->names != NULL) &&
                           (side->member_crc != NULL) && (side->member_bytes != NULL),
                       side, error, ENGINE_ERROR_RESOURCE))
    {
        parquet_side_release(side);
        return 0;
    }
    unsigned long long byte_at = 0ull;
    unsigned long long name_at = 0ull;
    for (unsigned long long leaf = 0ull; leaf < leaves; leaf += 1ull)
    {
        const size_t name_length = strlen(PARQUET_SIDE_NAMES[leaf]);
        side->byte_start[leaf] = byte_at;
        if (sizes[leaf] != 0ull)
        {
            memcpy(side->bytes + byte_at, parts[leaf], (size_t)sizes[leaf]);
        }
        side->member_crc[leaf] = zip_crc32(side->bytes + byte_at, sizes[leaf]);
        side->member_bytes[leaf] = sizes[leaf];
        byte_at += sizes[leaf];
        side->name_start[leaf] = name_at;
        memcpy(side->names + name_at, PARQUET_SIDE_NAMES[leaf], name_length);
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
    const unsigned long long total = 2ull * found.sizes[0];
    const int agrees = (found.rank == request->extent->rank) && (found.sizes[0] == request->extent->sizes[0]) &&
                       (found.element_bytes == request->extent->element_bytes) &&
                       (found.element_kind == request->extent->element_kind) && (request->first == 0ull) &&
                       (request->end == found.sizes[0]) && (total <= request->out_capacity);
    if (!PARQUET_CHECK(agrees, request->extent, error, ENGINE_ERROR_REQUEST))
    {
        parquet_resident_release();
        return -1LL;
    }
    const ParquetBytes *const values = &s_parquet_resident.column.values;
    if (values->count != 0ull)
    {
        memcpy(request->out, values->bytes, (size_t)values->count);
    }
    memset(request->out + values->count, 0, (size_t)(total - values->count));
    const int sided = (request->side == NULL) || parquet_side_fill(request->side, error);
    parquet_resident_release();
    // the total fits the caller's capacity, a size in memory, and so fits a long long
    return sided ? (long long)total : -1LL;
}

unsigned int parquet_members(const EngineIngestTools *tools, const char *path, char ***names)
{
    *names = NULL;
    EngineError probe;
    memset(&probe, 0, sizeof(probe));
    ParquetFooter *const footer = (ParquetFooter *)malloc(sizeof(ParquetFooter));
    if ((footer == NULL) || !parquet_footer_read(tools, path, footer, &probe))
    {
        free(footer);
        return 0u;
    }
    // at most 256 row groups of 64 leaves each and the footer, a count far below an unsigned int's ceiling
    const unsigned int count = 1u + (footer->row_group_count * footer->leaf_count);
    char **const listed = (char **)calloc((size_t)count + 1u, sizeof(char *));
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
            const size_t size = 12u + strlen(footer->leaf[leaf].path);
            listed[at] = (char *)malloc(size);
            ok = (listed[at] != NULL) && (snprintf(listed[at], size, "%u/%s", group, footer->leaf[leaf].path) > 0);
            at += 1u;
        }
    }
    free(footer);
    if (!ok)
    {
        for (unsigned int slot = 0u; (listed != NULL) && (slot < count); slot += 1u)
        {
            free(listed[slot]);
        }
        free(listed);
        return 0u;
    }
    *names = listed;
    return count;
}
