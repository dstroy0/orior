// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening.h: the place the least opening of the kernel the system accepted begins at, and the place its slot is, as
// the part answered each
#ifndef KHW_OPENING_H
#define KHW_OPENING_H

// the word of `run answers <k> opening least` in the part's .ksc at `ksc_path`, the place the least opening a kernel
// runs with begins at (The frame asked, Open 6.3); 0 where no such row stands or the file is not there
unsigned int khw_opening_read(const char *ksc_path);

// the slot's place where the part has not answered it
#define KHW_SLOT_NONE 0xffffffffu

// the word of `run answers <s> slot place` in the part's .ksc at `ksc_path`, the place of the kernel the system
// accepted whose instruction writes the register the kernel stores (The frame asked, Open 6.3); KHW_SLOT_NONE where no
// such row stands or the file is not there
unsigned int khw_slot_read(const char *ksc_path);

#endif
