// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_listing.h: the vendor's listing of a cubin of many kernels cut into each kernel's listing, as the vendor
// prints a kernel's code read alone
#ifndef CUBIN_LISTING_H
#define CUBIN_LISTING_H

#include <stddef.h>

// The listing of each kernel, cut out of one listing, all in one block of text. That of kernel `n` is `size[n]` bytes
// from `start[n]`, and `found[n]` is 1 where its code section is in the listing
typedef struct
{
    char *text;
    size_t text_size;
    size_t text_room;
    size_t *start;
    size_t *size;
    int *found;
} CubinListingCut;

// The listing `listing`, `length` bytes that the vendor printed for a cubin. The call cuts it into the listings of the
// kernels `names`, `count` of them, into `cut`, whose arrays have room for `count`.
//
// The listing of a kernel is every instruction line of its code section `.text.<name>` in order. Each is as the
// vendor printed it, with every label that it names, `` `(<label>) ``, put as the offset that the listing gives that
// label, `0x<offset>`. That is the offset of the instruction that the label is before, or the end of the section for
// one past the last. Then the lines of a kernel read as the vendor prints its code when it reads it alone. A label
// that the section does not set stays as printed. The return is 1, or 0 where the system gave no memory
int cubin_listing_cut(const char *listing, size_t length, const char *const *names, unsigned int count,
                      CubinListingCut *cut);

// the memory `cut` owns let go
void cubin_listing_cut_freed(CubinListingCut *cut);

#endif
