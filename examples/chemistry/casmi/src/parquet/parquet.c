// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file parquet.c
 * @brief Parquet footer and column chunk decoding over the engine's own codecs.
 *
 * Carries what the competition's files use and refuses the rest: PLAIN, PLAIN_DICTIONARY and
 * RLE_DICTIONARY values; RLE and bit-packed hybrid levels; data pages V1 and V2; uncompressed,
 * Snappy and Zstd bodies. A refusal is a return of ENGINE_BYTES_ERROR, never a partial column.
 */
#include "casmi_config.h"

#include "parquet.h"
#include "snappy.h"
#include "zstd.h"

/** The four bytes that open and close a parquet file. */
#define CASMI_PARQUET_MAGIC "PAR1"
#define CASMI_PARQUET_MAGIC_BYTES 4u

_Static_assert(CASMI_PARQUET_MAGIC_BYTES == (sizeof(CASMI_PARQUET_MAGIC) - 1u),
               "CASMI_PARQUET_MAGIC_BYTES: must count the bytes of CASMI_PARQUET_MAGIC without its terminator");

/** The trailer: a little-endian footer length and the closing magic. */
#define CASMI_PARQUET_TRAILER_BYTES 8u

/** Thrift compact protocol type codes. A struct field's code 0 is the stop byte. */
#define CASMI_THRIFT_STOP 0u
#define CASMI_THRIFT_TRUE 1u
#define CASMI_THRIFT_FALSE 2u
#define CASMI_THRIFT_BYTE 3u
#define CASMI_THRIFT_I16 4u
#define CASMI_THRIFT_I32 5u
#define CASMI_THRIFT_I64 6u
#define CASMI_THRIFT_DOUBLE 7u
#define CASMI_THRIFT_BINARY 8u
#define CASMI_THRIFT_LIST 9u
#define CASMI_THRIFT_SET 10u
#define CASMI_THRIFT_MAP 11u
#define CASMI_THRIFT_STRUCT 12u

/** The longest a varint may run: ten groups of seven bits cover sixty-four. */
#define CASMI_THRIFT_VARINT_BYTES_MOST 10u

_Static_assert((7u * CASMI_THRIFT_VARINT_BYTES_MOST) >= 64u,
               "CASMI_THRIFT_VARINT_BYTES_MOST: its seven-bit groups must cover a 64-bit value");
_Static_assert((7u * (CASMI_THRIFT_VARINT_BYTES_MOST - 1u)) < 64u,
               "CASMI_THRIFT_VARINT_BYTES_MOST: the last group's shift must stay below the 64-bit width");

/**
 * The bits of a varint's last group that land inside sixty-four: 64 - 7 x 9 = 1. The two asserts above
 * hold it to 1 through 7; a last group carrying a bit at or above it encodes more than 2^64 - 1.
 */
#define CASMI_THRIFT_VARINT_LAST_GROUP_BITS (64u - (7u * (CASMI_THRIFT_VARINT_BYTES_MOST - 1u)))

/** A Thrift field id is an i16: the least and the most a field header may carry. */
#define CASMI_THRIFT_FIELD_ID_LEAST (-32768ll)
#define CASMI_THRIFT_FIELD_ID_MOST 32767ll

/** Schema repetition types. */
#define CASMI_PARQUET_REQUIRED 0u
#define CASMI_PARQUET_REPEATED 2u

/** Page types. */
#define CASMI_PARQUET_DATA_PAGE 0u
#define CASMI_PARQUET_DICTIONARY_PAGE 2u
#define CASMI_PARQUET_DATA_PAGE_V2 3u

/** Encodings. */
#define CASMI_PARQUET_PLAIN 0u
#define CASMI_PARQUET_PLAIN_DICTIONARY 2u
#define CASMI_PARQUET_RLE 3u
#define CASMI_PARQUET_RLE_DICTIONARY 8u

/** The widest level or dictionary index the hybrid decoder carries, in bits. */
#define CASMI_PARQUET_HYBRID_BITS_MOST 32u

_Static_assert(CASMI_PARQUET_HYBRID_BITS_MOST <= (sizeof(unsigned int) * CHAR_BIT),
               "CASMI_PARQUET_HYBRID_BITS_MOST: a decoded level or index must fit the unsigned int it is stored in");
_Static_assert((CASMI_PARQUET_HYBRID_BITS_MOST + 7u) <= 64u,
               "CASMI_PARQUET_HYBRID_BITS_MOST: a bit-packed value plus its 7-bit offset must fit one 64-bit window");

/** A cursor over bytes held in memory. Any overrun sets broken, and every later read returns zero. */
typedef struct
{
    const unsigned char *bytes;
    unsigned long long byte_count;
    unsigned long long position;
    unsigned int broken;
} ParquetCursor;

/** One open container of a Thrift value being skipped: a struct's fields, or a list's or map's values. */
typedef struct
{
    unsigned int container_kind;
    unsigned long long values_left;
    unsigned int key_kind;
    unsigned int value_kind;
    long long field_id;
} ParquetThriftFrame;

/** One schema element's parent, as the preorder walk of the schema holds it. */
typedef struct
{
    unsigned long long children_left;
    unsigned int definition_most;
    unsigned int repetition_most;
    unsigned long long path_bytes;
} ParquetSchemaFrame;

/** A byte range of a file and where it lands. */
typedef struct
{
    const char *path;
    unsigned long long first_byte;
    unsigned long long byte_count;
    unsigned char *out;
} ParquetFileRange;

/** Every field a page header can carry that this reader reads. */
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

/** One RLE and bit-packed hybrid run of levels or dictionary indices, and where it decodes to. */
typedef struct
{
    const unsigned char *bytes;
    unsigned long long byte_count;
    unsigned int bit_width;
    unsigned long long value_count;
    unsigned int *out;
} ParquetHybridRead;

/** Everything one column chunk's decode carries between pages. */
typedef struct
{
    const CasmiParquetColumnRead *args;
    const CasmiParquetLeaf *leaf;
    const CasmiParquetChunk *chunk;
    unsigned int *repetition_level;
    unsigned int *definition_level;
    unsigned int *dictionary_index;
    unsigned long long *dictionary_offset;
    unsigned long long *dictionary_length;
    unsigned long long dictionary_count;
    unsigned char *chunk_bytes;
    unsigned char *page_bytes;
    unsigned char *dictionary_bytes;
    unsigned long long levels_done;
    unsigned long long rows_done;
    unsigned long long values_done;
    unsigned long long value_bytes_used;
} ParquetColumnWalk;

/** The next byte, or zero with broken set where none is left. */
CASMI_ALWAYS_INLINE unsigned int parquet_byte_read(ParquetCursor *cursor)
{
    if (cursor->broken || (cursor->position >= cursor->byte_count))
    {
        cursor->broken = 1u;
        return 0u;
    }
    const unsigned int byte = cursor->bytes[cursor->position];
    cursor->position += 1ull;
    return byte;
}

/**
 * An unsigned LEB128 varint of at most sixty-four bits. One that runs past CASMI_THRIFT_VARINT_BYTES_MOST
 * bytes, or whose last group carries a bit at or above CASMI_THRIFT_VARINT_LAST_GROUP_BITS, encodes more
 * than 2^64 - 1 and breaks the cursor.
 */
CASMI_ALWAYS_INLINE unsigned long long parquet_varint_read(ParquetCursor *cursor)
{
    unsigned long long value = 0ull;
    for (unsigned int group = 0u; group < CASMI_THRIFT_VARINT_BYTES_MOST; group += 1u)
    {
        const unsigned int byte = parquet_byte_read(cursor);
        const unsigned int group_bits = byte & 0x7Fu;
        if ((group == (CASMI_THRIFT_VARINT_BYTES_MOST - 1u)) && ((group_bits >> CASMI_THRIFT_VARINT_LAST_GROUP_BITS) != 0u))
        {
            cursor->broken = 1u;
            return 0ull;
        }
        // Widened to 64 bits before the shift, which reaches 63; the last group was held above to the bits
        // that land below bit 64; no bit is lost.
        value |= (unsigned long long)group_bits << (7u * group);
        if ((byte & 0x80u) == 0u)
        {
            return value;
        }
    }
    cursor->broken = 1u;
    return 0ull;
}

/** A zigzag varint: the signed form every Thrift compact integer takes. */
CASMI_ALWAYS_INLINE long long parquet_zigzag_read(ParquetCursor *cursor)
{
    const unsigned long long stored = parquet_varint_read(cursor);
    // The bits above the sign bit are below 2^63; the conversion to long long is exact. The low
    // bit alone decides the sign: set, the value is -value_bits - 1, which reaches -2^63 at most.
    const long long value_bits = (long long)(stored >> 1u);
    return ((stored & 1ull) != 0ull) ? (-value_bits - 1ll) : value_bits;
}

/** A zigzag integer that a count, size or offset field holds, refused where negative. */
CASMI_ALWAYS_INLINE unsigned long long parquet_nonnegative_integer_read(ParquetCursor *cursor)
{
    const long long value = parquet_zigzag_read(cursor);
    if (value < 0ll)
    {
        cursor->broken = 1u;
        return 0ull;
    }
    // Negative values were refused above; the conversion is exact.
    return (unsigned long long)value;
}

/** A length-prefixed byte string, returned in place with its length through *length. */
CASMI_ALWAYS_INLINE const unsigned char *parquet_binary_read(ParquetCursor *cursor, unsigned long long *length)
{
    *length = parquet_varint_read(cursor);
    if (cursor->broken || (*length > (cursor->byte_count - cursor->position)))
    {
        cursor->broken = 1u;
        *length = 0ull;
        return cursor->bytes;
    }
    const unsigned char *const start = cursor->bytes + cursor->position;
    cursor->position += *length;
    return start;
}

/** A list or set header: the element count, with the element type through *element_kind. */
CASMI_ALWAYS_INLINE unsigned long long parquet_list_header_read(ParquetCursor *cursor, unsigned int *element_kind)
{
    const unsigned int header = parquet_byte_read(cursor);
    *element_kind = header & 0x0Fu;
    const unsigned long long short_count = header >> 4u;
    return (short_count == 15ull) ? parquet_varint_read(cursor) : short_count;
}

/**
 * The next field of a struct: its id through *field_id and its type as the return, where
 * CASMI_THRIFT_STOP ends the struct. A short header carries the id as a delta from the last; a long
 * one carries it whole. Either way the id is an i16: one outside CASMI_THRIFT_FIELD_ID_LEAST through
 * CASMI_THRIFT_FIELD_ID_MOST breaks the cursor, leaves *field_id as it was and ends the struct.
 */
CASMI_ALWAYS_INLINE unsigned int parquet_field_read(ParquetCursor *cursor, long long *field_id)
{
    const unsigned int header = parquet_byte_read(cursor);
    if (header == CASMI_THRIFT_STOP)
    {
        return CASMI_THRIFT_STOP;
    }
    const unsigned int delta = header >> 4u;
    // A delta is at most 15; its widening to long long is exact. Every caller starts *field_id at
    // zero and only this function writes it; the last id is an i16 and the sum cannot overflow.
    const long long next_id = (delta == 0u) ? parquet_zigzag_read(cursor) : (*field_id + (long long)delta);
    if ((next_id < CASMI_THRIFT_FIELD_ID_LEAST) || (next_id > CASMI_THRIFT_FIELD_ID_MOST))
    {
        cursor->broken = 1u;
        return CASMI_THRIFT_STOP;
    }
    *field_id = next_id;
    return header & 0x0Fu;
}

/** Skip one value that holds no other value. A kind that is not a scalar breaks the cursor. */
CASMI_ALWAYS_INLINE void parquet_scalar_skip(ParquetCursor *cursor, unsigned int kind)
{
    switch (kind)
    {
        case CASMI_THRIFT_TRUE:
        case CASMI_THRIFT_FALSE:
        {
            // A struct field's boolean is carried in its type code and occupies no byte.
            break;
        }
        case CASMI_THRIFT_BYTE:
        {
            parquet_byte_read(cursor);
            break;
        }
        case CASMI_THRIFT_I16:
        case CASMI_THRIFT_I32:
        case CASMI_THRIFT_I64:
        {
            parquet_varint_read(cursor);
            break;
        }
        case CASMI_THRIFT_DOUBLE:
        {
            cursor->broken |= ((cursor->byte_count - cursor->position) < 8ull) ? 1u : 0u;
            cursor->position += cursor->broken ? 0ull : 8ull;
            break;
        }
        case CASMI_THRIFT_BINARY:
        {
            unsigned long long length = 0ull;
            parquet_binary_read(cursor, &length);
            break;
        }
        default:
        {
            cursor->broken = 1u;
            break;
        }
    }
}

/**
 * Skip one value of any kind, nested containers included, without recursion: each open struct, list
 * or map is a frame, and a value nested deeper than CASMI_THRIFT_DEPTH_MOST breaks the cursor.
 */
CASMI_ALWAYS_INLINE void parquet_value_skip(ParquetCursor *cursor, unsigned int kind)
{
    ParquetThriftFrame frame[CASMI_THRIFT_DEPTH_MOST];
    unsigned int depth = 0u;
    unsigned int next_kind = kind;
    while (!cursor->broken)
    {
        const int opens_container = (next_kind == CASMI_THRIFT_STRUCT) || (next_kind == CASMI_THRIFT_LIST) ||
                                    (next_kind == CASMI_THRIFT_SET) || (next_kind == CASMI_THRIFT_MAP);
        if (!opens_container)
        {
            parquet_scalar_skip(cursor, next_kind);
        }
        else if (depth == CASMI_THRIFT_DEPTH_MOST)
        {
            cursor->broken = 1u;
            return;
        }
        else
        {
            ParquetThriftFrame *const opened_frame = &frame[depth];
            opened_frame->container_kind = (next_kind == CASMI_THRIFT_SET) ? CASMI_THRIFT_LIST : next_kind;
            opened_frame->values_left = 0ull;
            opened_frame->key_kind = 0u;
            opened_frame->value_kind = 0u;
            opened_frame->field_id = 0ll;
            if (opened_frame->container_kind == CASMI_THRIFT_LIST)
            {
                opened_frame->values_left = parquet_list_header_read(cursor, &opened_frame->value_kind);
                opened_frame->key_kind = opened_frame->value_kind;
            }
            else if (opened_frame->container_kind == CASMI_THRIFT_MAP)
            {
                const unsigned long long entry_count = parquet_varint_read(cursor);
                const unsigned int map_kind_byte = (entry_count != 0ull) ? parquet_byte_read(cursor) : 0u;
                // Every entry takes at least one byte; more entries than bytes left is refused, and
                // the doubling to count keys and values apart cannot wrap.
                cursor->broken |= (entry_count > (cursor->byte_count - cursor->position)) ? 1u : 0u;
                opened_frame->values_left = cursor->broken ? 0ull : (2ull * entry_count);
                opened_frame->key_kind = map_kind_byte >> 4u;
                opened_frame->value_kind = map_kind_byte & 0x0Fu;
            }
            depth += 1u;
        }
        // The next value to skip is the innermost open container's next one; each container that has
        // none left closes, and when none is open the whole value has been skipped.
        next_kind = CASMI_THRIFT_STOP;
        while ((depth != 0u) && (next_kind == CASMI_THRIFT_STOP) && !cursor->broken)
        {
            ParquetThriftFrame *const innermost_frame = &frame[depth - 1u];
            if (innermost_frame->container_kind == CASMI_THRIFT_STRUCT)
            {
                next_kind = parquet_field_read(cursor, &innermost_frame->field_id);
            }
            else if (innermost_frame->values_left != 0ull)
            {
                innermost_frame->values_left -= 1ull;
                // A map counts down from an even total; an odd count left means a key comes next.
                next_kind = ((innermost_frame->values_left & 1ull) != 0ull) ? innermost_frame->key_kind : innermost_frame->value_kind;
                // A boolean inside a list or map is one byte, not a type code carrying the value.
                next_kind = ((next_kind == CASMI_THRIFT_TRUE) || (next_kind == CASMI_THRIFT_FALSE)) ? CASMI_THRIFT_BYTE : next_kind;
                cursor->broken |= (next_kind == CASMI_THRIFT_STOP) ? 1u : 0u;
            }
            if ((next_kind == CASMI_THRIFT_STOP) && !cursor->broken)
            {
                depth -= 1u;
            }
        }
        if (next_kind == CASMI_THRIFT_STOP)
        {
            return;
        }
    }
}

/** Read one byte range of a file into range->out. */
CASMI_ALWAYS_INLINE int parquet_range_read(const ParquetFileRange *range)
{
    FILE *const file = fopen(range->path, "rb");
    if (file == NULL)
    {
        return 0;
    }
#if CASMI_HAVE_WINDOWS_SEEK
    // _fseeki64 takes a signed 64-bit offset; no parquet file reaches 2^63 bytes.
    const int seek = _fseeki64(file, (long long)range->first_byte, SEEK_SET);
#else
    // long is 64 bits here, as casmi_config.h asserts; no parquet file reaches 2^63 bytes.
    const int seek = fseek(file, (long)range->first_byte, SEEK_SET);
#endif
    const unsigned long long got = (seek == 0) ? fread(range->out, 1u, range->byte_count, file) : 0ull;
    fclose(file);
    return (seek == 0) && (got == range->byte_count);
}

/** A file's size in bytes, or ENGINE_BYTES_ERROR where it cannot be opened or measured. */
CASMI_ALWAYS_INLINE long long parquet_file_size_read(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return ENGINE_BYTES_ERROR;
    }
#if CASMI_HAVE_WINDOWS_SEEK
    const int seek = _fseeki64(file, 0ll, SEEK_END);
    const long long size = (seek == 0) ? _ftelli64(file) : ENGINE_BYTES_ERROR;
#else
    const int seek = fseek(file, 0l, SEEK_END);
    // long is 64 bits here, as casmi_config.h asserts; the widening to long long is exact.
    const long long size = (seek == 0) ? (long long)ftell(file) : ENGINE_BYTES_ERROR;
#endif
    fclose(file);
    return size;
}

/** An unsigned integer stored little-endian in count bytes, count at most eight. */
CASMI_ALWAYS_INLINE unsigned long long parquet_little_endian_read(const unsigned char *bytes, unsigned int count)
{
    unsigned long long value = 0ull;
    for (unsigned int byte = 0u; byte < count; byte += 1u)
    {
        value |= (unsigned long long)bytes[byte] << (8u * byte);
    }
    return value;
}

/** The stored width of one fixed-width value, or zero for a type this reader does not carry. */
CASMI_ALWAYS_INLINE unsigned int parquet_value_width_measure(const CasmiParquetLeaf *leaf)
{
    switch (leaf->physical)
    {
        case CASMI_PARQUET_INT32:
        case CASMI_PARQUET_FLOAT:
        {
            return 4u;
        }
        case CASMI_PARQUET_INT64:
        case CASMI_PARQUET_DOUBLE:
        {
            return 8u;
        }
        case CASMI_PARQUET_INT96:
        {
            return 12u;
        }
        case CASMI_PARQUET_FIXED_LEN_BYTE_ARRAY:
        {
            return leaf->fixed_bytes;
        }
        default:
        {
            return 0u;
        }
    }
}

/**
 * Read one schema element and place it: the root opens the walk, an element with children opens a
 * frame, and an element without children becomes the footer's next leaf.
 */
CASMI_ALWAYS_INLINE int parquet_schema_element_read(ParquetCursor *cursor, CasmiParquetFooter *footer,
                                                    ParquetSchemaFrame *stack, unsigned int *depth, char *path)
{
    long long field_id = 0ll;
    unsigned long long physical = 0ull;
    unsigned long long fixed_bytes = 0ull;
    unsigned long long repetition_type = CASMI_PARQUET_REQUIRED;
    unsigned long long children = 0ull;
    const unsigned char *name = NULL;
    unsigned long long name_bytes = 0ull;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != CASMI_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1ll) && (kind == CASMI_THRIFT_I32))
        {
            physical = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 2ll) && (kind == CASMI_THRIFT_I32))
        {
            fixed_bytes = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 3ll) && (kind == CASMI_THRIFT_I32))
        {
            repetition_type = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 4ll) && (kind == CASMI_THRIFT_BINARY))
        {
            name = parquet_binary_read(cursor, &name_bytes);
        }
        else if ((field_id == 5ll) && (kind == CASMI_THRIFT_I32))
        {
            children = parquet_nonnegative_integer_read(cursor);
        }
        else
        {
            parquet_value_skip(cursor, kind);
        }
        if (cursor->broken)
        {
            return 0;
        }
    }
    // The root is the first element and names no column; its children open the walk.
    if (*depth == 0u)
    {
        stack[0].children_left = children;
        stack[0].definition_most = 0u;
        stack[0].repetition_most = 0u;
        stack[0].path_bytes = 0ull;
        *depth = 1u;
        return children != 0ull;
    }
    ParquetSchemaFrame *const parent = &stack[*depth - 1u];
    if (parent->children_left == 0ull)
    {
        return 0;
    }
    parent->children_left -= 1ull;
    const unsigned int definition = parent->definition_most + ((repetition_type != CASMI_PARQUET_REQUIRED) ? 1u : 0u);
    const unsigned int repetition = parent->repetition_most + ((repetition_type == CASMI_PARQUET_REPEATED) ? 1u : 0u);
    const unsigned long long separator = (parent->path_bytes == 0ull) ? 0ull : 1ull;
    const unsigned long long path_bytes = parent->path_bytes + separator + name_bytes;
    if ((name == NULL) || (path_bytes >= CASMI_PARQUET_PATH_BYTES))
    {
        return 0;
    }
    if (separator != 0ull)
    {
        path[parent->path_bytes] = '.';
    }
    memcpy(path + parent->path_bytes + separator, name, name_bytes);
    path[path_bytes] = '\0';
    if (children == 0ull)
    {
        if (footer->leaf_count >= CASMI_PARQUET_LEAVES_MOST)
        {
            return 0;
        }
        CasmiParquetLeaf *const leaf = &footer->leaf[footer->leaf_count];
        memcpy(leaf->path, path, path_bytes + 1ull);
        // A physical type is 0 through 7 and a fixed width is a few bytes; the writer stored both as
        // i32; neither narrowing loses a value the format allows. An unknown type is refused when
        // its column is bounded.
        leaf->physical = (CasmiParquetPhysical)physical;
        leaf->fixed_bytes = (unsigned int)fixed_bytes;
        leaf->definition_most = definition;
        leaf->repetition_most = repetition;
        footer->leaf_count += 1u;
    }
    else
    {
        if (*depth >= CASMI_PARQUET_SCHEMA_DEPTH_MOST)
        {
            return 0;
        }
        stack[*depth].children_left = children;
        stack[*depth].definition_most = definition;
        stack[*depth].repetition_most = repetition;
        stack[*depth].path_bytes = path_bytes;
        *depth += 1u;
    }
    while ((*depth > 1u) && (stack[*depth - 1u].children_left == 0ull))
    {
        *depth -= 1u;
    }
    return 1;
}

/** Read the schema list into the footer's leaves, each with its dotted path and level ceilings. */
CASMI_ALWAYS_INLINE int parquet_schema_read(ParquetCursor *cursor, CasmiParquetFooter *footer)
{
    unsigned int element_kind = 0u;
    const unsigned long long count = parquet_list_header_read(cursor, &element_kind);
    if (element_kind != CASMI_THRIFT_STRUCT)
    {
        return 0;
    }
    ParquetSchemaFrame stack[CASMI_PARQUET_SCHEMA_DEPTH_MOST];
    char path[CASMI_PARQUET_PATH_BYTES];
    unsigned int depth = 0u;
    for (unsigned long long element = 0ull; element < count; element += 1ull)
    {
        if (!parquet_schema_element_read(cursor, footer, stack, &depth, path))
        {
            return 0;
        }
    }
    return !cursor->broken;
}

/** Whether a chunk's path_in_schema list names the same dotted path as leaf_path. */
CASMI_ALWAYS_INLINE int parquet_path_match(ParquetCursor *cursor, const char *leaf_path)
{
    unsigned int element_kind = 0u;
    const unsigned long long parts = parquet_list_header_read(cursor, &element_kind);
    if (element_kind != CASMI_THRIFT_BINARY)
    {
        return 0;
    }
    unsigned long long at = 0ull;
    int matches = 1;
    for (unsigned long long part = 0ull; part < parts; part += 1ull)
    {
        unsigned long long length = 0ull;
        const unsigned char *const name = parquet_binary_read(cursor, &length);
        if (part != 0ull)
        {
            matches = matches && (leaf_path[at] == '.');
            at += 1ull;
        }
        matches = matches && ((at + length) < CASMI_PARQUET_PATH_BYTES) && (memcmp(leaf_path + at, name, length) == 0);
        at += length;
    }
    return matches && !cursor->broken && (at < CASMI_PARQUET_PATH_BYTES) && (leaf_path[at] == '\0');
}

/** Read one ColumnMetaData: the codec, the counts, the sizes and the chunk's first byte. */
CASMI_ALWAYS_INLINE int parquet_column_metadata_read(ParquetCursor *cursor, const CasmiParquetLeaf *leaf,
                                                     CasmiParquetChunk *chunk)
{
    long long field_id = 0ll;
    unsigned long long data_page_offset = 0ull;
    unsigned long long dictionary_page_offset = 0ull;
    int path_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != CASMI_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 3ll) && (kind == CASMI_THRIFT_LIST))
        {
            if (!parquet_path_match(cursor, leaf->path))
            {
                return 0;
            }
            path_seen = 1;
        }
        else if ((field_id == 4ll) && (kind == CASMI_THRIFT_I32))
        {
            // A codec is 0 through 7, stored as i32; an unknown one is refused when a page is decoded.
            chunk->codec = (CasmiParquetCodec)parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 5ll) && (kind == CASMI_THRIFT_I64))
        {
            chunk->level_count = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 6ll) && (kind == CASMI_THRIFT_I64))
        {
            chunk->uncompressed_bytes = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 7ll) && (kind == CASMI_THRIFT_I64))
        {
            chunk->compressed_bytes = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 9ll) && (kind == CASMI_THRIFT_I64))
        {
            data_page_offset = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 11ll) && (kind == CASMI_THRIFT_I64))
        {
            dictionary_page_offset = parquet_nonnegative_integer_read(cursor);
        }
        else
        {
            parquet_value_skip(cursor, kind);
        }
        if (cursor->broken)
        {
            return 0;
        }
    }
    // A writer that stores no dictionary may still write the field as zero; a dictionary offset
    // counts only where it is nonzero and ahead of the first data page.
    const int has_dictionary = (dictionary_page_offset != 0ull) && (dictionary_page_offset < data_page_offset);
    chunk->first_byte = has_dictionary ? dictionary_page_offset : data_page_offset;
    return path_seen;
}

/** Read one ColumnChunk, which must carry its ColumnMetaData. */
CASMI_ALWAYS_INLINE int parquet_column_chunk_read(ParquetCursor *cursor, const CasmiParquetLeaf *leaf,
                                                  CasmiParquetChunk *chunk)
{
    long long field_id = 0ll;
    int metadata_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != CASMI_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 3ll) && (kind == CASMI_THRIFT_STRUCT))
        {
            if (!parquet_column_metadata_read(cursor, leaf, chunk))
            {
                return 0;
            }
            metadata_seen = 1;
        }
        else
        {
            parquet_value_skip(cursor, kind);
        }
        if (cursor->broken)
        {
            return 0;
        }
    }
    return metadata_seen;
}

/** Read one RowGroup: its row count and one chunk per leaf, in leaf order. */
CASMI_ALWAYS_INLINE int parquet_row_group_read(ParquetCursor *cursor, const CasmiParquetFooter *footer,
                                               CasmiParquetRowGroup *row_group)
{
    long long field_id = 0ll;
    int columns_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != CASMI_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1ll) && (kind == CASMI_THRIFT_LIST))
        {
            unsigned int element_kind = 0u;
            const unsigned long long count = parquet_list_header_read(cursor, &element_kind);
            if ((element_kind != CASMI_THRIFT_STRUCT) || (count != footer->leaf_count))
            {
                return 0;
            }
            for (unsigned int leaf = 0u; leaf < footer->leaf_count; leaf += 1u)
            {
                if (!parquet_column_chunk_read(cursor, &footer->leaf[leaf], &row_group->chunk[leaf]))
                {
                    return 0;
                }
            }
            columns_seen = 1;
        }
        else if ((field_id == 3ll) && (kind == CASMI_THRIFT_I64))
        {
            row_group->rows = parquet_nonnegative_integer_read(cursor);
        }
        else
        {
            parquet_value_skip(cursor, kind);
        }
        if (cursor->broken)
        {
            return 0;
        }
    }
    return columns_seen;
}

/** Read the FileMetaData: the schema first, then the row count and the row groups. */
CASMI_ALWAYS_INLINE int parquet_file_metadata_read(ParquetCursor *cursor, CasmiParquetFooter *footer)
{
    long long field_id = 0ll;
    int schema_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != CASMI_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 2ll) && (kind == CASMI_THRIFT_LIST))
        {
            if (!parquet_schema_read(cursor, footer))
            {
                return 0;
            }
            schema_seen = 1;
        }
        else if ((field_id == 3ll) && (kind == CASMI_THRIFT_I64))
        {
            footer->rows = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 4ll) && (kind == CASMI_THRIFT_LIST) && schema_seen)
        {
            unsigned int element_kind = 0u;
            const unsigned long long count = parquet_list_header_read(cursor, &element_kind);
            if ((element_kind != CASMI_THRIFT_STRUCT) || (count > CASMI_PARQUET_ROW_GROUPS_MOST))
            {
                return 0;
            }
            for (unsigned long long group = 0ull; group < count; group += 1ull)
            {
                if (!parquet_row_group_read(cursor, footer, &footer->row_group[group]))
                {
                    return 0;
                }
            }
            // count was bounded by CASMI_PARQUET_ROW_GROUPS_MOST above; it fits unsigned int.
            footer->row_group_count = (unsigned int)count;
        }
        else
        {
            parquet_value_skip(cursor, kind);
        }
        if (cursor->broken)
        {
            return 0;
        }
    }
    return schema_seen;
}

/** The backend of casmi_parquet_footer_read. */
CASMI_ALWAYS_INLINE long long parquet_footer_decode(const CasmiParquetFooterRead *args)
{
    const long long file_bytes = parquet_file_size_read(args->path);
    // The sum is twelve; its conversion to long long is exact.
    if (file_bytes < (long long)(CASMI_PARQUET_MAGIC_BYTES + CASMI_PARQUET_TRAILER_BYTES))
    {
        return ENGINE_BYTES_ERROR;
    }
    // file_bytes was checked positive above; the conversion is exact.
    const unsigned long long file_size = (unsigned long long)file_bytes;
    unsigned char trailer[CASMI_PARQUET_TRAILER_BYTES];
    const ParquetFileRange trailer_range = {args->path, file_size - CASMI_PARQUET_TRAILER_BYTES, CASMI_PARQUET_TRAILER_BYTES, trailer};
    if (!parquet_range_read(&trailer_range) ||
        (memcmp(trailer + CASMI_PARQUET_MAGIC_BYTES, CASMI_PARQUET_MAGIC, CASMI_PARQUET_MAGIC_BYTES) != 0))
    {
        return ENGINE_BYTES_ERROR;
    }
    const unsigned long long footer_bytes = parquet_little_endian_read(trailer, 4u);
    if ((footer_bytes > CASMI_PARQUET_FOOTER_BYTES_MOST) ||
        (footer_bytes > (file_size - CASMI_PARQUET_MAGIC_BYTES - CASMI_PARQUET_TRAILER_BYTES)))
    {
        return ENGINE_BYTES_ERROR;
    }
    unsigned char *const footer_text = malloc(footer_bytes);
    if (footer_text == NULL)
    {
        return ENGINE_BYTES_ERROR;
    }
    // The footer is decoded into a local copy first; a refusal leaves the caller's footer untouched.
    CasmiParquetFooter *const decoded = malloc(sizeof(*decoded));
    const ParquetFileRange footer_range = {args->path, file_size - CASMI_PARQUET_TRAILER_BYTES - footer_bytes, footer_bytes, footer_text};
    ParquetCursor cursor = {footer_text, footer_bytes, 0ull, 0u};
    int good = (decoded != NULL) && parquet_range_read(&footer_range);
    if (good)
    {
        memset(decoded, 0, sizeof(*decoded));
        good = parquet_file_metadata_read(&cursor, decoded) && !cursor.broken;
    }
    if (good)
    {
        memcpy(args->footer, decoded, sizeof(*decoded));
    }
    free(decoded);
    free(footer_text);
    // leaf_count is at most CASMI_PARQUET_LEAVES_MOST; it fits long long.
    return good ? (long long)args->footer->leaf_count : ENGINE_BYTES_ERROR;
}

/** The backend of casmi_parquet_leaf_find. */
CASMI_ALWAYS_INLINE long long parquet_leaf_search(const CasmiParquetLeafFind *args)
{
    for (unsigned int leaf = 0u; leaf < args->footer->leaf_count; leaf += 1u)
    {
        if (strcmp(args->footer->leaf[leaf].path, args->path_in_schema) == 0)
        {
            // leaf is below CASMI_PARQUET_LEAVES_MOST; it fits long long.
            return (long long)leaf;
        }
    }
    return ENGINE_BYTES_ERROR;
}

/**
 * Set rooms->fixed_bytes and the four rooms one chunk's read needs, from the footer alone. Nothing is
 * written where the row group, the leaf or the leaf's type is refused.
 */
CASMI_ALWAYS_INLINE int parquet_column_rooms_measure(const CasmiParquetColumnRead *args, CasmiParquetColumn *rooms)
{
    if ((args->row_group >= args->footer->row_group_count) || (args->leaf >= args->footer->leaf_count))
    {
        return 0;
    }
    const CasmiParquetLeaf *const leaf = &args->footer->leaf[args->leaf];
    const CasmiParquetChunk *const chunk = &args->footer->row_group[args->row_group].chunk[args->leaf];
    const unsigned int value_width = parquet_value_width_measure(leaf);
    const int variable = (leaf->physical == CASMI_PARQUET_BYTE_ARRAY);
    if ((value_width == 0u) && !variable)
    {
        return 0;
    }
    rooms->fixed_bytes = value_width;
    rooms->row_start_room = args->footer->row_group[args->row_group].rows + 1ull;
    rooms->value_offset_room = variable ? (chunk->level_count + 1ull) : 0ull;
    rooms->value_bytes_room = variable ? chunk->uncompressed_bytes : (chunk->level_count * value_width);
    // Two dictionary tables of eight bytes per entry, three level or index tables of four bytes per
    // level, the compressed chunk, one uncompressed page and one uncompressed dictionary page.
    rooms->scratch_room = (16ull * (chunk->level_count + 1ull)) + (12ull * chunk->level_count) +
                          chunk->compressed_bytes + (2ull * chunk->uncompressed_bytes);
    return 1;
}

/** The backend of casmi_parquet_column_bound. */
CASMI_ALWAYS_INLINE long long parquet_column_measure(const CasmiParquetColumnRead *args)
{
    // A column's scratch room is bounded by the chunk's stated sizes, far below 2^63; the
    // conversion to long long is exact.
    return parquet_column_rooms_measure(args, args->column) ? (long long)args->column->scratch_room : ENGINE_BYTES_ERROR;
}

/**
 * Decode one hybrid run of RLE runs and bit-packed groups into read->out, stopping at read->value_count.
 * Bit-packed values are read least significant bit first.
 */
CASMI_ALWAYS_INLINE int parquet_hybrid_decode(const ParquetHybridRead *read)
{
    if (read->bit_width > CASMI_PARQUET_HYBRID_BITS_MOST)
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
        if (cursor.broken)
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
            const unsigned long long value = parquet_little_endian_read(cursor.bytes + cursor.position, value_bytes);
            cursor.position += value_bytes;
            if (value > mask)
            {
                return 0;
            }
            for (unsigned long long step = 0ull; (step < run) && (written < read->value_count); step += 1ull)
            {
                // value was checked against a mask of at most 32 bits; it fits unsigned int.
                read->out[written] = (unsigned int)value;
                written += 1ull;
            }
        }
        else
        {
            const unsigned long long groups = header >> 1u;
            // A group of eight values takes bit_width bytes; at any nonzero width more groups than
            // bytes left cannot fit; refusing them first keeps the product below from wrapping.
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
                // A value of at most 32 bits starts within its first byte's low 7 bits; the eight
                // bytes from that byte hold it whole; the window is cut short only at the run's end.
                const unsigned long long first_bit = step * read->bit_width;
                const unsigned long long first_byte = cursor.position + (first_bit >> 3u);
                const unsigned long long bytes_left = packed_end - first_byte;
                // bytes_left is below 8 on the narrowing arm; it fits unsigned int.
                const unsigned int window_bytes = (bytes_left < 8ull) ? (unsigned int)bytes_left : 8u;
                const unsigned long long window = parquet_little_endian_read(cursor.bytes + first_byte, window_bytes);
                // The masked value holds bit_width bits, at most 32; it fits unsigned int.
                read->out[written] = (unsigned int)((window >> (first_bit & 7ull)) & mask);
                written += 1ull;
            }
            cursor.position = packed_end;
        }
    }
    return 1;
}

/** The bit width of a level whose ceiling is most: the fewest bits that hold 0 through most. */
CASMI_ALWAYS_INLINE unsigned int parquet_level_width_measure(unsigned int most)
{
    unsigned int width = 0u;
    while ((1ull << width) <= most)
    {
        width += 1u;
    }
    return width;
}

/** Read one page header: its type, sizes, level count and encodings, for all three page kinds. */
CASMI_ALWAYS_INLINE int parquet_page_header_read(ParquetCursor *cursor, ParquetPageHeader *header)
{
    long long field_id = 0ll;
    memset(header, 0, sizeof(*header));
    header->values_compressed = 1u;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != CASMI_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1ll) && (kind == CASMI_THRIFT_I32))
        {
            // A page type is 0 through 3, stored as i32.
            header->page_type = (unsigned int)parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 2ll) && (kind == CASMI_THRIFT_I32))
        {
            header->uncompressed_bytes = parquet_nonnegative_integer_read(cursor);
        }
        else if ((field_id == 3ll) && (kind == CASMI_THRIFT_I32))
        {
            header->compressed_bytes = parquet_nonnegative_integer_read(cursor);
        }
        else if (((field_id == 5ll) || (field_id == 7ll) || (field_id == 8ll)) && (kind == CASMI_THRIFT_STRUCT))
        {
            // Field 5 is a V1 data page header, 7 a dictionary page header, 8 a V2 data page header.
            const long long page_kind = field_id;
            long long inner_id = 0ll;
            for (unsigned int inner = parquet_field_read(cursor, &inner_id); inner != CASMI_THRIFT_STOP;
                 inner = parquet_field_read(cursor, &inner_id))
            {
                // Encodings are 0 through 9 and level byte counts fit a page, all stored as i32;
                // each narrowing below keeps its value.
                if ((inner_id == 1ll) && (inner == CASMI_THRIFT_I32))
                {
                    header->level_count = parquet_nonnegative_integer_read(cursor);
                }
                else if ((page_kind != 8ll) && (inner_id == 2ll) && (inner == CASMI_THRIFT_I32))
                {
                    header->encoding = (unsigned int)parquet_nonnegative_integer_read(cursor);
                }
                else if ((page_kind == 5ll) && (inner_id == 3ll) && (inner == CASMI_THRIFT_I32))
                {
                    header->definition_encoding = (unsigned int)parquet_nonnegative_integer_read(cursor);
                }
                else if ((page_kind == 5ll) && (inner_id == 4ll) && (inner == CASMI_THRIFT_I32))
                {
                    header->repetition_encoding = (unsigned int)parquet_nonnegative_integer_read(cursor);
                }
                else if ((page_kind == 8ll) && (inner_id == 4ll) && (inner == CASMI_THRIFT_I32))
                {
                    header->encoding = (unsigned int)parquet_nonnegative_integer_read(cursor);
                }
                else if ((page_kind == 8ll) && (inner_id == 5ll) && (inner == CASMI_THRIFT_I32))
                {
                    header->definition_bytes = parquet_nonnegative_integer_read(cursor);
                }
                else if ((page_kind == 8ll) && (inner_id == 6ll) && (inner == CASMI_THRIFT_I32))
                {
                    header->repetition_bytes = parquet_nonnegative_integer_read(cursor);
                }
                else if ((page_kind == 8ll) && (inner_id == 7ll) &&
                         ((inner == CASMI_THRIFT_TRUE) || (inner == CASMI_THRIFT_FALSE)))
                {
                    header->values_compressed = (inner == CASMI_THRIFT_TRUE) ? 1u : 0u;
                }
                else
                {
                    parquet_value_skip(cursor, inner);
                }
                if (cursor->broken)
                {
                    return 0;
                }
            }
        }
        else
        {
            parquet_value_skip(cursor, kind);
        }
        if (cursor->broken)
        {
            return 0;
        }
    }
    return 1;
}

/** Decompress one page body with the chunk's codec, which must fill request->out_capacity exactly. */
CASMI_ALWAYS_INLINE int parquet_body_inflate(const EngineBytesRequest *request, CasmiParquetCodec codec)
{
    if (codec == CASMI_PARQUET_UNCOMPRESSED)
    {
        if (request->in_bytes != request->out_capacity)
        {
            return 0;
        }
        memcpy(request->out, request->in, request->in_bytes);
        return 1;
    }
    long long written = ENGINE_BYTES_ERROR;
    if (codec == CASMI_PARQUET_SNAPPY)
    {
        written = snappy_decode(request);
    }
    else if (codec == CASMI_PARQUET_ZSTD)
    {
        written = zstd_decode(request);
    }
    // A refusal is negative; only a nonnegative count reaches the comparison as unsigned.
    return (written >= 0ll) && ((unsigned long long)written == request->out_capacity);
}

/**
 * Decode a dictionary page. A BYTE_ARRAY dictionary lands in the value pool and its entries are
 * recorded as offsets past each 4-byte length; a fixed-width one lands in the scratch dictionary.
 */
CASMI_ALWAYS_INLINE int parquet_dictionary_page_decode(ParquetColumnWalk *walk, const ParquetPageHeader *header,
                                                       const unsigned char *body)
{
    CasmiParquetColumn *const column = walk->args->column;
    const int variable = (walk->leaf->physical == CASMI_PARQUET_BYTE_ARRAY);
    const unsigned long long room = variable ? (column->value_bytes_room - walk->value_bytes_used) : walk->chunk->uncompressed_bytes;
    if (((header->encoding != CASMI_PARQUET_PLAIN) && (header->encoding != CASMI_PARQUET_PLAIN_DICTIONARY)) ||
        (header->uncompressed_bytes > room) || (header->level_count > walk->chunk->level_count))
    {
        return 0;
    }
    unsigned char *const target = variable ? (column->value_bytes + walk->value_bytes_used) : walk->dictionary_bytes;
    const EngineBytesRequest request = {body, header->compressed_bytes, target, header->uncompressed_bytes};
    if (!parquet_body_inflate(&request, walk->chunk->codec))
    {
        return 0;
    }
    walk->dictionary_count = header->level_count;
    if (!variable)
    {
        return (header->level_count * column->fixed_bytes) <= header->uncompressed_bytes;
    }
    ParquetCursor entries = {target, header->uncompressed_bytes, 0ull, 0u};
    for (unsigned long long entry = 0ull; entry < header->level_count; entry += 1ull)
    {
        if ((entries.byte_count - entries.position) < 4ull)
        {
            return 0;
        }
        const unsigned long long length = parquet_little_endian_read(entries.bytes + entries.position, 4u);
        entries.position += 4ull;
        if (length > (entries.byte_count - entries.position))
        {
            return 0;
        }
        walk->dictionary_offset[entry] = walk->value_bytes_used + entries.position;
        walk->dictionary_length[entry] = length;
        entries.position += length;
    }
    walk->value_bytes_used += header->uncompressed_bytes;
    return 1;
}

/**
 * Decode one V1 page's levels at page->position: a 4-byte length, then that many bytes of hybrid run. A
 * level whose ceiling is zero is not stored and decodes to zeros.
 */
CASMI_ALWAYS_INLINE int parquet_levels_v1_decode(ParquetCursor *page, unsigned int level_most,
                                                 unsigned long long level_count, unsigned int *out)
{
    if (level_most == 0u)
    {
        memset(out, 0, level_count * sizeof(*out));
        return 1;
    }
    if ((page->byte_count - page->position) < 4ull)
    {
        return 0;
    }
    const unsigned long long length = parquet_little_endian_read(page->bytes + page->position, 4u);
    page->position += 4ull;
    if (length > (page->byte_count - page->position))
    {
        return 0;
    }
    const ParquetHybridRead read = {page->bytes + page->position, length, parquet_level_width_measure(level_most), level_count, out};
    page->position += length;
    return parquet_hybrid_decode(&read);
}

/** Decode present values from one page's value section, PLAIN or dictionary indices. */
CASMI_ALWAYS_INLINE int parquet_values_decode(ParquetColumnWalk *walk, unsigned int encoding,
                                              const ParquetCursor *values, unsigned long long present)
{
    CasmiParquetColumn *const column = walk->args->column;
    const int variable = (walk->leaf->physical == CASMI_PARQUET_BYTE_ARRAY);
    if ((walk->values_done + present) > walk->chunk->level_count)
    {
        return 0;
    }
    if ((encoding == CASMI_PARQUET_PLAIN_DICTIONARY) || (encoding == CASMI_PARQUET_RLE_DICTIONARY))
    {
        if (present == 0ull)
        {
            return 1;
        }
        if (values->byte_count == 0ull)
        {
            return 0;
        }
        // The first byte is the index bit width; the hybrid decoder refuses one past 32 bits.
        const ParquetHybridRead read = {values->bytes + 1u, values->byte_count - 1ull, values->bytes[0], present, walk->dictionary_index};
        if (!parquet_hybrid_decode(&read))
        {
            return 0;
        }
        for (unsigned long long value = 0ull; value < present; value += 1ull)
        {
            const unsigned long long entry = walk->dictionary_index[value];
            if (entry >= walk->dictionary_count)
            {
                return 0;
            }
            const unsigned long long slot = walk->values_done + value;
            if (variable)
            {
                column->value_offset[slot] = walk->dictionary_offset[entry];
                column->value_length[slot] = walk->dictionary_length[entry];
            }
            else
            {
                memcpy(column->value_bytes + (slot * column->fixed_bytes),
                       walk->dictionary_bytes + (entry * column->fixed_bytes), column->fixed_bytes);
            }
        }
        walk->values_done += present;
        return 1;
    }
    if (encoding != CASMI_PARQUET_PLAIN)
    {
        return 0;
    }
    if (!variable)
    {
        const unsigned long long needed = present * column->fixed_bytes;
        if (needed > values->byte_count)
        {
            return 0;
        }
        memcpy(column->value_bytes + (walk->values_done * column->fixed_bytes), values->bytes, needed);
        walk->values_done += present;
        return 1;
    }
    ParquetCursor plain = *values;
    for (unsigned long long value = 0ull; value < present; value += 1ull)
    {
        if ((plain.byte_count - plain.position) < 4ull)
        {
            return 0;
        }
        const unsigned long long length = parquet_little_endian_read(plain.bytes + plain.position, 4u);
        plain.position += 4ull;
        if ((length > (plain.byte_count - plain.position)) || (length > (column->value_bytes_room - walk->value_bytes_used)))
        {
            return 0;
        }
        memcpy(column->value_bytes + walk->value_bytes_used, plain.bytes + plain.position, length);
        const unsigned long long slot = walk->values_done + value;
        column->value_offset[slot] = walk->value_bytes_used;
        column->value_length[slot] = length;
        walk->value_bytes_used += length;
        plain.position += length;
    }
    walk->values_done += present;
    return 1;
}

/**
 * Assemble rows from one page's levels: a repetition level of zero starts a row, and a definition
 * level at its ceiling is a present value. The count of present values returns through *present.
 */
CASMI_ALWAYS_INLINE int parquet_rows_assemble(ParquetColumnWalk *walk, unsigned long long level_count,
                                              unsigned long long *present)
{
    CasmiParquetColumn *const column = walk->args->column;
    unsigned long long counted = walk->values_done;
    for (unsigned long long level = 0ull; level < level_count; level += 1ull)
    {
        if ((walk->leaf->repetition_most == 0u) || (walk->repetition_level[level] == 0u))
        {
            if ((walk->rows_done + 1ull) >= column->row_start_room)
            {
                return 0;
            }
            column->row_start[walk->rows_done] = counted;
            walk->rows_done += 1ull;
        }
        else if (walk->rows_done == 0ull)
        {
            return 0;
        }
        if (walk->definition_level[level] == walk->leaf->definition_most)
        {
            counted += 1ull;
        }
        else if (walk->definition_level[level] > walk->leaf->definition_most)
        {
            return 0;
        }
    }
    *present = counted - walk->values_done;
    return 1;
}

/** Decode one data page, V1 or V2: its levels, its rows and its values. */
CASMI_ALWAYS_INLINE int parquet_data_page_decode(ParquetColumnWalk *walk, const ParquetPageHeader *header,
                                                 const unsigned char *body)
{
    if (((walk->levels_done + header->level_count) > walk->chunk->level_count) ||
        (header->uncompressed_bytes > walk->chunk->uncompressed_bytes))
    {
        return 0;
    }
    ParquetCursor values = {walk->page_bytes, 0ull, 0ull, 0u};
    if (header->page_type == CASMI_PARQUET_DATA_PAGE)
    {
        const EngineBytesRequest request = {body, header->compressed_bytes, walk->page_bytes, header->uncompressed_bytes};
        if (((walk->leaf->definition_most != 0u) && (header->definition_encoding != CASMI_PARQUET_RLE)) ||
            ((walk->leaf->repetition_most != 0u) && (header->repetition_encoding != CASMI_PARQUET_RLE)) ||
            !parquet_body_inflate(&request, walk->chunk->codec))
        {
            return 0;
        }
        ParquetCursor page = {walk->page_bytes, header->uncompressed_bytes, 0ull, 0u};
        if (!parquet_levels_v1_decode(&page, walk->leaf->repetition_most, header->level_count, walk->repetition_level) ||
            !parquet_levels_v1_decode(&page, walk->leaf->definition_most, header->level_count, walk->definition_level))
        {
            return 0;
        }
        values.bytes = page.bytes + page.position;
        values.byte_count = page.byte_count - page.position;
    }
    else
    {
        // A V2 page stores its levels uncompressed and unprefixed ahead of the values, which alone
        // are compressed, and only where the header says so.
        const unsigned long long level_bytes = header->repetition_bytes + header->definition_bytes;
        if ((level_bytes > header->compressed_bytes) || (level_bytes > header->uncompressed_bytes))
        {
            return 0;
        }
        const ParquetHybridRead repetition = {body, header->repetition_bytes, parquet_level_width_measure(walk->leaf->repetition_most),
                                              header->level_count, walk->repetition_level};
        const ParquetHybridRead definition = {body + header->repetition_bytes, header->definition_bytes,
                                              parquet_level_width_measure(walk->leaf->definition_most), header->level_count,
                                              walk->definition_level};
        if (walk->leaf->repetition_most == 0u)
        {
            memset(walk->repetition_level, 0, header->level_count * sizeof(*walk->repetition_level));
        }
        else if (!parquet_hybrid_decode(&repetition))
        {
            return 0;
        }
        if (walk->leaf->definition_most == 0u)
        {
            memset(walk->definition_level, 0, header->level_count * sizeof(*walk->definition_level));
        }
        else if (!parquet_hybrid_decode(&definition))
        {
            return 0;
        }
        const EngineBytesRequest request = {body + level_bytes, header->compressed_bytes - level_bytes, walk->page_bytes,
                                            header->uncompressed_bytes - level_bytes};
        const CasmiParquetCodec codec = (header->values_compressed != 0u) ? walk->chunk->codec : CASMI_PARQUET_UNCOMPRESSED;
        if (!parquet_body_inflate(&request, codec))
        {
            return 0;
        }
        values.byte_count = request.out_capacity;
    }
    unsigned long long present = 0ull;
    if (!parquet_rows_assemble(walk, header->level_count, &present) ||
        !parquet_values_decode(walk, header->encoding, &values, present))
    {
        return 0;
    }
    walk->levels_done += header->level_count;
    return 1;
}

/** Walk every page of the chunk already read into walk->chunk_bytes. */
CASMI_ALWAYS_INLINE int parquet_pages_decode(ParquetColumnWalk *walk)
{
    ParquetCursor pages = {walk->chunk_bytes, walk->chunk->compressed_bytes, 0ull, 0u};
    while (walk->levels_done < walk->chunk->level_count)
    {
        ParquetPageHeader header;
        if (!parquet_page_header_read(&pages, &header) || (header.compressed_bytes > (pages.byte_count - pages.position)))
        {
            return 0;
        }
        const unsigned char *const body = pages.bytes + pages.position;
        pages.position += header.compressed_bytes;
        if (header.page_type == CASMI_PARQUET_DICTIONARY_PAGE)
        {
            if (!parquet_dictionary_page_decode(walk, &header, body))
            {
                return 0;
            }
        }
        else if ((header.page_type == CASMI_PARQUET_DATA_PAGE) || (header.page_type == CASMI_PARQUET_DATA_PAGE_V2))
        {
            if (!parquet_data_page_decode(walk, &header, body))
            {
                return 0;
            }
        }
    }
    return 1;
}

/** The backend of casmi_parquet_column_read. */
CASMI_ALWAYS_INLINE long long parquet_column_decode(const CasmiParquetColumnRead *args)
{
    CasmiParquetColumn rooms = {0};
    if (!parquet_column_rooms_measure(args, &rooms) || (args->column->scratch_room < rooms.scratch_room) ||
        (args->column->row_start_room < rooms.row_start_room) || (args->column->value_bytes_room < rooms.value_bytes_room) ||
        (args->column->value_offset_room < rooms.value_offset_room))
    {
        return ENGINE_BYTES_ERROR;
    }
    const CasmiParquetChunk *const chunk = &args->footer->row_group[args->row_group].chunk[args->leaf];
    // The scratch is carved in the order parquet_column_rooms_measure sized it, the eight-byte tables
    // first so every table lands on its own alignment inside one allocation.
    unsigned long long *const dictionary_offset = (unsigned long long *)(void *)args->column->scratch;
    unsigned long long *const dictionary_length = dictionary_offset + (chunk->level_count + 1ull);
    unsigned int *const repetition = (unsigned int *)(void *)(dictionary_length + (chunk->level_count + 1ull));
    unsigned char *const chunk_bytes = (unsigned char *)(void *)(repetition + (3ull * chunk->level_count));
    ParquetColumnWalk walk = {0};
    walk.args = args;
    walk.leaf = &args->footer->leaf[args->leaf];
    walk.chunk = chunk;
    walk.repetition_level = repetition;
    walk.definition_level = repetition + chunk->level_count;
    walk.dictionary_index = walk.definition_level + chunk->level_count;
    walk.dictionary_offset = dictionary_offset;
    walk.dictionary_length = dictionary_length;
    walk.chunk_bytes = chunk_bytes;
    walk.page_bytes = chunk_bytes + chunk->compressed_bytes;
    walk.dictionary_bytes = walk.page_bytes + chunk->uncompressed_bytes;
    args->column->fixed_bytes = rooms.fixed_bytes;
    const ParquetFileRange range = {args->path, chunk->first_byte, chunk->compressed_bytes, chunk_bytes};
    const int good = parquet_range_read(&range) && parquet_pages_decode(&walk) &&
                     (walk.rows_done == args->footer->row_group[args->row_group].rows);
    args->column->rows = good ? walk.rows_done : 0ull;
    args->column->values = good ? walk.values_done : 0ull;
    if (!good)
    {
        return ENGINE_BYTES_ERROR;
    }
    args->column->row_start[walk.rows_done] = walk.values_done;
    // values is bounded by the chunk's level count, far below 2^63; the conversion is exact.
    return (long long)walk.values_done;
}

CASMI_ENTRY(casmi_parquet_footer_read, CasmiParquetFooterRead, parquet_footer_decode)
CASMI_ENTRY(casmi_parquet_leaf_find, CasmiParquetLeafFind, parquet_leaf_search)
CASMI_ENTRY(casmi_parquet_column_bound, CasmiParquetColumnRead, parquet_column_measure)
CASMI_ENTRY(casmi_parquet_column_read, CasmiParquetColumnRead, parquet_column_decode)
