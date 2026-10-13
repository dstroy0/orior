// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mmio_read.c: the userspace reader of the GPU's BAR0 register aperture through the kernel driver (mmio_read.h). It
// opens the driver's device read-only, asks the extent it mapped, and reads 32-bit registers from it by IOCTL. No call
// writes the part: the driver answers reads alone.
#include "mmio_read.h"

#include <winioctl.h>

#include "mmio.h"

HANDLE mmio_open(void)
{
    return CreateFileA(MMIO_USER_PATH, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0u, NULL);
}

void mmio_close(HANDLE device)
{
    if ((device != NULL) && (device != INVALID_HANDLE_VALUE))
    {
        CloseHandle(device);
    }
}

int mmio_extent(HANDLE device, unsigned long long *base, unsigned long long *size)
{
    MmioExtent extent;
    DWORD given = 0ul;
    if (!DeviceIoControl(device, MMIO_IOCTL_EXTENT, NULL, 0ul, &extent, (DWORD)sizeof(extent), &given, NULL) ||
        (given != (DWORD)sizeof(extent)))
    {
        return 0;
    }
    *base = extent.base;
    *size = extent.size;
    return 1;
}

int mmio_read(HANDLE device, unsigned int offset, unsigned int words, unsigned int *out)
{
    if ((words == 0u) || (words > MMIO_READ_WORDS_MOST))
    {
        return 0;
    }
    MmioRead request;
    request.offset = offset;
    request.words = words;
    const DWORD wanted = (DWORD)(words * (unsigned int)sizeof(unsigned int));
    DWORD given = 0ul;
    if (!DeviceIoControl(device, MMIO_IOCTL_READ, &request, (DWORD)sizeof(request), out, wanted, &given, NULL) ||
        (given != wanted))
    {
        return 0;
    }
    return 1;
}
