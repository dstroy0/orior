// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_unprinted.c: bits the disassembler does not print, probed on the part. A question is a few
// instructions put in place of the kernel's IADD3, one of them the instruction whose bits are turned. That instruction
// is assembled through the machine file, every value of the field is written into its encoding in turn, and each
// encoding is run on the part over one case. What the part answers for each value is recorded, against the answer the
// printed text specifies. A value that answers as printed leaves the field without effect on this question; one that
// answers otherwise, or does not run, is the part reading the field.
//
// No disassembler is invoked. The pattern cubin, the kernel's text and the runner are an earlier run's. The runner
// loads a cubin and runs its kernel over one case of eight words.
//
//     interface_sass_probe_unprinted <runner> <pattern cubin> <kernel text> <machine file> <folder> <record>
//
// The kernel's text opens with its section's label, `.text.<kernel>:`, which names the kernel the cubin is written
// into. The kernel loads the case's first two words into R0 and R7 and stores R7 as the first word of the answer.
#include "interface_sass_probe_unprinted/unprinted_internal.h"
#include "../../../../../platform/platform.h"
#include "../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_linea.h"

int program_main(int count, char **words)
{
    const int forms = (count == 8) && (mmgr_text_equal(words[7], "forms", PLATFORM_TEXT_TO_TERMINATOR));
    if ((count != 7) && !forms)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "interface_sass_probe_unprinted <runner> <pattern cubin> <kernel text> "
                                             "<machine file> <folder> <record> [forms]");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    s_runner = words[1];
    s_folder = words[5];
    s_pattern_size = unprinted_file_read(words[2], s_pattern, sizeof(s_pattern));
    const unsigned long long kernel_text_size =
        unprinted_file_read(words[3], (unsigned char *)s_kernel_text, sizeof(s_kernel_text) - 1u);
    s_kernel_text[kernel_text_size] = '\0';
    if ((s_pattern_size == 0ULL) || (kernel_text_size == 0ULL) ||
        !unprinted_kernel(s_kernel_text, s_kernel, sizeof(s_kernel)))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the pattern cubin ");
            mmgr_line_put(&engine_note.line, words[2]);
            mmgr_line_put(&engine_note.line, " or the kernel ");
            mmgr_line_put(&engine_note.line, words[3]);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    if (!sass_machine_read(&s_machine, words[4]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, words[4]);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    FILE *const record = fopen(words[6], "wb");
    if (record == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the record ");
            mmgr_line_put(&engine_note.line, words[6]);
            mmgr_line_put(&engine_note.line, " could not be written");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    if (forms)
    {
        unprinted_forms(record);
        fclose(record);
        return 0;
    }
    const unsigned int questions = (unsigned int)(sizeof(s_questions) / sizeof(s_questions[0]));
    fprintf(record, "# Bits the disassembler does not print, probed on the part\n\n"
                    "Written by `interface_sass_probe_unprinted` (`interface_sass_unprinted.sh`) whole on every run. "
                    "Each row is one value of the field written into the question's instruction and run on the part "
                    "over the case 0xb, 0x7. A value that answers as printed leaves the field without effect on that "
                    "question.\n\n"
                    "Each question's lines, put in place of the kernel's `IADD3`:\n\n");
    for (unsigned int number = 0u; number < questions; number += 1u)
    {
        fprintf(record, "%u. `", number + 1u);
        for (const char *walk = s_questions[number].lines; *walk != '\0'; walk += 1)
        {
            if (*walk == '\n')
            {
                fputs("`; `", record);
            }
            else
            {
                fputc(*walk, record);
            }
        }
        fprintf(record, "`\n");
    }
    fprintf(record, "\n| question | instruction | bits | the form contains | value | answer |\n"
                    "|---|---|---|---|---|---|\n");
    unsigned int put = 0u;
    for (unsigned int number = 0u; number < questions; number += 1u)
    {
        put += (unprinted_query(&s_questions[number], number + 1u, record) >= 0) ? 1u : 0u;
    }
    fclose(record);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass unprinted: ");
        mmgr_line_decimal(output_line, put, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, questions, 1u);
        mmgr_line_put(output_line, " questions put, the record written to ");
        mmgr_line_put(output_line, words[6]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (put == questions) ? 0 : 1;
}
