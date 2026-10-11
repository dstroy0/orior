// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file parquet.h
 * @brief A parquet file's footer and one column chunk at a time, decoded to exact bytes.
 *
 * The footer is Thrift compact protocol. A column chunk is pages, each page a Thrift header and a
 * body the chunk's codec compressed. The bodies decode through the engine's own
 * Snappy and Zstd. A value is returned as the bytes the file stored: a DOUBLE stays its eight
 * little-endian bytes and no floating point value is formed here.
 *
 * Every buffer a read fills is sized beforehand by casmi_parquet_column_bound from facts the footer
 * states; a read allocates nothing and refuses and never overruns.
 */
#ifndef PARQUET_H
#define PARQUET_H

#include "casmi_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The physical types parquet stores, numbered as the format numbers them. */
typedef enum
{
    CASMI_PARQUET_BOOLEAN = 0,
    CASMI_PARQUET_INT32 = 1,
    CASMI_PARQUET_INT64 = 2,
    CASMI_PARQUET_INT96 = 3,
    CASMI_PARQUET_FLOAT = 4,
    CASMI_PARQUET_DOUBLE = 5,
    CASMI_PARQUET_BYTE_ARRAY = 6,
    CASMI_PARQUET_FIXED_LEN_BYTE_ARRAY = 7
} CasmiParquetPhysical;

/** The codecs parquet names, numbered as the format numbers them. */
typedef enum
{
    CASMI_PARQUET_UNCOMPRESSED = 0,
    CASMI_PARQUET_SNAPPY = 1,
    CASMI_PARQUET_GZIP = 2,
    CASMI_PARQUET_LZO = 3,
    CASMI_PARQUET_BROTLI = 4,
    CASMI_PARQUET_LZ4 = 5,
    CASMI_PARQUET_ZSTD = 6,
    CASMI_PARQUET_LZ4_RAW = 7
} CasmiParquetCodec;

/** One leaf column of the schema: its dotted path, its type and its two level ceilings. */
typedef struct
{
    char path[CASMI_PARQUET_PATH_BYTES];
    CasmiParquetPhysical physical;
    unsigned int fixed_bytes;
    unsigned int definition_most;
    unsigned int repetition_most;
} CasmiParquetLeaf;

/** One column chunk's place in the file and the sizes the footer states for it. */
typedef struct
{
    unsigned long long first_byte;
    unsigned long long compressed_bytes;
    unsigned long long uncompressed_bytes;
    unsigned long long level_count;
    CasmiParquetCodec codec;
} CasmiParquetChunk;

/** One row group: its row count and a chunk per leaf, in leaf order. */
typedef struct
{
    unsigned long long rows;
    CasmiParquetChunk chunk[CASMI_PARQUET_LEAVES_MOST];
} CasmiParquetRowGroup;

/** The whole footer as this reader keeps it. Large: hold it static or on the heap. */
typedef struct
{
    unsigned long long rows;
    unsigned int leaf_count;
    unsigned int row_group_count;
    CasmiParquetLeaf leaf[CASMI_PARQUET_LEAVES_MOST];
    CasmiParquetRowGroup row_group[CASMI_PARQUET_ROW_GROUPS_MOST];
} CasmiParquetFooter;

/** What casmi_parquet_footer_read needs: the file and where the footer goes. */
typedef struct
{
    const char *path;
    CasmiParquetFooter *footer;
} CasmiParquetFooterRead;

/**
 * One decoded column chunk.
 *
 * Row r holds the values from row_start[r] up to row_start[r + 1]. A flat column holds zero or one
 * value per row, zero where the row is null. A list column holds the list's present elements.
 *
 * A fixed-width value is fixed_bytes wide in value_bytes. A BYTE_ARRAY value is value_length[v]
 * bytes at value_bytes + value_offset[v]; a dictionary-encoded one points at its dictionary entry
 * and is not copied.
 *
 * The *_room members are set by casmi_parquet_column_bound and the pointers are the caller's,
 * allocated to at least those rooms before casmi_parquet_column_read.
 */
typedef struct
{
    unsigned long long rows;
    unsigned long long values;
    unsigned int fixed_bytes;
    unsigned long long *row_start;
    unsigned char *value_bytes;
    unsigned long long *value_offset;
    unsigned long long *value_length;
    unsigned char *scratch;
    unsigned long long row_start_room;
    unsigned long long value_bytes_room;
    unsigned long long value_offset_room;
    unsigned long long scratch_room;
} CasmiParquetColumn;

/** What a bound or a read of one column chunk needs. */
typedef struct
{
    const char *path;
    const CasmiParquetFooter *footer;
    unsigned int row_group;
    unsigned int leaf;
    CasmiParquetColumn *column;
} CasmiParquetColumnRead;

/** What casmi_parquet_leaf_find needs: the footer to search and the dotted path to find. */
typedef struct
{
    const CasmiParquetFooter *footer;
    const char *path_in_schema;
} CasmiParquetLeafFind;

/**
 * Read a parquet file's footer.
 *
 * @param args  the file and the footer to fill
 * @return      the leaf count, or ENGINE_BYTES_ERROR where the file is not parquet, the footer
 *              breaks the Thrift grammar, or the schema exceeds a bound in casmi_config.h
 */
long long casmi_parquet_footer_read(const CasmiParquetFooterRead *args);

/**
 * Set the rooms one column chunk's read will need, from the footer alone.
 *
 * @param args  the footer, the row group, the leaf, and the column whose rooms are set
 * @return      the scratch room, or ENGINE_BYTES_ERROR where the row group or leaf is out of range
 */
long long casmi_parquet_column_bound(const CasmiParquetColumnRead *args);

/**
 * Decode one column chunk into the caller's buffers.
 *
 * A refusal before any byte is read, for a row group, leaf or room out of bound, changes nothing. A
 * refusal during the decode leaves rows and values zero, since the buffers no longer hold what any
 * earlier count described.
 *
 * @param args  the file, the footer, the row group, the leaf, and the bounded column to fill
 * @return      the number of values decoded, or ENGINE_BYTES_ERROR on any encoding, codec or
 *              level this reader does not carry, or on a page the chunk's stated sizes do not hold
 */
long long casmi_parquet_column_read(const CasmiParquetColumnRead *args);

/**
 * Find the leaf whose dotted path is args->path_in_schema, such as "ms2_mzs.list.element".
 *
 * @param args  the footer to search and the dotted path to find
 * @return      the leaf index, or ENGINE_BYTES_ERROR where no leaf carries that path
 */
long long casmi_parquet_leaf_find(const CasmiParquetLeafFind *args);

#ifdef __cplusplus
}
#endif

#endif
