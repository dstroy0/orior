// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mmio_read_check.c: the reader held to the part. It opens the driver's device, asks the extent it mapped, and reads
// PMC_BOOT_0, the register at offset 0 of BAR0 that names the chip: a value that is neither 0 nor all ones proves the
// aperture is mapped and the reads reach the part's own registers. Nothing is written.
//
//     mmio_read_check [<offset> [<words>]]
//
// With no argument it reads PMC_BOOT_0 alone. Given an offset in hex, and a count of 32-bit words, it reads that many
// registers from that offset and prints each. Exit 0 where the read looks live, 1 where it does not, 2 where the
// driver was not reached.
#include "mmio_read.h"

#include <stdio.h>
#include <stdlib.h>

// the register at offset 0 of BAR0: the part's chip name, read to prove the aperture is live
#define MMIO_CHECK_BOOT_0 0x0u

int main(int count, char **words)
{
    const unsigned int offset = (count >= 2) ? (unsigned int)strtoul(words[1], NULL, 16) : MMIO_CHECK_BOOT_0;
    const unsigned int wanted = (count >= 3) ? (unsigned int)strtoul(words[2], NULL, 10) : 1u;
    HANDLE device = mmio_open();
    if (device == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "the BAR0 driver was not reached: load its service first (install_bar0.ps1)\n");
        return 2;
    }
    unsigned long long base = 0ull;
    unsigned long long size = 0ull;
    if (mmio_extent(device, &base, &size))
    {
        printf("BAR0 mapped at 0x%llx, %llu bytes\n", base, size);
    }
    static unsigned int s_read[1024];
    const unsigned int capped = (wanted == 0u) ? 1u : ((wanted > 1024u) ? 1024u : wanted);
    if (!mmio_read(device, offset, capped, s_read))
    {
        fprintf(stderr, "the read of %u words at 0x%x was refused\n", capped, offset);
        mmio_close(device);
        return 1;
    }
    for (unsigned int word = 0u; word < capped; word += 1u)
    {
        printf("  0x%06x  %08x\n", offset + (word * 4u), s_read[word]);
    }
    const unsigned int boot = s_read[0];
    const int live = (boot != 0u) && (boot != 0xffffffffu);
    if (offset == MMIO_CHECK_BOOT_0)
    {
        // NV_PMC_BOOT_0 names the chip: its architecture in bits 28:24, its implementation in 23:20, and its revision
        // in the low byte, the major in 7:4 and the minor in 3:0. Architecture 0x17 is Ampere, and implementation 0x4
        // under it is the GA104 of an RTX 3070
        const unsigned int architecture = (boot >> 24u) & 0x1fu;
        const unsigned int implementation = (boot >> 20u) & 0xfu;
        const unsigned int major = (boot >> 4u) & 0xfu;
        const unsigned int minor = boot & 0xfu;
        printf("PMC_BOOT_0 = %08x: architecture 0x%02x, implementation 0x%x, revision %X%X: the aperture is %s\n", boot,
               architecture, implementation, major, minor, live ? "live" : "not reading the part");
    }
    mmio_close(device);
    return live ? 0 : 1;
}
