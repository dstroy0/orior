// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// parquet_column.c: one column chunk's pages, decoded to its values' bytes and its levels
#include "parquet_internal.h"

// page types
#define PARQUET_DATA_PAGE 0u
#define PARQUET_DICTIONARY_PAGE 2u
#define PARQUET_DATA_PAGE_V2 3u

// encodings
#define PARQUET_PLAIN 0u
#define PARQUET_PLAIN_DICTIONARY 2u
#define PARQUET_RLE 3u
#define PARQUET_RLE_DICTIONARY 8u

// the widest level or dictionary index the hybrid decoder carries, in bits
#define PARQUET_HYBRID_BITS_MOST 32u

_Static_assert(PARQUET_HYBRID_BITS_MOST <= (sizeof(unsigned int) * CHAR_BIT),
               "PARQUET_HYBRID_BITS_MOST: a decoded level or index must fit the unsigned int it is held in");
_Static_assert((PARQUET_HYBRID_BITS_MOST + 7u) <= 64u,
               "PARQUET_HYBRID_BITS_MOST: a bit-packed value and its 7-bit offset must fit one 64-bit window");

// every field a page header carries that this reader reads
typedef struct
{
    unsigned int page_type;
    unsigned long long uncompressed_bytes;
    unsigned long long compressed_bytes;
    unsigned long long level_count;
    unsigned int encoding;
    unsigned int definition_encoding;
    unsigned int repetition_encoding;
    unsigned long long definition_bytes;
    unsigned long long repetition_bytes;
    unsigned int values_compressed;
} ParquetPageHeader;

// one run of RLE and bit-packed hybrid levels or dictionary indices, and where it decodes to
typedef struct
{
    const unsigned char *bytes;
    unsigned long long byte_count;
    unsigned int bit_width;
    unsigned long long value_count;
    unsigned int *out;
} ParquetHybridRead;

// everything one column chunk's decode carries from page to page. `scratch` holds one page's levels or indices at a
// time, and each page's levels are kept in the column at levels_done
typedef struct
{
    const EngineIngestTools *tools;
    const ParquetLeaf *leaf;
    const ParquetChunk *chunk;
    unsigned long long group_rows;
    unsigned int value_width;
    ParquetColumn *column;
    unsigned int *scratch;
    unsigned char *page_bytes;
    unsigned char *dictionary_bytes;
    unsigned long long *dictionary_offset;
    unsigned long long *dictionary_length;
    unsigned long long dictionary_count;
    unsigned long long levels_done;
} ParquetColumnWalk;

// the stored width of one fixed-width value, or zero for a BYTE_ARRAY and a type this reader does not carry
static unsigned int parquet_value_width(const ParquetLeaf *leaf)
{
    switch (leaf->physical)
    {
    case PARQUET_INT32:
    case PARQUET_FLOAT:
        return 4u;
    case PARQUET_INT64:
    case PARQUET_DOUBLE:
        return 8u;
    case PARQUET_INT96:
        return 12u;
    case PARQUET_FIXED_LEN_BYTE_ARRAY:
        return leaf->fixed_bytes;
    default:
        return 0u;
    }
}

// the bit width of a level whose ceiling is `most`: the fewest bits that hold 0 through most
static unsigned int parquet_level_width(unsigned int most)
{
    unsigned int width = 0u;
    while ((1ull << width) <= most)
    {
        width += 1u;
    }
    return width;
}

// one hybrid run of RLE runs and bit-packed groups into read->out, stopping at read->value_count. Bit-packed values
// are read least significant bit first
static int parquet_hybrid_decode(const ParquetHybridRead *read)
{
    if (read->bit_width > PARQUET_HYBRID_BITS_MOST)
    {
        return 0;
    }
    ParquetCursor cursor = {read->bytes, read->byte_count, 0ull, 0u};
    unsigned long long written = 0ull;
    const unsigned int value_bytes = (read->bit_width + 7u) / 8u;
    const unsigned long long mask = (1ull << read->bit_width) - 1ull;
    while (written < read->value_count)
    {
        const unsigned long long header = parquet_varint_read(&cursor);
        if (cursor.broken != 0u)
        {
            return 0;
        }
        if ((header & 1ull) == 0ull)
        {
            const unsigned long long run = header >> 1u;
            if ((run == 0ull) || ((cursor.byte_count - cursor.position) < value_bytes))
            {
                return 0;
            }
            const unsigned long long value = parquet_little(cursor.bytes + cursor.position, value_bytes);
            cursor.position += value_bytes;
            if (value > mask)
            {
                return 0;
            }
            for (unsigned long long step = 0ull; (step < run) && (written < read->value_count); step += 1ull)
            {
                // the value was held to a mask of at most 32 bits and fits an unsigned int
                read->out[written] = (unsigned int)value;
                written += 1ull;
            }
        }
        else
        {
            const unsigned long long groups = header >> 1u;
            // a group of eight values takes bit_width bytes: at any nonzero width more groups than bytes left do not
            // fit, and refusing them first keeps the product below from wrapping
            if ((groups == 0ull) || ((read->bit_width != 0u) && (groups > (cursor.byte_count - cursor.position))))
            {
                return 0;
            }
            const unsigned long long packed_bytes = groups * read->bit_width;
            if ((cursor.byte_count - cursor.position) < packed_bytes)
            {
                return 0;
            }
            const unsigned long long packed_end = cursor.position + packed_bytes;
            const unsigned long long packed_values = groups * 8ull;
            for (unsigned long long step = 0ull; (step < packed_values) && (written < read->value_count); step += 1ull)
            {
                // a value of at most 32 bits starts within its first byte's low 7 bits; the eight bytes from that
                // byte hold it whole; the window is cut short only at the run's end
                const unsigned long long first_bit = step * read->bit_width;
                const unsigned long long first_byte = cursor.position + (first_bit >> 3u);
                const unsigned long long bytes_left = packed_end - first_byte;
                // bytes_left is below 8 on the narrowing arm and fits an unsigned int
                const unsigned int window_bytes = (bytes_left < 8ull) ? (unsigned int)bytes_left : 8u;
                const unsigned long long window = parquet_little(cursor.bytes + first_byte, window_bytes);
                // the masked value holds bit_width bits, at most 32, and fits an unsigned int
                read->out[written] = (unsigned int)((window >> (first_bit & 7ull)) & mask);
                written += 1ull;
            }
            cursor.position = packed_end;
        }
    }
    return 1;
}

// one page header: its type, sizes, level count and encodings, for all three page kinds
static int parquet_page_header_read(ParquetCursor *cursor, ParquetPageHeader *header)
{
    long long field_id = 0ll;
    memset(header, 0, sizeof(*header));
    header->values_compressed = 1u;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != PARQUET_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1ll) && (kind == PARQUET_THRIFT_I32))
        {
            // a page type is 0 through 3, stored as i32; one past an unsigned int is no page this reader carries
            const unsigned long long page_type = parquet_nonnegative_read(cursor);
            header->page_type = (page_type <= UINT_MAX) ? (unsigned int)page_type : UINT_MAX;
        }
        else if ((field_id == 2ll) && (kind == PARQUET_THRIFT_I32))
        {
            header->uncompressed_bytes = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 3ll) && (kind == PARQUET_THRIFT_I32))
        {
            header->compressed_bytes = parquet_nonnegative_read(cursor);
        }
        else if (((field_id == 5ll) || (field_id == 7ll) || (field_id == 8ll)) && (kind == PARQUET_THRIFT_STRUCT))
        {
            // field 5 is a V1 data page header, 7 a dictionary page header, 8 a V2 data page header
            const long long page_kind = field_id;
            long long inner_id = 0ll;
            for (unsigned int inner = parquet_field_read(cursor, &inner_id); inner != PARQUET_THRIFT_STOP;
                 inner = parquet_field_read(cursor, &inner_id))
            {
                // encodings are 0 through 9, stored as i32; one past an unsigned int is no encoding this reader
                // carries
                const int encoding_field = ((page_kind != 8ll) && (inner_id == 2ll)) ||
                                           ((page_kind == 5ll) && ((inner_id == 3ll) || (inner_id == 4ll))) ||
                                           ((page_kind == 8ll) && (inner_id == 4ll));
                if ((inner_id == 1ll) && (inner == PARQUET_THRIFT_I32))
                {
                    header->level_count = parquet_nonnegative_read(cursor);
                }
                else if (encoding_field && (inner == PARQUET_THRIFT_I32))
                {
                    const unsigned long long read = parquet_nonnegative_read(cursor);
                    const unsigned int encoding = (read <= UINT_MAX) ? (unsigned int)read : UINT_MAX;
                    if ((page_kind == 5ll) && (inner_id == 3ll))
                    {
                        header->definition_encoding = encoding;
                    }
                    else if ((page_kind == 5ll) && (inner_id == 4ll))
                    {
                        header->repetition_encoding = encoding;
                    }
                    else
                    {
                        header->encoding = encoding;
                    }
                }
                else if ((page_kind == 8ll) && (inner_id == 5ll) && (inner == PARQUET_THRIFT_I32))
                {
                    header->definition_bytes = parquet_nonnegative_read(cursor);
                }
                else if ((page_kind == 8ll) && (inner_id == 6ll) && (inner == PARQUET_THRIFT_I32))
                {
                    header->repetition_bytes = parquet_nonnegative_read(cursor);
                }
                else if ((page_kind == 8ll) && (inner_id == 7ll) &&
                         ((inner == PARQUET_THRIFT_TRUE) || (inner == PARQUET_THRIFT_FALSE)))
                {
                    header->values_compressed = (inner == PARQUET_THRIFT_TRUE) ? 1u : 0u;
                }
                else
                {
                    parquet_value_skip(cursor, inner);
                }
                if (cursor->broken != 0u)
                {
                    return 0;
                }
            }
        }
        else
        {
            parquet_value_skip(cursor, kind);
        }
        if (cursor->broken != 0u)
        {
            return 0;
        }
    }
    return 1;
}

// one page body through the chunk's codec, which fills request->out_capacity exactly. Uncompressed, Snappy and Zstd
// are read, through the engine's own decoders
static int parquet_body_inflate(const EngineIngestTools *tools, const EngineBytesRequest *request, ParquetCodec codec)
{
    const int carried = (codec == PARQUET_UNCOMPRESSED) || (codec == PARQUET_SNAPPY) || (codec == PARQUET_ZSTD);
    const EngineCodec engine_codec = (codec == PARQUET_SNAPPY)
                                         ? ENGINE_CODEC_SNAPPY
                                         : ((codec == PARQUET_ZSTD) ? ENGINE_CODEC_ZSTD : ENGINE_CODEC_RAW);
    const EngineBytesDecode decode = carried ? tools->decode[engine_codec] : NULL;
    const long long written = (decode != NULL) ? decode(request) : ENGINE_BYTES_ERROR;
    // a refusal is negative, and only a count of zero or more reaches the comparison as unsigned
    return (written >= 0ll) && ((unsigned long long)written == request->out_capacity);
}

// a dictionary page, decoded into bytes of its own. A BYTE_ARRAY dictionary's entries are kept as the offset past
// each one's four-byte length and the length; a fixed-width one is its entries one after the other
static int parquet_dictionary_page_decode(ParquetColumnWalk *walk, const ParquetPageHeader *header,
                                          const unsigned char *body)
{
    if ((header->encoding != PARQUET_PLAIN) && (header->encoding != PARQUET_PLAIN_DICTIONARY))
    {
        return 0;
    }
    free(walk->dictionary_bytes);
    free(walk->dictionary_offset);
    free(walk->dictionary_length);
    walk->dictionary_offset = NULL;
    walk->dictionary_length = NULL;
    walk->dictionary_count = 0ull;
    walk->dictionary_bytes = (unsigned char *)malloc((size_t)header->uncompressed_bytes + 1u);
    if (walk->dictionary_bytes == NULL)
    {
        return 0;
    }
    const EngineBytesRequest request = {body, header->compressed_bytes, walk->dictionary_bytes,
                                        header->uncompressed_bytes};
    if (!parquet_body_inflate(walk->tools, &request, walk->chunk->codec))
    {
        return 0;
    }
    const unsigned long long entries = header->level_count;
    if (walk->value_width != 0u)
    {
        // a fixed-width dictionary is its entries' bytes and nothing past them; dividing keeps the product unformed
        walk->dictionary_count = entries;
        return entries <= (header->uncompressed_bytes / walk->value_width);
    }
    // every BYTE_ARRAY entry takes at least its four-byte length, which bounds the tables before they are made
    if (entries > (header->uncompressed_bytes / 4ull))
    {
        return 0;
    }
    walk->dictionary_offset = (unsigned long long *)malloc(((size_t)entries + 1u) * sizeof(unsigned long long));
    walk->dictionary_length = (unsigned long long *)malloc(((size_t)entries + 1u) * sizeof(unsigned long long));
    if ((walk->dictionary_offset == NULL) || (walk->dictionary_length == NULL))
    {
        return 0;
    }
    unsigned long long at = 0ull;
    for (unsigned long long entry = 0ull; entry < entries; entry += 1ull)
    {
        if ((header->uncompressed_bytes - at) < 4ull)
        {
            return 0;
        }
        const unsigned long long length = parquet_little(walk->dictionary_bytes + at, 4u);
        at += 4ull;
        if (length > (header->uncompressed_bytes - at))
        {
            return 0;
        }
        walk->dictionary_offset[entry] = at;
        walk->dictionary_length[entry] = length;
        at += length;
    }
    walk->dictionary_count = entries;
    return 1;
}

// one page's levels, decoded into the scratch and kept in `kept` at levels_done, each a byte. A level whose ceiling
// is zero is not stored and decodes to zeros; one past its ceiling is refused
static int parquet_levels_keep(ParquetColumnWalk *walk, const ParquetHybridRead *read, unsigned int most,
                               unsigned char *kept)
{
    unsigned char *const into = kept + walk->levels_done;
    if (most == 0u)
    {
        memset(into, 0, (size_t)read->value_count);
        return 1;
    }
    if (!parquet_hybrid_decode(read))
    {
        return 0;
    }
    for (unsigned long long level = 0ull; level < read->value_count; level += 1ull)
    {
        if (walk->scratch[level] > most)
        {
            return 0;
        }
        // a level at most its ceiling, which the schema's depth holds to a byte (parquet_internal.h)
        into[level] = (unsigned char)walk->scratch[level];
    }
    return 1;
}

// one V1 page's levels at page->position: a four-byte length, then that many bytes of hybrid run
static int parquet_levels_v1_keep(ParquetColumnWalk *walk, ParquetCursor *page, unsigned int most,
                                  unsigned long long level_count, unsigned char *kept)
{
    ParquetHybridRead read = {page->bytes, 0ull, parquet_level_width(most), level_count, walk->scratch};
    if (most != 0u)
    {
        if ((page->byte_count - page->position) < 4ull)
        {
            return 0;
        }
        const unsigned long long length = parquet_little(page->bytes + page->position, 4u);
        page->position += 4ull;
        if (length > (page->byte_count - page->position))
        {
            return 0;
        }
        read.bytes = page->bytes + page->position;
        read.byte_count = length;
        page->position += length;
    }
    return parquet_levels_keep(walk, &read, most, kept);
}

// one value's bytes after the ones held, and a BYTE_ARRAY value's length after the lengths held
static int parquet_value_keep(ParquetColumnWalk *walk, const unsigned char *bytes, unsigned long long length)
{
    unsigned char little[4];
    for (unsigned int place = 0u; place < 4u; place += 1u)
    {
        // a BYTE_ARRAY length is stored as four bytes and was read from them, and each byte is taken whole
        little[place] = (unsigned char)((length >> (8u * place)) & 0xFFull);
    }
    return parquet_bytes_append(&walk->column->values, bytes, length) &&
           ((walk->value_width != 0u) || parquet_bytes_append(&walk->column->lengths, little, 4u));
}

// the present values of one page's value section, PLAIN or dictionary indices
static int parquet_values_decode(ParquetColumnWalk *walk, unsigned int encoding, const ParquetCursor *values,
                                 unsigned long long present)
{
    const unsigned int width = walk->value_width;
    if ((encoding == PARQUET_PLAIN_DICTIONARY) || (encoding == PARQUET_RLE_DICTIONARY))
    {
        if (present == 0ull)
        {
            return 1;
        }
        if (values->byte_count == 0ull)
        {
            return 0;
        }
        // the first byte is the index bit width, and the hybrid decoder refuses one past 32 bits
        const ParquetHybridRead read = {values->bytes + 1u, values->byte_count - 1ull, values->bytes[0], present,
                                        walk->scratch};
        if (!parquet_hybrid_decode(&read))
        {
            return 0;
        }
        for (unsigned long long value = 0ull; value < present; value += 1ull)
        {
            const unsigned long long entry = walk->scratch[value];
            if (entry >= walk->dictionary_count)
            {
                return 0;
            }
            const int kept = (width != 0u)
                                 ? parquet_value_keep(walk, walk->dictionary_bytes + (entry * width), width)
                                 : parquet_value_keep(walk, walk->dictionary_bytes + walk->dictionary_offset[entry],
                                                      walk->dictionary_length[entry]);
            if (!kept)
            {
                return 0;
            }
        }
        return 1;
    }
    if (encoding != PARQUET_PLAIN)
    {
        return 0;
    }
    if (width != 0u)
    {
        // present is at most the chunk's level count, which the page's bytes bound before the product is formed
        return (present <= (values->byte_count / width)) &&
               parquet_bytes_append(&walk->column->values, values->bytes, present * width);
    }
    ParquetCursor plain = *values;
    for (unsigned long long value = 0ull; value < present; value += 1ull)
    {
        if ((plain.byte_count - plain.position) < 4ull)
        {
            return 0;
        }
        const unsigned long long length = parquet_little(plain.bytes + plain.position, 4u);
        plain.position += 4ull;
        if ((length > (plain.byte_count - plain.position)) ||
            !parquet_value_keep(walk, plain.bytes + plain.position, length))
        {
            return 0;
        }
        plain.position += length;
    }
    return 1;
}

// rows and present values from one page's kept levels: a repetition level of zero starts a row, and a definition
// level at its ceiling is a present value. The present count returns through *present
static int parquet_rows_count(ParquetColumnWalk *walk, unsigned long long level_count, unsigned long long *present)
{
    ParquetColumn *const column = walk->column;
    const unsigned char *const repetition = column->repetition + walk->levels_done;
    const unsigned char *const definition = column->definition + walk->levels_done;
    unsigned long long counted = 0ull;
    for (unsigned long long level = 0ull; level < level_count; level += 1ull)
    {
        if (repetition[level] == 0u)
        {
            if (column->rows >= walk->group_rows)
            {
                return 0;
            }
            column->rows += 1ull;
        }
        else if (column->rows == 0ull)
        {
            return 0;
        }
        counted += (definition[level] == walk->leaf->definition_most) ? 1ull : 0ull;
    }
    *present = counted;
    return 1;
}

// one data page, V1 or V2: its levels, its rows and its values
static int parquet_data_page_decode(ParquetColumnWalk *walk, const ParquetPageHeader *header, const unsigned char *body)
{
    if ((header->level_count > (walk->chunk->level_count - walk->levels_done)) ||
        (header->uncompressed_bytes > walk->chunk->uncompressed_bytes))
    {
        return 0;
    }
    ParquetColumn *const column = walk->column;
    ParquetCursor values = {walk->page_bytes, 0ull, 0ull, 0u};
    if (header->page_type == PARQUET_DATA_PAGE)
    {
        const EngineBytesRequest request = {body, header->compressed_bytes, walk->page_bytes,
                                            header->uncompressed_bytes};
        if (((walk->leaf->definition_most != 0u) && (header->definition_encoding != PARQUET_RLE)) ||
            ((walk->leaf->repetition_most != 0u) && (header->repetition_encoding != PARQUET_RLE)) ||
            !parquet_body_inflate(walk->tools, &request, walk->chunk->codec))
        {
            return 0;
        }
        ParquetCursor page = {walk->page_bytes, header->uncompressed_bytes, 0ull, 0u};
        if (!parquet_levels_v1_keep(walk, &page, walk->leaf->repetition_most, header->level_count,
                                    column->repetition) ||
            !parquet_levels_v1_keep(walk, &page, walk->leaf->definition_most, header->level_count, column->definition))
        {
            return 0;
        }
        values.bytes = page.bytes + page.position;
        values.byte_count = page.byte_count - page.position;
    }
    else
    {
        // a V2 page stores its levels uncompressed and unprefixed ahead of the values, which alone are compressed,
        // and only where the header says so
        const unsigned long long level_bytes = header->repetition_bytes + header->definition_bytes;
        if ((header->repetition_bytes > header->compressed_bytes) ||
            (header->definition_bytes > (header->compressed_bytes - header->repetition_bytes)) ||
            (level_bytes > header->uncompressed_bytes))
        {
            return 0;
        }
        const ParquetHybridRead repetition = {body, header->repetition_bytes,
                                              parquet_level_width(walk->leaf->repetition_most), header->level_count,
                                              walk->scratch};
        const ParquetHybridRead definition = {body + header->repetition_bytes, header->definition_bytes,
                                              parquet_level_width(walk->leaf->definition_most), header->level_count,
                                              walk->scratch};
        if (!parquet_levels_keep(walk, &repetition, walk->leaf->repetition_most, column->repetition) ||
            !parquet_levels_keep(walk, &definition, walk->leaf->definition_most, column->definition))
        {
            return 0;
        }
        const EngineBytesRequest request = {body + level_bytes, header->compressed_bytes - level_bytes,
                                            walk->page_bytes, header->uncompressed_bytes - level_bytes};
        const ParquetCodec codec = (header->values_compressed != 0u) ? walk->chunk->codec : PARQUET_UNCOMPRESSED;
        if (!parquet_body_inflate(walk->tools, &request, codec))
        {
            return 0;
        }
        values.byte_count = request.out_capacity;
    }
    unsigned long long present = 0ull;
    if (!parquet_rows_count(walk, header->level_count, &present) ||
        !parquet_values_decode(walk, header->encoding, &values, present))
    {
        return 0;
    }
    column->present += present;
    walk->levels_done += header->level_count;
    return 1;
}

// every page of the chunk held in `chunk_bytes`
static int parquet_pages_decode(ParquetColumnWalk *walk, const unsigned char *chunk_bytes)
{
    ParquetCursor pages = {chunk_bytes, walk->chunk->compressed_bytes, 0ull, 0u};
    while (walk->levels_done < walk->chunk->level_count)
    {
        ParquetPageHeader header;
        if (!parquet_page_header_read(&pages, &header) ||
            (header.compressed_bytes > (pages.byte_count - pages.position)))
        {
            return 0;
        }
        const unsigned char *const body = pages.bytes + pages.position;
        pages.position += header.compressed_bytes;
        if (header.page_type == PARQUET_DICTIONARY_PAGE)
        {
            if (!parquet_dictionary_page_decode(walk, &header, body))
            {
                return 0;
            }
        }
        else if ((header.page_type == PARQUET_DATA_PAGE) || (header.page_type == PARQUET_DATA_PAGE_V2))
        {
            if (!parquet_data_page_decode(walk, &header, body))
            {
                return 0;
            }
        }
    }
    return 1;
}

void parquet_column_release(ParquetColumn *column)
{
    free(column->values.bytes);
    free(column->lengths.bytes);
    free(column->definition);
    free(column->repetition);
    memset(column, 0, sizeof(*column));
}

int parquet_column_read(const EngineIngestTools *tools, const char *path, const ParquetFooter *footer,
                        unsigned int row_group, unsigned int leaf, ParquetColumn *column, EngineError *error)
{
    memset(column, 0, sizeof(*column));
    if (!PARQUET_CHECK((row_group < footer->row_group_count) && (leaf < footer->leaf_count), footer, error,
                       ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    const ParquetLeaf *const schema_leaf = &footer->leaf[leaf];
    const ParquetChunk *const chunk = &footer->row_group[row_group].chunk[leaf];
    ParquetColumnWalk walk;
    memset(&walk, 0, sizeof(walk));
    walk.tools = tools;
    walk.leaf = schema_leaf;
    walk.chunk = chunk;
    walk.group_rows = footer->row_group[row_group].rows;
    walk.value_width = parquet_value_width(schema_leaf);
    walk.column = column;
    if (!PARQUET_CHECK((walk.value_width != 0u) || (schema_leaf->physical == PARQUET_BYTE_ARRAY), schema_leaf, error,
                       ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    // the levels are kept a byte each, the values and lengths grow from the room the chunk states, and a page's
    // levels or indices are decoded a word each into the scratch
    const size_t levels = (size_t)chunk->level_count + 1u;
    unsigned char *const chunk_bytes = (unsigned char *)malloc((size_t)chunk->compressed_bytes + 1u);
    walk.page_bytes = (unsigned char *)malloc((size_t)chunk->uncompressed_bytes + 1u);
    walk.scratch = (unsigned int *)malloc(levels * sizeof(unsigned int));
    column->definition = (unsigned char *)malloc(levels);
    column->repetition = (unsigned char *)malloc(levels);
    column->values.room = chunk->uncompressed_bytes + 1ull;
    column->values.bytes = (unsigned char *)malloc((size_t)column->values.room);
    column->lengths.room = (walk.value_width == 0u) ? ((4ull * chunk->level_count) + 4ull) : 0ull;
    column->lengths.bytes = (walk.value_width == 0u) ? (unsigned char *)malloc((size_t)column->lengths.room) : NULL;
    int ok = PARQUET_CHECK((chunk_bytes != NULL) && (walk.page_bytes != NULL) && (walk.scratch != NULL) &&
                               (column->definition != NULL) && (column->repetition != NULL) &&
                               (column->values.bytes != NULL) &&
                               ((walk.value_width != 0u) || (column->lengths.bytes != NULL)),
                           column, error, ENGINE_ERROR_RESOURCE);
    if (ok)
    {
        EngineFileRange range;
        range.path = path;
        range.offset = chunk->first_byte;
        range.bytes = chunk->compressed_bytes;
        range.out = chunk_bytes;
        // the chunk's compressed bytes were held in memory above, a size that fits a long long
        ok = PARQUET_CHECK(tools->read(&range) == (long long)chunk->compressed_bytes, path, error,
                           ENGINE_ERROR_REQUEST) &&
             PARQUET_CHECK(parquet_pages_decode(&walk, chunk_bytes), chunk, error, ENGINE_ERROR_REQUEST) &&
             PARQUET_CHECK(column->rows == walk.group_rows, &column->rows, error, ENGINE_ERROR_REQUEST);
    }
    column->levels = walk.levels_done;
    free(chunk_bytes);
    free(walk.page_bytes);
    free(walk.scratch);
    free(walk.dictionary_bytes);
    free(walk.dictionary_offset);
    free(walk.dictionary_length);
    if (!ok)
    {
        parquet_column_release(column);
    }
    return ok;
}
