// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_listing.h: the vendor's listing of a cubin of many kernels cut into each kernel's listing, as the vendor
// prints a kernel's code read alone
#ifndef CUBIN_LISTING_H
#define CUBIN_LISTING_H

#include <stddef.h>

// Each kernel's listing cut out of one listing, all in one block of text: kernel `n`'s is `size[n]` bytes from
// `start[n]`, and `found[n]` 1 where its code section is in the listing
typedef struct
{
    char *text;
    size_t text_size;
    size_t text_room;
    size_t *start;
    size_t *size;
    int *found;
} CubinListingCut;

// The listing `listing`, `length` bytes the vendor printed for a cubin, cut into the listings of the kernels `names`,
// `count` of them, into `cut`, whose arrays hold `count`. A kernel's listing is every instruction line of its code
// section `.text.<name>` in order, each as the vendor printed it with every label it names, `` `(<label>) ``, put as
// the offset the listing gives that label, `0x<offset>`, the offset of the instruction the label stands before and
// the end of the section for one past the last: a kernel's lines then read as the vendor prints its code read alone. A
// label the section does not set is left as printed. 1, or 0 where the system gave no memory
int cubin_listing_cut(const char *listing, size_t length, const char *const *names, unsigned int count,
                      CubinListingCut *cut);

// the memory `cut` holds let go
void cubin_listing_cut_freed(CubinListingCut *cut);

#endif
