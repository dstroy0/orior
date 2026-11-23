// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tiff_page.c: a page's extent, its chunks and their decoders
#include "../../../src/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "tiff_internal.h"

static const unsigned long long tiff_tag_numbers[TIFF_TAGS] = {256ULL, 257ULL, 258ULL, 259ULL, 266ULL, 270ULL,
                                                               273ULL, 277ULL, 278ULL, 279ULL, 284ULL, 317ULL,
                                                               322ULL, 323ULL, 324ULL, 325ULL, 339ULL};

static int tiff_page_extent(TiffFile *file, TiffPage *page, const TiffEntry *found)
{
    unsigned long long bits = 1ULL;
    unsigned long long fill_order = 1ULL;
    unsigned long long samples = 1ULL;
    unsigned long long rows_per_strip = ~0ULL;
    unsigned long long planar = 1ULL;
    unsigned long long sample_format = 1ULL;
    unsigned long long tile_width = 0ULL;
    unsigned long long tile_length = 0ULL;
    tiff_entry_scalar(file, &found[TIFF_TAG_SAMPLES_PER_PIXEL], 1ULL, &samples);
    if ((file->reason == NULL) && (samples != 1ULL))
    {
        return tiff_fail_number(file, "SamplesPerPixel", samples, "is not supported; only 1 is");
    }
    tiff_entry_scalar(file, &found[TIFF_TAG_WIDTH], 0ULL, &page->width);
    tiff_entry_scalar(file, &found[TIFF_TAG_LENGTH], 0ULL, &page->height);
    tiff_entry_scalar(file, &found[TIFF_TAG_BITS_PER_SAMPLE], 1ULL, &bits);
    tiff_entry_scalar(file, &found[TIFF_TAG_COMPRESSION], 1ULL, &page->compression);
    tiff_entry_scalar(file, &found[TIFF_TAG_FILL_ORDER], 1ULL, &fill_order);
    tiff_entry_scalar(file, &found[TIFF_TAG_ROWS_PER_STRIP], ~0ULL, &rows_per_strip);
    tiff_entry_scalar(file, &found[TIFF_TAG_PLANAR_CONFIGURATION], 1ULL, &planar);
    tiff_entry_scalar(file, &found[TIFF_TAG_PREDICTOR], 1ULL, &page->predictor);
    tiff_entry_scalar(file, &found[TIFF_TAG_SAMPLE_FORMAT], 1ULL, &sample_format);
    tiff_entry_scalar(file, &found[TIFF_TAG_TILE_WIDTH], 0ULL, &tile_width);
    tiff_entry_scalar(file, &found[TIFF_TAG_TILE_LENGTH], 0ULL, &tile_length);
    if (file->reason != NULL)
    {
        return file->reason == NULL;
    }
    if ((page->width == 0ULL) || (page->height == 0ULL))
    {
        return tiff_fail(file, "a page with no ImageWidth or ImageLength");
    }
    if ((bits != 8ULL) && (bits != 16ULL) && (bits != 32ULL))
    {
        return tiff_fail_number(file, "BitsPerSample", bits, "is not supported; only 8, 16 and 32 are");
    }
    if ((sample_format < 1ULL) || (sample_format > 3ULL) || ((sample_format == 3ULL) && (bits == 8ULL)))
    {
        return tiff_fail_number(file, "SampleFormat", sample_format, "is not supported at this BitsPerSample");
    }
    const unsigned long long compression = page->compression;
    if ((compression != 1ULL) && (compression != 5ULL) && (compression != 8ULL) && (compression != 32946ULL) &&
        (compression != 32773ULL) && (compression != 50000ULL))
    {
        return tiff_fail_number(file, "Compression", compression, "is not supported");
    }
    if ((page->predictor != 1ULL) && (page->predictor != 2ULL))
    {
        return tiff_fail_number(file, "Predictor", page->predictor, "is not supported");
    }
    if ((planar != 1ULL) && (planar != 2ULL))
    {
        return tiff_fail_number(file, "PlanarConfiguration", planar, "is not supported");
    }
    if (fill_order != 1ULL)
    {
        return tiff_fail_number(file, "FillOrder", fill_order, "is not supported");
    }
    page->element_bytes = (unsigned int)(bits / 8ULL);
    page->element_type = (sample_format == 1ULL)   ? ENGINE_ELEMENT_UNSIGNED
                         : (sample_format == 2ULL) ? ENGINE_ELEMENT_SIGNED
                                                   : ENGINE_ELEMENT_FLOAT;
    unsigned long long samples_per_page = 0ULL;
    tiff_multiply(file, page->width, page->height, &samples_per_page);
    tiff_multiply(file, samples_per_page, page->element_bytes, &page->page_bytes);
    const unsigned int tiled = found[TIFF_TAG_TILE_WIDTH].present || found[TIFF_TAG_TILE_LENGTH].present ||
                               found[TIFF_TAG_TILE_OFFSETS].present || found[TIFF_TAG_TILE_BYTE_COUNTS].present;
    if (tiled)
    {
        if ((tile_width == 0ULL) || (tile_length == 0ULL) || !found[TIFF_TAG_TILE_OFFSETS].present ||
            !found[TIFF_TAG_TILE_BYTE_COUNTS].present)
        {
            return tiff_fail(file, "a tiled page missing TileWidth, TileLength, TileOffsets or TileByteCounts");
        }
        unsigned long long tile_samples = 0ULL;
        tiff_multiply(file, tile_width, tile_length, &tile_samples);
        if ((file->reason == NULL) && (tile_samples > samples_per_page) && (tile_samples > TIFF_TILE_SAMPLES_MAX))
        {
            return tiff_fail(file, "a tile far larger than its page");
        }
        page->chunk_width = tile_width;
        page->chunk_height = tile_length;
        page->offsets = found[TIFF_TAG_TILE_OFFSETS];
        page->byte_counts = found[TIFF_TAG_TILE_BYTE_COUNTS];
    }
    else
    {
        if (!found[TIFF_TAG_STRIP_OFFSETS].present || !found[TIFF_TAG_STRIP_BYTE_COUNTS].present)
        {
            return tiff_fail(file, "a page missing StripOffsets or StripByteCounts");
        }
        if (rows_per_strip == 0ULL)
        {
            return tiff_fail(file, "a RowsPerStrip of 0");
        }
        page->chunk_width = page->width;
        page->chunk_height = (rows_per_strip < page->height) ? rows_per_strip : page->height;
        page->offsets = found[TIFF_TAG_STRIP_OFFSETS];
        page->byte_counts = found[TIFF_TAG_STRIP_BYTE_COUNTS];
    }
    page->chunks_across = ((page->width - 1ULL) / page->chunk_width) + 1ULL;
    page->chunks_down = ((page->height - 1ULL) / page->chunk_height) + 1ULL;
    unsigned long long chunk_samples = 0ULL;
    unsigned long long chunk_bytes = 0ULL;
    tiff_multiply(file, page->chunk_width, page->chunk_height, &chunk_samples);
    tiff_multiply(file, chunk_samples, page->element_bytes, &chunk_bytes);
    return file->reason == NULL;
}

int tiff_page(TiffFile *file, unsigned long long ifd, TiffPage *page)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = page, .bytes = sizeof(*page), .val = 0);
    if (file->reason != NULL)
    {
        return file->reason == NULL;
    }
    const unsigned int count_bytes = file->bigtiff ? 8u : 2u;
    const unsigned int value_bytes = file->bigtiff ? 8u : 4u;
    const unsigned long long entry_bytes = 4ULL + (2ULL * value_bytes);
    unsigned char head[8u] = {0u};
    if (!tiff_fetch(file, ifd, count_bytes, head))
    {
        return file->reason == NULL;
    }
    const unsigned long long entries = tiff_unpack(file, head, count_bytes);
    if ((entries == 0ULL) || (entries > TIFF_ENTRIES_MAX))
    {
        return tiff_fail(file, "an IFD with no tags, or more tags than any image has");
    }
    const unsigned long long table_bytes = (entries * entry_bytes) + value_bytes;
    unsigned char *const table =
        (unsigned char *)platform_cell_taken((size_t)table_bytes, platform_place_g_program_owner, 0);
    if (table == NULL)
    {
        return tiff_fail(file, "out of memory");
    }
    TiffEntry found[TIFF_TAGS];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = found, .bytes = sizeof(found), .val = 0);
    if (tiff_fetch(file, ifd + count_bytes, table_bytes, table))
    {
        for (unsigned long long entry = 0ULL; entry < entries; entry += 1ULL)
        {
            const unsigned char *const raw = &table[entry * entry_bytes];
            const unsigned long long tag = tiff_unpack(file, raw, 2u);
            const unsigned int type = (unsigned int)tiff_unpack(file, &raw[2u], 2u);
            const unsigned long long count = tiff_unpack(file, &raw[4u], value_bytes);
            const unsigned long long place = tiff_unpack(file, &raw[4u + value_bytes], value_bytes);
            const unsigned long long width = tiff_type_bytes(type);
            const int outside = (width != 0ULL) && (count > (value_bytes / width)) &&
                                ((count > file->file_bytes) || (place > file->file_bytes) ||
                                 ((count * width) > (file->file_bytes - place)));
            if (outside)
            {
                tiff_fail_number(file, "the values of tag", tag, "run past the end of the file");
            }
            for (unsigned int at = 0u; at < (unsigned int)TIFF_TAGS; at += 1u)
            {
                if (tag == tiff_tag_numbers[at])
                {
                    found[at].present = 1u;
                    found[at].type = type;
                    found[at].count = count;
                    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = found[at].value, .src = &raw[4u + value_bytes],
                               .bytes = value_bytes);
                }
            }
        }
        page->next = tiff_unpack(file, &table[entries * entry_bytes], value_bytes);
    }
    platform_cell_released(table, platform_place_g_program_owner);
    page->description = found[TIFF_TAG_DESCRIPTION];
    return tiff_page_extent(file, page, found);
}

int tiff_page_same(const TiffPage *first, const TiffPage *page)
{
    return (first->width == page->width) && (first->height == page->height) &&
           (first->element_bytes == page->element_bytes) && (first->element_type == page->element_type);
}

int tiff_page_chunks(TiffFile *file, const TiffPage *page, unsigned long long **offsets, unsigned long long **counts)
{
    *offsets = NULL;
    *counts = NULL;
    if (file->reason != NULL)
    {
        return file->reason == NULL;
    }
    const unsigned long long chunks = page->chunks_across * page->chunks_down;
    if (chunks > file->file_bytes)
    {
        return tiff_fail(file, "more strips or tiles than the file has bytes");
    }
    *offsets = (unsigned long long *)platform_cell_taken((size_t)(chunks * sizeof(unsigned long long)),
                                                         platform_place_g_program_owner, 0);
    *counts = (unsigned long long *)platform_cell_taken((size_t)(chunks * sizeof(unsigned long long)),
                                                        platform_place_g_program_owner, 0);
    if ((*offsets == NULL) || (*counts == NULL))
    {
        tiff_fail(file, "out of memory");
    }
    tiff_entry_integers(file, &page->offsets, *offsets, chunks);
    tiff_entry_integers(file, &page->byte_counts, *counts, chunks);
    for (unsigned long long chunk = 0ULL; (file->reason == NULL) && (chunk < chunks); chunk += 1ULL)
    {
        const unsigned long long offset = (*offsets)[chunk];
        const unsigned long long bytes = (*counts)[chunk];
        if ((offset > file->file_bytes) || (bytes > (file->file_bytes - offset)))
        {
            tiff_fail(file, "a strip or tile runs past the end of the file");
        }
    }
    if (file->reason != NULL)
    {
        platform_cell_released(*offsets, platform_place_g_program_owner);
        platform_cell_released(*counts, platform_place_g_program_owner);
        *offsets = NULL;
        *counts = NULL;
    }
    return file->reason == NULL;
}

static long long tiff_lzw_decode(const EngineBytesRequest *request)
{
    if ((request->in_bytes >= 2ULL) && (request->in[0u] == 0u) && ((request->in[1u] & 1u) != 0u))
    {
        return ENGINE_BYTES_ERROR;
    }
    unsigned short prefix[TIFF_LZW_CODES];
    unsigned short length[TIFF_LZW_CODES];
    unsigned char suffix[TIFF_LZW_CODES];
    unsigned char head[TIFF_LZW_CODES];
    for (unsigned int code = 0u; code < TIFF_LZW_CODES; code += 1u)
    {
        prefix[code] = 0u;
        length[code] = (unsigned short)((code < TIFF_LZW_CLEAR) ? 1u : 0u);
        suffix[code] = (unsigned char)(code & 0xFFu);
        head[code] = (unsigned char)(code & 0xFFu);
    }
    unsigned long long produced = 0ULL;
    unsigned long long at = 0ULL;
    unsigned long accumulator = 0UL;
    unsigned int accumulated_bits = 0u;
    unsigned int width = TIFF_LZW_WIDTH_FIRST;
    unsigned int next = TIFF_LZW_FIRST;
    unsigned int previous = TIFF_LZW_CODES;
    for (;;)
    {
        while ((accumulated_bits < width) && (at < request->in_bytes))
        {
            accumulator = (accumulator << 8u) | (unsigned long)request->in[at];
            at += 1ULL;
            accumulated_bits += 8u;
        }
        if (accumulated_bits < width)
        {
            break;
        }
        const unsigned int code = (unsigned int)((accumulator >> (accumulated_bits - width)) & ((1UL << width) - 1UL));
        accumulated_bits -= width;
        accumulator &= (1UL << accumulated_bits) - 1UL;
        if (code == TIFF_LZW_CLEAR)
        {
            width = TIFF_LZW_WIDTH_FIRST;
            next = TIFF_LZW_FIRST;
            previous = TIFF_LZW_CODES;
            continue;
        }
        if (code == TIFF_LZW_END)
        {
            break;
        }
        if ((code > next) || ((previous == TIFF_LZW_CODES) && (code >= TIFF_LZW_CLEAR)))
        {
            return ENGINE_BYTES_ERROR;
        }
        if ((previous != TIFF_LZW_CODES) && (next < TIFF_LZW_CODES))
        {
            prefix[next] = (unsigned short)previous;
            suffix[next] = (code < next) ? head[code] : head[previous];
            head[next] = head[previous];
            length[next] = (unsigned short)(length[previous] + 1u);
            next += 1u;
            if ((next >= ((1u << width) - 1u)) && (width < TIFF_LZW_WIDTH_LAST))
            {
                width += 1u;
            }
        }
        const unsigned int size = length[code];
        if (size > (request->out_capacity - produced))
        {
            return ENGINE_BYTES_ERROR;
        }
        unsigned int walk = code;
        for (unsigned int remaining = size; remaining > 0u; remaining -= 1u)
        {
            request->out[produced + remaining - 1u] = suffix[walk];
            walk = prefix[walk];
        }
        produced += size;
        previous = code;
    }
    return (long long)produced;
}

static long long tiff_packbits_decode(const EngineBytesRequest *request)
{
    unsigned long long produced = 0ULL;
    unsigned long long at = 0ULL;
    while (at < request->in_bytes)
    {
        const unsigned int header = request->in[at];
        at += 1ULL;
        if (header < 128u)
        {
            const unsigned long long literal = (unsigned long long)header + 1ULL;
            if ((literal > (request->in_bytes - at)) || (literal > (request->out_capacity - produced)))
            {
                return ENGINE_BYTES_ERROR;
            }
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &request->out[produced], .src = &request->in[at],
                       .bytes = (size_t)literal);
            at += literal;
            produced += literal;
        }
        else if (header > 128u)
        {
            const unsigned long long run = 257ULL - header;
            if ((at >= request->in_bytes) || (run > (request->out_capacity - produced)))
            {
                return ENGINE_BYTES_ERROR;
            }
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &request->out[produced], .bytes = (size_t)run,
                       .val = request->in[at]);
            at += 1ULL;
            produced += run;
        }
    }
    return (long long)produced;
}

static void tiff_swap(unsigned char *chunk, unsigned long long elements, unsigned int element_bytes)
{
    for (unsigned long long element = 0ULL; element < elements; element += 1ULL)
    {
        unsigned char *const bytes = &chunk[element * element_bytes];
        for (unsigned int low = 0u; low < (element_bytes / 2u); low += 1u)
        {
            const unsigned int high = element_bytes - 1u - low;
            const unsigned char swapped = bytes[low];
            bytes[low] = bytes[high];
            bytes[high] = swapped;
        }
    }
}

static void tiff_undo_differencing(unsigned char *chunk, unsigned long long rows, unsigned long long row_samples,
                                   unsigned int element_bytes)
{
    const unsigned long long row_bytes = row_samples * element_bytes;
    for (unsigned long long row = 0ULL; row < rows; row += 1ULL)
    {
        unsigned char *const line = &chunk[row * row_bytes];
        for (unsigned long long sample = element_bytes; sample < row_bytes; sample += element_bytes)
        {
            unsigned int carry = 0u;
            for (unsigned int byte = 0u; byte < element_bytes; byte += 1u)
            {
                const unsigned int sum =
                    (unsigned int)line[sample + byte] + (unsigned int)line[sample - element_bytes + byte] + carry;
                line[sample + byte] = (unsigned char)(sum & 0xFFu);
                carry = sum >> 8u;
            }
        }
    }
}

int tiff_chunk_decode(TiffFile *file, const TiffPage *page, TiffScratch *scratch, unsigned long long offset,
                      unsigned long long bytes, unsigned long long rows)
{
    const unsigned long long capacity = page->chunk_width * page->chunk_height * page->element_bytes;
    const unsigned long long needed = rows * page->chunk_width * page->element_bytes;
    const unsigned long long compression = page->compression;
    long long produced = ENGINE_BYTES_ERROR;
    if (!tiff_reserve(file, &scratch->chunk, &scratch->chunk_capacity, capacity))
    {
        return file->reason == NULL;
    }
    if (compression == 1ULL)
    {
        const unsigned long long taken = (bytes < capacity) ? bytes : capacity;
        produced = tiff_fetch(file, offset, taken, scratch->chunk) ? (long long)taken : ENGINE_BYTES_ERROR;
    }
    else
    {
        const EngineBytesDecode decoder = (compression == 5ULL)       ? tiff_lzw_decode
                                          : (compression == 32773ULL) ? tiff_packbits_decode
                                          : (compression == 50000ULL) ? file->tools->decode[ENGINE_CODEC_ZSTD]
                                                                      : file->tools->decode[ENGINE_CODEC_ZLIB];
        if (decoder == NULL)
        {
            return tiff_fail_number(file, "Compression", compression, "needs a decoder the engine did not supply");
        }
        tiff_reserve(file, &scratch->packed, &scratch->packed_capacity, bytes);
        tiff_fetch(file, offset, bytes, scratch->packed);
        if (file->reason != NULL)
        {
            return file->reason == NULL;
        }
        const EngineBytesRequest request = {scratch->packed, bytes, scratch->chunk, capacity};
        produced = decoder(&request);
    }
    if (file->reason != NULL)
    {
        return file->reason == NULL;
    }
    if ((produced < 0LL) || ((unsigned long long)produced < needed))
    {
        return tiff_fail_number(file, "a strip or tile under Compression", compression, "does not decode to its size");
    }
    if (file->big_endian && (page->element_bytes > 1u))
    {
        tiff_swap(scratch->chunk, rows * page->chunk_width, page->element_bytes);
    }
    if (page->predictor == 2ULL)
    {
        tiff_undo_differencing(scratch->chunk, rows, page->chunk_width, page->element_bytes);
    }
    return file->reason == NULL;
}
