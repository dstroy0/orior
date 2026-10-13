// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_fields.c: a form's fields found by running it on the part, not by reading a disassembler. An
// instruction is assembled through the machine file, put in place of the kernel's IADD3, and run once for its answer;
// then each of its 104 operation bits is turned over one at a time and run again. A turned bit that changes the answer
// or stops the instruction running is a bit the part reads; one that changes nothing is control or unused.
//
// Which operand a read bit belongs to is matched instead of guessed: the same instruction is run with each source operand's
// register number or number moved by a known amount, and a turned encoding bit belongs to the operand whose move
// gives the same answer. A read bit no operand's move explains is the operation's own, or a field the text does not
// print.
//
// Nothing is compiled and no disassembler is run. The cubins are written here and run by interface_sass_run, which
// loads each through the driver and answers over one case; a cubin the part refuses ends that runner, and this restarts
// it past the refusal.
//
//     interface_sass_probe_fields <pattern cubin> <kernel text> <machine file> <folder> <instructions>
//
// The instructions file holds one instruction a line, as sass_krs_assemble writes the forms a ruleset uses. The tool
// writes <folder>/list.txt and the cubins it names; interface_sass_run runs that list and writes the answers, which a
// later pass reads back against the form's runs. The kernel opens with its section's label, loads the case into
// registers and stores the answer's first word from R7.
#include "interface_sass_probe_fields/fields_internal.h"

int main(int count, char **words)
{
    if ((count == 6) && (strcmp(words[1], "read") == 0))
    {
        return fields_read(words[2], words[3], words[4], words[5]);
    }
    if ((count == 4) && (strcmp(words[1], "names") == 0))
    {
        return fields_names(words[2], words[3]);
    }
    if (count != 6)
    {
        fprintf(stderr,
                "interface_sass_probe_fields <pattern cubin> <kernel text> <machine file> <folder> <instructions>\n"
                "interface_sass_probe_fields read <machine file> <instructions> <answers> <record>\n"
                "interface_sass_probe_fields names <machine file> <instructions>\n");
        return 2;
    }
    const char *const folder = words[4];
    s_pattern_size = fields_file_read(words[1], s_pattern, sizeof(s_pattern));
    const unsigned long long kernel_text_size = fields_file_read(words[2], (unsigned char *)s_kernel_text, sizeof(s_kernel_text) - 1u);
    s_kernel_text[kernel_text_size] = '\0';
    if ((s_pattern_size == 0ull) || (kernel_text_size == 0ull) || !fields_kernel(s_kernel_text, s_kernel, sizeof(s_kernel)))
    {
        fprintf(stderr, "the pattern cubin %s or the kernel %s did not read\n", words[1], words[2]);
        return 2;
    }
    if (!sass_machine_read(&s_machine, words[3]))
    {
        fprintf(stderr, "the machine file %s did not read\n", words[3]);
        return 2;
    }
    fields_keys();
    FILE *const instructions = fopen(words[5], "rb");
    if (instructions == NULL)
    {
        fprintf(stderr, "the instructions %s did not read\n", words[5]);
        return 2;
    }
    // step one: the first instruction turned bit by bit into cubins and a list, which interface_sass_run runs
    char line[512];
    while (fgets(line, sizeof(line), instructions) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        const char *walk = line + strspn(line, " \t");
        if ((*walk == '\0') || (strncmp(walk, "IADD3 R8", 8u) == 0))
        {
            continue;
        }
        // the first operand moved to R8, the register the kernel stores as the answer, which then holds what the form
        // writes
        char redirected[512];
        char observe[64];
        if (!fields_redirect(walk, redirected, sizeof(redirected), observe, sizeof(observe)))
        {
            printf("skip %s: could not be redirected\n", walk);
            continue;
        }
        char lines[2048];
        snprintf(lines, sizeof(lines), FIELDS_LINES_HEAD "%s%s" FIELDS_LINES_TAIL, redirected, observe);
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        const unsigned int assembled = fields_splice(s_kernel_text, lines, s_asking, sizeof(s_asking))
                                           ? sass_assemble_lines(&s_machine, s_asking, SASS_CONTROL_SAFE, s_code,
                                                                 sizeof(s_code))
                                           : 0u;
        const int alone =
            (assembled != 0u) && sass_assemble(&s_machine, redirected, 0ull, 0ull, SASS_CONTROL_SAFE, &low, &high);
        const unsigned int place = alone ? fields_find(s_code, assembled, low, high) : assembled;
        if (place == assembled)
        {
            printf("skip %s: did not assemble or was not found once\n", walk);
            continue;
        }
        // the kernel as it will run, each instruction read back through the machine file, for every register and
        // predicate it writes to be checked before anything reaches the part. A line no form reads back is printed as
        // its encoding
        for (unsigned int number = 0u; number < assembled; number += 1u)
        {
            const unsigned long long one = fields_word_read(&s_code[16u * number]);
            const unsigned long long two = fields_word_read(&s_code[(16u * number) + 8u]);
            char text[256];
            if (sass_encoding_read(&s_machine, one, two, 16ull * number, text, sizeof(text)))
            {
                printf("CODE\t%u\t%s%s\n", number, text, (number == place) ? "\t<- turned" : "");
            }
            else
            {
                printf("CODE\t%u\t0x%016llx 0x%016llx\n", number, one, two);
            }
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/list.txt", folder);
        FILE *const made = fopen(path, "wb");
        // the baseline first, its bit 128 turning nothing, then one line an operation bit. A bit whose turned operation
        // key holds a control transfer, a wait or no form is held off the part with a skip line in its place: a
        // bit-flip never loops or stalls the part, and the line order still names the bit a row belongs to
        for (unsigned int bit = 0u; bit <= FIELDS_OPERATION_BITS; bit += 1u)
        {
            const unsigned int turned = (bit == 0u) ? 128u : (bit - 1u);
            char name[64];
            char cubin[1024];
            snprintf(cubin, sizeof(cubin), "%s/bit_%03u.cubin", folder, bit);
            const int safe = fields_flip_safe(place, turned, name, sizeof(name));
            if (safe && fields_cubin(s_code, assembled, place, turned, cubin))
            {
                if (made != NULL)
                {
                    fprintf(made, "%s %s\n", cubin, s_kernel);
                }
            }
            else if (made != NULL)
            {
                fprintf(made, "skip %s\n", name);
            }
        }
        if (made != NULL)
        {
            fclose(made);
        }
        printf("wrote %u cubins for %s to %s\n", FIELDS_OPERATION_BITS + 1u, walk, path);
        break;
    }
    fclose(instructions);
    return 0;
}
