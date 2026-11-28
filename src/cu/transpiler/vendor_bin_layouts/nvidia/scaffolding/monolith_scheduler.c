// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// monolith_scheduler.c: the scheduler bits that the compiler of NVIDIA writes, read from its cubins and gathered by
// operation. The program reads no listing and runs no disassembler. It reads the code sections of each cubin from the
// ELF, and our reader names each instruction through the machine file. The program reads its bits 105 to 127 through
// the fields that the machine file names (sass_machine.h).
//
//     monolith_scheduler <machine file> <record> <cubin>...
//
// For each operation, the record holds how often NVIDIA wrote it and the stalls that it gave it. It holds how often it
// set a write or a read barrier, and how often it waited. It also holds two readings of what the scheduler bits are
// for:
//
// - where the operation sets no write barrier, the fewest cycles that NVIDIA leaves between it and the first reader of
//   its register. That is the sum of the stalls between them. That count is the soonest that NVIDIA lets a reader read
//   the result. It applies to the operation wherever the compiler writes it.
// - where the operation sets a write barrier, how often a wait on that barrier is between it and the first reader of
//   its register. The record puts that against how often none is. A barrier counts its producers, and a wait on it
//   blocks until every one of them is back. The wait can be at the reader, or at any instruction before it.
// - where the operation sets none, and a later instruction of the same operation sets one, waited on before the read.
//   Then the barrier of the later one blocks the earlier result, and the two come back in the order that they went.
// - where the operation sets none, no later one blocks it, and an earlier instruction of the same operation sets one,
//   waited on before the read. Then the result follows that of the earlier one out of the same unit. The record holds
//   the fewest cycles that NVIDIA leaves between that wait and the read.
//
// and whether the form of the machine file for the operation records the barrier that NVIDIA writes.
#include "monolith_scheduler/scheduler_internal.h"
#include "../../../../../../includes/platform/platform.h"
#include "../../../../engine/engine_note.h"

int program_main(int count, char **words)
{
    if ((count < 4) || !sass_machine_read(&s_machine, words[1]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "monolith_scheduler <machine file> <record> <cubin>...");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    for (int at = 3; at < count; at += 1)
    {
        PlatformFile *const file = platform_file_open(words[at], "rb");
        if (file == NULL)
        {
            continue;
        }
        const size_t size = platform_file_read_items(s_cubin, 1u, sizeof(s_cubin), file);
        platform_file_close(file);
        s_cubins += ((size < sizeof(s_cubin)) && scheduler_cubin(size)) ? 1u : 0u;
    }
    if (!scheduler_record(words[2]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the record ");
            mmgr_line_put(&engine_note.line, words[2]);
            mmgr_line_put(&engine_note.line, " was not written");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "monolith scheduler: ");
        mmgr_line_decimal(output_line, s_cubins, 1u);
        mmgr_line_put(output_line, " cubins, ");
        mmgr_line_decimal(output_line, s_instructions, 1u);
        mmgr_line_put(output_line, " instructions, ");
        mmgr_line_decimal(output_line, s_unread, 1u);
        mmgr_line_put(output_line, " no form reads, ");
        mmgr_line_decimal(output_line, s_operations, 1u);
        mmgr_line_put(output_line, " operations");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
