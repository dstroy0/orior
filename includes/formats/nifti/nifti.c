// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "nifti.h"
#include "../../../src/platform/platform.h"
#include "../../../src/cu/engine/engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NIFTI_ONE_HEADER 348ULL
#define NIFTI_TWO_HEADER 540ULL
#define NIFTI_DIMENSIONS 7u
#define NIFTI_BYTES_LIMIT 9223372036854775807ULL
#define NIFTI_GZIP_SMALLEST 18ULL

typedef struct
{
    const EngineIngestTools *tools;
    const char *path;
    unsigned long long length;
    unsigned char *memory;
} NiftiSource;

typedef struct
{
    EngineArrayExtent extent;
    unsigned int big_endian;
    unsigned int paired;
    unsigned int scaled;
    unsigned long long header_bytes;
    unsigned long long data_offset;
    unsigned long long data_bytes;
} NiftiLayout;

typedef struct
{
    unsigned long long code;
    unsigned int element_bytes;
    EngineElementType element_type;
} NiftiType;

static const NiftiType nifti_types[] = {{2ULL, 1u, ENGINE_ELEMENT_UNSIGNED},   {4ULL, 2u, ENGINE_ELEMENT_SIGNED},
                                        {8ULL, 4u, ENGINE_ELEMENT_SIGNED},     {16ULL, 4u, ENGINE_ELEMENT_FLOAT},
                                        {64ULL, 8u, ENGINE_ELEMENT_FLOAT},     {256ULL, 1u, ENGINE_ELEMENT_SIGNED},
                                        {512ULL, 2u, ENGINE_ELEMENT_UNSIGNED}, {768ULL, 4u, ENGINE_ELEMENT_UNSIGNED},
                                        {1024ULL, 8u, ENGINE_ELEMENT_SIGNED},  {1280ULL, 8u, ENGINE_ELEMENT_UNSIGNED}};

static const unsigned char nifti_one_single[4u] = {'n', '+', '1', 0x00u};

static const unsigned char nifti_one_paired[4u] = {'n', 'i', '1', 0x00u};

static const unsigned char nifti_two_single[8u] = {'n', '+', '2', 0x00u, 0x0Du, 0x0Au, 0x1Au, 0x0Au};

static const unsigned char nifti_two_paired[8u] = {'n', 'i', '2', 0x00u, 0x0Du, 0x0Au, 0x1Au, 0x0Au};

static const NiftiSource nifti_empty_source = {NULL, NULL, 0ULL, NULL};

static const NiftiLayout nifti_empty_layout = {
    {0u, {0ULL}, {'\0'}, 0u, ENGINE_ELEMENT_UNSIGNED}, 0u, 0u, 0u, 0ULL, 0ULL, 0ULL};

static unsigned long long nifti_load(const unsigned char *bytes, unsigned int count, unsigned int big_endian)
{
    unsigned long long value = 0ULL;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        const unsigned int at = (big_endian != 0u) ? place : (count - 1u - place);
        value = (value << 8u) | (unsigned long long)bytes[at];
    }
    return value;
}

static unsigned int nifti_fits_memory(unsigned long long bytes)
{
    return ((unsigned long long)(size_t)bytes == bytes) ? 1u : 0u;
}

static unsigned int nifti_multiply(unsigned long long left, unsigned long long right, unsigned long long *product)
{
    if ((left != 0ULL) && (right > (NIFTI_BYTES_LIMIT / left)))
    {
        return 0u;
    }
    *product = left * right;
    return 1u;
}

static unsigned int nifti_file_fetch(const EngineIngestTools *tools, const char *path, unsigned long long offset,
                                     unsigned long long bytes, unsigned char *out)
{
    if (bytes == 0ULL)
    {
        return 1u;
    }
    const EngineFileRange range = {path, offset, bytes, out};
    const long long got = tools->read(&range);
    return ((got >= 0LL) && ((unsigned long long)got == bytes)) ? 1u : 0u;
}

static unsigned int nifti_fetch(const NiftiSource *source, unsigned long long offset, unsigned long long bytes,
                                unsigned char *out)
{
    if ((offset > source->length) || (bytes > (source->length - offset)) || !nifti_fits_memory(bytes))
    {
        return 0u;
    }
    if ((source->memory != NULL) && (bytes != 0ULL))
    {
        mmgr_bytes_copy(out, source->memory + offset, (size_t)bytes);
        return 1u;
    }
    return nifti_file_fetch(source->tools, source->path, offset, bytes, out);
}

static void nifti_swap(unsigned char *bytes, unsigned long long total, unsigned int element_bytes)
{
    for (unsigned long long start = 0ULL; start < total; start += element_bytes)
    {
        for (unsigned int low = 0u; low < (element_bytes / 2u); low += 1u)
        {
            const unsigned int high = element_bytes - 1u - low;
            const unsigned char byte = bytes[start + low];
            bytes[start + low] = bytes[start + high];
            bytes[start + high] = byte;
        }
    }
}

static unsigned int nifti_integer(unsigned long long bits, unsigned long long *value)
{
    const unsigned long long exponent = (bits >> 23u) & 0xFFULL;
    const unsigned long long mantissa = (bits & 0x7FFFFFULL) | 0x800000ULL;
    if ((bits & 0x7FFFFFFFULL) == 0ULL)
    {
        *value = 0ULL;
        return 1u;
    }
    if (((bits >> 31u) != 0ULL) || (exponent < 127ULL) || (exponent > 189ULL))
    {
        return 0u;
    }
    if (exponent >= 150ULL)
    {
        *value = mantissa << (exponent - 150ULL);
        return 1u;
    }
    const unsigned long long shift = 150ULL - exponent;
    if ((mantissa & ((1ULL << shift) - 1ULL)) != 0ULL)
    {
        return 0u;
    }
    *value = mantissa >> shift;
    return 1u;
}

static unsigned int nifti_open(const EngineIngestTools *tools, const char *path, NiftiSource *source)
{
    const long long size = tools->size(path);
    unsigned char opening[2u];
    if ((size < 2LL) || !nifti_file_fetch(tools, path, 0ULL, 2ULL, opening))
    {
        return 0u;
    }
    const unsigned long long file_bytes = (unsigned long long)size;
    if ((opening[0u] != 0x1Fu) || (opening[1u] != 0x8Bu))
    {
        const NiftiSource plain = {tools, path, file_bytes, NULL};
        *source = plain;
        return 1u;
    }
    const EngineBytesDecode decode = tools->decode[ENGINE_CODEC_GZIP];
    unsigned char trailer[4u];
    if ((decode == NULL) || (file_bytes < NIFTI_GZIP_SMALLEST) || !nifti_fits_memory(file_bytes) ||
        !nifti_file_fetch(tools, path, file_bytes - 4ULL, 4ULL, trailer))
    {
        return 0u;
    }
    const unsigned long long expected = nifti_load(trailer, 4u, 0u);
    if ((expected == 0ULL) || !nifti_fits_memory(expected))
    {
        return 0u;
    }
    unsigned char *const packed = platform_cell_taken((size_t)file_bytes, platform_place_g_program_owner, 0);
    unsigned char *const unpacked = platform_cell_taken((size_t)expected, platform_place_g_program_owner, 0);
    int ok = (packed != NULL) && (unpacked != NULL) && nifti_file_fetch(tools, path, 0ULL, file_bytes, packed);
    if (ok)
    {
        const EngineBytesRequest request = {packed, file_bytes, unpacked, expected};
        const long long made = decode(&request);
        ok = (made >= 0LL) && ((unsigned long long)made == expected);
    }
    platform_cell_released(packed, platform_place_g_program_owner);
    if (!ok)
    {
        platform_cell_released(unpacked, platform_place_g_program_owner);
        return 0u;
    }
    const NiftiSource inflated = {tools, path, expected, unpacked};
    *source = inflated;
    return 1u;
}

static unsigned int nifti_header(const unsigned char *bytes, unsigned long long available, NiftiLayout *layout)
{
    if (available < NIFTI_ONE_HEADER)
    {
        return 0u;
    }
    const unsigned long long little = nifti_load(bytes, 4u, 0u);
    const unsigned int big_endian = ((little == NIFTI_ONE_HEADER) || (little == NIFTI_TWO_HEADER)) ? 0u : 1u;
    const unsigned long long header_bytes = nifti_load(bytes, 4u, big_endian);
    if (((header_bytes != NIFTI_ONE_HEADER) && (header_bytes != NIFTI_TWO_HEADER)) || (available < header_bytes))
    {
        return 0u;
    }
    const int second = (header_bytes == NIFTI_TWO_HEADER);
    const int single = second ? (mmgr_bytes_compare(bytes + 4u, nifti_two_single, sizeof nifti_two_single) == 0)
                              : (mmgr_bytes_compare(bytes + 344u, nifti_one_single, sizeof nifti_one_single) == 0);
    const int paired = second ? (mmgr_bytes_compare(bytes + 4u, nifti_two_paired, sizeof nifti_two_paired) == 0)
                              : (mmgr_bytes_compare(bytes + 344u, nifti_one_paired, sizeof nifti_one_paired) == 0);
    if (!single && !paired)
    {
        return 0u;
    }
    const unsigned int dimension_bytes = second ? 8u : 2u;
    const unsigned long long negative = second ? 0x8000000000000000ULL : 0x8000ULL;
    const unsigned char *const dimensions_at = bytes + (second ? 16u : 40u);
    unsigned long long dimensions[NIFTI_DIMENSIONS + 1u];
    for (unsigned int at = 0u; at <= NIFTI_DIMENSIONS; at += 1u)
    {
        dimensions[at] = nifti_load(dimensions_at + (at * dimension_bytes), dimension_bytes, big_endian);
    }
    const unsigned long long rank = dimensions[0u];
    if ((rank < 1ULL) || (rank > NIFTI_DIMENSIONS))
    {
        return 0u;
    }
    const unsigned long long datatype = nifti_load(bytes + (second ? 12u : 70u), 2u, big_endian);
    const unsigned long long bitpix = nifti_load(bytes + (second ? 14u : 72u), 2u, big_endian);
    NiftiLayout built = nifti_empty_layout;
    unsigned int typed = 0u;
    for (size_t entry = 0u; entry < (sizeof nifti_types / sizeof nifti_types[0u]); entry += 1u)
    {
        if (nifti_types[entry].code == datatype)
        {
            built.extent.element_bytes = nifti_types[entry].element_bytes;
            built.extent.element_type = nifti_types[entry].element_type;
            typed = 1u;
        }
    }
    if ((typed == 0u) || (bitpix != (8ULL * built.extent.element_bytes)))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "nifti: datatype ");
            mmgr_line_decimal(&engine_note.line, datatype, 1u);
            mmgr_line_put(&engine_note.line, " with bitpix ");
            mmgr_line_decimal(&engine_note.line, bitpix, 1u);
            mmgr_line_put(&engine_note.line, " errors");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0u;
    }
    unsigned long long data_bytes = built.extent.element_bytes;
    built.extent.rank = (unsigned int)rank;
    for (unsigned int axis = 0u; axis < built.extent.rank; axis += 1u)
    {
        const unsigned int at = built.extent.rank - axis;
        const unsigned long long extent = dimensions[at];
        if ((extent == 0ULL) || (extent >= negative) || !nifti_multiply(data_bytes, extent, &data_bytes))
        {
            return 0u;
        }
        built.extent.sizes[axis] = extent;
        built.extent.axes[axis] = (at == 1u) ? 'x' : (at == 2u) ? 'y' : (at == 3u) ? 'z' : (at == 4u) ? 't' : '\0';
    }
    const unsigned long long wide_offset = nifti_load(bytes + 168u, 8u, big_endian);
    unsigned long long data_offset = wide_offset;
    const int placed = second ? (wide_offset < negative)
                              : (nifti_integer(nifti_load(bytes + 108u, 4u, big_endian), &data_offset) != 0u);
    if (!placed || (single && (data_offset < header_bytes)))
    {
        return 0u;
    }
    const unsigned long long slope =
        second ? nifti_load(bytes + 176u, 8u, big_endian) : nifti_load(bytes + 112u, 4u, big_endian);
    const unsigned long long magnitude = slope & (second ? 0x7FFFFFFFFFFFFFFFULL : 0x7FFFFFFFULL);
    const unsigned long long one = second ? 0x3FF0000000000000ULL : 0x3F800000ULL;
    built.scaled = ((magnitude != 0ULL) && (slope != one)) ? 1u : 0u;
    built.big_endian = ((big_endian != 0u) && (built.extent.element_bytes > 1u)) ? 1u : 0u;
    built.paired = paired ? 1u : 0u;
    built.header_bytes = header_bytes;
    built.data_offset = data_offset;
    built.data_bytes = data_bytes;
    *layout = built;
    return 1u;
}

static unsigned int nifti_character_is(char defined, char lower)
{
    return ((defined == lower) || (defined == (char)(lower - 'a' + 'A'))) ? 1u : 0u;
}

static char *nifti_sibling(const char *path, const char *from, const char *to, unsigned int toggle_gzip)
{
    const size_t length = mmgr_text_length(path, PLATFORM_TEXT_TO_TERMINATOR);
    const int gzipped = (length >= 3u) && (path[length - 3u] == '.') && nifti_character_is(path[length - 2u], 'g') &&
                        nifti_character_is(path[length - 1u], 'z');
    const size_t stem_end = gzipped ? (length - 3u) : length;
    if ((stem_end < 4u) || (path[stem_end - 4u] != '.') || !nifti_character_is(path[stem_end - 3u], from[0u]) ||
        !nifti_character_is(path[stem_end - 2u], from[1u]) || !nifti_character_is(path[stem_end - 1u], from[2u]))
    {
        return NULL;
    }
    const int ends_gzipped = gzipped ? (toggle_gzip == 0u) : (toggle_gzip != 0u);
    char *const sibling = platform_cell_taken(stem_end + 4u, platform_place_g_program_owner, 0);
    if (sibling == NULL)
    {
        return NULL;
    }
    mmgr_bytes_copy(sibling, path, stem_end);
    for (size_t character = 0u; character < 3u; character += 1u)
    {
        const char original = path[stem_end - 3u + character];
        const int upper = (original >= 'A') && (original <= 'Z');
        sibling[stem_end - 3u + character] = upper ? (char)(to[character] - 'a' + 'A') : to[character];
    }
    mmgr_bytes_copy(sibling + stem_end, ends_gzipped ? ".gz" : "", ends_gzipped ? 4u : 1u);
    return sibling;
}

static char *nifti_existing_sibling(const EngineIngestTools *tools, const char *path, const char *from, const char *to)
{
    for (unsigned int toggle = 0u; toggle < 2u; toggle += 1u)
    {
        char *const sibling = nifti_sibling(path, from, to, toggle);
        if ((sibling != NULL) && (tools->size(sibling) >= 0LL))
        {
            return sibling;
        }
        platform_cell_released(sibling, platform_place_g_program_owner);
    }
    return NULL;
}

static unsigned int nifti_prepare(const char *path, const char *member, const EngineIngestTools *tools,
                                  unsigned int noting, NiftiLayout *layout, NiftiSource *data, char **image_path)
{
    *image_path = NULL;
    if (member != NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "nifti: ");
            mmgr_line_put(&engine_note.line, path);
            mmgr_line_put(&engine_note.line, " contains one image; a member was named");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0u;
    }
    char *const header_owned = nifti_existing_sibling(tools, path, "img", "hdr");
    const char *const header_path = (header_owned != NULL) ? header_owned : path;
    NiftiSource header = nifti_empty_source;
    unsigned char bytes[NIFTI_TWO_HEADER];
    NiftiLayout built = nifti_empty_layout;
    const int parsed =
        nifti_open(tools, header_path, &header) &&
        nifti_fetch(&header, 0ULL, (header.length < NIFTI_TWO_HEADER) ? header.length : NIFTI_TWO_HEADER, bytes) &&
        nifti_header(bytes, (header.length < NIFTI_TWO_HEADER) ? header.length : NIFTI_TWO_HEADER, &built);
    if (!parsed || ((built.paired == 0u) && (header_owned != NULL)))
    {
        platform_cell_released(header.memory, platform_place_g_program_owner);
        platform_cell_released(header_owned, platform_place_g_program_owner);
        return 0u;
    }
    if (built.paired == 0u)
    {
        *data = header;
    }
    else
    {
        platform_cell_released(header.memory, platform_place_g_program_owner);
        *image_path = nifti_existing_sibling(tools, header_path, "hdr", "img");
        NiftiSource image = nifti_empty_source;
        if ((*image_path == NULL) || !nifti_open(tools, *image_path, &image))
        {
            platform_cell_released(*image_path, platform_place_g_program_owner);
            *image_path = NULL;
            platform_cell_released(header_owned, platform_place_g_program_owner);
            return 0u;
        }
        *data = image;
    }
    if ((built.data_offset > data->length) || (built.data_bytes > (data->length - built.data_offset)))
    {
        platform_cell_released(data->memory, platform_place_g_program_owner);
        data->memory = NULL;
        platform_cell_released(*image_path, platform_place_g_program_owner);
        *image_path = NULL;
        platform_cell_released(header_owned, platform_place_g_program_owner);
        return 0u;
    }
    if ((noting != 0u) && (built.scaled != 0u))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "nifti: ");
        mmgr_line_put(&engine_note.line, header_path);
        mmgr_line_put(&engine_note.line, " has a scl_slope that is not 1; the raw stored values are read, unscaled");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    platform_cell_released(header_owned, platform_place_g_program_owner);
    *layout = built;
    return 1u;
}

static unsigned int nifti_same_extent(const EngineArrayExtent *found, const EngineArrayExtent *given)
{
    int same = (found->rank == given->rank) && (found->element_bytes == given->element_bytes) &&
               (found->element_type == given->element_type);
    for (unsigned int axis = 0u; same && (axis < found->rank); axis += 1u)
    {
        same = (found->sizes[axis] == given->sizes[axis]) && (found->axes[axis] == given->axes[axis]);
    }
    return same ? 1u : 0u;
}

long nifti_describe(const EngineDescribeRequest *request)
{
    NiftiLayout layout = nifti_empty_layout;
    NiftiSource data = nifti_empty_source;
    char *image_path = NULL;
    if (!nifti_prepare(request->path, request->member, request->tools, 1u, &layout, &data, &image_path))
    {
        return -1L;
    }
    platform_cell_released(data.memory, platform_place_g_program_owner);
    platform_cell_released(image_path, platform_place_g_program_owner);
    *request->extent = layout.extent;
    return 0L;
}

long long nifti_read(const EngineArrayRead *request)
{
    NiftiLayout layout = nifti_empty_layout;
    NiftiSource data = nifti_empty_source;
    char *image_path = NULL;
    if (!nifti_prepare(request->path, request->member, request->tools, 0u, &layout, &data, &image_path))
    {
        return -1LL;
    }
    const unsigned long long row_bytes = layout.data_bytes / layout.extent.sizes[0u];
    const int agrees = nifti_same_extent(&layout.extent, request->extent) && (request->first <= request->end) &&
                       (request->end <= layout.extent.sizes[0u]);
    const unsigned long long wanted = agrees ? ((request->end - request->first) * row_bytes) : 0ULL;
    const int copied = agrees && (wanted <= request->out_capacity) &&
                       nifti_fetch(&data, layout.data_offset + (request->first * row_bytes), wanted, request->out);
    platform_cell_released(data.memory, platform_place_g_program_owner);
    platform_cell_released(image_path, platform_place_g_program_owner);
    if (!copied)
    {
        if (agrees && (wanted <= request->out_capacity) && nifti_fits_memory(wanted))
        {
            mmgr_bytes_set(request->out, 0, (size_t)wanted);
        }
        return -1LL;
    }
    if (layout.big_endian != 0u)
    {
        nifti_swap(request->out, wanted, layout.extent.element_bytes);
    }
    return (long long)wanted;
}
