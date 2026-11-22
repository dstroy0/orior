// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// monolith_scheduler.c: the scheduler's bits NVIDIA's compiler writes, read off its cubins and gathered by operation.
// No listing is read and no disassembler is run: each cubin's code sections are read from the ELF, each instruction is
// named by our reader through the machine file, and its bits 105 to 127 are read through the fields the machine file
// names (sass_machine.h).
//
//     monolith_scheduler <machine file> <record> <cubin>...
//
// For each operation the record contains how often NVIDIA wrote it, the stalls it gave it, how often it set a write or
// a read barrier and how often it waited, and two readings of what the scheduler's bits are for:
//
// - where the operation sets no write barrier, the fewest cycles NVIDIA leaves between it and the first instruction
//   that reads the register it writes, the stalls between them summed. That count is the soonest NVIDIA lets the result
//   be read, and applies to the operation wherever it is written
// - where the operation sets a write barrier, how often a wait on that barrier is between it and the first
//   instruction that reads its register, against how often none does. A barrier counts its producers, and a wait on it
//   blocks until every one of them is back: the wait may are at the reader or at any instruction before it
// - where the operation sets none and a later instruction of the same operation sets one, waited on before the read:
//   the earlier result is blocked behind the later one's barrier, the two coming back in the order they were put
// - where the operation sets none, no later one blocks it, and an earlier instruction of the same operation sets one,
//   waited on before the read: the result follows the earlier one's out of the same unit, and the fewest cycles NVIDIA
//   leaves between that wait and the read
//
// and whether the machine file's form for the operation records the barrier NVIDIA writes.
#include "monolith_scheduler/scheduler_internal.h"
#include "../../../../../platform/platform.h"
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
