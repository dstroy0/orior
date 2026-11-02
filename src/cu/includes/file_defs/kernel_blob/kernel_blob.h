// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kernel_blob.h: the kernel the vendor's writer reads out of the container the system accepted, the blob of a record of
// the run (kernel_blob.oracle.tsv): its head, then every place's encoding as the container stores it, sixteen bytes a
// place
#ifndef KERNEL_BLOB_H
#define KERNEL_BLOB_H

#include <assert.h>

// the record's name the vendor's writer appends the kernel under
#define KERNEL_BLOB_NAME "kernel_blob"

// the probe place where the part's .ksc names none or names a place past the kernel
#define KERNEL_BLOB_PLACE_NONE 0xffffffffu

// the kernel's head: its places, the registers a thread of it uses, the place the least opening begins at, and the
// probe place
typedef struct
{
    unsigned int places;
    unsigned int registers;
    unsigned int from;
    unsigned int place;
} KernelBlobHead;

static_assert(sizeof(KernelBlobHead) == 16u, "KernelBlobHead must be the 16 bytes a kernel's blob opens with");

#endif
