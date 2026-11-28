// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mmio.h: the words the userspace reader and the kernel driver share to listen to the GPU's BAR0 register aperture
// (mmio_read.h, bar0_driver.c): the device the driver creates and the reader opens, the control codes the driver
// answers, and the shape of a read and of the mapped extent. The driver maps BAR0 read-only and answers reads alone;
// it exposes no way to write the part, matching the whole run channel's rule that the part is listened to, never
// written.
#ifndef MMIO_H
#define MMIO_H

// The device the driver creates and the path the reader opens it by: the GPU's BAR0 register aperture. The kernel name
// and the DOS name the driver makes; the user path the reader passes to CreateFile
#define MMIO_DEVICE_NAME L"\\Device\\GpuBar0"
#define MMIO_SYMLINK_NAME L"\\DosDevices\\GpuBar0"
#define MMIO_USER_PATH "\\\\.\\GpuBar0"

// The control codes that the driver answers, both reads and neither a write. They are the extent of the aperture that
// it mapped, and a read of 32-bit registers from it. CTL_CODE and the device and method names come from the system:
// winioctl.h for the reader, and wdm.h for the driver
#define MMIO_IOCTL_EXTENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define MMIO_IOCTL_READ CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)

// the max 32-bit registers one read returns, a request naming a reply the driver's buffer contains
#define MMIO_READ_WORDS_MAX 1024u

// A read of `words` 32-bit registers at byte `offset` into BAR0, the offset a whole count of 32-bit words. The reply is
// that many 32-bit words, in the order read
typedef struct
{
    unsigned int offset;
    unsigned int words;
} MmioRead;

// The extent of the aperture that the driver mapped: its physical base and its size in bytes. The install step gave
// them to the service of the driver (install_bar0.ps1)
typedef struct
{
    unsigned long long base;
    unsigned long long size;
} MmioExtent;

#endif
