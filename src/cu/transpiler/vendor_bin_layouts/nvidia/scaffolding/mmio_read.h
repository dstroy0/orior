// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mmio_read.h: the userspace reader of the BAR0 register aperture of the GPU through the kernel driver (mmio.h,
// bar0_driver.c). It opens the device of the driver, and reads 32-bit registers from the mapped aperture. It never
// writes, because the driver exposes no write. This is the host side of the most direct listen of the run channel on
// the part.
#ifndef MMIO_READ_H
#define MMIO_READ_H

#if defined(_WIN32)
#include <windows.h>

// The device of the driver, opened for a read: a handle to close with mmio_close. The return is INVALID_HANDLE_VALUE
// where the system did not load the driver, or did not start its service
HANDLE mmio_open(void);

// the device handle closed
void mmio_close(HANDLE device);

// The extent of the aperture the driver mapped into `base` and `size`: 1, or 0 where the driver did not answer
int mmio_extent(HANDLE device, unsigned long long *base, unsigned long long *size);

// `words` 32-bit registers at byte `offset` into BAR0 read into `out`, sized for `words`: 1, or 0 where the driver
// refused the read or gave back fewer words. The offset is a whole count of 32-bit words and the read stays inside the
// aperture, or the driver refuses it
int mmio_read(HANDLE device, unsigned int offset, unsigned int words, unsigned int *out);

#endif
#endif
