// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// funnel.h: the vendor's tables read off a capture of its disassembler: the records, the instruction set unpacked
// from its archive, and the latencies gathered from the copies out of their buffer
#ifndef TABLES_FUNNEL_H
#define TABLES_FUNNEL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // bytes the funnel owns, grown as it writes them; `bytes` is NULL and the counts 0 before the first write and
    // after tables_text_freed
    typedef struct
    {
        unsigned char *bytes;
        size_t length;
        size_t room;
    } TablesText;

    // one record of a capture: the copy's destination and source, 0 for the source of a block, and the bytes
    typedef struct
    {
        unsigned long long to;
        unsigned long long from;
        const unsigned char *bytes;
        size_t size;
    } TablesRecord;

    // `count` bytes at `bytes` added to the end of `text`: 1, or 0 where there was no memory, the text as it was
    int tables_text_added(TablesText *text, const void *bytes, size_t count);

    void tables_text_freed(TablesText *text);

    // The record at `*at` of the capture `bytes`, `size` long, read into `record` and `*at` moved past it: a line
    // `<dst src size>`, the addresses in hex and the count in decimal, then the count's bytes. 1, or 0 at the end
    // of the capture or where no record stands at `*at`
    int tables_record_read(const unsigned char *bytes, size_t size, size_t *at, TablesRecord *record);

    // 1 where `bytes`, `size` long, is an archive: its first word its own byte count, its second 0, and then
    // blocks each headed by its unpacked and packed byte counts, the first block unpacked to TABLES_BLOCK bytes
    int tables_archive(const unsigned char *bytes, size_t size);

    // The archive `bytes`, `size` long, unpacked into `text`: each block an LZ4 block whose matches reach back into
    // every block before it. 1, or 0 where a block does not unpack to the count its header gives, the text holding
    // what unpacked before it
    int tables_unpacked(const unsigned char *bytes, size_t size, TablesText *text);

    // The capture `bytes`, `size` long, read for the vendor's two tables: `instructions` the first archive among
    // its records unpacked, `latencies` the copies out of the buffer that holds `OPERATION SETS` at its start, in
    // their order. The count of tables found, 0 to 2, the instruction set counted where its archive unpacked whole
    unsigned int tables_funnelled(const unsigned char *bytes, size_t size, TablesText *instructions,
                                  TablesText *latencies);

#ifdef __cplusplus
}
#endif

#define TABLES_BLOCK 0x4000u

#endif
