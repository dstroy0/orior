// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// zarr_chunks.c: checksums, keys, codecs and placing a chunk
#include "../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "zarr_internal.h"

static unsigned int zarr_crc32c_step(unsigned int crc, unsigned int byte)
{
    unsigned int carried = crc ^ byte;
    for (unsigned int bit = 0u; bit < 8u; bit += 1u)
    {
        carried = ((carried & 1u) != 0u) ? ((carried >> 1u) ^ 0x82F63B78u) : (carried >> 1u);
    }
    return carried;
}

unsigned int zarr_crc32c(const unsigned char *bytes, unsigned long long count)
{
    static unsigned int table[256];
    static int table_ready = 0;
    if (table_ready == 0)
    {
        for (unsigned int byte = 0u; byte < 256u; byte += 1u)
        {
            table[byte] = zarr_crc32c_step(0u, byte);
        }
        table_ready = 1;
    }
    unsigned int crc = 0xFFFFFFFFu;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        crc = table[(crc ^ bytes[at]) & 0xFFu] ^ (crc >> 8u);
    }
    return crc ^ 0xFFFFFFFFu;
}

unsigned long long zarr_little(const unsigned char *bytes, unsigned int count)
{
    unsigned long long value = 0ULL;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        value |= (unsigned long long)bytes[place] << (8u * place);
    }
    return value;
}

unsigned long long zarr_big(const unsigned char *bytes, unsigned int count)
{
    unsigned long long value = 0ULL;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        value = (value << 8u) | (unsigned long long)bytes[place];
    }
    return value;
}

int zarr_reserve(unsigned char **buffer, unsigned long long *capacity, unsigned long long wanted)
{
    if (wanted <= *capacity)
    {
        return 1;
    }
    unsigned char *const grown =
        (unsigned char *)platform_cell_grown(*buffer, *capacity, (size_t)wanted, platform_place_g_program_owner, 0);
    if (grown == NULL)
    {
        return 0;
    }
    *buffer = grown;
    *capacity = wanted;
    return 1;
}

int zarr_key(const ZarrLayout *layout, const char *root, const unsigned long long *position, unsigned int rank,
             char *path)
{
    VerbaLinea formed_line_1 = mmgr_line_over(path, ZARR_PATH_CAPACITY);
    mmgr_line_put(&formed_line_1, root);
    mmgr_line_put(&formed_line_1, "/");
    (void)mmgr_line_finish(&formed_line_1);
    int written = (int)formed_line_1.at;
    if ((written < 0) || ((unsigned int)written >= ZARR_PATH_CAPACITY))
    {
        return 0;
    }
    size_t at = (size_t)written;
    const char separator = (layout->format == ZARR_FORMAT_N5) ? '/' : layout->separator;
    if (layout->prefixed != 0u)
    {
        VerbaLinea formed_line_2 = mmgr_line_over(&path[at], ZARR_PATH_CAPACITY - at);
        mmgr_line_put(&formed_line_2, "c");
        (void)mmgr_line_finish(&formed_line_2);
        written = (int)formed_line_2.at;
        at += (written > 0) ? (size_t)written : 0u;
    }
    for (unsigned int axis = 0u; axis < rank; axis += 1u)
    {
        const unsigned int named = (layout->format == ZARR_FORMAT_N5) ? (rank - 1u - axis) : axis;
        const int joined = (layout->prefixed != 0u) || (axis != 0u);
        VerbaLinea formed_line_14;
        VerbaLinea formed_line_15;
        written = joined ? (formed_line_14 = mmgr_line_over(&path[at], ZARR_PATH_CAPACITY - at),
                            mmgr_line_ch(&formed_line_14, (char)(separator)),
                            mmgr_line_decimal(&formed_line_14, position[named], 1u),
                            (void)mmgr_line_finish(&formed_line_14), (int)formed_line_14.at)
                         : (formed_line_15 = mmgr_line_over(&path[at], ZARR_PATH_CAPACITY - at),
                            mmgr_line_decimal(&formed_line_15, position[named], 1u),
                            (void)mmgr_line_finish(&formed_line_15), (int)formed_line_15.at);
        if ((written < 0) || ((size_t)written >= (ZARR_PATH_CAPACITY - at)))
        {
            return 0;
        }
        at += (size_t)written;
    }
    if ((rank == 0u) && (layout->prefixed == 0u))
    {
        VerbaLinea formed_line_3 = mmgr_line_over(&path[at], ZARR_PATH_CAPACITY - at);
        mmgr_line_put(&formed_line_3, "0");
        (void)mmgr_line_finish(&formed_line_3);
        written = (int)formed_line_3.at;
        at += (written > 0) ? (size_t)written : 0u;
    }
    return at < ZARR_PATH_CAPACITY;
}

long long zarr_file_read(const ZarrWalk *walk, const char *path, unsigned long long offset, unsigned long long bytes,
                         unsigned char *out)
{
    EngineFileRange range;
    range.path = path;
    range.offset = offset;
    range.bytes = bytes;
    range.out = out;
    return walk->request->tools->read(&range);
}

long long zarr_unchain(const ZarrWalk *walk, const ZarrChain *chain, unsigned char *raw, unsigned long long raw_bytes,
                       unsigned char *out, unsigned long long expected)
{
    unsigned long long data_bytes = raw_bytes;
    if (chain->crc32c != 0u)
    {
        if (data_bytes < 4ULL)
        {
            return ZARR_ERROR;
        }
        data_bytes -= 4ULL;
        if (zarr_crc32c(raw, data_bytes) != (unsigned int)zarr_little(&raw[data_bytes], 4u))
        {
            return ZARR_ERROR;
        }
    }
    if (chain->count == 0u)
    {
        if (data_bytes != expected)
        {
            return ZARR_ERROR;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = out, .src = raw, .bytes = (size_t)expected);
        return (long long)expected;
    }
    if (chain->count != 1u)
    {
        return ZARR_ERROR;
    }
    const EngineBytesDecode decode = walk->request->tools->decode[chain->codec[0]];
    if (decode == NULL)
    {
        return ZARR_ERROR;
    }
    EngineBytesRequest bytes;
    bytes.in = raw;
    bytes.in_bytes = data_bytes;
    bytes.out = out;
    bytes.out_capacity = expected;
    const long long made = decode(&bytes);
    return (made == (long long)expected) ? made : ZARR_ERROR;
}

void zarr_swap(unsigned char *bytes, unsigned long long elements, unsigned int element_bytes)
{
    for (unsigned long long element = 0ULL; element < elements; element += 1ULL)
    {
        unsigned char *const at = &bytes[element * element_bytes];
        for (unsigned int low = 0u; low < (element_bytes / 2u); low += 1u)
        {
            const unsigned char byte = at[low];
            at[low] = at[element_bytes - 1u - low];
            at[element_bytes - 1u - low] = byte;
        }
    }
}

void zarr_place(const ZarrWalk *walk, const unsigned long long *position, const unsigned long long *actual,
                unsigned long long elements)
{
    const ZarrReadRequest *const request = walk->request;
    const ZarrLayout *const layout = request->layout;
    const unsigned int rank = walk->rank;
    const unsigned int element_bytes = walk->element_bytes;
    unsigned long long origin[ENGINE_ARRAY_RANK];
    unsigned long long stored[ENGINE_ARRAY_RANK];
    unsigned int identity = 1u;
    for (unsigned int axis = 0u; axis < rank; axis += 1u)
    {
        origin[axis] = position[axis] * walk->leaf[axis];
        stored[axis] = actual[layout->order[axis]];
        identity &= (unsigned int)(layout->order[axis] == axis);
    }
    unsigned long long counter[ENGINE_ARRAY_RANK];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = counter, .bytes = sizeof(counter), .val = 0);
    unsigned long long source = 0ULL;
    const unsigned long long last = (rank != 0u) ? stored[rank - 1u] : 1ULL;
    const unsigned long long step = (identity != 0u) ? last : 1ULL;
    while (source < elements)
    {
        unsigned long long logical[ENGINE_ARRAY_RANK];
        for (unsigned int axis = 0u; axis < rank; axis += 1u)
        {
            logical[layout->order[axis]] = counter[axis];
        }
        unsigned int inside = 1u;
        unsigned long long target = 0ULL;
        for (unsigned int axis = 0u; axis < rank; axis += 1u)
        {
            const unsigned long long global = origin[axis] + logical[axis];
            const unsigned long long extent =
                (axis == 0u) ? (request->end - request->first) : layout->extent.sizes[axis];
            const unsigned long long shifted = (axis == 0u) ? (global - request->first) : global;
            inside &= (unsigned int)((axis != 0u) || ((global >= request->first) && (global < request->end)));
            inside &= (unsigned int)((axis == 0u) || (global < layout->extent.sizes[axis]));
            target = (target * extent) + ((inside != 0u) ? shifted : 0ULL);
        }
        if (inside != 0u)
        {
            unsigned long long run = step;
            if (identity != 0u)
            {
                const unsigned long long remaining =
                    layout->extent.sizes[rank - 1u] - (origin[rank - 1u] + counter[rank - 1u]);
                run = (run < remaining) ? run : remaining;
            }
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &request->out[target * element_bytes],
                       .src = &walk->chunk[source * element_bytes], .bytes = (size_t)(run * element_bytes));
        }
        source += step;
        for (unsigned int axis = rank; axis > 0u; axis -= 1u)
        {
            counter[axis - 1u] += (axis == rank) ? step : 1ULL;
            if ((counter[axis - 1u] < stored[axis - 1u]) || (axis == 1u))
            {
                break;
            }
            counter[axis - 1u] = 0ULL;
        }
    }
}
