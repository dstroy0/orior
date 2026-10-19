// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_read.c: a .qry's records read where they lie, each held to its CRC
#include "qry_read.h"

#include "../../codecs/crc/crc_key.h"

#include <string.h>

static const unsigned long long s_crc_table[256] = CRC_KEY_TABLE;

unsigned long long qry_record_crc(const unsigned char *bytes, unsigned long long size)
{
    unsigned long long crc = ~0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        crc = s_crc_table[(crc ^ bytes[at]) & 0xffull] ^ (crc >> 8u);
    }
    return ~crc;
}

int qry_record_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at,
                    const QryRecord **record)
{
    QryRecord head;
    if ((*at > size) || ((size - *at) < sizeof(head)))
    {
        return 0;
    }
    memcpy(&head, bytes + *at, sizeof(head));
    const unsigned long long whole = QRY_RECORD_BYTES(head.name_length, head.blob_length);
    if ((head.name_length == 0u) || (head.name_length > QRY_NAME_LONGEST) || (head.blob_length > size) ||
        (whole > (size - *at)))
    {
        return 0;
    }
    unsigned long long held = 0ull;
    memcpy(&held, bytes + *at + whole - 8ull, sizeof(held));
    if (qry_record_crc(bytes + *at, whole - 8ull) != held)
    {
        return 0;
    }
    *record = (const QryRecord *)(bytes + *at);
    *at += whole;
    return 1;
}
