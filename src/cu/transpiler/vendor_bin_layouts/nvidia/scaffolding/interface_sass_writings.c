// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_writings.c: a writing searched for on the part. Every form of the machine file that writes a register
// from registers, predicates and numbers alone is put in place of the kernel's IADD3 with the case's first two words as
// its sources, run on the part over every case at once, and read back against what each ladder relation and each
// precept gives each case. A form that gives every case a relation's word is a writing of that relation in one
// instruction, found by running it and by nothing else: no name, listing or compiler says which form adds. A number
// whose field is eight bits wide is a truth table of three inputs, and a form holding one is put with each of its 256
// values (precept_value.h).
//
// Nothing is compiled and no disassembler is run. The cubins are written here and run by interface_sass_run, which
// holds each to cubin_safe, loads it through the driver and runs it over the cases file this writes, a thread a case.
//
//     interface_sass_writings <pattern cubin> <kernel text> <machine file> <folder>
//     interface_sass_writings read <folder> <record>
//     interface_sass_writings chains <pattern cubin> <kernel text> <machine file> <folder> <kdm>
//     interface_sass_writings chains-read <folder> <record>
//
// The first writes <folder>/list.txt, the cubins it names, <folder>/cases.txt and <folder>/forms.txt, the
// instruction each cubin holds a line. The second reads <folder>/answers.txt, which interface_sass_run writes, against
// every relation and writes the record. The third writes every arrangement of the .kdm node by node, each node the
// first writing found for its precept, into <folder>/chains, and the fourth reads the chains' answers back against
// the relation each is a row of.
#include "interface_sass_writings/writings_internal.h"
#include "../../../../engine/rmc/precept_value.h"

int main(int count, char **words)
{
    if ((count == 5) && (strcmp(words[1], "krs-read") == 0))
    {
        return writings_krs_read(words[2], words[3], words[4]);
    }
    if (((count == 7) || (count == 8)) && (strcmp(words[1], "chains") == 0))
    {
        char into[1024];
        snprintf(into, sizeof(into), "%s/chains", words[5]);
        return writings_chains_write(words[2], words[3], words[4], words[5], words[6], (count == 8) ? words[7] : into);
    }
    if ((count >= 5) && ((count % 2) == 1) && (strcmp(words[1], "descent-read") == 0))
    {
        return writings_descent_read(words[2], (unsigned int)(count - 3) / 2u, &words[3]);
    }
    if ((count == 4) && (strcmp(words[1], "chains-read") == 0))
    {
        return writings_chains_read(words[2], words[3]);
    }
    if ((count == 4) && (strcmp(words[1], "read") == 0))
    {
        return writings_read(words[2], words[3]);
    }
    if (count != 5)
    {
        fprintf(stderr, "interface_sass_writings <pattern cubin> <kernel text> <machine file> <folder>\n"
                        "interface_sass_writings read <folder> <record>\n"
                        "interface_sass_writings chains <pattern cubin> <kernel text> <machine file> <folder> <kdm> "
                        "[<into>]\n"
                        "interface_sass_writings chains-read <folder> <record>\n"
                        "interface_sass_writings descent-read <record> <into> <cases> [<into> <cases>...]\n");
        return 2;
    }
    return writings_write(words[1], words[2], words[3], words[4]);
}
