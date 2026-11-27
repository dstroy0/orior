// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// parquet_footer.c: the footer's schema, row groups and column chunks
#include "../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "parquet_internal.h"

_Static_assert(PARQUET_MAGIC_BYTES == (sizeof(PARQUET_MAGIC) - 1u),
               "PARQUET_MAGIC_BYTES: must count the bytes of PARQUET_MAGIC without its terminator");

// schema repetition types
#define PARQUET_REQUIRED 0u
#define PARQUET_REPEATED 2u

// one schema element's parent, as the preorder walk of the schema records it
typedef struct
{
    unsigned long long children_left;
    unsigned int definition_max;
    unsigned int repetition_max;
    unsigned long long path_bytes;
} ParquetSchemaFrame;

// read one schema element and place it. The root opens the walk. An element with children opens a frame. An element
// without children is the next leaf of the footer
static int parquet_schema_element_read(ParquetCursor *cursor, ParquetFooter *footer, ParquetSchemaFrame *stack,
                                       unsigned int *depth, char *path)
{
    long long field_id = 0LL;
    unsigned long long physical = 0ULL;
    unsigned long long fixed_bytes = 0ULL;
    unsigned long long repetition_type = PARQUET_REQUIRED;
    unsigned long long children = 0ULL;
    const unsigned char *name = NULL;
    unsigned long long name_bytes = 0ULL;
    for (unsigned int type = parquet_field_read(cursor, &field_id); type != PARQUET_THRIFT_STOP;
         type = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1LL) && (type == PARQUET_THRIFT_I32))
        {
            physical = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 2LL) && (type == PARQUET_THRIFT_I32))
        {
            fixed_bytes = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 3LL) && (type == PARQUET_THRIFT_I32))
        {
            repetition_type = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 4LL) && (type == PARQUET_THRIFT_BINARY))
        {
            name = parquet_binary_read(cursor, &name_bytes);
        }
        else if ((field_id == 5LL) && (type == PARQUET_THRIFT_I32))
        {
            children = parquet_nonnegative_read(cursor);
        }
        else
        {
            parquet_value_skip(cursor, type);
        }
        if (cursor->broken != 0u)
        {
            return 0;
        }
    }
    // the root is the first element and names no column; its children open the walk
    if (*depth == 0u)
    {
        stack[0].children_left = children;
        stack[0].definition_max = 0u;
        stack[0].repetition_max = 0u;
        stack[0].path_bytes = 0ULL;
        *depth = 1u;
        return children != 0ULL;
    }
    ParquetSchemaFrame *const parent = &stack[*depth - 1u];
    if (parent->children_left == 0ULL)
    {
        return 0;
    }
    parent->children_left -= 1ULL;
    const unsigned int definition = parent->definition_max + ((repetition_type != PARQUET_REQUIRED) ? 1u : 0u);
    const unsigned int repetition = parent->repetition_max + ((repetition_type == PARQUET_REPEATED) ? 1u : 0u);
    const unsigned long long separator = (parent->path_bytes == 0ULL) ? 0ULL : 1ULL;
    const unsigned long long path_bytes = parent->path_bytes + separator + name_bytes;
    if ((name == NULL) || (path_bytes >= PARQUET_PATH_BYTES))
    {
        return 0;
    }
    if (separator != 0ULL)
    {
        path[parent->path_bytes] = '.';
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = path + parent->path_bytes + separator, .src = name,
               .bytes = (size_t)name_bytes);
    path[path_bytes] = '\0';
    if (children == 0ULL)
    {
        // a physical type is 0 through 7 and a fixed width a few bytes, both stored as i32. The read of a column
        // refuses an unknown type. This code refuses a width past an unsigned int
        if ((footer->leaf_count >= PARQUET_LEAVES_MAX) || (fixed_bytes > UINT_MAX) || (physical > UINT_MAX))
        {
            return 0;
        }
        ParquetLeaf *const leaf = &footer->leaf[footer->leaf_count];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = leaf->path, .src = path, .bytes = (size_t)path_bytes + 1u);
        leaf->physical = (ParquetPhysical)physical;
        leaf->fixed_bytes = (unsigned int)fixed_bytes;
        leaf->definition_max = definition;
        leaf->repetition_max = repetition;
        footer->leaf_count += 1u;
    }
    else
    {
        if (*depth >= PARQUET_SCHEMA_DEPTH_MAX)
        {
            return 0;
        }
        stack[*depth].children_left = children;
        stack[*depth].definition_max = definition;
        stack[*depth].repetition_max = repetition;
        stack[*depth].path_bytes = path_bytes;
        *depth += 1u;
    }
    while ((*depth > 1u) && (stack[*depth - 1u].children_left == 0ULL))
    {
        *depth -= 1u;
    }
    return 1;
}

// the schema list into the footer's leaves, each with its dotted path and level ceilings
static int parquet_schema_read(ParquetCursor *cursor, ParquetFooter *footer)
{
    unsigned int element_type = 0u;
    const unsigned long long count = parquet_list_header_read(cursor, &element_type);
    if (element_type != PARQUET_THRIFT_STRUCT)
    {
        return 0;
    }
    ParquetSchemaFrame stack[PARQUET_SCHEMA_DEPTH_MAX];
    char path[PARQUET_PATH_BYTES];
    unsigned int depth = 0u;
    for (unsigned long long element = 0ULL; element < count; element += 1ULL)
    {
        if (!parquet_schema_element_read(cursor, footer, stack, &depth, path))
        {
            return 0;
        }
    }
    return cursor->broken == 0u;
}

// whether a chunk's path_in_schema list names the same dotted path as leaf_path
static int parquet_path_match(ParquetCursor *cursor, const char *leaf_path)
{
    unsigned int element_type = 0u;
    const unsigned long long parts = parquet_list_header_read(cursor, &element_type);
    if (element_type != PARQUET_THRIFT_BINARY)
    {
        return 0;
    }
    unsigned long long at = 0ULL;
    int matches = 1;
    for (unsigned long long part = 0ULL; part < parts; part += 1ULL)
    {
        unsigned long long length = 0ULL;
        const unsigned char *const name = parquet_binary_read(cursor, &length);
        if (part != 0ULL)
        {
            matches = matches && (at < PARQUET_PATH_BYTES) && (leaf_path[at] == '.');
            at += 1ULL;
        }
        matches =
            matches && ((at + length) < PARQUET_PATH_BYTES) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = leaf_path + at, .other = name, .bytes = (size_t)length) == 0);
        at += length;
    }
    return matches && (cursor->broken == 0u) && (at < PARQUET_PATH_BYTES) && (leaf_path[at] == '\0');
}

// one ColumnMetaData: the codec, the counts, the sizes and the chunk's first byte
static int parquet_column_metadata_read(ParquetCursor *cursor, const ParquetLeaf *leaf, ParquetChunk *chunk)
{
    long long field_id = 0LL;
    unsigned long long data_page_offset = 0ULL;
    unsigned long long dictionary_page_offset = 0ULL;
    int path_seen = 0;
    for (unsigned int type = parquet_field_read(cursor, &field_id); type != PARQUET_THRIFT_STOP;
         type = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 3LL) && (type == PARQUET_THRIFT_LIST))
        {
            if (!parquet_path_match(cursor, leaf->path))
            {
                return 0;
            }
            path_seen = 1;
        }
        else if ((field_id == 4LL) && (type == PARQUET_THRIFT_I32))
        {
            const unsigned long long codec = parquet_nonnegative_read(cursor);
            // a codec is 0 through 7, stored as i32. The code stores one past it as the last unknown codec. The decode
            // of a page refuses an unknown codec
            chunk->codec = (ParquetCodec)((codec <= (unsigned long long)PARQUET_LZ4_RAW) ? codec : 0xFFULL);
        }
        else if ((field_id == 5LL) && (type == PARQUET_THRIFT_I64))
        {
            chunk->level_count = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 6LL) && (type == PARQUET_THRIFT_I64))
        {
            chunk->uncompressed_bytes = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 7LL) && (type == PARQUET_THRIFT_I64))
        {
            chunk->compressed_bytes = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 9LL) && (type == PARQUET_THRIFT_I64))
        {
            data_page_offset = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 11LL) && (type == PARQUET_THRIFT_I64))
        {
            dictionary_page_offset = parquet_nonnegative_read(cursor);
        }
        else
        {
            parquet_value_skip(cursor, type);
        }
        if (cursor->broken != 0u)
        {
            return 0;
        }
    }
    // a writer that stores no dictionary can still write the field as zero. A dictionary offset counts only where it is
    // nonzero and ahead of the first data page
    const int has_dictionary = (dictionary_page_offset != 0ULL) && (dictionary_page_offset < data_page_offset);
    chunk->first_byte = has_dictionary ? dictionary_page_offset : data_page_offset;
    return path_seen;
}

// one ColumnChunk, which carries its ColumnMetaData
static int parquet_column_chunk_read(ParquetCursor *cursor, const ParquetLeaf *leaf, ParquetChunk *chunk)
{
    long long field_id = 0LL;
    int metadata_seen = 0;
    for (unsigned int type = parquet_field_read(cursor, &field_id); type != PARQUET_THRIFT_STOP;
         type = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 3LL) && (type == PARQUET_THRIFT_STRUCT))
        {
            if (!parquet_column_metadata_read(cursor, leaf, chunk))
            {
                return 0;
            }
            metadata_seen = 1;
        }
        else
        {
            parquet_value_skip(cursor, type);
        }
        if (cursor->broken != 0u)
        {
            return 0;
        }
    }
    return metadata_seen;
}

// one RowGroup: its row count and one chunk a leaf, in leaf order
static int parquet_row_group_read(ParquetCursor *cursor, const ParquetFooter *footer, ParquetRowGroup *row_group)
{
    long long field_id = 0LL;
    int columns_seen = 0;
    for (unsigned int type = parquet_field_read(cursor, &field_id); type != PARQUET_THRIFT_STOP;
         type = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1LL) && (type == PARQUET_THRIFT_LIST))
        {
            unsigned int element_type = 0u;
            const unsigned long long count = parquet_list_header_read(cursor, &element_type);
            if ((element_type != PARQUET_THRIFT_STRUCT) || (count != footer->leaf_count))
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
        else if ((field_id == 3LL) && (type == PARQUET_THRIFT_I64))
        {
            row_group->rows = parquet_nonnegative_read(cursor);
        }
        else
        {
            parquet_value_skip(cursor, type);
        }
        if (cursor->broken != 0u)
        {
            return 0;
        }
    }
    return columns_seen;
}

// the FileMetaData: the schema first, then the row count and the row groups
static int parquet_file_metadata_read(ParquetCursor *cursor, ParquetFooter *footer)
{
    long long field_id = 0LL;
    int schema_seen = 0;
    for (unsigned int type = parquet_field_read(cursor, &field_id); type != PARQUET_THRIFT_STOP;
         type = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 2LL) && (type == PARQUET_THRIFT_LIST))
        {
            if (!parquet_schema_read(cursor, footer))
            {
                return 0;
            }
            schema_seen = 1;
        }
        else if ((field_id == 3LL) && (type == PARQUET_THRIFT_I64))
        {
            footer->rows = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 4LL) && (type == PARQUET_THRIFT_LIST) && schema_seen)
        {
            unsigned int element_type = 0u;
            const unsigned long long count = parquet_list_header_read(cursor, &element_type);
            if ((element_type != PARQUET_THRIFT_STRUCT) || (count > PARQUET_ROW_GROUPS_MAX))
            {
                return 0;
            }
            for (unsigned long long group = 0ULL; group < count; group += 1ULL)
            {
                if (!parquet_row_group_read(cursor, footer, &footer->row_group[group]))
                {
                    return 0;
                }
            }
            // the code above limited the count to PARQUET_ROW_GROUPS_MAX, and it fits an unsigned int
            footer->row_group_count = (unsigned int)count;
        }
        else
        {
            parquet_value_skip(cursor, type);
        }
        if (cursor->broken != 0u)
        {
            return 0;
        }
    }
    return schema_seen;
}

int parquet_footer_read(const EngineIngestTools *tools, const char *path, ParquetFooter *footer, EngineError *error)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = footer, .bytes = sizeof(*footer), .val = 0);
    if (!PARQUET_CHECK((tools != NULL) && (tools->size != NULL) && (tools->read != NULL) && (path != NULL), &tools,
                       error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    const long long file_bytes = tools->size(path);
    // the sum is twelve and converts exactly
    if (!PARQUET_CHECK(file_bytes >= (long long)(PARQUET_MAGIC_BYTES + PARQUET_TRAILER_BYTES), path, error,
                       ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    // the check above found the size at twelve or more, and the conversion is exact
    const unsigned long long file_size = (unsigned long long)file_bytes;
    unsigned char trailer[PARQUET_TRAILER_BYTES];
    EngineFileRange range;
    range.path = path;
    range.offset = file_size - PARQUET_TRAILER_BYTES;
    range.bytes = PARQUET_TRAILER_BYTES;
    range.out = trailer;
    if (!PARQUET_CHECK((tools->read(&range) == (long long)PARQUET_TRAILER_BYTES) &&
                           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = trailer + PARQUET_MAGIC_BYTES,
                                       .other = PARQUET_MAGIC, .bytes = PARQUET_MAGIC_BYTES) == 0),
                       path, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    const unsigned long long footer_bytes = parquet_little(trailer, 4u);
    if (!PARQUET_CHECK((footer_bytes <= PARQUET_FOOTER_BYTES_MAX) &&
                           (footer_bytes <= (file_size - PARQUET_MAGIC_BYTES - PARQUET_TRAILER_BYTES)),
                       trailer, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    unsigned char *const text =
        (unsigned char *)platform_cell_taken((size_t)footer_bytes + 1u, platform_place_g_program_owner, 0);
    if (!PARQUET_CHECK(text != NULL, &text, error, ENGINE_ERROR_RESOURCE))
    {
        return 0;
    }
    range.offset = file_size - PARQUET_TRAILER_BYTES - footer_bytes;
    range.bytes = footer_bytes;
    range.out = text;
    ParquetCursor cursor = {text, footer_bytes, 0ULL, 0u};
    // the code above limits the bytes of the footer to 16 MiB, and the count converts exactly
    const int read = (tools->read(&range) == (long long)footer_bytes);
    const int decoded = read && parquet_file_metadata_read(&cursor, footer) && (cursor.broken == 0u);
    platform_cell_released(text, platform_place_g_program_owner);
    footer->footer_at = range.offset;
    footer->footer_bytes = footer_bytes;
    return PARQUET_CHECK(read, path, error, ENGINE_ERROR_REQUEST) &&
           PARQUET_CHECK(decoded, footer, error, ENGINE_ERROR_REQUEST);
}
