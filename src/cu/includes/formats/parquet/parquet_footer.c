// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// parquet_footer.c: the footer's schema, row groups and column chunks
#include "parquet_internal.h"

_Static_assert(PARQUET_MAGIC_BYTES == (sizeof(PARQUET_MAGIC) - 1u),
               "PARQUET_MAGIC_BYTES: must count the bytes of PARQUET_MAGIC without its terminator");

// schema repetition types
#define PARQUET_REQUIRED 0u
#define PARQUET_REPEATED 2u

// one schema element's parent, as the preorder walk of the schema holds it
typedef struct
{
    unsigned long long children_left;
    unsigned int definition_most;
    unsigned int repetition_most;
    unsigned long long path_bytes;
} ParquetSchemaFrame;

// read one schema element and place it: the root opens the walk, an element with children opens a frame, and an
// element without children is the footer's next leaf
static int parquet_schema_element_read(ParquetCursor *cursor, ParquetFooter *footer, ParquetSchemaFrame *stack,
                                       unsigned int *depth, char *path)
{
    long long field_id = 0ll;
    unsigned long long physical = 0ull;
    unsigned long long fixed_bytes = 0ull;
    unsigned long long repetition_type = PARQUET_REQUIRED;
    unsigned long long children = 0ull;
    const unsigned char *name = NULL;
    unsigned long long name_bytes = 0ull;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != PARQUET_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1ll) && (kind == PARQUET_THRIFT_I32))
        {
            physical = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 2ll) && (kind == PARQUET_THRIFT_I32))
        {
            fixed_bytes = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 3ll) && (kind == PARQUET_THRIFT_I32))
        {
            repetition_type = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 4ll) && (kind == PARQUET_THRIFT_BINARY))
        {
            name = parquet_binary_read(cursor, &name_bytes);
        }
        else if ((field_id == 5ll) && (kind == PARQUET_THRIFT_I32))
        {
            children = parquet_nonnegative_read(cursor);
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
    // the root is the first element and names no column; its children open the walk
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
    const unsigned int definition = parent->definition_most + ((repetition_type != PARQUET_REQUIRED) ? 1u : 0u);
    const unsigned int repetition = parent->repetition_most + ((repetition_type == PARQUET_REPEATED) ? 1u : 0u);
    const unsigned long long separator = (parent->path_bytes == 0ull) ? 0ull : 1ull;
    const unsigned long long path_bytes = parent->path_bytes + separator + name_bytes;
    if ((name == NULL) || (path_bytes >= PARQUET_PATH_BYTES))
    {
        return 0;
    }
    if (separator != 0ull)
    {
        path[parent->path_bytes] = '.';
    }
    memcpy(path + parent->path_bytes + separator, name, (size_t)name_bytes);
    path[path_bytes] = '\0';
    if (children == 0ull)
    {
        // a physical type is 0 through 7 and a fixed width a few bytes, both stored as i32, and an unknown type is
        // refused where its column is read; a width past an unsigned int is refused here
        if ((footer->leaf_count >= PARQUET_LEAVES_MOST) || (fixed_bytes > UINT_MAX) || (physical > UINT_MAX))
        {
            return 0;
        }
        ParquetLeaf *const leaf = &footer->leaf[footer->leaf_count];
        memcpy(leaf->path, path, (size_t)path_bytes + 1u);
        leaf->physical = (ParquetPhysical)physical;
        leaf->fixed_bytes = (unsigned int)fixed_bytes;
        leaf->definition_most = definition;
        leaf->repetition_most = repetition;
        footer->leaf_count += 1u;
    }
    else
    {
        if (*depth >= PARQUET_SCHEMA_DEPTH_MOST)
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

// the schema list into the footer's leaves, each with its dotted path and level ceilings
static int parquet_schema_read(ParquetCursor *cursor, ParquetFooter *footer)
{
    unsigned int element_kind = 0u;
    const unsigned long long count = parquet_list_header_read(cursor, &element_kind);
    if (element_kind != PARQUET_THRIFT_STRUCT)
    {
        return 0;
    }
    ParquetSchemaFrame stack[PARQUET_SCHEMA_DEPTH_MOST];
    char path[PARQUET_PATH_BYTES];
    unsigned int depth = 0u;
    for (unsigned long long element = 0ull; element < count; element += 1ull)
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
    unsigned int element_kind = 0u;
    const unsigned long long parts = parquet_list_header_read(cursor, &element_kind);
    if (element_kind != PARQUET_THRIFT_BINARY)
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
            matches = matches && (at < PARQUET_PATH_BYTES) && (leaf_path[at] == '.');
            at += 1ull;
        }
        matches =
            matches && ((at + length) < PARQUET_PATH_BYTES) && (memcmp(leaf_path + at, name, (size_t)length) == 0);
        at += length;
    }
    return matches && (cursor->broken == 0u) && (at < PARQUET_PATH_BYTES) && (leaf_path[at] == '\0');
}

// one ColumnMetaData: the codec, the counts, the sizes and the chunk's first byte
static int parquet_column_metadata_read(ParquetCursor *cursor, const ParquetLeaf *leaf, ParquetChunk *chunk)
{
    long long field_id = 0ll;
    unsigned long long data_page_offset = 0ull;
    unsigned long long dictionary_page_offset = 0ull;
    int path_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != PARQUET_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 3ll) && (kind == PARQUET_THRIFT_LIST))
        {
            if (!parquet_path_match(cursor, leaf->path))
            {
                return 0;
            }
            path_seen = 1;
        }
        else if ((field_id == 4ll) && (kind == PARQUET_THRIFT_I32))
        {
            const unsigned long long codec = parquet_nonnegative_read(cursor);
            // a codec is 0 through 7, stored as i32; one past it is kept as the last unknown one, and an unknown
            // codec is refused where a page is decoded
            chunk->codec = (ParquetCodec)((codec <= (unsigned long long)PARQUET_LZ4_RAW) ? codec : 0xFFull);
        }
        else if ((field_id == 5ll) && (kind == PARQUET_THRIFT_I64))
        {
            chunk->level_count = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 6ll) && (kind == PARQUET_THRIFT_I64))
        {
            chunk->uncompressed_bytes = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 7ll) && (kind == PARQUET_THRIFT_I64))
        {
            chunk->compressed_bytes = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 9ll) && (kind == PARQUET_THRIFT_I64))
        {
            data_page_offset = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 11ll) && (kind == PARQUET_THRIFT_I64))
        {
            dictionary_page_offset = parquet_nonnegative_read(cursor);
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
    // a writer that stores no dictionary may still write the field as zero: a dictionary offset counts only where it
    // is nonzero and ahead of the first data page
    const int has_dictionary = (dictionary_page_offset != 0ull) && (dictionary_page_offset < data_page_offset);
    chunk->first_byte = has_dictionary ? dictionary_page_offset : data_page_offset;
    return path_seen;
}

// one ColumnChunk, which carries its ColumnMetaData
static int parquet_column_chunk_read(ParquetCursor *cursor, const ParquetLeaf *leaf, ParquetChunk *chunk)
{
    long long field_id = 0ll;
    int metadata_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != PARQUET_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 3ll) && (kind == PARQUET_THRIFT_STRUCT))
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
    long long field_id = 0ll;
    int columns_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != PARQUET_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 1ll) && (kind == PARQUET_THRIFT_LIST))
        {
            unsigned int element_kind = 0u;
            const unsigned long long count = parquet_list_header_read(cursor, &element_kind);
            if ((element_kind != PARQUET_THRIFT_STRUCT) || (count != footer->leaf_count))
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
        else if ((field_id == 3ll) && (kind == PARQUET_THRIFT_I64))
        {
            row_group->rows = parquet_nonnegative_read(cursor);
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
    return columns_seen;
}

// the FileMetaData: the schema first, then the row count and the row groups
static int parquet_file_metadata_read(ParquetCursor *cursor, ParquetFooter *footer)
{
    long long field_id = 0ll;
    int schema_seen = 0;
    for (unsigned int kind = parquet_field_read(cursor, &field_id); kind != PARQUET_THRIFT_STOP;
         kind = parquet_field_read(cursor, &field_id))
    {
        if ((field_id == 2ll) && (kind == PARQUET_THRIFT_LIST))
        {
            if (!parquet_schema_read(cursor, footer))
            {
                return 0;
            }
            schema_seen = 1;
        }
        else if ((field_id == 3ll) && (kind == PARQUET_THRIFT_I64))
        {
            footer->rows = parquet_nonnegative_read(cursor);
        }
        else if ((field_id == 4ll) && (kind == PARQUET_THRIFT_LIST) && schema_seen)
        {
            unsigned int element_kind = 0u;
            const unsigned long long count = parquet_list_header_read(cursor, &element_kind);
            if ((element_kind != PARQUET_THRIFT_STRUCT) || (count > PARQUET_ROW_GROUPS_MOST))
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
            // the count was held to PARQUET_ROW_GROUPS_MOST above and fits an unsigned int
            footer->row_group_count = (unsigned int)count;
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
    return schema_seen;
}

int parquet_footer_read(const EngineIngestTools *tools, const char *path, ParquetFooter *footer, EngineError *error)
{
    memset(footer, 0, sizeof(*footer));
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
    // the size was held at twelve or more above, and the conversion is exact
    const unsigned long long file_size = (unsigned long long)file_bytes;
    unsigned char trailer[PARQUET_TRAILER_BYTES];
    EngineFileRange range;
    range.path = path;
    range.offset = file_size - PARQUET_TRAILER_BYTES;
    range.bytes = PARQUET_TRAILER_BYTES;
    range.out = trailer;
    if (!PARQUET_CHECK((tools->read(&range) == (long long)PARQUET_TRAILER_BYTES) &&
                           (memcmp(trailer + PARQUET_MAGIC_BYTES, PARQUET_MAGIC, PARQUET_MAGIC_BYTES) == 0),
                       path, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    const unsigned long long footer_bytes = parquet_little(trailer, 4u);
    if (!PARQUET_CHECK((footer_bytes <= PARQUET_FOOTER_BYTES_MOST) &&
                           (footer_bytes <= (file_size - PARQUET_MAGIC_BYTES - PARQUET_TRAILER_BYTES)),
                       trailer, error, ENGINE_ERROR_REQUEST))
    {
        return 0;
    }
    unsigned char *const text = (unsigned char *)malloc((size_t)footer_bytes + 1u);
    if (!PARQUET_CHECK(text != NULL, &text, error, ENGINE_ERROR_RESOURCE))
    {
        return 0;
    }
    range.offset = file_size - PARQUET_TRAILER_BYTES - footer_bytes;
    range.bytes = footer_bytes;
    range.out = text;
    ParquetCursor cursor = {text, footer_bytes, 0ull, 0u};
    // the footer's bytes are held to 16 MiB above, and the count converts exactly
    const int read = (tools->read(&range) == (long long)footer_bytes);
    const int decoded = read && parquet_file_metadata_read(&cursor, footer) && (cursor.broken == 0u);
    free(text);
    footer->footer_at = range.offset;
    footer->footer_bytes = footer_bytes;
    return PARQUET_CHECK(read, path, error, ENGINE_ERROR_REQUEST) &&
           PARQUET_CHECK(decoded, footer, error, ENGINE_ERROR_REQUEST);
}
