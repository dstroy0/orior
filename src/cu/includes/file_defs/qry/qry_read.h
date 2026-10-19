// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_read.h: a .qry's records read where they lie (qry.h, qry.oracle.tsv)
#ifndef QRY_READ_H
#define QRY_READ_H

#include "qry.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // The record at `*at` of the `size` bytes at `bytes`, held whole and to its CRC: the record where it lies in
    // `*record`, its name after its head and its blob after its name, and `*at` moved past it. 1, or 0 where no
    // record stands at `*at` or its bytes do not hold to their CRC
    int qry_record_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                        const QryRecord **record);

    // the CRC-64/XZ of the `size` bytes at `bytes`, as a record's last word holds it of every byte before it
    unsigned long long qry_record_crc(const unsigned char *bytes, unsigned long long size);

#ifdef __cplusplus
}
#endif

#endif
