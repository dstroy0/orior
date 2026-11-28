// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef BUS_ENUM_H
#define BUS_ENUM_H

// The enumeration region in a run of types that a classify walk read, one type for each sampled address
// (`host_entry.h`). A record is a FIXED identifier with a LIVE sizing register next to it. Records recur at one stride
// over the region, and an empty record leaves a gap that is a whole multiple of the stride. The code finds this with
// no address written in. It names the region by how the parts answered, not by where a standard says they lie.
#include "host_entry.h"

// 1 where the call finds a region of two or more records. It puts the sample of its first record into `base`, and
// the samples between the start of one record and the next into `stride`. It counts the records into `records`. The
// return is 0 where it finds none, with the outputs left as they were.
int bus_enum_find(const unsigned int *types, unsigned long long count, unsigned long long *base,
                  unsigned long long *stride, unsigned long long *records);

// one enumerated record: the sample it begins at, the identifier its FIXED register contains, and the address bits its
// LIVE sizing register decodes
typedef struct
{
    unsigned long long sample;
    unsigned int identifier;
    unsigned int width;
} BusRecord;

// the records of a found region, read into `out`, which has room for `max`. For each record, it is the value of the
// FIXED identifier and the width of the LIVE sizing register. They come from the values that a walk captured, one for
// each sample. `values[s]` is the constant of the FIXED register at a FIXED sample, and the all-ones mask of the sizing
// register at a LIVE sample. The return is the records written, or 0 where the call finds no region. The call passes
// over an empty record on the stride.
unsigned long long bus_enum_list(const unsigned int *types, const unsigned int *values, unsigned long long count,
                                 BusRecord *out, unsigned long long max);

#endif
