# BAR0: the part's registers, listened to directly

The most direct listen the run channel has on the part. NVML's Xid stamp and the Windows System log report a fault
second-hand and late; the part's own state sits first in its BAR0 register aperture: the 16 MiB block of memory-mapped
registers the card exposes, the PGRAPH exception registers (an out-of-range register warp exception among them) inside
it. This module maps that aperture read-only and reads 32-bit registers out of it.

It only ever reads. The aperture is mapped `PAGE_READONLY`, the kernel unable to write the part whatever an IOCTL asks,
and the driver exposes no write control code. That matches the run channel's rule everywhere else: the part is listened
to, never written.

## Pieces

- `mmio.h`: the device name, the control codes, and the shape of a read and of the mapped extent, shared by the
  driver and the reader.
- `bar0_driver.c`: a software kernel driver. It reads the aperture's physical base and size from its own service key
  (`Bar0Base`, `Bar0Size` under Parameters), maps them read-only, and answers two controls: the extent it mapped, and a
  bounded read of 32-bit registers. It binds to no PCI device; it does not contend with the vendor's driver for the
  part.
- `mmio_read.c` / `mmio_read.h`, the userspace reader: open the device, ask the extent, read registers.
- `mmio_read_check.c`: reads `PMC_BOOT_0` at offset 0, the register that names the chip. A value that is neither 0 nor
  all ones proves the aperture is mapped and the reads reach the part.
- `build_bar0.ps1`: compile and link the driver to `build/harness/mmio/bin/bar0_driver.sys` with `cl` and `link`
  against the WDK. `bar0_driver.vcxproj` builds it instead under a full WDK Visual Studio setup.
- `install_bar0.ps1`: find BAR0, load the service, start it.

## Build and load

Building the driver needs the Windows Driver Kit (its kernel headers and libraries) and MSVC, from Visual Studio 2022
or the Build Tools. Loading a self-signed driver needs the machine in test-signing mode.

1. Build the driver:

   ```
   pwsh -File build_bar0.ps1
   ```

2. Turn on test signing, once, then reboot:

   ```
   bcdedit /set testsigning on
   ```

3. Sign the driver, load its service, and point it at BAR0 (elevated):

   ```
   pwsh -File install_bar0.ps1 -Sign
   ```

4. Read the part:

   ```
   build\harness\mmio\mmio_read_check.exe
   ```

   `PMC_BOOT_0` reads as a live chip-id value, the aperture mapped at `0xFA000000`, 16 MiB.

Give `mmio_read_check` a hex offset and a word count to read any registers: `mmio_read_check 0x1340 4`.

To remove the service: `pwsh -File install_bar0.ps1 -Remove`.

The protocol asks the clock through this listener: `orior protocol learn clock [<from> [<words>]]` puts the ADVANCES
qualifier at each word of a span of the aperture and keeps the first that advances in the part's `.ksc` as
`run answers <offset> clock bus` (`bus_clock.c`, beside the carrier). Where the service is not running the command says
the install above and asks nothing.

## Reading, not probing

With the register state read directly, the register boundary is known from the part's own report, not found by an ask
that walks a register count up until the part faults. The carrier already reads the same count through the driver
(`cubin_run.c`, the `registers <declared> <part>` line, from `cuFuncGetAttribute`); BAR0 is the richer listen behind
it. Some device registers clear when read; read only the offsets whose meaning is known.
