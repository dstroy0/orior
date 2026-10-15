// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mmio_read.h: the userspace reader of the GPU's BAR0 register aperture through the kernel driver (mmio.h,
// bar0_driver.c). It opens the driver's device and reads 32-bit registers from the mapped aperture; it never writes,
// the driver exposing no write. This is the host side of the run channel's most direct listen on the part.
#ifndef MMIO_READ_H
#define MMIO_READ_H

#if defined(_WIN32)
#include <windows.h>

// The driver's device opened for reading: a handle to close with mmio_close, or INVALID_HANDLE_VALUE where the driver
// is not loaded or its service is not started
HANDLE mmio_open(void);

// the device handle closed
void mmio_close(HANDLE device);

// The extent of the aperture the driver mapped into `base` and `size`: 1, or 0 where the driver did not answer
int mmio_extent(HANDLE device, unsigned long long *base, unsigned long long *size);

// `words` 32-bit registers at byte `offset` into BAR0 read into `out`, which holds `words`: 1, or 0 where the driver
// refused the read or gave back fewer words. The offset is a whole count of 32-bit words and the read stays inside the
// aperture, or the driver refuses it
int mmio_read(HANDLE device, unsigned int offset, unsigned int words, unsigned int *out);

#endif
#endif
