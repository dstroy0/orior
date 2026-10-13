// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// monolith_scheduler.c: the scheduler's bits NVIDIA's compiler writes, read off its cubins and gathered by operation.
// No listing is read and no disassembler is run: each cubin's code sections are read from the ELF, each instruction is
// named by our reader through the machine file, and its bits 105 to 127 are read through the fields the machine file
// names (sass_machine.h).
//
//     monolith_scheduler <machine file> <record> <cubin>...
//
// For each operation the record holds how often NVIDIA wrote it, the stalls it gave it, how often it set a write or a
// read barrier and how often it waited, and two readings of what the scheduler's bits are for:
//
// - where the operation sets no write barrier, the fewest cycles NVIDIA leaves between it and the first instruction
//   that reads the register it writes, the stalls between them summed. That count is the soonest NVIDIA lets the result
//   be read, and holds for the operation wherever it is written
// - where the operation sets a write barrier, how often a wait on that barrier stands between it and the first
//   instruction that reads its register, against how often none does. A barrier counts its producers, and a wait on it
//   holds until every one of them is back: the wait may stand at the reader or at any instruction before it
// - where the operation sets none and a later instruction of the same operation sets one, waited on before the read:
//   the earlier result is held behind the later one's barrier, the two coming back in the order they were put
// - where the operation sets none, no later one holds it, and an earlier instruction of the same operation sets one,
//   waited on before the read: the result follows the earlier one's out of the same unit, and the fewest cycles NVIDIA
//   leaves between that wait and the read
//
// and whether the machine file's form for the operation records the barrier NVIDIA writes.
#include "monolith_scheduler/scheduler_internal.h"

int main(int count, char **words)
{
    if ((count < 4) || !sass_machine_read(&s_machine, words[1]))
    {
        fprintf(stderr, "monolith_scheduler <machine file> <record> <cubin>...\n");
        return 2;
    }
    for (int at = 3; at < count; at += 1)
    {
        FILE *const file = fopen(words[at], "rb");
        if (file == NULL)
        {
            continue;
        }
        const size_t size = fread(s_cubin, 1u, sizeof(s_cubin), file);
        fclose(file);
        s_cubins += ((size < sizeof(s_cubin)) && scheduler_cubin(size)) ? 1u : 0u;
    }
    if (!scheduler_record(words[2]))
    {
        fprintf(stderr, "the record %s was not written\n", words[2]);
        return 2;
    }
    printf("monolith scheduler: %u cubins, %llu instructions, %llu no form reads, %u operations\n", s_cubins,
           s_instructions, s_unread, s_operations);
    return 0;
}
