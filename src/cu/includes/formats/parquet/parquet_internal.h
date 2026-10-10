// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the parquet_*.c pieces share: its includes, bounds, types and the functions one piece calls in another
#ifndef PARQUET_INTERNAL_H
#define PARQUET_INTERNAL_H

#include "parquet.h"

#include "../../codecs/zip/zip.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARQUET_CHECK(condition_, evacaddr_, error_, kind_)                                                            \
    engine_error_check((condition_), (kind_), ENGINE_MODULE_PARQUET, (unsigned int)__LINE__,                           \
                       (const void *)(evacaddr_), (error_))

// the four bytes that open and close a parquet file, and the trailer before the closing four: the footer's length,
// four bytes little-endian
#define PARQUET_MAGIC "PAR1"
#define PARQUET_MAGIC_BYTES 4u
#define PARQUET_TRAILER_BYTES 8u

// the most leaves a schema and row groups a footer may name, a leaf's dotted path with its terminator, the most
// bytes a footer may hold, and the deepest a schema and a skipped Thrift value may nest
#define PARQUET_LEAVES_MOST 64u
#define PARQUET_ROW_GROUPS_MOST 256u
#define PARQUET_PATH_BYTES 128u
#define PARQUET_FOOTER_BYTES_MOST (16ull * 1024ull * 1024ull)
#define PARQUET_SCHEMA_DEPTH_MOST 16u
#define PARQUET_THRIFT_DEPTH_MOST 16u

// a level is at most the schema's depth and the leaf's own one, and is kept in a byte
_Static_assert((PARQUET_SCHEMA_DEPTH_MOST + 1u) <= UCHAR_MAX,
               "PARQUET_SCHEMA_DEPTH_MOST: a definition or repetition level must fit the byte it is kept in");

// Thrift compact protocol type codes. A struct field's code 0 is the stop byte
#define PARQUET_THRIFT_STOP 0u
#define PARQUET_THRIFT_TRUE 1u
#define PARQUET_THRIFT_FALSE 2u
#define PARQUET_THRIFT_BYTE 3u
#define PARQUET_THRIFT_I16 4u
#define PARQUET_THRIFT_I32 5u
#define PARQUET_THRIFT_I64 6u
#define PARQUET_THRIFT_DOUBLE 7u
#define PARQUET_THRIFT_BINARY 8u
#define PARQUET_THRIFT_LIST 9u
#define PARQUET_THRIFT_SET 10u
#define PARQUET_THRIFT_MAP 11u
#define PARQUET_THRIFT_STRUCT 12u

// the physical types parquet stores, numbered as the format numbers them
typedef enum
{
    PARQUET_BOOLEAN = 0,
    PARQUET_INT32 = 1,
    PARQUET_INT64 = 2,
    PARQUET_INT96 = 3,
    PARQUET_FLOAT = 4,
    PARQUET_DOUBLE = 5,
    PARQUET_BYTE_ARRAY = 6,
    PARQUET_FIXED_LEN_BYTE_ARRAY = 7
} ParquetPhysical;

// the codecs parquet names, numbered as the format numbers them
typedef enum
{
    PARQUET_UNCOMPRESSED = 0,
    PARQUET_SNAPPY = 1,
    PARQUET_GZIP = 2,
    PARQUET_LZO = 3,
    PARQUET_BROTLI = 4,
    PARQUET_LZ4 = 5,
    PARQUET_ZSTD = 6,
    PARQUET_LZ4_RAW = 7
} ParquetCodec;

// one leaf column of the schema: its dotted path, its type and its two level ceilings
typedef struct
{
    char path[PARQUET_PATH_BYTES];
    ParquetPhysical physical;
    unsigned int fixed_bytes;
    unsigned int definition_most;
    unsigned int repetition_most;
} ParquetLeaf;

// one column chunk's place in the file and the sizes the footer states for it
typedef struct
{
    unsigned long long first_byte;
    unsigned long long compressed_bytes;
    unsigned long long uncompressed_bytes;
    unsigned long long level_count;
    ParquetCodec codec;
} ParquetChunk;

typedef struct
{
    unsigned long long rows;
    ParquetChunk chunk[PARQUET_LEAVES_MOST];
} ParquetRowGroup;

// the footer as this reader keeps it, and where its bytes sit in the file. Large: it is held on the heap
typedef struct
{
    unsigned long long rows;
    unsigned long long footer_at;
    unsigned long long footer_bytes;
    unsigned int leaf_count;
    unsigned int row_group_count;
    ParquetLeaf leaf[PARQUET_LEAVES_MOST];
    ParquetRowGroup row_group[PARQUET_ROW_GROUPS_MOST];
} ParquetFooter;

// bytes that grow as they are appended to
typedef struct
{
    unsigned char *bytes;
    unsigned long long count;
    unsigned long long room;
} ParquetBytes;

// one decoded column chunk: every present value's bytes in the file's order, a BYTE_ARRAY value's length four bytes
// little-endian each, and every level a byte
typedef struct
{
    ParquetBytes values;
    ParquetBytes lengths;
    unsigned char *definition;
    unsigned char *repetition;
    unsigned long long levels;
    unsigned long long present;
    unsigned long long rows;
} ParquetColumn;

// a cursor over bytes held in memory. Any overrun sets broken, and every later read returns zero
typedef struct
{
    const unsigned char *bytes;
    unsigned long long byte_count;
    unsigned long long position;
    unsigned int broken;
} ParquetCursor;

unsigned long long parquet_little(const unsigned char *bytes, unsigned int count);

unsigned int parquet_byte_read(ParquetCursor *cursor);

unsigned long long parquet_varint_read(ParquetCursor *cursor);

unsigned long long parquet_nonnegative_read(ParquetCursor *cursor);

const unsigned char *parquet_binary_read(ParquetCursor *cursor, unsigned long long *length);

unsigned long long parquet_list_header_read(ParquetCursor *cursor, unsigned int *element_kind);

unsigned int parquet_field_read(ParquetCursor *cursor, long long *field_id);

void parquet_value_skip(ParquetCursor *cursor, unsigned int kind);

int parquet_bytes_append(ParquetBytes *bytes, const unsigned char *from, unsigned long long count);

int parquet_footer_read(const EngineIngestTools *tools, const char *path, ParquetFooter *footer, EngineError *error);

int parquet_column_read(const EngineIngestTools *tools, const char *path, const ParquetFooter *footer,
                        unsigned int row_group, unsigned int leaf, ParquetColumn *column, EngineError *error);

void parquet_column_release(ParquetColumn *column);

#endif
