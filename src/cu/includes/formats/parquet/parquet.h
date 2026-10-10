// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// A parquet file as an ingest source: every leaf of every row group a member, each sealed whole
//
// A parquet file is a source the way an archive is: the ingest names its members and seals each. The members are the
// file's footer, named `footer`, and every leaf of every row group, named `<row group>/<leaf path>` with the row group
// counted from 0 and the leaf's dotted path as the schema gives it, `0/ms2_mzs.list.element`. parquet_members lists
// the footer first, then each row group in the file's order and its leaves in the schema's order. Nothing is dropped.
//
// A member's lanes are its bytes, two to a lane, the first byte the low half and an odd last byte padded with 0: the
// footer's own bytes for `footer`, and for a leaf the values its column chunk stores, in the file's order, each as
// the bytes the file holds it in. A DOUBLE stays its eight little-endian bytes and no floating point value is formed.
// A BYTE_ARRAY value is its bytes, one after the other. A null holds no value and no bytes. A member of no bytes has
// one lane of 0.
//
// What the lanes do not carry is the member's side, each part a leaf of the side bytes kept exact beside the lanes:
//   bytes       the member's byte count, eight bytes little-endian, which strips the pad;
//   definition  a leaf's definition levels, a byte each, one a level, in the file's order;
//   repetition  its repetition levels, the same;
//   lengths     a BYTE_ARRAY leaf's value lengths, four bytes little-endian each, one a value.
// The levels say every null and every empty list apart: a value is present where its definition level is the leaf's
// ceiling, and a row starts where its repetition level is 0.
//
// Pages V1 and V2, PLAIN, PLAIN_DICTIONARY and RLE_DICTIONARY values, RLE and bit-packed levels, and uncompressed,
// Snappy and Zstd bodies are read. Anything else is refused whole and nothing is sealed of the member.
#ifndef PARQUET_H
#define PARQUET_H

#include "../../../engine/engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // the member's lanes as an extent of rank 1 on axis x, two-byte unsigned elements. 0, or -1 where the file is not
    // parquet, the member is not one of its own, or its column chunk does not decode
    long parquet_describe(const EngineDescribeRequest *request);

    // the member's lanes into request->out and its side into request->side where that is given. The lane bytes written,
    // or -1 where the member does not read or the extent is not the one parquet_describe gave
    long long parquet_read(const EngineArrayRead *request);

    // every member of the parquet file at `path`, in the order above, each name allocated and the list ended by NULL,
    // into *names. The count, or 0 where the file is not parquet or its footer does not read
    unsigned int parquet_members(const EngineIngestTools *tools, const char *path, char ***names);

#ifdef __cplusplus
}
#endif

#endif
