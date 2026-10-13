// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_unprinted.c: bits the disassembler does not print, asked of the part. A question is a few
// instructions put in place of the kernel's IADD3, one of them the instruction whose bits are turned. That instruction
// is assembled through the machine file, every value of the field is written into its encoding in turn, and each
// encoding is run on the part over one case. What the part answers for each value is kept, against the answer the
// printed text says. A value that answers as printed leaves the field without effect on this question; one that
// answers otherwise, or does not run, is the part reading the field.
//
// No disassembler is asked. The pattern cubin, the kernel's text and the runner are an earlier run's. The runner
// loads a cubin and runs its kernel over one case of eight words.
//
//     interface_sass_probe_unprinted <runner> <pattern cubin> <kernel text> <machine file> <folder> <record>
//
// The kernel's text opens with its section's label, `.text.<kernel>:`, which names the kernel the cubin is written
// into. The kernel loads the case's first two words into R0 and R7 and stores R7 as the first word of the answer.
#include "interface_sass_probe_unprinted/unprinted_internal.h"

int main(int count, char **words)
{
    const int forms = (count == 8) && (strcmp(words[7], "forms") == 0);
    if ((count != 7) && !forms)
    {
        fprintf(stderr, "interface_sass_probe_unprinted <runner> <pattern cubin> <kernel text> <machine file> <folder> "
                        "<record> [forms]\n");
        return 2;
    }
    s_runner = words[1];
    s_folder = words[5];
    s_pattern_size = unprinted_file_read(words[2], s_pattern, sizeof(s_pattern));
    const unsigned long long kernel_text_size =
        unprinted_file_read(words[3], (unsigned char *)s_kernel_text, sizeof(s_kernel_text) - 1u);
    s_kernel_text[kernel_text_size] = '\0';
    if ((s_pattern_size == 0ull) || (kernel_text_size == 0ull) || !unprinted_kernel(s_kernel_text, s_kernel, sizeof(s_kernel)))
    {
        fprintf(stderr, "the pattern cubin %s or the kernel %s did not read\n", words[2], words[3]);
        return 2;
    }
    if (!sass_machine_read(&s_machine, words[4]))
    {
        fprintf(stderr, "the machine file %s did not read\n", words[4]);
        return 2;
    }
    FILE *const record = fopen(words[6], "wb");
    if (record == NULL)
    {
        fprintf(stderr, "the record %s could not be written\n", words[6]);
        return 2;
    }
    if (forms)
    {
        unprinted_forms(record);
        fclose(record);
        return 0;
    }
    const unsigned int questions = (unsigned int)(sizeof(s_questions) / sizeof(s_questions[0]));
    fprintf(record, "# Bits the disassembler does not print, asked of the part\n\n"
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
    fprintf(record, "\n| question | instruction | bits | the form holds | value | answer |\n"
                    "|---|---|---|---|---|---|\n");
    unsigned int put = 0u;
    for (unsigned int number = 0u; number < questions; number += 1u)
    {
        put += (unprinted_ask(&s_questions[number], number + 1u, record) >= 0) ? 1u : 0u;
    }
    fclose(record);
    printf("interface sass unprinted: %u of %u questions put, the record written to %s\n", put, questions, words[6]);
    return (put == questions) ? 0 : 1;
}
