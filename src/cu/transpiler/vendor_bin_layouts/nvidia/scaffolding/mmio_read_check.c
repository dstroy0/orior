// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mmio_read_check.c: the reader checked against the part. It opens the driver's device, queries the extent it mapped,
// and reads PMC_BOOT_0, the register at offset 0 of BAR0 that names the chip: a value that is neither 0 nor all ones
// proves the aperture is mapped and the reads reach the part's own registers. Nothing is written.
//
//     mmio_read_check [<offset> [<words>]]
//
// With no argument it reads PMC_BOOT_0 alone. Given an offset in hex, and a count of 32-bit words, it reads that many
// registers from that offset and prints each. Exit 0 where the read looks live, 1 where it does not, 2 where the
// driver was not reached.
#include "../../../../../../includes/platform/platform.h"
#include "../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "mmio_read.h"

// the register at offset 0 of BAR0: the part's chip name, read to prove the aperture is live
#define MMIO_CHECK_BOOT_0 0x0u

int program_main(int count, char **words)
{
    const unsigned int offset = (count >= 2) ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                                        .src = words[1], .end = NULL, .base = 16)
                                             : MMIO_CHECK_BOOT_0;
    const unsigned int wanted = (count >= 3) ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                                        .src = words[2], .end = NULL, .base = 10)
                                             : 1u;
    HANDLE device = mmio_open();
    if (device == INVALID_HANDLE_VALUE)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line,
                          "the BAR0 driver was not reached: load its service first (install_bar0.ps1)");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    unsigned long long base = 0ULL;
    unsigned long long size = 0ULL;
    if (mmio_extent(device, &base, &size))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "BAR0 mapped at 0x");
        mmgr_line_hex(output_line, base, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, size, 1u);
        mmgr_line_put(output_line, " bytes");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static unsigned int s_read[1024];
    const unsigned int capped = (wanted == 0u) ? 1u : ((wanted > 1024u) ? 1024u : wanted);
    if (!mmio_read(device, offset, capped, s_read))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the read of ");
            mmgr_line_decimal(&engine_note.line, capped, 1u);
            mmgr_line_put(&engine_note.line, " words at 0x");
            mmgr_line_hex(&engine_note.line, offset, 1u);
            mmgr_line_put(&engine_note.line, " was refused");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        mmio_close(device);
        return 1;
    }
    for (unsigned int word = 0u; word < capped; word += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  0x");
        mmgr_line_hex(output_line, offset + (word * 4u), 6u);
        mmgr_line_put(output_line, "  ");
        mmgr_line_hex(output_line, s_read[word], 8u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
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
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "PMC_BOOT_0 = ");
            mmgr_line_hex(output_line, boot, 8u);
            mmgr_line_put(output_line, ": architecture 0x");
            mmgr_line_hex(output_line, architecture, 2u);
            mmgr_line_put(output_line, ", implementation 0x");
            mmgr_line_hex(output_line, implementation, 1u);
            mmgr_line_put(output_line, ", revision ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_hex(&linea_piece_line, (uint64_t)(major), 1u);
                const size_t linea_length = mmgr_line_finish(&linea_piece_line);
                for (size_t linea_at = 0u; linea_at < linea_length; linea_at += 1u)
                {
                    if (linea_piece[linea_at] >= 'a')
                    {
                        linea_piece[linea_at] = (char)(linea_piece[linea_at] - ('a' - 'A'));
                    }
                }
                (void)mmgr_line_put_n(output_line, linea_piece, linea_length);
            }
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_hex(&linea_piece_line, (uint64_t)(minor), 1u);
                const size_t linea_length = mmgr_line_finish(&linea_piece_line);
                for (size_t linea_at = 0u; linea_at < linea_length; linea_at += 1u)
                {
                    if (linea_piece[linea_at] >= 'a')
                    {
                        linea_piece[linea_at] = (char)(linea_piece[linea_at] - ('a' - 'A'));
                    }
                }
                (void)mmgr_line_put_n(output_line, linea_piece, linea_length);
            }
            mmgr_line_put(output_line, ": the aperture is ");
            mmgr_line_put(output_line, live ? "live" : "not reading the part");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    mmio_close(device);
    return live ? 0 : 1;
}
