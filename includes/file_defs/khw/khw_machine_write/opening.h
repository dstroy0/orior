// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// opening.h: the place the least opening of the kernel the system accepted begins at, and the probe place, as
// the part answered each
#ifndef KHW_OPENING_H
#define KHW_OPENING_H

// the word of `run answers <k> opening least` in the .ksc of the part at `ksc_path`. It is the place where the least
// opening of a kernel starts (Open 6.3). The return is 0 where no such row is or the file is not there
unsigned int khw_opening_read(const char *ksc_path);

// the probe place where the part has not answered it
#define KHW_PROBE_PLACE_NONE 0xffffffffu

// the word of `run answers <s> probe place` in the .ksc of the part at `ksc_path`. It is the place in the kernel that
// the system accepted of the instruction that writes the register that the kernel stores (Open 6.3). The return is
// KHW_PROBE_PLACE_NONE where no such row is or the file is not there
unsigned int khw_probe_place_read(const char *ksc_path);

// the field the part's .ksc at `ksc_path` names `guard`, `field guard <first> <bits>`, into `first` and `bits`: 1, or 0
// where no such row is or the file is not there
int khw_guard_field_read(const char *ksc_path, unsigned int *first, unsigned int *bits);

#endif
