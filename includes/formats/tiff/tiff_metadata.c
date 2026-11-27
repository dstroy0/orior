// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tiff_metadata.c: rows, chains, ImageJ and OME metadata
#include "../../../src/cu/engine/engine_note.h"
#include "../../../src/platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "tiff_internal.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int tiff_metadata_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

int tiff_page_rows(TiffFile *file, const TiffPage *page, TiffScratch *scratch, unsigned long long row_first,
                   unsigned long long row_end, unsigned char *out)
{
    if ((file->reason != NULL) || (row_first >= row_end))
    {
        return file->reason == NULL;
    }
    unsigned long long *offsets = NULL;
    unsigned long long *counts = NULL;
    if (!tiff_page_chunks(file, page, &offsets, &counts))
    {
        return file->reason == NULL;
    }
    const unsigned long long element_bytes = page->element_bytes;
    const unsigned long long down_first = row_first / page->chunk_height;
    const unsigned long long down_end = ((row_end - 1ULL) / page->chunk_height) + 1ULL;
    for (unsigned long long down = down_first; (file->reason == NULL) && (down < down_end); down += 1ULL)
    {
        const unsigned long long top = down * page->chunk_height;
        const unsigned long long rows_left = page->height - top;
        const unsigned long long rows = (rows_left < page->chunk_height) ? rows_left : page->chunk_height;
        const unsigned long long copy_first = (row_first > top) ? row_first : top;
        const unsigned long long copy_end = (row_end < (top + rows)) ? row_end : (top + rows);
        for (unsigned long long across = 0ULL; (file->reason == NULL) && (across < page->chunks_across); across += 1ULL)
        {
            const unsigned long long chunk = (down * page->chunks_across) + across;
            if (!tiff_chunk_decode(file, page, scratch, offsets[chunk], counts[chunk], rows))
            {
                break;
            }
            const unsigned long long left = across * page->chunk_width;
            const unsigned long long columns_left = page->width - left;
            const unsigned long long columns = (columns_left < page->chunk_width) ? columns_left : page->chunk_width;
            for (unsigned long long row = copy_first; row < copy_end; row += 1ULL)
            {
                const unsigned char *const source = &scratch->chunk[(row - top) * page->chunk_width * element_bytes];
                unsigned char *const target = &out[(((row - row_first) * page->width) + left) * element_bytes];
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = target, .src = source,
                           .bytes = (size_t)(columns * element_bytes));
            }
        }
    }
    platform_cell_released(offsets, platform_place_g_program_owner);
    platform_cell_released(counts, platform_place_g_program_owner);
    return file->reason == NULL;
}

int tiff_chain_add(TiffFile *file, TiffChain *chain, unsigned long long offset)
{
    if (file->reason != NULL)
    {
        return file->reason == NULL;
    }
    for (unsigned long long seen = 0ULL; (offset <= chain->highest) && (seen < chain->count); seen += 1ULL)
    {
        if (chain->offsets[seen] == offset)
        {
            return tiff_fail(file, "the IFD chain loops back on itself");
        }
    }
    if (chain->count >= file->file_bytes)
    {
        return tiff_fail(file, "more pages than the file can contain");
    }
    if (chain->count == chain->capacity)
    {
        const unsigned long long capacity = (chain->capacity == 0ULL) ? 64ULL : (chain->capacity * 2ULL);
        const unsigned long long bytes = capacity * sizeof(unsigned long long);
        if ((unsigned long long)(size_t)bytes != bytes)
        {
            return tiff_fail(file, "more pages than the file can contain");
        }
        unsigned long long *const grown = (unsigned long long *)platform_cell_grown(
            chain->offsets, (size_t)chain->capacity * sizeof(unsigned long long), (size_t)bytes,
            platform_place_g_program_owner, 0);
        if (grown == NULL)
        {
            return tiff_fail(file, "out of memory");
        }
        chain->offsets = grown;
        chain->capacity = capacity;
    }
    chain->offsets[chain->count] = offset;
    chain->count += 1ULL;
    chain->highest = (offset > chain->highest) ? offset : chain->highest;
    return file->reason == NULL;
}

char *tiff_description(TiffFile *file, const TiffEntry *entry)
{
    if ((file->reason != NULL) || !entry->present)
    {
        return NULL;
    }
    if ((entry->type != 1u) && (entry->type != 2u) && (entry->type != 7u))
    {
        tiff_fail_number(file, "an ImageDescription of type", entry->type, "rather than text");
        return NULL;
    }
    if ((entry->count == 0ULL) || (entry->count > file->file_bytes))
    {
        tiff_fail(file, "an ImageDescription longer than the file");
        return NULL;
    }
    char *const text = (char *)platform_cell_taken((size_t)(entry->count + 1ULL), platform_place_g_program_owner, 0);
    if (text == NULL)
    {
        tiff_fail(file, "out of memory");
        return NULL;
    }
    if (!tiff_entry_bytes(file, entry, entry->count, (unsigned char *)text))
    {
        platform_cell_released(text, platform_place_g_program_owner);
        return NULL;
    }
    text[entry->count] = '\0';
    return text;
}

static int tiff_digits(const char *text, size_t length, unsigned long long *value)
{
    unsigned long long total = 0ULL;
    int fits = (length > 0u);
    for (size_t at = 0u; fits && (at < length); at += 1u)
    {
        // Explicit cast reads the byte as its code point
        const int digit = EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)text[at]);
        const unsigned long long place = digit ? (unsigned long long)(text[at] - '0') : 0ULL;
        fits = digit && (total <= ((~0ULL - place) / 10ULL));
        total = (total * 10ULL) + place;
    }
    *value = total;
    return fits;
}

static void tiff_imagej_value(TiffFile *file, const char *text, const char *key, unsigned long long *value,
                              unsigned int *present)
{
    const size_t key_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (const char *line = text; (line != NULL) && !*present;
         line = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\n'))
    {
        line += (*line == '\n') ? 1u : 0u;
        if ((tiff_metadata_text_order(line, key, (size_t)(key_length)) == 0) && (line[key_length] == '='))
        {
            const char *const digits = &line[key_length + 1u];
            const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = digits,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\r\n");
            *present = 1u;
            if (!tiff_digits(digits, length, value))
            {
                tiff_fail_named(file, "an ImageJ description with a malformed count:", key);
            }
        }
    }
}

void tiff_layout_axis(TiffLayout *layout, char axis, unsigned long long extent, unsigned long long stride)
{
    layout->axes[layout->leading] = axis;
    layout->extent[layout->leading] = extent;
    layout->stride[layout->leading] = stride;
    layout->leading += 1u;
}

int tiff_imagej(TiffFile *file, const char *text, unsigned long long pages, unsigned int pages_known,
                TiffLayout *layout)
{
    unsigned long long images = 1ULL;
    unsigned long long channels = 1ULL;
    unsigned long long slices = 1ULL;
    unsigned long long frames = 1ULL;
    unsigned int images_present = 0u;
    unsigned int channels_present = 0u;
    unsigned int slices_present = 0u;
    unsigned int frames_present = 0u;
    tiff_imagej_value(file, text, "images", &images, &images_present);
    tiff_imagej_value(file, text, "channels", &channels, &channels_present);
    tiff_imagej_value(file, text, "slices", &slices, &slices_present);
    tiff_imagej_value(file, text, "frames", &frames, &frames_present);
    if (file->reason != NULL)
    {
        return file->reason == NULL;
    }
    if ((images == 0ULL) || (channels == 0ULL) || (slices == 0ULL) || (frames == 0ULL))
    {
        return tiff_fail(file, "an ImageJ description with a count of 0");
    }
    unsigned long long frames_by_slices = 0ULL;
    unsigned long long product = 0ULL;
    tiff_multiply(file, frames, slices, &frames_by_slices);
    tiff_multiply(file, frames_by_slices, channels, &product);
    images = images_present ? images : product;
    if ((file->reason == NULL) && (product != images))
    {
        return tiff_fail(file, "the ImageJ frames, slices and channels do not multiply to images");
    }
    if ((file->reason == NULL) && pages_known && (images != pages))
    {
        return tiff_fail(file, "the ImageJ images count is not the number of pages");
    }
    if (frames_present)
    {
        tiff_layout_axis(layout, 't', frames, slices * channels);
    }
    if (slices_present)
    {
        tiff_layout_axis(layout, 'z', slices, channels);
    }
    if (channels > 1ULL)
    {
        tiff_layout_axis(layout, 'c', channels, 1ULL);
    }
    if ((layout->leading == 0u) && (images > 1ULL))
    {
        tiff_layout_axis(layout, '\0', images, 1ULL);
    }
    return file->reason == NULL;
}

static const char *tiff_xml_element(const char *text, const char *name)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (const char *at =
             EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '<');
         at != NULL;
         at = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = at + 1u, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '<'))
    {
        if (tiff_metadata_text_order(at + 1u, name, (size_t)(length)) == 0)
        {
            const char after = at[length + 1u];
            if ((after == ' ') || (after == '\t') || (after == '\r') || (after == '\n') || (after == '>') ||
                (after == '/'))
            {
                return at;
            }
        }
    }
    return NULL;
}

static const char *tiff_xml_attribute(const char *tag, const char *tag_end, const char *name, size_t *length)
{
    const size_t name_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (const char *at = tag + 1u; (size_t)(tag_end - at) > (name_length + 2u); at += 1u)
    {
        const char before = *(at - 1u);
        const int spaced = (before == ' ') || (before == '\t') || (before == '\r') || (before == '\n');
        if (spaced && (tiff_metadata_text_order(at, name, (size_t)(name_length)) == 0) && (at[name_length] == '=') &&
            ((at[name_length + 1u] == '"') || (at[name_length + 1u] == '\'')))
        {
            const char quote = at[name_length + 1u];
            const char *const value = &at[name_length + 2u];
            const char *close = value;
            while ((close < tag_end) && (*close != quote))
            {
                close += 1u;
            }
            if (close >= tag_end)
            {
                return NULL;
            }
            *length = (size_t)(close - value);
            return value;
        }
    }
    return NULL;
}

static int tiff_xml_unsigned(TiffFile *file, const char *tag, const char *tag_end, const char *name,
                             unsigned int required, unsigned long long *value)
{
    size_t length = 0u;
    const char *const text = tiff_xml_attribute(tag, tag_end, name, &length);
    *value = 0ULL;
    if ((text == NULL) && !required)
    {
        return file->reason == NULL;
    }
    if ((text == NULL) || !tiff_digits(text, length, value))
    {
        return tiff_fail_named(file, "an OME-XML attribute missing or not a whole number:", name);
    }
    return file->reason == NULL;
}

static void tiff_ome_candidates(TiffFile *file, const char *text)
{
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "tiff: ");
        mmgr_line_put(&engine_note.line, file->path);
        mmgr_line_put(&engine_note.line,
                      " contains more than one OME image; each is displayed as ID [SizeT SizeZ SizeC SizeY SizeX]:");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    for (const char *pixels = tiff_xml_element(text, "Pixels"); pixels != NULL;
         pixels = tiff_xml_element(pixels + 1u, "Pixels"))
    {
        const char *const tag_end =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = pixels, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '>');
        if (tag_end == NULL)
        {
            break;
        }
        size_t id_length = 0u;
        const char *const id = tiff_xml_attribute(pixels, tag_end, "ID", &id_length);
        const char *const names[5u] = {"SizeT", "SizeZ", "SizeC", "SizeY", "SizeX"};
        unsigned long long sizes[5u] = {0ULL, 0ULL, 0ULL, 0ULL, 0ULL};
        for (unsigned int axis = 0u; axis < 5u; axis += 1u)
        {
            size_t length = 0u;
            const char *const value = tiff_xml_attribute(pixels, tag_end, names[axis], &length);
            if ((value == NULL) || !tiff_digits(value, length, &sizes[axis]))
            {
                sizes[axis] = 0ULL;
            }
        }
        const char *const displayed = (id != NULL) ? id : "?";
        const int displayed_length = (int)((id != NULL) ? id_length : 1u);
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "tiff:   ");
            mmgr_line_put_max(&engine_note.line, displayed, (size_t)(displayed_length));
            mmgr_line_put(&engine_note.line, " [");
            mmgr_line_decimal(&engine_note.line, sizes[0u], 1u);
            mmgr_line_put(&engine_note.line, " ");
            mmgr_line_decimal(&engine_note.line, sizes[1u], 1u);
            mmgr_line_put(&engine_note.line, " ");
            mmgr_line_decimal(&engine_note.line, sizes[2u], 1u);
            mmgr_line_put(&engine_note.line, " ");
            mmgr_line_decimal(&engine_note.line, sizes[3u], 1u);
            mmgr_line_put(&engine_note.line, " ");
            mmgr_line_decimal(&engine_note.line, sizes[4u], 1u);
            mmgr_line_put(&engine_note.line, "]");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
    }
}

int tiff_ome(TiffFile *file, const TiffPage *page, const char *text, unsigned long long pages, unsigned int pages_known,
             TiffLayout *layout)
{
    const char *const pixels = tiff_xml_element(text, "Pixels");
    if (pixels == NULL)
    {
        return tiff_fail(file, "OME-XML with no Pixels element");
    }
    if (tiff_xml_element(pixels + 1u, "Pixels") != NULL)
    {
        tiff_ome_candidates(file, text);
        return tiff_fail(file, "the OME-XML contains more than one image");
    }
    const char *const pixels_end =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = pixels, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '>');
    if (pixels_end == NULL)
    {
        return tiff_fail(file, "OME-XML that ends inside the Pixels element");
    }
    unsigned long long extent_x = 0ULL;
    unsigned long long extent_y = 0ULL;
    unsigned long long extent_z = 0ULL;
    unsigned long long extent_c = 0ULL;
    unsigned long long extent_t = 0ULL;
    tiff_xml_unsigned(file, pixels, pixels_end, "SizeX", 1u, &extent_x);
    tiff_xml_unsigned(file, pixels, pixels_end, "SizeY", 1u, &extent_y);
    tiff_xml_unsigned(file, pixels, pixels_end, "SizeZ", 1u, &extent_z);
    tiff_xml_unsigned(file, pixels, pixels_end, "SizeC", 1u, &extent_c);
    tiff_xml_unsigned(file, pixels, pixels_end, "SizeT", 1u, &extent_t);
    size_t order_length = 0u;
    const char *const order = tiff_xml_attribute(pixels, pixels_end, "DimensionOrder", &order_length);
    if (file->reason != NULL)
    {
        return file->reason == NULL;
    }
    if (extent_c != 1ULL)
    {
        return tiff_fail_number(file, "an OME SizeC of", extent_c, "is not supported; only 1 is");
    }
    if ((extent_x != page->width) || (extent_y != page->height))
    {
        return tiff_fail(file, "the OME SizeX and SizeY are not the page width and height");
    }
    if ((extent_z == 0ULL) || (extent_t == 0ULL))
    {
        return tiff_fail(file, "an OME SizeZ or SizeT of 0");
    }
    if ((order == NULL) || (order_length != 5u) ||
        (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = order, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "XY",
                     .ci = EMBED_FALSE)) ||
        (EMBED_CALL(memor.chr, MemoriaCfg, .src = order, .bytes = 5u, .val = 'Z') == NULL) ||
        (EMBED_CALL(memor.chr, MemoriaCfg, .src = order, .bytes = 5u, .val = 'C') == NULL) ||
        (EMBED_CALL(memor.chr, MemoriaCfg, .src = order, .bytes = 5u, .val = 'T') == NULL))
    {
        return tiff_fail(file, "an OME DimensionOrder that is not XY followed by Z, C and T");
    }
    const size_t z_place =
        (size_t)((const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = order, .bytes = 5u, .val = 'Z') - order);
    const size_t t_place =
        (size_t)((const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = order, .bytes = 5u, .val = 'T') - order);
    const unsigned long long z_stride = (z_place < t_place) ? 1ULL : extent_t;
    const unsigned long long t_stride = (z_place < t_place) ? extent_z : 1ULL;
    unsigned long long planes = 0ULL;
    tiff_multiply(file, extent_z, extent_t, &planes);
    if ((file->reason == NULL) && pages_known && (planes != pages))
    {
        return tiff_fail(file, "the OME SizeZ and SizeT do not multiply to the number of pages");
    }
    const char *const closing =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = pixels_end, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "</Pixels", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    const char *const region_end = (closing != NULL)
                                       ? closing
                                       : (pixels_end + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = pixels_end,
                                                                  .cap = PLATFORM_TEXT_TO_TERMINATOR));
    for (const char *data = tiff_xml_element(pixels_end, "TiffData");
         (file->reason == NULL) && (data != NULL) && (data < region_end);
         data = tiff_xml_element(data + 1u, "TiffData"))
    {
        const char *const data_end =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = data, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '>');
        if (data_end == NULL)
        {
            return tiff_fail(file, "OME-XML that ends inside a TiffData element");
        }
        unsigned long long ifd = 0ULL;
        unsigned long long first_z = 0ULL;
        unsigned long long first_t = 0ULL;
        unsigned long long first_c = 0ULL;
        tiff_xml_unsigned(file, data, data_end, "IFD", 0u, &ifd);
        tiff_xml_unsigned(file, data, data_end, "FirstZ", 0u, &first_z);
        tiff_xml_unsigned(file, data, data_end, "FirstT", 0u, &first_t);
        tiff_xml_unsigned(file, data, data_end, "FirstC", 0u, &first_c);
        if ((file->reason == NULL) && ((first_z >= extent_z) || (first_t >= extent_t) || (first_c != 0ULL) ||
                                       (ifd != ((first_z * z_stride) + (first_t * t_stride)))))
        {
            return tiff_fail(file, "an OME TiffData that maps planes out of DimensionOrder");
        }
    }
    tiff_layout_axis(layout, 't', extent_t, t_stride);
    tiff_layout_axis(layout, 'z', extent_z, z_stride);
    return file->reason == NULL;
}

const char *tiff_ome_start(const char *text)
{
    const char *const element = tiff_xml_element(text, "OME");
    return ((element != NULL) && (element[4u] != '/')) ? element : NULL;
}
