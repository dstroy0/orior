// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// A parquet file as an ingest source: every leaf of every row group a member, each sealed whole
//
// A parquet file is a source in the way that an archive is. The ingest names its members and seals each.
//
// The members are the footer of the file, named `footer`, and each leaf of each row group. The name of a leaf
// is `<row group>/<leaf path>`, with the row group counted from 0. The leaf path has dots as the schema gives it, such
// as `0/ms2_mzs.list.element`.
//
// parquet_members lists the footer first. Then it lists each row group in the order of the file, and its leaves in the
// order of the schema. It drops nothing.
//
// The lanes of a member are its bytes, two to a lane. The first byte is the low half, and an odd last byte gets a pad
// of 0.
//   - for `footer`, the lanes are the own bytes of the footer.
//   - for a leaf, they are the values that its column chunk stores, in the order of the file. Each is the bytes that
//     the file stores it in.
//
// A DOUBLE stays its eight little-endian bytes, and the code forms no floating point value. A BYTE_ARRAY value is its
// bytes, one after the other. A null has no value and no bytes. A member of no bytes has one lane of 0.
//
// The side of the member is what the lanes do not carry. Each part is a leaf of the side bytes, stored exact beside the
// lanes:
//   - bytes: the byte count of the member, eight bytes little-endian, which strips the pad.
//   - definition: the definition levels of a leaf, a byte each, one a level, in the order of the file.
//   - repetition: its repetition levels, the same.
//   - lengths: the value lengths of a BYTE_ARRAY leaf, four bytes little-endian each, one a value.
//
// The levels tell each null and each empty list apart. A value is present where its definition level is the ceiling
// of the leaf. A row starts where its repetition level is 0.
//
// The reader reads pages V1 and V2, and PLAIN, PLAIN_DICTIONARY and RLE_DICTIONARY values. It reads RLE and bit-packed
// levels, and uncompressed, Snappy and Zstd bodies. It refuses anything else whole, and seals nothing of the member.
#ifndef PARQUET_H
#define PARQUET_H

#include "../../../src/cu/engine/engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // the member's lanes as an extent of rank 1 on axis x, two-byte unsigned elements. 0, or -1 where the file is not
    // parquet, the member is not one of its own, or its column chunk does not decode
    long parquet_describe(const EngineDescribeRequest *request);

    // the lanes of the member into request->out, and its side into request->side where the caller gives that. The
    // result is the written lane bytes. It is -1 where the member does not read, or the extent is not the one that
    // parquet_describe gave
    long long parquet_read(const EngineArrayRead *request);

    // every member of the parquet file at `path`, in the order above, each name allocated and the list ended by NULL,
    // into *names. The count, or 0 where the file is not parquet or its footer does not read
    unsigned int parquet_members(const EngineIngestTools *tools, const char *path, char ***names);

#ifdef __cplusplus
}
#endif

#endif
