// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening.h: the place the least opening of the kernel the system accepted begins at, as the part answered it
#ifndef KHW_OPENING_H
#define KHW_OPENING_H

// the word of `run answers <k> opening least` in the part's .ksc at `ksc_path`, the place the least opening a kernel
// runs with begins at (The frame asked, Open 6.3); 0 where no such row stands or the file is not there
unsigned int khw_opening_read(const char *ksc_path);

#endif
